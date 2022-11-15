/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SANDBOXPRIVATE_H__
#define __SANDBOXPRIVATE_H__

#include "mozilla/WeakPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/net/CookieJarSettings.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsGlobalWindowInner.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

#include "js/loader/ModuleLoaderBase.h"

#include "js/Object.h"  // JS::GetPrivate, JS::SetPrivate
#include "js/RootingAPI.h"

class SandboxPrivate : public nsIGlobalObject,
                       public nsIScriptObjectPrincipal,
                       public nsSupportsWeakReference,
                       public mozilla::SupportsWeakPtr,
                       public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(SandboxPrivate,
                                                        nsIGlobalObject)

  static void Create(nsIPrincipal* principal, JS::Handle<JSObject*> global) {
    RefPtr<SandboxPrivate> sbp = new SandboxPrivate(principal);
    sbp->SetWrapper(global);
    sbp->PreserveWrapper(ToSupports(sbp.get()));

    // Pass on ownership of sbp to |global|.
    // The type used to cast to void needs to match the one in GetPrivate.
    nsIScriptObjectPrincipal* sop =
        static_cast<nsIScriptObjectPrincipal*>(sbp.forget().take());
    JS::SetObjectISupports(global, sop);
  }

  static SandboxPrivate* GetPrivate(JSObject* obj) {
    // The type used to cast to void needs to match the one in Create.
    nsIScriptObjectPrincipal* sop =
        JS::GetObjectISupports<nsIScriptObjectPrincipal>(obj);
    return static_cast<SandboxPrivate*>(sop);
  }

  mozilla::OriginTrials Trials() const final { return {}; }

  nsIPrincipal* GetPrincipal() override { return mPrincipal; }

  nsIPrincipal* GetEffectiveCookiePrincipal() override { return mPrincipal; }

  nsIPrincipal* GetEffectiveStoragePrincipal() override { return mPrincipal; }

  mozilla::Result<mozilla::ipc::PrincipalInfo, nsresult> GetStorageKey()
      override {
    mozilla::ipc::PrincipalInfo principalInfo;
    nsresult rv = PrincipalToPrincipalInfo(mPrincipal, &principalInfo);
    // Block expanded and null principals, let content and system through
    // (system is for xpcshell tests)
    if (principalInfo.type() !=
            mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
        principalInfo.type() !=
            mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo) {
      rv = NS_ERROR_DOM_SECURITY_ERR;
    }
    if (NS_FAILED(rv)) {
      return mozilla::Err(rv);
    }
    return std::move(principalInfo);
  }

  nsIPrincipal* PartitionedPrincipal() override { return mPrincipal; }

  JSObject* GetGlobalJSObject() override { return GetWrapper(); }
  JSObject* GetGlobalJSObjectPreserveColor() const override {
    return GetWrapperPreserveColor();
  }

  mozilla::StorageAccess GetStorageAccess() final {
    MOZ_ASSERT(NS_IsMainThread());
    if (mozilla::StaticPrefs::dom_serviceWorkers_testing_enabled()) {
      // XXX: This is a hack to workaround bug 1732159 and is not intended
      return mozilla::StorageAccess::eAllow;
    }
    nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
        mozilla::net::CookieJarSettings::Create(mPrincipal);
    return mozilla::StorageAllowedForServiceWorker(mPrincipal,
                                                   cookieJarSettings);
  }

  void ForgetGlobalObject(JSObject* obj) { ClearWrapper(obj); }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override {
    MOZ_CRASH("SandboxPrivate doesn't use DOM bindings!");
  }

  JS::loader::ModuleLoaderBase* GetModuleLoader(JSContext* aCx) override;

  size_t ObjectMoved(JSObject* obj, JSObject* old) {
    UpdateWrapper(obj, old);
    return 0;
  }

 private:
  explicit SandboxPrivate(nsIPrincipal* principal) : mPrincipal(principal) {}

  virtual ~SandboxPrivate() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  RefPtr<JS::loader::ModuleLoaderBase> mModuleLoader;
};

#endif  // __SANDBOXPRIVATE_H__
