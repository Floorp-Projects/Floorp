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
#include "mozilla/WeakPtr.h"

#include "nsDOMCSSDeclaration.h"

namespace mozilla {

namespace dom {
class DocGroup;
} // namespace dom

class ServoDeclarationBlock;
class ServoStyleRule;

class ServoStyleRuleDeclaration final : public nsDOMCSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  css::Rule* GetParentRule() final;
  nsINode* GetParentObject() final;

protected:
  DeclarationBlock* GetCSSDeclaration(Operation aOperation) final;
  nsresult SetCSSDeclaration(DeclarationBlock* aDecl) final;
  nsIDocument* DocToUpdate() final;
  ParsingEnvironment
  GetParsingEnvironment(nsIPrincipal* aSubjectPrincipal) const final;

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
                           , public SupportsWeakPtr<ServoStyleRule>
{
public:
  ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule,
                 uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ServoStyleRule,
                                                         css::Rule)
  bool IsCCLeaf() const final MOZ_MUST_OVERRIDE;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(ServoStyleRule)

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
  uint16_t Type() const final { return dom::CSSRuleBinding::STYLE_RULE; }
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
