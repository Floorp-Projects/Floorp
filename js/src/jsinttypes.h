/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Jim Blandy <jimb@mozilla.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
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

#ifndef jsinttypes_h___
#define jsinttypes_h___

#include "js-config.h"

#include "mozilla/StdInt.h"

/*
 * Types:
 *   JSInt<N>, JSUint<N> (for <N> = 8, 16, 32, and 64)
 *   JSIntPtr, JSUIntPtr
 *
 * JSInt<N> and JSUint<N> are signed and unsigned types known to be
 * <N> bits long.  Note that neither JSInt8 nor JSUInt8 is necessarily
 * equivalent to a plain "char".
 *
 * JSIntPtr and JSUintPtr are signed and unsigned types capable of
 * holding an object pointer.
 *
 * These typedefs were once necessary to support platforms without a working
 * <stdint.h> (i.e. MSVC++ prior to 2010).  Now that we ship a custom <stdint.h>
 * for such compilers, they are no longer necessary and will likely be shortly
 * removed.
 */

typedef int8_t   JSInt8;
typedef int16_t  JSInt16;
typedef int32_t  JSInt32;
typedef int64_t  JSInt64;
typedef intptr_t JSIntPtr;

typedef uint8_t   JSUint8;
typedef uint16_t  JSUint16;
typedef uint32_t  JSUint32;
typedef uint64_t  JSUint64;
typedef uintptr_t JSUintPtr;

#endif /* jsinttypes_h___ */
