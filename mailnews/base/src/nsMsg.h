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

#ifndef nsMsg_h__
#define nsMsg_h__

#include "nsIMsg.h"
#include "nsRDFResource.h"

class nsMsg : public nsRDFResource, public nsIMsg {
public:

    NS_DECL_ISUPPORTS

    // nsIMsg methods:
    NS_IMETHOD GetSubject(nsString* *str);
    NS_IMETHOD GetAuthor(nsString* *str);
    NS_IMETHOD GetMessageId(nsString* *str);
    NS_IMETHOD GetReferences(nsString* *str);
    NS_IMETHOD GetRecipients(nsString* *str);
    NS_IMETHOD GetDate(PRTime *result);
    NS_IMETHOD GetMessageSize(PRUint32 *size);
    NS_IMETHOD GetFlags(PRUint32 *size);
    NS_IMETHOD GetNumChildren(PRUint16 *num);
    NS_IMETHOD GetNumNewChildren(PRUint16 *num);
    NS_IMETHOD GetPriority(nsMsgPriority *priority);

    NS_IMETHOD GetContainingFolder(nsIMsgFolder* *result);

    NS_IMETHOD GetThreadChildren(nsISupportsArray* *result);
    NS_IMETHOD GetUnreadThreadChildren(nsISupportsArray* *result);

    // nsMsg methods:
    nsMsg(const char* uri);
    virtual ~nsMsg(void);
    nsresult Init(void);

protected:
    
};

#endif // nsMsg_h__
