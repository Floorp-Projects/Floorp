/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEImageElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEImage)

DOMCI_NODE_DATA(SVGFEImageElement, nsSVGFEImageElement)

namespace mozilla {
namespace dom {

nsSVGElement::StringInfo nsSVGFEImageElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEImageElement,nsSVGFEImageElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEImageElement,nsSVGFEImageElementBase)

NS_INTERFACE_TABLE_HEAD(nsSVGFEImageElement)
  NS_NODE_INTERFACE_TABLE9(nsSVGFEImageElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEImageElement, nsIDOMSVGURIReference,
                           imgINotificationObserver, nsIImageLoadingContent,
                           imgIOnloadBlocker)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEImageElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEImageElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEImageElement::nsSVGFEImageElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGFEImageElementBase(aNodeInfo)
{
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

nsSVGFEImageElement::~nsSVGFEImageElement()
{
  DestroyImageLoadingContent();
}

//----------------------------------------------------------------------

nsresult
nsSVGFEImageElement::LoadSVGImage(bool aForce, bool aNotify)
{
  // resolve href attribute
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString href;
  mStringAttributes[HREF].GetAnimValue(href, this);
  href.Trim(" \t\n\r");

  if (baseURI && !href.IsEmpty())
    NS_MakeAbsoluteURI(href, href, baseURI);

  // Make sure we don't get in a recursive death-spiral
  nsIDocument* doc = OwnerDoc();
  nsCOMPtr<nsIURI> hrefAsURI;
  if (NS_SUCCEEDED(StringToURI(href, doc, getter_AddRefs(hrefAsURI)))) {
    bool isEqual;
    if (NS_SUCCEEDED(hrefAsURI->Equals(baseURI, &isEqual)) && isEqual) {
      // Image URI matches our URI exactly! Bail out.
      return NS_OK;
    }
  }

  return LoadImage(href, aForce, aNotify);
}

//----------------------------------------------------------------------
// nsIContent methods:

NS_IMETHODIMP_(bool)
nsSVGFEImageElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGraphicsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGFEImageElementBase::IsAttributeMapped(name);
}

nsresult
nsSVGFEImageElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                  const nsAttrValue* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_XLink && aName == nsGkAtoms::href) {

    // If there isn't a frame we still need to load the image in case
    // the frame is created later e.g. by attaching to a document.
    // If there is a frame then it should deal with loading as the image
    // url may be animated.
    if (!GetPrimaryFrame()) {

      // Prevent setting image.src by exiting early
      if (nsContentUtils::IsImageSrcSetDisabled()) {
        return NS_OK;
      }
      if (aValue) {
        LoadSVGImage(true, aNotify);
      } else {
        CancelImageRequests(aNotify);
      }
    }
  }

  return nsSVGFEImageElementBase::AfterSetAttr(aNamespaceID, aName,
                                               aValue, aNotify);
}

void
nsSVGFEImageElement::MaybeLoadSVGImage()
{
  if (mStringAttributes[HREF].IsExplicitlySet() &&
      (NS_FAILED(LoadSVGImage(false, true)) ||
       !LoadingEnabled())) {
    CancelImageRequests(true);
  }
}

nsresult
nsSVGFEImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsSVGFEImageElementBase::BindToTree(aDocument, aParent,
                                                    aBindingParent,
                                                    aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);

  if (mStringAttributes[HREF].IsExplicitlySet()) {
    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
    nsContentUtils::AddScriptRunner(
      NS_NewRunnableMethod(this, &nsSVGFEImageElement::MaybeLoadSVGImage));
  }

  return rv;
}

void
nsSVGFEImageElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsSVGFEImageElementBase::UnbindFromTree(aDeep, aNullParent);
}

nsEventStates
nsSVGFEImageElement::IntrinsicState() const
{
  return nsSVGFEImageElementBase::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEImageElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGFEImageElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGFEImageElement methods

nsresult
nsSVGFEImageElement::Filter(nsSVGFilterInstance *instance,
                            const nsTArray<const Image*>& aSources,
                            const Image* aTarget,
                            const nsIntRect& rect)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;

  nsCOMPtr<imgIRequest> currentRequest;
  GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
             getter_AddRefs(currentRequest));

  nsCOMPtr<imgIContainer> imageContainer;
  if (currentRequest)
    currentRequest->GetImage(getter_AddRefs(imageContainer));

  nsRefPtr<gfxASurface> currentFrame;
  if (imageContainer)
    imageContainer->GetFrame(imgIContainer::FRAME_CURRENT,
                             imgIContainer::FLAG_SYNC_DECODE,
                             getter_AddRefs(currentFrame));

  // We need to wrap the surface in a pattern to have somewhere to set the
  // graphics filter.
  nsRefPtr<gfxPattern> thebesPattern;
  if (currentFrame)
    thebesPattern = new gfxPattern(currentFrame);

  if (thebesPattern) {
    thebesPattern->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(frame));

    int32_t nativeWidth, nativeHeight;
    imageContainer->GetWidth(&nativeWidth);
    imageContainer->GetHeight(&nativeHeight);

    const gfxRect& filterSubregion = aTarget->mFilterPrimitiveSubregion;

    gfxMatrix viewBoxTM =
      SVGContentUtils::GetViewBoxTransform(filterSubregion.Width(), filterSubregion.Height(),
                                           0,0, nativeWidth, nativeHeight,
                                           mPreserveAspectRatio);

    gfxMatrix xyTM = gfxMatrix().Translate(gfxPoint(filterSubregion.X(), filterSubregion.Y()));

    gfxMatrix TM = viewBoxTM * xyTM;
    
    nsRefPtr<gfxContext> ctx = new gfxContext(aTarget->mImage);
    nsSVGUtils::CompositePatternMatrix(ctx, thebesPattern, TM, nativeWidth, nativeHeight, 1.0);
  }

  return NS_OK;
}

bool
nsSVGFEImageElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                               nsIAtom* aAttribute) const
{
  // nsGkAtoms::href is deliberately omitted as the frame has special
  // handling to load the image
  return nsSVGFEImageElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::preserveAspectRatio);
}

nsIntRect
nsSVGFEImageElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  // XXX can do better here ... we could check what we know of the source
  // image bounds and compute an accurate bounding box for the filter
  // primitive result.
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

SVGAnimatedPreserveAspectRatio *
nsSVGFEImageElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringAttributesInfo
nsSVGFEImageElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// imgINotificationObserver methods

NS_IMETHODIMP
nsSVGFEImageElement::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData)
{
  nsresult rv = nsImageLoadingContent::Notify(aRequest, aType, aData);

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Request a decode
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    NS_ABORT_IF_FALSE(container, "who sent the notification then?");
    container->StartDecoding();
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE ||
      aType == imgINotificationObserver::FRAME_UPDATE ||
      aType == imgINotificationObserver::SIZE_AVAILABLE) {
    Invalidate();
  }

  return rv;
}

//----------------------------------------------------------------------
// helper methods

void
nsSVGFEImageElement::Invalidate()
{
  if (GetParent() && GetParent()->IsSVG(nsGkAtoms::filter)) {
    static_cast<SVGFilterElement*>(GetParent())->Invalidate();
  }
}

} // namespace dom
} // namespace mozilla
