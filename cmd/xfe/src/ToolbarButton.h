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
// Name:        ToolbarButton.h                                         //
//                                                                      //
// Description:	XFE_ToolbarButton class header.                         //
//              A toolbar push button.                                  //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_toolbar_button_h_
#define _xfe_toolbar_button_h_

#include "ToolbarItem.h"

class XFE_RDFPopupMenu;

class XFE_ToolbarButton : public XFE_ToolbarItem
{
public:

    XFE_ToolbarButton(XFE_Frame *		frame,
					  Widget			parent,
                      HT_Resource		htResource,
					  const String		name);

    virtual ~XFE_ToolbarButton();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Initialize                                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	initialize			();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Sensitive interface                                              //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void		setSensitive			(Boolean state);
	virtual Boolean		isSensitive				();

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
	// configure                                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	configure			();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// addCallbacks                                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	addCallbacks		();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Button callback interface                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	activate			();
	virtual void	popup				(XEvent *event);

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private callbacks                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static void		activateCB			(Widget			w,
 										 XtPointer		clientData,
 										 XtPointer		callData);


	static void		popupCB				(Widget			w,
 										 XtPointer		clientData,
 										 XtPointer		callData);

	XFE_RDFPopupMenu *		_popup;
};

#endif // _xfe_toolbar_button_h_
