
// for strtod()
#include <stdlib.h>

#include "nsIRenderingContext.h"
#include "prlink.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <pango/pango.h>
#include <pango/pangox.h>
#include <pango/pango-fontmap.h>

#include "nsSystemFontsGTK2.h"

static PRInt32 GetXftDPI(void);

// Glue to avoid build/runtime dependencies on Pango > 1.6

static gboolean
(* PTR_pango_font_description_get_size_is_absolute)(PangoFontDescription*)
    = nsnull;

static void InitPangoLib()
{
    static PRBool initialized = PR_FALSE;
    if (initialized)
        return;
    initialized = PR_TRUE;

    PRLibrary* lib = PR_LoadLibrary("libpango-1.0.so");
    if (!lib)
        return;

    PTR_pango_font_description_get_size_is_absolute =
        (gboolean (*)(PangoFontDescription*))
        PR_FindFunctionSymbol(lib, "pango_font_description_get_size_is_absolute");

    // leak lib deliberately
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

#define DEFAULT_TWIP_FONT_SIZE 240

nsSystemFontsGTK2::nsSystemFontsGTK2(float aPixelsToTwips)
  : mDefaultFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
                 DEFAULT_TWIP_FONT_SIZE),
    mButtonFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
                DEFAULT_TWIP_FONT_SIZE),
    mFieldFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
               NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
               DEFAULT_TWIP_FONT_SIZE),
    mMenuFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
               NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE,
               DEFAULT_TWIP_FONT_SIZE)
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

    GetSystemFontInfo(label, &mDefaultFont, aPixelsToTwips);

    gtk_widget_destroy(window);  // no unref, windows are different

    // mFieldFont
    GtkWidget *entry = gtk_entry_new();
    parent = gtk_fixed_new();
    window = gtk_window_new(GTK_WINDOW_POPUP);

    gtk_container_add(GTK_CONTAINER(parent), entry);
    gtk_container_add(GTK_CONTAINER(window), parent);
    gtk_widget_ensure_style(entry);

    GetSystemFontInfo(entry, &mFieldFont, aPixelsToTwips);

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

    GetSystemFontInfo(accel_label, &mMenuFont, aPixelsToTwips);

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

    GetSystemFontInfo(label, &mButtonFont, aPixelsToTwips);

    gtk_widget_destroy(window);  // no unref, windows are different
}

nsresult
nsSystemFontsGTK2::GetSystemFontInfo(GtkWidget *aWidget, nsFont* aFont,
                                     float aPixelsToTwips) const
{
    GtkSettings *settings = gtk_widget_get_settings(aWidget);

    aFont->style       = NS_FONT_STYLE_NORMAL;
    aFont->decorations = NS_FONT_DECORATION_NONE;

    gchar *fontname;
    g_object_get(settings, "gtk-font-name", &fontname, NULL);

    PangoFontDescription *desc;
    desc = pango_font_description_from_string(fontname);

    aFont->systemFont = PR_TRUE;

    g_free(fontname);

    aFont->name.Assign(PRUnichar('"'));
    aFont->name.AppendWithConversion(pango_font_description_get_family(desc));
    aFont->name.Append(PRUnichar('"'));

    aFont->weight = pango_font_description_get_weight(desc);

    float size = float(pango_font_description_get_size(desc) / PANGO_SCALE);

    // |size| is now either pixels or pango-points (not Mozilla-points!)

    if (!MOZ_pango_font_description_get_size_is_absolute(desc)) {
        // |size| is in pango-points, so convert to pixels
        PRInt32 dpi = GetXftDPI();
        if (dpi != 0) {
            size *= float(dpi) / 72.0f;
        }
    }

    // |size| is now pixels

    aFont->size = NSToCoordRound(size * aPixelsToTwips);
  
    pango_font_description_free(desc);

    return NS_OK;
}

/* static */
PRInt32
GetXftDPI(void)
{
  char *val = XGetDefault(GDK_DISPLAY(), "Xft", "dpi");
  if (val) {
    char *e;
    double d = strtod(val, &e);

    if (e != val)
      return NSToCoordRound(d);
  }

  return 0;
}
