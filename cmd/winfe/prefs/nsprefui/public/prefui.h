/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __PREFS_H_
#define __PREFS_H_

#include "isppageo.h"

typedef void (CALLBACK *NETHELPFUNC)(LPCSTR lpszHelpTopic);

// Creates a property frame dialog, and doesn't return until the dialog
// is closed
//
// Note: Pass -1 for x and/or y to center the dialog horizontally or
// vertically with respect to hWndOwner
STDAPI
NS_CreatePropertyFrame(
	HWND     			    	  hWndOwner,  	 // parent window of propety sheet dialog
	int		 			    	  x,			 // horizontal position for dialog box, relative to hWndOwner
	int		 			    	  y,			 // vertical position for dialog box, relative to hWndOwner 
	LPCSTR   			    	  lpszCaption,	 // dialog box caption
	ULONG    			    	  nCategories,	 // number of property page providers in lplpProviders
	LPSPECIFYPROPERTYPAGEOBJECTS *lplpProviders, // array of property page providers
	ULONG						  nInitial,      // index of category to be initially displayed
    NETHELPFUNC                   lpfnNetHelp    // function to call to display NetHelp window 
);

#endif /* __PREFS_H_ */

