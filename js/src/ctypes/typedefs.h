/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header contains the builtin types available for arguments and return
 * values, representing their C counterparts. They are used inside higher-order
 * macros that the user must call, providing a macro that will consume the
 * arguments provided to it by the higher-order macro. The macros exposed are:
 *
 *   CTYPES_FOR_EACH_BOOL_TYPE(MACRO)
 *   CTYPES_FOR_EACH_CHAR_TYPE(MACRO)
 *   CTYPES_FOR_EACH_CHAR16_TYPE(MACRO)
 *   CTYPES_FOR_EACH_INT_TYPE(MACRO)
 *   CTYPES_FOR_EACH_WRAPPED_INT_TYPE(MACRO)
 *   CTYPES_FOR_EACH_FLOAT_TYPE(MACRO)
 *   CTYPES_FOR_EACH_TYPE(MACRO)
 *
 * The MACRO name provided to any of these macros will then be repeatedly
 * invoked as
 *
 *   MACRO(typename, ctype, ffitype)
 *
 * where 'typename' is the name of the type constructor (accessible as
 * ctypes.typename), 'ctype' is the corresponding C type declaration (from
 * which sizeof(ctype) and templated type conversions will be derived), and
 * 'ffitype' is the ffi_type to use. (Special types, such as 'void' and the
 * pointer, array, and struct types are handled separately.)
 */

#ifndef ctypes_typedefs_h
#define ctypes_typedefs_h

// MSVC doesn't have ssize_t. Help it along a little.
#ifdef _WIN32
#  define CTYPES_SSIZE_T intptr_t
#else
#  define CTYPES_SSIZE_T ssize_t
#endif

// Some #defines to make handling of types whose length varies by platform
// easier. These could be implemented as configure tests, but the expressions
// are all statically resolvable so there's no need. (See CTypes.cpp for the
// appropriate PR_STATIC_ASSERTs; they can't go here since this header is
// used in places where such asserts are illegal.)
#define CTYPES_FFI_BOOL (sizeof(bool) == 1 ? ffi_type_uint8 : ffi_type_uint32)
#define CTYPES_FFI_LONG (sizeof(long) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_ULONG (sizeof(long) == 4 ? ffi_type_uint32 : ffi_type_uint64)
#define CTYPES_FFI_SIZE_T \
  (sizeof(size_t) == 4 ? ffi_type_uint32 : ffi_type_uint64)
#define CTYPES_FFI_SSIZE_T \
  (sizeof(size_t) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_OFF_T \
  (sizeof(off_t) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_INTPTR_T \
  (sizeof(uintptr_t) == 4 ? ffi_type_sint32 : ffi_type_sint64)
#define CTYPES_FFI_UINTPTR_T \
  (sizeof(uintptr_t) == 4 ? ffi_type_uint32 : ffi_type_uint64)

#define CTYPES_FOR_EACH_BOOL_TYPE(MACRO) MACRO(bool, bool, CTYPES_FFI_BOOL)

#define CTYPES_FOR_EACH_INT_TYPE(MACRO)                  \
  MACRO(int8_t, int8_t, ffi_type_sint8)                  \
  MACRO(int16_t, int16_t, ffi_type_sint16)               \
  MACRO(int32_t, int32_t, ffi_type_sint32)               \
  MACRO(uint8_t, uint8_t, ffi_type_uint8)                \
  MACRO(uint16_t, uint16_t, ffi_type_uint16)             \
  MACRO(uint32_t, uint32_t, ffi_type_uint32)             \
  MACRO(short, short, ffi_type_sint16)                   \
  MACRO(unsigned_short, unsigned short, ffi_type_uint16) \
  MACRO(int, int, ffi_type_sint32)                       \
  MACRO(unsigned_int, unsigned int, ffi_type_uint32)

#define CTYPES_FOR_EACH_WRAPPED_INT_TYPE(MACRO)                  \
  MACRO(int64_t, int64_t, ffi_type_sint64)                       \
  MACRO(uint64_t, uint64_t, ffi_type_uint64)                     \
  MACRO(long, long, CTYPES_FFI_LONG)                             \
  MACRO(unsigned_long, unsigned long, CTYPES_FFI_ULONG)          \
  MACRO(long_long, long long, ffi_type_sint64)                   \
  MACRO(unsigned_long_long, unsigned long long, ffi_type_uint64) \
  MACRO(size_t, size_t, CTYPES_FFI_SIZE_T)                       \
  MACRO(ssize_t, CTYPES_SSIZE_T, CTYPES_FFI_SSIZE_T)             \
  MACRO(off_t, off_t, CTYPES_FFI_OFF_T)                          \
  MACRO(intptr_t, intptr_t, CTYPES_FFI_INTPTR_T)                 \
  MACRO(uintptr_t, uintptr_t, CTYPES_FFI_UINTPTR_T)

#define CTYPES_FOR_EACH_FLOAT_TYPE(MACRO)   \
  MACRO(float32_t, float, ffi_type_float)   \
  MACRO(float64_t, double, ffi_type_double) \
  MACRO(float, float, ffi_type_float)       \
  MACRO(double, double, ffi_type_double)

#define CTYPES_FOR_EACH_CHAR_TYPE(MACRO)          \
  MACRO(char, char, ffi_type_uint8)               \
  MACRO(signed_char, signed char, ffi_type_sint8) \
  MACRO(unsigned_char, unsigned char, ffi_type_uint8)

#define CTYPES_FOR_EACH_CHAR16_TYPE(MACRO) \
  MACRO(char16_t, char16_t, ffi_type_uint16)

#define CTYPES_FOR_EACH_TYPE(MACRO)       \
  CTYPES_FOR_EACH_BOOL_TYPE(MACRO)        \
  CTYPES_FOR_EACH_INT_TYPE(MACRO)         \
  CTYPES_FOR_EACH_WRAPPED_INT_TYPE(MACRO) \
  CTYPES_FOR_EACH_FLOAT_TYPE(MACRO)       \
  CTYPES_FOR_EACH_CHAR_TYPE(MACRO)        \
  CTYPES_FOR_EACH_CHAR16_TYPE(MACRO)

#endif /* ctypes_typedefs_h */
