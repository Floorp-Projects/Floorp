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
 LILoginAdvancedDialog.h
 By Daniel Malmer
 5/4/98

**********************************************************************/

#ifndef __LILoginAdvancedDialog_h
#define __LILoginAdvancedDialog_h

#include "Dialog.h"
#include "PrefsPageLIServer.h"
#include "PrefsPageLIFiles.h"

class XFE_LILoginAdvancedDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_LILoginAdvancedDialog(Widget parent);

	virtual ~XFE_LILoginAdvancedDialog();

	// Accessor functions

	// Modifier functions

	// virtual void show();

	// virtual void hide();

	void okCallback(Widget, XtPointer);
	void cancelCallback(Widget, XtPointer);

	static void ok_callback(Widget, XtPointer, XtPointer);
	static void cancel_callback(Widget, XtPointer, XtPointer);

private:
	XFE_PrefsPageLIServer* m_serverFrame;
	XFE_PrefsPageLIFiles* m_filesFrame;	
};

#endif /* __LILoginAdvancedDialog_h */
