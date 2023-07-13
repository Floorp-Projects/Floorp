/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMPathElement.h"

#include "nsDebug.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "nsContentUtils.h"
#include "nsIReferrerInfo.h"
#include "mozilla/dom/SVGMPathElementBinding.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(MPath)

namespace mozilla::dom {

JSObject* SVGMPathElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return SVGMPathElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGMPathElement::sStringInfo[2] = {
    {nsGkAtoms::href, kNameSpaceID_None, false},
    {nsGkAtoms::href, kNameSpaceID_XLink, false}};

// Cycle collection magic -- based on SVGUseElement
NS_IMPL_CYCLE_COLLECTION_CLASS(SVGMPathElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGMPathElement,
                                                SVGMPathElementBase)
  tmp->mMPathObserver = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGMPathElement,
                                                  SVGMPathElementBase)
  SVGObserverUtils::TraverseMPathObserver(tmp, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(SVGMPathElement,
                                               SVGMPathElementBase)

// Constructor
SVGMPathElement::SVGMPathElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGMPathElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMPathElement)

already_AddRefed<DOMSVGAnimatedString> SVGMPathElement::Href() {
  return mStringAttributes[HREF].IsExplicitlySet()
             ? mStringAttributes[HREF].ToDOMAnimatedString(this)
             : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

void SVGMPathElement::UnbindFromTree(bool aNullParent) {
  mMPathObserver = nullptr;
  NotifyParentOfMpathChange();
  SVGMPathElementBase::UnbindFromTree(aNullParent);
}

void SVGMPathElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                   const nsAttrValue* aValue,
                                   const nsAttrValue* aOldValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   bool aNotify) {
  if (aName == nsGkAtoms::href &&
      (aNamespaceID == kNameSpaceID_None ||
       (aNamespaceID == kNameSpaceID_XLink && !HasAttr(nsGkAtoms::href)))) {
    mMPathObserver = nullptr;
    NotifyParentOfMpathChange();
  }

  return SVGMPathElementBase::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo SVGMPathElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// Public helper methods

void SVGMPathElement::HrefAsString(nsAString& aHref) {
  if (mStringAttributes[SVGMPathElement::HREF].IsExplicitlySet()) {
    mStringAttributes[SVGMPathElement::HREF].GetBaseValue(aHref, this);
  } else {
    mStringAttributes[SVGMPathElement::XLINK_HREF].GetBaseValue(aHref, this);
  }
}

SVGGeometryElement* SVGMPathElement::GetReferencedPath() {
  return SVGObserverUtils::GetAndObserveMPathsPath(this);
}

void SVGMPathElement::NotifyParentOfMpathChange() {
  if (auto* animateMotionParent =
          SVGAnimateMotionElement::FromNodeOrNull(GetParent())) {
    animateMotionParent->MpathChanged();
    AnimationNeedsResample();
  }
}

}  // namespace mozilla::dom
