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
#include "nsIEventStateManager.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIFrame.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleRuleCollection.h"
#include "nsIDOMNode.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsICSSParser.h"
#include "nsCSSAtoms.h"
#include "nsINameSpaceManager.h"
#include "prlog.h"

//#define DEBUG_REFS
//#define DEBUG_RULES

static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIDOMStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kIDOMCSSStyleSheetIID, NS_IDOMCSSSTYLESHEET_IID);
static NS_DEFINE_IID(kIDOMCSSStyleRuleIID, NS_IDOMCSSSTYLERULE_IID);
static NS_DEFINE_IID(kIDOMCSSStyleRuleCollectionIID, NS_IDOMCSSSTYLERULECOLLECTION_IID);
static NS_DEFINE_IID(kIDOMStyleSheetCollectionIID, NS_IDOMSTYLESHEETCOLLECTION_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

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
    if ((nsnull != mNext) && (nsnull != mNext->mNext)) {
      delete mNext; // don't delete that last in the chain, it's shared
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
  void EnumerateAllRules(nsIAtom* aTag, nsIAtom* aID, const nsVoidArray& aClassList,
                         RuleEnumFunc aFunc, void* aData);
  void EnumerateTagRules(nsIAtom* aTag,
                         RuleEnumFunc aFunc, void* aData);

protected:
  void AppendRuleToTable(nsHashtable& aTable, nsIAtom* aAtom, nsICSSStyleRule* aRule);

  PRInt32     mRuleCount;
  nsHashtable mTagTable;
  nsHashtable mIdTable;
  nsHashtable mClassTable;

  RuleValue** mEnumList;
  PRInt32     mEnumListSize;
  RuleValue   mEndValue;
};

RuleHash::RuleHash(void)
  : mRuleCount(0),
    mTagTable(), mIdTable(), mClassTable(),
    mEnumList(nsnull), mEnumListSize(0),
    mEndValue(nsnull, -1)
{
}

static PRBool DeleteValue(nsHashKey* aKey, void* aValue, void* closure)
{
  delete ((RuleValue*)aValue);
  return PR_TRUE;
}

RuleHash::~RuleHash(void)
{
  mTagTable.Enumerate(DeleteValue);
  mIdTable.Enumerate(DeleteValue);
  mClassTable.Enumerate(DeleteValue);
  if (nsnull != mEnumList) {
    delete [] mEnumList;
  }
}

void RuleHash::AppendRuleToTable(nsHashtable& aTable, nsIAtom* aAtom, nsICSSStyleRule* aRule)
{
  NS_ASSERTION(nsnull != aAtom, "null hash key");

  RuleKey key(aAtom);
  RuleValue*  value = (RuleValue*)aTable.Get(&key);

  if (nsnull == value) {
    value = new RuleValue(aRule, mRuleCount++);
    aTable.Put(&key, value);
    value->mNext = &mEndValue;
  }
  else {
    while (&mEndValue != value->mNext) {
      value = value->mNext;
    }
    value->mNext = new RuleValue(aRule, mRuleCount++);
    value->mNext->mNext = &mEndValue;
  }
  mEndValue.mIndex = mRuleCount;
}

void RuleHash::AppendRule(nsICSSStyleRule* aRule)
{
  nsCSSSelector*  selector = aRule->FirstSelector();
  if (nsnull != selector->mID) {
    AppendRuleToTable(mIdTable, selector->mID, aRule);
  }
  else if (nsnull != selector->mClassList) {
    AppendRuleToTable(mClassTable, selector->mClassList->mAtom, aRule);
  }
  else if (nsnull != selector->mTag) {
    AppendRuleToTable(mTagTable, selector->mTag, aRule);
  }
  else {  // universal tag selector
    AppendRuleToTable(mTagTable, nsCSSAtoms::universalSelector, aRule);
  }
}

void RuleHash::EnumerateAllRules(nsIAtom* aTag, nsIAtom* aID, const nsVoidArray& aClassList, 
                                 RuleEnumFunc aFunc, void* aData)
{
  PRInt32 classCount = aClassList.Count();
  PRInt32 testCount = classCount + 1; // + 1 for universal selector
  if (nsnull != aTag) {
    testCount++;
  }
  if (nsnull != aID) {
    testCount++;
  }

  if (mEnumListSize < testCount) {
    delete [] mEnumList;
    mEnumList = new RuleValue*[testCount];
    mEnumListSize = testCount;
  }

  PRInt32 index;
  PRInt32 valueCount = 0;

  { // universal tag rules
    RuleKey universalKey(nsCSSAtoms::universalSelector);
    RuleValue*  value = (RuleValue*)mTagTable.Get(&universalKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  if (nsnull != aTag) {
    RuleKey tagKey(aTag);
    RuleValue* value = (RuleValue*)mTagTable.Get(&tagKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  if (nsnull != aID) {
    RuleKey idKey(aID);
    RuleValue* value = (RuleValue*)mIdTable.Get(&idKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  for (index = 0; index < classCount; index++) {
    nsIAtom* classAtom = (nsIAtom*)aClassList.ElementAt(index);
    RuleKey classKey(classAtom);
    RuleValue* value = (RuleValue*)mClassTable.Get(&classKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  NS_ASSERTION(valueCount <= testCount, "values exceeded list size");

  if (1 < valueCount) {
    PRInt32 ruleIndex = mRuleCount;
    PRInt32 valueIndex = -1;

    for (index = 0; index < valueCount; index++) {
      if (mEnumList[index]->mIndex < ruleIndex) {
        ruleIndex = mEnumList[index]->mIndex;
        valueIndex = index;
      }
    }
    do {
      (*aFunc)(mEnumList[valueIndex]->mRule, aData);
      mEnumList[valueIndex] = mEnumList[valueIndex]->mNext;
      ruleIndex = mEnumList[valueIndex]->mIndex;
      for (index = 0; index < valueCount; index++) {
        if (mEnumList[index]->mIndex < ruleIndex) {
          ruleIndex = mEnumList[index]->mIndex;
          valueIndex = index;
        }
      }
    } while (ruleIndex < mRuleCount);
  }
  else if (0 < valueCount) {  // fast loop over single value
    RuleValue* value = mEnumList[0];
    do {
      (*aFunc)(value->mRule, aData);
      value = value->mNext;
    } while (&mEndValue != value);
  }
}

void RuleHash::EnumerateTagRules(nsIAtom* aTag, RuleEnumFunc aFunc, void* aData)
{
  RuleKey tagKey(aTag);
  RuleKey universalKey(nsCSSAtoms::universalSelector);
  RuleValue*  tagValue = (RuleValue*)mTagTable.Get(&tagKey);
  RuleValue*  uniValue = (RuleValue*)mTagTable.Get(&universalKey);

  if (nsnull == tagValue) {
    if (nsnull != uniValue) {
      do {
        (*aFunc)(uniValue->mRule, aData);
        uniValue = uniValue->mNext;
      } while (&mEndValue != uniValue);
    }
  }
  else {
    if (nsnull == uniValue) {
      do {
        (*aFunc)(tagValue->mRule, aData);
        tagValue = tagValue->mNext;
      } while (&mEndValue != tagValue);
    }
    else {
      do {
        if (tagValue->mIndex < uniValue->mIndex) {
          (*aFunc)(tagValue->mRule, aData);
          tagValue = tagValue->mNext;
        }
        else {
          (*aFunc)(uniValue->mRule, aData);
          uniValue = uniValue->mNext;
        }
      } while ((&mEndValue != tagValue) || (&mEndValue != uniValue));
    }
  }
}

// -------------------------------
// Style Rule Collection for the DOM
//
class CSSStyleRuleCollectionImpl : public nsIDOMCSSStyleRuleCollection,
                                   public nsIScriptObjectOwner 
{
public:
  CSSStyleRuleCollectionImpl(nsICSSStyleSheet *aStyleSheet);

  NS_DECL_ISUPPORTS

  // nsIDOMCSSStyleRuleCollection interface
  NS_IMETHOD    GetLength(PRUint32* aLength); 
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMCSSStyleRule** aReturn); 

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);
  
  void DropReference() { mStyleSheet = nsnull; }

protected:
  virtual ~CSSStyleRuleCollectionImpl();

  nsICSSStyleSheet*  mStyleSheet;
  void*              mScriptObject;
};

CSSStyleRuleCollectionImpl::CSSStyleRuleCollectionImpl(nsICSSStyleSheet *aStyleSheet)
{
  NS_INIT_REFCNT();
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
  mScriptObject = nsnull;
}

CSSStyleRuleCollectionImpl::~CSSStyleRuleCollectionImpl()
{
}

NS_IMPL_ADDREF(CSSStyleRuleCollectionImpl);
NS_IMPL_RELEASE(CSSStyleRuleCollectionImpl);

nsresult 
CSSStyleRuleCollectionImpl::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIDOMCSSStyleRuleCollectionIID)) {
    nsIDOMCSSStyleRuleCollection *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMCSSStyleRuleCollection *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP    
CSSStyleRuleCollectionImpl::GetLength(PRUint32* aLength)
{
  if (nsnull != mStyleSheet) {
    *aLength = mStyleSheet->StyleRuleCount();
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleCollectionImpl::Item(PRUint32 aIndex, nsIDOMCSSStyleRule** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;
  if (nsnull != mStyleSheet) {
    nsICSSStyleRule *rule;

    result = mStyleSheet->GetStyleRuleAt(aIndex, rule);
    if (NS_OK == result) {
      result = rule->QueryInterface(kIDOMCSSStyleRuleIID, (void **)aReturn);
    }
    NS_RELEASE(rule);
  }
  
  return result;
}

NS_IMETHODIMP
CSSStyleRuleCollectionImpl::GetScriptObject(nsIScriptContext *aContext, 
                                            void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsIDOMCSSStyleRuleCollection *)this;

    // XXX Should be done through factory
    res = NS_NewScriptCSSStyleRuleCollection(aContext, 
                                             supports,
                                             (nsISupports *)mStyleSheet, 
                                             (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
CSSStyleRuleCollectionImpl::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

// -------------------------------
// Imports Collection for the DOM
//
class CSSImportsCollectionImpl : public nsIDOMStyleSheetCollection,
                                 public nsIScriptObjectOwner 
{
public:
  CSSImportsCollectionImpl(nsICSSStyleSheet *aStyleSheet);

  NS_DECL_ISUPPORTS

  // nsIDOMCSSStyleSheetCollection interface
  NS_IMETHOD    GetLength(PRUint32* aLength); 
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn); 

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);
  
  void DropReference() { mStyleSheet = nsnull; }

protected:
  virtual ~CSSImportsCollectionImpl();

  nsICSSStyleSheet*  mStyleSheet;
  void*              mScriptObject;
};

CSSImportsCollectionImpl::CSSImportsCollectionImpl(nsICSSStyleSheet *aStyleSheet)
{
  NS_INIT_REFCNT();
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
  mScriptObject = nsnull;
}

CSSImportsCollectionImpl::~CSSImportsCollectionImpl()
{
}

NS_IMPL_ADDREF(CSSImportsCollectionImpl);
NS_IMPL_RELEASE(CSSImportsCollectionImpl);

nsresult 
CSSImportsCollectionImpl::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIDOMStyleSheetCollectionIID)) {
    nsIDOMStyleSheetCollection *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMStyleSheetCollection *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
CSSImportsCollectionImpl::GetLength(PRUint32* aLength)
{
  if (nsnull != mStyleSheet) {
    *aLength = mStyleSheet->StyleSheetCount();
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
  if (nsnull != mStyleSheet) {
    nsICSSStyleSheet *sheet;

    result = mStyleSheet->GetStyleSheetAt(aIndex, sheet);
    if (NS_OK == result) {
      result = sheet->QueryInterface(kIDOMStyleSheetIID, (void **)aReturn);
    }
    NS_RELEASE(sheet);
  }
  
  return result;
}

NS_IMETHODIMP
CSSImportsCollectionImpl::GetScriptObject(nsIScriptContext *aContext, 
                                            void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsIDOMStyleSheetCollection *)this;

    // XXX Should be done through factory
    res = NS_NewScriptStyleSheetCollection(aContext, 
                                           supports,
                                           (nsISupports *)mStyleSheet, 
                                           (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
CSSImportsCollectionImpl::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

// -------------------------------
// CSS Style Sheet
//
class CSSStyleSheetImpl : public nsICSSStyleSheet, 
                          public nsIDOMCSSStyleSheet, 
                          public nsIScriptObjectOwner {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleSheetImpl();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // basic style sheet data
  NS_IMETHOD Init(nsIURL* aURL);
  NS_IMETHOD GetURL(nsIURL*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD SetTitle(const nsString& aTitle);
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD AppendMedium(nsIAtom* aMedium);

  NS_IMETHOD GetEnabled(PRBool& aEnabled) const;
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // may be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument);
  NS_IMETHOD SetOwningNode(nsIDOMNode* aOwningNode);


  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIStyleContext* aParentContext,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aParentContent,
                                nsIAtom* aPseudoTag,
                                nsIStyleContext* aParentContext,
                                nsISupportsArray* aResults);

  NS_IMETHOD HasStateDependentStyle(nsIPresContext* aPresContext,
                                    nsIContent*     aContent);

  virtual PRBool ContainsStyleSheet(nsIURL* aURL) const;

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet);

  // XXX do these belong here or are they generic?
  virtual void PrependStyleRule(nsICSSStyleRule* aRule);
  virtual void AppendStyleRule(nsICSSStyleRule* aRule);

  virtual PRInt32   StyleRuleCount(void) const;
  virtual nsresult  GetStyleRuleAt(PRInt32 aIndex, nsICSSStyleRule*& aRule) const;

  virtual PRInt32   StyleSheetCount(void) const;
  virtual nsresult  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  // nsIDOMStyleSheet interface
  NS_IMETHOD    GetType(nsString& aType);
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);
  NS_IMETHOD    SetDisabled(PRBool aDisabled);
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly);

  // nsIDOMCSSStyleSheet interface
  NS_IMETHOD    GetOwningNode(nsIDOMNode** aOwningNode);
  NS_IMETHOD    GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet);
  NS_IMETHOD    GetHref(nsString& aHref);
  NS_IMETHOD    GetTitle(nsString& aTitle);
  NS_IMETHOD    GetMedia(nsString& aMedia);
  NS_IMETHOD    GetCssRules(nsIDOMCSSStyleRuleCollection** aCssRules);
  NS_IMETHOD    InsertRule(const nsString& aRule, PRUint32 aIndex, PRUint32* aReturn);
  NS_IMETHOD    DeleteRule(PRUint32 aIndex);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

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
  nsString              mTitle;
  nsISupportsArray*     mMedia;
  nsICSSStyleSheetPtr   mFirstChild;
  nsISupportsArrayPtr   mOrderedRules;
  nsISupportsArrayPtr   mWeightedRules;
  nsICSSStyleSheetPtr   mNext;
  nsICSSStyleSheet*     mParent;
  RuleHash*             mRuleHash;
  nsVoidArray           mStateSelectors;
  CSSStyleRuleCollectionImpl* mRuleCollection;
  CSSImportsCollectionImpl* mImportsCollection;
  nsIDocument*          mDocument;
  nsIDOMNode*           mOwningNode;
  PRBool                mDisabled;
  void*                 mScriptObject;
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
      ::operator delete(ptr);
    }
  }
}

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

CSSStyleSheetImpl::CSSStyleSheetImpl()
  : nsICSSStyleSheet(),
    mURL(nsnull), mTitle(), mMedia(nsnull),
    mFirstChild(nsnull), 
    mOrderedRules(nsnull), mWeightedRules(nsnull), 
    mNext(nsnull),
    mRuleHash(nsnull)
{
  NS_INIT_REFCNT();
  nsCSSAtoms::AddrefAtoms();

  mParent = nsnull;
  mRuleCollection = nsnull;
  mImportsCollection = nsnull;
  mDocument = nsnull;
  mOwningNode = nsnull;
  mDisabled = PR_FALSE;
  mScriptObject = nsnull;
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSStyleSheet size: %d\n", gInstanceCount, sizeof(*this));
#endif
}

static PRBool DropStyleSheetReference(nsISupports* aElement, void *aData)
{
  nsICSSStyleRule *rule = (nsICSSStyleRule *)aElement;

  if (nsnull != rule) {
    rule->SetStyleSheet(nsnull);
  }

  return PR_TRUE;
}

CSSStyleSheetImpl::~CSSStyleSheetImpl()
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSStyleSheet\n", gInstanceCount);
#endif
  NS_IF_RELEASE(mMedia);
  if (mFirstChild.IsNotNull()) {
    nsICSSStyleSheet* child = mFirstChild;
    while (nsnull != child) {
      ((CSSStyleSheetImpl *)child)->mParent = nsnull;
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }
  }
  if (nsnull != mRuleCollection) {
    mRuleCollection->DropReference();
    NS_RELEASE(mRuleCollection);
  }
  if (nsnull != mImportsCollection) {
    mImportsCollection->DropReference();
    NS_RELEASE(mImportsCollection);
  }
  if (mOrderedRules.IsNotNull()) {
    mOrderedRules->EnumerateForwards(DropStyleSheetReference, nsnull);
  }
  ClearHash();
  // XXX The document reference is not reference counted and should
  // not be released. The document will let us know when it is going
  // away.
  nsCSSAtoms::ReleaseAtoms();
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
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMStyleSheetIID)) {
    nsIDOMStyleSheet *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMCSSStyleSheetIID)) {
    nsIDOMCSSStyleSheet *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICSSStyleSheet *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

static const PRUnichar kNullCh = PRUnichar('\0');

static PRBool ValueIncludes(const nsString& aValueList, const nsString& aValue, PRBool aCaseSensitive)
{
  nsAutoString  valueList(aValueList);

  valueList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* value = (PRUnichar*)(const PRUnichar*)aValue.GetUnicode();
  PRUnichar* start = (PRUnichar*)(const PRUnichar*)valueList.GetUnicode();
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsString::IsSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (aCaseSensitive) {
        if (aValue.Equals(value, start)) {
          return PR_TRUE;
        }
      }
      else {
        if (aValue.EqualsIgnoreCase(value, start)) {
          return PR_TRUE;
        }
      }
    }

    start = ++end;
  }
  return PR_FALSE;
}

static const PRUnichar kDashCh = PRUnichar('-');

static PRBool ValueDashMatch(const nsString& aValueList, const nsString& aValue, PRBool aCaseSensitive)
{
  nsAutoString  valueList(aValueList);

  valueList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* value = (PRUnichar*)(const PRUnichar*)aValue.GetUnicode();
  PRUnichar* start = (PRUnichar*)(const PRUnichar*)valueList.GetUnicode();
  PRUnichar* end   = start;

  if (kNullCh != *start) {
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (kDashCh != *end)) { // look for dash or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (aCaseSensitive) {
        if (aValue.Equals(value, start)) {
          return PR_TRUE;
        }
      }
      else {
        if (aValue.EqualsIgnoreCase(value, start)) {
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

static PRBool IsEventPseudo(nsIAtom* aAtom)
{
  return PRBool ((nsCSSAtoms::activePseudo == aAtom) || 
                 (nsCSSAtoms::focusPseudo == aAtom) || 
                 (nsCSSAtoms::hoverPseudo == aAtom)); 
  // XXX selected, enabled, disabled, selection?
}

static PRBool IsLinkPseudo(nsIAtom* aAtom)
{
  return PRBool ((nsCSSAtoms::linkPseudo == aAtom) || 
                 (nsCSSAtoms::outOfDatePseudo == aAtom) || 
                 (nsCSSAtoms::visitedPseudo == aAtom));
}

static PRBool SelectorMatches(nsIPresContext* aPresContext,
                              nsCSSSelector* aSelector, nsIContent* aContent,
                              PRBool aTestState)
{
  PRBool  result = PR_FALSE;
  nsIAtom*  contentTag;
  aContent->GetTag(contentTag);
  PRInt32 nameSpaceID;
  aContent->GetNameSpaceID(nameSpaceID);

  if (((nsnull == aSelector->mTag) || (aSelector->mTag == contentTag)) && 
      ((kNameSpaceID_Unknown == aSelector->mNameSpace) || (nameSpaceID == aSelector->mNameSpace))) {
    result = PR_TRUE;
    // namespace/tag match
    if (nsnull != aSelector->mAttrList) { // test for attribute match
      nsAttrSelector* attr = aSelector->mAttrList;
      do {
        nsAutoString  value;
        nsresult  attrState = aContent->GetAttribute(kNameSpaceID_Unknown, attr->mAttr, value);
        if (NS_CONTENT_ATTR_NOT_THERE == attrState) {
          result = PR_FALSE;
        }
        else {
          switch (attr->mFunction) {
            case NS_ATTR_FUNC_SET:    break;
            case NS_ATTR_FUNC_EQUALS: 
              if (attr->mCaseSensitive) {
                result = value.Equals(attr->mValue);
              }
              else {
                result = value.EqualsIgnoreCase(attr->mValue);
              }
              break;
            case NS_ATTR_FUNC_INCLUDES: 
              result = ValueIncludes(value, attr->mValue, attr->mCaseSensitive);
              break;
            case NS_ATTR_FUNC_DASHMATCH: 
              result = ValueDashMatch(value, attr->mValue, attr->mCaseSensitive);
              break;
          }
        }
        attr = attr->mNext;
      } while ((PR_TRUE == result) && (nsnull != attr));
    }
    if ((PR_TRUE == result) &&
        ((nsnull != aSelector->mID) || (nsnull != aSelector->mClassList))) {  // test for ID & class match
      result = PR_FALSE;
      nsIStyledContent* styledContent;
      if (NS_SUCCEEDED(aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent))) {
        nsIAtom* contentID;
        styledContent->GetID(contentID);
        if ((nsnull == aSelector->mID) || (aSelector->mID == contentID)) {
          result = PR_TRUE;
          nsAtomList* classList = aSelector->mClassList;
          while (nsnull != classList) {
            if (NS_COMFALSE == styledContent->HasClass(classList->mAtom)) {
              result = PR_FALSE;
              break;
            }
            classList = classList->mNext;
          }
        }
        NS_IF_RELEASE(contentID);
        NS_RELEASE(styledContent);
      }
    }
    if ((PR_TRUE == result) &&
        (nsnull != aSelector->mPseudoClassList)) {  // test for pseudo class match
      // first-child, lang, active, focus, hover, link, outOfDate, visited
      // XXX disabled, enabled, selected, selection
      nsAtomList* pseudoClass = aSelector->mPseudoClassList;
      PRInt32 eventState = NS_EVENT_STATE_UNSPECIFIED;
      nsLinkState linkState = nsLinkState(-1);  // not a link
      nsILinkHandler* linkHandler = nsnull;
      nsIEventStateManager* eventStateManager = nsnull;

      while ((PR_TRUE == result) && (nsnull != pseudoClass)) {
        if (nsCSSAtoms::firstChildPseudo == pseudoClass->mAtom) {
          nsIContent* firstChild = nsnull;
          nsIContent* parent;
          aContent->GetParent(parent);
          if (parent) {
            PRInt32 index = -1;
            do {
              parent->ChildAt(++index, firstChild);
              if (firstChild) { // skip text & comments
                nsIAtom* tag;
                firstChild->GetTag(tag);
                if ((tag != nsLayoutAtoms::textTagName) && 
                    (tag != nsLayoutAtoms::commentTagName) &&
                    (tag != nsLayoutAtoms::processingInstructionTagName)) {
                  NS_IF_RELEASE(tag);
                  break;
                }
                NS_IF_RELEASE(tag);
                NS_RELEASE(firstChild);
              }
              else {
                break;
              }
            } while (1 == 1);
            NS_RELEASE(parent);
          }
          result = PRBool(aContent == firstChild);
          NS_IF_RELEASE(firstChild);
        }
        else if (nsCSSAtoms::langPseudo == pseudoClass->mAtom) {
          // XXX not yet implemented
          result = PR_FALSE;
        }
        else if (IsEventPseudo(pseudoClass->mAtom)) {
          if (aTestState) {
            if (! eventStateManager) {
              aPresContext->GetEventStateManager(&eventStateManager);
              if (eventStateManager) {
                eventStateManager->GetContentState(aContent, eventState);
              }
            }
            if (nsCSSAtoms::activePseudo == pseudoClass->mAtom) {
              result = PRBool(0 != (eventState & NS_EVENT_STATE_ACTIVE));
            }
            else if (nsCSSAtoms::focusPseudo == pseudoClass->mAtom) {
              result = PRBool(0 != (eventState & NS_EVENT_STATE_FOCUS));
            }
            else if (nsCSSAtoms::hoverPseudo == pseudoClass->mAtom) {
              result = PRBool(0 != (eventState & NS_EVENT_STATE_HOVER));
            }
          }
        }
        else if (IsLinkPseudo(pseudoClass->mAtom)) {
          // XXX xml link too
          if (nsHTMLAtoms::a == contentTag) {
            if (aTestState) {
              if (! linkHandler) {
                aPresContext->GetLinkHandler(&linkHandler);
                if (linkHandler) {
                  nsAutoString base, href;
                  nsresult attrState = aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, href);

                  if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
                    nsIURL* docURL = nsnull;
                    nsIHTMLContent* htmlContent;
                    if (NS_SUCCEEDED(aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
                      htmlContent->GetBaseURL(docURL);
                      NS_RELEASE(htmlContent);
                    }
                    else {
                      nsIDocument* doc = nsnull;
                      aContent->GetDocument(doc);
                      if (nsnull != doc) {
                        doc->GetBaseURL(docURL);
                        NS_RELEASE(doc);
                      }
                    }

                    nsAutoString absURLSpec;
                    NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
                    NS_IF_RELEASE(docURL);

                    linkHandler->GetLinkState(absURLSpec.GetUnicode(), linkState);
                  }
                }
              }
              if (nsCSSAtoms::linkPseudo == pseudoClass->mAtom) {
                result = PRBool(eLinkState_Unvisited == linkState);
              }
              else if (nsCSSAtoms::outOfDatePseudo == pseudoClass->mAtom) {
                result = PRBool(eLinkState_OutOfDate == linkState);
              }
              else if (nsCSSAtoms::visitedPseudo == pseudoClass->mAtom) {
                result = PRBool(eLinkState_Visited == linkState);
              }
            }
          }
          else {
            result = PR_FALSE;  // not a link
          }
        }
        else {
          result = PR_FALSE;  // unknown pseudo class
        }
        pseudoClass = pseudoClass->mNext;
      }

      NS_IF_RELEASE(linkHandler);
      NS_IF_RELEASE(eventStateManager);
    }
  }
  NS_IF_RELEASE(contentTag);
  return result;
}

struct ContentEnumData {
  ContentEnumData(nsIPresContext* aPresContext, nsIContent* aContent, 
                  nsIStyleContext* aParentContext, nsISupportsArray* aResults)
  {
    mPresContext = aPresContext;
    mContent = aContent;
    mParentContext = aParentContext;
    mResults = aResults;
    mCount = 0;
  }

  nsIPresContext*   mPresContext;
  nsIContent*       mContent;
  nsIStyleContext*  mParentContext;
  nsISupportsArray* mResults;
  PRInt32           mCount;
};

static PRBool SelectorMatchesTree(nsIPresContext* aPresContext, 
                                  nsIContent* aLastContent, 
                                  nsCSSSelector* aSelector) 
{
  nsCSSSelector* selector = aSelector;

  if (selector) {
    nsIContent* content = nsnull;
    nsIContent* lastContent = aLastContent;
    NS_ADDREF(lastContent);
    while (nsnull != selector) { // check compound selectors
      if (PRUnichar('+') == selector->mOperator) { // fetch previous sibling
        nsIContent* parent;
        PRInt32 index;
        lastContent->GetParent(parent);
        if (parent) {
          parent->IndexOf(lastContent, index);
          while (0 <= --index) {  // skip text & comment nodes
            parent->ChildAt(index, content);
            nsIAtom* tag;
            content->GetTag(tag);
            if ((tag != nsLayoutAtoms::textTagName) && 
                (tag != nsLayoutAtoms::commentTagName)) {
              NS_IF_RELEASE(tag);
              break;
            }
            NS_RELEASE(content);
            NS_IF_RELEASE(tag);
          }
          NS_RELEASE(parent);
        }
      }
      else {  // fetch parent
        lastContent->GetParent(content);
      }
      if (! content) {
        break;
      }
      if (SelectorMatches(aPresContext, selector, content, PR_TRUE)) {
        selector = selector->mNext;
      }
      else {
        if (PRUnichar(0) != selector->mOperator) {
          NS_RELEASE(content);
          break;  // parent was required to match
        }
      }
      NS_IF_RELEASE(lastContent);
      lastContent = content;  // take refcount
      content = nsnull;
    }
    NS_IF_RELEASE(lastContent);
  }
  return PRBool(nsnull == selector);  // matches if ran out of selectors
}

static void ContentEnumFunc(nsICSSStyleRule* aRule, void* aData)
{
  ContentEnumData* data = (ContentEnumData*)aData;

  nsCSSSelector* selector = aRule->FirstSelector();
  if (SelectorMatches(data->mPresContext, selector, data->mContent, PR_TRUE)) {
    selector = selector->mNext;
    if (SelectorMatchesTree(data->mPresContext, data->mContent, selector)) {
      nsIStyleRule* iRule;
      if (NS_OK == aRule->QueryInterface(kIStyleRuleIID, (void**)&iRule)) {
        data->mResults->AppendElement(iRule);
        data->mCount++;
        NS_RELEASE(iRule);
        iRule = aRule->GetImportantRule();
        if (nsnull != iRule) {
          data->mResults->AppendElement(iRule);
          data->mCount++;
          NS_RELEASE(iRule);
        }
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
                                         nsIStyleContext* aParentContext,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIAtom* presMedium = nsnull;
  aPresContext->GetMedium(&presMedium);
  nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    PRBool mediumOK = PR_FALSE;
    PRInt32 mediumCount;
    child->GetMediumCount(mediumCount);
    if (0 < mediumCount) {
      PRInt32 index = 0;
      nsIAtom* medium;
      while ((PR_FALSE == mediumOK) && (index < mediumCount)) {
        child->GetMediumAt(index++, medium);
        if ((medium == nsLayoutAtoms::all) || (medium == presMedium)) {
          mediumOK = PR_TRUE;
        }
        NS_RELEASE(medium);
      }
    }
    else {
      mediumOK = PR_TRUE;
    }
    if (mediumOK) {
      matchCount += child->RulesMatching(aPresContext, aContent, aParentContext, aResults);
    }
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  if (mWeightedRules.IsNotNull()) {
    if (nsnull == mRuleHash) {
      BuildHash();
    }
    ContentEnumData data(aPresContext, aContent, aParentContext, aResults);
    nsIAtom* tagAtom;
    aContent->GetTag(tagAtom);
    nsIAtom* idAtom = nsnull;
    nsVoidArray classArray; // XXX need to recycle this guy (or make nsAutoVoidArray?)

    nsIStyledContent* styledContent;
    if (NS_SUCCEEDED(aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent))) {
      styledContent->GetID(idAtom);
      styledContent->GetClasses(classArray);
      NS_RELEASE(styledContent);
    }

    mRuleHash->EnumerateAllRules(tagAtom, idAtom, classArray, ContentEnumFunc, &data);
    matchCount += data.mCount;

#ifdef DEBUG_RULES
    nsISupportsArray* list1;
    nsISupportsArray* list2;
    NS_NewISupportsArray(&list1);
    NS_NewISupportsArray(&list2);

    data.mResults = list1;
    mRuleHash->EnumerateAllRules(tagAtom, idAtom, classArray, ContentEnumFunc, &data);
    data.mResults = list2;
    mWeightedRules->EnumerateBackwards(ContentEnumWrap, &data);
    NS_ASSERTION(list1->Equals(list2), "lists not equal");
    NS_RELEASE(list1);
    NS_RELEASE(list2);
#endif

    NS_IF_RELEASE(tagAtom);
    NS_IF_RELEASE(idAtom);
  }
  NS_IF_RELEASE(presMedium);
  return matchCount;
}

struct PseudoEnumData {
  PseudoEnumData(nsIPresContext* aPresContext, nsIContent* aParentContent,
                 nsIAtom* aPseudoTag, nsIStyleContext* aParentContext, 
                 nsISupportsArray* aResults)
  {
    mPresContext = aPresContext;
    mParentContent = aParentContent;
    mPseudoTag = aPseudoTag;
    mParentContext = aParentContext;
    mResults = aResults;
    mCount = 0;
  }

  nsIPresContext*   mPresContext;
  nsIContent*       mParentContent;
  nsIAtom*          mPseudoTag;
  nsIStyleContext*  mParentContext;
  nsISupportsArray* mResults;
  PRInt32           mCount;
};

static void PseudoEnumFunc(nsICSSStyleRule* aRule, void* aData)
{
  PseudoEnumData* data = (PseudoEnumData*)aData;

  nsCSSSelector* selector = aRule->FirstSelector();
  if (selector->mTag == data->mPseudoTag) {
    selector = selector->mNext;

    if (selector) { // test next selector specially
      if (PRUnichar('+') == selector->mOperator) {
        return; // not valid here, can't match
      }
      if (SelectorMatches(data->mPresContext, selector, data->mParentContent, PR_TRUE)) {
        selector = selector->mNext;
      }
      else {
        if (PRUnichar('>') == selector->mOperator) {
          return; // immediate parent didn't match
        }
      }
    }

    if (selector && 
        (! SelectorMatchesTree(data->mPresContext, data->mParentContent, selector))) {
      return; // remaining selectors didn't match
    }

    nsIStyleRule* iRule;
    if (NS_OK == aRule->QueryInterface(kIStyleRuleIID, (void**)&iRule)) {
      data->mResults->AppendElement(iRule);
      data->mCount++;
      NS_RELEASE(iRule);
      iRule = aRule->GetImportantRule();
      if (nsnull != iRule) {
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
                                         nsIContent* aParentContent,
                                         nsIAtom* aPseudoTag,
                                         nsIStyleContext* aParentContext,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aPseudoTag, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIAtom* presMedium = nsnull;
  aPresContext->GetMedium(&presMedium);
  nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    PRBool mediumOK = PR_FALSE;
    PRInt32 mediumCount;
    child->GetMediumCount(mediumCount);
    if (0 < mediumCount) {
      PRInt32 index = 0;
      nsIAtom* medium;
      while ((PR_FALSE == mediumOK) && (index < mediumCount)) {
        child->GetMediumAt(index++, medium);
        if ((medium == nsLayoutAtoms::all) || (medium == presMedium)) {
          mediumOK = PR_TRUE;
        }
        NS_RELEASE(medium);
      }
    }
    else {
      mediumOK = PR_TRUE;
    }
    if (mediumOK) {
      matchCount += child->RulesMatching(aPresContext, aParentContent, aPseudoTag, 
                                         aParentContext, aResults);
    }
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  if (mWeightedRules.IsNotNull()) {
    if (nsnull == mRuleHash) {
      BuildHash();
    }
    PseudoEnumData data(aPresContext, aParentContent, aPseudoTag, aParentContext, aResults);
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
    mWeightedRules->EnumerateBackwards(PseudoEnumWrap, &data);
    NS_ASSERTION(list1->Equals(list2), "lists not equal");
    NS_RELEASE(list1);
    NS_RELEASE(list2);
#endif
  }
  NS_IF_RELEASE(presMedium);
  return matchCount;
}

struct StateEnumData
{
  StateEnumData(nsIPresContext* aPresContext, nsIContent* aContent)
    : mPresContext(aPresContext),
      mContent(aContent) {}

  nsIPresContext*   mPresContext;
  nsIContent*       mContent;
};

static 
PRBool StateEnumFunc(void* aSelector, void* aData)
{
  StateEnumData* data = (StateEnumData*)aData;

  nsCSSSelector* selector = (nsCSSSelector*)aSelector;
  if (SelectorMatches(data->mPresContext, selector, data->mContent, PR_FALSE)) {
    selector = selector->mNext;
    if (SelectorMatchesTree(data->mPresContext, data->mContent, selector)) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

  // Test if style is dependent on content state
NS_IMETHODIMP
CSSStyleSheetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                          nsIContent*     aContent)
{
  PRBool isStateful = PR_FALSE;
  // look up content in state rule list
  StateEnumData data(aPresContext, aContent);
  isStateful = (! mStateSelectors.EnumerateForwards(StateEnumFunc, &data)); // if stopped, have state
  
  if (! isStateful) {
    nsIAtom* presMedium = nsnull;
    aPresContext->GetMedium(&presMedium);

    nsICSSStyleSheet*  child = mFirstChild;
    while ((! isStateful) && (nsnull != child)) {
      PRBool mediumOK = PR_FALSE;
      PRInt32 mediumCount;
      child->GetMediumCount(mediumCount);
      if (0 < mediumCount) {
        PRInt32 index = 0;
        nsIAtom* medium;
        while ((PR_FALSE == mediumOK) && (index < mediumCount)) {
          child->GetMediumAt(index++, medium);
          if ((medium == nsLayoutAtoms::all) || (medium == presMedium)) {
            mediumOK = PR_TRUE;
          }
          NS_RELEASE(medium);
        }
      }
      else {
        mediumOK = PR_TRUE;
      }
      if (mediumOK) {
        nsresult result = child->HasStateDependentStyle(aPresContext, aContent);
        isStateful = (NS_OK == result);
      }
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }
    NS_IF_RELEASE(presMedium);
  }
  return ((isStateful) ? NS_OK : NS_COMFALSE);
}



NS_IMETHODIMP
CSSStyleSheetImpl::Init(nsIURL* aURL)
{
  NS_PRECONDITION(aURL, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(mURL.IsNull(), "already initialized");
  if (mURL.IsNotNull())
    return NS_ERROR_ALREADY_INITIALIZED;

  mURL.SetAddRef(aURL);
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetURL(nsIURL*& aURL) const
{
  const nsIURL* url = mURL;
  aURL = (nsIURL*)url;
  NS_IF_ADDREF(aURL);
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetTitle(const nsString& aTitle)
{
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetType(nsString& aType) const
{
  aType.Truncate();
  aType.Append("text/css");
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = ((nsnull != mMedia) ? mMedia->Count() : 0);
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
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

NS_IMETHODIMP
CSSStyleSheetImpl::AppendMedium(nsIAtom* aMedium)
{
  nsresult result = NS_OK;
  if (nsnull == mMedia) {
    result = NS_NewISupportsArray(&mMedia);
  }
  if (NS_SUCCEEDED(result) && (nsnull != mMedia)) {
    mMedia->AppendElement(aMedium);
  }
  return result;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetEnabled(PRBool& aEnabled) const
{
  aEnabled = ((PR_TRUE == mDisabled) ? PR_FALSE : PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetEnabled(PRBool aEnabled)
{
  PRBool oldState = mDisabled;
  mDisabled = ((PR_TRUE == aEnabled) ? PR_FALSE : PR_TRUE);

  if ((nsnull != mDocument) && (mDisabled != oldState)) {
    mDocument->SetStyleSheetDisabledState(this, mDisabled);
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  NS_IF_ADDREF(mParent);
  aParent = mParent;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  nsIDocument*  doc = mDocument;
  CSSStyleSheetImpl* parent = (CSSStyleSheetImpl*)mParent;
  while ((nsnull == doc) && (nsnull != parent)) {
    doc = parent->mDocument;
    parent = (CSSStyleSheetImpl*)(parent->mParent);
  }

  NS_IF_ADDREF(doc);
  aDocument = doc;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{ // not ref counted
  mDocument = aDocument;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetOwningNode(nsIDOMNode* aOwningNode)
{ // not ref counted
  mOwningNode = aOwningNode;
  return NS_OK;
}

PRBool CSSStyleSheetImpl::ContainsStyleSheet(nsIURL* aURL) const
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

  PRBool result = mURL->Equals(aURL);

  const nsICSSStyleSheet*  child = mFirstChild;
  while ((PR_FALSE == result) && (nsnull != child)) {
    result = child->ContainsStyleSheet(aURL);
    child = ((const CSSStyleSheetImpl*)child)->mNext;
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
  
  // This is not reference counted. Our parent tells us when
  // it's going away.
  ((CSSStyleSheetImpl*)aSheet)->mParent = this;
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
    if (rule->GetWeight() >= weight) { // insert before rules with equal or lesser weight
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mWeightedRules->InsertElementAt(aRule, index + 1);
  mOrderedRules->InsertElementAt(aRule, 0);
  aRule->SetStyleSheet(this);
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
    if (rule->GetWeight() <= weight) { // insert after rules with greater weight (before equal or lower weight)
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mWeightedRules->InsertElementAt(aRule, index);
  mOrderedRules->AppendElement(aRule);
  aRule->SetStyleSheet(this);
}

PRInt32 CSSStyleSheetImpl::StyleRuleCount(void) const
{
  if (mOrderedRules.IsNotNull()) {
    return mOrderedRules->Count();
  }
  return 0;
}

nsresult CSSStyleSheetImpl::GetStyleRuleAt(PRInt32 aIndex, nsICSSStyleRule*& aRule) const
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

PRInt32 CSSStyleSheetImpl::StyleSheetCount(void) const
{
  // XXX Far from an ideal way to do this, but the hope is that
  // it won't be done too often. If it is, we might want to 
  // consider storing the children in an array.
  PRInt32 count = 0;
  if (mFirstChild.IsNotNull()) {
    const nsICSSStyleSheet* child = mFirstChild;
    while (nsnull != child) {
      count++;
      child = ((const CSSStyleSheetImpl*)child)->mNext;
    }
  }

  return count;
}

nsresult CSSStyleSheetImpl::GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const
{
  // XXX Ughh...an O(n^2) method for doing iteration. Again, we hope
  // that this isn't done too often. If it is, we need to change the
  // underlying storage mechanism
  aSheet = nsnull;
  if (mFirstChild.IsNotNull()) {
    const nsICSSStyleSheet* child = mFirstChild;
    while ((nsnull != child) && (0 != aIndex)) {
      --aIndex;
      child = ((const CSSStyleSheetImpl*)child)->mNext;
    }
    
    aSheet = (nsICSSStyleSheet*)child;
    NS_IF_ADDREF(aSheet);
  }

  return NS_OK;
}

void CSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  PRUnichar* buffer;
  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("CSS Style Sheet: ", out);
  mURL->ToString(&buffer);
  nsAutoString as(buffer,0);
  fputs(as, out);
  fputs("\n", out);
  delete buffer;

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
    mStateSelectors.Clear();
  }
}

static
PRBool BuildHashEnum(nsISupports* aRule, void* aHash)
{
  nsICSSStyleRule* rule = (nsICSSStyleRule*)aRule;
  RuleHash* hash = (RuleHash*)aHash;
  hash->AppendRule(rule);
  return PR_TRUE;
}

static
PRBool IsStateSelector(nsCSSSelector& aSelector)
{
  nsAtomList* pseudoClass = aSelector.mPseudoClassList;
  while (pseudoClass) {
    if ((pseudoClass->mAtom == nsCSSAtoms::activePseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::disabledPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::enabledPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::focusPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::hoverPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::linkPseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::outOfDatePseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::selectedPseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::selectionPseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::visitedPseudo)) {
      return PR_TRUE;
    }
    pseudoClass = pseudoClass->mNext;
  }
  return PR_FALSE;
}

static
PRBool BuildStateEnum(nsISupports* aRule, void* aArray)
{
  nsICSSStyleRule* rule = (nsICSSStyleRule*)aRule;
  nsVoidArray* array = (nsVoidArray*)aArray;

  nsCSSSelector* selector = rule->FirstSelector();

  while (selector) {
    if (IsStateSelector(*selector)) {
      array->AppendElement(selector);
    }
    selector = selector->mNext;
  }
  return PR_TRUE;
}

void CSSStyleSheetImpl::BuildHash(void)
{
  NS_ASSERTION(nsnull == mRuleHash, "clear rule hash first");

  mRuleHash = new RuleHash();
  if ((nsnull != mRuleHash) && mWeightedRules.IsNotNull()) {
    mWeightedRules->EnumerateBackwards(BuildHashEnum, mRuleHash);
    mWeightedRules->EnumerateBackwards(BuildStateEnum, &mStateSelectors);
  }
}

  // nsIDOMStyleSheet interface
NS_IMETHODIMP    
CSSStyleSheetImpl::GetType(nsString& aType)
{
  aType.Truncate();
  aType.Append("text/css");
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetDisabled(PRBool* aDisabled)
{
  *aDisabled = mDisabled;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::SetDisabled(PRBool aDisabled)
{
  PRBool oldState = mDisabled;
  mDisabled = aDisabled;

  if ((nsnull != mDocument) && (mDisabled != oldState)) {
    mDocument->SetStyleSheetDisabledState(this, mDisabled);
  }

  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetReadOnly(PRBool* aReadOnly)
{
  // XXX TBI
  *aReadOnly = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetOwningNode(nsIDOMNode** aOwningNode)
{
  NS_IF_ADDREF(mOwningNode);
  *aOwningNode = mOwningNode;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet)
{
  if (nsnull != mParent) {
    return mParent->QueryInterface(kIDOMStyleSheetIID, (void **)aParentStyleSheet);
  }
  else {
    *aParentStyleSheet = nsnull;
    return NS_OK;
  }
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetHref(nsString& aHref)
{
  if (mURL.IsNotNull()) {
    PRUnichar* str;
    mURL->ToString(&str);
    aHref = str;
    delete str;
  }
  else {
    aHref.SetLength(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetTitle(nsString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetMedia(nsString& aMedia)
{
  aMedia.Truncate();
  if (nsnull != mMedia) {
    PRInt32 count = mMedia->Count();
    PRInt32 index = 0;
    nsAutoString buffer;
    while (index < count) {
      nsIAtom* medium = (nsIAtom*)mMedia->ElementAt(index++);
      medium->ToString(buffer);
      aMedia.Append(buffer);
      if (index < count) {
        aMedia.Append(", ");
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::GetCssRules(nsIDOMCSSStyleRuleCollection** aCssRules)
{
  if (nsnull == mRuleCollection) {
    mRuleCollection = new CSSStyleRuleCollectionImpl(this);
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
CSSStyleSheetImpl::InsertRule(const nsString& aRule, 
                              PRUint32 aIndex, 
                              PRUint32* aReturn)
{
  nsICSSParser* css;
  nsresult result = NS_NewCSSParser(&css);
  if (NS_OK == result) {
    nsString* str = new nsString(aRule); // will be deleted by the input stream
    nsIUnicharInputStream* input = nsnull;
    result = NS_NewStringUnicharInputStream(&input, str);
    if (NS_OK == result) {
      nsICSSStyleSheet* tmp;
      css->SetStyleSheet(this);
      // XXX Currently, the parser will append the rule to the
      // style sheet. We shouldn't ignore the index.
      result = css->Parse(input, mURL, tmp);
      NS_ASSERTION(tmp == this, "parser incorrectly created a new stylesheet");
      NS_RELEASE(tmp);
      NS_RELEASE(input);
      *aReturn = mOrderedRules->Count();
      if (nsnull != mDocument) {
        nsICSSStyleRule* rule;

        rule = (nsICSSStyleRule*)mOrderedRules->ElementAt(aIndex);
        mDocument->StyleRuleAdded(this, rule);
        NS_IF_RELEASE(rule);
      }
    }

    NS_RELEASE(css);
  }

  return result;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::DeleteRule(PRUint32 aIndex)
{
  // XXX TBI: handle @rule types
  nsICSSStyleRule *rule;
  
  rule = (nsICSSStyleRule *)mOrderedRules->ElementAt(aIndex);
  if (nsnull != rule) {
    mOrderedRules->RemoveElementAt(aIndex);
    mWeightedRules->RemoveElement(rule);
    rule->SetStyleSheet(nsnull);
    NS_RELEASE(rule);
  }

  return NS_OK;
}

NS_IMETHODIMP 
CSSStyleSheetImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsICSSStyleSheet *)this;
    // XXX Should be done through factory
    res = NS_NewScriptCSSStyleSheet(aContext, 
                                    supports,
                                    (nsISupports *)global, 
                                    (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP 
CSSStyleSheetImpl::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


// XXX for backwards compatibility and convenience
NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURL* aURL)
{
  nsICSSStyleSheet* sheet;
  nsresult rv;
  if (NS_FAILED(rv = NS_NewCSSStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}

NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSStyleSheetImpl  *it = new CSSStyleSheetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
}
