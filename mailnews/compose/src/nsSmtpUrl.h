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
#include "nsIURI.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIFileSpec.h"
#include "nsIMsgIdentity.h"
#include "nsCOMPtr.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsISmtpServer.h"
#include "nsIInterfaceRequestor.h"

class nsMailtoUrl : public nsIMailtoUrl, public nsIURI
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURI
    NS_DECL_NSIMAILTOURL

    nsMailtoUrl();

protected:
  virtual ~nsMailtoUrl();
  nsresult ParseUrl();
  nsresult CleanupMailtoState();
  nsresult ParseMailtoUrl(char * searchPart);

  nsCOMPtr<nsIURI> m_baseURL;
    
  // data retrieved from parsing the url: (Note the url could be a post from file or it could be in the url)
  nsCString m_toPart;
	nsCString m_ccPart;
	nsCString	m_subjectPart;
	nsCString	m_newsgroupPart;
	nsCString	m_newsHostPart;
	nsCString	m_referencePart;
	nsCString	m_attachmentPart;
	nsCString	m_bodyPart;
	nsCString	m_bccPart;
	nsCString	m_followUpToPart;
	nsCString	m_fromPart;
	nsCString	m_htmlPart;
	nsCString	m_organizationPart;
	nsCString	m_replyToPart;
	nsCString	m_priorityPart;

  PRBool	  m_forcePlainText;
};

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
	virtual const char * GetUserName() { return m_userName.get();}

	// data retrieved from parsing the url: (Note the url could be a post from file or it could be inthe url)
  nsCString   m_toPart;

	PRBool		m_isPostMessage;

	/* Smtp specific event sinks */
	nsCString	m_userName;
	nsCOMPtr<nsIFileSpec> m_fileName;
	nsCOMPtr<nsIMsgIdentity> m_senderIdentity;
    nsCOMPtr<nsIPrompt> m_netPrompt;
    nsCOMPtr<nsIAuthPrompt> m_netAuthPrompt;
    nsCOMPtr<nsIInterfaceRequestor> m_callbacks;
    nsCOMPtr<nsISmtpServer> m_smtpServer;

	// it is possible to encode the message to parse in the form of a url.
	// This function is used to decompose the search and path part into the bare
	// message components (to, fcc, bcc, etc.)
	nsresult ParseMessageToPost(char * searchPart);
};

#endif // nsSmtpUrl_h__
