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


/*
   PrefsPageLIGeneral.cpp -- class for LI general preferences dialog.
   Created: Daniel Malmer <malmer@netscape.com>
 */

#ifndef _xfe_prefspageligeneral_h
#define _xfe_prefspageligeneral_h

#include <Xm/Xm.h>
#include "PrefsDialog.h"

class XFE_PrefsPageLIGeneral : public XFE_PrefsPage
{
 public:

	enum DialogUsage {pref, login};

    XFE_PrefsPageLIGeneral(XFE_PrefsDialog *dialog, DialogUsage usage = pref);
    XFE_PrefsPageLIGeneral(Widget parent, DialogUsage usage = pref);
    virtual ~XFE_PrefsPageLIGeneral();

    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();

    virtual Boolean verify();

#ifdef LI_BACKGROUND_SYNC_ENABLED
	void syncToggle(Widget, XtPointer);
	static void cb_toggle(Widget, XtPointer, XtPointer);
#endif

	Widget get_top_frame();
	Widget get_bottom_frame();

 private:
	DialogUsage m_usage;

	Widget m_top_frame;
	Widget m_bottom_frame;

	Widget m_enable_toggle;

#ifdef LI_BACKGROUND_SYNC_ENABLED
	Widget m_sync_toggle;
	Widget m_sync_text;
#endif

	Widget m_user_text;
	Widget m_password_text;
	Widget m_save_password_toggle;
};

#endif
