/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSPageRule for stylo */

#ifndef mozilla_ServoPageRule_h
#define mozilla_ServoPageRule_h

#include "mozilla/dom/CSSPageRule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsDOMCSSDeclaration.h"

namespace mozilla {

namespace dom {
class DocGroup;
} // namespace dom

class ServoDeclarationBlock;
class ServoPageRule;

class ServoPageRuleDeclaration final : public nsDOMCSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetParentRule(nsIDOMCSSRule** aParent) final;
  nsINode* GetParentObject() final;
  DocGroup* GetDocGroup() const final;

protected:
  DeclarationBlock* GetCSSDeclaration(Operation aOperation) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl) final;
  nsIDocument* DocToUpdate() final;
  void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv,
                                nsIPrincipal* aSubjectPrincipal) final;
  nsDOMCSSDeclaration::ServoCSSParsingEnvironment
  GetServoCSSParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final;

private:
  // For accessing the constructor.
  friend class ServoPageRule;

  explicit ServoPageRuleDeclaration(
    already_AddRefed<RawServoDeclarationBlock> aDecls);
  ~ServoPageRuleDeclaration();

  inline ServoPageRule* Rule();
  inline const ServoPageRule* Rule() const;

  RefPtr<ServoDeclarationBlock> mDecls;
};

class ServoPageRule final : public dom::CSSPageRule
{
public:
  ServoPageRule(RefPtr<RawServoPageRule> aRawRule,
                uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
    ServoPageRule, dom::CSSPageRule
  )
  bool IsCCLeaf() const final;

  RawServoPageRule* Raw() const { return mRawRule; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const final;
  nsICSSDeclaration* Style() final;

  // Methods of mozilla::css::Rule
  already_AddRefed<css::Rule> Clone() const final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

private:
  virtual ~ServoPageRule();

  // For computing the offset of mDecls.
  friend class ServoPageRuleDeclaration;

  RefPtr<RawServoPageRule> mRawRule;
  ServoPageRuleDeclaration mDecls;
};

ServoPageRule*
ServoPageRuleDeclaration::Rule()
{
  return reinterpret_cast<ServoPageRule*>(
    reinterpret_cast<uint8_t*>(this) - offsetof(ServoPageRule, mDecls));
}

const ServoPageRule*
ServoPageRuleDeclaration::Rule() const
{
  return reinterpret_cast<const ServoPageRule*>(
    reinterpret_cast<const uint8_t*>(this) - offsetof(ServoPageRule, mDecls));
}

} // namespace mozilla

#endif // mozilla_ServoPageRule_h
