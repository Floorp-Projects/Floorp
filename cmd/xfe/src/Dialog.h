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
/* 
   Dialog.h -- class definitions for FE dialog windows
   Created: Linda Wei <lwei@netscape.com>, 16-Sep-96.
 */



#ifndef _xfe_dialog_h
#define _xfe_dialog_h

#include "Component.h"

class XFE_Dialog : public XFE_Component
{
public:

	// Constructors, Destructors

	XFE_Dialog(Widget   parent,    
			   char    *name,  
			   Boolean  ok = TRUE,    
			   Boolean  cancel = TRUE,
			   Boolean  help = TRUE,  
			   Boolean  apply = TRUE, 
			   Boolean  separator = TRUE,
			   Boolean  modal = TRUE,
			   Widget	chrome_widget = NULL);

	virtual ~XFE_Dialog();

	// Accessor functions

	// Modifier functions

	virtual void show();

	virtual void hide();

protected:
	Widget m_wParent;        // parent widget
	Widget m_chrome;         // dialog chrome - selection box
	Widget m_okButton;
	Widget m_cancelButton;
	Widget m_helpButton;
	Widget m_applyButton;

private:
	virtual Widget createDialogChromeWidget(Widget   parent,    
											char    *name,
											Boolean  ok,
											Boolean  cancel,
											Boolean  help,
											Boolean  apply,
											Boolean  separator,
											Boolean  modal);
};

#endif /* _xfe_dialog_h */
