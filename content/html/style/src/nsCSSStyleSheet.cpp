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
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsICSSStyleRule.h"
#include "nsICSSNameSpaceRule.h"
#include "nsICSSMediaRule.h"
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
#include "nsINameSpace.h"
#include "prlog.h"

//#define DEBUG_REFS
//#define DEBUG_RULES
#define DEBUG_CASCADE

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
NS_DEF_PTR(nsIURI);
NS_DEF_PTR(nsISupportsArray);
NS_DEF_PTR(nsICSSStyleSheet);

// ----------------------
// Rule hash key
//
class AtomKey: public nsHashKey {
public:
  AtomKey(nsIAtom* aAtom);
  AtomKey(const AtomKey& aKey);
  virtual ~AtomKey(void);
  virtual PRUint32 HashValue(void) const;
  virtual PRBool Equals(const nsHashKey *aKey) const;
  virtual nsHashKey *Clone(void) const;
  nsIAtom*  mAtom;
};

AtomKey::AtomKey(nsIAtom* aAtom)
{
  mAtom = aAtom;
  NS_ADDREF(mAtom);
}

AtomKey::AtomKey(const AtomKey& aKey)
{
  mAtom = aKey.mAtom;
  NS_ADDREF(mAtom);
}

AtomKey::~AtomKey(void)
{
  NS_RELEASE(mAtom);
}

PRUint32 AtomKey::HashValue(void) const
{
  return (PRUint32)mAtom;
}

PRBool AtomKey::Equals(const nsHashKey* aKey) const
{
  return PRBool (((AtomKey*)aKey)->mAtom == mAtom);
}

nsHashKey* AtomKey::Clone(void) const
{
  return new AtomKey(*this);
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

  AtomKey key(aAtom);
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
    AtomKey universalKey(nsCSSAtoms::universalSelector);
    RuleValue*  value = (RuleValue*)mTagTable.Get(&universalKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  if (nsnull != aTag) {
    AtomKey tagKey(aTag);
    RuleValue* value = (RuleValue*)mTagTable.Get(&tagKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  if (nsnull != aID) {
    AtomKey idKey(aID);
    RuleValue* value = (RuleValue*)mIdTable.Get(&idKey);
    if (nsnull != value) {
      mEnumList[valueCount++] = value;
    }
  }
  for (index = 0; index < classCount; index++) {
    nsIAtom* classAtom = (nsIAtom*)aClassList.ElementAt(index);
    AtomKey classKey(classAtom);
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
  AtomKey tagKey(aTag);
  AtomKey universalKey(nsCSSAtoms::universalSelector);
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
// CSS Style Sheet Inner Data Container
//

class CSSStyleSheetInner {
public:
  CSSStyleSheetInner(nsICSSStyleSheet* aParentSheet);
  CSSStyleSheetInner(CSSStyleSheetInner& aCopy, nsICSSStyleSheet* aParentSheet);
  virtual ~CSSStyleSheetInner(void);

  virtual CSSStyleSheetInner* CloneFor(nsICSSStyleSheet* aParentSheet);
  virtual void AddSheet(nsICSSStyleSheet* aParentSheet);
  virtual void RemoveSheet(nsICSSStyleSheet* aParentSheet);

  virtual void ClearRuleCascades(void);
  virtual void RebuildNameSpaces(void);

  nsVoidArray           mSheets;

  nsIURI*               mURL;

  nsISupportsArray*     mOrderedRules;
  nsHashtable*          mMediumCascadeTable;

  nsINameSpace*         mNameSpace;
  PRInt32               mDefaultNameSpaceID;
};


//--------------------------------

struct RuleCascadeData {
  RuleCascadeData(void)
    : mWeightedRules(nsnull),
      mRuleHash(),
      mStateSelectors()
  {
    NS_NewISupportsArray(&mWeightedRules);
  }

  ~RuleCascadeData(void)
  {
    NS_IF_RELEASE(mWeightedRules);
  }
  nsISupportsArray* mWeightedRules;
  RuleHash          mRuleHash;
  nsVoidArray       mStateSelectors;
};


// -------------------------------
// CSS Style Sheet
//

class CSSImportsCollectionImpl;
class CSSStyleRuleCollectionImpl;

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
  NS_IMETHOD Init(nsIURI* aURL);
  NS_IMETHOD GetURL(nsIURI*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD SetTitle(const nsString& aTitle);
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD UseForMedium(nsIAtom* aMedium) const;
  NS_IMETHOD AppendMedium(nsIAtom* aMedium);
  NS_IMETHOD ClearMedia(void);

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

  NS_IMETHOD  ContainsStyleSheet(nsIURI* aURL) const;

  NS_IMETHOD AppendStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex);

  // XXX do these belong here or are they generic?
  NS_IMETHOD PrependStyleRule(nsICSSRule* aRule);
  NS_IMETHOD AppendStyleRule(nsICSSRule* aRule);

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;

  NS_IMETHOD  StyleSheetCount(PRInt32& aCount) const;
  NS_IMETHOD  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const;

  NS_IMETHOD  GetNameSpace(nsINameSpace*& aNameSpace) const;
  NS_IMETHOD  SetDefaultNameSpaceID(PRInt32 aDefaultNameSpaceID);

  NS_IMETHOD Clone(nsICSSStyleSheet*& aClone) const;

  NS_IMETHOD IsUnmodified(void) const;
  NS_IMETHOD SetModified(PRBool aModified);

  nsresult  EnsureUniqueInner(void);

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

  void ClearRuleCascades(void);
#ifdef DEBUG_CASCADE
  nsresult SlowCascadeRulesInto(nsIAtom* aMedium, nsISupportsArray* aRules);
#endif
  nsresult GatherRulesFor(nsIAtom* aMedium, nsISupportsArray* aRules);
  nsresult CascadeRulesInto(nsIAtom* aMedium, nsISupportsArray* aRules);
  RuleCascadeData* GetRuleCascade(nsIAtom* aMedium);

  nsresult WillDirty(void);
  void     DidDirty(void);

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsString              mTitle;
  nsISupportsArray*     mMedia;
  CSSStyleSheetImpl*    mFirstChild;
  CSSStyleSheetImpl*    mNext;
  nsICSSStyleSheet*     mParent;

  CSSImportsCollectionImpl* mImportsCollection;
  CSSStyleRuleCollectionImpl* mRuleCollection;
  nsIDocument*          mDocument;
  nsIDOMNode*           mOwningNode;
  PRBool                mDisabled;
  PRBool                mDirty; // has been modified 
  void*                 mScriptObject;

  CSSStyleSheetInner*   mInner;
};


// -------------------------------
// Style Rule Collection for the DOM
//
class CSSStyleRuleCollectionImpl : public nsIDOMCSSStyleRuleCollection,
                                   public nsIScriptObjectOwner 
{
public:
  CSSStyleRuleCollectionImpl(CSSStyleSheetImpl *aStyleSheet);

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

  CSSStyleSheetImpl*  mStyleSheet;
  void*               mScriptObject;
public:
  PRBool              mRulesAccessed;
};

CSSStyleRuleCollectionImpl::CSSStyleRuleCollectionImpl(CSSStyleSheetImpl *aStyleSheet)
{
  NS_INIT_REFCNT();
  // Not reference counted to avoid circular references.
  // The style sheet will tell us when its going away.
  mStyleSheet = aStyleSheet;
  mScriptObject = nsnull;
  mRulesAccessed = PR_FALSE;
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
CSSStyleRuleCollectionImpl::Item(PRUint32 aIndex, nsIDOMCSSStyleRule** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;
  if (nsnull != mStyleSheet) {
    result = mStyleSheet->EnsureUniqueInner(); // needed to ensure rules have correct parent
    if (NS_SUCCEEDED(result)) {
      nsICSSRule *rule;

      result = mStyleSheet->GetStyleRuleAt(aIndex, rule);
      if (NS_OK == result) {
        result = rule->QueryInterface(kIDOMCSSStyleRuleIID, (void **)aReturn);
        mRulesAccessed = PR_TRUE; // signal to never share rules again
      }
      NS_RELEASE(rule);
    }
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
                                             (nsISupports *)(nsICSSStyleSheet*)mStyleSheet, 
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


CSSStyleSheetInner::CSSStyleSheetInner(nsICSSStyleSheet* aParentSheet)
  : mSheets(),
    mURL(nsnull),
    mOrderedRules(nsnull),
    mMediumCascadeTable(nsnull),
    mNameSpace(nsnull),
    mDefaultNameSpaceID(kNameSpaceID_None)
{
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

CSSStyleSheetInner::CSSStyleSheetInner(CSSStyleSheetInner& aCopy,
                                       nsICSSStyleSheet* aParentSheet)
  : mSheets(),
    mURL(aCopy.mURL),
    mMediumCascadeTable(nsnull),
    mNameSpace(nsnull),
    mDefaultNameSpaceID(aCopy.mDefaultNameSpaceID)
{
  mSheets.AppendElement(aParentSheet);
  NS_IF_ADDREF(mURL);
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

CSSStyleSheetInner::~CSSStyleSheetInner(void)
{
  NS_IF_RELEASE(mURL);
  if (mOrderedRules) {
    mOrderedRules->EnumerateForwards(SetStyleSheetReference, nsnull);
    NS_RELEASE(mOrderedRules);
  }
  ClearRuleCascades();
  NS_IF_RELEASE(mNameSpace);
}

CSSStyleSheetInner* 
CSSStyleSheetInner::CloneFor(nsICSSStyleSheet* aParentSheet)
{
  return new CSSStyleSheetInner(*this, aParentSheet);
}

void
CSSStyleSheetInner::AddSheet(nsICSSStyleSheet* aParentSheet)
{
  mSheets.AppendElement(aParentSheet);
}

void
CSSStyleSheetInner::RemoveSheet(nsICSSStyleSheet* aParentSheet)
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

static PRBool DeleteRuleCascade(nsHashKey* aKey, void* aValue, void* closure)
{
  delete ((RuleCascadeData*)aValue);
  return PR_TRUE;
}

void
CSSStyleSheetInner::ClearRuleCascades(void)
{
  if (mMediumCascadeTable) {
    mMediumCascadeTable->Enumerate(DeleteRuleCascade, nsnull);
    delete mMediumCascadeTable;
    mMediumCascadeTable = nsnull;
  }
}


static PRBool
CreateNameSpace(nsISupports* aRule, void* aNameSpacePtr)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  PRInt32 type;
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
    lastNameSpace->CreateChildNameSpace(prefix, urlSpec, newNameSpace);
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
CSSStyleSheetInner::RebuildNameSpaces(void)
{
  nsINameSpaceManager*  nameSpaceMgr;
  if (mNameSpace) {
    mNameSpace->GetNameSpaceManager(nameSpaceMgr);
    NS_RELEASE(mNameSpace);
  }
  else {
    NS_NewNameSpaceManager(&nameSpaceMgr);
  }
  if (nameSpaceMgr) {
    nameSpaceMgr->CreateRootNameSpace(mNameSpace);
    if (kNameSpaceID_Unknown != mDefaultNameSpaceID) {
      nsINameSpace* defaultNameSpace;
      mNameSpace->CreateChildNameSpace(nsnull, mDefaultNameSpaceID, defaultNameSpace);
      if (defaultNameSpace) {
        NS_RELEASE(mNameSpace);
        mNameSpace = defaultNameSpace;
      }
    }
    NS_RELEASE(nameSpaceMgr);
    if (mOrderedRules) {
      mOrderedRules->EnumerateForwards(CreateNameSpace, &mNameSpace);
    }
  }
}


// -------------------------------
// CSS Style Sheet
//

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
    mTitle(), 
    mMedia(nsnull),
    mFirstChild(nsnull), 
    mNext(nsnull),
    mParent(nsnull),
    mImportsCollection(nsnull),
    mRuleCollection(nsnull),
    mDocument(nsnull),
    mOwningNode(nsnull),
    mDisabled(PR_FALSE),
    mDirty(PR_FALSE),
    mScriptObject(nsnull)
{
  NS_INIT_REFCNT();
  nsCSSAtoms::AddRefAtoms();

  mInner = new CSSStyleSheetInner(this);

#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSStyleSheet size: %d\n", gInstanceCount, sizeof(*this));
#endif
}

CSSStyleSheetImpl::CSSStyleSheetImpl(const CSSStyleSheetImpl& aCopy)
  : nsICSSStyleSheet(),
    mTitle(aCopy.mTitle), 
    mMedia(nsnull),
    mFirstChild(nsnull), 
    mNext(nsnull),
    mParent(aCopy.mParent),
    mImportsCollection(nsnull), // re-created lazily
    mRuleCollection(nsnull), // re-created lazily
    mDocument(aCopy.mDocument),
    mOwningNode(aCopy.mOwningNode),
    mDisabled(aCopy.mDisabled),
    mDirty(PR_FALSE),
    mScriptObject(nsnull),
    mInner(aCopy.mInner)
{
  NS_INIT_REFCNT();
  nsCSSAtoms::AddRefAtoms();

  mInner->AddSheet(this);

  if (aCopy.mRuleCollection && 
      aCopy.mRuleCollection->mRulesAccessed) {  // CSSOM's been there, force full copy now
    EnsureUniqueInner();
  }

  if (aCopy.mMedia) {
    NS_NewISupportsArray(&mMedia);
    if (mMedia) {
      mMedia->AppendElements(aCopy.mMedia);
    }
  }

  if (aCopy.mFirstChild) {
    CSSStyleSheetImpl*  otherChild = aCopy.mFirstChild;
    CSSStyleSheetImpl** ourSlot = &mFirstChild;
    do {
      CSSStyleSheetImpl* child = new CSSStyleSheetImpl(*otherChild);
      if (child) {
        NS_ADDREF(child);
        (*ourSlot) = child;
        ourSlot = &(child->mNext);
      }
      otherChild = otherChild->mNext;
    }
    while (otherChild && ourSlot);
  }

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
  NS_IF_RELEASE(mMedia);
  if (mFirstChild) {
    CSSStyleSheetImpl* child = mFirstChild;
    do {
      child->mParent = nsnull;
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
  mInner->RemoveSheet(this);
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
                 (nsCSSAtoms::dragOverPseudo == aAtom) || 
//                 (nsCSSAtoms::dragPseudo == aAtom) ||   // not real yet
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
        nsresult  attrState = aContent->GetAttribute(attr->mNameSpace, attr->mAttr, value);
        if (NS_FAILED(attrState) || (NS_CONTENT_ATTR_NOT_THERE == attrState)) {
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
      // first-child, root, lang, active, focus, hover, link, outOfDate, visited
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
        else if (nsCSSAtoms::rootPseudo == pseudoClass->mAtom) {
          nsIContent* parent;
          aContent->GetParent(parent);
          if (parent) {
            NS_RELEASE(parent);
            result = PR_FALSE;
          }
          else {
            result = PR_TRUE;
          }
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
            else if (nsCSSAtoms::dragOverPseudo == pseudoClass->mAtom) {
              result = PRBool(0 != (eventState & NS_EVENT_STATE_DRAGOVER));
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
                    nsIURI* docURL = nsnull;
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
#ifndef NECKO
                    NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
#else
                    nsresult rv;
                    nsIURI *baseUri = nsnull;
                    rv = docURL->QueryInterface(nsIURI::GetIID(), (void**)&baseUri);
                    if (NS_FAILED(rv)) return PR_FALSE;

                    NS_MakeAbsoluteURI(href, baseUri, absURLSpec);
                    NS_RELEASE(baseUri);
#endif // NECKO
                    NS_IF_RELEASE(docURL);

                    linkHandler->GetLinkState(absURLSpec.GetUnicode(), linkState);
                  }
                }
                else {
                  // no link handler?  then all links are unvisited
                  linkState = eLinkState_Unvisited;
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

  RuleCascadeData* cascade = GetRuleCascade(presMedium);

  if (cascade) {
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

    cascade->mRuleHash.EnumerateAllRules(tagAtom, idAtom, classArray, ContentEnumFunc, &data);
    matchCount += data.mCount;

#ifdef DEBUG_RULES
    nsISupportsArray* list1;
    nsISupportsArray* list2;
    NS_NewISupportsArray(&list1);
    NS_NewISupportsArray(&list2);

    data.mResults = list1;
    cascade->mRuleHash.EnumerateAllRules(tagAtom, idAtom, classArray, ContentEnumFunc, &data);
    data.mResults = list2;
    cascade->mWeightedRules->EnumerateBackwards(ContentEnumWrap, &data);
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

  RuleCascadeData* cascade = GetRuleCascade(presMedium);

  if (cascade) {
    PseudoEnumData data(aPresContext, aParentContent, aPseudoTag, aParentContext, aResults);
    cascade->mRuleHash.EnumerateTagRules(aPseudoTag, PseudoEnumFunc, &data);
    matchCount += data.mCount;

#ifdef DEBUG_RULES
    nsISupportsArray* list1;
    nsISupportsArray* list2;
    NS_NewISupportsArray(&list1);
    NS_NewISupportsArray(&list2);
    data.mResults = list1;
    cascade->mRuleHash.EnumerateTagRules(aPseudoTag, PseudoEnumFunc, &data);
    data.mResults = list2;
    cascade->mWeightedRules->EnumerateBackwards(PseudoEnumWrap, &data);
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

  nsIAtom* presMedium = nsnull;
  aPresContext->GetMedium(&presMedium);

  RuleCascadeData* cascade = GetRuleCascade(presMedium);

  if (cascade) {
    // look up content in state rule list
    StateEnumData data(aPresContext, aContent);
    isStateful = (! cascade->mStateSelectors.EnumerateForwards(StateEnumFunc, &data)); // if stopped, have state
  }
  NS_IF_RELEASE(presMedium);

  return ((isStateful) ? NS_OK : NS_COMFALSE);
}



NS_IMETHODIMP
CSSStyleSheetImpl::Init(nsIURI* aURL)
{
  NS_PRECONDITION(aURL, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  if (! mInner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ASSERTION(! mInner->mURL, "already initialized");
  if (mInner->mURL)
    return NS_ERROR_ALREADY_INITIALIZED;

  if (mInner->mURL) {
#ifdef NECKO
#ifdef DEBUG
    PRBool eq;
    nsresult rv = mInner->mURL->Equals(aURL, &eq);
    NS_ASSERTION(NS_SUCCEEDED(rv) && eq, "bad inner");
#endif
#else
    NS_ASSERTION(mInner->mURL->Equals(aURL), "bad inner");
#endif
  }
  else {
    mInner->mURL = aURL;
    NS_ADDREF(mInner->mURL);
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  const nsIURI* url = ((mInner) ? mInner->mURL : nsnull);
  aURL = (nsIURI*)url;
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
CSSStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  if (mMedia) {
    if (-1 != mMedia->IndexOf(aMedium)) {
      return NS_OK;
    }
    if (-1 != mMedia->IndexOf(nsLayoutAtoms::all)) {
      return NS_OK;
    }
    return NS_COMFALSE;
  }
  return NS_OK;
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
CSSStyleSheetImpl::ClearMedia(void)
{
  if (mMedia) {
    mMedia->Clear();
  }
  return NS_OK;
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

NS_IMETHODIMP
CSSStyleSheetImpl::ContainsStyleSheet(nsIURI* aURL) const
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

#ifdef NECKO
  PRBool result;
  nsresult rv = mInner->mURL->Equals(aURL, &result);
  if (NS_FAILED(rv)) result = PR_FALSE;
#else
  PRBool result = mInner->mURL->Equals(aURL);
#endif

  const CSSStyleSheetImpl*  child = mFirstChild;
  while ((PR_FALSE == result) && (nsnull != child)) {
    result = (NS_OK == child->ContainsStyleSheet(aURL));
    child = child->mNext;
  }
  return ((result) ? NS_OK : NS_COMFALSE);
}

NS_IMETHODIMP
CSSStyleSheetImpl::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (NS_SUCCEEDED(WillDirty())) {
    NS_ADDREF(aSheet);
    CSSStyleSheetImpl* sheet = (CSSStyleSheetImpl*)aSheet;

    if (! mFirstChild) {
      mFirstChild = sheet;
    }
    else {
      CSSStyleSheetImpl* child = mFirstChild;
      while (child->mNext) {
        child = child->mNext;
      }
      child->mNext = sheet;
    }
  
    // This is not reference counted. Our parent tells us when
    // it's going away.
    sheet->mParent = this;
    DidDirty();
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  nsresult result = WillDirty();

  if (NS_SUCCEEDED(result)) {
    NS_ADDREF(aSheet);
    CSSStyleSheetImpl* sheet = (CSSStyleSheetImpl*)aSheet;
    CSSStyleSheetImpl* child = mFirstChild;

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
    DidDirty();
  }
  return result;
}

NS_IMETHODIMP
CSSStyleSheetImpl::PrependStyleRule(nsICSSRule* aRule)
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

      PRInt32 type;
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
CSSStyleSheetImpl::AppendStyleRule(nsICSSRule* aRule)
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

      PRInt32 type;
      aRule->GetType(type);
      if (nsICSSRule::NAMESPACE_RULE == type) {
        if (! mInner->mNameSpace) {
          nsINameSpaceManager*  nameSpaceMgr;
          NS_NewNameSpaceManager(&nameSpaceMgr);
          if (nameSpaceMgr) {
            nameSpaceMgr->CreateRootNameSpace(mInner->mNameSpace);
            NS_RELEASE(nameSpaceMgr);
          }
        }

        if (mInner->mNameSpace) {
          nsICSSNameSpaceRule*  nameSpaceRule = (nsICSSNameSpaceRule*)aRule;
          nsINameSpace* newNameSpace = nsnull;

          nsIAtom*      prefix = nsnull;
          nsAutoString  urlSpec;
          nameSpaceRule->GetPrefix(prefix);
          nameSpaceRule->GetURLSpec(urlSpec);
          mInner->mNameSpace->CreateChildNameSpace(prefix, urlSpec, newNameSpace);
          NS_IF_RELEASE(prefix);
          if (newNameSpace) {
            NS_RELEASE(mInner->mNameSpace);
            mInner->mNameSpace = newNameSpace; // takes ref
          }
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::StyleRuleCount(PRInt32& aCount) const
{
  aCount = 0;
  if (mInner && mInner->mOrderedRules) {
    PRUint32 cnt;
    nsresult rv = ((CSSStyleSheetImpl*)this)->mInner->mOrderedRules->Count(&cnt);      // XXX bogus cast -- this method should not be const
    aCount = (PRInt32)cnt;
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const
{
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
CSSStyleSheetImpl::GetNameSpace(nsINameSpace*& aNameSpace) const
{
  aNameSpace = ((mInner) ? mInner->mNameSpace : nsnull);
  NS_IF_ADDREF(aNameSpace);
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetDefaultNameSpaceID(PRInt32 aDefaultNameSpaceID)
{
  if (mInner) {
    mInner->mDefaultNameSpaceID = aDefaultNameSpaceID;
    mInner->RebuildNameSpaces();
  }
  return NS_OK;
}


NS_IMETHODIMP
CSSStyleSheetImpl::StyleSheetCount(PRInt32& aCount) const
{
  // XXX Far from an ideal way to do this, but the hope is that
  // it won't be done too often. If it is, we might want to 
  // consider storing the children in an array.
  aCount = 0;

  const CSSStyleSheetImpl* child = mFirstChild;
  while (child) {
    aCount++;
    child = child->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSStyleSheetImpl::GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const
{
  // XXX Ughh...an O(n^2) method for doing iteration. Again, we hope
  // that this isn't done too often. If it is, we need to change the
  // underlying storage mechanism
  aSheet = nsnull;

  if (mFirstChild) {
    const CSSStyleSheetImpl* child = mFirstChild;
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
CSSStyleSheetImpl::EnsureUniqueInner(void)
{
  if (! mInner) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (1 < mInner->mSheets.Count()) {  
    CSSStyleSheetInner* clone = mInner->CloneFor(this);
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
CSSStyleSheetImpl::Clone(nsICSSStyleSheet*& aClone) const
{
  // XXX no, really need to clone
  CSSStyleSheetImpl* clone = new CSSStyleSheetImpl(*this);
  if (clone) {
    aClone = (nsICSSStyleSheet*)clone;
    NS_ADDREF(aClone);
  }
  return NS_OK;
}

static void
ListRules(nsISupportsArray* aRules, FILE* aOut, PRInt32 aIndent)
{
  PRUint32 count;
  PRUint32 index;
  if (aRules) {
    aRules->Count(&count);
    for (index = 0; index < count; index++) {
      nsICSSRule* rule = (nsICSSRule*)aRules->ElementAt(index);
      rule->List(aOut, aIndent);
      NS_RELEASE(rule);
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

static PRBool ListCascade(nsHashKey* aKey, void* aValue, void* aClosure)
{
  AtomKey* key = (AtomKey*)aKey;
  RuleCascadeData* cascade = (RuleCascadeData*)aValue;
  ListEnumData* data = (ListEnumData*)aClosure;

  fputs("\nRules in cascade order for medium: \"", data->mOut);
  nsAutoString  buffer;
  key->mAtom->ToString(buffer);
  fputs(buffer, data->mOut);
  fputs("\"\n", data->mOut);

  ListRules(cascade->mWeightedRules, data->mOut, data->mIndent);
  return PR_TRUE;
}


void CSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{

  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  if (! mInner) {
    fputs("CSS Style Sheet - without inner data storage - ERROR\n", out);
    return;
  }

  fputs("CSS Style Sheet: ", out);
#ifdef NECKO
  char* urlSpec = nsnull;
  nsresult rv = mInner->mURL->GetSpec(&urlSpec);
  if (NS_SUCCEEDED(rv) && urlSpec) {
    fputs(urlSpec, out);
    nsCRT::free(urlSpec);
  }
#else
  PRUnichar* urlSpec = nsnull;
  mInner->mURL->ToString(&urlSpec);
  if (urlSpec) {
    nsAutoString buffer(urlSpec);
    delete [] urlSpec;
    fputs(buffer, out);
  }
#endif

  if (mMedia) {
    fputs(" media: ", out);
    index = 0;
    PRUint32 count;
    mMedia->Count(&count);
    nsAutoString  buffer;
    while (index < PRInt32(count)) {
      nsIAtom* medium = (nsIAtom*)mMedia->ElementAt(index++);
      medium->ToString(buffer);
      fputs(buffer, out);
      if (index < PRInt32(count)) {
        fputs(", ", out);
      }
      NS_RELEASE(medium);
    }
  }
  fputs("\n", out);

  const CSSStyleSheetImpl*  child = mFirstChild;
  while (nsnull != child) {
    child->List(out, aIndent + 1);
    child = child->mNext;
  }

  fputs("Rules in source order:\n", out);
  ListRules(mInner->mOrderedRules, out, aIndent);

  if (mInner->mMediumCascadeTable) {
    ListEnumData  data(out, aIndent);
    mInner->mMediumCascadeTable->Enumerate(ListCascade, &data);
  }
}

void 
CSSStyleSheetImpl::ClearRuleCascades(void)
{
  mInner->ClearRuleCascades();
  if (mParent) {
    CSSStyleSheetImpl* parent = (CSSStyleSheetImpl*)mParent;
    parent->ClearRuleCascades();
  }
}

nsresult
CSSStyleSheetImpl::WillDirty(void)
{
  return EnsureUniqueInner();
}

void
CSSStyleSheetImpl::DidDirty(void)
{
  ClearRuleCascades();
  mDirty = PR_TRUE;
}

NS_IMETHODIMP 
CSSStyleSheetImpl::IsUnmodified(void) const
{
  return ((mDirty) ? NS_COMFALSE : NS_OK);
}

NS_IMETHODIMP
CSSStyleSheetImpl::SetModified(PRBool aModified)
{
  mDirty = aModified;
  return NS_OK;
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
        (pseudoClass->mAtom == nsCSSAtoms::checkedPseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::disabledPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::dragOverPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::dragPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::enabledPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::focusPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::hoverPseudo) || 
        (pseudoClass->mAtom == nsCSSAtoms::linkPseudo) ||
        (pseudoClass->mAtom == nsCSSAtoms::outOfDatePseudo) ||
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

struct CascadeEnumData {
  CascadeEnumData(nsIAtom* aMedium, nsISupportsArray* aRules)
    : mMedium(aMedium),
      mRules(aRules)
  {
  }
  nsIAtom* mMedium;
  nsISupportsArray* mRules;
};

static PRBool
GatherStyleRulesForMedium(nsISupports* aRule, void* aData)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  CascadeEnumData* data = (CascadeEnumData*)aData;
  PRInt32 type;
  rule->GetType(type);

  if (nsICSSRule::STYLE_RULE == type) {
    nsICSSStyleRule* styleRule = (nsICSSStyleRule*)rule;
    data->mRules->AppendElement(styleRule);
  }
  else if (nsICSSRule::MEDIA_RULE == type) {
    nsICSSMediaRule* mediaRule = (nsICSSMediaRule*)rule;
    if (NS_OK == mediaRule->UseForMedium(data->mMedium)) {
      mediaRule->EnumerateRulesForwards(GatherStyleRulesForMedium, aData);
    }
  }
  return PR_TRUE;
}

nsresult
CSSStyleSheetImpl::GatherRulesFor(nsIAtom* aMedium, nsISupportsArray* aRules)
{
  if (aRules) {
    CSSStyleSheetImpl*  child = mFirstChild;
    while (nsnull != child) {
      if (NS_OK == child->UseForMedium(aMedium)) {
        child->GatherRulesFor(aMedium, aRules);
      }
      child = child->mNext;
    }

    if (mInner && mInner->mOrderedRules) {
      CascadeEnumData data(aMedium, aRules);
      mInner->mOrderedRules->EnumerateForwards(GatherStyleRulesForMedium, &data);
    }
  }
  return NS_OK;
}

#ifdef DEBUG_CASCADE

struct WeightEnumData {
  WeightEnumData(PRInt32 aWeight)
    : mWeight(aWeight),
      mIndex(0)
  {
  }
  PRInt32 mWeight;
  PRInt32 mIndex;
};

static PRBool
FindEndOfWeight(nsISupports* aRule, void* aData)
{
  nsICSSStyleRule* rule = (nsICSSStyleRule*)aRule;
  WeightEnumData* data = (WeightEnumData*)aData;
  if (rule->GetWeight() <= data->mWeight) {
    return PR_FALSE;  // stop loop
  }
  data->mIndex++;
  return PR_TRUE;
}

static PRBool
InsertRuleByWeight(nsISupports* aRule, void* aData)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  CascadeEnumData* data = (CascadeEnumData*)aData;
  PRInt32 type;
  rule->GetType(type);

  if (nsICSSRule::STYLE_RULE == type) {
    nsICSSStyleRule* styleRule = (nsICSSStyleRule*)rule;

    WeightEnumData  weight(styleRule->GetWeight());
    data->mRules->EnumerateForwards(FindEndOfWeight, &weight);

    data->mRules->InsertElementAt(styleRule, weight.mIndex);
  }
  else if (nsICSSRule::MEDIA_RULE == type) {
    nsICSSMediaRule* mediaRule = (nsICSSMediaRule*)rule;
    if (NS_OK == mediaRule->UseForMedium(data->mMedium)) {
      mediaRule->EnumerateRulesForwards(InsertRuleByWeight, aData);
    }
  }
  return PR_TRUE;
}

nsresult
CSSStyleSheetImpl::SlowCascadeRulesInto(nsIAtom* aMedium, nsISupportsArray* aRules)
{
  if (aRules) {
    // get child rules first
    CSSStyleSheetImpl*  child = mFirstChild;
    while (nsnull != child) {
      if (NS_OK == child->UseForMedium(aMedium)) {
        child->SlowCascadeRulesInto(aMedium, aRules);
      }
      child = child->mNext;
    }
  
    if (mInner && mInner->mOrderedRules) {
      CascadeEnumData data(aMedium, aRules);
      mInner->mOrderedRules->EnumerateForwards(InsertRuleByWeight, &data);
    }
  }
  return NS_OK;
}
#endif

#ifdef FAST_CASCADE
static PRInt32
CompareStyleRuleWeight(nsISupports* aRule1, nsISupports* aRule2, void* aData)
{
  nsICSSStyleRule* rule1 = (nsICSSStyleRule*)aRule1;
  nsICSSStyleRule* rule2 = (nsICSSStyleRule*)aRule2;
  return (rule2->GetWeight() - rule1->GetWeight());
}
#endif

nsresult
CSSStyleSheetImpl::CascadeRulesInto(nsIAtom* aMedium, nsISupportsArray* aRules)
{
#ifdef FAST_CASCADE
  if (aRules) {
    GatherRulesFor(aMedium, aRules);
    aRules->ShellSort(CompareStyleRuleWeight, nsnull);

#ifdef DEBUG_CASCADE
    nsISupportsArray* verifyArray = nsnull;
    NS_NewISupportsArray(&verifyArray);
    if (verifyArray) {
      SlowCascadeRulesInto(aMedium, verifyArray);
      NS_ASSERTION(verifyArray->Equals(aRules), "fast cascade failed");
      NS_RELEASE(verifyArray);
    }
#endif
  }
  return NS_OK;
#else
  return SlowCascadeRulesInto(aMedium, aRules);
#endif
}


RuleCascadeData* 
CSSStyleSheetImpl::GetRuleCascade(nsIAtom* aMedium)
{
  AtomKey mediumKey(aMedium);
  RuleCascadeData* cascade = nsnull;

  if (mInner) {
    if (mInner->mMediumCascadeTable) {
      cascade = (RuleCascadeData*)mInner->mMediumCascadeTable->Get(&mediumKey);
    }

    if (! cascade) {
      if (mInner->mOrderedRules || mFirstChild) {
        if (! mInner->mMediumCascadeTable) {
          mInner->mMediumCascadeTable = new nsHashtable();
        }
        if (mInner->mMediumCascadeTable) {
          cascade = new RuleCascadeData();
          if (cascade) {
            mInner->mMediumCascadeTable->Put(&mediumKey, cascade);

            CascadeRulesInto(aMedium, cascade->mWeightedRules);
            cascade->mWeightedRules->EnumerateBackwards(BuildHashEnum, &(cascade->mRuleHash));
            cascade->mWeightedRules->EnumerateBackwards(BuildStateEnum, &(cascade->mStateSelectors));
          }
        }
      }
    }
  }
  return cascade;
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
  if (mInner && mInner->mURL) {
#ifdef NECKO
    char* str = nsnull;
    mInner->mURL->GetSpec(&str);
    aHref = str;
    if (str) {
      nsCRT::free(str);
    }
#else
    PRUnichar* str;
    mInner->mURL->ToString(&str);
    aHref = str;
    delete [] str;
#endif
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
    PRUint32 cnt;
    nsresult rv = mMedia->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    PRInt32 count = cnt;
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
  // XXX should get parser from CSS loader
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
      result = css->Parse(input, mInner->mURL, tmp);
      NS_ASSERTION(tmp == this, "parser incorrectly created a new stylesheet");
      NS_RELEASE(tmp);
      NS_RELEASE(input);
      PRUint32 cnt;
      if (mInner && mInner->mOrderedRules) {
        result = mInner->mOrderedRules->Count(&cnt);
        if (NS_SUCCEEDED(result)) {
          *aReturn = cnt;
          if (nsnull != mDocument) {
            nsICSSRule* rule;

            rule = (nsICSSRule*)(mInner->mOrderedRules->ElementAt(aIndex));
            mDocument->StyleRuleAdded(this, rule);
            NS_IF_RELEASE(rule);
          }
        }
      }
    }

    NS_RELEASE(css);
  }

  return result;
}

NS_IMETHODIMP    
CSSStyleSheetImpl::DeleteRule(PRUint32 aIndex)
{
  nsresult result = NS_ERROR_INVALID_ARG;

  // XXX TBI: handle @rule types
  if (mInner && mInner->mOrderedRules) {
    result = WillDirty();
  
    if (NS_SUCCEEDED(result)) {
      nsICSSRule *rule;

      rule = (nsICSSRule*)(mInner->mOrderedRules->ElementAt(aIndex));
      if (nsnull != rule) {
        mInner->mOrderedRules->RemoveElementAt(aIndex);
        rule->SetStyleSheet(nsnull);
        DidDirty();
        NS_RELEASE(rule);
      }
    }
  }

  return result;
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
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURI* aURL)
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
