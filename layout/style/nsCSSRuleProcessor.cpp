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
#include "nsCRT.h"
#include "nsIAtom.h"
#include "pldhash.h"
#include "nsICSSPseudoComparator.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/GroupRule.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsEventStateManager.h"
#include "nsGkAtoms.h"
#include "nsString.h"
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
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsIMediaList.h"
#include "nsCSSRules.h"
#include "nsIPrincipal.h"
#include "nsStyleSet.h"
#include "prlog.h"
#include "nsIObserverService.h"
#include "nsNetCID.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Element.h"
#include "nsNthIndexCache.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Likely.h"
#include "mozilla/Util.h"

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
  ((ch) == PRUnichar(' ') || (ch) == PRUnichar('>'))

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
  // logic in SizeOfRuleHashTableEntry().
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
  PLDHashTableOps ops;
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
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

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
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

  return match_atom == entry_atom;
}

static bool
RuleHash_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                   const void *key)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  new (entry) RuleHashTableEntry();
  return true;
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

static bool
RuleHash_TagTable_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                            const void *key)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  new (entry) RuleHashTagTableEntry();
  entry->mTag = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  return true;
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
  return entry->mRules[0].mSelector->mClassList->mAtom;
}

static nsIAtom*
RuleHash_IdTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    static_cast<const RuleHashTableEntry*>(hdr);
  return entry->mRules[0].mSelector->mIDList->mAtom;
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

  return NS_PTR_TO_INT32(key) ==
         entry->mRules[0].mSelector->mNameSpace;
}

static const PLDHashTableOps RuleHash_TagTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RuleHash_TagTable_MatchEntry,
  RuleHash_TagTable_MoveEntry,
  RuleHash_TagTable_ClearEntry,
  PL_DHashFinalizeStub,
  RuleHash_TagTable_InitEntry
};

// Case-sensitive ops.
static const RuleHashTableOps RuleHash_ClassTable_CSOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  PL_DHashFinalizeStub,
  RuleHash_InitEntry
  },
  RuleHash_ClassTable_GetKey
};

// Case-insensitive ops.
static const RuleHashTableOps RuleHash_ClassTable_CIOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  PL_DHashFinalizeStub,
  RuleHash_InitEntry
  },
  RuleHash_ClassTable_GetKey
};

// Case-sensitive ops.
static const RuleHashTableOps RuleHash_IdTable_CSOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  PL_DHashFinalizeStub,
  RuleHash_InitEntry
  },
  RuleHash_IdTable_GetKey
};

// Case-insensitive ops.
static const RuleHashTableOps RuleHash_IdTable_CIOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  PL_DHashFinalizeStub,
  RuleHash_InitEntry
  },
  RuleHash_IdTable_GetKey
};

static const PLDHashTableOps RuleHash_NameSpaceTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_NameSpaceTable_HashKey,
  RuleHash_NameSpaceTable_MatchEntry,
  RuleHash_MoveEntry,
  RuleHash_ClearEntry,
  PL_DHashFinalizeStub,
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
  RuleHash(bool aQuirksMode);
  ~RuleHash();
  void AppendRule(const RuleSelectorPair &aRuleInfo);
  void EnumerateAllRules(Element* aElement, ElementDependentRuleProcessorData* aData,
                         NodeMatchContext& aNodeMatchContext);

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;
  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

protected:
  typedef nsTArray<RuleValue> RuleValueList;
  void AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                         const RuleSelectorPair& aRuleInfo);
  void AppendUniversalRule(const RuleSelectorPair& aRuleInfo);

  int32_t     mRuleCount;
  // The hashtables are lazily initialized; we use a null .ops to
  // indicate that they need initialization.
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

  mTagTable.ops = nullptr;
  mIdTable.ops = nullptr;
  mClassTable.ops = nullptr;
  mNameSpaceTable.ops = nullptr;
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
        nsCOMPtr<nsIStyleSheet> sheet;
        value->mRule->GetStyleSheet(*getter_AddRefs(sheet));
        nsRefPtr<nsCSSStyleSheet> cssSheet = do_QueryObject(sheet);
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
  // delete arena for strings and small objects
  if (mIdTable.ops) {
    PL_DHashTableFinish(&mIdTable);
  }
  if (mClassTable.ops) {
    PL_DHashTableFinish(&mClassTable);
  }
  if (mTagTable.ops) {
    PL_DHashTableFinish(&mTagTable);
  }
  if (mNameSpaceTable.ops) {
    PL_DHashTableFinish(&mNameSpaceTable);
  }
}

void RuleHash::AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                                 const RuleSelectorPair& aRuleInfo)
{
  // Get a new or existing entry.
  RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                         (PL_DHashTableOperate(aTable, aKey, PL_DHASH_ADD));
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
    (PL_DHashTableOperate(aTable, aKey, PL_DHASH_ADD));
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
  if (nullptr != selector->mIDList) {
    if (!mIdTable.ops) {
      PL_DHashTableInit(&mIdTable,
                        mQuirksMode ? &RuleHash_IdTable_CIOps.ops
                                    : &RuleHash_IdTable_CSOps.ops,
                        nullptr, sizeof(RuleHashTableEntry), 16);
    }
    AppendRuleToTable(&mIdTable, selector->mIDList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mIdSelectors);
  }
  else if (nullptr != selector->mClassList) {
    if (!mClassTable.ops) {
      PL_DHashTableInit(&mClassTable,
                        mQuirksMode ? &RuleHash_ClassTable_CIOps.ops
                                    : &RuleHash_ClassTable_CSOps.ops,
                        nullptr, sizeof(RuleHashTableEntry), 16);
    }
    AppendRuleToTable(&mClassTable, selector->mClassList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mClassSelectors);
  }
  else if (selector->mLowercaseTag) {
    RuleValue ruleValue(aRuleInfo, mRuleCount++, mQuirksMode);
    if (!mTagTable.ops) {
      PL_DHashTableInit(&mTagTable, &RuleHash_TagTable_Ops, nullptr,
                        sizeof(RuleHashTagTableEntry), 16);
    }
    AppendRuleToTagTable(&mTagTable, selector->mLowercaseTag, ruleValue);
    RULE_HASH_STAT_INCREMENT(mTagSelectors);
    if (selector->mCasedTag &&
        selector->mCasedTag != selector->mLowercaseTag) {
      AppendRuleToTagTable(&mTagTable, selector->mCasedTag, ruleValue);
      RULE_HASH_STAT_INCREMENT(mTagSelectors);
    }
  }
  else if (kNameSpaceID_Unknown != selector->mNameSpace) {
    if (!mNameSpaceTable.ops) {
      PL_DHashTableInit(&mNameSpaceTable, &RuleHash_NameSpaceTable_Ops, nullptr,
                        sizeof(RuleHashTableEntry), 16);
    }
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
  nsIAtom* tag = aElement->Tag();
  nsIAtom* id = aElement->GetID();
  const nsAttrValue* classList = aElement->GetClasses();

  NS_ABORT_IF_FALSE(tag, "How could we not have a tag?");

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
  if (kNameSpaceID_Unknown != nameSpace && mNameSpaceTable.ops) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mNameSpaceTable, NS_INT32_TO_PTR(nameSpace),
                             PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementNameSpaceCalls);
    }
  }
  if (mTagTable.ops) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mTagTable, tag, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementTagCalls);
    }
  }
  if (id && mIdTable.ops) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mIdTable, id, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementIdCalls);
    }
  }
  if (mClassTable.ops) {
    for (int32_t index = 0; index < classCount; ++index) {
      RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                             (PL_DHashTableOperate(&mClassTable, classList->AtomAt(index),
                             PL_DHASH_LOOKUP));
      if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
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
SizeOfRuleHashTableEntry(PLDHashEntryHdr* aHdr, nsMallocSizeOfFun aMallocSizeOf, void *)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(aHdr);
  return entry->mRules.SizeOfExcludingThis(aMallocSizeOf);
}

size_t
RuleHash::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = 0;

  if (mIdTable.ops) {
    n += PL_DHashTableSizeOfExcludingThis(&mIdTable,
                                          SizeOfRuleHashTableEntry,
                                          aMallocSizeOf);
  }

  if (mClassTable.ops) {
    n += PL_DHashTableSizeOfExcludingThis(&mClassTable,
                                          SizeOfRuleHashTableEntry,
                                          aMallocSizeOf);
  }

  if (mTagTable.ops) {
    n += PL_DHashTableSizeOfExcludingThis(&mTagTable,
                                          SizeOfRuleHashTableEntry,
                                          aMallocSizeOf);
  }

  if (mNameSpaceTable.ops) {
    n += PL_DHashTableSizeOfExcludingThis(&mNameSpaceTable,
                                          SizeOfRuleHashTableEntry,
                                          aMallocSizeOf);
  }

  n += mUniversalRules.SizeOfExcludingThis(aMallocSizeOf);

  return n;
}

size_t
RuleHash::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

//--------------------------------

// A hash table mapping atoms to lists of selectors
struct AtomSelectorEntry : public PLDHashEntryHdr {
  nsIAtom *mAtom;
  // Auto length 2, because a decent fraction of these arrays ends up
  // with 2 elements, and each entry is cheap.
  nsAutoTArray<nsCSSSelector*, 2> mSelectors;
};

static void
AtomSelector_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  (static_cast<AtomSelectorEntry*>(hdr))->~AtomSelectorEntry();
}

static bool
AtomSelector_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                       const void *key)
{
  AtomSelectorEntry *entry = static_cast<AtomSelectorEntry*>(hdr);
  new (entry) AtomSelectorEntry();
  entry->mAtom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  return true;
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
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  PL_DHashFinalizeStub,
  AtomSelector_InitEntry
};

// Case-insensitive ops.
static const RuleHashTableOps AtomSelector_CIOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  AtomSelector_MoveEntry,
  AtomSelector_ClearEntry,
  PL_DHashFinalizeStub,
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
      mCacheKey(aMedium),
      mNext(nullptr),
      mQuirksMode(aQuirksMode)
  {
    // mAttributeSelectors is matching on the attribute _name_, not the value,
    // and we case-fold names at parse-time, so this is a case-sensitive match.
    PL_DHashTableInit(&mAttributeSelectors, &AtomSelector_CSOps, nullptr,
                      sizeof(AtomSelectorEntry), 16);
    PL_DHashTableInit(&mAnonBoxRules, &RuleHash_TagTable_Ops, nullptr,
                      sizeof(RuleHashTagTableEntry), 16);
    PL_DHashTableInit(&mIdSelectors,
                      aQuirksMode ? &AtomSelector_CIOps.ops :
                                    &AtomSelector_CSOps,
                      nullptr, sizeof(AtomSelectorEntry), 16);
    PL_DHashTableInit(&mClassSelectors,
                      aQuirksMode ? &AtomSelector_CIOps.ops :
                                    &AtomSelector_CSOps,
                      nullptr, sizeof(AtomSelectorEntry), 16);
    memset(mPseudoElementRuleHashes, 0, sizeof(mPseudoElementRuleHashes));
#ifdef MOZ_XUL
    PL_DHashTableInit(&mXULTreeRules, &RuleHash_TagTable_Ops, nullptr,
                      sizeof(RuleHashTagTableEntry), 16);
#endif
  }

  ~RuleCascadeData()
  {
    PL_DHashTableFinish(&mAttributeSelectors);
    PL_DHashTableFinish(&mAnonBoxRules);
    PL_DHashTableFinish(&mIdSelectors);
    PL_DHashTableFinish(&mClassSelectors);
#ifdef MOZ_XUL
    PL_DHashTableFinish(&mXULTreeRules);
#endif
    for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
      delete mPseudoElementRuleHashes[i];
    }
  }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  RuleHash                 mRuleHash;
  RuleHash*
    mPseudoElementRuleHashes[nsCSSPseudoElements::ePseudo_PseudoElementCount];
  nsTArray<nsCSSRuleProcessor::StateSelector>  mStateSelectors;
  nsEventStates            mSelectorDocumentStates;
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

  // Looks up or creates the appropriate list in |mAttributeSelectors|.
  // Returns null only on allocation failure.
  nsTArray<nsCSSSelector*>* AttributeListFor(nsIAtom* aAttribute);

  nsMediaQueryResultCacheKey mCacheKey;
  RuleCascadeData*  mNext; // for a different medium

  const bool mQuirksMode;
};

static size_t
SizeOfSelectorsEntry(PLDHashEntryHdr* aHdr, nsMallocSizeOfFun aMallocSizeOf, void *)
{
  AtomSelectorEntry* entry = static_cast<AtomSelectorEntry*>(aHdr);
  return entry->mSelectors.SizeOfExcludingThis(aMallocSizeOf);
}

size_t
RuleCascadeData::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mRuleHash.SizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < ArrayLength(mPseudoElementRuleHashes); ++i) {
    if (mPseudoElementRuleHashes[i])
      n += mPseudoElementRuleHashes[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += mStateSelectors.SizeOfExcludingThis(aMallocSizeOf);

  n += PL_DHashTableSizeOfExcludingThis(&mIdSelectors,
                                        SizeOfSelectorsEntry, aMallocSizeOf);
  n += PL_DHashTableSizeOfExcludingThis(&mClassSelectors,
                                        SizeOfSelectorsEntry, aMallocSizeOf);

  n += mPossiblyNegatedClassSelectors.SizeOfExcludingThis(aMallocSizeOf);
  n += mPossiblyNegatedIDSelectors.SizeOfExcludingThis(aMallocSizeOf);

  n += PL_DHashTableSizeOfExcludingThis(&mAttributeSelectors,
                                        SizeOfSelectorsEntry, aMallocSizeOf);
  n += PL_DHashTableSizeOfExcludingThis(&mAnonBoxRules,
                                        SizeOfRuleHashTableEntry, aMallocSizeOf);
#ifdef MOZ_XUL
  n += PL_DHashTableSizeOfExcludingThis(&mXULTreeRules,
                                        SizeOfRuleHashTableEntry, aMallocSizeOf);
#endif

  n += mFontFaceRules.SizeOfExcludingThis(aMallocSizeOf);
  n += mKeyframesRules.SizeOfExcludingThis(aMallocSizeOf);
  n += mFontFeatureValuesRules.SizeOfExcludingThis(aMallocSizeOf);
  n += mPageRules.SizeOfExcludingThis(aMallocSizeOf);

  return n;
}

nsTArray<nsCSSSelector*>*
RuleCascadeData::AttributeListFor(nsIAtom* aAttribute)
{
  AtomSelectorEntry *entry =
    static_cast<AtomSelectorEntry*>
               (PL_DHashTableOperate(&mAttributeSelectors, aAttribute,
                                     PL_DHASH_ADD));
  if (!entry)
    return nullptr;
  return &entry->mSelectors;
}

// -------------------------------
// CSS Style rule processor implementation
//

nsCSSRuleProcessor::nsCSSRuleProcessor(const sheet_array_type& aSheets,
                                       uint8_t aSheetType,
                                       Element* aScopeElement)
  : mSheets(aSheets)
  , mRuleCascades(nullptr)
  , mLastPresContext(nullptr)
  , mScopeElement(aScopeElement)
  , mSheetType(aSheetType)
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
  for (sheet_array_type::size_type i = mSheets.Length(); i-- != 0; ) {
    mSheets[i]->DropRuleProcessor(this);
  }
  mSheets.Clear();
  ClearRuleCascades();
}

NS_IMPL_ISUPPORTS1(nsCSSRuleProcessor, nsIStyleRuleProcessor)

/* static */ nsresult
nsCSSRuleProcessor::Startup()
{
  Preferences::AddBoolVarCache(&gSupportVisitedPseudo, VISITED_PSEUDO_PREF,
                               true);

  return NS_OK;
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

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::touch_enabled);
  }
 
  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MaemoClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::maemo_classic);
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
nsEventStates
nsCSSRuleProcessor::GetContentState(Element* aElement, const TreeMatchContext& aTreeMatchContext)
{
  nsEventStates state = aElement->StyleState();

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
  nsEventStates state = aElement->StyleState();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED);
}

/* static */
nsEventStates
nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                     Element* aElement,
                     const TreeMatchContext& aTreeMatchContext,
                     nsRuleWalker::VisitedHandlingType aVisitedHandling,
                     bool aIsRelevantLink)
{
  nsEventStates contentState = GetContentState(aElement, aTreeMatchContext);
  if (contentState.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)) {
    NS_ABORT_IF_FALSE(IsLink(aElement), "IsLink() should match state");
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
  const nsEventStates mStateMask;

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

  NodeMatchContext(nsEventStates aStateMask, bool aIsRelevantLink)
    : mStateMask(aStateMask)
    , mIsRelevantLink(aIsRelevantLink)
  {
  }
};

static bool ValueIncludes(const nsSubstring& aValueList,
                            const nsSubstring& aValue,
                            const nsStringComparator& aComparator)
{
  const PRUnichar *p = aValueList.BeginReading(),
              *p_end = aValueList.EndReading();

  while (p < p_end) {
    // skip leading space
    while (p != p_end && nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const PRUnichar *val_start = p;

    // look for space or end
    while (p != p_end && !nsContentUtils::IsHTMLWhitespace(*p))
      ++p;

    const PRUnichar *val_end = p;

    if (val_start < val_end &&
        aValue.Equals(Substring(val_start, val_end), aComparator))
      return true;

    ++p; // we know the next character is not whitespace
  }
  return false;
}

// Return whether we should apply a "global" (i.e., universal-tag)
// selector for event states in quirks mode.  Note that
// |IsLink()| is checked separately by the caller, so we return
// false for |nsGkAtoms::a|, which here means a named anchor.
inline bool IsQuirkEventSensitive(nsIAtom *aContentTag)
{
  return bool ((nsGkAtoms::button == aContentTag) ||
                 (nsGkAtoms::img == aContentTag)    ||
                 (nsGkAtoms::input == aContentTag)  ||
                 (nsGkAtoms::label == aContentTag)  ||
                 (nsGkAtoms::select == aContentTag) ||
                 (nsGkAtoms::textarea == aContentTag));
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
static const nsEventStates sPseudoClassStateDependences[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _pref)  \
  nsEventStates(),
#define CSS_STATE_DEPENDENT_PSEUDO_CLASS(_name, _value, _pref, _states)  \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_DEPENDENT_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  nsEventStates(),
  nsEventStates()
};

static const nsEventStates sPseudoClassStates[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _pref)  \
  nsEventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _pref, _states) \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  nsEventStates(),
  nsEventStates()
};
MOZ_STATIC_ASSERT(NS_ARRAY_LENGTH(sPseudoClassStates) ==
                  nsCSSPseudoClasses::ePseudoClass_NotPseudoClass + 1,
                  "ePseudoClass_NotPseudoClass is no longer at the end of"
                  "sPseudoClassStates");

// |aDependence| has two functions:
//  * when non-null, it indicates that we're processing a negation,
//    which is done only when SelectorMatches calls itself recursively
//  * what it points to should be set to true whenever a test is skipped
//    because of aNodeMatchContent.mStateMask
static bool SelectorMatches(Element* aElement,
                              nsCSSSelector* aSelector,
                              NodeMatchContext& aNodeMatchContext,
                              TreeMatchContext& aTreeMatchContext,
                              bool* const aDependence = nullptr)

{
  NS_PRECONDITION(!aSelector->IsPseudoElement(),
                  "Pseudo-element snuck into SelectorMatches?");
  NS_ABORT_IF_FALSE(aTreeMatchContext.mForStyling ||
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
      (aTreeMatchContext.mIsHTMLDocument && aElement->IsHTML()) ?
        aSelector->mLowercaseTag : aSelector->mCasedTag;
    if (selectorTag != aElement->Tag()) {
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
    nsEventStates statesToCheck = sPseudoClassStates[pseudoClass->mType];
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
                     child->Tag()->Equals(nsDependentString(pseudoClass->u.mString)))));
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
              int32_t end = language.FindChar(PRUnichar(','), begin);
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

          return false;
        }
        break;

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
            NS_ABORT_IF_FALSE(!s->mNext && !s->IsPseudoElement(),
                              "parser failed");
            if (SelectorMatches(aElement, s, aNodeMatchContext,
                                aTreeMatchContext)) {
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
        if (!aTreeMatchContext.mIsHTMLDocument || !aElement->IsHTML()) {
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
          NS_ASSERTION(dirString.EqualsLiteral("ltr") ||
                       dirString.EqualsLiteral("rtl"),
                       "invalid value for -moz-locale-dir");

          if (dirString.EqualsLiteral("rtl") != docIsRTL) {
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
          if (!aElement->IsHTML(nsGkAtoms::table)) {
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
            nsEventStates states
              = sPseudoClassStateDependences[pseudoClass->mType];
            if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(states)) {
              *aDependence = true;
              return false;
            }
          }

          // if we only had to consider HTML, directionality would be exclusively
          // LTR or RTL, and this could be just
          //
          //  if (dirString.EqualsLiteral("rtl") !=
          //    aElement->StyleState().HasState(NS_EVENT_STATE_RTL)
          //
          // However, in markup languages where there is no direction attribute
          // we have to consider the possibility that neither -moz-dir(rtl) nor
          // -moz-dir(ltr) matches.
          nsEventStates state = aElement->StyleState();
          bool elementIsRTL = state.HasState(NS_EVENT_STATE_RTL);
          bool elementIsLTR = state.HasState(NS_EVENT_STATE_LTR);
          nsDependentString dirString(pseudoClass->u.mString);

          if ((dirString.EqualsLiteral("rtl") && !elementIsRTL) ||
              (dirString.EqualsLiteral("ltr") && !elementIsLTR)) {
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
        NS_ABORT_IF_FALSE(false, "How did that happen?");
      }
    } else {
      // Bit-based pseudo-classes
      if (statesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE) &&
          aTreeMatchContext.mCompatMode == eCompatibility_NavQuirks &&
          // global selector:
          !aSelector->HasTagSelector() && !aSelector->mIDList && 
          !aSelector->mClassList && !aSelector->mAttrList &&
          // This (or the other way around) both make :not() asymmetric
          // in quirks mode (and it's hard to work around since we're
          // testing the current mNegations, not the first
          // (unnegated)). This at least makes it closer to the spec.
          !isNegated &&
          // important for |IsQuirkEventSensitive|:
          aElement->IsHTML() && !nsCSSRuleProcessor::IsLink(aElement) &&
          !IsQuirkEventSensitive(aElement->Tag())) {
        // In quirks mode, only make certain elements sensitive to
        // selectors ":hover" and ":active".
        return false;
      } else {
        if (aTreeMatchContext.mForStyling &&
            statesToCheck.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER)) {
          // Mark the element as having :hover-dependent style
          aElement->SetHasRelevantHoverRules();
        }
        if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(statesToCheck)) {
          if (aDependence)
            *aDependence = true;
        } else {
          nsEventStates contentState =
            nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                                         aElement,
                                         aTreeMatchContext,
                                         aTreeMatchContext.VisitedHandling(),
                                         aNodeMatchContext.mIsRelevantLink);
          if (!contentState.HasAtLeastOneOfStates(statesToCheck)) {
            return false;
          }
        }
      }
    }
  }

  bool result = true;
  if (aSelector->mAttrList) {
    // test for attribute match
    uint32_t attrCount = aElement->GetAttrCount();
    if (attrCount == 0) {
      // if no attributes on the content, no match
      return false;
    } else {
      result = true;
      nsAttrSelector* attr = aSelector->mAttrList;
      nsIAtom* matchAttribute;

      do {
        bool isHTML =
          (aTreeMatchContext.mIsHTMLDocument && aElement->IsHTML());
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
          for (uint32_t i = 0; i < attrCount; ++i) {
            const nsAttrName* attrName = aElement->GetAttrNameAt(i);
            NS_ASSERTION(attrName, "GetAttrCount lied or GetAttrNameAt failed");
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
                                aTreeMatchContext, &dependence);
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

// Right now, there are four operators:
//   ' ', the descendant combinator, is greedy
//   '~', the indirect adjacent sibling combinator, is greedy
//   '+' and '>', the direct adjacent sibling and child combinators, are not
#define NS_IS_GREEDY_OPERATOR(ch) \
  ((ch) == PRUnichar(' ') || (ch) == PRUnichar('~'))

static bool SelectorMatchesTree(Element* aPrevElement,
                                  nsCSSSelector* aSelector,
                                  TreeMatchContext& aTreeMatchContext,
                                  bool aLookForRelevantLink)
{
  nsCSSSelector* selector = aSelector;
  Element* prevElement = aPrevElement;
  while (selector) { // check compound selectors
    NS_ASSERTION(!selector->mNext ||
                 selector->mNext->mOperator != PRUnichar(0),
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
    if (PRUnichar('+') == selector->mOperator ||
        PRUnichar('~') == selector->mOperator) {
      // The relevant link must be an ancestor of the node being matched.
      aLookForRelevantLink = false;
      nsIContent* parent = prevElement->GetParent();
      if (parent) {
        if (aTreeMatchContext.mForStyling)
          parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);

        int32_t index = parent->IndexOf(prevElement);
        while (0 <= --index) {
          nsIContent* content = parent->GetChildAt(index);
          if (content->IsElement()) {
            element = content->AsElement();
            break;
          }
        }
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
      }
    }
    if (!element) {
      return false;
    }
    NodeMatchContext nodeContext(nsEventStates(),
                                 aLookForRelevantLink &&
                                   nsCSSRuleProcessor::IsLink(element));
    if (nodeContext.mIsRelevantLink) {
      // If we find an ancestor of the matched node that is a link
      // during the matching process, then it's the relevant link (see
      // constructor call above).
      // Since we are still matching against selectors that contain
      // :visited (they'll just fail), we will always find such a node
      // during the selector matching process if there is a relevant
      // link that can influence selector matching.
      aLookForRelevantLink = false;
      aTreeMatchContext.SetHaveRelevantLink();
    }
    if (SelectorMatches(element, selector, nodeContext, aTreeMatchContext)) {
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
        if (SelectorMatchesTree(element, selector, aTreeMatchContext,
                                aLookForRelevantLink)) {
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
  if (SelectorMatches(data->mElement, aSelector, nodeContext,
                      data->mTreeMatchContext)) {
    nsCSSSelector *next = aSelector->mNext;
    if (!next || SelectorMatchesTree(data->mElement, next,
                                     data->mTreeMatchContext,
                                     !nodeContext.mIsRelevantLink)) {
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
    NodeMatchContext nodeContext(nsEventStates(),
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
      NodeMatchContext nodeContext(nsEventStates(),
                                   nsCSSRuleProcessor::IsLink(aData->mElement));
      ruleHash->EnumerateAllRules(aData->mElement, aData, nodeContext);
    }
  }
}

/* virtual */ void
nsCSSRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade && cascade->mAnonBoxRules.entryCount) {
    RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>
      (PL_DHashTableOperate(&cascade->mAnonBoxRules, aData->mPseudoTag,
                            PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
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

  if (cascade && cascade->mXULTreeRules.entryCount) {
    RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>
      (PL_DHashTableOperate(&cascade->mXULTreeRules, aData->mPseudoTag,
                            PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      NodeMatchContext nodeContext(nsEventStates(),
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

static inline nsRestyleHint RestyleHintForOp(PRUnichar oper)
{
  if (oper == PRUnichar('+') || oper == PRUnichar('~')) {
    return eRestyle_LaterSiblings;
  }

  if (oper != PRUnichar(0)) {
    return eRestyle_Subtree;
  }

  return eRestyle_Self;
}

nsRestyleHint
nsCSSRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  MOZ_ASSERT(!aData->mTreeMatchContext.mForScopedStyle,
             "mCurrentStyleScope will need to be saved and restored after the "
             "SelectorMatchesTree call");

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
    NodeMatchContext nodeContext(aData->mStateMask, false);
    for(; iter != end; ++iter) {
      nsCSSSelector* selector = iter->mSelector;
      nsEventStates states = iter->mStates;

      nsRestyleHint possibleChange = RestyleHintForOp(selector->mOperator);

      // If hint already includes all the bits of possibleChange,
      // don't bother calling SelectorMatches, since even if it returns false
      // hint won't change.
      // Also don't bother calling SelectorMatches if none of the
      // states passed in are relevant here.
      if ((possibleChange & ~hint) &&
          states.HasAtLeastOneOfStates(aData->mStateMask) &&
          // We can optimize away testing selectors that only involve :hover, a
          // namespace, and a tag name against nodes that don't have the
          // NodeHasRelevantHoverRules flag: such a selector didn't match
          // the tag name or namespace the first time around (since the :hover
          // didn't set the NodeHasRelevantHoverRules flag), so it won't
          // match it now.  Check for our selector only having :hover states, or
          // the element having the hover rules flag, or the selector having
          // some sort of non-namespace, non-tagname data in it.
          (states != NS_EVENT_STATE_HOVER ||
           aData->mElement->HasRelevantHoverRules() ||
           selector->mIDList || selector->mClassList ||
           // We generally expect an mPseudoClassList, since we have a :hover.
           // The question is whether we have anything else in there.
           (selector->mPseudoClassList &&
            (selector->mPseudoClassList->mNext ||
             selector->mPseudoClassList->mType !=
               nsCSSPseudoClasses::ePseudoClass_hover)) ||
           selector->mAttrList || selector->mNegations) &&
          SelectorMatches(aData->mElement, selector, nodeContext,
                          aData->mTreeMatchContext) &&
          SelectorMatchesTree(aData->mElement, selector->mNext,
                              aData->mTreeMatchContext,
                              false))
      {
        hint = nsRestyleHint(hint | possibleChange);
      }
    }
  }
  return hint;
}

bool
nsCSSRuleProcessor::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  return cascade && cascade->mSelectorDocumentStates.HasAtLeastOneOfStates(aData->mStateMask);
}

struct AttributeEnumData {
  AttributeEnumData(AttributeRuleProcessorData *aData)
    : data(aData), change(nsRestyleHint(0)) {}

  AttributeRuleProcessorData *data;
  nsRestyleHint change;
};


static void
AttributeEnumFunc(nsCSSSelector* aSelector, AttributeEnumData* aData)
{
  AttributeRuleProcessorData *data = aData->data;

  if (!data->mTreeMatchContext.SetStyleScopeForSelectorMatching(data->mElement,
                                                                data->mScope)) {
    // The selector is for a rule in a scoped style sheet, and the subject
    // of the selector matching is not in its scope.
    return;
  }

  nsRestyleHint possibleChange = RestyleHintForOp(aSelector->mOperator);

  // If enumData->change already includes all the bits of possibleChange, don't
  // bother calling SelectorMatches, since even if it returns false
  // enumData->change won't change.
  NodeMatchContext nodeContext(nsEventStates(), false);
  if ((possibleChange & ~(aData->change)) &&
      SelectorMatches(data->mElement, aSelector, nodeContext,
                      data->mTreeMatchContext) &&
      SelectorMatchesTree(data->mElement, aSelector->mNext,
                          data->mTreeMatchContext, false)) {
    aData->change = nsRestyleHint(aData->change | possibleChange);
  }
}

nsRestyleHint
nsCSSRuleProcessor::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  //  We could try making use of aData->mModType, but :not rules make it a bit
  //  of a pain to do so...  So just ignore it for now.

  AttributeEnumData data(aData);

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
    if (aData->mAttribute == aData->mElement->GetIDAttributeName()) {
      nsIAtom* id = aData->mElement->GetID();
      if (id) {
        AtomSelectorEntry *entry =
          static_cast<AtomSelectorEntry*>
                     (PL_DHashTableOperate(&cascade->mIdSelectors,
                                           id, PL_DHASH_LOOKUP));
        if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
          nsCSSSelector **iter = entry->mSelectors.Elements(),
                        **end = iter + entry->mSelectors.Length();
          for(; iter != end; ++iter) {
            AttributeEnumFunc(*iter, &data);
          }
        }
      }

      nsCSSSelector **iter = cascade->mPossiblyNegatedIDSelectors.Elements(),
                    **end = iter + cascade->mPossiblyNegatedIDSelectors.Length();
      for(; iter != end; ++iter) {
        AttributeEnumFunc(*iter, &data);
      }
    }
    
    if (aData->mAttribute == aData->mElement->GetClassAttributeName()) {
      const nsAttrValue* elementClasses = aData->mElement->GetClasses();
      if (elementClasses) {
        int32_t atomCount = elementClasses->GetAtomCount();
        for (int32_t i = 0; i < atomCount; ++i) {
          nsIAtom* curClass = elementClasses->AtomAt(i);
          AtomSelectorEntry *entry =
            static_cast<AtomSelectorEntry*>
                       (PL_DHashTableOperate(&cascade->mClassSelectors,
                                             curClass, PL_DHASH_LOOKUP));
          if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
            nsCSSSelector **iter = entry->mSelectors.Elements(),
                          **end = iter + entry->mSelectors.Length();
            for(; iter != end; ++iter) {
              AttributeEnumFunc(*iter, &data);
            }
          }
        }
      }

      nsCSSSelector **iter = cascade->mPossiblyNegatedClassSelectors.Elements(),
                    **end = iter +
                              cascade->mPossiblyNegatedClassSelectors.Length();
      for (; iter != end; ++iter) {
        AttributeEnumFunc(*iter, &data);
      }
    }

    AtomSelectorEntry *entry =
      static_cast<AtomSelectorEntry*>
                 (PL_DHashTableOperate(&cascade->mAttributeSelectors,
                                       aData->mAttribute, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      nsCSSSelector **iter = entry->mSelectors.Elements(),
                    **end = iter + entry->mSelectors.Length();
      for(; iter != end; ++iter) {
        AttributeEnumFunc(*iter, &data);
      }
    }
  }

  return data.change;
}

/* virtual */ bool
nsCSSRuleProcessor::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  RuleCascadeData *old = mRuleCascades;
  // We don't want to do anything if there aren't any sets of rules
  // cached yet (or somebody cleared them and is thus responsible for
  // rebuilding things), since we should not build the rule cascade too
  // early (e.g., before we know whether the quirk style sheet should be
  // enabled).  And if there's nothing cached, it doesn't matter if
  // anything changed.  See bug 448281.
  if (old) {
    RefreshRuleCascade(aPresContext);
  }
  return (old != mRuleCascades);
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = 0;
  n += mSheets.SizeOfExcludingThis(aMallocSizeOf);
  for (RuleCascadeData* cascade = mRuleCascades; cascade;
       cascade = cascade->mNext) {
    n += cascade->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

/* virtual */ size_t
nsCSSRuleProcessor::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
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

// Append all the currently-active keyframes rules to aArray.  Return
// true for success and false for failure.
bool
nsCSSRuleProcessor::AppendKeyframesRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsCSSKeyframesRule*>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mKeyframesRules))
      return false;
  }
  
  return true;
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
nsEventStates ComputeSelectorStateDependence(nsCSSSelector& aSelector)
{
  nsEventStates states;
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
            nsCSSSelector* aSelectorPart)
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
          nsTArray<nsCSSSelector*> *array =
            aCascade->AttributeListFor(nsGkAtoms::border);
          if (!array) {
            return false;
          }
          array->AppendElement(aSelectorInTopLevel);
          break;
        }
        default: {
          break;
        }
      }
    }

    // Build mStateSelectors.
    nsEventStates dependentStates = ComputeSelectorStateDependence(*negation);
    if (!dependentStates.IsEmpty()) {
      aCascade->mStateSelectors.AppendElement(
        nsCSSRuleProcessor::StateSelector(dependentStates,
                                          aSelectorInTopLevel));
    }

    // Build mIDSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curID = negation->mIDList; curID;
           curID = curID->mNext) {
        AtomSelectorEntry *entry =
          static_cast<AtomSelectorEntry*>(PL_DHashTableOperate(&aCascade->mIdSelectors,
                                                               curID->mAtom,
                                                               PL_DHASH_ADD));
        if (entry) {
          entry->mSelectors.AppendElement(aSelectorInTopLevel);
        }
      }
    } else if (negation->mIDList) {
      aCascade->mPossiblyNegatedIDSelectors.AppendElement(aSelectorInTopLevel);
    }

    // Build mClassSelectors
    if (negation == aSelectorInTopLevel) {
      for (nsAtomList* curClass = negation->mClassList; curClass;
           curClass = curClass->mNext) {
        AtomSelectorEntry *entry =
          static_cast<AtomSelectorEntry*>(PL_DHashTableOperate(&aCascade->mClassSelectors,
                                                               curClass->mAtom,
                                                               PL_DHASH_ADD));
        if (entry) {
          entry->mSelectors.AppendElement(aSelectorInTopLevel);
        }
      }
    } else if (negation->mClassList) {
      aCascade->mPossiblyNegatedClassSelectors.AppendElement(aSelectorInTopLevel);
    }

    // Build mAttributeSelectors.
    for (nsAttrSelector *attr = negation->mAttrList; attr;
         attr = attr->mNext) {
      nsTArray<nsCSSSelector*> *array =
        aCascade->AttributeListFor(attr->mCasedAttr);
      if (!array) {
        return false;
      }
      array->AppendElement(aSelectorInTopLevel);
      if (attr->mLowercaseAttr != attr->mCasedAttr) {
        array = aCascade->AttributeListFor(attr->mLowercaseAttr);
        if (!array) {
          return false;
        }
        array->AppendElement(aSelectorInTopLevel);
      }
    }

    // Recur through any :-moz-any selectors
    for (nsPseudoClassList* pseudoClass = negation->mPseudoClassList;
         pseudoClass; pseudoClass = pseudoClass->mNext) {
      if (pseudoClass->mType == nsCSSPseudoClasses::ePseudoClass_any) {
        for (nsCSSSelectorList *l = pseudoClass->u.mSelectors; l; l = l->mNext) {
          nsCSSSelector *s = l->mSelectors;
          if (!AddSelector(aCascade, aSelectorInTopLevel, s)) {
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
    NS_ASSERTION(aRuleInfo->mSelector->mNext->mOperator == '>',
                 "Unexpected mNext combinator");
    aRuleInfo->mSelector = aRuleInfo->mSelector->mNext;
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
      NS_ASSERTION(!selector->mNegations, "Shouldn't have negations");
      // Make sure these selectors don't end up in the hashtables we use to
      // match against actual elements, no matter what.  Normally they wouldn't
      // anyway, but trees overload mPseudoClassList with weird stuff.
      continue;
    }
    if (!AddSelector(cascade, selector, selector)) {
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

static bool
InitWeightEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                const void *key)
{
  RuleByWeightEntry* entry = static_cast<RuleByWeightEntry*>(hdr);
  new (entry) RuleByWeightEntry();
  return true;
}

static PLDHashTableOps gRulesByWeightOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashIntKey,
    MatchWeightEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    InitWeightEntry
};

struct CascadeEnumData {
  CascadeEnumData(nsPresContext* aPresContext,
                  nsTArray<nsFontFaceRuleContainer>& aFontFaceRules,
                  nsTArray<nsCSSKeyframesRule*>& aKeyframesRules,
                  nsTArray<nsCSSFontFeatureValuesRule*>& aFontFeatureValuesRules,
                  nsTArray<nsCSSPageRule*>& aPageRules,
                  nsMediaQueryResultCacheKey& aKey,
                  uint8_t aSheetType)
    : mPresContext(aPresContext),
      mFontFaceRules(aFontFaceRules),
      mKeyframesRules(aKeyframesRules),
      mFontFeatureValuesRules(aFontFeatureValuesRules),
      mPageRules(aPageRules),
      mCacheKey(aKey),
      mSheetType(aSheetType)
  {
    if (!PL_DHashTableInit(&mRulesByWeight, &gRulesByWeightOps, nullptr,
                          sizeof(RuleByWeightEntry), 64))
      mRulesByWeight.ops = nullptr;

    // Initialize our arena
    PL_INIT_ARENA_POOL(&mArena, "CascadeEnumDataArena",
                       NS_CASCADEENUMDATA_ARENA_BLOCK_SIZE);
  }

  ~CascadeEnumData()
  {
    if (mRulesByWeight.ops)
      PL_DHashTableFinish(&mRulesByWeight);
    PL_FinishArenaPool(&mArena);
  }

  nsPresContext* mPresContext;
  nsTArray<nsFontFaceRuleContainer>& mFontFaceRules;
  nsTArray<nsCSSKeyframesRule*>& mKeyframesRules;
  nsTArray<nsCSSFontFeatureValuesRule*>& mFontFeatureValuesRules;
  nsTArray<nsCSSPageRule*>& mPageRules;
  nsMediaQueryResultCacheKey& mCacheKey;
  PLArenaPool mArena;
  // Hooray, a manual PLDHashTable since nsClassHashtable doesn't
  // provide a getter that gives me a *reference* to the value.
  PLDHashTable mRulesByWeight; // of PerWeightDataListItem linked lists
  uint8_t mSheetType;
};

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
        PL_DHashTableOperate(&data->mRulesByWeight, NS_INT32_TO_PTR(weight),
                             PL_DHASH_ADD));
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
           css::Rule::DOCUMENT_RULE == type ||
           css::Rule::SUPPORTS_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    if (groupRule->UseForPresentation(data->mPresContext, data->mCacheKey))
      if (!groupRule->EnumerateRulesForwards(CascadeRuleEnumFunc, aData))
        return false;
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
  return true;
}

/* static */ bool
nsCSSRuleProcessor::CascadeSheet(nsCSSStyleSheet* aSheet, CascadeEnumData* aData)
{
  if (aSheet->IsApplicable() &&
      aSheet->UseForPresentation(aData->mPresContext, aData->mCacheKey) &&
      aSheet->mInner) {
    nsCSSStyleSheet* child = aSheet->mInner->mFirstChild;
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


struct FillWeightArrayData {
  FillWeightArrayData(PerWeightData* aArrayData) :
    mIndex(0),
    mWeightArray(aArrayData)
  {
  }
  int32_t mIndex;
  PerWeightData* mWeightArray;
};


static PLDHashOperator
FillWeightArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                uint32_t number, void *arg)
{
  FillWeightArrayData* data = static_cast<FillWeightArrayData*>(arg);
  const RuleByWeightEntry *entry = (const RuleByWeightEntry *)hdr;

  data->mWeightArray[data->mIndex++] = entry->data;

  return PL_DHASH_NEXT;
}

RuleCascadeData*
nsCSSRuleProcessor::GetRuleCascade(nsPresContext* aPresContext)
{
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

  if (mSheets.Length() != 0) {
    nsAutoPtr<RuleCascadeData> newCascade(
      new RuleCascadeData(aPresContext->Medium(),
                          eCompatibility_NavQuirks == aPresContext->CompatibilityMode()));
    if (newCascade) {
      CascadeEnumData data(aPresContext, newCascade->mFontFaceRules,
                           newCascade->mKeyframesRules,
                           newCascade->mFontFeatureValuesRules,
                           newCascade->mPageRules,
                           newCascade->mCacheKey,
                           mSheetType);
      if (!data.mRulesByWeight.ops)
        return; /* out of memory */

      for (uint32_t i = 0; i < mSheets.Length(); ++i) {
        if (!CascadeSheet(mSheets.ElementAt(i), &data))
          return; /* out of memory */
      }

      // Sort the hash table of per-weight linked lists by weight.
      uint32_t weightCount = data.mRulesByWeight.entryCount;
      nsAutoArrayPtr<PerWeightData> weightArray(new PerWeightData[weightCount]);
      FillWeightArrayData fwData(weightArray);
      PL_DHashTableEnumerate(&data.mRulesByWeight, FillWeightArray, &fwData);
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
    NodeMatchContext nodeContext(nsEventStates(), false);
    if (SelectorMatches(aElement, sel, nodeContext, aTreeMatchContext)) {
      nsCSSSelector* next = sel->mNext;
      if (!next ||
          SelectorMatchesTree(aElement, next, aTreeMatchContext, false)) {
        return true;
      }
    }

    aSelectorList = aSelectorList->mNext;
  }

  return false;
}

// TreeMatchContext and AncestorFilter out of line methods
void
TreeMatchContext::InitAncestors(Element *aElement)
{
  MOZ_ASSERT(!mAncestorFilter.mFilter);
  MOZ_ASSERT(mAncestorFilter.mHashes.IsEmpty());

  mAncestorFilter.mFilter = new AncestorFilter::Filter();

  if (MOZ_LIKELY(aElement)) {
    MOZ_ASSERT(aElement->IsInDoc(),
               "aElement must be in the document for the assumption that "
               "GetParentNode() is non-null on all element ancestors of "
               "aElement to be true");
    // Collect up the ancestors
    nsAutoTArray<Element*, 50> ancestors;
    Element* cur = aElement;
    do {
      ancestors.AppendElement(cur);
      nsINode* parent = cur->GetParentNode();
      if (!parent->IsElement()) {
        break;
      }
      cur = parent->AsElement();
    } while (true);

    // Now push them in reverse order.
    for (uint32_t i = ancestors.Length(); i-- != 0; ) {
      mAncestorFilter.PushAncestor(ancestors[i]);
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
  mHashes.AppendElement(aElement->Tag()->hash());
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
  nsINode* cur = aElement->GetParentNode();
  while (cur && cur->IsElement()) {
    MOZ_ASSERT(mElements.Contains(cur));
    cur = cur->GetParentNode();
  }
}
#endif
