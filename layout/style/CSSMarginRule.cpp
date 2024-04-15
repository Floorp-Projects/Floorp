/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMarginRule.h"
#include "mozilla/dom/CSSMarginRuleBinding.h"

#include "mozilla/DeclarationBlock.h"
#include "mozilla/ServoBindings.h"

namespace mozilla::dom {

// -- CSSMarginRuleDeclaration ---------------------------------------

CSSMarginRuleDeclaration::CSSMarginRuleDeclaration(
    already_AddRefed<StyleLockedDeclarationBlock> aDecls)
    : mDecls(new DeclarationBlock(std::move(aDecls))) {
  mDecls->SetOwningRule(Rule());
}

CSSMarginRuleDeclaration::~CSSMarginRuleDeclaration() {
  mDecls->SetOwningRule(nullptr);
}

// QueryInterface implementation for CSSMarginRuleDeclaration
NS_INTERFACE_MAP_BEGIN(CSSMarginRuleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We forward the cycle collection interfaces to Rule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return Rule()->QueryInterface(aIID, aInstancePtr);
  }
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_ADDREF_USING_AGGREGATOR(CSSMarginRuleDeclaration, Rule())
NS_IMPL_RELEASE_USING_AGGREGATOR(CSSMarginRuleDeclaration, Rule())

/* nsDOMCSSDeclaration implementation */
css::Rule* CSSMarginRuleDeclaration::GetParentRule() { return Rule(); }

nsINode* CSSMarginRuleDeclaration::GetAssociatedNode() const {
  return Rule()->GetAssociatedDocumentOrShadowRoot();
}

nsISupports* CSSMarginRuleDeclaration::GetParentObject() const {
  return Rule()->GetParentObject();
}

DeclarationBlock* CSSMarginRuleDeclaration::GetOrCreateCSSDeclaration(
    Operation aOperation, DeclarationBlock** aCreated) {
  if (aOperation != Operation::Read) {
    if (StyleSheet* sheet = Rule()->GetStyleSheet()) {
      sheet->WillDirty();
    }
  }
  return mDecls;
}

void CSSMarginRuleDeclaration::SetRawAfterClone(
    RefPtr<StyleLockedDeclarationBlock> aDeclarationBlock) {
  mDecls->SetOwningRule(nullptr);
  mDecls = new DeclarationBlock(aDeclarationBlock.forget());
  mDecls->SetOwningRule(Rule());
}

nsresult CSSMarginRuleDeclaration::SetCSSDeclaration(
    DeclarationBlock* aDecl, MutationClosureData* aClosureData) {
  MOZ_ASSERT(aDecl, "must be non-null");
  CSSMarginRule* rule = Rule();

  if (aDecl != mDecls) {
    mDecls->SetOwningRule(nullptr);
    RefPtr<DeclarationBlock> decls = aDecl;
    // TODO alaskanemily: bug 1890418 for implementing this and margin-rule
    // style properties in general.
    // Servo_MarginRule_SetStyle(rule->Raw(), decls->Raw());
    mDecls = std::move(decls);
    mDecls->SetOwningRule(rule);
  }

  return NS_OK;
}

nsDOMCSSDeclaration::ParsingEnvironment
CSSMarginRuleDeclaration::GetParsingEnvironment(
    nsIPrincipal* aSubjectPrincipal) const {
  return GetParsingEnvironmentForRule(Rule(), StyleCssRuleType::Margin);
}

// -- CSSMarginRule --------------------------------------------------

CSSMarginRule::CSSMarginRule(RefPtr<StyleMarginRule> aRawRule,
                             StyleSheet* aSheet, css::Rule* aParentRule,
                             uint32_t aLine, uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(std::move(aRawRule)),
      mDecls(Servo_MarginRule_GetStyle(mRawRule).Consume()) {}

NS_IMPL_ADDREF_INHERITED(CSSMarginRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSMarginRule, css::Rule)

// QueryInterface implementation for MarginRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSMarginRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSMarginRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CSSMarginRule, css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSMarginRule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaration.
  //
  // Note that this has to happen before unlinking css::Rule.
  tmp->UnlinkDeclarationWrapper(tmp->mDecls);
  tmp->mDecls.mDecls->SetOwningRule(nullptr);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(css::Rule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSMarginRule, css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSMarginRule::IsCCLeaf() const {
  if (!Rule::IsCCLeaf()) {
    return false;
  }

  return !mDecls.PreservingWrapper();
}

void CSSMarginRule::SetRawAfterClone(RefPtr<StyleMarginRule> aRaw) {
  mRawRule = std::move(aRaw);
  mDecls.SetRawAfterClone(Servo_MarginRule_GetStyle(mRawRule.get()).Consume());
}

// WebIDL interfaces
StyleCssRuleType CSSMarginRule::Type() const {
  return StyleCssRuleType::Margin;
}

// CSSRule implementation

void CSSMarginRule::GetCssText(nsACString& aCssText) const {
  Servo_MarginRule_GetCssText(mRawRule, &aCssText);
}

void CSSMarginRule::GetName(nsACString& aRuleName) const {
  Servo_MarginRule_GetName(mRawRule, &aRuleName);
}

size_t CSSMarginRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  // TODO Implement this!
  return aMallocSizeOf(this);
}

#ifdef DEBUG
void CSSMarginRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_MarginRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

JSObject* CSSMarginRule::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return CSSMarginRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
