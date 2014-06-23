/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_navigator_h__
#define mozilla_dom_workers_navigator_h__

#include "Workers.h"
#include "nsString.h"
#include "nsWrapperCache.h"

// Need this to use Navigator::HasDataStoreSupport() in
// WorkerNavigatorBinding.cpp
#include "mozilla/dom/Navigator.h"

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_WORKERS_NAMESPACE

class WorkerNavigator MOZ_FINAL : public nsWrapperCache
{
  nsString mAppName;
  nsString mAppVersion;
  nsString mPlatform;
  nsString mUserAgent;
  bool mOnline;

  WorkerNavigator(const nsAString& aAppName,
                  const nsAString& aAppVersion,
                  const nsAString& aPlatform,
                  const nsAString& aUserAgent,
                  bool aOnline)
    : mAppName(aAppName)
    , mAppVersion(aAppVersion)
    , mPlatform(aPlatform)
    , mUserAgent(aUserAgent)
    , mOnline(aOnline)
  {
    MOZ_COUNT_CTOR(WorkerNavigator);
    SetIsDOMBinding();
  }

  ~WorkerNavigator()
  {
    MOZ_COUNT_DTOR(WorkerNavigator);
  }

public:

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WorkerNavigator)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WorkerNavigator)

  static already_AddRefed<WorkerNavigator>
  Create(bool aOnLine);

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const {
    return nullptr;
  }

  void GetAppCodeName(nsString& aAppCodeName) const
  {
    aAppCodeName.AssignLiteral("Mozilla");
  }
  void GetAppName(nsString& aAppName) const
  {
    aAppName = mAppName;
  }

  void GetAppVersion(nsString& aAppVersion) const
  {
    aAppVersion = mAppVersion;
  }

  void GetPlatform(nsString& aPlatform) const
  {
    aPlatform = mPlatform;
  }
  void GetProduct(nsString& aProduct) const
  {
    aProduct.AssignLiteral("Gecko");
  }
  bool TaintEnabled() const
  {
    return false;
  }

  void GetUserAgent(nsString& aUserAgent) const
  {
    aUserAgent = mUserAgent;
  }

  bool OnLine() const
  {
    return mOnline;
  }

  // Worker thread only!
  void SetOnLine(bool aOnline)
  {
    mOnline = aOnline;
  }

  already_AddRefed<Promise> GetDataStores(JSContext* aCx,
                                          const nsAString& aName,
                                          ErrorResult& aRv);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_navigator_h__
