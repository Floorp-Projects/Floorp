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
   PrefsPageSmartUpdate.cpp -- class for Smart Update preferences dialog.
   Created: Daniel Malmer <malmer@netscape.com>
 */

#ifndef _xfe_prefspagesmartupdate_h
#define _xfe_prefspagesmartupdate_h

#include <Xm/Xm.h>
#include "PrefsDialog.h"

class XFE_PrefsPageSmartUpdate : public XFE_PrefsPage
{
 public:

    XFE_PrefsPageSmartUpdate(XFE_PrefsDialog *dialog);
    virtual ~XFE_PrefsPageSmartUpdate();

    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();

    virtual Boolean verify();

	void uninstallCB(Widget, XtPointer);
	static void uninstall_cb(Widget, XtPointer, XtPointer);

 private:
	void populateList();
	Widget m_enable_toggle;
	Widget m_confirm_toggle;
	Widget m_uninstall_list;
	Widget m_fake_list;
};

#endif
