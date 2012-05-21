/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGDocumentWrapper.h"

#include "mozilla/dom/Element.h"
#include "nsIAtom.h"
#include "nsICategoryManager.h"
#include "nsIChannel.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIHttpChannel.h"
#include "nsIObserverService.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsIXMLContentSink.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsSize.h"
#include "gfxRect.h"
#include "nsSVGSVGElement.h"
#include "nsSVGLength2.h"
#include "nsSVGEffects.h"

using namespace mozilla::dom;

namespace mozilla {
namespace image {

NS_IMPL_ISUPPORTS4(SVGDocumentWrapper,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIObserver,
                   nsISupportsWeakReference)

SVGDocumentWrapper::SVGDocumentWrapper()
  : mIgnoreInvalidation(false),
    mRegisteredForXPCOMShutdown(false)
{
}

SVGDocumentWrapper::~SVGDocumentWrapper()
{
  DestroyViewer();
  if (mRegisteredForXPCOMShutdown) {
    UnregisterForXPCOMShutdown();
  }
}

void
SVGDocumentWrapper::DestroyViewer()
{
  if (mViewer) {
    mViewer->GetDocument()->OnPageHide(false, nsnull);
    mViewer->Close(nsnull);
    mViewer->Destroy();
    mViewer = nsnull;
  }
}

bool
SVGDocumentWrapper::GetWidthOrHeight(Dimension aDimension,
                                     PRInt32& aResult)
{
  nsSVGSVGElement* rootElem = GetRootSVGElem();
  NS_ABORT_IF_FALSE(rootElem, "root elem missing or of wrong type");
  nsresult rv;

  // Get the width or height SVG object
  nsRefPtr<nsIDOMSVGAnimatedLength> domAnimLength;
  if (aDimension == eWidth) {
    rv = rootElem->GetWidth(getter_AddRefs(domAnimLength));
  } else {
    NS_ABORT_IF_FALSE(aDimension == eHeight, "invalid dimension");
    rv = rootElem->GetHeight(getter_AddRefs(domAnimLength));
  }
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(domAnimLength, false);

  // Get the animated value from the object
  nsRefPtr<nsIDOMSVGLength> domLength;
  rv = domAnimLength->GetAnimVal(getter_AddRefs(domLength));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(domLength, false);

  // Check if it's a percent value (and fail if so)
  PRUint16 unitType;
  rv = domLength->GetUnitType(&unitType);
  NS_ENSURE_SUCCESS(rv, false);
  if (unitType == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    return false;
  }

  // Non-percent value - woot! Grab it & return it.
  float floatLength;
  rv = domLength->GetValue(&floatLength);
  NS_ENSURE_SUCCESS(rv, false);

  aResult = nsSVGUtils::ClampToInt(floatLength);

  return true;
}

nsIFrame*
SVGDocumentWrapper::GetRootLayoutFrame()
{
  Element* rootElem = GetRootSVGElem();
  return rootElem ? rootElem->GetPrimaryFrame() : nsnull;
}

void
SVGDocumentWrapper::UpdateViewportBounds(const nsIntSize& aViewportSize)
{
  NS_ABORT_IF_FALSE(!mIgnoreInvalidation, "shouldn't be reentrant");
  mIgnoreInvalidation = true;
  mViewer->SetBounds(nsIntRect(nsIntPoint(0, 0), aViewportSize));
  FlushLayout();
  mIgnoreInvalidation = false;
}

void
SVGDocumentWrapper::FlushImageTransformInvalidation()
{
  NS_ABORT_IF_FALSE(!mIgnoreInvalidation, "shouldn't be reentrant");

  nsSVGSVGElement* svgElem = GetRootSVGElem();
  if (!svgElem)
    return;

  mIgnoreInvalidation = true;
  svgElem->FlushImageTransformInvalidation();
  FlushLayout();
  mIgnoreInvalidation = false;
}

bool
SVGDocumentWrapper::IsAnimated()
{
  nsIDocument* doc = mViewer->GetDocument();
  return doc && doc->HasAnimationController() &&
    doc->GetAnimationController()->HasRegisteredAnimations();
}

void
SVGDocumentWrapper::StartAnimation()
{
  // Can be called for animated images during shutdown, after we've
  // already Observe()'d XPCOM shutdown and cleared out our mViewer pointer.
  if (!mViewer)
    return;

  nsIDocument* doc = mViewer->GetDocument();
  if (doc) {
    nsSMILAnimationController* controller = doc->GetAnimationController();
    if (controller) {
      controller->Resume(nsSMILTimeContainer::PAUSE_IMAGE);
    }
    doc->SetImagesNeedAnimating(true);
  }
}

void
SVGDocumentWrapper::StopAnimation()
{
  // Can be called for animated images during shutdown, after we've
  // already Observe()'d XPCOM shutdown and cleared out our mViewer pointer.
  if (!mViewer)
    return;

  nsIDocument* doc = mViewer->GetDocument();
  if (doc) {
    nsSMILAnimationController* controller = doc->GetAnimationController();
    if (controller) {
      controller->Pause(nsSMILTimeContainer::PAUSE_IMAGE);
    }
    doc->SetImagesNeedAnimating(false);
  }
}

void
SVGDocumentWrapper::ResetAnimation()
{
  nsSVGSVGElement* svgElem = GetRootSVGElem();
  if (!svgElem)
    return;

#ifdef DEBUG
  nsresult rv =
#endif
    svgElem->SetCurrentTime(0.0f);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "SetCurrentTime failed");
}


/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt,
                         in nsIInputStream inStr, in unsigned long sourceOffset,
                         in unsigned long count); */
NS_IMETHODIMP
SVGDocumentWrapper::OnDataAvailable(nsIRequest* aRequest, nsISupports* ctxt,
                                    nsIInputStream* inStr,
                                    PRUint32 sourceOffset,
                                    PRUint32 count)
{
  return mListener->OnDataAvailable(aRequest, ctxt, inStr,
                                    sourceOffset, count);
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
SVGDocumentWrapper::OnStartRequest(nsIRequest* aRequest, nsISupports* ctxt)
{
  nsresult rv = SetupViewer(aRequest,
                            getter_AddRefs(mViewer),
                            getter_AddRefs(mLoadGroup));

  if (NS_SUCCEEDED(rv) &&
      NS_SUCCEEDED(mListener->OnStartRequest(aRequest, nsnull))) {
    mViewer->GetDocument()->SetIsBeingUsedAsImage();
    StopAnimation(); // otherwise animations start automatically in helper doc

    rv = mViewer->Init(nsnull, nsIntRect(0, 0, 0, 0));
    if (NS_SUCCEEDED(rv)) {
      rv = mViewer->Open(nsnull, nsnull);
    }
  }
  return rv;
}


/* void onStopRequest (in nsIRequest request, in nsISupports ctxt,
                       in nsresult status); */
NS_IMETHODIMP
SVGDocumentWrapper::OnStopRequest(nsIRequest* aRequest, nsISupports* ctxt,
                                  nsresult status)
{
  if (mListener) {
    mListener->OnStopRequest(aRequest, ctxt, status);
    // A few levels up the stack, imgRequest::OnStopRequest is about to tell
    // all of its observers that we know our size and are ready to paint.  That
    // might not be true at this point, though -- so here, we synchronously
    // finish parsing & layout in our helper-document to make sure we can hold
    // up to this promise.
    nsCOMPtr<nsIParser> parser = do_QueryInterface(mListener);
    while (!parser->IsComplete()) {
      parser->CancelParsingEvents();
      parser->ContinueInterruptedParsing();
    }
    FlushLayout();
    mListener = nsnull;

    // In a normal document, this would be called by nsDocShell - but we don't
    // have a nsDocShell. So we do it ourselves. (If we don't, painting will
    // stay suppressed for a little while longer, for no good reason).
    mViewer->LoadComplete(NS_OK);
  }

  return NS_OK;
}

/** nsIObserver Methods **/
NS_IMETHODIMP
SVGDocumentWrapper::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar *aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Sever ties from rendering observers to helper-doc's root SVG node
    nsSVGSVGElement* svgElem = GetRootSVGElem();
    if (svgElem) {
      nsSVGEffects::RemoveAllRenderingObservers(svgElem);
    }

    // Clean up at XPCOM shutdown time.
    DestroyViewer();
    if (mListener)
      mListener = nsnull;
    if (mLoadGroup)
      mLoadGroup = nsnull;

    // Turn off "registered" flag, or else we'll try to unregister when we die.
    // (No need for that now, and the try would fail anyway -- it's too late.)
    mRegisteredForXPCOMShutdown = false;
  } else {
    NS_ERROR("Unexpected observer topic.");
  }
  return NS_OK;
}

/** Private helper methods **/

// This method is largely cribbed from
// nsExternalResourceMap::PendingLoad::SetupViewer.
nsresult
SVGDocumentWrapper::SetupViewer(nsIRequest* aRequest,
                                nsIContentViewer** aViewer,
                                nsILoadGroup** aLoadGroup)
{
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  // Check for HTTP error page
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
  if (httpChannel) {
    bool requestSucceeded;
    if (NS_FAILED(httpChannel->GetRequestSucceeded(&requestSucceeded)) ||
        !requestSucceeded) {
      return NS_ERROR_FAILURE;
    }
  }

  // Give this document its own loadgroup
  nsCOMPtr<nsILoadGroup> loadGroup;
  chan->GetLoadGroup(getter_AddRefs(loadGroup));

  nsCOMPtr<nsILoadGroup> newLoadGroup =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  NS_ENSURE_TRUE(newLoadGroup, NS_ERROR_OUT_OF_MEMORY);
  newLoadGroup->SetLoadGroup(loadGroup);

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMan, NS_ERROR_NOT_AVAILABLE);
  nsXPIDLCString contractId;
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", SVG_MIMETYPE,
                                         getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    do_GetService(contractId);
  NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsIStreamListener> listener;
  rv = docLoaderFactory->CreateInstance("external-resource", chan,
                                        newLoadGroup,
                                        SVG_MIMETYPE, nsnull, nsnull,
                                        getter_AddRefs(listener),
                                        getter_AddRefs(viewer));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(viewer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIParser> parser = do_QueryInterface(listener);
  NS_ENSURE_TRUE(parser, NS_ERROR_UNEXPECTED);

  // XML-only, because this is for SVG content
  nsIContentSink* sink = parser->GetContentSink();
  nsCOMPtr<nsIXMLContentSink> xmlSink = do_QueryInterface(sink);
  NS_ENSURE_TRUE(sink, NS_ERROR_UNEXPECTED);

  listener.swap(mListener);
  viewer.forget(aViewer);
  newLoadGroup.forget(aLoadGroup);

  RegisterForXPCOMShutdown();
  return NS_OK;
}

void
SVGDocumentWrapper::RegisterForXPCOMShutdown()
{
  NS_ABORT_IF_FALSE(!mRegisteredForXPCOMShutdown,
                    "re-registering for XPCOM shutdown");
  // Listen for xpcom-shutdown so that we can drop references to our
  // helper-document at that point. (Otherwise, we won't get cleaned up
  // until imgLoader::Shutdown, which can happen after the JAR service
  // and RDF service have been unregistered.)
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc = do_GetService(OBSERVER_SVC_CID, &rv);
  if (NS_FAILED(rv) ||
      NS_FAILED(obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                    true))) {
    NS_WARNING("Failed to register as observer of XPCOM shutdown");
  } else {
    mRegisteredForXPCOMShutdown = true;
  }
}

void
SVGDocumentWrapper::UnregisterForXPCOMShutdown()
{
  NS_ABORT_IF_FALSE(mRegisteredForXPCOMShutdown,
                    "unregistering for XPCOM shutdown w/out being registered");

  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc = do_GetService(OBSERVER_SVC_CID, &rv);
  if (NS_FAILED(rv) ||
      NS_FAILED(obsSvc->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    NS_WARNING("Failed to unregister as observer of XPCOM shutdown");
  } else {
    mRegisteredForXPCOMShutdown = false;
  }
}

void
SVGDocumentWrapper::FlushLayout()
{
  nsCOMPtr<nsIPresShell> presShell;
  mViewer->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    presShell->FlushPendingNotifications(Flush_Layout);
  }
}

nsSVGSVGElement*
SVGDocumentWrapper::GetRootSVGElem()
{
  if (!mViewer)
    return nsnull; // Can happen during destruction

  nsIDocument* doc = mViewer->GetDocument();
  if (!doc)
    return nsnull; // Can happen during destruction

  Element* rootElem = mViewer->GetDocument()->GetRootElement();
  if (!rootElem || !rootElem->IsSVG(nsGkAtoms::svg)) {
    return nsnull;
  }

  return static_cast<nsSVGSVGElement*>(rootElem);
}

} // namespace image
} // namespace mozilla
