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

#ifndef __nsPop3IncomingServer_h
#define __nsPop3IncomingServer_h

#include "msgCore.h"
#include "nsIPop3IncomingServer.h"
#include "nsMsgIncomingServer.h"

/* get some implementation from nsMsgIncomingServer */
class nsPop3IncomingServer : public nsMsgIncomingServer,
                             public nsIPop3IncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPOP3INCOMINGSERVER

    nsPop3IncomingServer();
    virtual ~nsPop3IncomingServer();

    NS_IMETHOD GetServerURI(char **);
    NS_IMETHOD PerformBiff();
    
private:
    PRUint32 m_capabilityFlags;
};

#endif
