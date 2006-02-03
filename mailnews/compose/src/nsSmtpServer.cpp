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
 *   Seth Spitzer <sspitzer@netscape.com>
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

#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsEscape.h"
#include "nsSmtpServer.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsIAuthPrompt.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsISmtpUrl.h"
#include "nsCRT.h"
#include "nsMsgUtils.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"

NS_IMPL_ADDREF(nsSmtpServer)
NS_IMPL_RELEASE(nsSmtpServer)
NS_INTERFACE_MAP_BEGIN(nsSmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISmtpServer)
NS_INTERFACE_MAP_END

nsSmtpServer::nsSmtpServer():
    mKey("")
{
    m_logonFailed = PR_FALSE;
    getPrefs();
}

nsSmtpServer::~nsSmtpServer()
{
}

NS_IMETHODIMP
nsSmtpServer::GetKey(char * *aKey)
{
    if (!aKey) return NS_ERROR_NULL_POINTER;
    if (mKey.IsEmpty())
        *aKey = nsnull;
    else
        *aKey = ToNewCString(mKey);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetKey(const char * aKey)
{
    NS_ASSERTION(aKey, "Bad key pointer");
    mKey = aKey;
    return getPrefs();
}

nsresult nsSmtpServer::getPrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString branchName;
    branchName.AssignLiteral("mail.smtpserver.");
    branchName += mKey;
    branchName.Append('.');
    rv = prefs->GetBranch(branchName.get(), getter_AddRefs(mPrefBranch));
    if (NS_FAILED(rv))
        return rv;

    if(!mDefPrefBranch) {
        branchName.AssignLiteral("mail.smtpserver.default.");
        rv = prefs->GetBranch(branchName.get(), getter_AddRefs(mDefPrefBranch));
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetHostname(char * *aHostname)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aHostname);
    rv = mPrefBranch->GetCharPref("hostname", aHostname);
    if (NS_FAILED(rv))
        *aHostname=nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetHostname(const char * aHostname)
{
    if (aHostname)
        return mPrefBranch->SetCharPref("hostname", aHostname);
    else
        mPrefBranch->ClearUserPref("hostname");
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetDescription(nsACString &aDescription)
{
    nsXPIDLCString temp;
    mPrefBranch->GetCharPref("description", getter_Copies(temp));
    aDescription.Assign(temp);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetDescription(const nsACString &aDescription)
{
    if (!aDescription.IsEmpty())
        return mPrefBranch->SetCharPref("description", PromiseFlatCString(aDescription).get());
    else
        mPrefBranch->ClearUserPref("description");
    return NS_OK;
}

// if GetPort returns 0, it means default port
NS_IMETHODIMP
nsSmtpServer::GetPort(PRInt32 *aPort)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aPort);
    rv = mPrefBranch->GetIntPref("port", aPort);
    if (NS_FAILED(rv))
        *aPort = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetPort(PRInt32 aPort)
{
    if (aPort)
        return mPrefBranch->SetIntPref("port", aPort);
    else
        mPrefBranch->ClearUserPref("port");
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetDisplayname(char * *aDisplayname)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aDisplayname);

    nsXPIDLCString hostname;
    rv = mPrefBranch->GetCharPref("hostname", getter_Copies(hostname));
    if (NS_FAILED(rv)) {
        *aDisplayname=nsnull;
        return NS_OK;
    }
    PRInt32 port;
    rv = mPrefBranch->GetIntPref("port", &port);
    if (NS_FAILED(rv))
        port = 0;
    
    if (port) {
        hostname.Append(':');
        hostname.AppendInt(port);
    }

    *aDisplayname = ToNewCString(hostname);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetTrySSL(PRInt32 *trySSL)
{
    NS_ENSURE_ARG_POINTER(trySSL);
    return getIntPrefWithDefault("try_ssl", trySSL, 0);
}

NS_IMETHODIMP
nsSmtpServer::SetTrySSL(PRInt32 trySSL)
{
    return mPrefBranch->SetIntPref("try_ssl", trySSL);
}

NS_IMETHODIMP
nsSmtpServer::GetTrySecAuth(PRBool *trySecAuth)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(trySecAuth);
    *trySecAuth = PR_TRUE;
    rv = mPrefBranch->GetBoolPref("trySecAuth", trySecAuth);
    if (NS_FAILED(rv))
        mDefPrefBranch->GetBoolPref("trySecAuth", trySecAuth);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetAuthMethod(PRInt32 *authMethod)
{
    NS_ENSURE_ARG_POINTER(authMethod);
    return getIntPrefWithDefault("auth_method", authMethod, 1);
}

nsresult
nsSmtpServer::getIntPrefWithDefault(const char *prefName,
                                    PRInt32 *val,
                                    PRInt32 defVal)
{
    nsresult rv = mPrefBranch->GetIntPref(prefName, val);
    if (NS_SUCCEEDED(rv))
        return NS_OK;

    rv = mDefPrefBranch->GetIntPref(prefName, val);

    if (NS_FAILED(rv))
    { // last resort
        *val = defVal;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetAuthMethod(PRInt32 authMethod)
{
    return mPrefBranch->SetIntPref("auth_method", authMethod);
}

NS_IMETHODIMP
nsSmtpServer::GetUsername(char * *aUsername)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aUsername);
    rv = mPrefBranch->GetCharPref("username", aUsername);
    if (NS_FAILED(rv))
        *aUsername = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetUsername(const char * aUsername)
{
    if (aUsername)
        return mPrefBranch->SetCharPref("username", aUsername);
    else
        mPrefBranch->ClearUserPref("username");
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetPassword(char * *aPassword)
{
    NS_ENSURE_ARG_POINTER(aPassword);
    if (m_password.IsEmpty() && !m_logonFailed)
    {
      // try to avoid prompting the user for another password. If the user has set
      // the appropriate pref, we'll use the password from an incoming server, if 
      // the user has already logged onto that server.

      // if this is set, we'll only use this, and not the other prefs
      // user_pref("mail.smtpserver.smtp1.incomingAccount", "server1");

      // if this is set, we'll accept an exact match of user name and server
      // user_pref("mail.smtp.useMatchingHostNameServer", true);

      // if this is set, and we don't find an exact match of user and host name, 
      // we'll accept a match of username and domain, where domain
      // is everything after the first '.'
      // user_pref("mail.smtp.useMatchingDomainServer", true);

      nsXPIDLCString accountKey;
      PRBool useMatchingHostNameServer = PR_FALSE;
      PRBool useMatchingDomainServer = PR_FALSE;
      mPrefBranch->GetCharPref("incomingAccount", getter_Copies(accountKey));

      nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID);
      nsCOMPtr<nsIMsgIncomingServer> incomingServerToUse;
      if (accountManager) 
      {
        if (!accountKey.IsEmpty()) 
        {
          accountManager->GetIncomingServer(accountKey.get(), getter_AddRefs(incomingServerToUse));
        }
        else
        {
          nsresult rv;
          nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
          NS_ENSURE_SUCCESS(rv,rv);
          prefBranch->GetBoolPref("mail.smtp.useMatchingHostNameServer", &useMatchingHostNameServer);
          prefBranch->GetBoolPref("mail.smtp.useMatchingDomainServer", &useMatchingDomainServer);
          if (useMatchingHostNameServer || useMatchingDomainServer)
          {
            nsXPIDLCString userName;
            nsXPIDLCString hostName;
            GetHostname(getter_Copies(hostName));
            GetUsername(getter_Copies(userName));
            if (useMatchingHostNameServer)
              // pass in empty type and port=0, to match imap and pop3.
              accountManager->FindRealServer(userName, hostName, "", 0, getter_AddRefs(incomingServerToUse));
            PRInt32 dotPos = -1;
            if (!incomingServerToUse && useMatchingDomainServer 
              && (dotPos = hostName.FindChar('.')) != kNotFound)
            {
              hostName.Cut(0, dotPos);
              nsCOMPtr<nsISupportsArray> allServers;
              accountManager->GetAllServers(getter_AddRefs(allServers));
              if (allServers)
              {
                PRUint32 count = 0;
                allServers->Count(&count);
                PRUint32 i;
                for (i = 0; i < count; i++) 
                {
                  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(allServers, i);
                  if (server)
                  {
                    nsXPIDLCString serverUserName;
                    nsXPIDLCString serverHostName;
                    server->GetRealUsername(getter_Copies(serverUserName));
                    server->GetRealHostName(getter_Copies(serverHostName));
                    if (serverUserName.Equals(userName))
                    {
                      PRInt32 serverDotPos = serverHostName.FindChar('.');
                      if (serverDotPos != kNotFound)
                      {
                        serverHostName.Cut(0, serverDotPos);
                        if (serverHostName.Equals(hostName))
                        {
                          incomingServerToUse = server;
                          break;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      if (incomingServerToUse)
        return incomingServerToUse->GetPassword(aPassword);

    }
    *aPassword = ToNewCString(m_password);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetPassword(const char * aPassword)
{
    m_password = aPassword;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetPasswordWithUI(const PRUnichar * aPromptMessage, const
                                PRUnichar *aPromptTitle, 
                                nsIAuthPrompt* aDialog,
                                char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty())
    {
        NS_ENSURE_ARG_POINTER(aDialog);

        // prompt the user for the password
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString uniPassword;
            PRBool okayValue = PR_TRUE;
            nsXPIDLCString serverUri;
            rv = GetServerURI(getter_Copies(serverUri));
            if (NS_FAILED(rv))
                return rv;
            nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
            PRBool passwordProtectLocalCache = PR_FALSE;

            (void) prefBranch->GetBoolPref( "mail.password_protect_local_cache", &passwordProtectLocalCache);

            PRUint32 savePasswordType = (passwordProtectLocalCache) ? nsIAuthPrompt::SAVE_PASSWORD_FOR_SESSION : nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY;
            rv = aDialog->PromptPassword(aPromptTitle, aPromptMessage, 
                    NS_ConvertASCIItoUTF16(serverUri).get(), savePasswordType,
                    getter_Copies(uniPassword), &okayValue);
            if (NS_FAILED(rv))
                return rv;

            if (!okayValue) // if the user pressed cancel, just return NULL;
            {
                *aPassword = nsnull;
                return rv;
            }

            // we got a password back...so remember it
            nsCString aCStr; aCStr.AssignWithConversion(uniPassword); 

            rv = SetPassword(aCStr.get());
            if (NS_FAILED(rv))
                return rv;
        } // if we got a prompt dialog
    } // if the password is empty

    rv = GetPassword(aPassword);
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::GetUsernamePasswordWithUI(const PRUnichar * aPromptMessage, const
                                PRUnichar *aPromptTitle, 
                                nsIAuthPrompt* aDialog,
                                char **aUsername,
                                char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aUsername);
    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty()) {
        NS_ENSURE_ARG_POINTER(aDialog);
        // prompt the user for the password
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString uniUsername;
            nsXPIDLString uniPassword;
            PRBool okayValue = PR_TRUE;
            nsXPIDLCString serverUri;
            rv = GetServerURI(getter_Copies(serverUri));
            if (NS_FAILED(rv))
                return rv;
            rv = aDialog->PromptUsernameAndPassword(aPromptTitle, aPromptMessage, 
                                         NS_ConvertASCIItoUTF16(serverUri).get(), nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                         getter_Copies(uniUsername), getter_Copies(uniPassword), &okayValue);
            if (NS_FAILED(rv))
                return rv;
				
            if (!okayValue) // if the user pressed cancel, just return NULL;
            {
                *aUsername = nsnull;
                *aPassword = nsnull;
                return rv;
            }

            // we got a userid and password back...so remember it
            nsCString aCStr; 

            aCStr.AssignWithConversion(uniUsername); 
            rv = SetUsername(aCStr.get());
            if (NS_FAILED(rv))
                return rv;

            aCStr.AssignWithConversion(uniPassword); 
            rv = SetPassword(aCStr.get());
            if (NS_FAILED(rv))
                return rv;
        } // if we got a prompt dialog
    } // if the password is empty

    rv = GetUsername(aUsername);
    if (NS_FAILED(rv))
        return rv;
    rv = GetPassword(aPassword);
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::ForgetPassword()
{
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), serverUri);

    //this is need to make sure wallet service has been created
    rv = CreateServicesForPasswordManager();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->NotifyObservers(uri, "login-failed", nsnull);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetPassword("");
    m_logonFailed = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::GetServerURI(char **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsresult rv;
    nsCAutoString uri;

    uri += "smtp";
    uri += "://";

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));

    if (NS_SUCCEEDED(rv) && ((const char*)username) && username[0]) {
        nsXPIDLCString escapedUsername;
        *((char **)getter_Copies(escapedUsername)) =
            nsEscape(username, url_XAlphas);
//            nsEscape(username, url_Path);
        // not all servers have a username 
        uri.Append(escapedUsername);
        uri += '@';
    }

    nsXPIDLCString hostname;
    rv = GetHostname(getter_Copies(hostname));

    if (NS_SUCCEEDED(rv) && ((const char*)hostname) && hostname[0]) {
        nsXPIDLCString escapedHostname;
        *((char **)getter_Copies(escapedHostname)) =
            nsEscape(hostname, url_Path);
        // not all servers have a hostname
        uri.Append(escapedHostname);
    }

    *aResult = ToNewCString(uri);
    return NS_OK;
}
    
NS_IMETHODIMP
nsSmtpServer::SetRedirectorType(const char *aRedirectorType)
{
    if (aRedirectorType)
        return mPrefBranch->SetCharPref("redirector_type", aRedirectorType);

    mPrefBranch->ClearUserPref("redirector_type");
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetRedirectorType(char **aResult)
{
    nsresult rv;

    rv = mPrefBranch->GetCharPref("redirector_type", aResult);
    if (NS_FAILED(rv)) 
      *aResult = nsnull;

    // Check if we need to change 'aol' to 'netscape' per #4696
    if (*aResult)
    { 
      if (!nsCRT::strcasecmp(*aResult, "aol"))
      {
        nsXPIDLCString hostName;
        rv = GetHostname(getter_Copies(hostName));
        if (NS_SUCCEEDED(rv) && (hostName.get()) && !nsCRT::strcmp(hostName, "smtp.netscape.net"))
        {
          PL_strfree(*aResult);
          rv = SetRedirectorType("netscape");
          NS_ENSURE_SUCCESS(rv,rv);
          *aResult = nsCRT::strdup("netscape");
        }
      }
    }
    else {
      // for people who have migrated from 4.x or outlook, or mistakenly
      // created redirected accounts as regular imap accounts, 
      // they won't have redirector type set properly
      // this fixes the redirector type for them automatically
      nsXPIDLCString hostName;
      rv = GetHostname(getter_Copies(hostName));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCAutoString prefName;
      prefName.AssignLiteral("default_redirector_type.smtp.");
      prefName.Append(hostName);

      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv,rv);
      nsXPIDLCString defaultRedirectorType;
      rv = prefBranch->GetCharPref(prefName.get(), getter_Copies(defaultRedirectorType));
      if (NS_SUCCEEDED(rv) && !defaultRedirectorType.IsEmpty()) 
      {
        // only set redirectory type in memory
        // if we call SetRedirectorType() that sets it in prefs
        // which makes this automatic redirector type repair permanent
        *aResult = ToNewCString(defaultRedirectorType);
      }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::ClearAllValues()
{
  return mPrefBranch->DeleteBranch("");
}
