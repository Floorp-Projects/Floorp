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
/* Name:		<XfeBm/BmRepTyep.c>										*/
/* Description:	Representation type(s) used by BmCascade and BmButton.	*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xm/RepType.h>
#include <Xfe/XfeBm.h>

/*----------------------------------------------------------------------*/
static void
RegisterAccentType(void)
{
    static String AccentNames[] = 
    { 
		"accent_box",
		"accent_none",
		"accent_underline",
    };
    
    XmRepTypeRegister(XmRAccentType,AccentNames,NULL,XtNumber(AccentNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmRegisterRepresentationTypes(void)
{
	RegisterAccentType();
}
/*----------------------------------------------------------------------*/
