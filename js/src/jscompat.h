/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996 Netscape Communications Corporation, All Rights Reserved.
 */
#ifndef jscompat_h___
#define jscompat_h___
/*
 * Compatibility glue for various NSPR versions.  We must always define int8,
 * int16, prword, and so on to minimize differences with js/ref, no matter what
 * the NSPR typedef names may be.
 */
#include "prtypes.h"
#include "prlong.h"
#ifdef NSPR20
typedef PRIntn intN;
typedef PRUintn uintN;
/* Following are already available in compatibility mode of NSPR 2.0 */
#if 0
typedef PRInt64 int64;
typedef PRInt32 int32;
typedef PRInt16 int16;
typedef PRInt8 int8;
typedef uint64 uint64;
typedef uint32 uint32;
typedef uint16 uint16;
typedef uint8 uint8;
#endif
typedef PRUword pruword;
typedef PRWord prword;
#else  /* NSPR 1.0 */
typedef int intN;
typedef uint uintN;
typedef uprword_t pruword;
typedef prword_t prword;
typedef int  PRIntn;
typedef unsigned int PRUintn;
typedef int64 PRInt64;
typedef int32 PRInt32;
typedef int16 PRInt16;
typedef int8  PRInt8;
typedef uint64 PRUint64;
typedef uint32 PRUint32;
typedef uint16 PRUint16;
typedef uint8  PRUint8;
typedef double PRFloat64;
typedef uprword_t PRUword;
typedef prword_t PRWord;
#define PR_EXTERN extern PR_PUBLIC_API
#define PR_IMPLEMENT PR_PUBLIC_API
#define PR_BEGIN_EXTERN_C NSPR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C NSPR_END_EXTERN_C
#define PR_BEGIN_MACRO NSPR_BEGIN_MACRO
#define PR_END_MACRO NSPR_END_MACRO
#endif /* NSPR 1.0 */
#ifndef NSPR20
typedef double float64;
#endif
typedef float float32;
#define allocPriv allocPool
#endif /* jscompat_h___ */
