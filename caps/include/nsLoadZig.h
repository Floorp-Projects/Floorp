/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef _NS_LOAD_ZIG_H_
#define _NS_LOAD_ZIG_H_

#include "prtypes.h"

PR_BEGIN_EXTERN_C

#include "zig.h"
#include "nsZip.h"

PR_PUBLIC_API(void *)
nsInitializeZig(ns_zip_t *zip,
                int (*callbackFnName) (int status, ZIG *zig, 
                                       const char *metafile, 
                                       char *pathname, char *errortext));
PR_END_EXTERN_C

#endif /* _NS_LOAD_ZIG_H_ */
