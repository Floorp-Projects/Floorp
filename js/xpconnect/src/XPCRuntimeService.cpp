/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "BackstagePass.h"
#include "nsIProgrammingLanguage.h"
#include "nsDOMClassInfo.h"
#include "nsIPrincipal.h"

#include "mozilla/dom/ResolveSystemBinding.h"

using mozilla::dom::ResolveSystemBinding;

NS_INTERFACE_MAP_BEGIN(BackstagePass)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(BackstagePass)
NS_IMPL_RELEASE(BackstagePass)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME   "BackstagePass"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_ENUMERATE
#define                             XPC_MAP_WANT_FINALIZE
#define                             XPC_MAP_WANT_PRECREATE

#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   |  \
                            nsIXPCScriptable::DONT_ENUM_STATIC_PROPS       |  \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    |  \
                            nsIXPCScriptable::IS_GLOBAL_OBJECT             |  \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */

/* bool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, out JSObjectPtr objp); */
NS_IMETHODIMP
BackstagePass::NewResolve(nsIXPConnectWrappedNative *wrapper,
                          JSContext * cx, JSObject * objArg,
                          jsid idArg, JSObject * *objpArg,
                          bool *_retval)
{
    JS::RootedObject obj(cx, objArg);
    JS::RootedId id(cx, idArg);

    bool resolved;
    *objpArg = nullptr;

    *_retval = !!JS_ResolveStandardClass(cx, obj, id, &resolved);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

    if (resolved) {
        *objpArg = obj;
        return NS_OK;
    }

    JS::RootedObject objp(cx, *objpArg);

    *_retval = ResolveSystemBinding(cx, obj, id, &objp);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

    if (objp) {
        *objpArg = objp;
        return NS_OK;
    }

    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *objArg, bool *_retval)
{
    JS::RootedObject obj(cx, objArg);

    *_retval = JS_EnumerateStandardClasses(cx, obj);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

    JS::RootedObject ignored(cx);
    *_retval = ResolveSystemBinding(cx, obj, JSID_VOIDHANDLE, &ignored);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

    return NS_OK;
}

/***************************************************************************/
/* void getInterfaces (out uint32_t count, [array, size_is (count), retval]
                       out nsIIDPtr array); */
NS_IMETHODIMP
BackstagePass::GetInterfaces(uint32_t *aCount, nsIID * **aArray)
{
    const uint32_t count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if (!array)
        return NS_ERROR_OUT_OF_MEMORY;

    uint32_t index = 0;
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

/* nsISupports getHelperForLanguage (in uint32_t language); */
NS_IMETHODIMP
BackstagePass::GetHelperForLanguage(uint32_t language,
                                    nsISupports **retval)
{
    nsCOMPtr<nsISupports> supports =
        do_QueryInterface(static_cast<nsIGlobalObject *>(this));
    supports.forget(retval);
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

/* readonly attribute uint32_t implementationLanguage; */
NS_IMETHODIMP
BackstagePass::GetImplementationLanguage(uint32_t *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute uint32_t flags; */
NS_IMETHODIMP
BackstagePass::GetFlags(uint32_t *aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP
BackstagePass::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp * fop, JSObject * obj)
{
    nsCOMPtr<nsIGlobalObject> bsp(do_QueryWrappedNative(wrapper));
    MOZ_ASSERT(bsp);
    static_cast<BackstagePass*>(bsp.get())->ForgetGlobalObject();
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::PreCreate(nsISupports *nativeObj, JSContext *cx,
                         JSObject *globalObj, JSObject **parentObj)
{
    // We do the same trick here as for WindowSH. Return the js global
    // as parent, so XPConenct can find the right scope and the wrapper
    // that already exists.
    nsCOMPtr<nsIGlobalObject> global(do_QueryInterface(nativeObj));
    MOZ_ASSERT(global, "nativeObj not a global object!");

    JSObject *jsglobal = global->GetGlobalJSObject();
    if (jsglobal)
        *parentObj = jsglobal;
    return NS_OK;
}

nsresult
NS_NewBackstagePass(BackstagePass** ret)
{
    nsRefPtr<BackstagePass> bsp = new BackstagePass(
        nsContentUtils::GetSystemPrincipal());
    bsp.forget(ret);
    return NS_OK;
}
