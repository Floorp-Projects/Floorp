/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        MenuUtils.h                                             //
//                                                                      //
// Description:	Utilities for creating and manipulatin menus and and    //
//              menu items.                                             //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_menu_utils_h_
#define _xfe_menu_utils_h_

#include <Xm/Xm.h>			// For XmString

class XFE_MenuUtils
{
public:
    
 	XFE_MenuUtils() {}
 	~XFE_MenuUtils() {}

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Menu items                                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
    static Widget	createPushButton		(Widget				parent,
											 const String		name,
											 Boolean			gadget,
											 Boolean			fancy,
											 XtCallbackProc		activate_cb,
											 XtCallbackProc		arm_cb,
											 XtCallbackProc		disarm_cb,
											 XtPointer			client_data,
											 ArgList			av,
											 Cardinal			ac);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// More button                                                      //
	//                                                                  //
	// The "More..." button appears at the end of a long menu pane.  It //
	// allows the user to cascade in order to find items on a long pane.//
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static Widget	createMoreButton		(Widget				menu,
											 const String		name,
											 const String		paneName,
											 Boolean			fancy);

	static Widget	getLastMoreMenu			(Widget				menu,
											 const String		name,
											 const String		paneName,
											 Boolean			fancy);

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private                                                          //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
    static Widget	createItem				(Widget				parent,
											 const String		name,
											 WidgetClass		wc,
											 XtCallbackProc		activate_cb,
											 XtCallbackProc		arm_cb,
											 XtCallbackProc		disarm_cb,
											 XtCallbackProc		cascading_cb,
											 XtPointer			client_data,
											 ArgList			av,
											 Cardinal			ac);
	
};

#endif // _xfe_menu_utils_h_
