/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org LDAP XPCOM component.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Leif Hedstrom <leif@netscape.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLDAPServer.h"
#include "nsReadableUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPServer, nsILDAPServer)

nsLDAPServer::nsLDAPServer()
    : mSizeLimit(0),
      mProtocolVersion(nsILDAPConnection::VERSION3)
{
}

nsLDAPServer::~nsLDAPServer()
{
}

// attribute wstring key;
NS_IMETHODIMP nsLDAPServer::GetKey(PRUnichar **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPServer::GetKey: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = ToNewUnicode(mKey);
    if (!*_retval) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetKey(const PRUnichar *aKey)
{
    mKey = aKey;
    return NS_OK;
}

// attribute AUTF8String username;
NS_IMETHODIMP nsLDAPServer::GetUsername(nsACString& _retval)
{
    _retval.Assign(mUsername);
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetUsername(const nsACString& aUsername)
{
    mUsername.Assign(aUsername);
    return NS_OK;
}

// attribute AUTF8String password;
NS_IMETHODIMP nsLDAPServer::GetPassword(nsACString& _retval)
{
    _retval.Assign(mPassword);
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetPassword(const nsACString& aPassword)
{
    mPassword.Assign(aPassword);
    return NS_OK;
}

// attribute AUTF8String binddn;
NS_IMETHODIMP nsLDAPServer::GetBinddn(nsACString& _retval)
{
    _retval.Assign(mBindDN);
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetBinddn(const nsACString& aBindDN)
{
    mBindDN.Assign(aBindDN);
    return NS_OK;
}

// attribute unsigned long sizelimit;
NS_IMETHODIMP nsLDAPServer::GetSizelimit(PRUint32 *_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPServer::GetSizelimit: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = mSizeLimit;
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetSizelimit(PRUint32 aSizeLimit)
{
    mSizeLimit = aSizeLimit;
    return NS_OK;
}

// attribute nsILDAPURL url;
NS_IMETHODIMP nsLDAPServer::GetUrl(nsILDAPURL **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPServer::GetUrl: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    NS_IF_ADDREF(*_retval = mURL);
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetUrl(nsILDAPURL *aURL)
{
    mURL = aURL;
    return NS_OK;
}

// attribute long protocolVersion
NS_IMETHODIMP nsLDAPServer::GetProtocolVersion(PRUint32 *_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPServer::GetProtocolVersion: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = mProtocolVersion;
    return NS_OK;
}
NS_IMETHODIMP nsLDAPServer::SetProtocolVersion(PRUint32 aVersion)
{
    if (aVersion != nsILDAPConnection::VERSION2 &&
        aVersion != nsILDAPConnection::VERSION3) {
        NS_ERROR("nsLDAPServer::SetProtocolVersion: invalid version");
        return NS_ERROR_INVALID_ARG;
    }

    mProtocolVersion = aVersion;
    return NS_OK;
}
