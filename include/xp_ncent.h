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

#ifndef __XP_NavCenter_H
#define __XP_NavCenter_H

#ifdef MOZILLA_CLIENT

#include "xp_core.h"
#include "htrdf.h"

XP_BEGIN_PROTOS

/*  Callback function, allows consumer of the list to specialize search
 *      via a callback.  This is needed because of different components
 *      that hide under MWContextBrowser, when they really should be
 *      something like MWContextEditor or MWContextNetcaster.
 *  The callback allows you to decide the details to match by.
 *  Return TRUE if the context is suitable, FALSE if it is not.
 *  The varargs will match the paramaters of the find function up to
 *      the callback exactly.
 */
typedef XP_Bool (*ContextMatch)(MWContext *pCX);

extern MWContext *XP_GetLastActiveContext(ContextMatch cxFilter);
extern void XP_SetLastActiveContext(MWContext *pCX);
extern void XP_RemoveContextFromLastActiveStack(MWContext *pCX);

extern void XP_RegisterNavCenter(HT_Pane htPane, MWContext *pDocked);
extern void XP_UnregisterNavCenter(HT_Pane htPane);

extern void XP_DockNavCenter(HT_Pane htPane, MWContext *pContext);
extern void XP_UndockNavCenter(HT_Pane htPane);
extern XP_Bool XP_IsNavCenterDocked(HT_Pane htPane);
extern MWContext *XP_GetNavCenterContext(HT_Pane htPane);

extern void XP_SetNavCenterUrl(MWContext *pContext, char *pUrl);
extern void XP_AddNavCenterSitemap(MWContext *pContext, char *pSitemap, char* name);
extern void XP_RemoveNavCenterInfo(MWContext *pContext);

extern void XP_RegisterViewHTMLPane(HT_View htView, MWContext *pContext);
extern int XP_GetURLForView(HT_View htView, char *pAddress);


XP_END_PROTOS

#endif /* MOZILLA_CLIENT */

#endif /* __XP_NavCenter_H */
