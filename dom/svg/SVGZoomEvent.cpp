/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGPoint.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGZoomEvent.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// Implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(SVGZoomEvent, UIEvent, mPreviousTranslate, mNewTranslate)

NS_IMPL_ADDREF_INHERITED(SVGZoomEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(SVGZoomEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGZoomEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

SVGZoomEvent::SVGZoomEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           InternalSVGZoomEvent* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent : new InternalSVGZoomEvent(false, eSVGZoom))
  , mPreviousScale(0)
  , mNewScale(0)
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }

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
        if (rootElement->IsSVGElement(nsGkAtoms::svg)) {
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

SVGZoomEvent::~SVGZoomEvent()
{
}

} // namespace dom
} // namespace mozilla


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<SVGZoomEvent>
NS_NewDOMSVGZoomEvent(EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      mozilla::InternalSVGZoomEvent* aEvent)
{
  nsRefPtr<SVGZoomEvent> it = new SVGZoomEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
