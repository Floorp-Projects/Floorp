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

#ifndef nsSmtpUrl_h__
#define nsSmtpUrl_h__

#include "nsISmtpUrl.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIFileSpec.h"
#include "nsIMsgIdentity.h"
#include "nsCOMPtr.h"

class nsSmtpUrl : public nsISmtpUrl, public nsMsgMailNewsUrl
{
public:
	NS_DECL_ISUPPORTS_INHERITED

	// nsIURI over-ride...
	NS_IMETHOD SetSpec(const char * aSpec);

	// From nsISmtpUrl
	NS_DECL_NSISMTPURL

	// nsSmtpUrl
    nsSmtpUrl();

protected:
    virtual ~nsSmtpUrl();
	// protocol specific code to parse a url...
    virtual nsresult ParseUrl();
	virtual const char * GetUserName() { return m_userName.GetBuffer();}

	// data retrieved from parsing the url: (Note the url could be a post from file or it could be inthe url)
    char		*m_toPart;
	char		*m_ccPart;
	char		*m_subjectPart;
	char		*m_newsgroupPart;
	char		*m_newsHostPart;
	char		*m_referencePart;
	char		*m_attachmentPart;
	char	    *m_bodyPart;
	char		*m_bccPart;
	char		*m_followUpToPart;
	char		*m_fromPart;
	char		*m_htmlPart;
	char		*m_organizationPart;
	char		*m_replyToPart;
	char		*m_priorityPart;


	PRBool	    m_forcePlainText;
	PRBool		m_isPostMessage;

	/* Smtp specific event sinks */
	nsCString	m_userName;
	nsCOMPtr<nsIFileSpec> m_fileName;
	nsCOMPtr<nsIMsgIdentity> m_senderIdentity;

	// it is possible to encode the message to parse in the form of a url.
	// This function is used to decompose the search and path part into the bare
	// message components (to, fcc, bcc, etc.)
	nsresult ParseMessageToPost(char * searchPart);
	// generic function to clear out our local url state...
	nsresult CleanupSmtpState();
};

// factory method
extern nsresult NS_NewSmtpUrl(const nsIID &aIID, void ** aInstancePtrResult);

#endif // nsSmtpUrl_h__
