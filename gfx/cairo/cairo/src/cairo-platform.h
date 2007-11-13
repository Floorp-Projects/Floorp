/* cairo - a vector graphics library with display and print output
 *
 * Copyright Å¬Å© 2005 Mozilla Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Stuart Parmenter <stuart@mozilla.com>
 */

#ifndef CAIRO_PLATFORM_H
#define CAIRO_PLATFORM_H

#include "prcpucfg.h"

#if defined(MOZ_ENABLE_LIBXUL)

#ifdef HAVE_VISIBILITY_HIDDEN_ATTRIBUTE
#define CVISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define CVISIBILITY_HIDDEN __hidden
#else
#define CVISIBILITY_HIDDEN
#endif

/* In libxul builds we don't ever want to export cairo symbols */
#define cairo_public extern CVISIBILITY_HIDDEN

#else

#ifdef MOZ_STATIC_BUILD
# define cairo_public
#else
# if defined(XP_WIN) || defined(XP_BEOS)
#  define cairo_public extern __declspec(dllexport)
# elif defined(XP_OS2)
#  ifdef __declspec
#   define cairo_public extern __declspec(dllexport)
#  else
#   define cairo_public extern
#  endif
# else
#  ifdef HAVE_VISIBILITY_ATTRIBUTE
#   define cairo_public extern __attribute__((visibility("default")))
#  elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#   define cairo_public extern __global
#  else
#   define cairo_public extern
#  endif
# endif
#endif

#endif

#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#ifdef MOZILLA_VERSION
#include "cairo-rename.h"
#endif

#if defined(IS_BIG_ENDIAN)
#define WORDS_BIGENDIAN
#define FLOAT_WORDS_BIGENDIAN
#endif

#define CAIRO_NO_MUTEX 1

#endif /* CAIRO_PLATFORM_H */
