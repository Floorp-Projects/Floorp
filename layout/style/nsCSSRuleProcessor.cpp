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

#define PL_ARENA_CONST_ALIGN_MASK 7
#define NS_RULEHASH_ARENA_BLOCK_SIZE (256)
#include "plarena.h"

#include "nsCRT.h"
#include "nsIAtom.h"
#include "pldhash.h"
#include "nsHashtable.h"
#include "nsICSSPseudoComparator.h"
#include "nsCSSRuleProcessor.h"
#include "nsICSSStyleRule.h"
#include "nsICSSGroupRule.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "nsDOMError.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStyleUtil.h"
#include "nsQuickSort.h"
#include "nsAttrValue.h"
#include "nsAttrName.h"

struct RuleValue {
  /**
   * |RuleValue|s are constructed before they become part of the
   * |RuleHash|, to act as rule/selector pairs.  |Add| is called when
   * they are added to the |RuleHash|, and can be considered the second
   * half of the constructor.
   *
   * |RuleValue|s are added to the rule hash from highest weight/order
   * to lowest (since this is the fast way to build a singly linked
   * list), so the index used to remember the order is backwards.
   */
  RuleValue(nsICSSStyleRule* aRule, nsCSSSelector* aSelector)
    : mRule(aRule), mSelector(aSelector) {}

  RuleValue* Add(PRInt32 aBackwardIndex, RuleValue *aNext)
  {
    mBackwardIndex = aBackwardIndex;
    mNext = aNext;
    return this;
  }
    
  // CAUTION: ~RuleValue will never get called as RuleValues are arena
  // allocated and arena cleanup will take care of deleting memory.
  // Add code to RuleHash::~RuleHash to get it to call the destructor
  // if any more cleanup needs to happen.
  ~RuleValue()
  {
    // Rule values are arena allocated. No need for any deletion.
  }

  // Placement new to arena allocate the RuleValues
  void *operator new(size_t aSize, PLArenaPool &aArena) CPP_THROW_NEW {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &aArena, aSize);
    return mem;
  }

  nsICSSStyleRule*  mRule;
  nsCSSSelector*    mSelector; // which of |mRule|'s selectors
  PRInt32           mBackwardIndex; // High index means low weight/order.
  RuleValue*        mNext;
};

// ------------------------------
// Rule hash table
//

// Uses any of the sets of ops below.
struct RuleHashTableEntry : public PLDHashEntryHdr {
  RuleValue *mRules; // linked list of |RuleValue|, null-terminated
};

PR_STATIC_CALLBACK(PLDHashNumber)
RuleHash_CIHashKey(PLDHashTable *table, const void *key)
{
  nsIAtom *atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*, key));

  nsAutoString str;
  atom->ToString(str);
  ToUpperCase(str);
  return HashString(str);
}

typedef nsIAtom*
(* PR_CALLBACK RuleHashGetKey)    (PLDHashTable *table,
                                   const PLDHashEntryHdr *entry);

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

PR_STATIC_CALLBACK(PRBool)
RuleHash_CIMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*,
                            key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

  // Check for case-sensitive match first.
  if (match_atom == entry_atom)
    return PR_TRUE;

  const char *match_str, *entry_str;
  match_atom->GetUTF8String(&match_str);
  entry_atom->GetUTF8String(&entry_str);

  return (nsCRT::strcasecmp(entry_str, match_str) == 0);
}

PR_STATIC_CALLBACK(PRBool)
RuleHash_CSMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*,
                            key));
  // Use our extra |getKey| callback to avoid code duplication.
  nsIAtom *entry_atom = ToLocalOps(table->ops)->getKey(table, hdr);

  return match_atom == entry_atom;
}

PR_STATIC_CALLBACK(nsIAtom*)
RuleHash_TagTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    NS_STATIC_CAST(const RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mTag;
}

PR_STATIC_CALLBACK(nsIAtom*)
RuleHash_ClassTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    NS_STATIC_CAST(const RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mClassList->mAtom;
}

PR_STATIC_CALLBACK(nsIAtom*)
RuleHash_IdTable_GetKey(PLDHashTable *table, const PLDHashEntryHdr *hdr)
{
  const RuleHashTableEntry *entry =
    NS_STATIC_CAST(const RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mIDList->mAtom;
}

PR_STATIC_CALLBACK(PLDHashNumber)
RuleHash_NameSpaceTable_HashKey(PLDHashTable *table, const void *key)
{
  return NS_PTR_TO_INT32(key);
}

PR_STATIC_CALLBACK(PRBool)
RuleHash_NameSpaceTable_MatchEntry(PLDHashTable *table,
                                   const PLDHashEntryHdr *hdr,
                                   const void *key)
{
  const RuleHashTableEntry *entry =
    NS_STATIC_CAST(const RuleHashTableEntry*, hdr);

  return NS_PTR_TO_INT32(key) ==
         entry->mRules->mSelector->mNameSpace;
}

static const RuleHashTableOps RuleHash_TagTable_Ops = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
  },
  RuleHash_TagTable_GetKey
};

// Case-sensitive ops.
static const RuleHashTableOps RuleHash_ClassTable_CSOps = {
  {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
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
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
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
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
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
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
  },
  RuleHash_IdTable_GetKey
};

static const PLDHashTableOps RuleHash_NameSpaceTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_NameSpaceTable_HashKey,
  RuleHash_NameSpaceTable_MatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL,
};

#undef RULE_HASH_STATS
#undef PRINT_UNIVERSAL_RULES

#ifdef DEBUG_dbaron
#define RULE_HASH_STATS
#define PRINT_UNIVERSAL_RULES
#endif

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT(var_) PR_BEGIN_MACRO ++(var_); PR_END_MACRO
#else
#define RULE_HASH_STAT_INCREMENT(var_) PR_BEGIN_MACRO PR_END_MACRO
#endif

// Enumerator callback function.
typedef void (*RuleEnumFunc)(nsICSSStyleRule* aRule,
                             nsCSSSelector* aSelector,
                             void *aData);

class RuleHash {
public:
  RuleHash(PRBool aQuirksMode);
  ~RuleHash();
  void PrependRule(RuleValue *aRuleInfo);
  void EnumerateAllRules(PRInt32 aNameSpace, nsIAtom* aTag, nsIAtom* aID,
                         const nsAttrValue* aClassList,
                         RuleEnumFunc aFunc, void* aData);
  void EnumerateTagRules(nsIAtom* aTag,
                         RuleEnumFunc aFunc, void* aData);
  PLArenaPool& Arena() { return mArena; }

protected:
  void PrependRuleToTable(PLDHashTable* aTable, const void* aKey,
                          RuleValue* aRuleInfo);
  void PrependUniversalRule(RuleValue* aRuleInfo);

  // All rule values in these hashtables are arena allocated
  PRInt32     mRuleCount;
  PLDHashTable mIdTable;
  PLDHashTable mClassTable;
  PLDHashTable mTagTable;
  PLDHashTable mNameSpaceTable;
  RuleValue *mUniversalRules;

  RuleValue** mEnumList;
  PRInt32     mEnumListSize;

  PLArenaPool mArena;

#ifdef RULE_HASH_STATS
  PRUint32    mUniversalSelectors;
  PRUint32    mNameSpaceSelectors;
  PRUint32    mTagSelectors;
  PRUint32    mClassSelectors;
  PRUint32    mIdSelectors;

  PRUint32    mElementsMatched;
  PRUint32    mPseudosMatched;

  PRUint32    mElementUniversalCalls;
  PRUint32    mElementNameSpaceCalls;
  PRUint32    mElementTagCalls;
  PRUint32    mElementClassCalls;
  PRUint32    mElementIdCalls;

  PRUint32    mPseudoTagCalls;
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
    mPseudosMatched(0),
    mElementUniversalCalls(0),
    mElementNameSpaceCalls(0),
    mElementTagCalls(0),
    mElementClassCalls(0),
    mElementIdCalls(0),
    mPseudoTagCalls(0)
#endif
{
  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "RuleHashArena", NS_RULEHASH_ARENA_BLOCK_SIZE);

  PL_DHashTableInit(&mTagTable, &RuleHash_TagTable_Ops.ops, nsnull,
                    sizeof(RuleHashTableEntry), 64);
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
#ifdef RULE_HASH_STATS
  printf(
"RuleHash(%p):\n"
"  Selectors: Universal (%u) NameSpace(%u) Tag(%u) Class(%u) Id(%u)\n"
"  Content Nodes: Elements(%u) Pseudo-Elements(%u)\n"
"  Element Calls: Universal(%u) NameSpace(%u) Tag(%u) Class(%u) Id(%u)\n"
"  Pseudo-Element Calls: Tag(%u)\n",
         NS_STATIC_CAST(void*, this),
         mUniversalSelectors, mNameSpaceSelectors, mTagSelectors,
           mClassSelectors, mIdSelectors,
         mElementsMatched,
         mPseudosMatched,
         mElementUniversalCalls, mElementNameSpaceCalls, mElementTagCalls,
           mElementClassCalls, mElementIdCalls,
         mPseudoTagCalls);
#ifdef PRINT_UNIVERSAL_RULES
  {
    RuleValue* value = mUniversalRules;
    if (value) {
      printf("  Universal rules:\n");
      do {
        nsAutoString selectorText;
        PRUint32 lineNumber = value->mRule->GetLineNumber();
        nsCOMPtr<nsIStyleSheet> sheet;
        value->mRule->GetStyleSheet(*getter_AddRefs(sheet));
        nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(sheet);
        value->mSelector->ToString(selectorText, cssSheet);

        printf("    line %d, %s\n",
               lineNumber, NS_ConvertUTF16toUTF8(selectorText).get());
        value = value->mNext;
      } while (value);
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

void RuleHash::PrependRuleToTable(PLDHashTable* aTable, const void* aKey,
                                  RuleValue* aRuleInfo)
{
  // Get a new or existing entry.
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
      PL_DHashTableOperate(aTable, aKey, PL_DHASH_ADD));
  if (!entry)
    return;
  entry->mRules = aRuleInfo->Add(mRuleCount++, entry->mRules);
}

void RuleHash::PrependUniversalRule(RuleValue *aRuleInfo)
{
  mUniversalRules = aRuleInfo->Add(mRuleCount++, mUniversalRules);
}

void RuleHash::PrependRule(RuleValue *aRuleInfo)
{
  nsCSSSelector *selector = aRuleInfo->mSelector;
  if (nsnull != selector->mIDList) {
    PrependRuleToTable(&mIdTable, selector->mIDList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mIdSelectors);
  }
  else if (nsnull != selector->mClassList) {
    PrependRuleToTable(&mClassTable, selector->mClassList->mAtom, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mClassSelectors);
  }
  else if (nsnull != selector->mTag) {
    PrependRuleToTable(&mTagTable, selector->mTag, aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mTagSelectors);
  }
  else if (kNameSpaceID_Unknown != selector->mNameSpace) {
    PrependRuleToTable(&mNameSpaceTable,
                       NS_INT32_TO_PTR(selector->mNameSpace), aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mNameSpaceSelectors);
  }
  else {  // universal tag selector
    PrependUniversalRule(aRuleInfo);
    RULE_HASH_STAT_INCREMENT(mUniversalSelectors);
  }
}

// this should cover practically all cases so we don't need to reallocate
#define MIN_ENUM_LIST_SIZE 8

#ifdef RULE_HASH_STATS
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_) \
  do { ++(var_); (list_) = (list_)->mNext; } while (list_)
#else
#define RULE_HASH_STAT_INCREMENT_LIST_COUNT(list_, var_) \
  PR_BEGIN_MACRO PR_END_MACRO
#endif

void RuleHash::EnumerateAllRules(PRInt32 aNameSpace, nsIAtom* aTag,
                                 nsIAtom* aID, const nsAttrValue* aClassList,
                                 RuleEnumFunc aFunc, void* aData)
{
  PRInt32 classCount = aClassList ? aClassList->GetAtomCount() : 0;

  // assume 1 universal, tag, id, and namespace, rather than wasting
  // time counting
  PRInt32 testCount = classCount + 4;

  if (mEnumListSize < testCount) {
    delete [] mEnumList;
    mEnumListSize = PR_MAX(testCount, MIN_ENUM_LIST_SIZE);
    mEnumList = new RuleValue*[mEnumListSize];
  }

  PRInt32 valueCount = 0;
  RULE_HASH_STAT_INCREMENT(mElementsMatched);

  { // universal rules
    RuleValue* value = mUniversalRules;
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(value, mElementUniversalCalls);
    }
  }
  // universal rules within the namespace
  if (kNameSpaceID_Unknown != aNameSpace) {
    RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
        PL_DHashTableOperate(&mNameSpaceTable, NS_INT32_TO_PTR(aNameSpace),
                             PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      RuleValue *value = entry->mRules;
      mEnumList[valueCount++] = value;
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(value, mElementNameSpaceCalls);
    }
  }
  if (nsnull != aTag) {
    RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
        PL_DHashTableOperate(&mTagTable, aTag, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      RuleValue *value = entry->mRules;
      mEnumList[valueCount++] = value;
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(value, mElementTagCalls);
    }
  }
  if (nsnull != aID) {
    RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
        PL_DHashTableOperate(&mIdTable, aID, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      RuleValue *value = entry->mRules;
      mEnumList[valueCount++] = value;
      RULE_HASH_STAT_INCREMENT_LIST_COUNT(value, mElementIdCalls);
    }
  }
  { // extra scope to work around compiler bugs with |for| scoping.
    for (PRInt32 index = 0; index < classCount; ++index) {
      RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
        PL_DHashTableOperate(&mClassTable, aClassList->AtomAt(index),
                             PL_DHASH_LOOKUP));
      if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        RuleValue *value = entry->mRules;
        mEnumList[valueCount++] = value;
        RULE_HASH_STAT_INCREMENT_LIST_COUNT(value, mElementClassCalls);
      }
    }
  }
  NS_ASSERTION(valueCount <= testCount, "values exceeded list size");

  if (valueCount > 0) {
    // Merge the lists while there are still multiple lists to merge.
    while (valueCount > 1) {
      PRInt32 valueIndex = 0;
      PRInt32 highestRuleIndex = mEnumList[valueIndex]->mBackwardIndex;
      for (PRInt32 index = 1; index < valueCount; ++index) {
        PRInt32 ruleIndex = mEnumList[index]->mBackwardIndex;
        if (ruleIndex > highestRuleIndex) {
          valueIndex = index;
          highestRuleIndex = ruleIndex;
        }
      }
      RuleValue *cur = mEnumList[valueIndex];
      (*aFunc)(cur->mRule, cur->mSelector, aData);
      RuleValue *next = cur->mNext;
      mEnumList[valueIndex] = next ? next : mEnumList[--valueCount];
    }

    // Fast loop over single value.
    RuleValue* value = mEnumList[0];
    do {
      (*aFunc)(value->mRule, value->mSelector, aData);
      value = value->mNext;
    } while (value);
  }
}

void RuleHash::EnumerateTagRules(nsIAtom* aTag, RuleEnumFunc aFunc, void* aData)
{
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*,
      PL_DHashTableOperate(&mTagTable, aTag, PL_DHASH_LOOKUP));

  RULE_HASH_STAT_INCREMENT(mPseudosMatched);
  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    RuleValue *tagValue = entry->mRules;
    do {
      RULE_HASH_STAT_INCREMENT(mPseudoTagCalls);
      (*aFunc)(tagValue->mRule, tagValue->mSelector, aData);
      tagValue = tagValue->mNext;
    } while (tagValue);
  }
}

//--------------------------------

// Attribute selectors hash table.
struct AttributeSelectorEntry : public PLDHashEntryHdr {
  nsIAtom *mAttribute;
  nsVoidArray *mSelectors;
};

PR_STATIC_CALLBACK(void)
AttributeSelectorClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  AttributeSelectorEntry *entry = NS_STATIC_CAST(AttributeSelectorEntry*, hdr);
  delete entry->mSelectors;
  memset(entry, 0, table->entrySize);
}

static const PLDHashTableOps AttributeSelectorOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  AttributeSelectorClearEntry,
  PL_DHashFinalizeStub,
  NULL
};


//--------------------------------

struct RuleCascadeData {
  RuleCascadeData(nsIAtom *aMedium, PRBool aQuirksMode)
    : mRuleHash(aQuirksMode),
      mStateSelectors(),
      mMedium(aMedium),
      mNext(nsnull)
  {
    PL_DHashTableInit(&mAttributeSelectors, &AttributeSelectorOps, nsnull,
                      sizeof(AttributeSelectorEntry), 16);
  }

  ~RuleCascadeData()
  {
    PL_DHashTableFinish(&mAttributeSelectors);
  }
  RuleHash          mRuleHash;
  nsVoidArray       mStateSelectors;
  nsVoidArray       mClassSelectors;
  nsVoidArray       mIDSelectors;
  PLDHashTable      mAttributeSelectors; // nsIAtom* -> nsVoidArray*

  // Looks up or creates the appropriate list in |mAttributeSelectors|.
  // Returns null only on allocation failure.
  nsVoidArray* AttributeListFor(nsIAtom* aAttribute);

  nsCOMPtr<nsIAtom> mMedium;
  RuleCascadeData*  mNext; // for a different medium
};

nsVoidArray*
RuleCascadeData::AttributeListFor(nsIAtom* aAttribute)
{
  AttributeSelectorEntry *entry = NS_STATIC_CAST(AttributeSelectorEntry*,
      PL_DHashTableOperate(&mAttributeSelectors, aAttribute, PL_DHASH_ADD));
  if (!entry)
    return nsnull;
  if (!entry->mSelectors) {
    if (!(entry->mSelectors = new nsVoidArray)) {
      PL_DHashTableRawRemove(&mAttributeSelectors, entry);
      return nsnull;
    }
    entry->mAttribute = aAttribute;
  }
  return entry->mSelectors;
}

// -------------------------------
// CSS Style rule processor implementation
//

nsCSSRuleProcessor::nsCSSRuleProcessor(const nsCOMArray<nsICSSStyleSheet>& aSheets)
  : mSheets(aSheets),
    mRuleCascades(nsnull)
{
  for (PRInt32 i = mSheets.Count() - 1; i >= 0; --i)
    mSheets[i]->AddRuleProcessor(this);
}

nsCSSRuleProcessor::~nsCSSRuleProcessor()
{
  for (PRInt32 i = mSheets.Count() - 1; i >= 0; --i)
    mSheets[i]->DropRuleProcessor(this);
  mSheets.Clear();
  ClearRuleCascades();
}

NS_IMPL_ISUPPORTS1(nsCSSRuleProcessor, nsIStyleRuleProcessor)

RuleProcessorData::RuleProcessorData(nsPresContext* aPresContext,
                                     nsIContent* aContent, 
                                     nsRuleWalker* aRuleWalker,
                                     nsCompatibility* aCompat /*= nsnull*/)
{
  MOZ_COUNT_CTOR(RuleProcessorData);

  NS_PRECONDITION(aPresContext, "null pointer");
  NS_ASSERTION(!aContent || aContent->IsNodeOfType(nsINode::eELEMENT),
               "non-element leaked into SelectorMatches");

  mPresContext = aPresContext;
  mContent = aContent;
  mParentContent = nsnull;
  mRuleWalker = aRuleWalker;
  mScopedRoot = nsnull;

  mContentTag = nsnull;
  mContentID = nsnull;
  mHasAttributes = PR_FALSE;
  mIsHTMLContent = PR_FALSE;
  mIsLink = PR_FALSE;
  mLinkState = eLinkState_Unknown;
  mEventState = 0;
  mNameSpaceID = kNameSpaceID_Unknown;
  mPreviousSiblingData = nsnull;
  mParentData = nsnull;
  mLanguage = nsnull;
  mClasses = nsnull;

  // get the compat. mode (unless it is provided)
  if (!aCompat) {
    mCompatMode = mPresContext->CompatibilityMode();
  } else {
    mCompatMode = *aCompat;
  }


  if (aContent) {
    // get the tag and parent
    mContentTag = aContent->Tag();
    mParentContent = aContent->GetParent();

    // get the event state
    mPresContext->EventStateManager()->GetContentState(aContent, mEventState);

    // get the ID and classes for the content
    mContentID = aContent->GetID();
    mClasses = aContent->GetClasses();

    // see if there are attributes for the content
    mHasAttributes = aContent->GetAttrCount() > 0;

    // check for HTMLContent and Link status
    if (aContent->IsNodeOfType(nsINode::eHTML)) {
      mIsHTMLContent = PR_TRUE;
      // Note that we want to treat non-XML HTML content as XHTML for namespace
      // purposes, since html.css has that namespace declared.
      mNameSpaceID = kNameSpaceID_XHTML;
    } else {
      // get the namespace
      mNameSpaceID = aContent->GetNameSpaceID();
    }



    // if HTML content and it has some attributes, check for an HTML link
    // NOTE: optimization: cannot be a link if no attributes (since it needs an href)
    if (mIsHTMLContent && mHasAttributes) {
      // check if it is an HTML Link
      if(nsStyleUtil::IsHTMLLink(aContent, mContentTag, mPresContext, &mLinkState)) {
        mIsLink = PR_TRUE;
      }
    } 

    // if not an HTML link, check for a simple xlink (cannot be both HTML link and xlink)
    // NOTE: optimization: cannot be an XLink if no attributes (since it needs an 
    if(!mIsLink &&
       mHasAttributes && 
       !(mIsHTMLContent || aContent->IsNodeOfType(nsINode::eXUL)) && 
       nsStyleUtil::IsLink(aContent, mPresContext, &mLinkState)) {
      mIsLink = PR_TRUE;
    } 
  }
}

RuleProcessorData::~RuleProcessorData()
{
  MOZ_COUNT_DTOR(RuleProcessorData);

  // Destroy potentially long chains of previous sibling and parent data
  // without more than one level of recursion.
  if (mPreviousSiblingData || mParentData) {
    nsAutoVoidArray destroyQueue;
    destroyQueue.AppendElement(this);

    do {
      RuleProcessorData *d = NS_STATIC_CAST(RuleProcessorData*,
        destroyQueue.FastElementAt(destroyQueue.Count() - 1));
      destroyQueue.RemoveElementAt(destroyQueue.Count() - 1);

      if (d->mPreviousSiblingData) {
        destroyQueue.AppendElement(d->mPreviousSiblingData);
        d->mPreviousSiblingData = nsnull;
      }
      if (d->mParentData) {
        destroyQueue.AppendElement(d->mParentData);
        d->mParentData = nsnull;
      }

      if (d != this)
        d->Destroy(mPresContext);
    } while (destroyQueue.Count());
  }

  delete mLanguage;
}

const nsString* RuleProcessorData::GetLang()
{
  if (!mLanguage) {
    mLanguage = new nsAutoString();
    if (!mLanguage)
      return nsnull;
    for (nsIContent* content = mContent; content;
         content = content->GetParent()) {
      if (content->GetAttrCount() > 0) {
        // xml:lang has precedence over lang on HTML elements (see
        // XHTML1 section C.7).
        nsAutoString value;
        PRBool hasAttr = content->GetAttr(kNameSpaceID_XML, nsGkAtoms::lang,
                                          value);
        if (!hasAttr && content->IsNodeOfType(nsINode::eHTML)) {
          hasAttr = content->GetAttr(kNameSpaceID_None, nsGkAtoms::lang,
                                     value);
        }
        if (hasAttr) {
          *mLanguage = value;
          break;
        }
      }
    }
  }
  return mLanguage;
}

static const PRUnichar kNullCh = PRUnichar('\0');

static PRBool ValueIncludes(const nsSubstring& aValueList,
                            const nsSubstring& aValue,
                            const nsStringComparator& aComparator)
{
  const PRUnichar *p = aValueList.BeginReading(),
              *p_end = aValueList.EndReading();

  while (p < p_end) {
    // skip leading space
    while (p != p_end && nsCRT::IsAsciiSpace(*p))
      ++p;

    const PRUnichar *val_start = p;

    // look for space or end
    while (p != p_end && !nsCRT::IsAsciiSpace(*p))
      ++p;

    const PRUnichar *val_end = p;

    if (val_start < val_end &&
        aValue.Equals(Substring(val_start, val_end), aComparator))
      return PR_TRUE;

    ++p; // we know the next character is not whitespace
  }
  return PR_FALSE;
}

inline PRBool IsLinkPseudo(nsIAtom* aAtom)
{
  return PRBool ((nsCSSPseudoClasses::link == aAtom) || 
                 (nsCSSPseudoClasses::visited == aAtom) ||
                 (nsCSSPseudoClasses::mozAnyLink == aAtom));
}

// Return whether we should apply a "global" (i.e., universal-tag)
// selector for event states in quirks mode.  Note that
// |data.mIsLink| is checked separately by the caller, so we return
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


static PRBool IsSignificantChild(nsIContent* aChild, PRBool aTextIsSignificant, PRBool aWhitespaceIsSignificant)
{
  NS_ASSERTION(!aWhitespaceIsSignificant || aTextIsSignificant,
               "Nonsensical arguments");

  PRBool isText = aChild->IsNodeOfType(nsINode::eTEXT);

  if (!isText && !aChild->IsNodeOfType(nsINode::eCOMMENT) &&
      !aChild->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
    return PR_TRUE;
  }

  return aTextIsSignificant && isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant ||
          !aChild->TextIsOnlyWhitespace());
}

// This function is to be called once we have fetched a value for an attribute
// whose namespace and name match those of aAttrSelector.  This function
// performs comparisons on the value only, based on aAttrSelector->mFunction.
static PRBool AttrMatchesValue(const nsAttrSelector* aAttrSelector,
                               const nsString& aValue)
{
  NS_PRECONDITION(aAttrSelector, "Must have an attribute selector");
  const nsDefaultStringComparator defaultComparator;
  const nsCaseInsensitiveStringComparator ciComparator;
  const nsStringComparator& comparator = aAttrSelector->mCaseSensitive
                ? NS_STATIC_CAST(const nsStringComparator&, defaultComparator)
                : NS_STATIC_CAST(const nsStringComparator&, ciComparator);
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

// NOTE:  The |aStateMask| code isn't going to work correctly anymore if
// we start batching style changes, because if multiple states change in
// separate notifications then we might determine the style is not
// state-dependent when it really is (e.g., determining that a
// :hover:active rule no longer matches when both states are unset).
// XXXldb This is a real problem for things like [checked]:checked where
// both states are determined exactly by an attribute.

// |aDependence| has two functions:
//  * when non-null, it indicates that we're processing a negation,
//    which is done only when SelectorMatches calls itself recursively
//  * what it points to should be set to true whenever a test is skipped
//    because of aStateMask or aAttribute
static PRBool SelectorMatches(RuleProcessorData &data,
                              nsCSSSelector* aSelector,
                              PRInt32 aStateMask, // states NOT to test
                              nsIAtom* aAttribute, // attribute NOT to test
                              PRBool* const aDependence = nsnull) 

{
  // namespace/tag match
  if ((kNameSpaceID_Unknown != aSelector->mNameSpace &&
       data.mNameSpaceID != aSelector->mNameSpace) ||
      (aSelector->mTag && aSelector->mTag != data.mContentTag)) {
    // optimization : bail out early if we can
    return PR_FALSE;
  }

  PRBool result = PR_TRUE;
  const PRBool isNegated = (aDependence != nsnull);

  // test for pseudo class match
  // first-child, root, lang, active, focus, hover, link, visited...
  // XXX disabled, enabled, selected, selection
  for (nsAtomStringList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass && result; pseudoClass = pseudoClass->mNext) {
    PRInt32 stateToCheck = 0;
    if ((nsCSSPseudoClasses::firstChild == pseudoClass->mAtom) ||
        (nsCSSPseudoClasses::firstNode == pseudoClass->mAtom) ) {
      nsIContent *firstChild = nsnull;
      nsIContent *parent = data.mParentContent;
      if (parent) {
        PRBool acceptNonWhitespace =
          nsCSSPseudoClasses::firstNode == pseudoClass->mAtom;
        PRInt32 index = -1;
        do {
          firstChild = parent->GetChildAt(++index);
          // stop at first non-comment and non-whitespace node (and
          // non-text node for firstChild)
        } while (firstChild &&
                 !IsSignificantChild(firstChild, acceptNonWhitespace, PR_FALSE));
      }
      result = (data.mContent == firstChild);
    }
    else if ((nsCSSPseudoClasses::lastChild == pseudoClass->mAtom) ||
             (nsCSSPseudoClasses::lastNode == pseudoClass->mAtom)) {
      nsIContent *lastChild = nsnull;
      nsIContent *parent = data.mParentContent;
      if (parent) {
        PRBool acceptNonWhitespace =
          nsCSSPseudoClasses::lastNode == pseudoClass->mAtom;
        PRUint32 index = parent->GetChildCount();
        do {
          lastChild = parent->GetChildAt(--index);
          // stop at first non-comment and non-whitespace node (and
          // non-text node for lastChild)
        } while (lastChild &&
                 !IsSignificantChild(lastChild, acceptNonWhitespace, PR_FALSE));
      }
      result = (data.mContent == lastChild);
    }
    else if (nsCSSPseudoClasses::onlyChild == pseudoClass->mAtom) {
      nsIContent *onlyChild = nsnull;
      nsIContent *moreChild = nsnull;
      nsIContent *parent = data.mParentContent;
      if (parent) {
        PRInt32 index = -1;
        do {
          onlyChild = parent->GetChildAt(++index);
          // stop at first non-comment, non-whitespace and non-text node
        } while (onlyChild &&
                 !IsSignificantChild(onlyChild, PR_FALSE, PR_FALSE));
        if (data.mContent == onlyChild) {
          // see if there's any more
          do {
            moreChild = parent->GetChildAt(++index);
          } while (moreChild && !IsSignificantChild(moreChild, PR_FALSE, PR_FALSE));
        }
      }
      result = (data.mContent == onlyChild && moreChild == nsnull);
    }
    else if (nsCSSPseudoClasses::empty == pseudoClass->mAtom ||
             nsCSSPseudoClasses::mozOnlyWhitespace == pseudoClass->mAtom) {
      nsIContent *child = nsnull;
      nsIContent *element = data.mContent;
      const PRBool isWhitespaceSignificant =
        nsCSSPseudoClasses::empty == pseudoClass->mAtom;
      PRInt32 index = -1;

      do {
        child = element->GetChildAt(++index);
        // stop at first non-comment (and non-whitespace for
        // :-moz-only-whitespace) node        
      } while (child && !IsSignificantChild(child, PR_TRUE, isWhitespaceSignificant));
      result = (child == nsnull);
    }
    else if (nsCSSPseudoClasses::mozEmptyExceptChildrenWithLocalname == pseudoClass->mAtom) {
      NS_ASSERTION(pseudoClass->mString, "Must have string!");
      nsIContent *child = nsnull;
      nsIContent *element = data.mContent;
      PRInt32 index = -1;

      do {
        child = element->GetChildAt(++index);
      } while (child &&
               (!IsSignificantChild(child, PR_TRUE, PR_FALSE) ||
                (child->GetNameSpaceID() == element->GetNameSpaceID() &&
                 child->Tag()->Equals(nsDependentString(pseudoClass->mString)))));
      result = (child == nsnull);
    }
    else if (nsCSSPseudoClasses::mozHasHandlerRef == pseudoClass->mAtom) {
      nsIContent *child = nsnull;
      nsIContent *element = data.mContent;
      PRInt32 index = -1;

      result = PR_FALSE;
      if (element) {
        do {
          child = element->GetChildAt(++index);
          if (child && child->IsNodeOfType(nsINode::eHTML) &&
              child->Tag() == nsGkAtoms::param &&
              child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                 NS_LITERAL_STRING("pluginurl"), eIgnoreCase)) {
            result = PR_TRUE;
            break;
          }
        } while (child);
      }
    }
    else if (nsCSSPseudoClasses::root == pseudoClass->mAtom) {
      result = (data.mParentContent == nsnull);
    }
    else if (nsCSSPseudoClasses::mozBoundElement == pseudoClass->mAtom) {
      // XXXldb How do we know where the selector came from?  And what
      // if there are multiple bindings, and we should be matching the
      // outer one?
      result = (data.mScopedRoot && data.mScopedRoot == data.mContent);
    }
    else if (nsCSSPseudoClasses::lang == pseudoClass->mAtom) {
      NS_ASSERTION(nsnull != pseudoClass->mString, "null lang parameter");
      result = PR_FALSE;
      if (pseudoClass->mString && *pseudoClass->mString) {
        // We have to determine the language of the current element.  Since
        // this is currently no property and since the language is inherited
        // from the parent we have to be prepared to look at all parent
        // nodes.  The language itself is encoded in the LANG attribute.
        const nsString* lang = data.GetLang();
        if (lang && !lang->IsEmpty()) { // null check for out-of-memory
          result = nsStyleUtil::DashMatchCompare(*lang,
                                    nsDependentString(pseudoClass->mString), 
                                    nsCaseInsensitiveStringComparator());
        }
        else if (data.mContent) {
          nsIDocument* doc = data.mContent->GetDocument();
          if (doc) {
            // Try to get the language from the HTTP header or if this
            // is missing as well from the preferences.
            // The content language can be a comma-separated list of
            // language codes.
            nsAutoString language;
            doc->GetContentLanguage(language);

            nsDependentString langString(pseudoClass->mString);
            language.StripWhitespace();
            PRInt32 begin = 0;
            PRInt32 len = language.Length();
            while (begin < len) {
              PRInt32 end = language.FindChar(PRUnichar(','), begin);
              if (end == kNotFound) {
                end = len;
              }
              if (nsStyleUtil::DashMatchCompare(Substring(language, begin, end-begin),
                                   langString,
                                   nsCaseInsensitiveStringComparator())) {
                result = PR_TRUE;
                break;
              }
              begin = end + 1;
            }
          }
        }
      }
    } else if (nsCSSPseudoClasses::active == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_ACTIVE;
    }
    else if (nsCSSPseudoClasses::focus == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_FOCUS;
    }
    else if (nsCSSPseudoClasses::hover == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_HOVER;
    }
    else if (nsCSSPseudoClasses::mozDragOver == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_DRAGOVER;
    }
    else if (nsCSSPseudoClasses::target == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_URLTARGET;
    }
    else if (IsLinkPseudo(pseudoClass->mAtom)) {
      if (data.mIsLink) {
        if (nsCSSPseudoClasses::mozAnyLink == pseudoClass->mAtom) {
          result = PR_TRUE;
        }
        else {
          NS_ASSERTION(nsCSSPseudoClasses::link == pseudoClass->mAtom ||
                       nsCSSPseudoClasses::visited == pseudoClass->mAtom,
                       "somebody changed IsLinkPseudo");
          NS_ASSERTION(data.mLinkState == eLinkState_Unvisited ||
                       data.mLinkState == eLinkState_Visited,
                       "unexpected link state for mIsLink");
          if (aStateMask & NS_EVENT_STATE_VISITED) {
            result = PR_TRUE;
            if (aDependence)
              *aDependence = PR_TRUE;
          } else {
            result = ((eLinkState_Unvisited == data.mLinkState) ==
                      (nsCSSPseudoClasses::link == pseudoClass->mAtom));
          }
        }
      }
      else {
        result = PR_FALSE;  // not a link
      }
    }
    else if (nsCSSPseudoClasses::checked == pseudoClass->mAtom) {
      // This pseudoclass matches the selected state on the following elements:
      //  <option>
      //  <input type=checkbox>
      //  <input type=radio>
      stateToCheck = NS_EVENT_STATE_CHECKED;
    }
    else if (nsCSSPseudoClasses::enabled == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_ENABLED;
    }
    else if (nsCSSPseudoClasses::disabled == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_DISABLED;
    }    
    else if (nsCSSPseudoClasses::mozBroken == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_BROKEN;
    }
    else if (nsCSSPseudoClasses::mozUserDisabled == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_USERDISABLED;
    }
    else if (nsCSSPseudoClasses::mozSuppressed == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_SUPPRESSED;
    }
    else if (nsCSSPseudoClasses::mozLoading == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_LOADING;
    }
    else if (nsCSSPseudoClasses::mozTypeUnsupported == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_TYPE_UNSUPPORTED;
    }
    else if (nsCSSPseudoClasses::defaultPseudo == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_DEFAULT;
    }
    else if (nsCSSPseudoClasses::required == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_REQUIRED;
    }
    else if (nsCSSPseudoClasses::optional == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_OPTIONAL;
    }
    else if (nsCSSPseudoClasses::valid == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_VALID;
    }
    else if (nsCSSPseudoClasses::invalid == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_INVALID;
    }
    else if (nsCSSPseudoClasses::inRange == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_INRANGE;
    }
    else if (nsCSSPseudoClasses::outOfRange == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_OUTOFRANGE;
    }
    else if (nsCSSPseudoClasses::mozReadOnly == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_MOZ_READONLY;
    }
    else if (nsCSSPseudoClasses::mozReadWrite == pseudoClass->mAtom) {
      stateToCheck = NS_EVENT_STATE_MOZ_READWRITE;
    }
    else if (nsCSSPseudoClasses::mozIsHTML == pseudoClass->mAtom) {
      result = data.mIsHTMLContent &&
        data.mContent->GetNameSpaceID() == kNameSpaceID_None;
    }
    else {
      NS_ERROR("CSS parser parsed a pseudo-class that we do not handle");
      result = PR_FALSE;  // unknown pseudo class
    }
    if (stateToCheck) {
      // check if the element is event-sensitive for :hover and :active
      if ((stateToCheck & (NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE)) &&
          data.mCompatMode == eCompatibility_NavQuirks &&
          // global selector (but don't check .class):
          !aSelector->mTag && !aSelector->mIDList && !aSelector->mAttrList &&
          // This (or the other way around) both make :not() asymmetric
          // in quirks mode (and it's hard to work around since we're
          // testing the current mNegations, not the first
          // (unnegated)). This at least makes it closer to the spec.
          !isNegated &&
          // important for |IsQuirkEventSensitive|:
          data.mIsHTMLContent && !data.mIsLink &&
          !IsQuirkEventSensitive(data.mContentTag)) {
        // In quirks mode, only make certain elements sensitive to
        // selectors ":hover" and ":active".
        result = PR_FALSE;
      } else {
        if (aStateMask & stateToCheck) {
          result = PR_TRUE;
          if (aDependence)
            *aDependence = PR_TRUE;
        } else {
          result = (0 != (data.mEventState & stateToCheck));
        }
      }
    }
  }

  if (result && aSelector->mAttrList) {
    // test for attribute match
    if (!data.mHasAttributes && !aAttribute) {
      // if no attributes on the content, no match
      result = PR_FALSE;
    } else {
      NS_ASSERTION(data.mContent,
                   "Must have content if either data.mHasAttributes or "
                   "aAttribute is set!");
      result = PR_TRUE;
      nsAttrSelector* attr = aSelector->mAttrList;
      do {
        if (attr->mAttr == aAttribute) {
          // XXX we should really have a namespace, not just an attr
          // name, in HasAttributeDependentStyle!
          result = PR_TRUE;
          if (aDependence)
            *aDependence = PR_TRUE;
        }
        else if (attr->mNameSpace == kNameSpaceID_Unknown) {
          // Attr selector with a wildcard namespace.  We have to examine all
          // the attributes on our content node....  This sort of selector is
          // essentially a boolean OR, over all namespaces, of equivalent attr
          // selectors with those namespaces.  So to evaluate whether it
          // matches, evaluate for each namespace (the only namespaces that
          // have a chance at matching, of course, are ones that the element
          // actually has attributes in), short-circuiting if we ever match.
          PRUint32 attrCount = data.mContent->GetAttrCount();
          result = PR_FALSE;
          for (PRUint32 i = 0; i < attrCount; ++i) {
            const nsAttrName* attrName =
              data.mContent->GetAttrNameAt(i);
            NS_ASSERTION(attrName, "GetAttrCount lied or GetAttrNameAt failed");
            if (attrName->LocalName() != attr->mAttr) {
              continue;
            }
            if (attr->mFunction == NS_ATTR_FUNC_SET) {
              result = PR_TRUE;
            } else {
              nsAutoString value;
#ifdef DEBUG
              PRBool hasAttr =
#endif
                data.mContent->GetAttr(attrName->NamespaceID(),
                                       attrName->LocalName(), value);
              NS_ASSERTION(hasAttr, "GetAttrNameAt lied");
              result = AttrMatchesValue(attr, value);
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
            data.mContent->
              AttrValueIs(attr->mNameSpace, attr->mAttr, attr->mValue,
                          attr->mCaseSensitive ? eCaseMatters : eIgnoreCase);
        }
        else if (!data.mContent->HasAttr(attr->mNameSpace, attr->mAttr)) {
          result = PR_FALSE;
        }
        else if (attr->mFunction != NS_ATTR_FUNC_SET) {
          nsAutoString value;
#ifdef DEBUG
          PRBool hasAttr =
#endif
              data.mContent->GetAttr(attr->mNameSpace, attr->mAttr, value);
          NS_ASSERTION(hasAttr, "HasAttr lied");
          result = AttrMatchesValue(attr, value);
        }
        
        attr = attr->mNext;
      } while (attr && result);
    }
  }
  nsAtomList* IDList = aSelector->mIDList;
  if (result && IDList) {
    // test for ID match
    result = PR_FALSE;

    if (aAttribute && aAttribute == data.mContent->GetIDAttributeName()) {
      result = PR_TRUE;
      if (aDependence)
        *aDependence = PR_TRUE;
    }
    else if (nsnull != data.mContentID) {
      // case sensitivity: bug 93371
      const PRBool isCaseSensitive =
        data.mCompatMode != eCompatibility_NavQuirks;

      result = PR_TRUE;
      if (isCaseSensitive) {
        do {
          if (IDList->mAtom != data.mContentID) {
            result = PR_FALSE;
            break;
          }
          IDList = IDList->mNext;
        } while (IDList);
      } else {
        const char* id1Str;
        data.mContentID->GetUTF8String(&id1Str);
        nsDependentCString id1(id1Str);
        do {
          const char* id2Str;
          IDList->mAtom->GetUTF8String(&id2Str);
          if (!id1.Equals(id2Str, nsCaseInsensitiveCStringComparator())) {
            result = PR_FALSE;
            break;
          }
          IDList = IDList->mNext;
        } while (IDList);
      }
    }
  }
    
  if (result && aSelector->mClassList) {
    // test for class match
    if (aAttribute && aAttribute == data.mContent->GetClassAttributeName()) {
      result = PR_TRUE;
      if (aDependence)
        *aDependence = PR_TRUE;
    }
    else {
      // case sensitivity: bug 93371
      const PRBool isCaseSensitive =
        data.mCompatMode != eCompatibility_NavQuirks;

      nsAtomList* classList = aSelector->mClassList;
      const nsAttrValue *elementClasses = data.mClasses;
      while (nsnull != classList) {
        if (!(elementClasses && elementClasses->Contains(classList->mAtom, isCaseSensitive ? eCaseMatters : eIgnoreCase))) {
          result = PR_FALSE;
          break;
        }
        classList = classList->mNext;
      }
    }
  }
  
  // apply SelectorMatches to the negated selectors in the chain
  if (!isNegated) {
    for (nsCSSSelector *negation = aSelector->mNegations;
         result && negation; negation = negation->mNegations) {
      PRBool dependence = PR_FALSE;
      result = !SelectorMatches(data, negation, aStateMask,
                                aAttribute, &dependence);
      // If the selector does match due to the dependence on aStateMask
      // or aAttribute, then we want to keep result true so that the
      // final result of SelectorMatches is true.  Doing so tells
      // StateEnumFunc or AttributeEnumFunc that there is a dependence
      // on the state or attribute.
      result = result || dependence;
    }
  }
  return result;
}

#undef STATE_CHECK

// Right now, there are four operators:
//   PRUnichar(0), the descendant combinator, is greedy
//   '~', the indirect adjacent sibling combinator, is greedy
//   '+' and '>', the direct adjacent sibling and child combinators, are not
#define NS_IS_GREEDY_OPERATOR(ch) (ch == PRUnichar(0) || ch == PRUnichar('~'))

static PRBool SelectorMatchesTree(RuleProcessorData& aPrevData,
                                  nsCSSSelector* aSelector) 
{
  nsCSSSelector* selector = aSelector;
  RuleProcessorData* prevdata = &aPrevData;
  while (selector) { // check compound selectors
    // If we don't already have a RuleProcessorData for the next
    // appropriate content (whether parent or previous sibling), create
    // one.

    // for adjacent sibling combinators, the content to test against the
    // selector is the previous sibling *element*
    RuleProcessorData* data;
    if (PRUnichar('+') == selector->mOperator ||
        PRUnichar('~') == selector->mOperator) {
      data = prevdata->mPreviousSiblingData;
      if (!data) {
        nsIContent* content = prevdata->mContent;
        nsIContent* parent = content->GetParent();
        if (parent) {
          PRInt32 index = parent->IndexOf(content);
          while (0 <= --index) {
            content = parent->GetChildAt(index);
            if (content->IsNodeOfType(nsINode::eELEMENT)) {
              data = new (prevdata->mPresContext)
                          RuleProcessorData(prevdata->mPresContext, content,
                                            prevdata->mRuleWalker,
                                            &prevdata->mCompatMode);
              prevdata->mPreviousSiblingData = data;    
              break;
            }
          }
        }
      }
    }
    // for descendant combinators and child combinators, the content
    // to test against is the parent
    else {
      data = prevdata->mParentData;
      if (!data) {
        nsIContent *content = prevdata->mContent->GetParent();
        if (content) {
          data = new (prevdata->mPresContext)
                      RuleProcessorData(prevdata->mPresContext, content,
                                        prevdata->mRuleWalker,
                                        &prevdata->mCompatMode);
          prevdata->mParentData = data;    
        }
      }
    }
    if (! data) {
      return PR_FALSE;
    }
    if (SelectorMatches(*data, selector, 0, nsnull)) {
      // to avoid greedy matching, we need to recur if this is a
      // descendant combinator and the next combinator is not
      if ((NS_IS_GREEDY_OPERATOR(selector->mOperator)) &&
          (selector->mNext) &&
          (!NS_IS_GREEDY_OPERATOR(selector->mNext->mOperator))) {

        // pretend the selector didn't match, and step through content
        // while testing the same selector

        // This approach is slightly strange in that when it recurs
        // it tests from the top of the content tree, down.  This
        // doesn't matter much for performance since most selectors
        // don't match.  (If most did, it might be faster...)
        if (SelectorMatchesTree(*data, selector)) {
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
    prevdata = data;
  }
  return PR_TRUE; // all the selectors matched.
}

static void ContentEnumFunc(nsICSSStyleRule* aRule, nsCSSSelector* aSelector,
                            void* aData)
{
  ElementRuleProcessorData* data = (ElementRuleProcessorData*)aData;

  if (SelectorMatches(*data, aSelector, 0, nsnull)) {
    nsCSSSelector *next = aSelector->mNext;
    if (!next || SelectorMatchesTree(*data, next)) {
      // for performance, require that every implementation of
      // nsICSSStyleRule return the same pointer for nsIStyleRule (why
      // would anything multiply inherit nsIStyleRule anyway?)
#ifdef DEBUG
      nsCOMPtr<nsIStyleRule> iRule = do_QueryInterface(aRule);
      NS_ASSERTION(NS_STATIC_CAST(nsIStyleRule*, aRule) == iRule.get(),
                   "Please fix QI so this performance optimization is valid");
#endif
      data->mRuleWalker->Forward(NS_STATIC_CAST(nsIStyleRule*, aRule));
      // nsStyleSet will deal with the !important rule
    }
  }
}

NS_IMETHODIMP
nsCSSRuleProcessor::RulesMatching(ElementRuleProcessorData *aData)
{
  NS_PRECONDITION(aData->mContent->IsNodeOfType(nsINode::eELEMENT),
                  "content must be element");

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade) {
    cascade->mRuleHash.EnumerateAllRules(aData->mNameSpaceID,
                                         aData->mContentTag,
                                         aData->mContentID,
                                         aData->mClasses,
                                         ContentEnumFunc,
                                         aData);
  }
  return NS_OK;
}

static void PseudoEnumFunc(nsICSSStyleRule* aRule, nsCSSSelector* aSelector,
                           void* aData)
{
  PseudoRuleProcessorData* data = (PseudoRuleProcessorData*)aData;

  NS_ASSERTION(aSelector->mTag == data->mPseudoTag, "RuleHash failure");
  PRBool matches = PR_TRUE;
  if (data->mComparator)
    data->mComparator->PseudoMatches(data->mPseudoTag, aSelector, &matches);

  if (matches) {
    nsCSSSelector *selector = aSelector->mNext;

    if (selector) { // test next selector specially
      if (PRUnichar('+') == selector->mOperator) {
        return; // not valid here, can't match
      }
      if (SelectorMatches(*data, selector, 0, nsnull)) {
        selector = selector->mNext;
      }
      else {
        if (PRUnichar('>') == selector->mOperator) {
          return; // immediate parent didn't match
        }
      }
    }

    if (selector && 
        (! SelectorMatchesTree(*data, selector))) {
      return; // remaining selectors didn't match
    }

    // for performance, require that every implementation of
    // nsICSSStyleRule return the same pointer for nsIStyleRule (why
    // would anything multiply inherit nsIStyleRule anyway?)
#ifdef DEBUG
    nsCOMPtr<nsIStyleRule> iRule = do_QueryInterface(aRule);
    NS_ASSERTION(NS_STATIC_CAST(nsIStyleRule*, aRule) == iRule.get(),
                 "Please fix QI so this performance optimization is valid");
#endif
    data->mRuleWalker->Forward(NS_STATIC_CAST(nsIStyleRule*, aRule));
    // nsStyleSet will deal with the !important rule
  }
}

NS_IMETHODIMP
nsCSSRuleProcessor::RulesMatching(PseudoRuleProcessorData* aData)
{
  NS_PRECONDITION(!aData->mContent ||
                  aData->mContent->IsNodeOfType(nsINode::eELEMENT),
                  "content (if present) must be element");

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade) {
    cascade->mRuleHash.EnumerateTagRules(aData->mPseudoTag,
                                         PseudoEnumFunc, aData);
  }
  return NS_OK;
}

inline PRBool
IsSiblingOperator(PRUnichar oper)
{
  return oper == PRUnichar('+') || oper == PRUnichar('~');
}

struct StateEnumData {
  StateEnumData(StateRuleProcessorData *aData)
    : data(aData), change(nsReStyleHint(0)) {}

  StateRuleProcessorData *data;
  nsReStyleHint change;
};

PR_STATIC_CALLBACK(PRBool) StateEnumFunc(void* aSelector, void* aData)
{
  StateEnumData *enumData = NS_STATIC_CAST(StateEnumData*, aData);
  StateRuleProcessorData *data = enumData->data;
  nsCSSSelector* selector = NS_STATIC_CAST(nsCSSSelector*, aSelector);

  nsReStyleHint possibleChange = IsSiblingOperator(selector->mOperator) ?
    eReStyle_LaterSiblings : eReStyle_Self;

  // If enumData->change already includes all the bits of possibleChange, don't
  // bother calling SelectorMatches, since even if it returns false
  // enumData->change won't change.
  if ((possibleChange & ~(enumData->change)) &&
      SelectorMatches(*data, selector, data->mStateMask, nsnull) &&
      SelectorMatchesTree(*data, selector->mNext)) {
    enumData->change = nsReStyleHint(enumData->change | possibleChange);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsCSSRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData,
                                           nsReStyleHint* aResult)
{
  NS_PRECONDITION(aData->mContent->IsNodeOfType(nsINode::eELEMENT),
                  "content must be element");

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  // Look up the content node in the state rule list, which points to
  // any (CSS2 definition) simple selector (whether or not it is the
  // subject) that has a state pseudo-class on it.  This means that this
  // code will be matching selectors that aren't real selectors in any
  // stylesheet (e.g., if there is a selector "body > p:hover > a", then
  // "body > p:hover" will be in |cascade->mStateSelectors|).  Note that
  // |IsStateSelector| below determines which selectors are in
  // |cascade->mStateSelectors|.
  StateEnumData data(aData);
  if (cascade)
    cascade->mStateSelectors.EnumerateForwards(StateEnumFunc, &data);
  *aResult = data.change;
  return NS_OK;
}

struct AttributeEnumData {
  AttributeEnumData(AttributeRuleProcessorData *aData)
    : data(aData), change(nsReStyleHint(0)) {}

  AttributeRuleProcessorData *data;
  nsReStyleHint change;
};


PR_STATIC_CALLBACK(PRBool) AttributeEnumFunc(void* aSelector, void* aData)
{
  AttributeEnumData *enumData = NS_STATIC_CAST(AttributeEnumData*, aData);
  AttributeRuleProcessorData *data = enumData->data;
  nsCSSSelector* selector = NS_STATIC_CAST(nsCSSSelector*, aSelector);

  nsReStyleHint possibleChange = IsSiblingOperator(selector->mOperator) ?
    eReStyle_LaterSiblings : eReStyle_Self;

  // If enumData->change already includes all the bits of possibleChange, don't
  // bother calling SelectorMatches, since even if it returns false
  // enumData->change won't change.
  if ((possibleChange & ~(enumData->change)) &&
      SelectorMatches(*data, selector, 0, data->mAttribute) &&
      SelectorMatchesTree(*data, selector->mNext)) {
    enumData->change = nsReStyleHint(enumData->change | possibleChange);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsCSSRuleProcessor::HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                               nsReStyleHint* aResult)
{
  NS_PRECONDITION(aData->mContent->IsNodeOfType(nsINode::eELEMENT),
                  "content must be element");

  AttributeEnumData data(aData);

  // Since we always have :-moz-any-link (and almost always have :link
  // and :visited rules from prefs), rather than hacking AddRule below
  // to add |href| to the hash, we'll just handle it here.
  if (aData->mAttribute == nsGkAtoms::href &&
      aData->mIsHTMLContent &&
      (aData->mContentTag == nsGkAtoms::a ||
       aData->mContentTag == nsGkAtoms::area ||
       aData->mContentTag == nsGkAtoms::link)) {
    data.change = nsReStyleHint(data.change | eReStyle_Self);
  }
  // XXX What about XLinks?
  // XXXbz now that :link and :visited are also states, do we need a
  // similar optimization in HasStateDependentStyle?

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  // We do the same thing for attributes that we do for state selectors
  // (see HasStateDependentStyle), except that instead of one big list
  // we have a hashtable with a per-attribute list.

  if (cascade) {
    if (aData->mAttribute == aData->mContent->GetIDAttributeName()) {
      cascade->mIDSelectors.EnumerateForwards(AttributeEnumFunc, &data);
    }
    
    if (aData->mAttribute == aData->mContent->GetClassAttributeName()) {
      cascade->mClassSelectors.EnumerateForwards(AttributeEnumFunc, &data);
    }

    AttributeSelectorEntry *entry = NS_STATIC_CAST(AttributeSelectorEntry*,
        PL_DHashTableOperate(&cascade->mAttributeSelectors, aData->mAttribute,
                             PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      entry->mSelectors->EnumerateForwards(AttributeEnumFunc, &data);
    }
  }

  *aResult = data.change;
  return NS_OK;
}

nsresult
nsCSSRuleProcessor::ClearRuleCascades()
{
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
  for (nsAtomStringList* pseudoClass = aSelector.mPseudoClassList;
       pseudoClass; pseudoClass = pseudoClass->mNext) {
    if ((pseudoClass->mAtom == nsCSSPseudoClasses::active) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::checked) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozDragOver) || 
        (pseudoClass->mAtom == nsCSSPseudoClasses::focus) || 
        (pseudoClass->mAtom == nsCSSPseudoClasses::hover) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::target) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::link) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::visited) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::enabled) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::disabled) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozBroken) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozUserDisabled) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozSuppressed) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozLoading) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::required) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::optional) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::valid) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::invalid) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::inRange) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::outOfRange) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozReadOnly) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::mozReadWrite) ||
        (pseudoClass->mAtom == nsCSSPseudoClasses::defaultPseudo)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PR_STATIC_CALLBACK(PRBool)
AddRule(void* aRuleInfo, void* aCascade)
{
  RuleValue* ruleInfo = NS_STATIC_CAST(RuleValue*, aRuleInfo);
  RuleCascadeData *cascade = NS_STATIC_CAST(RuleCascadeData*, aCascade);

  // Build the rule hash.
  cascade->mRuleHash.PrependRule(ruleInfo);

  nsVoidArray* stateArray = &cascade->mStateSelectors;
  nsVoidArray* classArray = &cascade->mClassSelectors;
  nsVoidArray* idArray = &cascade->mIDSelectors;
  
  for (nsCSSSelector* selector = ruleInfo->mSelector;
           selector; selector = selector->mNext) {
    // It's worth noting that this loop over negations isn't quite
    // optimal for two reasons.  One, we could add something to one of
    // these lists twice, which means we'll check it twice, but I don't
    // think that's worth worrying about.   (We do the same for multiple
    // attribute selectors on the same attribute.)  Two, we don't really
    // need to check negations past the first in the current
    // implementation (and they're rare as well), but that might change
    // in the future if :not() is extended. 
    for (nsCSSSelector* negation = selector; negation;
         negation = negation->mNegations) {
      // Build mStateSelectors.
      if (IsStateSelector(*negation))
        stateArray->AppendElement(selector);

      // Build mIDSelectors
      if (negation->mIDList) {
        idArray->AppendElement(selector);
      }
      
      // Build mClassSelectors
      if (negation->mClassList) {
        classArray->AppendElement(selector);
      }

      // Build mAttributeSelectors.
      for (nsAttrSelector *attr = negation->mAttrList; attr;
           attr = attr->mNext) {
        nsVoidArray *array = cascade->AttributeListFor(attr->mAttr);
        if (!array)
          return PR_FALSE;
        array->AppendElement(selector);
      }
    }
  }

  return PR_TRUE;
}

PR_STATIC_CALLBACK(PRIntn)
RuleArraysDestroy(nsHashKey *aKey, void *aData, void *aClosure)
{
  delete NS_STATIC_CAST(nsAutoVoidArray*, aData);
  return PR_TRUE;
}

struct CascadeEnumData {
  CascadeEnumData(nsPresContext* aPresContext, PLArenaPool& aArena)
    : mPresContext(aPresContext),
      mRuleArrays(nsnull, nsnull, RuleArraysDestroy, nsnull, 64),
      mArena(aArena)
  {
  }

  nsPresContext* mPresContext;
  nsObjectHashtable mRuleArrays; // of nsAutoVoidArray
  PLArenaPool& mArena;
};

static PRBool
InsertRuleByWeight(nsICSSRule* aRule, void* aData)
{
  CascadeEnumData* data = (CascadeEnumData*)aData;
  PRInt32 type = nsICSSRule::UNKNOWN_RULE;
  aRule->GetType(type);

  if (nsICSSRule::STYLE_RULE == type) {
    nsICSSStyleRule* styleRule = (nsICSSStyleRule*)aRule;

    for (nsCSSSelectorList *sel = styleRule->Selector();
         sel; sel = sel->mNext) {
      PRInt32 weight = sel->mWeight;
      nsPRUint32Key key(weight);
      nsAutoVoidArray *rules =
        NS_STATIC_CAST(nsAutoVoidArray*, data->mRuleArrays.Get(&key));
      if (!rules) {
        rules = new nsAutoVoidArray();
        if (!rules) return PR_FALSE; // out of memory
        data->mRuleArrays.Put(&key, rules);
      }
      RuleValue *info =
        new (data->mArena) RuleValue(styleRule, sel->mSelectors);
      rules->AppendElement(info);
    }
  }
  else if (nsICSSRule::MEDIA_RULE == type ||
           nsICSSRule::DOCUMENT_RULE == type) {
    nsICSSGroupRule* groupRule = (nsICSSGroupRule*)aRule;
    if (groupRule->UseForPresentation(data->mPresContext))
      groupRule->EnumerateRulesForwards(InsertRuleByWeight, aData);
  }
  return PR_TRUE;
}


static PRBool
CascadeSheetRulesInto(nsICSSStyleSheet* aSheet, void* aData)
{
  nsCSSStyleSheet*  sheet = NS_STATIC_CAST(nsCSSStyleSheet*, aSheet);
  CascadeEnumData* data = NS_STATIC_CAST(CascadeEnumData*, aData);
  PRBool bSheetApplicable = PR_TRUE;
  sheet->GetApplicable(bSheetApplicable);

  if (bSheetApplicable && sheet->UseForMedium(data->mPresContext)) {
    nsCSSStyleSheet* child = sheet->mFirstChild;
    while (child) {
      CascadeSheetRulesInto(child, data);
      child = child->mNext;
    }

    if (sheet->mInner) {
      sheet->mInner->mOrderedRules.EnumerateForwards(InsertRuleByWeight, data);
    }
  }
  return PR_TRUE;
}

struct RuleArrayData {
  PRInt32 mWeight;
  nsVoidArray* mRuleArray;
};

PR_STATIC_CALLBACK(int) CompareArrayData(const void* aArg1, const void* aArg2,
                                         void* closure)
{
  const RuleArrayData* arg1 = NS_STATIC_CAST(const RuleArrayData*, aArg1);
  const RuleArrayData* arg2 = NS_STATIC_CAST(const RuleArrayData*, aArg2);
  return arg1->mWeight - arg2->mWeight; // put lower weight first
}


struct FillArrayData {
  FillArrayData(RuleArrayData* aArrayData) :
    mIndex(0),
    mArrayData(aArrayData)
  {
  }
  PRInt32 mIndex;
  RuleArrayData* mArrayData;
};

PR_STATIC_CALLBACK(PRBool)
FillArray(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsPRUint32Key* key = NS_STATIC_CAST(nsPRUint32Key*, aKey);
  nsVoidArray* weightArray = NS_STATIC_CAST(nsVoidArray*, aData);
  FillArrayData* data = NS_STATIC_CAST(FillArrayData*, aClosure);

  RuleArrayData& ruleData = data->mArrayData[data->mIndex++];
  ruleData.mRuleArray = weightArray;
  ruleData.mWeight = key->GetValue();

  return PR_TRUE;
}

/**
 * Takes the hashtable of arrays (keyed by weight, in order sort) and
 * puts them all in one big array which has a primary sort by weight
 * and secondary sort by order.
 */
static void PutRulesInList(nsObjectHashtable* aRuleArrays,
                           nsVoidArray* aWeightedRules)
{
  PRInt32 arrayCount = aRuleArrays->Count();
  RuleArrayData* arrayData = new RuleArrayData[arrayCount];
  FillArrayData faData(arrayData);
  aRuleArrays->Enumerate(FillArray, &faData);
  NS_QuickSort(arrayData, arrayCount, sizeof(RuleArrayData),
               CompareArrayData, nsnull);
  for (PRInt32 i = 0; i < arrayCount; ++i)
    aWeightedRules->AppendElements(*arrayData[i].mRuleArray);

  delete [] arrayData;
}

RuleCascadeData*
nsCSSRuleProcessor::GetRuleCascade(nsPresContext* aPresContext)
{
  // Having RuleCascadeData objects be per-medium works for now since
  // nsCSSRuleProcessor objects are per-document.  (For a given set
  // of stylesheets they can vary based on medium (@media) or document
  // (@-moz-document).)  Things will get a little more complicated if
  // we implement media queries, though.

  RuleCascadeData **cascadep = &mRuleCascades;
  RuleCascadeData *cascade;
  nsIAtom *medium = aPresContext->Medium();
  while ((cascade = *cascadep)) {
    if (cascade->mMedium == medium)
      return cascade;
    cascadep = &cascade->mNext;
  }

  if (mSheets.Count() != 0) {
    cascade = new RuleCascadeData(medium,
                                  eCompatibility_NavQuirks == aPresContext->CompatibilityMode());
    if (cascade) {
      CascadeEnumData data(aPresContext, cascade->mRuleHash.Arena());
      mSheets.EnumerateForwards(CascadeSheetRulesInto, &data);
      nsVoidArray weightedRules;
      PutRulesInList(&data.mRuleArrays, &weightedRules);

      // Put things into the rule hash backwards because it's easier to
      // build a singly linked list lowest-first that way.
      if (!weightedRules.EnumerateBackwards(AddRule, cascade)) {
        delete cascade;
        cascade = nsnull;
      }

      *cascadep = cascade;
    }
  }
  return cascade;
}
