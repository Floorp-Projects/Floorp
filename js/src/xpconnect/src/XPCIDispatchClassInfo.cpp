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

#include "xpcprivate.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(XPCIDispatchClassInfo, nsIClassInfo)

XPCIDispatchClassInfo* XPCIDispatchClassInfo::sInstance = nsnull;

XPCIDispatchClassInfo* XPCIDispatchClassInfo::GetSingleton()
{
    if(!sInstance)
    {
        sInstance = new XPCIDispatchClassInfo;
        NS_IF_ADDREF(sInstance);
    }
    NS_IF_ADDREF(sInstance);
    return sInstance;
}

void XPCIDispatchClassInfo::FreeSingleton()
{
    NS_IF_RELEASE(sInstance);
    sInstance = nsnull;
}

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    *count = 0;
    *array = static_cast<nsIID**>(nsMemory::Alloc(sizeof(nsIID*)));
    NS_ENSURE_TRUE(*array, NS_ERROR_OUT_OF_MEMORY);

    **array = static_cast<nsIID *>(nsMemory::Clone(&NSID_IDISPATCH,
                                                      sizeof(NSID_IDISPATCH)));
    if(!**array)
    {
        nsMemory::Free(*array);
        *array = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    *count = 1;
    return NS_OK;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetHelperForLanguage(PRUint32 language, 
                                            nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_OK;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nsCRT::strdup("IDispatch");
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::UNKNOWN;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::DOM_OBJECT;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}
