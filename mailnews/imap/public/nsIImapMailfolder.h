/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIImapMailfolder_h__
#define nsIImapMailfolder_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsImapCore.h"
#include "nsIImapProtocol.h"

/* 3b2dd7e0-e72c-11d2-ab7b-00805f8ac968 */

#define NS_IIMAPMAILFOLDER_IID \
{ 0x3b2dd7e0, 0xe72c, 0x11d2, \
    { 0xab, 0x7b, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsIImapMailFolder : public nsISupports
{
public:
    static const nsIID& GetIID()
    {
        static nsIID iid = NS_IIMAPMAILFOLDER_IID;
        return iid;
    }

    // Tell mail master about a discovered imap mailbox
    NS_IMETHOD PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                   mailbox_spec* aSpec) = 0;
    NS_IMETHOD MailboxDiscoveryDone(nsIImapProtocol* aProtocol) = 0;
    // Tell mail master about the newly selected mailbox
    NS_IMETHOD UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                     mailbox_spec* aSpec) = 0;
    NS_IMETHOD UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec) = 0;
    NS_IMETHOD ChildDiscoverySucceeded(nsIImapProtocol* aProtocol) = 0;
    NS_IMETHOD OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                  const char* folderName) = 0;
    NS_IMETHOD OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                        const char* folderName) = 0;
    NS_IMETHOD OnlineFolderRename(nsIImapProtocol* aProtocol,
                                  folder_rename_struct* aStruct) = 0;
    NS_IMETHOD SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                        EIMAPSubscriptionUpgradeState* aState) = 0;
    NS_IMETHOD PromptUserForSubscribeUpdatePath(nsIImapProtocol* aProtocol,
                                                PRBool* aBool) = 0;
    NS_IMETHOD FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                FolderQueryInfo* aInfo) = 0;
    NS_IMETHOD SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo) = 0;

    NS_IMETHOD ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo) = 0;
    
    NS_IMETHOD NormalEndHeaderParseStream(nsIImapProtocol* aProtocol) = 0;
    
    NS_IMETHOD AbortHeaderParseStream(nsIImapProtocol* aProtocol) = 0;
    
    
};

#endif
