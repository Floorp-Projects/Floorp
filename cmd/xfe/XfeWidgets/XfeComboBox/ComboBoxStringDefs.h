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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ComboBoxStringDefs.h>								*/
/* Description:	XfeComboBox string definitions.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeComboBoxStringDefs_h_			/* start ComboBoxStringDefs.h*/
#define _XfeComboBoxStringDefs_h_

/*----------------------------------------------------------------------*/
/*																		*/
/* Callback Names														*/
/*																		*/
/*----------------------------------------------------------------------*/
								   
/*----------------------------------------------------------------------*/
/*																		*/
/* Resource Names														*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNarrow						"arrow"
#define XmNlistFontList					"listFontList"
#define XmNshareShell					"shareShell"
#define XmNshell						"shell"
#define XmNtitleShadowThickness			"titleShadowThickness"
#define XmNtitleShadowType				"titleShadowType"
#define XmNtitleFontList				"titleFontList"

/*----------------------------------------------------------------------*/
/*																		*/
/* Class Names															*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmCListFontList					"ListFontList"
#define XmCTitleFontList				"TitleFontList"
#define XmCShareShell					"ShareShell"

/*----------------------------------------------------------------------*/
/*																		*/
/* Things that conflict with Motif 2.x									*/
/*																		*/
/*----------------------------------------------------------------------*/
#if XmVersion < 2000
#define XmNcomboBoxType					"comboBoxType"
#define XmCComboBoxType					"ComboBoxType"
#define XmRComboBoxType					"ComboBoxType"
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Things that conflict elsewhere										*/
/*																		*/
/*----------------------------------------------------------------------*/
#ifndef XmNlist
#define XmNlist							"list"
#endif


#endif										/* end ComboBoxStringDefs.h	*/
