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
// Name:        ToolbarNavigation.h                                     //
//                                                                      //
// Description:	XFE_ToolbarNavigation class header.                     //
//              The Back/Forward toolbar buttons.                       //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_toolbar_navigation_h_
#define _xfe_toolbar_navigation_h_

#include "ToolbarButton.h"

class XFE_ToolbarNavigation : public XFE_ToolbarButton
{
public:
	
    XFE_ToolbarNavigation(XFE_Frame *		frame,
						  Widget			parent,
						  HT_Resource		htResource,
						  const String		name,
						  int				forward);

    virtual ~XFE_ToolbarNavigation();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Accessors                                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	Widget			getSubmenu();
	int				getForward();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Initialize                                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	initialize			();

protected:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Widget creation interface                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual Widget	createBaseWidget	(Widget			parent,
										 const String	name);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// ToolTip interface                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	tipStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString);
	
 	virtual void	docStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString);


 	virtual void	entryStringObtain	(XmString *		stringReturn,
										 Boolean *		needToFreeString);

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private data                                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	Widget			m_submenu;
	XP_Bool			m_forward;
};

#endif // _xfe_toolbar_navigation_h_
