/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsPop3IncomingServer_h
#define __nsPop3IncomingServer_h

#include "msgCore.h"
#include "nsIPop3IncomingServer.h"
#include "nsILocalMailIncomingServer.h"
#include "nsMsgIncomingServer.h"
#include "nsIPop3Protocol.h"
#include "nsIMsgWindow.h"

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
    NS_IMETHOD PerformBiff(nsIMsgWindow *aMsgWindow);
    NS_IMETHOD GetDownloadMessagesAtStartup(PRBool *getMessages);
    NS_IMETHOD GetCanBeDefaultServer(PRBool *canBeDefaultServer);
    NS_IMETHOD GetCanSearchMessages(PRBool *canSearchMessages);
    NS_IMETHOD GetOfflineSupportLevel(PRInt32 *aSupportLevel);
    NS_IMETHOD GetRootMsgFolder(nsIMsgFolder **aRootMsgFolder);
    NS_IMETHOD GetCanFileMessagesOnServer(PRBool *aCanFileMessagesOnServer);
    NS_IMETHOD GetCanCreateFoldersOnServer(PRBool *aCanCreateFoldersOnServer);
    NS_IMETHOD GetNewMessages(nsIMsgFolder *aFolder, nsIMsgWindow *aMsgWindow, 
                      nsIUrlListener *aUrlListener);

protected:
    nsresult GetInbox(nsIMsgWindow *msgWindow, nsIMsgFolder **inbox);

private:    
    PRUint32 m_capabilityFlags;
    PRBool m_authenticated;
    nsCOMPtr <nsIPop3Protocol> m_runningProtocol;
    nsCOMPtr <nsIMsgFolder> m_rootMsgFolder;
    nsCStringArray m_uidlsToMarkDeleted;
};

#endif
