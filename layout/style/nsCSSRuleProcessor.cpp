/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *   Daniel Glazman <glazman@netscape.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Rob Arnold <robarnold@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * style rule processor for CSS style sheets, responsible for selector
 * matching and cascading
 */

#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"

#define PL_ARENA_CONST_ALIGN_MASK 7
#define NS_RULEHASH_ARENA_BLOCK_SIZE (256)
#include "plarena.h"

#include "nsCRT.h"
#include "nsIAtom.h"
#include "pldhash.h"
#include "nsHashtable.h"
#include "nsICSSPseudoComparator.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/GroupRule.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsEventStateManager.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsDOMError.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStyleUtil.h"
#include "nsQuickSort.h"
#include "nsAttrValue.h"
#include "nsAttrName.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsIMediaList.h"
#include "nsCSSRules.h"
#include "nsIPrincipal.h"
#include "nsStyleSet.h"
#include "prlog.h"
#include "nsIObserverService.h"
#include "nsIPrivateBrowsingService.h"
#include "nsNetCID.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Element.h"
#include "nsGenericElement.h"
#include "nsNthIndexCache.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

#define VISITED_PSEUDO_PREF "layout.css.visited_links_enabled"

static PRBool gSupportVisitedPseudo = PR_TRUE;

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static nsTArray< nsCOMPtr<nsIAtom> >* sSystemMetrics = 0;

#ifdef XP_WIN
PRUint8 nsCSSRuleProcessor::sWinThemeId = nsILookAndFeel::eWindowsTheme_Generic;
#endif

/**
 * A struct representing a given CSS rule and a particular selector
 * from that rule's selector list.
 */
struct RuleSelectorPair {
  RuleSelectorPair(css::StyleRule* aRule, nsCSSSelector* aSelector)
    : mRule(aRule), mSelector(aSelector) {}

  css::StyleRule*   mRule;
  nsCSSSelector*    mSelector; // which of |mRule|'s selectors
};

/**
 * A struct representing a particular rule in an ordered list of rules
 * (the ordering depending on the weight of mSelector and the order of
 * our rules to start with).
 */
struct RuleValue : RuleSelectorPair {
  RuleValue(const RuleSelectorPair& aRuleSelectorPair, PRInt32 aIndex) :
    RuleSelectorPair(aRuleSelectorPair),
    mIndex(aIndex)
  {}
  PRInt32 mIndex; // High index means high weight/order.
};

// ------------------------------
// Rule hash table
//

// Uses any of the sets of ops below.
struct RuleHashTableEntry : public PLDHashEntryHdr {
  nsTArray<RuleValue> mRules;
};

struct RuleHashTagTableEntry : public RuleHashTableEntry {
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

static PRBool
RuleHash_CIMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

  // Check for case-sensitive match first.
  if (match_atom == entry_atom)
    return PR_TRUE;

  // Use EqualsIgnoreASCIICase instead of full on unicode case conversion
  // in order to save on performance. This is only used in quirks mode
  // anyway.

  return
    nsContentUtils::EqualsIgnoreASCIICase(nsDependentAtomString(entry_atom),
                                          nsDependentAtomString(match_atom));
}

static PRBool
RuleHash_CSMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

  return match_atom == entry_atom;
}

static PRBool
RuleHash_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                   const void *key)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  new (entry) RuleHashTableEntry();
  return PR_TRUE;
}

static void
RuleHash_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(hdr);
  entry->~RuleHashTableEntry();
}

static PRBool
RuleHash_TagTable_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>
                                              (key));
  nsIAtom *entry_atom = static_cast<const RuleHashTagTableEntry*>(hdr)->mTag;

  return match_atom == entry_atom;
}

static PRBool
RuleHash_TagTable_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                            const void *key)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  new (entry) RuleHashTagTableEntry();
  entry->mTag = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  return PR_TRUE;
}

static void
RuleHash_TagTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTagTableEntry* entry = static_cast<RuleHashTagTableEntry*>(hdr);
  entry->~RuleHashTagTableEntry();
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

static PRBool
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
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
  RuleHash(PRBool aQuirksMode);
  ~RuleHash();
  void AppendRule(const RuleSelectorPair &aRuleInfo);
  void EnumerateAllRules(Element* aElement, RuleProcessorData* aData,
                         NodeMatchContext& aNodeMatchContext);
  PLArenaPool& Arena() { return mArena; }

  PRInt64 SizeOf() const;

protected:
  typedef nsTArray<RuleValue> RuleValueList;
  void AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                         const RuleSelectorPair& aRuleInfo);
  void AppendUniversalRule(const RuleSelectorPair& aRuleInfo);

  PRInt32     mRuleCount;
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
  PRInt32   mEnumListSize;

  inline EnumData ToEnumData(const RuleValueList& arr) {
    EnumData data = { arr.Elements(), arr.Elements() + arr.Length() };
    return data;
  }

  PLArenaPool mArena;

#ifdef RULE_HASH_STATS
  PRUint32    mUniversalSelectors;
  PRUint32    mNameSpaceSelectors;
  PRUint32    mTagSelectors;
  PRUint32    mClassSelectors;
  PRUint32    mIdSelectors;

  PRUint32    mElementsMatched;

  PRUint32    mElementUniversalCalls;
  PRUint32    mElementNameSpaceCalls;
  PRUint32    mElementTagCalls;
  PRUint32    mElementClassCalls;
  PRUint32    mElementIdCalls;
#endif // RULE_HASH_STATS
};

RuleHash::RuleHash(PRBool aQuirksMode)
  : mRuleCount(0),
    mUniversalRules(nsnull),
    mEnumList(nsnull), mEnumListSize(0)
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
  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "RuleHashArena", NS_RULEHASH_ARENA_BLOCK_SIZE);

  PL_DHashTableInit(&mTagTable, &RuleHash_TagTable_Ops, nsnull,
                    sizeof(RuleHashTagTableEntry), 64);
  PL_DHashTableInit(&mIdTable,
                    aQuirksMode ? &RuleHash_IdTable_CIOps.ops
                                : &RuleHash_IdTable_CSOps.ops,
                    nsnull, sizeof(RuleHashTableEntry), 16);
  PL_DHashTableInit(&mClassTable,
                    aQuirksMode ? &RuleHash_ClassTable_CIOps.ops
                                : &RuleHash_ClassTable_CSOps.ops,
                    nsnull, sizeof(RuleHashTableEntry), 16);
  PL_DHashTableInit(&mNameSpaceTable, &RuleHash_NameSpaceTable_Ops, nsnull,
                    sizeof(RuleHashTableEntry), 16);
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
      for (PRUint32 i = 0; i < mUniversalRules.Length(); ++i) {
        RuleValue* value = &(mUniversalRules[i]);
        nsAutoString selectorText;
        PRUint32 lineNumber = value->mRule->GetLineNumber();
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
  if (nsnull != mEnumList) {
    delete [] mEnumList;
  }
  // delete arena for strings and small objects
  PL_DHashTableFinish(&mIdTable);
  PL_DHashTableFinish(&mClassTable);
  PL_DHashTableFinish(&mTagTable);
  PL_DHashTableFinish(&mNameSpaceTable);
  PL_FinishArenaPool(&mArena);
}

void RuleHash::AppendRuleToTable(PLDHashTable* aTable, const void* aKey,
                                 const RuleSelectorPair& aRuleInfo)
{
  // Get a new or existing entry.
  RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                         (PL_DHashTableOperate(aTable, aKey, PL_DHASH_ADD));
  if (!entry)
    return;
  entry->mRules.AppendElement(RuleValue(aRuleInfo, mRuleCount++));
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
  mUniversalRules.AppendElement(RuleValue(aRuleInfo, mRuleCount++));
}

void RuleHash::AppendRule(const RuleSelectorPair& aRuleInfo)
{
  nsCSSSelector *selector = aRuleInfo.mSelector;
  if (nsnull != selector->mIDList) {
    AppendRuleToTable(&mIdTable, selector->mIDList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mIdSelectors);
  }
  else if (nsnull != selector->mClassList) {
    AppendRuleToTable(&mClassTable, selector->mClassList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mClassSelectors);
  }
  else if (selector->mLowercaseTag) {
    RuleValue ruleValue(aRuleInfo, mRuleCount++);
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
void ContentEnumFunc(css::StyleRule* aRule, nsCSSSelector* aSelector,
                     RuleProcessorData* data, NodeMatchContext& nodeContext);

void RuleHash::EnumerateAllRules(Element* aElement, RuleProcessorData* aData,
                                 NodeMatchContext& aNodeContext)
{
  PRInt32 nameSpace = aElement->GetNameSpaceID();
  nsIAtom* tag = aElement->Tag();
  nsIAtom* id = aElement->GetID();
  const nsAttrValue* classList = aElement->GetClasses();

  NS_ABORT_IF_FALSE(tag, "How could we not have a tag?");

  PRInt32 classCount = classList ? classList->GetAtomCount() : 0;

  // assume 1 universal, tag, id, and namespace, rather than wasting
  // time counting
  PRInt32 testCount = classCount + 4;

  if (mEnumListSize < testCount) {
    delete [] mEnumList;
    mEnumListSize = NS_MAX(testCount, MIN_ENUM_LIST_SIZE);
    mEnumList = new EnumData[mEnumListSize];
  }

  PRInt32 valueCount = 0;
  RULE_HASH_STAT_INCREMENT(mElementsMatched);

  if (mUniversalRules.Length() != 0) { // universal rules
    mEnumList[valueCount++] = ToEnumData(mUniversalRules);
    RULE_HASH_STAT_INCREMENT_LIST_COUNT(mUniversalRules, mElementUniversalCalls);
  }
  // universal rules within the namespace
  if (kNameSpaceID_Unknown != nameSpace && mNameSpaceTable.entryCount) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mNameSpaceTable, NS_INT32_TO_PTR(nameSpace),
                             PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementNameSpaceCalls);
    }
  }
  if (mTagTable.entryCount) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mTagTable, tag, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementTagCalls);
    }
  }
  if (id && mIdTable.entryCount) {
    RuleHashTableEntry *entry = static_cast<RuleHashTableEntry*>
                                           (PL_DHashTableOperate(&mIdTable, id, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      mEnumList[valueCount++] = ToEnumData(entry->mRules);
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(entry->mRules, mElementIdCalls);
    }
  }
  if (mClassTable.entryCount) {
    for (PRInt32 index = 0; index < classCount; ++index) {
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
    // Merge the lists while there are still multiple lists to merge.
    while (valueCount > 1) {
      PRInt32 valueIndex = 0;
      PRInt32 lowestRuleIndex = mEnumList[valueIndex].mCurValue->mIndex;
      for (PRInt32 index = 1; index < valueCount; ++index) {
        PRInt32 ruleIndex = mEnumList[index].mCurValue->mIndex;
        if (ruleIndex < lowestRuleIndex) {
          valueIndex = index;
          lowestRuleIndex = ruleIndex;
        }
      }
      const RuleValue *cur = mEnumList[valueIndex].mCurValue;
      ContentEnumFunc(cur->mRule, cur->mSelector, aData, aNodeContext);
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
      ContentEnumFunc(value->mRule, value->mSelector, aData, aNodeContext);
    }
  }
}

static PLDHashOperator
RuleHashTableSizeOfEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                              PRUint32 i, void* aClosure)
{
  PRInt64* n = static_cast<PRInt64*>(aClosure);
  RuleHashTableEntry* entry = static_cast<RuleHashTableEntry*>(aHdr);

  *n += entry->mRules.SizeOf();
  // The size of the RuleHashTableEntry itself is accounted for already

  return PL_DHASH_NEXT;
}

PRInt64
RuleHash::SizeOf() const
{
  PRInt64 n = sizeof(*this);

  n += PL_DHASH_TABLE_SIZE(&mIdTable) * sizeof(RuleHashTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mIdTable),
                         RuleHashTableSizeOfEnumerator, &n);

  n += PL_DHASH_TABLE_SIZE(&mClassTable) * sizeof(RuleHashTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mClassTable),
                         RuleHashTableSizeOfEnumerator, &n);

  n += PL_DHASH_TABLE_SIZE(&mTagTable) * sizeof(RuleHashTagTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mTagTable),
                         RuleHashTableSizeOfEnumerator, &n);

  n += PL_DHASH_TABLE_SIZE(&mNameSpaceTable) * sizeof(RuleHashTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mNameSpaceTable),
                         RuleHashTableSizeOfEnumerator, &n);

  n += mUniversalRules.SizeOf();

  return n;
}

//--------------------------------

// A hash table mapping atoms to lists of selectors
struct AtomSelectorEntry : public PLDHashEntryHdr {
  nsIAtom *mAtom;
  nsTArray<nsCSSSelector*> mSelectors;
};

static void
AtomSelector_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  (static_cast<AtomSelectorEntry*>(hdr))->~AtomSelectorEntry();
}

static PRBool
AtomSelector_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                       const void *key)
{
  AtomSelectorEntry *entry = static_cast<AtomSelectorEntry*>(hdr);
  new (entry) AtomSelectorEntry();
  entry->mAtom = const_cast<nsIAtom*>(static_cast<const nsIAtom*>(key));
  return PR_TRUE;
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
  PL_DHashMoveEntryStub,
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
  PL_DHashMoveEntryStub,
  AtomSelector_ClearEntry,
  PL_DHashFinalizeStub,
  AtomSelector_InitEntry
  },
  AtomSelector_GetKey
};

//--------------------------------

struct RuleCascadeData {
  RuleCascadeData(nsIAtom *aMedium, PRBool aQuirksMode)
    : mRuleHash(aQuirksMode),
      mStateSelectors(),
      mSelectorDocumentStates(0),
      mCacheKey(aMedium),
      mNext(nsnull),
      mQuirksMode(aQuirksMode)
  {
    // mAttributeSelectors is matching on the attribute _name_, not the value,
    // and we case-fold names at parse-time, so this is a case-sensitive match.
    PL_DHashTableInit(&mAttributeSelectors, &AtomSelector_CSOps, nsnull,
                      sizeof(AtomSelectorEntry), 16);
    PL_DHashTableInit(&mAnonBoxRules, &RuleHash_TagTable_Ops, nsnull,
                      sizeof(RuleHashTagTableEntry), 16);
    PL_DHashTableInit(&mIdSelectors,
                      aQuirksMode ? &AtomSelector_CIOps.ops :
                                    &AtomSelector_CSOps,
                      nsnull, sizeof(AtomSelectorEntry), 16);
    PL_DHashTableInit(&mClassSelectors,
                      aQuirksMode ? &AtomSelector_CIOps.ops :
                                    &AtomSelector_CSOps,
                      nsnull, sizeof(AtomSelectorEntry), 16);
    memset(mPseudoElementRuleHashes, 0, sizeof(mPseudoElementRuleHashes));
#ifdef MOZ_XUL
    PL_DHashTableInit(&mXULTreeRules, &RuleHash_TagTable_Ops, nsnull,
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
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mPseudoElementRuleHashes); ++i) {
      delete mPseudoElementRuleHashes[i];
    }
  }

  PRInt64 SizeOf() const;

  RuleHash                 mRuleHash;
  RuleHash*
    mPseudoElementRuleHashes[nsCSSPseudoElements::ePseudo_PseudoElementCount];
  nsTArray<nsCSSSelector*> mStateSelectors;
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

  // Looks up or creates the appropriate list in |mAttributeSelectors|.
  // Returns null only on allocation failure.
  nsTArray<nsCSSSelector*>* AttributeListFor(nsIAtom* aAttribute);

  nsMediaQueryResultCacheKey mCacheKey;
  RuleCascadeData*  mNext; // for a different medium

  const PRBool mQuirksMode;
};

static PLDHashOperator
SelectorsSizeOfEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                          PRUint32 i, void* aClosure)
{
  PRInt64* n = (PRInt64*) aClosure;
  AtomSelectorEntry* entry = (AtomSelectorEntry*)aHdr;

  *n += entry->mSelectors.SizeOf();

  return PL_DHASH_NEXT;
}

PRInt64
RuleCascadeData::SizeOf() const
{
  PRInt64 n = sizeof(*this);

  n += mRuleHash.SizeOf();
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mPseudoElementRuleHashes); ++i) {
    if (mPseudoElementRuleHashes[i])
      n += mPseudoElementRuleHashes[i]->SizeOf();
  }

  n += mStateSelectors.SizeOf();

  n += PL_DHASH_TABLE_SIZE(&mIdSelectors) * sizeof(AtomSelectorEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mIdSelectors),
                         SelectorsSizeOfEnumerator, &n);
  n += PL_DHASH_TABLE_SIZE(&mClassSelectors) * sizeof(AtomSelectorEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mClassSelectors),
                         SelectorsSizeOfEnumerator, &n);

  n += mPossiblyNegatedClassSelectors.SizeOf();
  n += mPossiblyNegatedIDSelectors.SizeOf();

  n += PL_DHASH_TABLE_SIZE(&mAttributeSelectors) * sizeof(AtomSelectorEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mAttributeSelectors),
                         SelectorsSizeOfEnumerator, &n);

  n += PL_DHASH_TABLE_SIZE(&mAnonBoxRules) * sizeof(RuleHashTagTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mAnonBoxRules),
                         RuleHashTableSizeOfEnumerator, &n);

#ifdef MOZ_XUL
  n += PL_DHASH_TABLE_SIZE(&mXULTreeRules) * sizeof(RuleHashTagTableEntry);
  PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&mAnonBoxRules),
                         RuleHashTableSizeOfEnumerator, &n);
#endif

  n += mFontFaceRules.SizeOf();
  n += mKeyframesRules.SizeOf();

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
    return nsnull;
  return &entry->mSelectors;
}

class nsPrivateBrowsingObserver : nsIObserver,
                                  nsSupportsWeakReference
{
public:
  nsPrivateBrowsingObserver();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();
  PRBool InPrivateBrowsing() const { return mInPrivateBrowsing; }

private:
  PRBool mInPrivateBrowsing;
};

NS_IMPL_ISUPPORTS2(nsPrivateBrowsingObserver, nsIObserver, nsISupportsWeakReference)

nsPrivateBrowsingObserver::nsPrivateBrowsingObserver()
  : mInPrivateBrowsing(PR_FALSE)
{
}

void
nsPrivateBrowsingObserver::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-after-change", PR_TRUE);
    observerService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_TRUE);
  }
}

nsresult
nsPrivateBrowsingObserver::Observe(nsISupports *aSubject,
                                   const char *aTopic,
                                   const PRUnichar *aData)
{
  if (!strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC)) {
    if (!nsCRT::strcmp(aData, NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).get())) {
      mInPrivateBrowsing = PR_TRUE;
    } else {
      mInPrivateBrowsing = PR_FALSE;
    }
  }
  else if (!strcmp(aTopic, "profile-after-change")) {
    nsCOMPtr<nsIPrivateBrowsingService> pbService =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbService)
      pbService->GetPrivateBrowsingEnabled(&mInPrivateBrowsing);
  }
  return NS_OK;
}

static nsPrivateBrowsingObserver *gPrivateBrowsingObserver = nsnull;

// -------------------------------
// CSS Style rule processor implementation
//

nsCSSRuleProcessor::nsCSSRuleProcessor(const sheet_array_type& aSheets,
                                       PRUint8 aSheetType)
  : mSheets(aSheets)
  , mRuleCascades(nsnull)
  , mLastPresContext(nsnull)
  , mSheetType(aSheetType)
{
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
                               PR_TRUE);

  gPrivateBrowsingObserver = new nsPrivateBrowsingObserver();
  NS_ENSURE_TRUE(gPrivateBrowsingObserver, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(gPrivateBrowsingObserver);
  gPrivateBrowsingObserver->Init();

  return NS_OK;
}

static PRBool
InitSystemMetrics()
{
  NS_ASSERTION(!sSystemMetrics, "already initialized");

  sSystemMetrics = new nsTArray< nsCOMPtr<nsIAtom> >;
  NS_ENSURE_TRUE(sSystemMetrics, PR_FALSE);

  nsresult rv;
  nsCOMPtr<nsILookAndFeel> lookAndFeel(do_GetService(kLookAndFeelCID, &rv));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  /***************************************************************************
   * ANY METRICS ADDED HERE SHOULD ALSO BE ADDED AS MEDIA QUERIES IN         *
   * nsMediaFeatures.cpp                                                     *
   ***************************************************************************/

  PRInt32 metricResult;
  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ScrollArrowStyle, metricResult);
  if (metricResult & nsILookAndFeel::eMetric_ScrollArrowStartBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_backward);
  }
  if (metricResult & nsILookAndFeel::eMetric_ScrollArrowStartForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_forward);
  }
  if (metricResult & nsILookAndFeel::eMetric_ScrollArrowEndBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_backward);
  }
  if (metricResult & nsILookAndFeel::eMetric_ScrollArrowEndForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_forward);
  }

  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ScrollSliderStyle, metricResult);
  if (metricResult != nsILookAndFeel::eMetric_ScrollThumbStyleNormal) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_thumb_proportional);
  }

  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ImagesInMenus, metricResult);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::images_in_menus);
  }

  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ImagesInButtons, metricResult);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::images_in_buttons);
  }

  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_MenuBarDrag, metricResult);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::menubar_drag);
  }

  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_WindowsDefaultTheme, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_default_theme);
  }

  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_MacGraphiteTheme, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_graphite_theme);
  }

  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_DWMCompositor, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_compositor);
  }

  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_WindowsClassic, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_classic);
  }

  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TouchEnabled, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::touch_enabled);
  }
 
  rv = lookAndFeel->GetMetric(nsILookAndFeel::eMetric_MaemoClassic, metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::maemo_classic);
  }

#ifdef XP_WIN
  if (NS_SUCCEEDED(lookAndFeel->GetMetric(nsILookAndFeel::eMetric_WindowsThemeIdentifier,
                                          metricResult))) {
    nsCSSRuleProcessor::SetWindowsThemeIdentifier(static_cast<PRUint8>(metricResult));
    switch(metricResult) {
      case nsILookAndFeel::eWindowsTheme_Aero:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero);
        break;
      case nsILookAndFeel::eWindowsTheme_LunaBlue:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_blue);
        break;
      case nsILookAndFeel::eWindowsTheme_LunaOlive:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_olive);
        break;
      case nsILookAndFeel::eWindowsTheme_LunaSilver:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_silver);
        break;
      case nsILookAndFeel::eWindowsTheme_Royale:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_royale);
        break;
      case nsILookAndFeel::eWindowsTheme_Zune:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_zune);
        break;
      case nsILookAndFeel::eWindowsTheme_Generic:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_generic);
        break;
    }
  }
#endif

  return PR_TRUE;
}

/* static */ void
nsCSSRuleProcessor::FreeSystemMetrics()
{
  delete sSystemMetrics;
  sSystemMetrics = nsnull;
}

/* static */ void
nsCSSRuleProcessor::Shutdown()
{
  FreeSystemMetrics();
  // Make sure we don't crash if Shutdown is called before Init
  NS_IF_RELEASE(gPrivateBrowsingObserver);
}

/* static */ PRBool
nsCSSRuleProcessor::HasSystemMetric(nsIAtom* aMetric)
{
  if (!sSystemMetrics && !InitSystemMetrics()) {
    return PR_FALSE;
  }
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

#ifdef XP_WIN
/* static */ PRUint8
nsCSSRuleProcessor::GetWindowsThemeIdentifier()
{
  if (!sSystemMetrics)
    InitSystemMetrics();
  return sWinThemeId;
}
#endif

// If we have a useful @lang, then aLang will end up nonempty.
static void GetLang(nsIContent* aContent, nsString& aLang)
{
  for (nsIContent* content = aContent; content;
       content = content->GetParent()) {
    if (content->GetAttrCount() > 0) {
      // xml:lang has precedence over lang on HTML elements (see
      // XHTML1 section C.7).
      PRBool hasAttr = content->GetAttr(kNameSpaceID_XML, nsGkAtoms::lang,
                                        aLang);
      if (!hasAttr && content->IsHTML()) {
        hasAttr = content->GetAttr(kNameSpaceID_None, nsGkAtoms::lang,
                                   aLang);
      }
      NS_ASSERTION(hasAttr || aLang.IsEmpty(),
                   "GetAttr that returns false should not make string non-empty");
      if (hasAttr) {
        return;
      }
    }
  }
}

/* static */
nsEventStates
nsCSSRuleProcessor::GetContentState(Element* aElement)
{
  // FIXME: RequestLinkStateUpdate is a hack; see bug 660959.
  aElement->RequestLinkStateUpdate();
  nsEventStates state = aElement->State();

  // If we are not supposed to mark visited links as such, be sure to
  // flip the bits appropriately.  We want to do this here, rather
  // than in GetContentStateForVisitedHandling, so that we don't
  // expose that :visited support is disabled to the Web page.
  if ((!gSupportVisitedPseudo ||
      gPrivateBrowsingObserver->InPrivateBrowsing()) &&
      state.HasState(NS_EVENT_STATE_VISITED)) {
    state &= ~NS_EVENT_STATE_VISITED;
    state |= NS_EVENT_STATE_UNVISITED;
  }
  return state;
}

/* static */
PRBool
nsCSSRuleProcessor::IsLink(Element* aElement)
{
  // FIXME: RequestLinkStateUpdate is a hack; see bug 660959.
  aElement->RequestLinkStateUpdate();
  nsEventStates state = aElement->State();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED);
}

/* static */
nsEventStates
nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                     Element* aElement,
                     nsRuleWalker::VisitedHandlingType aVisitedHandling,
                     PRBool aIsRelevantLink)
{
  nsEventStates contentState = GetContentState(aElement);
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
  // NOTE: For |aStateMask| to work correctly, it's important that any
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
  const PRBool mIsRelevantLink;

  NodeMatchContext(nsEventStates aStateMask, PRBool aIsRelevantLink)
    : mStateMask(aStateMask)
    , mIsRelevantLink(aIsRelevantLink)
  {
  }
};

static PRBool ValueIncludes(const nsSubstring& aValueList,
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
      return PR_TRUE;

    ++p; // we know the next character is not whitespace
  }
  return PR_FALSE;
}

// Return whether we should apply a "global" (i.e., universal-tag)
// selector for event states in quirks mode.  Note that
// |IsLink()| is checked separately by the caller, so we return
// false for |nsGkAtoms::a|, which here means a named anchor.
inline PRBool IsQuirkEventSensitive(nsIAtom *aContentTag)
{
  return PRBool ((nsGkAtoms::button == aContentTag) ||
                 (nsGkAtoms::img == aContentTag)    ||
                 (nsGkAtoms::input == aContentTag)  ||
                 (nsGkAtoms::label == aContentTag)  ||
                 (nsGkAtoms::select == aContentTag) ||
                 (nsGkAtoms::textarea == aContentTag));
}


static inline PRBool
IsSignificantChild(nsIContent* aChild, PRBool aTextIsSignificant,
                   PRBool aWhitespaceIsSignificant)
{
  return nsStyleUtil::IsSignificantChild(aChild, aTextIsSignificant,
                                         aWhitespaceIsSignificant);
}

// This function is to be called once we have fetched a value for an attribute
// whose namespace and name match those of aAttrSelector.  This function
// performs comparisons on the value only, based on aAttrSelector->mFunction.
static PRBool AttrMatchesValue(const nsAttrSelector* aAttrSelector,
                               const nsString& aValue, PRBool isHTML)
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
    return PR_FALSE;

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
      return PR_FALSE;
  }
}

static inline PRBool
edgeChildMatches(Element* aElement, TreeMatchContext& aTreeMatchContext,
                 PRBool checkFirst, PRBool checkLast)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return PR_FALSE;
  }

  if (aTreeMatchContext.mForStyling)
    parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

  return (!checkFirst ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, PR_FALSE, PR_FALSE, PR_TRUE) == 1) &&
         (!checkLast ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, PR_FALSE, PR_TRUE, PR_TRUE) == 1);
}

static inline PRBool
nthChildGenericMatches(Element* aElement,
                       TreeMatchContext& aTreeMatchContext,
                       nsPseudoClassList* pseudoClass,
                       PRBool isOfType, PRBool isFromEnd)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return PR_FALSE;
  }

  if (aTreeMatchContext.mForStyling) {
    if (isFromEnd)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  const PRInt32 index = aTreeMatchContext.mNthIndexCache.
    GetNthIndex(aElement, isOfType, isFromEnd, PR_FALSE);
  if (index <= 0) {
    // Node is anonymous content (not really a child of its parent).
    return PR_FALSE;
  }

  const PRInt32 a = pseudoClass->u.mNumbers[0];
  const PRInt32 b = pseudoClass->u.mNumbers[1];
  // result should be true if there exists n >= 0 such that
  // a * n + b == index.
  if (a == 0) {
    return b == index;
  }

  // Integer division in C does truncation (towards 0).  So
  // check that the result is nonnegative, and that there was no
  // truncation.
  const PRInt32 n = (index - b) / a;
  return n >= 0 && (a * n == index - b);
}

static inline PRBool
edgeOfTypeMatches(Element* aElement, TreeMatchContext& aTreeMatchContext,
                  PRBool checkFirst, PRBool checkLast)
{
  nsIContent *parent = aElement->GetParent();
  if (!parent) {
    return PR_FALSE;
  }

  if (aTreeMatchContext.mForStyling) {
    if (checkLast)
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR);
    else
      parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  }

  return (!checkFirst ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, PR_TRUE, PR_FALSE, PR_TRUE) == 1) &&
         (!checkLast ||
          aTreeMatchContext.mNthIndexCache.
            GetNthIndex(aElement, PR_TRUE, PR_TRUE, PR_TRUE) == 1);
}

static inline PRBool
checkGenericEmptyMatches(Element* aElement,
                         TreeMatchContext& aTreeMatchContext,
                         PRBool isWhitespaceSignificant)
{
  nsIContent *child = nsnull;
  PRInt32 index = -1;

  if (aTreeMatchContext.mForStyling)
    aElement->SetFlags(NODE_HAS_EMPTY_SELECTOR);

  do {
    child = aElement->GetChildAt(++index);
    // stop at first non-comment (and non-whitespace for
    // :-moz-only-whitespace) node        
  } while (child && !IsSignificantChild(child, PR_TRUE, isWhitespaceSignificant));
  return (child == nsnull);
}

// An array of the states that are relevant for various pseudoclasses.
static const nsEventStates sPseudoClassStates[] = {
#define CSS_PSEUDO_CLASS(_name, _value)         \
  nsEventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _states) \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  nsEventStates(),
  nsEventStates()
};
PR_STATIC_ASSERT(NS_ARRAY_LENGTH(sPseudoClassStates) ==
                   nsCSSPseudoClasses::ePseudoClass_NotPseudoClass + 1);

// |aDependence| has two functions:
//  * when non-null, it indicates that we're processing a negation,
//    which is done only when SelectorMatches calls itself recursively
//  * what it points to should be set to true whenever a test is skipped
//    because of aStateMask
static PRBool SelectorMatches(Element* aElement,
                              nsCSSSelector* aSelector,
                              NodeMatchContext& aNodeMatchContext,
                              TreeMatchContext& aTreeMatchContext,
                              PRBool* const aDependence = nsnull)

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
    return PR_FALSE;

  if (aSelector->mLowercaseTag) {
    nsIAtom* selectorTag =
      (aTreeMatchContext.mIsHTMLDocument && aElement->IsHTML()) ?
        aSelector->mLowercaseTag : aSelector->mCasedTag;
    if (selectorTag != aElement->Tag()) {
      return PR_FALSE;
    }
  }

  nsAtomList* IDList = aSelector->mIDList;
  if (IDList) {
    nsIAtom* id = aElement->GetID();
    if (id) {
      // case sensitivity: bug 93371
      const PRBool isCaseSensitive =
        aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

      if (isCaseSensitive) {
        do {
          if (IDList->mAtom != id) {
            return PR_FALSE;
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
            return PR_FALSE;
          }
          IDList = IDList->mNext;
        } while (IDList);
      }
    } else {
      // Element has no id but we have an id selector
      return PR_FALSE;
    }
  }

  nsAtomList* classList = aSelector->mClassList;
  if (classList) {
    // test for class match
    const nsAttrValue *elementClasses = aElement->GetClasses();
    if (!elementClasses) {
      // Element has no classes but we have a class selector
      return PR_FALSE;
    }

    // case sensitivity: bug 93371
    const PRBool isCaseSensitive =
      aTreeMatchContext.mCompatMode != eCompatibility_NavQuirks;

    while (classList) {
      if (!elementClasses->Contains(classList->mAtom,
                                    isCaseSensitive ?
                                      eCaseMatters : eIgnoreCase)) {
        return PR_FALSE;
      }
      classList = classList->mNext;
    }
  }

  const PRBool isNegated = (aDependence != nsnull);
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
        if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozOnlyWhitespace:
        if (!checkGenericEmptyMatches(aElement, aTreeMatchContext, PR_FALSE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozEmptyExceptChildrenWithLocalname:
        {
          NS_ASSERTION(pseudoClass->u.mString, "Must have string!");
          nsIContent *child = nsnull;
          PRInt32 index = -1;

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
                   (!IsSignificantChild(child, PR_TRUE, PR_FALSE) ||
                    (child->GetNameSpaceID() == aElement->GetNameSpaceID() &&
                     child->Tag()->Equals(nsDependentString(pseudoClass->u.mString)))));
          if (child != nsnull) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lang:
        {
          NS_ASSERTION(nsnull != pseudoClass->u.mString, "null lang parameter");
          if (!pseudoClass->u.mString || !*pseudoClass->u.mString) {
            return PR_FALSE;
          }

          // We have to determine the language of the current element.  Since
          // this is currently no property and since the language is inherited
          // from the parent we have to be prepared to look at all parent
          // nodes.  The language itself is encoded in the LANG attribute.
          nsAutoString language;
          GetLang(aElement, language);
          if (!language.IsEmpty()) {
            if (!nsStyleUtil::DashMatchCompare(language,
                                               nsDependentString(pseudoClass->u.mString),
                                               nsASCIICaseInsensitiveStringComparator())) {
              return PR_FALSE;
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
            PRInt32 begin = 0;
            PRInt32 len = language.Length();
            while (begin < len) {
              PRInt32 end = language.FindChar(PRUnichar(','), begin);
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

          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozBoundElement:
        if (aTreeMatchContext.mScopedRoot != aElement) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_root:
        if (aElement->GetParent() ||
            aElement != aElement->GetOwnerDoc()->GetRootElement()) {
          return PR_FALSE;
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
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, PR_TRUE, PR_FALSE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstNode:
        {
          nsIContent *firstNode = nsnull;
          nsIContent *parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);

            PRInt32 index = -1;
            do {
              firstNode = parent->GetChildAt(++index);
              // stop at first non-comment and non-whitespace node
            } while (firstNode &&
                     !IsSignificantChild(firstNode, PR_TRUE, PR_FALSE));
          }
          if (aElement != firstNode) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, PR_FALSE, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastNode:
        {
          nsIContent *lastNode = nsnull;
          nsIContent *parent = aElement->GetParent();
          if (parent) {
            if (aTreeMatchContext.mForStyling)
              parent->SetFlags(NODE_HAS_EDGE_CHILD_SELECTOR);
            
            PRUint32 index = parent->GetChildCount();
            do {
              lastNode = parent->GetChildAt(--index);
              // stop at first non-comment and non-whitespace node
            } while (lastNode &&
                     !IsSignificantChild(lastNode, PR_TRUE, PR_FALSE));
          }
          if (aElement != lastNode) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_onlyChild:
        if (!edgeChildMatches(aElement, aTreeMatchContext, PR_TRUE, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_firstOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, PR_TRUE, PR_FALSE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_lastOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, PR_FALSE, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_onlyOfType:
        if (!edgeOfTypeMatches(aElement, aTreeMatchContext, PR_TRUE, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthChild:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    PR_FALSE, PR_FALSE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthLastChild:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    PR_FALSE, PR_TRUE)) {
          return PR_FALSE;
        }
      break;

      case nsCSSPseudoClasses::ePseudoClass_nthOfType:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    PR_TRUE, PR_FALSE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_nthLastOfType:
        if (!nthChildGenericMatches(aElement, aTreeMatchContext, pseudoClass,
                                    PR_TRUE, PR_TRUE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozHasHandlerRef:
        {
          nsIContent *child = nsnull;
          PRInt32 index = -1;

          do {
            child = aElement->GetChildAt(++index);
            if (child && child->IsHTML() &&
                child->Tag() == nsGkAtoms::param &&
                child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                   NS_LITERAL_STRING("pluginurl"),
                                   eIgnoreCase)) {
              break;
            }
          } while (child);
          if (!child) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozIsHTML:
        if (!aTreeMatchContext.mIsHTMLDocument || !aElement->IsHTML()) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozSystemMetric:
        {
          nsCOMPtr<nsIAtom> metric = do_GetAtom(pseudoClass->u.mString);
          if (!nsCSSRuleProcessor::HasSystemMetric(metric)) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLocaleDir:
        {
          PRBool docIsRTL =
            aTreeMatchContext.mDocument->GetDocumentState().
              HasState(NS_DOCUMENT_STATE_RTL_LOCALE);

          nsDependentString dirString(pseudoClass->u.mString);
          NS_ASSERTION(dirString.EqualsLiteral("ltr") ||
                       dirString.EqualsLiteral("rtl"),
                       "invalid value for -moz-locale-dir");

          if (dirString.EqualsLiteral("rtl") != docIsRTL) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWTheme:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() <=
                nsIDocument::Doc_Theme_None) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWThemeBrightText:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
                nsIDocument::Doc_Theme_Bright) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozLWThemeDarkText:
        {
          if (aTreeMatchContext.mDocument->GetDocumentLWTheme() !=
                nsIDocument::Doc_Theme_Dark) {
            return PR_FALSE;
          }
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozWindowInactive:
        if (!aTreeMatchContext.mDocument->GetDocumentState().
               HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
          return PR_FALSE;
        }
        break;

      case nsCSSPseudoClasses::ePseudoClass_mozTableBorderNonzero:
        {
          if (!aElement->IsHTML(nsGkAtoms::table)) {
            return PR_FALSE;
          }
          nsGenericElement *ge = static_cast<nsGenericElement*>(aElement);
          const nsAttrValue *val = ge->GetParsedAttr(nsGkAtoms::border);
          if (!val ||
              (val->Type() == nsAttrValue::eInteger &&
               val->GetIntegerValue() == 0)) {
            return PR_FALSE;
          }
        }
        break;

      default:
        NS_ABORT_IF_FALSE(PR_FALSE, "How did that happen?");
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
        return PR_FALSE;
      } else {
        if (aNodeMatchContext.mStateMask.HasAtLeastOneOfStates(statesToCheck)) {
          if (aDependence)
            *aDependence = PR_TRUE;
        } else {
          nsEventStates contentState =
            nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                                         aElement,
                                         aTreeMatchContext.VisitedHandling(),
                                         aNodeMatchContext.mIsRelevantLink);
          if (!contentState.HasAtLeastOneOfStates(statesToCheck)) {
            return PR_FALSE;
          }
        }
      }
    }
  }

  PRBool result = PR_TRUE;
  if (aSelector->mAttrList) {
    // test for attribute match
    PRUint32 attrCount = aElement->GetAttrCount();
    if (attrCount == 0) {
      // if no attributes on the content, no match
      return PR_FALSE;
    } else {
      result = PR_TRUE;
      nsAttrSelector* attr = aSelector->mAttrList;
      nsIAtom* matchAttribute;

      do {
        PRBool isHTML =
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
          result = PR_FALSE;
          for (PRUint32 i = 0; i < attrCount; ++i) {
            const nsAttrName* attrName = aElement->GetAttrNameAt(i);
            NS_ASSERTION(attrName, "GetAttrCount lied or GetAttrNameAt failed");
            if (attrName->LocalName() != matchAttribute) {
              continue;
            }
            if (attr->mFunction == NS_ATTR_FUNC_SET) {
              result = PR_TRUE;
            } else {
              nsAutoString value;
#ifdef DEBUG
              PRBool hasAttr =
#endif
                aElement->GetAttr(attrName->NamespaceID(),
                                  attrName->LocalName(), value);
              NS_ASSERTION(hasAttr, "GetAttrNameAt lied");
              result = AttrMatchesValue(attr, value, isHTML);
            }

            // At this point |result| has been set by us
            // explicitly in this loop.  If it's PR_FALSE, we may still match
            // -- the content may have another attribute with the same name but
            // in a different namespace.  But if it's PR_TRUE, we are done (we
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
          result = PR_FALSE;
        }
        else if (attr->mFunction != NS_ATTR_FUNC_SET) {
          nsAutoString value;
#ifdef DEBUG
          PRBool hasAttr =
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
      PRBool dependence = PR_FALSE;
      result = !SelectorMatches(aElement, negation, aNodeMatchContext,
                                aTreeMatchContext, &dependence);
      // If the selector does match due to the dependence on aStateMask,
      // then we want to keep result true so that the final result of
      // SelectorMatches is true.  Doing so tells StateEnumFunc that
      // there is a dependence on the state.
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

static PRBool SelectorMatchesTree(Element* aPrevElement,
                                  nsCSSSelector* aSelector,
                                  TreeMatchContext& aTreeMatchContext,
                                  PRBool aLookForRelevantLink)
{
  nsCSSSelector* selector = aSelector;
  Element* prevElement = aPrevElement;
  while (selector) { // check compound selectors
    NS_ASSERTION(!selector->mNext ||
                 selector->mNext->mOperator != PRUnichar(0),
                 "compound selector without combinator");

    // for adjacent sibling combinators, the content to test against the
    // selector is the previous sibling *element*
    Element* element = nsnull;
    if (PRUnichar('+') == selector->mOperator ||
        PRUnichar('~') == selector->mOperator) {
      // The relevant link must be an ancestor of the node being matched.
      aLookForRelevantLink = PR_FALSE;
      nsIContent* parent = prevElement->GetParent();
      if (parent) {
        if (aTreeMatchContext.mForStyling)
          parent->SetFlags(NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);

        PRInt32 index = parent->IndexOf(prevElement);
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
      }
    }
    if (!element) {
      return PR_FALSE;
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
      aLookForRelevantLink = PR_FALSE;
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
            (selector->mNext->mOperator == PRUnichar(' ') ||
             selector->mNext->mOperator == PRUnichar('>')))) {

        // pretend the selector didn't match, and step through content
        // while testing the same selector

        // This approach is slightly strange in that when it recurs
        // it tests from the top of the content tree, down.  This
        // doesn't matter much for performance since most selectors
        // don't match.  (If most did, it might be faster...)
        if (SelectorMatchesTree(element, selector, aTreeMatchContext,
                                aLookForRelevantLink)) {
          return PR_TRUE;
        }
      }
      selector = selector->mNext;
    }
    else {
      // for adjacent sibling and child combinators, if we didn't find
      // a match, we're done
      if (!NS_IS_GREEDY_OPERATOR(selector->mOperator)) {
        return PR_FALSE;  // parent was required to match
      }
    }
    prevElement = element;
  }
  return PR_TRUE; // all the selectors matched.
}

static inline
void ContentEnumFunc(css::StyleRule* aRule, nsCSSSelector* aSelector,
                     RuleProcessorData* data, NodeMatchContext& nodeContext)
{
  if (nodeContext.mIsRelevantLink) {
    data->mTreeMatchContext.SetHaveRelevantLink();
  }
  if (SelectorMatches(data->mElement, aSelector, nodeContext,
                      data->mTreeMatchContext)) {
    nsCSSSelector *next = aSelector->mNext;
    if (!next || SelectorMatchesTree(data->mElement, next,
                                     data->mTreeMatchContext,
                                     !nodeContext.mIsRelevantLink)) {
      aRule->RuleMatched();
      data->mRuleWalker->Forward(aRule);
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
          ContentEnumFunc(value->mRule, value->mSelector->mNext, aData,
                          nodeContext);
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
  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  // Look up the content node in the state rule list, which points to
  // any (CSS2 definition) simple selector (whether or not it is the
  // subject) that has a state pseudo-class on it.  This means that this
  // code will be matching selectors that aren't real selectors in any
  // stylesheet (e.g., if there is a selector "body > p:hover > a", then
  // "body > p:hover" will be in |cascade->mStateSelectors|).  Note that
  // |IsStateSelector| below determines which selectors are in
  // |cascade->mStateSelectors|.
  nsRestyleHint hint = nsRestyleHint(0);
  if (cascade) {
    nsCSSSelector **iter = cascade->mStateSelectors.Elements(),
                  **end = iter + cascade->mStateSelectors.Length();
    NodeMatchContext nodeContext(aData->mStateMask, PR_FALSE);
    for(; iter != end; ++iter) {
      nsCSSSelector* selector = *iter;

      nsRestyleHint possibleChange = RestyleHintForOp(selector->mOperator);

      // If hint already includes all the bits of possibleChange,
      // don't bother calling SelectorMatches, since even if it returns false
      // hint won't change.
      if ((possibleChange & ~hint) &&
          SelectorMatches(aData->mElement, selector, nodeContext,
                          aData->mTreeMatchContext) &&
          SelectorMatchesTree(aData->mElement, selector->mNext,
                              aData->mTreeMatchContext,
                              PR_FALSE))
      {
        hint = nsRestyleHint(hint | possibleChange);
      }
    }
  }
  return hint;
}

PRBool
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

  nsRestyleHint possibleChange = RestyleHintForOp(aSelector->mOperator);

  // If enumData->change already includes all the bits of possibleChange, don't
  // bother calling SelectorMatches, since even if it returns false
  // enumData->change won't change.
  NodeMatchContext nodeContext(nsEventStates(), PR_FALSE);
  if ((possibleChange & ~(aData->change)) &&
      SelectorMatches(data->mElement, aSelector, nodeContext,
                      data->mTreeMatchContext) &&
      SelectorMatchesTree(data->mElement, aSelector->mNext,
                          data->mTreeMatchContext, PR_FALSE)) {
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
        aData->mElement == aData->mElement->GetOwnerDoc()->GetRootElement())
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
        PRInt32 atomCount = elementClasses->GetAtomCount();
        for (PRInt32 i = 0; i < atomCount; ++i) {
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

/* virtual */ PRBool
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

/* virtual */ PRInt64
nsCSSRuleProcessor::SizeOf() const
{
  PRInt64 n = sizeof(*this);

  n += mSheets.SizeOf();
  for (RuleCascadeData* cascade = mRuleCascades; cascade;
       cascade = cascade->mNext) {
    n += cascade->SizeOf();
  }

  return n;
}

// Append all the currently-active font face rules to aArray.  Return
// true for success and false for failure.
PRBool
nsCSSRuleProcessor::AppendFontFaceRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsFontFaceRuleContainer>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mFontFaceRules))
      return PR_FALSE;
  }
  
  return PR_TRUE;
}

// Append all the currently-active keyframes rules to aArray.  Return
// true for success and false for failure.
PRBool
nsCSSRuleProcessor::AppendKeyframesRules(
                              nsPresContext *aPresContext,
                              nsTArray<nsCSSKeyframesRule*>& aArray)
{
  RuleCascadeData* cascade = GetRuleCascade(aPresContext);

  if (cascade) {
    if (!aArray.AppendElements(cascade->mKeyframesRules))
      return PR_FALSE;
  }
  
  return PR_TRUE;
}

nsresult
nsCSSRuleProcessor::ClearRuleCascades()
{
  // We rely on our caller (perhaps indirectly) to do something that
  // will rebuild style data and the user font set (either
  // nsIPresShell::ReconstructStyleData or
  // nsPresContext::RebuildAllStyleData).
  RuleCascadeData *data = mRuleCascades;
  mRuleCascades = nsnull;
  while (data) {
    RuleCascadeData *next = data->mNext;
    delete data;
    data = next;
  }
  return NS_OK;
}


// This function should return true only for selectors that need to be
// checked by |HasStateDependentStyle|.
inline
PRBool IsStateSelector(nsCSSSelector& aSelector)
{
  for (nsPseudoClassList* pseudoClass = aSelector.mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    // Tree pseudo-elements overload mPseudoClassList for things that
    // aren't pseudo-classes.
    if (pseudoClass->mType >= nsCSSPseudoClasses::ePseudoClass_Count) {
      continue;
    }
    if (!sPseudoClassStates[pseudoClass->mType].IsEmpty()) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

static PRBool
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
            return PR_FALSE;
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
    if (IsStateSelector(*negation))
      aCascade->mStateSelectors.AppendElement(aSelectorInTopLevel);

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
        return PR_FALSE;
      }
      array->AppendElement(aSelectorInTopLevel);
      if (attr->mLowercaseAttr != attr->mCasedAttr) {
        array = aCascade->AttributeListFor(attr->mLowercaseAttr);
        if (!array) {
          return PR_FALSE;
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
            return PR_FALSE;
          }
        }
      }
    }
  }

  return PR_TRUE;
}

static PRBool
AddRule(RuleSelectorPair* aRuleInfo, RuleCascadeData* aCascade)
{
  RuleCascadeData * const cascade = aCascade;

  // Build the rule hash.
  nsCSSPseudoElements::Type pseudoType = aRuleInfo->mSelector->PseudoType();
  if (NS_LIKELY(pseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement)) {
    cascade->mRuleHash.AppendRule(*aRuleInfo);
  } else if (pseudoType < nsCSSPseudoElements::ePseudo_PseudoElementCount) {
    RuleHash*& ruleHash = cascade->mPseudoElementRuleHashes[pseudoType];
    if (!ruleHash) {
      ruleHash = new RuleHash(cascade->mQuirksMode);
      if (!ruleHash) {
        // Out of memory; give up
        return PR_FALSE;
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
                         RuleValue(*aRuleInfo, 0));
  } else {
#ifdef MOZ_XUL
    NS_ASSERTION(pseudoType == nsCSSPseudoElements::ePseudo_XULTree,
                 "Unexpected pseudo type");
    // Index doesn't matter here, since we'll just be walking these
    // rules in order; just pass 0.
    AppendRuleToTagTable(&cascade->mXULTreeRules,
                         aRuleInfo->mSelector->mLowercaseTag,
                         RuleValue(*aRuleInfo, 0));
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
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

struct PerWeightData {
  PRInt32 mWeight;
  nsTArray<RuleSelectorPair> mRules; // forward order
};

struct RuleByWeightEntry : public PLDHashEntryHdr {
  PerWeightData data; // mWeight is key, mRules are value
};

static PLDHashNumber
HashIntKey(PLDHashTable *table, const void *key)
{
  return PLDHashNumber(NS_PTR_TO_INT32(key));
}

static PRBool
MatchWeightEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                 const void *key)
{
  const RuleByWeightEntry *entry = (const RuleByWeightEntry *)hdr;
  return entry->data.mWeight == NS_PTR_TO_INT32(key);
}

static PRBool
InitWeightEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                const void *key)
{
  RuleByWeightEntry* entry = static_cast<RuleByWeightEntry*>(hdr);
  new (entry) RuleByWeightEntry();
  return PR_TRUE;
}

static void
ClearWeightEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleByWeightEntry* entry = static_cast<RuleByWeightEntry*>(hdr);
  entry->~RuleByWeightEntry();
}

static PLDHashTableOps gRulesByWeightOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashIntKey,
    MatchWeightEntry,
    PL_DHashMoveEntryStub,
    ClearWeightEntry,
    PL_DHashFinalizeStub,
    InitWeightEntry
};

struct CascadeEnumData {
  CascadeEnumData(nsPresContext* aPresContext,
                  nsTArray<nsFontFaceRuleContainer>& aFontFaceRules,
                  nsTArray<nsCSSKeyframesRule*>& aKeyframesRules,
                  nsMediaQueryResultCacheKey& aKey,
                  PLArenaPool& aArena,
                  PRUint8 aSheetType)
    : mPresContext(aPresContext),
      mFontFaceRules(aFontFaceRules),
      mKeyframesRules(aKeyframesRules),
      mCacheKey(aKey),
      mArena(aArena),
      mSheetType(aSheetType)
  {
    if (!PL_DHashTableInit(&mRulesByWeight, &gRulesByWeightOps, nsnull,
                          sizeof(RuleByWeightEntry), 64))
      mRulesByWeight.ops = nsnull;
  }

  ~CascadeEnumData()
  {
    if (mRulesByWeight.ops)
      PL_DHashTableFinish(&mRulesByWeight);
  }

  nsPresContext* mPresContext;
  nsTArray<nsFontFaceRuleContainer>& mFontFaceRules;
  nsTArray<nsCSSKeyframesRule*>& mKeyframesRules;
  nsMediaQueryResultCacheKey& mCacheKey;
  PLArenaPool& mArena;
  // Hooray, a manual PLDHashTable since nsClassHashtable doesn't
  // provide a getter that gives me a *reference* to the value.
  PLDHashTable mRulesByWeight; // of RuleValue* linked lists (?)
  PRUint8 mSheetType;
};

/*
 * This enumerates style rules in a sheet (and recursively into any
 * grouping rules) in order to:
 *  (1) add any style rules, in order, into data->mRulesByWeight (for
 *      the primary CSS cascade), where they are separated by weight
 *      but kept in order per-weight, and
 *  (2) add any @font-face rules, in order, into data->mFontFaceRules.
 *  (3) add any @keyframes rules, in order, into data->mKeyframesRules.
 */
static PRBool
CascadeRuleEnumFunc(css::Rule* aRule, void* aData)
{
  CascadeEnumData* data = (CascadeEnumData*)aData;
  PRInt32 type = aRule->GetType();

  if (css::Rule::STYLE_RULE == type) {
    css::StyleRule* styleRule = static_cast<css::StyleRule*>(aRule);

    for (nsCSSSelectorList *sel = styleRule->Selector();
         sel; sel = sel->mNext) {
      PRInt32 weight = sel->mWeight;
      RuleByWeightEntry *entry = static_cast<RuleByWeightEntry*>(
        PL_DHashTableOperate(&data->mRulesByWeight, NS_INT32_TO_PTR(weight),
                             PL_DHASH_ADD));
      if (!entry)
        return PR_FALSE;
      entry->data.mWeight = weight;
      // entry->data.mRules must be in forward order.
      entry->data.mRules.AppendElement(RuleSelectorPair(styleRule,
                                                        sel->mSelectors));
    }
  }
  else if (css::Rule::MEDIA_RULE == type ||
           css::Rule::DOCUMENT_RULE == type) {
    css::GroupRule* groupRule = static_cast<css::GroupRule*>(aRule);
    if (groupRule->UseForPresentation(data->mPresContext, data->mCacheKey))
      if (!groupRule->EnumerateRulesForwards(CascadeRuleEnumFunc, aData))
        return PR_FALSE;
  }
  else if (css::Rule::FONT_FACE_RULE == type) {
    nsCSSFontFaceRule *fontFaceRule = static_cast<nsCSSFontFaceRule*>(aRule);
    nsFontFaceRuleContainer *ptr = data->mFontFaceRules.AppendElement();
    if (!ptr)
      return PR_FALSE;
    ptr->mRule = fontFaceRule;
    ptr->mSheetType = data->mSheetType;
  }
  else if (css::Rule::KEYFRAMES_RULE == type) {
    nsCSSKeyframesRule *keyframesRule =
      static_cast<nsCSSKeyframesRule*>(aRule);
    if (!data->mKeyframesRules.AppendElement(keyframesRule)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

/* static */ PRBool
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
      return PR_FALSE;
  }
  return PR_TRUE;
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
  PRInt32 mIndex;
  PerWeightData* mWeightArray;
};


static PLDHashOperator
FillWeightArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                PRUint32 number, void *arg)
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
                           newCascade->mCacheKey,
                           newCascade->mRuleHash.Arena(),
                           mSheetType);
      if (!data.mRulesByWeight.ops)
        return; /* out of memory */

      for (PRUint32 i = 0; i < mSheets.Length(); ++i) {
        if (!CascadeSheet(mSheets.ElementAt(i), &data))
          return; /* out of memory */
      }

      // Sort the hash table of per-weight linked lists by weight.
      PRUint32 weightCount = data.mRulesByWeight.entryCount;
      nsAutoArrayPtr<PerWeightData> weightArray(new PerWeightData[weightCount]);
      FillWeightArrayData fwData(weightArray);
      PL_DHashTableEnumerate(&data.mRulesByWeight, FillWeightArray, &fwData);
      NS_QuickSort(weightArray, weightCount, sizeof(PerWeightData),
                   CompareWeightData, nsnull);

      // Put things into the rule hash.
      // The primary sort is by weight...
      for (PRUint32 i = 0; i < weightCount; ++i) {
        // and the secondary sort is by order.  mRules are already in
        // the right order..
        nsTArray<RuleSelectorPair>& arr = weightArray[i].mRules;
        for (RuleSelectorPair *cur = arr.Elements(),
                              *end = cur + arr.Length();
             cur != end; ++cur) {
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

/* static */ PRBool
nsCSSRuleProcessor::SelectorListMatches(Element* aElement,
                                        TreeMatchContext& aTreeMatchContext,
                                        nsCSSSelectorList* aSelectorList)
{
  while (aSelectorList) {
    nsCSSSelector* sel = aSelectorList->mSelectors;
    NS_ASSERTION(sel, "Should have *some* selectors");
    NS_ASSERTION(!sel->IsPseudoElement(), "Shouldn't have been called");
    NodeMatchContext nodeContext(nsEventStates(), PR_FALSE);
    if (SelectorMatches(aElement, sel, nodeContext, aTreeMatchContext)) {
      nsCSSSelector* next = sel->mNext;
      if (!next ||
          SelectorMatchesTree(aElement, next, aTreeMatchContext, PR_FALSE)) {
        return PR_TRUE;
      }
    }

    aSelectorList = aSelectorList->mNext;
  }

  return PR_FALSE;
}
