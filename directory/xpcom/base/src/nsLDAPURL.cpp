/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 *                 Leif Hedstrom <leif@netscape.com>
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
#include "nsReadableUtils.h"

// The two schemes we support, LDAP and LDAPS
//
static const char *kLDAPScheme = "ldap";
static const char *kLDAPSSLScheme = "ldaps";


// Constructor and destructor
//
NS_IMPL_THREADSAFE_ISUPPORTS2(nsLDAPURL, nsILDAPURL, nsIURI)

nsLDAPURL::nsLDAPURL()
    : mPort(0),
      mScope(SCOPE_BASE),
      mOptions(0),
      mAttributes(0)
{
    NS_INIT_ISUPPORTS();
}

nsLDAPURL::~nsLDAPURL()
{
    // Delete the array of attributes
    delete mAttributes;
}

nsresult
nsLDAPURL::Init()
{
    if (!mAttributes) {
        mAttributes = new nsCStringArray();
        if (!mAttributes) {
            NS_ERROR("nsLDAPURL::Init: out of memory ");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}

// A string representation of the URI. Setting the spec 
// causes the new spec to be parsed, initializing the URI. Setting
// the spec (or any of the accessors) causes also any currently
// open streams on the URI's channel to be closed.
//
// attribute string spec;
//
NS_IMETHODIMP 
nsLDAPURL::GetSpec(char **_retval)
{
    nsCAutoString spec;
    PRUint32 count;
    
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetSpec: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    spec = ((mOptions & OPT_SECURE) ? kLDAPSSLScheme : kLDAPScheme);
    spec.Append("://");
    if (mHost.Length() > 0) {
        spec.Append(mHost);
    }
    if (mPort > 0) {
        spec.Append(':');
        spec.AppendInt(mPort);
    }
    spec.Append('/');
    if (mDN.Length() > 0) {
        spec.Append(mDN);
    }

    if ((count = mAttributes->Count())) {
        PRUint32 index = 0;

        spec.Append('?');
        while (index < count) {
            spec.Append(*(mAttributes->CStringAt(index++)));
            if (index < count) {
                spec.Append(',');
            }
        }
    }

    if ((mScope) || mFilter.Length()) {
        spec.Append((count ? "?" : "??"));
        if (mScope) {
            if (mScope == SCOPE_ONELEVEL) {
                spec.Append("one");
            } else if (mScope == SCOPE_SUBTREE) {
                spec.Append("sub");
            }
        }
        if (mFilter.Length()) {
            spec.Append('?');
            spec.Append(mFilter);
        }
    }

    *_retval = ToNewCString(spec);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetSpec: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetSpec(const char *aSpec)
{
    PRUint32 rv, count;
    LDAPURLDesc *desc;
    nsCString str;
    char **attributes;

    // This is from the LDAP C-SDK, which currently doesn't
    // support everything from RFC 2255... :(
    //
    rv = ldap_url_parse(aSpec, &desc);
    switch (rv) {
    case LDAP_SUCCESS:
        mHost = desc->lud_host;
        mPort = desc->lud_port;
        mDN = desc->lud_dn;
        mScope = desc->lud_scope;
        mFilter = desc->lud_filter;
        mOptions = desc->lud_options;

        // Set the attributes array, need to count it first.
        //
        count = 0;
        attributes = desc->lud_attrs;
        while (attributes && *attributes++) {
            count++;
        }
        if (count) {
            rv = SetAttributes(count,
                               NS_CONST_CAST(const char **, desc->lud_attrs));
            // This error could only be out-of-memory, so pass it up
            //
            if (NS_FAILED(rv)) {
                return rv;
            }
        } else {
            mAttributes->Clear();
        }

        ldap_free_urldesc(desc);
        return NS_OK;

    case LDAP_URL_ERR_NOTLDAP:
    case LDAP_URL_ERR_NODN:
    case LDAP_URL_ERR_BADSCOPE:
        return NS_ERROR_MALFORMED_URI;

    case LDAP_URL_ERR_MEM:
        NS_ERROR("nsLDAPURL::SetSpec: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;

    case LDAP_URL_ERR_PARAM: 
        return NS_ERROR_INVALID_POINTER;
    }

    // This shouldn't happen...
    return NS_ERROR_UNEXPECTED;
}

// attribute string prePath;
//
NS_IMETHODIMP nsLDAPURL::GetPrePath(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsLDAPURL::SetPrePath(const char *aPrePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// attribute string scheme;
//
NS_IMETHODIMP nsLDAPURL::GetScheme(char **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetScheme: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    
    *_retval = nsCRT::strdup((mOptions & OPT_SECURE) ? kLDAPSSLScheme :
                             kLDAPScheme);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetScheme: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetScheme(const char *aScheme)
{
    if (nsCRT::strcasecmp(aScheme, kLDAPScheme) == 0) {
        mOptions ^= OPT_SECURE;
    } else if (nsCRT::strcasecmp(aScheme, kLDAPSSLScheme) == 0) {
        mOptions |= OPT_SECURE;
    } else {
        return NS_ERROR_MALFORMED_URI;
    }

    return NS_OK;
}

// attribute string preHost;
//
NS_IMETHODIMP 
nsLDAPURL::GetPreHost(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsLDAPURL::SetPreHost(const char *aPreHost)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// attribute string username
//
NS_IMETHODIMP
nsLDAPURL::GetUsername(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsLDAPURL::SetUsername(const char *aUsername)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// attribute string password;
//
NS_IMETHODIMP 
nsLDAPURL::GetPassword(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsLDAPURL::SetPassword(const char *aPassword)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// attribute string host;
//
NS_IMETHODIMP 
nsLDAPURL::GetHost(char **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetHost: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = ToNewCString(mHost);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetHost: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetHost(const char *aHost)
{
    mHost = aHost;
    return NS_OK;
}

// C-SDK URL parser defaults port 389 as "0", while nsIURI
// specifies the default to be "-1", hence the translations.
//
// attribute long port;
//
NS_IMETHODIMP 
nsLDAPURL::GetPort(PRInt32 *_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetPort: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    if (!mPort) {
        *_retval = -1;
    } else {
        *_retval = mPort;
    }
        
    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPURL::SetPort(PRInt32 aPort)
{
    if (aPort == -1) {
        mPort = 0;
    } else if (aPort >= 0) {
        mPort = aPort;
    } else {
        return NS_ERROR_MALFORMED_URI;
    }
    
    return NS_OK;
}

// attribute string path;
// XXXleif: For now, these are identical to SetDn()/GetDn().
NS_IMETHODIMP nsLDAPURL::GetPath(char **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetPath: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = ToNewCString(mDN);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetPath: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetPath(const char *aPath)
{
    mDN = aPath;
    return NS_OK;
}

// boolean equals (in nsIURI other)
//
NS_IMETHODIMP nsLDAPURL::Equals(nsIURI *other, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// boolean schemeIs(in const char * scheme);
//
NS_IMETHODIMP nsLDAPURL::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
    if (!i_Scheme) return NS_ERROR_INVALID_ARG;
    if (*i_Scheme == 'l' || *i_Scheme == 'L') {
        *o_Equals = PL_strcasecmp("ldap", i_Scheme) ? PR_FALSE : PR_TRUE;
    } else {
        *o_Equals = PR_FALSE;
    }

    return NS_OK;
}

// nsIURI clone ();
//
NS_IMETHODIMP nsLDAPURL::Clone(nsIURI **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// string resolve (in string relativePath);
//
NS_IMETHODIMP nsLDAPURL::Resolve(const char *relativePath,
                                 char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// The following attributes come from nsILDAPURL

// attribute string dn;
//
NS_IMETHODIMP nsLDAPURL::GetDn(char **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetDn: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = ToNewCString(mDN);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetDN: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetDn(const char *aDn)
{
    mDN = aDn;
    return NS_OK;
}

// void getAttributes (out unsigned long aCount, 
//                     [array, size_is (aCount), retval] out string aAttrs);
//
NS_IMETHODIMP nsLDAPURL::GetAttributes(PRUint32 *aCount, char ***_retval)
{
    PRUint32 index = 0;
    PRUint32 count;
    char **cArray;

    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetAttributes: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    count = mAttributes->Count();
    cArray = NS_STATIC_CAST(char **, nsMemory::Alloc(count * sizeof(char *)));
    if (!cArray) {
        NS_ERROR("nsLDAPURL::GetAttributes: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Loop through the string array, and build up the C-array.
    //
    while (index < count) {
        if (!(cArray[index] = ToNewCString(*(mAttributes->CStringAt(index))))) {
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(index, cArray);
            NS_ERROR("nsLDAPURL::GetAttributes: out of memory ");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        index++;
    }
    *aCount = count;
    *_retval = cArray;

    return NS_OK;
}
// void setAttributes (in unsigned long aCount,
//                     [array, size_is (aCount)] in string aAttrs); */
NS_IMETHODIMP nsLDAPURL::SetAttributes(PRUint32 count, const char **aAttrs)
{
    PRUint32 index = 0;
    nsCString str;
    
    mAttributes->Clear();
    while (index < count) {
        // Have to assign the str into this temporary nsCString, to make
        // the compilers happy...
        //
        str = nsDependentCString(aAttrs[index]);
        if (!mAttributes->InsertCStringAt(str, index++)) {
            NS_ERROR("nsLDAPURL::SetAttributes: out of memory ");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}
// void addAttribute (in string aAttribute);
//
NS_IMETHODIMP nsLDAPURL::AddAttribute(const char *aAttribute)
{
    nsCString str;

    str = nsDependentCString(aAttribute);
    if (mAttributes->IndexOfIgnoreCase(str) >= 0) {
        return NS_OK;
    }

    if (!mAttributes->InsertCStringAt(str, mAttributes->Count())) {
        NS_ERROR("nsLDAPURL::AddAttribute: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
// void removeAttribute (in string aAttribute);
//
NS_IMETHODIMP nsLDAPURL::RemoveAttribute(const char *aAttribute)
{
    nsCString str;

    str = nsDependentCString(aAttribute);
    mAttributes->RemoveCString(str);

    return NS_OK;
}
// boolean hasAttribute (in string aAttribute);
//
NS_IMETHODIMP nsLDAPURL::HasAttribute(const char *aAttribute, PRBool *_retval)
{
    nsCString str;

    if (!_retval) {
        NS_ERROR("nsLDAPURL::HasAttribute: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    str = nsDependentCString(aAttribute);
    if (mAttributes->IndexOfIgnoreCase(str) >= 0) {
        *_retval = PR_TRUE;
    } else {
        *_retval = PR_FALSE;
    }
    
    return NS_OK;
}

// attribute long scope;
//
NS_IMETHODIMP nsLDAPURL::GetScope(PRInt32 *_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetScope: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = mScope;
    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetScope(PRInt32 aScope)
{
    // Only allow scopes supported by the C-SDK
    if ((aScope != SCOPE_BASE) &&
        (aScope != SCOPE_ONELEVEL) &&
        (aScope != SCOPE_SUBTREE)) {
        return NS_ERROR_MALFORMED_URI;
    }

    mScope = aScope;

    return NS_OK;
}

// attribute string filter;
//
NS_IMETHODIMP nsLDAPURL::GetFilter(char **_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetFilter: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = ToNewCString(mFilter);
    if (!*_retval) {
        NS_ERROR("nsLDAPURL::GetFilter: out of memory ");
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetFilter(const char *aFilter)
{
    mFilter = aFilter;
    return NS_OK;
}

// attribute unsigned long options;
//
NS_IMETHODIMP nsLDAPURL::GetOptions(PRUint32 *_retval)
{
    if (!_retval) {
        NS_ERROR("nsLDAPURL::GetOptions: null pointer ");
        return NS_ERROR_NULL_POINTER;
    }

    *_retval = mOptions;
    return NS_OK;
}
NS_IMETHODIMP nsLDAPURL::SetOptions(PRUint32 aOptions)
{
    mOptions = aOptions;
    return NS_OK;
}
