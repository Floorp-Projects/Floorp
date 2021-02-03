/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This implementation has support only for http requests. It is because the
 * spec has defined event streams only for http. HTTP is required because
 * this implementation uses some http headers: "Last-Event-ID", "Cache-Control"
 * and "Accept".
 */

#ifndef mozilla_dom_EventSource_h
#define mozilla_dom_EventSource_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsDeque.h"
#include "nsICookieJarSettings.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

struct EventSourceInit;

class EventSourceImpl;

class EventSource final : public DOMEventTargetHelper {
  friend class EventSourceImpl;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EventSource, DOMEventTargetHelper)
  virtual bool IsCertainlyAliveForCC() const override;

  // EventTarget
  void DisconnectFromOwner() override {
    DOMEventTargetHelper::DisconnectFromOwner();
    Close();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  static already_AddRefed<EventSource> Constructor(
      const GlobalObject& aGlobal, const nsAString& aURL,
      const EventSourceInit& aEventSourceInitDict, ErrorResult& aRv);

  void GetUrl(nsAString& aURL) const {
    AssertIsOnTargetThread();
    aURL = mOriginalURL;
  }

  bool WithCredentials() const {
    AssertIsOnTargetThread();
    return mWithCredentials;
  }

  uint16_t ReadyState() const {
    AssertIsOnTargetThread();
    return mReadyState;
  }

  IMPL_EVENT_HANDLER(open)
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(error)

  void Close();

 private:
  EventSource(nsIGlobalObject* aGlobal,
              nsICookieJarSettings* aCookieJarSettings, bool aWithCredentials);
  virtual ~EventSource();
  // prevent bad usage
  EventSource(const EventSource& x) = delete;
  EventSource& operator=(const EventSource& x) = delete;

  void AssertIsOnTargetThread() const {
    MOZ_ASSERT(NS_IsMainThread() == mIsMainThread);
  }

  nsresult CreateAndDispatchSimpleEvent(const nsAString& aName);

  // This EventSourceImpl is created, managed and destroyed
  // by EventSource.
  RefPtr<EventSourceImpl> mESImpl;
  nsString mOriginalURL;
  Atomic<uint32_t> mReadyState;
  const bool mWithCredentials;
  const bool mIsMainThread;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EventSource_h
