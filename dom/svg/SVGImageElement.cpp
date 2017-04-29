/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"

#include "mozilla/dom/SVGImageElement.h"
#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "imgINotificationObserver.h"
#include "mozilla/dom/SVGImageElementBinding.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Image)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGImageElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGImageElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGImageElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

nsSVGElement::StringInfo SVGImageElement::sStringInfo[2] =
{
  { &nsGkAtoms::href, kNameSpaceID_None, true },
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(SVGImageElement, SVGImageElementBase,
                            nsIDOMNode, nsIDOMElement,
                            nsIDOMSVGElement,
                            imgINotificationObserver,
                            nsIImageLoadingContent, imgIOnloadBlocker)

//----------------------------------------------------------------------
// Implementation

SVGImageElement::SVGImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGImageElementBase(aNodeInfo)
{
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

already_AddRefed<SVGAnimatedLength>
SVGImageElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGImageElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGImageElement::PreserveAspectRatio()
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

already_AddRefed<SVGAnimatedString>
SVGImageElement::Href()
{
  return mStringAttributes[HREF].IsExplicitlySet()
         ? mStringAttributes[HREF].ToDOMAnimatedString(this)
         : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------

nsresult
SVGImageElement::LoadSVGImage(bool aForce, bool aNotify)
{
  // resolve href attribute
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString href;
  if (mStringAttributes[HREF].IsExplicitlySet()) {
    mStringAttributes[HREF].GetAnimValue(href, this);
  } else {
    mStringAttributes[XLINK_HREF].GetAnimValue(href, this);
  }
  href.Trim(" \t\n\r");

  if (baseURI && !href.IsEmpty())
    NS_MakeAbsoluteURI(href, href, baseURI);

  return LoadImage(href, aForce, aNotify, eImageLoadType_Normal);
}

//----------------------------------------------------------------------
// EventTarget methods:

void
SVGImageElement::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
SVGImageElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                              const nsAttrValue* aValue, bool aNotify)
{
  if (aName == nsGkAtoms::href &&
      (aNamespaceID == kNameSpaceID_None ||
       aNamespaceID == kNameSpaceID_XLink)) {

    // If there isn't a frame we still need to load the image in case
    // the frame is created later e.g. by attaching to a document.
    // If there is a frame then it should deal with loading as the image
    // url may be animated
    if (!GetPrimaryFrame()) {
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
  if ((mStringAttributes[HREF].IsExplicitlySet() ||
       mStringAttributes[XLINK_HREF].IsExplicitlySet()) &&
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

  if (mStringAttributes[HREF].IsExplicitlySet() ||
      mStringAttributes[XLINK_HREF].IsExplicitlySet()) {
    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod(this, &SVGImageElement::MaybeLoadSVGImage));
  }

  return rv;
}

void
SVGImageElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  SVGImageElementBase::UnbindFromTree(aDeep, aNullParent);
}

EventStates
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
// SVGGeometryElement methods

/* For the purposes of the update/invalidation logic pretend to
   be a rectangle. */
bool
SVGImageElement::GetGeometryBounds(Rect* aBounds,
                                   const StrokeOptions& aStrokeOptions,
                                   const Matrix& aToBoundsSpace,
                                   const Matrix* aToNonScalingStrokeSpace)
{
  Rect rect;
  GetAnimatedLengthValues(&rect.x, &rect.y, &rect.width,
                          &rect.height, nullptr);

  if (rect.IsEmpty()) {
    // Rendering of the element disabled
    rect.SetEmpty(); // Make sure width/height are zero and not negative
  }

  *aBounds = aToBoundsSpace.TransformBounds(rect);
  return true;
}

already_AddRefed<Path>
SVGImageElement::BuildPath(PathBuilder* aBuilder)
{
  // We get called in order to get bounds for this element, and for
  // hit-testing against it. For that we just pretend to be a rectangle.

  float x, y, width, height;
  GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  Rect r(x, y, width, height);
  aBuilder->MoveTo(r.TopLeft());
  aBuilder->LineTo(r.TopRight());
  aBuilder->LineTo(r.BottomRight());
  aBuilder->LineTo(r.BottomLeft());
  aBuilder->Close();

  return aBuilder->Finish();
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
SVGImageElement::CopyInnerTo(Element* aDest, bool aPreallocateChildren)
{
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticImageClone(static_cast<SVGImageElement*>(aDest));
  }
  return SVGImageElementBase::CopyInnerTo(aDest, aPreallocateChildren);
}

} // namespace dom
} // namespace mozilla

