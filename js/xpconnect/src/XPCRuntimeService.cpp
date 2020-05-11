/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#include "nsContentUtils.h"
#include "BackstagePass.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(BackstagePass, nsIXPCScriptable, nsIGlobalObject,
                  nsIClassInfo, nsIScriptObjectPrincipal,
                  nsISupportsWeakReference)

BackstagePass::BackstagePass()
    : mPrincipal(nsContentUtils::GetSystemPrincipal()), mWrapper(nullptr) {}

// XXX(nika): It appears we don't have support for mayresolve hooks in
// nsIXPCScriptable, and I don't really want to add it because I'd rather just
// kill nsIXPCScriptable alltogether, so we don't use it here.

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME "BackstagePass"
#define XPC_MAP_FLAGS                                               \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
   XPC_SCRIPTABLE_WANT_FINALIZE | XPC_SCRIPTABLE_WANT_PRECREATE |   \
   XPC_SCRIPTABLE_USE_JSSTUB_FOR_ADDPROPERTY |                      \
   XPC_SCRIPTABLE_USE_JSSTUB_FOR_DELPROPERTY |                      \
   XPC_SCRIPTABLE_DONT_ENUM_QUERY_INTERFACE |                       \
   XPC_SCRIPTABLE_IS_GLOBAL_OBJECT |                                \
   XPC_SCRIPTABLE_DONT_REFLECT_INTERFACE_NAMES)
#include "xpc_map_end.h" /* This will #undef the above */

JSObject* BackstagePass::GetGlobalJSObject() {
  if (mWrapper) {
    return mWrapper->GetFlatJSObject();
  }
  return nullptr;
}

JSObject* BackstagePass::GetGlobalJSObjectPreserveColor() const {
  if (mWrapper) {
    return mWrapper->GetFlatJSObjectPreserveColor();
  }
  return nullptr;
}

void BackstagePass::SetGlobalObject(JSObject* global) {
  nsISupports* p = XPCWrappedNative::Get(global);
  MOZ_ASSERT(p);
  mWrapper = static_cast<XPCWrappedNative*>(p);
}

NS_IMETHODIMP
BackstagePass::Resolve(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                       JSObject* objArg, jsid idArg, bool* resolvedp,
                       bool* _retval) {
  JS::RootedObject obj(cx, objArg);
  JS::RootedId id(cx, idArg);
  *_retval =
      WebIDLGlobalNameHash::ResolveForSystemGlobal(cx, obj, id, resolvedp);
  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BackstagePass::NewEnumerate(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                            JSObject* objArg,
                            JS::MutableHandleIdVector properties,
                            bool enumerableOnly, bool* _retval) {
  JS::RootedObject obj(cx, objArg);
  *_retval = WebIDLGlobalNameHash::NewEnumerateSystemGlobal(cx, obj, properties,
                                                            enumerableOnly);
  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

/***************************************************************************/
NS_IMETHODIMP
BackstagePass::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCScriptable),
                           NS_GET_IID(nsIScriptObjectPrincipal)};
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetScriptableHelper(nsIXPCScriptable** retval) {
  nsCOMPtr<nsIXPCScriptable> scriptable = this;
  scriptable.forget(retval);
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("BackstagePass");
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetFlags(uint32_t* aFlags) {
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
BackstagePass::Finalize(nsIXPConnectWrappedNative* wrapper, JSFreeOp* fop,
                        JSObject* obj) {
  nsCOMPtr<nsIGlobalObject> bsp(do_QueryInterface(wrapper->Native()));
  MOZ_ASSERT(bsp);
  static_cast<BackstagePass*>(bsp.get())->ForgetGlobalObject();
  return NS_OK;
}

NS_IMETHODIMP
BackstagePass::PreCreate(nsISupports* nativeObj, JSContext* cx,
                         JSObject* globalObj, JSObject** parentObj) {
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
