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

#include "nsDOMClassInfoID.h"
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

NS_IMETHODIMP
ServoStyleRuleDeclaration::GetParentRule(nsIDOMCSSRule** aParent)
{
  *aParent = do_AddRef(Rule()).take();
  return NS_OK;
}

nsINode*
ServoStyleRuleDeclaration::GetParentObject()
{
  return Rule()->GetDocument();
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
    nsCOMPtr<nsIDocument> owningDoc = sheet->GetOwningDocument();
    mozAutoDocUpdate updateBatch(owningDoc, UPDATE_STYLE, true);
    if (aDecl != mDecls) {
      RefPtr<ServoDeclarationBlock> decls = aDecl->AsServo();
      Servo_StyleRule_SetStyle(rule->Raw(), decls->Raw());
      mDecls = decls.forget();
    }
    if (owningDoc) {
      owningDoc->StyleRuleChanged(sheet, rule);
    }
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
  CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(Rule(), aCSSParseEnv);
}

// -- ServoStyleRule --------------------------------------------------

ServoStyleRule::ServoStyleRule(already_AddRefed<RawServoStyleRule> aRawRule)
  : css::Rule(0, 0)
  , mRawRule(aRawRule)
  , mDecls(Servo_StyleRule_GetStyle(mRawRule).Consume())
{
}

// QueryInterface implementation for ServoStyleRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServoStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, css::Rule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSStyleRule)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ServoStyleRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ServoStyleRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoStyleRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ServoStyleRule)
  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoStyleRule)
  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.ReleaseWrapper(static_cast<nsISupports*>(p));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ServoStyleRule)
  // Just NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS here: that will call
  // into our Trace hook, where we do the right thing with declarations
  // already.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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
  // TODO Implement this!
  return aMallocSizeOf(this);
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

NS_IMETHODIMP
ServoStyleRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetCssText(nsAString& aCssText)
{
  Servo_StyleRule_GetCssText(mRawRule, &aCssText);
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::SetCssText(const nsAString& aCssText)
{
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return css::Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
ServoStyleRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  *aParentRule = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

css::Rule*
ServoStyleRule::GetCSSRule()
{
  return this;
}

/* CSSStyleRule implementation */

NS_IMETHODIMP
ServoStyleRule::GetSelectorText(nsAString& aSelectorText)
{
  Servo_StyleRule_GetSelectorText(mRawRule, &aSelectorText);
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::SetSelectorText(const nsAString& aSelectorText)
{
  // XXX We need to implement this... But Gecko doesn't have this either
  //     so it's probably okay to leave it unimplemented currently?
  //     See bug 37468 and mozilla::css::StyleRule::SetSelectorText.
  return NS_OK;
}

NS_IMETHODIMP
ServoStyleRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  *aStyle = do_AddRef(&mDecls).take();
  return NS_OK;
}

} // namespace mozilla
