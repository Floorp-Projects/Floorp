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

/* -*- Mode: C++; tab-width: 4 -*- */
/*
   PrefsDialogNServer.h -- Multiple news server preferences dialog
    Created: Alec Flett <alecf@netscape.com>
 */

#ifndef _xfe_prefsdialognserver_h
#define _xfe_prefsdialognserver_h

#include "rosetta.h"
#include "Dialog.h"

class XFE_PrefsNServerDialog : public XFE_Dialog
{
 public:

    XFE_PrefsNServerDialog(Widget parent);
    void create();
    void init(const char *server, int32 port, XP_Bool xxx, XP_Bool password);
    void setPort(int32 port);
    char *getServerName();
    int32 getPort();
    HG78266
    XP_Bool getPassword();
    XP_Bool prompt();
    
    static void cb_cancel(Widget, XtPointer, XtPointer);
    static void cb_ok(Widget, XtPointer, XtPointer);
    HG18177

 private:
    Widget m_server_text;
    Widget m_port_text;
    HG18760
    Widget m_password_toggle;

    XP_Bool m_doneWithLoop;
    XP_Bool m_retVal;
    
};


#endif
