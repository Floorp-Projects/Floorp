/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSStyleRule_h
#define mozilla_CSSStyleRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/NotNull.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/CSSStyleRuleBinding.h"

#include "nsDOMCSSDeclaration.h"

namespace mozilla {

class DeclarationBlock;

namespace dom {
class DocGroup;
class CSSStyleRule;

class CSSStyleRuleDeclaration final : public nsDOMCSSDeclaration {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetAssociatedNode() const final;
  nsISupports* GetParentObject() const final;

 protected:
  mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, mozilla::DeclarationBlock** aCreated) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl,
                             MutationClosureData* aClosureData) final;
  Document* DocToUpdate() final;
  ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

 private:
  // For accessing the constructor.
  friend class CSSStyleRule;

  explicit CSSStyleRuleDeclaration(
      already_AddRefed<StyleLockedDeclarationBlock> aDecls);
  ~CSSStyleRuleDeclaration();

  inline CSSStyleRule* Rule();
  inline const CSSStyleRule* Rule() const;

  void SetRawAfterClone(RefPtr<StyleLockedDeclarationBlock>);

  RefPtr<DeclarationBlock> mDecls;
};

class CSSStyleRule final : public css::GroupRule, public SupportsWeakPtr {
 public:
  CSSStyleRule(already_AddRefed<StyleLockedStyleRule> aRawRule,
               StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
               uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSStyleRule,
                                                         css::GroupRule)
  bool IsCCLeaf() const final MOZ_MUST_OVERRIDE;

  uint32_t SelectorCount() const;
  void SelectorTextAt(uint32_t aSelectorIndex, bool aDesugared,
                      nsACString& aText);
  uint64_t SelectorSpecificityAt(uint32_t aSelectorIndex, bool aDesugared);
  bool SelectorMatchesElement(uint32_t aSelectorIndex, dom::Element&,
                              const nsAString& aPseudo,
                              bool aRelevantLinkVisited);
  NotNull<DeclarationBlock*> GetDeclarationBlock() const;
  void GetSelectorWarnings(nsTArray<SelectorWarning>& aResult) const;

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  void GetSelectorText(nsACString& aSelectorText);
  void SetSelectorText(const nsACString& aSelectorText);
  nsICSSDeclaration* Style();

  StyleLockedStyleRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleLockedStyleRule>);
  already_AddRefed<StyleLockedCssRules> GetOrCreateRawRules() final;

  // Methods of mozilla::css::Rule
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~CSSStyleRule() = default;

  void GetSelectorDataAtIndex(uint32_t aSelectorIndex, bool aDesugared,
                              nsACString* aText, uint64_t* aSpecificity);

  // For computing the offset of mDecls.
  friend class CSSStyleRuleDeclaration;

  RefPtr<StyleLockedStyleRule> mRawRule;
  CSSStyleRuleDeclaration mDecls;
};

CSSStyleRule* CSSStyleRuleDeclaration::Rule() {
  return reinterpret_cast<CSSStyleRule*>(reinterpret_cast<uint8_t*>(this) -
                                         offsetof(CSSStyleRule, mDecls));
}

const CSSStyleRule* CSSStyleRuleDeclaration::Rule() const {
  return reinterpret_cast<const CSSStyleRule*>(
      reinterpret_cast<const uint8_t*>(this) - offsetof(CSSStyleRule, mDecls));
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_CSSStyleRule_h
