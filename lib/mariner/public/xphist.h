/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
