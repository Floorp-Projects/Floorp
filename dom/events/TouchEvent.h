/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_TouchEvent_h_
#define mozilla_dom_TouchEvent_h_

#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchEventBinding.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TouchEvents.h"
#include "nsJSEnvironment.h"
#include "nsStringFwd.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class TouchList final : public nsISupports
                      , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TouchList)

  explicit TouchList(nsISupports* aParent)
    : mParent(aParent)
  {
    nsJSContext::LikelyShortLivingObjectCreated();
  }
  TouchList(nsISupports* aParent,
            const WidgetTouchEvent::TouchArray& aTouches)
    : mParent(aParent)
    , mPoints(aTouches)
  {
    nsJSContext::LikelyShortLivingObjectCreated();
  }

  void Append(Touch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal);

  uint32_t Length() const
  {
    return mPoints.Length();
  }
  Touch* Item(uint32_t aIndex) const
  {
    return mPoints.SafeElementAt(aIndex);
  }
  Touch* IndexedGetter(uint32_t aIndex, bool& aFound) const
  {
    aFound = aIndex < mPoints.Length();
    if (!aFound) {
      return nullptr;
    }
    return mPoints[aIndex];
  }

  void Clear()
  {
    mPoints.Clear();
  }

protected:
  ~TouchList() {}

  nsCOMPtr<nsISupports> mParent;
  WidgetTouchEvent::TouchArray mPoints;
};

class TouchEvent : public UIEvent
{
public:
  TouchEvent(EventTarget* aOwner,
             nsPresContext* aPresContext,
             WidgetTouchEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TouchEvent, UIEvent)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return TouchEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  already_AddRefed<TouchList>
  CopyTouches(const Sequence<OwningNonNull<Touch>>& aTouches);

  TouchList* Touches();
  // TargetTouches() populates mTargetTouches from widget event's touch list.
  TouchList* TargetTouches();
  // GetExistingTargetTouches just returns the existing target touches list.
  TouchList* GetExistingTargetTouches()
  {
    return mTargetTouches;
  }
  TouchList* ChangedTouches();

  bool AltKey();
  bool MetaKey();
  bool CtrlKey();
  bool ShiftKey();

  void InitTouchEvent(const nsAString& aType,
                      bool aCanBubble,
                      bool aCancelable,
                      nsGlobalWindowInner* aView,
                      int32_t aDetail,
                      bool aCtrlKey,
                      bool aAltKey,
                      bool aShiftKey,
                      bool aMetaKey,
                      TouchList* aTouches,
                      TouchList* aTargetTouches,
                      TouchList* aChangedTouches);

  static bool PlatformSupportsTouch();
  static bool PrefEnabled(JSContext* aCx, JSObject* aGlobal);
  static bool PrefEnabled(nsIDocShell* aDocShell);

  static already_AddRefed<TouchEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const TouchEventInit& aParam,
                                                  ErrorResult& aRv);

protected:
  ~TouchEvent() {}

  void AssignTouchesToWidgetEvent(TouchList* aList, bool aCheckDuplicates);

  RefPtr<TouchList> mTouches;
  RefPtr<TouchList> mTargetTouches;
  RefPtr<TouchList> mChangedTouches;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::TouchEvent>
NS_NewDOMTouchEvent(mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    mozilla::WidgetTouchEvent* aEvent);

#endif // mozilla_dom_TouchEvent_h_
