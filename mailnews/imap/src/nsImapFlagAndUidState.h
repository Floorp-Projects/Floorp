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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsImapFlagAndUidState_h___
#define nsImapFlagAndUidState_h___

#include "nsMsgKeyArray.h"
#include "nsIImapFlagAndUidState.h"

const PRInt32 kImapFlagAndUidStateSize =	100;

class nsImapFlagAndUidState : public nsIImapFlagAndUidState
{
public:
    NS_DECL_ISUPPORTS
    nsImapFlagAndUidState(int numberOfMessages, PRUint16 flags = 0);
    nsImapFlagAndUidState(const nsImapFlagAndUidState& state, PRUint16 flags = 0);
    virtual				 ~nsImapFlagAndUidState();
    

	NS_DECL_NSIIMAPFLAGANDUIDSTATE

//    NS_IMETHOD			GetNumberOfMessages(PRInt32 *result);
//    NS_IMETHOD			GetUidOfMessage(PRInt32 zeroBasedIndex, PRUint32 *result);
//    NS_IMETHOD			GetMessageFlags(PRInt32 zeroBasedIndex, imapMessageFlagsType *result);
//	NS_IMETHOD			GetNumberOfRecentMessages(PRInt32 *result);

    PRInt32				GetNumberOfDeletedMessages();
    
    imapMessageFlagsType GetMessageFlagsFromUID(PRUint32 uid, PRBool *foundIt, PRInt32 *ndx);
	PRBool				 IsLastMessageUnseen(void);
    void				 ExpungeByIndex(PRUint32 index);
	// adds to sorted list.  protects against duplicates and going past fNumberOfMessageSlotsAllocated  
    void				 AddUidFlagPair(PRUint32 uid, imapMessageFlagsType flags);
    
    PRUint32			 GetHighestNonDeletedUID();
	void				 Reset(PRUint32 howManyLeft);
    void                 SetSupportedUserFlags(PRUint16 flags);
    PRUint16               GetSupportedUserFlags() { return fSupportedUserFlags; };

private:
    
    PRInt32                 fNumberOfMessagesAdded;
    PRInt32					fNumberOfMessageSlotsAllocated;
	PRInt32					fNumberDeleted;
    nsMsgKeyArray           fUids;
    imapMessageFlagsType    *fFlags;
	PRUint16				fSupportedUserFlags;
};




#endif
