/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGZOOMEVENT_H__
#define __NS_SVGZOOMEVENT_H__

#include "nsAutoPtr.h"
#include "nsDOMUIEvent.h"
#include "nsIDOMSVGZoomEvent.h"

class nsGUIEvent;
class nsPresContext;

namespace mozilla {
class DOMSVGPoint;
}

class nsDOMSVGZoomEvent : public nsDOMUIEvent,
                          public nsIDOMSVGZoomEvent
{
public:
  typedef mozilla::DOMSVGPoint DOMSVGPoint;

  nsDOMSVGZoomEvent(nsPresContext* aPresContext, nsGUIEvent* aEvent);
                     
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

#endif // __NS_SVGZOOMEVENT_H__
