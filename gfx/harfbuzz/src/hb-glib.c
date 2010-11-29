/*
 * Copyright (C) 2009  Red Hat, Inc.
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

#include "hb-private.h"

#include "hb-glib.h"

#include "hb-unicode-private.h"

#include <glib.h>

HB_BEGIN_DECLS


static hb_codepoint_t hb_glib_get_mirroring (hb_codepoint_t unicode) { g_unichar_get_mirror_char (unicode, &unicode); return unicode; }
static hb_category_t hb_glib_get_general_category (hb_codepoint_t unicode) { return g_unichar_type (unicode); }
static hb_script_t hb_glib_get_script (hb_codepoint_t unicode) { return g_unichar_get_script (unicode); }
static unsigned int hb_glib_get_combining_class (hb_codepoint_t unicode) { return g_unichar_combining_class (unicode); }
static unsigned int hb_glib_get_eastasian_width (hb_codepoint_t unicode) { return g_unichar_iswide (unicode); }


static hb_unicode_funcs_t glib_ufuncs = {
  HB_REFERENCE_COUNT_INVALID, /* ref_count */
  TRUE, /* immutable */
  {
    hb_glib_get_general_category,
    hb_glib_get_combining_class,
    hb_glib_get_mirroring,
    hb_glib_get_script,
    hb_glib_get_eastasian_width
  }
};

hb_unicode_funcs_t *
hb_glib_get_unicode_funcs (void)
{
  return &glib_ufuncs;
}


HB_END_DECLS
