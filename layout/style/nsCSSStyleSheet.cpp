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
      ::delete ptr;
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

static PRBool SelectorMatches(nsIPresContext* aPresContext,
                              nsCSSSelector* aSelector, nsIContent* aContent)
{
  PRBool  result = PR_FALSE;
  nsIAtom*  contentTag;
  aContent->GetTag(contentTag);

  if ((nsnull == aSelector->mTag) || (aSelector->mTag == contentTag)) {
    if ((nsnull != aSelector->mClass) || (nsnull != aSelector->mID) || 
        (nsnull != aSelector->mPseudoClass)) {
      nsIHTMLContent* htmlContent;
      if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
        nsIAtom* contentClass;
        htmlContent->GetClass(contentClass);
        nsIAtom* contentID;
        htmlContent->GetID(contentID);
        if ((nsnull == aSelector->mClass) || (aSelector->mClass == contentClass)) {
          if ((nsnull == aSelector->mID) || (aSelector->mID == contentID)) {
            if ((contentTag == nsHTMLAtoms::a) && (nsnull != aSelector->mPseudoClass)) {
              // test link state
              nsILinkHandler* linkHandler;

              if ((NS_OK == aPresContext->GetLinkHandler(&linkHandler)) &&
                  (nsnull != linkHandler)) {
                nsAutoString base, href;  // XXX base??
                nsresult attrState = htmlContent->GetAttribute("href", href);

                if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
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

static void ContentEnumFunc(nsICSSStyleRule* aRule, void* aData)
{
  ContentEnumData* data = (ContentEnumData*)aData;

  nsCSSSelector* selector = aRule->FirstSelector();
  if (SelectorMatches(data->mPresContext, selector, data->mContent)) {
    selector = selector->mNext;

    nsIContent* content = nsnull;
    nsIContent* lastContent = nsnull;
    data->mContent->GetParent(content);
    while ((nsnull != selector) && (nsnull != content)) { // check compound selectors
      if (SelectorMatches(data->mPresContext, selector, content)) {
        selector = selector->mNext;
      }
      lastContent = content;
      content->GetParent(content);
      NS_IF_RELEASE(lastContent);
    }
    NS_IF_RELEASE(content);
    if (nsnull == selector) { // ran out, it matched
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
  aPresContext->GetMedium(presMedium);
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
    nsIAtom* classAtom = nsnull;

    nsIHTMLContent* htmlContent;
    if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
      htmlContent->GetID(idAtom);
      htmlContent->GetClass(classAtom);
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
    mWeightedRules->EnumerateBackwards(ContentEnumWrap, &data);
    NS_ASSERTION(list1->Equals(list2), "lists not equal");
    NS_RELEASE(list1);
    NS_RELEASE(list2);
#endif

    NS_IF_RELEASE(tagAtom);
    NS_IF_RELEASE(idAtom);
    NS_IF_RELEASE(classAtom);
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
    nsIContent* content = data->mParentContent;
    nsIContent* lastContent = nsnull;
    NS_IF_ADDREF(content);
    while ((nsnull != selector) && (nsnull != content)) { // check compound selectors
      if (SelectorMatches(data->mPresContext, selector, content)) {
        selector = selector->mNext;
      }
      lastContent = content;
      content->GetParent(content);
      NS_IF_RELEASE(lastContent);
    }
    NS_IF_RELEASE(content);
    if (nsnull == selector) { // ran out, it matched
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
  aPresContext->GetMedium(presMedium);
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

NS_IMETHODIMP
CSSStyleSheetImpl::Init(nsIURL* aURL)
{
  NS_PRECONDITION(aURL, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(mURL == nsnull, "already initialized");
  if (mURL != nsnull)
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

  PRBool result = (*mURL == *aURL);

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
    mWeightedRules->EnumerateBackwards(BuildHashEnum, mRuleHash);
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
    mURL->ToString(aHref);
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
    nsAutoString str(aRule);
    nsIUnicharInputStream* input = nsnull;
    result = NS_NewStringUnicharInputStream(&input, &str);
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
