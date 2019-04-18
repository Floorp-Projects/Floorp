/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGScriptElement_h
#define mozilla_dom_SVGScriptElement_h

#include "SVGAnimatedString.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/ScriptElement.h"
#include "mozilla/dom/SVGElement.h"

nsresult NS_NewSVGScriptElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser);

namespace mozilla {
namespace dom {

typedef SVGElement SVGScriptElementBase;

class SVGScriptElement final : public SVGScriptElementBase,
                               public ScriptElement {
 protected:
  friend nsresult(::NS_NewSVGScriptElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      mozilla::dom::FromParser aFromParser));
  SVGScriptElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                   FromParser aFromParser);

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIScriptElement
  virtual bool GetScriptType(nsAString& type) override;
  virtual void GetScriptText(nsAString& text) override;
  virtual void GetScriptCharset(nsAString& charset) override;
  virtual void FreezeExecutionAttrs(Document* aOwnerDoc) override;
  virtual CORSMode GetCORSMode() const override;

  // ScriptElement
  virtual bool HasScriptContent() override;

  // nsIContent specializations:
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  void GetType(nsAString& aType);
  void SetType(const nsAString& aType, ErrorResult& rv);
  void GetCrossOrigin(nsAString& aCrossOrigin);
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError);
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  ~SVGScriptElement() = default;

  virtual StringAttributesInfo GetStringInfo() override;

  // SVG Script elements don't have the ability to set async properties on
  // themselves, so this will always return false.
  virtual bool GetAsyncState() override { return false; }

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGScriptElement_h
