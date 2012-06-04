/*
 * Copyright © 2009  Red Hat, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 */

#include "hb-private.hh"

#include "hb-shape.h"

#include "hb-buffer-private.hh"

#ifdef HAVE_GRAPHITE
#include "hb-graphite2-private.hh"
#endif
#ifdef HAVE_UNISCRIBE
# include "hb-uniscribe-private.hh"
#endif
#ifdef HAVE_OT
# include "hb-ot-shape-private.hh"
#endif
#include "hb-fallback-shape-private.hh"

typedef hb_bool_t (*hb_shape_func_t) (hb_font_t          *font,
				      hb_buffer_t        *buffer,
				      const hb_feature_t *features,
				      unsigned int        num_features);

#define HB_SHAPER_IMPLEMENT(name) {#name, _hb_##name##_shape}
static struct hb_shaper_pair_t {
  char name[16];
  hb_shape_func_t func;
} shapers[] = {
  /* v--- Add new shapers in the right place here */
#ifdef HAVE_GRAPHITE
  HB_SHAPER_IMPLEMENT (graphite2),
#endif
#ifdef HAVE_UNISCRIBE
  HB_SHAPER_IMPLEMENT (uniscribe),
#endif
#ifdef HAVE_OT
  HB_SHAPER_IMPLEMENT (ot),
#endif
  HB_SHAPER_IMPLEMENT (fallback) /* should be last */
};
#undef HB_SHAPER_IMPLEMENT

static struct static_shaper_list_t
{
  static_shaper_list_t (void)
  {
    char *env = getenv ("HB_SHAPER_LIST");
    if (env && *env)
    {
       /* Reorder shaper list to prefer requested shaper list. */
      unsigned int i = 0;
      char *end, *p = env;
      for (;;) {
        end = strchr (p, ',');
        if (!end)
          end = p + strlen (p);

	for (unsigned int j = i; j < ARRAY_LENGTH (shapers); j++)
	  if (end - p == (int) strlen (shapers[j].name) &&
	      0 == strncmp (shapers[j].name, p, end - p))
	  {
	    /* Reorder this shaper to position i */
	   struct hb_shaper_pair_t t = shapers[j];
	   memmove (&shapers[i + 1], &shapers[i], sizeof (shapers[i]) * (j - i));
	   shapers[i] = t;
	   i++;
	  }

        if (!*end)
          break;
        else
          p = end + 1;
      }
    }

    ASSERT_STATIC ((ARRAY_LENGTH (shapers) + 1) * sizeof (*shaper_list) <= sizeof (shaper_list));
    unsigned int i;
    for (i = 0; i < ARRAY_LENGTH (shapers); i++)
      shaper_list[i] = shapers[i].name;
    shaper_list[i] = NULL;
  }

  const char *shaper_list[ARRAY_LENGTH (shapers) + 1];
} static_shaper_list;

const char **
hb_shape_list_shapers (void)
{
  return static_shaper_list.shaper_list;
}

hb_bool_t
hb_shape_full (hb_font_t          *font,
	       hb_buffer_t        *buffer,
	       const hb_feature_t *features,
	       unsigned int        num_features,
	       const char * const *shaper_list)
{
  hb_font_make_immutable (font); /* So we can safely cache stuff on it */

  if (likely (!shaper_list)) {
    for (unsigned int i = 0; i < ARRAY_LENGTH (shapers); i++)
      if (likely (shapers[i].func (font, buffer, features, num_features)))
        return TRUE;
  } else {
    while (*shaper_list) {
      for (unsigned int i = 0; i < ARRAY_LENGTH (shapers); i++)
	if (0 == strcmp (*shaper_list, shapers[i].name)) {
	  if (likely (shapers[i].func (font, buffer, features, num_features)))
	    return TRUE;
	  break;
	}
      shaper_list++;
    }
  }
  return FALSE;
}

void
hb_shape (hb_font_t           *font,
	  hb_buffer_t         *buffer,
	  const hb_feature_t  *features,
	  unsigned int         num_features)
{
  hb_shape_full (font, buffer, features, num_features, NULL);
}
