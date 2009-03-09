/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jonathan.watt@strath.ac.uk> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMSVGZoomEvent.h"
#include "nsContentUtils.h"
#include "nsSVGRect.h"
#include "nsSVGPoint.h"
#include "nsSVGSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"

//----------------------------------------------------------------------
// Implementation

nsDOMSVGZoomEvent::nsDOMSVGZoomEvent(nsPresContext* aPresContext,
                                     nsGUIEvent* aEvent)
  : nsDOMUIEvent(aPresContext,
                 aEvent ? aEvent : new nsGUIEvent(PR_FALSE, NS_SVG_ZOOM, 0))
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
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
      nsIContent *rootContent = doc->GetRootContent();
      if (rootContent) {
        // If the root element isn't an SVG 'svg' element this QI will fail
        // (e.g. if this event was created by calling createEvent on a
        // non-SVGDocument). In these circumstances the "New" and "Previous"
        // properties will be left null which is probably what we want.
        nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(rootContent);
        if (svgElement) {
          svgElement->GetCurrentScale(&mNewScale);
          float x, y;
          nsCOMPtr<nsIDOMSVGPoint> currentTranslate;
          svgElement->GetCurrentTranslate(getter_AddRefs(currentTranslate));
          currentTranslate->GetX(&x);
          currentTranslate->GetY(&y);
          NS_NewSVGReadonlyPoint(getter_AddRefs(mNewTranslate), x, y);

          nsSVGSVGElement *SVGSVGElement =
            static_cast<nsSVGSVGElement*>(rootContent);
          mPreviousScale = SVGSVGElement->GetPreviousScale();
          NS_NewSVGReadonlyPoint(getter_AddRefs(mPreviousTranslate),
                                 SVGSVGElement->GetPreviousTranslate_x(),
                                 SVGSVGElement->GetPreviousTranslate_y());
          // Important: we call RecordCurrentST() here to make sure that
          // scripts that create an SVGZoomEvent won't get our "Previous" data
          SVGSVGElement->RecordCurrentScaleTranslate();
        }
      }
    }
  }
}


//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF_INHERITED(nsDOMSVGZoomEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMSVGZoomEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMSVGZoomEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGZoomEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMUIEvent,nsIDOMSVGZoomEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGZoomEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)


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
