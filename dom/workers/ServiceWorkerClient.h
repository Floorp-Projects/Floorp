/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_workers_serviceworkerclient_h
#define mozilla_dom_workers_serviceworkerclient_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ClientBinding.h"

class nsIDocument;

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerClient;
class ServiceWorkerWindowClient;

// Used as a container object for information needed to create
// client objects.
class ServiceWorkerClientInfo final
{
  friend class ServiceWorkerClient;
  friend class ServiceWorkerWindowClient;

public:
  explicit ServiceWorkerClientInfo(nsIDocument* aDoc, uint32_t aOrdinal = 0);

  const nsString& ClientId() const
  {
    return mClientId;
  }

  bool operator<(const ServiceWorkerClientInfo& aRight) const;
  bool operator==(const ServiceWorkerClientInfo& aRight) const;

private:
  const mozilla::dom::ClientType mType;
  const uint32_t mOrdinal;
  nsString mClientId;
  uint64_t mWindowId;
  nsString mUrl;

  // Window Clients
  VisibilityState mVisibilityState;
  FrameType mFrameType;
  TimeStamp mLastFocusTime;
  bool mFocused;
};

class ServiceWorkerClient : public nsISupports,
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ServiceWorkerClient)

  ServiceWorkerClient(nsISupports* aOwner,
                      const ServiceWorkerClientInfo& aClientInfo)
    : mOwner(aOwner)
    , mType(aClientInfo.mType)
    , mId(aClientInfo.mClientId)
    , mUrl(aClientInfo.mUrl)
    , mWindowId(aClientInfo.mWindowId)
    , mFrameType(aClientInfo.mFrameType)
  {
    MOZ_ASSERT(aOwner);
  }

  nsISupports*
  GetParentObject() const
  {
    return mOwner;
  }

  void GetId(nsString& aRetval) const
  {
    aRetval = mId;
  }

  void
  GetUrl(nsAString& aUrl) const
  {
    aUrl.Assign(mUrl);
  }

  mozilla::dom::FrameType
  FrameType() const
  {
    return mFrameType;
  }

  mozilla::dom::ClientType
  Type() const;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~ServiceWorkerClient()
  { }

private:
  nsCOMPtr<nsISupports> mOwner;
  const ClientType mType;
  nsString mId;
  nsString mUrl;

protected:
  uint64_t mWindowId;
  mozilla::dom::FrameType mFrameType;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerclient_h
