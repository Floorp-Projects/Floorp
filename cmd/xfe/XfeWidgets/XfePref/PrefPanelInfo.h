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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/_XfePrefPanelInfoP_h_.h>									*/
/* Description:	Preference panel info object header.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfePrefPanelInfo_h_					/* start PrefPanelInfo.h*/
#define _XfePrefPanelInfo_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePref opaque types													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfePrefPanelInfoRec *		XfePrefPanelInfo;

/*----------------------------------------------------------------------*/
extern XfePrefPanelInfo
XfePrefPanelInfoConstruct			(String				panel_name);
/*----------------------------------------------------------------------*/
extern void
XfePrefPanelInfoDestroy				(XfePrefPanelInfo	panel_info);
/*----------------------------------------------------------------------*/
extern String
XfePrefPanelInfoGetName				(XfePrefPanelInfo	panel_info);
/*----------------------------------------------------------------------*/
	
XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end PrefPanelInfo.h	*/

