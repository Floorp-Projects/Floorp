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

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"

#include "nsMimeTransition.h"

#ifndef XP_MAC
	#include "nsTextFragment.h"
#endif
#include "msgCore.h"
#include "mimebuf.h"

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

extern "C" NET_cinfo *
NET_cinfo_find_type (char *uri)
{
  return NULL;  
}

extern "C" NET_cinfo *
NET_cinfo_find_info_by_type (char *uri)
{
  return NULL;  
}
