/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGZoomEvent_h
#define mozilla_dom_SVGZoomEvent_h

#include "DOMSVGPoint.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/SVGZoomEventBinding.h"
#include "mozilla/EventForwards.h"
#include "nsAutoPtr.h"

class nsPresContext;

namespace mozilla {

class nsISVGPoint;

namespace dom {

class SVGZoomEvent MOZ_FINAL : public UIEvent
{
public:

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGZoomEvent, UIEvent)
  NS_DECL_ISUPPORTS_INHERITED

  SVGZoomEvent(EventTarget* aOwner, nsPresContext* aPresContext,
               InternalSVGZoomEvent* aEvent);

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return SVGZoomEventBinding::Wrap(aCx, this);
  }

  float PreviousScale() const
  {
    return mPreviousScale;
  }

  nsISVGPoint* GetPreviousTranslate() const
  {
    return mPreviousTranslate;
  }

  float NewScale() const
  {
    return mNewScale;
  }

  nsISVGPoint* GetNewTranslate() const
  {
    return mNewTranslate;
  }

private:
  ~SVGZoomEvent();

  float mPreviousScale;
  float mNewScale;
  nsRefPtr<DOMSVGPoint> mPreviousTranslate;
  nsRefPtr<DOMSVGPoint> mNewTranslate;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGZoomEvent_h
