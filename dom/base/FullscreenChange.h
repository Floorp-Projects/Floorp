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
#include "mozilla/dom/Document.h"
#include "nsIScriptError.h"
#include "nsRefreshDriver.h"

namespace mozilla {

class FullscreenChange : public LinkedListElement<FullscreenChange> {
 public:
  FullscreenChange(const FullscreenChange&) = delete;

  enum ChangeType {
    eEnter,
    eExit,
  };

  ChangeType Type() const { return mType; }
  dom::Document* Document() const { return mDocument; }
  dom::Promise* GetPromise() const { return mPromise; }

  void MayResolvePromise() const {
    if (mPromise) {
      MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);
      mPromise->MaybeResolveWithUndefined();
    }
  }

  void MayRejectPromise(const nsACString& aMessage) {
    if (mPromise) {
      MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);
      mPromise->MaybeRejectWithTypeError(aMessage);
    }
  }
  template <int N>
  void MayRejectPromise(const char (&aMessage)[N]) {
    MayRejectPromise(nsLiteralCString(aMessage));
  }

 protected:
  typedef dom::Promise Promise;

  FullscreenChange(ChangeType aType, dom::Document* aDocument,
                   already_AddRefed<Promise> aPromise)
      : mType(aType), mDocument(aDocument), mPromise(aPromise) {
    MOZ_ASSERT(aDocument);
  }

  ~FullscreenChange() {
    MOZ_ASSERT_IF(mPromise,
                  mPromise->State() != Promise::PromiseState::Pending);
  }

 private:
  ChangeType mType;
  nsCOMPtr<dom::Document> mDocument;
  RefPtr<Promise> mPromise;
};

class FullscreenRequest : public FullscreenChange {
 public:
  static const ChangeType kType = eEnter;

  static UniquePtr<FullscreenRequest> Create(dom::Element* aElement,
                                             dom::CallerType aCallerType,
                                             ErrorResult& aRv) {
    RefPtr<Promise> promise = Promise::Create(aElement->GetOwnerGlobal(), aRv);
    return WrapUnique(
        new FullscreenRequest(aElement, promise.forget(), aCallerType, true));
  }

  static UniquePtr<FullscreenRequest> CreateForRemote(dom::Element* aElement) {
    return WrapUnique(new FullscreenRequest(aElement, nullptr,
                                            dom::CallerType::NonSystem, false));
  }

  MOZ_COUNTED_DTOR(FullscreenRequest)

  dom::Element* Element() const { return mElement; }

  // Reject the fullscreen request with the given reason.
  // It will dispatch the fullscreenerror event.
  void Reject(const char* aReason) {
    if (nsPresContext* presContext = Document()->GetPresContext()) {
      auto pendingEvent = MakeUnique<PendingFullscreenEvent>(
          FullscreenEventType::Error, Document(), mElement);
      presContext->RefreshDriver()->ScheduleFullscreenEvent(
          std::move(pendingEvent));
    }
    MayRejectPromise("Fullscreen request denied");
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                    Document(), nsContentUtils::eDOM_PROPERTIES,
                                    aReason);
  }

 private:
  RefPtr<dom::Element> mElement;

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
                    dom::CallerType aCallerType, bool aShouldNotifyNewOrigin)
      : FullscreenChange(kType, aElement->OwnerDoc(), std::move(aPromise)),
        mElement(aElement),
        mCallerType(aCallerType),
        mShouldNotifyNewOrigin(aShouldNotifyNewOrigin) {
    MOZ_COUNT_CTOR(FullscreenRequest);
  }
};

class FullscreenExit : public FullscreenChange {
 public:
  static const ChangeType kType = eExit;

  static UniquePtr<FullscreenExit> Create(dom::Document* aDoc,
                                          ErrorResult& aRv) {
    RefPtr<Promise> promise = Promise::Create(aDoc->GetOwnerGlobal(), aRv);
    return WrapUnique(new FullscreenExit(aDoc, promise.forget()));
  }

  static UniquePtr<FullscreenExit> CreateForRemote(dom::Document* aDoc) {
    return WrapUnique(new FullscreenExit(aDoc, nullptr));
  }

  MOZ_COUNTED_DTOR(FullscreenExit)

 private:
  FullscreenExit(dom::Document* aDoc, already_AddRefed<Promise> aPromise)
      : FullscreenChange(kType, aDoc, std::move(aPromise)) {
    MOZ_COUNT_CTOR(FullscreenExit);
  }
};

}  // namespace mozilla

#endif  // mozilla_FullscreenRequest_h
