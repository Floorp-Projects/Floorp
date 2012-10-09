/*
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#define HB_SHAPER old
#define hb_old_shaper_face_data_t HB_FaceRec_
#define hb_old_shaper_font_data_t HB_Font_
#include "hb-shaper-impl-private.hh"

#include <harfbuzz.h>


#ifndef HB_DEBUG_OLD
#define HB_DEBUG_OLD (HB_DEBUG+0)
#endif


static HB_Script
hb_old_script_from_script (hb_script_t script)
{
  switch ((hb_tag_t) script)
  {
    default:
    case HB_SCRIPT_COMMON:		return HB_Script_Common;
    case HB_SCRIPT_GREEK:		return HB_Script_Greek;
    case HB_SCRIPT_CYRILLIC:		return HB_Script_Cyrillic;
    case HB_SCRIPT_ARMENIAN:		return HB_Script_Armenian;
    case HB_SCRIPT_HEBREW:		return HB_Script_Hebrew;
    case HB_SCRIPT_ARABIC:		return HB_Script_Arabic;
    case HB_SCRIPT_SYRIAC:		return HB_Script_Syriac;
    case HB_SCRIPT_THAANA:		return HB_Script_Thaana;
    case HB_SCRIPT_DEVANAGARI:		return HB_Script_Devanagari;
    case HB_SCRIPT_BENGALI:		return HB_Script_Bengali;
    case HB_SCRIPT_GURMUKHI:		return HB_Script_Gurmukhi;
    case HB_SCRIPT_GUJARATI:		return HB_Script_Gujarati;
    case HB_SCRIPT_ORIYA:		return HB_Script_Oriya;
    case HB_SCRIPT_TAMIL:		return HB_Script_Tamil;
    case HB_SCRIPT_TELUGU:		return HB_Script_Telugu;
    case HB_SCRIPT_KANNADA:		return HB_Script_Kannada;
    case HB_SCRIPT_MALAYALAM:		return HB_Script_Malayalam;
    case HB_SCRIPT_SINHALA:		return HB_Script_Sinhala;
    case HB_SCRIPT_THAI:		return HB_Script_Thai;
    case HB_SCRIPT_LAO:			return HB_Script_Lao;
    case HB_SCRIPT_TIBETAN:		return HB_Script_Tibetan;
    case HB_SCRIPT_MYANMAR:		return HB_Script_Myanmar;
    case HB_SCRIPT_GEORGIAN:		return HB_Script_Georgian;
    case HB_SCRIPT_HANGUL:		return HB_Script_Hangul;
    case HB_SCRIPT_OGHAM:		return HB_Script_Ogham;
    case HB_SCRIPT_RUNIC:		return HB_Script_Runic;
    case HB_SCRIPT_KHMER:		return HB_Script_Khmer;
    case HB_SCRIPT_NKO:			return HB_Script_Nko;
    case HB_SCRIPT_INHERITED:		return HB_Script_Inherited;
  }
}


static HB_Bool
hb_old_convertStringToGlyphIndices (HB_Font old_font,
				    const HB_UChar16 *string,
				    hb_uint32 length,
				    HB_Glyph *glyphs,
				    hb_uint32 *numGlyphs,
				    HB_Bool rightToLeft)
{
  hb_font_t *font = (hb_font_t *) old_font->userData;

  for (unsigned int i = 0; i < length; i++)
  {
    hb_codepoint_t u;

    /* XXX Handle UTF-16.  Ugh */
    u = string[i];

    if (rightToLeft)
      u = hb_unicode_funcs_get_default ()->mirroring (u);

    font->get_glyph (u, 0, &u); /* TODO Variation selectors */

    glyphs[i] = u;
  }
  *numGlyphs = length; // XXX

  return true;
}

static void
hb_old_getGlyphAdvances (HB_Font old_font,
			 const HB_Glyph *glyphs,
			 hb_uint32 numGlyphs,
			 HB_Fixed *advances,
			 int flags /*HB_ShaperFlag*/)
{
  hb_font_t *font = (hb_font_t *) old_font->userData;

  for (unsigned int i = 0; i < numGlyphs; i++)
    advances[i] = font->get_glyph_h_advance (glyphs[i]);
}

static HB_Bool
hb_old_canRender (HB_Font old_font,
		  const HB_UChar16 *string,
		  hb_uint32 length)
{
  return true; // TODO
}

static HB_Error
hb_old_getPointInOutline (HB_Font old_font,
			  HB_Glyph glyph,
			  int flags /*HB_ShaperFlag*/,
			  hb_uint32 point,
			  HB_Fixed *xpos,
			  HB_Fixed *ypos,
			  hb_uint32 *nPoints)
{
  return HB_Err_Ok; // TODO
}

static void
hb_old_getGlyphMetrics (HB_Font old_font,
			HB_Glyph glyph,
			HB_GlyphMetrics *metrics)
{
  hb_font_t *font = (hb_font_t *) old_font->userData;

  hb_glyph_extents_t extents;

  font->get_glyph_extents (glyph, &extents);

  metrics->x       = extents.x_bearing;
  metrics->y       = extents.y_bearing;
  metrics->width   = extents.width;
  metrics->height  = extents.height;
  metrics->xOffset = font->get_glyph_h_advance (glyph);
  metrics->yOffset = 0;
}

static HB_Fixed
hb_old_getFontMetric (HB_Font old_font,
		      HB_FontMetric metric)
{
  hb_font_t *font = (hb_font_t *) old_font->userData;

  switch (metric)
  {
    case HB_FontAscent:
       return font->y_scale; /* XXX We don't have ascent data yet. */

    default:
      return 0;
  }
}

static const HB_FontClass hb_old_font_class = {
  hb_old_convertStringToGlyphIndices,
  hb_old_getGlyphAdvances,
  hb_old_canRender,
  hb_old_getPointInOutline,
  hb_old_getGlyphMetrics,
  hb_old_getFontMetric
};



static HB_Error
table_func (void *font, HB_Tag tag, HB_Byte *buffer, HB_UInt *length)
{
  hb_face_t *face = (hb_face_t *) font;
  hb_blob_t *blob = face->reference_table ((hb_tag_t) tag);
  unsigned int capacity = *length;
  *length = hb_blob_get_length (blob);
  memcpy (buffer, hb_blob_get_data (blob, NULL), MIN (capacity, *length));
  hb_blob_destroy (blob);
 return HB_Err_Ok;
}


/*
 * shaper face data
 */

hb_old_shaper_face_data_t *
_hb_old_shaper_face_data_create (hb_face_t *face)
{
  return HB_NewFace (face, table_func);
}

void
_hb_old_shaper_face_data_destroy (hb_old_shaper_face_data_t *data)
{
  HB_FreeFace (data);
}


/*
 * shaper font data
 */

hb_old_shaper_font_data_t *
_hb_old_shaper_font_data_create (hb_font_t *font)
{
  HB_FontRec *data = (HB_FontRec *) calloc (1, sizeof (HB_FontRec));
  if (unlikely (!data)) {
    DEBUG_MSG (OLD, font, "malloc()ing HB_Font failed");
    return NULL;
  }

  data->klass = &hb_old_font_class;
  data->x_ppem = font->x_ppem;
  data->y_ppem = font->y_ppem;
  data->x_scale = font->x_scale; // XXX
  data->y_scale = font->y_scale; // XXX
  data->userData = font;

  return data;
}

void
_hb_old_shaper_font_data_destroy (hb_old_shaper_font_data_t *data)
{
  free (data);
}


/*
 * shaper shape_plan data
 */

struct hb_old_shaper_shape_plan_data_t {};

hb_old_shaper_shape_plan_data_t *
_hb_old_shaper_shape_plan_data_create (hb_shape_plan_t    *shape_plan,
				       const hb_feature_t *user_features,
				       unsigned int        num_user_features)
{
  return (hb_old_shaper_shape_plan_data_t *) HB_SHAPER_DATA_SUCCEEDED;
}

void
_hb_old_shaper_shape_plan_data_destroy (hb_old_shaper_shape_plan_data_t *data)
{
}


/*
 * shaper
 */

hb_bool_t
_hb_old_shape (hb_shape_plan_t    *shape_plan,
	       hb_font_t          *font,
	       hb_buffer_t        *buffer,
	       const hb_feature_t *features,
	       unsigned int        num_features)
{
  hb_face_t *face = font->face;
  HB_Face old_face = HB_SHAPER_DATA_GET (face);
  HB_Font old_font = HB_SHAPER_DATA_GET (font);

  bool backward = HB_DIRECTION_IS_BACKWARD (buffer->props.direction);

retry:

  unsigned int scratch_size;
  char *scratch = (char *) buffer->get_scratch_buffer (&scratch_size);

#define utf16_index() var1.u32
  HB_UChar16 *pchars = (HB_UChar16 *) scratch;
  unsigned int chars_len = 0;
  for (unsigned int i = 0; i < buffer->len; i++) {
    hb_codepoint_t c = buffer->info[i].codepoint;
    buffer->info[i].utf16_index() = chars_len;
    if (likely (c < 0x10000))
      pchars[chars_len++] = c;
    else if (unlikely (c >= 0x110000))
      pchars[chars_len++] = 0xFFFD;
    else {
      pchars[chars_len++] = 0xD800 + ((c - 0x10000) >> 10);
      pchars[chars_len++] = 0xDC00 + ((c - 0x10000) & ((1 << 10) - 1));
    }
  }


#define ALLOCATE_ARRAY(Type, name, len) \
  name = (Type *) scratch; \
  scratch += (len) * sizeof ((name)[0]); \
  scratch_size -= (len) * sizeof ((name)[0]);


  HB_ShaperItem item = {0};

  ALLOCATE_ARRAY (const HB_UChar16, item.string, chars_len);
  ALLOCATE_ARRAY (unsigned short, item.log_clusters, chars_len + 2);
  item.stringLength = chars_len;
  item.item.pos = 0;
  item.item.length = item.stringLength;
  item.item.script = hb_old_script_from_script (buffer->props.script);
  item.item.bidiLevel = backward ? 1 : 0;

  item.font = old_font;
  item.face = old_face;
  item.shaperFlags = 0;

  item.glyphIndicesPresent = false;

  /* TODO Alignment. */
  unsigned int num_glyphs = scratch_size / (sizeof (HB_Glyph) +
					    sizeof (HB_GlyphAttributes) +
					    sizeof (HB_Fixed) +
					    sizeof (HB_FixedPoint) +
					    sizeof (uint32_t));

  item.num_glyphs = num_glyphs;
  ALLOCATE_ARRAY (HB_Glyph, item.glyphs, num_glyphs);
  ALLOCATE_ARRAY (HB_GlyphAttributes, item.attributes, num_glyphs);
  ALLOCATE_ARRAY (HB_Fixed, item.advances, num_glyphs);
  ALLOCATE_ARRAY (HB_FixedPoint, item.offsets, num_glyphs);
  /* Apparently in some cases the offsets array will not be fully assigned to.
   * Clear it. */
  memset (item.offsets, 0, num_glyphs * sizeof (item.offsets[0]));
  uint32_t *vis_clusters;
  ALLOCATE_ARRAY (uint32_t, vis_clusters, num_glyphs);

#undef ALLOCATE_ARRAY

  if (!HB_ShapeItem (&item))
  {
    if (unlikely (item.num_glyphs > num_glyphs))
    {
      buffer->ensure (buffer->allocated * 2);
      if (buffer->in_error)
        return false;
      goto retry;
    }
    return false;
  }
  num_glyphs = item.num_glyphs;

  /* Ok, we've got everything we need, now compose output buffer,
   * very, *very*, carefully! */

  /* Calculate visual-clusters.  That's what we ship. */
  for (unsigned int i = 0; i < num_glyphs; i++)
    vis_clusters[i] = -1;
  for (unsigned int i = 0; i < buffer->len; i++) {
    uint32_t *p = &vis_clusters[item.log_clusters[buffer->info[i].utf16_index()]];
    *p = MIN (*p, buffer->info[i].cluster);
  }
  for (unsigned int i = 1; i < num_glyphs; i++)
    if (vis_clusters[i] == -1)
      vis_clusters[i] = vis_clusters[i - 1];

#undef utf16_index

  buffer->ensure (num_glyphs);
  if (buffer->in_error)
    return false;


  buffer->len = num_glyphs;
  hb_glyph_info_t *info = buffer->info;
  for (unsigned int i = 0; i < num_glyphs; i++)
  {
    info[i].codepoint = item.glyphs[i];
    info[i].cluster = vis_clusters[i];

    info[i].mask = item.advances[i];
    info[i].var1.u32 = item.offsets[i].x;
    info[i].var2.u32 = item.offsets[i].y;
  }

  buffer->clear_positions ();

  for (unsigned int i = 0; i < num_glyphs; ++i) {
    hb_glyph_info_t *info = &buffer->info[i];
    hb_glyph_position_t *pos = &buffer->pos[i];

    /* TODO vertical */
    pos->x_advance = info->mask;
    pos->x_offset = info->var1.u32;
    pos->y_offset = info->var2.u32;
  }

  if (HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
    buffer->reverse ();

  return true;
}
