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

#ifndef nsIDBFolderInfo_h__
#define nsIDBFolderInfo_h__

#include "MailNewsTypes.h"
#include "nsString.h"

#define NS_IDBFOLDERINFO_IID                              \
{ /* 5cb11c00-cb8b-11d2-8d67-00805f8a6617 */         \
    0x5cb11c00,                                      \
    0xcb8b,                                          \
    0x11d2,                                          \
    {0x8d, 0x67, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17} \
}

class nsIDBFolderInfo : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IDBFOLDERINFO_IID; return iid; }

    NS_IMETHOD			GetFlags(PRInt32 *result) = 0 ;
    NS_IMETHOD			SetFlags(PRInt32 flags) = 0;
    NS_IMETHOD			OrFlags(PRInt32 flags, PRInt32 *result) = 0;
    NS_IMETHOD			AndFlags(PRInt32 flags, PRInt32 *result) = 0;
	NS_IMETHOD			SetHighWater(nsMsgKey highWater, PRBool force = FALSE) = 0;
	NS_IMETHOD			GetHighWater(nsMsgKey *result) = 0;
	NS_IMETHOD			SetExpiredMark(nsMsgKey expiredKey) = 0;
	NS_IMETHOD			ChangeNumNewMessages(PRInt32 delta) = 0;
	NS_IMETHOD			ChangeNumMessages(PRInt32 delta) = 0;
	NS_IMETHOD			ChangeNumVisibleMessages(PRInt32 delta) = 0;
	NS_IMETHOD			GetNumNewMessages(PRInt32 *result)  = 0;
	NS_IMETHOD			GetNumMessages(PRInt32 *result)  = 0;
	NS_IMETHOD			GetNumVisibleMessages(PRInt32 *result)  = 0;
	NS_IMETHOD			GetImapUidValidity(PRInt32 *result)  = 0;
	NS_IMETHOD			SetImapUidValidity(PRInt32 uidValidity)  = 0;
    NS_IMETHOD			GetImapTotalPendingMessages(PRInt32 *result) = 0;
    NS_IMETHOD			GetImapUnreadPendingMessages(PRInt32 *result) = 0;
    NS_IMETHOD			GetCSID(PRInt16 *result) = 0;
    
	NS_IMETHOD			SetVersion(PRUint32 version)  = 0;
	NS_IMETHOD			GetVersion(PRUint32 *result) = 0;

	NS_IMETHOD			GetLastMessageLoaded(nsMsgKey *result) = 0;
	NS_IMETHOD			SetLastMessageLoaded(nsMsgKey lastLoaded) = 0;

	NS_IMETHOD			GetFolderSize(PRUint32 *size) = 0;
	NS_IMETHOD			SetFolderSize(PRUint32 size) = 0;
	NS_IMETHOD			GetFolderDate(time_t *date) = 0;
	NS_IMETHOD			SetFolderDate(time_t date) = 0;

    NS_IMETHOD			GetDiskVersion(int *version) = 0;

    NS_IMETHOD          ChangeExpungedBytes(PRInt32 delta) = 0;

	NS_IMETHOD			GetProperty(const char *propertyName, nsString &resultProperty) = 0;
	NS_IMETHOD			SetProperty(const char *propertyName, nsString &propertyStr) = 0;
	NS_IMETHOD			SetUint32Property(const char *propertyName, PRUint32 propertyValue) = 0;
	NS_IMETHOD			GetUint32Property(const char *propertyName, PRUint32 &propertyValue) = 0;
};

#endif // nsIDBFolderInfo_h__

