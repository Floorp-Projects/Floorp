/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* Include files we are going to want available to all files....these files include
   NSPR, memory, and string header files among others */

#include "nscore.h"
#include "xp_core.h"
#include "nsCRT.h"
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsVoidArray.h"

/* Right now, plstr.h does not implement strok yet, so we'll go through the string library for this.... */
/* We need to fix this! strtok is not thread-safe on most platforms.
 * we need a better solution for this */
#include <string.h>
#define XP_STRTOK                 	strtok

/* see mozilla/xpcom/public/nsError.h for details */
#define NS_ERROR_MODULE_MAIL 16
