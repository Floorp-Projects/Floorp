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
 LIConflictDialog.h
 By Daniel Malmer
 5/18/98

**********************************************************************/

#ifndef __LIConflictDialog_h
#define __LIConflictDialog_h

#include "Dialog.h"

class XFE_LIConflictDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_LIConflictDialog(Widget parent, const char*, const char*, const char*, const char*);

	virtual ~XFE_LIConflictDialog();

	// Accessor functions

	// Modifier functions

	// virtual void show();

	// virtual void hide();

	void useLocalCallback(Widget, XtPointer);
	void useServerCallback(Widget, XtPointer);

	static void ok_callback(Widget, XtPointer, XtPointer);
	static void cancel_callback(Widget, XtPointer, XtPointer);

	int state();

	int selection_made() {return m_selection_made;}

private:
	Widget m_message_label;
	Widget m_query_label;
	Widget m_always_toggle;
	int m_state;
	int m_selection_made;
	void setTitle(const char* title);
};

#endif /* __LIConflictDialog_h */
