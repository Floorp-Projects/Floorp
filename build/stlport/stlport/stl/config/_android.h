#ifndef __stl_config__android_h
#define __stl_config__android_h

#define _STLP_PLATFORM "Android"

// Mostly Unix-like.
#define _STLP_UNIX 1

// Have pthreads support.
#define _PTHREADS

// Don't have native <cplusplus> headers
#define _STLP_HAS_NO_NEW_C_HEADERS 1

// Use unix for streams
#define _STLP_USE_UNIX_IO 1

// We don't want rtti support
#define _STLP_NO_RTTI 1

// C library is in the global namespace.
#define _STLP_VENDOR_GLOBAL_CSTD 1

// Don't have underlying local support.
#undef _STLP_REAL_LOCALE_IMPLEMENTED

// No pthread_spinlock_t in Android
#define _STLP_DONT_USE_PTHREAD_SPINLOCK 1

// Enable thread support
#undef _NOTHREADS

// Little endian platform.
#define _STLP_LITTLE_ENDIAN 1

// No <exception> headers
#undef _STLP_NO_EXCEPTION_HEADER

// No throwing exceptions
#define _STLP_NO_EXCEPTIONS 1
#define _STLP_NO_EXCEPTION_HEADER 1

// No need to define our own namespace
#define _STLP_NO_OWN_NAMESPACE 1

// Use __new_alloc instead of __node_alloc, so we don't need static functions.
#define _STLP_USE_SIMPLE_NODE_ALLOC 1

// Don't use extern versions of range errors, so we don't need to
// compile as a library.
#define _STLP_USE_NO_EXTERN_RANGE_ERRORS 1

// The system math library doesn't have long double variants, e.g
// sinl, cosl, etc
#define _STLP_NO_VENDOR_MATH_L 1

// Include most of the gcc settings.
#include <stl/config/_gcc.h>

// Do not use glibc, Android is missing some things.
#undef _STLP_USE_GLIBC

// No exceptions.
#define _STLP_NO_UNCAUGHT_EXCEPT_SUPPORT 1
#define _STLP_NO_UNEXPECTED_EXCEPT_SUPPORT 1

#define _STLP_HAS_INCLUDE_NEXT 1

// Use operator new instead of stlport own node allocator
#undef _STLP_USE_NEWALLOC
#define _STLP_USE_NEWALLOC 1

#endif /* __stl_config__android_h */
