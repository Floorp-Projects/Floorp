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

	virtual Pixel getFGPixel();
	virtual Pixel getBGPixel();
	virtual Pixel getTopShadowPixel();
	virtual Pixel getBottomShadowPixel();

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
