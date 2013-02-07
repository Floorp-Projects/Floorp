/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/SVGImageElement.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "gfxContext.h"
#include "mozilla/dom/SVGImageElementBinding.h"
#include "nsContentUtils.h"

DOMCI_NODE_DATA(SVGImageElement, mozilla::dom::SVGImageElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Image)

namespace mozilla {
namespace dom {

JSObject*
SVGImageElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGImageElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGImageElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

nsSVGElement::StringInfo SVGImageElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGImageElement,SVGImageElementBase)
NS_IMPL_RELEASE_INHERITED(SVGImageElement,SVGImageElementBase)

NS_INTERFACE_TABLE_HEAD(SVGImageElement)
  NS_NODE_INTERFACE_TABLE8(SVGImageElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGImageElement,
                           nsIDOMSVGURIReference, imgINotificationObserver,
                           nsIImageLoadingContent, imgIOnloadBlocker)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGImageElement)
NS_INTERFACE_MAP_END_INHERITING(SVGImageElementBase)

//----------------------------------------------------------------------
// Implementation

SVGImageElement::SVGImageElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGImageElementBase(aNodeInfo)
{
  SetIsDOMBinding();
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

SVGImageElement::~SVGImageElement()
{
  DestroyImageLoadingContent();
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGImageElement)


//----------------------------------------------------------------------
// nsIDOMSVGImageElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP SVGImageElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = X().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP SVGImageElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = Y().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP SVGImageElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = Width().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP SVGImageElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = Height().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
SVGImageElement::GetPreserveAspectRatio(nsISupports
                                        **aPreserveAspectRatio)
{
  *aPreserveAspectRatio = PreserveAspectRatio().get();
  return NS_OK;
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGImageElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  return ratio.forget();
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
SVGImageElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = Href().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedString>
SVGImageElement::Href()
{
  return mStringAttributes[HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------

nsresult
SVGImageElement::LoadSVGImage(bool aForce, bool aNotify)
{
  // resolve href attribute
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString href;
  mStringAttributes[HREF].GetAnimValue(href, this);
  href.Trim(" \t\n\r");

  if (baseURI && !href.IsEmpty())
    NS_MakeAbsoluteURI(href, href, baseURI);

  return LoadImage(href, aForce, aNotify);
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
SVGImageElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                              const nsAttrValue* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_XLink && aName == nsGkAtoms::href) {

    // If there isn't a frame we still need to load the image in case
    // the frame is created later e.g. by attaching to a document.
    // If there is a frame then it should deal with loading as the image
    // url may be animated
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
  return SVGImageElementBase::AfterSetAttr(aNamespaceID, aName,
                                           aValue, aNotify);
}

void
SVGImageElement::MaybeLoadSVGImage()
{
  if (mStringAttributes[HREF].IsExplicitlySet() &&
      (NS_FAILED(LoadSVGImage(false, true)) ||
       !LoadingEnabled())) {
    CancelImageRequests(true);
  }
}

nsresult
SVGImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = SVGImageElementBase::BindToTree(aDocument, aParent,
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
      NS_NewRunnableMethod(this, &SVGImageElement::MaybeLoadSVGImage));
  }

  return rv;
}

void
SVGImageElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  SVGImageElementBase::UnbindFromTree(aDeep, aNullParent);
}

nsEventStates
SVGImageElement::IntrinsicState() const
{
  return SVGImageElementBase::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

NS_IMETHODIMP_(bool)
SVGImageElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sViewportsMap,
  };

  return FindAttributeDependence(name, map) ||
    SVGImageElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

/* For the purposes of the update/invalidation logic pretend to
   be a rectangle. */
void
SVGImageElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, width, height;

  GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  if (width <= 0 || height <= 0)
    return;

  aCtx->Rectangle(gfxRect(x, y, width, height));
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGImageElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() &&
         mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGImageElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGAnimatedPreserveAspectRatio *
SVGImageElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringAttributesInfo
SVGImageElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsresult
SVGImageElement::CopyInnerTo(Element* aDest)
{
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticImageClone(static_cast<SVGImageElement*>(aDest));
  }
  return SVGImageElementBase::CopyInnerTo(aDest);
}

} // namespace dom
} // namespace mozilla

