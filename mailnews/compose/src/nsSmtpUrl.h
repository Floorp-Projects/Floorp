/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIInterfaceRequestorUtils.h"

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
