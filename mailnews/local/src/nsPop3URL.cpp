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

#include "msgCore.h"    // precompiled header...

#include "nsIURL.h"
#include "nsPop3URL.h"
#include "nsPop3Protocol.h"
#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

nsPop3URL::nsPop3URL(): nsMsgMailNewsUrl()
{
}
 
nsPop3URL::~nsPop3URL()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsPop3URL, nsMsgMailNewsUrl, nsIPop3URL)  
  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIPop3URL specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3URL::SetPop3Sink(nsIPop3Sink* aPop3Sink)
{
    if (aPop3Sink)
        m_pop3Sink = dont_QueryInterface(aPop3Sink);
    return NS_OK;
}

nsresult nsPop3URL::GetPop3Sink(nsIPop3Sink** aPop3Sink)
{
    if (aPop3Sink)
	{
        *aPop3Sink = m_pop3Sink;
		NS_IF_ADDREF(*aPop3Sink);
	}
    return NS_OK;
}

NS_IMETHODIMP nsPop3URL::SetUsername(const char *aUserName)
{
	nsresult rv = NS_OK;
	if (aUserName)
    {
		m_userName = aUserName;
        nsMsgMailNewsUrl::SetUsername(aUserName);
    }
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

NS_IMETHODIMP
nsPop3URL::GetMessageUri(char ** aMessageUri)
{
    if(!aMessageUri || m_messageUri.Length() == 0)
        return NS_ERROR_NULL_POINTER;
    *aMessageUri = m_messageUri.ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
nsPop3URL::SetMessageUri(const char *aMessageUri)
{
    if (aMessageUri)
        m_messageUri = aMessageUri;
    return NS_OK;
}

nsresult nsPop3URL::ParseUrl(const nsString& aSpec)
{
	// mscott - i don't believe I'm going to need this
	// method anymore..
	NS_ASSERTION(0, "we shouldn't need to call this method anymore");
    return NS_OK;
}



