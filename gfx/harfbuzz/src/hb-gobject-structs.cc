/*
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

/* g++ didn't like older gtype.h gcc-only code path. */
#include <glib.h>
#if !GLIB_CHECK_VERSION(2,29,16)
#undef __GNUC__
#undef __GNUC_MINOR__
#define __GNUC__ 2
#define __GNUC_MINOR__ 6
#endif

#include "hb-gobject.h"

#define _HB_DEFINE_BOXED_TYPE(Name,underscore_name,copy_func,free_func) \
GType \
underscore_name##_get_type (void) \
{ \
   static volatile gsize type = 0; \
   if (g_once_init_enter (&type)) { \
      GType t = g_boxed_type_register_static (g_intern_static_string (#Name), \
					      (GBoxedCopyFunc) copy_func, \
					      (GBoxedFreeFunc) free_func); \
      g_once_init_leave (&type, t); \
   } \
   return type; \
}

#define HB_DEFINE_BOXED_TYPE(name) \
	_HB_DEFINE_BOXED_TYPE (hb_##name, hb_gobject_##name, hb_##name##_reference, hb_##name##_destroy);

HB_DEFINE_BOXED_TYPE (buffer)
HB_DEFINE_BOXED_TYPE (blob)
HB_DEFINE_BOXED_TYPE (face)
HB_DEFINE_BOXED_TYPE (font)
HB_DEFINE_BOXED_TYPE (font_funcs)
HB_DEFINE_BOXED_TYPE (unicode_funcs)

