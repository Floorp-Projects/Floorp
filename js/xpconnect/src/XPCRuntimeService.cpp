/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#include "nsContentUtils.h"
#include "BackstagePass.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/BindingUtils.h"

NS_IMPL_ISUPPORTS(BackstagePass,
                  nsIXPCScriptable,
                  nsIGlobalObject,
                  nsIClassInfo,
                  nsIScriptObjectPrincipal,
                  nsISupportsWeakReference)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME "BackstagePass"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_ENUMERATE | \
                       XPC_SCRIPTABLE_WANT_FINALIZE | \
                       XPC_SCRIPTABLE_WANT_PRECREATE | \
                       XPC_SCRIPTABLE_USE_JSSTUB_FOR_ADDPROPERTY |  \
                       XPC_SCRIPTABLE_USE_JSSTUB_FOR_DELPROPERTY |  \
                       XPC_SCRIPTABLE_DONT_ENUM_QUERY_INTERFACE |  \
                       XPC_SCRIPTABLE_IS_GLOBAL_OBJECT |  \
                       XPC_SCRIPTABLE_DONT_REFLECT_INTERFACE_NAMES)
#include "xpc_map_end.h" /* This will #undef the above */


JSObject*
BackstagePass::GetGlobalJSObject()
{
    if (mWrapper) {
        return mWrapper->GetFlatJSObject();
    }
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
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCScriptable).Clone();
    array[1] = NS_GET_IID(nsIScriptObjectPrincipal).Clone();
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetScriptableHelper(nsIXPCScriptable** retval)
{
    nsCOMPtr<nsIXPCScriptable> scriptable = this;
    scriptable.forget(retval);
    return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("BackstagePass");
    return NS_OK;
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
    if (jsglobal) {
        *parentObj = jsglobal;
    }
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
