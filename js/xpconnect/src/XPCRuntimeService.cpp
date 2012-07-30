/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#include "mozilla/dom/workers/Workers.h"
using mozilla::dom::workers::ResolveWorkerClasses;

NS_INTERFACE_MAP_BEGIN(BackstagePass)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(BackstagePass)
NS_IMPL_THREADSAFE_RELEASE(BackstagePass)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME   "BackstagePass"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   |  \
                            nsIXPCScriptable::DONT_ENUM_STATIC_PROPS       |  \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    |  \
                            nsIXPCScriptable::IS_GLOBAL_OBJECT             |  \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */

/* bool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
BackstagePass::NewResolve(nsIXPConnectWrappedNative *wrapper,
                          JSContext * cx, JSObject * obj_,
                          jsid id_, PRUint32 flags,
                          JSObject * *objp_, bool *_retval)
{
    JS::RootedObject obj(cx, obj_);
    JS::RootedId id(cx, id_);

    JSBool resolved;

    *_retval = !!JS_ResolveStandardClass(cx, obj, id, &resolved);
    if (!*_retval) {
        *objp_ = nullptr;
        return NS_OK;
    }

    if (resolved) {
        *objp_ = obj;
        return NS_OK;
    }

    JS::RootedObject objp(cx, *objp_);
    *_retval = !!ResolveWorkerClasses(cx, obj, id, flags, &objp);
    *objp_ = objp;
    return NS_OK;
}

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval]
                       out nsIIDPtr array); */
NS_IMETHODIMP
BackstagePass::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    const PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if (!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id)                                                          \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),           \
                                                 sizeof(nsIID)));             \
    if (!clone)                                                               \
        goto oom;                                                             \
    array[index++] = clone;

    PUSH_IID(nsIXPCScriptable)
    PUSH_IID(nsIScriptObjectPrincipal)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nullptr;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP
BackstagePass::GetHelperForLanguage(PRUint32 language,
                                    nsISupports **retval)
{
    *retval = nullptr;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP
BackstagePass::GetContractID(char * *aContractID)
{
    *aContractID = nullptr;
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
    *aClassID = nullptr;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP
BackstagePass::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
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
