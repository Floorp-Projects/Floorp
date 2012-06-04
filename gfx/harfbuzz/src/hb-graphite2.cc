/*
 * Copyright © 2011  Martin Hosken
 * Copyright © 2011  SIL International
 * Copyright © 2011  Google, Inc.
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

#include "hb-private.hh"

#include "hb-graphite2.h"

#include "hb-buffer-private.hh"
#include "hb-font-private.hh"
#include "hb-ot-tag.h"

#include <graphite2/Font.h>
#include <graphite2/Segment.h>


struct hb_gr_cluster_t {
  unsigned int base_char;
  unsigned int num_chars;
  unsigned int base_glyph;
  unsigned int num_glyphs;
};


typedef struct hb_gr_tablelist_t {
  hb_blob_t   *blob;
  struct hb_gr_tablelist_t *next;
  unsigned int tag;
} hb_gr_tablelist_t;

static struct hb_gr_face_data_t {
  hb_face_t         *face;
  gr_face           *grface;
  hb_gr_tablelist_t *tlist;
} _hb_gr_face_data_nil = {NULL, NULL};

static struct hb_gr_font_data_t {
  gr_font   *grfont;
  gr_face   *grface;
} _hb_gr_font_data_nil = {NULL, NULL};


static const void *hb_gr_get_table (const void *data, unsigned int tag, size_t *len)
{
  hb_gr_tablelist_t *pl = NULL, *p;
  hb_gr_face_data_t *face = (hb_gr_face_data_t *) data;
  hb_gr_tablelist_t *tlist = face->tlist;

  for (p = tlist; p; p = p->next)
    if (p->tag == tag ) {
      unsigned int tlen;
      const char *d = hb_blob_get_data (p->blob, &tlen);
      *len = tlen;
      return d;
    } else
      pl = p;

  if (!face->face)
    return NULL;
  hb_blob_t *blob = hb_face_reference_table (face->face, tag);

  if (!pl || pl->blob)
  {
    p = (hb_gr_tablelist_t *) malloc (sizeof (hb_gr_tablelist_t));
    if (!p) {
      hb_blob_destroy (blob);
      return NULL;
    }
    p->next = NULL;
    if (pl)
      pl->next = p;
    else
      face->tlist = p;
    pl = p;
  }
  pl->blob = blob;
  pl->tag = tag;

  unsigned int tlen;
  const char *d = hb_blob_get_data (blob, &tlen);
  *len = tlen;
  return d;
}

static float hb_gr_get_advance (const void *hb_font, unsigned short gid)
{
  return hb_font_get_glyph_h_advance ((hb_font_t *) hb_font, gid);
}

static void _hb_gr_face_data_destroy (void *data)
{
  hb_gr_face_data_t *f = (hb_gr_face_data_t *) data;
  hb_gr_tablelist_t *tlist = f->tlist;
  while (tlist)
  {
    hb_gr_tablelist_t *old = tlist;
    hb_blob_destroy (tlist->blob);
    tlist = tlist->next;
    free (old);
  }
  gr_face_destroy (f->grface);
}

static void _hb_gr_font_data_destroy (void *data)
{
  hb_gr_font_data_t *f = (hb_gr_font_data_t *) data;

  gr_font_destroy (f->grfont);
  free (f);
}

static hb_user_data_key_t hb_gr_data_key;

static hb_gr_face_data_t *
_hb_gr_face_get_data (hb_face_t *face)
{
  hb_gr_face_data_t *data = (hb_gr_face_data_t *) hb_face_get_user_data (face, &hb_gr_data_key);
  if (likely (data)) return data;

  data = (hb_gr_face_data_t *) calloc (1, sizeof (hb_gr_face_data_t));
  if (unlikely (!data))
    return &_hb_gr_face_data_nil;


  hb_blob_t *silf_blob = hb_face_reference_table (face, HB_GRAPHITE_TAG_Silf);
  if (!hb_blob_get_length (silf_blob))
  {
    hb_blob_destroy (silf_blob);
    return &_hb_gr_face_data_nil;
  }

  data->face = face;
  data->grface = gr_make_face (data, &hb_gr_get_table, gr_face_default);


  if (unlikely (!hb_face_set_user_data (face, &hb_gr_data_key, data,
					(hb_destroy_func_t) _hb_gr_face_data_destroy,
					FALSE)))
  {
    _hb_gr_face_data_destroy (data);
    data = (hb_gr_face_data_t *) hb_face_get_user_data (face, &hb_gr_data_key);
    if (data)
      return data;
    else
      return &_hb_gr_face_data_nil;
  }

  return data;
}

static hb_gr_font_data_t *
_hb_gr_font_get_data (hb_font_t *font)
{
  hb_gr_font_data_t *data = (hb_gr_font_data_t *) hb_font_get_user_data (font, &hb_gr_data_key);
  if (likely (data)) return data;

  data = (hb_gr_font_data_t *) calloc (1, sizeof (hb_gr_font_data_t));
  if (unlikely (!data))
    return &_hb_gr_font_data_nil;


  hb_blob_t *silf_blob = hb_face_reference_table (font->face, HB_GRAPHITE_TAG_Silf);
  if (!hb_blob_get_length (silf_blob))
  {
    hb_blob_destroy (silf_blob);
    return &_hb_gr_font_data_nil;
  }

  data->grface = _hb_gr_face_get_data (font->face)->grface;
  int scale;
  hb_font_get_scale (font, &scale, NULL);
  data->grfont = gr_make_font_with_advance_fn (scale, font, &hb_gr_get_advance, data->grface);


  if (unlikely (!hb_font_set_user_data (font, &hb_gr_data_key, data,
					(hb_destroy_func_t) _hb_gr_font_data_destroy,
					FALSE)))
  {
    _hb_gr_font_data_destroy (data);
    data = (hb_gr_font_data_t *) hb_font_get_user_data (font, &hb_gr_data_key);
    if (data)
      return data;
    else
      return &_hb_gr_font_data_nil;
  }

  return data;
}


hb_bool_t
_hb_graphite_shape (hb_font_t          *font,
		   hb_buffer_t        *buffer,
		   const hb_feature_t *features,
		   unsigned int        num_features)
{

  buffer->guess_properties ();

  /* XXX We do a hell of a lot of stuff just to figure out this font
   * is not graphite!  Shouldn't do. */

  hb_gr_font_data_t *data = _hb_gr_font_get_data (font);
  if (!data->grface) return FALSE;

  unsigned int charlen;
  hb_glyph_info_t *bufferi = hb_buffer_get_glyph_infos (buffer, &charlen);

  int success = 0;

  if (!charlen) return TRUE;

  const char *lang = hb_language_to_string (hb_buffer_get_language (buffer));
  const char *lang_end = strchr (lang, '-');
  int lang_len = lang_end ? lang_end - lang : -1;
  gr_feature_val *feats = gr_face_featureval_for_lang (data->grface, lang ? hb_tag_from_string (lang, lang_len) : 0);

  while (num_features--)
  {
    const gr_feature_ref *fref = gr_face_find_fref (data->grface, features->tag);
    if (fref)
      gr_fref_set_feature_value (fref, features->value, feats);
    features++;
  }

  hb_codepoint_t *gids = NULL, *pg;
  hb_gr_cluster_t *clusters = NULL;
  gr_segment *seg = NULL;
  uint32_t *text = NULL;
  const gr_slot *is;
  unsigned int ci = 0, ic = 0;
  float curradvx = 0., curradvy = 0.;
  unsigned int glyphlen = 0;
  unsigned int *p;

  text = (uint32_t *) malloc ((charlen + 1) * sizeof (uint32_t));
  if (!text) goto dieout;

  p = text;
  for (unsigned int i = 0; i < charlen; ++i)
    *p++ = bufferi++->codepoint;
  *p = 0;

  hb_tag_t script_tag[2];
  hb_ot_tags_from_script (hb_buffer_get_script (buffer), &script_tag[0], &script_tag[1]);

  seg = gr_make_seg (data->grfont, data->grface,
		     script_tag[1] == HB_TAG_NONE ? script_tag[0] : script_tag[1],
		     feats,
		     gr_utf32, text, charlen,
		     2 | (hb_buffer_get_direction (buffer) == HB_DIRECTION_RTL ? 1 : 0));
  if (!seg) goto dieout;

  glyphlen = gr_seg_n_slots (seg);
  clusters = (hb_gr_cluster_t *) calloc (charlen, sizeof (hb_gr_cluster_t));
  if (!glyphlen || !clusters) goto dieout;

  gids = (hb_codepoint_t *) malloc (glyphlen * sizeof (hb_codepoint_t));
  if (!gids) goto dieout;

  pg = gids;
  for (is = gr_seg_first_slot (seg), ic = 0; is; is = gr_slot_next_in_segment (is), ic++)
  {
    unsigned int before = gr_slot_before (is);
    unsigned int after = gr_slot_after (is);
    *pg = gr_slot_gid (is);
    pg++;
    while (clusters[ci].base_char > before && ci)
    {
      clusters[ci-1].num_chars += clusters[ci].num_chars;
      clusters[ci-1].num_glyphs += clusters[ci].num_glyphs;
      ci--;
    }

    if (gr_slot_can_insert_before (is) && clusters[ci].num_chars && before >= clusters[ci].base_char + clusters[ci].num_chars)
    {
      hb_gr_cluster_t *c = clusters + ci + 1;
      c->base_char = clusters[ci].base_char + clusters[ci].num_chars;
      c->num_chars = before - c->base_char;
      c->base_glyph = ic;
      c->num_glyphs = 0;
      ci++;
    }
    clusters[ci].num_glyphs++;

    if (clusters[ci].base_char + clusters[ci].num_chars < after + 1)
	clusters[ci].num_chars = after + 1 - clusters[ci].base_char;
  }
  ci++;

  buffer->clear_output ();
  for (unsigned int i = 0; i < ci; ++i)
    buffer->replace_glyphs (clusters[i].num_chars, clusters[i].num_glyphs, gids + clusters[i].base_glyph);
  buffer->swap_buffers ();

  hb_glyph_position_t *pPos;
  for (pPos = hb_buffer_get_glyph_positions (buffer, NULL), is = gr_seg_first_slot (seg);
       is; pPos++, is = gr_slot_next_in_segment (is))
  {
    pPos->x_offset = gr_slot_origin_X(is) - curradvx;
    pPos->y_offset = gr_slot_origin_Y(is) - curradvy;
    pPos->x_advance = gr_slot_advance_X(is, data->grface, data->grfont);
    pPos->y_advance = gr_slot_advance_Y(is, data->grface, data->grfont);
//    if (pPos->x_advance < 0 && gr_slot_attached_to(is))
//      pPos->x_advance = 0;
    curradvx += pPos->x_advance;
    curradvy += pPos->y_advance;
  }
  pPos[-1].x_advance += gr_seg_advance_X(seg) - curradvx;

  /* TODO(behdad):
   * This shaper is badly broken with RTL text.  It returns glyphs
   * in the logical order!
   */
//  if (HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
//    hb_buffer_reverse (buffer);

  success = 1;

dieout:
  if (gids) free (gids);
  if (clusters) free (clusters);
  if (seg) gr_seg_destroy (seg);
  if (text) free (text);
  return success;
}
