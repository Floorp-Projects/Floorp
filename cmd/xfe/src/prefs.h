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
   prefs.h -- the C api to preferences dialogs
   Created: Chris Toshok <toshok@netscape.com>, 12-Feb-97
 */



#ifndef _xfe_prefs_h_
#define _xfe_prefs_h_

#include "Component.h"

XP_BEGIN_PROTOS

void fe_showPreferences(XFE_Component *topLevel, MWContext *context);

void fe_showMailNewsPreferences(XFE_Component *topLevel, MWContext *context);
void fe_showMailServerPreferences(XFE_Component *toplevel, MWContext *context);

XP_END_PROTOS

#endif /* _xfe_prefs_h_ */
