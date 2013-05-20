/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGZoomEvent.h"
#include "DOMSVGPoint.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// Implementation

SVGZoomEvent::SVGZoomEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           nsGUIEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext,
                 aEvent ? aEvent : new nsGUIEvent(false, NS_SVG_ZOOM, 0))
  , mPreviousScale(0)
  , mNewScale(0)
{
  SetIsDOMBinding();

  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->eventStructType = NS_SVGZOOM_EVENT;
    mEvent->time = PR_Now();
  }

  mEvent->mFlags.mCancelable = false;

  // We must store the "Previous" and "New" values before this event is
  // dispatched. Reading the values from the root 'svg' element after we've
  // been dispatched is not an option since event handler code may change
  // currentScale and currentTranslate in response to this event.
  nsIPresShell *presShell;
  if (mPresContext && (presShell = mPresContext->GetPresShell())) {
    nsIDocument *doc = presShell->GetDocument();
    if (doc) {
      Element *rootElement = doc->GetRootElement();
      if (rootElement) {
        // If the root element isn't an SVG 'svg' element
        // (e.g. if this event was created by calling createEvent on a
        // non-SVGDocument), then the "New" and "Previous"
        // properties will be left null which is probably what we want.
        if (rootElement->IsSVG(nsGkAtoms::svg)) {
          SVGSVGElement *SVGSVGElem =
            static_cast<SVGSVGElement*>(rootElement);

          mNewScale = SVGSVGElem->GetCurrentScale();
          mPreviousScale = SVGSVGElem->GetPreviousScale();

          const SVGPoint& translate = SVGSVGElem->GetCurrentTranslate();
          mNewTranslate =
            new DOMSVGPoint(translate.GetX(), translate.GetY());
          mNewTranslate->SetReadonly(true);

          const SVGPoint& prevTranslate = SVGSVGElem->GetPreviousTranslate();
          mPreviousTranslate =
            new DOMSVGPoint(prevTranslate.GetX(), prevTranslate.GetY());
          mPreviousTranslate->SetReadonly(true);
        }
      }
    }
  }
}


} // namespace dom
} // namespace mozilla


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewDOMSVGZoomEvent(nsIDOMEvent** aInstancePtrResult,
                      mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      nsGUIEvent *aEvent)
{
  mozilla::dom::SVGZoomEvent* it =
    new mozilla::dom::SVGZoomEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
