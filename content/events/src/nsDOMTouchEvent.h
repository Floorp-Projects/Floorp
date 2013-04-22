/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMTouchEvent_h_
#define nsDOMTouchEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMTouchEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "nsJSEnvironment.h"
#include "mozilla/dom/TouchEventBinding.h"

class nsDOMTouchList MOZ_FINAL : public nsIDOMTouchList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouchList)
  NS_DECL_NSIDOMTOUCHLIST

  nsDOMTouchList()
  {
    nsJSContext::LikelyShortLivingObjectCreated();
  }
  nsDOMTouchList(nsTArray<nsCOMPtr<nsIDOMTouch> > &aTouches);

  void Append(nsIDOMTouch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

  nsIDOMTouch* GetItemAt(uint32_t aIndex)
  {
    return mPoints.SafeElementAt(aIndex, nullptr);
  }

protected:
  nsTArray<nsCOMPtr<nsIDOMTouch> > mPoints;
};

class nsDOMTouchEvent : public nsDOMUIEvent,
                        public nsIDOMTouchEvent
{
public:
  nsDOMTouchEvent(mozilla::dom::EventTarget* aOwner,
                  nsPresContext* aPresContext, nsTouchEvent* aEvent);
  virtual ~nsDOMTouchEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_DECL_NSIDOMTOUCHEVENT

  NS_FORWARD_TO_NSDOMUIEVENT

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::TouchEventBinding::Wrap(aCx, aScope, this);
  }

  already_AddRefed<nsIDOMTouchList> GetTouches()
  {
    nsCOMPtr<nsIDOMTouchList> t;
    GetTouches(getter_AddRefs(t));
    return t.forget();
  }

  already_AddRefed<nsIDOMTouchList> GetTargetTouches()
  {
    nsCOMPtr<nsIDOMTouchList> t;
    GetTargetTouches(getter_AddRefs(t));
    return t.forget();
  }

  already_AddRefed<nsIDOMTouchList> GetChangedTouches()
  {
    nsCOMPtr<nsIDOMTouchList> t;
    GetChangedTouches(getter_AddRefs(t));
    return t.forget();
  }

  bool AltKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsAlt();
  }

  bool MetaKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsMeta();
  }

  bool CtrlKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsControl();
  }

  bool ShiftKey()
  {
    return static_cast<nsInputEvent*>(mEvent)->IsShift();
  }

  void InitTouchEvent(const nsAString& aType,
                      bool aCanBubble,
                      bool aCancelable,
                      nsIDOMWindow* aView,
                      int32_t aDetail,
                      bool aCtrlKey,
                      bool aAltKey,
                      bool aShiftKey,
                      bool aMetaKey,
                      nsIDOMTouchList* aTouches,
                      nsIDOMTouchList* aTargetTouches,
                      nsIDOMTouchList* aChangedTouches,
                      mozilla::ErrorResult& aRv)
  {
    aRv = InitTouchEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                         aCtrlKey, aAltKey, aShiftKey, aMetaKey,
                         aTouches, aTargetTouches, aChangedTouches);
  }

  static bool PrefEnabled();
protected:
  nsCOMPtr<nsIDOMTouchList> mTouches;
  nsCOMPtr<nsIDOMTouchList> mTargetTouches;
  nsCOMPtr<nsIDOMTouchList> mChangedTouches;
};

#endif /* !defined(nsDOMTouchEvent_h_) */
