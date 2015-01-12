/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header contains the builtin types available for arguments and return
 * values, representing their C counterparts. They are used inside higher-order
 * macros that the user must call, providing a macro that will consume the
 * arguments provided to it by the higher-order macro. The macros exposed are:
 *
 *   CTYPES_FOR_EACH_BOOL_TYPE(macro)
 *   CTYPES_FOR_EACH_CHAR_TYPE(macro)
 *   CTYPES_FOR_EACH_CHAR16_TYPE(macro)
 *   CTYPES_FOR_EACH_INT_TYPE(macro)
 *   CTYPES_FOR_EACH_WRAPPED_INT_TYPE(macro)
 *   CTYPES_FOR_EACH_FLOAT_TYPE(macro)
 *   CTYPES_FOR_EACH_TYPE(macro)
 *
 * The macro name provided to any of these macros will then be repeatedly
 * invoked as
 *
 *   macro(typename, ctype, ffitype)
 *
 * where 'typename' is the name of the type constructor (accessible as
 * ctypes.typename), 'ctype' is the corresponding C type declaration (from
 * which sizeof(ctype) and templated type conversions will be derived), and
 * 'ffitype' is the ffi_type to use. (Special types, such as 'void' and the
 * pointer, array, and struct types are handled separately.)
 */

#ifndef ctypes_typedefs_h
#define  ctypes_typedefs_h

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

#define CTYPES_FOR_EACH_BOOL_TYPE(macro) \
  macro(bool,               bool,               CTYPES_FFI_BOOL)

#define CTYPES_FOR_EACH_INT_TYPE(macro) \
  macro(int8_t,             int8_t,             ffi_type_sint8) \
  macro(int16_t,            int16_t,            ffi_type_sint16) \
  macro(int32_t,            int32_t,            ffi_type_sint32) \
  macro(uint8_t,            uint8_t,            ffi_type_uint8) \
  macro(uint16_t,           uint16_t,           ffi_type_uint16) \
  macro(uint32_t,           uint32_t,           ffi_type_uint32) \
  macro(short,              short,              ffi_type_sint16) \
  macro(unsigned_short,     unsigned short,     ffi_type_uint16) \
  macro(int,                int,                ffi_type_sint32) \
  macro(unsigned_int,       unsigned int,       ffi_type_uint32)

#define CTYPES_FOR_EACH_WRAPPED_INT_TYPE(macro) \
  macro(int64_t,            int64_t,            ffi_type_sint64) \
  macro(uint64_t,           uint64_t,           ffi_type_uint64) \
  macro(long,               long,               CTYPES_FFI_LONG) \
  macro(unsigned_long,      unsigned long,      CTYPES_FFI_ULONG) \
  macro(long_long,          long long,          ffi_type_sint64) \
  macro(unsigned_long_long, unsigned long long, ffi_type_uint64) \
  macro(size_t,             size_t,             CTYPES_FFI_SIZE_T) \
  macro(ssize_t,            CTYPES_SSIZE_T,     CTYPES_FFI_SSIZE_T) \
  macro(off_t,              off_t,              CTYPES_FFI_OFF_T) \
  macro(intptr_t,           intptr_t,           CTYPES_FFI_INTPTR_T) \
  macro(uintptr_t,          uintptr_t,          CTYPES_FFI_UINTPTR_T)

#define CTYPES_FOR_EACH_FLOAT_TYPE(macro) \
  macro(float32_t,          float,              ffi_type_float) \
  macro(float64_t,          double,             ffi_type_double) \
  macro(float,              float,              ffi_type_float) \
  macro(double,             double,             ffi_type_double)

#define CTYPES_FOR_EACH_CHAR_TYPE(macro) \
  macro(char,               char,               ffi_type_uint8) \
  macro(signed_char,        signed char,        ffi_type_sint8) \
  macro(unsigned_char,      unsigned char,      ffi_type_uint8)

#define CTYPES_FOR_EACH_CHAR16_TYPE(macro) \
  macro(char16_t,           char16_t,           ffi_type_uint16)

#define CTYPES_FOR_EACH_TYPE(macro) \
  CTYPES_FOR_EACH_BOOL_TYPE(macro) \
  CTYPES_FOR_EACH_INT_TYPE(macro) \
  CTYPES_FOR_EACH_WRAPPED_INT_TYPE(macro) \
  CTYPES_FOR_EACH_FLOAT_TYPE(macro) \
  CTYPES_FOR_EACH_CHAR_TYPE(macro) \
  CTYPES_FOR_EACH_CHAR16_TYPE(macro)

#endif /* ctypes_typedefs_h */
