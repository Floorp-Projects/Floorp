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
   PrefsPageLIServer.cpp -- class for LI server preferences dialog.
   Created: Daniel Malmer <malmer@netscape.com>
 */

#ifndef _xfe_prefspageliserver_h
#define _xfe_prefspageliserver_h

#include <Xm/Xm.h>
#include "PrefsDialog.h"
#include "PrefsData.h"

class XFE_PrefsPageLIServer : public XFE_PrefsPage
{
 public:

    XFE_PrefsPageLIServer(XFE_PrefsDialog *dialog);
    XFE_PrefsPageLIServer(Widget parent);
    virtual ~XFE_PrefsPageLIServer();

    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();

    virtual Boolean verify();

	Widget get_frame();

	void toggleCallback(Widget, XtPointer);
	static void cb_toggle(Widget, XtPointer, XtPointer);

 private:
	Widget m_ldap_toggle;
	Widget m_ldap_addr_text;
	Widget m_ldap_base_text;

	Widget m_http_toggle;
	Widget m_http_base_text;
};

#endif
