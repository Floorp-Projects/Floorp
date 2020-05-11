/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BackstagePass_h__
#define BackstagePass_h__

#include "nsISupports.h"
#include "nsWeakReference.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIXPCScriptable.h"

#include "js/HeapAPI.h"

class XPCWrappedNative;

class BackstagePass : public nsIGlobalObject,
                      public nsIScriptObjectPrincipal,
                      public nsIXPCScriptable,
                      public nsIClassInfo,
                      public nsSupportsWeakReference {
 public:
  BackstagePass();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  virtual nsIPrincipal* GetPrincipal() override { return mPrincipal; }

  virtual nsIPrincipal* GetEffectiveStoragePrincipal() override {
    return mPrincipal;
  }

  virtual nsIPrincipal* IntrinsicStoragePrincipal() override {
    return mPrincipal;
  }

  JSObject* GetGlobalJSObject() override;
  JSObject* GetGlobalJSObjectPreserveColor() const override;

  void ForgetGlobalObject() { mWrapper = nullptr; }

  void SetGlobalObject(JSObject* global);

 private:
  virtual ~BackstagePass() = default;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  XPCWrappedNative* mWrapper;
};

#endif  // BackstagePass_h__
