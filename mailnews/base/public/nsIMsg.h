/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIMsg_h__
#define nsIMsg_h__

#include "nsIRDFNode.h"
#include "prtime.h"

class nsString;
class nsISupportsArray;
class nsIMsgFolder;

enum nsMsgPriority {
    nsMsgPriority_NotSet,
    nsMsgPriority_None,
    nsMsgPriority_Lowest,
    nsMsgPriority_Low,
    nsMsgPriority_Normal,
    nsMsgPriority_High,
    nsMsgPriority_Highest
};

class nsIMsg : public nsISupports {
public:

    NS_IMETHOD GetSubject(nsString* *str) = 0;
    NS_IMETHOD GetAuthor(nsString* *str) = 0;
    NS_IMETHOD GetMessageId(nsString* *str) = 0;
    NS_IMETHOD GetReferences(nsString* *str) = 0;
    NS_IMETHOD GetRecipients(nsString* *str) = 0;
    NS_IMETHOD GetDate(PRTime *result) = 0;
    NS_IMETHOD GetMessageSize(PRUint32 *size) = 0;
    NS_IMETHOD GetFlags(PRUint32 *size) = 0;
    NS_IMETHOD GetNumChildren(PRUint16 *num) = 0;
    NS_IMETHOD GetNumNewChildren(PRUint16 *num) = 0;
    NS_IMETHOD GetPriority(nsMsgPriority *priority) = 0;

    NS_IMETHOD GetContainingFolder(nsIMsgFolder* *result) = 0;

    NS_IMETHOD GetThreadChildren(nsISupportsArray* *result) = 0;
    NS_IMETHOD GetUnreadThreadChildren(nsISupportsArray* *result) = 0;

};

#define NS_IMSG_IID_STR "b9b8d1b0-bbec-11d2-8177-006008119d7a"
#define NS_IMSG_IID \
  {0xb9b8d1b0, 0xbbec, 0x11d2, \
      { 0x81, 0x77, 0x00, 0x60, 0x08, 0x11, 0xd, 0x7a } }

#endif // nsIMsg_h__
