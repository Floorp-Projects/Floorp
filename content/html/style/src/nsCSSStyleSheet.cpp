/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsICSSStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsICSSStyleRule.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsILinkHandler.h"
#include "nsHTMLAtoms.h"
#include "nsIFrame.h"
#include "nsString.h"
#include "nsIPtr.h"

//#define DEBUG_REFS
//#define DEBUG_RULES

static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);

NS_DEF_PTR(nsIHTMLContent);
NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleRule);
NS_DEF_PTR(nsICSSStyleRule);
NS_DEF_PTR(nsIURL);
NS_DEF_PTR(nsISupportsArray);
NS_DEF_PTR(nsICSSStyleSheet);

// ----------------------
// Rule hash key
//
class RuleKey: public nsHashKey {
public:
  RuleKey(nsIAtom* aAtom);
  RuleKey(const RuleKey& aKey);
  virtual ~RuleKey(void);
  virtual PRUint32 HashValue(void) const;
  virtual PRBool Equals(const nsHashKey *aKey) const;
  virtual nsHashKey *Clone(void) const;
  nsIAtom*  mAtom;
};

RuleKey::RuleKey(nsIAtom* aAtom)
{
  mAtom = aAtom;
  NS_ADDREF(mAtom);
}

RuleKey::RuleKey(const RuleKey& aKey)
{
  mAtom = aKey.mAtom;
  NS_ADDREF(mAtom);
}

RuleKey::~RuleKey(void)
{
  NS_RELEASE(mAtom);
}

PRUint32 RuleKey::HashValue(void) const
{
  return (PRUint32)mAtom;
}

PRBool RuleKey::Equals(const nsHashKey* aKey) const
{
  return PRBool (((RuleKey*)aKey)->mAtom == mAtom);
}

nsHashKey* RuleKey::Clone(void) const
{
  return new RuleKey(*this);
}

struct RuleValue {
  RuleValue(nsICSSStyleRule* aRule, PRInt32 aIndex)
  {
    mRule = aRule;
    mIndex = aIndex;
    mNext = nsnull;
  }
  ~RuleValue(void)
  {
    if (nsnull != mNext) {
      delete mNext;
    }
  }

  nsICSSStyleRule*  mRule;
  PRInt32           mIndex;
  RuleValue*        mNext;
};

// ------------------------------
// Rule hash table
//

// Enumerator callback function.
typedef void (*RuleEnumFunc)(nsICSSStyleRule* aRule, void *aData);

class RuleHash {
public:
  RuleHash(void);
  ~RuleHash(void);
  void AppendRule(nsICSSStyleRule* aRule);
  void EnumerateAllRules(nsIAtom* aTag, nsIAtom* aID, nsIAtom* aClass, 
                         RuleEnumFunc aFunc, void* aData);
  void EnumerateTagRules(nsIAtom* aTag,
                         RuleEnumFunc aFunc, void* aData);
protected:
  void AppendRuleToTable(nsHashtable& aTable, nsIAtom* aAtom, nsICSSStyleRule* aRule);

  PRInt32     mRuleCount;
  nsHashtable mTagTable;
  nsHashtable mIdTable;
  nsHashtable mClassTable;
};

RuleHash::RuleHash(void)
  : mRuleCount(0),
    mTagTable(), mIdTable(), mClassTable()
{
}

static PRBool DeleteValue(nsHashKey* aKey, void* aValue)
{
  delete ((RuleValue*)aValue);
  return PR_TRUE;
}

RuleHash::~RuleHash(void)
{
  mTagTable.Enumerate(DeleteValue);
  mIdTable.Enumerate(DeleteValue);
  mClassTable.Enumerate(DeleteValue);
}

void RuleHash::AppendRuleToTable(nsHashtable& aTable, nsIAtom* aAtom, nsICSSStyleRule* aRule)
{
  RuleKey key(aAtom);
  RuleValue*  value = (RuleValue*)aTable.Get(&key);

  if (nsnull == value) {
    value = new RuleValue(aRule, mRuleCount++);
    aTable.Put(&key, value);
  }
  else {
    while (nsnull != value->mNext) {
      value = value->mNext;
    }
    value->mNext = new RuleValue(aRule, mRuleCount++);
  }
}

void RuleHash::AppendRule(nsICSSStyleRule* aRule)
{
  nsCSSSelector*  selector = aRule->FirstSelector();
  if (nsnull != selector->mID) {
    AppendRuleToTable(mIdTable, selector->mID, aRule);
  }
  else if (nsnull != selector->mClass) {
    AppendRuleToTable(mClassTable, selector->mClass, aRule);
  }
  else if (nsnull != selector->mTag) {
    AppendRuleToTable(mTagTable, selector->mTag, aRule);
  }
}

void RuleHash::EnumerateAllRules(nsIAtom* aTag, nsIAtom* aID, nsIAtom* aClass, 
                                 RuleEnumFunc aFunc, void* aData)
{
  RuleValue*  tagValue = nsnull;
  RuleValue*  idValue = nsnull;
  RuleValue*  classValue = nsnull;

  if (nsnull != aTag) {
    RuleKey tagKey(aTag);
    tagValue = (RuleValue*)mTagTable.Get(&tagKey);
  }
  if (nsnull != aID) {
    RuleKey idKey(aID);
    idValue = (RuleValue*)mIdTable.Get(&idKey);
  }
  if (nsnull != aClass) {
    RuleKey classKey(aClass);
    classValue = (RuleValue*)mClassTable.Get(&classKey);
  }

  PRInt32 tagIndex    = ((nsnull != tagValue)   ? tagValue->mIndex    : mRuleCount);
  PRInt32 idIndex     = ((nsnull != idValue)    ? idValue->mIndex     : mRuleCount);
  PRInt32 classIndex  = ((nsnull != classValue) ? classValue->mIndex  : mRuleCount);

  while ((nsnull != tagValue) || (nsnull != idValue) || (nsnull != classValue)) {
    if ((tagIndex < idIndex) && (tagIndex < classIndex)) {
      (*aFunc)(tagValue->mRule, aData);
      tagValue = tagValue->mNext;
      tagIndex = ((nsnull != tagValue) ? tagValue->mIndex : mRuleCount);
    }
    else if (idIndex < classIndex) {
      (*aFunc)(idValue->mRule, aData);
      idValue = idValue->mNext;
      idIndex = ((nsnull != idValue) ? idValue->mIndex : mRuleCount);
    }
    else {
      (*aFunc)(classValue->mRule, aData);
      classValue = classValue->mNext;
      classIndex = ((nsnull != classValue) ? classValue->mIndex : mRuleCount);
    }
  }
}

void RuleHash::EnumerateTagRules(nsIAtom* aTag, RuleEnumFunc aFunc, void* aData)
{
  RuleKey tagKey(aTag);
  RuleValue*  value = (RuleValue*)mTagTable.Get(&tagKey);

  while (nsnull != value) {
    (*aFunc)(value->mRule, aData);
    value = value->mNext;
  }
}


// -------------------------------
// CSS Style Sheet
//
class CSSStyleSheetImpl : public nsICSSStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleSheetImpl(nsIURL* aURL);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIAtom* aPseudoTag,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual nsIURL* GetURL(void);

  virtual PRBool ContainsStyleSheet(nsIURL* aURL);

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet);

  // XXX do these belong here or are they generic?
  virtual void PrependStyleRule(nsICSSStyleRule* aRule);
  virtual void AppendStyleRule(nsICSSStyleRule* aRule);

  virtual PRInt32   StyleRuleCount(void);
  virtual nsresult  GetStyleRuleAt(PRInt32 aIndex, nsICSSStyleRule*& aRule);

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  CSSStyleSheetImpl(const CSSStyleSheetImpl& aCopy); 
  CSSStyleSheetImpl& operator=(const CSSStyleSheetImpl& aCopy); 

protected:
  virtual ~CSSStyleSheetImpl();

  void ClearHash(void);
  void BuildHash(void);

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURLPtr             mURL;
  nsICSSStyleSheetPtr   mFirstChild;
  nsISupportsArrayPtr   mOrderedRules;
  nsISupportsArrayPtr   mWeightedRules;
  nsICSSStyleSheetPtr   mNext;
  RuleHash*             mRuleHash;
};


void* CSSStyleSheetImpl::operator new(size_t size)
{
  CSSStyleSheetImpl* rv = (CSSStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* CSSStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  CSSStyleSheetImpl* rv = (CSSStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void CSSStyleSheetImpl::operator delete(void* ptr)
{
  CSSStyleSheetImpl* sheet = (CSSStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::delete ptr;
    }
  }
}

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

CSSStyleSheetImpl::CSSStyleSheetImpl(nsIURL* aURL)
  : nsICSSStyleSheet(),
    mURL(nsnull), mFirstChild(nsnull), 
    mOrderedRules(nsnull), mWeightedRules(nsnull), 
    mNext(nsnull),
    mRuleHash(nsnull)
{
  NS_INIT_REFCNT();
  mURL.SetAddRef(aURL);
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSStyleSheet size: %d\n", gInstanceCount, sizeof(*this));
#endif
}

CSSStyleSheetImpl::~CSSStyleSheetImpl()
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSStyleSheet\n", gInstanceCount);
#endif
  ClearHash();
}

NS_IMPL_ADDREF(CSSStyleSheetImpl)
NS_IMPL_RELEASE(CSSStyleSheetImpl)

nsresult CSSStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                           void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kICSSStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsICSSStyleSheet*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

static PRBool SelectorMatches(nsIPresContext* aPresContext,
                              nsCSSSelector* aSelector, nsIContent* aContent)
{
  PRBool  result = PR_FALSE;
  nsIAtom*  contentTag = aContent->GetTag();

  if ((nsnull == aSelector->mTag) || (aSelector->mTag == contentTag)) {
    if ((nsnull != aSelector->mClass) || (nsnull != aSelector->mID) || 
        (nsnull != aSelector->mPseudoClass)) {
      nsIHTMLContent* htmlContent;
      if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
        nsIAtom* contentClass = htmlContent->GetClass();
        nsIAtom* contentID = htmlContent->GetID();
        if ((nsnull == aSelector->mClass) || (aSelector->mClass == contentClass)) {
          if ((nsnull == aSelector->mID) || (aSelector->mID == contentID)) {
            if ((contentTag == nsHTMLAtoms::a) && (nsnull != aSelector->mPseudoClass)) {
              // test link state
              nsILinkHandler* linkHandler;

              if (NS_OK == aPresContext->GetLinkHandler(&linkHandler)) {
                nsAutoString base, href;  // XXX base??
                htmlContent->GetAttribute("href", href);

                nsIURL* docURL = nsnull;
                nsIDocument* doc = nsnull;
                aContent->GetDocument(doc);
                if (nsnull != doc) {
                  docURL = doc->GetDocumentURL();
                  NS_RELEASE(doc);
                }

                nsAutoString absURLSpec;
                nsresult rv = NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
                NS_IF_RELEASE(docURL);

                nsLinkState  state;
                if (NS_OK == linkHandler->GetLinkState(absURLSpec, state)) {
                  switch (state) {
                    case eLinkState_Unvisited:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::link);
                      break;
                    case eLinkState_Visited:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::visited);
                      break;
                    case eLinkState_OutOfDate:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::outOfDate);
                      break;
                    case eLinkState_Active:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::active);
                      break;
                    case eLinkState_Hover:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::hover);
                      break;
                  }
                }
                NS_RELEASE(linkHandler);
              }
            }
            else {
              result = PR_TRUE;
            }
          }
        }
        NS_RELEASE(htmlContent);
        NS_IF_RELEASE(contentClass);
        NS_IF_RELEASE(contentID);
      }
    }
    else {
      result = PR_TRUE;
    }
  }
  NS_IF_RELEASE(contentTag);
  return result;
}

struct ContentEnumData {
  ContentEnumData(nsIPresContext* aPresContext, nsIContent* aContent, 
                  nsIFrame* aParentFrame, nsISupportsArray* aResults)
  {
    mPresContext = aPresContext;
    mContent = aContent;
    mParentFrame = aParentFrame;
    mResults = aResults;
    mCount = 0;
  }

  nsIPresContext*   mPresContext;
  nsIContent*       mContent;
  nsIFrame*         mParentFrame;
  nsISupportsArray* mResults;
  PRInt32           mCount;
};

void ContentEnumFunc(nsICSSStyleRule* aRule, void* aData)
{
  ContentEnumData* data = (ContentEnumData*)aData;

  nsCSSSelector* selector = aRule->FirstSelector();
  if (SelectorMatches(data->mPresContext, selector, data->mContent)) {
    selector = selector->mNext;
    nsIFrame* frame = data->mParentFrame;
    nsIContent* lastContent = nsnull;
    while ((nsnull != selector) && (nsnull != frame)) { // check compound selectors
      nsIContent* content;
      frame->GetContent(content);
      if ((content != lastContent) && // skip pseudo frames (actually we're skipping pseudo's parent, but same result)
          SelectorMatches(data->mPresContext, selector, content)) {
        selector = selector->mNext;
      }
      frame->GetGeometricParent(frame);
      NS_IF_RELEASE(lastContent);
      lastContent = content;
    }
    NS_IF_RELEASE(lastContent);
    if (nsnull == selector) { // ran out, it matched
      nsIStyleRule* iRule;
      if (NS_OK == aRule->QueryInterface(kIStyleRuleIID, (void**)&iRule)) {
        data->mResults->AppendElement(iRule);
        data->mCount++;
        NS_RELEASE(iRule);
      }
    }
  }
}

#ifdef DEBUG_RULES
static PRBool ContentEnumWrap(nsISupports* aRule, void* aData)
{
  ContentEnumFunc((nsICSSStyleRule*)aRule, aData);
  return PR_TRUE;
}
#endif

PRInt32 CSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsIFrame* aParentFrame,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIContent* parentContent = nsnull;
  if (nsnull != aParentFrame) {
    aParentFrame->GetContent(parentContent);
  }

  if (aContent != parentContent) {  // if not a pseudo frame...
    nsICSSStyleSheet*  child = mFirstChild;
    while (nsnull != child) {
      matchCount += child->RulesMatching(aPresContext, aContent, aParentFrame, aResults);
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }

    if (mWeightedRules.IsNotNull()) {
      if (nsnull == mRuleHash) {
        BuildHash();
      }
      ContentEnumData data(aPresContext, aContent, aParentFrame, aResults);
      nsIAtom* tagAtom = aContent->GetTag();
      nsIAtom* idAtom = nsnull;
      nsIAtom* classAtom = nsnull;

      nsIHTMLContent* htmlContent;
      if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
        idAtom = htmlContent->GetID();
        classAtom = htmlContent->GetClass();
        NS_RELEASE(htmlContent);
      }

      mRuleHash->EnumerateAllRules(tagAtom, idAtom, classAtom, ContentEnumFunc, &data);
      matchCount += data.mCount;

#ifdef DEBUG_RULES
      nsISupportsArray* list1;
      nsISupportsArray* list2;
      NS_NewISupportsArray(&list1);
      NS_NewISupportsArray(&list2);

      data.mResults = list1;
      mRuleHash->EnumerateAllRules(tagAtom, idAtom, classAtom, ContentEnumFunc, &data);
      data.mResults = list2;
      mWeightedRules->EnumerateForwards(ContentEnumWrap, &data);
      NS_ASSERTION(list1->Equals(list2), "lists not equal");
      NS_RELEASE(list1);
      NS_RELEASE(list2);
#endif

      NS_IF_RELEASE(tagAtom);
      NS_IF_RELEASE(idAtom);
      NS_IF_RELEASE(classAtom);
    }
  }
  NS_IF_RELEASE(parentContent);
  return matchCount;
}

struct PseudoEnumData {
  PseudoEnumData(nsIPresContext* aPresContext, nsIAtom* aPseudoTag, 
                 nsIFrame* aParentFrame, nsISupportsArray* aResults)
  {
    mPresContext = aPresContext;
    mPseudoTag = aPseudoTag;
    mParentFrame = aParentFrame;
    mResults = aResults;
    mCount = 0;
  }

  nsIPresContext*   mPresContext;
  nsIAtom*          mPseudoTag;
  nsIFrame*         mParentFrame;
  nsISupportsArray* mResults;
  PRInt32           mCount;
};

void PseudoEnumFunc(nsICSSStyleRule* aRule, void* aData)
{
  PseudoEnumData* data = (PseudoEnumData*)aData;

  nsCSSSelector* selector = aRule->FirstSelector();
  if (selector->mTag == data->mPseudoTag) {
    selector = selector->mNext;
    nsIFrame* frame = data->mParentFrame;
    nsIContent* lastContent = nsnull;
    while ((nsnull != selector) && (nsnull != frame)) { // check compound selectors
      nsIContent* content;
      frame->GetContent(content);
      if ((content != lastContent) && // skip pseudo frames (actually we're skipping pseudo's parent, but same result)
          SelectorMatches(data->mPresContext, selector, content)) {
        selector = selector->mNext;
      }
      frame->GetGeometricParent(frame);
      NS_IF_RELEASE(lastContent);
      lastContent = content;
    }
    NS_IF_RELEASE(lastContent);
    if (nsnull == selector) { // ran out, it matched
      nsIStyleRule* iRule;
      if (NS_OK == aRule->QueryInterface(kIStyleRuleIID, (void**)&iRule)) {
        data->mResults->AppendElement(iRule);
        data->mCount++;
        NS_RELEASE(iRule);
      }
    }
  }
}

#ifdef DEBUG_RULES
static PRBool PseudoEnumWrap(nsISupports* aRule, void* aData)
{
  PseudoEnumFunc((nsICSSStyleRule*)aRule, aData);
  return PR_TRUE;
}
#endif

PRInt32 CSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                         nsIAtom* aPseudoTag,
                                         nsIFrame* aParentFrame,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aPseudoTag, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    matchCount += child->RulesMatching(aPresContext, aPseudoTag, aParentFrame, aResults);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  if (mWeightedRules.IsNotNull()) {
    if (nsnull == mRuleHash) {
      BuildHash();
    }
    PseudoEnumData data(aPresContext, aPseudoTag, aParentFrame, aResults);
    mRuleHash->EnumerateTagRules(aPseudoTag, PseudoEnumFunc, &data);
    matchCount += data.mCount;

#ifdef DEBUG_RULES
    nsISupportsArray* list1;
    nsISupportsArray* list2;
    NS_NewISupportsArray(&list1);
    NS_NewISupportsArray(&list2);
    data.mResults = list1;
    mRuleHash->EnumerateTagRules(aPseudoTag, PseudoEnumFunc, &data);
    data.mResults = list2;
    mWeightedRules->EnumerateForwards(PseudoEnumWrap, &data);
    NS_ASSERTION(list1->Equals(list2), "lists not equal");
    NS_RELEASE(list1);
    NS_RELEASE(list2);
#endif
  }
  return matchCount;
}

nsIURL* CSSStyleSheetImpl::GetURL(void)
{
  return mURL.AddRef();
}

PRBool CSSStyleSheetImpl::ContainsStyleSheet(nsIURL* aURL)
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

  PRBool result = (*mURL == *aURL);

  nsICSSStyleSheet*  child = mFirstChild;
  while ((PR_FALSE == result) && (nsnull != child)) {
    result = child->ContainsStyleSheet(aURL);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }
  return result;
}

void CSSStyleSheetImpl::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (mFirstChild.IsNull()) {
    mFirstChild.SetAddRef(aSheet);
  }
  else {
    nsICSSStyleSheet* child = mFirstChild;
    while (((CSSStyleSheetImpl*)child)->mNext.IsNotNull()) {
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }
    ((CSSStyleSheetImpl*)child)->mNext.SetAddRef(aSheet);
  }
}

void CSSStyleSheetImpl::PrependStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  ClearHash();
  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (mWeightedRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mWeightedRules.AssignPtr()))
      return;
  }
  if (mOrderedRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mOrderedRules.AssignPtr()))
      return;
  }
  PRInt32 index = mWeightedRules->Count();
  while (0 <= --index) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mWeightedRules->ElementAt(index);
    if (rule->GetWeight() > weight) { // insert before rules with equal or lesser weight
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mWeightedRules->InsertElementAt(aRule, index + 1);
  mOrderedRules->InsertElementAt(aRule, 0);
}

void CSSStyleSheetImpl::AppendStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  ClearHash();
  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (mWeightedRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mWeightedRules.AssignPtr()))
      return;
  }
  if (mOrderedRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mOrderedRules.AssignPtr()))
      return;
  }
  PRInt32 count = mWeightedRules->Count();
  PRInt32 index = -1;
  while (++index < count) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mWeightedRules->ElementAt(index);
    if (rule->GetWeight() < weight) { // insert after rules with equal or greater weight (before lower weight)
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mWeightedRules->InsertElementAt(aRule, index);
  mOrderedRules->AppendElement(aRule);
}

PRInt32 CSSStyleSheetImpl::StyleRuleCount(void)
{
  if (mOrderedRules.IsNotNull()) {
    return mOrderedRules->Count();
  }
  return 0;
}

nsresult CSSStyleSheetImpl::GetStyleRuleAt(PRInt32 aIndex, nsICSSStyleRule*& aRule)
{
  nsresult result = NS_ERROR_ILLEGAL_VALUE;

  if (mOrderedRules.IsNotNull()) {
    aRule = (nsICSSStyleRule*)mOrderedRules->ElementAt(aIndex);
    if (nsnull != aRule) {
      result = NS_OK;
    }
  }
  else {
    aRule = nsnull;
  }
  return result;
}

void CSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;
  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("CSS Style Sheet: ", out);
  mURL->ToString(buffer);
  fputs(buffer, out);
  fputs("\n", out);

  const nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    child->List(out, aIndent + 1);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  PRInt32 count = (mWeightedRules.IsNotNull() ? mWeightedRules->Count() : 0);

  for (index = 0; index < count; index++) {
    nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mWeightedRules->ElementAt(index);
    rule->List(out, aIndent);
  }
}

void CSSStyleSheetImpl::ClearHash(void)
{
  if (nsnull != mRuleHash) {
    delete mRuleHash;
    mRuleHash = nsnull;
  }
}

PRBool BuildHashEnum(nsISupports* aRule, void* aHash)
{
  nsICSSStyleRule* rule = (nsICSSStyleRule*)aRule;
  RuleHash* hash = (RuleHash*)aHash;
  hash->AppendRule(rule);
  return PR_TRUE;
}

void CSSStyleSheetImpl::BuildHash(void)
{
  NS_ASSERTION(nsnull == mRuleHash, "clear rule hash first");

  mRuleHash = new RuleHash();
  if ((nsnull != mRuleHash) && mWeightedRules.IsNotNull()) {
    mWeightedRules->EnumerateForwards(BuildHashEnum, mRuleHash);
  }
}

NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURL* aURL)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSStyleSheetImpl  *it = new CSSStyleSheetImpl(aURL);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSStyleSheetIID, (void **) aInstancePtrResult);
}
