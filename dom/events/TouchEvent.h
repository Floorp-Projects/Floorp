/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
#include "nsWrapperCache.h"

class nsAString;

namespace mozilla {
namespace dom {

class TouchList MOZ_FINAL : public nsISupports
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

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  static bool PrefEnabled(JSContext* aCx = nullptr,
                          JSObject* aGlobal = nullptr);

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
  Touch* IdentifiedTouch(int32_t aIdentifier) const;

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

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return TouchEventBinding::Wrap(aCx, this);
  }

  TouchList* Touches();
  TouchList* TargetTouches();
  TouchList* ChangedTouches();

  bool AltKey();
  bool MetaKey();
  bool CtrlKey();
  bool ShiftKey();

  void InitTouchEvent(const nsAString& aType,
                      bool aCanBubble,
                      bool aCancelable,
                      nsIDOMWindow* aView,
                      int32_t aDetail,
                      bool aCtrlKey,
                      bool aAltKey,
                      bool aShiftKey,
                      bool aMetaKey,
                      TouchList* aTouches,
                      TouchList* aTargetTouches,
                      TouchList* aChangedTouches,
                      ErrorResult& aRv);

  static bool PrefEnabled(JSContext* aCx = nullptr,
                          JSObject* aGlobal = nullptr);

protected:
  ~TouchEvent() {}

  nsRefPtr<TouchList> mTouches;
  nsRefPtr<TouchList> mTargetTouches;
  nsRefPtr<TouchList> mChangedTouches;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TouchEvent_h_
