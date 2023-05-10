/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSharedElement_h
#define mozilla_dom_HTMLSharedElement_h

#include "nsGenericHTMLElement.h"

#include "nsGkAtoms.h"

#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"

namespace mozilla::dom {

class HTMLSharedElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLSharedElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {
    if (mNodeInfo->Equals(nsGkAtoms::head) ||
        mNodeInfo->Equals(nsGkAtoms::html)) {
      SetHasWeirdParserInsertionMode();
    }
  }

  // nsIContent
  void DoneAddingChildren(bool aHaveNotified) override;

  nsresult BindToTree(BindContext&, nsINode& aParent) override;

  void UnbindFromTree(bool aNullParent = true) override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL API
  // HTMLParamElement
  void GetName(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::name, aValue, aResult);
  }
  void GetValue(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::value, aValue);
  }
  void SetValue(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::value, aValue, aResult);
  }
  void GetType(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::type, aValue, aResult);
  }
  void GetValueType(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::valuetype, aValue);
  }
  void SetValueType(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::valuetype, aValue, aResult);
  }

  // HTMLBaseElement
  void GetTarget(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    GetHTMLAttr(nsGkAtoms::target, aValue);
  }
  void SetTarget(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    SetHTMLAttr(nsGkAtoms::target, aValue, aResult);
  }

  void GetHref(nsAString& aValue);
  void SetHref(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    SetHTMLAttr(nsGkAtoms::href, aValue, aResult);
  }

  // HTMLDirectoryElement
  bool Compact() const {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::dir));
    return GetBoolAttr(nsGkAtoms::compact);
  }
  void SetCompact(bool aCompact, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::dir));
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, aResult);
  }

  // HTMLQuoteElement
  void GetCite(nsString& aCite) { GetHTMLURIAttr(nsGkAtoms::cite, aCite); }

  void SetCite(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::q) ||
               mNodeInfo->Equals(nsGkAtoms::blockquote));
    SetHTMLAttr(nsGkAtoms::cite, aValue, aResult);
  }

  // HTMLHtmlElement
  void GetVersion(DOMString& aValue) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
    GetHTMLAttr(nsGkAtoms::version, aValue);
  }
  void SetVersion(const nsAString& aValue, ErrorResult& aResult) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
    SetHTMLAttr(nsGkAtoms::version, aValue, aResult);
  }

 protected:
  virtual ~HTMLSharedElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLSharedElement_h
