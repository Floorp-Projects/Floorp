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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsCSSStyleSheet.h"

#define PL_ARENA_CONST_ALIGN_MASK 7
#define NS_RULEHASH_ARENA_BLOCK_SIZE (256)
#include "plarena.h"

#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "pldhash.h"
#include "nsHashtable.h"
#include "nsICSSPseudoComparator.h"
#include "nsCSSRuleProcessor.h"
#include "nsICSSStyleRule.h"
#include "nsICSSNameSpaceRule.h"
#include "nsICSSGroupRule.h"
#include "nsIMediaList.h"
#include "nsIStyledContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIFrame.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "nsIUnicharInputStream.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSRuleList.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMNode.h"
#include "nsDOMError.h"
#include "nsIPresShell.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsITextContent.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsStyleUtil.h"
#include "nsQuickSort.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsAttrValue.h"

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

PR_STATIC_CALLBACK(PRBool)
RuleHash_CIMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                      const void *key)
{
  nsIAtom *match_atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*,
                            key));
  // Use the |getKey| callback to avoid code duplication.
  // XXX Ugh!  Why does |getKey| have different |const|-ness?
  nsIAtom *entry_atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*,
             table->ops->getKey(table, NS_CONST_CAST(PLDHashEntryHdr*, hdr))));

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
  // Use the |getKey| callback to avoid code duplication.
  // XXX Ugh!  Why does |getKey| have different |const|-ness?
  nsIAtom *entry_atom = NS_CONST_CAST(nsIAtom*, NS_STATIC_CAST(const nsIAtom*,
             table->ops->getKey(table, NS_CONST_CAST(PLDHashEntryHdr*, hdr))));

  return match_atom == entry_atom;
}

PR_STATIC_CALLBACK(const void*)
RuleHash_TagTable_GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mTag;
}

PR_STATIC_CALLBACK(const void*)
RuleHash_ClassTable_GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mClassList->mAtom;
}

PR_STATIC_CALLBACK(const void*)
RuleHash_IdTable_GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*, hdr);
  return entry->mRules->mSelector->mIDList->mAtom;
}

PR_STATIC_CALLBACK(const void*)
RuleHash_NameSpaceTable_GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  RuleHashTableEntry *entry = NS_STATIC_CAST(RuleHashTableEntry*, hdr);
  return NS_INT32_TO_PTR(entry->mRules->mSelector->mNameSpace);
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

static PLDHashTableOps RuleHash_TagTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_TagTable_GetKey,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

// Case-sensitive ops.
static PLDHashTableOps RuleHash_ClassTable_CSOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_ClassTable_GetKey,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

// Case-insensitive ops.
static PLDHashTableOps RuleHash_ClassTable_CIOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_ClassTable_GetKey,
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

// Case-sensitive ops.
static PLDHashTableOps RuleHash_IdTable_CSOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_IdTable_GetKey,
  PL_DHashVoidPtrKeyStub,
  RuleHash_CSMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

// Case-insensitive ops.
static PLDHashTableOps RuleHash_IdTable_CIOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_IdTable_GetKey,
  RuleHash_CIHashKey,
  RuleHash_CIMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};

static PLDHashTableOps RuleHash_NameSpaceTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  RuleHash_NameSpaceTable_GetKey,
  RuleHash_NameSpaceTable_HashKey,
  RuleHash_NameSpaceTable_MatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
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

  PL_DHashTableInit(&mTagTable, &RuleHash_TagTable_Ops, nsnull,
                    sizeof(RuleHashTableEntry), 64);
  PL_DHashTableInit(&mIdTable,
                    aQuirksMode ? &RuleHash_IdTable_CIOps
                                : &RuleHash_IdTable_CSOps,
                    nsnull, sizeof(RuleHashTableEntry), 16);
  PL_DHashTableInit(&mClassTable,
                    aQuirksMode ? &RuleHash_ClassTable_CIOps
                                : &RuleHash_ClassTable_CSOps,
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
               lineNumber, NS_ConvertUCS2toUTF8(selectorText).get());
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

static PLDHashTableOps AttributeSelectorOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashGetKeyStub,
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
// Style Rule List for the DOM
//
class CSSRuleListImpl : public nsIDOMCSSRuleList
{
public:
  CSSRuleListImpl(nsCSSStyleSheet *aStyleSheet);

  NS_DECL_ISUPPORTS

  // nsIDOMCSSRuleList interface
  NS_IMETHOD    GetLength(PRUint32* aLength); 
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn); 

  void DropReference() { mStyleSheet = nsnull; }

protected:
  virtual ~CSSRuleListImpl();

  nsCSSStyleSheet*  mStyleSheet;
public:
  PRBool              mRulesAccessed;
};

CSSRuleListImpl::CSSRuleListImpl(nsCSSStyleSheet *aStyleSheet)
{
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
  mRulesAccessed = PR_FALSE;
}

CSSRuleListImpl::~CSSRuleListImpl()
{
}

// QueryInterface implementation for CSSRuleList
NS_INTERFACE_MAP_BEGIN(CSSRuleListImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSRuleList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(CSSRuleListImpl)
NS_IMPL_RELEASE(CSSRuleListImpl)


NS_IMETHODIMP    
CSSRuleListImpl::GetLength(PRUint32* aLength)
{
  if (nsnull != mStyleSheet) {
    PRInt32 count;
    mStyleSheet->StyleRuleCount(count);
    *aLength = (PRUint32)count;
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP    
CSSRuleListImpl::Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;
  if (mStyleSheet) {
    result = mStyleSheet->EnsureUniqueInner(); // needed to ensure rules have correct parent
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsICSSRule> rule;

      result = mStyleSheet->GetStyleRuleAt(aIndex, *getter_AddRefs(rule));
      if (rule) {
        result = rule->GetDOMRule(aReturn);
        mRulesAccessed = PR_TRUE; // signal to never share rules again
      } else if (result == NS_ERROR_ILLEGAL_VALUE) {
        result = NS_OK; // per spec: "Return Value ... null if ... not a valid index."
      }
    }
  }
  
  return result;
}

class DOMMediaListImpl : public nsIDOMMediaList,
                         public nsIMediaList
{
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMMEDIALIST

  NS_FORWARD_NSISUPPORTSARRAY(mArray->)
  NS_FORWARD_NSICOLLECTION(mArray->);
  NS_FORWARD_NSISERIALIZABLE(mArray->); // XXXbe temporary

  NS_IMETHOD_(PRBool) operator==(const nsISupportsArray& other) {
    return PR_FALSE;
  }

  NS_IMETHOD_(nsISupports*)  operator[](PRUint32 aIndex) {
    return mArray->ElementAt(aIndex);
  }

  // nsIMediaList methods
  NS_DECL_NSIMEDIALIST
  
  DOMMediaListImpl(nsISupportsArray *aArray, nsCSSStyleSheet *aStyleSheet);
  virtual ~DOMMediaListImpl();

private:
  nsresult Delete(const nsAString & aOldMedium);
  nsresult Append(const nsAString & aOldMedium);

  nsCOMPtr<nsISupportsArray> mArray;
  // not refcounted; sheet will let us know when it goes away
  // mStyleSheet is the sheet that needs to be dirtied when this medialist
  // changes
  nsCSSStyleSheet*         mStyleSheet;
};

// QueryInterface implementation for DOMMediaListImpl
NS_INTERFACE_MAP_BEGIN(DOMMediaListImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaList)
  NS_INTERFACE_MAP_ENTRY(nsIMediaList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMediaList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(MediaList)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(DOMMediaListImpl)
NS_IMPL_RELEASE(DOMMediaListImpl)


DOMMediaListImpl::DOMMediaListImpl(nsISupportsArray *aArray,
                                   nsCSSStyleSheet *aStyleSheet)
  : mArray(aArray), mStyleSheet(aStyleSheet)
{

  NS_ABORT_IF_FALSE(mArray, "This can't be used without an array!!");
}

DOMMediaListImpl::~DOMMediaListImpl()
{
}

nsresult
NS_NewMediaList(nsISupportsArray* aArray,
                nsICSSStyleSheet* aSheet,
                nsIMediaList** aInstancePtrResult)
{
  NS_ASSERTION(aInstancePtrResult, "Null out param.");
  DOMMediaListImpl* medialist = new DOMMediaListImpl(aArray, NS_STATIC_CAST(nsCSSStyleSheet*, aSheet));
  *aInstancePtrResult = medialist;
  NS_ENSURE_TRUE(medialist, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aInstancePtrResult);
  return NS_OK;
}

NS_IMETHODIMP
DOMMediaListImpl::GetText(nsAString& aMediaText)
{
  aMediaText.Truncate();

  PRUint32 cnt;
  nsresult rv = Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  PRInt32 count = cnt, index = 0;

  while (index < count) {
    nsCOMPtr<nsIAtom> medium;
    QueryElementAt(index++, NS_GET_IID(nsIAtom), getter_AddRefs(medium));
    NS_ENSURE_TRUE(medium, NS_ERROR_FAILURE);

    nsAutoString buffer;
    medium->ToString(buffer);
    aMediaText.Append(buffer);
    if (index < count) {
      aMediaText.AppendLiteral(", ");
    }
  }

  return NS_OK;
}

// XXXbz this is so ill-defined in the spec, it's not clear quite what
// it should be doing....
NS_IMETHODIMP
DOMMediaListImpl::SetText(const nsAString& aMediaText)
{
  nsresult rv = Clear();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString buf(aMediaText);
  PRInt32 n = buf.FindChar(',');

  do {
    if (n < 0)
      n = buf.Length();

    nsAutoString tmp;

    buf.Left(tmp, n);

    tmp.CompressWhitespace();

    if (!tmp.IsEmpty()) {
      rv = Append(tmp);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    buf.Cut(0, n + 1);

    n = buf.FindChar(',');
  } while (!buf.IsEmpty());

  return rv;
}

/*
 * aMatch is true when we contain the desired medium or contain the
 * "all" medium or contain no media at all, which is the same as
 * containing "all"
 */
NS_IMETHODIMP
DOMMediaListImpl::MatchesMedium(nsIAtom* aMedium, PRBool* aMatch)
{
  NS_ENSURE_ARG_POINTER(aMatch);
  *aMatch = PR_FALSE;
  *aMatch = (-1 != IndexOf(aMedium)) ||
            (-1 != IndexOf(nsLayoutAtoms::all));
  if (*aMatch)
    return NS_OK;
  PRUint32 count;
  nsresult rv = Count(&count);
  if(NS_FAILED(rv))
    return rv;  
  *aMatch = (count == 0);
  return NS_OK;
}

NS_IMETHODIMP
DOMMediaListImpl::DropReference()
{
  mStyleSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMMediaListImpl::GetMediaText(nsAString& aMediaText)
{
  return GetText(aMediaText);
}

// "sheet" should be an nsCSSStyleSheet and "doc" should be an
// nsCOMPtr<nsIDocument>
#define BEGIN_MEDIA_CHANGE(sheet, doc)                         \
  if (sheet) {                                                 \
    rv = sheet->GetOwningDocument(*getter_AddRefs(doc));       \
    NS_ENSURE_SUCCESS(rv, rv);                                 \
  }                                                            \
  mozAutoDocUpdate updateBatch(doc, UPDATE_STYLE, PR_TRUE);    \
  if (sheet) {                                                 \
    rv = sheet->WillDirty();                                   \
    NS_ENSURE_SUCCESS(rv, rv);                                 \
  }

#define END_MEDIA_CHANGE(sheet, doc)                           \
  if (sheet) {                                                 \
    sheet->DidDirty();                                         \
  }                                                            \
  /* XXXldb Pass something meaningful? */                      \
  if (doc) {                                                   \
    doc->StyleRuleChanged(sheet, nsnull, nsnull);              \
  }


NS_IMETHODIMP
DOMMediaListImpl::SetMediaText(const nsAString& aMediaText)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)

  rv = SetText(aMediaText);
  if (NS_FAILED(rv))
    return rv;
  
  END_MEDIA_CHANGE(mStyleSheet, doc)

  return rv;
}
                               
NS_IMETHODIMP
DOMMediaListImpl::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  PRUint32 cnt;

  nsresult rv = Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  *aLength = cnt;

  return NS_OK;
}

NS_IMETHODIMP
DOMMediaListImpl::Item(PRUint32 aIndex, nsAString& aReturn)
{
  nsCOMPtr<nsISupports> tmp(dont_AddRef(ElementAt(aIndex)));

  if (tmp) {
    nsCOMPtr<nsIAtom> medium(do_QueryInterface(tmp));
    NS_ENSURE_TRUE(medium, NS_ERROR_FAILURE);

    nsAutoString buffer;
    medium->ToString(buffer);
    aReturn.Assign(buffer);
  } else {
    aReturn.Truncate();
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMMediaListImpl::DeleteMedium(const nsAString& aOldMedium)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)
  
  rv = Delete(aOldMedium);
  if (NS_FAILED(rv))
    return rv;

  END_MEDIA_CHANGE(mStyleSheet, doc)
  
  return rv;
}

NS_IMETHODIMP
DOMMediaListImpl::AppendMedium(const nsAString& aNewMedium)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocument> doc;

  BEGIN_MEDIA_CHANGE(mStyleSheet, doc)
  
  rv = Append(aNewMedium);
  if (NS_FAILED(rv))
    return rv;

  END_MEDIA_CHANGE(mStyleSheet, doc)
  
  return rv;
}

nsresult
DOMMediaListImpl::Delete(const nsAString& aOldMedium)
{
  if (aOldMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  nsCOMPtr<nsIAtom> old = do_GetAtom(aOldMedium);
  NS_ENSURE_TRUE(old, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 indx = IndexOf(old);

  if (indx < 0) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  RemoveElementAt(indx);

  return NS_OK;
}

nsresult
DOMMediaListImpl::Append(const nsAString& aNewMedium)
{
  if (aNewMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  nsCOMPtr<nsIAtom> media = do_GetAtom(aNewMedium);
  NS_ENSURE_TRUE(media, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 indx = IndexOf(media);

  if (indx >= 0) {
    RemoveElementAt(indx);
  }

  AppendElement(media);

  return NS_OK;
}

// -------------------------------
// Imports Collection for the DOM
//
class CSSImportsCollectionImpl : public nsIDOMStyleSheetList
{
public:
  CSSImportsCollectionImpl(nsICSSStyleSheet *aStyleSheet);

  NS_DECL_ISUPPORTS

  // nsIDOMCSSStyleSheetList interface
  NS_IMETHOD    GetLength(PRUint32* aLength); 
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn); 

  void DropReference() { mStyleSheet = nsnull; }

protected:
  virtual ~CSSImportsCollectionImpl();

  nsICSSStyleSheet*  mStyleSheet;
};

CSSImportsCollectionImpl::CSSImportsCollectionImpl(nsICSSStyleSheet *aStyleSheet)
{
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
}

CSSImportsCollectionImpl::~CSSImportsCollectionImpl()
{
}


// QueryInterface implementation for CSSImportsCollectionImpl
NS_INTERFACE_MAP_BEGIN(CSSImportsCollectionImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheetList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(StyleSheetList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(CSSImportsCollectionImpl)
NS_IMPL_RELEASE(CSSImportsCollectionImpl)


NS_IMETHODIMP
CSSImportsCollectionImpl::GetLength(PRUint32* aLength)
{
  if (nsnull != mStyleSheet) {
    PRInt32 count;
    mStyleSheet->StyleSheetCount(count);
    *aLength = (PRUint32)count;
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP    
CSSImportsCollectionImpl::Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;

  if (mStyleSheet) {
    nsCOMPtr<nsICSSStyleSheet> sheet;

    result = mStyleSheet->GetStyleSheetAt(aIndex, *getter_AddRefs(sheet));
    if (NS_SUCCEEDED(result)) {
      result = CallQueryInterface(sheet, aReturn);
    }
  }
  
  return result;
}

// -------------------------------
// CSS Style Sheet Inner Data Container
//


static PRBool SetStyleSheetReference(nsISupports* aElement, void* aSheet)
{
  nsICSSRule*  rule = (nsICSSRule*)aElement;

  if (nsnull != rule) {
    rule->SetStyleSheet((nsICSSStyleSheet*)aSheet);
  }
  return PR_TRUE;
}

nsCSSStyleSheetInner::nsCSSStyleSheetInner(nsICSSStyleSheet* aParentSheet)
  : mSheets(),
    mOrderedRules(nsnull),
    mNameSpace(nsnull),
    mComplete(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsCSSStyleSheetInner);
  mSheets.AppendElement(aParentSheet);
}

static PRBool
CloneRuleInto(nsISupports* aRule, void* aArray)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  nsICSSRule* clone = nsnull;
  rule->Clone(clone);
  if (clone) {
    nsISupportsArray* array = (nsISupportsArray*)aArray;
    array->AppendElement(clone);
    NS_RELEASE(clone);
  }
  return PR_TRUE;
}

nsCSSStyleSheetInner::nsCSSStyleSheetInner(nsCSSStyleSheetInner& aCopy,
                                       nsICSSStyleSheet* aParentSheet)
  : mSheets(),
    mSheetURI(aCopy.mSheetURI),
    mBaseURI(aCopy.mBaseURI),
    mNameSpace(nsnull),
    mComplete(aCopy.mComplete)
{
  MOZ_COUNT_CTOR(nsCSSStyleSheetInner);
  mSheets.AppendElement(aParentSheet);
  if (aCopy.mOrderedRules) {
    NS_NewISupportsArray(&mOrderedRules);
    if (mOrderedRules) {
      aCopy.mOrderedRules->EnumerateForwards(CloneRuleInto, mOrderedRules);
      mOrderedRules->EnumerateForwards(SetStyleSheetReference, aParentSheet);
    }
  }
  else {
    mOrderedRules = nsnull;
  }
  RebuildNameSpaces();
}

nsCSSStyleSheetInner::~nsCSSStyleSheetInner()
{
  MOZ_COUNT_DTOR(nsCSSStyleSheetInner);
  if (mOrderedRules) {
    mOrderedRules->EnumerateForwards(SetStyleSheetReference, nsnull);
    NS_RELEASE(mOrderedRules);
  }
}

nsCSSStyleSheetInner* 
nsCSSStyleSheetInner::CloneFor(nsICSSStyleSheet* aParentSheet)
{
  return new nsCSSStyleSheetInner(*this, aParentSheet);
}

void
nsCSSStyleSheetInner::AddSheet(nsICSSStyleSheet* aParentSheet)
{
  mSheets.AppendElement(aParentSheet);
}

void
nsCSSStyleSheetInner::RemoveSheet(nsICSSStyleSheet* aParentSheet)
{
  if (1 == mSheets.Count()) {
    NS_ASSERTION(aParentSheet == (nsICSSStyleSheet*)mSheets.ElementAt(0), "bad parent");
    delete this;
    return;
  }
  if (aParentSheet == (nsICSSStyleSheet*)mSheets.ElementAt(0)) {
    mSheets.RemoveElementAt(0);
    NS_ASSERTION(mSheets.Count(), "no parents");
    if (mOrderedRules) {
      mOrderedRules->EnumerateForwards(SetStyleSheetReference, 
                                       (nsICSSStyleSheet*)mSheets.ElementAt(0));
    }
  }
  else {
    mSheets.RemoveElement(aParentSheet);
  }
}

static PRBool
CreateNameSpace(nsISupports* aRule, void* aNameSpacePtr)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  PRInt32 type = nsICSSRule::UNKNOWN_RULE;
  rule->GetType(type);
  if (nsICSSRule::NAMESPACE_RULE == type) {
    nsICSSNameSpaceRule*  nameSpaceRule = (nsICSSNameSpaceRule*)rule;
    nsINameSpace** nameSpacePtr = (nsINameSpace**)aNameSpacePtr;
    nsINameSpace* lastNameSpace = *nameSpacePtr;
    nsINameSpace* newNameSpace;

    nsIAtom*      prefix = nsnull;
    nsAutoString  urlSpec;
    nameSpaceRule->GetPrefix(prefix);
    nameSpaceRule->GetURLSpec(urlSpec);
    lastNameSpace->CreateChildNameSpace(prefix, urlSpec, &newNameSpace);
    NS_IF_RELEASE(prefix);
    if (newNameSpace) {
      NS_RELEASE(lastNameSpace);
      (*nameSpacePtr) = newNameSpace; // takes ref
    }

    return PR_TRUE;
  }
  // stop if not namespace, import or charset because namespace can't follow anything else
  return (((nsICSSRule::CHARSET_RULE == type) || 
           (nsICSSRule::IMPORT_RULE)) ? PR_TRUE : PR_FALSE); 
}

void 
nsCSSStyleSheetInner::RebuildNameSpaces()
{
  nsContentUtils::GetNSManagerWeakRef()->
      CreateRootNameSpace(getter_AddRefs(mNameSpace));

  if (mOrderedRules) {
    mOrderedRules->EnumerateForwards(CreateNameSpace, address_of(mNameSpace));
  }
}


// -------------------------------
// CSS Style Sheet
//

MOZ_DECL_CTOR_COUNTER(nsCSSStyleSheet)

nsCSSStyleSheet::nsCSSStyleSheet()
  : nsICSSStyleSheet(),
    mRefCnt(0),
    mTitle(), 
    mMedia(nsnull),
    mFirstChild(nsnull), 
    mNext(nsnull),
    mParent(nsnull),
    mOwnerRule(nsnull),
    mImportsCollection(nsnull),
    mRuleCollection(nsnull),
    mDocument(nsnull),
    mOwningNode(nsnull),
    mDisabled(PR_FALSE),
    mDirty(PR_FALSE),
    mRuleProcessors(nsnull)
{

  mInner = new nsCSSStyleSheetInner(this);
}

nsCSSStyleSheet::nsCSSStyleSheet(const nsCSSStyleSheet& aCopy,
                                     nsICSSStyleSheet* aParentToUse,
                                     nsICSSImportRule* aOwnerRuleToUse,
                                     nsIDocument* aDocumentToUse,
                                     nsIDOMNode* aOwningNodeToUse)
  : nsICSSStyleSheet(),
    mRefCnt(0),
    mTitle(aCopy.mTitle), 
    mMedia(nsnull),
    mFirstChild(nsnull), 
    mNext(nsnull),
    mParent(aParentToUse),
    mOwnerRule(aOwnerRuleToUse),
    mImportsCollection(nsnull), // re-created lazily
    mRuleCollection(nsnull), // re-created lazily
    mDocument(aDocumentToUse),
    mOwningNode(aOwningNodeToUse),
    mDisabled(aCopy.mDisabled),
    mDirty(PR_FALSE),
    mInner(aCopy.mInner),
    mRuleProcessors(nsnull)
{

  mInner->AddSheet(this);

  if (aCopy.mRuleCollection && 
      aCopy.mRuleCollection->mRulesAccessed) {  // CSSOM's been there, force full copy now
    NS_ASSERTION(mInner->mComplete, "Why have rules been accessed on an incomplete sheet?");
    EnsureUniqueInner();
  }

  if (aCopy.mMedia) {
    nsCOMPtr<nsISupportsArray> tmp;
    (NS_STATIC_CAST(nsISupportsArray *, aCopy.mMedia))->Clone(getter_AddRefs(tmp));
    mMedia = new DOMMediaListImpl(tmp, this);
    NS_IF_ADDREF(mMedia);
  }

  if (aCopy.mFirstChild) {
    nsCSSStyleSheet*  otherChild = aCopy.mFirstChild;
    nsCSSStyleSheet** ourSlot = &mFirstChild;
    do {
      // XXX This is wrong; we should be keeping @import rules and
      // sheets in sync!
      nsCSSStyleSheet* child = new nsCSSStyleSheet(*otherChild,
                                                       this,
                                                       nsnull,
                                                       aDocumentToUse,
                                                       nsnull);
      if (child) {
        NS_ADDREF(child);
        (*ourSlot) = child;
        ourSlot = &(child->mNext);
      }
      otherChild = otherChild->mNext;
    }
    while (otherChild && ourSlot);
  }
}

nsCSSStyleSheet::~nsCSSStyleSheet()
{
  if (mFirstChild) {
    nsCSSStyleSheet* child = mFirstChild;
    do {
      child->mParent = nsnull;
      child->mDocument = nsnull;
      child = child->mNext;
    } while (child);
    NS_RELEASE(mFirstChild);
  }
  NS_IF_RELEASE(mNext);
  if (nsnull != mRuleCollection) {
    mRuleCollection->DropReference();
    NS_RELEASE(mRuleCollection);
  }
  if (nsnull != mImportsCollection) {
    mImportsCollection->DropReference();
    NS_RELEASE(mImportsCollection);
  }
  if (mMedia) {
    mMedia->DropReference();
    NS_RELEASE(mMedia);
  }
  mInner->RemoveSheet(this);
  // XXX The document reference is not reference counted and should
  // not be released. The document will let us know when it is going
  // away.
  if (mRuleProcessors) {
    NS_ASSERTION(mRuleProcessors->Count() == 0, "destucting sheet with rule processor reference");
    delete mRuleProcessors; // weak refs, should be empty here anyway
  }
}


// QueryInterface implementation for nsCSSStyleSheet
NS_INTERFACE_MAP_BEGIN(nsCSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSStyleSheet)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleSheet)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsCSSStyleSheet)
NS_IMPL_RELEASE(nsCSSStyleSheet)


NS_IMETHODIMP
nsCSSStyleSheet::AddRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (! mRuleProcessors) {
    mRuleProcessors = new nsAutoVoidArray();
    if (!mRuleProcessors)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ASSERTION(-1 == mRuleProcessors->IndexOf(aProcessor),
               "processor already registered");
  mRuleProcessors->AppendElement(aProcessor); // weak ref
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::DropRuleProcessor(nsCSSRuleProcessor* aProcessor)
{
  if (!mRuleProcessors)
    return NS_ERROR_FAILURE;
  return mRuleProcessors->RemoveElement(aProcessor)
           ? NS_OK
           : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsCSSStyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aBaseURI)
{
  NS_PRECONDITION(aSheetURI && aBaseURI, "null ptr");

  if (! mInner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ASSERTION(!mInner->mOrderedRules && !mInner->mComplete,
               "Can't call SetURL on sheets that are complete or have rules");

  mInner->mSheetURI = aSheetURI;
  mInner->mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetSheetURI(nsIURI** aSheetURI) const
{
  NS_IF_ADDREF(*aSheetURI = (mInner ? mInner->mSheetURI.get() : nsnull));
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetBaseURI(nsIURI** aBaseURI) const
{
  NS_IF_ADDREF(*aBaseURI = (mInner ? mInner->mBaseURI.get() : nsnull));
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetTitle(const nsAString& aTitle)
{
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetType(nsString& aType) const
{
  aType.AssignLiteral("text/css");
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetMediumCount(PRInt32& aCount) const
{
  if (mMedia) {
    PRUint32 cnt;
    nsresult rv = mMedia->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    aCount = cnt;
  }
  else
    aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  nsIAtom* medium = nsnull;
  if (nsnull != mMedia) {
    medium = (nsIAtom*)mMedia->ElementAt(aIndex);
  }
  if (nsnull != medium) {
    aMedium = medium;
    return NS_OK;
  }
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP_(PRBool)
nsCSSStyleSheet::UseForMedium(nsIAtom* aMedium) const
{
  if (mMedia) {
    PRBool matches = PR_FALSE;
    mMedia->MatchesMedium(aMedium, &matches);
    return matches;
  }
  return PR_TRUE;
}



NS_IMETHODIMP
nsCSSStyleSheet::AppendMedium(nsIAtom* aMedium)
{
  nsresult result = NS_OK;
  if (!mMedia) {
    nsCOMPtr<nsISupportsArray> tmp;
    result = NS_NewISupportsArray(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(result, result);

    mMedia = new DOMMediaListImpl(tmp, this);
    NS_ENSURE_TRUE(mMedia, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mMedia);
  }

  if (mMedia) {
    mMedia->AppendElement(aMedium);
  }
  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::ClearMedia()
{
  if (mMedia) {
    mMedia->Clear();
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsCSSStyleSheet::HasRules() const
{
  PRInt32 count;
  StyleRuleCount(count);
  return count != 0;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetApplicable(PRBool& aApplicable) const
{
  aApplicable = !mDisabled && mInner && mInner->mComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetEnabled(PRBool aEnabled)
{
  return nsCSSStyleSheet::SetDisabled(!aEnabled);
}

NS_IMETHODIMP
nsCSSStyleSheet::GetComplete(PRBool& aComplete) const
{
  aComplete = mInner && mInner->mComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetComplete()
{
  if (!mInner)
    return NS_ERROR_UNEXPECTED;
  NS_ASSERTION(!mDirty, "Can't set a dirty sheet complete!");
  mInner->mComplete = PR_TRUE;
  if (mDocument && !mDisabled) {
    // Let the document know
    mDocument->BeginUpdate(UPDATE_STYLE);
    mDocument->SetStyleSheetApplicableState(this, PR_TRUE);
    mDocument->EndUpdate(UPDATE_STYLE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwningDocument(nsIDocument*& aDocument) const
{
  aDocument = mDocument;
  NS_IF_ADDREF(aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{ // not ref counted
  mDocument = aDocument;
  // Now set the same document on all our child sheets....
  for (nsCSSStyleSheet* child = mFirstChild; child; child = child->mNext) {
    child->mDocument = aDocument;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwningNode(nsIDOMNode* aOwningNode)
{ // not ref counted
  mOwningNode = aOwningNode;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetOwnerRule(nsICSSImportRule* aOwnerRule)
{ // not ref counted
  mOwnerRule = aOwnerRule;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwnerRule(nsICSSImportRule** aOwnerRule)
{
  *aOwnerRule = mOwnerRule;
  NS_IF_ADDREF(*aOwnerRule);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::ContainsStyleSheet(nsIURI* aURL, PRBool& aContains, nsIStyleSheet** aTheChild /*=nsnull*/)
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

  if (!mInner || !mInner->mSheetURI) {
    // We're not yet far enough along in our load to know what our URL is (we
    // may still get redirected and such).  Assert (caller should really not be
    // calling this on us at this stage) and return.
    NS_ERROR("ContainsStyleSheet called on a sheet that's still loading");
    aContains = PR_FALSE;
    return NS_OK;
  }
  
  // first check ourself out
  nsresult rv = mInner->mSheetURI->Equals(aURL, &aContains);
  if (NS_FAILED(rv)) aContains = PR_FALSE;

  if (aContains) {
    // if we found it and the out-param is there, set it and addref
    if (aTheChild) {
      rv = QueryInterface( NS_GET_IID(nsIStyleSheet), (void **)aTheChild);
    }
  } else {
    nsCSSStyleSheet*  child = mFirstChild;
    // now check the chil'ins out (recursively)
    while ((PR_FALSE == aContains) && (nsnull != child)) {
      child->ContainsStyleSheet(aURL, aContains, aTheChild);
      if (aContains) {
        break;
      } else {
        child = child->mNext;
      }
    }
  }

  // NOTE: if there are errors in the above we are handling them locally 
  //       and not promoting them to the caller
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    NS_ADDREF(aSheet);
    nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;

    if (! mFirstChild) {
      mFirstChild = sheet;
    }
    else {
      nsCSSStyleSheet* child = mFirstChild;
      while (child->mNext) {
        child = child->mNext;
      }
      child->mNext = sheet;
    }
  
    // This is not reference counted. Our parent tells us when
    // it's going away.
    sheet->mParent = this;
    sheet->mDocument = mDocument;
    DidDirty();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  nsresult result = WillDirty();

  if (NS_SUCCEEDED(result)) {
    NS_ADDREF(aSheet);
    nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;
    nsCSSStyleSheet* child = mFirstChild;

    if (aIndex && child) {
      while ((0 < --aIndex) && child->mNext) {
        child = child->mNext;
      }
      sheet->mNext = child->mNext;
      child->mNext = sheet;
    }
    else {
      sheet->mNext = mFirstChild;
      mFirstChild = sheet; 
    }

    // This is not reference counted. Our parent tells us when
    // it's going away.
    sheet->mParent = this;
    sheet->mDocument = mDocument;
    DidDirty();
  }
  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::PrependStyleRule(nsICSSRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    if (! mInner->mOrderedRules) {
      NS_NewISupportsArray(&(mInner->mOrderedRules));
    }
    if (mInner->mOrderedRules) {
      mInner->mOrderedRules->InsertElementAt(aRule, 0);
      aRule->SetStyleSheet(this);
      DidDirty();

      PRInt32 type = nsICSSRule::UNKNOWN_RULE;
      aRule->GetType(type);
      if (nsICSSRule::NAMESPACE_RULE == type) {
        // no api to prepend a namespace (ugh), release old ones and re-create them all
        mInner->RebuildNameSpaces();
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::AppendStyleRule(nsICSSRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    if (! mInner->mOrderedRules) {
      NS_NewISupportsArray(&(mInner->mOrderedRules));
    }
    if (mInner->mOrderedRules) {
      mInner->mOrderedRules->AppendElement(aRule);
      aRule->SetStyleSheet(this);
      DidDirty();

      PRInt32 type = nsICSSRule::UNKNOWN_RULE;
      aRule->GetType(type);
      if (nsICSSRule::NAMESPACE_RULE == type) {
        if (! mInner->mNameSpace) {
          nsContentUtils::GetNSManagerWeakRef()->
              CreateRootNameSpace(getter_AddRefs(mInner->mNameSpace));
        }

        if (mInner->mNameSpace) {
          nsCOMPtr<nsICSSNameSpaceRule> nameSpaceRule(do_QueryInterface(aRule));
          nsCOMPtr<nsINameSpace> newNameSpace;

          nsCOMPtr<nsIAtom> prefix;
          nsAutoString  urlSpec;
          nameSpaceRule->GetPrefix(*getter_AddRefs(prefix));
          nameSpaceRule->GetURLSpec(urlSpec);
          mInner->mNameSpace->
              CreateChildNameSpace(prefix, urlSpec,
                                   getter_AddRefs(newNameSpace));
          if (newNameSpace) {
            mInner->mNameSpace = newNameSpace;
          }
        }
      }
        
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew)
{
  NS_PRECONDITION(mInner->mOrderedRules, "can't have old rule");
  NS_PRECONDITION(mInner && mInner->mComplete,
                  "No replacing in an incomplete sheet!");

  if (NS_SUCCEEDED(WillDirty())) {
    PRInt32 index = mInner->mOrderedRules->IndexOf(aOld);
    NS_ENSURE_TRUE(index != -1, NS_ERROR_UNEXPECTED);
    mInner->mOrderedRules->ReplaceElementAt(aNew, index);

    aNew->SetStyleSheet(this);
    aOld->SetStyleSheet(nsnull);
    DidDirty();
#ifdef DEBUG
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    aNew->GetType(type);
    NS_ASSERTION(nsICSSRule::NAMESPACE_RULE != type, "not yet implemented");
    aOld->GetType(type);
    NS_ASSERTION(nsICSSRule::NAMESPACE_RULE != type, "not yet implemented");
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::StyleRuleCount(PRInt32& aCount) const
{
  aCount = 0;
  if (mInner && mInner->mOrderedRules) {
    PRUint32 cnt;
    nsresult rv = ((nsCSSStyleSheet*)this)->mInner->mOrderedRules->Count(&cnt);      // XXX bogus cast -- this method should not be const
    aCount = (PRInt32)cnt;
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const
{
  // Important: If this function is ever made scriptable, we must add
  // a security check here. See GetCSSRules below for an example.
  nsresult result = NS_ERROR_ILLEGAL_VALUE;

  if (mInner && mInner->mOrderedRules) {
    aRule = (nsICSSRule*)(mInner->mOrderedRules->ElementAt(aIndex));
    if (nsnull != aRule) {
      result = NS_OK;
    }
  }
  else {
    aRule = nsnull;
  }
  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetNameSpace(nsINameSpace*& aNameSpace) const
{
  if (mInner) {
    aNameSpace = mInner->mNameSpace;
    NS_IF_ADDREF(aNameSpace);
  }
  else {
    aNameSpace = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::StyleSheetCount(PRInt32& aCount) const
{
  // XXX Far from an ideal way to do this, but the hope is that
  // it won't be done too often. If it is, we might want to 
  // consider storing the children in an array.
  aCount = 0;

  const nsCSSStyleSheet* child = mFirstChild;
  while (child) {
    aCount++;
    child = child->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const
{
  // XXX Ughh...an O(n^2) method for doing iteration. Again, we hope
  // that this isn't done too often. If it is, we need to change the
  // underlying storage mechanism
  aSheet = nsnull;

  if (mFirstChild) {
    const nsCSSStyleSheet* child = mFirstChild;
    while ((child) && (0 != aIndex)) {
      --aIndex;
      child = child->mNext;
    }
    
    aSheet = (nsICSSStyleSheet*)child;
    NS_IF_ADDREF(aSheet);
  }

  return NS_OK;
}

nsresult  
nsCSSStyleSheet::EnsureUniqueInner()
{
  if (! mInner) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (1 < mInner->mSheets.Count()) {
    nsCSSStyleSheetInner* clone = mInner->CloneFor(this);
    if (clone) {
      mInner->RemoveSheet(this);
      mInner = clone;
    }
    else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::Clone(nsICSSStyleSheet* aCloneParent,
                         nsICSSImportRule* aCloneOwnerRule,
                         nsIDocument* aCloneDocument,
                         nsIDOMNode* aCloneOwningNode,
                         nsICSSStyleSheet** aClone) const
{
  NS_PRECONDITION(aClone, "Null out param!");
  nsCSSStyleSheet* clone = new nsCSSStyleSheet(*this,
                                                   aCloneParent,
                                                   aCloneOwnerRule,
                                                   aCloneDocument,
                                                   aCloneOwningNode);
  if (clone) {
    *aClone = NS_STATIC_CAST(nsICSSStyleSheet*, clone);
    NS_ADDREF(*aClone);
  }
  return NS_OK;
}

#ifdef DEBUG
static void
ListRules(nsISupportsArray* aRules, FILE* aOut, PRInt32 aIndent)
{
  PRUint32 count;
  PRInt32 index;
  if (aRules) {
    aRules->Count(&count);
    for (index = count - 1; index >= 0; --index) {
      nsCOMPtr<nsICSSRule> rule = dont_AddRef((nsICSSRule*)aRules->ElementAt(index));
      rule->List(aOut, aIndent);
    }
  }
}

struct ListEnumData {
  ListEnumData(FILE* aOut, PRInt32 aIndent)
    : mOut(aOut),
      mIndent(aIndent)
  {
  }
  FILE*   mOut;
  PRInt32 mIndent;
};

#if 0
static PRBool ListCascade(nsHashKey* aKey, void* aValue, void* aClosure)
{
  AtomKey* key = (AtomKey*)aKey;
  RuleCascadeData* cascade = (RuleCascadeData*)aValue;
  ListEnumData* data = (ListEnumData*)aClosure;

  fputs("\nRules in cascade order for medium: \"", data->mOut);
  nsAutoString  buffer;
  key->mAtom->ToString(buffer);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), data->mOut);
  fputs("\"\n", data->mOut);

  ListRules(cascade->mWeightedRules, data->mOut, data->mIndent);
  return PR_TRUE;
}
#endif


void nsCSSStyleSheet::List(FILE* out, PRInt32 aIndent) const
{

  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  if (! mInner) {
    fputs("CSS Style Sheet - without inner data storage - ERROR\n", out);
    return;
  }

  fputs("CSS Style Sheet: ", out);
  nsCAutoString urlSpec;
  nsresult rv = mInner->mSheetURI->GetSpec(urlSpec);
  if (NS_SUCCEEDED(rv) && !urlSpec.IsEmpty()) {
    fputs(urlSpec.get(), out);
  }

  if (mMedia) {
    fputs(" media: ", out);
    index = 0;
    PRUint32 count;
    mMedia->Count(&count);
    nsAutoString  buffer;
    while (index < PRInt32(count)) {
      nsCOMPtr<nsIAtom> medium = dont_AddRef((nsIAtom*)mMedia->ElementAt(index++));
      medium->ToString(buffer);
      fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
      if (index < PRInt32(count)) {
        fputs(", ", out);
      }
    }
  }
  fputs("\n", out);

  const nsCSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    child->List(out, aIndent + 1);
    child = child->mNext;
  }

  fputs("Rules in source order:\n", out);
  ListRules(mInner->mOrderedRules, out, aIndent);
}
#endif

static PRBool PR_CALLBACK
EnumClearRuleCascades(void* aProcessor, void* aData)
{
  nsCSSRuleProcessor* processor =
    NS_STATIC_CAST(nsCSSRuleProcessor*, aProcessor);
  processor->ClearRuleCascades();
  return PR_TRUE;
}

void 
nsCSSStyleSheet::ClearRuleCascades()
{
  if (mRuleProcessors) {
    mRuleProcessors->EnumerateForwards(EnumClearRuleCascades, nsnull);
  }
  if (mParent) {
    nsCSSStyleSheet* parent = (nsCSSStyleSheet*)mParent;
    parent->ClearRuleCascades();
  }
}

nsresult
nsCSSStyleSheet::WillDirty()
{
  if (mInner && !mInner->mComplete) {
    // Do nothing
    return NS_OK;
  }
  
  return EnsureUniqueInner();
}

void
nsCSSStyleSheet::DidDirty()
{
  ClearRuleCascades();
  mDirty = PR_TRUE;
}

NS_IMETHODIMP 
nsCSSStyleSheet::IsModified(PRBool* aSheetModified) const
{
  *aSheetModified = mDirty;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::SetModified(PRBool aModified)
{
  mDirty = aModified;
  return NS_OK;
}

  // nsIDOMStyleSheet interface
NS_IMETHODIMP    
nsCSSStyleSheet::GetType(nsAString& aType)
{
  aType.AssignLiteral("text/css");
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetDisabled(PRBool* aDisabled)
{
  *aDisabled = mDisabled;
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::SetDisabled(PRBool aDisabled)
{
  PRBool oldDisabled = mDisabled;
  mDisabled = aDisabled;

  if (mDocument && mInner && mInner->mComplete && oldDisabled != mDisabled) {
    ClearRuleCascades();

    mDocument->BeginUpdate(UPDATE_STYLE);
    mDocument->SetStyleSheetApplicableState(this, !mDisabled);
    mDocument->EndUpdate(UPDATE_STYLE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetOwnerNode(nsIDOMNode** aOwnerNode)
{
  *aOwnerNode = mOwningNode;
  NS_IF_ADDREF(*aOwnerNode);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aParentStyleSheet);

  nsresult rv = NS_OK;

  if (mParent) {
    rv =  mParent->QueryInterface(NS_GET_IID(nsIDOMStyleSheet),
                                  (void **)aParentStyleSheet);
  } else {
    *aParentStyleSheet = nsnull;
  }

  return rv;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetHref(nsAString& aHref)
{
  nsCAutoString str;

  // XXXldb The DOM spec says that this should be null for inline style sheets.
  if (mInner && mInner->mSheetURI) {
    mInner->mSheetURI->GetSpec(str);
  }

  CopyUTF8toUTF16(str, aHref);

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetTitle(nsString& aTitle) const
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::GetMedia(nsIDOMMediaList** aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  *aMedia = nsnull;

  if (!mMedia) {
    nsCOMPtr<nsISupportsArray> tmp;
    NS_NewISupportsArray(getter_AddRefs(tmp));
    NS_ENSURE_TRUE(tmp, NS_ERROR_NULL_POINTER);

    mMedia = new DOMMediaListImpl(tmp, this);
    NS_IF_ADDREF(mMedia);
  }

  *aMedia = mMedia;
  NS_IF_ADDREF(*aMedia);

  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetOwnerRule(nsIDOMCSSRule** aOwnerRule)
{
  if (mOwnerRule) {
    return mOwnerRule->GetDOMRule(aOwnerRule);
  }

  *aOwnerRule = nsnull;
  return NS_OK;    
}

NS_IMETHODIMP    
nsCSSStyleSheet::GetCssRules(nsIDOMCSSRuleList** aCssRules)
{
  // No doing this on incomplete sheets!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  
  //-- Security check: Only scripts from the same origin as the
  //   style sheet can access rule collections

  // Get JSContext from stack
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext *cx = nsnull;
  nsresult rv = NS_OK;

  rv = stack->Peek(&cx);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cx)
    return NS_ERROR_FAILURE;

  // Get the security manager and do the same-origin check
  rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(cx,
                                                             mInner->mSheetURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // OK, security check passed, so get the rule collection
  if (nsnull == mRuleCollection) {
    mRuleCollection = new CSSRuleListImpl(this);
    if (nsnull == mRuleCollection) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mRuleCollection);
  }

  *aCssRules = mRuleCollection;
  NS_ADDREF(mRuleCollection);

  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::InsertRule(const nsAString& aRule, 
                            PRUint32 aIndex, 
                            PRUint32* aReturn)
{
  NS_ENSURE_TRUE(mInner, NS_ERROR_FAILURE);
  // No doing this if the sheet is not complete!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  if (aRule.IsEmpty()) {
    // Nothing to do here
    return NS_OK;
  }
  
  nsresult result;
  result = WillDirty();
  if (NS_FAILED(result))
    return result;
  
  if (! mInner->mOrderedRules) {
    result = NS_NewISupportsArray(&(mInner->mOrderedRules));
  }
  if (NS_FAILED(result))
    return result;

  PRUint32 count;
  mInner->mOrderedRules->Count(&count);
  if (aIndex > count)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  
  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  nsCOMPtr<nsICSSLoader> loader;
  if (mDocument) {
    loader = mDocument->GetCSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }
  
  nsCOMPtr<nsICSSParser> css;
  if (loader) {
    result = loader->GetParserFor(this, getter_AddRefs(css));
  }
  else {
    result = NS_NewCSSParser(getter_AddRefs(css));
    if (css) {
      css->SetStyleSheet(this);
    }
  }
  if (NS_FAILED(result))
    return result;

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

  nsCOMPtr<nsISupportsArray> rules;
  result = css->ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                          getter_AddRefs(rules));
  if (NS_FAILED(result))
    return result;
  
  PRUint32 rulecount = 0;
  rules->Count(&rulecount);  
  if (rulecount == 0) {
    // Since we know aRule was not an empty string, just throw
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  
  // Hierarchy checking.  Just check the first and last rule in the list.
  
  // check that we're not inserting before a charset rule
  nsCOMPtr<nsICSSRule> nextRule;
  PRInt32 nextType = nsICSSRule::UNKNOWN_RULE;
  nextRule = dont_AddRef((nsICSSRule*)mInner->mOrderedRules->ElementAt(aIndex));
  if (nextRule) {
    nextRule->GetType(nextType);
    if (nextType == nsICSSRule::CHARSET_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    // check last rule in list
    nsCOMPtr<nsICSSRule> lastRule = dont_AddRef((nsICSSRule*)rules->ElementAt(rulecount-1));
    PRInt32 lastType = nsICSSRule::UNKNOWN_RULE;
    lastRule->GetType(lastType);
    
    if (nextType == nsICSSRule::IMPORT_RULE &&
        lastType != nsICSSRule::CHARSET_RULE &&
        lastType != nsICSSRule::IMPORT_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
    
    if (nextType == nsICSSRule::NAMESPACE_RULE &&
        lastType != nsICSSRule::CHARSET_RULE &&
        lastType != nsICSSRule::IMPORT_RULE &&
        lastType != nsICSSRule::NAMESPACE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    } 
  }
  
  // check first rule in list
  nsCOMPtr<nsICSSRule> firstRule = dont_AddRef((nsICSSRule*)rules->ElementAt(0));
  PRInt32 firstType = nsICSSRule::UNKNOWN_RULE;
  firstRule->GetType(firstType);
  if (aIndex != 0) {
    if (firstType == nsICSSRule::CHARSET_RULE) { // no inserting charset at nonzero position
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  
    nsCOMPtr<nsICSSRule> prevRule = dont_AddRef((nsICSSRule*)mInner->mOrderedRules->ElementAt(aIndex-1));
    PRInt32 prevType = nsICSSRule::UNKNOWN_RULE;
    prevRule->GetType(prevType);

    if (firstType == nsICSSRule::IMPORT_RULE &&
        prevType != nsICSSRule::CHARSET_RULE &&
        prevType != nsICSSRule::IMPORT_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    if (firstType == nsICSSRule::NAMESPACE_RULE &&
        prevType != nsICSSRule::CHARSET_RULE &&
        prevType != nsICSSRule::IMPORT_RULE &&
        prevType != nsICSSRule::NAMESPACE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  
  PRBool insertResult = mInner->mOrderedRules->InsertElementsAt(rules, aIndex);
  NS_ENSURE_TRUE(insertResult, NS_ERROR_OUT_OF_MEMORY);
  DidDirty();

  nsCOMPtr<nsICSSRule> cssRule;
  PRUint32 counter;
  for (counter = 0; counter < rulecount; counter++) {
    cssRule = dont_AddRef((nsICSSRule*)rules->ElementAt(counter));
    cssRule->SetStyleSheet(this);
    
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    cssRule->GetType(type);
    if (type == nsICSSRule::NAMESPACE_RULE) {
      if (! mInner->mNameSpace) {
        nsContentUtils::GetNSManagerWeakRef()->
            CreateRootNameSpace(getter_AddRefs(mInner->mNameSpace));
      }

      NS_ENSURE_TRUE(mInner->mNameSpace, NS_ERROR_FAILURE);

      nsCOMPtr<nsICSSNameSpaceRule> nameSpaceRule(do_QueryInterface(cssRule));
      nsCOMPtr<nsINameSpace> newNameSpace;
    
      nsCOMPtr<nsIAtom> prefix;
      nsAutoString urlSpec;
      nameSpaceRule->GetPrefix(*getter_AddRefs(prefix));
      nameSpaceRule->GetURLSpec(urlSpec);
      mInner->mNameSpace->CreateChildNameSpace(prefix, urlSpec,
                                               getter_AddRefs(newNameSpace));
      if (newNameSpace) {
        mInner->mNameSpace = newNameSpace;
      }
    }

    // We don't notify immediately for @import rules, but rather when
    // the sheet the rule is importing is loaded
    PRBool notify = PR_TRUE;
    if (type == nsICSSRule::IMPORT_RULE) {
      nsCOMPtr<nsIDOMCSSImportRule> importRule(do_QueryInterface(cssRule));
      NS_ASSERTION(importRule, "Rule which has type IMPORT_RULE and does not implement nsIDOMCSSImportRule!");
      nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
      importRule->GetStyleSheet(getter_AddRefs(childSheet));
      if (!childSheet) {
        notify = PR_FALSE;
      }
    }
    if (mDocument && notify) {
      mDocument->StyleRuleAdded(this, cssRule);
    }
  }
  
  if (loader) {
    loader->RecycleParser(css);
  }
  
  *aReturn = aIndex;
  return NS_OK;
}

NS_IMETHODIMP    
nsCSSStyleSheet::DeleteRule(PRUint32 aIndex)
{
  nsresult result = NS_ERROR_DOM_INDEX_SIZE_ERR;
  // No doing this if the sheet is not complete!
  PRBool complete;
  GetComplete(complete);
  if (!complete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  // XXX TBI: handle @rule types
  if (mInner && mInner->mOrderedRules) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);
    
    result = WillDirty();

    if (NS_SUCCEEDED(result)) {
      PRUint32 count;
      mInner->mOrderedRules->Count(&count);
      if (aIndex >= count)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

      nsCOMPtr<nsICSSRule> rule =
        dont_AddRef((nsICSSRule*)mInner->mOrderedRules->ElementAt(aIndex));
      if (rule) {
        mInner->mOrderedRules->RemoveElementAt(aIndex);
        rule->SetStyleSheet(nsnull);
        DidDirty();

        if (mDocument) {
          mDocument->StyleRuleRemoved(this, rule);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsCSSStyleSheet::DeleteRuleFromGroup(nsICSSGroupRule* aGroup, PRUint32 aIndex)
{
  NS_ENSURE_ARG_POINTER(aGroup);
  NS_ASSERTION(mInner && mInner->mComplete,
               "No deleting from an incomplete sheet!");
  nsresult result;
  nsCOMPtr<nsICSSRule> rule;
  result = aGroup->GetStyleRuleAt(aIndex, *getter_AddRefs(rule));
  NS_ENSURE_SUCCESS(result, result);
  
  // check that the rule actually belongs to this sheet!
  nsCOMPtr<nsIStyleSheet> ruleSheet;
  rule->GetStyleSheet(*getter_AddRefs(ruleSheet));
  if (this != ruleSheet) {
    return NS_ERROR_INVALID_ARG;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);
  
  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  result = aGroup->DeleteStyleRuleAt(aIndex);
  NS_ENSURE_SUCCESS(result, result);
  
  rule->SetStyleSheet(nsnull);
  
  DidDirty();

  if (mDocument) {
    mDocument->StyleRuleRemoved(this, rule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::InsertRuleIntoGroup(const nsAString & aRule,
                                     nsICSSGroupRule* aGroup,
                                     PRUint32 aIndex,
                                     PRUint32* _retval)
{
  nsresult result;
  NS_ASSERTION(mInner && mInner->mComplete,
               "No inserting into an incomplete sheet!");
  // check that the group actually belongs to this sheet!
  nsCOMPtr<nsIStyleSheet> groupSheet;
  aGroup->GetStyleSheet(*getter_AddRefs(groupSheet));
  if (this != groupSheet) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aRule.IsEmpty()) {
    // Nothing to do here
    return NS_OK;
  }
  
  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  nsCOMPtr<nsICSSLoader> loader;
  if (mDocument) {
    loader = mDocument->GetCSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  }

  nsCOMPtr<nsICSSParser> css;
  if (loader) {
    result = loader->GetParserFor(this, getter_AddRefs(css));
  }
  else {
    result = NS_NewCSSParser(getter_AddRefs(css));
    if (css) {
      css->SetStyleSheet(this);
    }
  }
  NS_ENSURE_SUCCESS(result, result);

  // parse and grab the rule
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  nsCOMPtr<nsISupportsArray> rules;
  result = css->ParseRule(aRule, mInner->mSheetURI, mInner->mBaseURI,
                          getter_AddRefs(rules));
  NS_ENSURE_SUCCESS(result, result);

  PRUint32 rulecount = 0;
  rules->Count(&rulecount);
  if (rulecount == 0) {
    // Since we know aRule was not an empty string, just throw
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  PRUint32 counter;
  nsCOMPtr<nsICSSRule> rule;
  for (counter = 0; counter < rulecount; counter++) {
    // Only rulesets are allowed in a group as of CSS2
    PRInt32 type = nsICSSRule::UNKNOWN_RULE;
    rule = dont_AddRef((nsICSSRule*)rules->ElementAt(counter));
    rule->GetType(type);
    if (type != nsICSSRule::STYLE_RULE) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  
  result = aGroup->InsertStyleRulesAt(aIndex, rules);
  NS_ENSURE_SUCCESS(result, result);
  DidDirty();
  for (counter = 0; counter < rulecount; counter++) {
    rule = dont_AddRef((nsICSSRule*)rules->ElementAt(counter));
  
    if (mDocument) {
      mDocument->StyleRuleAdded(this, rule);
    }
  }

  if (loader) {
    loader->RecycleParser(css);
  }

  *_retval = aIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSStyleSheet::ReplaceRuleInGroup(nsICSSGroupRule* aGroup,
                                      nsICSSRule* aOld, nsICSSRule* aNew)
{
  nsresult result;
  NS_PRECONDITION(mInner && mInner->mComplete,
                  "No replacing in an incomplete sheet!");
#ifdef DEBUG
  {
    nsCOMPtr<nsIStyleSheet> groupSheet;
    aGroup->GetStyleSheet(*getter_AddRefs(groupSheet));
    NS_ASSERTION(this == groupSheet, "group doesn't belong to this sheet");
  }
#endif
  result = WillDirty();
  NS_ENSURE_SUCCESS(result, result);

  result = aGroup->ReplaceStyleRule(aOld, aNew);
  DidDirty();
  return result;
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
nsCSSStyleSheet::StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify)
{
#ifdef DEBUG
  nsCOMPtr<nsIStyleSheet> styleSheet(do_QueryInterface(aSheet));
  NS_ASSERTION(styleSheet, "Sheet not implementing nsIStyleSheet!\n");
  nsCOMPtr<nsIStyleSheet> parentSheet;
  aSheet->GetParentSheet(*getter_AddRefs(parentSheet));
  nsCOMPtr<nsIStyleSheet> thisSheet;
  QueryInterface(NS_GET_IID(nsIStyleSheet), getter_AddRefs(thisSheet));
  NS_ASSERTION(thisSheet == parentSheet, "We are being notified of a sheet load for a sheet that is not our child!\n");
#endif
  
  if (mDocument && aNotify) {
    nsCOMPtr<nsICSSImportRule> ownerRule;
    aSheet->GetOwnerRule(getter_AddRefs(ownerRule));
    
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, PR_TRUE);

    // XXXldb @import rules shouldn't even implement nsIStyleRule (but
    // they do)!
    nsCOMPtr<nsIStyleRule> styleRule(do_QueryInterface(ownerRule));
    
    mDocument->StyleRuleAdded(this, styleRule);
  }

  return NS_OK;
}

nsresult
NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult)
{
  nsCSSStyleSheet  *it = new nsCSSStyleSheet();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
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

MOZ_DECL_CTOR_COUNTER(RuleProcessorData)

RuleProcessorData::RuleProcessorData(nsPresContext* aPresContext,
                                     nsIContent* aContent, 
                                     nsRuleWalker* aRuleWalker,
                                     nsCompatibility* aCompat /*= nsnull*/)
{
  MOZ_COUNT_CTOR(RuleProcessorData);

  NS_PRECONDITION(aPresContext, "null pointer");
  NS_ASSERTION(!aContent || aContent->IsContentOfType(nsIContent::eELEMENT),
               "non-element leaked into SelectorMatches");

  mPresContext = aPresContext;
  mContent = aContent;
  mParentContent = nsnull;
  mRuleWalker = aRuleWalker;
  mScopedRoot = nsnull;

  mContentTag = nsnull;
  mContentID = nsnull;
  mStyledContent = nsnull;
  mIsHTMLContent = PR_FALSE;
  mIsHTMLLink = PR_FALSE;
  mIsSimpleXLink = PR_FALSE;
  mIsChecked = PR_FALSE;
  mLinkState = eLinkState_Unknown;
  mEventState = NS_EVENT_STATE_UNSPECIFIED;
  mNameSpaceID = kNameSpaceID_Unknown;
  mPreviousSiblingData = nsnull;
  mParentData = nsnull;
  mLanguage = nsnull;

  // get the compat. mode (unless it is provided)
  if(!aCompat) {
    mCompatMode = mPresContext->CompatibilityMode();
  } else {
    mCompatMode = *aCompat;
  }


  if(aContent){
    // we hold no ref to the content...
    mContent = aContent;

    // get the namespace
    aContent->GetNameSpaceID(&mNameSpaceID);

    // get the tag and parent
    mContentTag = aContent->Tag();
    mParentContent = aContent->GetParent();

    // get the event state
    mPresContext->EventStateManager()->GetContentState(aContent, mEventState);

    // get the styledcontent interface and the ID
    if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIStyledContent), (void**)&mStyledContent))) {
      NS_ASSERTION(mStyledContent, "Succeeded but returned null");
      mStyledContent->GetID(&mContentID);
    }

    // see if there are attributes for the content
    mHasAttributes = aContent->GetAttrCount() > 0;

    // check for HTMLContent and Link status
    if (aContent->IsContentOfType(nsIContent::eHTML)) 
      mIsHTMLContent = PR_TRUE;

    // if HTML content and it has some attributes, check for an HTML link
    // NOTE: optimization: cannot be a link if no attributes (since it needs an href)
    if (mIsHTMLContent && mHasAttributes) {
      // check if it is an HTML Link
      if(nsStyleUtil::IsHTMLLink(aContent, mContentTag, mPresContext, &mLinkState)) {
        mIsHTMLLink = PR_TRUE;
      }
    } 

    // if not an HTML link, check for a simple xlink (cannot be both HTML link and xlink)
    // NOTE: optimization: cannot be an XLink if no attributes (since it needs an 
    if(!mIsHTMLLink &&
       mHasAttributes && 
       !(mIsHTMLContent || aContent->IsContentOfType(nsIContent::eXUL)) && 
       nsStyleUtil::IsSimpleXlink(aContent, mPresContext, &mLinkState)) {
      mIsSimpleXLink = PR_TRUE;
    } 

    if (mIsHTMLContent) {
      PRBool isChecked = PR_FALSE;
      if (mContentTag == nsHTMLAtoms::option) {
        nsCOMPtr<nsIDOMHTMLOptionElement> optEl = do_QueryInterface(mContent);
        optEl->GetSelected(&isChecked);
      } else if (mContentTag == nsHTMLAtoms::input) {
        nsCOMPtr<nsIDOMHTMLInputElement> inputEl = do_QueryInterface(mContent);
        inputEl->GetChecked(&isChecked);
      }
      mIsChecked = isChecked;
    }
  }
}

RuleProcessorData::~RuleProcessorData()
{
  MOZ_COUNT_DTOR(RuleProcessorData);

  if (mPreviousSiblingData)
    mPreviousSiblingData->Destroy(mPresContext);
  if (mParentData)
    mParentData->Destroy(mPresContext);

  NS_IF_RELEASE(mContentID);
  NS_IF_RELEASE(mStyledContent);

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
        nsresult attrState = content->GetAttr(kNameSpaceID_XML,
                                              nsHTMLAtoms::lang, value);
        if (attrState != NS_CONTENT_ATTR_HAS_VALUE &&
            content->IsContentOfType(nsIContent::eHTML)) {
          attrState = content->GetAttr(kNameSpaceID_None,
                                       nsHTMLAtoms::lang, value);
        }
        if (attrState == NS_CONTENT_ATTR_HAS_VALUE) {
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

inline PRBool IsEventPseudo(nsIAtom* aAtom)
{
  return nsCSSPseudoClasses::active == aAtom || 
         nsCSSPseudoClasses::mozDragOver == aAtom || 
         nsCSSPseudoClasses::focus == aAtom || 
         nsCSSPseudoClasses::hover == aAtom ||
         nsCSSPseudoClasses::target == aAtom; 
         // XXX selected, enabled, disabled, selection?
}

inline PRBool IsLinkPseudo(nsIAtom* aAtom)
{
  return PRBool ((nsCSSPseudoClasses::link == aAtom) || 
                 (nsCSSPseudoClasses::visited == aAtom) ||
                 (nsCSSPseudoClasses::mozAnyLink == aAtom));
}

// Return whether we should apply a "global" (i.e., universal-tag)
// selector for event states in quirks mode.  Note that
// |data.mIsHTMLLink| is checked separately by the caller, so we return
// false for |nsHTMLAtoms::a|, which here means a named anchor.
inline PRBool IsQuirkEventSensitive(nsIAtom *aContentTag)
{
  return PRBool ((nsHTMLAtoms::button == aContentTag) ||
                 (nsHTMLAtoms::img == aContentTag)    ||
                 (nsHTMLAtoms::input == aContentTag)  ||
                 (nsHTMLAtoms::label == aContentTag)  ||
                 (nsHTMLAtoms::select == aContentTag) ||
                 (nsHTMLAtoms::textarea == aContentTag));
}


static PRBool IsSignificantChild(nsIContent* aChild, PRBool aAcceptNonWhitespaceText)
{
  nsIAtom *tag = aChild->Tag(); // skip text, comments, and PIs
  if ((tag != nsLayoutAtoms::textTagName) && 
      (tag != nsLayoutAtoms::commentTagName) &&
      (tag != nsLayoutAtoms::processingInstructionTagName)) {
    return PR_TRUE;
  }

  if (aAcceptNonWhitespaceText) {
    if (tag == nsLayoutAtoms::textTagName) {  // skip only whitespace text
      nsCOMPtr<nsITextContent> text = do_QueryInterface(aChild);

      if (text && !text->IsOnlyWhitespace())
        return PR_TRUE;
    }
  }

  return PR_FALSE;
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
static PRBool SelectorMatches(RuleProcessorData &data,
                              nsCSSSelector* aSelector,
                              PRInt32 aStateMask, // states NOT to test
                              nsIAtom* aAttribute, // attribute NOT to test
                              PRInt8 aNegationIndex) 

{
  // if we are dealing with negations, reverse the values of PR_TRUE and PR_FALSE
  PRBool localFalse = 0 < aNegationIndex;
  PRBool localTrue = 0 == aNegationIndex;
  PRBool result = localTrue;

  // Do not perform the test if aNegationIndex==1
  // because it then contains only negated IDs, classes, attributes and pseudo-
  // classes
  if (1 != aNegationIndex) {
    if (kNameSpaceID_Unknown != aSelector->mNameSpace) {
      if (data.mNameSpaceID != aSelector->mNameSpace) {
        result = localFalse;
      }
    }
    if (localTrue == result) {
      if ((nsnull != aSelector->mTag) && (aSelector->mTag != data.mContentTag)) {
        result = localFalse;
      }
    }
    // optimization : bail out early if we can
    if (!result) {
      return PR_FALSE;
    }
  }

  result = PR_TRUE;

  // test for pseudo class match
  // first-child, root, lang, active, focus, hover, link, visited...
  // XXX disabled, enabled, selected, selection
  for (nsAtomStringList* pseudoClass = aSelector->mPseudoClassList;
       pseudoClass && result; pseudoClass = pseudoClass->mNext) {
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
                 !IsSignificantChild(firstChild, acceptNonWhitespace));
      }
      result = localTrue == (data.mContent == firstChild);
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
                 !IsSignificantChild(lastChild, acceptNonWhitespace));
      }
      result = localTrue == (data.mContent == lastChild);
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
                 !IsSignificantChild(onlyChild, PR_FALSE));
        if (data.mContent == onlyChild) {
          // see if there's any more
          do {
            moreChild = parent->GetChildAt(++index);
          } while (moreChild && !IsSignificantChild(moreChild, PR_FALSE));
        }
      }
      result = localTrue == (data.mContent == onlyChild &&
                             moreChild == nsnull);
    }
    else if (nsCSSPseudoClasses::empty == pseudoClass->mAtom) {
      nsIContent *child = nsnull;
      nsIContent *element = data.mContent;
      PRInt32 index = -1;
      do {
        child = element->GetChildAt(++index);
        // stop at first non-comment and non-whitespace node
      } while (child && !IsSignificantChild(child, PR_TRUE));
      result = localTrue == (child == nsnull);
    }
    else if (nsCSSPseudoClasses::root == pseudoClass->mAtom) {
      if (data.mParentContent) {
        result = localFalse;
      }
      else {
        result = localTrue;
      }
    }
    else if (nsCSSPseudoClasses::mozBoundElement == pseudoClass->mAtom) {
      // XXXldb How do we know where the selector came from?  And what
      // if there are multiple bindings, and we should be matching the
      // outer one?
      result = (data.mScopedRoot && data.mScopedRoot == data.mContent)
                 ? localTrue : localFalse;
    }
    else if (nsCSSPseudoClasses::lang == pseudoClass->mAtom) {
      NS_ASSERTION(nsnull != pseudoClass->mString, "null lang parameter");
      result = localFalse;
      if (pseudoClass->mString && *pseudoClass->mString) {
        // We have to determine the language of the current element.  Since
        // this is currently no property and since the language is inherited
        // from the parent we have to be prepared to look at all parent
        // nodes.  The language itself is encoded in the LANG attribute.
        const nsString* lang = data.GetLang();
        if (lang && !lang->IsEmpty()) { // null check for out-of-memory
          result = localTrue == nsStyleUtil::DashMatchCompare(*lang,
                                    nsDependentString(pseudoClass->mString), 
                                    nsCaseInsensitiveStringComparator());
        }
        else {
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
                result = localTrue;
                break;
              }
              begin = end + 1;
            }
          }
        }
      }
    }
    else if (IsEventPseudo(pseudoClass->mAtom)) {
      // check if the element is event-sensitive
      if (data.mCompatMode == eCompatibility_NavQuirks &&
          // global selector (but don't check .class):
          !aSelector->mTag && !aSelector->mIDList && !aSelector->mAttrList &&
          // This (or the other way around) both make :not() asymmetric
          // in quirks mode (and it's hard to work around since we're
          // testing the current mNegations, not the first
          // (unnegated)). This at least makes it closer to the spec.
          aNegationIndex == 0 &&
          // :hover or :active
          (nsCSSPseudoClasses::active == pseudoClass->mAtom ||
           nsCSSPseudoClasses::hover == pseudoClass->mAtom) &&
          // important for |IsQuirkEventSensitive|:
          data.mIsHTMLContent && !data.mIsHTMLLink &&
          !IsQuirkEventSensitive(data.mContentTag)) {
        // In quirks mode, only make certain elements sensitive to
        // selectors ":hover" and ":active".
        result = localFalse;
      } else {
        if (nsCSSPseudoClasses::active == pseudoClass->mAtom) {
          result = (aStateMask & NS_EVENT_STATE_ACTIVE) ||
                   (localTrue == (0 != (data.mEventState & NS_EVENT_STATE_ACTIVE)));
        }
        else if (nsCSSPseudoClasses::focus == pseudoClass->mAtom) {
          result = (aStateMask & NS_EVENT_STATE_FOCUS) ||
                   (localTrue == (0 != (data.mEventState & NS_EVENT_STATE_FOCUS)));
        }
        else if (nsCSSPseudoClasses::hover == pseudoClass->mAtom) {
          result = (aStateMask & NS_EVENT_STATE_HOVER) ||
                   (localTrue == (0 != (data.mEventState & NS_EVENT_STATE_HOVER)));
        }
        else if (nsCSSPseudoClasses::mozDragOver == pseudoClass->mAtom) {
          result = (aStateMask & NS_EVENT_STATE_DRAGOVER) ||
                   (localTrue == (0 != (data.mEventState & NS_EVENT_STATE_DRAGOVER)));
        }
        else if (nsCSSPseudoClasses::target == pseudoClass->mAtom) {
          result = (aStateMask & NS_EVENT_STATE_URLTARGET) ||
                   (localTrue == (0 != (data.mEventState & NS_EVENT_STATE_URLTARGET)));
        }
      } 
    }
    else if (IsLinkPseudo(pseudoClass->mAtom)) {
      if (data.mIsHTMLLink || data.mIsSimpleXLink) {
        if (nsCSSPseudoClasses::mozAnyLink == pseudoClass->mAtom) {
          result = localTrue;
        }
        else if (nsCSSPseudoClasses::link == pseudoClass->mAtom) {
          result = localTrue == (eLinkState_Unvisited == data.mLinkState);
        }
        else if (nsCSSPseudoClasses::visited == pseudoClass->mAtom) {
          result = localTrue == (eLinkState_Visited == data.mLinkState);
        }
      }
      else {
        result = localFalse;  // not a link
      }
    }
    else if (nsCSSPseudoClasses::checked == pseudoClass->mAtom) {
      // This pseudoclass matches the selected state on the following elements:
      //  <option>
      //  <input type=checkbox>
      //  <input type=radio>
      if (!(aStateMask & NS_EVENT_STATE_CHECKED))
        result = data.mIsChecked ? localTrue : localFalse;
    }
    else {
      NS_ERROR("CSS parser parsed a pseudo-class that we do not handle");
      result = PR_FALSE;  // unknown pseudo class
    }
  }

  // namespace/tag match

  if (result && aSelector->mAttrList) {
    // test for attribute match
    if (!data.mHasAttributes && !aAttribute) {
      // if no attributes on the content, no match
      result = localFalse;
    } else {
      result = localTrue;
      nsAttrSelector* attr = aSelector->mAttrList;
      do {
        if (attr->mAttr == aAttribute) {
          // XXX we should really have a namespace, not just an attr
          // name, in HasAttributeDependentStyle!
          result = PR_TRUE;
        }
        else if (attr->mNameSpace == kNameSpaceID_Unknown) {
          // Attr selector with a wildcard namespace.  We have to examine all
          // the attributes on our content node....  This sort of selector is
          // essentially a boolean OR, over all namespaces, of equivalent attr
          // selectors with those namespaces.  So to evaluate whether it
          // matches, evaluate for each namespace (the only namespaces that
          // have a chance at matching, of course, are ones that the element
          // actually has attributes in), short-circuiting if we ever match.
          // Then deal with the localFalse/localTrue stuff.
          PRUint32 attrCount = data.mContent->GetAttrCount();
          PRInt32 nameSpaceID;
          nsCOMPtr<nsIAtom> name;
          nsCOMPtr<nsIAtom> prefix;
          PRBool attrSelectorMatched = PR_FALSE;
          for (PRUint32 i = 0; i < attrCount; ++i) {
#ifdef DEBUG
            nsresult attrState =
#endif
              data.mContent->GetAttrNameAt(i, &nameSpaceID,
                                           getter_AddRefs(name),
                                           getter_AddRefs(prefix));
            NS_ASSERTION(NS_SUCCEEDED(attrState),
                         "GetAttrCount lied or GetAttrNameAt failed");
            if (name != attr->mAttr) {
              continue;
            }
            if (attr->mFunction == NS_ATTR_FUNC_SET) {
              attrSelectorMatched = PR_TRUE;
            } else {
              nsAutoString value;
#ifdef DEBUG
              attrState =
#endif
                data.mContent->GetAttr(nameSpaceID, name, value);
              NS_ASSERTION(NS_SUCCEEDED(attrState) &&
                           NS_CONTENT_ATTR_NOT_THERE != attrState,
                           "GetAttrNameAt lied or GetAttr failed");
              attrSelectorMatched = AttrMatchesValue(attr, value);
            }

            // At this point |attrSelectorMatched| has been set by us
            // explicitly in this loop.  If it's PR_FALSE, we may still match
            // -- the content may have another attribute with the same name but
            // in a different namespace.  But if it's PR_TRUE, we are done (we
            // can short-circuit the boolean OR described above).
            if (attrSelectorMatched) {
              break;
            }
          }
          
          // Now adjust for a possible negation
          result = localTrue == attrSelectorMatched;
        }
        else if (!data.mContent->HasAttr(attr->mNameSpace, attr->mAttr)) {
          result = localFalse;
        }
        else if (attr->mFunction != NS_ATTR_FUNC_SET) {
          nsAutoString value;
#ifdef DEBUG
          nsresult attrState =
#endif
              data.mContent->GetAttr(attr->mNameSpace, attr->mAttr, value);
          NS_ASSERTION(NS_SUCCEEDED(attrState) &&
                       NS_CONTENT_ATTR_NOT_THERE != attrState,
                       "HasAttr lied or GetAttr failed");
          result = localTrue == AttrMatchesValue(attr, value);
        }
        
        attr = attr->mNext;
      } while (attr && result);
    }
  }
  if (result && (aSelector->mIDList || aSelector->mClassList)) {
    // test for ID & class match
    result = localFalse;
    if (data.mStyledContent) {
      // case sensitivity: bug 93371
      PRBool isCaseSensitive = data.mCompatMode != eCompatibility_NavQuirks;
      nsAtomList* IDList = aSelector->mIDList;
      if (nsnull == IDList ||
          (aAttribute && aAttribute == data.mContent->GetIDAttributeName())) {
        result = PR_TRUE;
      }
      else if (nsnull != data.mContentID) {
        result = PR_TRUE;
        if (isCaseSensitive) {
          do {
            if (localTrue == (IDList->mAtom != data.mContentID)) {
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
            nsDependentCString id2(id2Str);
            if (localTrue !=
                id1.Equals(id2, nsCaseInsensitiveCStringComparator())) {
              result = PR_FALSE;
              break;
            }
            IDList = IDList->mNext;
          } while (IDList);
        }
      }
      
      if (result &&
          (!aAttribute || aAttribute != data.mContent->GetClassAttributeName())) {
        nsAtomList* classList = aSelector->mClassList;
        while (nsnull != classList) {
          if (localTrue == (!data.mStyledContent->HasClass(classList->mAtom, isCaseSensitive))) {
            result = PR_FALSE;
            break;
          }
          classList = classList->mNext;
        }
      }
    }
  }
  
  // apply SelectorMatches to the negated selectors in the chain
  if (result && (nsnull != aSelector->mNegations)) {
    result = SelectorMatches(data, aSelector->mNegations, aStateMask,
                             aAttribute, aNegationIndex+1);
  }
  return result;
}

// Right now, there are four operators:
//   PRUnichar(0), the descendent combinator, is greedy
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
            if (content->IsContentOfType(nsIContent::eELEMENT)) {
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
    if (SelectorMatches(*data, selector, 0, nsnull, 0)) {
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

  if (SelectorMatches(*data, aSelector, 0, nsnull, 0)) {
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
  NS_PRECONDITION(aData->mContent->IsContentOfType(nsIContent::eELEMENT),
                  "content must be element");

  RuleCascadeData* cascade = GetRuleCascade(aData->mPresContext);

  if (cascade) {
    nsIStyledContent* styledContent = aData->mStyledContent;
    const nsAttrValue* classes = nsnull;
    if (styledContent)
      classes = styledContent->GetClasses();
    
    cascade->mRuleHash.EnumerateAllRules(aData->mNameSpaceID,
                                         aData->mContentTag,
                                         aData->mContentID,
                                         classes,
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
      if (SelectorMatches(*data, selector, 0, nsnull, 0)) {
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
                  aData->mContent->IsContentOfType(nsIContent::eELEMENT),
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

  if (SelectorMatches(*data, selector, data->mStateMask, nsnull, 0) &&
      SelectorMatchesTree(*data, selector->mNext)) {
    if (IsSiblingOperator(selector->mOperator))
      enumData->change = nsReStyleHint(enumData->change | eReStyle_LaterSiblings);
    else
      enumData->change = nsReStyleHint(enumData->change | eReStyle_Self);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsCSSRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData,
                                           nsReStyleHint* aResult)
{
  NS_PRECONDITION(aData->mContent->IsContentOfType(nsIContent::eELEMENT),
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

  if (SelectorMatches(*data, selector, 0, data->mAttribute, 0) &&
      SelectorMatchesTree(*data, selector->mNext)) {
    if (IsSiblingOperator(selector->mOperator))
      enumData->change = nsReStyleHint(enumData->change | eReStyle_LaterSiblings);
    else
      enumData->change = nsReStyleHint(enumData->change | eReStyle_Self);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsCSSRuleProcessor::HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                               nsReStyleHint* aResult)
{
  NS_PRECONDITION(aData->mContent->IsContentOfType(nsIContent::eELEMENT),
                  "content must be element");

  AttributeEnumData data(aData);

  // Since we always have :-moz-any-link (and almost always have :link
  // and :visited rules from prefs), rather than hacking AddRule below
  // to add |href| to the hash, we'll just handle it here.
  if (aData->mAttribute == nsHTMLAtoms::href &&
      aData->mIsHTMLContent &&
      (aData->mContentTag == nsHTMLAtoms::a ||
       aData->mContentTag == nsHTMLAtoms::area ||
       aData->mContentTag == nsHTMLAtoms::link)) {
    data.change = nsReStyleHint(data.change | eReStyle_Self);
  }
  // XXX What about XLinks?

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
        (pseudoClass->mAtom == nsCSSPseudoClasses::target)) {
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
InsertRuleByWeight(nsISupports* aRule, void* aData)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  CascadeEnumData* data = (CascadeEnumData*)aData;
  PRInt32 type = nsICSSRule::UNKNOWN_RULE;
  rule->GetType(type);

  if (nsICSSRule::STYLE_RULE == type) {
    nsICSSStyleRule* styleRule = (nsICSSStyleRule*)rule;

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
    nsICSSGroupRule* groupRule = (nsICSSGroupRule*)rule;
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

  if (bSheetApplicable && sheet->UseForMedium(data->mPresContext->Medium())) {
    nsCSSStyleSheet* child = sheet->mFirstChild;
    while (child) {
      CascadeSheetRulesInto(child, data);
      child = child->mNext;
    }

    if (sheet->mInner && sheet->mInner->mOrderedRules) {
      sheet->mInner->mOrderedRules->EnumerateForwards(InsertRuleByWeight, data);
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
