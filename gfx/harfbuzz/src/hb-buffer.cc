/*
 * Copyright (C) 1998-2004  David Turner and Werner Lemberg
 * Copyright (C) 2004,2007,2009,2010  Red Hat, Inc.
 *
 * This is part of HarfBuzz, a text shaping library.
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
 * Red Hat Author(s): Owen Taylor, Behdad Esfahbod
 */

#include "hb-buffer-private.hh"

#include <string.h>

HB_BEGIN_DECLS


static hb_buffer_t _hb_buffer_nil = {
  HB_REFERENCE_COUNT_INVALID, /* ref_count */

  &_hb_unicode_funcs_nil  /* unicode */
};

/* Here is how the buffer works internally:
 *
 * There are two info pointers: info and out_info.  They always have
 * the same allocated size, but different lengths.
 *
 * As an optimization, both info and out_info may point to the
 * same piece of memory, which is owned by info.  This remains the
 * case as long as out_len doesn't exceed i at any time.
 * In that case, swap() is no-op and the glyph operations operate
 * mostly in-place.
 *
 * As soon as out_info gets longer than info, out_info is moved over
 * to an alternate buffer (which we reuse the pos buffer for!), and its
 * current contents (out_len entries) are copied to the new place.
 * This should all remain transparent to the user.  swap() then
 * switches info and out_info.
 */


static hb_bool_t
_hb_buffer_enlarge (hb_buffer_t *buffer, unsigned int size)
{
  if (unlikely (buffer->in_error))
    return FALSE;

  unsigned int new_allocated = buffer->allocated;
  hb_glyph_position_t *new_pos;
  hb_glyph_info_t *new_info;
  bool separate_out;

  separate_out = buffer->out_info != buffer->info;

  while (size > new_allocated)
    new_allocated += (new_allocated >> 1) + 8;

  new_pos = (hb_glyph_position_t *) realloc (buffer->pos, new_allocated * sizeof (buffer->pos[0]));
  new_info = (hb_glyph_info_t *) realloc (buffer->info, new_allocated * sizeof (buffer->info[0]));

  if (unlikely (!new_pos || !new_info))
    buffer->in_error = TRUE;

  if (likely (new_pos))
    buffer->pos = new_pos;

  if (likely (new_info))
    buffer->info = new_info;

  buffer->out_info = separate_out ? (hb_glyph_info_t *) buffer->pos : buffer->info;
  if (likely (!buffer->in_error))
    buffer->allocated = new_allocated;

  return likely (!buffer->in_error);
}

static inline hb_bool_t
_hb_buffer_ensure (hb_buffer_t *buffer, unsigned int size)
{
  return likely (size <= buffer->allocated) ? TRUE : _hb_buffer_enlarge (buffer, size);
}

static inline hb_bool_t
_hb_buffer_ensure_separate (hb_buffer_t *buffer, unsigned int size)
{
  if (unlikely (!_hb_buffer_ensure (buffer, size))) return FALSE;

  if (buffer->out_info == buffer->info)
  {
    assert (buffer->have_output);

    buffer->out_info = (hb_glyph_info_t *) buffer->pos;
    memcpy (buffer->out_info, buffer->info, buffer->out_len * sizeof (buffer->out_info[0]));
  }

  return TRUE;
}


/* Public API */

hb_buffer_t *
hb_buffer_create (unsigned int pre_alloc_size)
{
  hb_buffer_t *buffer;

  if (!HB_OBJECT_DO_CREATE (hb_buffer_t, buffer))
    return &_hb_buffer_nil;

  if (pre_alloc_size)
    _hb_buffer_ensure (buffer, pre_alloc_size);

  buffer->unicode = &_hb_unicode_funcs_nil;

  return buffer;
}

hb_buffer_t *
hb_buffer_reference (hb_buffer_t *buffer)
{
  HB_OBJECT_DO_REFERENCE (buffer);
}

unsigned int
hb_buffer_get_reference_count (hb_buffer_t *buffer)
{
  HB_OBJECT_DO_GET_REFERENCE_COUNT (buffer);
}

void
hb_buffer_destroy (hb_buffer_t *buffer)
{
  HB_OBJECT_DO_DESTROY (buffer);

  hb_unicode_funcs_destroy (buffer->unicode);

  free (buffer->info);
  free (buffer->pos);

  free (buffer);
}


void
hb_buffer_set_unicode_funcs (hb_buffer_t        *buffer,
			     hb_unicode_funcs_t *unicode)
{
  if (!unicode)
    unicode = &_hb_unicode_funcs_nil;

  hb_unicode_funcs_reference (unicode);
  hb_unicode_funcs_destroy (buffer->unicode);
  buffer->unicode = unicode;
}

hb_unicode_funcs_t *
hb_buffer_get_unicode_funcs (hb_buffer_t        *buffer)
{
  return buffer->unicode;
}

void
hb_buffer_set_direction (hb_buffer_t    *buffer,
			 hb_direction_t  direction)

{
  buffer->props.direction = direction;
}

hb_direction_t
hb_buffer_get_direction (hb_buffer_t    *buffer)
{
  return buffer->props.direction;
}

void
hb_buffer_set_script (hb_buffer_t *buffer,
		      hb_script_t  script)
{
  buffer->props.script = script;
}

hb_script_t
hb_buffer_get_script (hb_buffer_t *buffer)
{
  return buffer->props.script;
}

void
hb_buffer_set_language (hb_buffer_t   *buffer,
			hb_language_t  language)
{
  buffer->props.language = language;
}

hb_language_t
hb_buffer_get_language (hb_buffer_t *buffer)
{
  return buffer->props.language;
}


void
hb_buffer_clear (hb_buffer_t *buffer)
{
  buffer->have_output = FALSE;
  buffer->have_positions = FALSE;
  buffer->in_error = FALSE;
  buffer->len = 0;
  buffer->out_len = 0;
  buffer->i = 0;
  buffer->out_info = buffer->info;
  buffer->serial = 0;
}

hb_bool_t
hb_buffer_ensure (hb_buffer_t *buffer, unsigned int size)
{
  return _hb_buffer_ensure (buffer, size);
}

void
hb_buffer_add_glyph (hb_buffer_t    *buffer,
		     hb_codepoint_t  codepoint,
		     hb_mask_t       mask,
		     unsigned int    cluster)
{
  hb_glyph_info_t *glyph;

  if (unlikely (!_hb_buffer_ensure (buffer, buffer->len + 1))) return;

  glyph = &buffer->info[buffer->len];

  memset (glyph, 0, sizeof (*glyph));
  glyph->codepoint = codepoint;
  glyph->mask = mask;
  glyph->cluster = cluster;

  buffer->len++;
}

void
hb_buffer_clear_positions (hb_buffer_t *buffer)
{
  _hb_buffer_clear_output (buffer);
  buffer->have_output = FALSE;
  buffer->have_positions = TRUE;

  if (unlikely (!buffer->pos))
  {
    buffer->pos = (hb_glyph_position_t *) calloc (buffer->allocated, sizeof (buffer->pos[0]));
    return;
  }

  memset (buffer->pos, 0, sizeof (buffer->pos[0]) * buffer->len);
}

/* HarfBuzz-Internal API */

void
_hb_buffer_clear_output (hb_buffer_t *buffer)
{
  buffer->have_output = TRUE;
  buffer->have_positions = FALSE;
  buffer->out_len = 0;
  buffer->out_info = buffer->info;
}

void
_hb_buffer_swap (hb_buffer_t *buffer)
{
  unsigned int tmp;

  assert (buffer->have_output);

  if (unlikely (buffer->in_error)) return;

  if (buffer->out_info != buffer->info)
  {
    hb_glyph_info_t *tmp_string;
    tmp_string = buffer->info;
    buffer->info = buffer->out_info;
    buffer->out_info = tmp_string;
    buffer->pos = (hb_glyph_position_t *) buffer->out_info;
  }

  tmp = buffer->len;
  buffer->len = buffer->out_len;
  buffer->out_len = tmp;

  buffer->i = 0;
}

void
_hb_buffer_replace_glyphs_be16 (hb_buffer_t *buffer,
				unsigned int num_in,
				unsigned int num_out,
				const uint16_t *glyph_data_be)
{
  if (buffer->out_info != buffer->info ||
      buffer->out_len + num_out > buffer->i + num_in)
  {
    if (unlikely (!_hb_buffer_ensure_separate (buffer, buffer->out_len + num_out)))
      return;
  }

  hb_glyph_info_t orig_info = buffer->info[buffer->i];

  for (unsigned int i = 0; i < num_out; i++)
  {
    hb_glyph_info_t *info = &buffer->out_info[buffer->out_len + i];
    *info = orig_info;
    info->codepoint = hb_be_uint16 (glyph_data_be[i]);
  }

  buffer->i  += num_in;
  buffer->out_len += num_out;
}

void
_hb_buffer_replace_glyph (hb_buffer_t *buffer,
			  hb_codepoint_t glyph_index)
{
  hb_glyph_info_t *info;

  if (buffer->out_info != buffer->info)
  {
    if (unlikely (!_hb_buffer_ensure (buffer, buffer->out_len + 1))) return;
    buffer->out_info[buffer->out_len] = buffer->info[buffer->i];
  }
  else if (buffer->out_len != buffer->i)
    buffer->out_info[buffer->out_len] = buffer->info[buffer->i];

  info = &buffer->out_info[buffer->out_len];
  info->codepoint = glyph_index;

  buffer->i++;
  buffer->out_len++;
}

void
_hb_buffer_next_glyph (hb_buffer_t *buffer)
{
  if (buffer->have_output)
  {
    if (buffer->out_info != buffer->info)
    {
      if (unlikely (!_hb_buffer_ensure (buffer, buffer->out_len + 1))) return;
      buffer->out_info[buffer->out_len] = buffer->info[buffer->i];
    }
    else if (buffer->out_len != buffer->i)
      buffer->out_info[buffer->out_len] = buffer->info[buffer->i];

    buffer->out_len++;
  }

  buffer->i++;
}

void
_hb_buffer_reset_masks (hb_buffer_t *buffer,
			hb_mask_t    mask)
{
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    buffer->info[i].mask = mask;
}

void
_hb_buffer_add_masks (hb_buffer_t *buffer,
		      hb_mask_t    mask)
{
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    buffer->info[i].mask |= mask;
}

void
_hb_buffer_set_masks (hb_buffer_t *buffer,
		      hb_mask_t    value,
		      hb_mask_t    mask,
		      unsigned int cluster_start,
		      unsigned int cluster_end)
{
  hb_mask_t not_mask = ~mask;
  value &= mask;

  if (!mask)
    return;

  if (cluster_start == 0 && cluster_end == (unsigned int)-1) {
    unsigned int count = buffer->len;
    for (unsigned int i = 0; i < count; i++)
      buffer->info[i].mask = (buffer->info[i].mask & not_mask) | value;
    return;
  }

  /* XXX can't bsearch since .cluster may not be sorted. */
  /* Binary search to find the start position and go from there. */
  unsigned int min = 0, max = buffer->len;
  while (min < max)
  {
    unsigned int mid = min + ((max - min) / 2);
    if (buffer->info[mid].cluster < cluster_start)
      min = mid + 1;
    else
      max = mid;
  }
  unsigned int count = buffer->len;
  for (unsigned int i = min; i < count && buffer->info[i].cluster < cluster_end; i++)
    buffer->info[i].mask = (buffer->info[i].mask & not_mask) | value;
}


/* Public API again */

unsigned int
hb_buffer_get_length (hb_buffer_t *buffer)
{
  return buffer->len;
}

/* Return value valid as long as buffer not modified */
hb_glyph_info_t *
hb_buffer_get_glyph_infos (hb_buffer_t *buffer)
{
  return (hb_glyph_info_t *) buffer->info;
}

/* Return value valid as long as buffer not modified */
hb_glyph_position_t *
hb_buffer_get_glyph_positions (hb_buffer_t *buffer)
{
  if (!buffer->have_positions)
    hb_buffer_clear_positions (buffer);

  return (hb_glyph_position_t *) buffer->pos;
}


static void
reverse_range (hb_buffer_t *buffer,
	       unsigned int start,
	       unsigned int end)
{
  hb_glyph_info_t *i = buffer->info + start;
  hb_glyph_info_t *j = buffer->info + end - 1;
  while (i < j) {
    hb_glyph_info_t t = *i;
    *i++ = *j;
    *j-- = t;
  }

  if (buffer->pos) {
    hb_glyph_position_t *ii = buffer->pos + start;
    hb_glyph_position_t *jj = buffer->pos + end - 1;
    while (ii < jj) {
      hb_glyph_position_t tt = *ii;
      *ii++ = *jj;
      *jj-- = tt;
    }
  }
}

void
hb_buffer_reverse (hb_buffer_t *buffer)
{
  if (unlikely (!buffer->len))
    return;

  reverse_range (buffer, 0, buffer->len);
}

void
hb_buffer_reverse_clusters (hb_buffer_t *buffer)
{
  unsigned int i, start, count, last_cluster;

  if (unlikely (!buffer->len))
    return;

  hb_buffer_reverse (buffer);

  count = buffer->len;
  start = 0;
  last_cluster = buffer->info[0].cluster;
  for (i = 1; i < count; i++) {
    if (last_cluster != buffer->info[i].cluster) {
      reverse_range (buffer, start, i);
      start = i;
      last_cluster = buffer->info[i].cluster;
    }
  }
  reverse_range (buffer, start, i);
}


#define ADD_UTF(T) \
	HB_STMT_START { \
	  const T *next = (const T *) text + item_offset; \
	  const T *end = next + item_length; \
	  while (next < end) { \
	    hb_codepoint_t u; \
	    const T *old_next = next; \
	    next = UTF_NEXT (next, end, u); \
	    hb_buffer_add_glyph (buffer, u, 1,  old_next - (const T *) text); \
	  } \
	} HB_STMT_END


#define UTF8_COMPUTE(Char, Mask, Len) \
  if (Char < 128) { Len = 1; Mask = 0x7f; } \
  else if ((Char & 0xe0) == 0xc0) { Len = 2; Mask = 0x1f; } \
  else if ((Char & 0xf0) == 0xe0) { Len = 3; Mask = 0x0f; } \
  else if ((Char & 0xf8) == 0xf0) { Len = 4; Mask = 0x07; } \
  else Len = 0;

static inline const uint8_t *
hb_utf8_next (const uint8_t *text,
	      const uint8_t *end,
	      hb_codepoint_t *unicode)
{
  uint8_t c = *text;
  unsigned int mask, len;

  /* TODO check for overlong sequences?  also: optimize? */

  UTF8_COMPUTE (c, mask, len);
  if (unlikely (!len || (unsigned int) (end - text) < len)) {
    *unicode = -1;
    return text + 1;
  } else {
    hb_codepoint_t result;
    unsigned int i;
    result = c & mask;
    for (i = 1; i < len; i++)
      {
	if (unlikely ((text[i] & 0xc0) != 0x80))
	  {
	    *unicode = -1;
	    return text + 1;
	  }
	result <<= 6;
	result |= (text[i] & 0x3f);
      }
    *unicode = result;
    return text + len;
  }
}

void
hb_buffer_add_utf8 (hb_buffer_t  *buffer,
		    const char   *text,
		    unsigned int  text_length HB_UNUSED,
		    unsigned int  item_offset,
		    unsigned int  item_length)
{
#define UTF_NEXT(S, E, U)	hb_utf8_next (S, E, &(U))
  ADD_UTF (uint8_t);
#undef UTF_NEXT
}

static inline const uint16_t *
hb_utf16_next (const uint16_t *text,
	       const uint16_t *end,
	       hb_codepoint_t *unicode)
{
  uint16_t c = *text++;

  if (unlikely (c >= 0xd800 && c < 0xdc00)) {
    /* high surrogate */
    uint16_t l;
    if (text < end && ((l = *text), unlikely (l >= 0xdc00 && l < 0xe000))) {
      /* low surrogate */
      *unicode = ((hb_codepoint_t) ((c) - 0xd800) * 0x400 + (l) - 0xdc00 + 0x10000);
       text++;
    } else
      *unicode = -1;
  } else
    *unicode = c;

  return text;
}

void
hb_buffer_add_utf16 (hb_buffer_t    *buffer,
		     const uint16_t *text,
		     unsigned int    text_length HB_UNUSED,
		     unsigned int    item_offset,
		     unsigned int    item_length)
{
#define UTF_NEXT(S, E, U)	hb_utf16_next (S, E, &(U))
  ADD_UTF (uint16_t);
#undef UTF_NEXT
}

void
hb_buffer_add_utf32 (hb_buffer_t    *buffer,
		     const uint32_t *text,
		     unsigned int    text_length HB_UNUSED,
		     unsigned int    item_offset,
		     unsigned int    item_length)
{
#define UTF_NEXT(S, E, U)	((U) = *(S), (S)+1)
  ADD_UTF (uint32_t);
#undef UTF_NEXT
}


HB_END_DECLS
