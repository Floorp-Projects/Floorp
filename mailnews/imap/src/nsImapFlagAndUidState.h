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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsImapFlagAndUidState_h___
#define nsImapFlagAndUidState_h___

#include "nsMsgKeyArray.h"

const PRInt32 kImapFlagAndUidStateSize =	100;

class nsImapFlagAndUidState {
public:
    nsImapFlagAndUidState(int numberOfMessages, PRUint16 flags = 0);
    nsImapFlagAndUidState(const nsImapFlagAndUidState& state, PRUint16 flags = 0);
    virtual				 ~nsImapFlagAndUidState();
    
    PRInt32					 GetNumberOfMessages();
    PRInt32					 GetNumberOfDeletedMessages();
    
    PRUint32             GetUidOfMessage(PRInt32 zeroBasedIndex);
    imapMessageFlagsType GetMessageFlags(PRInt32 zeroBasedIndex);
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
