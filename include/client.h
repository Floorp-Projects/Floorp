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
 * This file should be included after xp_mcom.h
 *
 * All definitions for intermodule communications in the Netscape
 * client should be contained in this file
 */

#ifndef _CLIENT_H_
#define _CLIENT_H_

#define NEW_FE_CONTEXT_FUNCS

/* include header files needed for prototypes/etc */

#include "xp_mcom.h"

#include "ntypes.h" /* typedefs for commonly used Netscape data structures */
#include "fe_proto.h" /* all the standard FE functions */
#include "proto.h" /* library functions */

/* global data structures */
#include "structs.h"
#include "merrors.h"

#ifndef XP_MAC /* don't include everything in the world */

/* --------------------------------------------------------------------- */
/* include other bits of the Netscape client library */
#include "lo_ele.h"  /* Layout structures */
#include "net.h"
#include "gui.h"
#include "shist.h"
#include "hotlist.h"
#include "glhist.h"
#include "mime.h"

#endif /* !XP_MAC */

#endif /* _CLIENT_H_ */

