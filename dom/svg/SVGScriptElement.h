/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGSCRIPTELEMENT_H_
#define DOM_SVG_SVGSCRIPTELEMENT_H_

#include "SVGAnimatedString.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/ScriptElement.h"
#include "mozilla/dom/SVGElement.h"

nsresult NS_NewSVGScriptElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser);

namespace mozilla::dom {

using SVGScriptElementBase = SVGElement;

class SVGScriptElement final : public SVGScriptElementBase,
                               public ScriptElement {
 protected:
  friend nsresult(::NS_NewSVGScriptElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      mozilla::dom::FromParser aFromParser));
  SVGScriptElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                   FromParser aFromParser);

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIScriptElement
  void GetScriptText(nsAString& text) const override;
  void GetScriptCharset(nsAString& charset) override;
  void FreezeExecutionAttrs(const Document* aOwnerDoc) override;
  CORSMode GetCORSMode() const override;
  FetchPriority GetFetchPriority() const override;

  // ScriptElement
  bool HasScriptContent() override;

  // nsIContent specializations:
  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  void GetType(nsAString& aType);
  void SetType(const nsAString& aType, ErrorResult& rv);
  bool Async() { return mForceAsync || GetBoolAttr(nsGkAtoms::async); }
  void SetAsync(bool aValue) {
    mForceAsync = false;
    SetBoolAttr(nsGkAtoms::async, aValue);
  }
  bool Defer() { return GetBoolAttr(nsGkAtoms::defer); }
  void SetDefer(bool aDefer) { SetBoolAttr(nsGkAtoms::defer, aDefer); }
  void GetCrossOrigin(nsAString& aCrossOrigin);
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError);
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  ~SVGScriptElement() = default;

  StringAttributesInfo GetStringInfo() override;

  // SVG Script elements don't have the ability to set async properties on
  // themselves, so this will always return false.
  bool GetAsyncState() override { return false; }

  nsIContent* GetAsContent() override { return this; }

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGSCRIPTELEMENT_H_
