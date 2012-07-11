/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMSVGZoomEvent.h"
#include "nsSVGRect.h"
#include "DOMSVGPoint.h"
#include "nsSVGSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsDOMSVGZoomEvent::nsDOMSVGZoomEvent(nsPresContext* aPresContext,
                                     nsGUIEvent* aEvent)
  : nsDOMUIEvent(aPresContext,
                 aEvent ? aEvent : new nsGUIEvent(false, NS_SVG_ZOOM, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->eventStructType = NS_SVGZOOM_EVENT;
    mEvent->time = PR_Now();
  }

  mEvent->flags |= NS_EVENT_FLAG_CANT_CANCEL;

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
        // If the root element isn't an SVG 'svg' element this QI will fail
        // (e.g. if this event was created by calling createEvent on a
        // non-SVGDocument). In these circumstances the "New" and "Previous"
        // properties will be left null which is probably what we want.
        nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(rootElement);
        if (svgElement) {
          nsSVGSVGElement *SVGSVGElement =
            static_cast<nsSVGSVGElement*>(rootElement);
  
          mNewScale = SVGSVGElement->GetCurrentScale();
          mPreviousScale = SVGSVGElement->GetPreviousScale();

          const nsSVGTranslatePoint& translate =
            SVGSVGElement->GetCurrentTranslate();
          mNewTranslate =
            new DOMSVGPoint(translate.GetX(), translate.GetY());
          mNewTranslate->SetReadonly(true);

          const nsSVGTranslatePoint& prevTranslate =
            SVGSVGElement->GetPreviousTranslate();
          mPreviousTranslate =
            new DOMSVGPoint(prevTranslate.GetX(), prevTranslate.GetY());
          mPreviousTranslate->SetReadonly(true);
        }
      }
    }
  }
}


//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF_INHERITED(nsDOMSVGZoomEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMSVGZoomEvent, nsDOMUIEvent)

DOMCI_DATA(SVGZoomEvent, nsDOMSVGZoomEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMSVGZoomEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGZoomEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGZoomEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)


//----------------------------------------------------------------------
// nsIDOMSVGZoomEvent methods:

/* readonly attribute SVGRect zoomRectScreen; */
NS_IMETHODIMP nsDOMSVGZoomEvent::GetZoomRectScreen(nsIDOMSVGRect **aZoomRectScreen)
{
  // The spec says about this attribute:
  //
  //   The specified zoom rectangle in screen units.
  //   The object itself and its contents are both readonly.
  //
  // This is so badly underspecified we don't implement it. It was probably
  // thrown in without much thought as a way of finding the zoom box ASV style
  // zooming uses. I don't see how this is useful though since SVGZoom event's
  // get dispatched *after* the zoom level has changed.
  //
  // Be sure to use NS_NewSVGReadonlyRect and not NS_NewSVGRect if we
  // eventually do implement this!

  *aZoomRectScreen = nsnull;
  NS_NOTYETIMPLEMENTED("nsDOMSVGZoomEvent::GetZoomRectScreen");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute float previousScale; */
NS_IMETHODIMP
nsDOMSVGZoomEvent::GetPreviousScale(float *aPreviousScale)
{
  *aPreviousScale = mPreviousScale;
  return NS_OK;
}

/* readonly attribute SVGPoint previousTranslate; */
NS_IMETHODIMP
nsDOMSVGZoomEvent::GetPreviousTranslate(nsIDOMSVGPoint **aPreviousTranslate)
{
  *aPreviousTranslate = mPreviousTranslate;
  NS_IF_ADDREF(*aPreviousTranslate);
  return NS_OK;
}

/* readonly attribute float newScale; */
NS_IMETHODIMP nsDOMSVGZoomEvent::GetNewScale(float *aNewScale)
{
  *aNewScale = mNewScale;
  return NS_OK;
}

/* readonly attribute SVGPoint newTranslate; */
NS_IMETHODIMP
nsDOMSVGZoomEvent::GetNewTranslate(nsIDOMSVGPoint **aNewTranslate)
{
  *aNewTranslate = mNewTranslate;
  NS_IF_ADDREF(*aNewTranslate);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewDOMSVGZoomEvent(nsIDOMEvent** aInstancePtrResult,
                      nsPresContext* aPresContext,
                      nsGUIEvent *aEvent)
{
  nsDOMSVGZoomEvent* it = new nsDOMSVGZoomEvent(aPresContext, aEvent);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(it, aInstancePtrResult);
}
