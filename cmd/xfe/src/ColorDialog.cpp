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
   ColorDialog.cpp -- class for XFE color dialog, packaged from fe_colorPicker
   Created: Linda Wei <lwei@netscape.com>, 08-Nov-96.
 */



#include "structs.h"
#include "xfe.h"
#include "ColorDialog.h"

#include <Xm/DrawnB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/CascadeBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/ArrowBG.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 

extern "C"
{
	char   *XP_GetString(int i);
}

// ==================== Public Functions ====================

extern "C" Boolean
fe_colorDialog(Widget parent, MWContext *context, char *colorName, Pixel *pixel)
{
	XFE_ColorDialog   *theDialog = 0;

	// Instantiate a color dialog

	if ((theDialog = new XFE_ColorDialog(parent, "colorDialog", context)) == 0) {
	    extern int XFE_OUT_OF_MEMORY_URL;	// from xfe_err.h
	    fe_perror(context, XP_GetString( XFE_OUT_OF_MEMORY_URL ) );
	    return FALSE;
	}

	// Set default color.
	if (colorName != NULL && colorName[0] != '\0') {
		unsigned red;
		unsigned blue;
		unsigned green;

		if (sscanf(colorName, "#%02x%02x%02x", &red,  &green, &blue) == 3) {
			LO_Color color;
			color.red = red;
			color.green = green;
			color.blue = blue;
			fe_ColorPickerSetColor(theDialog->getColorPicker(), &color);
		}
	}

	// Pop up the color dialog

	theDialog->show();

	while (theDialog->getDoneButton() == XFE_ColorDialog::XFE_DIALOG_NONE_BUTTON) {
		fe_EventLoop();
	}

	char    selectedColorName[32];
	Pixel   selectedPixel = theDialog->getSelectedPixel();
	int     done_button = theDialog->getDoneButton();

	selectedColorName[0] = '\0';
	if (theDialog->getSelectedColorName())
		strcpy(selectedColorName, theDialog->getSelectedColorName());

	if (done_button != XFE_ColorDialog::XFE_DIALOG_DESTROY_BUTTON)
		delete theDialog;

	if (done_button == XFE_ColorDialog::XFE_DIALOG_OK_BUTTON) {
		if (XP_STRLEN(selectedColorName) > 0)
			strcpy (colorName, selectedColorName);
		else 
			*colorName = '\0';
		*pixel = selectedPixel;
		return TRUE;
	}
    return FALSE;
}

// ==================== Public Member Functions ====================

// Member:       XFE_ColorDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates a preferences dialog

XFE_ColorDialog::XFE_ColorDialog(Widget     parent,      // dialog parent
								 char      *name,        // dialog name
								 MWContext *context,     // context
								 Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_context(context),
	  m_colorPicker(0),
	  m_done(XFE_ColorDialog::XFE_DIALOG_NONE_BUTTON),
	  m_selectedColorName(0),
	  m_selectedPixel(0)
{
	Arg      args[16];
	Cardinal n;

	n = 0;
	m_colorPicker = fe_CreateColorPicker(m_chrome, "colorPicker", args, n);
    XtManageChild(m_colorPicker);

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, colorDialogCb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, colorDialogCb_cancel, this);
	XtAddCallback(m_chrome, XmNdestroyCallback, colorDialogCb_destroy, this);
}

// Member:       ~XFE_ColorDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_ColorDialog::~XFE_ColorDialog()
{
	// Clean up
	if (m_selectedColorName) XtFree(m_selectedColorName);
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_ColorDialog::show()
{
	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       setSelectedColorName
// Description:  sets selected color name
// Inputs:
// Side effects: 

void XFE_ColorDialog::setSelectedColorName(char *name)
{
	if (m_selectedColorName) XtFree(m_selectedColorName);
	m_selectedColorName = (char*) XtNewString(name);
}

// Member:       getSelectedPixel
// Description:  returns the selected pixel
// Inputs:
// Side effects: 

void XFE_ColorDialog::setSelectedPixel(Pixel pix)
{
	m_selectedPixel = pix;
}

// Member:       getDoneButton
// Description:  returns the done button
// Inputs:
// Side effects: 

int XFE_ColorDialog::getDoneButton()
{
	return m_done;
}

// Member:       getSelectedColorName
// Description:  returns the selected color name
// Inputs:
// Side effects: 

char *XFE_ColorDialog::getSelectedColorName()
{
	return m_selectedColorName;
}

// Member:       getSelectedPixel
// Description:  returns the selected pixel
// Inputs:
// Side effects: 

Pixel XFE_ColorDialog::getSelectedPixel()
{
	return m_selectedPixel;
}

// Member:       getContext
// Description:  Gets MWContext (for now) 
// Inputs:
// Side effects: 

MWContext *XFE_ColorDialog::getContext()
{
	return m_context;
}

// Member:       getColorPicker
// Description:  Gets the color picker folder
// Inputs:
// Side effects: 

Widget XFE_ColorDialog::getColorPicker()
{
	return m_colorPicker;
}

// ==================== Protected Member Functions ====================


// ==================== Private Member Functions ====================


// ==================== Friend Functions ====================


// Member:       colorDialogCb_ok
// Description:  
// Inputs:
// Side effects: 

void colorDialogCb_ok(Widget    /* w */,
					  XtPointer closure,
					  XtPointer /* callData */)
{
	XFE_ColorDialog *theDialog = (XFE_ColorDialog *)closure;
	LO_Color         color;
	char             buf[32];

	theDialog->m_done = XFE_ColorDialog::XFE_DIALOG_OK_BUTTON;

	fe_ColorPickerGetColor(theDialog->getColorPicker(), &color);
	XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color.red, color.green, color.blue);
	theDialog->setSelectedColorName(buf);

	Pixel pixel = fe_GetPixel(theDialog->getContext(), color.red, color.green, color.blue);
	theDialog->setSelectedPixel(pixel);
}

// Member:       colorDialogCb_cancel
// Description:  
// Inputs:
// Side effects: 

void colorDialogCb_cancel(Widget    /* w */,
						  XtPointer closure,
						  XtPointer /* callData */)
{
	XFE_ColorDialog *theDialog = (XFE_ColorDialog *)closure;
	theDialog->m_done = XFE_ColorDialog::XFE_DIALOG_CANCEL_BUTTON;
}

// Member:       colorDialogCb_destroy
// Description:  
// Inputs:
// Side effects: 

void colorDialogCb_destroy(Widget    /* w */,
						   XtPointer closure,
						   XtPointer /* callData */)
{
	XFE_ColorDialog *theDialog = (XFE_ColorDialog *)closure;
	theDialog->m_done = XFE_ColorDialog::XFE_DIALOG_DESTROY_BUTTON;
}

