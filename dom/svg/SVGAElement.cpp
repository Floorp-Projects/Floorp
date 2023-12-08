/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAElement.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/SVGAElementBinding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIContentInlines.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(A)

namespace mozilla::dom {

JSObject* SVGAElement::WrapNode(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return SVGAElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGAElement::sStringInfo[3] = {
    {nsGkAtoms::href, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_XLink, true},
    {nsGkAtoms::target, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAElement)
  NS_INTERFACE_MAP_ENTRY(Link)
NS_INTERFACE_MAP_END_INHERITING(SVGAElementBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(SVGAElement, SVGAElementBase, mRelList)

NS_IMPL_ADDREF_INHERITED(SVGAElement, SVGAElementBase)
NS_IMPL_RELEASE_INHERITED(SVGAElement, SVGAElementBase)

//----------------------------------------------------------------------
// Implementation

SVGAElement::SVGAElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGAElementBase(std::move(aNodeInfo)), Link(this) {}

already_AddRefed<DOMSVGAnimatedString> SVGAElement::Href() {
  return mStringAttributes[HREF].IsExplicitlySet()
             ? mStringAttributes[HREF].ToDOMAnimatedString(this)
             : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsINode methods

void SVGAElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  Element::GetEventTargetParent(aVisitor);

  GetEventTargetParentForLinks(aVisitor);
}

nsresult SVGAElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  return PostHandleEventForLinks(aVisitor);
}

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGAElement::Target() {
  return mStringAttributes[TARGET].ToDOMAnimatedString(this);
}

void SVGAElement::GetDownload(nsAString& aDownload) {
  GetAttr(nsGkAtoms::download, aDownload);
}

void SVGAElement::SetDownload(const nsAString& aDownload, ErrorResult& rv) {
  SetAttr(nsGkAtoms::download, aDownload, rv);
}

void SVGAElement::GetPing(nsAString& aPing) { GetAttr(nsGkAtoms::ping, aPing); }

void SVGAElement::SetPing(const nsAString& aPing, ErrorResult& rv) {
  SetAttr(nsGkAtoms::ping, aPing, rv);
}

void SVGAElement::GetRel(nsAString& aRel) { GetAttr(nsGkAtoms::rel, aRel); }

void SVGAElement::SetRel(const nsAString& aRel, ErrorResult& rv) {
  SetAttr(nsGkAtoms::rel, aRel, rv);
}

void SVGAElement::GetReferrerPolicy(nsAString& aPolicy) {
  GetEnumAttr(nsGkAtoms::referrerpolicy, "", aPolicy);
}

void SVGAElement::SetReferrerPolicy(const nsAString& aPolicy,
                                    mozilla::ErrorResult& rv) {
  SetAttr(nsGkAtoms::referrerpolicy, aPolicy, rv);
}

nsDOMTokenList* SVGAElement::RelList() {
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, sSupportedRelValues);
  }
  return mRelList;
}

void SVGAElement::GetHreflang(nsAString& aHreflang) {
  GetAttr(nsGkAtoms::hreflang, aHreflang);
}

void SVGAElement::SetHreflang(const nsAString& aHreflang,
                              mozilla::ErrorResult& rv) {
  SetAttr(nsGkAtoms::hreflang, aHreflang, rv);
}

void SVGAElement::GetType(nsAString& aType) { GetAttr(nsGkAtoms::type, aType); }

void SVGAElement::SetType(const nsAString& aType, mozilla::ErrorResult& rv) {
  SetAttr(nsGkAtoms::type, aType, rv);
}

void SVGAElement::GetText(nsAString& aText, mozilla::ErrorResult& rv) const {
  if (NS_WARN_IF(
          !nsContentUtils::GetNodeTextContent(this, true, aText, fallible))) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void SVGAElement::SetText(const nsAString& aText, mozilla::ErrorResult& rv) {
  rv = nsContentUtils::SetNodeTextContent(this, aText, false);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult SVGAElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = SVGAElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  Link::BindToTree(aContext);
  return NS_OK;
}

void SVGAElement::UnbindFromTree(bool aNullParent) {
  SVGAElementBase::UnbindFromTree(aNullParent);
  // Without removing the link state we risk a dangling pointer
  // in the mStyledLinks hashtable
  Link::UnbindFromTree();
}

int32_t SVGAElement::TabIndexDefault() { return 0; }

Focusable SVGAElement::IsFocusableWithoutStyle(bool aWithMouse) {
  Focusable result;
  if (IsSVGFocusable(&result.mFocusable, &result.mTabIndex)) {
    return result;
  }

  if (!OwnerDoc()->LinkHandlingEnabled()) {
    return {};
  }

  // Links that are in an editable region should never be focusable, even if
  // they are in a contenteditable="false" region.
  if (nsContentUtils::IsNodeInEditableRegion(this)) {
    return {};
  }

  if (GetTabIndexAttrValue().isNothing()) {
    // check whether we're actually a link
    if (!IsLink()) {
      // Not tabbable or focusable without href (bug 17605), unless
      // forced to be via presence of nonnegative tabindex attribute
      return {};
    }
  }
  if ((sTabFocusModel & eTabFocus_linksMask) == 0) {
    result.mTabIndex = -1;
  }
  return result;
}

bool SVGAElement::HasHref() const {
  // Currently our SMIL implementation does not modify the DOM attributes. Once
  // we implement the SVG 2 SMIL behaviour this can be removed.
  return mStringAttributes[HREF].IsExplicitlySet() ||
         mStringAttributes[XLINK_HREF].IsExplicitlySet();
}

already_AddRefed<nsIURI> SVGAElement::GetHrefURI() const {
  // Optimization: check for href first for early return
  bool useBareHref = mStringAttributes[HREF].IsExplicitlySet();
  if (useBareHref || mStringAttributes[XLINK_HREF].IsExplicitlySet()) {
    // Get absolute URI
    nsAutoString str;
    const uint8_t idx = useBareHref ? HREF : XLINK_HREF;
    mStringAttributes[idx].GetAnimValue(str, this);
    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), str,
                                              OwnerDoc(), GetBaseURI());
    return uri.forget();
  }
  return nullptr;
}

void SVGAElement::GetLinkTarget(nsAString& aTarget) {
  mStringAttributes[TARGET].GetAnimValue(aTarget, this);
  if (aTarget.IsEmpty()) {
    static Element::AttrValuesArray sShowVals[] = {nsGkAtoms::_new,
                                                   nsGkAtoms::replace, nullptr};

    switch (FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show, sShowVals,
                            eCaseMatters)) {
      case 0:
        aTarget.AssignLiteral("_blank");
        return;
      case 1:
        return;
    }
    Document* ownerDoc = OwnerDoc();
    if (ownerDoc) {
      ownerDoc->GetBaseTarget(aTarget);
    }
  }
}

void SVGAElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                               const nsAttrValue* aValue,
                               const nsAttrValue* aOldValue,
                               nsIPrincipal* aMaybeScriptedPrincipal,
                               bool aNotify) {
  if (aName == nsGkAtoms::href && (aNameSpaceID == kNameSpaceID_XLink ||
                                   aNameSpaceID == kNameSpaceID_None)) {
    // We can't assume that null aValue means we no longer have an href, because
    // we could be unsetting xlink:href but still have a null-namespace href, or
    // vice versa.  But we can fast-path the case when we _do_ have a new value.
    Link::ResetLinkState(aNotify, aValue || Link::ElementHasHref());
  }

  return SVGAElementBase::AfterSetAttr(aNameSpaceID, aName, aValue, aOldValue,
                                       aMaybeScriptedPrincipal, aNotify);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo SVGAElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace mozilla::dom
