/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    
    PRUint32			 GetHighestNonDeletedUID();
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
