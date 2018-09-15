/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * Struct for holding fullscreen request.
 */

#ifndef mozilla_FullscreenRequest_h
#define mozilla_FullscreenRequest_h

#include "mozilla/LinkedList.h"
#include "mozilla/PendingFullscreenEvent.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "nsIDocument.h"
#include "nsIScriptError.h"

namespace mozilla {

struct FullscreenRequest : public LinkedListElement<FullscreenRequest>
{
  typedef dom::Promise Promise;

  static UniquePtr<FullscreenRequest> Create(Element* aElement,
                                             dom::CallerType aCallerType,
                                             ErrorResult& aRv)
  {
    RefPtr<Promise> promise = Promise::Create(aElement->GetOwnerGlobal(), aRv);
    return WrapUnique(new FullscreenRequest(aElement, promise.forget(),
                                            aCallerType, true));
  }

  static UniquePtr<FullscreenRequest> CreateForRemote(Element* aElement)
  {
    return WrapUnique(new FullscreenRequest(aElement, nullptr,
                                            dom::CallerType::NonSystem,
                                            false));
  }

  FullscreenRequest(const FullscreenRequest&) = delete;

  ~FullscreenRequest()
  {
    MOZ_COUNT_DTOR(FullscreenRequest);
    MOZ_ASSERT_IF(mPromise,
                  mPromise->State() != Promise::PromiseState::Pending);
  }

  dom::Element* Element() const { return mElement; }
  nsIDocument* Document() const { return mDocument; }
  dom::Promise* GetPromise() const { return mPromise; }

  void MayResolvePromise() const
  {
    if (mPromise) {
      MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);
      mPromise->MaybeResolveWithUndefined();
    }
  }

  void MayRejectPromise() const
  {
    if (mPromise) {
      MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);
      mPromise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
    }
  }

  // Reject the fullscreen request with the given reason.
  // It will dispatch the fullscreenerror event.
  void Reject(const char* aReason) const
  {
    if (nsPresContext* presContext = mDocument->GetPresContext()) {
      auto pendingEvent = MakeUnique<PendingFullscreenEvent>(
          FullscreenEventType::Error, mDocument, mElement);
      presContext->RefreshDriver()->
        ScheduleFullscreenEvent(std::move(pendingEvent));
    }
    MayRejectPromise();
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("DOM"),
                                    mDocument,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    aReason);
  }

private:
  RefPtr<dom::Element> mElement;
  RefPtr<nsIDocument> mDocument;
  RefPtr<dom::Promise> mPromise;

public:
  // This value should be true if the fullscreen request is
  // originated from system code.
  const dom::CallerType mCallerType;
  // This value denotes whether we should trigger a NewOrigin event if
  // requesting fullscreen in its document causes the origin which is
  // fullscreen to change. We may want *not* to trigger that event if
  // we're calling RequestFullscreen() as part of a continuation of a
  // request in a subdocument in different process, whereupon the caller
  // need to send some notification itself with the real origin.
  const bool mShouldNotifyNewOrigin;

private:
  FullscreenRequest(dom::Element* aElement,
                    already_AddRefed<dom::Promise> aPromise,
                    dom::CallerType aCallerType,
                    bool aShouldNotifyNewOrigin)
    : mElement(aElement)
    , mDocument(aElement->OwnerDoc())
    , mPromise(aPromise)
    , mCallerType(aCallerType)
    , mShouldNotifyNewOrigin(aShouldNotifyNewOrigin)
  {
    MOZ_COUNT_CTOR(FullscreenRequest);
  }
};

} // namespace mozilla

#endif // mozilla_FullscreenRequest_h
