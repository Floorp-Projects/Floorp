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

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsEscape.h"
#include "nsSmtpServer.h"
#include "nsIPrompt.h"
#include "nsIWalletService.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);

NS_IMPL_ADDREF(nsSmtpServer)
NS_IMPL_RELEASE(nsSmtpServer)
NS_INTERFACE_MAP_BEGIN(nsSmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISmtpServer)
NS_INTERFACE_MAP_END

nsSmtpServer::nsSmtpServer()
{
    NS_INIT_REFCNT();    
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
        *aKey = mKey.ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetKey(const char * aKey)
{
    mKey = aKey;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetHostname(char * *aHostname)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("hostname", pref);
    rv = prefs->CopyCharPref(pref, aHostname);
    if (NS_FAILED(rv)) *aHostname=nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetHostname(const char * aHostname)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("hostname", pref);
    if (aHostname)
        return prefs->SetCharPref(pref, aHostname);
    else
        prefs->ClearUserPref(pref);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetTrySSL(PRInt32 *trySSL)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(trySSL);
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    *trySSL= 0;
    getPrefString("try_ssl", pref);
    rv = prefs->GetIntPref(pref, trySSL);
    if (NS_FAILED(rv)) *trySSL = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetTrySSL(PRInt32 trySSL)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    getPrefString("try_ssl", pref);
    return prefs->SetIntPref(pref, trySSL);
}

NS_IMETHODIMP
nsSmtpServer::GetAuthMethod(PRInt32 *authMethod)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(authMethod);
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    *authMethod = 0;
    getPrefString("auth_method", pref);
    rv = prefs->GetIntPref(pref, authMethod);
    if (NS_FAILED(rv)) *authMethod = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetAuthMethod(PRInt32 authMethod)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    getPrefString("auth_method", pref);
    return prefs->SetIntPref(pref, authMethod);
}

NS_IMETHODIMP
nsSmtpServer::GetUsername(char * *aUsername)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("username", pref);
    rv = prefs->CopyCharPref(pref, aUsername);
    if (NS_FAILED(rv)) *aUsername = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetUsername(const char * aUsername)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("username", pref);
    if (aUsername)
        return prefs->SetCharPref(pref, aUsername);
    else
        prefs->ClearUserPref(pref);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetPassword(char * *aPassword)
{
    NS_ENSURE_ARG_POINTER(aPassword);
	*aPassword = m_password.ToNewCString();
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
                                nsIPrompt* aDialog,
                                char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty()) {
        NS_ENSURE_ARG_POINTER(aDialog);
        
		// prompt the user for the password
		if (NS_SUCCEEDED(rv))
		{
            nsXPIDLString uniPassword;
			PRBool okayValue = PR_TRUE;
			nsXPIDLCString serverUri;
			rv = GetServerURI(getter_Copies(serverUri));
			if (NS_FAILED(rv)) return rv;
			rv = aDialog->PromptPassword(aPromptTitle, aPromptMessage, 
                                         NS_ConvertToString(serverUri).GetUnicode(), nsIPrompt::SAVE_PASSWORD_PERMANENTLY,
                                         getter_Copies(uniPassword), &okayValue);
            if (NS_FAILED(rv)) return rv;
				
			if (!okayValue) // if the user pressed cancel, just return NULL;
			{
				*aPassword = nsnull;
				return rv;
			}

			// we got a password back...so remember it
			nsCString aCStr; aCStr.AssignWithConversion(uniPassword); 

			rv = SetPassword((const char *) aCStr);
            if (NS_FAILED(rv)) return rv;
		} // if we got a prompt dialog
	} // if the password is empty

    rv = GetPassword(aPassword);
	return rv;
}

NS_IMETHODIMP
nsSmtpServer::ForgetPassword()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    
    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    rv = SetPassword("");
    if (NS_FAILED(rv)) return rv;
    
    
    rv = walletservice->SI_RemoveUser((const char *)serverUri, nsnull);
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

    *aResult = uri.ToNewCString();
    return NS_OK;
}
                          
nsresult
nsSmtpServer::getPrefString(const char *pref, nsCAutoString& result)
{
    result = "mail.smtpserver.";
    result += mKey;
    result += ".";
    result += pref;

    return NS_OK;
}
    
NS_IMETHODIMP
nsSmtpServer::SetRedirectorType(const char *aRedirectorType)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("redirector_type", pref);
    if (aRedirectorType)
        return prefs->SetCharPref(pref, aRedirectorType);
    else
        prefs->ClearUserPref(pref);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetRedirectorType(char **aResult)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    getPrefString("redirector_type", pref);
    rv = prefs->CopyCharPref(pref, aResult);
    if (NS_FAILED(rv)) *aResult=nsnull;
    return NS_OK;
}
