/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a CSS style sheet */

#include "mozilla/CSSStyleSheet.h"

#include "nsAtom.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "mozilla/css/NameSpaceRule.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/css/ImportRule.h"
#include "nsCSSRules.h"
#include "nsMediaList.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsQueryObject.h"
#include "nsString.h"
#include "nsStyleSet.h"
#include "nsTArray.h"
#include "nsIDOMCSSStyleSheet.h"
#include "mozilla/dom/CSSRuleList.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMNode.h"
#include "nsError.h"
#include "nsCSSParser.h"
#include "mozilla/css/Loader.h"
#include "nsNameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "mozAutoDocUpdate.h"
#include "nsRuleNode.h"
#include "nsMediaFeatures.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/Likely.h"
#include "nsComponentManagerUtils.h"
#include "NullPrincipal.h"
#include "mozilla/RuleProcessorCache.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsDOMWindowUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------
// Style Rule List for the DOM
//
class CSSRuleListImpl final : public CSSRuleList
{
public:
  explicit CSSRuleListImpl(CSSStyleSheet *aStyleSheet);

  virtual CSSStyleSheet* GetParentObject() override;

  virtual css::Rule*
  IndexedGetter(uint32_t aIndex, bool& aFound) override;
  virtual uint32_t
  Length() override;

  void DropReference() { mStyleSheet = nullptr; }

protected:
  virtual ~CSSRuleListImpl();

  CSSStyleSheet*  mStyleSheet;
};

CSSRuleListImpl::CSSRuleListImpl(CSSStyleSheet *aStyleSheet)
{
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
}

CSSRuleListImpl::~CSSRuleListImpl()
{
}

CSSStyleSheet*
CSSRuleListImpl::GetParentObject()
{
  return mStyleSheet;
}

uint32_t
CSSRuleListImpl::Length()
{
  if (!mStyleSheet) {
    return 0;
  }

  return AssertedCast<uint32_t>(mStyleSheet->StyleRuleCount());
}

css::Rule*
CSSRuleListImpl::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;

  if (mStyleSheet) {
    // ensure rules have correct parent
    mStyleSheet->EnsureUniqueInner();
    css::Rule* rule = mStyleSheet->GetStyleRuleAt(aIndex);
    if (rule) {
      aFound = true;
      return rule;
    }
  }

  // Per spec: "Return Value ... null if ... not a valid index."
  return nullptr;
}

namespace mozilla {

// -------------------------------
// CSS Style Sheet Inner Data Container
//


CSSStyleSheetInner::CSSStyleSheetInner(CORSMode aCORSMode,
                                       ReferrerPolicy aReferrerPolicy,
                                       const SRIMetadata& aIntegrity)
  : StyleSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
  MOZ_COUNT_CTOR(CSSStyleSheetInner);
}

bool
CSSStyleSheet::RebuildChildList(css::Rule* aRule,
                                ChildSheetListBuilder* aBuilder)
{
  int32_t type = aRule->GetType();
  if (type < css::Rule::IMPORT_RULE) {
    // Keep going till we get to the import rules.
    return true;
  }

  if (type != css::Rule::IMPORT_RULE) {
    // We're past all the import rules; stop the enumeration.
    return false;
  }

  // XXXbz We really need to decomtaminate all this stuff.  Is there a reason
  // that I can't just QI to ImportRule and get a CSSStyleSheet
  // directly from it?
  nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(aRule));
  NS_ASSERTION(importRule, "GetType lied");

  nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
  importRule->GetStyleSheet(getter_AddRefs(childSheet));

  // Have to do this QI to be safe, since XPConnect can fake
  // nsIDOMCSSStyleSheets
  RefPtr<CSSStyleSheet> sheet = do_QueryObject(childSheet);
  if (!sheet) {
    return true;
  }

  (*aBuilder->sheetSlot) = sheet;
  aBuilder->SetParentLinks(*aBuilder->sheetSlot);
  aBuilder->sheetSlot = &(*aBuilder->sheetSlot)->mNext;
  return true;
}

size_t
CSSStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = StyleSheet::SizeOfIncludingThis(aMallocSizeOf);
  const CSSStyleSheet* s = this;
  while (s) {
    // Each inner can be shared by multiple sheets.  So we only count the inner
    // if this sheet is the last one in the list of those sharing it.  As a
    // result, the last such sheet takes all the blame for the memory
    // consumption of the inner, which isn't ideal but it's better than
    // double-counting the inner.  We use last instead of first since the first
    // sheet may be held in the nsXULPrototypeCache and not used in a window at
    // all.
    if (s->Inner()->mSheets.LastElement() == s) {
      n += s->Inner()->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - s->mRuleCollection
    // - s->mRuleProcessors
    //
    // The following members are not measured:
    // - s->mOwnerRule, because it's non-owning
    // - s->mScopeElement, because it's non-owning

    s = s->mNext ? s->mNext->AsGecko() : nullptr;
  }
  return n;
}

CSSStyleSheetInner::CSSStyleSheetInner(CSSStyleSheetInner& aCopy,
                                       CSSStyleSheet* aPrimarySheet)
  : StyleSheetInfo(aCopy, aPrimarySheet)
{
  MOZ_COUNT_CTOR(CSSStyleSheetInner);
  for (css::Rule* rule : aCopy.mOrderedRules) {
    RefPtr<css::Rule> clone = rule->Clone();
    mOrderedRules.AppendObject(clone);
    clone->SetStyleSheet(aPrimarySheet);
  }

  StyleSheet::ChildSheetListBuilder builder = { &mFirstChild, aPrimarySheet };
  for (css::Rule* rule : mOrderedRules) {
    if (!CSSStyleSheet::RebuildChildList(rule, &builder)) {
      break;
    }
  }

  RebuildNameSpaces();
}

CSSStyleSheetInner::~CSSStyleSheetInner()
{
  MOZ_COUNT_DTOR(CSSStyleSheetInner);
  for (css::Rule* rule : mOrderedRules) {
    rule->SetStyleSheet(nullptr);
  }
}

StyleSheetInfo*
CSSStyleSheetInner::CloneFor(StyleSheet* aPrimarySheet)
{
  return new CSSStyleSheetInner(*this,
                                static_cast<CSSStyleSheet*>(aPrimarySheet));
}

void
CSSStyleSheetInner::RemoveSheet(StyleSheet* aSheet)
{
  if (aSheet == mSheets.ElementAt(0) && mSheets.Length() > 1) {
    StyleSheet* sheet = mSheets[1];
    for (css::Rule* rule : mOrderedRules) {
      rule->SetStyleSheet(sheet);
    }
  }

  // Don't do anything after this call, because superclass implementation
  // may delete this.
  StyleSheetInfo::RemoveSheet(aSheet);
}

static void
AddNamespaceRuleToMap(css::Rule* aRule, nsXMLNameSpaceMap* aMap)
{
  NS_ASSERTION(aRule->GetType() == css::Rule::NAMESPACE_RULE, "Bogus rule type");

  RefPtr<css::NameSpaceRule> nameSpaceRule = do_QueryObject(aRule);

  nsAutoString  urlSpec;
  nameSpaceRule->GetURLSpec(urlSpec);

  aMap->AddPrefix(nameSpaceRule->GetPrefix(), urlSpec);
}

void
CSSStyleSheetInner::RebuildNameSpaces()
{
  // Just nuke our existing namespace map, if any
  if (NS_SUCCEEDED(CreateNamespaceMap())) {
    for (css::Rule* rule : mOrderedRules) {
      switch (rule->GetType()) {
        case css::Rule::NAMESPACE_RULE:
          AddNamespaceRuleToMap(rule, mNameSpaceMap);
          continue;
        case css::Rule::CHARSET_RULE:
        case css::Rule::IMPORT_RULE:
          continue;
      }
      break;
    }
  }
}

nsresult
CSSStyleSheetInner::CreateNamespaceMap()
{
  mNameSpaceMap = nsXMLNameSpaceMap::Create(false);
  NS_ENSURE_TRUE(mNameSpaceMap, NS_ERROR_OUT_OF_MEMORY);
  // Override the default namespace map behavior for the null prefix to
  // return the wildcard namespace instead of the null namespace.
  mNameSpaceMap->AddPrefix(nullptr, kNameSpaceID_Unknown);
  return NS_OK;
}

size_t
CSSStyleSheetInner::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mOrderedRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mOrderedRules.Length(); i++) {
    n += mOrderedRules[i]->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mFirstChild ? mFirstChild->SizeOfIncludingThis(aMallocSizeOf) : 0;

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mSheetURI
  // - mOriginalSheetURI
  // - mBaseURI
  // - mPrincipal
  // - mNameSpaceMap
  //
  // The following members are not measured:
  // - mSheets, because it's non-owning

  return n;
}

// -------------------------------
// CSS Style Sheet
//

CSSStyleSheet::CSSStyleSheet(css::SheetParsingMode aParsingMode,
                             CORSMode aCORSMode, ReferrerPolicy aReferrerPolicy)
  : StyleSheet(StyleBackendType::Gecko, aParsingMode),
    mInRuleProcessorCache(false),
    mScopeElement(nullptr),
    mRuleProcessors(nullptr)
{
  mInner = new CSSStyleSheetInner(aCORSMode, aReferrerPolicy,
                                  SRIMetadata());
  mInner->AddSheet(this);
}

CSSStyleSheet::CSSStyleSheet(css::SheetParsingMode aParsingMode,
                             CORSMode aCORSMode,
                             ReferrerPolicy aReferrerPolicy,
                             const SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Gecko, aParsingMode),
    mInRuleProcessorCache(false),
    mScopeElement(nullptr),
    mRuleProcessors(nullptr)
{
  mInner = new CSSStyleSheetInner(aCORSMode, aReferrerPolicy,
                                  aIntegrity);
  mInner->AddSheet(this);
}

CSSStyleSheet::CSSStyleSheet(const CSSStyleSheet& aCopy,
                             CSSStyleSheet* aParentToUse,
                             dom::CSSImportRule* aOwnerRuleToUse,
                             nsIDocument* aDocumentToUse,
                             nsINode* aOwningNodeToUse)
  : StyleSheet(aCopy,
               aParentToUse,
               aOwnerRuleToUse,
               aDocumentToUse,
               aOwningNodeToUse)
  , mInRuleProcessorCache(false)
  , mScopeElement(nullptr)
  , mRuleProcessors(nullptr)
{
  if (mDirty) { // CSSOM's been there, force full copy now
    NS_ASSERTION(mInner->mComplete,
                 "Why have rules been accessed on an incomplete sheet?");
    // FIXME: handle failure?
    //
    // NOTE: It's important to call this from the subclass, since it could
    // access uninitialized members otherwise.
    EnsureUniqueInner();
  }
}

CSSStyleSheet::~CSSStyleSheet()
{
}

void
CSSStyleSheet::LastRelease()
{
  DropRuleCollection();
  // XXX The document reference is not reference counted and should
  // not be released. The document will let us know when it is going
  // away.
  if (mRuleProcessors) {
    NS_ASSERTION(mRuleProcessors->Length() == 0, "destructing sheet with rule processor reference");
    delete mRuleProcessors; // weak refs, should be empty here anyway
    mRuleProcessors = nullptr;
  }
  if (mInRuleProcessorCache) {
    RuleProcessorCache::RemoveSheet(this);
  }
}

void
CSSStyleSheet::DropRuleCollection()
{
  if (mRuleCollection) {
    mRuleCollection->DropReference();
    mRuleCollection = nullptr;
  }
}

void
CSSStyleSheet::UnlinkInner()
{
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  for (css::Rule* rule : Inner()->mOrderedRules) {
    rule->SetStyleSheet(nullptr);
  }
  Inner()->mOrderedRules.Clear();

  StyleSheet::UnlinkInner();
}

void
CSSStyleSheet::TraverseInner(nsCycleCollectionTraversalCallback &cb)
{
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  const nsCOMArray<css::Rule>& rules = Inner()->mOrderedRules;
  for (int32_t i = 0, count = rules.Count(); i < count; ++i) {
    if (!rules[i]->IsCCLeaf()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mOrderedRules[i]");
      cb.NoteXPCOMChild(rules[i]);
    }
  }

  StyleSheet::TraverseInner(cb);
}

// QueryInterface implementation for CSSStyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSSStyleSheet)
  if (aIID.Equals(NS_GET_IID(CSSStyleSheet)))
    foundInterface = reinterpret_cast<nsISupports*>(this);
  else
NS_INTERFACE_MAP_END_INHERITING(StyleSheet)

NS_IMPL_ADDREF_INHERITED(CSSStyleSheet, StyleSheet)
NS_IMPL_RELEASE_INHERITED(CSSStyleSheet, StyleSheet)

NS_IMPL_CYCLE_COLLECTION_CLASS(CSSStyleSheet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CSSStyleSheet)
  // We do not unlink mNext; our parent will handle that.  If we
  // unlinked it here, our parent would not be able to walk its list
  // of child sheets and null out the back-references to it, if we got
  // unlinked before it does.
  tmp->DropRuleCollection();
  tmp->mScopeElement = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(StyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSStyleSheet, StyleSheet)
  // We do not traverse mNext; our parent will handle that.  See
  // comments in Unlink for why.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScopeElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsresult
CSSStyleSheet::AddRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (! mRuleProcessors) {
    mRuleProcessors = new AutoTArray<nsCSSRuleProcessor*, 8>();
    if (!mRuleProcessors)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ASSERTION(mRuleProcessors->NoIndex == mRuleProcessors->IndexOf(aProcessor),
               "processor already registered");
  mRuleProcessors->AppendElement(aProcessor); // weak ref
  return NS_OK;
}

nsresult
CSSStyleSheet::DropRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (!mRuleProcessors)
    return NS_ERROR_FAILURE;
  return mRuleProcessors->RemoveElement(aProcessor)
           ? NS_OK
           : NS_ERROR_FAILURE;
}

bool
CSSStyleSheet::UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) const
{
  if (mMedia) {
    MOZ_ASSERT(aPresContext);
    auto media = static_cast<nsMediaList*>(mMedia.get());
    return media->Matches(aPresContext, &aKey);
  }
  return true;
}


bool
CSSStyleSheet::HasRules() const
{
  return StyleRuleCount() != 0;
}

void
CSSStyleSheet::EnabledStateChangedInternal()
{
  ClearRuleCascades();
}

void
CSSStyleSheet::AppendStyleRule(css::Rule* aRule)
{
  NS_PRECONDITION(nullptr != aRule, "null arg");

  WillDirty();
  Inner()->mOrderedRules.AppendObject(aRule);
  aRule->SetStyleSheet(this);
  DidDirty();

  if (css::Rule::NAMESPACE_RULE == aRule->GetType()) {
#ifdef DEBUG
    nsresult rv =
#endif
      RegisterNamespaceRule(aRule);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RegisterNamespaceRule returned error");
  }
}

int32_t
CSSStyleSheet::StyleRuleCount() const
{
  return Inner()->mOrderedRules.Count();
}

css::Rule*
CSSStyleSheet::GetStyleRuleAt(int32_t aIndex) const
{
  // Important: If this function is ever made scriptable, we must add
  // a security check here. See GetCssRules below for an example.
  return Inner()->mOrderedRules.SafeObjectAt(aIndex);
}

already_AddRefed<StyleSheet>
CSSStyleSheet::Clone(StyleSheet* aCloneParent,
                     dom::CSSImportRule* aCloneOwnerRule,
                     nsIDocument* aCloneDocument,
                     nsINode* aCloneOwningNode) const
{
  RefPtr<StyleSheet> clone = new CSSStyleSheet(*this,
    static_cast<CSSStyleSheet*>(aCloneParent),
    aCloneOwnerRule,
    aCloneDocument,
    aCloneOwningNode);
  return clone.forget();
}

#ifdef DEBUG
static void
ListRules(const nsCOMArray<css::Rule>& aRules, FILE* aOut, int32_t aIndent)
{
  for (int32_t index = aRules.Count() - 1; index >= 0; --index) {
    aRules.ObjectAt(index)->List(aOut, aIndent);
  }
}

void
CSSStyleSheet::List(FILE* out, int32_t aIndent) const
{
  StyleSheet::List(out, aIndent);

  fprintf_stderr(out, "%s", "Rules in source order:\n");
  ListRules(Inner()->mOrderedRules, out, aIndent);
}
#endif

void
CSSStyleSheet::ClearRuleCascades()
{
  // We might be in ClearRuleCascadesInternal because we had a modification
  // to the sheet that resulted in an nsCSSSelector being destroyed.
  // Tell the RestyleManager for each document we're used in
  // so that they can drop any nsCSSSelector pointers (used for
  // eRestyle_SomeDescendants) in their mPendingRestyles.
  for (StyleSetHandle setHandle : mStyleSets) {
    setHandle->AsGecko()->ClearSelectors();
  }

  bool removedSheetFromRuleProcessorCache = false;
  if (mRuleProcessors) {
    nsCSSRuleProcessor **iter = mRuleProcessors->Elements(),
                       **end = iter + mRuleProcessors->Length();
    for(; iter != end; ++iter) {
      if (!removedSheetFromRuleProcessorCache && (*iter)->IsShared()) {
        // Since the sheet has been modified, we need to remove all
        // RuleProcessorCache entries that contain this sheet, as the
        // list of @-moz-document rules might have changed.
        RuleProcessorCache::RemoveSheet(this);
        removedSheetFromRuleProcessorCache = true;
      }
      (*iter)->ClearRuleCascades();
    }
  }

  if (mParent) {
    static_cast<CSSStyleSheet*>(mParent)->ClearRuleCascades();
  }
}

void
CSSStyleSheet::DidDirty()
{
  MOZ_ASSERT(!mInner->mComplete || mDirty,
             "caller must have called WillDirty()");
  ClearRuleCascades();
}

nsresult
CSSStyleSheet::RegisterNamespaceRule(css::Rule* aRule)
{
  if (!Inner()->mNameSpaceMap) {
    nsresult rv = Inner()->CreateNamespaceMap();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  AddNamespaceRuleToMap(aRule, Inner()->mNameSpaceMap);
  return NS_OK;
}

void
CSSStyleSheet::SetScopeElement(dom::Element* aScopeElement)
{
  mScopeElement = aScopeElement;
}

CSSRuleList*
CSSStyleSheet::GetCssRulesInternal()
{
  if (!mRuleCollection) {
    mRuleCollection = new CSSRuleListImpl(this);
  }
  return mRuleCollection;
}

uint32_t
CSSStyleSheet::InsertRuleInternal(const nsAString& aRule,
                                  uint32_t aIndex,
                                  ErrorResult& aRv)
{
  MOZ_ASSERT(mInner->mComplete);

  WillDirty();

  if (aIndex > uint32_t(Inner()->mOrderedRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  NS_ASSERTION(uint32_t(Inner()->mOrderedRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }

  nsCSSParser css(loader, this);

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  RefPtr<css::Rule> rule;
  aRv = css.ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                      mInner->mPrincipal, getter_AddRefs(rule));
  if (NS_WARN_IF(aRv.Failed())) {
    return 0;
  }

  // Hierarchy checking.
  int32_t newType = rule->GetType();

  // check that we're not inserting before a charset rule
  css::Rule* nextRule = Inner()->mOrderedRules.SafeObjectAt(aIndex);
  if (nextRule) {
    int32_t nextType = nextRule->GetType();
    if (nextType == css::Rule::CHARSET_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }

    if (nextType == css::Rule::IMPORT_RULE &&
        newType != css::Rule::CHARSET_RULE &&
        newType != css::Rule::IMPORT_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }

    if (nextType == css::Rule::NAMESPACE_RULE &&
        newType != css::Rule::CHARSET_RULE &&
        newType != css::Rule::IMPORT_RULE &&
        newType != css::Rule::NAMESPACE_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }
  }

  if (aIndex != 0) {
    // no inserting charset at nonzero position
    if (newType == css::Rule::CHARSET_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }

    css::Rule* prevRule = Inner()->mOrderedRules.SafeObjectAt(aIndex - 1);
    int32_t prevType = prevRule->GetType();

    if (newType == css::Rule::IMPORT_RULE &&
        prevType != css::Rule::CHARSET_RULE &&
        prevType != css::Rule::IMPORT_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }

    if (newType == css::Rule::NAMESPACE_RULE &&
        prevType != css::Rule::CHARSET_RULE &&
        prevType != css::Rule::IMPORT_RULE &&
        prevType != css::Rule::NAMESPACE_RULE) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return 0;
    }
  }

  if (!Inner()->mOrderedRules.InsertObjectAt(rule, aIndex)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return 0;
  }

  DidDirty();

  rule->SetStyleSheet(this);

  int32_t type = rule->GetType();
  if (type == css::Rule::NAMESPACE_RULE) {
    // XXXbz does this screw up when inserting a namespace rule before
    // another namespace rule that binds the same prefix to a different
    // namespace?
    aRv = RegisterNamespaceRule(rule);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }
  }

  // We don't notify immediately for @import rules, but rather when
  // the sheet the rule is importing is loaded (see StyleSheetLoaded)
  if (type != css::Rule::IMPORT_RULE || !RuleHasPendingChildSheet(rule)) {
    RuleAdded(*rule);
  }

  return aIndex;
}

void
CSSStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
  // XXX TBI: handle @rule types
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  WillDirty();

  if (aIndex >= uint32_t(Inner()->mOrderedRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  NS_ASSERTION(uint32_t(Inner()->mOrderedRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  // Hold a strong ref to the rule so it doesn't die when we RemoveObjectAt
  RefPtr<css::Rule> rule = Inner()->mOrderedRules.ObjectAt(aIndex);
  if (rule) {
    Inner()->mOrderedRules.RemoveObjectAt(aIndex);
    rule->SetStyleSheet(nullptr);
    RuleRemoved(*rule);
  }
}

nsresult
CSSStyleSheet::InsertRuleIntoGroupInternal(const nsAString& aRule,
                                           css::GroupRule* aGroup,
                                           uint32_t aIndex)
{
  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }

  nsCSSParser css(loader, this);

  RefPtr<css::Rule> rule;
  nsresult result = css.ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                                  mInner->mPrincipal, getter_AddRefs(rule));
  if (NS_FAILED(result))
    return result;

  switch (rule->GetType()) {
    case css::Rule::STYLE_RULE:
    case css::Rule::MEDIA_RULE:
    case css::Rule::FONT_FACE_RULE:
    case css::Rule::PAGE_RULE:
    case css::Rule::KEYFRAMES_RULE:
    case css::Rule::COUNTER_STYLE_RULE:
    case css::Rule::DOCUMENT_RULE:
    case css::Rule::SUPPORTS_RULE:
      // these types are OK to insert into a group
      break;
    case css::Rule::CHARSET_RULE:
    case css::Rule::IMPORT_RULE:
    case css::Rule::NAMESPACE_RULE:
      // these aren't
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    default:
      NS_NOTREACHED("unexpected rule type");
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  return aGroup->InsertStyleRuleAt(aIndex, rule);
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
CSSStyleSheet::StyleSheetLoaded(StyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus)
{
  MOZ_ASSERT(aSheet->IsGecko(),
             "why we were called back with a ServoStyleSheet?");

  CSSStyleSheet* sheet = aSheet->AsGecko();

  if (sheet->GetParentSheet() == nullptr) {
    return NS_OK; // ignore if sheet has been detached already (see parseSheet)
  }
  NS_ASSERTION(this == sheet->GetParentSheet(),
               "We are being notified of a sheet load for a sheet that is not our child!");

  if (NS_SUCCEEDED(aStatus)) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
    RuleAdded(*sheet->GetOwnerRule());
  }

  return NS_OK;
}

nsresult
CSSStyleSheet::ReparseSheet(const nsAString& aInput)
{
  // Not doing this if the sheet is not complete!
  if (!mInner->mComplete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  } else {
    loader = new css::Loader(StyleBackendType::Gecko, nullptr);
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  WillDirty();

  // detach existing rules (including child sheets via import rules)
  css::LoaderReusableStyleSheets reusableSheets;
  int ruleCount;
  while ((ruleCount = Inner()->mOrderedRules.Count()) != 0) {
    RefPtr<css::Rule> rule = Inner()->mOrderedRules.ObjectAt(ruleCount - 1);
    Inner()->mOrderedRules.RemoveObjectAt(ruleCount - 1);
    rule->SetStyleSheet(nullptr);
    if (rule->GetType() == css::Rule::IMPORT_RULE) {
      nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(rule));
      NS_ASSERTION(importRule, "GetType lied");

      nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
      importRule->GetStyleSheet(getter_AddRefs(childSheet));

      RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(childSheet);
      if (cssSheet && cssSheet->GetOriginalURI()) {
        reusableSheets.AddReusableSheet(cssSheet);
      }
    }
    RuleRemoved(*rule);
  }

  // nuke child sheets list and current namespace map
  for (StyleSheet* child = GetFirstChild(); child; ) {
    NS_ASSERTION(child->mParent == this, "Child sheet is not parented to this!");
    StyleSheet* next = child->mNext;
    child->mParent = nullptr;
    child->SetAssociatedDocument(nullptr, NotOwnedByDocument);
    child->mNext = nullptr;
    child = next;
  }
  SheetInfo().mFirstChild = nullptr;
  Inner()->mNameSpaceMap = nullptr;

  uint32_t lineNumber = 1;
  if (mOwningNode) {
    nsCOMPtr<nsIStyleSheetLinkingElement> link = do_QueryInterface(mOwningNode);
    if (link) {
      lineNumber = link->GetLineNumber();
    }
  }

  nsCSSParser parser(loader, this);
  nsresult rv = parser.ParseSheet(aInput, mInner->mSheetURI, mInner->mBaseURI,
                                  mInner->mPrincipal, lineNumber, &reusableSheets);
  DidDirty(); // we are always 'dirty' here since we always remove rules first
  NS_ENSURE_SUCCESS(rv, rv);

  // notify document of all new rules
  for (int32_t index = 0; index < Inner()->mOrderedRules.Count(); ++index) {
    RefPtr<css::Rule> rule = Inner()->mOrderedRules.ObjectAt(index);
    if (rule->GetType() == css::Rule::IMPORT_RULE &&
        RuleHasPendingChildSheet(rule)) {
      continue; // notify when loaded (see StyleSheetLoaded)
    }
    RuleAdded(*rule);
  }
  return NS_OK;
}

} // namespace mozilla
