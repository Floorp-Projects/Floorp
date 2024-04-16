/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSMarginRule_h
#define mozilla_dom_CSSMarginRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsDOMCSSDeclaration.h"
#include "nsICSSDeclaration.h"

namespace mozilla {
class DeclarationBlock;

namespace dom {
class CSSMarginRule;

class CSSMarginRuleDeclaration final : public nsDOMCSSDeclaration {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetAssociatedNode() const final;
  nsISupports* GetParentObject() const final;

 protected:
  DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, DeclarationBlock** aCreated) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl,
                             MutationClosureData* aClosureData) final;
  Document* DocToUpdate() final { return nullptr; }
  nsDOMCSSDeclaration::ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal) const final;

 private:
  // For accessing the constructor.
  friend class CSSMarginRule;

  explicit CSSMarginRuleDeclaration(
      already_AddRefed<StyleLockedDeclarationBlock> aDecls);
  void SetRawAfterClone(RefPtr<StyleLockedDeclarationBlock>);

  ~CSSMarginRuleDeclaration();

  inline CSSMarginRule* Rule();
  inline const CSSMarginRule* Rule() const;

  RefPtr<DeclarationBlock> mDecls;
};

class CSSMarginRule final : public css::Rule {
 public:
  CSSMarginRule(RefPtr<StyleMarginRule> aRawRule, StyleSheet* aSheet,
                css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSMarginRule,
                                                         css::Rule)

  bool IsCCLeaf() const final;

  StyleMarginRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleMarginRule>);

  // WebIDL interfaces
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  nsICSSDeclaration* Style() { return &mDecls; }

  void GetName(nsACString& aRuleName) const;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 private:
  ~CSSMarginRule() = default;

  // For computing the offset of mDecls.
  friend class CSSMarginRuleDeclaration;

  RefPtr<StyleMarginRule> mRawRule;
  CSSMarginRuleDeclaration mDecls;
};

CSSMarginRule* CSSMarginRuleDeclaration::Rule() {
  return reinterpret_cast<CSSMarginRule*>(reinterpret_cast<uint8_t*>(this) -
                                          offsetof(CSSMarginRule, mDecls));
}

const CSSMarginRule* CSSMarginRuleDeclaration::Rule() const {
  return reinterpret_cast<const CSSMarginRule*>(
      reinterpret_cast<const uint8_t*>(this) - offsetof(CSSMarginRule, mDecls));
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CSSMarginRule_h
