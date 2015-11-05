/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * the container for the style sheets that apply to a presentation, and
 * the internal API that the style system exposes for creating (and
 * potentially re-creating) style contexts
 */

#include "nsStyleSet.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RuleProcessorCache.h"
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
#include "nsStyleSheetService.h"
#include "mozilla/dom/Element.h"
#include "GeckoProfiler.h"
#include "nsHTMLCSSStyleSheet.h"
#include "nsHTMLStyleSheet.h"
#include "SVGAttrAnimationRuleProcessor.h"
#include "nsCSSRules.h"
#include "nsPrintfCString.h"
#include "nsIFrame.h"
#include "RestyleManager.h"
#include "nsQueryObject.h"

#include <inttypes.h>

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsEmptyStyleRule, nsIStyleRule)

/* virtual */ void
nsEmptyStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
}

#ifdef DEBUG
/* virtual */ void
nsEmptyStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t index = aIndent; --index >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[empty style rule] {}\n", indentStr.get());
}
#endif

NS_IMPL_ISUPPORTS(nsInitialStyleRule, nsIStyleRule)

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
                          eCSSProperty_script_min_size) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty_math_variant) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty_math_display)) {
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
  nsAutoCString indentStr;
  for (int32_t index = aIndent; --index >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[initial style rule] {}\n", indentStr.get());
}
#endif

NS_IMPL_ISUPPORTS(nsDisableTextZoomStyleRule, nsIStyleRule)

/* virtual */ void
nsDisableTextZoomStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (!(aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Font)))
    return;

  nsCSSValue* value = aRuleData->ValueForTextZoom();
  if (value->GetUnit() == eCSSUnit_Null)
    value->SetNoneValue();
}

#ifdef DEBUG
/* virtual */ void
nsDisableTextZoomStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t index = aIndent; --index >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[disable text zoom style rule] {}\n", indentStr.get());
}
#endif

static const SheetType gCSSSheetTypes[] = {
  // From lowest to highest in cascading order.
  SheetType::Agent,
  SheetType::User,
  SheetType::Doc,
  SheetType::ScopedDoc,
  SheetType::Override
};

static bool IsCSSSheetType(SheetType aSheetType)
{
  for (SheetType type : gCSSSheetTypes) {
    if (type == aSheetType) {
      return true;
    }
  }
  return false;
}

nsStyleSet::nsStyleSet()
  : mRuleTree(nullptr),
    mBatching(0),
    mInShutdown(false),
    mAuthorStyleDisabled(false),
    mInReconstruct(false),
    mInitFontFeatureValuesLookup(true),
    mNeedsRestyleAfterEnsureUniqueInner(false),
    mDirty(0),
    mUnusedRuleNodeCount(0)
{
}

nsStyleSet::~nsStyleSet()
{
  for (SheetType type : gCSSSheetTypes) {
    for (uint32_t i = 0, n = mSheets[type].Length(); i < n; i++) {
      static_cast<CSSStyleSheet*>(mSheets[type][i])->DropStyleSet(this);
    }
  }

  // drop reference to cached rule processors
  nsCSSRuleProcessor* rp;
  rp = static_cast<nsCSSRuleProcessor*>(mRuleProcessors[SheetType::Agent].get());
  if (rp) {
    MOZ_ASSERT(rp->IsShared());
    rp->ReleaseStyleSetRef();
  }
  rp = static_cast<nsCSSRuleProcessor*>(mRuleProcessors[SheetType::User].get());
  if (rp) {
    MOZ_ASSERT(rp->IsShared());
    rp->ReleaseStyleSetRef();
  }
}

size_t
nsStyleSet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  for (SheetType type : MakeEnumeratedRange(SheetType::Count)) {
    if (mRuleProcessors[type]) {
      bool shared = false;
      if (type == SheetType::Agent || type == SheetType::User) {
        // The only two origins we consider caching rule processors for.
        nsCSSRuleProcessor* rp =
          static_cast<nsCSSRuleProcessor*>(mRuleProcessors[type].get());
        shared = rp->IsShared();
      }
      if (!shared) {
        n += mRuleProcessors[type]->SizeOfIncludingThis(aMallocSizeOf);
      }
    }
    // mSheets is a C-style array of nsCOMArrays.  We do not own the sheets in
    // the nsCOMArrays (either the nsLayoutStyleSheetCache singleton or our
    // document owns them) so we do not count the sheets here (we pass nullptr
    // as the aSizeOfElementIncludingThis argument).  All we're doing here is
    // counting the size of the nsCOMArrays' buffers.
    n += mSheets[type].SizeOfExcludingThis(nullptr, aMallocSizeOf);
  }

  for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
    n += mScopedDocSheetRuleProcessors[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mScopedDocSheetRuleProcessors.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mRoots.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mOldRuleTrees.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return n;
}

void
nsStyleSet::Init(nsPresContext *aPresContext)
{
  mFirstLineRule = new nsEmptyStyleRule;
  mFirstLetterRule = new nsEmptyStyleRule;
  mPlaceholderRule = new nsEmptyStyleRule;
  mDisableTextZoomStyleRule = new nsDisableTextZoomStyleRule;

  mRuleTree = nsRuleNode::CreateRootNode(aPresContext);

  // Make an explicit GatherRuleProcessors call for the levels that
  // don't have style sheets.  The other levels will have their calls
  // triggered by DirtyRuleProcessors.
  GatherRuleProcessors(SheetType::PresHint);
  GatherRuleProcessors(SheetType::SVGAttrAnimation);
  GatherRuleProcessors(SheetType::StyleAttr);
  GatherRuleProcessors(SheetType::Animation);
  GatherRuleProcessors(SheetType::Transition);
}

nsresult
nsStyleSet::BeginReconstruct()
{
  NS_ASSERTION(!mInReconstruct, "Unmatched begin/end?");
  NS_ASSERTION(mRuleTree, "Reconstructing before first construction?");

  // Clear any ArenaRefPtr-managed style contexts, as we don't want them
  // held on to after the rule tree has been reconstructed.
  PresContext()->PresShell()->ClearArenaRefPtrs(eArenaObjectID_nsStyleContext);

  // Create a new rule tree root
  nsRuleNode* newTree = nsRuleNode::CreateRootNode(mRuleTree->PresContext());

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
    // Since nsStyleContext's mParent and mRuleNode are immutable, and
    // style contexts own their parents, and nsStyleContext asserts in
    // its constructor that the style context and its parent are in the
    // same rule tree, we don't need to check any of the children of
    // mRoots; we only need to check the rule nodes of mRoots
    // themselves.

    NS_ASSERTION(mRoots[i]->RuleNode()->RuleTree() == mRuleTree,
                 "style context has old rule node");
  }
#endif
  // This *should* destroy the only element of mOldRuleTrees, but in
  // case of some bugs (which would trigger the above assertions), it
  // won't.
  GCRuleTrees();
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
  CSSStyleSheet* mSheet;
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
SortStyleSheetsByScope(nsTArray<CSSStyleSheet*>& aSheets)
{
  uint32_t n = aSheets.Length();
  if (n == 1) {
    return;
  }

  ScopeDepthCache cache;

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
nsStyleSet::GatherRuleProcessors(SheetType aType)
{
  NS_ENSURE_FALSE(mInShutdown, NS_ERROR_FAILURE);

  // We might be in GatherRuleProcessors because we are dropping a sheet,
  // resulting in an nsCSSSelector being destroyed.  Tell the
  // RestyleManager for each document we're used in so that they can
  // drop any nsCSSSelector pointers (used for eRestyle_SomeDescendants)
  // in their mPendingRestyles.
  if (IsCSSSheetType(aType)) {
    ClearSelectors();
  }
  nsCOMPtr<nsIStyleRuleProcessor> oldRuleProcessor(mRuleProcessors[aType]);
  nsTArray<nsCOMPtr<nsIStyleRuleProcessor>> oldScopedDocRuleProcessors;
  if (aType == SheetType::Agent || aType == SheetType::User) {
    // drop reference to cached rule processor
    nsCSSRuleProcessor* rp =
      static_cast<nsCSSRuleProcessor*>(mRuleProcessors[aType].get());
    if (rp) {
      MOZ_ASSERT(rp->IsShared());
      rp->ReleaseStyleSetRef();
    }
  }
  mRuleProcessors[aType] = nullptr;
  if (aType == SheetType::ScopedDoc) {
    for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
      nsIStyleRuleProcessor* processor = mScopedDocSheetRuleProcessors[i].get();
      Element* scope =
        static_cast<nsCSSRuleProcessor*>(processor)->GetScopeElement();
      scope->ClearIsScopedStyleRoot();
    }

    // Clear mScopedDocSheetRuleProcessors, but save it.
    oldScopedDocRuleProcessors.SwapElements(mScopedDocSheetRuleProcessors);
  }
  if (mAuthorStyleDisabled && (aType == SheetType::Doc || 
                               aType == SheetType::ScopedDoc ||
                               aType == SheetType::StyleAttr)) {
    // Don't regather if this level is disabled.  Note that we gather
    // preshint sheets no matter what, but then skip them for some
    // elements later if mAuthorStyleDisabled.
    return NS_OK;
  }
  switch (aType) {
    // handle the types for which have a rule processor that does not
    // implement the style sheet interface.
    case SheetType::Animation:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] = PresContext()->AnimationManager();
      return NS_OK;
    case SheetType::Transition:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] = PresContext()->TransitionManager();
      return NS_OK;
    case SheetType::StyleAttr:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] = PresContext()->Document()->GetInlineStyleSheet();
      return NS_OK;
    case SheetType::PresHint:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] =
        PresContext()->Document()->GetAttributeStyleSheet();
      return NS_OK;
    case SheetType::SVGAttrAnimation:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] =
        PresContext()->Document()->GetSVGAttrAnimationRuleProcessor();
      return NS_OK;
    default:
      // keep going
      break;
  }
  if (aType == SheetType::ScopedDoc) {
    // Create a rule processor for each scope.
    uint32_t count = mSheets[SheetType::ScopedDoc].Count();
    if (count) {
      // Gather the scoped style sheets into an array as
      // CSSStyleSheets, and mark all of their scope elements
      // as scoped style roots.
      nsTArray<CSSStyleSheet*> sheets(count);
      for (uint32_t i = 0; i < count; i++) {
        RefPtr<CSSStyleSheet> sheet =
          do_QueryObject(mSheets[SheetType::ScopedDoc].ObjectAt(i));
        sheets.AppendElement(sheet);

        Element* scope = sheet->GetScopeElement();
        scope->SetIsScopedStyleRoot();
      }

      // Sort the scoped style sheets so that those for the same scope are
      // adjacent and that ancestor scopes come before descendent scopes.
      SortStyleSheetsByScope(sheets);

      // Put the old scoped rule processors in a hashtable so that we
      // can retrieve them efficiently, even in edge cases like the
      // simultaneous removal and addition of a large number of elements
      // with scoped sheets.
      nsDataHashtable<nsPtrHashKey<Element>,
                      nsCSSRuleProcessor*> oldScopedRuleProcessorHash;
      for (size_t i = oldScopedDocRuleProcessors.Length(); i-- != 0; ) {
        nsCSSRuleProcessor* oldRP =
          static_cast<nsCSSRuleProcessor*>(oldScopedDocRuleProcessors[i].get());
        Element* scope = oldRP->GetScopeElement();
        MOZ_ASSERT(!oldScopedRuleProcessorHash.Get(scope),
                   "duplicate rule processors for same scope element?");
        oldScopedRuleProcessorHash.Put(scope, oldRP);
      }

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
        nsTArray<RefPtr<CSSStyleSheet>> sheetsForScope;
        sheetsForScope.AppendElements(sheets.Elements() + start, end - start);
        nsCSSRuleProcessor* oldRP = oldScopedRuleProcessorHash.Get(scope);
        mScopedDocSheetRuleProcessors.AppendElement
          (new nsCSSRuleProcessor(sheetsForScope, aType, scope, oldRP));

        start = end;
      } while (start < count);
    }
    return NS_OK;
  }
  if (mSheets[aType].Count()) {
    switch (aType) {
      case SheetType::Agent:
      case SheetType::User: {
        // levels containing non-scoped CSS style sheets whose rule processors
        // we want to re-use
        nsCOMArray<nsIStyleSheet>& sheets = mSheets[aType];
        nsTArray<RefPtr<CSSStyleSheet>> cssSheets(sheets.Count());
        for (int32_t i = 0, i_end = sheets.Count(); i < i_end; ++i) {
          RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(sheets[i]);
          NS_ASSERTION(cssSheet, "not a CSS sheet");
          cssSheets.AppendElement(cssSheet);
        }
        nsTArray<CSSStyleSheet*> cssSheetsRaw(cssSheets.Length());
        for (int32_t i = 0, i_end = cssSheets.Length(); i < i_end; ++i) {
          cssSheetsRaw.AppendElement(cssSheets[i]);
        }
        nsCSSRuleProcessor* rp =
          RuleProcessorCache::GetRuleProcessor(cssSheetsRaw, PresContext());
        if (!rp) {
          rp = new nsCSSRuleProcessor(cssSheets, aType, nullptr,
                                      static_cast<nsCSSRuleProcessor*>(
                                       oldRuleProcessor.get()),
                                      true /* aIsShared */);
          nsTArray<css::DocumentRule*> documentRules;
          nsDocumentRuleResultCacheKey cacheKey;
          rp->TakeDocumentRulesAndCacheKey(PresContext(),
                                           documentRules, cacheKey);
          RuleProcessorCache::PutRuleProcessor(cssSheetsRaw,
                                               Move(documentRules),
                                               cacheKey, rp);
        }
        mRuleProcessors[aType] = rp;
        rp->AddStyleSetRef();
        break;
      }
      case SheetType::Doc:
      case SheetType::Override: {
        // levels containing non-scoped CSS stylesheets whose rule processors
        // we don't want to re-use
        nsCOMArray<nsIStyleSheet>& sheets = mSheets[aType];
        nsTArray<RefPtr<CSSStyleSheet>> cssSheets(sheets.Count());
        for (int32_t i = 0, i_end = sheets.Count(); i < i_end; ++i) {
          RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(sheets[i]);
          NS_ASSERTION(cssSheet, "not a CSS sheet");
          cssSheets.AppendElement(cssSheet);
        }
        mRuleProcessors[aType] =
          new nsCSSRuleProcessor(cssSheets, aType, nullptr,
                                 static_cast<nsCSSRuleProcessor*>(
                                   oldRuleProcessor.get()));
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
  RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(aSheet);
  NS_ASSERTION(cssSheet, "expected aSheet to be a CSSStyleSheet");

  return cssSheet->GetScopeElement();
}

nsresult
nsStyleSet::AppendStyleSheet(SheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  bool present = mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].AppendObject(aSheet))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!present && IsCSSSheetType(aType)) {
    static_cast<CSSStyleSheet*>(aSheet)->AddStyleSet(this);
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::PrependStyleSheet(SheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  bool present = mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].InsertObjectAt(aSheet, 0))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!present && IsCSSSheetType(aType)) {
    static_cast<CSSStyleSheet*>(aSheet)->AddStyleSet(this);
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::RemoveStyleSheet(SheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsComplete(),
               "Incomplete sheet being removed from style set");
  if (mSheets[aType].RemoveObject(aSheet)) {
    if (IsCSSSheetType(aType)) {
      static_cast<CSSStyleSheet*>(aSheet)->DropStyleSet(this);
    }
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::ReplaceSheets(SheetType aType,
                          const nsCOMArray<nsIStyleSheet> &aNewSheets)
{
  bool cssSheetType = IsCSSSheetType(aType);
  if (cssSheetType) {
    for (uint32_t i = 0, n = mSheets[aType].Length(); i < n; i++) {
      static_cast<CSSStyleSheet*>(mSheets[aType][i])->DropStyleSet(this);
    }
  }

  mSheets[aType].Clear();
  if (!mSheets[aType].AppendObjects(aNewSheets))
    return NS_ERROR_OUT_OF_MEMORY;

  if (cssSheetType) {
    for (uint32_t i = 0, n = mSheets[aType].Length(); i < n; i++) {
      static_cast<CSSStyleSheet*>(mSheets[aType][i])->AddStyleSet(this);
    }
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::InsertStyleSheetBefore(SheetType aType, nsIStyleSheet *aNewSheet,
                                   nsIStyleSheet *aReferenceSheet)
{
  NS_PRECONDITION(aNewSheet && aReferenceSheet, "null arg");
  NS_ASSERTION(aNewSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");

  bool present = mSheets[aType].RemoveObject(aNewSheet);
  int32_t idx = mSheets[aType].IndexOf(aReferenceSheet);
  if (idx < 0)
    return NS_ERROR_INVALID_ARG;

  if (!mSheets[aType].InsertObjectAt(aNewSheet, idx))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!present && IsCSSSheetType(aType)) {
    static_cast<CSSStyleSheet*>(aNewSheet)->AddStyleSet(this);
  }

  return DirtyRuleProcessors(aType);
}

static inline uint32_t
DirtyBit(SheetType aType)
{
  return 1 << uint32_t(aType);
}

nsresult
nsStyleSet::DirtyRuleProcessors(SheetType aType)
{
  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= DirtyBit(aType);
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
    mDirty |= DirtyBit(SheetType::Doc) |
              DirtyBit(SheetType::ScopedDoc) |
              DirtyBit(SheetType::StyleAttr);
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

  SheetType type = IsScopedStyleSheet(aSheet) ?
                     SheetType::ScopedDoc :
                     SheetType::Doc;
  nsCOMArray<nsIStyleSheet>& sheets = mSheets[type];

  bool present = sheets.RemoveObject(aSheet);
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

  if (!present) {
    static_cast<CSSStyleSheet*>(aSheet)->AddStyleSet(this);
  }

  return DirtyRuleProcessors(type);
}

nsresult
nsStyleSet::RemoveDocStyleSheet(nsIStyleSheet *aSheet)
{
  RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(aSheet);
  bool isScoped = cssSheet && cssSheet->GetScopeElement();
  return RemoveStyleSheet(isScoped ? SheetType::ScopedDoc : SheetType::Doc,
                          aSheet);
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

  for (SheetType type : MakeEnumeratedRange(SheetType::Count)) {
    if (mDirty & DirtyBit(type)) {
      nsresult rv = GatherRuleProcessors(type);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mDirty = 0;
  return NS_OK;
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
         (aRuleNode->GetLevel() == SheetType::Transition ||
          aRuleNode->IsImportantRule());
}

static nsIStyleRule*
GetAnimationRule(nsRuleNode *aRuleNode)
{
  nsRuleNode *n = aRuleNode;
  while (IsMoreSpecificThanAnimation(n)) {
    n = n->GetParent();
  }

  if (n->IsRoot() || n->GetLevel() != SheetType::Animation) {
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
    MOZ_ASSERT(n->GetRule() == aOldAnimRule, "wrong rule");
    MOZ_ASSERT(n->GetLevel() == SheetType::Animation,
               "wrong level");
    n = n->GetParent();
  }

  MOZ_ASSERT(!IsMoreSpecificThanAnimation(n) &&
             (n->IsRoot() || n->GetLevel() != SheetType::Animation),
             "wrong level");

  if (aNewAnimRule) {
    n = n->Transition(aNewAnimRule, SheetType::Animation, false);
    n->SetIsAnimationRule();
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

  bool relevantLinkVisited = (aFlags & eIsLink) ?
    (aFlags & eIsVisitedLink) :
    (aParentContext && aParentContext->RelevantLinkVisited());

  RefPtr<nsStyleContext> result;
  if (aParentContext)
    result = aParentContext->FindChildWithRules(aPseudoTag, aRuleNode,
                                                aVisitedRuleNode,
                                                relevantLinkVisited);

#ifdef NOISY_DEBUG
  if (result)
    fprintf(stdout, "--- SharedSC %d ---\n", ++gSharedCount);
  else
    fprintf(stdout, "+++ NewSC %d +++\n", ++gNewCount);
#endif

  if (!result) {
    result = NS_NewStyleContext(aParentContext, aPseudoTag, aPseudoType,
                                aRuleNode,
                                aFlags & eSkipParentDisplayBasedStyleFixup);
    if (aVisitedRuleNode) {
      RefPtr<nsStyleContext> resultIfVisited =
        NS_NewStyleContext(parentIfVisited, aPseudoTag, aPseudoType,
                           aVisitedRuleNode,
                           aFlags & eSkipParentDisplayBasedStyleFixup);
      if (!parentIfVisited) {
        mRoots.AppendElement(resultIfVisited);
      }
      resultIfVisited->SetIsStyleIfVisited();
      result->SetStyleIfVisited(resultIfVisited.forget());

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
    MOZ_ASSERT(result->RuleNode() == aRuleNode,
               "unexpected rule node");
    MOZ_ASSERT(!result->GetStyleIfVisited() == !aVisitedRuleNode,
               "unexpected visited rule node");
    MOZ_ASSERT(!aVisitedRuleNode ||
               result->GetStyleIfVisited()->RuleNode() == aVisitedRuleNode,
               "unexpected visited rule node");
    MOZ_ASSERT(!aVisitedRuleNode ||
               oldAnimRule == GetAnimationRule(aVisitedRuleNode),
               "animation rule mismatch between rule nodes");
    if (oldAnimRule != animRule) {
      nsRuleNode *ruleNode =
        ReplaceAnimationRule(aRuleNode, oldAnimRule, animRule);
      nsRuleNode *visitedRuleNode = aVisitedRuleNode
        ? ReplaceAnimationRule(aVisitedRuleNode, oldAnimRule, animRule)
        : nullptr;
      MOZ_ASSERT(!visitedRuleNode ||
                 GetAnimationRule(ruleNode) ==
                   GetAnimationRule(visitedRuleNode),
                 "animation rule mismatch between rule nodes");
      result = GetContext(aParentContext, ruleNode, visitedRuleNode,
                          aPseudoTag, aPseudoType, nullptr,
                          aFlags & ~eDoAnimation);
    }
  }

  if (aElementForAnimation &&
      aElementForAnimation->IsHTMLElement(nsGkAtoms::body) &&
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
    NS_ASSERTION(RefPtr<css::StyleRule>(do_QueryObject(node->GetRule())),
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
    RefPtr<css::StyleRule> rule(do_QueryObject(node->GetRule()));
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
    RefPtr<css::StyleRule> cssRule(do_QueryObject(rule));
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
  PROFILER_LABEL("nsStyleSet", "FileRules",
    js::ProfileEntry::Category::CSS);

  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  // Cascading order:
  // [least important]
  //  - UA normal rules                    = Agent        normal
  //  - User normal rules                  = User         normal
  //  - Presentation hints                 = PresHint     normal
  //  - SVG Animation (highest pres hint)  = SVGAttrAnimation normal
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

  aRuleWalker->SetLevel(SheetType::Agent, false, true);
  if (mRuleProcessors[SheetType::Agent])
    (*aCollectorFunc)(mRuleProcessors[SheetType::Agent], aData);
  nsRuleNode* lastAgentRN = aRuleWalker->CurrentNode();
  bool haveImportantUARules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(SheetType::User, false, true);
  bool skipUserStyles =
    aElement && aElement->IsInNativeAnonymousSubtree();
  if (!skipUserStyles && mRuleProcessors[SheetType::User]) // NOTE: different
    (*aCollectorFunc)(mRuleProcessors[SheetType::User], aData);
  nsRuleNode* lastUserRN = aRuleWalker->CurrentNode();
  bool haveImportantUserRules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(SheetType::PresHint, false, false);
  if (mRuleProcessors[SheetType::PresHint])
    (*aCollectorFunc)(mRuleProcessors[SheetType::PresHint], aData);

  aRuleWalker->SetLevel(SheetType::SVGAttrAnimation, false, false);
  if (mRuleProcessors[SheetType::SVGAttrAnimation])
    (*aCollectorFunc)(mRuleProcessors[SheetType::SVGAttrAnimation], aData);
  nsRuleNode* lastSVGAttrAnimationRN = aRuleWalker->CurrentNode();

  aRuleWalker->SetLevel(SheetType::Doc, false, true);
  bool cutOffInheritance = false;
  if (mBindingManager && aElement) {
    // We can supply additional document-level sheets that should be walked.
    mBindingManager->WalkRules(aCollectorFunc,
                               static_cast<ElementDependentRuleProcessorData*>(aData),
                               &cutOffInheritance);
  }
  if (!skipUserStyles && !cutOffInheritance && // NOTE: different
      mRuleProcessors[SheetType::Doc])
    (*aCollectorFunc)(mRuleProcessors[SheetType::Doc], aData);
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
      aRuleWalker->SetLevel(SheetType::ScopedDoc, false, true);
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
  aRuleWalker->SetLevel(SheetType::StyleAttr, false, true);
  if (mRuleProcessors[SheetType::StyleAttr])
    (*aCollectorFunc)(mRuleProcessors[SheetType::StyleAttr], aData);
  nsRuleNode* lastStyleAttrRN = aRuleWalker->CurrentNode();
  bool haveImportantStyleAttrRules = !aRuleWalker->GetCheckForImportantRules();

  aRuleWalker->SetLevel(SheetType::Override, false, true);
  if (mRuleProcessors[SheetType::Override])
    (*aCollectorFunc)(mRuleProcessors[SheetType::Override], aData);
  nsRuleNode* lastOvrRN = aRuleWalker->CurrentNode();
  bool haveImportantOverrideRules = !aRuleWalker->GetCheckForImportantRules();

  // This needs to match IsMoreSpecificThanAnimation() above.
  aRuleWalker->SetLevel(SheetType::Animation, false, false);
  (*aCollectorFunc)(mRuleProcessors[SheetType::Animation], aData);

  if (haveAnyImportantScopedRules) {
    for (uint32_t i = lastScopedRNs.Length(); i-- != 0; ) {
      aRuleWalker->SetLevel(SheetType::ScopedDoc, true, false);
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
    aRuleWalker->SetLevel(SheetType::Doc, true, false);
    AddImportantRules(lastDocRN, lastSVGAttrAnimationRN, aRuleWalker);  // doc
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastDocRN, lastSVGAttrAnimationRN);
  }
#endif

  if (haveImportantStyleAttrRules) {
    aRuleWalker->SetLevel(SheetType::StyleAttr, true, false);
    AddImportantRules(lastStyleAttrRN, lastScopedRN, aRuleWalker);  // style attr
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastStyleAttrRN, lastScopedRN);
  }
#endif

  if (haveImportantOverrideRules) {
    aRuleWalker->SetLevel(SheetType::Override, true, false);
    AddImportantRules(lastOvrRN, lastStyleAttrRN, aRuleWalker);  // override
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastOvrRN, lastStyleAttrRN);
  }
#endif

#ifdef DEBUG
  AssertNoCSSRules(lastSVGAttrAnimationRN, lastUserRN);
#endif

  if (haveImportantUserRules) {
    aRuleWalker->SetLevel(SheetType::User, true, false);
    AddImportantRules(lastUserRN, lastAgentRN, aRuleWalker); //user
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastUserRN, lastAgentRN);
  }
#endif

  if (haveImportantUARules) {
    aRuleWalker->SetLevel(SheetType::Agent, true, false);
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
  aRuleWalker->SetLevel(SheetType::Transition, false, false);
  (*aCollectorFunc)(mRuleProcessors[SheetType::Transition], aData);
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
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  if (mRuleProcessors[SheetType::Agent])
    (*aFunc)(mRuleProcessors[SheetType::Agent], aData);

  bool skipUserStyles = aData->mElement->IsInNativeAnonymousSubtree();
  if (!skipUserStyles && mRuleProcessors[SheetType::User]) // NOTE: different
    (*aFunc)(mRuleProcessors[SheetType::User], aData);

  if (mRuleProcessors[SheetType::PresHint])
    (*aFunc)(mRuleProcessors[SheetType::PresHint], aData);

  if (mRuleProcessors[SheetType::SVGAttrAnimation])
    (*aFunc)(mRuleProcessors[SheetType::SVGAttrAnimation], aData);

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
    if (mRuleProcessors[SheetType::Doc]) // NOTE: different
      (*aFunc)(mRuleProcessors[SheetType::Doc], aData);
    if (aData->mElement->IsElementInStyleScope()) {
      for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++)
        (*aFunc)(mScopedDocSheetRuleProcessors[i], aData);
    }
  }
  if (mRuleProcessors[SheetType::StyleAttr])
    (*aFunc)(mRuleProcessors[SheetType::StyleAttr], aData);
  if (mRuleProcessors[SheetType::Override])
    (*aFunc)(mRuleProcessors[SheetType::Override], aData);
  (*aFunc)(mRuleProcessors[SheetType::Animation], aData);
  (*aFunc)(mRuleProcessors[SheetType::Transition], aData);
}

static void
InitStyleScopes(TreeMatchContext& aTreeContext, Element* aElement)
{
  if (aElement->IsElementInStyleScope()) {
    aTreeContext.InitStyleScopes(aElement->GetParentElementCrossingShadowRoot());
  }
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleFor(Element* aElement,
                            nsStyleContext* aParentContext)
{
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
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
  WalkDisableTextZoomRule(aElement, &ruleWalker);
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
  if (aTreeMatchContext.mSkippingParentDisplayBasedStyleFixup) {
    flags |= eSkipParentDisplayBasedStyleFixup;
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
  ruleWalker.SetLevel(SheetType::Doc, false, false);
  for (uint32_t i = 0; i < aRules.Length(); i++) {
    ruleWalker.ForwardOnPossiblyCSSRule(aRules.ElementAt(i));
  }

  return GetContext(aParentContext, ruleWalker.CurrentNode(), nullptr,
                    nullptr, nsCSSPseudoElements::ePseudo_NotPseudoElement,
                    nullptr, eNoFlags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleByAddingRules(nsStyleContext* aBaseContext,
                                      const nsCOMArray<nsIStyleRule> &aRules)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  ruleWalker.SetCurrentNode(aBaseContext->RuleNode());
  // This needs to be the transition sheet because that is the highest
  // level of the cascade, and thus the only thing that makes sense if
  // we are ever going to call ResolveStyleWithReplacement on the
  // resulting context.  It's also the right thing for the one case (the
  // transition manager's cover rule) where we put the result of this
  // function in the style context tree.
  ruleWalker.SetLevel(SheetType::Transition, false, false);
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

    // GetContext handles propagating RelevantLinkVisited state from the
    // parent in non-link cases; all we need to pass in is if this link
    // is visited.
    if (aBaseContext->RelevantLinkVisited()) {
      flags |= eIsVisitedLink;
    }
  }
  return GetContext(aBaseContext->GetParent(), ruleNode, visitedRuleNode,
                    aBaseContext->GetPseudo(),
                    aBaseContext->GetPseudoType(),
                    nullptr, flags);
}

struct RuleNodeInfo {
  nsIStyleRule* mRule;
  SheetType mLevel;
  bool mIsImportant;
  bool mIsAnimationRule;
};

struct CascadeLevel {
  SheetType mLevel;
  bool mIsImportant;
  bool mCheckForImportantRules;
  nsRestyleHint mLevelReplacementHint;
};

static const CascadeLevel gCascadeLevels[] = {
  { SheetType::Agent,            false, false, nsRestyleHint(0) },
  { SheetType::User,             false, false, nsRestyleHint(0) },
  { SheetType::PresHint,         false, false, nsRestyleHint(0) },
  { SheetType::SVGAttrAnimation, false, false, eRestyle_SVGAttrAnimations },
  { SheetType::Doc,              false, false, nsRestyleHint(0) },
  { SheetType::ScopedDoc,        false, false, nsRestyleHint(0) },
  { SheetType::StyleAttr,        false, true,  eRestyle_StyleAttribute |
                                               eRestyle_StyleAttribute_Animations },
  { SheetType::Override,         false, false, nsRestyleHint(0) },
  { SheetType::Animation,        false, false, eRestyle_CSSAnimations },
  { SheetType::ScopedDoc,        true,  false, nsRestyleHint(0) },
  { SheetType::Doc,              true,  false, nsRestyleHint(0) },
  { SheetType::StyleAttr,        true,  false, eRestyle_StyleAttribute |
                                               eRestyle_StyleAttribute_Animations },
  { SheetType::Override,         true,  false, nsRestyleHint(0) },
  { SheetType::User,             true,  false, nsRestyleHint(0) },
  { SheetType::Agent,            true,  false, nsRestyleHint(0) },
  { SheetType::Transition,       false, false, eRestyle_CSSTransitions },
};

nsRuleNode*
nsStyleSet::RuleNodeWithReplacement(Element* aElement,
                                    Element* aPseudoElement,
                                    nsRuleNode* aOldRuleNode,
                                    nsCSSPseudoElements::Type aPseudoType,
                                    nsRestyleHint aReplacements)
{
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  MOZ_ASSERT(!aPseudoElement ==
             (aPseudoType >= nsCSSPseudoElements::ePseudo_PseudoElementCount ||
              !(nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(aPseudoType) ||
                nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudoType))),
             "should have aPseudoElement only for certain pseudo elements");

  MOZ_ASSERT(!(aReplacements & ~(eRestyle_CSSTransitions |
                                 eRestyle_CSSAnimations |
                                 eRestyle_SVGAttrAnimations |
                                 eRestyle_StyleAttribute |
                                 eRestyle_StyleAttribute_Animations |
                                 eRestyle_Force |
                                 eRestyle_ForceDescendants)),
             "unexpected replacement bits");

  // FIXME (perf): This should probably not rebuild the whole path, but
  // only the path from the last change in the rule tree, like
  // ReplaceAnimationRule in nsStyleSet.cpp does.  (That could then
  // perhaps share this code, too?)
  // But if we do that, we'll need to pass whether we are rebuilding the
  // rule tree from ElementRestyler::RestyleSelf to avoid taking that
  // path when we're rebuilding the rule tree.

  nsTArray<RuleNodeInfo> rules;
  for (nsRuleNode* ruleNode = aOldRuleNode; !ruleNode->IsRoot();
       ruleNode = ruleNode->GetParent()) {
    RuleNodeInfo* curRule = rules.AppendElement();
    curRule->mRule = ruleNode->GetRule();
    curRule->mLevel = ruleNode->GetLevel();
    curRule->mIsImportant = ruleNode->IsImportantRule();
    curRule->mIsAnimationRule = ruleNode->IsAnimationRule();
  }

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  auto rulesIndex = rules.Length();

  // We need to transfer this information between the non-!important and
  // !important phases for the style attribute level.
  nsRuleNode* lastScopedRN = nullptr;
  nsRuleNode* lastStyleAttrRN = nullptr;
  bool haveImportantStyleAttrRules = false;

  for (const CascadeLevel *level = gCascadeLevels,
                       *levelEnd = ArrayEnd(gCascadeLevels);
       level != levelEnd; ++level) {

    bool doReplace = level->mLevelReplacementHint & aReplacements;

    ruleWalker.SetLevel(level->mLevel, level->mIsImportant,
                        level->mCheckForImportantRules && doReplace);

    if (doReplace) {
      switch (level->mLevel) {
        case SheetType::Animation: {
          if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
              aPseudoType == nsCSSPseudoElements::ePseudo_before ||
              aPseudoType == nsCSSPseudoElements::ePseudo_after) {
            nsIStyleRule* rule = PresContext()->AnimationManager()->
              GetAnimationRule(aElement, aPseudoType);
            if (rule) {
              ruleWalker.ForwardOnPossiblyCSSRule(rule);
              ruleWalker.CurrentNode()->SetIsAnimationRule();
            }
          }
          break;
        }
        case SheetType::Transition: {
          if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
              aPseudoType == nsCSSPseudoElements::ePseudo_before ||
              aPseudoType == nsCSSPseudoElements::ePseudo_after) {
            nsIStyleRule* rule = PresContext()->TransitionManager()->
              GetAnimationRule(aElement, aPseudoType);
            if (rule) {
              ruleWalker.ForwardOnPossiblyCSSRule(rule);
              ruleWalker.CurrentNode()->SetIsAnimationRule();
            }
          }
          break;
        }
        case SheetType::SVGAttrAnimation: {
          SVGAttrAnimationRuleProcessor* ruleProcessor =
            static_cast<SVGAttrAnimationRuleProcessor*>(
              mRuleProcessors[SheetType::SVGAttrAnimation].get());
          if (ruleProcessor &&
              aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
            ruleProcessor->ElementRulesMatching(aElement, &ruleWalker);
          }
          break;
        }
        case SheetType::StyleAttr: {
          if (!level->mIsImportant) {
            // First time through, we handle the non-!important rule.
            nsHTMLCSSStyleSheet* ruleProcessor =
              static_cast<nsHTMLCSSStyleSheet*>(
                mRuleProcessors[SheetType::StyleAttr].get());
            if (ruleProcessor) {
              lastScopedRN = ruleWalker.CurrentNode();
              if (aPseudoType ==
                    nsCSSPseudoElements::ePseudo_NotPseudoElement) {
                ruleProcessor->ElementRulesMatching(PresContext(),
                                                    aElement,
                                                    &ruleWalker);
              } else if (aPseudoType <
                           nsCSSPseudoElements::ePseudo_PseudoElementCount &&
                         nsCSSPseudoElements::
                           PseudoElementSupportsStyleAttribute(aPseudoType)) {
                ruleProcessor->PseudoElementRulesMatching(aPseudoElement,
                                                          aPseudoType,
                                                          &ruleWalker);
              }
              lastStyleAttrRN = ruleWalker.CurrentNode();
              haveImportantStyleAttrRules =
                !ruleWalker.GetCheckForImportantRules();
            }
          } else {
            // Second time through, we handle the !important rule(s).
            if (haveImportantStyleAttrRules) {
              AddImportantRules(lastStyleAttrRN, lastScopedRN, &ruleWalker);
            }
          }
          break;
        }
        default:
          MOZ_ASSERT(false, "unexpected result from gCascadeLevels lookup");
          break;
      }
    }

    while (rulesIndex != 0) {
      --rulesIndex;
      const RuleNodeInfo& ruleInfo = rules[rulesIndex];

      if (ruleInfo.mLevel != level->mLevel ||
          ruleInfo.mIsImportant != level->mIsImportant) {
        ++rulesIndex;
        break;
      }

      if (!doReplace) {
        ruleWalker.ForwardOnPossiblyCSSRule(ruleInfo.mRule);
        if (ruleInfo.mIsAnimationRule) {
          ruleWalker.CurrentNode()->SetIsAnimationRule();
        }
      }
    }
  }

  NS_ASSERTION(rulesIndex == 0,
               "rules are in incorrect cascading order, "
               "which means we replaced them incorrectly");

  return ruleWalker.CurrentNode();
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleWithReplacement(Element* aElement,
                                        Element* aPseudoElement,
                                        nsStyleContext* aNewParentContext,
                                        nsStyleContext* aOldStyleContext,
                                        nsRestyleHint aReplacements,
                                        uint32_t aFlags)
{
  nsRuleNode* ruleNode =
    RuleNodeWithReplacement(aElement, aPseudoElement,
                            aOldStyleContext->RuleNode(),
                            aOldStyleContext->GetPseudoType(), aReplacements);

  nsRuleNode* visitedRuleNode = nullptr;
  nsStyleContext* oldStyleIfVisited = aOldStyleContext->GetStyleIfVisited();
  if (oldStyleIfVisited) {
    if (oldStyleIfVisited->RuleNode() == aOldStyleContext->RuleNode()) {
      visitedRuleNode = ruleNode;
    } else {
      visitedRuleNode =
        RuleNodeWithReplacement(aElement, aPseudoElement,
                                oldStyleIfVisited->RuleNode(),
                                oldStyleIfVisited->GetPseudoType(),
                                aReplacements);
    }
  }

  uint32_t flags = eNoFlags;
  if (aOldStyleContext->IsLinkContext()) {
    flags |= eIsLink;

    // GetContext handles propagating RelevantLinkVisited state from the
    // parent in non-link cases; all we need to pass in is if this link
    // is visited.
    if (aOldStyleContext->RelevantLinkVisited()) {
      flags |= eIsVisitedLink;
    }
  }

  nsCSSPseudoElements::Type pseudoType = aOldStyleContext->GetPseudoType();
  Element* elementForAnimation = nullptr;
  if (!(aFlags & eSkipStartingAnimations) &&
      (pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
       pseudoType == nsCSSPseudoElements::ePseudo_before ||
       pseudoType == nsCSSPseudoElements::ePseudo_after)) {
    // We want to compute a correct elementForAnimation to pass in
    // because at this point the parameter is more than just the element
    // for animation; it's also used for the SetBodyTextColor call when
    // it's the body element.
    // However, we only want to set the flag to call CheckAnimationRule
    // if we're dealing with a replacement (such as style attribute
    // replacement) that could lead to the animation property changing,
    // and we explicitly do NOT want to call CheckAnimationRule when
    // we're trying to do an animation-only update.
    if (aReplacements & ~(eRestyle_CSSTransitions | eRestyle_CSSAnimations)) {
      flags |= eDoAnimation;
    }
    elementForAnimation = aElement;
    NS_ASSERTION(pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
                 !elementForAnimation->GetPrimaryFrame() ||
                 elementForAnimation->GetPrimaryFrame()->StyleContext()->
                     GetPseudoType() ==
                   nsCSSPseudoElements::ePseudo_NotPseudoElement,
                 "aElement should be the element and not the pseudo-element");
  }

  if (aElement && aElement->IsRootOfAnonymousSubtree()) {
    // For anonymous subtree roots, don't tweak "display" value based on whether
    // or not the parent is styled as a flex/grid container. (If the parent
    // has anonymous-subtree kids, then we know it's not actually going to get
    // a flex/grid container frame, anyway.)
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  return GetContext(aNewParentContext, ruleNode, visitedRuleNode,
                    aOldStyleContext->GetPseudo(), pseudoType,
                    elementForAnimation, flags);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleWithoutAnimation(dom::Element* aTarget,
                                         nsStyleContext* aStyleContext,
                                         nsRestyleHint aWhichToRemove)
{
#ifdef DEBUG
  nsCSSPseudoElements::Type pseudoType = aStyleContext->GetPseudoType();
#endif
  MOZ_ASSERT(pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
             pseudoType == nsCSSPseudoElements::ePseudo_before ||
             pseudoType == nsCSSPseudoElements::ePseudo_after,
             "unexpected type for animations");
  RestyleManager* restyleManager = PresContext()->RestyleManager();

  bool oldSkipAnimationRules = restyleManager->SkipAnimationRules();
  restyleManager->SetSkipAnimationRules(true);

  RefPtr<nsStyleContext> result =
    ResolveStyleWithReplacement(aTarget, nullptr, aStyleContext->GetParent(),
                                aStyleContext, aWhichToRemove,
                                eSkipStartingAnimations);

  restyleManager->SetSkipAnimationRules(oldSkipAnimationRules);

  return result.forget();
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
  aRuleWalker->SetLevel(SheetType::Agent, false, false);
  if (aPseudoType == nsCSSPseudoElements::ePseudo_firstLetter)
    aRuleWalker->Forward(mFirstLetterRule);
  else if (aPseudoType == nsCSSPseudoElements::ePseudo_firstLine)
    aRuleWalker->Forward(mFirstLineRule);
  else if (aPseudoType == nsCSSPseudoElements::ePseudo_mozPlaceholder)
    aRuleWalker->Forward(mPlaceholderRule);
}

void
nsStyleSet::WalkDisableTextZoomRule(Element* aElement, nsRuleWalker* aRuleWalker)
{
  aRuleWalker->SetLevel(SheetType::Agent, false, false);
  if (aElement->IsSVGElement(nsGkAtoms::text))
    aRuleWalker->Forward(mDisableTextZoomStyleRule);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolvePseudoElementStyle(Element* aParentElement,
                                      nsCSSPseudoElements::Type aType,
                                      nsStyleContext* aParentContext,
                                      Element* aPseudoElement)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
               "must have pseudo element type");
  NS_ASSERTION(aParentElement, "Must have parent element");

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aParentElement->OwnerDoc());
  InitStyleScopes(treeContext, aParentElement);
  PseudoElementRuleProcessorData data(PresContext(), aParentElement,
                                      &ruleWalker, aType, treeContext,
                                      aPseudoElement);
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
    // Flex and grid containers don't expect to have any pseudo-element children
    // aside from ::before and ::after.  So if we have such a child, we're not
    // actually in a flex/grid container, and we should skip flex/grid item
    // style fixup.
    flags |= eSkipParentDisplayBasedStyleFixup;
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
  InitStyleScopes(treeContext, aParentElement);
  return ProbePseudoElementStyle(aParentElement, aType, aParentContext,
                                 treeContext);
}

already_AddRefed<nsStyleContext>
nsStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                    nsCSSPseudoElements::Type aType,
                                    nsStyleContext* aParentContext,
                                    TreeMatchContext& aTreeMatchContext,
                                    Element* aPseudoElement)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
               "must have pseudo element type");
  NS_ASSERTION(aParentElement, "aParentElement must not be null");

  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  aTreeMatchContext.ResetForUnvisitedMatching();
  PseudoElementRuleProcessorData data(PresContext(), aParentElement,
                                      &ruleWalker, aType, aTreeMatchContext,
                                      aPseudoElement);
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
    // Flex and grid containers don't expect to have any pseudo-element children
    // aside from ::before and ::after.  So if we have such a child, we're not
    // actually in a flex/grid container, and we should skip flex/grid item
    // style fixup.
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  RefPtr<nsStyleContext> result =
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
                                     nsStyleContext* aParentContext,
                                     uint32_t aFlags)
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
    PresContext()->StyleSet()->AppendPageRules(rules);
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
                    nullptr, aFlags);
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
  InitStyleScopes(treeContext, aParentElement);
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
nsStyleSet::AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  nsPresContext* presContext = PresContext();
  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    if (gCSSSheetTypes[i] == SheetType::ScopedDoc)
      continue;
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc && !ruleProc->AppendFontFaceRules(presContext, aArray))
      return false;
  }
  return true;
}

nsCSSKeyframesRule*
nsStyleSet::KeyframesRuleForName(const nsString& aName)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  nsPresContext* presContext = PresContext();
  for (uint32_t i = ArrayLength(gCSSSheetTypes); i-- != 0; ) {
    if (gCSSSheetTypes[i] == SheetType::ScopedDoc)
      continue;
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (!ruleProc)
      continue;
    nsCSSKeyframesRule* result =
      ruleProc->KeyframesRuleForName(presContext, aName);
    if (result)
      return result;
  }
  return nullptr;
}

nsCSSCounterStyleRule*
nsStyleSet::CounterStyleRuleForName(const nsAString& aName)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  nsPresContext* presContext = PresContext();
  for (uint32_t i = ArrayLength(gCSSSheetTypes); i-- != 0; ) {
    if (gCSSSheetTypes[i] == SheetType::ScopedDoc)
      continue;
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (!ruleProc)
      continue;
    nsCSSCounterStyleRule *result =
      ruleProc->CounterStyleRuleForName(presContext, aName);
    if (result)
      return result;
  }
  return nullptr;
}

bool
nsStyleSet::AppendFontFeatureValuesRules(
                                 nsTArray<nsCSSFontFeatureValuesRule*>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  nsPresContext* presContext = PresContext();
  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    nsCSSRuleProcessor *ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc &&
        !ruleProc->AppendFontFeatureValuesRules(presContext, aArray))
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
    AppendFontFeatureValuesRules(rules);

    mFontFeatureValuesLookup = new gfxFontFeatureValueSet();

    uint32_t i, numRules = rules.Length();
    for (i = 0; i < numRules; i++) {
      nsCSSFontFeatureValuesRule *rule = rules[i];

      const nsTArray<FontFamilyName>& familyList = rule->GetFamilyList().GetFontlist();
      const nsTArray<gfxFontFeatureValueSet::FeatureValues>&
        featureValues = rule->GetFeatureValues();

      // for each family
      size_t f, numFam;

      numFam = familyList.Length();
      for (f = 0; f < numFam; f++) {
        mFontFeatureValuesLookup->AddFontFeatureValues(familyList[f].mName,
                                                       featureValues);
      }
    }
  }

  RefPtr<gfxFontFeatureValueSet> lookup = mFontFeatureValuesLookup;
  return lookup.forget();
}

bool
nsStyleSet::AppendPageRules(nsTArray<nsCSSPageRule*>& aArray)
{
  NS_ENSURE_FALSE(mInShutdown, false);
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  nsPresContext* presContext = PresContext();
  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    if (gCSSSheetTypes[i] == SheetType::ScopedDoc)
      continue;
    nsCSSRuleProcessor* ruleProc = static_cast<nsCSSRuleProcessor*>
                                    (mRuleProcessors[gCSSSheetTypes[i]].get());
    if (ruleProc && !ruleProc->AppendPageRules(presContext, aArray))
      return false;
  }
  return true;
}

void
nsStyleSet::BeginShutdown()
{
  mInShutdown = 1;
  mRoots.Clear(); // no longer valid, since we won't keep it up to date
}

void
nsStyleSet::Shutdown()
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
nsStyleSet::NotifyStyleContextDestroyed(nsStyleContext* aStyleContext)
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

already_AddRefed<nsStyleContext>
nsStyleSet::ReparentStyleContext(nsStyleContext* aStyleContext,
                                 nsStyleContext* aNewParentContext,
                                 Element* aElement)
{
  MOZ_ASSERT(aStyleContext, "aStyleContext must not be null");

  // This short-circuit is OK because we don't call TryStartingTransition
  // during style reresolution if the style context pointer hasn't changed.
  if (aStyleContext->GetParent() == aNewParentContext) {
    RefPtr<nsStyleContext> ret = aStyleContext;
    return ret.forget();
  }

  nsIAtom* pseudoTag = aStyleContext->GetPseudo();
  nsCSSPseudoElements::Type pseudoType = aStyleContext->GetPseudoType();
  nsRuleNode* ruleNode = aStyleContext->RuleNode();

  NS_ASSERTION(!PresContext()->RestyleManager()->SkipAnimationRules(),
               "we no longer handle SkipAnimationRules()");

  nsRuleNode* visitedRuleNode = nullptr;
  nsStyleContext* visitedContext = aStyleContext->GetStyleIfVisited();
  // Reparenting a style context just changes where we inherit from,
  // not what rules we match or what our DOM looks like.  In
  // particular, it doesn't change whether this is a style context for
  // a link.
  if (visitedContext) {
    visitedRuleNode = visitedContext->RuleNode();
  }

  uint32_t flags = eNoFlags;
  if (aStyleContext->IsLinkContext()) {
    flags |= eIsLink;

    // GetContext handles propagating RelevantLinkVisited state from the
    // parent in non-link cases; all we need to pass in is if this link
    // is visited.
    if (aStyleContext->RelevantLinkVisited()) {
      flags |= eIsVisitedLink;
    }
  }

  if (pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
      pseudoType == nsCSSPseudoElements::ePseudo_before ||
      pseudoType == nsCSSPseudoElements::ePseudo_after) {
    flags |= eDoAnimation;
  }

  if (aElement && aElement->IsRootOfAnonymousSubtree()) {
    // For anonymous subtree roots, don't tweak "display" value based on whether
    // or not the parent is styled as a flex/grid container. (If the parent
    // has anonymous-subtree kids, then we know it's not actually going to get
    // a flex/grid container frame, anyway.)
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  return GetContext(aNewParentContext, ruleNode, visitedRuleNode,
                    pseudoTag, pseudoType,
                    aElement, flags);
}

struct MOZ_STACK_CLASS StatefulData : public StateRuleProcessorData {
  StatefulData(nsPresContext* aPresContext, Element* aElement,
               EventStates aStateMask, TreeMatchContext& aTreeMatchContext)
    : StateRuleProcessorData(aPresContext, aElement, aStateMask,
                             aTreeMatchContext),
      mHint(nsRestyleHint(0))
  {}
  nsRestyleHint   mHint;
};

struct MOZ_STACK_CLASS StatefulPseudoElementData : public PseudoElementStateRuleProcessorData {
  StatefulPseudoElementData(nsPresContext* aPresContext, Element* aElement,
               EventStates aStateMask, nsCSSPseudoElements::Type aPseudoType,
               TreeMatchContext& aTreeMatchContext, Element* aPseudoElement)
    : PseudoElementStateRuleProcessorData(aPresContext, aElement, aStateMask,
                                          aPseudoType, aTreeMatchContext,
                                          aPseudoElement),
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
nsStyleSet::HasDocumentStateDependentStyle(nsIContent*    aContent,
                                           EventStates    aStateMask)
{
  if (!aContent || !aContent->IsElement())
    return false;

  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aContent->OwnerDoc());
  InitStyleScopes(treeContext, aContent->AsElement());
  StatefulData data(PresContext(), aContent->AsElement(), aStateMask,
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

static bool SheetHasStatefulPseudoElementStyle(nsIStyleRuleProcessor* aProcessor,
                                               void *aData)
{
  StatefulPseudoElementData* data = (StatefulPseudoElementData*)aData;
  nsRestyleHint hint = aProcessor->HasStateDependentStyle(data);
  data->mHint = nsRestyleHint(data->mHint | hint);
  return true; // continue
}

// Test if style is dependent on content state
nsRestyleHint
nsStyleSet::HasStateDependentStyle(Element*             aElement,
                                   EventStates          aStateMask)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
  StatefulData data(PresContext(), aElement, aStateMask, treeContext);
  WalkRuleProcessors(SheetHasStatefulStyle, &data, false);
  return data.mHint;
}

nsRestyleHint
nsStyleSet::HasStateDependentStyle(Element* aElement,
                                   nsCSSPseudoElements::Type aPseudoType,
                                   Element* aPseudoElement,
                                   EventStates aStateMask)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
  StatefulPseudoElementData data(PresContext(), aElement, aStateMask,
                                 aPseudoType, treeContext, aPseudoElement);
  WalkRuleProcessors(SheetHasStatefulPseudoElementStyle, &data, false);
  return data.mHint;
}

struct MOZ_STACK_CLASS AttributeData : public AttributeRuleProcessorData {
  AttributeData(nsPresContext* aPresContext,
                Element* aElement, nsIAtom* aAttribute, int32_t aModType,
                bool aAttrHasChanged, const nsAttrValue* aOtherValue,
                TreeMatchContext& aTreeMatchContext)
    : AttributeRuleProcessorData(aPresContext, aElement, aAttribute, aModType,
                                 aAttrHasChanged, aOtherValue, aTreeMatchContext),
      mHint(nsRestyleHint(0))
  {}
  nsRestyleHint mHint;
  RestyleHintData mHintData;
};

static bool
SheetHasAttributeStyle(nsIStyleRuleProcessor* aProcessor, void *aData)
{
  AttributeData* data = (AttributeData*)aData;
  nsRestyleHint hint =
    aProcessor->HasAttributeDependentStyle(data, data->mHintData);
  data->mHint = nsRestyleHint(data->mHint | hint);
  return true; // continue
}

// Test if style is dependent on content state
nsRestyleHint
nsStyleSet::HasAttributeDependentStyle(Element*       aElement,
                                       nsIAtom*       aAttribute,
                                       int32_t        aModType,
                                       bool           aAttrHasChanged,
                                       const nsAttrValue* aOtherValue,
                                       mozilla::RestyleHintData&
                                         aRestyleHintDataResult)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
  AttributeData data(PresContext(), aElement, aAttribute,
                     aModType, aAttrHasChanged, aOtherValue, treeContext);
  WalkRuleProcessors(SheetHasAttributeStyle, &data, false);
  if (!(data.mHint & eRestyle_Subtree)) {
    // No point keeping the list of selectors around if we are going to
    // restyle the whole subtree unconditionally.
    aRestyleHintDataResult = Move(data.mHintData);
  }
  return data.mHint;
}

bool
nsStyleSet::MediumFeaturesChanged()
{
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  // We can't use WalkRuleProcessors without a content node.
  nsPresContext* presContext = PresContext();
  bool stylesChanged = false;
  for (nsIStyleRuleProcessor* processor : mRuleProcessors) {
    if (!processor) {
      continue;
    }
    bool thisChanged = processor->MediumFeaturesChanged(presContext);
    stylesChanged = stylesChanged || thisChanged;
  }
  for (nsIStyleRuleProcessor* processor : mScopedDocSheetRuleProcessors) {
    bool thisChanged = processor->MediumFeaturesChanged(presContext);
    stylesChanged = stylesChanged || thisChanged;
  }

  if (mBindingManager) {
    bool thisChanged = false;
    mBindingManager->MediumFeaturesChanged(presContext, &thisChanged);
    stylesChanged = stylesChanged || thisChanged;
  }

  return stylesChanged;
}

bool
nsStyleSet::EnsureUniqueInnerOnCSSSheets()
{
  nsAutoTArray<CSSStyleSheet*, 32> queue;
  for (uint32_t i = 0; i < ArrayLength(gCSSSheetTypes); ++i) {
    nsCOMArray<nsIStyleSheet> &sheets = mSheets[gCSSSheetTypes[i]];
    for (uint32_t j = 0, j_end = sheets.Count(); j < j_end; ++j) {
      CSSStyleSheet* sheet = static_cast<CSSStyleSheet*>(sheets[j]);
      queue.AppendElement(sheet);
    }
  }

  if (mBindingManager) {
    mBindingManager->AppendAllSheets(queue);
  }

  while (!queue.IsEmpty()) {
    uint32_t idx = queue.Length() - 1;
    CSSStyleSheet* sheet = queue[idx];
    queue.RemoveElementAt(idx);

    sheet->EnsureUniqueInner();

    // Enqueue all the sheet's children.
    sheet->AppendAllChildSheets(queue);
  }

  bool res = mNeedsRestyleAfterEnsureUniqueInner;
  mNeedsRestyleAfterEnsureUniqueInner = false;
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

bool
nsStyleSet::HasRuleProcessorUsedByMultipleStyleSets(SheetType aSheetType)
{
  MOZ_ASSERT(size_t(aSheetType) < ArrayLength(mRuleProcessors));
  if (!IsCSSSheetType(aSheetType) || !mRuleProcessors[aSheetType]) {
    return false;
  }
  nsCSSRuleProcessor* rp =
    static_cast<nsCSSRuleProcessor*>(mRuleProcessors[aSheetType].get());
  return rp->IsUsedByMultipleStyleSets();
}

void
nsStyleSet::ClearSelectors()
{
  // We might be called before we've done our first rule tree construction.
  if (!mRuleTree) {
    return;
  }
  PresContext()->RestyleManager()->ClearSelectors();
}
