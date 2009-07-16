/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * \file nsDispatchSupport.cpp
 * Contains the implementation for the nsDispatchSupport class
 * this is an XPCOM service
 */

#include "XPCPrivate.h"

#include "nsIActiveXSecurityPolicy.h"

static PRBool
ClassIsListed(HKEY hkeyRoot, const TCHAR *szKey, const CLSID &clsid, PRBool &listIsEmpty)
{
    // Test if the specified CLSID is found in the specified registry key

    listIsEmpty = PR_TRUE;

    CRegKey keyList;
    if(keyList.Open(hkeyRoot, szKey, KEY_READ) != ERROR_SUCCESS)
    {
        // Class is not listed, because there is no key to read!
        return PR_FALSE;
    }

    // Enumerate CLSIDs looking for this one
    int i = 0;
    do {
        USES_CONVERSION;
        TCHAR szCLSID[64];
        DWORD kBufLength = sizeof(szCLSID) / sizeof(szCLSID[0]);
        DWORD len = kBufLength;
        memset(szCLSID, 0, sizeof(szCLSID));
        if(::RegEnumKeyEx(keyList, i, szCLSID, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
        {
            // End of list
            break;
        }
        ++i;
        listIsEmpty = PR_FALSE;
        szCLSID[len - 1] = TCHAR('\0');
        CLSID clsidToCompare = GUID_NULL;
        if(SUCCEEDED(::CLSIDFromString(T2OLE(szCLSID), &clsidToCompare)) &&
            ::IsEqualCLSID(clsid, clsidToCompare))
        {
            // Class is listed
            return PR_TRUE;
        }
    } while(1);

    // Class not found
    return PR_FALSE;
}

static PRBool
ClassExists(const CLSID &clsid)
{
    // Test if there is a CLSID entry. If there isn't then obviously
    // the object doesn't exist.
    CRegKey key;
    if(key.Open(HKEY_CLASSES_ROOT, _T("CLSID"), KEY_READ) != ERROR_SUCCESS)
        return PR_FALSE; // Must fail if we can't even open this!
    
    LPOLESTR szCLSID = NULL;
    if(FAILED(StringFromCLSID(clsid, &szCLSID)))
        return PR_FALSE; // Can't allocate string from CLSID

    USES_CONVERSION;
    CRegKey keyCLSID;
    LONG lResult = keyCLSID.Open(key, W2CT(szCLSID), KEY_READ);
    CoTaskMemFree(szCLSID);
    if(lResult != ERROR_SUCCESS)
        return PR_FALSE; // Class doesn't exist

    return PR_TRUE;
}

static PRBool
ClassImplementsCategory(const CLSID &clsid, const CATID &catid, PRBool &bClassExists)
{
#ifndef WINCE
    bClassExists = ClassExists(clsid);
    // Non existent classes won't implement any category...
    if(!bClassExists)
        return PR_FALSE;

    // CLSID exists, so try checking what categories it implements
    bClassExists = PR_TRUE;
    CComPtr<ICatInformation> catInfo;
    HRESULT hr = CoCreateInstance(CLSID_StdComponentCategoriesMgr, NULL,
        CLSCTX_INPROC_SERVER, __uuidof(ICatInformation), (LPVOID*) &catInfo);
    if(catInfo == NULL)
        return PR_FALSE; // Must fail if we can't open the category manager
    
    // See what categories the class implements
    CComPtr<IEnumCATID> enumCATID;
    if(FAILED(catInfo->EnumImplCategoriesOfClass(clsid, &enumCATID)))
        return PR_FALSE; // Can't enumerate classes in category so fail

    // Search for matching categories
    BOOL bFound = FALSE;
    CATID catidNext = GUID_NULL;
    while(enumCATID->Next(1, &catidNext, NULL) == S_OK)
    {
        if(::IsEqualCATID(catid, catidNext))
            return PR_TRUE; // Match
    }
#endif
    return PR_FALSE;
}

nsDispatchSupport* nsDispatchSupport::mInstance = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDispatchSupport, nsIDispatchSupport)

nsDispatchSupport::nsDispatchSupport()
{
    /* member initializers and constructor code */
}

nsDispatchSupport::~nsDispatchSupport()
{
    /* destructor code */
}

/**
 * Converts a COM variant to a jsval
 * @param comvar the variant to convert
 * @param val pointer to the jsval to receive the value
 * @return nsresult
 */
NS_IMETHODIMP nsDispatchSupport::COMVariant2JSVal(VARIANT * comvar, jsval *val)
{
    XPCCallContext ccx(NATIVE_CALLER);
    nsresult retval;
    XPCDispConvert::COMToJS(ccx, *comvar, *val, retval);
    return retval;
}

/**
 * Converts a jsval to a COM variant
 * @param val the jsval to be converted
 * @param comvar pointer to the variant to receive the value
 * @return nsresult
 */
NS_IMETHODIMP nsDispatchSupport::JSVal2COMVariant(jsval val, VARIANT * comvar)
{
    XPCCallContext ccx(NATIVE_CALLER);
    nsresult retval;
    XPCDispConvert::JSToCOM(ccx, val, *comvar, retval);
    return retval;
}

/* boolean isClassSafeToHost (in nsCIDRef clsid, out boolean classExists); */
NS_IMETHODIMP nsDispatchSupport::IsClassSafeToHost(JSContext * cx,
                                                   const nsCID & cid,
                                                   PRBool ignoreException, 
                                                   PRBool *classExists, 
                                                   PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(classExists);

    *aResult = PR_FALSE;

    CLSID clsid = XPCDispnsCID2CLSID(cid);

    // Ask security manager if it's ok to create this object
    XPCCallContext ccx(JS_CALLER, cx);
    nsIXPCSecurityManager* sm =
            ccx.GetXPCContext()->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    *aResult = !sm ||
        NS_SUCCEEDED(sm->CanCreateInstance(ccx, cid));

    if(!*aResult)
    {
        if (ignoreException)
            JS_ClearPendingException(ccx);
        *classExists = PR_TRUE;
        return NS_OK;
    }
    *classExists = ClassExists(clsid);

    // Test the Internet Explorer black list
    const TCHAR kIEControlsBlacklist[] = _T("SOFTWARE\\Microsoft\\Internet Explorer\\ActiveX Compatibility");
    CRegKey keyExplorer;
    if(keyExplorer.Open(HKEY_LOCAL_MACHINE,
        kIEControlsBlacklist, KEY_READ) == ERROR_SUCCESS)
    {
        LPOLESTR szCLSID = NULL;
        ::StringFromCLSID(clsid, &szCLSID);
        if(szCLSID)
        {
            CRegKey keyCLSID;
            USES_CONVERSION;
            if(keyCLSID.Open(keyExplorer, W2T(szCLSID), KEY_READ) == ERROR_SUCCESS)
            {
                DWORD dwType = REG_DWORD;
                DWORD dwFlags = 0;
                DWORD dwBufSize = sizeof(dwFlags);
                if(::RegQueryValueEx(keyCLSID, _T("Compatibility Flags"),
                    NULL, &dwType, (LPBYTE) &dwFlags, &dwBufSize) == ERROR_SUCCESS)
                {
                    // Documented flags for this reg key
                    const DWORD kKillBit = 0x00000400; // MS Knowledge Base 240797
                    if(dwFlags & kKillBit)
                    {
                        ::CoTaskMemFree(szCLSID);
                        *aResult = PR_FALSE;
                        return NS_OK;
                    }
                }
            }
            ::CoTaskMemFree(szCLSID);
        }
    }

    *aResult = PR_TRUE;
    return NS_OK;
}

/* boolean isClassMarkedSafeForScripting (in nsCIDRef clsid, out boolean classExists); */
NS_IMETHODIMP nsDispatchSupport::IsClassMarkedSafeForScripting(const nsCID & cid, PRBool *classExists, PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(classExists);
    // Test the category the object belongs to
    CLSID clsid = XPCDispnsCID2CLSID(cid);
    *aResult = ClassImplementsCategory(clsid, CATID_SafeForScripting, *classExists);
    return NS_OK;
}

/* boolean isObjectSafeForScripting (in voidPtr theObject, in nsIIDRef iid); */
NS_IMETHODIMP nsDispatchSupport::IsObjectSafeForScripting(void * theObject, const nsIID & id, PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(theObject);
    NS_ENSURE_ARG_POINTER(aResult);

    // Test if the object implements IObjectSafety and is marked safe for scripting
    IUnknown *pObject = (IUnknown *) theObject;
    IID iid = XPCDispIID2IID(id);

    // Ask the control if its safe for scripting
    CComQIPtr<IObjectSafety> objectSafety = pObject;
    if(!objectSafety)
    {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    DWORD dwSupported = 0; // Supported options (mask)
    DWORD dwEnabled = 0;   // Enabled options

    // Assume scripting via IDispatch
    if(FAILED(objectSafety->GetInterfaceSafetyOptions(
            iid, &dwSupported, &dwEnabled)))
    {
        // Interface is not safe or failure.
        *aResult = PR_FALSE;
        return NS_OK;
    }

    // Test if safe for scripting
    if(!(dwEnabled & dwSupported) & INTERFACESAFE_FOR_UNTRUSTED_CALLER)
    {
        // Object says it is not set to be safe, but supports unsafe calling,
        // try enabling it and asking again.

        if(!(dwSupported & INTERFACESAFE_FOR_UNTRUSTED_CALLER) ||
            FAILED(objectSafety->SetInterfaceSafetyOptions(
                iid, INTERFACESAFE_FOR_UNTRUSTED_CALLER, INTERFACESAFE_FOR_UNTRUSTED_CALLER)) ||
            FAILED(objectSafety->GetInterfaceSafetyOptions(
                iid, &dwSupported, &dwEnabled)) ||
            !(dwEnabled & dwSupported) & INTERFACESAFE_FOR_UNTRUSTED_CALLER)
        {
            *aResult = PR_FALSE;
            return NS_OK;
        }
    }

    *aResult = PR_TRUE;
    return NS_OK;
}

static const PRUint32 kDefaultHostingFlags =
    nsIActiveXSecurityPolicy::HOSTING_FLAGS_HOST_NOTHING;

/* unsigned long getHostingFlags (in string aContext); */
NS_IMETHODIMP nsDispatchSupport::GetHostingFlags(const char *aContext, PRUint32 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    // Ask the activex security policy what the hosting flags are
    nsresult rv;
    nsCOMPtr<nsIActiveXSecurityPolicy> securityPolicy =
        do_GetService(NS_IACTIVEXSECURITYPOLICY_CONTRACTID, &rv);
    if(NS_SUCCEEDED(rv) && securityPolicy)
        return securityPolicy->GetHostingFlags(aContext, aResult);
    
    // No policy so use the defaults
    *aResult = kDefaultHostingFlags;
    return NS_OK;
}

nsDispatchSupport* nsDispatchSupport::GetSingleton()
{
    if(!mInstance)
    {
        mInstance = new nsDispatchSupport;
        NS_IF_ADDREF(mInstance);
    }
    NS_IF_ADDREF(mInstance);
    return mInstance;
}
