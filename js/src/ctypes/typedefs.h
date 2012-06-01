/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#define CTYPES_FFI_OFF_T     (sizeof(off_t)     == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_INTPTR_T  (sizeof(uintptr_t) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_UINTPTR_T (sizeof(uintptr_t) == 4 ? ffi_type_uint32 : ffi_type_uint64)

// The meat.
DEFINE_BOOL_TYPE       (bool,               bool,               CTYPES_FFI_BOOL)
DEFINE_INT_TYPE        (int8_t,             int8_t,             ffi_type_sint8)
DEFINE_INT_TYPE        (int16_t,            int16_t,            ffi_type_sint16)
DEFINE_INT_TYPE        (int32_t,            int32_t,            ffi_type_sint32)
DEFINE_INT_TYPE        (uint8_t,            uint8_t,            ffi_type_uint8)
DEFINE_INT_TYPE        (uint16_t,           uint16_t,           ffi_type_uint16)
DEFINE_INT_TYPE        (uint32_t,           uint32_t,           ffi_type_uint32)
DEFINE_INT_TYPE        (short,              short,              ffi_type_sint16)
DEFINE_INT_TYPE        (unsigned_short,     unsigned short,     ffi_type_uint16)
DEFINE_INT_TYPE        (int,                int,                ffi_type_sint32)
DEFINE_INT_TYPE        (unsigned_int,       unsigned int,       ffi_type_uint32)
DEFINE_WRAPPED_INT_TYPE(int64_t,            int64_t,            ffi_type_sint64)
DEFINE_WRAPPED_INT_TYPE(uint64_t,           uint64_t,           ffi_type_uint64)
DEFINE_WRAPPED_INT_TYPE(long,               long,               CTYPES_FFI_LONG)
DEFINE_WRAPPED_INT_TYPE(unsigned_long,      unsigned long,      CTYPES_FFI_ULONG)
DEFINE_WRAPPED_INT_TYPE(long_long,          long long,          ffi_type_sint64)
DEFINE_WRAPPED_INT_TYPE(unsigned_long_long, unsigned long long, ffi_type_uint64)
DEFINE_WRAPPED_INT_TYPE(size_t,             size_t,             CTYPES_FFI_SIZE_T)
DEFINE_WRAPPED_INT_TYPE(ssize_t,            CTYPES_SSIZE_T,     CTYPES_FFI_SSIZE_T)
DEFINE_WRAPPED_INT_TYPE(off_t,              off_t,              CTYPES_FFI_OFF_T)
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

