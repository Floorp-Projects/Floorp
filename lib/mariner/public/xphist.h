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
   xphist.h --- an API for XP history navigation.
   Created: Chris Toshok <toshok@netscape.com>, 6-Sep-1998.
 */


#ifndef _xp_hist_h
#define _xp_hist_h

#include "structs.h"
#include "shist.h"
#include "shistele.h"

XP_BEGIN_PROTOS

extern void XPHIST_GoBack(MWContext *top_context);
extern void XPHIST_GoForward(MWContext *top_context);

XP_END_PROTOS

#endif /* _xp_tab_h */
