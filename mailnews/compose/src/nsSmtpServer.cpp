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

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsSmtpServer.h"

NS_IMPL_ISUPPORTS1(nsSmtpServer, nsISmtpServer)

nsSmtpServer::nsSmtpServer()
{
    
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
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
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
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    getPrefString("hostname", pref);
    return prefs->SetCharPref(pref, aHostname);
}

NS_IMETHODIMP
nsSmtpServer::GetUsername(char * *aUsername)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
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
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    getPrefString("username", pref);
    return prefs->SetCharPref(pref, aUsername);
}

NS_IMETHODIMP
nsSmtpServer::GetPassword(char * *aPassword)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    getPrefString("password", pref);
    rv = prefs->CopyCharPref(pref, aPassword);
    if (NS_FAILED(rv)) *aPassword = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetPassword(const char * aPassword)
{
    nsresult rv;
    nsCAutoString pref;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    getPrefString("password", pref);
    return prefs->SetCharPref(pref, aPassword);
}
                          
nsresult
nsSmtpServer::getPrefString(const char *pref, nsCAutoString& result)
{
    result = "mail.smtpserver.";
    result += mKey;
    result += pref;

    return NS_OK;
}
    
