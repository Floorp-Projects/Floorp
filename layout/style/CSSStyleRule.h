/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSStyleRule_h
#define mozilla_CSSStyleRule_h

#include "mozilla/BindingStyleRule.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/WeakPtr.h"

#include "nsDOMCSSDeclaration.h"

namespace mozilla {

class DeclarationBlock;

namespace dom {
class DocGroup;
class CSSStyleRule;

class CSSStyleRuleDeclaration final : public nsDOMCSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetParentObject() final;

protected:
  mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
    Operation aOperation, mozilla::DeclarationBlock** aCreated) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl,
                             MutationClosureData* aClosureData) final;
  nsIDocument* DocToUpdate() final;
  ParsingEnvironment
  GetParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final;

private:
  // For accessing the constructor.
  friend class CSSStyleRule;

  explicit CSSStyleRuleDeclaration(
    already_AddRefed<RawServoDeclarationBlock> aDecls);
  ~CSSStyleRuleDeclaration();

  inline CSSStyleRule* Rule();
  inline const CSSStyleRule* Rule() const;

  RefPtr<DeclarationBlock> mDecls;
};

class CSSStyleRule final : public BindingStyleRule
                         , public SupportsWeakPtr<CSSStyleRule>
{
public:
  CSSStyleRule(already_AddRefed<RawServoStyleRule> aRawRule,
               StyleSheet* aSheet,
               css::Rule* aParentRule,
               uint32_t aLine,
               uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSStyleRule,
                                                         css::Rule)
  bool IsCCLeaf() const final MOZ_MUST_OVERRIDE;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(CSSStyleRule)

  uint32_t GetSelectorCount() override;
  nsresult GetSelectorText(uint32_t aSelectorIndex,
                           nsAString& aText) override;
  nsresult GetSpecificity(uint32_t aSelectorIndex,
                          uint64_t* aSpecificity) override;
  nsresult SelectorMatchesElement(dom::Element* aElement,
                                  uint32_t aSelectorIndex,
                                  const nsAString& aPseudo,
                                  bool* aMatches) override;
  NotNull<DeclarationBlock*> GetDeclarationBlock() const override;

  // WebIDL interface
  uint16_t Type() const final { return dom::CSSRule_Binding::STYLE_RULE; }
  void GetCssText(nsAString& aCssText) const final;
  void GetSelectorText(nsAString& aSelectorText) final;
  void SetSelectorText(const nsAString& aSelectorText) final;
  nsICSSDeclaration* Style() final;

  RawServoStyleRule* Raw() const { return mRawRule; }

  // Methods of mozilla::css::Rule
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

private:
  ~CSSStyleRule() {}

  // For computing the offset of mDecls.
  friend class CSSStyleRuleDeclaration;

  RefPtr<RawServoStyleRule> mRawRule;
  CSSStyleRuleDeclaration mDecls;
};

CSSStyleRule*
CSSStyleRuleDeclaration::Rule()
{
  return reinterpret_cast<CSSStyleRule*>(
    reinterpret_cast<uint8_t*>(this) - offsetof(CSSStyleRule, mDecls));
}

const CSSStyleRule*
CSSStyleRuleDeclaration::Rule() const
{
  return reinterpret_cast<const CSSStyleRule*>(
    reinterpret_cast<const uint8_t*>(this) - offsetof(CSSStyleRule, mDecls));
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_CSSStyleRule_h
