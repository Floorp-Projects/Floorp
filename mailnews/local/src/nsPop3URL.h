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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsPop3URL_h__
#define nsPop3URL_h__

#include "nsIPop3URL.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIMsgIncomingServer.h"
#include "nsCOMPtr.h"

class nsPop3URL : public nsIPop3URL, public nsMsgMailNewsUrl
{
public:
    NS_DECL_NSIPOP3URL
    nsPop3URL();
	
    NS_DECL_ISUPPORTS_INHERITED

protected:
	virtual ~nsPop3URL();
	// protocol specific code to parse a url...
    virtual nsresult ParseUrl(const nsString& aSpec);
	virtual const char * GetUserName() { return m_userName.GetBuffer();}

	nsCString m_userName;
    nsCString m_messageUri;

	/* Pop3 specific event sinks */
    nsCOMPtr<nsIPop3Sink> m_pop3Sink;
};

#endif // nsPop3URL_h__
