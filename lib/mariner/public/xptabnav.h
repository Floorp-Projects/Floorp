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
   xptabnav.h --- an API for XP HTML area tabbing.
   Created: Chris Toshok <toshok@netscape.com>, 6-Nov-1997.
 */


#ifndef _xp_tabnav_h
#define _xp_tabnav_h

#include "xp_mem.h"
#include "xp_core.h"
#include "ntypes.h"
#include "lo_ele.h"
#include "layout.h"

XP_BEGIN_PROTOS

/* To be called when creating a new context. */
extern void XPTAB_InitTabData(MWContext *context);

/* To be called when destroying a context. */
extern void XPTAB_DestroyTabData(MWContext *context);

/* Call when the user clicks the mouse to set the focus to a particular form element or anchor. */
extern void XPTAB_SetFocusElement(MWContext *context, LO_Element *element);

extern LO_Element* XPTAB_GetFocusElement(MWContext *context);
extern int32 XPTAB_GetFocusMapAreaIndex(MWContext *context);
extern void XPTAB_GetFocusMapArea(MWContext *context, lo_MapAreaRec **map_area);

/* Call to clear the current tab focus. */
extern void XPTAB_ClearTabFocus(MWContext *context);

/* these two functions should be invoked when the user pressed TAB (XPTAB_ToNext*Element)
   and shift TAB (XPTAB_ToPrev*Element).

   they return TRUE if the tab actually resulted in focus going to a new element, and
   FALSE otherwise. */
extern Bool XPTAB_TabToNextElement(MWContext *context);
extern Bool XPTAB_TabToPrevElement(MWContext *context);

extern Bool XPTAB_TabToNextFormElement(MWContext *context);
extern Bool XPTAB_TabToPrevFormElement(MWContext *context);

XP_END_PROTOS

#endif /* _xp_tab_h */
