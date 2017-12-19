/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RuleProcessorCache.h"
#include "mozilla/StyleSheetInlines.h"
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
#include "nsAnimationManager.h"
#include "nsStyleSheetService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "GeckoProfiler.h"
#include "nsHTMLCSSStyleSheet.h"
#include "nsHTMLStyleSheet.h"
#include "nsCSSRules.h"
#include "nsPrintfCString.h"
#include "nsIFrame.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "nsQueryObject.h"
#include "nsStyleContextInlines.h"

#include <inttypes.h>

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsEmptyStyleRule, nsIStyleRule)

/* virtual */ void
nsEmptyStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
}

/* virtual */ bool
nsEmptyStyleRule::MightMapInheritedStyleData()
{
  return false;
}

/* virtual */ bool
nsEmptyStyleRule::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                                nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
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
                          eCSSProperty__moz_script_level) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty__moz_script_size_multiplier) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty__moz_script_min_size) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty__moz_math_variant) ||
              index == nsCSSProps::PropertyIndexInStruct(
                          eCSSProperty__moz_math_display)) {
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

/* virtual */ bool
nsInitialStyleRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsInitialStyleRule::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                                  nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
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

/* virtual */ bool
nsDisableTextZoomStyleRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsDisableTextZoomStyleRule::
GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty, nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
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

nsStyleSet::nsStyleSet()
  : mRuleTree(nullptr),
    mBatching(0),
    mStylesHaveChanged(0),
    mInShutdown(false),
    mInGC(false),
    mAuthorStyleDisabled(false),
    mInReconstruct(false),
    mNeedsRestyleAfterEnsureUniqueInner(false),
    mUsesViewportUnits(false),
    mDirty(0),
    mRootStyleContextCount(0),
#ifdef DEBUG
    mOldRootNode(nullptr),
#endif
    mUnusedRuleNodeCount(0)
{
}

nsStyleSet::~nsStyleSet()
{
  for (SheetType type : gCSSSheetTypes) {
    for (CSSStyleSheet* sheet : mSheets[type]) {
      sheet->DropStyleSet(this);
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

void
nsStyleSet::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const
{
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;

  size_t n = mallocSizeOf(this);

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
        n += mRuleProcessors[type]->SizeOfIncludingThis(mallocSizeOf);
      }
    }
    // We don't own the sheets (either the nsLayoutStyleSheetCache singleton
    // or our document owns them).
    n += mSheets[type].ShallowSizeOfExcludingThis(mallocSizeOf);
  }

  for (uint32_t i = 0; i < mScopedDocSheetRuleProcessors.Length(); i++) {
    n += mScopedDocSheetRuleProcessors[i]->SizeOfIncludingThis(mallocSizeOf);
  }
  n += mScopedDocSheetRuleProcessors.ShallowSizeOfExcludingThis(mallocSizeOf);

  aSizes.mLayoutGeckoStyleSets += n;
}

void
nsStyleSet::Init(nsPresContext* aPresContext, nsBindingManager* aBindingManager)
{
  mFirstLineRule = new nsEmptyStyleRule;
  mFirstLetterRule = new nsEmptyStyleRule;
  mPlaceholderRule = new nsEmptyStyleRule;
  mDisableTextZoomStyleRule = new nsDisableTextZoomStyleRule;

  mRuleTree = nsRuleNode::CreateRootNode(aPresContext);
  mBindingManager = aBindingManager;

  // Make an explicit GatherRuleProcessors call for the levels that
  // don't have style sheets.  The other levels will have their calls
  // triggered by DirtyRuleProcessors.
  GatherRuleProcessors(SheetType::PresHint);
  GatherRuleProcessors(SheetType::StyleAttr);
  GatherRuleProcessors(SheetType::Animation);
  GatherRuleProcessors(SheetType::Transition);
}

nsresult
nsStyleSet::BeginReconstruct()
{
  NS_ASSERTION(!mInReconstruct, "Unmatched begin/end?");
  NS_ASSERTION(mRuleTree, "Reconstructing before first construction?");
  mInReconstruct = true;

  // Clear any ArenaRefPtr-managed style contexts, as we don't want them
  // held on to after the rule tree has been reconstructed.
  PresContext()->PresShell()->ClearArenaRefPtrs(eArenaObjectID_GeckoStyleContext);

  // Clear our cached style contexts for non-inheriting anonymous boxes.
  ClearNonInheritingStyleContexts();

#ifdef DEBUG
  MOZ_ASSERT(!mOldRootNode);
  mOldRootNode = mRuleTree;
#endif

  // Create a new rule tree root, dropping the reference to our old rule tree.
  // After reconstruction, we will re-enable GC, and allow everything to be
  // collected.
  mRuleTree = nsRuleNode::CreateRootNode(mRuleTree->PresContext());

  return NS_OK;
}

void
nsStyleSet::EndReconstruct()
{
  NS_ASSERTION(mInReconstruct, "Unmatched begin/end?");
  mInReconstruct = false;
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
    // levels that do not contain CSS style sheets
    case SheetType::Animation:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] = PresContext()->EffectCompositor()->
        RuleProcessor(EffectCompositor::CascadeLevel::Animations);
      return NS_OK;
    case SheetType::Transition:
      MOZ_ASSERT(mSheets[aType].IsEmpty());
      mRuleProcessors[aType] = PresContext()->EffectCompositor()->
        RuleProcessor(EffectCompositor::CascadeLevel::Transitions);
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
    default:
      // keep going
      break;
  }
  MOZ_ASSERT(IsCSSSheetType(aType));
  if (aType == SheetType::ScopedDoc) {
    // Create a rule processor for each scope.
    uint32_t count = mSheets[SheetType::ScopedDoc].Length();
    if (count) {
      // Gather the scoped style sheets into an array as
      // CSSStyleSheets, and mark all of their scope elements
      // as scoped style roots.
      nsTArray<CSSStyleSheet*> sheets(count);
      for (CSSStyleSheet* sheet : mSheets[SheetType::ScopedDoc]) {
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
          (new nsCSSRuleProcessor(Move(sheetsForScope), aType, scope, oldRP));

        start = end;
      } while (start < count);
    }
    return NS_OK;
  }
  if (!mSheets[aType].IsEmpty()) {
    switch (aType) {
      case SheetType::Agent:
      case SheetType::User: {
        // levels containing non-scoped CSS style sheets whose rule processors
        // we want to re-use
        nsTArray<CSSStyleSheet*> sheets(mSheets[aType].Length());
        for (CSSStyleSheet* sheet : mSheets[aType]) {
          sheets.AppendElement(sheet);
        }
        nsCSSRuleProcessor* rp =
          RuleProcessorCache::GetRuleProcessor(sheets, PresContext());
        if (!rp) {
          rp = new nsCSSRuleProcessor(mSheets[aType], aType, nullptr,
                                      static_cast<nsCSSRuleProcessor*>(
                                       oldRuleProcessor.get()),
                                      true /* aIsShared */);
          nsTArray<css::DocumentRule*> documentRules;
          nsDocumentRuleResultCacheKey cacheKey;
          rp->TakeDocumentRulesAndCacheKey(PresContext(),
                                           documentRules, cacheKey);
          RuleProcessorCache::PutRuleProcessor(sheets,
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
        mRuleProcessors[aType] =
          new nsCSSRuleProcessor(mSheets[aType], aType, nullptr,
                                 static_cast<nsCSSRuleProcessor*>(
                                   oldRuleProcessor.get()));
      } break;

      default:
        MOZ_ASSERT_UNREACHABLE("non-CSS sheet types should be handled above");
        break;
    }
  }

  return NS_OK;
}

nsresult
nsStyleSet::AppendStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  bool present = mSheets[aType].RemoveElement(aSheet);
  mSheets[aType].AppendElement(aSheet);

  if (!present && IsCSSSheetType(aType)) {
    aSheet->AddStyleSet(StyleSetHandle(this));
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::PrependStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");
  bool present = mSheets[aType].RemoveElement(aSheet);
  mSheets[aType].InsertElementAt(0, aSheet);

  if (!present && IsCSSSheetType(aType)) {
    aSheet->AddStyleSet(StyleSetHandle(this));
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::RemoveStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  NS_ASSERTION(aSheet->IsComplete(),
               "Incomplete sheet being removed from style set");
  if (mSheets[aType].RemoveElement(aSheet)) {
    if (IsCSSSheetType(aType)) {
      aSheet->DropStyleSet(StyleSetHandle(this));
    }
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::ReplaceSheets(SheetType aType,
                          const nsTArray<RefPtr<CSSStyleSheet>>& aNewSheets)
{
  bool cssSheetType = IsCSSSheetType(aType);
  if (cssSheetType) {
    for (CSSStyleSheet* sheet : mSheets[aType]) {
      sheet->DropStyleSet(StyleSetHandle(this));
    }
  }

  mSheets[aType].Clear();
  mSheets[aType].AppendElements(aNewSheets);

  if (cssSheetType) {
    for (CSSStyleSheet* sheet : mSheets[aType]) {
      sheet->AddStyleSet(StyleSetHandle(this));
    }
  }

  return DirtyRuleProcessors(aType);
}

nsresult
nsStyleSet::InsertStyleSheetBefore(SheetType aType, CSSStyleSheet* aNewSheet,
                                   CSSStyleSheet* aReferenceSheet)
{
  NS_PRECONDITION(aNewSheet && aReferenceSheet, "null arg");
  NS_ASSERTION(aNewSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");

  bool present = mSheets[aType].RemoveElement(aNewSheet);
  int32_t idx = mSheets[aType].IndexOf(aReferenceSheet);
  if (idx < 0)
    return NS_ERROR_INVALID_ARG;

  mSheets[aType].InsertElementAt(idx, aNewSheet);

  if (!present && IsCSSSheetType(aType)) {
    aNewSheet->AddStyleSet(StyleSetHandle(this));
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
nsStyleSet::GetAuthorStyleDisabled() const
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
nsStyleSet::AddDocStyleSheet(CSSStyleSheet* aSheet, nsIDocument* aDocument)
{
  NS_PRECONDITION(aSheet && aDocument, "null arg");
  NS_ASSERTION(aSheet->IsApplicable(),
               "Inapplicable sheet being placed in style set");

  SheetType type = aSheet->GetScopeElement() ?
                     SheetType::ScopedDoc :
                     SheetType::Doc;
  nsTArray<RefPtr<CSSStyleSheet>>& sheets = mSheets[type];

  bool present = sheets.RemoveElement(aSheet);

  size_t index = aDocument->FindDocStyleSheetInsertionPoint(sheets, *aSheet);
  sheets.InsertElementAt(index, aSheet);

  if (!present) {
    aSheet->AddStyleSet(StyleSetHandle(this));
  }

  return DirtyRuleProcessors(type);
}

void
nsStyleSet::AppendAllXBLStyleSheets(nsTArray<StyleSheet*>& aArray) const
{
  if (mBindingManager) {
    mBindingManager->AppendAllSheets(aArray);
  }
}

nsresult
nsStyleSet::RemoveDocStyleSheet(CSSStyleSheet* aSheet)
{
  bool isScoped = aSheet->GetScopeElement();
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
already_AddRefed<GeckoStyleContext>
nsStyleSet::GetContext(GeckoStyleContext* aParentContext,
                       nsRuleNode* aRuleNode,
                       // aVisitedRuleNode may be null; if it is null
                       // it means that we don't need to force creation
                       // of a StyleIfVisited.  (But if we make one
                       // because aParentContext has one, then aRuleNode
                       // should be used.)
                       nsRuleNode* aVisitedRuleNode,
                       nsAtom* aPseudoTag,
                       CSSPseudoElementType aPseudoType,
                       Element* aElementForAnimation,
                       uint32_t aFlags)
{
  NS_PRECONDITION((!aPseudoTag &&
                   aPseudoType ==
                     CSSPseudoElementType::NotPseudo) ||
                  (aPseudoTag &&
                   nsCSSPseudoElements::GetPseudoType(
                     aPseudoTag, CSSEnabledState::eIgnoreEnabledState) ==
                   aPseudoType),
                  "Pseudo mismatch");

  if (aVisitedRuleNode == aRuleNode) {
    // No need to force creation of a visited style in this case.
    aVisitedRuleNode = nullptr;
  }

  // Ensure |aVisitedRuleNode != nullptr| corresponds to the need to
  // create an if-visited style context, and that in that case, we have
  // parentIfVisited set correctly.
  GeckoStyleContext *parentIfVisited =
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

  RefPtr<GeckoStyleContext> result;
  if (aParentContext)
    result = aParentContext->AsGecko()->FindChildWithRules(aPseudoTag, aRuleNode,
                                                aVisitedRuleNode,
                                                relevantLinkVisited);

  if (!result) {
    // |aVisitedRuleNode| may have a ref-count of zero since we are yet
    // to create the style context that will hold an owning reference to it.
    // As a result, we need to make sure it stays alive until that point
    // in case something in the first call to NS_NewStyleContext triggers a
    // GC sweep of rule nodes.
    RefPtr<nsRuleNode> kungFuDeathGrip{aVisitedRuleNode};

    result = NS_NewStyleContext(aParentContext, aPseudoTag, aPseudoType,
                                aRuleNode,
                                aFlags & eSkipParentDisplayBasedStyleFixup);
    if (aVisitedRuleNode) {
      RefPtr<GeckoStyleContext> resultIfVisited =
        NS_NewStyleContext(parentIfVisited, aPseudoTag, aPseudoType,
                           aVisitedRuleNode,
                           aFlags & eSkipParentDisplayBasedStyleFixup);
      resultIfVisited->SetIsStyleIfVisited();
      result->AsGecko()->SetStyleIfVisited(resultIfVisited.forget());

      if (relevantLinkVisited) {
        result->AddStyleBit(NS_STYLE_RELEVANT_LINK_VISITED);
      }
    }
  }
  else {
    NS_ASSERTION(result->GetPseudoType() == aPseudoType, "Unexpected type");
    NS_ASSERTION(result->GetPseudo() == aPseudoTag, "Unexpected pseudo");
  }

  if (aFlags & eDoAnimation) {

    nsIStyleRule *oldAnimRule = GetAnimationRule(aRuleNode);
    nsIStyleRule *animRule = nullptr;

    // Ignore animations for print or print preview, and for elements
    // that are not attached to the document tree.
    if (PresContext()->IsDynamic() &&
        aElementForAnimation->IsInComposedDoc()) {
      // Update CSS animations in case the animation-name has just changed.
      PresContext()->AnimationManager()->UpdateAnimations(result->AsGecko(),
                                                          aElementForAnimation);
      PresContext()->EffectCompositor()->UpdateEffectProperties(
        result->AsGecko(), aElementForAnimation, result->GetPseudoType());

      animRule = PresContext()->EffectCompositor()->
                   GetAnimationRule(aElementForAnimation,
                                    result->GetPseudoType(),
                                    EffectCompositor::CascadeLevel::Animations,
                                    result);
    }

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
      // FIXME: This should use ResolveStyleWithReplacement instead (and
      // we can remove ReplaceAnimationRule), since
      // ResolveStyleWithReplacement should now be equally efficient
      // (except for the extra code complexity to be more general).
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
      aPseudoType == CSSPseudoElementType::NotPseudo &&
      PresContext()->CompatibilityMode() == eCompatibility_NavQuirks) {
    nsIDocument* doc = aElementForAnimation->GetUncomposedDoc();
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

  AutoTArray<nsIStyleRule*, 16> importantRules;
  for (nsRuleNode *node = aCurrLevelNode; node != aLastPrevLevelNode;
       node = node->GetParent()) {
    // We guarantee that we never walk the root node here, so no need
    // to null-check GetRule().  Furthermore, it must be a CSS rule.
    NS_ASSERTION(RefPtr<css::Declaration>(do_QueryObject(node->GetRule())),
                 "Unexpected non-CSS rule");

    nsIStyleRule* impRule =
      static_cast<css::Declaration*>(node->GetRule())->GetImportantStyleData();
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
    RefPtr<css::Declaration> declaration(do_QueryObject(node->GetRule()));
    NS_ASSERTION(declaration, "Unexpected non-CSS rule");

    NS_ASSERTION(!declaration->GetImportantStyleData(),
                 "Unexpected important style source");
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
    RefPtr<css::Declaration> declaration(do_QueryObject(rule));
    if (declaration) {
      RefPtr<css::StyleRule> cssRule =
        do_QueryObject(declaration->GetOwningRule());
      NS_ASSERTION(!cssRule || !cssRule->Selector(),
                   "Unexpected CSS rule");
    }
  }
}
#endif

// Enumerate the rules in a way that cares about the order of the rules.
void
nsStyleSet::FileRules(nsIStyleRuleProcessor::EnumFunc aCollectorFunc,
                      RuleProcessorData* aData, Element* aElement,
                      nsRuleWalker* aRuleWalker)
{
  AUTO_PROFILER_LABEL("nsStyleSet::FileRules", CSS);

  NS_ASSERTION(mBatching == 0, "rule processors out of date");

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
  nsRuleNode* lastPresHintRN = aRuleWalker->CurrentNode();

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
    AddImportantRules(lastDocRN, lastPresHintRN, aRuleWalker);  // doc
  }
#ifdef DEBUG
  else {
    AssertNoImportantRules(lastDocRN, lastPresHintRN);
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
  AssertNoCSSRules(lastPresHintRN, lastUserRN);
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

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleFor(Element* aElement,
                            GeckoStyleContext* aParentContext)
{
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
  return ResolveStyleFor(aElement, aParentContext, treeContext);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleForInternal(Element* aElement,
                                    GeckoStyleContext* aParentContext,
                                    TreeMatchContext& aTreeMatchContext,
                                    AnimationFlag aAnimationFlag)
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

  uint32_t flags = (aAnimationFlag == eWithAnimation) ? eDoAnimation : eNoFlags;
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
                    nullptr, CSSPseudoElementType::NotPseudo,
                    aElement, flags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleFor(Element* aElement,
                            GeckoStyleContext* aParentContext,
                            TreeMatchContext& aTreeMatchContext)
{
  return ResolveStyleForInternal(aElement,
                                 aParentContext,
                                 aTreeMatchContext,
                                 eWithAnimation);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleForRules(GeckoStyleContext* aParentContext,
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
                    nullptr, CSSPseudoElementType::NotPseudo,
                    nullptr, eNoFlags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleByAddingRules(GeckoStyleContext* aBaseContext,
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
                                    CSSPseudoElementType aPseudoType,
                                    nsRestyleHint aReplacements)
{
  NS_ASSERTION(mBatching == 0, "rule processors out of date");

  MOZ_ASSERT(!aPseudoElement ==
             (aPseudoType >= CSSPseudoElementType::Count ||
              !(nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(aPseudoType) ||
                nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudoType))),
             "should have aPseudoElement only for certain pseudo elements");

  // Remove the Force bits, which we don't need and which could confuse
  // the remainingReplacements code below.
  aReplacements &= ~(eRestyle_Force | eRestyle_ForceDescendants);

  MOZ_ASSERT(!(aReplacements & ~(eRestyle_CSSTransitions |
                                 eRestyle_CSSAnimations |
                                 eRestyle_StyleAttribute |
                                 eRestyle_StyleAttribute_Animations)),
             "unexpected replacement bits");

  // This array can be hot and often grows to ~20 elements, so inline storage
  // is best.
  AutoTArray<RuleNodeInfo, 30> rules;

  const CascadeLevel* startingLevel = gCascadeLevels;
  nsRuleNode* startingNode = mRuleTree;
  if (mInReconstruct) {
    // Replace the entire path in the rule tree, since the rule tree has
    // a new root.

    for (nsRuleNode* ruleNode = aOldRuleNode; !ruleNode->IsRoot();
         ruleNode = ruleNode->GetParent()) {
      RuleNodeInfo* curRule = rules.AppendElement();
      curRule->mRule = ruleNode->GetRule();
      curRule->mLevel = ruleNode->GetLevel();
      curRule->mIsImportant = ruleNode->IsImportantRule();
      curRule->mIsAnimationRule = ruleNode->IsAnimationRule();
    }
  } else {
    if (aReplacements == nsRestyleHint(0)) {
      // Nothing to do.
      return aOldRuleNode;
    }

    // Walk up the rule tree from aOldNode to figure out the *part* of
    // the path in the rule tree that we need to replace.

    nsRestyleHint remainingReplacements = aReplacements;
    nsRuleNode* ruleNode = aOldRuleNode;
    for (const CascadeLevel *level = ArrayEnd(gCascadeLevels);
         level-- != gCascadeLevels; ) {
      SheetType nodeLevel;
      bool nodeIsImportant;
      while (!ruleNode->IsRoot() &&
             (nodeLevel = ruleNode->GetLevel()) == level->mLevel &&
             (nodeIsImportant = ruleNode->IsImportantRule()) ==
               level->mIsImportant) {
        if (!(level->mLevelReplacementHint & aReplacements)) {
          RuleNodeInfo* curRule = rules.AppendElement();
          curRule->mRule = ruleNode->GetRule();
          curRule->mLevel = nodeLevel;
          curRule->mIsImportant = nodeIsImportant;
          curRule->mIsAnimationRule = ruleNode->IsAnimationRule();
        }

        ruleNode = ruleNode->GetParent();
      }
      if (!level->mIsImportant &&
          (level->mLevelReplacementHint & aReplacements)) {
        remainingReplacements &= ~level->mLevelReplacementHint;
        if (remainingReplacements == nsRestyleHint(0)) {
          // We've found the part of the path in the rule tree we
          // need to replace.
          startingLevel = level;
          startingNode = ruleNode;
          break;
        }
      }
    }
    MOZ_ASSERT(remainingReplacements == nsRestyleHint(0),
               "unexpected replacements (but we safely handle this case by "
               "falling through to replacing the whole path");
  }

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  ruleWalker.SetCurrentNode(startingNode);
  auto rulesIndex = rules.Length();

  // We need to transfer this information between the non-!important and
  // !important phases for the style attribute level.
  nsRuleNode* lastScopedRN = nullptr;
  nsRuleNode* lastStyleAttrRN = nullptr;
  bool haveImportantStyleAttrRules = false;

  for (const CascadeLevel *level = startingLevel,
                       *levelEnd = ArrayEnd(gCascadeLevels);
       level != levelEnd; ++level) {

    bool doReplace = level->mLevelReplacementHint & aReplacements;

    ruleWalker.SetLevel(level->mLevel, level->mIsImportant,
                        level->mCheckForImportantRules && doReplace);

    if (doReplace) {
      switch (level->mLevel) {
        case SheetType::Animation: {
          if (aPseudoType == CSSPseudoElementType::NotPseudo ||
              aPseudoType == CSSPseudoElementType::before ||
              aPseudoType == CSSPseudoElementType::after) {
            nsIStyleRule* rule = PresContext()->EffectCompositor()->
              GetAnimationRule(aElement, aPseudoType,
                               EffectCompositor::CascadeLevel::Animations,
                               nullptr);
            if (rule) {
              ruleWalker.ForwardOnPossiblyCSSRule(rule);
              ruleWalker.CurrentNode()->SetIsAnimationRule();
            }
          }
          break;
        }
        case SheetType::Transition: {
          if (aPseudoType == CSSPseudoElementType::NotPseudo ||
              aPseudoType == CSSPseudoElementType::before ||
              aPseudoType == CSSPseudoElementType::after) {
            nsIStyleRule* rule = PresContext()->EffectCompositor()->
              GetAnimationRule(aElement, aPseudoType,
                               EffectCompositor::CascadeLevel::Transitions,
                               nullptr);
            if (rule) {
              ruleWalker.ForwardOnPossiblyCSSRule(rule);
              ruleWalker.CurrentNode()->SetIsAnimationRule();
            }
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
                    CSSPseudoElementType::NotPseudo) {
                ruleProcessor->ElementRulesMatching(PresContext(),
                                                    aElement,
                                                    &ruleWalker);
              } else if (aPseudoType <
                           CSSPseudoElementType::Count &&
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

      // When mIsReconstruct is true, we have rules we need to skip in
      // the array, so we need to test !doReplace here.
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

static bool
SkipsParentDisplayBasedStyleFixup(GeckoStyleContext* aStyleContext)
{
  CSSPseudoElementType type = aStyleContext->GetPseudoType();
  switch (type) {
    case CSSPseudoElementType::InheritingAnonBox:
    case CSSPseudoElementType::NonInheritingAnonBox:
       return true;
    case CSSPseudoElementType::NotPseudo:
       return false;
    default:
       return !nsCSSPseudoElements::PseudoElementIsFlexOrGridItem(type);
  }
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleWithReplacement(Element* aElement,
                                        Element* aPseudoElement,
                                        GeckoStyleContext* aNewParentContext,
                                        GeckoStyleContext* aOldStyleContext,
                                        nsRestyleHint aReplacements,
                                        uint32_t aFlags)
{
  nsRuleNode* ruleNode =
    RuleNodeWithReplacement(aElement, aPseudoElement,
                            aOldStyleContext->RuleNode(),
                            aOldStyleContext->GetPseudoType(), aReplacements);

  nsRuleNode* visitedRuleNode = nullptr;
  GeckoStyleContext* oldStyleIfVisited = aOldStyleContext->GetStyleIfVisited();
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

  CSSPseudoElementType pseudoType = aOldStyleContext->GetPseudoType();
  Element* elementForAnimation = nullptr;
  if (!(aFlags & eSkipStartingAnimations) &&
      (pseudoType == CSSPseudoElementType::NotPseudo ||
       pseudoType == CSSPseudoElementType::before ||
       pseudoType == CSSPseudoElementType::after)) {
    // We want to compute a correct elementForAnimation to pass in
    // because at this point the parameter is more than just the element
    // for animation; it's also used for the SetBodyTextColor call when
    // it's the body element.
    // However, we only want to set the flag to call UpdateAnimations
    // if we're dealing with a replacement (such as style attribute
    // replacement) that could lead to the animation property changing,
    // and we explicitly do NOT want to call UpdateAnimations when
    // we're trying to do an animation-only update.
    if (aReplacements & ~(eRestyle_CSSTransitions | eRestyle_CSSAnimations)) {
      flags |= eDoAnimation;
    }
    elementForAnimation = aElement;
#ifdef DEBUG
    {
      nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(elementForAnimation);
      NS_ASSERTION(pseudoType == CSSPseudoElementType::NotPseudo ||
                   !styleFrame ||
                   styleFrame->StyleContext()->GetPseudoType() ==
                     CSSPseudoElementType::NotPseudo,
                   "aElement should be the element and not the pseudo-element");
    }
#endif
  }

  if ((aElement && aElement->IsRootOfAnonymousSubtree()) ||
      SkipsParentDisplayBasedStyleFixup(aOldStyleContext)) {
    // For anonymous subtree roots, don't tweak "display" value based on whether
    // or not the parent is styled as a flex/grid container. (If the parent
    // has anonymous-subtree kids, then we know it's not actually going to get
    // a flex/grid container frame, anyway.)  Same for certain anonymous boxes
    // and most pseudos.
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  return GetContext(aNewParentContext, ruleNode, visitedRuleNode,
                    aOldStyleContext->GetPseudo(), pseudoType,
                    elementForAnimation, flags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleByRemovingAnimation(dom::Element* aTarget,
                                            GeckoStyleContext* aStyleContext,
                                            nsRestyleHint aWhichToRemove)
{
#ifdef DEBUG
  CSSPseudoElementType pseudoType = aStyleContext->GetPseudoType();
#endif
  MOZ_ASSERT(pseudoType == CSSPseudoElementType::NotPseudo ||
             pseudoType == CSSPseudoElementType::before ||
             pseudoType == CSSPseudoElementType::after,
             "unexpected type for animations");
  MOZ_ASSERT(PresContext()->RestyleManager()->IsGecko(),
             "stylo: the style set and restyle manager must have the same "
             "StyleBackendType");
  GeckoRestyleManager* restyleManager =
    PresContext()->RestyleManager()->AsGecko();

  bool oldSkipAnimationRules = restyleManager->SkipAnimationRules();
  restyleManager->SetSkipAnimationRules(true);

  RefPtr<GeckoStyleContext> result =
    ResolveStyleWithReplacement(aTarget, nullptr, aStyleContext->GetParent(),
                                aStyleContext, aWhichToRemove,
                                eSkipStartingAnimations);

  restyleManager->SetSkipAnimationRules(oldSkipAnimationRules);

  return result.forget();
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleWithoutAnimation(Element* aTarget,
                                         GeckoStyleContext* aParentContext)
{
  GeckoRestyleManager* restyleManager =
    PresContext()->RestyleManager()->AsGecko();

  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aTarget->OwnerDoc());
  InitStyleScopes(treeContext, aTarget);

  bool oldSkipAnimationRules = restyleManager->SkipAnimationRules();
  restyleManager->SetSkipAnimationRules(true);

  // Here we can call ResolveStyleForInternal() instead of
  // ResolveStyleWithReplacement() since we don't need any animation rules
  // (CSS Animations, Transitions and script animations). That's because
  // EffectCompositor::GetAnimationRule() skips all of animations rules if
  // SkipAnimationRules flag is true.
  RefPtr<GeckoStyleContext> result =
    ResolveStyleForInternal(aTarget,
                            aParentContext,
                            treeContext,
                            eWithoutAnimation);

  restyleManager->SetSkipAnimationRules(oldSkipAnimationRules);

  return result.forget();
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                GeckoStyleContext* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsNodeOfType(nsINode::eTEXT));
  return GetContext(aParentContext, mRuleTree, nullptr,
                    nsCSSAnonBoxes::mozText,
                    CSSPseudoElementType::InheritingAnonBox, nullptr, eNoFlags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleForFirstLetterContinuation(GeckoStyleContext* aParentContext)
{
  return GetContext(aParentContext, mRuleTree, nullptr,
                    nsCSSAnonBoxes::firstLetterContinuation,
                    CSSPseudoElementType::InheritingAnonBox, nullptr, eNoFlags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveStyleForPlaceholder()
{
  RefPtr<GeckoStyleContext>& cache =
    mNonInheritingStyleContexts[nsCSSAnonBoxes::NonInheriting::oofPlaceholder];
  if (cache) {
    RefPtr<GeckoStyleContext> retval = cache;
    return retval.forget();
  }

  RefPtr<GeckoStyleContext> retval =
    GetContext(nullptr, mRuleTree, nullptr,
               nsCSSAnonBoxes::oofPlaceholder,
               CSSPseudoElementType::NonInheritingAnonBox, nullptr, eNoFlags);
  cache = retval;
  return retval.forget();
}

void
nsStyleSet::WalkRestrictionRule(CSSPseudoElementType aPseudoType,
                                nsRuleWalker* aRuleWalker)
{
  // This needs to match GetPseudoRestriction in nsRuleNode.cpp.
  aRuleWalker->SetLevel(SheetType::Agent, false, false);
  if (aPseudoType == CSSPseudoElementType::firstLetter)
    aRuleWalker->Forward(mFirstLetterRule);
  else if (aPseudoType == CSSPseudoElementType::firstLine)
    aRuleWalker->Forward(mFirstLineRule);
  else if (aPseudoType == CSSPseudoElementType::placeholder)
    aRuleWalker->Forward(mPlaceholderRule);
}

void
nsStyleSet::WalkDisableTextZoomRule(Element* aElement, nsRuleWalker* aRuleWalker)
{
  aRuleWalker->SetLevel(SheetType::Agent, false, false);
  if (aElement->IsSVGElement(nsGkAtoms::text))
    aRuleWalker->Forward(mDisableTextZoomStyleRule);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolvePseudoElementStyleInternal(
  Element* aParentElement,
  CSSPseudoElementType aType,
  GeckoStyleContext* aParentContext,
  Element* aPseudoElement,
  AnimationFlag aAnimationFlag)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < CSSPseudoElementType::Count,
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
  if (aType == CSSPseudoElementType::before ||
      aType == CSSPseudoElementType::after) {
    if (aAnimationFlag == eWithAnimation) {
      flags |= eDoAnimation;
    }
  }

  if (!nsCSSPseudoElements::PseudoElementIsFlexOrGridItem(aType)) {
    // Only pseudo-elements that act as items in flex and grid containers
    // have parent display-based style fixup.
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  return GetContext(aParentContext, ruleNode, visitedRuleNode,
                    nsCSSPseudoElements::GetPseudoAtom(aType), aType,
                    aParentElement, flags);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolvePseudoElementStyle(Element* aParentElement,
                                      CSSPseudoElementType aType,
                                      GeckoStyleContext* aParentContext,
                                      Element* aPseudoElement)
{
  return ResolvePseudoElementStyleInternal(aParentElement,
                                           aType,
                                           aParentContext,
                                           aPseudoElement,
                                           eWithAnimation);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolvePseudoElementStyleWithoutAnimation(
  Element* aParentElement,
  CSSPseudoElementType aType,
  GeckoStyleContext* aParentContext,
  Element* aPseudoElement)
{
  return ResolvePseudoElementStyleInternal(aParentElement,
                                           aType,
                                           aParentContext,
                                           aPseudoElement,
                                           eWithoutAnimation);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                    CSSPseudoElementType aType,
                                    GeckoStyleContext* aParentContext)
{
  TreeMatchContext treeContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                               aParentElement->OwnerDoc());
  InitStyleScopes(treeContext, aParentElement);
  return ProbePseudoElementStyle(aParentElement, aType, aParentContext,
                                 treeContext);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                    CSSPseudoElementType aType,
                                    GeckoStyleContext* aParentContext,
                                    TreeMatchContext& aTreeMatchContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  NS_ASSERTION(aType < CSSPseudoElementType::Count,
               "must have pseudo element type");
  NS_ASSERTION(aParentElement, "aParentElement must not be null");

  nsAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  aTreeMatchContext.ResetForUnvisitedMatching();
  PseudoElementRuleProcessorData data(PresContext(), aParentElement,
                                      &ruleWalker, aType, aTreeMatchContext,
                                      /* aPseudoElement = */ nullptr);
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
  if (aType == CSSPseudoElementType::before ||
      aType == CSSPseudoElementType::after) {
    flags |= eDoAnimation;
  }

  if (!nsCSSPseudoElements::PseudoElementIsFlexOrGridItem(aType)) {
    // Only pseudo-elements that act as items in flex and grid containers
    // have parent display-based style fixup.
    flags |= eSkipParentDisplayBasedStyleFixup;
  }

  RefPtr<GeckoStyleContext> result =
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
    if (display->mDisplay == StyleDisplay::None ||
        content->ContentCount() == 0) {
      result = nullptr;
    }
  }

  return result.forget();
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveInheritingAnonymousBoxStyle(nsAtom* aPseudoTag,
                                               GeckoStyleContext* aParentContext)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

#ifdef DEBUG
    bool isAnonBox = nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
                     !nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag)
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
    nsTArray<css::ImportantStyleData*> importantRules;
    AppendPageRules(rules);
    for (uint32_t i = 0, i_end = rules.Length(); i != i_end; ++i) {
      css::Declaration* declaration = rules[i]->Declaration();
      declaration->SetImmutable();
      ruleWalker.Forward(declaration);
      css::ImportantStyleData* importantRule =
        declaration->GetImportantStyleData();
      if (importantRule) {
        importantRules.AppendElement(importantRule);
      }
    }
    for (uint32_t i = 0, i_end = importantRules.Length(); i != i_end; ++i) {
      ruleWalker.Forward(importantRules[i]);
    }
  }

  return GetContext(aParentContext, ruleWalker.CurrentNode(), nullptr,
                    aPseudoTag, CSSPseudoElementType::InheritingAnonBox,
                    nullptr, eSkipParentDisplayBasedStyleFixup);
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveNonInheritingAnonymousBoxStyle(nsAtom* aPseudoTag)
{
  NS_ENSURE_FALSE(mInShutdown, nullptr);

#ifdef DEBUG
    bool isAnonBox = nsCSSAnonBoxes::IsAnonBox(aPseudoTag) &&
                     nsCSSAnonBoxes::IsNonInheritingAnonBox(aPseudoTag)
#ifdef MOZ_XUL
                 && !nsCSSAnonBoxes::IsTreePseudoElement(aPseudoTag)
#endif
      ;
    NS_PRECONDITION(isAnonBox, "Unexpected pseudo");
#endif

  nsCSSAnonBoxes::NonInheriting type =
    nsCSSAnonBoxes::NonInheritingTypeForPseudoTag(aPseudoTag);
  RefPtr<GeckoStyleContext>& cache = mNonInheritingStyleContexts[type];
  if (cache) {
    RefPtr<GeckoStyleContext> retval = cache;
    return retval.forget();
  }

  nsRuleWalker ruleWalker(mRuleTree, mAuthorStyleDisabled);
  AnonBoxRuleProcessorData data(PresContext(), aPseudoTag, &ruleWalker);
  FileRules(EnumRulesMatching<AnonBoxRuleProcessorData>, &data, nullptr,
            &ruleWalker);

  MOZ_ASSERT(aPseudoTag != nsCSSAnonBoxes::pageContent,
             "If nsCSSAnonBoxes::pageContent ends up non-inheriting, move the "
             "@page handling from ResolveInheritingAnonymousBoxStyle to "
             "ResolveNonInheritingAnonymousBoxStyle");

  RefPtr<GeckoStyleContext> retval =
    GetContext(nullptr, ruleWalker.CurrentNode(), nullptr,
               aPseudoTag, CSSPseudoElementType::NonInheritingAnonBox,
               nullptr, eNoFlags);
  cache = retval;
  return retval.forget();
}

#ifdef MOZ_XUL
already_AddRefed<GeckoStyleContext>
nsStyleSet::ResolveXULTreePseudoStyle(Element* aParentElement,
                                      nsICSSAnonBoxPseudo* aPseudoTag,
                                      GeckoStyleContext* aParentContext,
                                      const mozilla::AtomArray& aInputWord)
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
                                aPseudoTag, aInputWord, treeContext);
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
                    aPseudoTag, CSSPseudoElementType::XULTree,
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
nsStyleSet::KeyframesRuleForName(nsAtom* aName)
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
nsStyleSet::CounterStyleRuleForName(nsAtom* aName)
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

already_AddRefed<gfxFontFeatureValueSet>
nsStyleSet::BuildFontFeatureValueSet()
{
  nsTArray<nsCSSFontFeatureValuesRule*> rules;
  AppendFontFeatureValuesRules(rules);

  if (rules.IsEmpty()) {
    return nullptr;
  }

  RefPtr<gfxFontFeatureValueSet> set = new gfxFontFeatureValueSet();
  for (nsCSSFontFeatureValuesRule* rule : rules) {
    const nsTArray<FontFamilyName>& familyList = rule->GetFamilyList()->mNames;
    const nsTArray<gfxFontFeatureValueSet::FeatureValues>&
      featureValues = rule->GetFeatureValues();

    for (const FontFamilyName& f : familyList) {
      set->AddFontFeatureValues(f.mName, featureValues);
    }
  }
  return set.forget();
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
}

void
nsStyleSet::Shutdown()
{
  // Make sure we drop our cached style contexts before the presshell arena
  // starts going away.
  ClearNonInheritingStyleContexts();
  mRuleTree = nullptr;
  GCRuleTrees();
  MOZ_ASSERT(mUnusedRuleNodeList.isEmpty());
  MOZ_ASSERT(mUnusedRuleNodeCount == 0);
}

void
nsStyleSet::SheetChanged(CSSStyleSheet& aStyleSheet)
{
  MOZ_ASSERT(mBatching != 0, "Should be in an update");

  if (mStylesHaveChanged) {
    return;
  }

  if (Element* scopeElement = aStyleSheet.GetScopeElement()) {
    mChangedScopeStyleRoots.AppendElement(scopeElement);
    return;
  }

  mStylesHaveChanged = true;
  // If we need to restyle everything, no need to restyle individual
  // scoped style roots.
  mChangedScopeStyleRoots.Clear();
}

void
nsStyleSet::RecordShadowStyleChange(ShadowRoot* aShadowRoot)
{
  if (mStylesHaveChanged) {
    return;
  }

  mChangedScopeStyleRoots.AppendElement(aShadowRoot->GetHost()->AsElement());
}

void
nsStyleSet::InvalidateStyleForCSSRuleChanges()
{
  MOZ_ASSERT(!mStylesHaveChanged || mChangedScopeStyleRoots.IsEmpty());

  AutoTArray<RefPtr<mozilla::dom::Element>, 1> scopeRoots;
  mChangedScopeStyleRoots.SwapElements(scopeRoots);
  mStylesHaveChanged = false;

  nsPresContext* presContext = PresContext();
  RestyleManager* restyleManager = presContext->RestyleManager()->AsGecko();
  Element* root = presContext->Document()->GetRootElement();
  if (!root) {
    // No content to restyle
    return;
  }

  if (scopeRoots.IsEmpty()) {
    // If scopeRoots is empty, we know that mStylesHaveChanged was true at
    // the beginning of this function, and that we need to restyle the whole
    // document.
    restyleManager->PostRestyleEvent(root,
                                     eRestyle_Subtree,
                                     nsChangeHint(0));
  } else {
    for (Element* scopeRoot : scopeRoots) {
      restyleManager->PostRestyleEvent(scopeRoot,
                                       eRestyle_Subtree,
                                       nsChangeHint(0));
    }
  }
}

void
nsStyleSet::GCRuleTrees()
{
  MOZ_ASSERT(!mInReconstruct);
  MOZ_ASSERT(!mInGC);
  mInGC = true;

  while (!mUnusedRuleNodeList.isEmpty()) {
    nsRuleNode* node = mUnusedRuleNodeList.popFirst();
#ifdef DEBUG
    if (node == mOldRootNode) {
      // Flag that we've GCed the old root, if any.
      mOldRootNode = nullptr;
    }
#endif
    node->Destroy();
  }

#ifdef DEBUG
  NS_ASSERTION(!mOldRootNode, "Should have GCed old root node");
  mOldRootNode = nullptr;
#endif
  mUnusedRuleNodeCount = 0;
  mInGC = false;
}

already_AddRefed<GeckoStyleContext>
nsStyleSet::ReparentStyleContext(GeckoStyleContext* aStyleContext,
                                 GeckoStyleContext* aNewParentContext,
                                 Element* aElement)
{
  MOZ_ASSERT(aStyleContext, "aStyleContext must not be null");

  // This short-circuit is OK because we don't call TryInitatingTransition
  // during style reresolution if the style context pointer hasn't changed.
  if (aStyleContext->GetParent() == aNewParentContext) {
    RefPtr<GeckoStyleContext> ret = aStyleContext;
    return ret.forget();
  }

  nsAtom* pseudoTag = aStyleContext->GetPseudo();
  CSSPseudoElementType pseudoType = aStyleContext->GetPseudoType();
  nsRuleNode* ruleNode = aStyleContext->RuleNode();

  MOZ_ASSERT(PresContext()->RestyleManager()->IsGecko(),
             "stylo: the style set and restyle manager must have the same "
             "StyleBackendType");
  NS_ASSERTION(!PresContext()->RestyleManager()->AsGecko()->SkipAnimationRules(),
               "we no longer handle SkipAnimationRules()");

  nsRuleNode* visitedRuleNode = nullptr;
  GeckoStyleContext* visitedContext = aStyleContext->GetStyleIfVisited();
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

  if (pseudoType == CSSPseudoElementType::NotPseudo ||
      pseudoType == CSSPseudoElementType::before ||
      pseudoType == CSSPseudoElementType::after) {
    flags |= eDoAnimation;
  }

  if ((aElement && aElement->IsRootOfAnonymousSubtree()) ||
      SkipsParentDisplayBasedStyleFixup(aStyleContext)) {
    // For anonymous subtree roots, don't tweak "display" value based on whether
    // or not the parent is styled as a flex/grid container. (If the parent
    // has anonymous-subtree kids, then we know it's not actually going to get
    // a flex/grid container frame, anyway.)  Same for certain anonymous boxes
    // and most pseudos.
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
               EventStates aStateMask, CSSPseudoElementType aPseudoType,
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
                                   CSSPseudoElementType aPseudoType,
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
  AttributeData(nsPresContext* aPresContext, Element* aElement,
                int32_t aNameSpaceID, nsAtom* aAttribute, int32_t aModType,
                bool aAttrHasChanged, const nsAttrValue* aOtherValue,
                TreeMatchContext& aTreeMatchContext)
    : AttributeRuleProcessorData(aPresContext, aElement, aNameSpaceID,
                                 aAttribute, aModType, aAttrHasChanged,
                                 aOtherValue, aTreeMatchContext),
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
                                       int32_t        aNameSpaceID,
                                       nsAtom*       aAttribute,
                                       int32_t        aModType,
                                       bool           aAttrHasChanged,
                                       const nsAttrValue* aOtherValue,
                                       mozilla::RestyleHintData&
                                         aRestyleHintDataResult)
{
  TreeMatchContext treeContext(false, nsRuleWalker::eLinksVisitedOrUnvisited,
                               aElement->OwnerDoc());
  InitStyleScopes(treeContext, aElement);
  AttributeData data(PresContext(), aElement, aNameSpaceID, aAttribute,
                     aModType, aAttrHasChanged, aOtherValue, treeContext);
  WalkRuleProcessors(SheetHasAttributeStyle, &data, false);
  if (!(data.mHint & eRestyle_Subtree)) {
    // No point keeping the list of selectors around if we are going to
    // restyle the whole subtree unconditionally.
    aRestyleHintDataResult = Move(data.mHintData);
  }
  return data.mHint;
}

nsRestyleHint
nsStyleSet::MediumFeaturesChanged(bool aViewportChanged)
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
    bool thisChanged =
      mBindingManager->MediumFeaturesChanged(presContext);
    stylesChanged = stylesChanged || thisChanged;
  }

  if (stylesChanged) {
    return eRestyle_Subtree;
  }
  if (aViewportChanged && mUsesViewportUnits) {
    // Rebuild all style data without rerunning selector matching.
    return eRestyle_ForceDescendants;
  }
  return nsRestyleHint(0);
}

bool
nsStyleSet::EnsureUniqueInnerOnCSSSheets()
{
  AutoTArray<StyleSheet*, 32> queue;
  for (SheetType type : gCSSSheetTypes) {
    for (StyleSheet* sheet : mSheets[type]) {
      queue.AppendElement(sheet);
    }
  }

  if (mBindingManager) {
    AutoTArray<StyleSheet*, 32> sheets;
    // XXXheycam stylo: AppendAllSheets will need to be able to return either
    // CSSStyleSheets or ServoStyleSheets, on request (and then here requesting
    // CSSStyleSheets).
    mBindingManager->AppendAllSheets(sheets);
    for (StyleSheet* sheet : sheets) {
      MOZ_ASSERT(sheet->IsGecko(), "stylo: AppendAllSheets shouldn't give us "
                                   "ServoStyleSheets yet");
      queue.AppendElement(sheet->AsGecko());
    }
  }

  while (!queue.IsEmpty()) {
    uint32_t idx = queue.Length() - 1;
    StyleSheet* sheet = queue[idx];
    queue.RemoveElementAt(idx);

    // Only call EnsureUniqueInner for complete sheets. If we do call it on
    // incomplete sheets, we'll cause problems when the sheet is actually
    // loaded. We don't care about incomplete sheets here anyway, because this
    // method is only invoked by nsPresContext::EnsureSafeToHandOutCSSRules.
    // The CSSRule objects we are handing out won't contain any rules derived
    // from incomplete sheets (because they aren't yet applied in styling).
    if (sheet->IsComplete()) {
      sheet->EnsureUniqueInner();
    }

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
  MOZ_ASSERT(PresContext()->RestyleManager()->IsGecko(),
             "stylo: the style set and restyle manager must have the same "
             "StyleBackendType");
  PresContext()->RestyleManager()->AsGecko()->ClearSelectors();
}

void
nsStyleSet::ClearNonInheritingStyleContexts()
{
  for (RefPtr<GeckoStyleContext>& ptr : mNonInheritingStyleContexts) {
    ptr = nullptr;
  }
}
