/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Stuart Parmenter <pavlov@pavlov.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// for strtod()
#include <stdlib.h>

#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "prlink.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <pango/pango.h>
#include <pango/pangox.h>
#include <pango/pango-fontmap.h>

#include <fontconfig/fontconfig.h>
#include "nsSystemFontsGTK2.h"
#include "gfxPlatformGtk.h"

// Glue to avoid build/runtime dependencies on Pango > 1.6
#ifndef THEBES_USE_PANGO_CAIRO
static gboolean
(* PTR_pango_font_description_get_size_is_absolute)(PangoFontDescription*)
    = nsnull;
static PRLibrary *gPangoLib = nsnull;

static void InitPangoLib()
{
    static PRBool initialized = PR_FALSE;
    if (initialized)
        return;
    initialized = PR_TRUE;

    gPangoLib = PR_LoadLibrary("libpango-1.0.so");
    if (!gPangoLib)
        return;

    PTR_pango_font_description_get_size_is_absolute =
        (gboolean (*)(PangoFontDescription*))
        PR_FindFunctionSymbol(gPangoLib,
                              "pango_font_description_get_size_is_absolute");
}

static void
ShutdownPangoLib()
{
    if (gPangoLib) {
        PR_UnloadLibrary(gPangoLib);
        gPangoLib = nsnull;
    }
}

static gboolean
MOZ_pango_font_description_get_size_is_absolute(PangoFontDescription *desc)
{
    if (PTR_pango_font_description_get_size_is_absolute) {
        return PTR_pango_font_description_get_size_is_absolute(desc);
    }

    // In old versions of pango, this was always false.
    return PR_FALSE;
}
#else
static inline void InitPangoLib()
{
}

static inline void ShutdownPangoLib()
{
}

static inline gboolean
MOZ_pango_font_description_get_size_is_absolute(PangoFontDescription *desc)
{
    pango_font_description_get_size_is_absolute(desc);
}
#endif

#define DEFAULT_PIXEL_FONT_SIZE 16.0f

nsSystemFontsGTK2::nsSystemFontsGTK2()
  : mDefaultFontName(NS_LITERAL_STRING("sans-serif"))
  , mButtonFontName(NS_LITERAL_STRING("sans-serif"))
  , mFieldFontName(NS_LITERAL_STRING("sans-serif"))
  , mMenuFontName(NS_LITERAL_STRING("sans-serif"))
  , mDefaultFontStyle(FONT_STYLE_NORMAL, FONT_VARIANT_NORMAL,
                 FONT_WEIGHT_NORMAL, FONT_DECORATION_NONE,
                 DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
                 0.0f, PR_TRUE, PR_FALSE)
  , mButtonFontStyle(FONT_STYLE_NORMAL, FONT_VARIANT_NORMAL,
                FONT_WEIGHT_NORMAL, FONT_DECORATION_NONE,
                DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
                0.0f, PR_TRUE, PR_FALSE)
  , mFieldFontStyle(FONT_STYLE_NORMAL, FONT_VARIANT_NORMAL,
               FONT_WEIGHT_NORMAL, FONT_DECORATION_NONE,
               DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
               0.0f, PR_TRUE, PR_FALSE)
  , mMenuFontStyle(FONT_STYLE_NORMAL, FONT_VARIANT_NORMAL,
               FONT_WEIGHT_NORMAL, FONT_DECORATION_NONE,
               DEFAULT_PIXEL_FONT_SIZE, NS_LITERAL_CSTRING(""),
               0.0f, PR_TRUE, PR_FALSE)
{
    InitPangoLib();

    /*
     * Much of the widget creation code here is similar to the code in
     * nsLookAndFeel::InitColors().
     */

    // mDefaultFont
    GtkWidget *label = gtk_label_new("M");
    GtkWidget *parent = gtk_fixed_new();
    GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);

    gtk_container_add(GTK_CONTAINER(parent), label);
    gtk_container_add(GTK_CONTAINER(window), parent);

    gtk_widget_ensure_style(label);

    GetSystemFontInfo(label, &mDefaultFontName, &mDefaultFontStyle);

    gtk_widget_destroy(window);  // no unref, windows are different

    // mFieldFont
    GtkWidget *entry = gtk_entry_new();
    parent = gtk_fixed_new();
    window = gtk_window_new(GTK_WINDOW_POPUP);

    gtk_container_add(GTK_CONTAINER(parent), entry);
    gtk_container_add(GTK_CONTAINER(window), parent);
    gtk_widget_ensure_style(entry);

    GetSystemFontInfo(entry, &mFieldFontName, &mFieldFontStyle);

    gtk_widget_destroy(window);  // no unref, windows are different

    // mMenuFont
    GtkWidget *accel_label = gtk_accel_label_new("M");
    GtkWidget *menuitem = gtk_menu_item_new();
    GtkWidget *menu = gtk_menu_new();
    gtk_object_ref(GTK_OBJECT(menu));
    gtk_object_sink(GTK_OBJECT(menu));

    gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
    gtk_menu_append(GTK_MENU(menu), menuitem);

    gtk_widget_ensure_style(accel_label);

    GetSystemFontInfo(accel_label, &mMenuFontName, &mMenuFontStyle);

    gtk_widget_unref(menu);

    // mButtonFont
    parent = gtk_fixed_new();
    GtkWidget *button = gtk_button_new();
    label = gtk_label_new("M");
    window = gtk_window_new(GTK_WINDOW_POPUP);
          
    gtk_container_add(GTK_CONTAINER(button), label);
    gtk_container_add(GTK_CONTAINER(parent), button);
    gtk_container_add(GTK_CONTAINER(window), parent);

    gtk_widget_ensure_style(label);

    GetSystemFontInfo(label, &mButtonFontName, &mButtonFontStyle);

    gtk_widget_destroy(window);  // no unref, windows are different
}

nsSystemFontsGTK2::~nsSystemFontsGTK2()
{
    ShutdownPangoLib();
}

nsresult
nsSystemFontsGTK2::GetSystemFontInfo(GtkWidget *aWidget, nsString *aFontName,
                                     gfxFontStyle *aFontStyle) const
{
    GtkSettings *settings = gtk_widget_get_settings(aWidget);

    aFontStyle->style       = FONT_STYLE_NORMAL;
    aFontStyle->decorations = FONT_DECORATION_NONE;

    gchar *fontname;
    g_object_get(settings, "gtk-font-name", &fontname, NULL);

    PangoFontDescription *desc;
    desc = pango_font_description_from_string(fontname);

    aFontStyle->systemFont = PR_TRUE;

    g_free(fontname);

    NS_NAMED_LITERAL_STRING(quote, "\"");
    NS_ConvertUTF8toUTF16 family(pango_font_description_get_family(desc));
    *aFontName = quote + family + quote;

    aFontStyle->weight = pango_font_description_get_weight(desc);

    float size = float(pango_font_description_get_size(desc) / PANGO_SCALE);

    // |size| is now either pixels or pango-points (not Mozilla-points!)

    if (!MOZ_pango_font_description_get_size_is_absolute(desc)) {
        // |size| is in pango-points, so convert to pixels.
        size *= float(gfxPlatformGtk::DPI()) / 72.0f;
    }

    // |size| is now pixels

    aFontStyle->size = size;
  
    pango_font_description_free(desc);

    return NS_OK;
}

nsresult
nsSystemFontsGTK2::GetSystemFont(nsSystemFontID anID, nsString *aFontName,
                                 gfxFontStyle *aFontStyle) const
{
    switch (anID) {
    case eSystemFont_Menu:         // css2
    case eSystemFont_PullDownMenu: // css3
        *aFontName = mMenuFontName;
        *aFontStyle = mMenuFontStyle;
        break;

    case eSystemFont_Field:        // css3
    case eSystemFont_List:         // css3
        *aFontName = mFieldFontName;
        *aFontStyle = mFieldFontStyle;
        break;

    case eSystemFont_Button:       // css3
        *aFontName = mButtonFontName;
        *aFontStyle = mButtonFontStyle;
        break;

    case eSystemFont_Caption:      // css2
    case eSystemFont_Icon:         // css2
    case eSystemFont_MessageBox:   // css2
    case eSystemFont_SmallCaption: // css2
    case eSystemFont_StatusBar:    // css2
    case eSystemFont_Window:       // css3
    case eSystemFont_Document:     // css3
    case eSystemFont_Workspace:    // css3
    case eSystemFont_Desktop:      // css3
    case eSystemFont_Info:         // css3
    case eSystemFont_Dialog:       // css3
    case eSystemFont_Tooltips:     // moz
    case eSystemFont_Widget:       // moz
        *aFontName = mDefaultFontName;
        *aFontStyle = mDefaultFontStyle;
        break;
    }

    return NS_OK;
}
