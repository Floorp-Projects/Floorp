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
// Name:        ToolbarUrlBar.h                                         //
//                                                                      //
// Description:	XFE_ToolbarUrlBar class header.                         //
//              A toolbar url bar combo box.                            //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_toolbar_url_bar_h_
#define _xfe_toolbar_url_bar_h_

#include "ToolbarItem.h"
#include "LocationDrag.h"

class XFE_Button;

class XFE_ToolbarUrlBar : public XFE_ToolbarItem
{
public:

    XFE_ToolbarUrlBar(XFE_Frame *		frame,
					  Widget			parent,
                      HT_Resource		htResource,
					  const String		name);

    virtual ~XFE_ToolbarUrlBar();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Initialize                                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	initialize			();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Text string methods                                              //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	void setTextStringFromURL		(URL_Struct * url);
	void setTextString				(const String str);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// XFE_ToolbarUrlBar notifications                                  //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static const char *	urlBarTextActivatedNotice;

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
	// Urlbar callback interface                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	textActivate		();

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private data                                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	XFE_Button  *		m_proxyIcon;
	XFE_LocationDrag *	m_proxyIconDragSite;

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private methods                                                  //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	void			createProxyIcon		(Widget			parent,
										 const String	name);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private callbacks                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static void		textActivateCB		(Widget			w,
 										 XtPointer		clientData,
 										 XtPointer		callData);


	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Called when a new page loads as a result of fe_GetUrl() - i think//
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	XFE_CALLBACK_DECL(newPageLoadingNotice)
};

#endif // _xfe_toolbar_url_bar_h_
