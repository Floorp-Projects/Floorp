/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMPathElement.h"

#include "nsDebug.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGPathElement.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGMPathElementBinding.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(MPath)

namespace mozilla {
namespace dom {

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
  tmp->UnlinkHrefTarget(false);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGMPathElement,
                                                  SVGMPathElementBase)
  tmp->mPathTracker.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(SVGMPathElement,
                                             SVGMPathElementBase,
                                             nsIMutationObserver)

// Constructor
SVGMPathElement::SVGMPathElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGMPathElementBase(std::move(aNodeInfo)), mPathTracker(this) {}

SVGMPathElement::~SVGMPathElement() { UnlinkHrefTarget(false); }

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

nsresult SVGMPathElement::BindToTree(Document* aDocument, nsIContent* aParent,
                                     nsIContent* aBindingParent) {
  MOZ_ASSERT(!mPathTracker.get(),
             "Shouldn't have href-target yet (or it should've been cleared)");
  nsresult rv =
      SVGMPathElementBase::BindToTree(aDocument, aParent, aBindingParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    const nsAttrValue* hrefAttrValue =
        HasAttr(kNameSpaceID_None, nsGkAtoms::href)
            ? mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_None)
            : mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
    if (hrefAttrValue) {
      UpdateHrefTarget(aParent, hrefAttrValue->GetStringValue());
    }
  }

  return NS_OK;
}

void SVGMPathElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  UnlinkHrefTarget(true);
  SVGMPathElementBase::UnbindFromTree(aDeep, aNullParent);
}

bool SVGMPathElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult) {
  bool returnVal = SVGMPathElementBase::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
  if ((aNamespaceID == kNameSpaceID_XLink ||
       aNamespaceID == kNameSpaceID_None) &&
      aAttribute == nsGkAtoms::href && IsInComposedDoc()) {
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

nsresult SVGMPathElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       bool aNotify) {
  if (!aValue && aName == nsGkAtoms::href) {
    // href attr being removed.
    if (aNamespaceID == kNameSpaceID_None) {
      UnlinkHrefTarget(true);

      // After unsetting href, we may still have xlink:href, so we should
      // try to add it back.
      const nsAttrValue* xlinkHref =
          mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
      if (xlinkHref) {
        UpdateHrefTarget(GetParent(), xlinkHref->GetStringValue());
      }
    } else if (aNamespaceID == kNameSpaceID_XLink &&
               !HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      UnlinkHrefTarget(true);
    }  // else: we unset some random-namespace href attribute, or unset
       // xlink:href but still have href attribute, so keep the target linking
       // to href.
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
// nsIMutationObserver methods

void SVGMPathElement::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                       nsAtom* aAttribute, int32_t aModType,
                                       const nsAttrValue* aOldValue) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::d) {
      NotifyParentOfMpathChange(GetParent());
    }
  }
}

//----------------------------------------------------------------------
// Public helper methods

SVGPathElement* SVGMPathElement::GetReferencedPath() {
  if (!HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
    MOZ_ASSERT(!mPathTracker.get(),
               "We shouldn't have a href target "
               "if we don't have an xlink:href or href attribute");
    return nullptr;
  }

  nsIContent* genericTarget = mPathTracker.get();
  if (genericTarget && genericTarget->IsSVGElement(nsGkAtoms::path)) {
    return static_cast<SVGPathElement*>(genericTarget);
  }
  return nullptr;
}

//----------------------------------------------------------------------
// Protected helper methods

void SVGMPathElement::UpdateHrefTarget(nsIContent* aParent,
                                       const nsAString& aHrefStr) {
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), aHrefStr,
                                            OwnerDoc(), baseURI);

  // Stop observing old target (if any)
  if (mPathTracker.get()) {
    mPathTracker.get()->RemoveMutationObserver(this);
  }

  if (aParent) {
    // Pass in |aParent| instead of |this| -- first argument is only used
    // for a call to GetComposedDoc(), and |this| might not have a current
    // document yet (if our caller is BindToTree).
    // Bug 1415044 to investigate which referrer we should use
    mPathTracker.ResetToURIFragmentID(aParent, targetURI,
                                      OwnerDoc()->GetDocumentURI(),
                                      OwnerDoc()->GetReferrerPolicy());
  } else {
    // if we don't have a parent, then there's no animateMotion element
    // depending on our target, so there's no point tracking it right now.
    mPathTracker.Unlink();
  }

  // Start observing new target (if any)
  if (mPathTracker.get()) {
    mPathTracker.get()->AddMutationObserver(this);
  }

  NotifyParentOfMpathChange(aParent);
}

void SVGMPathElement::UnlinkHrefTarget(bool aNotifyParent) {
  // Stop observing old target (if any)
  if (mPathTracker.get()) {
    mPathTracker.get()->RemoveMutationObserver(this);
  }
  mPathTracker.Unlink();

  if (aNotifyParent) {
    NotifyParentOfMpathChange(GetParent());
  }
}

void SVGMPathElement::NotifyParentOfMpathChange(nsIContent* aParent) {
  if (aParent && aParent->IsSVGElement(nsGkAtoms::animateMotion)) {
    SVGAnimateMotionElement* animateMotionParent =
        static_cast<SVGAnimateMotionElement*>(aParent);

    animateMotionParent->MpathChanged();
    AnimationNeedsResample();
  }
}

}  // namespace dom
}  // namespace mozilla
