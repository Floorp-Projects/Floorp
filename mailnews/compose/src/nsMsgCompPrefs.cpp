/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgCompPrefs.h"

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 


nsMsgCompPrefs::nsMsgCompPrefs(void * identiy /*= nsnull*/)
{
	nsresult res;

	m_organization = nsnull;
	m_userFullName = nsnull;
	m_userEmail = nsnull;

	// get the current identity from the mail session....
	nsIMsgMailSession * mailSession = nsnull;
	res = nsServiceManager::GetService(kCMsgMailSessionCID,
	    							  nsIMsgMailSession::GetIID(),
                                      (nsISupports **) &mailSession);
	if (NS_SUCCEEDED(res) && mailSession) {
		nsIMsgIdentity * identity = nsnull;
		res = mailSession->GetCurrentIdentity(&identity);
		// now release the mail service because we are done with it
		nsServiceManager::ReleaseService(kCMsgMailSessionCID, mailSession);
		if (NS_SUCCEEDED(res) && identity) {
			const char * aString = nsnull;

			identity->GetOrganization(&aString);
			if (aString)
				m_organization = PL_strdup(aString);

			identity->GetUserFullName(&aString);
			if (aString)
				m_userFullName = PL_strdup(aString);

			identity->GetUserEmail(&aString);
			if (aString)
				m_userEmail = PL_strdup(aString);
			else
				NS_ASSERTION(0, "no email address defined for this user....");

			identity->GetReplyTo(&aString);
			if (aString)
				m_replyTo= PL_strdup(aString);


			// release the identity
			NS_IF_RELEASE(identity);
		} // if we have an identity
		else
			NS_ASSERTION(0, "no current identity found for this user....");
	}
}

nsMsgCompPrefs::~nsMsgCompPrefs()
{
	PR_FREEIF(m_organization);
	PR_FREEIF(m_userFullName);
	PR_FREEIF(m_userEmail);
}
