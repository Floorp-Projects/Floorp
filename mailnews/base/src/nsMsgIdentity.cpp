/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgIdentity.h"
#include "nsIPref.h"

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS(nsMsgIdentity, nsIMsgIdentity::GetIID());


nsMsgIdentity::nsMsgIdentity()
{
	NS_INIT_REFCNT();

	m_popName = nsnull;
	m_smtpName = nsnull;
	m_smtpHost = nsnull;
	m_popHost = nsnull;

	m_organization = nsnull;
	m_userFullName = nsnull;
	m_userEmail = nsnull;
	m_userPassword = nsnull;
	m_replyTo = nsnull;

	m_rootPath = nsnull;

	m_imapName = nsnull;
	m_imapHost = nsnull;
	m_imapPassword = nsnull;

	InitializeIdentity();
}

nsMsgIdentity::~nsMsgIdentity()
{
	PR_FREEIF(m_smtpName);
	PR_FREEIF(m_popName);
	PR_FREEIF(m_organization);
	PR_FREEIF(m_userFullName);
	PR_FREEIF(m_userEmail);
	PR_FREEIF(m_userPassword);
	PR_FREEIF(m_replyTo);
	PR_FREEIF(m_smtpHost);
	PR_FREEIF(m_popHost);
	PR_FREEIF(m_rootPath);
	PR_FREEIF(m_imapName);
	PR_FREEIF(m_imapHost);
	PR_FREEIF(m_imapPassword);
}

void nsMsgIdentity::InitializeIdentity()
{
	// propogating bienvenu's preferences hack.....
	#define PREF_LENGTH 128 
	char prefValue[PREF_LENGTH];
	PRInt32 prefLength = PREF_LENGTH;
	nsIPref* prefs = nsnull;
	nsresult rv;
	rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
    if (prefs && NS_SUCCEEDED(rv))
	{
		prefs->Startup("prefs.js");

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.rootFolder", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_rootPath = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.identity.organization", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_organization = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.identity.username", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_userFullName = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.identity.useremail", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_userEmail = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.identity.reply_to", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_replyTo = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("network.hosts.pop_server", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_popHost = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("network.hosts.smtp_server", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_smtpHost = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.pop_name", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_popName = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.smtp_name", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_smtpName = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.pop_password", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_userPassword = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.imap_name", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_imapName = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.imap_password", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_imapPassword = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mail.imap_host", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_imapHost = PL_strdup(prefValue);
		
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}
	else
		NS_ASSERTION(0, "unable to create the prefs service!!");
}

nsresult nsMsgIdentity::GetPopName(const char ** aUserName)
{
	if (aUserName)
		*aUserName = m_popName;
	return NS_OK;
}

nsresult nsMsgIdentity::GetSmtpName(const char ** aSmtpName)
{
	if (aSmtpName)
	{
		if (m_smtpName && *m_smtpName)  
			*aSmtpName = m_smtpName;
		else							// if we don't have a smtp name use the pop name...
			return GetPopName(aSmtpName); 
	}

	return NS_OK;
}

nsresult nsMsgIdentity::GetOrganization(const char ** aOrganization)
{
	if (aOrganization)
		*aOrganization = m_organization;
	return NS_OK;
}

nsresult nsMsgIdentity::GetUserFullName(const char ** aUserFullName)
{
	if (aUserFullName)
		*aUserFullName = m_userFullName;
	return NS_OK;
}

nsresult nsMsgIdentity::GetUserEmail(const char ** aUserEmail)
{
	if (aUserEmail)
		*aUserEmail = m_userEmail;
	return NS_OK;
}

nsresult nsMsgIdentity::GetPopPassword(const char ** aUserPassword)
{
	if (aUserPassword)
		*aUserPassword = m_userPassword;
	return NS_OK;
}


nsresult nsMsgIdentity::GetPopServer(const char ** aHostName)
{
	if (aHostName)
		*aHostName = m_popHost;
	return NS_OK;
}

nsresult nsMsgIdentity::GetSmtpServer(const char ** aHostName)
{
	if (aHostName)
		*aHostName = m_smtpHost;
	return NS_OK;
}

nsresult nsMsgIdentity::GetRootFolderPath(const char ** aRootFolderPath)
{
	if (aRootFolderPath)
		*aRootFolderPath = m_rootPath;
	return NS_OK;
}	

nsresult nsMsgIdentity::GetReplyTo(const char ** aReplyTo)
{
	if (aReplyTo)
		*aReplyTo = m_replyTo;
	return NS_OK;
}	

nsresult nsMsgIdentity::GetImapServer(const char ** aHostName)
{
	if (aHostName)
		*aHostName = m_imapHost;
	return NS_OK;
}

nsresult nsMsgIdentity::GetImapName(const char ** aImapName)
{
	if (aImapName)
		*aImapName = m_imapName;
	return NS_OK;
}

nsresult nsMsgIdentity::GetImapPassword(const char ** aImapPassword)
{
	if (aImapPassword)
		*aImapPassword = m_imapPassword;
	return NS_OK;
}
