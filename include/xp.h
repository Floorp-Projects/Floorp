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


/*
xp.h

This file is left around for backwards compatability.  Nothing new should
be added to this file.  Rather, add it to the client specific section or
the cross-platform specific area depending on what is appropriate.

-------------------------------------------------------------------------*/
#ifndef _XP_H_
#define _XP_H_

#include "xp_mcom.h"
#include "client.h"

#ifdef HEAPAGENT

#define MEM_DEBUG         1
#define DEFINE_NEW_MACRO  1
#include <heapagnt.h>

#endif /* HEAPAGENT */

#endif /* !_XP_H_ */   
																  
