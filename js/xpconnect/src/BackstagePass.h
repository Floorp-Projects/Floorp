/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BackstagePass_h__
#define BackstagePass_h__

#include "js/loader/ModuleLoaderBase.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StorageAccess.h"
#include "nsISupports.h"
#include "nsWeakReference.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIXPCScriptable.h"

#include "js/HeapAPI.h"

class XPCWrappedNative;

class BackstagePass final : public nsIGlobalObject,
                            public nsIScriptObjectPrincipal,
                            public nsIXPCScriptable,
                            public nsIClassInfo,
                            public nsSupportsWeakReference {
 public:
  BackstagePass();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  using ModuleLoaderBase = JS::loader::ModuleLoaderBase;

  nsIPrincipal* GetPrincipal() override { return mPrincipal; }

  nsIPrincipal* GetEffectiveCookiePrincipal() override { return mPrincipal; }

  nsIPrincipal* GetEffectiveStoragePrincipal() override { return mPrincipal; }

  nsIPrincipal* PartitionedPrincipal() override { return mPrincipal; }

  mozilla::OriginTrials Trials() const override { return {}; }

  JSObject* GetGlobalJSObject() override;
  JSObject* GetGlobalJSObjectPreserveColor() const override;

  ModuleLoaderBase* GetModuleLoader(JSContext* aCx) override {
    return mModuleLoader;
  }

  mozilla::StorageAccess GetStorageAccess() final {
    MOZ_ASSERT(NS_IsMainThread());
    return mozilla::StorageAccess::eAllow;
  }

  mozilla::Result<mozilla::ipc::PrincipalInfo, nsresult> GetStorageKey()
      override;

  void ForgetGlobalObject() { mWrapper = nullptr; }

  void SetGlobalObject(JSObject* global);

  void InitModuleLoader(ModuleLoaderBase* aModuleLoader) {
    MOZ_ASSERT(!mModuleLoader);
    mModuleLoader = aModuleLoader;
  }

  nsISerialEventTarget* SerialEventTarget() const final {
    return mozilla::GetMainThreadSerialEventTarget();
  }
  nsresult Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) const final {
    return mozilla::SchedulerGroup::Dispatch(std::move(aRunnable));
  }

  bool ShouldResistFingerprinting(RFPTarget aTarget) const override {
    // BackstagePass is always the System Principal
    MOZ_RELEASE_ASSERT(mPrincipal->IsSystemPrincipal());
    return false;
  }

 private:
  virtual ~BackstagePass() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  XPCWrappedNative* mWrapper;

  RefPtr<JS::loader::ModuleLoaderBase> mModuleLoader;
};

#endif  // BackstagePass_h__
