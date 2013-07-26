/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_navigator_h__
#define mozilla_dom_workers_navigator_h__

#include "Workers.h"
#include "mozilla/dom/workers/bindings/DOMBindingBase.h"
#include "nsStringGlue.h"

BEGIN_WORKERS_NAMESPACE

class WorkerNavigator MOZ_FINAL : public DOMBindingBase
{
  nsString mAppName;
  nsString mAppVersion;
  nsString mPlatform;
  nsString mUserAgent;

  WorkerNavigator(JSContext* aCx,
                  const nsAString& aAppName,
                  const nsAString& aAppVersion,
                  const nsAString& aPlatform,
                  const nsAString& aUserAgent)
    : DOMBindingBase(aCx)
    , mAppName(aAppName)
    , mAppVersion(aAppVersion)
    , mPlatform(aPlatform)
    , mUserAgent(aUserAgent)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerNavigator);
  }

public:
  static already_AddRefed<WorkerNavigator>
  Create(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  ~WorkerNavigator()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerNavigator);
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
  void GetUserAgent(nsString& aUserAgent) const
  {
    aUserAgent = mUserAgent;
  }

};
END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_navigator_h__
