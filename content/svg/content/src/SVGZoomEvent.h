/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGZoomEvent_h
#define mozilla_dom_SVGZoomEvent_h

#include "nsAutoPtr.h"
#include "nsDOMUIEvent.h"
#include "nsIDOMSVGZoomEvent.h"

class nsGUIEvent;
class nsPresContext;

namespace mozilla {
class DOMSVGPoint;

namespace dom {

class SVGZoomEvent : public nsDOMUIEvent,
                     public nsIDOMSVGZoomEvent
{
public:
  SVGZoomEvent(EventTarget* aOwner, nsPresContext* aPresContext,
               nsGUIEvent* aEvent);
                     
  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMSVGZoomEvent interface:
  NS_DECL_NSIDOMSVGZOOMEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMUIEVENT

private:
  float mPreviousScale;
  float mNewScale;
  nsRefPtr<DOMSVGPoint> mPreviousTranslate;
  nsRefPtr<DOMSVGPoint> mNewTranslate;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGZoomEvent_h
