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
#include "nsCOMPtr.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsIArena.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsStyleUtil.h"
#include "nsIFontMetrics.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"

//#define DEBUG_REFS

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);
static NS_DEFINE_IID(kIDOMCSSStyleSheetIID, NS_IDOMCSSSTYLESHEET_IID);
static NS_DEFINE_IID(kIDOMCSSRuleIID, NS_IDOMCSSRULE_IID);
static NS_DEFINE_IID(kIDOMCSSStyleRuleIID, NS_IDOMCSSSTYLERULE_IID);
static NS_DEFINE_IID(kIDOMCSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);
static NS_DEFINE_IID(kCSSTableSID, NS_CSS_TABLE_SID);
static NS_DEFINE_IID(kCSSContentSID, NS_CSS_CONTENT_SID);

// -- nsCSSSelector -------------------------------

#define NS_IF_COPY(dest,source,type)  \
  if (nsnull != source)  dest = new type(*(source))

#define NS_IF_DELETE(ptr)   \
  if (nsnull != ptr) { delete ptr; ptr = nsnull; }

nsAtomList::nsAtomList(nsIAtom* aAtom)
  : mAtom(aAtom),
    mNext(nsnull)
{
  NS_IF_ADDREF(mAtom);
}

nsAtomList::nsAtomList(const nsString& aAtomValue)
  : mAtom(nsnull),
    mNext(nsnull)
{
  mAtom = NS_NewAtom(aAtomValue);
}

nsAtomList::nsAtomList(const nsAtomList& aCopy)
  : mAtom(aCopy.mAtom),
    mNext(nsnull)
{
  NS_IF_ADDREF(mAtom);
  NS_IF_COPY(mNext, aCopy.mNext, nsAtomList);
}

nsAtomList::~nsAtomList(void)
{
  NS_IF_RELEASE(mAtom);
  NS_IF_DELETE(mNext);
}

PRBool nsAtomList::Equals(const nsAtomList* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if (mAtom == aOther->mAtom) {
      if (nsnull != mNext) {
        return mNext->Equals(aOther->mNext);
      }
      return PRBool(nsnull == aOther->mNext);
    }
  }
  return PR_FALSE;
}

nsAttrSelector::nsAttrSelector(const nsString& aAttr)
  : mAttr(nsnull),
    mFunction(NS_ATTR_FUNC_SET),
    mCaseSensitive(1),
    mValue(),
    mNext(nsnull)
{
  mAttr = NS_NewAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(const nsString& aAttr, PRUint8 aFunction, const nsString& aValue,
                               PRBool aCaseSensitive)
  : mAttr(nsnull),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive),
    mValue(aValue),
    mNext(nsnull)
{
  mAttr = NS_NewAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(const nsAttrSelector& aCopy)
  : mAttr(aCopy.mAttr),
    mFunction(aCopy.mFunction),
    mCaseSensitive(aCopy.mCaseSensitive),
    mValue(aCopy.mValue),
    mNext(nsnull)
{
  NS_IF_ADDREF(mAttr);
  NS_IF_COPY(mNext, aCopy.mNext, nsAttrSelector);
}

nsAttrSelector::~nsAttrSelector(void)
{
  NS_IF_RELEASE(mAttr);
  NS_IF_DELETE(mNext);
}

PRBool nsAttrSelector::Equals(const nsAttrSelector* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if ((mAttr == aOther->mAttr) && 
        (mFunction == aOther->mFunction) && 
        (mCaseSensitive == aOther->mCaseSensitive) &&
        mValue.Equals(aOther->mValue)) {
      if (nsnull != mNext) {
        return mNext->Equals(aOther->mNext);
      }
      return PRBool(nsnull == aOther->mNext);
    }
  }
  return PR_FALSE;
}

nsCSSSelector::nsCSSSelector(void)
  : mNameSpace(kNameSpaceID_Unknown), mTag(nsnull), 
    mID(nsnull), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(0),
    mNext(nsnull)
{
}

nsCSSSelector::nsCSSSelector(const nsCSSSelector& aCopy) 
  : mNameSpace(aCopy.mNameSpace), mTag(aCopy.mTag), 
    mID(aCopy.mID), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(aCopy.mOperator),
    mNext(nsnull)
{
  NS_IF_ADDREF(mTag);
  NS_IF_ADDREF(mID);
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
}

nsCSSSelector::~nsCSSSelector(void)  
{
  Reset();
}

nsCSSSelector& nsCSSSelector::operator=(const nsCSSSelector& aCopy)
{
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);

  mNameSpace    = aCopy.mNameSpace;
  mTag          = aCopy.mTag;
  mID           = aCopy.mID;
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
  mOperator     = aCopy.mOperator;

  NS_IF_ADDREF(mTag);
  NS_IF_ADDREF(mID);
  return *this;
}

PRBool nsCSSSelector::Equals(const nsCSSSelector* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if ((aOther->mNameSpace == mNameSpace) && 
        (aOther->mTag == mTag) && 
        (aOther->mID == mID) && 
        (aOther->mOperator == mOperator)) {
      if (nsnull != mClassList) {
        if (PR_FALSE == mClassList->Equals(aOther->mClassList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mClassList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mPseudoClassList) {
        if (PR_FALSE == mPseudoClassList->Equals(aOther->mPseudoClassList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mPseudoClassList) {
          return PR_FALSE;
        }
      }
      if (nsnull != mAttrList) {
        if (PR_FALSE == mAttrList->Equals(aOther->mAttrList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mAttrList) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


void nsCSSSelector::Reset(void)
{
  mNameSpace = kNameSpaceID_Unknown;
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  mOperator = PRUnichar(0);
}

void nsCSSSelector::SetNameSpace(PRInt32 aNameSpace)
{
  mNameSpace = aNameSpace;
}

void nsCSSSelector::SetTag(const nsString& aTag)
{
  NS_IF_RELEASE(mTag);
  if (0 < aTag.Length()) {
    mTag = NS_NewAtom(aTag);
  }
}

void nsCSSSelector::SetID(const nsString& aID)
{
  NS_IF_RELEASE(mID);
  if (0 < aID.Length()) {
    mID = NS_NewAtom(aID);
  }
}

void nsCSSSelector::AddClass(const nsString& aClass)
{
  if (0 < aClass.Length()) {
    nsAtomList** list = &mClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aClass);
  }
}

void nsCSSSelector::AddPseudoClass(const nsString& aPseudoClass)
{
  if (0 < aPseudoClass.Length()) {
    nsAtomList** list = &mPseudoClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aPseudoClass);
  }
}

void nsCSSSelector::AddPseudoClass(nsIAtom* aPseudoClass)
{
  if (nsnull != aPseudoClass) {
    nsAtomList** list = &mPseudoClassList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aPseudoClass);
  }
}

void nsCSSSelector::AddAttribute(const nsString& aAttr)
{
  if (0 < aAttr.Length()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aAttr);
  }
}

void nsCSSSelector::AddAttribute(const nsString& aAttr, PRUint8 aFunc, const nsString& aValue,
                                 PRBool aCaseSensitive)
{
  if (0 < aAttr.Length()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aAttr, aFunc, aValue, aCaseSensitive);
  }
}

void nsCSSSelector::SetOperator(PRUnichar aOperator)
{
  mOperator = aOperator;
}

PRInt32 nsCSSSelector::CalcWeight(void) const
{
  PRInt32 weight = 0;

  if (nsnull != mTag) {
    weight += 0x000001;
  }
  if (nsnull != mID) {
    weight += 0x010000;
  }
  nsAtomList* list = mClassList;
  while (nsnull != list) {
    weight += 0x000100;
    list = list->mNext;
  }
  list = mPseudoClassList;
  while (nsnull != list) {
    weight += 0x000100;
    list = list->mNext;
  }
  nsAttrSelector* attr = mAttrList;
  while (nsnull != attr) {
    weight += 0x000100;
    attr = attr->mNext;
  }
  return weight;
}

// -- CSSImportantRule -------------------------------

static nscoord CalcLength(const nsCSSValue& aValue, const nsStyleFont* aFont, 
                          nsIPresContext* aPresContext);
static PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                       PRInt32 aMask, const nsStyleFont* aFont, 
                       nsIPresContext* aPresContext);

static void MapDeclarationInto(nsICSSDeclaration* aDeclaration, 
                               nsIStyleContext* aContext, nsIPresContext* aPresContext);


class CSSStyleRuleImpl;

class CSSImportantRule : public nsIStyleRule {
public:
  CSSImportantRule(nsICSSStyleSheet* aSheet, nsICSSDeclaration* aDeclaration);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

protected:
  virtual ~CSSImportantRule(void);

  nsICSSDeclaration*  mDeclaration;
  nsICSSStyleSheet*   mSheet;

friend CSSStyleRuleImpl;
};

CSSImportantRule::CSSImportantRule(nsICSSStyleSheet* aSheet, nsICSSDeclaration* aDeclaration)
  : mDeclaration(aDeclaration),
    mSheet(aSheet)
{
  NS_INIT_REFCNT();
  NS_IF_ADDREF(mDeclaration);
}

CSSImportantRule::~CSSImportantRule(void)
{
  NS_IF_RELEASE(mDeclaration);
}

NS_IMPL_ISUPPORTS(CSSImportantRule, kIStyleRuleIID);

NS_IMETHODIMP
CSSImportantRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(aRule == this);
  return NS_OK;
}

NS_IMETHODIMP
CSSImportantRule::HashValue(PRUint32& aValue) const
{
  aValue = PRUint32(mDeclaration);
  return NS_OK;
}

NS_IMETHODIMP
CSSImportantRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, useful for mapping CSS ! important
NS_IMETHODIMP
CSSImportantRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 1;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportantRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  MapDeclarationInto(mDeclaration, aContext, aPresContext);
  return NS_OK;
}

NS_IMETHODIMP
CSSImportantRule::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("! Important rule ", out);
  if (nsnull != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);

  return NS_OK;
}

// -- nsDOMStyleRuleDeclaration -------------------------------

class DOMCSSDeclarationImpl : public nsDOMCSSDeclaration
{
public:
  DOMCSSDeclarationImpl(nsICSSStyleRule *aRule);
  ~DOMCSSDeclarationImpl();

  virtual void DropReference();
  virtual nsresult GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                     PRBool aAllocate);
  virtual nsresult StylePropertyChanged(const nsString& aPropertyName,
                                        PRInt32 aHint);
  virtual nsresult GetParent(nsISupports **aParent);
  virtual nsresult GetBaseURL(nsIURL** aURL);

protected:
  nsICSSStyleRule *mRule;
};

DOMCSSDeclarationImpl::DOMCSSDeclarationImpl(nsICSSStyleRule *aRule)
{
  // This reference is not reference-counted. The rule
  // object tells us when its about to go away.
  mRule = aRule;
}

DOMCSSDeclarationImpl::~DOMCSSDeclarationImpl()
{
}

void 
DOMCSSDeclarationImpl::DropReference()
{
  mRule = nsnull;
}

nsresult
DOMCSSDeclarationImpl::GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                             PRBool aAllocate)
{
  if (nsnull != mRule) {
    *aDecl = mRule->GetDeclaration();
  }
  else {
    *aDecl = nsnull;
  }

  return NS_OK;
}

nsresult 
DOMCSSDeclarationImpl::StylePropertyChanged(const nsString& aPropertyName,
                                            PRInt32 aHint)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull != mRule) {
    mRule->GetStyleSheet(sheet);
    if (nsnull != sheet) {
      nsIDocument*  doc = nsnull;
      sheet->GetOwningDocument(doc);
      if (nsnull != doc) {
        doc->StyleRuleChanged(sheet, mRule, aHint);
      }
    }
  }

  return NS_OK;
}

nsresult 
DOMCSSDeclarationImpl::GetParent(nsISupports **aParent)
{
  if (nsnull != mRule) {
    return mRule->QueryInterface(kISupportsIID, (void **)aParent);
  }

  return NS_OK;
}

nsresult
DOMCSSDeclarationImpl::GetBaseURL(nsIURL** aURL)
{
  NS_ASSERTION(nsnull != aURL, "null pointer");

  nsresult result = NS_ERROR_NULL_POINTER;

  if (nsnull != aURL) {
    *aURL = nsnull;
    if (nsnull != mRule) {
      nsIStyleSheet* sheet = nsnull;
      result = mRule->GetStyleSheet(sheet);
      if (nsnull != sheet) {
        result = sheet->GetURL(*aURL);
      }
    }
  }
  return result;
}

// -- nsCSSStyleRule -------------------------------

class CSSStyleRuleImpl : public nsICSSStyleRule, 
                         public nsIDOMCSSStyleRule, 
                         public nsIScriptObjectOwner {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleRuleImpl(const nsCSSSelector& aSelector);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  virtual nsCSSSelector* FirstSelector(void);
  virtual void AddSelector(const nsCSSSelector& aSelector);
  virtual void DeleteSelector(nsCSSSelector* aSelector);
  virtual void SetSourceSelectorText(const nsString& aSelectorText);
  virtual void GetSourceSelectorText(nsString& aSelectorText) const;

  virtual nsICSSDeclaration* GetDeclaration(void) const;
  virtual void SetDeclaration(nsICSSDeclaration* aDeclaration);

  virtual PRInt32 GetWeight(void) const;
  virtual void SetWeight(PRInt32 aWeight);

  virtual nsIStyleRule* GetImportantRule(void);

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  // nsIDOMCSSRule interface
  NS_IMETHOD    GetType(PRUint16* aType);
  NS_IMETHOD    GetCssText(nsString& aCssText);
  NS_IMETHOD    SetCssText(const nsString& aCssText);
  NS_IMETHOD    GetSheet(nsIDOMCSSStyleSheet** aSheet);

  // nsIDOMCSSStyleRule interface
  NS_IMETHOD    GetSelectorText(nsString& aSelectorText);
  NS_IMETHOD    SetSelectorText(const nsString& aSelectorText);
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);
  NS_IMETHOD    SetStyle(nsIDOMCSSStyleDeclaration* aStyle);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

private: 
  // These are not supported and are not implemented! 
  CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy); 
  CSSStyleRuleImpl& operator=(const CSSStyleRuleImpl& aCopy); 

protected:
  virtual ~CSSStyleRuleImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsCSSSelector           mSelector;
  nsString                mSelectorText;
  nsICSSDeclaration*      mDeclaration;
  PRInt32                 mWeight;
  CSSImportantRule*       mImportantRule;
  nsICSSStyleSheet*       mSheet;                         
  DOMCSSDeclarationImpl*  mDOMDeclaration;                          
  void*                   mScriptObject;                           
#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};


void* CSSStyleRuleImpl::operator new(size_t size)
{
  CSSStyleRuleImpl* rv = (CSSStyleRuleImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* CSSStyleRuleImpl::operator new(size_t size, nsIArena* aArena)
{
  CSSStyleRuleImpl* rv = (CSSStyleRuleImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void CSSStyleRuleImpl::operator delete(void* ptr)
{
  CSSStyleRuleImpl* rule = (CSSStyleRuleImpl*) ptr;
  if (nsnull != rule) {
    if (rule->mInHeap) {
      ::operator delete(ptr);
    }
  }
}


#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
static const PRInt32 kInstrument = 1075;
#endif

CSSStyleRuleImpl::CSSStyleRuleImpl(const nsCSSSelector& aSelector)
  : mSelector(aSelector), mSelectorText(), mDeclaration(nsnull), 
    mWeight(0), mImportantRule(nsnull)
{
  NS_INIT_REFCNT();
  mDOMDeclaration = nsnull;
  mScriptObject = nsnull;
#ifdef DEBUG_REFS
  mInstance = gInstanceCount++;
  fprintf(stdout, "%d of %d + CSSStyleRule\n", mInstance, gInstanceCount);
#endif
}

CSSStyleRuleImpl::~CSSStyleRuleImpl()
{
  nsCSSSelector*  next = mSelector.mNext;

  while (nsnull != next) {
    nsCSSSelector*  selector = next;
    next = selector->mNext;
    delete selector;
  }
  NS_IF_RELEASE(mDeclaration);
  if (nsnull != mImportantRule) {
    mImportantRule->mSheet = nsnull;
    NS_RELEASE(mImportantRule);
  }
  if (nsnull != mDOMDeclaration) {
    mDOMDeclaration->DropReference();
  }
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d of %d - CSSStyleRule\n", mInstance, gInstanceCount);
#endif
}

#ifdef DEBUG_REFS
nsrefcnt CSSStyleRuleImpl::AddRef(void)                                
{                                    
  if (mInstance == kInstrument) {
    fprintf(stdout, "%d AddRef CSSStyleRule\n", mRefCnt + 1);
  }
  return ++mRefCnt;                                          
}

nsrefcnt CSSStyleRuleImpl::Release(void)                         
{                                                      
  if (mInstance == kInstrument) {
    fprintf(stdout, "%d Release CSSStyleRule\n", mRefCnt - 1);
  }
  if (--mRefCnt == 0) {                                
    NS_DELETEXPCOM(this);
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}
#else
NS_IMPL_ADDREF(CSSStyleRuleImpl)
NS_IMPL_RELEASE(CSSStyleRuleImpl)
#endif

nsresult CSSStyleRuleImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kICSSStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMCSSRuleIID)) {
    nsIDOMCSSRule *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMCSSStyleRuleIID)) {
    nsIDOMCSSStyleRule *tmp = this;
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
    nsICSSStyleRule *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP CSSStyleRuleImpl::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  nsICSSStyleRule* iCSSRule;

  if (this == aRule) {
    aResult = PR_TRUE;
  }
  else {
    aResult = PR_FALSE;
    if ((nsnull != aRule) && 
        (NS_OK == ((nsIStyleRule*)aRule)->QueryInterface(kICSSStyleRuleIID, (void**) &iCSSRule))) {

      CSSStyleRuleImpl* rule = (CSSStyleRuleImpl*)iCSSRule;
      const nsCSSSelector* local = &mSelector;
      const nsCSSSelector* other = &(rule->mSelector);
      aResult = PR_TRUE;

      if ((rule->mDeclaration != mDeclaration) || 
          (rule->mWeight != mWeight)) {
        aResult = PR_FALSE;
      }
      while ((PR_TRUE == aResult) && (nsnull != local) && (nsnull != other)) {
        if (! local->Equals(other)) {
          aResult = PR_FALSE;
        }
        local = local->mNext;
        other = other->mNext;
      }
      if ((nsnull != local) || (nsnull != other)) { // more were left
        aResult = PR_FALSE;
      }
      NS_RELEASE(iCSSRule);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)this;
  return NS_OK;
}

// Strength is an out-of-band weighting, useful for mapping CSS ! important
NS_IMETHODIMP
CSSStyleRuleImpl::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

nsCSSSelector* CSSStyleRuleImpl::FirstSelector(void)
{
  return &mSelector;
}

void CSSStyleRuleImpl::AddSelector(const nsCSSSelector& aSelector)
{
  nsCSSSelector*  selector = new nsCSSSelector(aSelector);
  nsCSSSelector*  last = &mSelector;

  while (nsnull != last->mNext) {
    last = last->mNext;
  }
  last->mNext = selector;
}


void CSSStyleRuleImpl::DeleteSelector(nsCSSSelector* aSelector)
{
  if (nsnull != aSelector) {
    if (&mSelector == aSelector) {  // handle first selector
      if (nsnull != mSelector.mNext) {
        nsCSSSelector* nextOne = mSelector.mNext;
        mSelector = *nextOne; // assign values
        mSelector.mNext = nextOne->mNext;
        delete nextOne;
      }
      else {
        mSelector.Reset();
      }
    }
    else {
      nsCSSSelector*  selector = &mSelector;

      while (nsnull != selector->mNext) {
        if (aSelector == selector->mNext) {
          selector->mNext = aSelector->mNext;
          delete aSelector;
          return;
        }
        selector = selector->mNext;
      }
    }
  }
}

void CSSStyleRuleImpl::SetSourceSelectorText(const nsString& aSelectorText)
{
  mSelectorText = aSelectorText;
}

void CSSStyleRuleImpl::GetSourceSelectorText(nsString& aSelectorText) const
{
  aSelectorText = mSelectorText;
}


nsICSSDeclaration* CSSStyleRuleImpl::GetDeclaration(void) const
{
  NS_IF_ADDREF(mDeclaration);
  return mDeclaration;
}

void CSSStyleRuleImpl::SetDeclaration(nsICSSDeclaration* aDeclaration)
{
  if (mDeclaration != aDeclaration) {
    NS_IF_RELEASE(mImportantRule); 
    NS_IF_RELEASE(mDeclaration);
    mDeclaration = aDeclaration;
    NS_IF_ADDREF(mDeclaration);
  }
}

PRInt32 CSSStyleRuleImpl::GetWeight(void) const
{
  return mWeight;
}

void CSSStyleRuleImpl::SetWeight(PRInt32 aWeight)
{
  mWeight = aWeight;
}

nsIStyleRule* CSSStyleRuleImpl::GetImportantRule(void)
{
  if ((nsnull == mImportantRule) && (nsnull != mDeclaration)) {
    nsICSSDeclaration*  important;
    mDeclaration->GetImportantValues(important);
    if (nsnull != important) {
      mImportantRule = new CSSImportantRule(mSheet, important);
      NS_ADDREF(mImportantRule);
      NS_RELEASE(important);
    }
  }
  NS_IF_ADDREF(mImportantRule);
  return mImportantRule;
}

NS_IMETHODIMP
CSSStyleRuleImpl::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = aSheet;
  if (nsnull != mImportantRule) { // we're responsible for this guy too
    mImportantRule->mSheet = aSheet;
  }
  return NS_OK;
}

nscoord CalcLength(const nsCSSValue& aValue,
                   const nsStyleFont* aFont, 
                   nsIPresContext* aPresContext)
{
  NS_ASSERTION(aValue.IsLengthUnit(), "not a length unit");
  if (aValue.IsFixedLengthUnit()) {
    return aValue.GetLengthTwips();
  }
  nsCSSUnit unit = aValue.GetUnit();
  switch (unit) {
    case eCSSUnit_EM:
      return NSToCoordRound(aValue.GetFloatValue() * (float)aFont->mFont.size);
      // XXX scale against font metrics height instead
    case eCSSUnit_EN:
      return NSToCoordRound((aValue.GetFloatValue() * (float)aFont->mFont.size) / 2.0f);
    case eCSSUnit_XHeight: {
      nsIFontMetrics* fm;
      aPresContext->GetMetricsFor(aFont->mFont, &fm);
      NS_ASSERTION(nsnull != fm, "can't get font metrics");
      nscoord xHeight;
      if (nsnull != fm) {
        fm->GetXHeight(xHeight);
        NS_RELEASE(fm);
      }
      else {
        xHeight = ((aFont->mFont.size * 2) / 3);
      }
      return NSToCoordRound(aValue.GetFloatValue() * (float)xHeight);
    }
    case eCSSUnit_CapHeight: {
      NS_NOTYETIMPLEMENTED("cap height unit");
      nscoord capHeight = ((aFont->mFont.size / 3) * 2); // XXX HACK!
      return NSToCoordRound(aValue.GetFloatValue() * (float)capHeight);
    }
    case eCSSUnit_Pixel:
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      return NSFloatPixelsToTwips(aValue.GetFloatValue(), p2t);

    default:
      break;
  }
  return 0;
}


#define SETCOORD_NORMAL       0x01
#define SETCOORD_AUTO         0x02
#define SETCOORD_INHERIT      0x04
#define SETCOORD_PERCENT      0x08
#define SETCOORD_FACTOR       0x10
#define SETCOORD_LENGTH       0x20
#define SETCOORD_INTEGER      0x40
#define SETCOORD_ENUMERATED   0x80

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPFHN  (SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_NORMAL)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPEH   (SETCOORD_LP | SETCOORD_ENUMERATED | SETCOORD_INHERIT)
#define SETCOORD_LE     (SETCOORD_LENGTH | SETCOORD_ENUMERATED)
#define SETCOORD_LEH    (SETCOORD_LE | SETCOORD_INHERIT)
#define SETCOORD_IAH    (SETCOORD_INTEGER | SETCOORD_AH)

static PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                       PRInt32 aMask, const nsStyleFont* aFont, 
                       nsIPresContext* aPresContext)
{
  PRBool  result = PR_TRUE;
  if (aValue.GetUnit() == eCSSUnit_Null) {
    result = PR_FALSE;
  }
  else if (((aMask & SETCOORD_LENGTH) != 0) && 
           aValue.IsLengthUnit()) {
    aCoord.SetCoordValue(CalcLength(aValue, aFont, aPresContext));
  } 
  else if (((aMask & SETCOORD_PERCENT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Percent)) {
    aCoord.SetPercentValue(aValue.GetPercentValue());
  } 
  else if (((aMask & SETCOORD_INTEGER) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Integer)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Integer);
  } 
  else if (((aMask & SETCOORD_ENUMERATED) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Enumerated)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Enumerated);
  } 
  else if (((aMask & SETCOORD_AUTO) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Auto)) {
    aCoord.SetAutoValue();
  } 
  else if (((aMask & SETCOORD_INHERIT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Inherit)) {
    aCoord.SetInheritValue();
  }
  else if (((aMask & SETCOORD_NORMAL) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Normal)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_FACTOR) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Number)) {
    aCoord.SetFactorValue(aValue.GetFloatValue());
  }
  else {
    result = PR_FALSE;  // didn't set anything
  }
  return result;
}

static PRBool SetColor(const nsCSSValue& aValue, const nscolor aParentColor, nscolor& aResult)
{
  PRBool  result = PR_FALSE;
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Color == unit) {
    aResult = aValue.GetColorValue();
    result = PR_TRUE;
  }
  else if (eCSSUnit_String == unit) {
    nsAutoString  value;
    char  cbuf[100];
    aValue.GetStringValue(value);
    value.ToCString(cbuf, sizeof(cbuf));
    nscolor rgba;
    if (NS_ColorNameToRGB(cbuf, &rgba)) {
      aResult = rgba;
      result = PR_TRUE;
    }
  }
  else if (eCSSUnit_Inherit == unit) {
    aResult = aParentColor;
    result = PR_TRUE;
  }
  return result;
}

NS_IMETHODIMP
CSSStyleRuleImpl::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  MapDeclarationInto(mDeclaration, aContext, aPresContext);
  return NS_OK;
}

nsString& Unquote(nsString& aString)
{
  PRUnichar start = aString.First();
  PRUnichar end = aString.Last();

  if ((start == end) && 
      ((start == PRUnichar('"')) || 
       (start == PRUnichar('\'')))) {
    PRInt32 length = aString.Length();
    aString.Truncate(length - 1);
    aString.Cut(0, 1);
  }
  return aString;
}

void MapDeclarationInto(nsICSSDeclaration* aDeclaration, 
                        nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (nsnull != aDeclaration) {
    nsIStyleContext* parentContext = aContext->GetParent();
    nsStyleFont*  font = (nsStyleFont*)aContext->GetMutableStyleData(eStyleStruct_Font);
    const nsStyleFont* parentFont = font;
    if (nsnull != parentContext) {
      parentFont = (const nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    nsCSSFont*  ourFont;
    if (NS_OK == aDeclaration->GetData(kCSSFontSID, (nsCSSStruct**)&ourFont)) {
      if (nsnull != ourFont) {
        const nsFont& defaultFont = aPresContext->GetDefaultFontDeprecated();
        const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFontDeprecated();

        // font-family: string list, enum, inherit
        if (eCSSUnit_String == ourFont->mFamily.GetUnit()) {
          nsCOMPtr<nsIDeviceContext> dc;
          aPresContext->GetDeviceContext(getter_AddRefs(dc));
          if (dc) {
            nsAutoString  familyList;

            ourFont->mFamily.GetStringValue(familyList);

            font->mFont.name = familyList;
            nsAutoString  face;
            if (NS_OK == dc->FirstExistingFont(font->mFont, face)) {
              if (face.EqualsIgnoreCase("monospace")) {
                font->mFont = font->mFixedFont;
              }
              else {
                font->mFixedFont.name = familyList;
              }
            }
            else {
              font->mFont.name = defaultFont.name;
              font->mFixedFont.name = defaultFixedFont.name;
            }
            font->mFlags |= NS_STYLE_FONT_FACE_EXPLICIT;
          }
        }
        else if (eCSSUnit_Enumerated == ourFont->mFamily.GetUnit()) {
          NS_NOTYETIMPLEMENTED("system font");
        }
        else if (eCSSUnit_Inherit == ourFont->mFamily.GetUnit()) {
          font->mFont.name = parentFont->mFont.name;
          font->mFixedFont.name = parentFont->mFixedFont.name;
          font->mFlags &= ~NS_STYLE_FONT_FACE_EXPLICIT;
          font->mFlags |= (parentFont->mFlags & NS_STYLE_FONT_FACE_EXPLICIT);
        }

        // font-style: enum, normal, inherit
        if (eCSSUnit_Enumerated == ourFont->mStyle.GetUnit()) {
          font->mFont.style = ourFont->mStyle.GetIntValue();
          font->mFixedFont.style = ourFont->mStyle.GetIntValue();
        }
        else if (eCSSUnit_Normal == ourFont->mStyle.GetUnit()) {
          font->mFont.style = NS_STYLE_FONT_STYLE_NORMAL;
          font->mFixedFont.style = NS_STYLE_FONT_STYLE_NORMAL;
        }
        else if (eCSSUnit_Inherit == ourFont->mStyle.GetUnit()) {
          font->mFont.style = parentFont->mFont.style;
          font->mFixedFont.style = parentFont->mFixedFont.style;
        }

        // font-variant: enum, normal, inherit
        if (eCSSUnit_Enumerated == ourFont->mVariant.GetUnit()) {
          font->mFont.variant = ourFont->mVariant.GetIntValue();
          font->mFixedFont.variant = ourFont->mVariant.GetIntValue();
        }
        else if (eCSSUnit_Normal == ourFont->mVariant.GetUnit()) {
          font->mFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
          font->mFixedFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
        }
        else if (eCSSUnit_Inherit == ourFont->mVariant.GetUnit()) {
          font->mFont.variant = parentFont->mFont.variant;
          font->mFixedFont.variant = parentFont->mFixedFont.variant;
        }

        // font-weight: int, enum, normal, inherit
        if (eCSSUnit_Integer == ourFont->mWeight.GetUnit()) {
          font->mFont.weight = ourFont->mWeight.GetIntValue();
          font->mFixedFont.weight = ourFont->mWeight.GetIntValue();
        }
        else if (eCSSUnit_Enumerated == ourFont->mWeight.GetUnit()) {
          PRInt32 value = ourFont->mWeight.GetIntValue();
          switch (value) {
            case NS_STYLE_FONT_WEIGHT_NORMAL:
            case NS_STYLE_FONT_WEIGHT_BOLD:
              font->mFont.weight = value;
              font->mFixedFont.weight = value;
              break;
            case NS_STYLE_FONT_WEIGHT_BOLDER:
            case NS_STYLE_FONT_WEIGHT_LIGHTER:
              font->mFont.weight = (parentFont->mFont.weight + value);
              font->mFixedFont.weight = (parentFont->mFixedFont.weight + value);
              break;
          }
        }
        else if (eCSSUnit_Normal == ourFont->mWeight.GetUnit()) {
          font->mFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
          font->mFixedFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
        }
        else if (eCSSUnit_Inherit == ourFont->mWeight.GetUnit()) {
          font->mFont.weight = parentFont->mFont.weight;
          font->mFixedFont.weight = parentFont->mFixedFont.weight;
        }

        // font-size: enum, length, percent, inherit
        if (eCSSUnit_Enumerated == ourFont->mSize.GetUnit()) {
          PRInt32 value = ourFont->mSize.GetIntValue();
          PRInt32 scaler;
          aPresContext->GetFontScaler(&scaler);
          float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);

          if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
              (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
            font->mFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFont.size, scaleFactor);
            font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFixedFont.size, scaleFactor);
          }
          else if (NS_STYLE_FONT_SIZE_LARGER == value) {
            PRInt32 index = nsStyleUtil::FindNextLargerFontSize(parentFont->mFont.size, (PRInt32)defaultFont.size, scaleFactor);
            font->mFont.size = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFont.size, scaleFactor);
            font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFixedFont.size, scaleFactor);
          }
          else if (NS_STYLE_FONT_SIZE_SMALLER == value) {
            PRInt32 index = nsStyleUtil::FindNextSmallerFontSize(parentFont->mFont.size, (PRInt32)defaultFont.size, scaleFactor);
            font->mFont.size = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFont.size, scaleFactor);
            font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFixedFont.size, scaleFactor);
          }
          // this does NOT explicitly set font size
          font->mFlags &= ~NS_STYLE_FONT_SIZE_EXPLICIT;
        }
        else if (ourFont->mSize.IsLengthUnit()) {
          font->mFont.size = CalcLength(ourFont->mSize, parentFont, aPresContext);
          font->mFixedFont.size = CalcLength(ourFont->mSize, parentFont, aPresContext);
          font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
        }
        else if (eCSSUnit_Percent == ourFont->mSize.GetUnit()) {
          font->mFont.size = (nscoord)((float)(parentFont->mFont.size) * ourFont->mSize.GetPercentValue());
          font->mFixedFont.size = (nscoord)((float)(parentFont->mFixedFont.size) * ourFont->mSize.GetPercentValue());
          font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
        }
        else if (eCSSUnit_Inherit == ourFont->mSize.GetUnit()) {
          font->mFont.size = parentFont->mFont.size;
          font->mFixedFont.size = parentFont->mFixedFont.size;
          font->mFlags &= ~NS_STYLE_FONT_SIZE_EXPLICIT;
          font->mFlags |= (parentFont->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT);
        }
      }
    }

    nsCSSText*  ourText;
    if (NS_OK == aDeclaration->GetData(kCSSTextSID, (nsCSSStruct**)&ourText)) {
      if (nsnull != ourText) {
        // Get our text style and our parent's text style
        nsStyleText* text = (nsStyleText*) aContext->GetMutableStyleData(eStyleStruct_Text);
        const nsStyleText* parentText = text;
        if (nsnull != parentContext) {
          parentText = (const nsStyleText*)parentContext->GetStyleData(eStyleStruct_Text);
        }

        // letter-spacing: normal, length, inherit
        SetCoord(ourText->mLetterSpacing, text->mLetterSpacing, SETCOORD_LH | SETCOORD_NORMAL, 
                 font, aPresContext);

        // line-height: normal, number, length, percent, inherit
        SetCoord(ourText->mLineHeight, text->mLineHeight, SETCOORD_LPFHN, font, aPresContext);

        // text-align: enum, string, inherit
        if (eCSSUnit_Enumerated == ourText->mTextAlign.GetUnit()) {
          text->mTextAlign = ourText->mTextAlign.GetIntValue();
        }
        else if (eCSSUnit_String == ourText->mTextAlign.GetUnit()) {
          NS_NOTYETIMPLEMENTED("align string");
        }
        else if (eCSSUnit_Inherit == ourText->mTextAlign.GetUnit()) {
          text->mTextAlign = parentText->mTextAlign;
        }

        // text-indent: length, percent, inherit
        SetCoord(ourText->mTextIndent, text->mTextIndent, SETCOORD_LPH, font, aPresContext);

        // text-decoration: none, enum (bit field), inherit
        if (eCSSUnit_Enumerated == ourText->mDecoration.GetUnit()) {
          PRInt32 td = ourText->mDecoration.GetIntValue();
          font->mFont.decorations = (parentFont->mFont.decorations | td);
          font->mFixedFont.decorations = (parentFont->mFixedFont.decorations | td);
          text->mTextDecoration = td;
        }
        else if (eCSSUnit_None == ourText->mDecoration.GetUnit()) {
          font->mFont.decorations = parentFont->mFont.decorations;
          font->mFixedFont.decorations = parentFont->mFixedFont.decorations;
          text->mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
        }
        else if (eCSSUnit_Inherit == ourText->mDecoration.GetUnit()) {
          font->mFont.decorations = parentFont->mFont.decorations;
          font->mFixedFont.decorations = parentFont->mFixedFont.decorations;
          text->mTextDecoration = parentText->mTextDecoration;
        }

        // text-transform: enum, none, inherit
        if (eCSSUnit_Enumerated == ourText->mTextTransform.GetUnit()) {
          text->mTextTransform = ourText->mTextTransform.GetIntValue();
        }
        else if (eCSSUnit_None == ourText->mTextTransform.GetUnit()) {
          text->mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
        }
        else if (eCSSUnit_Inherit == ourText->mTextTransform.GetUnit()) {
          text->mTextTransform = parentText->mTextTransform;
        }

        // vertical-align: enum, length, percent, inherit
        if (! SetCoord(ourText->mVerticalAlign, text->mVerticalAlign, SETCOORD_LP | SETCOORD_ENUMERATED,
                 font, aPresContext)) {
          // XXX this really needs to pass the inherit value along...
          if (eCSSUnit_Inherit == ourText->mVerticalAlign.GetUnit()) {
            text->mVerticalAlign = parentText->mVerticalAlign;
          }
        }

        // white-space: enum, normal, inherit
        if (eCSSUnit_Enumerated == ourText->mWhiteSpace.GetUnit()) {
          text->mWhiteSpace = ourText->mWhiteSpace.GetIntValue();
        }
        else if (eCSSUnit_Normal == ourText->mWhiteSpace.GetUnit()) {
          text->mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
        }
        else if (eCSSUnit_Inherit == ourText->mWhiteSpace.GetUnit()) {
          text->mWhiteSpace = parentText->mWhiteSpace;
        }

        // word-spacing: normal, length, inherit
        SetCoord(ourText->mWordSpacing, text->mWordSpacing, SETCOORD_LH | SETCOORD_NORMAL,
                 font, aPresContext);
      }
    }

    nsCSSDisplay*  ourDisplay;
    if (NS_OK == aDeclaration->GetData(kCSSDisplaySID, (nsCSSStruct**)&ourDisplay)) {
      if (nsnull != ourDisplay) {
        // Get our style and our parent's style
        nsStyleDisplay* display = (nsStyleDisplay*)
          aContext->GetMutableStyleData(eStyleStruct_Display);

        const nsStyleDisplay* parentDisplay = display;
        if (nsnull != parentContext) {
          parentDisplay = (const nsStyleDisplay*)parentContext->GetStyleData(eStyleStruct_Display);
        }

        // display: enum, none, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mDisplay.GetUnit()) {
          display->mDisplay = ourDisplay->mDisplay.GetIntValue();
        }
        else if (eCSSUnit_None == ourDisplay->mDisplay.GetUnit()) {
          display->mDisplay = NS_STYLE_DISPLAY_NONE;
        }
        else if (eCSSUnit_Inherit == ourDisplay->mDisplay.GetUnit()) {
          display->mDisplay = parentDisplay->mDisplay;
        }

        // direction: enum, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mDirection.GetUnit()) {
          display->mDirection = ourDisplay->mDirection.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourDisplay->mDirection.GetUnit()) {
          display->mDirection = parentDisplay->mDirection;
        }

        // clear: enum, none, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mClear.GetUnit()) {
          display->mBreakType = ourDisplay->mClear.GetIntValue();
        }
        else if (eCSSUnit_None == ourDisplay->mClear.GetUnit()) {
          display->mBreakType = NS_STYLE_CLEAR_NONE;
        }
        else if (eCSSUnit_Inherit == ourDisplay->mClear.GetUnit()) {
          display->mBreakType = parentDisplay->mBreakType;
        }

        // float: enum, none, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mFloat.GetUnit()) {
          display->mFloats = ourDisplay->mFloat.GetIntValue();
        }
        else if (eCSSUnit_None == ourDisplay->mFloat.GetUnit()) {
          display->mFloats = NS_STYLE_FLOAT_NONE;
        }
        else if (eCSSUnit_Inherit == ourDisplay->mFloat.GetUnit()) {
          display->mFloats = parentDisplay->mFloats;
        }

        // visibility: enum, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mVisibility.GetUnit()) {
          display->mVisible = ourDisplay->mVisibility.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourDisplay->mVisibility.GetUnit()) {
          display->mVisible = parentDisplay->mVisible;
        }

        // overflow: enum, auto, inherit
        if (eCSSUnit_Enumerated == ourDisplay->mOverflow.GetUnit()) {
          display->mOverflow = ourDisplay->mOverflow.GetIntValue();
        }
        else if (eCSSUnit_Auto == ourDisplay->mOverflow.GetUnit()) {
          display->mOverflow = NS_STYLE_OVERFLOW_AUTO;
        }
        else if (eCSSUnit_Inherit == ourDisplay->mOverflow.GetUnit()) {
          display->mOverflow = parentDisplay->mOverflow;
        }

        // clip property: length, auto, inherit
        if (nsnull != ourDisplay->mClip) {
          if (eCSSUnit_Inherit == ourDisplay->mClip->mTop.GetUnit()) { // if one is inherit, they all are
            display->mClipFlags = NS_STYLE_CLIP_INHERIT;
          }
          else {
            PRBool  fullAuto = PR_TRUE;

            display->mClipFlags = 0; // clear it

            if (eCSSUnit_Auto == ourDisplay->mClip->mTop.GetUnit()) {
              display->mClip.top = 0;
              display->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
            } 
            else if (ourDisplay->mClip->mTop.IsLengthUnit()) {
              display->mClip.top = CalcLength(ourDisplay->mClip->mTop, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (eCSSUnit_Auto == ourDisplay->mClip->mRight.GetUnit()) {
              display->mClip.right = 0;
              display->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
            } 
            else if (ourDisplay->mClip->mRight.IsLengthUnit()) {
              display->mClip.right = CalcLength(ourDisplay->mClip->mRight, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (eCSSUnit_Auto == ourDisplay->mClip->mBottom.GetUnit()) {
              display->mClip.bottom = 0;
              display->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
            } 
            else if (ourDisplay->mClip->mBottom.IsLengthUnit()) {
              display->mClip.bottom = CalcLength(ourDisplay->mClip->mBottom, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (eCSSUnit_Auto == ourDisplay->mClip->mLeft.GetUnit()) {
              display->mClip.left = 0;
              display->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
            } 
            else if (ourDisplay->mClip->mLeft.IsLengthUnit()) {
              display->mClip.left = CalcLength(ourDisplay->mClip->mLeft, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            display->mClipFlags &= ~NS_STYLE_CLIP_TYPE_MASK;
            if (fullAuto) {
              display->mClipFlags |= NS_STYLE_CLIP_AUTO;
            }
            else {
              display->mClipFlags |= NS_STYLE_CLIP_RECT;
            }
          }
        }
      }
    }


    nsCSSColor*  ourColor;
    if (NS_OK == aDeclaration->GetData(kCSSColorSID, (nsCSSStruct**)&ourColor)) {
      if (nsnull != ourColor) {
        nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);

        const nsStyleColor* parentColor = color;
        if (nsnull != parentContext) {
          parentColor = (const nsStyleColor*)parentContext->GetStyleData(eStyleStruct_Color);
        }

        // color: color, string, inherit
        if (! SetColor(ourColor->mColor, parentColor->mColor, color->mColor)) {
        }

        // cursor: enum, auto, url, inherit
        nsCSSValueList*  list = ourColor->mCursor;
        if (nsnull != list) {
          // XXX need to deal with multiple URL values
          if (eCSSUnit_Enumerated == list->mValue.GetUnit()) {
            color->mCursor = list->mValue.GetIntValue();
          }
          else if (eCSSUnit_Auto == list->mValue.GetUnit()) {
            color->mCursor = NS_STYLE_CURSOR_AUTO;
          }
          else if (eCSSUnit_URL == list->mValue.GetUnit()) {
            list->mValue.GetStringValue(color->mCursorImage);
          }
          else if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
            color->mCursor = parentColor->mCursor;
          }
        }

        // background-color: color, string, enum (flags), inherit
        if (eCSSUnit_Inherit == ourColor->mBackColor.GetUnit()) { // do inherit first, so SetColor doesn't do it
          color->mBackgroundColor = parentColor->mBackgroundColor;
          color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
          color->mBackgroundFlags |= (parentColor->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
        }
        else if (SetColor(ourColor->mBackColor, parentColor->mBackgroundColor, color->mBackgroundColor)) {
          color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
        }
        else if (eCSSUnit_Enumerated == ourColor->mBackColor.GetUnit()) {
          color->mBackgroundFlags |= NS_STYLE_BG_COLOR_TRANSPARENT;
        }

        // background-image: url, none, inherit
        if (eCSSUnit_URL == ourColor->mBackImage.GetUnit()) {
          ourColor->mBackImage.GetStringValue(color->mBackgroundImage);
          color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
        }
        else if (eCSSUnit_None == ourColor->mBackImage.GetUnit()) {
          color->mBackgroundImage.Truncate();
          color->mBackgroundFlags |= NS_STYLE_BG_IMAGE_NONE;
        }
        else if (eCSSUnit_Inherit == ourColor->mBackImage.GetUnit()) {
          color->mBackgroundImage = parentColor->mBackgroundImage;
          color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
          color->mBackgroundFlags |= (parentColor->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE);
        }

        // background-repeat: enum, inherit
        if (eCSSUnit_Enumerated == ourColor->mBackRepeat.GetUnit()) {
          color->mBackgroundRepeat = ourColor->mBackRepeat.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourColor->mBackRepeat.GetUnit()) {
          color->mBackgroundRepeat = parentColor->mBackgroundRepeat;
        }

        // background-attachment: enum, inherit
        if (eCSSUnit_Enumerated == ourColor->mBackAttachment.GetUnit()) {
          color->mBackgroundAttachment = ourColor->mBackAttachment.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourColor->mBackAttachment.GetUnit()) {
          color->mBackgroundAttachment = parentColor->mBackgroundAttachment;
        }

        // background-position: enum, length, percent (flags), inherit
        if (eCSSUnit_Percent == ourColor->mBackPositionX.GetUnit()) {
          color->mBackgroundXPosition = (nscoord)(100.0f * ourColor->mBackPositionX.GetPercentValue());
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionX.IsLengthUnit()) {
          color->mBackgroundXPosition = CalcLength(ourColor->mBackPositionX,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PERCENT;
        }
        else if (eCSSUnit_Enumerated == ourColor->mBackPositionX.GetUnit()) {
          color->mBackgroundXPosition = (nscoord)ourColor->mBackPositionX.GetIntValue();
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
        }
        else if (eCSSUnit_Inherit == ourColor->mBackPositionX.GetUnit()) {
          color->mBackgroundXPosition = parentColor->mBackgroundXPosition;
          color->mBackgroundFlags &= ~(NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT);
          color->mBackgroundFlags |= (parentColor->mBackgroundFlags & (NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT));
        }

        if (eCSSUnit_Percent == ourColor->mBackPositionY.GetUnit()) {
          color->mBackgroundYPosition = (nscoord)(100.0f * ourColor->mBackPositionY.GetPercentValue());
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionY.IsLengthUnit()) {
          color->mBackgroundYPosition = CalcLength(ourColor->mBackPositionY,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PERCENT;
        }
        else if (eCSSUnit_Enumerated == ourColor->mBackPositionY.GetUnit()) {
          color->mBackgroundYPosition = (nscoord)ourColor->mBackPositionY.GetIntValue();
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
        }
        else if (eCSSUnit_Inherit == ourColor->mBackPositionY.GetUnit()) {
          color->mBackgroundYPosition = parentColor->mBackgroundYPosition;
          color->mBackgroundFlags &= ~(NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT);
          color->mBackgroundFlags |= (parentColor->mBackgroundFlags & (NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT));
        }

        // opacity: factor, percent, inherit
        if (eCSSUnit_Percent == ourColor->mOpacity.GetUnit()) {
          float opacity = parentColor->mOpacity * ourColor->mOpacity.GetPercentValue();
          if (opacity < 0.0f) {
            color->mOpacity = 0.0f;
          } else if (1.0 < opacity) {
            color->mOpacity = 1.0f;
          }
          else {
            color->mOpacity = opacity;
          }
        }
        else if (eCSSUnit_Number == ourColor->mOpacity.GetUnit()) {
          color->mOpacity = ourColor->mOpacity.GetFloatValue();
        }
        else if (eCSSUnit_Inherit == ourColor->mOpacity.GetUnit()) {
          color->mOpacity = parentColor->mOpacity;
        }

  // XXX: NYI        nsCSSValue mBackFilter;
      }
    }

    nsCSSMargin*  ourMargin;
    if (NS_OK == aDeclaration->GetData(kCSSMarginSID, (nsCSSStruct**)&ourMargin)) {
      if (nsnull != ourMargin) {
        nsStyleSpacing* spacing = (nsStyleSpacing*)
          aContext->GetMutableStyleData(eStyleStruct_Spacing);

        const nsStyleSpacing* parentSpacing = spacing;
        if (nsnull != parentContext) {
          parentSpacing = (const nsStyleSpacing*)parentContext->GetStyleData(eStyleStruct_Spacing);
        }

        // margin: length, percent, auto, inherit
        if (nsnull != ourMargin->mMargin) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mMargin->mLeft, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetLeft(coord);
          }
          if (SetCoord(ourMargin->mMargin->mTop, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetTop(coord);
          }
          if (SetCoord(ourMargin->mMargin->mRight, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetRight(coord);
          }
          if (SetCoord(ourMargin->mMargin->mBottom, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetBottom(coord);
          }
        }

        // padding: length, percent, inherit
        if (nsnull != ourMargin->mPadding) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mPadding->mLeft, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetLeft(coord);
          }
          if (SetCoord(ourMargin->mPadding->mTop, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetTop(coord);
          }
          if (SetCoord(ourMargin->mPadding->mRight, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetRight(coord);
          }
          if (SetCoord(ourMargin->mPadding->mBottom, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetBottom(coord);
          }
        }

        // border-size: length, enum, inherit
        if (nsnull != ourMargin->mBorderWidth) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mBorderWidth->mLeft, coord, SETCOORD_LE, font, aPresContext)) {
            spacing->mBorder.SetLeft(coord);
          }
          else if (eCSSUnit_Inherit == ourMargin->mBorderWidth->mLeft.GetUnit()) {
            spacing->mBorder.SetLeft(parentSpacing->mBorder.GetLeft(coord));
          }

          if (SetCoord(ourMargin->mBorderWidth->mTop, coord, SETCOORD_LE, font, aPresContext)) {
            spacing->mBorder.SetTop(coord);
          }
          else if (eCSSUnit_Inherit == ourMargin->mBorderWidth->mTop.GetUnit()) {
            spacing->mBorder.SetTop(parentSpacing->mBorder.GetTop(coord));
          }

          if (SetCoord(ourMargin->mBorderWidth->mRight, coord, SETCOORD_LE, font, aPresContext)) {
            spacing->mBorder.SetRight(coord);
          }
          else if (eCSSUnit_Inherit == ourMargin->mBorderWidth->mRight.GetUnit()) {
            spacing->mBorder.SetRight(parentSpacing->mBorder.GetRight(coord));
          }

          if (SetCoord(ourMargin->mBorderWidth->mBottom, coord, SETCOORD_LE, font, aPresContext)) {
            spacing->mBorder.SetBottom(coord);
          }
          else if (eCSSUnit_Inherit == ourMargin->mBorderWidth->mBottom.GetUnit()) {
            spacing->mBorder.SetBottom(parentSpacing->mBorder.GetBottom(coord));
          }
        }

        // border-style: enum, none, inhert
        if (nsnull != ourMargin->mBorderStyle) {
          nsCSSRect* ourStyle = ourMargin->mBorderStyle;
          if (eCSSUnit_Enumerated == ourStyle->mTop.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_TOP, ourStyle->mTop.GetIntValue());
          }
          else if (eCSSUnit_None == ourStyle->mTop.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
          }
          else if (eCSSUnit_Inherit == ourStyle->mTop.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_TOP, parentSpacing->GetBorderStyle(NS_SIDE_TOP));
          }

          if (eCSSUnit_Enumerated == ourStyle->mRight.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_RIGHT, ourStyle->mRight.GetIntValue());
          }
          else if (eCSSUnit_None == ourStyle->mRight.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
          }
          else if (eCSSUnit_Inherit == ourStyle->mRight.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_RIGHT, parentSpacing->GetBorderStyle(NS_SIDE_RIGHT));
          }

          if (eCSSUnit_Enumerated == ourStyle->mBottom.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_BOTTOM, ourStyle->mBottom.GetIntValue());
          }
          else if (eCSSUnit_None == ourStyle->mBottom.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
          }
          else if (eCSSUnit_Inherit == ourStyle->mBottom.GetUnit()) { 
            spacing->SetBorderStyle(NS_SIDE_BOTTOM, parentSpacing->GetBorderStyle(NS_SIDE_BOTTOM));
          }

          if (eCSSUnit_Enumerated == ourStyle->mLeft.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_LEFT, ourStyle->mLeft.GetIntValue());
          }
          else if (eCSSUnit_None == ourStyle->mLeft.GetUnit()) {
           spacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
          }
          else if (eCSSUnit_Inherit == ourStyle->mLeft.GetUnit()) {
            spacing->SetBorderStyle(NS_SIDE_LEFT, parentSpacing->GetBorderStyle(NS_SIDE_LEFT));
          }
        }

        // border-color: color, string, enum, inherit
        if (nsnull != ourMargin->mBorderColor) {
          nsCSSRect* ourColor = ourMargin->mBorderColor;
          nscolor borderColor;
          nscolor unused = NS_RGB(0,0,0);

          if (eCSSUnit_Inherit == ourColor->mTop.GetUnit()) {
            if (parentSpacing->GetBorderColor(NS_SIDE_TOP, borderColor)) {
              spacing->SetBorderColor(NS_SIDE_TOP, borderColor);
            }
            else {
              spacing->SetBorderTransparent(NS_SIDE_TOP);
            }
          }
          else if (SetColor(ourColor->mTop, unused, borderColor)) {
            spacing->SetBorderColor(NS_SIDE_TOP, borderColor);
          }
          else if (eCSSUnit_Enumerated == ourColor->mTop.GetUnit()) {
            spacing->SetBorderTransparent(NS_SIDE_TOP);
          }

          if (eCSSUnit_Inherit == ourColor->mRight.GetUnit()) {
            if (parentSpacing->GetBorderColor(NS_SIDE_RIGHT, borderColor)) {
              spacing->SetBorderColor(NS_SIDE_RIGHT, borderColor);
            }
            else {
              spacing->SetBorderTransparent(NS_SIDE_RIGHT);
            }
          }
          else if (SetColor(ourColor->mRight, unused, borderColor)) {
            spacing->SetBorderColor(NS_SIDE_RIGHT, borderColor);
          }
          else if (eCSSUnit_Enumerated == ourColor->mRight.GetUnit()) {
            spacing->SetBorderTransparent(NS_SIDE_RIGHT);
          }

          if (eCSSUnit_Inherit == ourColor->mBottom.GetUnit()) {
            if (parentSpacing->GetBorderColor(NS_SIDE_BOTTOM, borderColor)) {
              spacing->SetBorderColor(NS_SIDE_BOTTOM, borderColor);
            }
            else {
              spacing->SetBorderTransparent(NS_SIDE_BOTTOM);
            }
          }
          else if (SetColor(ourColor->mBottom, unused, borderColor)) {
            spacing->SetBorderColor(NS_SIDE_BOTTOM, borderColor);
          }
          else if (eCSSUnit_Enumerated == ourColor->mBottom.GetUnit()) {
            spacing->SetBorderTransparent(NS_SIDE_BOTTOM);
          }

          if (eCSSUnit_Inherit == ourColor->mLeft.GetUnit()) {
            if (parentSpacing->GetBorderColor(NS_SIDE_LEFT, borderColor)) {
              spacing->SetBorderColor(NS_SIDE_LEFT, borderColor);
            }
            else {
              spacing->SetBorderTransparent(NS_SIDE_LEFT);
            }
          }
          else if (SetColor(ourColor->mLeft, unused, borderColor)) {
            spacing->SetBorderColor(NS_SIDE_LEFT, borderColor);
          }
          else if (eCSSUnit_Enumerated == ourColor->mLeft.GetUnit()) {
            spacing->SetBorderTransparent(NS_SIDE_LEFT);
          }
        }

        // outline-width: length, enum, inherit
        if (! SetCoord(ourMargin->mOutlineWidth, spacing->mOutlineWidth, 
                       SETCOORD_LE, font, aPresContext)) {
          if (eCSSUnit_Inherit == ourMargin->mOutlineWidth.GetUnit()) {
            spacing->mOutlineWidth = parentSpacing->mOutlineWidth;
          }
        }

        // outline-color: color, string, enum, inherit
        nscolor outlineColor;
        nscolor unused = NS_RGB(0,0,0);
        if (eCSSUnit_Inherit == ourMargin->mOutlineColor.GetUnit()) {
          if (parentSpacing->GetOutlineColor(outlineColor)) {
            spacing->SetOutlineColor(outlineColor);
          }
          else {
            spacing->SetOutlineInvert();
          }
        }
        else if (SetColor(ourMargin->mOutlineColor, unused, outlineColor)) {
          spacing->SetOutlineColor(outlineColor);
        }
        else if (eCSSUnit_Enumerated == ourMargin->mOutlineColor.GetUnit()) {
          spacing->SetOutlineInvert();
        }

        // outline-style: enum, none, inherit
        if (eCSSUnit_Enumerated == ourMargin->mOutlineStyle.GetUnit()) {
          spacing->SetOutlineStyle(ourMargin->mOutlineStyle.GetIntValue());
        }
        else if (eCSSUnit_None == ourMargin->mOutlineStyle.GetUnit()) {
          spacing->SetOutlineStyle(NS_STYLE_BORDER_STYLE_NONE);
        }
        else if (eCSSUnit_Inherit == ourMargin->mOutlineStyle.GetUnit()) {
          spacing->SetOutlineStyle(parentSpacing->GetOutlineStyle());
        }
      }
    }

    nsCSSPosition*  ourPosition;
    if (NS_OK == aDeclaration->GetData(kCSSPositionSID, (nsCSSStruct**)&ourPosition)) {
      if (nsnull != ourPosition) {
        nsStylePosition* position = (nsStylePosition*)aContext->GetMutableStyleData(eStyleStruct_Position);

        const nsStylePosition* parentPosition = position;
        if (nsnull != parentContext) {
          parentPosition = (const nsStylePosition*)parentContext->GetStyleData(eStyleStruct_Position);
        }

        // position: enum, inherit
        if (eCSSUnit_Enumerated == ourPosition->mPosition.GetUnit()) {
          position->mPosition = ourPosition->mPosition.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourPosition->mPosition.GetUnit()) {
          position->mPosition = parentPosition->mPosition;
        }

        // box offsets: length, percent, auto, inherit
        if (nsnull != ourPosition->mOffset) {
          nsStyleCoord  coord;
          if (SetCoord(ourPosition->mOffset->mTop, coord, SETCOORD_LPAH, font, aPresContext)) {
            position->mOffset.SetTop(coord);            
          }
          if (SetCoord(ourPosition->mOffset->mRight, coord, SETCOORD_LPAH, font, aPresContext)) {
            position->mOffset.SetRight(coord);            
          }
          if (SetCoord(ourPosition->mOffset->mBottom, coord, SETCOORD_LPAH, font, aPresContext)) {
            position->mOffset.SetBottom(coord);
          }
          if (SetCoord(ourPosition->mOffset->mLeft, coord, SETCOORD_LPAH, font, aPresContext)) {
            position->mOffset.SetLeft(coord);
          }
        }

        SetCoord(ourPosition->mWidth, position->mWidth, SETCOORD_LPAH, font, aPresContext);
        SetCoord(ourPosition->mMinWidth, position->mMinWidth, SETCOORD_LPH, font, aPresContext);
        if (! SetCoord(ourPosition->mMaxWidth, position->mMaxWidth, SETCOORD_LPH, font, aPresContext)) {
          if (eCSSUnit_None == ourPosition->mMaxWidth.GetUnit()) {
            position->mMaxWidth.Reset();
          }
        }

        SetCoord(ourPosition->mHeight, position->mHeight, SETCOORD_LPAH, font, aPresContext);
        SetCoord(ourPosition->mMinHeight, position->mMinHeight, SETCOORD_LPH, font, aPresContext);
        if (! SetCoord(ourPosition->mMaxHeight, position->mMaxHeight, SETCOORD_LPH, font, aPresContext)) {
          if (eCSSUnit_None == ourPosition->mMaxHeight.GetUnit()) {
            position->mMaxHeight.Reset();
          }
        }

        // z-index
        SetCoord(ourPosition->mZIndex, position->mZIndex, SETCOORD_IAH, nsnull, nsnull);
      }
    }

    nsCSSList* ourList;
    if (NS_OK == aDeclaration->GetData(kCSSListSID, (nsCSSStruct**)&ourList)) {
      if (nsnull != ourList) {
        nsStyleList* list = (nsStyleList*)aContext->GetMutableStyleData(eStyleStruct_List);

        const nsStyleList* parentList = list;
        if (nsnull != parentContext) {
          parentList = (const nsStyleList*)parentContext->GetStyleData(eStyleStruct_List);
        }

        // list-style-type: enum, none, inherit
        if (eCSSUnit_Enumerated == ourList->mType.GetUnit()) {
          list->mListStyleType = ourList->mType.GetIntValue();
        }
        else if (eCSSUnit_None == ourList->mType.GetUnit()) {
          list->mListStyleType = NS_STYLE_LIST_STYLE_NONE;
        }
        else if (eCSSUnit_Inherit == ourList->mType.GetUnit()) {
          list->mListStyleType = parentList->mListStyleType;
        }

        // list-style-image: url, none, inherit
        if (eCSSUnit_URL == ourList->mImage.GetUnit()) {
          ourList->mImage.GetStringValue(list->mListStyleImage);
        }
        else if (eCSSUnit_None == ourList->mImage.GetUnit()) {
          list->mListStyleImage.Truncate();
        }
        else if (eCSSUnit_Inherit == ourList->mImage.GetUnit()) {
          list->mListStyleImage = parentList->mListStyleImage;
        }

        // list-style-position: enum, inherit
        if (eCSSUnit_Enumerated == ourList->mPosition.GetUnit()) {
          list->mListStylePosition = ourList->mPosition.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourList->mPosition.GetUnit()) {
          list->mListStylePosition = parentList->mListStylePosition;
        }
      }
    }

    nsCSSTable* ourTable;
    if (NS_OK == aDeclaration->GetData(kCSSTableSID, (nsCSSStruct**)&ourTable)) {
      if (nsnull != ourTable) {
        nsStyleTable* table = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);

        const nsStyleTable* parentTable = table;
        if (nsnull != parentContext) {
          parentTable = (const nsStyleTable*)parentContext->GetStyleData(eStyleStruct_Table);
        }
        nsStyleCoord  coord;

        // border-collapse: enum, inherit
        if (eCSSUnit_Enumerated == ourTable->mBorderCollapse.GetUnit()) {
          table->mBorderCollapse = ourTable->mBorderCollapse.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourTable->mBorderCollapse.GetUnit()) {
          table->mBorderCollapse = parentTable->mBorderCollapse;
        }

        // border-spacing-x: length, inherit
        if (SetCoord(ourTable->mBorderSpacingX, coord, SETCOORD_LENGTH, font, aPresContext)) {
          table->mBorderSpacingX = coord.GetCoordValue();
        }
        else if (eCSSUnit_Inherit == ourTable->mBorderSpacingX.GetUnit()) {
          table->mBorderSpacingX = parentTable->mBorderSpacingX;
        }
        // border-spacing-y: length, inherit
        if (SetCoord(ourTable->mBorderSpacingY, coord, SETCOORD_LENGTH, font, aPresContext)) {
          table->mBorderSpacingY = coord.GetCoordValue();
        }
        else if (eCSSUnit_Inherit == ourTable->mBorderSpacingY.GetUnit()) {
          table->mBorderSpacingY = parentTable->mBorderSpacingY;
        }

        // caption-side: enum, inherit
        if (eCSSUnit_Enumerated == ourTable->mCaptionSide.GetUnit()) {
          table->mCaptionSide = ourTable->mCaptionSide.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourTable->mCaptionSide.GetUnit()) {
          table->mCaptionSide = parentTable->mCaptionSide;
        }

        // empty-cells: enum, inherit
        if (eCSSUnit_Enumerated == ourTable->mEmptyCells.GetUnit()) {
          table->mEmptyCells = ourTable->mEmptyCells.GetIntValue();
        }
        else if (eCSSUnit_Inherit == ourTable->mEmptyCells.GetUnit()) {
          table->mEmptyCells = parentTable->mEmptyCells;
        }

        // table-layout: auto, enum, inherit
        if (eCSSUnit_Enumerated == ourTable->mLayout.GetUnit()) {
          table->mLayoutStrategy = ourTable->mLayout.GetIntValue();
        }
        else if (eCSSUnit_Auto == ourTable->mLayout.GetUnit()) {
          table->mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
        }
        else if (eCSSUnit_Inherit == ourTable->mLayout.GetUnit()) {
          table->mLayoutStrategy = parentTable->mLayoutStrategy;
        }
      }
    }

    nsCSSContent* ourContent;
    if (NS_OK == aDeclaration->GetData(kCSSContentSID, (nsCSSStruct**)&ourContent)) {
      if (ourContent) {
        nsStyleContent* content = (nsStyleContent*)aContext->GetMutableStyleData(eStyleStruct_Content);

        const nsStyleContent* parentContent = content;
        if (nsnull != parentContext) {
          parentContent = (const nsStyleContent*)parentContext->GetStyleData(eStyleStruct_Content);
        }

        PRUint32 count;
        nsAutoString  buffer;

        // content: [string, url, counter, attr, enum]+, inherit
        nsCSSValueList* contentValue = ourContent->mContent;
        if (contentValue) {
          if (eCSSUnit_Inherit == contentValue->mValue.GetUnit()) {
            count = parentContent->ContentCount();
            if (NS_SUCCEEDED(content->AllocateContents(count))) {
              nsStyleContentType type;
              while (0 < count--) {
                parentContent->GetContentAt(count, type, buffer);
                content->SetContentAt(count, type, buffer);
              }
            }
          }
          else {
            count = 0;
            while (contentValue) {
              count++;
              contentValue = contentValue->mNext;
            }
            if (NS_SUCCEEDED(content->AllocateContents(count))) {
              const nsAutoString  nullStr;
              count = 0;
              contentValue = ourContent->mContent;
              while (contentValue) {
                const nsCSSValue& value = contentValue->mValue;
                nsCSSUnit unit = value.GetUnit();
                nsStyleContentType type;
                switch (unit) {
                  case eCSSUnit_String:   type = eStyleContentType_String;    break;
                  case eCSSUnit_URL:      type = eStyleContentType_URL;       break;
                  case eCSSUnit_Attr:     type = eStyleContentType_Attr;      break;
                  case eCSSUnit_Counter:  type = eStyleContentType_Counter;   break;
                  case eCSSUnit_Counters: type = eStyleContentType_Counters;  break;
                  case eCSSUnit_Enumerated:
                    switch (value.GetIntValue()) {
                      case NS_STYLE_CONTENT_OPEN_QUOTE:     
                        type = eStyleContentType_OpenQuote;     break;
                      case NS_STYLE_CONTENT_CLOSE_QUOTE:
                        type = eStyleContentType_CloseQuote;    break;
                      case NS_STYLE_CONTENT_NO_OPEN_QUOTE:
                        type = eStyleContentType_NoOpenQuote;   break;
                      case NS_STYLE_CONTENT_NO_CLOSE_QUOTE:
                        type = eStyleContentType_NoCloseQuote;  break;
                      default:
                        NS_ERROR("bad content value");
                    }
                    break;
                  default:
                    NS_ERROR("bad content type");
                }
                if (type < eStyleContentType_OpenQuote) {
                  value.GetStringValue(buffer);
                  Unquote(buffer);
                  content->SetContentAt(count++, type, buffer);
                }
                else {
                  content->SetContentAt(count++, type, nullStr);
                }
                contentValue = contentValue->mNext;
              }
            } 
          }
        }

        // counter-increment: [string [int]]+, none, inherit
        nsCSSCounterData* ourIncrement = ourContent->mCounterIncrement;
        if (ourIncrement) {
          PRInt32 increment;
          if (eCSSUnit_Inherit == ourIncrement->mCounter.GetUnit()) {
            count = parentContent->CounterIncrementCount();
            if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
              while (0 < count--) {
                parentContent->GetCounterIncrementAt(count, buffer, increment);
                content->SetCounterIncrementAt(count, buffer, increment);
              }
            }
          }
          else if (eCSSUnit_None == ourIncrement->mCounter.GetUnit()) {
            content->AllocateCounterIncrements(0);
          }
          else if (eCSSUnit_String == ourIncrement->mCounter.GetUnit()) {
            count = 0;
            while (ourIncrement) {
              count++;
              ourIncrement = ourIncrement->mNext;
            }
            if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
              count = 0;
              ourIncrement = ourContent->mCounterIncrement;
              while (ourIncrement) {
                if (eCSSUnit_Integer == ourIncrement->mValue.GetUnit()) {
                  increment = ourIncrement->mValue.GetIntValue();
                }
                else {
                  increment = 1;
                }
                ourIncrement->mCounter.GetStringValue(buffer);
                content->SetCounterIncrementAt(count++, buffer, increment);
                ourIncrement = ourIncrement->mNext;
              }
            }
          }
        }

        // counter-reset: [string [int]]+, none, inherit
        nsCSSCounterData* ourReset = ourContent->mCounterReset;
        if (ourReset) {
          PRInt32 reset;
          if (eCSSUnit_Inherit == ourReset->mCounter.GetUnit()) {
            count = parentContent->CounterResetCount();
            if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
              while (0 < count--) {
                parentContent->GetCounterResetAt(count, buffer, reset);
                content->SetCounterResetAt(count, buffer, reset);
              }
            }
          }
          else if (eCSSUnit_None == ourReset->mCounter.GetUnit()) {
            content->AllocateCounterResets(0);
          }
          else if (eCSSUnit_String == ourReset->mCounter.GetUnit()) {
            count = 0;
            while (ourReset) {
              count++;
              ourReset = ourReset->mNext;
            }
            if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
              count = 0;
              ourReset = ourContent->mCounterReset;
              while (ourReset) {
                if (eCSSUnit_Integer == ourReset->mValue.GetUnit()) {
                  reset = ourReset->mValue.GetIntValue();
                }
                else {
                  reset = 0;
                }
                ourReset->mCounter.GetStringValue(buffer);
                content->SetCounterResetAt(count++, buffer, reset);
                ourReset = ourReset->mNext;
              }
            }
          }
        }

        // marker-offset: length, auto, inherit
        if (! SetCoord(ourContent->mMarkerOffset, content->mMarkerOffset,
                       SETCOORD_LENGTH | SETCOORD_AUTO, font, aPresContext)) {
          if (eCSSUnit_Inherit == ourContent->mMarkerOffset.GetUnit()) {
            content->mMarkerOffset = parentContent->mMarkerOffset;
          }
        }

        // quotes: [string string]+, none, inherit
        nsCSSQuotes* ourQuotes = ourContent->mQuotes;
        if (ourQuotes) {
          nsAutoString  closeBuffer;
          if (eCSSUnit_Inherit == ourQuotes->mOpen.GetUnit()) {
            count = parentContent->QuotesCount();
            if (NS_SUCCEEDED(content->AllocateQuotes(count))) {
              while (0 < count--) {
                parentContent->GetQuotesAt(count, buffer, closeBuffer);
                content->SetQuotesAt(count, buffer, closeBuffer);
              }
            }
          }
          else if (eCSSUnit_None == ourQuotes->mOpen.GetUnit()) {
            content->AllocateQuotes(0);
          }
          else if (eCSSUnit_String == ourQuotes->mOpen.GetUnit()) {
            count = 0;
            while (ourQuotes) {
              count++;
              ourQuotes = ourQuotes->mNext;
            }
            if (NS_SUCCEEDED(content->AllocateQuotes(count))) {
              count = 0;
              ourQuotes = ourContent->mQuotes;
              while (ourQuotes) {
                ourQuotes->mOpen.GetStringValue(buffer);
                ourQuotes->mClose.GetStringValue(closeBuffer);
                Unquote(buffer);
                Unquote(closeBuffer);
                content->SetQuotesAt(count++, buffer, closeBuffer);
                ourQuotes = ourQuotes->mNext;
              }
            }
          }
        }
      }
    }

    NS_IF_RELEASE(parentContext);
  }
}



static void ListSelector(FILE* out, const nsCSSSelector* aSelector)
{
  nsAutoString buffer;

  if (0 != aSelector->mOperator) {
    buffer.Truncate();
    buffer.Append(aSelector->mOperator);
    buffer.Append(" ");
    fputs(buffer, out);
  }
  if (kNameSpaceID_None < aSelector->mNameSpace) {
    buffer.Append(aSelector->mNameSpace, 10);
    fputs(buffer, out);
    fputs("\\:", out);
  }
  if (nsnull != aSelector->mTag) {
    aSelector->mTag->ToString(buffer);
    fputs(buffer, out);
  }
  if (nsnull != aSelector->mID) {
    aSelector->mID->ToString(buffer);
    fputs("#", out);
    fputs(buffer, out);
  }
  nsAtomList* list = aSelector->mClassList;
  while (nsnull != list) {
    list->mAtom->ToString(buffer);
    fputs(".", out);
    fputs(buffer, out);
    list = list->mNext;
  }
  list = aSelector->mPseudoClassList;
  while (nsnull != list) {
    list->mAtom->ToString(buffer);
    fputs(buffer, out);
    list = list->mNext;
  }
  nsAttrSelector* attr = aSelector->mAttrList;
  while (nsnull != attr) {
    fputs("[", out);
    attr->mAttr->ToString(buffer);
    fputs(buffer, out);
    if (NS_ATTR_FUNC_SET != attr->mFunction) {
      switch (attr->mFunction) {
        case NS_ATTR_FUNC_EQUALS:    fputs("=", out);  break;
        case NS_ATTR_FUNC_INCLUDES:  fputs("~=", out);  break;
        case NS_ATTR_FUNC_DASHMATCH: fputs("|=", out);  break;
      }
      fputs(attr->mValue, out);
    }
    fputs("]", out);
    attr = attr->mNext;
  }
}

NS_IMETHODIMP
CSSStyleRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  const nsCSSSelector*  selector = &mSelector;

  while (nsnull != selector) {
    ListSelector(out, selector);
    fputs(" ", out);
    selector = selector->mNext;
  }

  nsAutoString buffer;

  buffer.Append("weight: ");
  buffer.Append(mWeight, 10);
  buffer.Append(" ");
  fputs(buffer, out);
  if (nsnull != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);

  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetCssText(nsString& aCssText)
{
  aCssText = mSelectorText;
  // XXX TBI append declaration too
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::SetCssText(const nsString& aCssText)
{
  // XXX TBI - need to re-parse rule & declaration
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (nsnull != mSheet) {
    return mSheet->QueryInterface(kIDOMCSSStyleSheetIID, (void**)aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetSelectorText(nsString& aSelectorText)
{
  aSelectorText = mSelectorText;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::SetSelectorText(const nsString& aSelectorText)
{
  // XXX TBI - get a parser and re-parse the selectors, 
  // XXX then need to re-compute the cascade
  mSelectorText = aSelectorText;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  if (nsnull == mDOMDeclaration) {
    mDOMDeclaration = new DOMCSSDeclarationImpl(this);
    if (nsnull == mDOMDeclaration) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mDOMDeclaration);
  }
  
  *aStyle = mDOMDeclaration;
  NS_ADDREF(mDOMDeclaration);
  
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetStyle(nsIDOMCSSStyleDeclaration* aStyle)
{
  // XXX TBI
  return NS_OK;
}

NS_IMETHODIMP 
CSSStyleRuleImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsICSSStyleRule *)this;
    // XXX Parent should be the style sheet
    // XXX Should be done through factory
    res = NS_NewScriptCSSStyleRule(aContext, 
                                   supports,
                                   (nsISupports *)global, 
                                   (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP 
CSSStyleRuleImpl::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_HTML nsresult
  NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult, const nsCSSSelector& aSelector)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSStyleRuleImpl  *it = new CSSStyleRuleImpl(aSelector);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSStyleRuleIID, (void **) aInstancePtrResult);
}
