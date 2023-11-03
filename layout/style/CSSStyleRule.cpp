/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSStyleRule.h"

#include "mozilla/CSSEnabledState.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/PseudoStyleType.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsCSSPseudoElements.h"

#include "mozAutoDocUpdate.h"
#include "nsISupports.h"

namespace mozilla::dom {

// -- CSSStyleRuleDeclaration ---------------------------------------

CSSStyleRuleDeclaration::CSSStyleRuleDeclaration(
    already_AddRefed<StyleLockedDeclarationBlock> aDecls)
    : mDecls(new DeclarationBlock(std::move(aDecls))) {
  mDecls->SetOwningRule(Rule());
}

CSSStyleRuleDeclaration::~CSSStyleRuleDeclaration() {
  mDecls->SetOwningRule(nullptr);
}

// QueryInterface implementation for CSSStyleRuleDeclaration
NS_INTERFACE_MAP_BEGIN(CSSStyleRuleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We forward the cycle collection interfaces to Rule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return Rule()->QueryInterface(aIID, aInstancePtr);
  }
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_ADDREF_USING_AGGREGATOR(CSSStyleRuleDeclaration, Rule())
NS_IMPL_RELEASE_USING_AGGREGATOR(CSSStyleRuleDeclaration, Rule())

/* nsDOMCSSDeclaration implementation */

css::Rule* CSSStyleRuleDeclaration::GetParentRule() { return Rule(); }

nsINode* CSSStyleRuleDeclaration::GetAssociatedNode() const {
  return Rule()->GetAssociatedDocumentOrShadowRoot();
}

nsISupports* CSSStyleRuleDeclaration::GetParentObject() const {
  return Rule()->GetParentObject();
}

DeclarationBlock* CSSStyleRuleDeclaration::GetOrCreateCSSDeclaration(
    Operation aOperation, DeclarationBlock** aCreated) {
  if (aOperation != Operation::Read) {
    if (StyleSheet* sheet = Rule()->GetStyleSheet()) {
      sheet->WillDirty();
    }
  }
  return mDecls;
}

void CSSStyleRule::SetRawAfterClone(RefPtr<StyleLockedStyleRule> aRaw) {
  mRawRule = std::move(aRaw);
  mDecls.SetRawAfterClone(Servo_StyleRule_GetStyle(mRawRule).Consume());
  GroupRule::DidSetRawAfterClone();
}

already_AddRefed<StyleLockedCssRules> CSSStyleRule::GetOrCreateRawRules() {
  return Servo_StyleRule_EnsureRules(mRawRule, IsReadOnly()).Consume();
}

void CSSStyleRuleDeclaration::SetRawAfterClone(
    RefPtr<StyleLockedDeclarationBlock> aRaw) {
  RefPtr<DeclarationBlock> block = new DeclarationBlock(aRaw.forget());
  mDecls->SetOwningRule(nullptr);
  mDecls = std::move(block);
  mDecls->SetOwningRule(Rule());
}

nsresult CSSStyleRuleDeclaration::SetCSSDeclaration(
    DeclarationBlock* aDecl, MutationClosureData* aClosureData) {
  CSSStyleRule* rule = Rule();

  if (StyleSheet* sheet = rule->GetStyleSheet()) {
    if (aDecl != mDecls) {
      mDecls->SetOwningRule(nullptr);
      RefPtr<DeclarationBlock> decls = aDecl;
      Servo_StyleRule_SetStyle(rule->Raw(), decls->Raw());
      mDecls = std::move(decls);
      mDecls->SetOwningRule(rule);
    }
    sheet->RuleChanged(rule, StyleRuleChangeKind::StyleRuleDeclarations);
  }
  return NS_OK;
}

Document* CSSStyleRuleDeclaration::DocToUpdate() { return nullptr; }

nsDOMCSSDeclaration::ParsingEnvironment
CSSStyleRuleDeclaration::GetParsingEnvironment(
    nsIPrincipal* aSubjectPrincipal) const {
  return GetParsingEnvironmentForRule(Rule(), StyleCssRuleType::Style);
}

// -- CSSStyleRule --------------------------------------------------

CSSStyleRule::CSSStyleRule(already_AddRefed<StyleLockedStyleRule> aRawRule,
                           StyleSheet* aSheet, css::Rule* aParentRule,
                           uint32_t aLine, uint32_t aColumn)
    : GroupRule(aSheet, aParentRule, aLine, aColumn),
      mRawRule(aRawRule),
      mDecls(Servo_StyleRule_GetStyle(mRawRule).Consume()) {}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(CSSStyleRule, GroupRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSStyleRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CSSStyleRule, GroupRule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecls.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSStyleRule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaration.
  //
  // Note that this has to happen before unlinking css::Rule.
  tmp->UnlinkDeclarationWrapper(tmp->mDecls);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(GroupRule)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSStyleRule, GroupRule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool CSSStyleRule::IsCCLeaf() const {
  if (!GroupRule::IsCCLeaf()) {
    return false;
  }
  return !mDecls.PreservingWrapper();
}

size_t CSSStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mRawRule
  // - mDecls

  return n;
}

#ifdef DEBUG
void CSSStyleRule::List(FILE* out, int32_t aIndent) const {
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_StyleRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

/* CSSRule implementation */

StyleCssRuleType CSSStyleRule::Type() const { return StyleCssRuleType::Style; }

void CSSStyleRule::GetCssText(nsACString& aCssText) const {
  Servo_StyleRule_GetCssText(mRawRule, &aCssText);
}

nsICSSDeclaration* CSSStyleRule::Style() { return &mDecls; }

/* CSSStyleRule implementation */

void CSSStyleRule::GetSelectorText(nsACString& aSelectorText) {
  Servo_StyleRule_GetSelectorText(mRawRule, &aSelectorText);
}

void CSSStyleRule::SetSelectorText(const nsACString& aSelectorText) {
  if (IsReadOnly()) {
    return;
  }

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->WillDirty();

    // TODO(emilio): May actually be more efficient to handle this as rule
    // removal + addition, from the point of view of invalidation...
    const StyleStylesheetContents* contents = sheet->RawContents();
    if (Servo_StyleRule_SetSelectorText(contents, mRawRule, &aSelectorText)) {
      sheet->RuleChanged(this, StyleRuleChangeKind::Generic);
    }
  }
}

uint32_t CSSStyleRule::SelectorCount() const {
  return Servo_StyleRule_GetSelectorCount(mRawRule);
}

static void CollectStyleRules(CSSStyleRule& aDeepestRule, bool aDesugared,
                              nsTArray<const StyleLockedStyleRule*>& aResult) {
  aResult.AppendElement(aDeepestRule.Raw());
  if (aDesugared) {
    for (auto* rule = aDeepestRule.GetParentRule(); rule;
         rule = rule->GetParentRule()) {
      if (rule->Type() == StyleCssRuleType::Style) {
        aResult.AppendElement(static_cast<CSSStyleRule*>(rule)->Raw());
      }
    }
  }
}

void CSSStyleRule::GetSelectorDataAtIndex(uint32_t aSelectorIndex,
                                          bool aDesugared, nsACString* aText,
                                          uint64_t* aSpecificity) {
  AutoTArray<const StyleLockedStyleRule*, 8> rules;
  CollectStyleRules(*this, aDesugared, rules);
  Servo_StyleRule_GetSelectorDataAtIndex(&rules, aSelectorIndex, aText,
                                         aSpecificity);
}

void CSSStyleRule::SelectorTextAt(uint32_t aSelectorIndex, bool aDesugared,
                                  nsACString& aText) {
  GetSelectorDataAtIndex(aSelectorIndex, aDesugared, &aText, nullptr);
}

uint64_t CSSStyleRule::SelectorSpecificityAt(uint32_t aSelectorIndex,
                                             bool aDesugared) {
  uint64_t s = 0;
  GetSelectorDataAtIndex(aSelectorIndex, aDesugared, nullptr, &s);
  return s;
}

bool CSSStyleRule::SelectorMatchesElement(uint32_t aSelectorIndex,
                                          Element& aElement,
                                          const nsAString& aPseudo,
                                          bool aRelevantLinkVisited) {
  Maybe<PseudoStyleType> pseudoType = nsCSSPseudoElements::GetPseudoType(
      aPseudo, CSSEnabledState::IgnoreEnabledState);
  if (!pseudoType) {
    return false;
  }

  auto* host = [&]() -> Element* {
    auto* sheet = GetStyleSheet();
    if (!sheet) {
      return nullptr;
    }
    if (auto* owner = sheet->GetAssociatedDocumentOrShadowRoot()) {
      if (auto* shadow = ShadowRoot::FromNode(owner->AsNode())) {
        return shadow->Host();
      }
    }
    for (auto* adopter : sheet->SelfOrAncestorAdopters()) {
      // Try to guess. This is not fully correct but it's the best we can do
      // with the info at hand...
      auto* shadow = ShadowRoot::FromNode(adopter->AsNode());
      if (!shadow) {
        continue;
      }
      if (shadow->Host() == &aElement ||
          shadow == aElement.GetContainingShadow()) {
        return shadow->Host();
      }
    }
    return nullptr;
  }();

  AutoTArray<const StyleLockedStyleRule*, 8> rules;
  CollectStyleRules(*this, /* aDesugared = */ true, rules);

  return Servo_StyleRule_SelectorMatchesElement(
      &rules, &aElement, aSelectorIndex, host, *pseudoType,
      aRelevantLinkVisited);
}

NotNull<DeclarationBlock*> CSSStyleRule::GetDeclarationBlock() const {
  return WrapNotNull(mDecls.mDecls);
}

SelectorWarningKind ToWebIDLSelectorWarningKind(
    StyleSelectorWarningKind aKind) {
  switch (aKind) {
    case StyleSelectorWarningKind::UnconstraintedRelativeSelector:
      return SelectorWarningKind::UnconstrainedHas;
  }
  MOZ_ASSERT_UNREACHABLE("Unhandled selector warning kind");
  // Return something for assert-disabled builds.
  return SelectorWarningKind::UnconstrainedHas;
}

void CSSStyleRule::GetSelectorWarnings(
    nsTArray<SelectorWarning>& aResult) const {
  nsTArray<StyleSelectorWarningData> result;
  Servo_GetSelectorWarnings(mRawRule, &result);
  for (const auto& warning : result) {
    auto& entry = *aResult.AppendElement();
    entry.mIndex = warning.index;
    entry.mKind = ToWebIDLSelectorWarningKind(warning.kind);
  }
}

/* virtual */
JSObject* CSSStyleRule::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return CSSStyleRule_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
