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

#include "msgCore.h"    // precompiled header...

#include "nsMsg.h"
#include "nsISupportsArray.h"

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIMsgIID, NS_IMSG_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID, NS_IRDFNODE_IID);

////////////////////////////////////////////////////////////////////////////////

nsMsg::nsMsg(const char* uri)
    : nsRDFResource(uri)
{
}

nsresult
nsMsg::Init(void)
{
    return NS_OK;
}

nsMsg::~nsMsg(void)
{
}

NS_IMPL_ADDREF(nsMsg)
NS_IMPL_RELEASE(nsMsg)

NS_IMETHODIMP
nsMsg::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIMsgIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIMsg*, this);
        AddRef();
        return NS_OK;
    }
    return nsRDFResource::QueryInterface(iid, result);
}

////////////////////////////////////////////////////////////////////////////////
// nsIMsg methods:

NS_METHOD
nsMsg::GetSubject(nsString* *str)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetAuthor(nsString* *str)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetMessageId(nsString* *str)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetReferences(nsString* *str)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetRecipients(nsString* *str)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetDate(PRTime *result)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetMessageSize(PRUint32 *size)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetFlags(PRUint32 *size)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetNumChildren(PRUint16 *num)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetNumNewChildren(PRUint16 *num)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetPriority(nsMsgPriority *priority)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetContainingFolder(nsIMsgFolder* *result)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetThreadChildren(nsISupportsArray* *result)
{
    return NS_OK;
}

NS_METHOD
nsMsg::GetUnreadThreadChildren(nsISupportsArray* *result)
{
    return NS_OK;
}

