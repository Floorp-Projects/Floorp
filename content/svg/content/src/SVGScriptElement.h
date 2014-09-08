/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGScriptElement_h
#define mozilla_dom_SVGScriptElement_h

#include "nsSVGElement.h"
#include "nsCOMPtr.h"
#include "nsSVGString.h"
#include "nsScriptElement.h"

class nsIDocument;

nsresult NS_NewSVGScriptElement(nsIContent **aResult,
                                already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                                mozilla::dom::FromParser aFromParser);

namespace mozilla {
namespace dom {

typedef nsSVGElement SVGScriptElementBase;

class SVGScriptElement MOZ_FINAL : public SVGScriptElementBase,
                                   public nsScriptElement
{
protected:
  friend nsresult (::NS_NewSVGScriptElement(nsIContent **aResult,
                                            already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                                            mozilla::dom::FromParser aFromParser));
  SVGScriptElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                   FromParser aFromParser);

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type) MOZ_OVERRIDE;
  virtual void GetScriptText(nsAString& text) MOZ_OVERRIDE;
  virtual void GetScriptCharset(nsAString& charset) MOZ_OVERRIDE;
  virtual void FreezeUriAsyncDefer() MOZ_OVERRIDE;
  virtual CORSMode GetCORSMode() const MOZ_OVERRIDE;

  // nsScriptElement
  virtual bool HasScriptContent() MOZ_OVERRIDE;

  // nsIContent specializations:
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  void GetType(nsAString & aType);
  void SetType(const nsAString & aType, ErrorResult& rv);
  void GetCrossOrigin(nsAString & aCrossOrigin);
  void SetCrossOrigin(const nsAString & aCrossOrigin, ErrorResult& aError);
  already_AddRefed<SVGAnimatedString> Href();

protected:
  ~SVGScriptElement();

  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGScriptElement_h
