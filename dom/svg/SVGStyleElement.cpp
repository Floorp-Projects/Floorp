/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGStyleElement.h"

#include "mozilla/RefPtr.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/SVGStyleElementBinding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Style)

namespace mozilla::dom {

JSObject* SVGStyleElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return SVGStyleElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(SVGStyleElement,
                                             SVGStyleElementBase,
                                             nsIMutationObserver)

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGStyleElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGStyleElement,
                                                  SVGStyleElementBase)
  tmp->LinkStyle::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGStyleElement,
                                                SVGStyleElementBase)
  tmp->LinkStyle::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

//----------------------------------------------------------------------
// Implementation

SVGStyleElement::SVGStyleElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGStyleElementBase(std::move(aNodeInfo)) {
  AddMutationObserver(this);
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGStyleElement)

//----------------------------------------------------------------------
// nsIContent methods

nsresult SVGStyleElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = SVGStyleElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  LinkStyle::BindToTree();
  return rv;
}

void SVGStyleElement::UnbindFromTree(bool aNullParent) {
  nsCOMPtr<Document> oldDoc = GetUncomposedDoc();
  ShadowRoot* oldShadow = GetContainingShadow();
  SVGStyleElementBase::UnbindFromTree(aNullParent);
  Unused << UpdateStyleSheetInternal(oldDoc, oldShadow);
}

void SVGStyleElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                   const nsAttrValue* aValue,
                                   const nsAttrValue* aOldValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::title || aName == nsGkAtoms::media ||
        aName == nsGkAtoms::type) {
      Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
    }
  }

  return SVGStyleElementBase::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

bool SVGStyleElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::crossorigin) {
    ParseCORSValue(aValue, aResult);
    return true;
  }

  return SVGStyleElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aMaybeScriptedPrincipal, aResult);
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void SVGStyleElement::CharacterDataChanged(nsIContent* aContent,
                                           const CharacterDataChangeInfo&) {
  ContentChanged(aContent);
}

void SVGStyleElement::ContentAppended(nsIContent* aFirstNewContent) {
  ContentChanged(aFirstNewContent->GetParent());
}

void SVGStyleElement::ContentInserted(nsIContent* aChild) {
  ContentChanged(aChild);
}

void SVGStyleElement::ContentRemoved(nsIContent* aChild,
                                     nsIContent* aPreviousSibling) {
  ContentChanged(aChild);
}

void SVGStyleElement::ContentChanged(nsIContent* aContent) {
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    Unused << UpdateStyleSheetInternal(nullptr, nullptr);
  }
}

//----------------------------------------------------------------------

void SVGStyleElement::GetMedia(nsAString& aMedia) {
  GetAttr(nsGkAtoms::media, aMedia);
}

void SVGStyleElement::SetMedia(const nsAString& aMedia, ErrorResult& rv) {
  SetAttr(nsGkAtoms::media, aMedia, rv);
}

void SVGStyleElement::GetType(nsAString& aType) {
  GetAttr(nsGkAtoms::type, aType);
}

void SVGStyleElement::SetType(const nsAString& aType, ErrorResult& rv) {
  SetAttr(nsGkAtoms::type, aType, rv);
}

void SVGStyleElement::GetTitle(nsAString& aTitle) {
  GetAttr(nsGkAtoms::title, aTitle);
}

void SVGStyleElement::SetTitle(const nsAString& aTitle, ErrorResult& rv) {
  SetAttr(nsGkAtoms::title, aTitle, rv);
}

bool SVGStyleElement::Disabled() const {
  StyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

void SVGStyleElement::SetDisabled(bool aDisabled) {
  if (StyleSheet* ss = GetSheet()) {
    ss->SetDisabled(aDisabled);
  }
}

//----------------------------------------------------------------------
// nsStyleLinkElement methods

Maybe<LinkStyle::SheetInfo> SVGStyleElement::GetStyleSheetInfo() {
  if (!IsCSSMimeTypeAttributeForStyleElement(*this)) {
    return Nothing();
  }

  nsAutoString title;
  nsAutoString media;
  GetTitleAndMediaForElement(*this, title, media);

  return Some(SheetInfo{
      *OwnerDoc(),
      this,
      nullptr,
      // FIXME(bug 1459822): Why doesn't this need a principal, but
      // HTMLStyleElement does?
      nullptr,
      // FIXME(bug 1459822): Why does this need a crossorigin attribute, but
      // HTMLStyleElement doesn't?
      MakeAndAddRef<ReferrerInfo>(*this),
      AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin)),
      title,
      media,
      /* integrity = */ u""_ns,
      /* nsStyleUtil::CSPAllowsInlineStyle takes care of nonce checking for
         inline styles. Bug 1607011 */
      /* nonce = */ u""_ns,
      HasAlternateRel::No,
      IsInline::Yes,
      IsExplicitlyEnabled::No,
  });
}

}  // namespace mozilla::dom
