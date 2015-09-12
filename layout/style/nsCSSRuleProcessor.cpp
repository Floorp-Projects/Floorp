/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for CSS style sheets, responsible for selector
 * matching and cascading
 */

#define PL_ARENA_CONST_ALIGN_MASK 7
// We want page-sized arenas so there's no fragmentation involved.
// Including plarena.h must come first to avoid it being included by some
// header file thereby making PL_ARENA_CONST_ALIGN_MASK ineffective.
#define NS_CASCADEENUMDATA_ARENA_BLOCK_SIZE (4096)
#include "plarena.h"

#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include <algorithm>
#include "nsIAtom.h"
#include "pldhash.h"
#include "nsICSSPseudoComparator.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/GroupRule.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsUnicharUtils.h"
#include "nsError.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStyleUtil.h"
#include "nsQuickSort.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsAttrName.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsIMediaList.h"
#include "nsCSSRules.h"
#include "nsStyleSet.h"
#include "mozilla/dom/Element.h"
#include "nsNthIndexCache.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Likely.h"
#include "mozilla/TypedEnumBits.h"
#include "RuleProcessorCache.h"
#include "nsIDOMMutationEvent.h"

using namespace mozilla;
using namespace mozilla::dom;

#define VISITED_PSEUDO_PREF "layout.css.visited_links_enabled"

static bool gSupportVisitedPseudo = true;

static nsTArray< nsCOMPtr<nsIAtom> >* sSystemMetrics = 0;

#ifdef XP_WIN
uint8_t nsCSSRuleProcessor::sWinThemeId = LookAndFeel::eWindowsTheme_Generic;
#endif

/**
 * A struct representing a given CSS rule and a particular selector
 * from that rule's selector list.
 */
struct RuleSelectorPair {
  RuleSelectorPair(css::StyleRule* aRule, nsCSSSelector* aSelector)
    : mRule(aRule), mSelector(aSelector) {}
  // If this class ever grows a destructor, deal with
  // PerWeightDataListItem appropriately.

  css::StyleRule*   mRule;
  nsCSSSelector*    mSelector; // which of |mRule|'s selectors
};

#define NS_IS_ANCESTOR_OPERATOR(ch) \
  ((ch) == char16_t(' ') || (ch) == char16_t('>'))

/**
 * A struct representing a particular rule in an ordered list of rules
 * (the ordering depending on the weight of mSelector and the order of
 * our rules to start with).
 */
struct RuleValue : RuleSelectorPair {
  enum {
    eMaxAncestorHashes = 4
  };

  RuleValue(const RuleSelectorPair& aRuleSelectorPair, int32_t aIndex,
            bool aQuirksMode) :
    RuleSelectorPair(aRuleSelectorPair),
    mIndex(aIndex)
  {
    CollectAncestorHashes(aQuirksMode);
  }

  int32_t mIndex; // High index means high weight/order.
  uint32_t mAncestorSelectorHashes[eMaxAncestorHashes];

private:
  void CollectAncestorHashes(bool aQuirksMode) {
    // Collect up our mAncestorSelectorHashes.  It's not clear whether it's
    // better to stop once we've found eMaxAncestorHashes of them or to keep
    // going and preferentially collect information from selectors higher up the
    // chain...  Let's do the former for now.
    size_t hashIndex = 0;
    for (nsCSSSelector* sel = mSelector->mNext; sel; sel = sel->mNext) {
      if (!NS_IS_ANCESTOR_OPERATOR(sel->mOperator)) {
        // |sel| is going to select something that's not actually one of our
        // ancestors, so don't add it to mAncestorSelectorHashes.  But keep
        // going, because it'll select a sibling of one of our ancestors, so its
        // ancestors would be our ancestors too.
        continue;
      }

      // Now sel is supposed to select one of our ancestors.  Grab
      // whatever info we can from it into mAncestorSelectorHashes.
      // But in qurks mode, don't grab IDs and classes because those
      // need to be matched case-insensitively.
      if (!aQuirksMode) {
        nsAtomList* ids = sel->mIDList;
        while (ids) {
          mAncestorSelectorHashes[hashIndex++] = ids->mAtom->hash();
          if (hashIndex == eMaxAncestorHashes) {
            return;
          }
          ids = ids->mNext;
        }

        nsAtomList* classes = sel->mClassList;
        while (classes) {
          mAncestorSelectorHashes[hashIndex++] = classes->mAtom->hash();
          if (hashIndex == eMaxAncestorHashes) {
            return;
          }
          classes = classes->mNext;
        }
      }

      // Only put in the tag name if it's all-lowercase.  Otherwise we run into
      // trouble because we may test the wrong one of mLowercaseTag and
      // mCasedTag against the filter.
      if (sel->mLowercaseTag && sel->mCasedTag == sel->mLowercaseTag) {
        mAncestorSelectorHashes[hashIndex++] = sel->mLowercaseTag->hash();
        if (hashIndex == eMaxAncestorHashes) {
          return;
        }
      }
    }

    while (hashIndex != eMaxAncestorHashes) {
      mAncestorSelectorHashes[hashIndex++] = 0;
    }
  }
};

// ------------------------------
// Rule hash table
//

// Uses any of the sets of ops below.
struct RuleHashTableEntry : public PLDHashEntryHdr {
  // If you add members that have heap allocated memory be sure to change the
  // logic in SizeOfRuleHashTable().
  // Auto length 1, because we always have at least one entry in mRules.
  nsAutoTArray<RuleValue, 1> mRules;
};

struct RuleHashTagTableEntry : public RuleHashTableEntry {
  // If you add members that have heap allocated memory be sure to change the
  // logic in RuleHash::SizeOf{In,Ex}cludingThis.
  nsCOMPtr<nsIAtom> mTag;
};

static PLDHashNumber
RuleHash_CIHashKey(PLDHashTable *table, const void *key)
{
  nsIAtom *atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));

  nsAutoString str;
  atom->ToString(str);
  nsContentUtils::ASCIIToLower(str);
  return HashString(str);
}

typedef nsIAtom*
(* RuleHashGetKey) (PLDHashTable *table, const PLDHashEntryHdr *entry);

struct RuleHashTableOps {
  const PLDHashTableOps ops;
  // Extra callback to avoid duplicating the matchEntry callback for
  // each table.  (There used to be a getKey callback in
  // PLDHashTableOps.)
  RuleHashGetKey getKey;
};

inline const RuleHashTableOps*
ToLocalOps(const PLDHashTableOps *aOps)
{
  return (const RuleHashTableOps*)
           (((const char*) aOps) - offsetof(RuleHashTableOps, ops));
}

static bool
RuleHash_CIMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->Ops())->getKey(table, hdr);

  // Check for case-sensitive match first.
  if (match_atom == entry_atom)
    return true;

  // Use EqualsIgnoreASCIICase instead of full on unicode case conversion
  // in order to save on performance. This is only used in quirks mode
  // anyway.

  return
    nsContentUtils::EqualsIgnoreASCIICase(nsDependentAtomString(entry_atom),
                                          nsDependentAtomString(match_atom));
}

static bool
RuleHash_CSMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->Ops())->getKey(table, hdr);

  return match_atom == entry_atom;
}

static void
RuleHash_InitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  new (entry) RuleHashTableEntry();
}

static void
RuleHash_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  entry->~RuleHashTableEntry();
}

static void
RuleHash_MoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                   PLDHashEntryHdr *to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  RuleHashTableEntry *oldEntry =
    const_cast<RuleHashTableEntry*>(
      static_cast<const RuleHashTableEntry*>(from));
  RuleHashTableEntry *newEntry = new (to) RuleHashTableEntry();
  newEntry->mRules.SwapElements(oldEntry->mRules);
  oldEntry->~RuleHashTableEntry();
}

static bool
RuleHash_TagTable_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  nsIAtom *entry_atom = static_cast<const RuleHashTagTableEntry*>(hdr)->mTag;

  return match_atom == entry_atom;
}

static void
RuleHash_TagTable_InitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  new (entry) RuleHashTagTableEntry();
  entry->mTag = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
}

static void
RuleHash_TagTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  entry->~RuleHashTagTableEntry();
}

static void
RuleHash_TagTable_MoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                            PLDHashEntryHdr *to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  RuleHashTagTableEntry *oldEntry =
    const_cast<RuleHashTagTableEntry*>(
      static_cast<const RuleHashTagTableEntry*>(from));
  RuleHashTagTableEntry *newEntry = new (to) RuleHashTagTableEntry();
  newEntry->mTag.swap(oldEntry->mTag);
  newEntry->mRules.SwapElements(oldEntry->mRules);
  oldEntry->~RuleHashTagTableEntry();
}

static nsIAtom*
RuleHash_ClassTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    static_cast<const RuleHashTableEntry*>(hdr);
  nsCSSSelector* selector = entry->mRules[0].mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  return selector->mClassList->mAtom;
}

static nsIAtom*
RuleHash_IdTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    static_cast<const RuleHashTableEntry*>(hdr);
  nsCSSSelector* selector = entry->mRules[0].mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  return selector->mIDList->mAtom;
}

static PLDHashNumber
RuleHash_NameSpaceTable_HashKey(PLDHashTable *table, const void *key)
{
  return NS_PTR_TO_INT32(key);
}

static bool
RuleHash_NameSpaceTable_MatchEntry(PLDHashTable *table,
                                   const PLDHashEntryHdr *hdr,
                                   const void *key)
{
  const RuleHashTableEntry *entry =
    static_cast<const RuleHashTableEntry*>(hdr);

  nsCSSSelector* selector = entry->mRules[0].mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  return NS_PTR_TO_INT32(key) == selector->mNameSpace;
}

static const PLDHashTableOps RuleHash_TagTable_Ops = {
  PL_DHashVoidPtrKeyStub,
  RuleHash_TagTable_MatchEntry,
  RuleHash_TagTable_MoveEntry,
  RuleHash_TagTable_ClearEntry,
  RuleHash_TagTable_InitEntry
};

// Case-sensitive ops.
static const RuleHashTableOps RuleHash_ClassTable_CSOps = {
  {
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
  },
  RuleHash_ClassTable_GetKey
};

// Case-insensitive ops.
static const RuleHashTableOps RuleHash_ClassTable_CIOps = {
  {
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
  },
  RuleHash_ClassTable_GetKey
};

// Case-sensitive ops.
static const RuleHashTableOps RuleHash_IdTable_CSOps = {
  {
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
  },
  RuleHash_IdTable_GetKey
};

// Case-insensitive ops.
static const RuleHashTableOps RuleHash_IdTable_CIOps = {
  {
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
  },
  RuleHash_IdTable_GetKey
};

static const PLDHashTableOps RuleHash_NameSpaceTable_Ops = {
  RuleHash_NameSpaceTable_HashKey,
  RuleHash_NameSpaceTable_MatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  RuleHash_InitEntry
};

#undef RULE_HASH_STATS
#undef PRINT_UNIVERSAL_RULES

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT(var_) PR_BEGIN_MACRO ++(var_); PR_END_MACRO
#else
#define RULE_HASH_STAT_INCREMENT(var_) PR_BEGIN_MACRO PR_END_MACRO
#endif

struct NodeMatchContext;

class RuleHash {
public:
  explicit RuleHash(bool aQuirksMode);
  ~RuleHash();
  void AppendRule(const RuleSelectorPair &aRuleInfo);
  void EnumerateAllRules(Element* aElement, ElementDependentRuleProcessorData* aData,
                         NodeMatchContext& aNodeMatchContext);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

protected:
  typedef nsTArray<RuleValue> RuleValueList;
  void AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                         const RuleSelectorPair& aRuleInfo);
  void AppendUniversalRule(const RuleSelectorPair& aRuleInfo);

  int32_t     mRuleCount;

  PLDHashTable mIdTable;
  PLDHashTable mClassTable;
  PLDHashTable mTagTable;
  PLDHashTable mNameSpaceTable;
  RuleValueList mUniversalRules;

  struct EnumData {
    const RuleValue* mCurValue;
    const RuleValue* mEnd;
  };
  EnumData* mEnumList;
  int32_t   mEnumListSize;

  bool mQuirksMode;

  inline EnumData ToEnumData(const RuleValueList& arr) {
    EnumData data = { arr.Elements(), arr.Elements() + arr.Length() };
    return data;
  }

#ifdef RULE_HASH_STATS
  uint32_t    mUniversalSelectors;
  uint32_t    mNameSpaceSelectors;
  uint32_t    mTagSelectors;
  uint32_t    mClassSelectors;
  uint32_t    mIdSelectors;

  uint32_t    mElementsMatched;

  uint32_t    mElementUniversalCalls;
  uint32_t    mElementNameSpaceCalls;
  uint32_t    mElementTagCalls;
  uint32_t    mElementClassCalls;
  uint32_t    mElementIdCalls;
#endif // RULE_HASH_STATS
};

RuleHash::RuleHash(bool aQuirksMode)
  : mRuleCount(0),
    mIdTable(aQuirksMode ? &RuleHash_IdTable_CIOps.ops
                         : &RuleHash_IdTable_CSOps.ops,
             sizeof(RuleHashTableEntry)),
    mClassTable(aQuirksMode ? &RuleHash_ClassTable_CIOps.ops
                            : &RuleHash_ClassTable_CSOps.ops,
                sizeof(RuleHashTableEntry)),
    mTagTable(&RuleHash_TagTable_Ops, sizeof(RuleHashTagTableEntry)),
    mNameSpaceTable(&RuleHash_NameSpaceTable_Ops, sizeof(RuleHashTableEntry)),
    mUniversalRules(0),
    mEnumList(nullptr), mEnumListSize(0),
    mQuirksMode(aQuirksMode)
#ifdef RULE_HASH_STATS
    ,
    mUniversalSelectors(0),
    mNameSpaceSelectors(0),
    mTagSelectors(0),
    mClassSelectors(0),
    mIdSelectors(0),
    mElementsMatched(0),
    mElementUniversalCalls(0),
    mElementNameSpaceCalls(0),
    mElementTagCalls(0),
    mElementClassCalls(0),
    mElementIdCalls(0)
#endif
{
  MOZ_COUNT_CTOR(RuleHash);
}

RuleHash::~RuleHash()
{
  MOZ_COUNT_DTOR(RuleHash);
#ifdef RULE_HASH_STATS
  printf(
"RuleHash(%p):\n"
"  Selectors: Universal (%u) NameSpace(%u) Tag(%u) Class(%u) Id(%u)\n"
"  Content Nodes: Elements(%u)\n"
"  Element Calls: Universal(%u) NameSpace(%u) Tag(%u) Class(%u) Id(%u)\n"
         static_cast<void*>(this),
         mUniversalSelectors, mNameSpaceSelectors, mTagSelectors,
           mClassSelectors, mIdSelectors,
         mElementsMatched,
         mElementUniversalCalls, mElementNameSpaceCalls, mElementTagCalls,
           mElementClassCalls, mElementIdCalls);
#ifdef PRINT_UNIVERSAL_RULES
  {
    if (mUniversalRules.Length() > 0) {
      printf("  Universal rules:\n");
      for (uint32_t i = 0; i < mUniversalRules.Length(); ++i) {
        RuleValue* value = &(mUniversalRules[i]);
        nsAutoString selectorText;
        uint32_t lineNumber = value->mRule->GetLineNumber();
        nsRefPtr<CSSStyleSheet> cssSheet = value->mRule->GetStyleSheet();
        value->mSelector->ToString(selectorText, cssSheet);

        printf("    line %d, %s\n",
               lineNumber, NS_ConvertUTF16toUTF8(selectorText).get());
      }
    }
  }
#endif // PRINT_UNIVERSAL_RULES
#endif // RULE_HASH_STATS
  // Rule Values are arena allocated no need to delete them. Their destructor
  // isn't doing any cleanup. So we dont even bother to enumerate through
  // the hash tables and call their destructors.
  if (nullptr != mEnumList) {
    delete [] mEnumList;
  }
}

void RuleHash::AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                                 const RuleSelectorPair& aRuleInfo)
{
  // Get a new or existing entry.
  RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
    (PL_DHashTableAdd(aTable, aKey, fallible));
  if (!entry)
    return;
  entry->mRules.AppendElement(RuleValue(aRuleInfo, mRuleCount++, mQuirksMode));
}

static void
AppendRuleToTagTable(PLDHashTable* aTable, nsIAtom* aKey,
                     const RuleValue& aRuleInfo)
{
  // Get a new or exisiting entry
  RuleHashTagTableEntry *entry = static_cast<RuleHashTagTableEntry*>
    (PL_DHashTableAdd(aTable, aKey, fallible));
  if (!entry)
    return;

  entry->mRules.AppendElement(aRuleInfo);
}

void RuleHash::AppendUniversalRule(const RuleSelectorPair& aRuleInfo)
{
  mUniversalRules.AppendElement(RuleValue(aRuleInfo, mRuleCount++, mQuirksMode));
}

void RuleHash::AppendRule(const RuleSelectorPair& aRuleInfo)
{
  nsCSSSelector *selector = aRuleInfo.mSelector;
  if (selector->IsPseudoElement()) {
    selector = selector->mNext;
  }
  if (nullptr != selector->mIDList) {
    AppendRuleToTable(&mIdTable, selector->mIDList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mIdSelectors);
  }
  else if (nullptr != selector->mClassList) {
    AppendRuleToTable(&mClassTable, selector->mClassList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mClassSelectors);
  }
  else if (selector->mLowercaseTag) {
    RuleValue ruleValue(aRuleInfo, mRuleCount++, mQuirksMode);
    AppendRuleToTagTable(&mTagTable, selector->mLowercaseTag, ruleValue);
    RULE_HASH_STAT_INCREMENT(mTagSelectors);
    if (selector->mCasedTag &&
        selector->mCasedTag != selector->mLowercaseTag) {
      AppendRuleToTagTable(&mTagTable, selector->mCasedTag, ruleValue);
      RULE_HASH_STAT_INCREMENT(mTagSelectors);
    }
  }
  else if (kNameSpaceID_Unknown != selector->mNameSpace) {
    AppendRuleToTable(&mNameSpaceTable,
                      NS_INT32_TO_PTR(selector->mNameSpace), aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mNameSpaceSelectors);
  }
  else {  // universal tag selector
    AppendUniversalRule(aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mUniversalSelectors);
  }
}

// this should cover practically all cases so we don't need to reallocate
#define MIN_ENUM_LIST_SIZE 8

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_) \
  (var_) += (list_).Length()
#else
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_) \
  PR_BEGIN_MACRO PR_END_MACRO
#endif

static inline
void ContentEnumFunc(const RuleValue &value, nsCSSSelector* selector,
                     ElementDependentRuleProcessorData* data, NodeMatchContext& nodeContext,
                     AncestorFilter *ancestorFilter);

void RuleHash::EnumerateAllRules(Element* aElement, ElementDependentRuleProcessorData* aData,
                                 NodeMatchContext& aNodeContext)
{
  int32_t nameSpace = aElement->GetNameSpaceID();
  nsIAtom* tag = aElement->NodeInfo()->NameAtom();
  nsIAtom* id = aElement->GetID();
  const nsAttrValue* classList = aElement->GetClasses();

  MOZ_ASSERT(tag, "How could we not have a tag?");

  int32_t classCount = classList ? classList->GetAtomCount() : 0;

  // assume 1 universal, tag, id, and namespace, rather than wasting
  // time counting
  int32_t testCount = classCount + 4;

  if (mEnumListSize < testCount) {
    delete [] mEnumList;
    mEnumListSize = std::max(testCount, MIN_ENUM_LIST_SIZE);
    mEnumList = new EnumData[mEnumListSize];
  }

  int32_t valueCount = 0;
  RULE_HASH_STAT_INCREMENT(mElementsMatched);

  if (mUniversalRules.Length() != 0) { // universal rules
    mEnumList[valueCount++] = ToEnumData(mUniversalRules);
    RULE_HASH_STAT_INCREMENT_LIST_COUNT(mUniversalRules, mElementUniversalCalls);
  }
  // universal rules within the namespace
  if (kNameSpaceID_Unknown != nameSpace && mNameSpaceTable.EntryCount() > 0) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableSearch(&mNameSpaceTable, NS_INT32_TO_PTR(nameSpace)));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementNameSpaceCalls);
    }
  }
  if (mTagTable.EntryCount() > 0) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableSearch(&mTagTable, tag));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementTagCalls);
    }
  }
  if (id && mIdTable.EntryCount() > 0) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableSearch(&mIdTable, id));
    if (entry) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementIdCalls);
    }
  }
  if (mClassTable.EntryCount() > 0) {
    for (int32_t index = 0; index < classCount; ++index) {
      RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                             (PL_DHashTableSearch(&mClassTable, classList->AtomAt(index)));
      if (entry) {
        mEnumList[valueCount++] = ToEnumData(entry->mRules);
        RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementClassCalls);
      }
    }
  }
  NS_ASSERTION(valueCount <= testCount, "values exceeded list size");

  if (valueCount > 0) {
    AncestorFilter *filter =
      aData->mTreeMatchContext.mAncestorFilter.HasFilter() ?
        &aData->mTreeMatchContext.mAncestorFilter : nullptr;
#ifdef DEBUG
    if (filter) {
      filter->AssertHasAllAncestors(aElement);
    }
#endif
    // Merge the lists while there are still multiple lists to merge.
    while (valueCount > 1) {
      int32_t valueIndex = 0;
      int32_t lowestRuleIndex = mEnumList[valueIndex].mCurValue->mIndex;
      for (int32_t index = 1; index < valueCount; ++index) {
        int32_t ruleIndex = mEnumList[index].mCurValue->mIndex;
        if (ruleIndex < lowestRuleIndex) {
          valueIndex = index;
          lowestRuleIndex = ruleIndex;
        }
      }
      const RuleValue *cur = mEnumList[valueIndex].mCurValue;
      ContentEnumFunc(*cur, cur->mSelector, aData, aNodeContext, filter);
      cur++;
      if (cur == mEnumList[valueIndex].mEnd) {
        mEnumList[valueIndex] = mEnumList[--valueCount];
      } else {
        mEnumList[valueIndex].mCurValue = cur;
      }
    }

    // Fast loop over single value.
    for (const RuleValue *value = mEnumList[0].mCurValue,
                         *end = mEnumList[0].mEnd;
         value != end; ++value) {
      ContentEnumFunc(*value, value->mSelector, aData, aNodeContext, filter);
    }
  }
}

static size_t
SizeOfRuleHashTable(const PLDHashTable& aTable, MallocSizeOf aMallocSizeOf)
{
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<RuleHashTableEntry*>(iter.Get());
    n += entry->mRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

size_t
RuleHash::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  n += SizeOfRuleHashTable(mIdTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mClassTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mTagTable, aMallocSizeOf);

  n += SizeOfRuleHashTable(mNameSpaceTable, aMallocSizeOf);

  n += mUniversalRules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return n;
}

size_t
RuleHash::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

//--------------------------------

/**
 * A struct that stores an nsCSSSelector pointer along side a pointer to
 * the rightmost nsCSSSelector in the selector.  For example, for
 *
 *   .main p > span
 *
 * if mSelector points to the |p| nsCSSSelector, mRightmostSelector would
 * point to the |span| nsCSSSelector.
 *
 * Both mSelector and mRightmostSelector are always top-level selectors,
 * i.e. they aren't selectors within a :not() or :-moz-any().
 */
struct SelectorPair
{
  SelectorPair(nsCSSSelector* aSelector, nsCSSSelector* aRightmostSelector)
    : mSelector(aSelector), mRightmostSelector(aRightmostSelector)
  {
    MOZ_ASSERT(aSelector);
    MOZ_ASSERT(mRightmostSelector);
  }
  SelectorPair(const SelectorPair& aOther)
    : mSelector(aOther.mSelector)
    , mRightmostSelector(aOther.mRightmostSelector) {}
  nsCSSSelector* const mSelector;
  nsCSSSelector* const mRightmostSelector;
};

// A hash table mapping atoms to lists of selectors
struct AtomSelectorEntry : public PLDHashEntryHdr {
  nsIAtom *mAtom;
  // Auto length 2, because a decent fraction of these arrays ends up
  // with 2 elements, and each entry is cheap.
  nsAutoTArray<SelectorPair, 2> mSelectors;
};

static void
AtomSelector_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  (static_cast<AtomSelectorEntry*>(hdr))->~AtomSelectorEntry();
}

static void
AtomSelector_InitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  AtomSelectorEntry *entry = static_cast<AtomSelectorEntry*>(hdr);
  new (entry) AtomSelectorEntry();
  entry->mAtom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
}

static void
AtomSelector_MoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                       PLDHashEntryHdr *to)
{
  NS_PRECONDITION(from != to, "This is not going to work!");
  AtomSelectorEntry *oldEntry =
    const_cast<AtomSelectorEntry*>(static_cast<const AtomSelectorEntry*>(from));
  AtomSelectorEntry *newEntry = new (to) AtomSelectorEntry();
  newEntry->mAtom = oldEntry->mAtom;
  newEntry->mSelectors.SwapElements(oldEntry->mSelectors);
  oldEntry->~AtomSelectorEntry();
}

static nsIAtom*
AtomSelector_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const AtomSelectorEntry *entry = static_cast<const AtomSelectorEntry*>(hdr);
  return entry->mAtom;
}

// Case-sensitive ops.
static const PLDHashTableOps AtomSelector_CSOps = {
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  AtomSelector_InitEntry
};

// Case-insensitive ops.
static const RuleHashTableOps AtomSelector_CIOps = {
  {
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  AtomSelector_InitEntry
  },
  AtomSelector_GetKey
};

//--------------------------------

struct RuleCascadeData {
  RuleCascadeData(nsIAtom *aMedium, bool aQuirksMode)
    : mRuleHash(aQuirksMode),
      mStateSelectors(),
      mSelectorDocumentStates(0),
      mClassSelectors(aQuirksMode ? &AtomSelector_CIOps.ops
                                  : &AtomSelector_CSOps,
                      sizeof(AtomSelectorEntry)),
      mIdSelectors(aQuirksMode ? &AtomSelector_CIOps.ops
                               : &AtomSelector_CSOps,
                   sizeof(AtomSelectorEntry)),
      // mAttributeSelectors is matching on the attribute _name_, not the
      // value, and we case-fold names at parse-time, so this is a
      // case-sensitive match.
      mAttributeSelectors(&AtomSelector_CSOps, sizeof(AtomSelectorEntry)),
      mAnonBoxRules(&RuleHash_TagTable_Ops, sizeof(RuleHashTagTableEntry)),
#ifdef MOZ_XUL
      mXULTreeRules(&RuleHash_TagTable_Ops, sizeof(RuleHashTagTableEntry)),
#endif
      mKeyframesRuleTable(),
      mCounterStyleRuleTable(),
      mCacheKey(aMedium),
      mNext(nullptr),
      mQuirksMode(aQuirksMode)
  {
    memset(mPseudoElementRuleHashes, 0, sizeof(mPseudoElementRuleHashes));
  }

  ~RuleCascadeData()
  {
    for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
      delete mPseudoElementRuleHashes[i];
    }
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  RuleHash                 mRuleHash;
  RuleHash*
    mPseudoElementRuleHashes[nsCSSPseudoElements::ePseudo_PseudoElementCount];
  nsTArray<nsCSSRuleProcessor::StateSelector>  mStateSelectors;
  EventStates              mSelectorDocumentStates;
  PLDHashTable             mClassSelectors;
  PLDHashTable             mIdSelectors;
  nsTArray<nsCSSSelector*> mPossiblyNegatedClassSelectors;
  nsTArray<nsCSSSelector*> mPossiblyNegatedIDSelectors;
  PLDHashTable             mAttributeSelectors;
  PLDHashTable             mAnonBoxRules;
#ifdef MOZ_XUL
  PLDHashTable             mXULTreeRules;
#endif

  nsTArray<nsFontFaceRuleContainer> mFontFaceRules;
  nsTArray<nsCSSKeyframesRule*> mKeyframesRules;
  nsTArray<nsCSSFontFeatureValuesRule*> mFontFeatureValuesRules;
  nsTArray<nsCSSPageRule*> mPageRules;
  nsTArray<nsCSSCounterStyleRule*> mCounterStyleRules;

  nsDataHashtable<nsStringHashKey, nsCSSKeyframesRule*> mKeyframesRuleTable;
  nsDataHashtable<nsStringHashKey, nsCSSCounterStyleRule*> mCounterStyleRuleTable;

  // Looks up or creates the appropriate list in |mAttributeSelectors|.
  // Returns null only on allocation failure.
  nsTArray<SelectorPair>* AttributeListFor(nsIAtom* aAttribute);

  nsMediaQueryResultCacheKey mCacheKey;
  RuleCascadeData*  mNext; // for a different medium

  const bool mQuirksMode;
};

static size_t
SizeOfSelectorsHashTable(const PLDHashTable& aTable, MallocSizeOf aMallocSizeOf)
{
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<AtomSelectorEntry*>(iter.Get());
    n += entry->mSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

size_t
RuleCascadeData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mRuleHash.SizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
    if (mPseudoElementRuleHashes[i])
      n += mPseudoElementRuleHashes[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += mStateSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += SizeOfSelectorsHashTable(mIdSelectors, aMallocSizeOf);
  n += SizeOfSelectorsHashTable(mClassSelectors, aMallocSizeOf);

  n += mPossiblyNegatedClassSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mPossiblyNegatedIDSelectors.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += SizeOfSelectorsHashTable(mAttributeSelectors, aMallocSizeOf);
  n += SizeOfRuleHashTable(mAnonBoxRules, aMallocSizeOf);
#ifdef MOZ_XUL
  n += SizeOfRuleHashTable(mXULTreeRules, aMallocSizeOf);
#endif

  n += mFontFaceRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mKeyframesRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mFontFeatureValuesRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mPageRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mCounterStyleRules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mKeyframesRuleTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mKeyframesRuleTable.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't own the nsCSSKeyframesRule objects so we don't count them. We
    // do care about the size of the keys' nsAString members' buffers though.
    //
    // Note that we depend on nsStringHashKey::GetKey() returning a reference,
    // since otherwise aKey would be a copy of the string key and we would not
    // be measuring the right object here.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

nsTArray<SelectorPair>*
RuleCascadeData::AttributeListFor(nsIAtom* aAttribute)
{
  AtomSelectorEntry *entry =
    static_cast<AtomSelectorEntry*>
               (PL_DHashTableAdd(&mAttributeSelectors, aAttribute, fallible));
  if (!entry)
    return nullptr;
  return &entry->mSelectors;
}

// -------------------------------
// CSS Style rule processor implementation
//

nsCSSRuleProcessor::nsCSSRuleProcessor(const sheet_array_type& aSheets,
                                       uint8_t aSheetType,
                                       Element* aScopeElement,
                                       nsCSSRuleProcessor*
                                         aPreviousCSSRuleProcessor,
                                       bool aIsShared)
  : mSheets(aSheets)
  , mRuleCascades(nullptr)
  , mPreviousCacheKey(aPreviousCSSRuleProcessor
                       ? aPreviousCSSRuleProcessor->CloneMQCacheKey()
                       : UniquePtr<nsMediaQueryResultCacheKey>())
  , mLastPresContext(nullptr)
  , mScopeElement(aScopeElement)
  , mStyleSetRefCnt(0)
  , mSheetType(aSheetType)
  , mIsShared(aIsShared)
  , mMustGatherDocumentRules(aIsShared)
  , mInRuleProcessorCache(false)
#ifdef DEBUG
  , mDocumentRulesAndCacheKeyValid(false)
#endif
{
  NS_ASSERTION(!!mScopeElement == (aSheetType == nsStyleSet::eScopedDocSheet),
               "aScopeElement must be specified iff aSheetType is "
               "eScopedDocSheet");
  for (sheet_array_type::size_type i = mSheets.Length(); i-- != 0; ) {
    mSheets[i]->AddRuleProcessor(this);
  }
}

nsCSSRuleProcessor::~nsCSSRuleProcessor()
{
  if (mInRuleProcessorCache) {
    RuleProcessorCache::RemoveRuleProcessor(this);
  }
  MOZ_ASSERT(!mExpirationState.IsTracked());
  MOZ_ASSERT(mStyleSetRefCnt == 0);
  ClearSheets();
  ClearRuleCascades();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSRuleProcessor)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRuleProcessor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSRuleProcessor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCSSRuleProcessor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCSSRuleProcessor)
  tmp->ClearSheets();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScopeElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCSSRuleProcessor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScopeElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void
nsCSSRuleProcessor::ClearSheets()
{
  for (sheet_array_type::size_type i = mSheets.Length(); i-- != 0; ) {
    mSheets[i]->DropRuleProcessor(this);
  }
  mSheets.Clear();
}

/* static */ void
nsCSSRuleProcessor::Startup()
{
  Preferences::AddBoolVarCache(&gSupportVisitedPseudo, VISITED_PSEUDO_PREF,
                               true);
}

static bool
InitSystemMetrics()
{
  NS_ASSERTION(!sSystemMetrics, "already initialized");

  sSystemMetrics = new nsTArray< nsCOMPtr<nsIAtom> >;
  NS_ENSURE_TRUE(sSystemMetrics, false);

  /***************************************************************************
   * ANY METRICS ADDED HERE SHOULD ALSO BE ADDED AS MEDIA QUERIES IN         *
   * nsMediaFeatures.cpp                                                     *
   ***************************************************************************/

  int32_t metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollArrowStyle);
  if (metricResult & LookAndFeel::eScrollArrow_StartBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_StartForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_forward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_forward);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollSliderStyle);
  if (metricResult != LookAndFeel::eScrollThumbStyle_Normal) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_thumb_proportional);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ImagesInMenus);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::images_in_menus);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ImagesInButtons);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::images_in_buttons);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::overlay_scrollbars);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenuBarDrag);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::menubar_drag);
  }

  nsresult rv =
    LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsDefaultTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_default_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacGraphiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_graphite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacLionTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_lion_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacYosemiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_yosemite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_ColorPickerAvailable, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::color_picker_available);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::touch_enabled);
  }
 
  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_SwipeAnimationEnabled,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::swipe_animation_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_PhysicalHomeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::physical_home_button);
  }

#ifdef XP_WIN
  if (NS_SUCCEEDED(
        LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsThemeIdentifier,
                            &metricResult))) {
    nsCSSRuleProcessor::SetWindowsThemeIdentifier(static_cast<uint8_t>(metricResult));
    switch(metricResult) {
      case LookAndFeel::eWindowsTheme_Aero:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero);
        break;
      case LookAndFeel::eWindowsTheme_AeroLite:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero_lite);
        break;
      case LookAndFeel::eWindowsTheme_LunaBlue:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_blue);
        break;
      case LookAndFeel::eWindowsTheme_LunaOlive:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_olive);
        break;
      case LookAndFeel::eWindowsTheme_LunaSilver:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_silver);
        break;
      case LookAndFeel::eWindowsTheme_Royale:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_royale);
        break;
      case LookAndFeel::eWindowsTheme_Zune:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_zune);
        break;
      case LookAndFeel::eWindowsTheme_Generic:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_generic);
        break;
    }
  }
#endif

  return true;
}

/* static */ void
nsCSSRuleProcessor::FreeSystemMetrics()
{
  delete sSystemMetrics;
  sSystemMetrics = nullptr;
}

/* static */ void
nsCSSRuleProcessor::Shutdown()
{
  FreeSystemMetrics();
}

/* static */ bool
nsCSSRuleProcessor::HasSystemMetric(nsIAtom* aMetric)
{
  if (!sSystemMetrics && !InitSystemMetrics()) {
    return false;
  }
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

#ifdef XP_WIN
/* static */ uint8_t
nsCSSRuleProcessor::GetWindowsThemeIdentifier()
{
  if (!sSystemMetrics)
    InitSystemMetrics();
  return sWinThemeId;
}
#endif

/* static */
EventStates
nsCSSRuleProcessor::GetContentState(Element* aElement, const TreeMatchContext& aTreeMatchContext)
{
  EventStates state = aElement->StyleState();

  // If we are not supposed to mark visited links as such, be sure to
  // flip the bits appropriately.  We want to do this here, rather
  // than in GetContentStateForVisitedHandling, so that we don't
  // expose that :visited support is disabled to the Web page.
  if (state.HasState(NS_EVENT_STATE_VISITED) &&
      (!gSupportVisitedPseudo ||
       aElement->OwnerDoc()->IsBeingUsedAsImage() ||
       aTreeMatchContext.mUsingPrivateBrowsing)) {
    state &= ~NS_EVENT_STATE_VISITED;
    state |= NS_EVENT_STATE_UNVISITED;
  }
  return state;
}

/* static */
bool
nsCSSRuleProcessor::IsLink(Element* aElement)
{
  EventStates state = aElement->StyleState();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED);
}

/* static */
EventStates
nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                     Element* aElement,
                     const TreeMatchContext& aTreeMatchContext,
                     nsRuleWalker::VisitedHandlingType aVisitedHandling,
                     bool aIsRelevantLink)
{
  EventStates contentState = GetContentState(aElement, aTreeMatchContext);
  if (contentState.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)) {
    MOZ_ASSERT(IsLink(aElement), "IsLink() should match state");
    contentState &= ~(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED);
    if (aIsRelevantLink) {
      switch (aVisitedHandling) {
        case nsRuleWalker::eRelevantLinkUnvisited:
          contentState |= NS_EVENT_STATE_UNVISITED;
          break;
        case nsRuleWalker::eRelevantLinkVisited:
          contentState |= NS_EVENT_STATE_VISITED;
          break;
        case nsRuleWalker::eLinksVisitedOrUnvisited:
          contentState |= NS_EVENT_STATE_UNVISITED | NS_EVENT_STATE_VISITED;
          break;
      }
    } else {
      contentState |= NS_EVENT_STATE_UNVISITED;
    }
  }
  return contentState;
}

/**
 * A |NodeMatchContext| has data about matching a selector (without
 * combinators) against a single node.  It contains only input to the
 * matching.
 *
 * Unlike |RuleProcessorData|, which is similar, a |NodeMatchContext|
 * can vary depending on the selector matching process.  In other words,
 * there might be multiple NodeMatchContexts corresponding to a single
 * node, but only one possible RuleProcessorData.
 */
struct NodeMatchContext {
  // In order to implement nsCSSRuleProcessor::HasStateDependentStyle,
  // we need to be able to see if a node might match an
  // event-state-dependent selector for any value of that event state.
  // So mStateMask contains the states that should NOT be tested.
  //
  // NOTE: For |mStateMask| to work correctly, it's important that any
  // change that changes multiple state bits include all those state
  // bits in the notification.  Otherwise, if multiple states change but
  // we do separate notifications then we might determine the style is
  // not state-dependent when it really is (e.g., determining that a
  // :hover:active rule no longer matches when both states are unset).
  const EventStates mStateMask;

  // Is this link the unique link whose visitedness can affect the style
  // of the node being matched?  (That link is the nearest link to the
  // node being matched that is itself or an ancestor.)
  //
  // Always false when TreeMatchContext::mForStyling is false.  (We
  // could figure it out for SelectorListMatches, but we're starting
  // from the middle of the selector list when doing
  // Has{Attribute,State}DependentStyle, so we can't tell.  So when
  // mForStyling is false, we have to assume we don't know.)
  const bool mIsRelevantLink;

  NodeMatchContext(EventStates aStateMask, bool aIsRelevantLink)
    : mStateMask(aStateMask)
    , mIsRelevantLink(aIsRelevantLink)
  {
  }
};

/**
 * Additional information about a selector (without combinators) that is
 * being matched.
 */
enum class SelectorMatchesFlags : uint8_t {
  NONE = 0,

  // The selector's flags are unknown.  This happens when you don't know
  // if you're starting from the top of a selector.  Only used in cases
  // where it's acceptable for matching to return a false positive.
  // (It's not OK to return a false negative.)
  UNKNOWN = 1 << 0,

  // The selector is part of a compound selector which has been split in
  // half, where the other half is a pseudo-element.  The current
  // selector is not a pseudo-element itself.
  HAS_PSEUDO_ELEMENT = 1 << 1,

  // The selector is part of an argument to a functional pseudo-class or
  // pseudo-element.
  IS_PSEUDO_CLASS_ARGUMENT = 1 << 2
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SelectorMatchesFlags)

static bool ValueIncludes(const nsSubstring& aValueList,
                            const nsSubstring& aValue,
                            const nsStringComparator& aComparator)
{
  const char16_t *p = aValueList.BeginReading(),
              *p_end = aValueList.EndReading();

  while (p < p_end) {
    // skip leading space
    while (p != p_end && nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const char16_t *val_start = p;

    // look for space or end
    while (p != p_end && !nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const char16_t *val_end = p;

    if (val_start < val_end &&
        aValue.Equals(Substring(val_start, val_end), aComparator))
      return true;

    ++p; // we know the next character is not whitespace
  }
  return false;
}

// Return whether the selector matches conditions for the :active and
// :hover quirk.
static inline bool ActiveHoverQuirkMatches(nsCSSSelector* aSelector,
                                           SelectorMatchesFlags aSelectorFlags)
{
  if (aSelector->HasTagSelector() || aSelector->mAttrList ||
      aSelector->mIDList || aSelector->mClassList ||
      aSelector->IsPseudoElement() ||
      // Having this quirk means that some selectors will no longer match,
      // so it's better to return false when we aren't sure (i.e., the
      // flags are unknown).
      aSelectorFlags & (SelectorMatchesFlags::UNKNOWN |
                        SelectorMatchesFlags::HAS_PSEUDO_ELEMENT |
                        SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT)) {
    return false;
  }

  // No pseudo-class other than :active and :hover.
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    if (pseudoClass->mType != nsCSSPseudoClasses::ePseudoClass_hover &&
        pseudoClass->mType != nsCSSPseudoClasses::ePseudoClass_active) {
      return false;
    }
  }

  return true;
}


static inline bool
IsSignificantChild(nsIContent* aChild, bool aTextIsSignificant,
                   bool aWhitespaceIsSignificant)
{
  return nsStyleUtil::IsSignificantChild(aChild, aTextIsSignificant,
                                         aWhitespaceIsSignificant);
}

// This function is to be called once we have fetched a value for an attribute
// whose namespace and name match those of aAttrSelector.  This function
// performs comparisons on the value only, based on aAttrSelector->mFunction.
static bool AttrMatchesValue(const nsAttrSelector* aAttrSelector,
                               const nsString& aValue, bool isHTML)
{
  NS_PRECONDITION(aAttrSelector, "Must have an attribute selector");

  // http://lists.w3.org/Archives/Public/www-style/2008Apr/0038.html
  // *= (CONTAINSMATCH) ~= (INCLUDES) ^= (BEGINSMATCH) $= (ENDSMATCH)
  // all accept the empty string, but match nothing.
  if (aAttrSelector->mValue.IsEmpty() &&
      (aAttrSelector->mFunction == NS_ATTR_FUNC_INCLUDES ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_ENDSMATCH ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_BEGINSMATCH ||
       aAttrSelector->mFunction == NS_ATTR_FUNC_CONTAINSMATCH))
    return false;

  const nsDefaultStringComparator defaultComparator;
  const nsASCIICaseInsensitiveStringComparator ciComparator;
  const nsStringComparator& comparator =
      (aAttrSelector->mCaseSensitive || !isHTML)
                ? static_cast<const nsStringComparator&>(defaultComparator)
                : static_cast<const nsStringComparator&>(ciComparator);

  switch (aAttrSelector->mFunction) {
    case NS_ATTR_FUNC_EQUALS:
      return aValue.Equals(aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_INCLUDES:
      return ValueIncludes(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_DASHMATCH:
      return nsStyleUtil::DashMatchCompare(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_ENDSMATCH:
      return StringEndsWith(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_BEGINSMATCH:
      return StringBeginsWith(aValue, aAttrSelector->mValue, comparator);
    case NS_ATTR_FUNC_CONTAINSMATCH:
      return FindInReadable(aAttrSelector->mValue, aValue, comparator);
    default:
      NS_NOTREACHED("Shouldn't be ending up here");
      return false;
  }
}

static inline bool
edgeChildMatches(Element* aElement, TreeMatchContext& aTreeMatchContext,
                 bool checkFirst, bool checkLast)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return false;
  }

  if (aTreeMatchContext.mForStyling)
    parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

  return (!checkFirst ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, false, false, true) == 1) &&
         (!checkLast ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, false, true, true) == 1);
}

static inline bool
nthChildGenericMatches(Element* aElement,
                       TreeMatchContext& aTreeMatchContext,
                       nsPseudoClassList* pseudoClass,
                       bool isOfType, bool isFromEnd)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return false;
  }

  if (aTreeMatchContext.mForStyling) {
    if (isFromEnd)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  const int32_t index = aTreeMatchContext.mNthIndexCache.
    GetNthIndex(aElement, isOfType, isFromEnd, false);
  if (index <= 0) {
    // Node is anonymous content (not really a child of its parent).
    return false;
  }

  const int32_t a = pseudoClass->u.mNumbers[0];
  const int32_t b = pseudoClass->u.mNumbers[1];
  // result should be true if there exists n >= 0 such that
  // a * n + b == index.
  if (a == 0) {
    return b == index;
  }

  // Integer division in C does truncation (towards 0).  So
  // check that the result is nonnegative, and that there was no
  // truncation.
  const int32_t n = (index - b) / a;
  return n >= 0 && (a * n == index - b);
}

static inline bool
edgeOfTypeMatches(Element* aElement, TreeMatchContext& aTreeMatchContext,
                  bool checkFirst, bool checkLast)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return false;
  }

  if (aTreeMatchContext.mForStyling) {
    if (checkLast)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  return (!checkFirst ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, true, false, true) == 1) &&
         (!checkLast ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, true, true, true) == 1);
}

static inline bool
checkGenericEmptyMatches(Element* aElement,
                         TreeMatchContext& aTreeMatchContext,
                         bool isWhitespaceSignificant)
{
  nsIContent *child = nullptr;
  int32_t index = -1;

  if (aTreeMatchContext.mForStyling)
    aElement->SetFlags(NODE_HAS_EMPTY_SELECTOR);

  do {
    child = aElement->GetChildAt(++index);
    // stop at first non-comment (and non-whitespace for
    // :-moz-only-whitespace) node        
  } while (child && !IsSignificantChild(child, true, isWhitespaceSignificant));
  return (child == nullptr);
}

// Arrays of the states that are relevant for various pseudoclasses.
static const EventStates sPseudoClassStateDependences[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
  EventStates(),
#define CSS_STATE_DEPENDENT_PSEUDO_CLASS(_name, _value, _flags, _pref, _states) \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_DEPENDENT_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  EventStates(),
  EventStates()
};

static const EventStates sPseudoClassStates[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
  EventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _flags, _pref, _states) \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  EventStates(),
  EventStates()
};
static_assert(MOZ_ARRAY_LENGTH(sPseudoClassStates) ==
              nsCSSPseudoClasses::ePseudoClass_NotPseudoClass + 1,
              "ePseudoClass_NotPseudoClass is no longer at the end of"
              "sPseudoClassStates");

static bool
StateSelectorMatches(Element* aElement,
                     nsCSSSelector* aSelector,
                     NodeMatchContext& aNodeMatchContext,
                     TreeMatchContext& aTreeMatchContext,
                     SelectorMatchesFlags aSelectorFlags,
                     bool* const aDependence,
                     EventStates aStatesToCheck)
{
  NS_PRECONDITION(!aStatesToCheck.IsEmpty(),
                  "should only need to call StateSelectorMatches if "
                  "aStatesToCheck is not empty");

  // Bit-based pseudo-classes
  if (aStatesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_ACTIVE |
                                           NS_EVENT_STATE_HOVER) &&
      aTreeMatchContext.mCompatMode == eCompatibility_NavQuirks &&
      ActiveHoverQuirkMatches(aSelector, aSelectorFlags) &&
      aElement->IsHTMLElement() && !nsCSSRuleProcessor::IsLink(aElement)) {
    // In quirks mode, only make links sensitive to selectors ":active"
    // and ":hover".
    return false;
  }

  if (aTreeMatchContext.mForStyling &&
      aStatesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER)) {
    // Mark the element as having :hover-dependent style
    aElement->SetHasRelevantHoverRules();
  }

  if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(aStatesToCheck)) {
    if (aDependence) {
      *aDependence = true;
    }
  } else {
    EventStates contentState =
      nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                                   aElement,
                                   aTreeMatchContext,
                                   aTreeMatchContext.VisitedHandling(),
                                   aNodeMatchContext.mIsRelevantLink);
    if (!contentState.HasAtLeastOneOfStates(aStatesToCheck)) {
      return false;
    }
  }

  return true;
}

static bool
StateSelectorMatches(Element* aElement,
                     nsCSSSelector* aSelector,
                     NodeMatchContext& aNodeMatchContext,
                     TreeMatchContext& aTreeMatchContext,
                     SelectorMatchesFlags aSelectorFlags)
{
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    EventStates statesToCheck = sPseudoClassStates[pseudoClass->mType];
    if (!statesToCheck.IsEmpty() &&
        !StateSelectorMatches(aElement, aSelector, aNodeMatchContext,
                              aTreeMatchContext, aSelectorFlags, nullptr,
                              statesToCheck)) {
      return false;
    }
  }
  return true;
}

// |aDependence| has two functions:
//  * when non-null, it indicates that we're processing a negation,
//    which is done only when SelectorMatches calls itself recursively
//  * what it points to should be set to true whenever a test is skipped
//    because of aNodeMatchContent.mStateMask
static bool SelectorMatches(Element* aElement,
                            nsCSSSelector* aSelector,
                            NodeMatchContext& aNodeMatchContext,
                            TreeMatchContext& aTreeMatchContext,
                            SelectorMatchesFlags aSelectorFlags,
                            bool* const aDependence = nullptr)
{
  NS_PRECONDITION(!aSelector->IsPseudoElement(),
                  "Pseudo-element snuck into SelectorMatches?");
  MOZ_ASSERT(aTreeMatchContext.mForStyling ||
             !aNodeMatchContext.mIsRelevantLink,
             "mIsRelevantLink should be set to false when mForStyling "
             "is false since we don't know how to set it correctly in "
             "Has(Attribute|State)DependentStyle");

  // namespace/tag match
  // optimization : bail out early if we can
  if ((kNameSpaceID_Unknown != aSelector->mNameSpace &&
       aElement->GetNameSpaceID() != aSelector->mNameSpace))
    return false;

  if (aSelector->mLowercaseTag) {
    nsIAtom* selectorTag =
      (aTreeMatchContext.mIsHTMLDocument && aElement->IsHTMLElement()) ?
        aSelector->mLowercaseTag : aSelector->mCasedTag;
    if (selectorTag != aElement->NodeInfo()->NameAtom()) {
      return false;
    }
  }

  nsAtomList* IDList = aSelector->mIDList;
  if (IDList) {
    nsIAtom* id = aElement->GetID();
    if (id) {
      // case sensitivity: bug 93371
      const bool isCaseSensitive =
        aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

      if (isCaseSensitive) {
        do {
          if (IDList->mAtom != id) {
            return false;
          }
          IDList = IDList->mNext;
        } while (IDList);
      } else {
        // Use EqualsIgnoreASCIICase instead of full on unicode case conversion
        // in order to save on performance. This is only used in quirks mode
        // anyway.
        nsDependentAtomString id1Str(id);
        do {
          if (!nsContentUtils::EqualsIgnoreASCIICase(id1Str,
                 nsDependentAtomString(IDList->mAtom))) {
            return false;
          }
          IDList = IDList->mNext;
        } while (IDList);
      }
    } else {
      // Element has no id but we have an id selector
      return false;
    }
  }

  nsAtomList* classList = aSelector->mClassList;
  if (classList) {
    // test for class match
    const nsAttrValue *elementClasses = aElement->GetClasses();
    if (!elementClasses) {
      // Element has no classes but we have a class selector
      return false;
    }

    // case sensitivity: bug 93371
    const bool isCaseSensitive =
      aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

    while (classList) {
      if (!elementClasses->Contains(classList->mAtom,
                                    isCaseSensitive ?
                                      eCaseMatters : eIgnoreCase)) {
        return false;
      }
      classList = classList->mNext;
    }
  }

  const bool isNegated = (aDependence != nullptr);
  // The selectors for which we set node bits are, unfortunately, early
  // in this function (because they're pseudo-classes, which are
  // generally quick to test, and thus earlier).  If they were later,
  // we'd probably avoid setting those bits in more cases where setting
  // them is unnecessary.
  NS_ASSERTION(aNodeMatchContext.mStateMask.IsEmpty() ||
               !aTreeMatchContext.mForStyling,
               "mForStyling must be false if we're just testing for "
               "state-dependence");

  // test for pseudo class match
  for (nsPseudoClassList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    EventStates statesToCheck = sPseudoClassStates[pseudoClass->mType];
    if (statesToCheck.IsEmpty()) {
      // keep the cases here in the same order as the list in
      // nsCSSPseudoClassList.h
      switch (pseudoClass->mType) {
      case nsCSSPseudoClasses::ePseudoClass_empty:
        if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozOnlyWhitespace:
        if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, false)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozEmptyExceptChildrenWithLocalname:
        {
          NS_ASSERTION(pseudoClass->u.mString, "Must have string!");
          nsIContent *child = nullptr;
          int32_t index = -1;

          if (aTreeMatchContext.mForStyling)
            // FIXME:  This isn't sufficient to handle:
            //   :-moz-empty-except-children-with-localname() + E
            //   :-moz-empty-except-children-with-localname() ~ E
            // because we don't know to restyle the grandparent of the
            // inserted/removed element (as in bug 534804 for :empty).
            aElement->SetFlags(NODE_HAS_SLOW_SELECTOR);
          do {
            child = aElement->GetChildAt(++index);
          } while (child &&
                   (!IsSignificantChild(child, true, false) ||
                    (child->GetNameSpaceID() == aElement->GetNameSpaceID() &&
                     child->NodeInfo()->NameAtom()->Equals(nsDependentString(pseudoClass->u.mString)))));
          if (child != nullptr) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lang:
        {
          NS_ASSERTION(nullptr != pseudoClass->u.mString, "null lang parameter");
          if (!pseudoClass->u.mString || !*pseudoClass->u.mString) {
            return false;
          }

          // We have to determine the language of the current element.  Since
          // this is currently no property and since the language is inherited
          // from the parent we have to be prepared to look at all parent
          // nodes.  The language itself is encoded in the LANG attribute.
          nsAutoString language;
          aElement->GetLang(language);
          if (!language.IsEmpty()) {
            if (!nsStyleUtil::DashMatchCompare(language,
                                               nsDependentString(pseudoClass->u.mString),
                                               nsASCIICaseInsensitiveStringComparator())) {
              return false;
            }
            // This pseudo-class matched; move on to the next thing
            break;
          }

          nsIDocument* doc = aTreeMatchContext.mDocument;
          if (doc) {
            // Try to get the language from the HTTP header or if this
            // is missing as well from the preferences.
            // The content language can be a comma-separated list of
            // language codes.
            doc->GetContentLanguage(language);

            nsDependentString langString(pseudoClass->u.mString);
            language.StripWhitespace();
            int32_t begin = 0;
            int32_t len = language.Length();
            while (begin < len) {
              int32_t end = language.FindChar(char16_t(','), begin);
              if (end == kNotFound) {
                end = len;
              }
              if (nsStyleUtil::DashMatchCompare(Substring(language, begin,
                                                          end-begin),
                                                langString,
                                                nsASCIICaseInsensitiveStringComparator())) {
                break;
              }
              begin = end + 1;
            }
            if (begin < len) {
              // This pseudo-class matched
              break;
            }
          }
        }
        return false;

      case nsCSSPseudoClasses::ePseudoClass_mozBoundElement:
        if (aTreeMatchContext.mScopedRoot != aElement) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_root:
        if (aElement != aElement->OwnerDoc()->GetRootElement()) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_any:
        {
          nsCSSSelectorList *l;
          for (l = pseudoClass->u.mSelectors; l; l = l->mNext) {
            nsCSSSelector *s = l->mSelectors;
            MOZ_ASSERT(!s->mNext && !s->IsPseudoElement(),
                       "parser failed");
            if (SelectorMatches(
                  aElement, s, aNodeMatchContext, aTreeMatchContext,
                  SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT)) {
              break;
            }
          }
          if (!l) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, true, false)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstNode:
        {
          nsIContent *firstNode = nullptr;
          nsIContent *parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

            int32_t index = -1;
            do {
              firstNode = parent->GetChildAt(++index);
              // stop at first non-comment and non-whitespace node
            } while (firstNode &&
                     !IsSignificantChild(firstNode, true, false));
          }
          if (aElement != firstNode) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, false, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastNode:
        {
          nsIContent *lastNode = nullptr;
          nsIContent *parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);
            
            uint32_t index = parent->GetChildCount();
            do {
              lastNode = parent->GetChildAt(--index);
              // stop at first non-comment and non-whitespace node
            } while (lastNode &&
                     !IsSignificantChild(lastNode, true, false));
          }
          if (aElement != lastNode) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_onlyChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, true, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, true, false)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, false, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_onlyOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, true, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthChild:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    false, false)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthLastChild:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    false, true)) {
          return false;
        }
      break;

      case nsCSSPseudoClasses::ePseudoClass_nthOfType:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    true, false)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthLastOfType:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    true, true)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozIsHTML:
        if (!aTreeMatchContext.mIsHTMLDocument || !aElement->IsHTMLElement()) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozNativeAnonymous:
        if (!aElement->IsInNativeAnonymousSubtree()) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozSystemMetric:
        {
          nsCOMPtr<nsIAtom> metric = do_GetAtom(pseudoClass->u.mString);
          if (!nsCSSRuleProcessor::HasSystemMetric(metric)) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLocaleDir:
        {
          bool docIsRTL =
            aTreeMatchContext.mDocument->GetDocumentState().
              HasState(NS_DOCUMENT_STATE_RTL_LOCALE);

          nsDependentString dirString(pseudoClass->u.mString);

          if (dirString.EqualsLiteral("rtl")) {
            if (!docIsRTL) {
              return false;
            }
          } else if (dirString.EqualsLiteral("ltr")) {
            if (docIsRTL) {
              return false;
            }
          } else {
            // Selectors specifying other directions never match.
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWTheme:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() <=
                nsIDocument::Doc_Theme_None) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWThemeBrightText:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
                nsIDocument::Doc_Theme_Bright) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWThemeDarkText:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
                nsIDocument::Doc_Theme_Dark) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozWindowInactive:
        if (!aTreeMatchContext.mDocument->GetDocumentState().
               HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
          return false;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozTableBorderNonzero:
        {
          if (!aElement->IsHTMLElement(nsGkAtoms::table)) {
            return false;
          }
          const nsAttrValue *val = aElement->GetParsedAttr(nsGkAtoms::border);
          if (!val ||
              (val->Type() == nsAttrValue::eInteger &&
               val->GetIntegerValue() == 0)) {
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_dir:
        {
          if (aDependence) {
            EventStates states
              = sPseudoClassStateDependences[pseudoClass->mType];
            if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(states)) {
              *aDependence = true;
              return false;
            }
          }

          // If we only had to consider HTML, directionality would be
          // exclusively LTR or RTL.
          //
          // However, in markup languages where there is no direction attribute
          // we have to consider the possibility that neither -moz-dir(rtl) nor
          // -moz-dir(ltr) matches.
          EventStates state = aElement->StyleState();
          nsDependentString dirString(pseudoClass->u.mString);

          if (dirString.EqualsLiteral("rtl")) {
            if (!state.HasState(NS_EVENT_STATE_RTL)) {
              return false;
            }
          } else if (dirString.EqualsLiteral("ltr")) {
            if (!state.HasState(NS_EVENT_STATE_LTR)) {
              return false;
            }
          } else {
            // Selectors specifying other directions never match.
            return false;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_scope:
        if (aTreeMatchContext.mForScopedStyle) {
          if (aTreeMatchContext.mCurrentStyleScope) {
            // If mCurrentStyleScope is null, aElement must be the style
            // scope root.  This is because the PopStyleScopeForSelectorMatching
            // call in SelectorMatchesTree sets mCurrentStyleScope to null
            // as soon as we visit the style scope element, and we won't
            // progress further up the tree after this call to
            // SelectorMatches.  Thus if mCurrentStyleScope is still set,
            // we know the selector does not match.
            return false;
          }
        } else if (aTreeMatchContext.HasSpecifiedScope()) {
          if (!aTreeMatchContext.IsScopeElement(aElement)) {
            return false;
          }
        } else {
          if (aElement != aElement->OwnerDoc()->GetRootElement()) {
            return false;
          }
        }
        break;

      default:
        MOZ_ASSERT(false, "How did that happen?");
      }
    } else {
      if (!StateSelectorMatches(aElement, aSelector, aNodeMatchContext,
                                aTreeMatchContext, aSelectorFlags, aDependence,
                                statesToCheck)) {
        return false;
      }
    }
  }

  bool result = true;
  if (aSelector->mAttrList) {
    // test for attribute match
    if (!aElement->HasAttrs()) {
      // if no attributes on the content, no match
      return false;
    } else {
      result = true;
      nsAttrSelector* attr = aSelector->mAttrList;
      nsIAtom* matchAttribute;

      do {
        bool isHTML =
          (aTreeMatchContext.mIsHTMLDocument && aElement->IsHTMLElement());
        matchAttribute = isHTML ? attr->mLowercaseAttr : attr->mCasedAttr;
        if (attr->mNameSpace == kNameSpaceID_Unknown) {
          // Attr selector with a wildcard namespace.  We have to examine all
          // the attributes on our content node....  This sort of selector is
          // essentially a boolean OR, over all namespaces, of equivalent attr
          // selectors with those namespaces.  So to evaluate whether it
          // matches, evaluate for each namespace (the only namespaces that
          // have a chance at matching, of course, are ones that the element
          // actually has attributes in), short-circuiting if we ever match.
          result = false;
          const nsAttrName* attrName;
          for (uint32_t i = 0; (attrName = aElement->GetAttrNameAt(i)); ++i) {
            if (attrName->LocalName() != matchAttribute) {
              continue;
            }
            if (attr->mFunction == NS_ATTR_FUNC_SET) {
              result = true;
            } else {
              nsAutoString value;
#ifdef DEBUG
              bool hasAttr =
#endif
                aElement->GetAttr(attrName->NamespaceID(),
                                  attrName->LocalName(), value);
              NS_ASSERTION(hasAttr, "GetAttrNameAt lied");
              result = AttrMatchesValue(attr, value, isHTML);
            }

            // At this point |result| has been set by us
            // explicitly in this loop.  If it's false, we may still match
            // -- the content may have another attribute with the same name but
            // in a different namespace.  But if it's true, we are done (we
            // can short-circuit the boolean OR described above).
            if (result) {
              break;
            }
          }
        }
        else if (attr->mFunction == NS_ATTR_FUNC_EQUALS) {
          result =
            aElement->
              AttrValueIs(attr->mNameSpace, matchAttribute, attr->mValue,
                          (!isHTML || attr->mCaseSensitive) ? eCaseMatters
                                                            : eIgnoreCase);
        }
        else if (!aElement->HasAttr(attr->mNameSpace, matchAttribute)) {
          result = false;
        }
        else if (attr->mFunction != NS_ATTR_FUNC_SET) {
          nsAutoString value;
#ifdef DEBUG
          bool hasAttr =
#endif
              aElement->GetAttr(attr->mNameSpace, matchAttribute, value);
          NS_ASSERTION(hasAttr, "HasAttr lied");
          result = AttrMatchesValue(attr, value, isHTML);
        }
        
        attr = attr->mNext;
      } while (attr && result);
    }
  }

  // apply SelectorMatches to the negated selectors in the chain
  if (!isNegated) {
    for (nsCSSSelector *negation = aSelector->mNegations;
         result && negation; negation = negation->mNegations) {
      bool dependence = false;
      result = !SelectorMatches(aElement, negation, aNodeMatchContext,
                                aTreeMatchContext,
                                SelectorMatchesFlags::IS_PSEUDO_CLASS_ARGUMENT,
                                &dependence);
      // If the selector does match due to the dependence on
      // aNodeMatchContext.mStateMask, then we want to keep result true
      // so that the final result of SelectorMatches is true.  Doing so
      // tells StateEnumFunc that there is a dependence on the state.
      result = result || dependence;
    }
  }
  return result;
}

#undef STATE_CHECK

#ifdef DEBUG
static bool
HasPseudoClassSelectorArgsWithCombinators(nsCSSSelector* aSelector)
{
  for (nsPseudoClassList* p = aSelector->mPseudoClassList; p; p = p->mNext) {
    if (nsCSSPseudoClasses::HasSelectorListArg(p->mType)) {
      for (nsCSSSelectorList* l = p->u.mSelectors; l; l = l->mNext) {
        if (l->mSelectors->mNext) {
          return true;
        }
      }
    }
  }
  for (nsCSSSelector* n = aSelector->mNegations; n; n = n->mNegations) {
    if (n->mNext) {
      return true;
    }
  }
  return false;
}
#endif

/* static */ bool
nsCSSRuleProcessor::RestrictedSelectorMatches(
    Element* aElement,
    nsCSSSelector* aSelector,
    TreeMatchContext& aTreeMatchContext)
{
  MOZ_ASSERT(aSelector->IsRestrictedSelector(),
             "aSelector must not have a pseudo-element");

  NS_WARN_IF_FALSE(!HasPseudoClassSelectorArgsWithCombinators(aSelector),
                   "processing eRestyle_SomeDescendants can be slow if "
                   "pseudo-classes with selector arguments can now have "
                   "combinators in them");

  // We match aSelector as if :visited and :link both match visited and
  // unvisited links.

  NodeMatchContext nodeContext(EventStates(),
                               nsCSSRuleProcessor::IsLink(aElement));
  if (nodeContext.mIsRelevantLink) {
    aTreeMatchContext.SetHaveRelevantLink();
  }
  aTreeMatchContext.ResetForUnvisitedMatching();
  bool matches = SelectorMatches(aElement, aSelector, nodeContext,
                                 aTreeMatchContext, SelectorMatchesFlags::NONE);
  if (nodeContext.mIsRelevantLink) {
    aTreeMatchContext.ResetForVisitedMatching();
    if (SelectorMatches(aElement, aSelector, nodeContext, aTreeMatchContext,
                        SelectorMatchesFlags::NONE)) {
      matches = true;
    }
  }
  return matches;
}

// Right now, there are four operators:
//   ' ', the descendant combinator, is greedy
//   '~', the indirect adjacent sibling combinator, is greedy
//   '+' and '>', the direct adjacent sibling and child combinators, are not
#define NS_IS_GREEDY_OPERATOR(ch) \
  ((ch) == char16_t(' ') || (ch) == char16_t('~'))

/**
 * Flags for SelectorMatchesTree.
 */
enum SelectorMatchesTreeFlags {
  // Whether we still have not found the closest ancestor link element and
  // thus have to check the current element for it.
  eLookForRelevantLink = 0x1,

  // Whether SelectorMatchesTree should check for, and return true upon
  // finding, an ancestor element that has an eRestyle_SomeDescendants
  // restyle hint pending.
  eMatchOnConditionalRestyleAncestor = 0x2,
};

static bool
SelectorMatchesTree(Element* aPrevElement,
                    nsCSSSelector* aSelector,
                    TreeMatchContext& aTreeMatchContext,
                    SelectorMatchesTreeFlags aFlags)
{
  MOZ_ASSERT(!aSelector || !aSelector->IsPseudoElement());
  nsCSSSelector* selector = aSelector;
  Element* prevElement = aPrevElement;
  while (selector) { // check compound selectors
    NS_ASSERTION(!selector->mNext ||
                 selector->mNext->mOperator != char16_t(0),
                 "compound selector without combinator");

    // If after the previous selector match we are now outside the
    // current style scope, we don't need to match any further.
    if (aTreeMatchContext.mForScopedStyle &&
        !aTreeMatchContext.IsWithinStyleScopeForSelectorMatching()) {
      return false;
    }

    // for adjacent sibling combinators, the content to test against the
    // selector is the previous sibling *element*
    Element* element = nullptr;
    if (char16_t('+') == selector->mOperator ||
        char16_t('~') == selector->mOperator) {
      // The relevant link must be an ancestor of the node being matched.
      aFlags = SelectorMatchesTreeFlags(aFlags & ~eLookForRelevantLink);
      nsIContent* parent = prevElement->GetParent();
      if (parent) {
        if (aTreeMatchContext.mForStyling)
          parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);

        element = prevElement->GetPreviousElementSibling();
      }
    }
    // for descendant combinators and child combinators, the element
    // to test against is the parent
    else {
      nsIContent *content = prevElement->GetParent();
      // GetParent could return a document fragment; we only want
      // element parents.
      if (content && content->IsElement()) {
        element = content->AsElement();
        if (aTreeMatchContext.mForScopedStyle) {
          // We are moving up to the parent element; tell the
          // TreeMatchContext, so that in case this element is the
          // style scope element, selector matching stops before we
          // traverse further up the tree.
          aTreeMatchContext.PopStyleScopeForSelectorMatching(element);
        }

        // Compatibility hack: First try matching this selector as though the
        // <xbl:children> element wasn't in the tree to allow old selectors
        // were written before <xbl:children> participated in CSS selector
        // matching to work.
        if (selector->mOperator == '>' && element->IsActiveChildrenElement()) {
          Element* styleScope = aTreeMatchContext.mCurrentStyleScope;
          if (SelectorMatchesTree(element, selector, aTreeMatchContext,
                                  aFlags)) {
            // It matched, don't try matching on the <xbl:children> element at
            // all.
            return true;
          }
          // We want to reset mCurrentStyleScope on aTreeMatchContext
          // back to its state before the SelectorMatchesTree call, in
          // case that call happens to traverse past the style scope element
          // and sets it to null.
          aTreeMatchContext.mCurrentStyleScope = styleScope;
        }
      }
    }
    if (!element) {
      return false;
    }
    if ((aFlags & eMatchOnConditionalRestyleAncestor) &&
        element->HasFlag(ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR)) {
      // If we're looking at an element that we already generated an
      // eRestyle_SomeDescendants restyle hint for, then we should pretend
      // that we matched here, because we don't know what the values of
      // attributes on |element| were at the time we generated the
      // eRestyle_SomeDescendants.  This causes AttributeEnumFunc and
      // HasStateDependentStyle below to generate a restyle hint for the
      // change we're currently looking at, as we don't know whether the LHS
      // of the selector we looked up matches or not.  (We only pass in aFlags
      // to cause us to look for eRestyle_SomeDescendants here under
      // AttributeEnumFunc and HasStateDependentStyle.)
      return true;
    }
    const bool isRelevantLink = (aFlags & eLookForRelevantLink) &&
                                nsCSSRuleProcessor::IsLink(element);
    NodeMatchContext nodeContext(EventStates(), isRelevantLink);
    if (isRelevantLink) {
      // If we find an ancestor of the matched node that is a link
      // during the matching process, then it's the relevant link (see
      // constructor call above).
      // Since we are still matching against selectors that contain
      // :visited (they'll just fail), we will always find such a node
      // during the selector matching process if there is a relevant
      // link that can influence selector matching.
      aFlags = SelectorMatchesTreeFlags(aFlags & ~eLookForRelevantLink);
      aTreeMatchContext.SetHaveRelevantLink();
    }
    if (SelectorMatches(element, selector, nodeContext, aTreeMatchContext,
                        SelectorMatchesFlags::NONE)) {
      // to avoid greedy matching, we need to recur if this is a
      // descendant or general sibling combinator and the next
      // combinator is different, but we can make an exception for
      // sibling, then parent, since a sibling's parent is always the
      // same.
      if (NS_IS_GREEDY_OPERATOR(selector->mOperator) &&
          selector->mNext &&
          selector->mNext->mOperator != selector->mOperator &&
          !(selector->mOperator == '~' &&
            NS_IS_ANCESTOR_OPERATOR(selector->mNext->mOperator))) {

        // pretend the selector didn't match, and step through content
        // while testing the same selector

        // This approach is slightly strange in that when it recurs
        // it tests from the top of the content tree, down.  This
        // doesn't matter much for performance since most selectors
        // don't match.  (If most did, it might be faster...)
        Element* styleScope = aTreeMatchContext.mCurrentStyleScope;
        if (SelectorMatchesTree(element, selector, aTreeMatchContext, aFlags)) {
          return true;
        }
        // We want to reset mCurrentStyleScope on aTreeMatchContext
        // back to its state before the SelectorMatchesTree call, in
        // case that call happens to traverse past the style scope element
        // and sets it to null.
        aTreeMatchContext.mCurrentStyleScope = styleScope;
      }
      selector = selector->mNext;
    }
    else {
      // for adjacent sibling and child combinators, if we didn't find
      // a match, we're done
      if (!NS_IS_GREEDY_OPERATOR(selector->mOperator)) {
        return false;  // parent was required to match
      }
    }
    prevElement = element;
  }
  return true; // all the selectors matched.
}

static inline
void ContentEnumFunc(const RuleValue& value, nsCSSSelector* aSelector,
                     ElementDependentRuleProcessorData* data, NodeMatchContext& nodeContext,
                     AncestorFilter *ancestorFilter)
{
  if (nodeContext.mIsRelevantLink) {
    data->mTreeMatchContext.SetHaveRelevantLink();
  }
  if (ancestorFilter &&
      !ancestorFilter->MightHaveMatchingAncestor<RuleValue::eMaxAncestorHashes>(
          value.mAncestorSelectorHashes)) {
    // We won't match; nothing else to do here
    return;
  }
  if (!data->mTreeMatchContext.SetStyleScopeForSelectorMatching(data->mElement,
                                                                data->mScope)) {
    // The selector is for a rule in a scoped style sheet, and the subject
    // of the selector matching is not in its scope.
    return;
  }
  nsCSSSelector* selector = aSelector;
  if (selector->IsPseudoElement()) {
    PseudoElementRuleProcessorData* pdata =
      static_cast<PseudoElementRuleProcessorData*>(data);
    if (!pdata->mPseudoElement && selector->mPseudoClassList) {
      // We can get here when calling getComputedStyle(aElt, aPseudo) if:
      //
      //   * aPseudo is a pseudo-element that supports a user action
      //     pseudo-class, like "::-moz-placeholder";
      //   * there is a style rule that uses a pseudo-class on this
      //     pseudo-element in the document, like ::-moz-placeholder:hover; and
      //   * aElt does not have such a pseudo-element.
      //
      // We know that the selector can't match, since there is no element for
      // the user action pseudo-class to match against.
      return;
    }
    if (!StateSelectorMatches(pdata->mPseudoElement, aSelector, nodeContext,
                              data->mTreeMatchContext,
                              SelectorMatchesFlags::NONE)) {
      return;
    }
    selector = selector->mNext;
  }

  SelectorMatchesFlags selectorFlags = SelectorMatchesFlags::NONE;
  if (aSelector->IsPseudoElement()) {
    selectorFlags |= SelectorMatchesFlags::HAS_PSEUDO_ELEMENT;
  }
  if (SelectorMatches(data->mElement, selector, nodeContext,
                      data->mTreeMatchContext, selectorFlags)) {
    nsCSSSelector *next = selector->mNext;
    if (!next ||
        SelectorMatchesTree(data->mElement, next,
                            data->mTreeMatchContext,
                            nodeContext.mIsRelevantLink ?
                              SelectorMatchesTreeFlags(0) :
                              eLookForRelevantLink)) {
      css::StyleRule *rule = value.mRule;
      rule->RuleMatched();
      data->mRuleWalker->Forward(rule);
      // nsStyleSet will deal with the !important rule
    }
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(ElementRuleProcessorData *aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade) {
    NodeMatchContext nodeContext(EventStates(),
                                 nsCSSRuleProcessor::IsLink(aData->mElement));
    cascade->mRuleHash.EnumerateAllRules(aData->mElement, aData, nodeContext);
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade) {
    RuleHash* ruleHash = cascade->mPseudoElementRuleHashes[aData->mPseudoType];
    if (ruleHash) {
      NodeMatchContext nodeContext(EventStates(),
                                   nsCSSRuleProcessor::IsLink(aData->mElement));
      ruleHash->EnumerateAllRules(aData->mElement, aData, nodeContext);
    }
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade && cascade->mAnonBoxRules.EntryCount()) {
    RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>
      (PL_DHashTableSearch(&cascade->mAnonBoxRules, aData->mPseudoTag));
    if (entry) {
      nsTArray<RuleValue>& rules = entry->mRules;
      for (RuleValue *value = rules.Elements(), *end = value + rules.Length();
           value != end; ++value) {
        value->mRule->RuleMatched();
        aData->mRuleWalker->Forward(value->mRule);
      }
    }
  }
}

#ifdef MOZ_XUL
/* virtual */ void
nsCSSRuleProcessor::RulesMatching(XULTreeRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade && cascade->mXULTreeRules.EntryCount()) {
    RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>
      (PL_DHashTableSearch(&cascade->mXULTreeRules, aData->mPseudoTag));
    if (entry) {
      NodeMatchContext nodeContext(EventStates(),
                                   nsCSSRuleProcessor::IsLink(aData->mElement));
      nsTArray<RuleValue>& rules = entry->mRules;
      for (RuleValue *value = rules.Elements(), *end = value + rules.Length();
           value != end; ++value) {
        if (aData->mComparator->PseudoMatches(value->mSelector)) {
          ContentEnumFunc(*value, value->mSelector->mNext, aData, nodeContext,
                          nullptr);
        }
      }
    }
  }
}
#endif

static inline nsRestyleHint RestyleHintForOp(char16_t oper)
{
  if (oper == char16_t('+') || oper == char16_t('~')) {
    return eRestyle_LaterSiblings;
  }

  if (oper != char16_t(0)) {
    return eRestyle_Subtree;
  }

  return eRestyle_Self;
}

nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(ElementDependentRuleProcessorData* aData,
                                           Element* aStatefulElement,
                                           nsCSSPseudoElements::Type aPseudoType,
                                           EventStates aStateMask)
{
  MOZ_ASSERT(!aData->mTreeMatchContext.mForScopedStyle,
             "mCurrentStyleScope will need to be saved and restored after the "
             "SelectorMatchesTree call");

  bool isPseudoElement =
    aPseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement;

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  // Look up the content node in the state rule list, which points to
  // any (CSS2 definition) simple selector (whether or not it is the
  // subject) that has a state pseudo-class on it.  This means that this
  // code will be matching selectors that aren't real selectors in any
  // stylesheet (e.g., if there is a selector "body > p:hover > a", then
  // "body > p:hover" will be in |cascade->mStateSelectors|).  Note that
  // |ComputeSelectorStateDependence| below determines which selectors are in
  // |cascade->mStateSelectors|.
  nsRestyleHint hint = nsRestyleHint(0);
  if (cascade) {
    StateSelector *iter = cascade->mStateSelectors.Elements(),
                  *end = iter + cascade->mStateSelectors.Length();
    NodeMatchContext nodeContext(aStateMask, false);
    for(; iter != end; ++iter) {
      nsCSSSelector* selector = iter->mSelector;
      EventStates states = iter->mStates;

      if (selector->IsPseudoElement() != isPseudoElement) {
        continue;
      }

      nsCSSSelector* selectorForPseudo;
      if (isPseudoElement) {
        if (selector->PseudoType() != aPseudoType) {
          continue;
        }
        selectorForPseudo = selector;
        selector = selector->mNext;
      }

      nsRestyleHint possibleChange = RestyleHintForOp(selector->mOperator);
      SelectorMatchesFlags selectorFlags = SelectorMatchesFlags::UNKNOWN;

      // If hint already includes all the bits of possibleChange,
      // don't bother calling SelectorMatches, since even if it returns false
      // hint won't change.
      // Also don't bother calling SelectorMatches if none of the
      // states passed in are relevant here.
      if ((possibleChange & ~hint) &&
          states.HasAtLeastOneOfStates(aStateMask) &&
          // We can optimize away testing selectors that only involve :hover, a
          // namespace, and a tag name against nodes that don't have the
          // NodeHasRelevantHoverRules flag: such a selector didn't match
          // the tag name or namespace the first time around (since the :hover
          // didn't set the NodeHasRelevantHoverRules flag), so it won't
          // match it now.  Check for our selector only having :hover states, or
          // the element having the hover rules flag, or the selector having
          // some sort of non-namespace, non-tagname data in it.
          (states != NS_EVENT_STATE_HOVER ||
           aStatefulElement->HasRelevantHoverRules() ||
           selector->mIDList || selector->mClassList ||
           // We generally expect an mPseudoClassList, since we have a :hover.
           // The question is whether we have anything else in there.
           (selector->mPseudoClassList &&
            (selector->mPseudoClassList->mNext ||
             selector->mPseudoClassList->mType !=
               nsCSSPseudoClasses::ePseudoClass_hover)) ||
           selector->mAttrList || selector->mNegations) &&
          (!isPseudoElement ||
           StateSelectorMatches(aStatefulElement, selectorForPseudo,
                                nodeContext, aData->mTreeMatchContext,
                                selectorFlags, nullptr, aStateMask)) &&
          SelectorMatches(aData->mElement, selector, nodeContext,
                          aData->mTreeMatchContext, selectorFlags) &&
          SelectorMatchesTree(aData->mElement, selector->mNext,
                              aData->mTreeMatchContext,
                              eMatchOnConditionalRestyleAncestor))
      {
        hint = nsRestyleHint(hint | possibleChange);
      }
    }
  }
  return hint;
}

nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return HasStateDependentStyle(aData,
                                aData->mElement,
                                nsCSSPseudoElements::ePseudo_NotPseudoElement,
                                aData->mStateMask);
}

nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return HasStateDependentStyle(aData,
                                aData->mPseudoElement,
                                aData->mPseudoType,
                                aData->mStateMask);
}

bool
nsCSSRuleProcessor::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  return cascade && cascade->mSelectorDocumentStates.HasAtLeastOneOfStates(aData->mStateMask);
}

struct AttributeEnumData {
  AttributeEnumData(AttributeRuleProcessorData *aData,
                    RestyleHintData& aRestyleHintData)
    : data(aData), change(nsRestyleHint(0)), hintData(aRestyleHintData) {}

  AttributeRuleProcessorData *data;
  nsRestyleHint change;
  RestyleHintData& hintData;
};


static inline nsRestyleHint
RestyleHintForSelectorWithAttributeChange(nsRestyleHint aCurrentHint,
                                          nsCSSSelector* aSelector,
                                          nsCSSSelector* aRightmostSelector)
{
  MOZ_ASSERT(aSelector);

  char16_t oper = aSelector->mOperator;

  if (oper == char16_t('+') || oper == char16_t('~')) {
    return eRestyle_LaterSiblings;
  }

  if (oper == char16_t(':')) {
    return eRestyle_Subtree;
  }

  if (oper != char16_t(0)) {
    // Check whether the selector is in a form that supports
    // eRestyle_SomeDescendants.  If it isn't, return eRestyle_Subtree.

    if (aCurrentHint & eRestyle_Subtree) {
      // No point checking, since we'll end up restyling the whole
      // subtree anyway.
      return eRestyle_Subtree;
    }

    if (!aRightmostSelector) {
      // aSelector wasn't a top-level selector, which means we were inside
      // a :not() or :-moz-any().  We don't support that.
      return eRestyle_Subtree;
    }

    MOZ_ASSERT(aSelector != aRightmostSelector,
               "if aSelector == aRightmostSelector then we should have "
               "no operator");

    // Check that aRightmostSelector can be passed to RestrictedSelectorMatches.
    if (!aRightmostSelector->IsRestrictedSelector()) {
      return eRestyle_Subtree;
    }

    // We also don't support pseudo-elements on any of the selectors
    // between aRightmostSelector and aSelector.
    // XXX Can we lift this restriction, so that we don't have to loop
    // over all the selectors?
    for (nsCSSSelector* sel = aRightmostSelector->mNext;
         sel != aSelector;
         sel = sel->mNext) {
      MOZ_ASSERT(sel, "aSelector must be reachable from aRightmostSelector");
      if (sel->PseudoType() != nsCSSPseudoElements::ePseudo_NotPseudoElement) {
        return eRestyle_Subtree;
      }
    }

    return eRestyle_SomeDescendants;
  }

  return eRestyle_Self;
}

static void
AttributeEnumFunc(nsCSSSelector* aSelector,
                  nsCSSSelector* aRightmostSelector,
                  AttributeEnumData* aData)
{
  AttributeRuleProcessorData *data = aData->data;

  if (!data->mTreeMatchContext.SetStyleScopeForSelectorMatching(data->mElement,
                                                                data->mScope)) {
    // The selector is for a rule in a scoped style sheet, and the subject
    // of the selector matching is not in its scope.
    return;
  }

  nsRestyleHint possibleChange =
    RestyleHintForSelectorWithAttributeChange(aData->change,
                                              aSelector, aRightmostSelector);

  // If, ignoring eRestyle_SomeDescendants, enumData->change already includes
  // all the bits of possibleChange, don't bother calling SelectorMatches, since
  // even if it returns false enumData->change won't change.  If possibleChange
  // has eRestyle_SomeDescendants, we need to call SelectorMatches(Tree)
  // regardless as it might give us new selectors to append to
  // mSelectorsForDescendants.
  NodeMatchContext nodeContext(EventStates(), false);
  if (((possibleChange & (~(aData->change) | eRestyle_SomeDescendants))) &&
      SelectorMatches(data->mElement, aSelector, nodeContext,
                      data->mTreeMatchContext, SelectorMatchesFlags::UNKNOWN) &&
      SelectorMatchesTree(data->mElement, aSelector->mNext,
                          data->mTreeMatchContext,
                          eMatchOnConditionalRestyleAncestor)) {
    aData->change = nsRestyleHint(aData->change | possibleChange);
    if (possibleChange & eRestyle_SomeDescendants) {
      aData->hintData.mSelectorsForDescendants.AppendElement(aRightmostSelector);
    }
  }
}

static MOZ_ALWAYS_INLINE void
EnumerateSelectors(nsTArray<SelectorPair>& aSelectors, AttributeEnumData* aData)
{
  SelectorPair *iter = aSelectors.Elements(),
               *end = iter + aSelectors.Length();
  for (; iter != end; ++iter) {
    AttributeEnumFunc(iter->mSelector, iter->mRightmostSelector, aData);
  }
}

static MOZ_ALWAYS_INLINE void
EnumerateSelectors(nsTArray<nsCSSSelector*>& aSelectors, AttributeEnumData* aData)
{
  nsCSSSelector **iter = aSelectors.Elements(),
                **end = iter + aSelectors.Length();
  for (; iter != end; ++iter) {
    AttributeEnumFunc(*iter, nullptr, aData);
  }
}

nsRestyleHint
nsCSSRuleProcessor::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
{
  //  We could try making use of aData->mModType, but :not rules make it a bit
  //  of a pain to do so...  So just ignore it for now.

  AttributeEnumData data(aData, aRestyleHintDataResult);

  // Don't do our special handling of certain attributes if the attr
  // hasn't changed yet.
  if (aData->mAttrHasChanged) {
    // check for the lwtheme and lwthemetextcolor attribute on root XUL elements
    if ((aData->mAttribute == nsGkAtoms::lwtheme ||
         aData->mAttribute == nsGkAtoms::lwthemetextcolor) &&
        aData->mElement->GetNameSpaceID() == kNameSpaceID_XUL &&
        aData->mElement == aData->mElement->OwnerDoc()->GetRootElement())
      {
        data.change = nsRestyleHint(data.change | eRestyle_Subtree);
      }

    // We don't know the namespace of the attribute, and xml:lang applies to
    // all elements.  If the lang attribute changes, we need to restyle our
    // whole subtree, since the :lang selector on our descendants can examine
    // our lang attribute.
    if (aData->mAttribute == nsGkAtoms::lang) {
      data.change = nsRestyleHint(data.change | eRestyle_Subtree);
    }
  }

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  // Since we get both before and after notifications for attributes, we
  // don't have to ignore aData->mAttribute while matching.  Just check
  // whether we have selectors relevant to aData->mAttribute that we
  // match.  If this is the before change notification, that will catch
  // rules we might stop matching; if the after change notification, the
  // ones we might have started matching.
  if (cascade) {
    if (aData->mAttribute == nsGkAtoms::id) {
      nsIAtom* id = aData->mElement->GetID();
      if (id) {
        AtomSelectorEntry *entry =
          static_cast<AtomSelectorEntry*>
                     (PL_DHashTableSearch(&cascade->mIdSelectors, id));
        if (entry) {
          EnumerateSelectors(entry->mSelectors, &data);
        }
      }

      EnumerateSelectors(cascade->mPossiblyNegatedIDSelectors, &data);
    }

    if (aData->mAttribute == nsGkAtoms::_class) {
      const nsAttrValue* otherClasses = aData->mOtherValue;
      NS_ASSERTION(otherClasses ||
                   aData->mModType == nsIDOMMutationEvent::REMOVAL,
                   "All class values should be StoresOwnData and parsed"
                   "via Element::BeforeSetAttr, so available here");
      // For WillChange, enumerate classes that will be removed to see which
      // rules apply before the change.
      // For Changed, enumerate classes that have been added to see which rules
      // apply after the change.
      // In both cases we're interested in the classes that are currently on
      // the element but not in mOtherValue.
      const nsAttrValue* elementClasses = aData->mElement->GetClasses();
      if (elementClasses) {
        int32_t atomCount = elementClasses->GetAtomCount();
        if (atomCount > 0) {
          nsTHashtable<nsPtrHashKey<nsIAtom>> otherClassesTable;
          if (otherClasses) {
            int32_t otherClassesCount = otherClasses->GetAtomCount();
            for (int32_t i = 0; i < otherClassesCount; ++i) {
              otherClassesTable.PutEntry(otherClasses->AtomAt(i));
            }
          }
          for (int32_t i = 0; i < atomCount; ++i) {
            nsIAtom* curClass = elementClasses->AtomAt(i);
            if (!otherClassesTable.Contains(curClass)) {
              AtomSelectorEntry *entry =
                static_cast<AtomSelectorEntry*>
                           (PL_DHashTableSearch(&cascade->mClassSelectors,
                                                curClass));
              if (entry) {
                EnumerateSelectors(entry->mSelectors, &data);
              }
            }
          }
        }
      }

      EnumerateSelectors(cascade->mPossiblyNegatedClassSelectors, &data);
    }

    AtomSelectorEntry *entry =
      static_cast<AtomSelectorEntry*>
                 (PL_DHashTableSearch(&cascade->mAttributeSelectors,
                                      aData->mAttribute));
    if (entry) {
      EnumerateSelectors(entry->mSelectors, &data);
    }
  }

  return data.change;
}

/* virtual */ bool
nsCSSRuleProcessor::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  // We don't want to do anything if there aren't any sets of rules
  // cached yet, since we should not build the rule cascade too early
  // (e.g., before we know whether the quirk style sheet should be
  // enabled).  And if there's nothing cached, it doesn't matter if
  // anything changed.  But in the cases where it does matter, we've
  // cached a previous cache key to test against, instead of our current
  // rule cascades.  See bug 448281 and bug 1089417.
  MOZ_ASSERT(!(mRuleCascades && mPreviousCacheKey));
  RuleCascadeData *old = mRuleCascades;
  if (old) {
    RefreshRuleCascade(aPresContext);
    return (old != mRuleCascades);
  }

  if (mPreviousCacheKey) {
    // RefreshRuleCascade will get rid of mPreviousCacheKey anyway to
    // maintain the invariant that we can't have both an mRuleCascades
    // and an mPreviousCacheKey.  But we need to hold it a little
    // longer.
    UniquePtr<nsMediaQueryResultCacheKey> previousCacheKey(
      Move(mPreviousCacheKey));
    RefreshRuleCascade(aPresContext);

    // This test is a bit pessimistic since the cache key's operator==
    // just does list comparison rather than set comparison, but it
    // should catch all the cases we care about (i.e., where the cascade
    // order hasn't changed).  Other cases will do a restyle anyway, so
    // we shouldn't need to worry about posting a second.
    return !mRuleCascades || // all sheets gone, but we had sheets before
           mRuleCascades->mCacheKey != *previousCacheKey;
  }

  return false;
}

UniquePtr<nsMediaQueryResultCacheKey>
nsCSSRuleProcessor::CloneMQCacheKey()
{
  MOZ_ASSERT(!(mRuleCascades && mPreviousCacheKey));

  RuleCascadeData* c = mRuleCascades;
  if (!c) {
    // We might have an mPreviousCacheKey.  It already comes from a call
    // to CloneMQCacheKey, so don't bother checking
    // HasFeatureConditions().
    if (mPreviousCacheKey) {
      NS_ASSERTION(mPreviousCacheKey->HasFeatureConditions(),
                   "we shouldn't have a previous cache key unless it has "
                   "feature conditions");
      return MakeUnique<nsMediaQueryResultCacheKey>(*mPreviousCacheKey);
    }

    return UniquePtr<nsMediaQueryResultCacheKey>();
  }

  if (!c->mCacheKey.HasFeatureConditions()) {
    return UniquePtr<nsMediaQueryResultCacheKey>();
  }

  return MakeUnique<nsMediaQueryResultCacheKey>(c->mCacheKey);
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mSheets.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (RuleCascadeData* cascade = mRuleCascades; cascade;
       cascade = cascade->mNext) {
    n += cascade->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

// Append all the currently-active font face rules to aArray.  Return
// true for success and false for failure.
bool
nsCSSRuleProcessor::AppendFontFaceRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsFontFaceRuleContainer>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mFontFaceRules))
      return false;
  }
  
  return true;
}

nsCSSKeyframesRule*
nsCSSRuleProcessor::KeyframesRuleForName(nsPresContext* aPresContext,
                                         const nsString& aName)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    return cascade->mKeyframesRuleTable.Get(aName);
  }

  return nullptr;
}

nsCSSCounterStyleRule*
nsCSSRuleProcessor::CounterStyleRuleForName(nsPresContext* aPresContext,
                                            const nsAString& aName)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    return cascade->mCounterStyleRuleTable.Get(aName);
  }

  return nullptr;
}

// Append all the currently-active page rules to aArray.  Return
// true for success and false for failure.
bool
nsCSSRuleProcessor::AppendPageRules(
                              nsPresContext* aPresContext,
                              nsTArray<nsCSSPageRule*>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mPageRules)) {
      return false;
    }
  }
  
  return true;
}

bool
nsCSSRuleProcessor::AppendFontFeatureValuesRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mFontFeatureValuesRules))
      return false;
  }

  return true;
}

nsresult
nsCSSRuleProcessor::ClearRuleCascades()
{
  if (!mPreviousCacheKey) {
    mPreviousCacheKey = CloneMQCacheKey();
  }

  // No need to remove the rule processor from the RuleProcessorCache here,
  // since CSSStyleSheet::ClearRuleCascades will have called
  // RuleProcessorCache::RemoveSheet() passing itself, which will catch
  // this rule processor (and any others for different @-moz-document
  // cache key results).
  MOZ_ASSERT(!RuleProcessorCache::HasRuleProcessor(this));

#ifdef DEBUG
  // For shared rule processors, if we've already gathered document
  // rules, then they will now be out of date.  We don't actually need
  // them to be up-to-date (see the comment in RefreshRuleCascade), so
  // record their invalidity so we can assert if we try to use them.
  if (!mMustGatherDocumentRules) {
    mDocumentRulesAndCacheKeyValid = false;
  }
#endif

  // We rely on our caller (perhaps indirectly) to do something that
  // will rebuild style data and the user font set (either
  // nsIPresShell::ReconstructStyleData or
  // nsPresContext::RebuildAllStyleData).
  RuleCascadeData *data = mRuleCascades;
  mRuleCascades = nullptr;
  while (data) {
    RuleCascadeData *next = data->mNext;
    delete data;
    data = next;
  }
  return NS_OK;
}


// This function should return the set of states that this selector
// depends on; this is used to implement HasStateDependentStyle.  It
// does NOT recur down into things like :not and :-moz-any.
inline
EventStates ComputeSelectorStateDependence(nsCSSSelector& aSelector)
{
  EventStates states;
  for (nsPseudoClassList* pseudoClass = aSelector.mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    // Tree pseudo-elements overload mPseudoClassList for things that
    // aren't pseudo-classes.
    if (pseudoClass->mType >= nsCSSPseudoClasses::ePseudoClass_Count) {
      continue;
    }
    states |= sPseudoClassStateDependences[pseudoClass->mType];
  }
  return states;
}

static bool
AddSelector(RuleCascadeData* aCascade,
            // The part between combinators at the top level of the selector
            nsCSSSelector* aSelectorInTopLevel,
            // The part we should look through (might be in :not or :-moz-any())
            nsCSSSelector* aSelectorPart,
            // The right-most selector at the top level
            nsCSSSelector* aRightmostSelector)
{
  // It's worth noting that this loop over negations isn't quite
  // optimal for two reasons.  One, we could add something to one of
  // these lists twice, which means we'll check it twice, but I don't
  // think that's worth worrying about.   (We do the same for multiple
  // attribute selectors on the same attribute.)  Two, we don't really
  // need to check negations past the first in the current
  // implementation (and they're rare as well), but that might change
  // in the future if :not() is extended.
  for (nsCSSSelector* negation = aSelectorPart; negation;
       negation = negation->mNegations) {
    // Track both document states and attribute dependence in pseudo-classes.
    for (nsPseudoClassList* pseudoClass = negation->mPseudoClassList;
         pseudoClass; pseudoClass = pseudoClass->mNext) {
      switch (pseudoClass->mType) {
        case nsCSSPseudoClasses::ePseudoClass_mozLocaleDir: {
          aCascade->mSelectorDocumentStates |= NS_DOCUMENT_STATE_RTL_LOCALE;
          break;
        }
        case nsCSSPseudoClasses::ePseudoClass_mozWindowInactive: {
          aCascade->mSelectorDocumentStates |= NS_DOCUMENT_STATE_WINDOW_INACTIVE;
          break;
        }
        case nsCSSPseudoClasses::ePseudoClass_mozTableBorderNonzero: {
          nsTArray<SelectorPair> *array =
            aCascade->AttributeListFor(nsGkAtoms::border);
          if (!array) {
            return false;
          }
          array->AppendElement(SelectorPair(aSelectorInTopLevel,
                                            aRightmostSelector));
          break;
        }
        default: {
          break;
        }
      }
    }

    // Build mStateSelectors.
    EventStates dependentStates = ComputeSelectorStateDependence(*negation);
    if (!dependentStates.IsEmpty()) {
      aCascade->mStateSelectors.AppendElement(
        nsCSSRuleProcessor::StateSelector(dependentStates,
                                          aSelectorInTopLevel));
    }

    // Build mIDSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curID = negation->mIDList; curID;
           curID = curID->mNext) {
        AtomSelectorEntry *entry = static_cast<AtomSelectorEntry*>
          (PL_DHashTableAdd(&aCascade->mIdSelectors, curID->mAtom, fallible));
        if (entry) {
          entry->mSelectors.AppendElement(SelectorPair(aSelectorInTopLevel,
                                                       aRightmostSelector));
        }
      }
    } else if (negation->mIDList) {
      aCascade->mPossiblyNegatedIDSelectors.AppendElement(aSelectorInTopLevel);
    }

    // Build mClassSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curClass = negation->mClassList; curClass;
           curClass = curClass->mNext) {
        AtomSelectorEntry *entry = static_cast<AtomSelectorEntry*>
          (PL_DHashTableAdd(&aCascade->mClassSelectors, curClass->mAtom,
                            fallible));
        if (entry) {
          entry->mSelectors.AppendElement(SelectorPair(aSelectorInTopLevel,
                                                       aRightmostSelector));
        }
      }
    } else if (negation->mClassList) {
      aCascade->mPossiblyNegatedClassSelectors.AppendElement(aSelectorInTopLevel);
    }

    // Build mAttributeSelectors.
    for (nsAttrSelector *attr = negation->mAttrList; attr;
         attr = attr->mNext) {
      nsTArray<SelectorPair> *array =
        aCascade->AttributeListFor(attr->mCasedAttr);
      if (!array) {
        return false;
      }
      array->AppendElement(SelectorPair(aSelectorInTopLevel,
                                        aRightmostSelector));
      if (attr->mLowercaseAttr != attr->mCasedAttr) {
        array = aCascade->AttributeListFor(attr->mLowercaseAttr);
        if (!array) {
          return false;
        }
        array->AppendElement(SelectorPair(aSelectorInTopLevel,
                                          aRightmostSelector));
      }
    }

    // Recur through any :-moz-any selectors
    for (nsPseudoClassList* pseudoClass = negation->mPseudoClassList;
         pseudoClass; pseudoClass = pseudoClass->mNext) {
      if (pseudoClass->mType == nsCSSPseudoClasses::ePseudoClass_any) {
        for (nsCSSSelectorList *l = pseudoClass->u.mSelectors; l; l = l->mNext) {
          nsCSSSelector *s = l->mSelectors;
          if (!AddSelector(aCascade, aSelectorInTopLevel, s,
                           aRightmostSelector)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

static bool
AddRule(RuleSelectorPair* aRuleInfo, RuleCascadeData* aCascade)
{
  RuleCascadeData * const cascade = aCascade;

  // Build the rule hash.
  nsCSSPseudoElements::Type pseudoType = aRuleInfo->mSelector->PseudoType();
  if (MOZ_LIKELY(pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement)) {
    cascade->mRuleHash.AppendRule(*aRuleInfo);
  } else if (pseudoType < nsCSSPseudoElements::ePseudo_PseudoElementCount) {
    RuleHash*& ruleHash = cascade->mPseudoElementRuleHashes[pseudoType];
    if (!ruleHash) {
      ruleHash = new RuleHash(cascade->mQuirksMode);
      if (!ruleHash) {
        // Out of memory; give up
        return false;
      }
    }
    NS_ASSERTION(aRuleInfo->mSelector->mNext,
                 "Must have mNext; parser screwed up");
    NS_ASSERTION(aRuleInfo->mSelector->mNext->mOperator == ':',
                 "Unexpected mNext combinator");
    ruleHash->AppendRule(*aRuleInfo);
  } else if (pseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
    NS_ASSERTION(!aRuleInfo->mSelector->mCasedTag &&
                 !aRuleInfo->mSelector->mIDList &&
                 !aRuleInfo->mSelector->mClassList &&
                 !aRuleInfo->mSelector->mPseudoClassList &&
                 !aRuleInfo->mSelector->mAttrList &&
                 !aRuleInfo->mSelector->mNegations &&
                 !aRuleInfo->mSelector->mNext &&
                 aRuleInfo->mSelector->mNameSpace == kNameSpaceID_Unknown,
                 "Parser messed up with anon box selector");

    // Index doesn't matter here, since we'll just be walking these
    // rules in order; just pass 0.
    AppendRuleToTagTable(&cascade->mAnonBoxRules,
                         aRuleInfo->mSelector->mLowercaseTag,
                         RuleValue(*aRuleInfo, 0, aCascade->mQuirksMode));
  } else {
#ifdef MOZ_XUL
    NS_ASSERTION(pseudoType == nsCSSPseudoElements::ePseudo_XULTree,
                 "Unexpected pseudo type");
    // Index doesn't matter here, since we'll just be walking these
    // rules in order; just pass 0.
    AppendRuleToTagTable(&cascade->mXULTreeRules,
                         aRuleInfo->mSelector->mLowercaseTag,
                         RuleValue(*aRuleInfo, 0, aCascade->mQuirksMode));
#else
    NS_NOTREACHED("Unexpected pseudo type");
#endif
  }

  for (nsCSSSelector* selector = aRuleInfo->mSelector;
           selector; selector = selector->mNext) {
    if (selector->IsPseudoElement()) {
      nsCSSPseudoElements::Type pseudo = selector->PseudoType();
      if (pseudo >= nsCSSPseudoElements::ePseudo_PseudoElementCount ||
          !nsCSSPseudoElements::PseudoElementSupportsUserActionState(pseudo)) {
        NS_ASSERTION(!selector->mNegations, "Shouldn't have negations");
        // We do store selectors ending with pseudo-elements that allow :hover
        // and :active after them in the hashtables corresponding to that
        // selector's mNext (i.e. the thing that matches against the element),
        // but we want to make sure that selectors for any other kinds of
        // pseudo-elements don't end up in the hashtables.  In particular, tree
        // pseudos store strange things in mPseudoClassList that we don't want
        // to try to match elements against.
        continue;
      }
    }
    if (!AddSelector(cascade, selector, selector, aRuleInfo->mSelector)) {
      return false;
    }
  }

  return true;
}

struct PerWeightDataListItem : public RuleSelectorPair {
  PerWeightDataListItem(css::StyleRule* aRule, nsCSSSelector* aSelector)
    : RuleSelectorPair(aRule, aSelector)
    , mNext(nullptr)
  {}
  // No destructor; these are arena-allocated


  // Placement new to arena allocate the PerWeightDataListItem
  void *operator new(size_t aSize, PLArenaPool &aArena) CPP_THROW_NEW {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &aArena, aSize);
    return mem;
  }

  PerWeightDataListItem *mNext;
};

struct PerWeightData {
  PerWeightData()
    : mRuleSelectorPairs(nullptr)
    , mTail(&mRuleSelectorPairs)
  {}

  int32_t mWeight;
  PerWeightDataListItem *mRuleSelectorPairs;
  PerWeightDataListItem **mTail;
};

struct RuleByWeightEntry : public PLDHashEntryHdr {
  PerWeightData data; // mWeight is key, mRuleSelectorPairs are value
};

static PLDHashNumber
HashIntKey(PLDHashTable *table, const void *key)
{
  return PLDHashNumber(NS_PTR_TO_INT32(key));
}

static bool
MatchWeightEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                 const void *key)
{
  const RuleByWeightEntry *entry = (const RuleByWeightEntry *)hdr;
  return entry->data.mWeight == NS_PTR_TO_INT32(key);
}

static void
InitWeightEntry(PLDHashEntryHdr *hdr, const void *key)
{
  RuleByWeightEntry* entry = static_cast<RuleByWeightEntry*>(hdr);
  new (entry) RuleByWeightEntry();
}

static const PLDHashTableOps gRulesByWeightOps = {
    HashIntKey,
    MatchWeightEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    InitWeightEntry
};

struct CascadeEnumData {
  CascadeEnumData(nsPresContext* aPresContext,
                  nsTArray<nsFontFaceRuleContainer>& aFontFaceRules,
                  nsTArray<nsCSSKeyframesRule*>& aKeyframesRules,
                  nsTArray<nsCSSFontFeatureValuesRule*>& aFontFeatureValuesRules,
                  nsTArray<nsCSSPageRule*>& aPageRules,
                  nsTArray<nsCSSCounterStyleRule*>& aCounterStyleRules,
                  nsTArray<css::DocumentRule*>& aDocumentRules,
                  nsMediaQueryResultCacheKey& aKey,
                  nsDocumentRuleResultCacheKey& aDocumentKey,
                  uint8_t aSheetType,
                  bool aMustGatherDocumentRules)
    : mPresContext(aPresContext),
      mFontFaceRules(aFontFaceRules),
      mKeyframesRules(aKeyframesRules),
      mFontFeatureValuesRules(aFontFeatureValuesRules),
      mPageRules(aPageRules),
      mCounterStyleRules(aCounterStyleRules),
      mDocumentRules(aDocumentRules),
      mCacheKey(aKey),
      mDocumentCacheKey(aDocumentKey),
      mRulesByWeight(&gRulesByWeightOps, sizeof(RuleByWeightEntry), 32),
      mSheetType(aSheetType),
      mMustGatherDocumentRules(aMustGatherDocumentRules)
  {
    // Initialize our arena
    PL_INIT_ARENA_POOL(&mArena, "CascadeEnumDataArena",
                       NS_CASCADEENUMDATA_ARENA_BLOCK_SIZE);
  }

  ~CascadeEnumData()
  {
    PL_FinishArenaPool(&mArena);
  }

  nsPresContext* mPresContext;
  nsTArray<nsFontFaceRuleContainer>& mFontFaceRules;
  nsTArray<nsCSSKeyframesRule*>& mKeyframesRules;
  nsTArray<nsCSSFontFeatureValuesRule*>& mFontFeatureValuesRules;
  nsTArray<nsCSSPageRule*>& mPageRules;
  nsTArray<nsCSSCounterStyleRule*>& mCounterStyleRules;
  nsTArray<css::DocumentRule*>& mDocumentRules;
  nsMediaQueryResultCacheKey& mCacheKey;
  nsDocumentRuleResultCacheKey& mDocumentCacheKey;
  PLArenaPool mArena;
  // Hooray, a manual PLDHashTable since nsClassHashtable doesn't
  // provide a getter that gives me a *reference* to the value.
  PLDHashTable mRulesByWeight; // of PerWeightDataListItem linked lists
  uint8_t mSheetType;
  bool mMustGatherDocumentRules;
};

/**
 * Recursively traverses rules in order to:
 *  (1) add any @-moz-document rules into data->mDocumentRules.
 *  (2) record any @-moz-document rules whose conditions evaluate to true
 *      on data->mDocumentCacheKey.
 *
 * See also CascadeRuleEnumFunc below, which calls us via
 * EnumerateRulesForwards.  If modifying this function you may need to
 * update CascadeRuleEnumFunc too.
 */
static bool
GatherDocRuleEnumFunc(css::Rule* aRule, void* aData)
{
  CascadeEnumData* data = (CascadeEnumData*)aData;
  int32_t type = aRule->GetType();

  MOZ_ASSERT(data->mMustGatherDocumentRules,
             "should only call GatherDocRuleEnumFunc if "
             "mMustGatherDocumentRules is true");

  if (css::Rule::MEDIA_RULE == type ||
      css::Rule::SUPPORTS_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    if (!groupRule->EnumerateRulesForwards(GatherDocRuleEnumFunc, aData)) {
      return false;
    }
  }
  else if (css::Rule::DOCUMENT_RULE == type) {
    css::DocumentRule* docRule = static_cast<css::DocumentRule*>(aRule);
    if (!data->mDocumentRules.AppendElement(docRule)) {
      return false;
    }
    if (docRule->UseForPresentation(data->mPresContext)) {
      if (!data->mDocumentCacheKey.AddMatchingRule(docRule)) {
        return false;
      }
    }
    if (!docRule->EnumerateRulesForwards(GatherDocRuleEnumFunc, aData)) {
      return false;
    }
  }
  return true;
}

/*
 * This enumerates style rules in a sheet (and recursively into any
 * grouping rules) in order to:
 *  (1) add any style rules, in order, into data->mRulesByWeight (for
 *      the primary CSS cascade), where they are separated by weight
 *      but kept in order per-weight, and
 *  (2) add any @font-face rules, in order, into data->mFontFaceRules.
 *  (3) add any @keyframes rules, in order, into data->mKeyframesRules.
 *  (4) add any @font-feature-value rules, in order,
 *      into data->mFontFeatureValuesRules.
 *  (5) add any @page rules, in order, into data->mPageRules.
 *  (6) add any @counter-style rules, in order, into data->mCounterStyleRules.
 *  (7) add any @-moz-document rules into data->mDocumentRules.
 *  (8) record any @-moz-document rules whose conditions evaluate to true
 *      on data->mDocumentCacheKey.
 *
 * See also GatherDocRuleEnumFunc above, which we call to traverse into
 * @-moz-document rules even if their (or an ancestor's) condition
 * fails.  This means we might look at the result of some @-moz-document
 * rules that don't actually affect whether a RuleProcessorCache lookup
 * is a hit or a miss.  The presence of @-moz-document rules inside
 * @media etc. rules should be rare, and looking at all of them in the
 * sheets lets us avoid the complication of having different document
 * cache key results for different media.
 *
 * If modifying this function you may need to update
 * GatherDocRuleEnumFunc too.
 */
static bool
CascadeRuleEnumFunc(css::Rule* aRule, void* aData)
{
  CascadeEnumData* data = (CascadeEnumData*)aData;
  int32_t type = aRule->GetType();

  if (css::Rule::STYLE_RULE == type) {
    css::StyleRule* styleRule = static_cast<css::StyleRule*>(aRule);

    for (nsCSSSelectorList *sel = styleRule->Selector();
         sel; sel = sel->mNext) {
      int32_t weight = sel->mWeight;
      RuleByWeightEntry *entry = static_cast<RuleByWeightEntry*>(
        PL_DHashTableAdd(&data->mRulesByWeight, NS_INT32_TO_PTR(weight),
                         fallible));
      if (!entry)
        return false;
      entry->data.mWeight = weight;
      // entry->data.mRuleSelectorPairs should be linked in forward order;
      // entry->data.mTail is the slot to write to.
      PerWeightDataListItem *newItem =
        new (data->mArena) PerWeightDataListItem(styleRule, sel->mSelectors);
      if (newItem) {
        *(entry->data.mTail) = newItem;
        entry->data.mTail = &newItem->mNext;
      }
    }
  }
  else if (css::Rule::MEDIA_RULE == type ||
           css::Rule::SUPPORTS_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    const bool use =
      groupRule->UseForPresentation(data->mPresContext, data->mCacheKey);
    if (use || data->mMustGatherDocumentRules) {
      if (!groupRule->EnumerateRulesForwards(use ? CascadeRuleEnumFunc :
                                                   GatherDocRuleEnumFunc,
                                             aData)) {
        return false;
      }
    }
  }
  else if (css::Rule::DOCUMENT_RULE == type) {
    css::DocumentRule* docRule = static_cast<css::DocumentRule*>(aRule);
    if (data->mMustGatherDocumentRules) {
      if (!data->mDocumentRules.AppendElement(docRule)) {
        return false;
      }
    }
    const bool use = docRule->UseForPresentation(data->mPresContext);
    if (use && data->mMustGatherDocumentRules) {
      if (!data->mDocumentCacheKey.AddMatchingRule(docRule)) {
        return false;
      }
    }
    if (use || data->mMustGatherDocumentRules) {
      if (!docRule->EnumerateRulesForwards(use ? CascadeRuleEnumFunc
                                               : GatherDocRuleEnumFunc,
                                           aData)) {
        return false;
      }
    }
  }
  else if (css::Rule::FONT_FACE_RULE == type) {
    nsCSSFontFaceRule *fontFaceRule = static_cast<nsCSSFontFaceRule*>(aRule);
    nsFontFaceRuleContainer *ptr = data->mFontFaceRules.AppendElement();
    if (!ptr)
      return false;
    ptr->mRule = fontFaceRule;
    ptr->mSheetType = data->mSheetType;
  }
  else if (css::Rule::KEYFRAMES_RULE == type) {
    nsCSSKeyframesRule *keyframesRule =
      static_cast<nsCSSKeyframesRule*>(aRule);
    if (!data->mKeyframesRules.AppendElement(keyframesRule)) {
      return false;
    }
  }
  else if (css::Rule::FONT_FEATURE_VALUES_RULE == type) {
    nsCSSFontFeatureValuesRule *fontFeatureValuesRule =
      static_cast<nsCSSFontFeatureValuesRule*>(aRule);
    if (!data->mFontFeatureValuesRules.AppendElement(fontFeatureValuesRule)) {
      return false;
    }
  }
  else if (css::Rule::PAGE_RULE == type) {
    nsCSSPageRule* pageRule = static_cast<nsCSSPageRule*>(aRule);
    if (!data->mPageRules.AppendElement(pageRule)) {
      return false;
    }
  }
  else if (css::Rule::COUNTER_STYLE_RULE == type) {
    nsCSSCounterStyleRule* counterStyleRule =
      static_cast<nsCSSCounterStyleRule*>(aRule);
    if (!data->mCounterStyleRules.AppendElement(counterStyleRule)) {
      return false;
    }
  }
  return true;
}

/* static */ bool
nsCSSRuleProcessor::CascadeSheet(CSSStyleSheet* aSheet, CascadeEnumData* aData)
{
  if (aSheet->IsApplicable() &&
      aSheet->UseForPresentation(aData->mPresContext, aData->mCacheKey) &&
      aSheet->mInner) {
    CSSStyleSheet* child = aSheet->mInner->mFirstChild;
    while (child) {
      CascadeSheet(child, aData);
      child = child->mNext;
    }

    if (!aSheet->mInner->mOrderedRules.EnumerateForwards(CascadeRuleEnumFunc,
                                                         aData))
      return false;
  }
  return true;
}

static int CompareWeightData(const void* aArg1, const void* aArg2,
                             void* closure)
{
  const PerWeightData* arg1 = static_cast<const PerWeightData*>(aArg1);
  const PerWeightData* arg2 = static_cast<const PerWeightData*>(aArg2);
  return arg1->mWeight - arg2->mWeight; // put lower weight first
}

RuleCascadeData*
nsCSSRuleProcessor::GetRuleCascade(nsPresContext* aPresContext)
{
  // FIXME:  Make this infallible!

  // If anything changes about the presentation context, we will be
  // notified.  Otherwise, our cache is valid if mLastPresContext
  // matches aPresContext.  (The only rule processors used for multiple
  // pres contexts are for XBL.  These rule processors are probably less
  // likely to have @media rules, and thus the cache is pretty likely to
  // hit instantly even when we're switching between pres contexts.)

  if (!mRuleCascades || aPresContext != mLastPresContext) {
    RefreshRuleCascade(aPresContext);
  }
  mLastPresContext = aPresContext;

  return mRuleCascades;
}

void
nsCSSRuleProcessor::RefreshRuleCascade(nsPresContext* aPresContext)
{
  // Having RuleCascadeData objects be per-medium (over all variation
  // caused by media queries, handled through mCacheKey) works for now
  // since nsCSSRuleProcessor objects are per-document.  (For a given
  // set of stylesheets they can vary based on medium (@media) or
  // document (@-moz-document).)

  for (RuleCascadeData **cascadep = &mRuleCascades, *cascade;
       (cascade = *cascadep); cascadep = &cascade->mNext) {
    if (cascade->mCacheKey.Matches(aPresContext)) {
      // Ensure that the current one is always mRuleCascades.
      *cascadep = cascade->mNext;
      cascade->mNext = mRuleCascades;
      mRuleCascades = cascade;

      return;
    }
  }

  // We're going to make a new rule cascade; this means that we should
  // now stop using the previous cache key that we're holding on to from
  // the last time we had rule cascades.
  mPreviousCacheKey = nullptr;

  if (mSheets.Length() != 0) {
    nsAutoPtr<RuleCascadeData> newCascade(
      new RuleCascadeData(aPresContext->Medium(),
                          eCompatibility_NavQuirks == aPresContext->CompatibilityMode()));
    if (newCascade) {
      CascadeEnumData data(aPresContext, newCascade->mFontFaceRules,
                           newCascade->mKeyframesRules,
                           newCascade->mFontFeatureValuesRules,
                           newCascade->mPageRules,
                           newCascade->mCounterStyleRules,
                           mDocumentRules,
                           newCascade->mCacheKey,
                           mDocumentCacheKey,
                           mSheetType,
                           mMustGatherDocumentRules);

      for (uint32_t i = 0; i < mSheets.Length(); ++i) {
        if (!CascadeSheet(mSheets.ElementAt(i), &data))
          return; /* out of memory */
      }

      // Sort the hash table of per-weight linked lists by weight.
      uint32_t weightCount = data.mRulesByWeight.EntryCount();
      nsAutoArrayPtr<PerWeightData> weightArray(new PerWeightData[weightCount]);
      int32_t j = 0;
      for (auto iter = data.mRulesByWeight.Iter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<const RuleByWeightEntry*>(iter.Get());
        weightArray[j++] = entry->data;
      }
      NS_QuickSort(weightArray, weightCount, sizeof(PerWeightData),
                   CompareWeightData, nullptr);

      // Put things into the rule hash.
      // The primary sort is by weight...
      for (uint32_t i = 0; i < weightCount; ++i) {
        // and the secondary sort is by order.  mRuleSelectorPairs is already in
        // the right order..
        for (PerWeightDataListItem *cur = weightArray[i].mRuleSelectorPairs;
             cur;
             cur = cur->mNext) {
          if (!AddRule(cur, newCascade))
            return; /* out of memory */
        }
      }

      // Build mKeyframesRuleTable.
      for (nsTArray<nsCSSKeyframesRule*>::size_type i = 0,
             iEnd = newCascade->mKeyframesRules.Length(); i < iEnd; ++i) {
        nsCSSKeyframesRule* rule = newCascade->mKeyframesRules[i];
        newCascade->mKeyframesRuleTable.Put(rule->GetName(), rule);
      }

      // Build mCounterStyleRuleTable
      for (nsTArray<nsCSSCounterStyleRule*>::size_type i = 0,
           iEnd = newCascade->mCounterStyleRules.Length(); i < iEnd; ++i) {
        nsCSSCounterStyleRule* rule = newCascade->mCounterStyleRules[i];
        newCascade->mCounterStyleRuleTable.Put(rule->GetName(), rule);
      }

      // mMustGatherDocumentRules controls whether we build mDocumentRules
      // and mDocumentCacheKey so that they can be used as keys by the
      // RuleProcessorCache, as obtained by TakeDocumentRulesAndCacheKey
      // later.  We set it to false just below so that we only do this
      // the first time we build a RuleProcessorCache for a shared rule
      // processor.
      //
      // An up-to-date mDocumentCacheKey is only needed if we
      // are still in the RuleProcessorCache (as we store a copy of the
      // cache key in the RuleProcessorCache), and an up-to-date
      // mDocumentRules is only needed at the time TakeDocumentRulesAndCacheKey
      // is called, which is immediately after the rule processor is created
      // (by nsStyleSet).
      //
      // Note that when nsCSSRuleProcessor::ClearRuleCascades is called,
      // by CSSStyleSheet::ClearRuleCascades, we will have called
      // RuleProcessorCache::RemoveSheet, which will remove the rule
      // processor from the cache.  (This is because the list of document
      // rules now may not match the one used as they key in the
      // RuleProcessorCache.)
      //
      // Thus, as we'll no longer be in the RuleProcessorCache, and we won't
      // have TakeDocumentRulesAndCacheKey called on us, we don't need to ensure
      // mDocumentCacheKey and mDocumentRules are up-to-date after the
      // first time GetRuleCascade is called.
      if (mMustGatherDocumentRules) {
        mDocumentRules.Sort();
        mDocumentCacheKey.Finalize();
        mMustGatherDocumentRules = false;
#ifdef DEBUG
        mDocumentRulesAndCacheKeyValid = true;
#endif
      }

      // Ensure that the current one is always mRuleCascades.
      newCascade->mNext = mRuleCascades;
      mRuleCascades = newCascade.forget();
    }
  }
  return;
}

/* static */ bool
nsCSSRuleProcessor::SelectorListMatches(Element* aElement,
                                        TreeMatchContext& aTreeMatchContext,
                                        nsCSSSelectorList* aSelectorList)
{
  MOZ_ASSERT(!aTreeMatchContext.mForScopedStyle,
             "mCurrentStyleScope will need to be saved and restored after the "
             "SelectorMatchesTree call");

  while (aSelectorList) {
    nsCSSSelector* sel = aSelectorList->mSelectors;
    NS_ASSERTION(sel, "Should have *some* selectors");
    NS_ASSERTION(!sel->IsPseudoElement(), "Shouldn't have been called");
    NodeMatchContext nodeContext(EventStates(), false);
    if (SelectorMatches(aElement, sel, nodeContext, aTreeMatchContext,
                        SelectorMatchesFlags::NONE)) {
      nsCSSSelector* next = sel->mNext;
      if (!next ||
          SelectorMatchesTree(aElement, next, aTreeMatchContext,
                              SelectorMatchesTreeFlags(0))) {
        return true;
      }
    }

    aSelectorList = aSelectorList->mNext;
  }

  return false;
}

void
nsCSSRuleProcessor::TakeDocumentRulesAndCacheKey(
    nsPresContext* aPresContext,
    nsTArray<css::DocumentRule*>& aDocumentRules,
    nsDocumentRuleResultCacheKey& aCacheKey)
{
  MOZ_ASSERT(mIsShared);

  GetRuleCascade(aPresContext);
  MOZ_ASSERT(mDocumentRulesAndCacheKeyValid);

  aDocumentRules.Clear();
  aDocumentRules.SwapElements(mDocumentRules);
  aCacheKey.Swap(mDocumentCacheKey);

#ifdef DEBUG
  mDocumentRulesAndCacheKeyValid = false;
#endif
}

void
nsCSSRuleProcessor::AddStyleSetRef()
{
  MOZ_ASSERT(mIsShared);
  if (++mStyleSetRefCnt == 1) {
    RuleProcessorCache::StopTracking(this);
  }
}

void
nsCSSRuleProcessor::ReleaseStyleSetRef()
{
  MOZ_ASSERT(mIsShared);
  MOZ_ASSERT(mStyleSetRefCnt > 0);
  if (--mStyleSetRefCnt == 0 && mInRuleProcessorCache) {
    RuleProcessorCache::StartTracking(this);
  }
}

// TreeMatchContext and AncestorFilter out of line methods
void
TreeMatchContext::InitAncestors(Element *aElement)
{
  MOZ_ASSERT(!mAncestorFilter.mFilter);
  MOZ_ASSERT(mAncestorFilter.mHashes.IsEmpty());
  MOZ_ASSERT(mStyleScopes.IsEmpty());

  mAncestorFilter.mFilter = new AncestorFilter::Filter();

  if (MOZ_LIKELY(aElement)) {
    MOZ_ASSERT(aElement->GetCurrentDoc() ||
               aElement->HasFlag(NODE_IS_IN_SHADOW_TREE),
               "aElement must be in the document or in shadow tree "
               "for the assumption that GetParentNode() is non-null "
               "on all element ancestors of aElement to be true");
    // Collect up the ancestors
    nsAutoTArray<Element*, 50> ancestors;
    Element* cur = aElement;
    do {
      ancestors.AppendElement(cur);
      cur = cur->GetParentElementCrossingShadowRoot();
    } while (cur);

    // Now push them in reverse order.
    for (uint32_t i = ancestors.Length(); i-- != 0; ) {
      mAncestorFilter.PushAncestor(ancestors[i]);
      PushStyleScope(ancestors[i]);
    }
  }
}

void
TreeMatchContext::InitStyleScopes(Element* aElement)
{
  MOZ_ASSERT(mStyleScopes.IsEmpty());

  if (MOZ_LIKELY(aElement)) {
    // Collect up the ancestors
    nsAutoTArray<Element*, 50> ancestors;
    Element* cur = aElement;
    do {
      ancestors.AppendElement(cur);
      cur = cur->GetParentElementCrossingShadowRoot();
    } while (cur);

    // Now push them in reverse order.
    for (uint32_t i = ancestors.Length(); i-- != 0; ) {
      PushStyleScope(ancestors[i]);
    }
  }
}

void
AncestorFilter::PushAncestor(Element *aElement)
{
  MOZ_ASSERT(mFilter);

  uint32_t oldLength = mHashes.Length();

  mPopTargets.AppendElement(oldLength);
#ifdef DEBUG
  mElements.AppendElement(aElement);
#endif
  mHashes.AppendElement(aElement->NodeInfo()->NameAtom()->hash());
  nsIAtom *id = aElement->GetID();
  if (id) {
    mHashes.AppendElement(id->hash());
  }
  const nsAttrValue *classes = aElement->GetClasses();
  if (classes) {
    uint32_t classCount = classes->GetAtomCount();
    for (uint32_t i = 0; i < classCount; ++i) {
      mHashes.AppendElement(classes->AtomAt(i)->hash());
    }
  }

  uint32_t newLength = mHashes.Length();
  for (uint32_t i = oldLength; i < newLength; ++i) {
    mFilter->add(mHashes[i]);
  }
}

void
AncestorFilter::PopAncestor()
{
  MOZ_ASSERT(!mPopTargets.IsEmpty());
  MOZ_ASSERT(mPopTargets.Length() == mElements.Length());

  uint32_t popTargetLength = mPopTargets.Length();
  uint32_t newLength = mPopTargets[popTargetLength-1];

  mPopTargets.TruncateLength(popTargetLength-1);
#ifdef DEBUG
  mElements.TruncateLength(popTargetLength-1);
#endif

  uint32_t oldLength = mHashes.Length();
  for (uint32_t i = newLength; i < oldLength; ++i) {
    mFilter->remove(mHashes[i]);
  }
  mHashes.TruncateLength(newLength);
}

#ifdef DEBUG
void
AncestorFilter::AssertHasAllAncestors(Element *aElement) const
{
  Element* cur = aElement->GetParentElementCrossingShadowRoot();
  while (cur) {
    MOZ_ASSERT(mElements.Contains(cur));
    cur = cur->GetParentElementCrossingShadowRoot();
  }
}

void
TreeMatchContext::AssertHasAllStyleScopes(Element* aElement) const
{
  Element* cur = aElement->GetParentElementCrossingShadowRoot();
  while (cur) {
    if (cur->IsScopedStyleRoot()) {
      MOZ_ASSERT(mStyleScopes.Contains(cur));
    }
    cur = cur->GetParentElementCrossingShadowRoot();
  }
}
#endif
