/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/**********************************************************************
 LILoginDialog.h
 By Daniel Malmer
 5/4/98

**********************************************************************/

#ifndef __LILoginDialog_h
#define __LILoginDialog_h

#include "Dialog.h"
#include "PrefsPageLIGeneral.h"

class XFE_LILoginDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_LILoginDialog(Widget parent);

	virtual ~XFE_LILoginDialog();

	// Accessor functions

	// Modifier functions

	// virtual void show();

	// virtual void hide();

	void okCallback(Widget, XtPointer);
	void cancelCallback(Widget, XtPointer);
	void advancedCallback(Widget, XtPointer);

	static void advanced_callback(Widget, XtPointer, XtPointer);
	static void ok_callback(Widget, XtPointer, XtPointer);
	static void cancel_callback(Widget, XtPointer, XtPointer);

	int selection_made() {return m_selection_made;}

private:
	XFE_PrefsPageLIGeneral* m_userFrame;
	int m_selection_made;
};

#endif /* __LILoginDialog_h */
