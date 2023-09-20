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
#include "mozilla/ServoTypes.h"
#include "nsDOMCSSDeclaration.h"

struct RawServoUnlockedDeclarationBlock;

namespace mozilla {

class SMILValue;
class SVGAnimatedLength;
class SVGAnimatedPathSegList;

namespace dom {
class DomGroup;
class Element;
}  // namespace dom
}  // namespace mozilla

class nsDOMCSSAttributeDeclaration final : public nsDOMCSSDeclaration {
 public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::SMILValue SMILValue;
  typedef mozilla::SVGAnimatedLength SVGAnimatedLength;
  nsDOMCSSAttributeDeclaration(Element* aContent, bool aIsSMILOverride);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_AMBIGUOUS(
      nsDOMCSSAttributeDeclaration, nsICSSDeclaration)

  mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, mozilla::DeclarationBlock** aCreated) final;

  nsDOMCSSDeclaration::ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

  mozilla::css::Rule* GetParentRule() override { return nullptr; }

  nsINode* GetAssociatedNode() const override { return mElement; }
  nsINode* GetParentObject() const override { return mElement; }

  nsresult SetSMILValue(const nsCSSPropertyID aPropID, const SMILValue& aValue);
  nsresult SetSMILValue(const nsCSSPropertyID aPropID,
                        const SVGAnimatedLength& aLength);
  nsresult SetSMILValue(const nsCSSPropertyID,
                        const mozilla::SVGAnimatedPathSegList& aPath);
  void ClearSMILValue(const nsCSSPropertyID aPropID) {
    // Put empty string in override style for our property
    SetPropertyValue(aPropID, ""_ns, nullptr, mozilla::IgnoreErrors());
  }

  void SetPropertyValue(const nsCSSPropertyID aPropID, const nsACString& aValue,
                        nsIPrincipal* aSubjectPrincipal,
                        mozilla::ErrorResult& aRv) override;

  static void MutationClosureFunction(void* aData, nsCSSPropertyID);

  void GetPropertyChangeClosure(
      mozilla::DeclarationBlockMutationClosure* aClosure,
      mozilla::MutationClosureData* aClosureData) final {
    if (!mIsSMILOverride) {
      aClosure->function = MutationClosureFunction;
      aClosure->data = aClosureData;
      aClosureData->mShouldBeCalled = true;
      aClosureData->mElement = mElement;
    }
  }

 protected:
  ~nsDOMCSSAttributeDeclaration();

  nsresult SetCSSDeclaration(
      mozilla::DeclarationBlock* aDecl,
      mozilla::MutationClosureData* aClosureData) override;
  mozilla::dom::Document* DocToUpdate() override;

  RefPtr<Element> mElement;

  /* If true, this indicates that this nsDOMCSSAttributeDeclaration
   * should interact with mContent's SMIL override style rule (rather
   * than the inline style rule).
   */
  const bool mIsSMILOverride;

 private:
  template <typename SetterFunc>
  nsresult SetSMILValueHelper(SetterFunc aFunc);
};

#endif /* nsDOMCSSAttributeDeclaration_h */
