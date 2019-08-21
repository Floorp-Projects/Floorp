/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLScriptElement_h
#define mozilla_dom_HTMLScriptElement_h

#include "nsGenericHTMLElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ScriptElement.h"

namespace mozilla {
namespace dom {

class HTMLScriptElement final : public nsGenericHTMLElement,
                                public ScriptElement {
 public:
  using Element::GetText;

  HTMLScriptElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                    FromParser aFromParser);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  void GetInnerHTML(nsAString& aInnerHTML, OOMReporter& aError) override;
  virtual void SetInnerHTML(const nsAString& aInnerHTML,
                            nsIPrincipal* aSubjectPrincipal,
                            mozilla::ErrorResult& aError) override;

  // nsIScriptElement
  virtual bool GetScriptType(nsAString& type) override;
  virtual void GetScriptText(nsAString& text) override;
  virtual void GetScriptCharset(nsAString& charset) override;
  virtual void FreezeExecutionAttrs(Document* aOwnerDoc) override;
  virtual CORSMode GetCORSMode() const override;
  virtual mozilla::dom::ReferrerPolicy GetReferrerPolicy() override;

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // Element
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;

  // WebIDL
  void GetText(nsAString& aValue, ErrorResult& aRv);

  void SetText(const nsAString& aValue, ErrorResult& aRv);

  void GetCharset(nsAString& aCharset) {
    GetHTMLAttr(nsGkAtoms::charset, aCharset);
  }
  void SetCharset(const nsAString& aCharset, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::charset, aCharset, aRv);
  }

  bool Defer() { return GetBoolAttr(nsGkAtoms::defer); }
  void SetDefer(bool aDefer, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::defer, aDefer, aRv);
  }

  void GetSrc(nsAString& aSrc) { GetURIAttr(nsGkAtoms::src, nullptr, aSrc); }
  void SetSrc(const nsAString& aSrc, nsIPrincipal* aTriggeringPrincipal,
              ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aTriggeringPrincipal, aRv);
  }

  void GetType(nsAString& aType) { GetHTMLAttr(nsGkAtoms::type, aType); }
  void SetType(const nsAString& aType, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::type, aType, aRv);
  }

  void GetHtmlFor(nsAString& aHtmlFor) {
    GetHTMLAttr(nsGkAtoms::_for, aHtmlFor);
  }
  void SetHtmlFor(const nsAString& aHtmlFor, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::_for, aHtmlFor, aRv);
  }

  void GetEvent(nsAString& aEvent) { GetHTMLAttr(nsGkAtoms::event, aEvent); }
  void SetEvent(const nsAString& aEvent, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::event, aEvent, aRv);
  }

  bool Async() { return mForceAsync || GetBoolAttr(nsGkAtoms::async); }

  void SetAsync(bool aValue, ErrorResult& aRv) {
    mForceAsync = false;
    SetHTMLBoolAttr(nsGkAtoms::async, aValue, aRv);
  }

  bool NoModule() { return GetBoolAttr(nsGkAtoms::nomodule); }

  void SetNoModule(bool aValue, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::nomodule, aValue, aRv);
  }

  void GetCrossOrigin(nsAString& aResult) {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aResult);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError) {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }
  void GetIntegrity(nsAString& aIntegrity) {
    GetHTMLAttr(nsGkAtoms::integrity, aIntegrity);
  }
  void SetIntegrity(const nsAString& aIntegrity, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::integrity, aIntegrity, aRv);
  }
  void SetReferrerPolicy(const nsAString& aReferrerPolicy,
                         ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aReferrerPolicy, aError);
  }
  void GetReferrerPolicy(nsAString& aReferrerPolicy) {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(),
                aReferrerPolicy);
  }

 protected:
  virtual ~HTMLScriptElement();

  virtual bool GetAsyncState() override { return Async(); }

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  // ScriptElement
  virtual bool HasScriptContent() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLScriptElement_h
