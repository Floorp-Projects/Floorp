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
   ColorDialog.h -- class definitions for Color dialog, packaged from fe_ColorPicker
   Created: Linda Wei <lwei@netscape.com>, 09-Nov-96.
 */



#ifndef _xfe_colordialog_h
#define _xfe_colordialog_h

#include "Dialog.h"

extern "C" Boolean fe_colorDialog(Widget parent, MWContext *context, char *colorName,
								  Pixel *pixel);

class XFE_ColorDialog : public XFE_Dialog

{
public:

	// Constants

	enum {
		XFE_DIALOG_NONE_BUTTON = 0,
		XFE_DIALOG_OK_BUTTON = 1,
		XFE_DIALOG_CANCEL_BUTTON = 2,
		XFE_DIALOG_DESTROY_BUTTON = 3
	};

	// Constructors, Destructors

	XFE_ColorDialog(Widget     parent,    
					char      *name,  
					MWContext *context,
					Boolean    modal = TRUE);

	virtual ~XFE_ColorDialog();

	virtual void show();                // pop up dialog

	void setSelectedColorName(char *);
	void setSelectedPixel(Pixel);

	int getDoneButton();
	char *getSelectedColorName();
	Pixel getSelectedPixel();
	MWContext *getContext();
	Widget getColorPicker();

	// Callbacks

	friend void colorDialogCb_ok(Widget, XtPointer, XtPointer);
	friend void colorDialogCb_cancel(Widget, XtPointer, XtPointer);
	friend void colorDialogCb_destroy(Widget, XtPointer, XtPointer);
	friend void colorDialogCb_selectColor(Widget, XtPointer, XtPointer);

private:
	
	MWContext *m_context;
	Widget     m_colorPicker;
	int        m_done;
	char      *m_selectedColorName;
	Pixel      m_selectedPixel;
};

#endif /* _xfe_colordialog_h */

