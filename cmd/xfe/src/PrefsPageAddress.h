/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
   PrefsDialog.cpp -- class for XFE preferences dialogs.
   Created: Alec Flett <alecf@netscape.com>
 */

#ifndef _xfe_prefspageaddress_h
#define _xfe_prefspageaddress_h

#include <Xm/Xm.h>
#include "PrefsDialog.h"

class XFE_PrefsPageAddress : public XFE_PrefsPage
{
 public:

    XFE_PrefsPageAddress(XFE_PrefsDialog *dialog);
    virtual ~XFE_PrefsPageAddress();

    virtual void create();

    virtual void init();
    virtual void install();

    virtual void save();

    virtual Boolean verify();

 private:

    Widget createMessageFrame(Widget parent, Widget attachTo);
    Widget createSortFrame(Widget parent, Widget attachTo);
    Widget m_complete_ab_toggle;
    Widget m_complete_dir_toggle;
    Widget m_complete_dir_combo;

    Widget m_conflict_show_toggle;
    Widget m_conflict_accept_toggle;

    Widget m_only_ab_toggle;

    Widget m_sort_first_toggle;
    Widget m_sort_last_toggle;

    XP_List     *m_pLdapServers; /* list of DIR_Servers */
    DIR_Server **m_dirList;      /* array of DIR_Servers */
    XmString    *m_dirNames;     /* XmString names of DIR_Servers */
    int          m_dirCount;
    
    XmString     m_dirSelected;  /* intitially selected directory */
};

#endif
