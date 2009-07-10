/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>

#include <xfconf/xfconf.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "xfce-randr.h"
#include "xfce-randr-legacy.h"
#include "display-dialog_glade.h"

/* not available yet */
#undef HAS_RANDR_ONE_POINT_TWO

enum
{
    COLUMN_OUTPUT_NAME,
    COLUMN_OUTPUT_ICON_NAME,
    COLUMN_OUTPUT_ID,
    N_OUTPUT_COLUMNS
};

enum
{
    COLUMN_COMBO_NAME,
    COLUMN_COMBO_VALUE,
    N_COMBO_COLUMNS
};




/* xrandr rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { RR_Rotate_0,   N_("Normal") },
    { RR_Rotate_90,  N_("Left") },
    { RR_Rotate_180, N_("Inverted") },
    { RR_Rotate_270, N_("Right") }
};



/* option entries */
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* global xfconf channel */
static XfconfChannel *display_channel;

/* pointer to the used randr structure */
#ifdef HAS_RANDR_ONE_POINT_TWO
XfceRandr *xfce_randr = NULL;
#endif
XfceRandrLegacy *xfce_randr_legacy = NULL;



static gboolean
display_setting_combo_box_get_value (GtkComboBox *combobox,
                                     gint        *value)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        model = gtk_combo_box_get_model (combobox);
        gtk_tree_model_get (model, &iter, COLUMN_COMBO_VALUE, value, -1);

        return TRUE;
    }

    return FALSE;
}



static void
display_setting_rotations_changed (GtkComboBox *combobox,
                                   GladeXML    *gxml)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new rotation */
#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
        XFCE_RANDR_ROTATION (xfce_randr) = value;
    else
#endif
        XFCE_RANDR_LEGACY_ROTATION (xfce_randr_legacy) = value;
}



static void
display_setting_rotations_populate (GladeXML *gxml)
{
    GtkTreeModel *model;
    GtkWidget    *combobox;
    Rotation      rotations;
    Rotation      active_rotation;
    guint         n;
    GtkTreeIter   iter;

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "randr-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        rotations = active_rotation = RR_Rotate_0;
    }
    else
#endif
    {
        /* load all possible rotations */
        rotations = XRRConfigRotations (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy), &active_rotation);
        active_rotation = XFCE_RANDR_LEGACY_ROTATION (xfce_randr_legacy);
    }

    /* try to insert the rotations */
    for (n = 0; n < G_N_ELEMENTS (rotation_names); n++)
    {
        if ((rotations & rotation_names[n].rotation) != 0)
        {
            /* insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(rotation_names[n].name),
                                COLUMN_COMBO_VALUE, rotation_names[n].rotation, -1);

            /* select active rotation */
            if (rotation_names[n].rotation == active_rotation)
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }
}



static void
display_setting_refresh_rates_changed (GtkComboBox *combobox,
                                       GladeXML    *gxml)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new rotation */
#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
        XFCE_RANDR_MODE (xfce_randr) = value;
    else
#endif
        XFCE_RANDR_LEGACY_RATE (xfce_randr_legacy) = value;
}



static void
display_setting_refresh_rates_populate (GladeXML *gxml)
{
    GtkTreeModel *model;
    GtkWidget    *combobox;
    gshort       *rates;
    gint          nrates;
    GtkTreeIter   iter;
    gchar        *name = NULL;
    gint          n, active = -1;
    gshort        diff, active_diff = G_MAXSHORT;
#ifdef HAS_RANDR_ONE_POINT_TWO
    GtkWidget    *rescombo;
    XRRModeInfo  *mode_info;
    gchar        *mode_name;
    gfloat        rate;
    RRMode        active_mode;
#endif

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    /* get the selected resolution mode */
    rescombo = glade_xml_get_widget (gxml, "randr-resolution");
    if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (rescombo), (gint *) &active_mode))
        active_mode = 0;
    
    if (xfce_randr)
    {
        /* find the selected resolution name */
        for (n = 0; n < xfce_randr->resources->nmode; n++)
        {
            if (xfce_randr->resources->modes[n].id == active_mode)
            {
                /* set the mode (resolution) name */
                mode_name = xfce_randr->resources->modes[n].name;

                /* walk the modes again to add the resolutions that match the name */
                for (n = 0; n < xfce_randr->resources->nmode; n++)
                {
                    mode_info = &xfce_randr->resources->modes[n];

                    if (strcmp (mode_name, mode_info->name) != 0)
                        continue;

                    /* calculate the refresh rate */
                    rate = (gfloat) mode_info->dotClock / ((gfloat) mode_info->hTotal * (gfloat) mode_info->vTotal);

                    /* insert in the combo box */
                    name = g_strdup_printf (_("%d Hz"), (gint) rate);
                    gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                        COLUMN_COMBO_NAME, name,
                                        COLUMN_COMBO_VALUE, mode_info->id, -1);
                    g_free (name);

                    /* select the active mode */
                    if (mode_info->id == XFCE_RANDR_MODE (xfce_randr))
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
                }

                /* select the first item if there is no active */
                if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
                    gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

                /* finished */
                break;
            }
        }
    }
    else
#endif
    {
        /* get the refresh rates */
        rates = XRRConfigRates (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy),
                                XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy), &nrates);

        for (n = 0; n < nrates; n++)
        {
            /* insert */
            name = g_strdup_printf (_("%d Hz"), rates[n]);
            gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, name,
                                COLUMN_COMBO_VALUE, rates[n], -1);
            g_free (name);

            /* get the active rate closest to the current diff */
            diff = ABS (rates[n] - XFCE_RANDR_LEGACY_RATE (xfce_randr_legacy));

            /* store active */
            if (active_diff > diff)
            {
                active = nrates - n - 1;
                active_diff = diff;
            }
        }

        /* set closest refresh rate */
        if (G_LIKELY (active != -1))
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), active);
    }
}



static void
display_setting_resolutions_changed (GtkComboBox *combobox,
                                     GladeXML    *gxml)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new rotation */
    if (xfce_randr_legacy)
        XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy) = value;

    /* update refresh rates */
    display_setting_refresh_rates_populate (gxml);
}



static void
display_setting_resolutions_populate (GladeXML *gxml)
{
    GtkTreeModel  *model;
    GtkWidget     *combobox;
    XRRScreenSize *screen_sizes;
    gint           n, nsizes;
    gchar         *name;
    GtkTreeIter    iter;
#ifdef HAS_RANDR_ONE_POINT_TWO
    XRRModeInfo   *mode, *prev = NULL;
    gint           m;
#endif

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* walk all the modes */
        for (n = 0; n < xfce_randr->resources->nmode; n++)
        {
            /* get the mode */
            mode = &xfce_randr->resources->modes[n];

            /* check if this mode is supported by the output */
            for (m = 0; m < XFCE_RANDR_OUTPUT_INFO (xfce_randr)->nmode; m++)
            {
                if (XFCE_RANDR_OUTPUT_INFO (xfce_randr)->modes[m] == mode->id)
                {
                    /* avoid dupplicates */
                    if (!prev || strcmp (prev->name, mode->name) != 0)
                    {
                        /* insert the mode */
                        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                            COLUMN_COMBO_NAME, mode->name,
                                            COLUMN_COMBO_VALUE, mode->id, -1);

                        /* set the previous mode */
                        prev = mode;
                    }

                    /* finished */
                    break;
                }
            }
        }
    }
    else
#endif
    {
        /* get the possible screen sizes for this screen */
        screen_sizes = XRRConfigSizes (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy), &nsizes);

        for (n = 0; n < nsizes; n++)
        {
            /* insert in the model */
            name = g_strdup_printf ("%dx%d", screen_sizes[n].width, screen_sizes[n].height);
            gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, n,
                                               COLUMN_COMBO_NAME, name,
                                               COLUMN_COMBO_VALUE, n, -1);
            g_free (name);

            /* select active */
            if (G_UNLIKELY (XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy) == n))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }
}



static void
display_settings_treeview_selection_changed (GtkTreeSelection *selection,
                                             GladeXML         *gxml)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gint          active_id;

    /* get the selection */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get the output info */
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &active_id, -1);

        /* set the new active screen or output */
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr->active_output = active_id;
        else
#endif
            xfce_randr_legacy->active_screen = active_id;

        /* update the combo boxes */
        display_setting_resolutions_populate (gxml);
        display_setting_rotations_populate (gxml);
    }
}



static void
display_settings_treeview_populate (GladeXML *gxml)
{
    gint              n;
    GtkListStore     *store;
    GtkWidget        *treeview;
    GtkTreeIter       iter;
    gchar            *name;
    GtkTreeSelection *selection;

    /* create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                G_TYPE_STRING, /* COLUMN_OUTPUT_ICON_NAME */
                                G_TYPE_INT);   /* COLUMN_OUTPUT_ID */
    
    /* set the treeview model */
    treeview = glade_xml_get_widget (gxml, "randr-outputs");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* walk all the outputs */
        for (n = 0; n < xfce_randr->resources->noutput; n++)
        {
            /* only show screen that are primary or secondary */
            if (xfce_randr->status[n] == XFCE_OUTPUT_STATUS_NONE)
                continue;

            /* get a friendly name for the output */
            name = (gchar *) xfce_randr_friendly_name (xfce_randr->output_info[n]->name);

            /* insert the output in the store */
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, name,
                                COLUMN_OUTPUT_ICON_NAME, "video-display",
                                COLUMN_OUTPUT_ID, n, -1);
            
            /* select active output */
            if (n == xfce_randr->active_output)
                gtk_tree_selection_select_iter (selection, &iter);
        }
    }
    else
#endif
    {
        /* walk all the screens */
        for (n = 0; n < xfce_randr_legacy->num_screens; n++)
        {
            /* create name */
            name = g_strdup_printf (_("Screen %d"), n + 1);

            /* insert the output in the store */
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, name,
                                COLUMN_OUTPUT_ICON_NAME, "video-display",
                                COLUMN_OUTPUT_ID, n, -1);

            /* cleanup */
            g_free (name);
            
            /* select active screen */
            if (n == xfce_randr_legacy->active_screen)
                gtk_tree_selection_select_iter (selection, &iter);
        }
    }

    /* release the store */
    g_object_unref (G_OBJECT (store));
}



static void
display_settings_combo_box_create (GtkComboBox *combobox)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;

    /* create and set the combobox model */
    store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));

    /* setup renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", COLUMN_COMBO_NAME);
}



static void
display_settings_dialog_response (GtkDialog *dialog,
                                  gint       response_id,
                                  GladeXML  *gxml)
{
    if (response_id == 1)
    {
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr_save (xfce_randr, "Default", display_channel);
        else
#endif
            xfce_randr_legacy_save (xfce_randr_legacy, "Default", display_channel);
    }
    else if (response_id == 2)
    {
        /* reload randr information */
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr_reload (xfce_randr);
        else
#endif
            xfce_randr_legacy_reload (xfce_randr_legacy);

        /* update dialog */
        display_settings_treeview_populate (gxml);
    }
    else
    {
        /* close */
        gtk_main_quit ();
    }
}



static GtkWidget *
display_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget        *treeview;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *selection;
    GtkWidget        *combobox;

    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "randr-outputs");
#if GTK_CHECK_VERSION (2, 12, 0)
        gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_OUTPUT_NAME);
#endif

    /* icon renderer */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 0, "", renderer, "icon-name", COLUMN_OUTPUT_ICON_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);

    /* text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 1, "", renderer, "text", COLUMN_OUTPUT_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* treeview selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_treeview_selection_changed), gxml);

    /* setup the combo boxes */
    combobox = glade_xml_get_widget (gxml, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), gxml);

    combobox = glade_xml_get_widget (gxml, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), gxml);

    combobox = glade_xml_get_widget (gxml, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), gxml);

    /* populate the treeview */
    display_settings_treeview_populate (gxml);

    return glade_xml_get_widget (gxml, "display-dialog");
}



gint
main (gint argc, gchar **argv)
{
    GtkWidget  *dialog;
    GladeXML   *gxml;
    GError     *error = NULL;
    GdkDisplay *display;
    gboolean    succeeded = TRUE;
    gint        event_base, error_base;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2008");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* get the default display */
    display = gdk_display_get_default ();

    /* check if the randr extension is avaible on the system */
    if (!XRRQueryExtension (gdk_x11_display_get_xdisplay (display), &event_base, &error_base))
    {
        dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
                                         _("RandR extension missing on display \"%s\""),
                                         gdk_display_get_name (display));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  _("The Resize and Rotate extension (RandR) is not enabled on "
                                                    "this display. Try to enable it and run the dialog again."));
        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_CLOSE);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        return EXIT_FAILURE;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    display_channel = xfconf_channel_new ("displays");
    if (G_LIKELY (display_channel))
    {
#ifdef HAS_RANDR_ONE_POINT_TWO
        /* create a new xfce randr (>= 1.2) for this display
         * this will only work if there is 1 screen on this display */
        if (gdk_display_get_n_screens (display) == 1)
            xfce_randr = xfce_randr_new (display, NULL);

        /* fall back on the legacy backend */
        if (xfce_randr == NULL)
#endif
        {
            xfce_randr_legacy = xfce_randr_legacy_new (display, &error);
            if (G_UNLIKELY (xfce_randr_legacy == NULL))
            {
                /* show an error dialog the version is too old */
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
                                                 _("Failed to use the RandR extension"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), error->message);
                gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_CLOSE);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                g_error_free (error);

                /* leave and cleanup the data */
                goto err1;
            }
        }

        /* load the dialog glade xml */
        gxml = glade_xml_new_from_buffer (display_dialog_glade, display_dialog_glade_length, NULL, NULL);
        if (G_LIKELY (gxml))
        {
            /* build the dialog */
            dialog = display_settings_dialog_new_from_xml (gxml);
            g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (display_settings_dialog_response), gxml);

#if 0
#ifdef HAS_RANDR_ONE_POINT_TWO
            if (xfce_randr == NULL)
#endif
            {
                /* destroy the devices box */
                box = glade_xml_get_widget (gxml, "randr-devices-tab");
                gtk_widget_destroy (box);
            }
#endif

            /* show the dialog */
            gtk_widget_show (dialog);

            /* enter the main loop */
            gtk_main ();

            /* destroy the dialog */
            if (GTK_IS_WIDGET (dialog))
                gtk_widget_destroy (dialog);

            /* release the glade xml */
            g_object_unref (G_OBJECT (gxml));
        }
        
        err1:

        /* release the channel */
        g_object_unref (G_OBJECT (display_channel));
    }

#ifdef HAS_RANDR_ONE_POINT_TWO
    /* free the randr 1.2 backend */
    if (xfce_randr)
        xfce_randr_free (xfce_randr);
#endif

    /* free the legacy backend */
    if (xfce_randr_legacy)
        xfce_randr_legacy_free (xfce_randr_legacy);

    /* shutdown xfconf */
    xfconf_shutdown ();

    return (succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
    
}
