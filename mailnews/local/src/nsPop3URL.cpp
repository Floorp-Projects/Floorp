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

#include "msgCore.h"    // precompiled header...

#include "nsIURL.h"
#include "nsPop3URL.h"
#include "nsPop3Protocol.h"
#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

nsresult NS_NewPopUrl(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (aInstancePtrResult)
	{
		nsPop3URL * popUrl = new nsPop3URL(); 
		if (popUrl)
			return popUrl->QueryInterface(nsIPop3URL::GetIID(), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

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
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
        m_pop3Sink = dont_QueryInterface(aPop3Sink);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetPop3Sink(nsIPop3Sink** aPop3Sink)
{
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
	{
        *aPop3Sink = m_pop3Sink;
		NS_IF_ADDREF(*aPop3Sink);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}


nsresult nsPop3URL::ParseUrl(const nsString& aSpec)
{
	// mscott - i don't believe I'm going to need this
	// method anymore..
	NS_ASSERTION(0, "we shouldn't need to call this method anymore");
    return NS_OK;
}

