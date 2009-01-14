/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Jim Blandy
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This header provides definitions for the <stdint.h> types we use,
   even on systems that lack <stdint.h>.  */

#ifndef jsstdint_h___
#define jsstdint_h___

#include "js-config.h"

/* If we have a working stdint.h, use it.  */
#if defined(JS_HAVE_STDINT_H)
#include <stdint.h>

/* If the configure script was able to find appropriate types for us,
   use those.  */
#elif defined(JS_INT8_TYPE)

typedef signed   JS_INT8_TYPE   int8_t;
typedef signed   JS_INT16_TYPE  int16_t;
typedef signed   JS_INT32_TYPE  int32_t;
typedef signed   JS_INT64_TYPE  int64_t;
typedef signed   JS_INTPTR_TYPE intptr_t;

typedef unsigned JS_INT8_TYPE   uint8_t;
typedef unsigned JS_INT16_TYPE  uint16_t;
typedef unsigned JS_INT32_TYPE  uint32_t;
typedef unsigned JS_INT64_TYPE  uint64_t;
typedef unsigned JS_INTPTR_TYPE uintptr_t;

#else

/* Microsoft Visual C/C++ has built-in __intN types.  */
#if defined(JS_HAVE___INTN)

typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#else
#error "couldn't find exact-width integer types"
#endif

/* Microsoft Visual C/C++ defines intptr_t and uintptr_t in <stddef.h>.  */
#if defined(JS_STDDEF_H_HAS_INTPTR_T)
#include <stddef.h>
#else
#error "couldn't find definitions for intptr_t, uintptr_t"
#endif

#endif /* JS_HAVE_STDINT_H */

#endif /* jsstdint_h___ */
