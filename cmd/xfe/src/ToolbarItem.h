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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        ToolbarItem.h                                           //
//                                                                      //
// Description:	XFE_ToolbarItem class header.                           //
//              Superclass for anything that goes in a toolbar.         //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_toolbar_item_h_
#define _xfe_toolbar_item_h_

#include "Component.h"
#include "Frame.h"
#include "htrdf.h"

class XFE_ToolbarItem : public XFE_Component
{
public:

    XFE_ToolbarItem(XFE_Frame *		frame,
					Widget			parent,
                    HT_Resource		htResource,
					const String	name);

    virtual ~XFE_ToolbarItem();


	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Accessors                                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	const String	getName();
	Widget			getParent();
	XFE_Frame *		getAncestorFrame();
	MWContext *		getAncestorContext();
	HT_Resource		getHtResource();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Initialize                                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	initialize			() = 0;	

protected:
	
	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// XFE_Component methods                                            //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	setBaseWidget		(Widget w);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Widget creation interface                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual Widget	createBaseWidget	(Widget			parent,
										 const String	name) = 0;

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// ToolTip interface                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	tipStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString) = 0;
	
 	virtual void	docStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString) = 0;
	
 	virtual void	docStringSet		(XmString		string) = 0;

 	virtual void	docStringClear		(XmString		string) = 0;


	void			addToolTipSupport	();
	XmString		getTipStringFromAppDefaults	();
	XmString		getDocStringFromAppDefaults	();

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private data                                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	String			m_name;
	Widget			m_parent;
	XFE_Frame *		m_ancestorFrame;
	HT_Resource		m_htResource;

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private callbacks                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	static void		tipStringObtainCB	(Widget			w,
 										 XtPointer		clientData,
 										 XmString *		stringReturn,
 										 Boolean *		needToFreeString);

 	static void		docStringObtainCB	(Widget			w,
 										 XtPointer		clientData,
 										 XmString *		stringReturn,
 										 Boolean *		needToFreeString);

 	static void		docStringCB			(Widget			w,
 										 XtPointer		clientData,
 										 unsigned char	reason,
 										 XmString		string);


	static void		freeNameCB			(Widget			w,
 										 XtPointer		clientData,
 										 XtPointer		callData);
};

#endif // _xfe_toolbar_item_h_
