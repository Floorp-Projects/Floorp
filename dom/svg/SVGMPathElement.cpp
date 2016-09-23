/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/dom/SVGMPathElement.h"
#include "nsDebug.h"
#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGPathElement.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGMPathElementBinding.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(MPath)

namespace mozilla {
namespace dom {

JSObject*
SVGMPathElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGMPathElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::StringInfo SVGMPathElement::sStringInfo[2] =
{
  { &nsGkAtoms::href, kNameSpaceID_None, false },
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};

// Cycle collection magic -- based on SVGUseElement
NS_IMPL_CYCLE_COLLECTION_CLASS(SVGMPathElement)

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
  NS_INTERFACE_TABLE_INHERITED(SVGMPathElement, nsIDOMNode, nsIDOMElement,
                               nsIDOMSVGElement,
                               nsIMutationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGMPathElementBase)

// Constructor
SVGMPathElement::SVGMPathElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGMPathElementBase(aNodeInfo),
    mHrefTarget(this)
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
  return mStringAttributes[HREF].IsExplicitlySet()
         ? mStringAttributes[HREF].ToDOMAnimatedString(this)
         : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
SVGMPathElement::BindToTree(nsIDocument* aDocument,
                            nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  MOZ_ASSERT(!mHrefTarget.get(),
             "Shouldn't have href-target yet (or it should've been cleared)");
  nsresult rv = SVGMPathElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aDocument) {
    const nsAttrValue* hrefAttrValue =
      HasAttr(kNameSpaceID_None, nsGkAtoms::href)
      ? mAttrsAndChildren.GetAttr(nsGkAtoms::href, kNameSpaceID_None)
      : mAttrsAndChildren.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
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
  if ((aNamespaceID == kNameSpaceID_XLink ||
       aNamespaceID == kNameSpaceID_None ) &&
      aAttribute == nsGkAtoms::href &&
      IsInUncomposedDoc()) {
    // Note: If we fail the IsInDoc call, it's ok -- we'll update the target
    // on next BindToTree call.

    // Note: "href" takes priority over xlink:href. So if "xlink:href" is being
    // set here, we only let that update our target if "href" is *unset*.
    if (aNamespaceID != kNameSpaceID_XLink ||
        !mStringAttributes[HREF].IsExplicitlySet()) {
      UpdateHrefTarget(GetParent(), aValue);
    }
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

  if (aAttribute == nsGkAtoms::href) {
    if (aNamespaceID == kNameSpaceID_None) {
      UnlinkHrefTarget(true);

      // After unsetting href, we may still have xlink:href, so we should
      // try to add it back.
      const nsAttrValue* xlinkHref =
        mAttrsAndChildren.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
      if (xlinkHref) {
        UpdateHrefTarget(GetParent(), xlinkHref->GetStringValue());
      }
    } else if (!HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      UnlinkHrefTarget(true);
    } // else: we unset xlink:href, but we still have href attribute, so keep
      // the target linking to href.
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
                                  int32_t aModType,
                                  const nsAttrValue* aOldValue)
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
  if (!HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
    MOZ_ASSERT(!mHrefTarget.get(),
               "We shouldn't have a href target "
               "if we don't have an xlink:href or href attribute");
    return nullptr;
  }

  nsIContent* genericTarget = mHrefTarget.get();
  if (genericTarget && genericTarget->IsSVGElement(nsGkAtoms::path)) {
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
    // for a call to GetComposedDoc(), and |this| might not have a current
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
  if (aParent && aParent->IsSVGElement(nsGkAtoms::animateMotion)) {

    SVGAnimateMotionElement* animateMotionParent =
      static_cast<SVGAnimateMotionElement*>(aParent);

    animateMotionParent->MpathChanged();
    AnimationNeedsResample();
  }
}

} // namespace dom
} // namespace mozilla

