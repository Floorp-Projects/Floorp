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
 
#ifndef __FESelect_H
#define __FESelect_H

//  Different select types.
enum SelectType {
    DNSSelect = 0,
    SocketSelect,
    ConnectSelect,
    FileSelect,
    NetlibSelect,

    //  Leave at the end, for array bound sizing.
    MaxSelect
};

//  Class to hold all select fds of the various types.
class CSelectTracker {
    //  The various select lists.
private:
    CPtrList m_cplSelectLists[(SelectType)MaxSelect];

    //  Access to the above.
public:
    void AddSelect(SelectType stType, PRFileDesc *fd);
    void RemoveSelect(SelectType stType, PRFileDesc *fd);

    //  Informational.
public:
    BOOL HasSelect(SelectType stType);
    BOOL HasSelect(SelectType stType, PRFileDesc *fd);
    int CountSelect(SelectType stType);
	Bool PollNetlib(void);
};

//  global var used in timer.cpp.
extern CSelectTracker selecttracker;

#endif // __FESelect_H
