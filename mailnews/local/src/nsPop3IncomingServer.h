/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsPop3IncomingServer_h
#define __nsPop3IncomingServer_h

#include "msgCore.h"
#include "nsIPop3IncomingServer.h"
#include "nsILocalMailIncomingServer.h"
#include "nsMsgIncomingServer.h"

/* get some implementation from nsMsgIncomingServer */
class nsPop3IncomingServer : public nsMsgIncomingServer,
                             public nsIPop3IncomingServer,
                             public nsILocalMailIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPOP3INCOMINGSERVER
    NS_DECL_NSILOCALMAILINCOMINGSERVER

    nsPop3IncomingServer();
    virtual ~nsPop3IncomingServer();

    NS_IMETHOD GetLocalStoreType(char **);
    NS_IMETHOD PerformBiff();
    NS_IMETHOD GetDownloadMessagesAtStartup(PRBool *getMessages);
private:

    static nsresult setSubFolderFlag(nsIFolder *aRootFolder,
                                     PRUnichar *folderName,
                                     PRUint32 flag);
    
    // copied from nsMsgFolder because flag setting is done in the
    // server, not the folder :(
    static nsrefcnt gInstanceCount;
    static nsresult initializeStrings();
    
    static PRUnichar *kInboxName;
    static PRUnichar *kTrashName;
    static PRUnichar *kSentName;
    static PRUnichar *kDraftsName;
    static PRUnichar *kTemplatesName;
    static PRUnichar *kUnsentName;
    
    PRUint32 m_capabilityFlags;
};

#endif
