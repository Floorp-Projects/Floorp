/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGZoomEvent_h
#define mozilla_dom_SVGZoomEvent_h

#include "DOMSVGPoint.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/SVGZoomEventBinding.h"
#include "mozilla/EventForwards.h"

class nsPresContext;

namespace mozilla {

class nsISVGPoint;

namespace dom {

class SVGZoomEvent final : public UIEvent
{
public:

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGZoomEvent, UIEvent)
  NS_DECL_ISUPPORTS_INHERITED

  SVGZoomEvent(EventTarget* aOwner, nsPresContext* aPresContext,
               InternalSVGZoomEvent* aEvent);

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return SVGZoomEventBinding::Wrap(aCx, this, aGivenProto);
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
  RefPtr<DOMSVGPoint> mPreviousTranslate;
  RefPtr<DOMSVGPoint> mNewTranslate;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::SVGZoomEvent>
NS_NewDOMSVGZoomEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      mozilla::InternalSVGZoomEvent* aEvent);

#endif // mozilla_dom_SVGZoomEvent_h
