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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsSmtpUrl_h__
#define nsSmtpUrl_h__

#include "nsISmtpUrl.h"
#include "nsMsgMailNewsUrl.h"

class nsSmtpUrl : public nsISmtpUrl, public nsMsgMailNewsUrl
{
public:
	NS_DECL_ISUPPORTS_INHERITED

	// nsIURI over-ride...
	NS_IMETHOD SetSpec(char * aSpec);

	// From nsISmtpUrl

	// mscott: I used to have individual getters for ALL of these fields but it was
	// getting way out of hand...besides in the actual protocol, we want all of these
	// fields anyway so why go through the extra step of making the protocol call
	// 12 get functions...
	NS_IMETHOD GetMessageContents(const char ** aToPart, const char ** aCcPart, const char ** aBccPart, 
		const char ** aFromPart, const char ** aFollowUpToPart, const char ** aOrganizationPart, 
		const char ** aReplyToPart, const char ** aSubjectPart, const char ** aBodyPart, const char ** aHtmlPart, 
		const char ** aReferencePart, const char ** aAttachmentPart, const char ** aPriorityPart, 
		const char ** aNewsgroupPart, const char ** aNewsHostPart, PRBool * aforcePlainText);

	// Caller must call PR_FREE on list when it is done with it. This list is a list of all
	// recipients to send the email to. each name is NULL terminated...
	NS_IMETHOD GetAllRecipients(char ** aRecipientsList);

	// is the url a post message url or a bring up the compose window url? 
	NS_IMETHOD IsPostMessage(PRBool * aPostMessage); 
	
	// used to set the url as a post message url...
	NS_IMETHOD SetPostMessage(PRBool aPostMessage);

	// the message can be stored in a file....allow accessors for getting and setting
	// the file name to post...
	NS_IMETHOD SetPostMessageFile(const nsFilePath& aFileName);
	NS_IMETHOD GetPostMessageFile(const nsFilePath ** aFileName);

	/////////////////////////////////////////////////////////////////////////////// 
	// SMTP Url instance specific getters and setters --> info the protocol needs
	// to know in order to run the url...these are NOT event sinks which are things
	// the caller needs to know...
	///////////////////////////////////////////////////////////////////////////////

	// mscott -- when we have identities it would be nice to just have an identity 
	// interface here that would encapsulte things like username, domain, password,
	// etc...
	NS_IMETHOD GetUserEmailAddress(const char ** aUserName);
	NS_IMETHOD SetUserEmailAddress(const char * aUserName);

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

	/* Smtp specific event sinks */
	nsCString	m_userName;
	nsFilePath  m_fileName;

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
