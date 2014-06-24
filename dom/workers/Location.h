/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_location_h__
#define mozilla_dom_workers_location_h__

#include "Workers.h"
#include "WorkerPrivate.h"
#include "nsWrapperCache.h"

BEGIN_WORKERS_NAMESPACE

class WorkerLocation MOZ_FINAL : public nsWrapperCache
{
  nsString mHref;
  nsString mProtocol;
  nsString mHost;
  nsString mHostname;
  nsString mPort;
  nsString mPathname;
  nsString mSearch;
  nsString mHash;
  nsString mOrigin;

  WorkerLocation(const nsAString& aHref,
                 const nsAString& aProtocol,
                 const nsAString& aHost,
                 const nsAString& aHostname,
                 const nsAString& aPort,
                 const nsAString& aPathname,
                 const nsAString& aSearch,
                 const nsAString& aHash,
                 const nsAString& aOrigin)
    : mHref(aHref)
    , mProtocol(aProtocol)
    , mHost(aHost)
    , mHostname(aHostname)
    , mPort(aPort)
    , mPathname(aPathname)
    , mSearch(aSearch)
    , mHash(aHash)
    , mOrigin(aOrigin)
  {
    MOZ_COUNT_CTOR(WorkerLocation);
    SetIsDOMBinding();
  }

  ~WorkerLocation()
  {
    MOZ_COUNT_DTOR(WorkerLocation);
  }

public:

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WorkerLocation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WorkerLocation)

  static already_AddRefed<WorkerLocation>
  Create(WorkerPrivate::LocationInfo& aInfo);

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const {
    return nullptr;
  }

  void Stringify(nsString& aHref) const
  {
    aHref = mHref;
  }
  void GetHref(nsString& aHref) const
  {
    aHref = mHref;
  }
  void GetProtocol(nsString& aProtocol) const
  {
    aProtocol = mProtocol;
  }
  void GetHost(nsString& aHost) const
  {
    aHost = mHost;
  }
  void GetHostname(nsString& aHostname) const
  {
    aHostname = mHostname;
  }
  void GetPort(nsString& aPort) const
  {
    aPort = mPort;
  }
  void GetPathname(nsString& aPathname) const
  {
    aPathname = mPathname;
  }
  void GetSearch(nsString& aSearch) const
  {
    aSearch = mSearch;
  }
  void GetHash(nsString& aHash) const
  {
    aHash = mHash;
  }
  void GetOrigin(nsString& aOrigin) const
  {
    aOrigin = mOrigin;
  }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_location_h__
