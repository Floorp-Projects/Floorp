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

// part of prefs hack until they become a service...
extern "C" const nsID kIPrefIID = {0xa22ad7b0, 0xca86, 0x11d1, 0xa9, 0xa4, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4};
// {DC26E0E0-CA94-11d1-A9A4-00805F8A7AC4}
extern "C" const nsID kPrefCID = {0xdc26e0e0, 0xca94, 0x11d1, 0xa9, 0xa4, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4};

NS_IMPL_ISUPPORTS(nsMsgIdentity, nsIMsgIdentity::GetIID());


nsMsgIdentity::nsMsgIdentity()
{
	NS_INIT_REFCNT();
	m_userName = nsnull;
	m_userPassword = nsnull;
	m_smtpHost = nsnull;
	m_popHost = nsnull;
	m_rootPath = nsnull;
	InitializeIdentity();
}

nsMsgIdentity::~nsMsgIdentity()
{
	PR_FREEIF(m_userName);
	PR_FREEIF(m_userPassword);
	PR_FREEIF(m_smtpHost);
	PR_FREEIF(m_popHost);
	PR_FREEIF(m_rootPath);
}

void nsMsgIdentity::InitializeIdentity()
{
	// propogating bienvenu's preferences hack.....
	#define PREF_LENGTH 128 
	char prefValue[PREF_LENGTH];
	PRInt32 prefLength = PREF_LENGTH;
	nsIPref* prefs;
	nsresult rv;
	rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
    if (prefs && NS_SUCCEEDED(rv))
	{
		prefs->Startup("prefs.js");
		rv = prefs->GetCharPref("mailnews.rootFolder", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_rootPath = PL_strdup(prefValue);
		
		rv = prefs->GetCharPref("mailnews.pop_server", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_popHost = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mailnews.smtp_server", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_smtpHost = PL_strdup(prefValue);
		
		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mailnews.user_name", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_userName = PL_strdup(prefValue);

		prefLength = PREF_LENGTH;
		rv = prefs->GetCharPref("mailnews.pop_password", prefValue, &prefLength);
		if (NS_SUCCEEDED(rv) && prefLength > 0)
			m_userPassword = PL_strdup(prefValue);

		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}
}

nsresult nsMsgIdentity::GetPopPassword(const char ** aUserPassword)
{
	if (aUserPassword)
		*aUserPassword = m_userPassword;
	return NS_OK;
}

nsresult nsMsgIdentity::GetUserName(const char ** aUserName)
{
	if (aUserName)
		*aUserName = m_userName;
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
