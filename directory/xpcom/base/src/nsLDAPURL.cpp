/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAPURL.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLDAPURL, nsILDAPURL, nsIURI)

nsLDAPURL::nsLDAPURL()
{
    NS_INIT_ISUPPORTS();
}

nsLDAPURL::~nsLDAPURL()
{
    ldap_free_urldesc(mDesc);
}

const char *kEmptyString = "";

// A string representation of the URI. Setting the spec 
// causes the new spec to be parsed, initializing the URI. Setting
// the spec (or any of the accessors) causes also any currently
// open streams on the URI's channel to be closed.
//
// attribute string spec;
//
NS_IMETHODIMP 
nsLDAPURL::GetSpec(char* *aSpec)
{
    NS_ENSURE_ARG_POINTER(aSpec);
    NS_ENSURE_FALSE(mSpec.IsEmpty(), NS_ERROR_NOT_INITIALIZED);

    // copy it out
    //
    *aSpec = mSpec.ToNewCString();

    // XXXdmose - presumably this is the right thing.  nsString.h doesn't
    // document whether there can be a NULL return code.
    //
    if (*aSpec == nsnull) {
	return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetSpec(const char * aSpec)
{
    PRUint32 rc;

    // save this off
    //
    mSpec = aSpec;

    rc = ldap_url_parse(aSpec, &mDesc);
    switch (rc) {

    case LDAP_SUCCESS:
	return NS_OK;
	break;

    case LDAP_URL_ERR_NOTLDAP:
    case LDAP_URL_ERR_NODN:
    case LDAP_URL_ERR_BADSCOPE:
	return NS_ERROR_MALFORMED_URI;
	break;

    case LDAP_URL_ERR_MEM:
	return NS_ERROR_OUT_OF_MEMORY;
	break;

    case LDAP_URL_ERR_PARAM: 
	return NS_ERROR_INVALID_POINTER;
	break;
    }

    // this shouldn't happen
    //
    return NS_ERROR_UNEXPECTED;
}

/* attribute string prePath; */
NS_IMETHODIMP nsLDAPURL::GetPrePath(char * *aPrePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsLDAPURL::SetPrePath(const char * aPrePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string scheme; */
NS_IMETHODIMP nsLDAPURL::GetScheme(char * *aScheme)
{
    // need to deal with ldaps here too?
    //
    char *scheme = nsCRT::strdup("ldap");
    if (scheme == nsnull) {
	return NS_ERROR_OUT_OF_MEMORY;
    }

    *aScheme = scheme;
    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetScheme(const char * aScheme)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string preHost; */
NS_IMETHODIMP 
nsLDAPURL::GetPreHost(char * *aPreHost)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsLDAPURL::SetPreHost(const char * aPreHost)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string username; */
NS_IMETHODIMP
nsLDAPURL::GetUsername(char * *aUsername)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsLDAPURL::SetUsername(const char * aUsername)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string password; */
NS_IMETHODIMP 
nsLDAPURL::GetPassword(char * *aPassword)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsLDAPURL::SetPassword(const char * aPassword)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string host; */
NS_IMETHODIMP 
nsLDAPURL::GetHost(char * *aHost)
{
    NS_ENSURE_FALSE(mDesc == nsnull, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_ARG_POINTER(aHost);

    if ( mDesc->lud_host == nsnull ) {
	*aHost = nsCRT::strdup(kEmptyString);
    } else 
	*aHost = nsCRT::strdup(mDesc->lud_host);
    
    if (*aHost == nsnull) 
	return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetHost(const char * aHost)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long port; */
NS_IMETHODIMP 
nsLDAPURL::GetPort(PRInt32 *aPort)
{
    NS_ENSURE_ARG_POINTER(aPort);
    NS_ENSURE_FALSE(mDesc == nsnull, NS_ERROR_NOT_INITIALIZED);

    if ( mDesc->lud_port )
	*aPort = mDesc->lud_port;
    else
	*aPort = -1;

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetPort(PRInt32 aPort)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string path; */
NS_IMETHODIMP nsLDAPURL::GetPath(char * *aPath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsLDAPURL::SetPath(const char * aPath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean equals (in nsIURI other); */
NS_IMETHODIMP nsLDAPURL::Equals(nsIURI *other, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIURI clone (); */
NS_IMETHODIMP nsLDAPURL::Clone(nsIURI **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string resolve (in string relativePath); */
NS_IMETHODIMP nsLDAPURL::Resolve(const char *relativePath, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* the following attributes come from nsILDAPURL */

/* attribute string dn; */
NS_IMETHODIMP nsLDAPURL::GetDn(char * *aDn)
{
    if (!aDn) {
	return NS_ERROR_ILLEGAL_VALUE;
    }

    if (!mDesc) {
	return NS_ERROR_NOT_INITIALIZED;
    }

    if ( mDesc->lud_dn == NULL ) {
	*aDn = nsCRT::strdup(kEmptyString);
    } else {
	*aDn = nsCRT::strdup(mDesc->lud_dn);
    }

    if ( *aDn == nsnull )
	return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetDn(const char * aDn)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getAttributes (out unsigned long count, [array, size_is (count), retval] out string values); */
NS_IMETHODIMP nsLDAPURL::GetAttributes(PRUint32 *count, char ***values)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long scope; */
NS_IMETHODIMP nsLDAPURL::GetScope(PRInt32 *aScope)
{
    NS_ENSURE_ARG_POINTER(aScope);
    NS_ENSURE_FALSE(mDesc == nsnull, NS_ERROR_NOT_INITIALIZED);

    *aScope = mDesc->lud_scope;

    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetScope(PRInt32 aScope)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string filter; */
NS_IMETHODIMP nsLDAPURL::GetFilter(char * *aFilter)
{
    NS_ENSURE_ARG_POINTER(aFilter);
    NS_ENSURE_FALSE(mDesc == nsnull, NS_ERROR_NOT_INITIALIZED);

    // if these aren't set, insert the defaults.
    //
    if ((mDesc->lud_filter == NULL) || (nsCRT::strlen(mDesc->lud_filter) == 0))
	*aFilter = nsCRT::strdup("objectClass=*");
    else 
	*aFilter = nsCRT::strdup(mDesc->lud_filter);

    if (*aFilter == nsnull)
	return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetFilter(const char * aFilter)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long options; */
NS_IMETHODIMP nsLDAPURL::GetOptions(PRUint32 *aOptions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsLDAPURL::SetOptions(PRUint32 aOptions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
