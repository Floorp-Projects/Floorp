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

#ifndef _nsIMsgThread_H
#define _nsIMsgThread_H

#include "nsISupports.h"
#include "nsString.h"
#include "MailNewsTypes.h"
#include "mdb.h"

class nsIMessage;
class nsIEnumerator;

#define NS_IMSGTHREAD_IID                           \
{/* df64af90-e2da-11d2-8d6c-00805f8a6617	*/		\
	0xdf64af90,										\
	0xe2da,											\
	0x11d2,											\
	0x8d, 0x6c, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17 \
}

class nsIMsgThread : public nsISupports {
public:
 
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGTHREAD_IID; return iid; }
	NS_IMETHOD		SetThreadKey(nsMsgKey threadKey) = 0;
	NS_IMETHOD		GetThreadKey(nsMsgKey *result) = 0;
    NS_IMETHOD		GetFlags(PRUint32 *result) = 0;
    NS_IMETHOD		SetFlags(PRUint32 flags) = 0;
	NS_IMETHOD		GetNumChildren(PRUint32 *result) = 0;
	NS_IMETHOD		GetNumUnreadChildren (PRUint32 *result) = 0;
	NS_IMETHOD		AddChild(nsIMessage *child, PRBool threadInThread) = 0;
	NS_IMETHOD		GetChildAt(PRInt32 index, nsIMessage **result) = 0;
	NS_IMETHOD		GetChild(nsMsgKey msgKey, nsIMessage **result) = 0;
	NS_IMETHOD		GetChildHdrAt(PRInt32 index, nsIMessage **result) = 0;
	NS_IMETHOD 		RemoveChildAt(PRInt32 index) = 0;
	NS_IMETHOD		RemoveChild(nsMsgKey msgKey) = 0;
	NS_IMETHOD		MarkChildRead(PRBool bRead) = 0;
	NS_IMETHOD		EnumerateMessages(nsMsgKey parent, nsIEnumerator* *result) = 0;
};

#endif
