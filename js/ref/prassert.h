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

#ifndef prassert_h___
#define prassert_h___
/*
 * PR assertion checker.
 */
#include "prtypes.h"

PR_BEGIN_EXTERN_C

#ifdef DEBUG
#define PR_ASSERT(ex) ((ex) ? (void)0 : pr_AssertBotch(#ex, __FILE__, __LINE__))
#else
#define PR_ASSERT(ex) /* nothing */
#endif

extern PR_PUBLIC_API(void)
pr_AssertBotch(const char *expr, const char *file, uintN line);

PR_END_EXTERN_C

#endif /* prassert_h___ */
