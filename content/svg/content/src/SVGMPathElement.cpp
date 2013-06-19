/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/SVGMPathElement.h"
#include "nsDebug.h"
#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGPathElement.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGMPathElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(MPath)

namespace mozilla {
namespace dom {

JSObject*
SVGMPathElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGMPathElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::StringInfo SVGMPathElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};

// Cycle collection magic -- based on nsSVGUseElement
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGMPathElement,
                                                SVGMPathElementBase)
  tmp->UnlinkHrefTarget(false);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGMPathElement,
                                                  SVGMPathElementBase)
  tmp->mHrefTarget.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGMPathElement,SVGMPathElementBase)
NS_IMPL_RELEASE_INHERITED(SVGMPathElement,SVGMPathElementBase)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(SVGMPathElement)
  NS_INTERFACE_TABLE_INHERITED4(SVGMPathElement, nsIDOMNode, nsIDOMElement,
                                nsIDOMSVGElement,
                                nsIMutationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGMPathElementBase)

// Constructor
#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mHrefTarget's constructor 
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
SVGMPathElement::SVGMPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGMPathElementBase(aNodeInfo),
    mHrefTarget(this)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
}

SVGMPathElement::~SVGMPathElement()
{
  UnlinkHrefTarget(false);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMPathElement)

already_AddRefed<SVGAnimatedString>
SVGMPathElement::Href()
{
  return mStringAttributes[HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
SVGMPathElement::BindToTree(nsIDocument* aDocument,
                            nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                    "Shouldn't have href-target yet "
                    "(or it should've been cleared)");
  nsresult rv = SVGMPathElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aDocument) {
    const nsAttrValue* hrefAttrValue =
      mAttrsAndChildren.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
    if (hrefAttrValue) {
      UpdateHrefTarget(aParent, hrefAttrValue->GetStringValue());
    }
  }

  return NS_OK;
}

void
SVGMPathElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  UnlinkHrefTarget(true);
  SVGMPathElementBase::UnbindFromTree(aDeep, aNullParent);
}

bool
SVGMPathElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  bool returnVal =
    SVGMPathElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                          aValue, aResult);
  if (aNamespaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href &&
      IsInDoc()) {
    // NOTE: If we fail the IsInDoc call, it's ok -- we'll update the target
    // on next BindToTree call.
    UpdateHrefTarget(GetParent(), aValue);
  }
  return returnVal;
}

nsresult
SVGMPathElement::UnsetAttr(int32_t aNamespaceID,
                           nsIAtom* aAttribute, bool aNotify)
{
  nsresult rv = SVGMPathElementBase::UnsetAttr(aNamespaceID, aAttribute,
                                               aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    UnlinkHrefTarget(true);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGMPathElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
SVGMPathElement::AttributeChanged(nsIDocument* aDocument,
                                  Element* aElement,
                                  int32_t aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::d) {
      NotifyParentOfMpathChange(GetParent());
    }
  }
}

//----------------------------------------------------------------------
// Public helper methods

SVGPathElement*
SVGMPathElement::GetReferencedPath()
{
  if (!HasAttr(kNameSpaceID_XLink, nsGkAtoms::href)) {
    NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                      "We shouldn't have an xlink:href target "
                      "if we don't have an xlink:href attribute");
    return nullptr;
  }

  nsIContent* genericTarget = mHrefTarget.get();
  if (genericTarget && genericTarget->IsSVG(nsGkAtoms::path)) {
    return static_cast<SVGPathElement*>(genericTarget);
  }
  return nullptr;
}

//----------------------------------------------------------------------
// Protected helper methods

void
SVGMPathElement::UpdateHrefTarget(nsIContent* aParent,
                                  const nsAString& aHrefStr)
{
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI),
                                            aHrefStr, OwnerDoc(), baseURI);

  // Stop observing old target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->RemoveMutationObserver(this);
  }

  if (aParent) {
    // Pass in |aParent| instead of |this| -- first argument is only used
    // for a call to GetCurrentDoc(), and |this| might not have a current
    // document yet (if our caller is BindToTree).
    mHrefTarget.Reset(aParent, targetURI);
  } else {
    // if we don't have a parent, then there's no animateMotion element
    // depending on our target, so there's no point tracking it right now.
    mHrefTarget.Unlink();
  }

  // Start observing new target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->AddMutationObserver(this);
  }

  NotifyParentOfMpathChange(aParent);
}

void
SVGMPathElement::UnlinkHrefTarget(bool aNotifyParent)
{
  // Stop observing old target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->RemoveMutationObserver(this);
  }
  mHrefTarget.Unlink();

  if (aNotifyParent) {
    NotifyParentOfMpathChange(GetParent());
  }
}

void
SVGMPathElement::NotifyParentOfMpathChange(nsIContent* aParent)
{
  if (aParent && aParent->IsSVG(nsGkAtoms::animateMotion)) {

    SVGAnimateMotionElement* animateMotionParent =
      static_cast<SVGAnimateMotionElement*>(aParent);

    animateMotionParent->MpathChanged();
    AnimationNeedsResample();
  }
}

} // namespace dom
} // namespace mozilla

