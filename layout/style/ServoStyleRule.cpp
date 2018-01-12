/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of CSSStyleRule for stylo */

#include "mozilla/ServoStyleRule.h"

#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoDeclarationBlock.h"
#include "mozilla/dom/CSSStyleRuleBinding.h"
#include "nsCSSPseudoClasses.h"

#include "mozAutoDocUpdate.h"

namespace mozilla {

// -- ServoStyleRuleDeclaration ---------------------------------------

ServoStyleRuleDeclaration::ServoStyleRuleDeclaration(
  already_AddRefed<RawServoDeclarationBlock> aDecls)
  : mDecls(new ServoDeclarationBlock(Move(aDecls)))
{
}

ServoStyleRuleDeclaration::~ServoStyleRuleDeclaration()
{
  mDecls->SetOwningRule(nullptr);
}

// QueryInterface implementation for ServoStyleRuleDeclaration
NS_INTERFACE_MAP_BEGIN(ServoStyleRuleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We forward the cycle collection interfaces to Rule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return Rule()->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_IMPL_QUERY_TAIL_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_ADDREF_USING_AGGREGATOR(ServoStyleRuleDeclaration, Rule())
NS_IMPL_RELEASE_USING_AGGREGATOR(ServoStyleRuleDeclaration, Rule())

/* nsDOMCSSDeclaration implementation */

css::Rule*
ServoStyleRuleDeclaration::GetParentRule()
{
  return Rule();
}

nsINode*
ServoStyleRuleDeclaration::GetParentObject()
{
  return Rule()->GetDocument();
}

DocGroup*
ServoStyleRuleDeclaration::GetDocGroup() const
{
  nsIDocument* document = Rule()->GetDocument();
  return document ? document->GetDocGroup() : nullptr;
}

DeclarationBlock*
ServoStyleRuleDeclaration::GetCSSDeclaration(Operation aOperation)
{
  return mDecls;
}

nsresult
ServoStyleRuleDeclaration::SetCSSDeclaration(DeclarationBlock* aDecl)
{
  ServoStyleRule* rule = Rule();
  if (RefPtr<ServoStyleSheet> sheet = rule->GetStyleSheet()->AsServo()) {
    nsCOMPtr<nsIDocument> doc = sheet->GetAssociatedDocument();
    mozAutoDocUpdate updateBatch(doc, UPDATE_STYLE, true);
    if (aDecl != mDecls) {
      mDecls->SetOwningRule(nullptr);
      RefPtr<ServoDeclarationBlock> decls = aDecl->AsServo();
      Servo_StyleRule_SetStyle(rule->Raw(), decls->Raw());
      mDecls = decls.forget();
      mDecls->SetOwningRule(rule);
    }
    sheet->RuleChanged(rule);
  }
  return NS_OK;
}

nsIDocument*
ServoStyleRuleDeclaration::DocToUpdate()
{
  return nullptr;
}

void
ServoStyleRuleDeclaration::GetCSSParsingEnvironment(
  CSSParsingEnvironment& aCSSParseEnv,
  nsIPrincipal* aSubjectPrincipal)
{
  MOZ_ASSERT_UNREACHABLE("GetCSSParsingEnvironment "
                         "shouldn't be calling for a Servo rule");
  GetCSSParsingEnvironmentForRule(Rule(), aCSSParseEnv);
}

nsDOMCSSDeclaration::ServoCSSParsingEnvironment
ServoStyleRuleDeclaration::GetServoCSSParsingEnvironment(
  nsIPrincipal* aSubjectPrincipal) const
{
  return GetServoCSSParsingEnvironmentForRule(Rule());
}

// -- ServoStyleRule --------------------------------------------------

ServoStyleRule::ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule,
                               uint32_t aLine, uint32_t aColumn)
  : BindingStyleRule(aLine, aColumn)
  , mRawRule(aRawRule)
  , mDecls(Servo_StyleRule_GetStyle(mRawRule).Consume())
{
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ServoStyleRule, css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoStyleRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ServoStyleRule, css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServoStyleRule, css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.ReleaseWrapper(static_cast<nsISupports*>(p));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoStyleRule, css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
ServoStyleRule::IsCCLeaf() const
{
  if (!Rule::IsCCLeaf()) {
    return false;
  }

  return !mDecls.PreservingWrapper();
}

already_AddRefed<css::Rule>
ServoStyleRule::Clone() const
{
  // Rule::Clone is only used when CSSStyleSheetInner is cloned in
  // preparation of being mutated. However, ServoStyleSheet never clones
  // anything, so this method should never be called.
  MOZ_ASSERT_UNREACHABLE("Shouldn't be cloning ServoStyleRule");
  return nullptr;
}

size_t
ServoStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mRawRule
  // - mDecls

  return n;
}

#ifdef DEBUG
void
ServoStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_StyleRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

/* CSSRule implementation */

uint16_t
ServoStyleRule::Type() const
{
  return CSSRuleBinding::STYLE_RULE;
}

void
ServoStyleRule::GetCssTextImpl(nsAString& aCssText) const
{
  Servo_StyleRule_GetCssText(mRawRule, &aCssText);
}

nsICSSDeclaration*
ServoStyleRule::Style()
{
  return &mDecls;
}

/* CSSStyleRule implementation */

void
ServoStyleRule::GetSelectorText(nsAString& aSelectorText)
{
  Servo_StyleRule_GetSelectorText(mRawRule, &aSelectorText);
}

void
ServoStyleRule::SetSelectorText(const nsAString& aSelectorText)
{
  // XXX We need to implement this... But Gecko doesn't have this either
  //     so it's probably okay to leave it unimplemented currently?
  //     See bug 37468 and mozilla::css::StyleRule::SetSelectorText.
}

uint32_t
ServoStyleRule::GetSelectorCount()
{
  uint32_t aCount;
  Servo_StyleRule_GetSelectorCount(mRawRule, &aCount);

  return aCount;
}

nsresult
ServoStyleRule::GetSelectorText(uint32_t aSelectorIndex, nsAString& aText)
{
  Servo_StyleRule_GetSelectorTextAtIndex(mRawRule, aSelectorIndex, &aText);
  return NS_OK;
}

nsresult
ServoStyleRule::GetSpecificity(uint32_t aSelectorIndex, uint64_t* aSpecificity)
{
  Servo_StyleRule_GetSpecificityAtIndex(mRawRule, aSelectorIndex, aSpecificity);
  return NS_OK;
}

nsresult
ServoStyleRule::SelectorMatchesElement(Element* aElement,
                                       uint32_t aSelectorIndex,
                                       const nsAString& aPseudo,
                                       bool* aMatches)
{
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  if (!aPseudo.IsEmpty()) {
    RefPtr<nsAtom> pseudoElt = NS_Atomize(aPseudo);
    pseudoType = nsCSSPseudoElements::GetPseudoType(
        pseudoElt, CSSEnabledState::eIgnoreEnabledState);

    if (pseudoType == CSSPseudoElementType::NotPseudo) {
      *aMatches = false;
      return NS_OK;
    }
  }

  *aMatches = Servo_StyleRule_SelectorMatchesElement(mRawRule, aElement,
                                                     aSelectorIndex, pseudoType);

  return NS_OK;
}

NotNull<DeclarationBlock*>
ServoStyleRule::GetDeclarationBlock() const
{
  return WrapNotNull(mDecls.mDecls);
}

} // namespace mozilla
