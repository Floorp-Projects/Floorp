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
   PrefsPageLIFiles.cpp -- class for LI file preferences dialog.
   Created: Daniel Malmer <malmer@netscape.com>
 */

#ifndef _xfe_prefspagelifiles_h
#define _xfe_prefspagelifiles_h

#include "rosetta.h"
#include <Xm/Xm.h>
#include "PrefsDialog.h"

class XFE_PrefsPageLIFiles : public XFE_PrefsPage
{
 public:

    XFE_PrefsPageLIFiles(XFE_PrefsDialog *dialog);
    XFE_PrefsPageLIFiles(Widget parent);
    virtual ~XFE_PrefsPageLIFiles();

    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();

    virtual Boolean verify();

	Widget get_frame();

 private:
	Widget m_bookmark_toggle;
	Widget m_cookies_toggle;
	Widget m_filter_toggle;
	Widget m_addrbook_toggle;
	Widget m_navcenter_toggle;
	Widget m_prefs_toggle;
	Widget m_javasec_toggle;
	HG89217
};

#endif
