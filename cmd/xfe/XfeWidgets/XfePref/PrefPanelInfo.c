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
/* Name:		<Xfe/PrefPanelInfo.c>									*/
/* Description:	Preference panel info object source.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/PrefPanelInfoP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Construct															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XfePrefPanelInfo
XfePrefPanelInfoConstruct(String panel_name)
{
    XfePrefPanelInfoRec *		panel_info = NULL;
	
	assert( panel_name != NULL );
	
	panel_info = (XfePrefPanelInfo) XtMalloc(sizeof(XfePrefPanelInfoRec) * 1);

	panel_info->panel_name		= (String) XtNewString(panel_name);
	panel_info->panel_widget	= NULL;
	panel_info->sub_panel_list	= NULL;
	
	return panel_info;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfePrefPanelInfoDestroy(XfePrefPanelInfo panel_info)
{
	assert( panel_info != NULL );

	if (panel_info->panel_name != NULL)
	{
		XtFree(panel_info->panel_name);
	}

	XtFree((char *) panel_info);
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfePrefPanelInfoGetName(XfePrefPanelInfo panel_info)
{
	assert( panel_info != NULL );

	return panel_info->panel_name;
}
/*----------------------------------------------------------------------*/
