/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  Outlook Express (Win32) settings

*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsReadableUtils.h"
#include "nsOutlookImport.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsOutlookRegUtil.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgAccount.h"
#include "nsIImportSettings.h"
#include "nsOutlookSettings.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsOutlookStringBundle.h"
#include "OutlookDebugLog.h"
#include "nsIPop3IncomingServer.h"
#include "nsIMessengerMigrator.h"

class OutlookSettings {
public:
	static HKEY FindAccountsKey( void);

	static PRBool DoImport( nsIMsgAccount **ppAccount);

	static PRBool DoIMAPServer( nsIMsgAccountManager *pMgr, HKEY hKey, char *pServerName, nsIMsgAccount **ppAccount);
	static PRBool DoPOP3Server( nsIMsgAccountManager *pMgr, HKEY hKey, char *pServerName, nsIMsgAccount **ppAccount);
	
	static void SetIdentities( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, HKEY hKey);
	static PRBool IdentityMatches( nsIMsgIdentity *pIdent, const char *pName, const char *pServer, const char *pEmail, const char *pReply, const char *pUserName);

	static void SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, char *pServer, char *pUser);
  static nsresult GetAccountName(HKEY hKey, char *defaultName, nsString &acctName);
};


////////////////////////////////////////////////////////////////////////
nsresult nsOutlookSettings::Create(nsIImportSettings** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new nsOutlookSettings();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

nsOutlookSettings::nsOutlookSettings()
{
}

nsOutlookSettings::~nsOutlookSettings()
{
}

NS_IMPL_ISUPPORTS1(nsOutlookSettings, nsIImportSettings)

NS_IMETHODIMP nsOutlookSettings::AutoLocate(PRUnichar **description, nsIFileSpec **location, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!description || !_retval)
		return( NS_ERROR_NULL_POINTER);
	
	*description = nsOutlookStringBundle::GetStringByID( OUTLOOKIMPORT_NAME);
	*_retval = PR_FALSE;

	if (location)
		*location = nsnull;

	// look for the registry key for the accounts
	HKEY key = OutlookSettings::FindAccountsKey();
	if (key != nsnull) {
		*_retval = PR_TRUE;
		::RegCloseKey( key);
	}

	return( NS_OK);
}

NS_IMETHODIMP nsOutlookSettings::SetLocation(nsIFileSpec *location)
{
	return( NS_OK);
}

NS_IMETHODIMP nsOutlookSettings::Import(nsIMsgAccount **localMailAccount, PRBool *_retval)
{
	NS_PRECONDITION( _retval != nsnull, "null ptr");
	
	if (OutlookSettings::DoImport( localMailAccount)) {
		*_retval = PR_TRUE;
		IMPORT_LOG0( "Settings import appears successful\n");
	}
	else {
		*_retval = PR_FALSE;
		IMPORT_LOG0( "Settings import returned FALSE\n");
	}

	return( NS_OK);
}


HKEY OutlookSettings::FindAccountsKey( void)
{
	HKEY	sKey;
	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Office\\Outlook\\OMI Account Manager\\Accounts", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &sKey) == ERROR_SUCCESS) {
		return( sKey);
	}
	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Office\\8.0\\Outlook\\OMI Account Manager\\Accounts", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &sKey) == ERROR_SUCCESS) {
		return( sKey);
	}

	return( nsnull);
}

PRBool OutlookSettings::DoImport( nsIMsgAccount **ppAccount)
{
	HKEY	hKey = FindAccountsKey();
	if (hKey == nsnull) {
		IMPORT_LOG0( "*** Error finding Outlook registry account keys\n");
		return( PR_FALSE);
	}

	nsresult	rv;
	
	nsCOMPtr<nsIMsgAccountManager> accMgr = 
	         do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to create a account manager!\n");
		return( PR_FALSE);
	}

	HKEY		subKey = NULL;
	nsCString	defMailName;

	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Office\\Outlook\\OMI Account Manager", 0, KEY_QUERY_VALUE, &subKey) != ERROR_SUCCESS)
		if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Office\\8.0\\Outlook\\OMI Account Manager", 0, KEY_QUERY_VALUE, &subKey) != ERROR_SUCCESS)
			subKey = NULL;

	if (subKey != NULL) {
		// First let's get the default mail account key name
		BYTE *	pBytes = nsOutlookRegUtil::GetValueBytes( subKey, "Default Mail Account");
		::RegCloseKey( subKey);
		if (pBytes) {
			defMailName = (const char *)pBytes;
			nsOutlookRegUtil::FreeValueBytes( pBytes);
		}
	}

	// Iterate the accounts looking for POP3 & IMAP accounts...
	// Ignore LDAP & NNTP for now!
	DWORD		index = 0;
	DWORD		numChars;
	TCHAR		keyName[256];
	FILETIME	modTime;
	LONG		result = ERROR_SUCCESS;
	BYTE *		pBytes;
	int			popCount = 0;
	int			accounts = 0;
	nsCString	keyComp;

	while (result == ERROR_SUCCESS) {
		numChars = 256;
		result = ::RegEnumKeyEx( hKey, index, keyName, &numChars, NULL, NULL, NULL, &modTime);
		index++;
		if (result == ERROR_SUCCESS) {
			if (::RegOpenKeyEx( hKey, keyName, 0, KEY_QUERY_VALUE, &subKey) == ERROR_SUCCESS) {
				// Get the values for this account.
				IMPORT_LOG1( "Opened Outlook account: %s\n", (char *)keyName);

				nsIMsgAccount	*anAccount = nsnull;
				pBytes = nsOutlookRegUtil::GetValueBytes( subKey, "IMAP Server");
				if (pBytes) {
					if (DoIMAPServer( accMgr, subKey, (char *)pBytes, &anAccount))
						accounts++;
					nsOutlookRegUtil::FreeValueBytes( pBytes);
				}

				pBytes = nsOutlookRegUtil::GetValueBytes( subKey, "POP3 Server");
				if (pBytes) {
					if (popCount == 0) {
						if (DoPOP3Server( accMgr, subKey, (char *)pBytes, &anAccount)) {
							popCount++;
							accounts++;
							if (ppAccount && anAccount) {
								*ppAccount = anAccount;
								NS_ADDREF( anAccount);
							}
						}
					}
					else {
						if (DoPOP3Server( accMgr, subKey, (char *)pBytes, &anAccount)) {
							popCount++;
							accounts++;
							// If we created a mail account, get rid of it since
							// we have 2 POP accounts!
							if (ppAccount && *ppAccount) {
								NS_RELEASE( *ppAccount);
								*ppAccount = nsnull;
							}
						}
					}
					nsOutlookRegUtil::FreeValueBytes( pBytes);
				}
				
				if (anAccount) {
					// Is this the default account?
					keyComp = keyName;
					if (keyComp.Equals( defMailName)) {
						accMgr->SetDefaultAccount( anAccount);
					}
					NS_RELEASE( anAccount);
				}

				::RegCloseKey( subKey);
			}
		}
	}

	// Now save the new acct info to pref file.
	rv = accMgr->SaveAccountInfo();
	NS_ASSERTION(NS_SUCCEEDED(rv), "Can't save account info to pref file");

	return( accounts != 0);
}

nsresult OutlookSettings::GetAccountName(HKEY hKey, char *defaultName, nsString &acctName)
{
  BYTE *pAccName = nsOutlookRegUtil::GetValueBytes( hKey, "Account Name");
  nsresult rv = NS_OK;
  if (pAccName) {
    nsCOMPtr<nsIImportService> impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
    if (impSvc)
      rv = impSvc->SystemStringToUnicode((const char *)pAccName, acctName);
    nsOutlookRegUtil::FreeValueBytes( pAccName);
  }
  else
    acctName.Assign(NS_ConvertASCIItoUCS2(defaultName));
  return rv;
}

PRBool OutlookSettings::DoIMAPServer( nsIMsgAccountManager *pMgr, HKEY hKey, char *pServerName, nsIMsgAccount **ppAccount)
{
	if (ppAccount)
		*ppAccount = nsnull;

	BYTE *pBytes;
	pBytes = nsOutlookRegUtil::GetValueBytes( hKey, "IMAP User Name");
	if (!pBytes)
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	// I now have a user name/server name pair, find out if it already exists?
	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = pMgr->FindServer( (const char *)pBytes, pServerName, "imap", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = pMgr->CreateIncomingServer( (const char *)pBytes, pServerName, "imap", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "imap");
			// rv = in->SetHostName( pServerName);
			// rv = in->SetUsername( (char *)pBytes);
			
			IMPORT_LOG2( "Created IMAP server named: %s, userName: %s\n", pServerName, (char *)pBytes);

			nsString	prettyName;
      if (NS_SUCCEEDED(GetAccountName(hKey, pServerName, prettyName)))
      {
			PRUnichar *pretty = ToNewUnicode(prettyName);
        if (pretty)
        {
			rv = in->SetPrettyName( pretty);
			nsCRT::free( pretty);
        }
      }
			
			// We have a server, create an account.
			nsCOMPtr<nsIMsgAccount>	account;
			rv = pMgr->CreateAccount( getter_AddRefs( account));
			if (NS_SUCCEEDED( rv) && account) {
				rv = account->SetIncomingServer( in);	
				
				IMPORT_LOG0( "Created an account and set the IMAP server as the incoming server\n");

				// Fiddle with the identities
				SetIdentities( pMgr, account, hKey);
				result = PR_TRUE;
				if (ppAccount)
					account->QueryInterface( NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
			}				
		}
	}
	else
		result = PR_TRUE;
	
	nsOutlookRegUtil::FreeValueBytes( pBytes);

	return( result);
}

PRBool OutlookSettings::DoPOP3Server( nsIMsgAccountManager *pMgr, HKEY hKey, char *pServerName, nsIMsgAccount **ppAccount)
{
	if (ppAccount)
		*ppAccount = nsnull;

	BYTE *pBytes;
	pBytes = nsOutlookRegUtil::GetValueBytes( hKey, "POP3 User Name");
	if (!pBytes)
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	// I now have a user name/server name pair, find out if it already exists?
	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = pMgr->FindServer( (const char *)pBytes, pServerName, "pop3", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = pMgr->CreateIncomingServer( (const char *)pBytes, pServerName, "pop3", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "pop3");

        nsCOMPtr<nsIPop3IncomingServer> pop3Server = do_QueryInterface(in);
        if (pop3Server) {
            // set local folders as the Inbox to use for this POP3 server
            nsCOMPtr<nsIMsgIncomingServer> localFoldersServer;
            pMgr->GetLocalFoldersServer(getter_AddRefs(localFoldersServer));

            if (!localFoldersServer)
            {
                // XXX: We may need to move this local folder creation code to the generic nsImportSettings code
                // if the other import modules end up needing to do this too.    
                // if Local Folders does not exist already, create it
                nsCOMPtr <nsIMessengerMigrator> messengerMigrator = do_GetService(NS_MESSENGERMIGRATOR_CONTRACTID, &rv);
                if (NS_FAILED(rv)) {
                    IMPORT_LOG0( "*** Failed to create messenger migrator!\n");
                    return PR_FALSE;
                }
          
                rv = messengerMigrator->CreateLocalMailAccount(PR_FALSE);
                if (NS_FAILED(rv)) {
                    IMPORT_LOG0( "*** Failed to create Local Folders!\n");
                    return PR_FALSE;
                }
    
                pMgr->GetLocalFoldersServer(getter_AddRefs(localFoldersServer)); 
            }

            // now get the account for this server
            nsCOMPtr<nsIMsgAccount> localFoldersAccount;
            pMgr->FindAccountForServer(localFoldersServer, getter_AddRefs(localFoldersAccount)); 
            if (localFoldersAccount)
            {
                nsXPIDLCString localFoldersAcctKey;
                localFoldersAccount->GetKey(getter_Copies(localFoldersAcctKey));
                pop3Server->SetDeferredToAccount(localFoldersAcctKey.get()); 
                pop3Server->SetDeferGetNewMail(PR_TRUE);
            }
        }

			IMPORT_LOG2( "Created POP3 server named: %s, userName: %s\n", pServerName, (char *)pBytes);

			nsString	prettyName;
      if (NS_SUCCEEDED(GetAccountName(hKey, pServerName, prettyName)))
      {
			PRUnichar *pretty = ToNewUnicode(prettyName);
        if (pretty)
        {
			rv = in->SetPrettyName( pretty);
			nsCRT::free( pretty);
        }
      }
			
			// We have a server, create an account.
			nsCOMPtr<nsIMsgAccount>	account;
			rv = pMgr->CreateAccount( getter_AddRefs( account));
			if (NS_SUCCEEDED( rv) && account) {
				rv = account->SetIncomingServer( in);
				
				IMPORT_LOG0( "Created a new account and set the incoming server to the POP3 server.\n");
					
        nsCOMPtr<nsIPop3IncomingServer> pop3Server = do_QueryInterface(in, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
        BYTE *pLeaveOnServer = nsOutlookRegUtil::GetValueBytes( hKey, "Leave Mail On Server");
        if (pLeaveOnServer)
        {
          pop3Server->SetLeaveMessagesOnServer(*pLeaveOnServer == 1 ? PR_TRUE : PR_FALSE);
          nsOutlookRegUtil::FreeValueBytes(pLeaveOnServer);
        }

        // Fiddle with the identities
				SetIdentities( pMgr, account, hKey);
				result = PR_TRUE;
				if (ppAccount)
					account->QueryInterface( NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
			}				
		}
	}
	else
		result = PR_TRUE;
	
	nsOutlookRegUtil::FreeValueBytes( pBytes);

	return( result);
}

PRBool OutlookSettings::IdentityMatches( nsIMsgIdentity *pIdent, const char *pName, const char *pServer, const char *pEmail, const char *pReply, const char *pUserName)
{
	if (!pIdent)
		return( PR_FALSE);

	char *	pIName = nsnull;
	char *	pIEmail = nsnull;
	char *	pIReply = nsnull;
	
	PRBool	result = PR_TRUE;

	// The test here is:
	// If the smtp host is the same
	//	and the email address is the same (if it is supplied)
	//	and the reply to address is the same (if it is supplied)
	//	then we match regardless of the full name.
	
	PRUnichar *ppIName = nsnull;

	nsresult rv = pIdent->GetFullName( &ppIName);
	rv = pIdent->GetEmail( &pIEmail);
	rv = pIdent->GetReplyTo( &pIReply);
	
	if (ppIName) {
		nsString name(ppIName);
		nsCRT::free( ppIName);
		pIName = ToNewCString(name);
	}

	// for now, if it's the same server and reply to and email then it matches
	if (pReply) {
		if (!pIReply || nsCRT::strcasecmp( pReply, pIReply))
			result = PR_FALSE;
	}
	if (pEmail) {
		if (!pIEmail || nsCRT::strcasecmp( pEmail, pIEmail))
			result = PR_FALSE;
	}

	nsCRT::free( pIName);
	nsCRT::free( pIEmail);
	nsCRT::free( pIReply);

	return( result);
}

void OutlookSettings::SetIdentities( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, HKEY hKey)
{
	// Get the relevant information for an identity
	char *pName = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP Display Name");
	char *pServer = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP Server");
	char *pEmail = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP Email Address");
	char *pReply = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP Reply To Email Address");
	char *pUserName = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP User Name");
  char *pOrgName = (char *)nsOutlookRegUtil::GetValueBytes( hKey, "SMTP Organization Name");

	nsresult	rv;

	if (pEmail && pName && pServer) {
		// The default identity, nor any other identities matched,
		// create a new one and add it to the account.
		nsCOMPtr<nsIMsgIdentity>	id;
		rv = pMgr->CreateIdentity( getter_AddRefs( id));
		if (id) {
      nsAutoString name, organization;
      nsCOMPtr<nsIImportService> impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
      if (impSvc)
      {
        rv = impSvc->SystemStringToUnicode((const char *)pName, name);
        if (NS_SUCCEEDED(rv))
        {
			id->SetFullName( name.get());
			id->SetIdentityName( name.get());
        }
        rv = impSvc->SystemStringToUnicode((const char *)pOrgName, organization);
        if (NS_SUCCEEDED(rv))
          id->SetOrganization( organization.get());
      }
			id->SetEmail( pEmail);
			if (pReply)
				id->SetReplyTo( pReply);
			pAcc->AddIdentity( id);

			IMPORT_LOG0( "Created identity and added to the account\n");
			IMPORT_LOG1( "\tname: %s\n", pName);
			IMPORT_LOG1( "\temail: %s\n", pEmail);
		}
	}
	
  if (!pUserName) {
    nsCOMPtr <nsIMsgIncomingServer>	incomingServer;
    rv = pAcc->GetIncomingServer(getter_AddRefs( incomingServer));
    if (NS_SUCCEEDED(rv) && incomingServer) 
      rv = incomingServer->GetUsername(&pUserName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to get UserName from incomingServer");
  }

	SetSmtpServer( pMgr, pAcc, pServer, pUserName);

	nsOutlookRegUtil::FreeValueBytes( (BYTE *)pName);
	nsOutlookRegUtil::FreeValueBytes( (BYTE *)pServer);
	nsOutlookRegUtil::FreeValueBytes( (BYTE *)pEmail);
	nsOutlookRegUtil::FreeValueBytes( (BYTE *)pReply);
	nsOutlookRegUtil::FreeValueBytes( (BYTE *)pUserName);
}

void OutlookSettings::SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, char *pServer, char *pUser)
{
	nsresult	rv;


	nsCOMPtr<nsISmtpService> smtpService(do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv)); 
	if (NS_SUCCEEDED(rv) && smtpService) {
		nsCOMPtr<nsISmtpServer>		foundServer;
	
		rv = smtpService->FindServer( pUser, pServer, getter_AddRefs( foundServer));
		if (NS_SUCCEEDED( rv) && foundServer) {
			IMPORT_LOG1( "SMTP server already exists: %s\n", pServer);
			return;
		}
		nsCOMPtr<nsISmtpServer>		smtpServer;
		
		rv = smtpService->CreateSmtpServer( getter_AddRefs( smtpServer));
		if (NS_SUCCEEDED( rv) && smtpServer) {
			smtpServer->SetHostname( pServer);
			if (pUser)
				smtpServer->SetUsername( pUser);

			IMPORT_LOG1( "Created new SMTP server: %s\n", pServer);
		}
 	}
}


