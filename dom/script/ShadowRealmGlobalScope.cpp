/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowInner.h"
#include "nsIGlobalObject.h"
#include "xpcpublic.h"
#include "js/TypeDecls.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ModuleLoader.h"
#include "mozilla/dom/ShadowRealmGlobalScope.h"
#include "mozilla/dom/ShadowRealmGlobalScopeBinding.h"

#include "js/loader/ModuleLoaderBase.h"

using namespace JS::loader;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ShadowRealmGlobalScope, mModuleLoader,
                                      mCreatingGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ShadowRealmGlobalScope)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ShadowRealmGlobalScope)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ShadowRealmGlobalScope)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(ShadowRealmGlobalScope)
NS_INTERFACE_MAP_END

JSObject* NewShadowRealmGlobal(JSContext* aCx, JS::RealmOptions& aOptions,
                               JSPrincipals* aPrincipals,
                               JS::Handle<JSObject*> aGlobalObj) {
  GlobalObject global(aCx, aGlobalObj);

  nsCOMPtr<nsIGlobalObject> nsGlobal =
      do_QueryInterface(global.GetAsSupports());

  MOZ_ASSERT(nsGlobal);

  RefPtr<ShadowRealmGlobalScope> scope = new ShadowRealmGlobalScope(nsGlobal);

  JS::Rooted<JSObject*> reflector(aCx);

  ShadowRealmGlobalScope_Binding::Wrap(aCx, scope, scope, aOptions, aPrincipals,
                                       true, &reflector);

  return reflector;
}

static nsIGlobalObject* FindEnclosingNonShadowRealmGlobal(
    ShadowRealmGlobalScope* scope) {
  nsCOMPtr<nsIGlobalObject> global = scope->GetCreatingGlobal();

  do {
    nsCOMPtr<ShadowRealmGlobalScope> shadowRealmGlobalScope =
        do_QueryInterface(global);
    if (!shadowRealmGlobalScope) {
      break;
    }

    // Our global was a ShadowRealmGlobal; that's a problem, as we can't find a
    // window or worker global associated with a ShadowRealmGlobal... so we
    // continue following the chain.
    //
    // This will happen if you have nested ShadowRealms.
    global = shadowRealmGlobalScope->GetCreatingGlobal();
  } while (true);

  return global;
}

ModuleLoaderBase* ShadowRealmGlobalScope::GetModuleLoader(JSContext* aCx) {
  if (mModuleLoader) {
    return mModuleLoader;
  }

  // Note: if this fails, we don't need to report an exception, as one will be
  // reported by ModuleLoaderBase::GetCurrentModuleLoader.

  // Don't bother asking the ShadowRealmGlobal itself for a host object to get a
  // module loader from, instead, delegate to the enclosing global of the shadow
  // realm
  nsCOMPtr<nsIGlobalObject> global = FindEnclosingNonShadowRealmGlobal(this);
  MOZ_RELEASE_ASSERT(global);

  JSObject* object = global->GetGlobalJSObject();
  MOZ_ASSERT(object);

  // Currently Workers will never get here, because dynamic import is disabled
  // in Worker context, and so importValue will throw before we get here.
  //
  // See  https://bugzilla.mozilla.org/show_bug.cgi?id=1247687 and
  //      https://bugzilla.mozilla.org/show_bug.cgi?id=1772162
  nsGlobalWindowInner* window = xpc::WindowGlobalOrNull(object);
  if (!window) {
    return nullptr;
  }

  RefPtr<Document> doc = window->GetExtantDoc();
  if (!doc) {
    return nullptr;
  }

  ScriptLoader* scriptLoader = doc->ScriptLoader();

  mModuleLoader = new ModuleLoader(scriptLoader, this, ModuleLoader::Normal);

  // Register the shadow realm module loader for tracing and ownership.
  scriptLoader->RegisterShadowRealmModuleLoader(
      static_cast<ModuleLoader*>(mModuleLoader.get()));

  return mModuleLoader;
}

}  // namespace mozilla::dom
