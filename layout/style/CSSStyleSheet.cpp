/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a CSS style sheet */

#include "mozilla/CSSStyleSheet.h"

#include "nsIAtom.h"
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
#include "nsICSSLoaderObserver.h"
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
#include "nsNullPrincipal.h"
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

  virtual nsIDOMCSSRule*
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

nsIDOMCSSRule*    
CSSRuleListImpl::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;

  if (mStyleSheet) {
    // ensure rules have correct parent
    mStyleSheet->EnsureUniqueInner();
    css::Rule* rule = mStyleSheet->GetStyleRuleAt(aIndex);
    if (rule) {
      aFound = true;
      return rule->GetDOMRule();
    }
  }

  // Per spec: "Return Value ... null if ... not a valid index."
  return nullptr;
}

namespace mozilla {

// -------------------------------
// CSS Style Sheet Inner Data Container
//


CSSStyleSheetInner::CSSStyleSheetInner(CSSStyleSheet* aPrimarySheet,
                                       CORSMode aCORSMode,
                                       ReferrerPolicy aReferrerPolicy,
                                       const SRIMetadata& aIntegrity)
  : StyleSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
  MOZ_COUNT_CTOR(CSSStyleSheetInner);
  mSheets.AppendElement(aPrimarySheet);
}

static bool SetStyleSheetReference(css::Rule* aRule, void* aSheet)
{
  if (aRule) {
    aRule->SetStyleSheet(static_cast<CSSStyleSheet*>(aSheet));
  }
  return true;
}

struct ChildSheetListBuilder {
  RefPtr<CSSStyleSheet>* sheetSlot;
  CSSStyleSheet* parent;

  void SetParentLinks(CSSStyleSheet* aSheet) {
    aSheet->mParent = parent;
    aSheet->SetOwningDocument(parent->mDocument);
  }

  static void ReparentChildList(CSSStyleSheet* aPrimarySheet,
                                CSSStyleSheet* aFirstChild)
  {
    for (CSSStyleSheet *child = aFirstChild; child; child = child->mNext) {
      child->mParent = aPrimarySheet;
      child->SetOwningDocument(aPrimarySheet->mDocument);
    }
  }
};
  
bool
CSSStyleSheet::RebuildChildList(css::Rule* aRule, void* aBuilder)
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

  ChildSheetListBuilder* builder =
    static_cast<ChildSheetListBuilder*>(aBuilder);

  // XXXbz We really need to decomtaminate all this stuff.  Is there a reason
  // that I can't just QI to ImportRule and get a CSSStyleSheet
  // directly from it?
  nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(aRule));
  NS_ASSERTION(importRule, "GetType lied");

  nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
  importRule->GetStyleSheet(getter_AddRefs(childSheet));

  // Have to do this QI to be safe, since XPConnect can fake
  // nsIDOMCSSStyleSheets
  RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(childSheet);
  if (!cssSheet) {
    return true;
  }

  (*builder->sheetSlot) = cssSheet;
  builder->SetParentLinks(*builder->sheetSlot);
  builder->sheetSlot = &(*builder->sheetSlot)->mNext;
  return true;
}

size_t
CSSStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const CSSStyleSheet* s = this;
  while (s) {
    n += aMallocSizeOf(s);

    // Each inner can be shared by multiple sheets.  So we only count the inner
    // if this sheet is the last one in the list of those sharing it.  As a
    // result, the last such sheet takes all the blame for the memory
    // consumption of the inner, which isn't ideal but it's better than
    // double-counting the inner.  We use last instead of first since the first
    // sheet may be held in the nsXULPrototypeCache and not used in a window at
    // all.
    if (s->mInner->mSheets.LastElement() == s) {
      n += s->mInner->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - s->mTitle
    // - s->mMedia
    // - s->mRuleCollection
    // - s->mRuleProcessors
    //
    // The following members are not measured:
    // - s->mOwnerRule, because it's non-owning

    s = s->mNext;
  }
  return n;
}

CSSStyleSheetInner::CSSStyleSheetInner(CSSStyleSheetInner& aCopy,
                                       CSSStyleSheet* aPrimarySheet)
  : StyleSheetInfo(aCopy)
{
  MOZ_COUNT_CTOR(CSSStyleSheetInner);
  AddSheet(aPrimarySheet);
  aCopy.mOrderedRules.EnumerateForwards(css::GroupRule::CloneRuleInto, &mOrderedRules);
  mOrderedRules.EnumerateForwards(SetStyleSheetReference, aPrimarySheet);

  ChildSheetListBuilder builder = { &mFirstChild, aPrimarySheet };
  mOrderedRules.EnumerateForwards(CSSStyleSheet::RebuildChildList, &builder);

  RebuildNameSpaces();
}

CSSStyleSheetInner::~CSSStyleSheetInner()
{
  MOZ_COUNT_DTOR(CSSStyleSheetInner);
  mOrderedRules.EnumerateForwards(SetStyleSheetReference, nullptr);
}

CSSStyleSheetInner*
CSSStyleSheetInner::CloneFor(CSSStyleSheet* aPrimarySheet)
{
  return new CSSStyleSheetInner(*this, aPrimarySheet);
}

void
CSSStyleSheetInner::AddSheet(CSSStyleSheet* aSheet)
{
  mSheets.AppendElement(aSheet);
}

void
CSSStyleSheetInner::RemoveSheet(CSSStyleSheet* aSheet)
{
  if (1 == mSheets.Length()) {
    NS_ASSERTION(aSheet == mSheets.ElementAt(0), "bad parent");
    delete this;
    return;
  }
  if (aSheet == mSheets.ElementAt(0)) {
    mSheets.RemoveElementAt(0);
    NS_ASSERTION(mSheets.Length(), "no parents");
    mOrderedRules.EnumerateForwards(SetStyleSheetReference,
                                    mSheets.ElementAt(0));

    ChildSheetListBuilder::ReparentChildList(mSheets[0], mFirstChild);
  }
  else {
    mSheets.RemoveElement(aSheet);
  }
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

static bool
CreateNameSpace(css::Rule* aRule, void* aNameSpacePtr)
{
  int32_t type = aRule->GetType();
  if (css::Rule::NAMESPACE_RULE == type) {
    AddNamespaceRuleToMap(aRule,
                          static_cast<nsXMLNameSpaceMap*>(aNameSpacePtr));
    return true;
  }
  // stop if not namespace, import or charset because namespace can't follow
  // anything else
  return (css::Rule::CHARSET_RULE == type || css::Rule::IMPORT_RULE == type);
}

void 
CSSStyleSheetInner::RebuildNameSpaces()
{
  // Just nuke our existing namespace map, if any
  if (NS_SUCCEEDED(CreateNamespaceMap())) {
    mOrderedRules.EnumerateForwards(CreateNameSpace, mNameSpaceMap);
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
    mParent(nullptr),
    mOwnerRule(nullptr),
    mDirty(false),
    mInRuleProcessorCache(false),
    mScopeElement(nullptr),
    mRuleProcessors(nullptr)
{
  mInner = new CSSStyleSheetInner(this, aCORSMode, aReferrerPolicy,
                                  SRIMetadata());
}

CSSStyleSheet::CSSStyleSheet(css::SheetParsingMode aParsingMode,
                             CORSMode aCORSMode,
                             ReferrerPolicy aReferrerPolicy,
                             const SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Gecko, aParsingMode),
    mParent(nullptr),
    mOwnerRule(nullptr),
    mDirty(false),
    mInRuleProcessorCache(false),
    mScopeElement(nullptr),
    mRuleProcessors(nullptr)
{
  mInner = new CSSStyleSheetInner(this, aCORSMode, aReferrerPolicy,
                                  aIntegrity);
}

CSSStyleSheet::CSSStyleSheet(const CSSStyleSheet& aCopy,
                             CSSStyleSheet* aParentToUse,
                             css::ImportRule* aOwnerRuleToUse,
                             nsIDocument* aDocumentToUse,
                             nsINode* aOwningNodeToUse)
  : StyleSheet(aCopy, aDocumentToUse, aOwningNodeToUse),
    mParent(aParentToUse),
    mOwnerRule(aOwnerRuleToUse),
    mDirty(aCopy.mDirty),
    mInRuleProcessorCache(false),
    mScopeElement(nullptr),
    mInner(aCopy.mInner),
    mRuleProcessors(nullptr)
{

  mInner->AddSheet(this);

  if (mDirty) { // CSSOM's been there, force full copy now
    NS_ASSERTION(mInner->mComplete, "Why have rules been accessed on an incomplete sheet?");
    // FIXME: handle failure?
    EnsureUniqueInner();
  }
}

CSSStyleSheet::~CSSStyleSheet()
{
  for (CSSStyleSheet* child = mInner->mFirstChild;
       child;
       child = child->mNext) {
    // XXXbz this is a little bogus; see the XXX comment where we
    // declare mFirstChild.
    if (child->mParent == this) {
      child->mParent = nullptr;
      child->mDocument = nullptr;
    }
  }
  DropRuleCollection();
  mInner->RemoveSheet(this);
  // XXX The document reference is not reference counted and should
  // not be released. The document will let us know when it is going
  // away.
  if (mRuleProcessors) {
    NS_ASSERTION(mRuleProcessors->Length() == 0, "destructing sheet with rule processor reference");
    delete mRuleProcessors; // weak refs, should be empty here anyway
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

  mInner->mOrderedRules.EnumerateForwards(SetStyleSheetReference, nullptr);
  mInner->mOrderedRules.Clear();

  // Have to be a bit careful with child sheets, because we want to
  // drop their mNext pointers and null out their mParent and
  // mDocument, but don't want to work with deleted objects.  And we
  // don't want to do any addrefing in the process, just to make sure
  // we don't confuse the cycle collector (though on the face of it,
  // addref/release pairs during unlink should probably be ok).
  RefPtr<CSSStyleSheet> child;
  child.swap(mInner->mFirstChild);
  while (child) {
    MOZ_ASSERT(child->mParent == this, "We have a unique inner!");
    child->mParent = nullptr;
    child->mDocument = nullptr;
    RefPtr<CSSStyleSheet> next;
    // Null out child->mNext, but don't let it die yet
    next.swap(child->mNext);
    // Switch to looking at the old value of child->mNext next iteration
    child.swap(next);
    // "next" is now our previous value of child; it'll get released
    // as we loop around.
  }
}

void
CSSStyleSheet::TraverseInner(nsCycleCollectionTraversalCallback &cb)
{
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  RefPtr<CSSStyleSheet>* childSheetSlot = &mInner->mFirstChild;
  while (*childSheetSlot) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "child sheet");
    cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMCSSStyleSheet*, childSheetSlot->get()));
    childSheetSlot = &(*childSheetSlot)->mNext;
  }

  const nsCOMArray<css::Rule>& rules = mInner->mOrderedRules;
  for (int32_t i = 0, count = rules.Count(); i < count; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mOrderedRules[i]");
    cb.NoteXPCOMChild(rules[i]->GetExistingDOMRule());
  }
}

// QueryInterface implementation for CSSStyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, StyleSheet)
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
  tmp->UnlinkInner();
  tmp->mScopeElement = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(StyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CSSStyleSheet, StyleSheet)
  // We do not traverse mNext; our parent will handle that.  See
  // comments in Unlink for why.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScopeElement)
  tmp->TraverseInner(cb);
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

void
CSSStyleSheet::AddStyleSet(nsStyleSet* aStyleSet)
{
  NS_ASSERTION(!mStyleSets.Contains(aStyleSet),
               "style set already registered");
  mStyleSets.AppendElement(aStyleSet);
}

void
CSSStyleSheet::DropStyleSet(nsStyleSet* aStyleSet)
{
  DebugOnly<bool> found = mStyleSets.RemoveElement(aStyleSet);
  NS_ASSERTION(found, "didn't find style set");
}

bool
CSSStyleSheet::UseForPresentation(nsPresContext* aPresContext,
                                  nsMediaQueryResultCacheKey& aKey) const
{
  if (mMedia) {
    return mMedia->Matches(aPresContext, &aKey);
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

CSSStyleSheet*
CSSStyleSheet::GetParentSheet() const
{
  return mParent;
}

void
CSSStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{ // not ref counted
  mDocument = aDocument;
  // Now set the same document on all our child sheets....
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild.
  for (CSSStyleSheet* child = mInner->mFirstChild;
       child; child = child->mNext) {
    if (child->mParent == this) {
      child->SetOwningDocument(aDocument);
    }
  }
}

uint64_t
CSSStyleSheet::FindOwningWindowInnerID() const
{
  uint64_t windowID = 0;
  if (mDocument) {
    windowID = mDocument->InnerWindowID();
  }

  if (windowID == 0 && mOwningNode) {
    windowID = mOwningNode->OwnerDoc()->InnerWindowID();
  }

  if (windowID == 0 && mOwnerRule) {
    RefPtr<StyleSheet> sheet =
      static_cast<css::Rule*>(mOwnerRule)->GetStyleSheet();
    if (sheet) {
      windowID = sheet->AsGecko()->FindOwningWindowInnerID();
    }
  }

  if (windowID == 0 && mParent) {
    windowID = mParent->FindOwningWindowInnerID();
  }

  return windowID;
}

void
CSSStyleSheet::AppendStyleSheet(CSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nullptr != aSheet, "null arg");

  WillDirty();
  RefPtr<CSSStyleSheet>* tail = &mInner->mFirstChild;
  while (*tail) {
    tail = &(*tail)->mNext;
  }
  *tail = aSheet;

  // This is not reference counted. Our parent tells us when
  // it's going away.
  aSheet->mParent = this;
  aSheet->mDocument = mDocument;
  DidDirty();
}

void
CSSStyleSheet::AppendStyleRule(css::Rule* aRule)
{
  NS_PRECONDITION(nullptr != aRule, "null arg");

  WillDirty();
  mInner->mOrderedRules.AppendObject(aRule);
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
  return mInner->mOrderedRules.Count();
}

css::Rule*
CSSStyleSheet::GetStyleRuleAt(int32_t aIndex) const
{
  // Important: If this function is ever made scriptable, we must add
  // a security check here. See GetCssRules below for an example.
  return mInner->mOrderedRules.SafeObjectAt(aIndex);
}

void
CSSStyleSheet::EnsureUniqueInner()
{
  mDirty = true;

  MOZ_ASSERT(mInner->mSheets.Length() != 0,
             "unexpected number of outers");
  if (mInner->mSheets.Length() == 1) {
    // already unique
    return;
  }
  CSSStyleSheetInner* clone = mInner->CloneFor(this);
  MOZ_ASSERT(clone);
  mInner->RemoveSheet(this);
  mInner = clone;

  // otherwise the rule processor has pointers to the old rules
  ClearRuleCascades();

  // let our containing style sets know that if we call
  // nsPresContext::EnsureSafeToHandOutCSSRules we will need to restyle the
  // document
  for (nsStyleSet* styleSet : mStyleSets) {
    styleSet->SetNeedsRestyleAfterEnsureUniqueInner();
  }
}

void
CSSStyleSheet::AppendAllChildSheets(nsTArray<CSSStyleSheet*>& aArray)
{
  for (CSSStyleSheet* child = mInner->mFirstChild; child;
       child = child->mNext) {
    aArray.AppendElement(child);
  }
}

already_AddRefed<CSSStyleSheet>
CSSStyleSheet::Clone(CSSStyleSheet* aCloneParent,
                     css::ImportRule* aCloneOwnerRule,
                     nsIDocument* aCloneDocument,
                     nsINode* aCloneOwningNode) const
{
  RefPtr<CSSStyleSheet> clone = new CSSStyleSheet(*this,
                                                    aCloneParent,
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

  int32_t index;

  // Indent
  nsAutoCString str;
  for (index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("CSS Style Sheet: ");
  nsAutoCString urlSpec;
  nsresult rv = mInner->mSheetURI->GetSpec(urlSpec);
  if (NS_SUCCEEDED(rv) && !urlSpec.IsEmpty()) {
    str.Append(urlSpec);
  }

  if (mMedia) {
    str.AppendLiteral(" media: ");
    nsAutoString  buffer;
    mMedia->GetText(buffer);
    AppendUTF16toUTF8(buffer, str);
  }
  str.Append('\n');
  fprintf_stderr(out, "%s", str.get());

  for (const CSSStyleSheet* child = mInner->mFirstChild;
       child;
       child = child->mNext) {
    child->List(out, aIndent + 1);
  }

  fprintf_stderr(out, "%s", "Rules in source order:\n");
  ListRules(mInner->mOrderedRules, out, aIndent);
}
#endif

void 
CSSStyleSheet::ClearRuleCascades()
{
  // We might be in ClearRuleCascades because we had a modification
  // to the sheet that resulted in an nsCSSSelector being destroyed.
  // Tell the RestyleManager for each document we're used in
  // so that they can drop any nsCSSSelector pointers (used for
  // eRestyle_SomeDescendants) in their mPendingRestyles.
  for (nsStyleSet* styleSet : mStyleSets) {
    styleSet->ClearSelectors();
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
    CSSStyleSheet* parent = (CSSStyleSheet*)mParent;
    parent->ClearRuleCascades();
  }
}

void
CSSStyleSheet::WillDirty()
{
  if (mInner->mComplete) {
    EnsureUniqueInner();
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
  if (!mInner->mNameSpaceMap) {
    nsresult rv = mInner->CreateNamespaceMap();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  AddNamespaceRuleToMap(aRule, mInner->mNameSpaceMap);
  return NS_OK;
}

nsIDOMCSSRule*
CSSStyleSheet::GetDOMOwnerRule() const
{
  return mOwnerRule ? mOwnerRule->GetDOMRule() : nullptr;
}

CSSRuleList*
CSSStyleSheet::GetCssRulesInternal(ErrorResult& aRv)
{
  if (!mRuleCollection) {
    mRuleCollection = new CSSRuleListImpl(this);
  }
  return mRuleCollection;
}

static bool
RuleHasPendingChildSheet(css::Rule *cssRule)
{
  nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(cssRule));
  NS_ASSERTION(importRule, "Rule which has type IMPORT_RULE and does not implement nsIDOMCSSImportRule!");
  nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
  importRule->GetStyleSheet(getter_AddRefs(childSheet));
  RefPtr<CSSStyleSheet> cssSheet = do_QueryObject(childSheet);
  return cssSheet != nullptr && !cssSheet->IsComplete();
}

uint32_t
CSSStyleSheet::InsertRuleInternal(const nsAString& aRule,
                                  uint32_t aIndex,
                                  ErrorResult& aRv)
{
  MOZ_ASSERT(mInner->mComplete);

  WillDirty();
  
  if (aIndex > uint32_t(mInner->mOrderedRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }
  
  NS_ASSERTION(uint32_t(mInner->mOrderedRules.Count()) <= INT32_MAX,
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
  css::Rule* nextRule = mInner->mOrderedRules.SafeObjectAt(aIndex);
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

    css::Rule* prevRule = mInner->mOrderedRules.SafeObjectAt(aIndex - 1);
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

  if (!mInner->mOrderedRules.InsertObjectAt(rule, aIndex)) {
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
  if ((type != css::Rule::IMPORT_RULE || !RuleHasPendingChildSheet(rule)) &&
      mDocument) {
    mDocument->StyleRuleAdded(this, rule);
  }

  return aIndex;
}

void
CSSStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
  // XXX TBI: handle @rule types
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
    
  WillDirty();

  if (aIndex >= uint32_t(mInner->mOrderedRules.Count())) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  NS_ASSERTION(uint32_t(mInner->mOrderedRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  // Hold a strong ref to the rule so it doesn't die when we RemoveObjectAt
  RefPtr<css::Rule> rule = mInner->mOrderedRules.ObjectAt(aIndex);
  if (rule) {
    mInner->mOrderedRules.RemoveObjectAt(aIndex);
    if (mDocument && mDocument->StyleSheetChangeEventsEnabled()) {
      // Force creation of the DOM rule, so that it can be put on the
      // StyleRuleRemoved event object.
      rule->GetDOMRule();
    }
    rule->SetStyleSheet(nullptr);
    DidDirty();

    if (mDocument) {
      mDocument->StyleRuleRemoved(this, rule);
    }
  }
}

nsresult
CSSStyleSheet::DeleteRuleFromGroup(css::GroupRule* aGroup, uint32_t aIndex)
{
  NS_ENSURE_ARG_POINTER(aGroup);
  NS_ASSERTION(mInner->mComplete, "No deleting from an incomplete sheet!");
  RefPtr<css::Rule> rule = aGroup->GetStyleRuleAt(aIndex);
  NS_ENSURE_TRUE(rule, NS_ERROR_ILLEGAL_VALUE);

  // check that the rule actually belongs to this sheet!
  if (this != rule->GetStyleSheet()) {
    return NS_ERROR_INVALID_ARG;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  
  WillDirty();

  nsresult result = aGroup->DeleteStyleRuleAt(aIndex);
  NS_ENSURE_SUCCESS(result, result);
  
  rule->SetStyleSheet(nullptr);
  
  DidDirty();

  if (mDocument) {
    mDocument->StyleRuleRemoved(this, rule);
  }

  return NS_OK;
}

nsresult
CSSStyleSheet::InsertRuleIntoGroup(const nsAString & aRule,
                                   css::GroupRule* aGroup,
                                   uint32_t aIndex,
                                   uint32_t* _retval)
{
  NS_ASSERTION(mInner->mComplete, "No inserting into an incomplete sheet!");
  // check that the group actually belongs to this sheet!
  if (this != aGroup->GetStyleSheet()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }

  nsCSSParser css(loader, this);

  // parse and grab the rule
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  WillDirty();

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

  result = aGroup->InsertStyleRuleAt(aIndex, rule);
  NS_ENSURE_SUCCESS(result, result);
  DidDirty();

  if (mDocument) {
    mDocument->StyleRuleAdded(this, rule);
  }

  *_retval = aIndex;
  return NS_OK;
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

  if (mDocument && NS_SUCCEEDED(aStatus)) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

    // XXXldb @import rules shouldn't even implement nsIStyleRule (but
    // they do)!
    mDocument->StyleRuleAdded(this, sheet->GetOwnerRule());
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
    loader = new css::Loader(StyleBackendType::Gecko);
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  WillDirty();

  // detach existing rules (including child sheets via import rules)
  css::LoaderReusableStyleSheets reusableSheets;
  int ruleCount;
  while ((ruleCount = mInner->mOrderedRules.Count()) != 0) {
    RefPtr<css::Rule> rule = mInner->mOrderedRules.ObjectAt(ruleCount - 1);
    mInner->mOrderedRules.RemoveObjectAt(ruleCount - 1);
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
    if (mDocument) {
      mDocument->StyleRuleRemoved(this, rule);
    }
  }

  // nuke child sheets list and current namespace map
  for (CSSStyleSheet* child = mInner->mFirstChild; child; ) {
    NS_ASSERTION(child->mParent == this, "Child sheet is not parented to this!");
    CSSStyleSheet* next = child->mNext;
    child->mParent = nullptr;
    child->mDocument = nullptr;
    child->mNext = nullptr;
    child = next;
  }
  mInner->mFirstChild = nullptr;
  mInner->mNameSpaceMap = nullptr;

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
  if (mDocument) {
    for (int32_t index = 0; index < mInner->mOrderedRules.Count(); ++index) {
      RefPtr<css::Rule> rule = mInner->mOrderedRules.ObjectAt(index);
      if (rule->GetType() == css::Rule::IMPORT_RULE &&
          RuleHasPendingChildSheet(rule)) {
        continue; // notify when loaded (see StyleSheetLoaded)
      }
      mDocument->StyleRuleAdded(this, rule);
    }
  }
  return NS_OK;
}

} // namespace mozilla
