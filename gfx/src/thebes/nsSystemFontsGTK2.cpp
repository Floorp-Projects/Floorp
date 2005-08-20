
// for strtod()
#include <stdlib.h>

#include "nsIRenderingContext.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <pango/pango.h>
#include <pango/pangox.h>
#include <pango/pango-fontmap.h>

#include "nsSystemFontsGTK2.h"

static PRInt32 GetXftDPI(void);
static void AppendFontFFREName(nsString& aString, const char* aXLFDName);

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

#ifdef MOZ_ENABLE_COREXFONTS
static void xlfd_from_pango_font_description(GtkWidget *aWidget,
                                             const PangoFontDescription *aFontDesc,
                                             nsString& aFontName);
#endif /* MOZ_ENABLE_COREXFONTS */

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

    aFont->name.Truncate();
#ifdef MOZ_ENABLE_XFT
    aFont->name.Assign(PRUnichar('"'));
    aFont->name.AppendWithConversion(pango_font_description_get_family(desc));
    aFont->name.Append(PRUnichar('"'));
#endif /* MOZ_ENABLE_XFT */

#ifdef MOZ_ENABLE_COREXFONTS
    // if name already set by Xft, do nothing
    if (!aFont->name.Length()) {
        xlfd_from_pango_font_description(aWidget, desc, aFont->name);
    }
#endif /* MOZ_ENABLE_COREXFONTS */
    aFont->weight = pango_font_description_get_weight(desc);

    float size = float(pango_font_description_get_size(desc) / PANGO_SCALE);
#ifdef MOZ_ENABLE_XFT
    PRInt32 dpi = GetXftDPI();
    if (dpi != 0) {
        // pixels/inch * twips/pixel * inches/twip == 1, except it isn't, since
        // our idea of dpi may be different from Xft's.
        size *= float(dpi) * aPixelsToTwips * (1.0f/1440.0f);
    }
#endif /* MOZ_ENABLE_XFT */
    aFont->size = NSFloatPointsToTwips(size);
  
    pango_font_description_free(desc);

    return NS_OK;
}

#if defined(MOZ_ENABLE_COREXFONTS)
// xlfd_from_pango_font_description copied from vte, which was
// written by nalin@redhat.com, and added some codes.
static void
xlfd_from_pango_font_description(GtkWidget *aWidget,
         const PangoFontDescription *aFontDesc,
                                 nsString& aFontName)
{
  char *spec;
  PangoContext *context;
  PangoFont *font;
  PangoXSubfont *subfont_ids;
  PangoFontMap *fontmap;
  int *subfont_charsets, i, count = 0;
  char *subfont;
  char *encodings[] = {
    "ascii-0",
    "big5-0",
    "dos-437",
    "dos-737",
    "gb18030.2000-0",
    "gb18030.2000-1",
    "gb2312.1980-0",
    "iso8859-1",
    "iso8859-2",
    "iso8859-3",
    "iso8859-4",
    "iso8859-5",
    "iso8859-7",
    "iso8859-8",
    "iso8859-9",
    "iso8859-10",
    "iso8859-15",
    "iso10646-0",
    "iso10646-1",
    "jisx0201.1976-0",
    "jisx0208.1983-0",
    "jisx0208.1990-0",
    "jisx0208.1997-0",
    "jisx0212.1990-0",
    "jisx0213.2000-1",
    "jisx0213.2000-2",
    "koi8-r",
    "koi8-u",
    "koi8-ub",
    "ksc5601.1987-0",
    "ksc5601.1992-3",
    "tis620-0",
    "iso8859-13",
    "microsoft-cp1251"
    "misc-fontspecific",
  };
#if XlibSpecificationRelease >= 6
  XOM xom;
#endif
  if (!aFontDesc) {
    return;
  }

  context = gtk_widget_get_pango_context(GTK_WIDGET(aWidget));

  pango_context_set_language (context, gtk_get_default_language ());
  fontmap = pango_x_font_map_for_display(GDK_DISPLAY());

  if (!fontmap) {
    return;
  }

  font = pango_font_map_load_font(fontmap, context, aFontDesc);
  if (!font) {
    return;
  }

#if XlibSpecificationRelease >= 6
  xom = XOpenOM (GDK_DISPLAY(), NULL, NULL, NULL);
  if (xom) {
    XOMCharSetList cslist;
    int n_encodings = 0;
    cslist.charset_count = 0;
    XGetOMValues (xom,
      XNRequiredCharSet, &cslist,
      NULL);
    n_encodings = cslist.charset_count;
    if (n_encodings) {
      char **xom_encodings = (char**) g_malloc (sizeof(char*) * n_encodings);

      for (i = 0; i < n_encodings; i++) {
        xom_encodings[i] = g_ascii_strdown (cslist.charset_list[i], -1);
      }
      count = pango_x_list_subfonts(font, xom_encodings, n_encodings,
            &subfont_ids, &subfont_charsets);

      for(i = 0; i < n_encodings; i++) {
        g_free (xom_encodings[i]);
      }
      g_free (xom_encodings);
    }
    XCloseOM (xom);
  }
#endif
  if (count == 0) {
    count = pango_x_list_subfonts(font, encodings, G_N_ELEMENTS(encodings),
          &subfont_ids, &subfont_charsets);
  }

  for (i = 0; i < count; i++) {
    subfont = pango_x_font_subfont_xlfd(font, subfont_ids[i]);
    AppendFontFFREName(aFontName, subfont);
    g_free(subfont);
    aFontName.Append(PRUnichar(','));
  }

  spec = pango_font_description_to_string(aFontDesc);

  if (subfont_ids != NULL) {
    g_free(subfont_ids);
  }
  if (subfont_charsets != NULL) {
    g_free(subfont_charsets);
  }
  g_free(spec);
}
#endif /* MOZ_ENABLE_COREXFONTS */

#if defined(MOZ_ENABLE_COREXFONTS) || defined(MOZ_WIDGET_GTK)

#define LOCATE_MINUS(pos, str)  { \
   pos = str.FindChar('-'); \
   if (pos < 0) \
     return ; \
  }
#define NEXT_MINUS(pos, str) { \
   pos = str.FindChar('-', pos+1); \
   if (pos < 0) \
     return ; \
  }  

static void
AppendFontFFREName(nsString& aString, const char* aXLFDName)
{
  // convert fontname from XFLD to FFRE and append, ie. from
  // -adobe-courier-medium-o-normal--14-140-75-75-m-90-iso8859-15
  // to
  // adobe-courier-iso8859-15
  nsCAutoString nameStr(aXLFDName);
  PRInt32 pos1, pos2;
  // remove first '-' and everything before it. 
  LOCATE_MINUS(pos1, nameStr);
  nameStr.Cut(0, pos1+1);

  // skip foundry and family name
  LOCATE_MINUS(pos1, nameStr);
  NEXT_MINUS(pos1, nameStr);
  pos2 = pos1;

  // find '-' just before charset registry
  for (PRInt32 i=0; i < 10; i++) {
    NEXT_MINUS(pos2, nameStr);
  }

  // remove everything in between
  nameStr.Cut(pos1, pos2-pos1);

  aString.AppendWithConversion(nameStr.get());
}
#endif /* MOZ_ENABLE_COREXFONTS || MOZ_WIDGET_GTK*/

#ifdef MOZ_ENABLE_XFT
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
#endif /* MOZ_ENABLE_XFT */
