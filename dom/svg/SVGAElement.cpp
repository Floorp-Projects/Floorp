/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAElement.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/SVGAElementBinding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIContentInlines.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(A)

namespace mozilla {
namespace dom {

JSObject* SVGAElement::WrapNode(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return SVGAElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGAElement::sStringInfo[3] = {
    {nsGkAtoms::href, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_XLink, true},
    {nsGkAtoms::target, kNameSpaceID_None, true}};

// static
const DOMTokenListSupportedToken SVGAElement::sSupportedRelValues[] = {
    "noreferrer", "noopener", nullptr};

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
// Link methods

bool SVGAElement::ElementHasHref() const {
  return mStringAttributes[HREF].IsExplicitlySet() ||
         mStringAttributes[XLINK_HREF].IsExplicitlySet();
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
  GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aPolicy);
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

void SVGAElement::GetText(nsAString& aText, mozilla::ErrorResult& rv) {
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

nsresult SVGAElement::BindToTree(Document* aDocument, nsIContent* aParent,
                                 nsIContent* aBindingParent) {
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = SVGAElementBase::BindToTree(aDocument, aParent, aBindingParent);
  NS_ENSURE_SUCCESS(rv, rv);

  Document* doc = GetComposedDoc();
  if (doc) {
    doc->RegisterPendingLinkUpdate(this);
  }

  return NS_OK;
}

void SVGAElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  // Without removing the link state we risk a dangling pointer
  // in the mStyledLinks hashtable
  Link::ResetLinkState(false, Link::ElementHasHref());

  SVGAElementBase::UnbindFromTree(aDeep, aNullParent);
}

already_AddRefed<nsIURI> SVGAElement::GetHrefURI() const {
  nsCOMPtr<nsIURI> hrefURI;
  return IsLink(getter_AddRefs(hrefURI)) ? hrefURI.forget() : nullptr;
}

NS_IMETHODIMP_(bool)
SVGAElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sFEFloodMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sLightingEffectsMap,
                                                    sMarkersMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap};

  return FindAttributeDependence(name, map) ||
         SVGAElementBase::IsAttributeMapped(name);
}

int32_t SVGAElement::TabIndexDefault() { return 0; }

static bool IsNodeInEditableRegion(nsINode* aNode) {
  while (aNode) {
    if (aNode->IsEditable()) {
      return true;
    }
    aNode = aNode->GetParent();
  }
  return false;
}

bool SVGAElement::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) {
  bool isFocusable = false;
  if (IsSVGFocusable(&isFocusable, aTabIndex)) {
    return isFocusable;
  }

  // cannot focus links if there is no link handler
  Document* doc = GetComposedDoc();
  if (doc) {
    nsPresContext* presContext = doc->GetPresContext();
    if (presContext && !presContext->GetLinkHandler()) {
      return false;
    }
  }

  // Links that are in an editable region should never be focusable, even if
  // they are in a contenteditable="false" region.
  if (IsNodeInEditableRegion(this)) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    return false;
  }

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
    // check whether we're actually a link
    if (!Link::HasURI()) {
      // Not tabbable or focusable without href (bug 17605), unless
      // forced to be via presence of nonnegative tabindex attribute
      if (aTabIndex) {
        *aTabIndex = -1;
      }
      return false;
    }
  }

  if (aTabIndex && (sTabFocusModel & eTabFocus_linksMask) == 0) {
    *aTabIndex = -1;
  }

  return true;
}

bool SVGAElement::IsLink(nsIURI** aURI) const {
  // To be a clickable XLink for styling and interaction purposes, we require:
  //
  //   xlink:href    - must be set
  //   xlink:type    - must be unset or set to "" or set to "simple"
  //   xlink:show    - must be unset or set to "", "new" or "replace"
  //   xlink:actuate - must be unset or set to "" or "onRequest"
  //
  // For any other values, we're either not a *clickable* XLink, or the end
  // result is poorly specified. Either way, we return false.

  static Element::AttrValuesArray sTypeVals[] = {nsGkAtoms::_empty,
                                                 nsGkAtoms::simple, nullptr};

  static Element::AttrValuesArray sShowVals[] = {
      nsGkAtoms::_empty, nsGkAtoms::_new, nsGkAtoms::replace, nullptr};

  static Element::AttrValuesArray sActuateVals[] = {
      nsGkAtoms::_empty, nsGkAtoms::onRequest, nullptr};

  // Optimization: check for href first for early return
  bool useBareHref = mStringAttributes[HREF].IsExplicitlySet();

  if ((useBareHref || mStringAttributes[XLINK_HREF].IsExplicitlySet()) &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::type, sTypeVals,
                      eCaseMatters) != Element::ATTR_VALUE_NO_MATCH &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show, sShowVals,
                      eCaseMatters) != Element::ATTR_VALUE_NO_MATCH &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::actuate, sActuateVals,
                      eCaseMatters) != Element::ATTR_VALUE_NO_MATCH) {
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    // Get absolute URI
    nsAutoString str;
    const uint8_t idx = useBareHref ? HREF : XLINK_HREF;
    mStringAttributes[idx].GetAnimValue(str, this);
    nsContentUtils::NewURIWithDocumentCharset(aURI, str, OwnerDoc(), baseURI);
    // must promise out param is non-null if we return true
    return !!*aURI;
  }

  *aURI = nullptr;
  return false;
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

EventStates SVGAElement::IntrinsicState() const {
  return Link::LinkState() | SVGAElementBase::IntrinsicState();
}

nsresult SVGAElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
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

}  // namespace dom
}  // namespace mozilla
