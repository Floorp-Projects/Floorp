/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEImageElement.h"

#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/SVGFEImageElementBinding.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsNetUtil.h"
#include "SVGObserverUtils.h"
#include "imgIContainer.h"
#include "gfx2DGlue.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEImage)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEImageElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEImageElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGFEImageElement::sStringInfo[3] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_XLink, true}};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(SVGFEImageElement, SVGFEImageElementBase,
                            imgINotificationObserver, nsIImageLoadingContent)

//----------------------------------------------------------------------
// Implementation

SVGFEImageElement::SVGFEImageElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGFEImageElementBase(std::move(aNodeInfo)), mImageAnimationMode(0) {
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

SVGFEImageElement::~SVGFEImageElement() { DestroyImageLoadingContent(); }

//----------------------------------------------------------------------

nsresult SVGFEImageElement::LoadSVGImage(bool aForce, bool aNotify) {
  // resolve href attribute
  nsIURI* baseURI = GetBaseURI();

  nsAutoString href;
  if (mStringAttributes[HREF].IsExplicitlySet()) {
    mStringAttributes[HREF].GetAnimValue(href, this);
  } else {
    mStringAttributes[XLINK_HREF].GetAnimValue(href, this);
  }
  href.Trim(" \t\n\r");

  if (baseURI && !href.IsEmpty()) NS_MakeAbsoluteURI(href, href, baseURI);

  // Make sure we don't get in a recursive death-spiral
  Document* doc = OwnerDoc();
  nsCOMPtr<nsIURI> hrefAsURI;
  if (NS_SUCCEEDED(StringToURI(href, doc, getter_AddRefs(hrefAsURI)))) {
    bool isEqual;
    if (NS_SUCCEEDED(hrefAsURI->Equals(baseURI, &isEqual)) && isEqual) {
      // Image URI matches our URI exactly! Bail out.
      return NS_OK;
    }
  }

  // Mark channel as urgent-start before load image if the image load is
  // initaiated by a user interaction.
  mUseUrgentStartForChannel = EventStateManager::IsHandlingUserInput();
  return LoadImage(href, aForce, aNotify, eImageLoadType_Normal);
}

//----------------------------------------------------------------------
// EventTarget methods:

void SVGFEImageElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

//----------------------------------------------------------------------
// nsIContent methods:

NS_IMETHODIMP_(bool)
SVGFEImageElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sGraphicsMap};

  return FindAttributeDependence(name, map) ||
         SVGFEImageElementBase::IsAttributeMapped(name);
}

nsresult SVGFEImageElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  if (aName == nsGkAtoms::href && (aNamespaceID == kNameSpaceID_XLink ||
                                   aNamespaceID == kNameSpaceID_None)) {
    if (aValue) {
      LoadSVGImage(true, aNotify);
    } else {
      CancelImageRequests(aNotify);
    }
  }

  return SVGFEImageElementBase::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void SVGFEImageElement::MaybeLoadSVGImage() {
  if ((mStringAttributes[HREF].IsExplicitlySet() ||
       mStringAttributes[XLINK_HREF].IsExplicitlySet()) &&
      (NS_FAILED(LoadSVGImage(false, true)) || !LoadingEnabled())) {
    CancelImageRequests(true);
  }
}

nsresult SVGFEImageElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = SVGFEImageElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aContext, aParent);

  if (mStringAttributes[HREF].IsExplicitlySet() ||
      mStringAttributes[XLINK_HREF].IsExplicitlySet()) {
    nsContentUtils::AddScriptRunner(
        NewRunnableMethod("dom::SVGFEImageElement::MaybeLoadSVGImage", this,
                          &SVGFEImageElement::MaybeLoadSVGImage));
  }

  return rv;
}

void SVGFEImageElement::UnbindFromTree(bool aNullParent) {
  nsImageLoadingContent::UnbindFromTree(aNullParent);
  SVGFEImageElementBase::UnbindFromTree(aNullParent);
}

EventStates SVGFEImageElement::IntrinsicState() const {
  return SVGFEImageElementBase::IntrinsicState() |
         nsImageLoadingContent::ImageState();
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEImageElement)

already_AddRefed<DOMSVGAnimatedString> SVGFEImageElement::Href() {
  return mStringAttributes[HREF].IsExplicitlySet()
             ? mStringAttributes[HREF].ToDOMAnimatedString(this)
             : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIDOMSVGFEImageElement methods

FilterPrimitiveDescription SVGFEImageElement::GetPrimitiveDescription(
    nsSVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return FilterPrimitiveDescription();
  }

  nsCOMPtr<imgIRequest> currentRequest;
  GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
             getter_AddRefs(currentRequest));

  nsCOMPtr<imgIContainer> imageContainer;
  if (currentRequest) {
    currentRequest->GetImage(getter_AddRefs(imageContainer));
  }

  RefPtr<SourceSurface> image;
  if (imageContainer) {
    uint32_t flags =
        imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY;
    image = imageContainer->GetFrame(imgIContainer::FRAME_CURRENT, flags);
  }

  if (!image) {
    return FilterPrimitiveDescription();
  }

  IntSize nativeSize;
  imageContainer->GetWidth(&nativeSize.width);
  imageContainer->GetHeight(&nativeSize.height);

  Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
      aFilterSubregion.width, aFilterSubregion.height, 0, 0, nativeSize.width,
      nativeSize.height, mPreserveAspectRatio);
  Matrix TM = viewBoxTM;
  TM.PostTranslate(aFilterSubregion.x, aFilterSubregion.y);

  SamplingFilter samplingFilter =
      nsLayoutUtils::GetSamplingFilterForFrame(frame);

  ImageAttributes atts;
  atts.mFilter = (uint32_t)samplingFilter;
  atts.mTransform = TM;

  // Append the image to aInputImages and store its index in the description.
  size_t imageIndex = aInputImages.Length();
  aInputImages.AppendElement(image);
  atts.mInputIndex = (uint32_t)imageIndex;
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEImageElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsAtom* aAttribute) const {
  // nsGkAtoms::href is deliberately omitted as the frame has special
  // handling to load the image
  return SVGFEImageElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                          aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::preserveAspectRatio);
}

bool SVGFEImageElement::OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                                        nsIPrincipal* aReferencePrincipal) {
  nsresult rv;
  nsCOMPtr<imgIRequest> currentRequest;
  GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
             getter_AddRefs(currentRequest));

  if (!currentRequest) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = currentRequest->GetImagePrincipal(getter_AddRefs(principal));
  if (NS_FAILED(rv) || !principal) {
    return true;
  }

  int32_t corsmode;
  if (NS_SUCCEEDED(currentRequest->GetCORSMode(&corsmode)) &&
      corsmode != imgIRequest::CORS_NONE) {
    // If CORS was used to load the image, the page is allowed to read from it.
    return false;
  }

  if (aReferencePrincipal->Subsumes(principal)) {
    // The page is allowed to read from the image.
    return false;
  }

  return true;
}

//----------------------------------------------------------------------
// SVGElement methods

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGFEImageElement::PreserveAspectRatio() {
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

SVGAnimatedPreserveAspectRatio*
SVGFEImageElement::GetAnimatedPreserveAspectRatio() {
  return &mPreserveAspectRatio;
}

SVGElement::StringAttributesInfo SVGFEImageElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIImageLoadingContent methods
NS_IMETHODIMP_(void)
SVGFEImageElement::FrameCreated(nsIFrame* aFrame) {
  nsImageLoadingContent::FrameCreated(aFrame);

  uint64_t mode = aFrame->PresContext()->ImageAnimationMode();
  if (mode == mImageAnimationMode) {
    return;
  }

  mImageAnimationMode = mode;

  if (mPendingRequest) {
    nsCOMPtr<imgIContainer> container;
    mPendingRequest->GetImage(getter_AddRefs(container));
    if (container) {
      container->SetAnimationMode(mode);
    }
  }

  if (mCurrentRequest) {
    nsCOMPtr<imgIContainer> container;
    mCurrentRequest->GetImage(getter_AddRefs(container));
    if (container) {
      container->SetAnimationMode(mode);
    }
  }
}

//----------------------------------------------------------------------
// imgINotificationObserver methods

NS_IMETHODIMP
SVGFEImageElement::Notify(imgIRequest* aRequest, int32_t aType,
                          const nsIntRect* aData) {
  nsresult rv = nsImageLoadingContent::Notify(aRequest, aType, aData);

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Request a decode
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    MOZ_ASSERT(container, "who sent the notification then?");
    container->StartDecoding(imgIContainer::FLAG_NONE);
    container->SetAnimationMode(mImageAnimationMode);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE ||
      aType == imgINotificationObserver::FRAME_UPDATE ||
      aType == imgINotificationObserver::SIZE_AVAILABLE) {
    if (GetParent() && GetParent()->IsSVGElement(nsGkAtoms::filter)) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(
          static_cast<SVGFilterElement*>(GetParent()));
    }
  }

  return rv;
}

}  // namespace dom
}  // namespace mozilla
