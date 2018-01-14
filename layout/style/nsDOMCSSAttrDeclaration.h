/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object for element.style */

#ifndef nsDOMCSSAttributeDeclaration_h
#define nsDOMCSSAttributeDeclaration_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/DocGroup.h"
#include "nsDOMCSSDeclaration.h"


namespace mozilla {
namespace dom {
class DomGroup;
class Element;
} // namespace dom
} // namespace mozilla

class nsDOMCSSAttributeDeclaration final : public nsDOMCSSDeclaration
{
public:
  typedef mozilla::dom::Element Element;
  nsDOMCSSAttributeDeclaration(Element* aContent, bool aIsSMILOverride);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMCSSAttributeDeclaration,
                                                                   nsICSSDeclaration)

  // If GetCSSDeclaration returns non-null, then the decl it returns
  // is owned by our current style rule.
  virtual mozilla::DeclarationBlock* GetCSSDeclaration(Operation aOperation) override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv,
                                        nsIPrincipal* aSubjectPrincipal) override;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment
  GetServoCSSParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final override;
  mozilla::css::Rule* GetParentRule() override;

  virtual nsINode* GetParentObject() override;
  virtual mozilla::dom::DocGroup* GetDocGroup() const override;

  NS_IMETHOD SetPropertyValue(const nsCSSPropertyID aPropID,
                              const nsAString& aValue,
                              nsIPrincipal* aSubjectPrincipal) override;

protected:
  ~nsDOMCSSAttributeDeclaration();

  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl) override;
  virtual nsIDocument* DocToUpdate() override;

  RefPtr<Element> mElement;

  /* If true, this indicates that this nsDOMCSSAttributeDeclaration
   * should interact with mContent's SMIL override style rule (rather
   * than the inline style rule).
   */
  const bool mIsSMILOverride;
};

#endif /* nsDOMCSSAttributeDeclaration_h */
