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

#ifndef __nsNntpIncomingServer_h
#define __nsNntpIncomingServer_h

#include "nsINntpIncomingServer.h"
#include "nscore.h"

#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

/* get some implementation from nsMsgIncomingServer */
class nsNntpIncomingServer : public nsMsgIncomingServer,
                             public nsINntpIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsNntpIncomingServer();
    virtual ~nsNntpIncomingServer();
    
    NS_IMETHOD GetRootFolderPath(char **);
    NS_IMETHOD SetRootFolderPath(char *);

    NS_IMETHOD GetNewsrcFilePath(char **);
    NS_IMETHOD SetNewsrcFilePath(char *);
    
    NS_IMETHOD GetServerURI(char * *uri);
};

#endif
