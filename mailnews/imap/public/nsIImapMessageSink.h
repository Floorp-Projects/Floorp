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
#ifndef nsIImapMessageSink_h__
#define nsIImapMessageSink_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsImapCore.h"
#include "nsIImapProtocol.h"

/* f8abba00-e70e-11d2-ab7a-00805f8ac968 */

#define NS_IIMAPMESSAGESINK_IID \
{ 0xf8abba00, 0xe70e, 0x11d2, \
    { 0xab, 0x7a, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsIImapMessageSink : public nsISupports
{
public:
    static const nsIID& GetIID()
    {
        static nsIID iid = NS_IIMAPMESSAGESINK_IID;
        return iid;
    }
    
    // set up messge download output stream
    // FE calls nsIImapProtocol::SetMessageDownloadOutputStream 
    NS_IMETHOD SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo) = 0;

    NS_IMETHOD ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo) = 0;
    
    NS_IMETHOD NormalEndMsgWriteStream(nsIImapProtocol* aProtocol) = 0;
    
    NS_IMETHOD AbortMsgWriteStream(nsIImapProtocol* aProtocol) = 0;
    
    // message move/copy related methods
    NS_IMETHOD OnlineCopyReport(nsIImapProtocol* aProtocol,
                                ImapOnlineCopyState* aCopyState) = 0;
    NS_IMETHOD BeginMessageUpload(nsIImapProtocol* aProtocol) = 0;
    NS_IMETHOD UploadMessageFile(nsIImapProtocol* aProtocol,
                                 UploadMessageInfo* aMsgInfo) = 0;

    // message flags operation
    NS_IMETHOD NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                  FlagsKeyStruct* aKeyStruct) = 0;

    NS_IMETHOD NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                    delete_message_struct* aStruct) = 0;
    NS_IMETHOD GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                    MessageSizeInfo* sizeInfo) = 0;

};
	

#endif
