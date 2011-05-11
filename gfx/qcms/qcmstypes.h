#ifndef QCMS_TYPES_H
#define QCMS_TYPES_H

#ifdef MOZ_QCMS

#include "prtypes.h"

/* prtypes.h defines IS_LITTLE_ENDIAN and IS_BIG ENDIAN */

#if defined (__SVR4) && defined (__sun)
/* int_types.h gets included somehow, so avoid redefining the types differently */
#include <sys/int_types.h>
#elif defined (_AIX)
#include <sys/types.h>
#elif !defined(ANDROID)
typedef PRInt8 int8_t;
typedef PRUint8 uint8_t;
typedef PRInt16 int16_t;
typedef PRUint16 uint16_t;
typedef PRInt32 int32_t;
typedef PRUint32 uint32_t;
typedef PRInt64 int64_t;
typedef PRUint64 uint64_t;

#ifdef __OS2__
/* OS/2's stdlib typdefs uintptr_t. So we'll just include that so we don't collide */
#include <stdlib.h>
#elif !defined(__intptr_t_defined) && !defined(_UINTPTR_T_DEFINED)
typedef PRUptrdiff uintptr_t;
#endif
#endif

#else // MOZ_QCMS

#if BYTE_ORDER == LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
#define IS_BIG_ENDIAN
#endif

/* all of the platforms that we use _MSC_VER on are little endian
 * so this is sufficient for now */
#ifdef _MSC_VER
#define IS_LITTLE_ENDIAN
#endif

#ifdef __OS2__
#define IS_LITTLE_ENDIAN
#endif

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
#error Unknown endianess
#endif

#if defined (_SVR4) || defined (SVR4) || defined (__OpenBSD__) || defined (_sgi) || defined (__sun) || defined (sun) || defined (__digital__)
#  include <inttypes.h>
#elif defined (_MSC_VER)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#ifdef _WIN64
typedef unsigned __int64 uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif

#elif defined (_AIX)
#  include <sys/inttypes.h>
#else
#  include <stdint.h>
#endif

#endif

typedef qcms_bool bool;
#define true 1
#define false 0

#endif
