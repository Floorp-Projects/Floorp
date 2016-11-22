/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSStyleRule for stylo */

#ifndef mozilla_ServoStyleRule_h
#define mozilla_ServoStyleRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsIDOMCSSStyleRule.h"
#include "nsDOMCSSDeclaration.h"

namespace mozilla {

class ServoDeclarationBlock;

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

private:
  // For accessing the constructor.
  friend class ServoStyleRule;

  explicit ServoStyleRuleDeclaration(
    already_AddRefed<RawServoDeclarationBlock> aDecls);
  ~ServoStyleRuleDeclaration();

  inline ServoStyleRule* Rule();

  RefPtr<ServoDeclarationBlock> mDecls;
};

class ServoStyleRule final : public css::Rule
                           , public nsIDOMCSSStyleRule
{
public:
  explicit ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(ServoStyleRule,
                                                         css::Rule)
  NS_DECL_NSIDOMCSSRULE
  NS_DECL_NSIDOMCSSSTYLERULE

  RawServoStyleRule* Raw() const { return mRawRule; }

  // Methods of mozilla::css::Rule
  int32_t GetType() const final { return css::Rule::STYLE_RULE; }
  already_AddRefed<Rule> Clone() const final;
  nsIDOMCSSRule* GetDOMRule() final { return this; }
  nsIDOMCSSRule* GetExistingDOMRule() final { return this; }
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
  return reinterpret_cast<ServoStyleRule*>(reinterpret_cast<uint8_t*>(this) -
                                           offsetof(ServoStyleRule, mDecls));
}

} // namespace mozilla

#endif // mozilla_ServoStyleRule_h
