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

#include "nsIPop3IncomingServer.h"
#include "nsPop3IncomingServer.h"
#include "nsMsgIncomingServer.h"
#include "nsIPop3Service.h"
#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);

/* get some implementation from nsMsgIncomingServer */
class nsPop3IncomingServer : public nsMsgIncomingServer,
                             public nsIPop3IncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsPop3IncomingServer();
    virtual ~nsPop3IncomingServer();
    
    NS_IMETHOD GetRootFolderPath(char **);
    NS_IMETHOD SetRootFolderPath(char *);

    NS_IMETHOD GetLeaveMessagesOnServer(PRBool *);
    NS_IMETHOD SetLeaveMessagesOnServer(PRBool);

    NS_IMETHOD GetDeleteMailLeftOnServer(PRBool *);
    NS_IMETHOD SetDeleteMailLeftOnServer(PRBool);

    NS_IMETHOD GetServerURI(char * *uri);

	NS_IMETHOD PerformBiff();
    
private:
    char *m_rootFolderPath;
    PRBool m_leaveOnServer;
    PRBool m_deleteMailLeftOnServer;
};

NS_IMPL_ISUPPORTS_INHERITED(nsPop3IncomingServer,
                            nsMsgIncomingServer,
                            nsIPop3IncomingServer);

                            

nsPop3IncomingServer::nsPop3IncomingServer() :
    m_rootFolderPath(0),
    m_leaveOnServer(PR_FALSE),
    m_deleteMailLeftOnServer(PR_FALSE)
{    
    NS_INIT_REFCNT();
}

nsPop3IncomingServer::~nsPop3IncomingServer()
{
    PR_FREEIF(m_rootFolderPath);
}

NS_IMPL_SERVERPREF_STR(nsPop3IncomingServer,
                       RootFolderPath,
                       "directory")

NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        LeaveMessagesOnServer,
                        "leave_on_server")

NS_IMPL_SERVERPREF_INT(nsPop3IncomingServer,
                       DeleteMailLeftOnServer,
                       "delete_mail_left_on_server")

nsresult
nsPop3IncomingServer::GetServerURI(char **uri)
{
    nsresult rv;

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));
    if (NS_FAILED(rv)) return rv;

    *uri = PR_smprintf("mailbox://%s@%s",
                       (const char *)username,
                       (const char *)hostname);
    return rv;
}

NS_IMETHODIMP nsPop3IncomingServer::PerformBiff()
{
	nsresult rv;

	NS_WITH_SERVICE(nsIPop3Service, pop3Service, kCPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIMsgFolder> inbox;
	nsCOMPtr<nsIFolder> rootFolder;
	rv = GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder);
		if(rootMsgFolder)
		{
			PRUint32 numFolders;
			rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, getter_AddRefs(inbox), 1, &numFolders);
		}
	}

	rv = pop3Service->CheckForNewMail(nsnull, inbox, this, nsnull);

	return NS_OK;
}
    
nsresult NS_NewPop3IncomingServer(const nsIID& iid,
                                  void **result)
{
    nsPop3IncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsPop3IncomingServer();

    return server->QueryInterface(iid, result);
}


