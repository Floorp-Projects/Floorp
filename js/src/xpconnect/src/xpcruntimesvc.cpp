/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   John Bandhauer <jband@netscape.com>
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

#include "xpcprivate.h"

NS_INTERFACE_MAP_BEGIN(BackstagePass)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
#ifndef XPCONNECT_STANDALONE
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(BackstagePass)
NS_IMPL_THREADSAFE_RELEASE(BackstagePass)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME   "BackstagePass"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   | \
                            nsIXPCScriptable::DONT_ENUM_STATIC_PROPS       | \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    | \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
BackstagePass::NewResolve(nsIXPConnectWrappedNative *wrapper,
                          JSContext * cx, JSObject * obj,
                          jsval id, PRUint32 flags, 
                          JSObject * *objp, PRBool *_retval)
{
    JSBool resolved;

    *_retval = JS_ResolveStandardClass(cx, obj, id, &resolved);
    if(*_retval && resolved)
        *objp = obj;
    return NS_OK;
}

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
BackstagePass::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 1;
#ifndef XPCONNECT_STANDALONE
    ++count;
#endif
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCScriptable)
#ifndef XPCONNECT_STANDALONE
    PUSH_IID(nsIScriptObjectPrincipal)
#endif
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
BackstagePass::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
BackstagePass::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
BackstagePass::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "BackstagePass";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
BackstagePass::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
BackstagePass::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
BackstagePass::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
BackstagePass::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}
