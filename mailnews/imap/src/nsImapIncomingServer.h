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

#ifndef __nsImapIncomingServer_h
#define __nsImapIncomingServer_h

#include "msgCore.h"
#include "nsIImapIncomingServer.h"
#include "nsISupportsArray.h"
#include "nsMsgIncomingServer.h"
#include "nsIImapServerSink.h"
#include "nsIStringBundle.h"

/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer,
							 public nsIImapServerSink
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

    // overriding nsMsgIncomingServer methods
	NS_IMETHOD SetKey(const char * aKey);  // override nsMsgIncomingServer's implementation...
	NS_IMETHOD GetLocalStoreType(char * *type);

	NS_DECL_NSIIMAPINCOMINGSERVER
	NS_DECL_NSIIMAPSERVERSINK
    
	NS_IMETHOD PerformBiff();
	NS_IMETHOD CloseCachedConnections();

protected:
	nsresult GetFolder(const char* name, nsIMsgFolder** pFolder);
	nsresult GetUnverifiedSubFolders(nsIFolder *parentFolder, nsISupportsArray *aFoldersArray, PRInt32 *aNumUnverifiedFolders);
	nsresult GetUnverifiedFolders(nsISupportsArray *aFolderArray, PRInt32 *aNumUnverifiedFolders);

	nsresult DeleteNonVerifiedFolders(nsIFolder *parentFolder);
	PRBool NoDescendentsAreVerified(nsIFolder *parentFolder);
	PRBool AllDescendentsAreNoSelect(nsIFolder *parentFolder);
private:
    nsresult CreateImapConnection (nsIEventQueue* aEventQueue,
                                   nsIImapUrl* aImapUrl,
                                   nsIImapProtocol** aImapConnection);
    PRBool ConnectionTimeOut(nsIImapProtocol* aImapConnection);
    nsCOMPtr<nsISupportsArray> m_connectionCache;
    nsCOMPtr<nsISupportsArray> m_urlQueue;
	nsCOMPtr<nsIStringBundle>	m_stringBundle;
    nsVoidArray					m_urlConsumers;
	PRUint32					m_capability;
	nsCString					m_manageMailAccountUrl;
};


#endif
