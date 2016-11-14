/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOMIntersectionObserver_h
#define DOMIntersectionObserver_h

#include "mozilla/dom/IntersectionObserverBinding.h"
#include "nsCSSValue.h"
#include "nsTArray.h"

using mozilla::dom::DOMRect;
using mozilla::dom::Element;

namespace mozilla {
namespace dom {

class DOMIntersectionObserver;

class DOMIntersectionObserverEntry final : public nsISupports,
                                           public nsWrapperCache
{
  ~DOMIntersectionObserverEntry() {}

public:
  DOMIntersectionObserverEntry(nsISupports* aOwner,
                               DOMHighResTimeStamp aTime,
                               RefPtr<DOMRect> aRootBounds,
                               RefPtr<DOMRect> aBoundingClientRect,
                               RefPtr<DOMRect> aIntersectionRect,
                               Element* aTarget,
                               double aIntersectionRatio)
  : mOwner(aOwner),
    mTime(aTime),
    mRootBounds(aRootBounds),
    mBoundingClientRect(aBoundingClientRect),
    mIntersectionRect(aIntersectionRect),
    mTarget(aTarget),
    mIntersectionRatio(aIntersectionRatio)
  {
  }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMIntersectionObserverEntry)

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::IntersectionObserverEntryBinding::Wrap(aCx, this, aGivenProto);
  }

  DOMHighResTimeStamp Time()
  {
    return mTime;
  }

  DOMRect* GetRootBounds()
  {
    return mRootBounds;
  }

  DOMRect* BoundingClientRect()
  {
    return mBoundingClientRect;
  }

  DOMRect* IntersectionRect()
  {
    return mIntersectionRect;
  }

  double IntersectionRatio()
  {
    return mIntersectionRatio;
  }

  Element* Target()
  {
    return mTarget;
  }

protected:
  nsCOMPtr<nsISupports> mOwner;
  DOMHighResTimeStamp   mTime;
  RefPtr<DOMRect>       mRootBounds;
  RefPtr<DOMRect>       mBoundingClientRect;
  RefPtr<DOMRect>       mIntersectionRect;
  RefPtr<Element>       mTarget;
  double                mIntersectionRatio;
};

#define NS_DOM_INTERSECTION_OBSERVER_IID \
{ 0x8570a575, 0xe303, 0x4d18, \
  { 0xb6, 0xb1, 0x4d, 0x2b, 0x49, 0xd8, 0xef, 0x94 } }

class DOMIntersectionObserver final : public nsISupports,
                                      public nsWrapperCache
{
  virtual ~DOMIntersectionObserver() { }

public:
  DOMIntersectionObserver(already_AddRefed<nsPIDOMWindowInner>&& aOwner,
                          mozilla::dom::IntersectionCallback& aCb)
  : mOwner(aOwner), mCallback(&aCb), mConnected(false)
  {
  }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMIntersectionObserver)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_INTERSECTION_OBSERVER_IID)

  static already_AddRefed<DOMIntersectionObserver>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              mozilla::dom::IntersectionCallback& aCb,
              mozilla::ErrorResult& aRv);
  static already_AddRefed<DOMIntersectionObserver>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              mozilla::dom::IntersectionCallback& aCb,
              const mozilla::dom::IntersectionObserverInit& aOptions,
              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::IntersectionObserverBinding::Wrap(aCx, this, aGivenProto);
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  Element* GetRoot() const {
    return mRoot;
  }

  void GetRootMargin(mozilla::dom::DOMString& aRetVal);
  void GetThresholds(nsTArray<double>& aRetVal);
  void Observe(Element& aTarget);
  void Unobserve(Element& aTarget);

  bool UnlinkTarget(Element& aTarget);
  void Disconnect();

  void TakeRecords(nsTArray<RefPtr<DOMIntersectionObserverEntry>>& aRetVal);

  mozilla::dom::IntersectionCallback* IntersectionCallback() { return mCallback; }

  bool SetRootMargin(const nsAString& aString);

  void Update(nsIDocument* aDocument, DOMHighResTimeStamp time);
  void Notify();

protected:
  void Connect();
  void QueueIntersectionObserverEntry(Element* aTarget,
                                      DOMHighResTimeStamp time,
                                      const Maybe<nsRect>& aRootRect,
                                      const nsRect& aTargetRect,
                                      const Maybe<nsRect>& aIntersectionRect,
                                      double aIntersectionRatio);

  nsCOMPtr<nsPIDOMWindowInner>                    mOwner;
  RefPtr<mozilla::dom::IntersectionCallback>      mCallback;
  RefPtr<Element>                                 mRoot;
  nsCSSRect                                       mRootMargin;
  nsTArray<double>                                mThresholds;
  nsTHashtable<nsPtrHashKey<Element>>             mObservationTargets;
  nsTArray<RefPtr<DOMIntersectionObserverEntry>>  mQueuedEntries;
  bool                                            mConnected;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMIntersectionObserver, NS_DOM_INTERSECTION_OBSERVER_IID)

} // namespace dom
} // namespace mozilla

#endif
