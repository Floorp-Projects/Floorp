/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef ptypes_h__
#define ptypes_h__

#include "nscore.h"
#include "nspr.h"

typedef double Date;

#define ZERO_ERROR 0

typedef PRInt32   ErrorCode;
typedef PRInt8    t_int8;
typedef PRInt32   t_int32;
typedef PRBool    t_bool;
typedef PRInt32   TextOffset;

#ifdef _IMPL_NS_NLS
#define NS_NLS NS_EXPORT
#else
#define NS_NLS NS_IMPORT
#endif


#define SUCCESS(x) ((x)<=ZERO_ERROR)
#define FAILURE(x) ((x)>ZERO_ERROR)

#define kMillisPerSecond (PR_INT32(PR_MSEC_PER_SEC))
#define kMillisPerMinute (PR_INT32(60) * kMillisPerSecond)
#define kMillisPerHour   (PR_INT32(60) * kMillisPerMinute)
#define kMillisPerDay    (PR_INT32(24) * kMillisPerHour)

#endif
