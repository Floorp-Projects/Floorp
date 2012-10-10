/*
 * Copyright © 2011,2012  Google, Inc.
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

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#define HB_SHAPER uniscribe
#include "hb-shaper-impl-private.hh"

#include <windows.h>
#include <usp10.h>

#include "hb-uniscribe.h"

#include "hb-ot-name-table.hh"
#include "hb-ot-tag.h"


#ifndef HB_DEBUG_UNISCRIBE
#define HB_DEBUG_UNISCRIBE (HB_DEBUG+0)
#endif


/*
DWORD GetFontData(
  __in   HDC hdc,
  __in   DWORD dwTable,
  __in   DWORD dwOffset,
  __out  LPVOID lpvBuffer,
  __in   DWORD cbData
);
*/


HB_SHAPER_DATA_ENSURE_DECLARE(uniscribe, face)
HB_SHAPER_DATA_ENSURE_DECLARE(uniscribe, font)


/*
 * shaper face data
 */

struct hb_uniscribe_shaper_face_data_t {
  HANDLE fh;
};

hb_uniscribe_shaper_face_data_t *
_hb_uniscribe_shaper_face_data_create (hb_face_t *face)
{
  hb_uniscribe_shaper_face_data_t *data = (hb_uniscribe_shaper_face_data_t *) calloc (1, sizeof (hb_uniscribe_shaper_face_data_t));
  if (unlikely (!data))
    return NULL;

  hb_blob_t *blob = hb_face_reference_blob (face);
  unsigned int blob_length;
  const char *blob_data = hb_blob_get_data (blob, &blob_length);
  if (unlikely (!blob_length))
    DEBUG_MSG (UNISCRIBE, face, "Face has empty blob");

  DWORD num_fonts_installed;
  data->fh = AddFontMemResourceEx ((void *) blob_data, blob_length, 0, &num_fonts_installed);
  hb_blob_destroy (blob);
  if (unlikely (!data->fh)) {
    DEBUG_MSG (UNISCRIBE, face, "Face AddFontMemResourceEx() failed");
    free (data);
    return NULL;
  }

  return data;
}

void
_hb_uniscribe_shaper_face_data_destroy (hb_uniscribe_shaper_face_data_t *data)
{
  RemoveFontMemResourceEx (data->fh);
  free (data);
}


/*
 * shaper font data
 */

struct hb_uniscribe_shaper_font_data_t {
  HDC hdc;
  LOGFONTW log_font;
  HFONT hfont;
  SCRIPT_CACHE script_cache;
};

static bool
populate_log_font (LOGFONTW  *lf,
		   hb_font_t *font)
{
  memset (lf, 0, sizeof (*lf));
  lf->lfHeight = -font->y_scale;
  lf->lfCharSet = DEFAULT_CHARSET;

  hb_blob_t *blob = OT::Sanitizer<OT::name>::sanitize (hb_face_reference_table (font->face, HB_TAG ('n','a','m','e')));
  const OT::name *name_table = OT::Sanitizer<OT::name>::lock_instance (blob);
  unsigned int len = name_table->get_name (3, 1, 0x409, 4,
					   lf->lfFaceName,
					   sizeof (lf->lfFaceName[0]) * LF_FACESIZE)
					  / sizeof (lf->lfFaceName[0]);
  hb_blob_destroy (blob);

  if (unlikely (!len)) {
    DEBUG_MSG (UNISCRIBE, NULL, "Didn't find English name table entry");
    return false;
  }
  if (unlikely (len >= LF_FACESIZE)) {
    DEBUG_MSG (UNISCRIBE, NULL, "Font name too long");
    return false;
  }

  for (unsigned int i = 0; i < len; i++)
    lf->lfFaceName[i] = hb_be_uint16 (lf->lfFaceName[i]);
  lf->lfFaceName[len] = 0;

  return true;
}

hb_uniscribe_shaper_font_data_t *
_hb_uniscribe_shaper_font_data_create (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_face_data_ensure (font->face))) return NULL;

  hb_uniscribe_shaper_font_data_t *data = (hb_uniscribe_shaper_font_data_t *) calloc (1, sizeof (hb_uniscribe_shaper_font_data_t));
  if (unlikely (!data))
    return NULL;

  data->hdc = GetDC (NULL);

  if (unlikely (!populate_log_font (&data->log_font, font))) {
    DEBUG_MSG (UNISCRIBE, font, "Font populate_log_font() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
    return NULL;
  }

  data->hfont = CreateFontIndirectW (&data->log_font);
  if (unlikely (!data->hfont)) {
    DEBUG_MSG (UNISCRIBE, font, "Font CreateFontIndirectW() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
     return NULL;
  }

  if (!SelectObject (data->hdc, data->hfont)) {
    DEBUG_MSG (UNISCRIBE, font, "Font SelectObject() failed");
    _hb_uniscribe_shaper_font_data_destroy (data);
     return NULL;
  }

  return data;
}

void
_hb_uniscribe_shaper_font_data_destroy (hb_uniscribe_shaper_font_data_t *data)
{
  if (data->hdc)
    ReleaseDC (NULL, data->hdc);
  if (data->hfont)
    DeleteObject (data->hfont);
  if (data->script_cache)
    ScriptFreeCache (&data->script_cache);
  free (data);
}


/*
 * shaper shape_plan data
 */

struct hb_uniscribe_shaper_shape_plan_data_t {};

hb_uniscribe_shaper_shape_plan_data_t *
_hb_uniscribe_shaper_shape_plan_data_create (hb_shape_plan_t    *shape_plan HB_UNUSED,
					     const hb_feature_t *user_features HB_UNUSED,
					     unsigned int        num_user_features HB_UNUSED)
{
  return (hb_uniscribe_shaper_shape_plan_data_t *) HB_SHAPER_DATA_SUCCEEDED;
}

void
_hb_uniscribe_shaper_shape_plan_data_destroy (hb_uniscribe_shaper_shape_plan_data_t *data HB_UNUSED)
{
}


/*
 * shaper
 */

LOGFONTW *
hb_uniscribe_font_get_logfontw (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_font_data_ensure (font))) return NULL;
    return NULL;
  hb_uniscribe_shaper_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return &font_data->log_font;
}

HFONT
hb_uniscribe_font_get_hfont (hb_font_t *font)
{
  if (unlikely (!hb_uniscribe_shaper_font_data_ensure (font))) return NULL;
  hb_uniscribe_shaper_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return font_data->hfont;
}


hb_bool_t
_hb_uniscribe_shape (hb_shape_plan_t    *shape_plan,
		     hb_font_t          *font,
		     hb_buffer_t        *buffer,
		     const hb_feature_t *features,
		     unsigned int        num_features)
{
  hb_face_t *face = font->face;
  hb_uniscribe_shaper_face_data_t *face_data = HB_SHAPER_DATA_GET (face);
  hb_uniscribe_shaper_font_data_t *font_data = HB_SHAPER_DATA_GET (font);

#define FAIL(...) \
  HB_STMT_START { \
    DEBUG_MSG (UNISCRIBE, NULL, __VA_ARGS__); \
    return false; \
  } HB_STMT_END;

  HRESULT hr;

retry:

  unsigned int scratch_size;
  char *scratch = (char *) buffer->get_scratch_buffer (&scratch_size);

  /* Allocate char buffers; they all fit */

#define ALLOCATE_ARRAY(Type, name, len) \
  Type *name = (Type *) scratch; \
  scratch += (len) * sizeof ((name)[0]); \
  scratch_size -= (len) * sizeof ((name)[0]);

#define utf16_index() var1.u32

  WCHAR *pchars = (WCHAR *) scratch;
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

  ALLOCATE_ARRAY (WCHAR, wchars, chars_len);
  ALLOCATE_ARRAY (WORD, log_clusters, chars_len);
  ALLOCATE_ARRAY (SCRIPT_CHARPROP, char_props, chars_len);

  /* On Windows, we don't care about alignment...*/
  unsigned int glyphs_size = scratch_size / (sizeof (WORD) +
					     sizeof (SCRIPT_GLYPHPROP) +
					     sizeof (int) +
					     sizeof (GOFFSET) +
					     sizeof (uint32_t));

  ALLOCATE_ARRAY (WORD, glyphs, glyphs_size);
  ALLOCATE_ARRAY (SCRIPT_GLYPHPROP, glyph_props, glyphs_size);
  ALLOCATE_ARRAY (int, advances, glyphs_size);
  ALLOCATE_ARRAY (GOFFSET, offsets, glyphs_size);
  ALLOCATE_ARRAY (uint32_t, vis_clusters, glyphs_size);

#undef ALLOCATE_ARRAY

#define MAX_ITEMS 256

  SCRIPT_ITEM items[MAX_ITEMS + 1];
  SCRIPT_CONTROL bidi_control = {0};
  SCRIPT_STATE bidi_state = {0};
  ULONG script_tags[MAX_ITEMS];
  int item_count;

  /* MinGW32 doesn't define fMergeNeutralItems, so we bruteforce */
  //bidi_control.fMergeNeutralItems = true;
  *(uint32_t*)&bidi_control |= 1<<24;

  bidi_state.uBidiLevel = HB_DIRECTION_IS_FORWARD (buffer->props.direction) ? 0 : 1;
  bidi_state.fOverrideDirection = 1;

  hr = ScriptItemizeOpenType (wchars,
			      chars_len,
			      MAX_ITEMS,
			      &bidi_control,
			      &bidi_state,
			      items,
			      script_tags,
			      &item_count);
  if (unlikely (FAILED (hr)))
    FAIL ("ScriptItemizeOpenType() failed: 0x%08xL", hr);

#undef MAX_ITEMS

  int *range_char_counts = NULL;
  TEXTRANGE_PROPERTIES **range_properties = NULL;
  int range_count = 0;
  if (num_features) {
    /* TODO setup ranges */
  }

  OPENTYPE_TAG language_tag = hb_uint32_swap (hb_ot_tag_from_language (buffer->props.language));

  unsigned int glyphs_offset = 0;
  unsigned int glyphs_len;
  bool backward = HB_DIRECTION_IS_BACKWARD (buffer->props.direction);
  for (unsigned int j = 0; j < item_count; j++)
  {
    unsigned int i = backward ? item_count - 1 - j : j;
    unsigned int chars_offset = items[i].iCharPos;
    unsigned int item_chars_len = items[i + 1].iCharPos - chars_offset;

  retry_shape:
    hr = ScriptShapeOpenType (font_data->hdc,
			      &font_data->script_cache,
			      &items[i].a,
			      script_tags[i],
			      language_tag,
			      range_char_counts,
			      range_properties,
			      range_count,
			      wchars + chars_offset,
			      item_chars_len,
			      glyphs_size - glyphs_offset,
			      /* out */
			      log_clusters + chars_offset,
			      char_props + chars_offset,
			      glyphs + glyphs_offset,
			      glyph_props + glyphs_offset,
			      (int *) &glyphs_len);

    if (unlikely (items[i].a.fNoGlyphIndex))
      FAIL ("ScriptShapeOpenType() set fNoGlyphIndex");
    if (unlikely (hr == E_OUTOFMEMORY))
    {
      buffer->ensure (buffer->allocated * 2);
      if (buffer->in_error)
	FAIL ("Buffer resize failed");
      goto retry;
    }
    if (unlikely (hr == USP_E_SCRIPT_NOT_IN_FONT))
    {
      if (items[i].a.eScript == SCRIPT_UNDEFINED)
	FAIL ("ScriptShapeOpenType() failed: Font doesn't support script");
      items[i].a.eScript = SCRIPT_UNDEFINED;
      goto retry_shape;
    }
    if (unlikely (FAILED (hr)))
    {
      FAIL ("ScriptShapeOpenType() failed: 0x%08xL", hr);
    }

    for (unsigned int j = chars_offset; j < chars_offset + item_chars_len; j++)
      log_clusters[j] += glyphs_offset;

    hr = ScriptPlaceOpenType (font_data->hdc,
			      &font_data->script_cache,
			      &items[i].a,
			      script_tags[i],
			      language_tag,
			      range_char_counts,
			      range_properties,
			      range_count,
			      wchars + chars_offset,
			      log_clusters + chars_offset,
			      char_props + chars_offset,
			      item_chars_len,
			      glyphs + glyphs_offset,
			      glyph_props + glyphs_offset,
			      glyphs_len,
			      /* out */
			      advances + glyphs_offset,
			      offsets + glyphs_offset,
			      NULL);
    if (unlikely (FAILED (hr)))
      FAIL ("ScriptPlaceOpenType() failed: 0x%08xL", hr);

    glyphs_offset += glyphs_len;
  }
  glyphs_len = glyphs_offset;

  /* Ok, we've got everything we need, now compose output buffer,
   * very, *very*, carefully! */

  /* Calculate visual-clusters.  That's what we ship. */
  for (unsigned int i = 0; i < glyphs_len; i++)
    vis_clusters[i] = -1;
  for (unsigned int i = 0; i < buffer->len; i++) {
    uint32_t *p = &vis_clusters[log_clusters[buffer->info[i].utf16_index()]];
    *p = MIN (*p, buffer->info[i].cluster);
  }
  if (!backward) {
    for (unsigned int i = 1; i < glyphs_len; i++)
      if (vis_clusters[i] == -1)
	vis_clusters[i] = vis_clusters[i - 1];
  } else {
    for (int i = glyphs_len - 2; i >= 0; i--)
      if (vis_clusters[i] == -1)
	vis_clusters[i] = vis_clusters[i + 1];
  }

#undef utf16_index

  buffer->ensure (glyphs_len);
  if (buffer->in_error)
    FAIL ("Buffer in error");

#undef FAIL

  /* Set glyph infos */
  buffer->len = 0;
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[buffer->len++];

    info->codepoint = glyphs[i];
    info->cluster = vis_clusters[i];

    /* The rest is crap.  Let's store position info there for now. */
    info->mask = advances[i];
    info->var1.u32 = offsets[i].du;
    info->var2.u32 = offsets[i].dv;
  }

  /* Set glyph positions */
  buffer->clear_positions ();
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[i];
    hb_glyph_position_t *pos = &buffer->pos[i];

    /* TODO vertical */
    pos->x_advance = info->mask;
    pos->x_offset = info->var1.u32;
    pos->y_offset = info->var2.u32;
  }

  /* Wow, done! */
  return true;
}


