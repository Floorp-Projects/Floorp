/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// This file allocates and initializes the CLSIDs

#include <windows.h>

#ifndef _WIN32
#include <string.h>
#include <compobj.h>
#endif //!WIN32

// This redefines the DEFINE_GUID() macro to do allocation.
#include <initguid.h>

#ifndef INITGUID
#define INITGUID
#endif

#include "prefuiid.h"
#include "brprefid.h"
#ifdef MOZ_LOC_INDEP
#include "liprefid.h"
#endif
#ifdef MOZ_MAIL_NEWS
#include "mnprefid.h"
#endif
#ifdef EDITOR
#include "edprefid.h"
#endif // EDITOR

// Note: these are defined in uuid2.lib for MSVC42 or less
#ifndef _WIN32
#include <olectlid.h>
#endif

