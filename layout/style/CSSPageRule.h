/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSPageRule_h
#define mozilla_dom_CSSPageRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsDOMCSSDeclaration.h"
#include "nsICSSDeclaration.h"

namespace mozilla {
class DeclarationBlock;

namespace dom {
class DocGroup;
class CSSPageRule;

class CSSPageRuleDeclaration final : public nsDOMCSSDeclaration
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
  nsDOMCSSDeclaration::ParsingEnvironment
  GetParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final;

private:
  // For accessing the constructor.
  friend class CSSPageRule;

  explicit CSSPageRuleDeclaration(
    already_AddRefed<RawServoDeclarationBlock> aDecls);
  ~CSSPageRuleDeclaration();

  inline CSSPageRule* Rule();
  inline const CSSPageRule* Rule() const;

  RefPtr<DeclarationBlock> mDecls;
};

class CSSPageRule final : public css::Rule
{
public:
  CSSPageRule(RefPtr<RawServoPageRule> aRawRule,
              StyleSheet* aSheet,
              css::Rule* aParentRule,
              uint32_t aLine,
              uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
    CSSPageRule, css::Rule
  )

  bool IsCCLeaf() const final;

  RawServoPageRule* Raw() const { return mRawRule; }

  // WebIDL interfaces
  uint16_t Type() const final { return CSSRule_Binding::PAGE_RULE; }
  void GetCssText(nsAString& aCssText) const final;
  nsICSSDeclaration* Style();

  size_t
  SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

private:
  ~CSSPageRule() = default;

  // For computing the offset of mDecls.
  friend class CSSPageRuleDeclaration;

  RefPtr<RawServoPageRule> mRawRule;
  CSSPageRuleDeclaration mDecls;
};

CSSPageRule*
CSSPageRuleDeclaration::Rule()
{
  return reinterpret_cast<CSSPageRule*>(
    reinterpret_cast<uint8_t*>(this) - offsetof(CSSPageRule, mDecls));
}

const CSSPageRule*
CSSPageRuleDeclaration::Rule() const
{
  return reinterpret_cast<const CSSPageRule*>(
    reinterpret_cast<const uint8_t*>(this) - offsetof(CSSPageRule, mDecls));
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSPageRule_h
