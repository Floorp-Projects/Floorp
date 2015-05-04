/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLScriptElement_h
#define mozilla_dom_HTMLScriptElement_h

#include "nsIDOMHTMLScriptElement.h"
#include "nsScriptElement.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class HTMLScriptElement final : public nsGenericHTMLElement,
                                public nsIDOMHTMLScriptElement,
                                public nsScriptElement
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLScriptElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                    FromParser aFromParser);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) override;
  using nsGenericHTMLElement::SetInnerHTML;
  virtual void SetInnerHTML(const nsAString& aInnerHTML,
                            mozilla::ErrorResult& aError) override;

  // nsIDOMHTMLScriptElement
  NS_DECL_NSIDOMHTMLSCRIPTELEMENT

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type) override;
  virtual void GetScriptText(nsAString& text) override;
  virtual void GetScriptCharset(nsAString& charset) override;
  virtual void FreezeUriAsyncDefer() override;
  virtual CORSMode GetCORSMode() const override;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // Element
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;

  // WebIDL
  void SetText(const nsAString& aValue, ErrorResult& rv);
  void SetCharset(const nsAString& aCharset, ErrorResult& rv);
  void SetDefer(bool aDefer, ErrorResult& rv);
  bool Defer();
  void SetSrc(const nsAString& aSrc, ErrorResult& rv);
  void SetType(const nsAString& aType, ErrorResult& rv);
  void SetHtmlFor(const nsAString& aHtmlFor, ErrorResult& rv);
  void SetEvent(const nsAString& aEvent, ErrorResult& rv);
  void GetCrossOrigin(nsAString& aResult)
  {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aResult);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError)
  {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }
  bool Async();
  void SetAsync(bool aValue, ErrorResult& rv);

protected:
  virtual ~HTMLScriptElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
  // nsScriptElement
  virtual bool HasScriptContent() override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLScriptElement_h
