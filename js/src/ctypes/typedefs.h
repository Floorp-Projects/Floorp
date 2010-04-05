/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Witte <dwitte@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * This header contains the builtin types available for arguments and return
 * values, representing their C counterparts. They are listed inside macros
 * that the #includer is expected to #define. Format is:
 *
 * DEFINE_X_TYPE(typename, ctype, ffitype)
 *
 * where 'typename' is the name of the type constructor (accessible as
 * ctypes.typename), 'ctype' is the corresponding C type declaration (from
 * which sizeof(ctype) and templated type conversions will be derived), and
 * 'ffitype' is the ffi_type to use. (Special types, such as 'void' and the
 * pointer, array, and struct types are handled separately.)
 */

// If we're not breaking the types out, combine them together under one
// DEFINE_TYPE macro. Otherwise, turn off whichever ones we're not using.
#if defined(DEFINE_TYPE)
#  define DEFINE_CHAR_TYPE(x, y, z)         DEFINE_TYPE(x, y, z)
#  define DEFINE_JSCHAR_TYPE(x, y, z)       DEFINE_TYPE(x, y, z)
#  define DEFINE_BOOL_TYPE(x, y, z)         DEFINE_TYPE(x, y, z)
#  define DEFINE_INT_TYPE(x, y, z)          DEFINE_TYPE(x, y, z)
#  define DEFINE_WRAPPED_INT_TYPE(x, y, z)  DEFINE_TYPE(x, y, z)
#  define DEFINE_FLOAT_TYPE(x, y, z)        DEFINE_TYPE(x, y, z)
#else
#  ifndef DEFINE_BOOL_TYPE
#    define DEFINE_BOOL_TYPE(x, y, z)
#  endif
#  ifndef DEFINE_CHAR_TYPE
#    define DEFINE_CHAR_TYPE(x, y, z)
#  endif
#  ifndef DEFINE_JSCHAR_TYPE
#    define DEFINE_JSCHAR_TYPE(x, y, z)
#  endif
#  ifndef DEFINE_INT_TYPE
#    define DEFINE_INT_TYPE(x, y, z)
#  endif
#  ifndef DEFINE_WRAPPED_INT_TYPE
#    define DEFINE_WRAPPED_INT_TYPE(x, y, z)
#  endif
#  ifndef DEFINE_FLOAT_TYPE
#    define DEFINE_FLOAT_TYPE(x, y, z)
#  endif
#endif

// MSVC doesn't have ssize_t. Help it along a little.
#ifdef HAVE_SSIZE_T
#define CTYPES_SSIZE_T ssize_t
#else
#define CTYPES_SSIZE_T intptr_t
#endif

// Some #defines to make handling of types whose length varies by platform
// easier. These could be implemented as configure tests, but the expressions
// are all statically resolvable so there's no need. (See CTypes.cpp for the
// appropriate PR_STATIC_ASSERTs; they can't go here since this header is
// used in places where such asserts are illegal.)
#define CTYPES_FFI_BOOL      (sizeof(bool)      == 1 ? ffi_type_uint8  : ffi_type_uint32)
#define CTYPES_FFI_LONG      (sizeof(long)      == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_ULONG     (sizeof(long)      == 4 ? ffi_type_uint32 : ffi_type_uint64)
#define CTYPES_FFI_SIZE_T    (sizeof(size_t)    == 4 ? ffi_type_uint32 : ffi_type_uint64)
#define CTYPES_FFI_SSIZE_T   (sizeof(size_t)    == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_INTPTR_T  (sizeof(uintptr_t) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_UINTPTR_T (sizeof(uintptr_t) == 4 ? ffi_type_uint32 : ffi_type_uint64)

// The meat.
DEFINE_BOOL_TYPE       (bool,               bool,               CTYPES_FFI_BOOL)
DEFINE_INT_TYPE        (int8_t,             JSInt8,             ffi_type_sint8)
DEFINE_INT_TYPE        (int16_t,            JSInt16,            ffi_type_sint16)
DEFINE_INT_TYPE        (int32_t,            JSInt32,            ffi_type_sint32)
DEFINE_INT_TYPE        (uint8_t,            JSUint8,            ffi_type_uint8)
DEFINE_INT_TYPE        (uint16_t,           JSUint16,           ffi_type_uint16)
DEFINE_INT_TYPE        (uint32_t,           JSUint32,           ffi_type_uint32)
DEFINE_INT_TYPE        (short,              short,              ffi_type_sint16)
DEFINE_INT_TYPE        (unsigned_short,     unsigned short,     ffi_type_uint16)
DEFINE_INT_TYPE        (int,                int,                ffi_type_sint32)
DEFINE_INT_TYPE        (unsigned_int,       unsigned int,       ffi_type_uint32)
DEFINE_WRAPPED_INT_TYPE(int64_t,            JSInt64,            ffi_type_sint64)
DEFINE_WRAPPED_INT_TYPE(uint64_t,           JSUint64,           ffi_type_uint64)
DEFINE_WRAPPED_INT_TYPE(long,               long,               CTYPES_FFI_LONG)
DEFINE_WRAPPED_INT_TYPE(unsigned_long,      unsigned long,      CTYPES_FFI_ULONG)
DEFINE_WRAPPED_INT_TYPE(long_long,          long long,          ffi_type_sint64)
DEFINE_WRAPPED_INT_TYPE(unsigned_long_long, unsigned long long, ffi_type_uint64)
DEFINE_WRAPPED_INT_TYPE(size_t,             size_t,             CTYPES_FFI_SIZE_T)
DEFINE_WRAPPED_INT_TYPE(ssize_t,            CTYPES_SSIZE_T,     CTYPES_FFI_SSIZE_T)
DEFINE_WRAPPED_INT_TYPE(intptr_t,           intptr_t,           CTYPES_FFI_INTPTR_T)
DEFINE_WRAPPED_INT_TYPE(uintptr_t,          uintptr_t,          CTYPES_FFI_UINTPTR_T)
DEFINE_FLOAT_TYPE      (float32_t,          float,              ffi_type_float)
DEFINE_FLOAT_TYPE      (float64_t,          double,             ffi_type_double)
DEFINE_FLOAT_TYPE      (float,              float,              ffi_type_float)
DEFINE_FLOAT_TYPE      (double,             double,             ffi_type_double)
DEFINE_CHAR_TYPE       (char,               char,               ffi_type_uint8)
DEFINE_CHAR_TYPE       (signed_char,        signed char,        ffi_type_sint8)
DEFINE_CHAR_TYPE       (unsigned_char,      unsigned char,      ffi_type_uint8)
DEFINE_JSCHAR_TYPE     (jschar,             jschar,             ffi_type_uint16)

#undef CTYPES_SSIZE_T
#undef CTYPES_FFI_BOOL
#undef CTYPES_FFI_LONG
#undef CTYPES_FFI_ULONG
#undef CTYPES_FFI_SIZE_T
#undef CTYPES_FFI_SSIZE_T
#undef CTYPES_FFI_INTPTR_T
#undef CTYPES_FFI_UINTPTR_T

#undef DEFINE_TYPE
#undef DEFINE_CHAR_TYPE
#undef DEFINE_JSCHAR_TYPE
#undef DEFINE_BOOL_TYPE
#undef DEFINE_INT_TYPE
#undef DEFINE_WRAPPED_INT_TYPE
#undef DEFINE_FLOAT_TYPE

