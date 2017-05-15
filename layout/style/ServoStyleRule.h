/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSStyleRule for stylo */

#ifndef mozilla_ServoStyleRule_h
#define mozilla_ServoStyleRule_h

#include "mozilla/BindingStyleRule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsIDOMCSSStyleRule.h"
#include "nsDOMCSSDeclaration.h"

namespace mozilla {

class ServoDeclarationBlock;
class ServoStyleRule;

class ServoStyleRuleDeclaration final : public nsDOMCSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetParentRule(nsIDOMCSSRule** aParent) final;
  nsINode* GetParentObject() final;

protected:
  DeclarationBlock* GetCSSDeclaration(Operation aOperation) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl) final;
  nsIDocument* DocToUpdate() final;
  void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv) final;
  ServoCSSParsingEnvironment GetServoCSSParsingEnvironment() const final;

private:
  // For accessing the constructor.
  friend class ServoStyleRule;

  explicit ServoStyleRuleDeclaration(
    already_AddRefed<RawServoDeclarationBlock> aDecls);
  ~ServoStyleRuleDeclaration();

  inline ServoStyleRule* Rule();
  inline const ServoStyleRule* Rule() const;

  RefPtr<ServoDeclarationBlock> mDecls;
};

class ServoStyleRule final : public BindingStyleRule
                           , public nsIDOMCSSStyleRule
{
public:
  ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule,
                 uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ServoStyleRule,
                                                         css::Rule)
  virtual bool IsCCLeaf() const final MOZ_MUST_OVERRIDE;
  NS_DECL_NSIDOMCSSSTYLERULE

  // WebIDL interface
  uint16_t Type() const final;
  void GetCssTextImpl(nsAString& aCssText) const final;
  virtual nsICSSDeclaration* Style() final;

  RawServoStyleRule* Raw() const { return mRawRule; }

  // Methods of mozilla::css::Rule
  int32_t GetType() const final { return css::Rule::STYLE_RULE; }
  using Rule::GetType;
  already_AddRefed<Rule> Clone() const final;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

private:
  ~ServoStyleRule() {}

  // For computing the offset of mDecls.
  friend class ServoStyleRuleDeclaration;

  RefPtr<RawServoStyleRule> mRawRule;
  ServoStyleRuleDeclaration mDecls;
};

ServoStyleRule*
ServoStyleRuleDeclaration::Rule()
{
  return reinterpret_cast<ServoStyleRule*>(
    reinterpret_cast<uint8_t*>(this) - offsetof(ServoStyleRule, mDecls));
}

const ServoStyleRule*
ServoStyleRuleDeclaration::Rule() const
{
  return reinterpret_cast<const ServoStyleRule*>(
    reinterpret_cast<const uint8_t*>(this) - offsetof(ServoStyleRule, mDecls));
}

} // namespace mozilla

#endif // mozilla_ServoStyleRule_h
