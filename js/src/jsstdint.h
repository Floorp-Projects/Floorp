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

/*
 * This header provides definitions for the <stdint.h> types we use,
 * even on systems that lack <stdint.h>.
 *
 * NOTE: This header should only be included in private SpiderMonkey
 * code; public headers should use only the JS{Int,Uint}N types; see
 * the comment for them in "jsinttypes.h".
 *
 * At the moment, these types are not widely used within SpiderMonkey;
 * this file is meant to make existing uses portable, and to allow us
 * to transition portably to using them more, if desired.
 */

#ifndef jsstdint_h___
#define jsstdint_h___

#include "jsinttypes.h"

/* If we have a working stdint.h, then jsinttypes.h has already
   defined the standard integer types.  Otherwise, define the standard
   names in terms of the 'JS' types.  */
#if ! defined(JS_HAVE_STDINT_H)

typedef JSInt8  int8_t;
typedef JSInt16 int16_t;
typedef JSInt32 int32_t;
typedef JSInt64 int64_t;

typedef JSUint8  uint8_t;
typedef JSUint16 uint16_t;
typedef JSUint32 uint32_t;
typedef JSUint64 uint64_t;

/* If JS_STDDEF_H_HAS_INTPTR_T or JS_CRTDEFS_H_HAS_INTPTR_T are
   defined, then jsinttypes.h included the given header, which
   introduced definitions for intptr_t and uintptr_t.  Otherwise,
   define the standard names in terms of the 'JS' types.  */
#if !defined(JS_STDDEF_H_HAS_INTPTR_T) && !defined(JS_CRTDEFS_H_HAS_INTPTR_T)
typedef JSIntPtr  intptr_t;
typedef JSUintPtr uintptr_t;
#endif

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)

#define INT8_MAX  127
#define INT8_MIN  (-INT8_MAX - 1)
#define INT16_MAX 32767
#define INT16_MIN (-INT16_MAX - 1)
#define INT32_MAX 2147483647
#define INT32_MIN (-INT32_MAX - 1)
#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-INT64_MAX - 1)

#define UINT8_MAX  255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615ULL

/*
 * These are technically wrong as they can't be used in the preprocessor, but
 * we would require compiler assistance, and at the moment we don't need
 * preprocessor-correctness.
 */
#define INTPTR_MAX  ((intptr_t) (UINTPTR_MAX >> 1))
#define INTPTR_MIN  (intptr_t(uintptr_t(INTPTR_MAX) + uintptr_t(1)))
#define UINTPTR_MAX ((uintptr_t) -1)
#define SIZE_MAX UINTPTR_MAX
#define PTRDIFF_MAX INTPTR_MAX
#define PTRDIFF_MIN INTPTR_MIN

#endif /* !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS) */

#endif /* JS_HAVE_STDINT_H */

#endif /* jsstdint_h___ */
