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

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Command interface interface.                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	void				setCommand				(CommandType command);
	CommandType			getCommand				();

	void				setCallData				(void * callData);
	void * 				getCallData				();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// XFE_ToolbarButton notifications                                  //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static const char * doCommandNotice;

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
	// Configure                                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	configure			();

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// ToolTip interface                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
 	virtual void	tipStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString);
	
 	virtual void	docStringObtain		(XmString *		stringReturn,
										 Boolean *		needToFreeString);
	
 	virtual void	docStringSet		(XmString		string);

 	virtual void	docStringClear		(XmString		string);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Style and layout interface                                       //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	int32			getButtonStyle		();
	unsigned char	styleToLayout		(int32 button_style);

	void			getPixmapsForEntry	(Pixmap *    pixmapOut,
										 Pixmap *    maskOut,
										 Pixmap *    armedPixmapOut,
										 Pixmap *    armedMaskOut);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Button callback interface                                        //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	virtual void	activate			();

private:

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private data                                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	CommandType		m_command;
	void *			m_callData;

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// Private callbacks                                                //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static void		activateCB			(Widget			w,
 										 XtPointer		clientData,
 										 XtPointer		callData);

};

#endif // _xfe_toolbar_button_h_
