/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef prmacos_h___
#define prmacos_h___
//
// This file contains all changes and additions that need to be made to the
// runtime for the Macintosh platform hosting the Metrowerks environment.
// This file should only be included in Macintosh builds.
//

PR_BEGIN_EXTERN_C

#include <stddef.h>

extern void* reallocSmaller(void* block, size_t newSize);

PR_END_EXTERN_C

#endif /* prmacos_h___ */
