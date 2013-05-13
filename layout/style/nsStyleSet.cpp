/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * the container for the style sheets that apply to a presentation, and
 * the internal API that the style system exposes for creating (and
 * potentially re-creating) style contexts
 */

#include "mozilla/Util.h"

#include "nsStyleSet.h"
#include "nsCSSStyleSheet.h"
#include "nsIDocumentInlines.h"
#include "nsRuleWalker.h"
#include "nsStyleContext.h"
#include "mozilla/css/StyleRule.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsDataHashtable.h"
#include "nsIContent.h"
#include "nsRuleData.h"
#include "nsRuleProcessorData.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "nsEventStates.h"
#include "nsStyleSheetService.h"
#include "mozilla/dom/Element.h"
#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(nsEmptyStyleRule, nsIStyleRule)

/* virtual */ void
nsEmptyStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
}

#ifdef DEBUG
/* virtual */ void
nsEmptyStyleRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[empty style rule] {}\n", out);
}
#endif

NS_IMPL_ISUPPORTS1(nsInitialStyleRule, nsIStyleRule)

/* virtual */ void
nsInitialStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // Iterate over the property groups
  for (nsStyleStructID sid = nsStyleStructID(0);
       sid < nsStyleStructID_Length; sid = nsStyleStructID(sid + 1)) {
    if (aRuleData->mSIDs & (1 << sid)) {
      // Iterate over nsCSSValues within the property group
      nsCSSValue * const value_start =
        aRuleData->mValueStorage + aRuleData->mValueOffsets[sid];
      for (nsCSSValue *value = value_start,
           *value_end = value + nsCSSProps::PropertyCountInStruct(sid);
           value != value_end; ++value) {
        // If MathML is disabled take care not to set MathML properties (or we
        // will trigger assertions in nsRuleNode)
        if (sid == eStyleStruct_Font &&
            !aRuleData->mPresContext->Document()->GetMathMLEnabled()) {
          size_t index = value - value_start;
          if (index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty_script_level) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty_script_size_multiplier) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty_script_min_size)) {
            continue;
          }
        }
        if (value->GetUnit() == eCSSUnit_Null) {
          value->SetInitialValue();
        }
      }
    }
  }
}

#ifdef DEBUG
/* virtual */ void
nsInitialStyleRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[initial style rule] {}\n", out);
}
#endif

static const nsStyleSet::sheetType gCSSSheetTypes[] = {
  nsStyleSet::eAgentSheet,
  nsStyleSet::eUserSheet,
  nsStyleSet::eDocSheet,
  nsStyleSet::eScopedDocSheet,
  nsStyleSet::eOverrideSheet
};

nsStyleSet::nsStyleSet()
  : mRuleTree(nullptr),
    mBatching(0),
    mInShutdown(false),
    mAuthorStyleDisabled(false),
    mInReconstruct(false),
    mInitFontFeatureValuesLookup(true),
    mDirty(0),
    mUnusedRuleNodeCount(0)
{
}

size_t
nsStyleSet::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  for (int i = 0; i < eSheetTypeCount; i++) {
    if (mRuleProcessors[i]) {
      n += mRuleProcessors[i]->SizeOfIncludingThis(aMallocSizeOf);
    }
    n += mSheets[i].SizeOfExcludingThis(nullptr, aMallocSizeOf);
  }

  for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
    n += mScopedDocSheetRuleProcessors[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mScopedDocSheetRuleProcessors.SizeOfExcludingThis(aMallocSizeOf);

  n += mRoots.SizeOfExcludingThis(aMallocSizeOf);
  n += mOldRuleTrees.SizeOfExcludingThis(aMallocSizeOf);

  return n;
}

void
nsStyleSet::Init(nsPresContext *aPresContext)
{
  mFirstLineRule = new nsEmptyStyleRule;
  mFirstLetterRule = new nsEmptyStyleRule;
  mPlaceholderRule = new nsEmptyStyleRule;

  mRuleTree = nsRuleNode::CreateRootNode(aPresContext);

  GatherRuleProcessors(eAnimationSheet);
  GatherRuleProcessors(eTransitionSheet);
}

nsresult
nsStyleSet::BeginReconstruct()
{
  NS_ASSERTION(!mInReconstruct, "Unmatched begin/end?");
  NS_ASSERTION(mRuleTree, "Reconstructing before first construction?");

  // Create a new rule tree root
  nsRuleNode* newTree =
    nsRuleNode::CreateRootNode(mRuleTree->PresContext());
  if (!newTree)
    return NS_ERROR_OUT_OF_MEMORY;

  // Save the old rule tree so we can destroy it later
  if (!mOldRuleTrees.AppendElement(mRuleTree)) {
    newTree->Destroy();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We need to keep mRoots so that the rule tree GC will only free the
  // rule trees that really aren't referenced anymore (which should be
  // all of them, if there are no bugs in reresolution code).

  mInReconstruct = true;
  mRuleTree = newTree;

  return NS_OK;
}

void
nsStyleSet::EndReconstruct()
{
  NS_ASSERTION(mInReconstruct, "Unmatched begin/end?");
  mInReconstruct = false;
#ifdef DEBUG
  for (int32_t i = mRoots.Length() - 1; i >= 0; --i) {
    nsRuleNode *n = mRoots[i]->RuleNode();
    while (n->GetParent()) {
      n = n->GetParent();
    }
    // Since nsStyleContext's mParent and mRuleNode are immutable, and
    // style contexts own their parents, and nsStyleContext asserts in
    // its constructor that the style context and its parent are in the
    // same rule tree, we don't need to check any of the children of
    // mRoots; we only need to check the rule nodes of mRoots
    // themselves.

    NS_ASSERTION(n == mRuleTree, "style context has old rule node");
  }
#endif
  // This *should* destroy the only element of mOldRuleTrees, but in
  // case of some bugs (which would trigger the above assertions), it
  // won't.
  GCRuleTrees();
}

void
nsStyleSet::SetQuirkStyleSheet(nsIStyleSheet* aQuirkStyleSheet)
{
  NS_ASSERTION(aQuirkStyleSheet, "Must have quirk sheet if this is called");
  NS_ASSERTION(!mQuirkStyleSheet, "Multiple calls to SetQuirkStyleSheet?");
  NS_ASSERTION(mSheets[eAgentSheet].IndexOf(aQuirkStyleSheet) != -1,
               "Quirk style sheet not one of our agent sheets?");
  mQuirkStyleSheet = aQuirkStyleSheet;
}

typedef nsDataHashtable<nsPtrHashKey<nsINode>, uint32_t> ScopeDepthCache;

// Returns the depth of a style scope element, with 1 being the depth of
// a style scope element that has no ancestor style scope elements.  The
// depth does not count intervening non-scope elements.
static uint32_t
GetScopeDepth(nsINode* aScopeElement, ScopeDepthCache& aCache)
{
  nsINode* parent = aScopeElement->GetParent();
  if (!parent || !parent->IsElementInStyleScope()) {
    return 1;
  }

  uint32_t depth = aCache.Get(aScopeElement);
  if (!depth) {
    for (nsINode* n = parent; n; n = n->GetParent()) {
      if (n->IsScopedStyleRoot()) {
        depth = GetScopeDepth(n, aCache) + 1;
        aCache.Put(aScopeElement, depth);
        break;
      }
    }
  }
  return depth;
}

struct ScopedSheetOrder
{
  nsCSSStyleSheet* mSheet;
  uint32_t mDepth;
  uint32_t mOrder;

  bool operator==(const ScopedSheetOrder& aRHS) const
  {
    return mDepth == aRHS.mDepth &&
           mOrder == aRHS.mOrder;
  }

  bool operator<(const ScopedSheetOrder& aRHS) const
  {
    if (mDepth != aRHS.mDepth) {
      return mDepth < aRHS.mDepth;
    }
    return mOrder < aRHS.mOrder;
  }
};

// Sorts aSheets such that style sheets for ancestor scopes come
// before those for descendant scopes, and with sheets for a single
// scope in document order.
static void
SortStyleSheetsByScope(nsTArray<nsCSSStyleSheet*>& aSheets)
{
  uint32_t n = aSheets.Length();
  if (n == 1) {
    return;
  }

  ScopeDepthCache cache;
  cache.Init();

  nsTArray<ScopedSheetOrder> sheets;
  sheets.SetLength(n);

  // For each sheet, record the depth of its scope element and its original
  // document order.
  for (uint32_t i = 0; i < n; i++) {
    sheets[i].mSheet = aSheets[i];
    sheets[i].mDepth = GetScopeDepth(aSheets[i]->GetScopeElement(), cache);
    sheets[i].mOrder = i;
  }

  // Sort by depth first, then document order.
  sheets.Sort();

  for (uint32_t i = 0; i < n; i++) {
    aSheets[i] = sheets[i].mSheet;
  }
}

nsresult
nsStyleSet::GatherRuleProcessors(sheetType aType)
{
  mRuleProcessors[aType] = nullptr;
  if (aType == eScopedDocSheet) {
    for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
      nsIStyleRuleProcessor* processor = mScopedDocSheetRuleProcessors[i].get();
      Element* scope =
        static_cast<nsCSSRuleProcessor*>(processor)->GetScopeElement();
      scope->ClearIsScopedStyleRoot();
    }
    mScopedDocSheetRuleProcessors.Clear();
  }
  if (mAuthorStyleDisabled && (aType == eDocSheet || 
                               aType == eScopedDocSheet ||
                               aType == eStyleAttrSheet)) {
    // Don't regather if this level is disabled.  Note that we gather
    // preshint sheets no matter what, but then skip them for some
    // elements later if mAuthorStyleDisabled.
    return NS_OK;
  }
  if (aType == eAnimationSheet) {
    // We have no sheet for the animations level; just a rule
    // processor.  (XXX: We should probably do this for the other
    // non-CSS levels too!)
    mRuleProcessors[aType] = PresContext()->AnimationManager();
    return NS_OK;
  }
  if (aType == eTransitionSheet) {
    // We have no sheet for the transitions level; just a rule
    // processor.  (XXX: We should probably do this for the other
    // non-CSS levels too!)
    mRuleProcessors[aType] = PresContext()->TransitionManager();
    return NS_OK;
  }
  if (aType == eScopedDocSheet) {
    // Create a rule processor for each scope.
    uint32_t count = mSheets[eScopedDocSheet].Count();
    if (count) {
      // Gather the scoped style sheets into an array as
      // nsCSSStyleSheets, and mark all of their scope elements
      // as scoped style roots.
      nsTArray<nsCSSStyleSheet*> sheets(count);
      for (uint32_t i = 0; i < count; i++) {
        nsRefPtr<nsCSSStyleSheet> sheet =
          do_QueryObject(mSheets[eScopedDocSheet].ObjectAt(i));
        sheets.AppendElement(sheet);

        Element* scope = sheet->GetScopeElement();
        scope->SetIsScopedStyleRoot();
      }

      // Sort the scoped style sheets so that those for the same scope are
      // adjacent and that ancestor scopes come before descendent scopes.
      SortStyleSheetsByScope(sheets);

      uint32_t start = 0, end;
      do {
        // Find the range of style sheets with the same scope.
        Element* scope = sheets[start]->GetScopeElement();
        end = start + 1;
        while (end < count && sheets[end]->GetScopeElement() == scope) {
          end++;
        }

        scope->SetIsScopedStyleRoot();

        // Create a rule processor for the scope.
        nsTArray< nsRefPtr<nsCSSStyleSheet> > sheetsForScope;
        sheetsForScope.AppendElements(sheets.Elements() + start, end - start);
        mScopedDocSheetRuleProcessors.AppendElement
          (new nsCSSRuleProcessor(sheetsForScope, uint8_t(aType), scope));

        start = end;
      } while (start < count);
    }
    return NS_OK;
  }
  if (mSheets[aType].Count()) {
    switch (aType) {
      case eAgentSheet:
      case eUserSheet:
      case eDocSheet:
      case eOverrideSheet: {
        // levels containing CSS stylesheets (apart from eScopedDocSheet)
        nsCOMArray<nsIStyleSheet>& sheets = mSheets[aType];
        nsTArray<nsRefPtr<nsCSSStyleSheet> > cssSheets(sheets.Count());
        for (int32_t i = 0, i_end = sheets.Count(); i < i_end; ++i) {
          nsRefPtr<nsCSSStyleSheet> cssSheet = do_QueryObject(sheets[i]);
          NS_ASSERTION(cssSheet, "not a CSS sheet");
          cssSheets.AppendElement(cssSheet);
        }
        mRuleProcessors[aType] =
          new nsCSSRuleProcessor(cssSheets, uint8_t(aType), nullptr);
      } break;

      default:
        // levels containing non-CSS stylesheets
        NS_ASSERTION(mSheets[aType].Count() == 1, "only one sheet per level");
        mRuleProcessors[aType] = do_QueryInterface(mSheets[aType][0]);
        break;
    }
  }

  return NS_OK;
}

static bool
IsScopedStyleSheet(nsIStyleSheet* aSheet)
{
  nsRefPtr<nsCSSStyleSheet> cssSheet = do_QueryObject(aSheet);
  NS_ASSERTION(cssSheet, "expected aSheet to be an nsCSSStyleSheet");

  return cssSheet->GetScopeElement();
}

nsresult
nsStyleSet::AppendStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].AppendObject(aSheet))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::PrependStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].InsertObjectAt(aSheet, 0))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::RemoveStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsComplete(),
               "Incomplete sheet being removed from style set");
  mSheets[aType].RemoveObject(aSheet);
  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::ReplaceSheets(sheetType aType,
                          const nsCOMArray<nsIStyleSheet> &aNewSheets)
{
  mSheets[aType].Clear();
  if (!mSheets[aType].AppendObjects(aNewSheets))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::InsertStyleSheetBefore(sheetType aType, nsIStyleSheet *aNewSheet,
                                   nsIStyleSheet *aReferenceSheet)
{
  NS_PRECONDITION(aNewSheet && aReferenceSheet, "null arg");
  NS_ASSERTION(aNewSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");

  mSheets[aType].RemoveObject(aNewSheet);
  int32_t idx = mSheets[aType].IndexOf(aReferenceSheet);
  if (idx < 0)
    return NS_ERROR_INVALID_ARG;
  
  if (!mSheets[aType].InsertObjectAt(aNewSheet, idx))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

bool
nsStyleSet::GetAuthorStyleDisabled()
{
  return mAuthorStyleDisabled;
}

nsresult
nsStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  if (aStyleDisabled == !mAuthorStyleDisabled) {
    mAuthorStyleDisabled = aStyleDisabled;
    BeginUpdate();
    mDirty |= 1 << eDocSheet |
              1 << eScopedDocSheet |
              1 << eStyleAttrSheet;
    return EndUpdate();
  }
  return NS_OK;
}

// -------- Doc Sheets

nsresult
nsStyleSet::AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument)
{
  NS_PRECONDITION(aSheet && aDocument, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");

  sheetType type = IsScopedStyleSheet(aSheet) ?
                     eScopedDocSheet :
                     eDocSheet;
  nsCOMArray<nsIStyleSheet>& sheets = mSheets[type];

  sheets.RemoveObject(aSheet);
  nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();

  // lowest index first
  int32_t newDocIndex = aDocument->GetIndexOfStyleSheet(aSheet);

  int32_t count = sheets.Count();
  int32_t index;
  for (index = 0; index < count; index++) {
    nsIStyleSheet* sheet = sheets.ObjectAt(index);
    int32_t sheetDocIndex = aDocument->GetIndexOfStyleSheet(sheet);
    if (sheetDocIndex > newDocIndex)
      break;

    // If the sheet is not owned by the document it can be an author
    // sheet registered at nsStyleSheetService or an additional author
    // sheet on the document, which means the new 
    // doc sheet should end up before it.
    if (sheetDocIndex < 0 &&
        ((sheetService &&
        sheetService->AuthorStyleSheets()->IndexOf(sheet) >= 0) ||
        sheet == aDocument->FirstAdditionalAuthorSheet()))
        break;
  }
  if (!sheets.InsertObjectAt(aSheet, index))
    return NS_ERROR_OUT_OF_MEMORY;
  if (!mBatching)
    return GatherRuleProcessors(type);

  mDirty |= 1 << type;
  return NS_OK;
}

nsresult
nsStyleSet::RemoveDocStyleSheet(nsIStyleSheet *aSheet)
{
  nsRefPtr<nsCSSStyleSheet> cssSheet = do_QueryObject(aSheet);
  bool isScoped = cssSheet && cssSheet->GetScopeElement();
  return RemoveStyleSheet(isScoped ? eScopedDocSheet : eDocSheet, aSheet);
}

// Batching
void
nsStyleSet::BeginUpdate()
{
  ++mBatching;
}

nsresult
nsStyleSet::EndUpdate()
{
  NS_ASSERTION(mBatching > 0, "Unbalanced EndUpdate");
  if (--mBatching) {
    // We're not completely done yet.
    return NS_OK;
  }

  for (int i = 0; i < eSheetTypeCount; ++i) {
    if (mDirty & (1 << i)) {
      nsresult rv = GatherRuleProcessors(sheetType(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mDirty = 0;
  return NS_OK;
}

void
nsStyleSet::EnableQuirkStyleSheet(bool aEnable)
{
#ifdef DEBUG
  bool oldEnabled;
  {
    nsCOMPtr<nsIDOMCSSStyleSheet> domSheet =
      do_QueryInterface(mQuirkStyleSheet);
    domSheet->GetDisabled(&oldEnabled);
    oldEnabled = !oldEnabled;
  }
#endif
  mQuirkStyleSheet->SetEnabled(aEnable);
#ifdef DEBUG
  // This should always be OK, since SetEnabled should call
  // ClearRuleCascades.
  // Note that we can hit this codepath multiple times when document.open()
  // (potentially implied) happens multiple times.
  if (mRuleProcessors[eAgentSheet] && aEnable != oldEnabled) {
    static_cast<nsCSSRuleProcessor*>(static_cast<nsIStyleRuleProcessor*>(
      mRuleProcessors[eAgentSheet]))->AssertQuirksChangeOK();
  }
#endif
}

template<class T>
static bool
EnumRulesMatching(nsIStyleRuleProcessor* aProcessor, void* aData)
{
  T* data = static_cast<T*>(aData);
  aProcessor->RulesMatching(data);
  return true;
}

static inline bool
IsMoreSpecificThanAnimation(nsRuleNode *aRuleNode)
{
  return !aRuleNode->IsRoot() &&
         (aRuleNode->GetLevel() == nsStyleSet::eTransitionSheet ||
          aRuleNode->IsImportantRule());
}

static nsIStyleRule*
GetAnimationRule(nsRuleNode *aRuleNode)
{
  nsRuleNode *n = aRuleNode;
  while (IsMoreSpecificThanAnimation(n)) {
    n = n->GetParent();
  }

  if (n->IsRoot() || n->GetLevel() != nsStyleSet::eAnimationSheet) {
    return nullptr;
  }

  return n->GetRule();
}

static nsRuleNode*
ReplaceAnimationRule(nsRuleNode *aOldRuleNode,
                     nsIStyleRule *aOldAnimRule,
                     nsIStyleRule *aNewAnimRule)
{
  nsTArray<nsRuleNode*> moreSpecificNodes;

  nsRuleNode *n = aOldRuleNode;
  while (IsMoreSpecificThanAnimation(n)) {
    moreSpecificNodes.AppendElement(n);
    n = n->GetParent();
  }

  if (aOldAnimRule) {
    NS_ABORT_IF_FALSE(n->GetRule() == aOldAnimRule, "wrong rule");
    NS_ABORT_IF_FALSE(n->GetLevel() == nsStyleSet::eAnimationSheet,
                      "wrong level");
    n = n->GetParent();
  }

  NS_ABORT_IF_FALSE(!IsMoreSpecificThanAnimation(n) &&
                    (n->IsRoot() ||
                     n->GetLevel() != nsStyleSet::eAnimationSheet),
                    "wrong level");

  if (aNewAnimRule) {
    n = n->Transition(aNewAnimRule, nsStyleSet::eAnimationSheet, false);
  }

  for (uint32_t i = moreSpecificNodes.Length(); i-- != 0; ) {
    nsRuleNode *oldNode = moreSpecificNodes[i];
    n = n->Transition(oldNode->GetRule(), oldNode->GetLevel(),
                      oldNode->IsImportantRule());
  }

  return n;
}

/**
 * |GetContext| implements sharing of style contexts (not just the data
 * on the rule nodes) between siblings and cousins of the same
 * generation.  (It works for cousins of the same generation since
 * |aParentContext| could itself be a shared context.)
 */
already_AddRefed<nsStyleContext>
nsStyleSet::GetContext(nsStyleContext* aParentContext,
                       nsRuleNode* aRuleNode,
                       // aVisitedRuleNode may be null; if it is null
                       // it means that we don't need to force creation
                       // of a StyleIfVisited.  (But if we make one
                       // because aParentContext has one, then aRuleNode
                       // should be used.)
                       nsRuleNode* aVisitedRuleNode,
                       nsIAtom* aPseudoTag,
                       nsCSSPseudoElements::Type aPseudoType,
                       Element* aElementForAnimation,
                       uint32_t aFlags)
{
  NS_PRECONDITION((!aPseudoTag &&
                   aPseudoType ==
                     nsCSSPseudoElements::ePseudo_NotPseudoElement) ||
                  (aPseudoTag &&
                   nsCSSPseudoElements::GetPseudoType(aPseudoTag) ==
                     aPseudoType),
                  "Pseudo mismatch");

  if (aVisitedRuleNode == aRuleNode) {
    // No need to force creation of a visited style in this case.
    aVisitedRuleNode = nullptr;
  }

  // Ensure |aVisitedRuleNode != nullptr| corresponds to the need to
  // create an if-visited style context, and that in that case, we have
  // parentIfVisited set correctly.
  nsStyleContext *parentIfVisited =
    aParentContext ? aParentContext->GetStyleIfVisited() : nullptr;
  if (parentIfVisited) {
    if (!aVisitedRuleNode) {
      aVisitedRuleNode = aRuleNode;
    }
  } else {
    if (aVisitedRuleNode) {
      parentIfVisited = aParentContext;
    }
  }

  if (aFlags & eIsLink) {
    // If this node is a link, we want its visited's style context's
    // parent to be the regular style context of its parent, because
    // only the visitedness of the relevant link should influence style.
    parentIfVisited = aParentContext;
  }

  nsRefPtr<nsStyleContext> result;
  if (aParentContext)
    result = aParentContext->FindChildWithRules(aPseudoTag, aRuleNode,
                                                aVisitedRuleNode,
                                                aFlags & eIsVisitedLink);

#ifdef NOISY_DEBUG
  if (result)
    fprintf(stdout, "--- SharedSC %d ---\n", ++gSharedCount);
  else
    fprintf(stdout, "+++ NewSC %d +++\n", ++gNewCount);
#endif

  if (!result) {
    result = NS_NewStyleContext(aParentContext, aPseudoTag, aPseudoType,
                                aRuleNode, aFlags & eSkipFlexItemStyleFixup);
    if (!result)
      return nullptr;
    if (aVisitedRuleNode) {
      nsRefPtr<nsStyleContext> resultIfVisited =
        NS_NewStyleContext(parentIfVisited, aPseudoTag, aPseudoType,
                           aVisitedRuleNode,
                           aFlags & eSkipFlexItemStyleFixup);
      if (!resultIfVisited) {
        return nullptr;
      }
      if (!parentIfVisited) {
        mRoots.AppendElement(resultIfVisited);
      }
      resultIfVisited->SetIsStyleIfVisited();
      result->SetStyleIfVisited(resultIfVisited.forget());

      bool relevantLinkVisited = (aFlags & eIsLink) ?
        (aFlags & eIsVisitedLink) :
        (aParentContext && aParentContext->RelevantLinkVisited());

      if (relevantLinkVisited) {
        result->AddStyleBit(NS_STYLE_RELEVANT_LINK_VISITED);
      }
    }
    if (!aParentContext) {
      mRoots.AppendElement(result);
    }
  }
  else {
    NS_ASSERTION(result->GetPseudoType() == aPseudoType, "Unexpected type");
    NS_ASSERTION(result->GetPseudo() == aPseudoTag, "Unexpected pseudo");
  }

  if (aFlags & eDoAnimation) {
    // Normally the animation manager has already added the correct
    // style rule.  However, if the animation-name just changed, it
    // might have been wrong.  So ask it to double-check based on the
    // resulting style context.
    nsIStyleRule *oldAnimRule = GetAnimationRule(aRuleNode);
    nsIStyleRule *animRule = PresContext()->AnimationManager()->
      CheckAnimationRule(result, aElementForAnimation);
    NS_ABORT_IF_FALSE(result->RuleNode() == aRuleNode,
                      "unexpected rule node");
    NS_ABORT_IF_FALSE(!result->GetStyleIfVisited() == !aVisitedRuleNode,
                      "unexpected visited rule node");
    NS_ABORT_IF_FALSE(!aVisitedRuleNode ||
                      result->GetStyleIfVisited()->RuleNode() ==
                        aVisitedRuleNode,
                      "unexpected visited rule node");
    if (oldAnimRule != animRule) {
      nsRuleNode *ruleNode =
        ReplaceAnimationRule(aRuleNode, oldAnimRule, animRule);
      nsRuleNode *visitedRuleNode = aVisitedRuleNode
        ? ReplaceAnimationRule(aVisitedRuleNode, oldAnimRule, animRule)
        : nullptr;
      result = GetContext(aParentContext, ruleNode, visitedRuleNode,
                          aPseudoTag, aPseudoType, nullptr,
                          aFlags & ~eDoAnimation);
    }
  }

  if (aElementForAnimation && aElementForAnimation->IsHTML(nsGkAtoms::body) &&
      aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement &&
      PresContext()->CompatibilityMode() == eCompatibility_NavQuirks) {
    nsIDocument* doc = aElementForAnimation->GetCurrentDoc();
    if (doc && doc->GetBodyElement() == aElementForAnimation) {
      // Update the prescontext's body color
      PresContext()->SetBodyTextColor(result->StyleColor()->mColor);
    }
  }

  return result.forget();
}

void
nsStyleSet::AddImportantRules(nsRuleNode* aCurrLevelNode,
                              nsRuleNode* aLastPrevLevelNode,
                              nsRuleWalker* aRuleWalker)
{
  NS_ASSERTION(aCurrLevelNode &&
               aCurrLevelNode != aLastPrevLevelNode, "How did we get here?");

  nsAutoTArray<nsIStyleRule*, 16> importantRules;
  for (nsRuleNode *node = aCurrLevelNode; node != aLastPrevLevelNode;
       node = node->GetParent()) {
    // We guarantee that we never walk the root node here, so no need
    // to null-check GetRule().  Furthermore, it must be a CSS rule.
    NS_ASSERTION(nsRefPtr<css::StyleRule>(do_QueryObject(node->GetRule())),
                 "Unexpected non-CSS rule");

    nsIStyleRule* impRule =
      static_cast<css::StyleRule*>(node->GetRule())->GetImportantRule();
    if (impRule)
      importantRules.AppendElement(impRule);
  }

  NS_ASSERTION(importantRules.Length() != 0,
               "Why did we think there were important rules?");

  for (uint32_t i = importantRules.Length(); i-- != 0; ) {
    aRuleWalker->Forward(importantRules[i]);
  }
}

#ifdef DEBUG
void
nsStyleSet::AssertNoImportantRules(nsRuleNode* aCurrLevelNode,
                                   nsRuleNode* aLastPrevLevelNode)
{
  if (!aCurrLevelNode)
    return;

  for (nsRuleNode *node = aCurrLevelNode; node != aLastPrevLevelNode;
       node = node->GetParent()) {
    nsRefPtr<css::StyleRule> rule(do_QueryObject(node->GetRule()));
    NS_ASSERTION(rule, "Unexpected non-CSS rule");

    NS_ASSERTION(!rule->GetImportantRule(), "Unexpected important rule");
  }
}

void
nsStyleSet::AssertNoCSSRules(nsRuleNode* aCurrLevelNode,
                             nsRuleNode* aLastPrevLevelNode)
{
  if (!aCurrLevelNode)
    return;

  for (nsRuleNode *node = aCurrLevelNode; node != aLastPrevLevelNode;
       node = node->GetParent()) {
    nsIStyleRule *rule = node->GetRule();
    nsRefPtr<css::StyleRule> cssRule(do_QueryObject(rule));
    NS_ASSERTION(!cssRule || !cssRule->Selector(), "Unexpected CSS rule");
  }
}
#endif

// Enumerate the rules in a way that cares about the order of the rules.
void
nsStyleSet::FileRules(nsIStyleRuleProcessor::EnumFunc aCollectorFunc, 
                      RuleProcessorData* aData, Element* aElement,
                      nsRuleWalker* aRuleWalker)
{
  PROFILER_LABEL("nsStyleSet", "FileRules");

  // Cascading order:
  // [least important]
  //  - UA normal rules                    = Agent        normal
  //  - User normal rules                  = User         normal
  //  - Presentation hints                 = PresHint     normal
  //  - Author normal rules                = Document     normal
  //  - Override normal rules              = Override     normal
  //  - animation rules                    = Animation    normal
  //  - Author !important rules            = Document     !important
  //  - Override !important rules          = Override     !important
  //  - User !important rules              = User         !important
  //  - UA !important rules                = Agent        !important
  //  - transition rules                   = Transition   normal
  // [most important]

  // Save off the last rule before we start walking our agent sheets;
  // this will be either the root or one of the restriction rules.
  nsRuleNode* lastRestrictionRN = aRuleWalker->CurrentNode();

  aRuleWalker->SetLevel(eAgentSheet, false, true);
  if (mRuleProcessors[eAgentSheet])
    (*aCollectorFunc)(mRuleProcessors[eAgentSheet], aData);
  nsRuleNode* lastAgentRN = aRuleWalker->CurrentNode();
  bool haveImportantUARules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(eUserSheet, false, true);
  bool skipUserStyles =
    aElement && aElement->IsInNativeAnonymousSubtree();
  if (!skipUserStyles && mRuleProcessors[eUserSheet]) // NOTE: different
    (*aCollectorFunc)(mRuleProcessors[eUserSheet], aData);
  nsRuleNode* lastUserRN = aRuleWalker->CurrentNode();
  bool haveImportantUserRules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(ePresHintSheet, false, false);
  if (mRuleProcessors[ePresHintSheet])
    (*aCollectorFunc)(mRuleProcessors[ePresHintSheet], aData);
  nsRuleNode* lastPresHintRN = aRuleWalker->CurrentNode();
  
  aRuleWalker->SetLevel(eDocSheet, false, true);
  bool cutOffInheritance = false;
  if (mBindingManager && aElement) {
    // We can supply additional document-level sheets that should be walked.
    mBindingManager->WalkRules(aCollectorFunc,
                               static_cast<ElementDependentRuleProcessorData*>(aData),
                               &cutOffInheritance);
  }
  if (!skipUserStyles && !cutOffInheritance && // NOTE: different
      mRuleProcessors[eDocSheet])
    (*aCollectorFunc)(mRuleProcessors[eDocSheet], aData);
  nsRuleNode* lastDocRN = aRuleWalker->CurrentNode();
  bool haveImportantDocRules = !aRuleWalker->GetCheckForImportantRules();
  nsTArray<nsRuleNode*> lastScopedRNs;
  nsTArray<bool> haveImportantScopedRules;
  bool haveAnyImportantScopedRules = false;
  if (!skipUserStyles && !cutOffInheritance &&
      aElement && aElement->IsElementInStyleScope()) {
    lastScopedRNs.SetLength(mScopedDocSheetRuleProcessors.Length());
    haveImportantScopedRules.SetLength(mScopedDocSheetRuleProcessors.Length());
    for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
      aRuleWalker->SetLevel(eScopedDocSheet, false, true);
      nsCSSRuleProcessor* processor =
        static_cast<nsCSSRuleProcessor*>(mScopedDocSheetRuleProcessors[i].get());
      aData->mScope = processor->GetScopeElement();
      (*aCollectorFunc)(mScopedDocSheetRuleProcessors[i], aData);
      lastScopedRNs[i] = aRuleWalker->CurrentNode();
      haveImportantScopedRules[i] = !aRuleWalker->GetCheckForImportantRules();
      haveAnyImportantScopedRules = haveAnyImportantScopedRules || haveImportantScopedRules[i];
    }
    aData->mScope = nullptr;
  }
  nsRuleNode* lastScopedRN = aRuleWalker->CurrentNode();
  aRuleWalker->SetLevel(eStyleAttrSheet, false, true);
  if (mRuleProcessors[eStyleAttrSheet])
    (*aCollectorFunc)(mRuleProcessors[eStyleAttrSheet], aData);
  nsRuleNode* lastStyleAttrRN = aRuleWalker->CurrentNode();
  bool haveImportantStyleAttrRules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(eOverrideSheet, false, true);
  if (mRuleProcessors[eOverrideSheet])
    (*aCollectorFunc)(mRuleProcessors[eOverrideSheet], aData);
  nsRuleNode* lastOvrRN = aRuleWalker->CurrentNode();
  bool haveImportantOverrideRules = !aRuleWalker->GetCheckForImportantRules();

  // This needs to match IsMoreSpecificThanAnimation() above.
  aRuleWalker->SetLevel(eAnimationSheet, false, false);
  (*aCollectorFunc)(mRuleProcessors[eAnimationSheet], aData);

  if (haveAnyImportantScopedRules) {
    for (uint32_t i = lastScopedRNs.Length(); i-- != 0; ) {
      aRuleWalker->SetLevel(eScopedDocSheet, true, false);
      nsRuleNode* startRN = lastScopedRNs[i];
      nsRuleNode* endRN = i == 0 ? lastDocRN : lastScopedRNs[i - 1];
      if (haveImportantScopedRules[i]) {
        AddImportantRules(startRN, endRN, aRuleWalker);  // scoped
      }
#ifdef DEBUG
      else {
        AssertNoImportantRules(startRN, endRN);
      }
#endif
    }
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastScopedRN, lastDocRN);
  }
#endif

  if (haveImportantDocRules) {
    aRuleWalker->SetLevel(eDocSheet, true, false);
    AddImportantRules(lastDocRN, lastPresHintRN, aRuleWalker);  // doc
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastDocRN, lastPresHintRN);
  }
#endif

  if (haveImportantStyleAttrRules) {
    aRuleWalker->SetLevel(eStyleAttrSheet, true, false);
    AddImportantRules(lastStyleAttrRN, lastScopedRN, aRuleWalker);  // style attr
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastStyleAttrRN, lastScopedRN);
  }
#endif

  if (haveImportantOverrideRules) {
    aRuleWalker->SetLevel(eOverrideSheet, true, false);
    AddImportantRules(lastOvrRN, lastStyleAttrRN, aRuleWalker);  // override
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastOvrRN, lastStyleAttrRN);
  }
#endif

#ifdef DEBUG
  AssertNoCSSRules(lastPresHintRN, lastUserRN);
#endif

  if (haveImportantUserRules) {
    aRuleWalker->SetLevel(eUserSheet, true, false);
    AddImportantRules(lastUserRN, lastAgentRN, aRuleWalker); //user
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastUserRN, lastAgentRN);
  }
#endif

  if (haveImportantUARules) {
    aRuleWalker->SetLevel(eAgentSheet, true, false);
    AddImportantRules(lastAgentRN, lastRestrictionRN, aRuleWalker); //agent
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastAgentRN, lastRestrictionRN);
  }
#endif

#ifdef DEBUG
  AssertNoCSSRules(lastRestrictionRN, mRuleTree);
#endif

#ifdef DEBUG
  nsRuleNode *lastImportantRN = aRuleWalker->CurrentNode();
#endif
  aRuleWalker->SetLevel(eTransitionSheet, false, false);
  (*aCollectorFunc)(mRuleProcessors[eTransitionSheet], aData);
#ifdef DEBUG
  AssertNoCSSRules(aRuleWalker->CurrentNode(), lastImportantRN);
#endif

}

// Enumerate all the rules in a way that doesn't care about the order
// of the rules and doesn't walk !important-rules.
void
nsStyleSet::WalkRuleProcessors(nsIStyleRuleProcessor::EnumFunc aFunc,
                               ElementDependentRuleProcessorData* aData,
                               bool aWalkAllXBLStylesheets)
{
  if (mRuleProcessors[eAgentSheet])
    (*aFunc)(mRuleProcessors[eAgentSheet], aData);

  bool skipUserStyles = aData->mElement->IsInNativeAnonymousSubtree();
  if (!skipUserStyles && mRuleProcessors[eUserSheet]) // NOTE: different
    (*aFunc)(mRuleProcessors[eUserSheet], aData);

  if (mRuleProcessors[ePresHintSheet])
    (*aFunc)(mRuleProcessors[ePresHintSheet], aData);

  bool cutOffInheritance = false;
  if (mBindingManager) {
    // We can supply additional document-level sheets that should be walked.
    if (aWalkAllXBLStylesheets) {
      mBindingManager->WalkAllRules(aFunc, aData);
    } else {
      mBindingManager->WalkRules(aFunc, aData, &cutOffInheritance);
    }
  }
  if (!skipUserStyles && !cutOffInheritance) {
    if (mRuleProcessors[eDocSheet]) // NOTE: different
      (*aFunc)(mRuleProcessors[eDocSheet], aData);
    if (aData->mElement->IsElementInStyleScope()) {
      for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++)
        (*aFunc)(mScopedDocSheetRuleProcessors[i], aData);
    }
  }
  if (mRuleProcessors[eStyleAttrSheet])
    (*aFunc)(mRuleProcessors[eStyleAttrSheet], aData);
  if (mRuleProcessors[eOverrideSheet])
    (*aFunc)(mRuleProcessors[eOverrideSheet], aData);
  (*aFunc)(mRuleProcessors[eAnimationSheet], aData);
  (*aFunc)(mRuleProcessors[eTransitionSheet], aData);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleFor(Element* aElement,
                            nsStyleContext* aParentContext)
{
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aElement->OwnerDoc());
  return ResolveStyleFor(aElement, aParentContext, treeContext);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleFor(Element* aElement,
                            nsStyleContext* aParentContext,
                            TreeMatchContext& aTreeMatchContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);
  NS_ASSERTION(aElement, "aElement must not be null");

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  aTreeMatchContext.ResetForUnvisitedMatching();
  ElementRuleProcessorData data(PresContext(), aElement, &ruleWalker,
                                aTreeMatchContext);
  FileRules(EnumRulesMatching<ElementRuleProcessorData>, &data, aElement,
            &ruleWalker);

  nsRuleNode *ruleNode = ruleWalker.CurrentNode();
  nsRuleNode *visitedRuleNode = nullptr;

  if (aTreeMatchContext.HaveRelevantLink()) {
    aTreeMatchContext.ResetForVisitedMatching();
    ruleWalker.Reset();
    FileRules(EnumRulesMatching<ElementRuleProcessorData>, &data, aElement,
              &ruleWalker);
    visitedRuleNode = ruleWalker.CurrentNode();
  }

  uint32_t flags = eDoAnimation;
  if (nsCSSRuleProcessor::IsLink(aElement)) {
    flags |= eIsLink;
  }
  if (nsCSSRuleProcessor::GetContentState(aElement, aTreeMatchContext).
                            HasState(NS_EVENT_STATE_VISITED)) {
    flags |= eIsVisitedLink;
  }
  if (aTreeMatchContext.mSkippingFlexItemStyleFixup) {
    flags |= eSkipFlexItemStyleFixup;
  }

  return GetContext(aParentContext, ruleNode, visitedRuleNode,
                    nullptr, nsCSSPseudoElements::ePseudo_NotPseudoElement,
                    aElement, flags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleForRules(nsStyleContext* aParentContext,
                                 const nsTArray< nsCOMPtr<nsIStyleRule> > &aRules)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  // FIXME: Perhaps this should be passed in, but it probably doesn't
  // matter.
  ruleWalker.SetLevel(eDocSheet, false, false);
  for (uint32_t i = 0; i < aRules.Length(); i++) {
    ruleWalker.ForwardOnPossiblyCSSRule(aRules.ElementAt(i));
  }

  return GetContext(aParentContext, ruleWalker.CurrentNode(), nullptr,
                    nullptr, nsCSSPseudoElements::ePseudo_NotPseudoElement,
                    nullptr, eNoFlags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleForRules(nsStyleContext* aParentContext,
                                 nsStyleContext* aOldStyle,
                                 const nsTArray<RuleAndLevel>& aRules)
{
  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  for (int32_t i = aRules.Length() - 1; i >= 0; --i) {
    ruleWalker.SetLevel(aRules[i].mLevel, false, false);
    ruleWalker.ForwardOnPossiblyCSSRule(aRules[i].mRule);
  }

  uint32_t flags = eNoFlags;
  if (aOldStyle->IsLinkContext()) {
    flags |= eIsLink;
  }
  if (aOldStyle->RelevantLinkVisited()) {
    flags |= eIsVisitedLink;
  }

  return GetContext(aParentContext, ruleWalker.CurrentNode(), nullptr,
                    nullptr, nsCSSPseudoElements::ePseudo_NotPseudoElement,
                    nullptr, flags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleByAddingRules(nsStyleContext* aBaseContext,
                                      const nsCOMArray<nsIStyleRule> &aRules)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  ruleWalker.SetCurrentNode(aBaseContext->RuleNode());
  // FIXME: Perhaps this should be passed in, but it probably doesn't
  // matter.
  ruleWalker.SetLevel(eDocSheet, false, false);
  for (int32_t i = 0; i < aRules.Count(); i++) {
    ruleWalker.ForwardOnPossiblyCSSRule(aRules.ObjectAt(i));
  }

  nsRuleNode *ruleNode = ruleWalker.CurrentNode();
  nsRuleNode *visitedRuleNode = nullptr;

  if (aBaseContext->GetStyleIfVisited()) {
    ruleWalker.SetCurrentNode(aBaseContext->GetStyleIfVisited()->RuleNode());
    for (int32_t i = 0; i < aRules.Count(); i++) {
      ruleWalker.ForwardOnPossiblyCSSRule(aRules.ObjectAt(i));
    }
    visitedRuleNode = ruleWalker.CurrentNode();
  }

  uint32_t flags = eNoFlags;
  if (aBaseContext->IsLinkContext()) {
    flags |= eIsLink;
  }
  if (aBaseContext->RelevantLinkVisited()) {
    flags |= eIsVisitedLink;
  }
  return GetContext(aBaseContext->GetParent(), ruleNode, visitedRuleNode,
                    aBaseContext->GetPseudo(),
                    aBaseContext->GetPseudoType(),
                    nullptr, flags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleForNonElement(nsStyleContext* aParentContext)
{
  return GetContext(aParentContext, mRuleTree, nullptr,
                    nsCSSAnonBoxes::mozNonElement,
                    nsCSSPseudoElements::ePseudo_AnonBox, nullptr,
                    eNoFlags);
}

void
nsStyleSet::WalkRestrictionRule(nsCSSPseudoElements::Type aPseudoType,
                                nsRuleWalker* aRuleWalker)
{
  // This needs to match GetPseudoRestriction in nsRuleNode.cpp.
  aRuleWalker->SetLevel(eAgentSheet, false, false);
  if (aPseudoType == nsCSSPseudoElements::ePseudo_firstLetter)
    aRuleWalker->Forward(mFirstLetterRule);
  else if (aPseudoType == nsCSSPseudoElements::ePseudo_firstLine)
    aRuleWalker->Forward(mFirstLineRule);
  else if (aPseudoType == nsCSSPseudoElements::ePseudo_mozPlaceholder)
    aRuleWalker->Forward(mPlaceholderRule);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolvePseudoElementStyle(Element* aParentElement,
                                      nsCSSPseudoElements::Type aType,
                                      nsStyleContext* aParentContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
               "must have pseudo element type");
  NS_ASSERTION(aParentElement, "Must have parent element");

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aParentElement->OwnerDoc());
  PseudoElementRuleProcessorData data(PresContext(), aParentElement,
                                      &ruleWalker, aType, treeContext);
  WalkRestrictionRule(aType, &ruleWalker);
  FileRules(EnumRulesMatching<PseudoElementRuleProcessorData>, &data,
            aParentElement, &ruleWalker);

  nsRuleNode *ruleNode = ruleWalker.CurrentNode();
  nsRuleNode *visitedRuleNode = nullptr;

  if (treeContext.HaveRelevantLink()) {
    treeContext.ResetForVisitedMatching();
    ruleWalker.Reset();
    WalkRestrictionRule(aType, &ruleWalker);
    FileRules(EnumRulesMatching<PseudoElementRuleProcessorData>, &data,
              aParentElement, &ruleWalker);
    visitedRuleNode = ruleWalker.CurrentNode();
  }

  // For pseudos, |data.IsLink()| being true means that
  // our parent node is a link.
  uint32_t flags = eNoFlags;
  if (aType == nsCSSPseudoElements::ePseudo_before ||
      aType == nsCSSPseudoElements::ePseudo_after) {
    flags |= eDoAnimation;
  } else {
    // Flex containers don't expect to have any pseudo-element children aside
    // from ::before and ::after.  So if we have such a child, we're not
    // actually in a flex container, and we should skip flex-item style fixup.
    flags |= eSkipFlexItemStyleFixup;
  }

  return GetContext(aParentContext, ruleNode, visitedRuleNode,
                    nsCSSPseudoElements::GetPseudoAtom(aType), aType,
                    aParentElement, flags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                    nsCSSPseudoElements::Type aType,
                                    nsStyleContext* aParentContext)
{
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aParentElement->OwnerDoc());
  return ProbePseudoElementStyle(aParentElement, aType, aParentContext,
                                 treeContext);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                    nsCSSPseudoElements::Type aType,
                                    nsStyleContext* aParentContext,
                                    TreeMatchContext& aTreeMatchContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
               "must have pseudo element type");
  NS_ASSERTION(aParentElement, "aParentElement must not be null");

  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  aTreeMatchContext.ResetForUnvisitedMatching();
  PseudoElementRuleProcessorData data(PresContext(), aParentElement,
                                      &ruleWalker, aType, aTreeMatchContext);
  WalkRestrictionRule(aType, &ruleWalker);
  // not the root if there was a restriction rule
  nsRuleNode *adjustedRoot = ruleWalker.CurrentNode();
  FileRules(EnumRulesMatching<PseudoElementRuleProcessorData>, &data,
            aParentElement, &ruleWalker);

  nsRuleNode *ruleNode = ruleWalker.CurrentNode();
  if (ruleNode == adjustedRoot) {
    return nullptr;
  }

  nsRuleNode *visitedRuleNode = nullptr;

  if (aTreeMatchContext.HaveRelevantLink()) {
    aTreeMatchContext.ResetForVisitedMatching();
    ruleWalker.Reset();
    WalkRestrictionRule(aType, &ruleWalker);
    FileRules(EnumRulesMatching<PseudoElementRuleProcessorData>, &data,
              aParentElement, &ruleWalker);
    visitedRuleNode = ruleWalker.CurrentNode();
  }

  // For pseudos, |data.IsLink()| being true means that
  // our parent node is a link.
  uint32_t flags = eNoFlags;
  if (aType == nsCSSPseudoElements::ePseudo_before ||
      aType == nsCSSPseudoElements::ePseudo_after) {
    flags |= eDoAnimation;
  } else {
    // Flex containers don't expect to have any pseudo-element children aside
    // from ::before and ::after.  So if we have such a child, we're not
    // actually in a flex container, and we should skip flex-item style fixup.
    flags |= eSkipFlexItemStyleFixup;
  }

  nsRefPtr<nsStyleContext> result =
    GetContext(aParentContext, ruleNode, visitedRuleNode,
               pseudoTag, aType,
               aParentElement, flags);

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  if (result &&
      (pseudoTag == nsCSSPseudoElements::before ||
       pseudoTag == nsCSSPseudoElements::after)) {
    const nsStyleDisplay *display = result->StyleDisplay();
    const nsStyleContent *content = result->StyleContent();
    // XXXldb What is contentCount for |content: ""|?
    if (display->mDisplay == NS_STYLE_DISPLAY_NONE ||
        content->ContentCount() == 0) {
      result = nullptr;
    }
  }

  return result.forget();
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                     nsStyleContext* aParentContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

#ifdef DEBUG
    bool isAnonBox = nsCSSAnonBoxes::IsAnonBox(aPseudoTag)
#ifdef MOZ_XUL
                 && !nsCSSAnonBoxes::IsTreePseudoElement(aPseudoTag)
#endif
      ;
    NS_PRECONDITION(isAnonBox, "Unexpected pseudo");
#endif

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  AnonBoxRuleProcessorData data(PresContext(), aPseudoTag, &ruleWalker);
  FileRules(EnumRulesMatching<AnonBoxRuleProcessorData>, &data, nullptr,
            &ruleWalker);

  if (aPseudoTag == nsCSSAnonBoxes::pageContent) {
    // Add any @page rules that are specified.
    nsTArray<nsCSSPageRule*> rules;
    nsTArray<css::ImportantRule*> importantRules;
    nsPresContext* presContext = PresContext();
    presContext->StyleSet()->AppendPageRules(presContext, rules);
    for (uint32_t i = 0, i_end = rules.Length(); i != i_end; ++i) {
      ruleWalker.Forward(rules[i]);
      css::ImportantRule* importantRule = rules[i]->GetImportantRule();
      if (importantRule) {
        importantRules.AppendElement(importantRule);
      }
    }
    for (uint32_t i = 0, i_end = importantRules.Length(); i != i_end; ++i) {
      ruleWalker.Forward(importantRules[i]);
    }
  }

  return GetContext(aParentContext, ruleWalker.CurrentNode(), nullptr,
                    aPseudoTag, nsCSSPseudoElements::ePseudo_AnonBox,
                    nullptr, eNoFlags);
}

#ifdef MOZ_XUL
already_AddRefed<nsStyleContext>
nsStyleSet::ResolveXULTreePseudoStyle(Element* aParentElement,
                                      nsIAtom* aPseudoTag,
                                      nsStyleContext* aParentContext,
                                      nsICSSPseudoComparator* aComparator)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(nsCSSAnonBoxes::IsTreePseudoElement(aPseudoTag),
               "Unexpected pseudo");

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aParentElement->OwnerDoc());
  XULTreeRuleProcessorData data(PresContext(), aParentElement, &ruleWalker,
                                aPseudoTag, aComparator, treeContext);
  FileRules(EnumRulesMatching<XULTreeRuleProcessorData>, &data, aParentElement,
            &ruleWalker);

  nsRuleNode *ruleNode = ruleWalker.CurrentNode();
  nsRuleNode *visitedRuleNode = nullptr;

  if (treeContext.HaveRelevantLink()) {
    treeContext.ResetForVisitedMatching();
    ruleWalker.Reset();
    FileRules(EnumRulesMatching<XULTreeRuleProcessorData>, &data,
              aParentElement, &ruleWalker);
    visitedRuleNode = ruleWalker.CurrentNode();
  }

  return GetContext(aParentContext, ruleNode, visitedRuleNode,
                    // For pseudos, |data.IsLink()| being true means that
                    // our parent node is a link.
                    aPseudoTag, nsCSSPseudoElements::ePseudo_XULTree,
                    nullptr, eNoFlags);
}
#endif

bool
nsStyleSet::AppendFontFaceRules(nsPresContext* aPresContext,
                                nsTArray<nsFontFaceRuleContainer>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);

  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    if (gCSSSheetTypes[i] == eScopedDocSheet)
      continue;
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc && !ruleProc->AppendFontFaceRules(aPresContext, aArray))
      return false;
  }
  return true;
}

bool
nsStyleSet::AppendKeyframesRules(nsPresContext* aPresContext,
                                 nsTArray<nsCSSKeyframesRule*>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);

  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    if (gCSSSheetTypes[i] == eScopedDocSheet)
      continue;
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc && !ruleProc->AppendKeyframesRules(aPresContext, aArray))
      return false;
  }
  return true;
}

bool
nsStyleSet::AppendFontFeatureValuesRules(nsPresContext* aPresContext,
                                 nsTArray<nsCSSFontFeatureValuesRule*>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);

  for (uint32_t i = 0; i < NS_ARRAY_LENGTH(gCSSSheetTypes); ++i) {
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc &&
        !ruleProc->AppendFontFeatureValuesRules(aPresContext, aArray))
    {
      return false;
    }
  }
  return true;
}

already_AddRefed<gfxFontFeatureValueSet>
nsStyleSet::GetFontFeatureValuesLookup()
{
  if (mInitFontFeatureValuesLookup) {
    mInitFontFeatureValuesLookup = false;

    nsTArray<nsCSSFontFeatureValuesRule*> rules;
    AppendFontFeatureValuesRules(PresContext(), rules);

    mFontFeatureValuesLookup = new gfxFontFeatureValueSet();

    uint32_t i, numRules = rules.Length();
    for (i = 0; i < numRules; i++) {
      nsCSSFontFeatureValuesRule *rule = rules[i];

      const nsTArray<nsString>& familyList = rule->GetFamilyList();
      const nsTArray<gfxFontFeatureValueSet::FeatureValues>&
        featureValues = rule->GetFeatureValues();

      // for each family
      uint32_t f, numFam;

      numFam = familyList.Length();
      for (f = 0; f < numFam; f++) {
        const nsString& family = familyList.ElementAt(f);
        nsAutoString silly(family);
        mFontFeatureValuesLookup->AddFontFeatureValues(silly, featureValues);
      }
    }
  }

  nsRefPtr<gfxFontFeatureValueSet> lookup = mFontFeatureValuesLookup;
  return lookup.forget();
}

bool
nsStyleSet::AppendPageRules(nsPresContext* aPresContext,
                            nsTArray<nsCSSPageRule*>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);

  for (uint32_t i = 0; i < NS_ARRAY_LENGTH(gCSSSheetTypes); ++i) {
    if (gCSSSheetTypes[i] == eScopedDocSheet)
      continue;
    nsCSSRuleProcessor* ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc && !ruleProc->AppendPageRules(aPresContext, aArray))
      return false;
  }
  return true;
}

void
nsStyleSet::BeginShutdown(nsPresContext* aPresContext)
{
  mInShutdown = 1;
  mRoots.Clear(); // no longer valid, since we won't keep it up to date
}

void
nsStyleSet::Shutdown(nsPresContext* aPresContext)
{
  mRuleTree->Destroy();
  mRuleTree = nullptr;

  // We can have old rule trees either because:
  //   (1) we failed the assertions in EndReconstruct, or
  //   (2) we're shutting down within a reconstruct (see bug 462392)
  for (uint32_t i = mOldRuleTrees.Length(); i > 0; ) {
    --i;
    mOldRuleTrees[i]->Destroy();
  }
  mOldRuleTrees.Clear();
}

static const uint32_t kGCInterval = 300;

void
nsStyleSet::NotifyStyleContextDestroyed(nsPresContext* aPresContext,
                                        nsStyleContext* aStyleContext)
{
  if (mInShutdown)
    return;

  // Remove style contexts from mRoots even if mOldRuleTree is non-null.  This
  // could be a style context from the new ruletree!
  if (!aStyleContext->GetParent()) {
    mRoots.RemoveElement(aStyleContext);
  }

  if (mInReconstruct)
    return;

  if (mUnusedRuleNodeCount >= kGCInterval) {
    GCRuleTrees();
  }
}

void
nsStyleSet::GCRuleTrees()
{
  mUnusedRuleNodeCount = 0;

  // Mark the style context tree by marking all style contexts which
  // have no parent, which will mark all descendants.  This will reach
  // style contexts in the undisplayed map and "additional style
  // contexts" since they are descendants of the roots.
  for (int32_t i = mRoots.Length() - 1; i >= 0; --i) {
    mRoots[i]->Mark();
  }

  // Sweep the rule tree.
#ifdef DEBUG
  bool deleted =
#endif
    mRuleTree->Sweep();
  NS_ASSERTION(!deleted, "Root node must not be gc'd");

  // Sweep the old rule trees.
  for (uint32_t i = mOldRuleTrees.Length(); i > 0; ) {
    --i;
    if (mOldRuleTrees[i]->Sweep()) {
      // It was deleted, as it should be.
      mOldRuleTrees.RemoveElementAt(i);
    } else {
      NS_NOTREACHED("old rule tree still referenced");
    }
  }
}

static inline nsRuleNode*
SkipAnimationRules(nsRuleNode* aRuleNode, Element* aElement, bool isPseudo)
{
  nsRuleNode* ruleNode = aRuleNode;
  while (!ruleNode->IsRoot() &&
         (ruleNode->GetLevel() == nsStyleSet::eTransitionSheet ||
          ruleNode->GetLevel() == nsStyleSet::eAnimationSheet)) {
    ruleNode = ruleNode->GetParent();
  }
  if (ruleNode != aRuleNode) {
    NS_ASSERTION(aElement, "How can we have transition rules but no element?");
    // Need to do an animation restyle, just like
    // nsTransitionManager::WalkTransitionRule and
    // nsAnimationManager::GetAnimationRule would.
    nsRestyleHint hint = isPseudo ? eRestyle_Subtree : eRestyle_Self;
    aRuleNode->PresContext()->PresShell()->RestyleForAnimation(aElement, hint);
  }
  return ruleNode;
}

already_AddRefed<nsStyleContext>
nsStyleSet::ReparentStyleContext(nsStyleContext* aStyleContext,
                                 nsStyleContext* aNewParentContext,
                                 Element* aElement)
{
  if (!aStyleContext) {
    NS_NOTREACHED("must have style context");
    return nullptr;
  }

  // This short-circuit is OK because we don't call TryStartingTransition
  // during style reresolution if the style context pointer hasn't changed.
  if (aStyleContext->GetParent() == aNewParentContext) {
    nsRefPtr<nsStyleContext> ret = aStyleContext;
    return ret.forget();
  }

  nsIAtom* pseudoTag = aStyleContext->GetPseudo();
  nsCSSPseudoElements::Type pseudoType = aStyleContext->GetPseudoType();
  nsRuleNode* ruleNode = aStyleContext->RuleNode();

  // Skip transition rules as needed just like
  // nsTransitionManager::WalkTransitionRule would.
  bool skipAnimationRules = PresContext()->IsProcessingRestyles() &&
    !PresContext()->IsProcessingAnimationStyleChange();
  if (skipAnimationRules) {
    // Make sure that we're not using transition rules or animation rules for
    // our new style context.  If we need them, an animation restyle will
    // provide.
    ruleNode =
      SkipAnimationRules(ruleNode, aElement,
                         pseudoType !=
                           nsCSSPseudoElements::ePseudo_NotPseudoElement);
  }

  nsRuleNode* visitedRuleNode = nullptr;
  nsStyleContext* visitedContext = aStyleContext->GetStyleIfVisited();
  // Reparenting a style context just changes where we inherit from,
  // not what rules we match or what our DOM looks like.  In
  // particular, it doesn't change whether this is a style context for
  // a link.
  if (visitedContext) {
     visitedRuleNode = visitedContext->RuleNode();
     // Again, skip transition rules as needed
     if (skipAnimationRules) {
      // FIXME do something here for animations?
       visitedRuleNode =
         SkipAnimationRules(visitedRuleNode, aElement,
                            pseudoType !=
                              nsCSSPseudoElements::ePseudo_NotPseudoElement);
     }
  }

  uint32_t flags = eNoFlags;
  if (aStyleContext->IsLinkContext()) {
    flags |= eIsLink;
  }

  // If we're a style context for a link, then we already know whether
  // our relevant link is visited, since that does not depend on our
  // parent.  Otherwise, we need to match aNewParentContext.
  bool relevantLinkVisited = aStyleContext->IsLinkContext() ?
    aStyleContext->RelevantLinkVisited() :
    aNewParentContext->RelevantLinkVisited();

  if (relevantLinkVisited) {
    flags |= eIsVisitedLink;
  }

  if (pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
      pseudoType == nsCSSPseudoElements::ePseudo_before ||
      pseudoType == nsCSSPseudoElements::ePseudo_after) {
    flags |= eDoAnimation;
  }

  if (aElement && aElement->IsRootOfAnonymousSubtree()) {
    // For anonymous subtree roots, don't tweak "display" value based on
    // whether or not the parent is styled as a flex container. (If the parent
    // has anonymous-subtree kids, then we know it's not actually going to get
    // a flex container frame, anyway.)
    flags |= eSkipFlexItemStyleFixup;
  }

  return GetContext(aNewParentContext, ruleNode, visitedRuleNode,
                    pseudoTag, pseudoType,
                    aElement, flags);
}

struct MOZ_STACK_CLASS StatefulData : public StateRuleProcessorData {
  StatefulData(nsPresContext* aPresContext, Element* aElement,
               nsEventStates aStateMask, TreeMatchContext& aTreeMatchContext)
    : StateRuleProcessorData(aPresContext, aElement, aStateMask,
                             aTreeMatchContext),
      mHint(nsRestyleHint(0))
  {}
  nsRestyleHint   mHint;
};

static bool SheetHasDocumentStateStyle(nsIStyleRuleProcessor* aProcessor,
                                         void *aData)
{
  StatefulData* data = (StatefulData*)aData;
  if (aProcessor->HasDocumentStateDependentStyle(data)) {
    data->mHint = eRestyle_Self;
    return false; // don't continue
  }
  return true; // continue
}

// Test if style is dependent on a document state.
bool
nsStyleSet::HasDocumentStateDependentStyle(nsPresContext* aPresContext,
                                           nsIContent*    aContent,
                                           nsEventStates  aStateMask)
{
  if (!aContent || !aContent->IsElement())
    return false;

  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aContent->OwnerDoc());
  StatefulData data(aPresContext, aContent->AsElement(), aStateMask,
                    treeContext);
  WalkRuleProcessors(SheetHasDocumentStateStyle, &data, true);
  return data.mHint != 0;
}

static bool SheetHasStatefulStyle(nsIStyleRuleProcessor* aProcessor,
                                    void *aData)
{
  StatefulData* data = (StatefulData*)aData;
  nsRestyleHint hint = aProcessor->HasStateDependentStyle(data);
  data->mHint = nsRestyleHint(data->mHint | hint);
  return true; // continue
}

// Test if style is dependent on content state
nsRestyleHint
nsStyleSet::HasStateDependentStyle(nsPresContext*       aPresContext,
                                   Element*             aElement,
                                   nsEventStates        aStateMask)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  StatefulData data(aPresContext, aElement, aStateMask, treeContext);
  WalkRuleProcessors(SheetHasStatefulStyle, &data, false);
  return data.mHint;
}

struct MOZ_STACK_CLASS AttributeData : public AttributeRuleProcessorData {
  AttributeData(nsPresContext* aPresContext,
                Element* aElement, nsIAtom* aAttribute, int32_t aModType,
                bool aAttrHasChanged, TreeMatchContext& aTreeMatchContext)
    : AttributeRuleProcessorData(aPresContext, aElement, aAttribute, aModType,
                                 aAttrHasChanged, aTreeMatchContext),
      mHint(nsRestyleHint(0))
  {}
  nsRestyleHint   mHint;
};

static bool
SheetHasAttributeStyle(nsIStyleRuleProcessor* aProcessor, void *aData)
{
  AttributeData* data = (AttributeData*)aData;
  nsRestyleHint hint = aProcessor->HasAttributeDependentStyle(data);
  data->mHint = nsRestyleHint(data->mHint | hint);
  return true; // continue
}

// Test if style is dependent on content state
nsRestyleHint
nsStyleSet::HasAttributeDependentStyle(nsPresContext* aPresContext,
                                       Element*       aElement,
                                       nsIAtom*       aAttribute,
                                       int32_t        aModType,
                                       bool           aAttrHasChanged)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  AttributeData data(aPresContext, aElement, aAttribute,
                     aModType, aAttrHasChanged, treeContext);
  WalkRuleProcessors(SheetHasAttributeStyle, &data, false);
  return data.mHint;
}

bool
nsStyleSet::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  // We can't use WalkRuleProcessors without a content node.
  bool stylesChanged = false;
  for (uint32_t i = 0; i < ArrayLength(mRuleProcessors); ++i) {
    nsIStyleRuleProcessor *processor = mRuleProcessors[i];
    if (!processor) {
      continue;
    }
    bool thisChanged = processor->MediumFeaturesChanged(aPresContext);
    stylesChanged = stylesChanged || thisChanged;
  }
  for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); ++i) {
    nsIStyleRuleProcessor *processor = mScopedDocSheetRuleProcessors[i];
    bool thisChanged = processor->MediumFeaturesChanged(aPresContext);
    stylesChanged = stylesChanged || thisChanged;
  }

  if (mBindingManager) {
    bool thisChanged = false;
    mBindingManager->MediumFeaturesChanged(aPresContext, &thisChanged);
    stylesChanged = stylesChanged || thisChanged;
  }

  return stylesChanged;
}

nsCSSStyleSheet::EnsureUniqueInnerResult
nsStyleSet::EnsureUniqueInnerOnCSSSheets()
{
  nsAutoTArray<nsCSSStyleSheet*, 32> queue;
  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    nsCOMArray<nsIStyleSheet> &sheets = mSheets[gCSSSheetTypes[i]];
    for (uint32_t j = 0, j_end = sheets.Count(); j < j_end; ++j) {
      nsCSSStyleSheet *sheet = static_cast<nsCSSStyleSheet*>(sheets[j]);
      if (!queue.AppendElement(sheet)) {
        return nsCSSStyleSheet::eUniqueInner_CloneFailed;
      }
    }
  }

  if (mBindingManager) {
    mBindingManager->AppendAllSheets(queue);
  }

  nsCSSStyleSheet::EnsureUniqueInnerResult res =
    nsCSSStyleSheet::eUniqueInner_AlreadyUnique;
  while (!queue.IsEmpty()) {
    uint32_t idx = queue.Length() - 1;
    nsCSSStyleSheet *sheet = queue[idx];
    queue.RemoveElementAt(idx);

    nsCSSStyleSheet::EnsureUniqueInnerResult sheetRes =
      sheet->EnsureUniqueInner();
    if (sheetRes == nsCSSStyleSheet::eUniqueInner_CloneFailed) {
      return sheetRes;
    }
    if (sheetRes == nsCSSStyleSheet::eUniqueInner_ClonedInner) {
      res = sheetRes;
    }

    // Enqueue all the sheet's children.
    if (!sheet->AppendAllChildSheets(queue)) {
      return nsCSSStyleSheet::eUniqueInner_CloneFailed;
    }
  }
  return res;
}

nsIStyleRule*
nsStyleSet::InitialStyleRule()
{
  if (!mInitialStyleRule) {
    mInitialStyleRule = new nsInitialStyleRule;
  }
  return mInitialStyleRule;
}
