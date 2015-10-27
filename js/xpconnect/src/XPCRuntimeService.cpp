/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#include "nsContentUtils.h"
#include "BackstagePass.h"
#include "nsDOMClassInfo.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/BindingUtils.h"

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
#define                             XPC_MAP_WANT_RESOLVE
#define                             XPC_MAP_WANT_ENUMERATE
#define                             XPC_MAP_WANT_FINALIZE
#define                             XPC_MAP_WANT_PRECREATE

#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   |  \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   |  \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    |  \
                            nsIXPCScriptable::IS_GLOBAL_OBJECT             |  \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */


JSObject*
BackstagePass::GetGlobalJSObject()
{
    if (mWrapper)
        return mWrapper->GetFlatJSObject();
    return nullptr;
}

void
BackstagePass::SetGlobalObject(JSObject* global)
{
    nsISupports* p = XPCWrappedNative::Get(global);
    MOZ_ASSERT(p);
    mWrapper = static_cast<XPCWrappedNative*>(p);
}

NS_IMETHODIMP
BackstagePass::Resolve(nsIXPConnectWrappedNative* wrapper,
                       JSContext * cx, JSObject * objArg,
                       jsid idArg, bool* resolvedp,
                       bool* _retval)
{
    JS::RootedObject obj(cx, objArg);
    JS::RootedId id(cx, idArg);
    *_retval = mozilla::dom::SystemGlobalResolve(cx, obj, id, resolvedp);
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BackstagePass::Enumerate(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                         JSObject* objArg, bool* _retval)
{
    JS::RootedObject obj(cx, objArg);
    *_retval = mozilla::dom::SystemGlobalEnumerate(cx, obj);
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

/***************************************************************************/
NS_IMETHODIMP
BackstagePass::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    const uint32_t count = 2;
    *aCount = count;
    nsIID** array;
    *aArray = array = static_cast<nsIID**>(moz_xmalloc(count * sizeof(nsIID*)));
    if (!array)
        return NS_ERROR_OUT_OF_MEMORY;

    uint32_t index = 0;
    nsIID* clone;
#define PUSH_IID(id)                                                          \
    clone = static_cast<nsIID*>(nsMemory::Clone(&NS_GET_IID( id ),           \
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
        free(array[--index]);
    free(array);
    *aArray = nullptr;
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
BackstagePass::GetScriptableHelper(nsIXPCScriptable** retval)
{
    nsCOMPtr<nsIXPCScriptable> scriptable = this;
    scriptable.forget(retval);
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetContractID(char * *aContractID)
{
    *aContractID = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "BackstagePass";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
BackstagePass::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetFlags(uint32_t* aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::Finalize(nsIXPConnectWrappedNative* wrapper, JSFreeOp * fop, JSObject * obj)
{
    nsCOMPtr<nsIGlobalObject> bsp(do_QueryWrappedNative(wrapper));
    MOZ_ASSERT(bsp);
    static_cast<BackstagePass*>(bsp.get())->ForgetGlobalObject();
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::PreCreate(nsISupports* nativeObj, JSContext* cx,
                         JSObject* globalObj, JSObject** parentObj)
{
    // We do the same trick here as for WindowSH. Return the js global
    // as parent, so XPConenct can find the right scope and the wrapper
    // that already exists.
    nsCOMPtr<nsIGlobalObject> global(do_QueryInterface(nativeObj));
    MOZ_ASSERT(global, "nativeObj not a global object!");

    JSObject* jsglobal = global->GetGlobalJSObject();
    if (jsglobal)
        *parentObj = jsglobal;
    return NS_OK;
}

nsresult
NS_NewBackstagePass(BackstagePass** ret)
{
    RefPtr<BackstagePass> bsp = new BackstagePass(
        nsContentUtils::GetSystemPrincipal());
    bsp.forget(ret);
    return NS_OK;
}
