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

#ifndef __nsNoIncomingServer_h
#define __nsNoIncomingServer_h

#include "msgCore.h"
#include "nsINoIncomingServer.h"
#include "nsMsgIncomingServer.h"

/* get some implementation from nsMsgIncomingServer */
class nsNoIncomingServer : public nsMsgIncomingServer,
                             public nsINoIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINOINCOMINGSERVER

    nsNoIncomingServer();
    virtual ~nsNoIncomingServer();
    
    NS_IMETHOD GetLocalStoreType(char * *type);
};


#endif
