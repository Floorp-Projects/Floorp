/* A first attempt to set up a font selection scheme for mozilla
 * Perhaps this should be put in a seperate widget, who knows.
 
 * This code is a massive rip off from the GTK Font Selector widget.
 * The font look up is probably not as efficient as it ought to be...

 * Moz_font_select: code for selecting fonts in mozilla
 * Based on GtkFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
 */

#include "gdk/gdktypes.h"
#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"

#include <gtk/gtk.h>

#include "xp_core.h"
#define Bool char
#include <lo_ele.h>
#include <X11/Xlib.h>
#define Bool char

#include "xpassert.h"

#include "g-font-select.h"

#define MAX_FONTS 32767

/* This is the number of properties which we keep in the properties array,
   i.e. Weight, Slant, Set Width, Spacing, Charset & Foundry. */
#define NUM_FONT_PROPERTIES	 6

/* This is the number of properties each style has i.e. Weight, Slant,
   Set Width, Spacing & Charset. Note that Foundry is not included,
   since it is the same for all styles of the same FontInfo. */
#define NUM_STYLE_PROPERTIES 5

/* This is the largest field length we will accept. If a fontname has a field
   larger than this we will skip it. */
#define XLFD_MAX_FIELD_LEN 64

/* This is the number of fields in an X Logical Font Description font name.
   Note that we count the registry & encoding as 1. */
#define XLFD_NUM_FIELDS 13

/* Used for the flags field in FontStyle. Note that they can be combined,
   e.g. a style can have multiple bitmaps and a true scalable version.
   The displayed flag is used when displaying styles to remember which
   styles have already been displayed. */
enum
{
  BITMAP_FONT		= 1 << 0,
  SCALABLE_FONT		= 1 << 1,
  SCALABLE_BITMAP_FONT	= 1 << 2,
  DISPLAYED		= 1 << 3
};

typedef struct _FontSelInfo FontSelInfo;
typedef struct _FontStyle FontStyle;
typedef struct _MozFontInfo MozFontInfo;

/* This struct represents one family of fonts (with one foundry), e.g. adobe
   courier or sony fixed. It stores the family name, the index of the foundry
   name, and the index of and number of available styles. */

/* This represents one style, as displayed in the Font Style clist. It can
   have a number of available pixel sizes and point sizes. The indexes point
   into the two big fontsel_info->pixel_sizes & fontsel_info->point_sizes arrays. */
struct _FontStyle
{
  guint16  properties[NUM_STYLE_PROPERTIES];
  gint	   pixel_sizes_index;
  guint16  npixel_sizes;
  gint	   point_sizes_index;
  guint16  npoint_sizes;
  guint8   flags;
};

struct _MozFontInfo
{
  gchar   *family;
  guint16  foundry;
  gint	   style_index;
  guint16  nstyles;
};

struct _FontSelInfo {  
  MozFontInfo *font_info;
  gint nfonts;
  
  /* This stores all the valid combinations of properties for every family.
     Each FontInfo holds an index into its own space in this one big array. */
  FontStyle *font_styles;
  gint nstyles;
  
  /* This stores all the font sizes available for every style.
     Each style holds an index into these arrays. */
  guint16 *pixel_sizes;
  guint16 *point_sizes;
  
  /* These are the arrays of strings of all possible weights, slants, 
     set widths, spacings, charsets & foundries, and the amount of space
     allocated for each array. */
  gchar **properties[NUM_FONT_PROPERTIES];
  guint16 nproperties[NUM_FONT_PROPERTIES];
  guint16 space_allocated[NUM_FONT_PROPERTIES];
  
  /* Whether any scalable bitmap fonts are available. If not, the 'Allow
     scaled bitmap fonts' toggle button is made insensitive. */
  gboolean scaled_bitmaps_available;
};

		
typedef enum
{
  XLFD_FOUNDRY		= 0,
  XLFD_FAMILY		= 1,
  XLFD_WEIGHT		= 2,
  XLFD_SLANT		= 3,
  XLFD_SET_WIDTH	= 4,
  XLFD_ADD_STYLE	= 5,
  XLFD_PIXELS		= 6,
  XLFD_POINTS		= 7,
  XLFD_RESOLUTION_X	= 8,
  XLFD_RESOLUTION_Y	= 9,
  XLFD_SPACING		= 10,
  XLFD_AVERAGE_WIDTH	= 11,
  XLFD_CHARSET		= 12
} FontField;

typedef enum 
{
  METRIC_PIXEL = 0,
  METRIC_POINT = 1,
} MozFontMetric;

/* These are the names of the fields, used on the info & filter page. */
static gchar* xlfd_field_names[XLFD_NUM_FIELDS] = {
  "Foundry:",
  "Family:",
  "Weight:",
  "Slant:",
  "Set Width:",
  "Add Style:",
  "Pixel Size:",
  "Point Size:",
  "Resolution X:",
  "Resolution Y:",
  "Spacing:",
  "Average Width:",
  "Charset:",
};

/* These are the array indices of the font properties used in several arrays,
   and should match the xlfd_index array below. */
typedef enum
{
  WEIGHT	= 0,
  SLANT		= 1,
  SET_WIDTH	= 2,
  SPACING	= 3,
  CHARSET	= 4,
  FOUNDRY	= 5
} PropertyIndexType;

/* This is used to look up a field in a fontname given one of the above
   property indices. */
static FontField xlfd_index[NUM_FONT_PROPERTIES] = {
  XLFD_WEIGHT,
  XLFD_SLANT,
  XLFD_SET_WIDTH,
  XLFD_SPACING,
  XLFD_CHARSET,
  XLFD_FOUNDRY
};

/* The initial size and increment of each of the arrays of property values. */
#define PROPERTY_ARRAY_INCREMENT	16


/* prototypes */
static gchar*   
moz_get_xlfd_field (const gchar *fontname,
		    FontField    field_num,
		    gchar       *buffer);
static gint
moz_insert_field (gchar		       *fontname,
		  gint			prop);
static gboolean 
moz_xlfd_font_name (const gchar *fontname);

static void
moz_insert_font (GSList		      *fontnames[],
		 gint		      *ntable,
		 gchar		      *fontname);

static char *
NameFromList(char *namelist);
/**/

static FontSelInfo *fontsel_info = NULL;

/* initialize the fontselinfo struct */
void 
moz_font_init()  {  
  gchar **xfontnames;
  GSList **fontnames;
  gchar *fontname;
  GSList * temp_list;
  gint num_fonts;
  gint i, prop, style, size;
  gint npixel_sizes = 0, npoint_sizes = 0;
  MozFontInfo *font;
  FontStyle *current_style, *prev_style, *tmp_style;
  gboolean matched_style, found_size;
  gint pixels, points, res_x, res_y;
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint8 flags;
  guint16 *pixel_sizes, *point_sizes, *tmp_sizes;
    
  fontsel_info = g_new(FontSelInfo, 1);
  xfontnames = XListFonts (GDK_DISPLAY(), "-*", MAX_FONTS, &num_fonts);  
  if (num_fonts == MAX_FONTS) {
    g_warning("Maximum number of fonsts exceeded.");    
  }
  
  /* Are MozFontInfo etc defined? */
  fontsel_info->font_info = g_new (MozFontInfo, num_fonts);
  fontsel_info->font_styles = g_new (FontStyle, num_fonts);
  fontsel_info->pixel_sizes = g_new (guint16, num_fonts);
  fontsel_info->point_sizes = g_new (guint16, num_fonts);
  
  fontnames = g_new (GSList*, num_fonts);
  
  /* idem for NUM_FONT_PROPERTIES etc */ 
  for (prop = 0; prop < NUM_FONT_PROPERTIES; prop++) {
    fontsel_info->properties[prop] = g_new(gchar*, PROPERTY_ARRAY_INCREMENT);
    fontsel_info->space_allocated[prop] = PROPERTY_ARRAY_INCREMENT;
    fontsel_info->nproperties[prop] = 1;
    fontsel_info->properties[prop][0] = "*";
  }
 
  /* Insert the font families into the main table, sorted by family and
     foundry (fonts with different foundries are placed in seaparate MozFontInfos.
     All fontnames in each family + foundry are placed into the fontnames
     array of lists. */
  fontsel_info->nfonts = 0;
  for (i = 0; i < num_fonts; i++) {
#ifdef FRV_DEBUG
    printf("Adding font %s\n", xfontnames[i]); 
#endif
    if (moz_xlfd_font_name (xfontnames[i])) {
      moz_insert_font (fontnames, &fontsel_info->nfonts, xfontnames[i]);
    } 
    else
      {
#ifdef FRV_DEBUG
	g_warning("Skipping invalid font: %s", xfontnames[i]);
#endif
      }
  }
  /* Since many font names will be in the same MozFontInfo not all of the
     allocated MozFontInfo table will be used, so we will now reallocate it
     with the real size. */
  fontsel_info->font_info = g_realloc(fontsel_info->font_info,
				      sizeof(MozFontInfo) * fontsel_info->nfonts);
  
  /* Now we work out which choices of weight/slant etc. are valid for each
     font. */
  fontsel_info->nstyles = 0;
  current_style = fontsel_info->font_styles;
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      font = &fontsel_info->font_info[i];
      
      /* Use the next free position in the styles array. */
      font->style_index = fontsel_info->nstyles;
      
      /* Now step through each of the fontnames with this family, and create
	 a style for each fontname. Each style contains the index into the
	 weights/slants etc. arrays, and a number of pixel/point sizes. */
      style = 0;
      temp_list = fontnames[i];
      while (temp_list)
	{
	  fontname = temp_list->data;
	  temp_list = temp_list->next;
	  
	  for (prop = 0; prop < NUM_STYLE_PROPERTIES; prop++)
	    {
	      current_style->properties[prop]
		= moz_insert_field (fontname, prop);
	    }
	  current_style->pixel_sizes_index = npixel_sizes;
	  current_style->npixel_sizes = 0;
	  current_style->point_sizes_index = npoint_sizes;
	  current_style->npoint_sizes = 0;
	  current_style->flags = 0;
	  
	  
	  field = moz_get_xlfd_field (fontname, XLFD_PIXELS,
				      field_buffer);
	  pixels = atoi(field);
	  
	  field = moz_get_xlfd_field (fontname, XLFD_POINTS,
				      field_buffer);
	  points = atoi(field);
	  
	  field = moz_get_xlfd_field (fontname, XLFD_RESOLUTION_X,
				      field_buffer);
	  res_x = atoi(field);
	  
	  field = moz_get_xlfd_field (fontname,
				      XLFD_RESOLUTION_Y,
				      field_buffer);
	  res_y = atoi(field);
	  
	  if (pixels == 0 && points == 0)
	    {
	      if (res_x == 0 && res_y == 0)
		flags = SCALABLE_FONT;
	      else
		{
		  flags = SCALABLE_BITMAP_FONT;
		  fontsel_info->scaled_bitmaps_available = TRUE;
		}
	    }
	  else
	    flags = BITMAP_FONT;
	  
	  /* Now we check to make sure that the style is unique. If it isn't
	     we forget it. */
	  prev_style = fontsel_info->font_styles + font->style_index;
	  matched_style = FALSE;
	  while (prev_style < current_style)
	    {
	      matched_style = TRUE;
	      for (prop = 0; prop < NUM_STYLE_PROPERTIES; prop++)
		{
		  if (prev_style->properties[prop]
		      != current_style->properties[prop])
		    {
		      matched_style = FALSE;
		      break;
		    }
		}
	      if (matched_style)
		break;
	      prev_style++;
	    }
	  
	  /* If we matched an existing style, we need to add the pixels &
	     point sizes to the style. If not, we insert the pixel & point
	     sizes into our new style. Note that we don't add sizes for
	     scalable fonts. */
	  if (matched_style)
	    {
	      prev_style->flags |= flags;
	      if (flags == BITMAP_FONT)
		{
		  pixel_sizes = fontsel_info->pixel_sizes
		    + prev_style->pixel_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npixel_sizes; size++)
		    {
		      if (pixels == *pixel_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (pixels < *pixel_sizes)
			break;
		      pixel_sizes++;
		    }
		  /* We need to move all the following pixel sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->pixel_sizes + npixel_sizes;
			   tmp_sizes > pixel_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *pixel_sizes = pixels;
		      npixel_sizes++;
		      prev_style->npixel_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->pixel_sizes_index++;
			  tmp_style++;
			}
		    }
		  
		  point_sizes = fontsel_info->point_sizes
		    + prev_style->point_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npoint_sizes; size++)
		    {
		      if (points == *point_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (points < *point_sizes)
			break;
		      point_sizes++;
		    }
		  /* We need to move all the following point sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->point_sizes + npoint_sizes;
			   tmp_sizes > point_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *point_sizes = points;
		      npoint_sizes++;
		      prev_style->npoint_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->point_sizes_index++;
			  tmp_style++;
			}
		    }
		}
	    }
	  else
	    {
	      current_style->flags = flags;
	      if (flags == BITMAP_FONT)
		{
		  fontsel_info->pixel_sizes[npixel_sizes++] = pixels;
		  current_style->npixel_sizes = 1;
		  fontsel_info->point_sizes[npoint_sizes++] = points;
		  current_style->npoint_sizes = 1;
		}
	      style++;
	      fontsel_info->nstyles++;
	      current_style++;
	    }
	}
      g_slist_free(fontnames[i]);
      
      /* Set nstyles to the real value, minus duplicated fontnames.
	 Note that we aren't using all the allocated memory if fontnames are
	 duplicated. */
      font->nstyles = style;
    }
  
  /* Since some repeated styles may be skipped we won't have used all the
     allocated space, so we will now reallocate it with the real size. */
  fontsel_info->font_styles = g_realloc(fontsel_info->font_styles,
					sizeof(FontStyle) * fontsel_info->nstyles);
  fontsel_info->pixel_sizes = g_realloc(fontsel_info->pixel_sizes,
					sizeof(guint16) * npixel_sizes);
  fontsel_info->point_sizes = g_realloc(fontsel_info->point_sizes,
					sizeof(guint16) * npoint_sizes);
  g_free(fontnames);
  XFreeFontNames (xfontnames);
  
  
  /* Debugging Output */
  /* This outputs all MozFontInfos. */
#ifdef FONTSEL_DEBUG
  g_message("\n\n Font Family           Weight    Slant     Set Width Spacing   Charset\n\n");
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      MozFontInfo *font = &fontsel_info->font_info[i];
      FontStyle *styles = fontsel_info->font_styles + font->style_index;
      for (style = 0; style < font->nstyles; style++)
	{
	  g_message("%5i %-16.16s ", i, font->family);
	  for (prop = 0; prop < NUM_STYLE_PROPERTIES; prop++)
	    g_message("%-9.9s ",
		      fontsel_info->properties[prop][styles->properties[prop]]);
	  g_message("\n      ");
	  
	  if (styles->flags & BITMAP_FONT)
	    g_message("Bitmapped font  ");
	  if (styles->flags & SCALABLE_FONT)
	    g_message("Scalable font  ");
	  if (styles->flags & SCALABLE_BITMAP_FONT)
	    g_message("Scalable-Bitmapped font  ");
	  g_message("\n");
	  
	  if (styles->npixel_sizes)
	    {
	      g_message("      Pixel sizes: ");
	      tmp_sizes = fontsel_info->pixel_sizes + styles->pixel_sizes_index;
	      for (size = 0; size < styles->npixel_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  if (styles->npoint_sizes)
	    {
	      g_message("      Point sizes: ");
	      tmp_sizes = fontsel_info->point_sizes + styles->point_sizes_index;
	      for (size = 0; size < styles->npoint_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  g_message("\n");
	  styles++;
	}
    }
  /* This outputs all available properties. */
  for (prop = 0; prop < NUM_FONT_PROPERTIES; prop++)
    {
      g_message("Property: %s\n", xlfd_field_names[xlfd_index[prop]]);
      for (i = 0; i < fontsel_info->nproperties[prop]; i++)
        g_message("  %s\n", fontsel_info->properties[prop][i]);
    }
#endif
}
/************************************************************************
 * The field parsing and manipulation routines
 ***********************************************************************

/* This checks that the specified field of the given fontname is in the
   appropriate properties array. If not it is added. Thus eventually we get
   arrays of all possible weights/slants etc. It returns the array index. */
static gint
moz_insert_field (gchar		       *fontname,
		  gint			prop)
{
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint16 index;
  
  field = moz_get_xlfd_field (fontname, xlfd_index[prop],
			      field_buffer);
  if (!field)
    return 0;
  
  /* If the field is already in the array just return its index. */
  for (index = 0; index < fontsel_info->nproperties[prop]; index++)
    if (!strcmp(field, fontsel_info->properties[prop][index]))
      return index;
  
  /* Make sure we have enough space to add the field. */
  if (fontsel_info->nproperties[prop] == fontsel_info->space_allocated[prop])
    {
      fontsel_info->space_allocated[prop] += PROPERTY_ARRAY_INCREMENT;
      fontsel_info->properties[prop] = g_realloc(fontsel_info->properties[prop],
						 sizeof(gchar*)
						 * fontsel_info->space_allocated[prop]);
    }
  
  /* Add the new field. */
  index = fontsel_info->nproperties[prop];
  fontsel_info->properties[prop][index] = g_strdup(field);
  fontsel_info->nproperties[prop]++;
  return index;
}


/* This inserts the given fontname into the FontInfo table.
   If a FontInfo already exists with the same family and foundry, then the
   fontname is added to the FontInfos list of fontnames, else a new FontInfo
   is created and inserted in alphabetical order in the table. */
static void
moz_insert_font (GSList		      *fontnames[],
		 gint		      *ntable,
		 gchar		      *fontname)
{
  MozFontInfo *table;
  MozFontInfo temp_info;
  GSList *temp_fontname;
  gchar *family;
  gboolean family_exists = FALSE;
  gint foundry;
  gint lower, upper;
  gint middle, cmp;
  gchar family_buffer[XLFD_MAX_FIELD_LEN];
  
  table = fontsel_info->font_info;
  
  /* insert a fontname into a table */
  family = moz_get_xlfd_field (fontname, XLFD_FAMILY,
			       family_buffer);
  if (!family)
    return;
  
  foundry = moz_insert_field (fontname, FOUNDRY);
  
  lower = 0;
  if (*ntable > 0)
    {
      /* Do a binary search to determine if we have already encountered
       *  a font with this family & foundry. */
      upper = *ntable;
      while (lower < upper)
	{
	  middle = (lower + upper) >> 1;
	  
	  cmp = strcmp (family, table[middle].family);
	  /* If the family matches we sort by the foundry. */
	  if (cmp == 0)
	    {
	      family_exists = TRUE;
	      family = table[middle].family;
	      cmp = strcmp(fontsel_info->properties[FOUNDRY][foundry],
			   fontsel_info->properties[FOUNDRY][table[middle].foundry]);
	    }
	  
	  if (cmp == 0)
	    {
	      fontnames[middle] = g_slist_prepend (fontnames[middle],
						   fontname);
	      return;
	    }
	  else if (cmp < 0)
	    upper = middle;
	  else
	    lower = middle+1;
	}
    }
  
  /* Add another entry to the table for this new font family */
  temp_info.family = family_exists ? family : g_strdup(family);
  temp_info.foundry = foundry;
  temp_fontname = g_slist_prepend (NULL, fontname);
  
  (*ntable)++;
  
  /* Quickly insert the entry into the table in sorted order
   *  using a modification of insertion sort and the knowledge
   *  that the entries proper position in the table was determined
   *  above in the binary search and is contained in the "lower"
   *  variable. */
  if (*ntable > 1)
    {
      upper = *ntable - 1;
      while (lower != upper)
	{
	  table[upper] = table[upper-1];
	  fontnames[upper] = fontnames[upper-1];
	  upper--;
	}
    }
  table[lower] = temp_info;
  fontnames[lower] = temp_fontname;
}

/*****************************************************************************
 * These functions all deal with X Logical Font Description (XLFD) fontnames.
 * See the freely available documentation about this.
 *****************************************************************************/

/*
 * Returns TRUE if the fontname is a valid XLFD.
 * (It just checks if the number of dashes is 14, and that each
 * field < XLFD_MAX_FIELD_LEN  characters long - that's not in the XLFD but it
 * makes it easier for me).
 */
static gboolean
moz_xlfd_font_name (const gchar *fontname)
{
  gint i = 0;
  gint field_len = 0;
  
  while (*fontname)
    {
      if (*fontname++ == '-')
	{
	  if (field_len > XLFD_MAX_FIELD_LEN) return FALSE;
	  field_len = 0;
	  i++;
	}
      else
	field_len++;
    }
  
  return (i == 14) ? TRUE : FALSE;
}

/*
 * This fills the buffer with the specified field from the X Logical Font
 * Description name, and returns it. If fontname is NULL or the field is
 * longer than XFLD_MAX_FIELD_LEN it returns NULL.
 * Note: For the charset field, we also return the encoding, e.g. 'iso8859-1'.
 */
static gchar*
moz_get_xlfd_field (const gchar *fontname,
		    FontField    field_num,
		    gchar       *buffer)
{
  const gchar *t1, *t2;
  gint countdown, len, num_dashes;
  
  if (!fontname)
    return NULL;
  
  /* we assume this is a valid fontname...that is, it has 14 fields */
  
  countdown = field_num;
  t1 = fontname;
  while (*t1 && (countdown >= 0))
    if (*t1++ == '-')
      countdown--;
  
  num_dashes = (field_num == XLFD_CHARSET) ? 2 : 1;
  for (t2 = t1; *t2; t2++)
    { 
      if (*t2 == '-' && --num_dashes == 0)
	break;
    }
  
  if (t1 != t2)
    {
      /* Check we don't overflow the buffer */
      len = (long) t2 - (long) t1;
      if (len > XLFD_MAX_FIELD_LEN - 1)
	return NULL;
      strncpy (buffer, t1, len);
      buffer[len] = 0;
    }
  else
    strcpy(buffer, "(nil)");
  
  return buffer;
}

/*
 * This returns a X Logical Font Description font name, given all the pieces.
 * Note: this retval must be freed by the caller.
 */
static gchar *
moz_create_xlfd (gint		  size,
		 MozFontMetric   metric,
		 gchar		  *foundry,
		 gchar		  *family,
		 gchar		  *weight,
		 gchar		  *slant,
		 gchar		  *set_width,
		 gchar		  *spacing,
		 gchar		  *charset)
{
  gchar buffer[16];
  gchar *pixel_size = "*", *point_size = "*", *fontname;
  gint length;
  
  if (size <= 0)
    return NULL;
  
  sprintf (buffer, "%d", (int) size);
  if (metric == METRIC_PIXEL)
    pixel_size = buffer;
  else
    point_size = buffer;
  
  /* Note: be careful here - don't overrun the allocated memory. */
  length = strlen(foundry) + strlen(family) + strlen(weight) + strlen(slant)
    + strlen(set_width) + strlen(pixel_size) + strlen(point_size)
    + strlen(spacing) + strlen(charset)
    + 1 + 1 + 1 + 1 + 1 + 3 + 1 + 5 + 3
    + 1 /* for the terminating '\0'. */;
  
  fontname = g_new(gchar, length);
  /* **NOTE**: If you change this string please change length above! */
  sprintf(fontname, "-%s-%s-%s-%s-%s-*-%s-%s-*-*-%s-*-%s",
	  foundry, family, weight, slant, set_width, pixel_size,
	  point_size, spacing, charset);
  return fontname;
}


/* Returns the Gdkfont matching the description
 * Possible bug: Foundry info is not token into 
 * account! First font that matches the family 
 * name is taken. Feel free to fix this
 */ 
GdkFont *moz_get_font_with_family(char *fam, 
				  int pts, 
				  char *wgt, 
				  gboolean ita) {
  MozFontInfo *table;
  MozFontInfo *font;
  FontStyle *styles;
  gint tsize;
  gint middle, upper, lower;
  gint style;
  gint weight_index, slant_index, width_index, spacing_index, charset_index;
  gchar *slant;
  gchar *set_width;
  gchar *spacing;
  gchar *charset;
  gchar *weight;
  gint index=0;
  gint cmp;
  /* initialize fontsel_info if necessary */
  /* perhaps needs to go into main routine */
  if (!fontsel_info) {
    moz_font_init();
  }

  tsize =  fontsel_info->nfonts;
  table = fontsel_info->font_info;

  /* die a horible death if tsize == 0 */
  /*XP_ASSERT(tsize > 0);*/ 

  /* do a binary search for the family */
  /* I am ignoring foundry issues here */
  
  lower = 0;
  upper = tsize;
  middle;
  while (lower < upper) {
    middle = (lower + upper) >> 1;
    cmp = strcmp (fam, table[middle].family);
    if (cmp <  0 ) {
      upper = middle;
    } 
    else if (cmp > 0) {
      lower = middle+1; 
    } 
    else {
      /* we have have a hit! */
      index=middle;
      break;
    }
  }
  
  /* if we haven't found our font, return NULL */
  /* check that index is 0 to be sure */
  if ( (lower >= upper) && (index == 0) ) { 
    return NULL;
  }
  
  font = &table[index];
  /* Find the best match (seperate routine perhaps?) */
  styles= &fontsel_info->font_styles[font->style_index];
  for (style=0; style < font->nstyles; style++) {
    /* grab the indices */
    weight_index = styles[style].properties[WEIGHT];
    slant_index = styles[style].properties[SLANT];
    width_index =  styles[style].properties[SET_WIDTH];
    spacing_index =  styles[style].properties[SPACING];
    charset_index =  styles[style].properties[CHARSET];
    weight    = fontsel_info->properties[WEIGHT]   [weight_index];
    slant     = fontsel_info->properties[SLANT]    [slant_index];
    set_width = fontsel_info->properties[SET_WIDTH][width_index];
    spacing   = fontsel_info->properties[SPACING]  [spacing_index];
    charset   = fontsel_info->properties[CHARSET]  [charset_index];
    if (strcmp(set_width, "nil") == 0) 
      set_width="";
    if (strcmp(spacing, "nil") == 0 )
      set_width="";
    if (strcmp(charset, "nil") == 0 )
      charset="";

    /* find a match, grab the font and run */
    if (strcmp(weight, wgt) == 0) {
      if (ita) {
	if (g_strcasecmp(slant, "i") == 0) {
	  return 
	    gdk_font_load (
			   moz_create_xlfd(pts,
					   METRIC_PIXEL,
					   fontsel_info->properties[FOUNDRY][font->foundry],
					   fam,
					   wgt,
					   slant,
					   set_width,
					   spacing,
					   charset));
	}
      }
      else if (g_strcasecmp(slant, "r") == 0) {
	return
	  gdk_font_load (
			 moz_create_xlfd(pts,
					 METRIC_PIXEL,
					 fontsel_info->properties[FOUNDRY][font->foundry],
					 fam,
					 wgt,
					 slant,
					 set_width,
					 spacing,
					 charset));	
      }
    }
  }
}

typedef enum 
{
  WEIGHT_LIGHT = 25,
  WEIGHT_MEDIUM=50,
  WEIGHT_BOLD = 75
} WeightType;
				    
GdkFont *
moz_get_font(LO_TextAttr *text_attr) {
  GdkFont *result;
  char fam[100];
  char *family_name;
  char *weight;
  int pts, wgt;
  gboolean ita;
  int size3 = 12;
  int length, index;
  
  if (text_attr->FE_Data) {    
    return (GdkFont*)(text_attr->FE_Data);
  }
  
  if (!text_attr->font_face) 
    {
      /* where can I retrieve the preferneces default font???? */
      strcpy(fam, (text_attr->fontmask & LO_FONT_FIXED) ? "courier" : "times");
    } 
  else 
    {
      strcpy(fam, text_attr->font_face);
      /* to make sure we always get a font returning... */
      if (strlen(fam) < 90) 
	strcat(fam, ",times");
    }
  wgt=text_attr->font_weight;
  
  /* this is a bit hacky... */
  if (!wgt) {
    wgt = (text_attr->fontmask & LO_FONT_BOLD) ? WEIGHT_BOLD  : WEIGHT_MEDIUM;
  }

  if (wgt <= WEIGHT_LIGHT) {
    weight = "light";
  } 
  else if (wgt > WEIGHT_LIGHT && wgt < WEIGHT_BOLD) {
    weight = "medium";
  } else {
    weight = "bold";
  }
  
  ita = (text_attr->fontmask & LO_FONT_ITALIC) ? TRUE : FALSE;
  
  /* I don't understand this formula, and stole is straight from 
   * qtfe. Corrections welcome
   */
  pts = (text_attr->point_size);
  if ( pts <= 0) {

    switch ( text_attr->size ) {
      case 1:
	pts = 8;
	break;
      case 2:
	pts = 10;
        break;
      case 3:
	pts = 12;
        break;
      case 4:
	pts = 14;
        break;
      case 5:
	pts = 18;
        break;
      case 6:
	pts = 24;
        break;
      default:
	pts = (text_attr->size-6)*4+24;
    }
    pts = pts * size3 / 12;
  }
  length = strlen(fam);
  family_name = NameFromList(fam);
  while (family_name)  
    {
      result = moz_get_font_with_family(family_name, pts, weight, ita);
      if (result) 
	break;
      /* wipe out the name, and go on to the next */
      index = 0;
      while (fam[index] != '\0')
	{
	  fam[index] = ' ';
	  index++;
	}
      if (index != length) 
	fam[index] = ' ';      
      family_name = NameFromList(fam);
    }
  XP_ASSERT(result);
  
  text_attr->FE_Data = result;  
  return result;
}


void
moz_font_release_text_attribute(LO_TextAttr *attr) 
{
  g_free( (GdkFont*)attr->FE_Data);
}

/* Adapted from XFE, but returns  */
static char *
NameFromList(char *namelist)
{
  static char * list;
  static int next;
  static int len;
  
  int start;
  int end;
  char final = '\0';
  
  if (namelist) 
    {
      list = namelist;
      len = strlen(namelist);
      next = 0;
    }
    
  start = next;
  
  if (! list || list[start] == '\0')
    return (char *) NULL;
  
  /* skip over leading whitespace and commas to find the start of the name */
  while (list[start] != '\0' && (list[start] == ',' || isspace(list[start])))
    start++;
  
  /* find the end of the name */
  next = start;
  while(list[next] != '\0' && list[next] != ',') {
    if (list[next] == '"' || list[next] == '\'') {
      final = list[next];
      
      /* find the matching quote */
      next++;
      while (list[next] != '\0' && list[next] != final)
	next++;

      /* if list[next] is null, there was no matching quote, so bail */
      if (list[next] == '\0')
	break;
    }
    next++;
  }
  end = next - 1;
  
  if (list[next] != '\0' && next < len)
    next++;

  /* strip off trailing whitespace */        
  while (end >= start && isspace(list[end]))
    end--;
  
  /* if it's quoted, strip off the quotes */
  if ((list[start] == '"' || list[start] == '\'') && list[end] == list[start]) {
    start++;
    end--;
  }
  
  end++;
  list[end] = '\0';
  return &(list[start]);
}


