/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   David Hyatt <hyatt@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
 */
#include "nsCOMPtr.h"
#include "nsCSSRule.h"
#include "nsICSSStyleRule.h"
#include "nsICSSGroupRule.h"
#include "nsICSSDeclaration.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsIHTMLContentContainer.h"
#include "nsIURL.h"
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
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsILookAndFeel.h"
#include "xp_core.h"
#include "nsIRuleNode.h"

#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"

#include "nsContentUtils.h"

// MJA: bug 31816
#include "nsIPresShell.h"
#include "nsIDocShellTreeItem.h"
// - END MJA

// #define DEBUG_REFS

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);
static NS_DEFINE_IID(kCSSTableSID, NS_CSS_TABLE_SID);
static NS_DEFINE_IID(kCSSContentSID, NS_CSS_CONTENT_SID);
static NS_DEFINE_IID(kCSSUserInterfaceSID, NS_CSS_USER_INTERFACE_SID);
static NS_DEFINE_IID(kCSSBreaksSID, NS_CSS_BREAKS_SID);
static NS_DEFINE_IID(kCSSPageSID, NS_CSS_PAGE_SID);
#ifdef INCLUDE_XUL
static NS_DEFINE_IID(kCSSXULSID, NS_CSS_XUL_SID);
#endif

// -- nsCSSSelector -------------------------------

#define NS_IF_COPY(dest,source,type)  \
  if (nsnull != source)  dest = new type(*(source))

#define NS_IF_DELETE(ptr)   \
  if (nsnull != ptr) { delete ptr; ptr = nsnull; }

#define NS_IF_NEGATED_START(bool,str)  \
  if (bool) { str.Append(NS_LITERAL_STRING(":not(")); }

#define NS_IF_NEGATED_END(bool,str)  \
  if (bool) { str.Append(PRUnichar(')')); }

MOZ_DECL_CTOR_COUNTER(nsAtomList)

nsAtomList::nsAtomList(nsIAtom* aAtom)
  : mAtom(aAtom),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
  NS_IF_ADDREF(mAtom);
}

nsAtomList::nsAtomList(const nsString& aAtomValue)
  : mAtom(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
  mAtom = NS_NewAtom(aAtomValue);
}

nsAtomList::nsAtomList(const nsAtomList& aCopy)
  : mAtom(aCopy.mAtom),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAtomList);
  NS_IF_ADDREF(mAtom);
  NS_IF_COPY(mNext, aCopy.mNext, nsAtomList);
}

nsAtomList::~nsAtomList(void)
{
  MOZ_COUNT_DTOR(nsAtomList);
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

MOZ_DECL_CTOR_COUNTER(nsAttrSelector)

#ifdef DEBUG_REFS
PRUint32 gAttrSelectorCount=0;
#endif

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr)
  : mNameSpace(aNameSpace),
    mAttr(nsnull),
    mFunction(NS_ATTR_FUNC_SET),
    mCaseSensitive(1),
    mValue(),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

#ifdef DEBUG_REFS
  gAttrSelectorCount++;
  printf( "nsAttrSelector Instances (ctor): %ld\n", (long)gAttrSelectorCount);
#endif

  mAttr = NS_NewAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunction, 
                               const nsString& aValue, PRBool aCaseSensitive)
  : mNameSpace(aNameSpace),
    mAttr(nsnull),
    mFunction(aFunction),
    mCaseSensitive(aCaseSensitive),
    mValue(aValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

#ifdef DEBUG_REFS
  gAttrSelectorCount++;
  printf( "nsAttrSelector Instances (ctor): %ld\n", (long)gAttrSelectorCount);
#endif

  mAttr = NS_NewAtom(aAttr);
}

nsAttrSelector::nsAttrSelector(const nsAttrSelector& aCopy)
  : mNameSpace(aCopy.mNameSpace),
    mAttr(aCopy.mAttr),
    mFunction(aCopy.mFunction),
    mCaseSensitive(aCopy.mCaseSensitive),
    mValue(aCopy.mValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsAttrSelector);

#ifdef DEBUG_REFS
  gAttrSelectorCount++;
  printf( "nsAttrSelector Instances (cp-ctor): %ld\n", (long)gAttrSelectorCount);
#endif

  NS_IF_ADDREF(mAttr);
  NS_IF_COPY(mNext, aCopy.mNext, nsAttrSelector);
}

nsAttrSelector::~nsAttrSelector(void)
{
  MOZ_COUNT_DTOR(nsAttrSelector);

#ifdef DEBUG_REFS
  gAttrSelectorCount--;
  printf( "nsAttrSelector Instances (dtor): %ld\n", (long)gAttrSelectorCount);
#endif

  NS_IF_RELEASE(mAttr);
  NS_IF_DELETE(mNext);
}

PRBool nsAttrSelector::Equals(const nsAttrSelector* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }
  if (nsnull != aOther) {
    if ((mNameSpace == aOther->mNameSpace) &&
        (mAttr == aOther->mAttr) && 
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

/******************************************************************************
* SizeOf method:
*
*  Self (reported as nsAttrSelector's size): 
*    1) sizeof(*this) + the size of mAttr atom (if it exists and is unique)
*
*  Contained / Aggregated data (not reported as nsAttrSelector's size):
*    none
*
*  Children / siblings / parents:
*    1) Recurses to the mMext instance which is reported as a seperate instance
*    
******************************************************************************/
void nsAttrSelector::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("nsAttrSelector"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);

  // add in the mAttr atom
  if (mAttr && uniqueItems->AddItem(mAttr)){
    mAttr->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }
  aSizeOfHandler->AddSize(tag,aSize);

  // recurse to the next one...
  if(mNext){
    mNext->SizeOf(aSizeOfHandler, localSize);
  }
}

MOZ_DECL_CTOR_COUNTER(nsCSSSelector)

#ifdef DEBUG_REFS
PRUint32 gSelectorCount=0;
#endif

nsCSSSelector::nsCSSSelector(void)
  : mNameSpace(kNameSpaceID_Unknown), mTag(nsnull), 
    mIDList(nsnull), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(0),
    mNegations(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelector);

#ifdef DEBUG_REFS
  gSelectorCount++;
  printf( "nsCSSSelector Instances (ctor): %ld\n", (long)gSelectorCount);
#endif
}

nsCSSSelector::nsCSSSelector(const nsCSSSelector& aCopy) 
  : mNameSpace(aCopy.mNameSpace), mTag(aCopy.mTag), 
    mIDList(nsnull), 
    mClassList(nsnull), 
    mPseudoClassList(nsnull),
    mAttrList(nsnull), 
    mOperator(aCopy.mOperator),
    mNegations(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSelector);
  NS_IF_ADDREF(mTag);
  NS_IF_COPY(mIDList, aCopy.mIDList, nsAtomList);
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
  NS_IF_COPY(mNegations, aCopy.mNegations, nsCSSSelector);
  
#ifdef DEBUG_REFS
  gSelectorCount++;
  printf( "nsCSSSelector Instances (cp-ctor): %ld\n", (long)gSelectorCount);
#endif
}

nsCSSSelector::~nsCSSSelector(void)  
{
  MOZ_COUNT_DTOR(nsCSSSelector);
  Reset();

#ifdef DEBUG_REFS
  gSelectorCount--;
  printf( "nsCSSSelector Instances (dtor): %ld\n", (long)gSelectorCount);
#endif
}

nsCSSSelector& nsCSSSelector::operator=(const nsCSSSelector& aCopy)
{
  NS_IF_RELEASE(mTag);
  NS_IF_DELETE(mIDList);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  NS_IF_DELETE(mNegations);
  
  mNameSpace    = aCopy.mNameSpace;
  mTag          = aCopy.mTag;
  NS_IF_COPY(mIDList, aCopy.mIDList, nsAtomList);
  NS_IF_COPY(mClassList, aCopy.mClassList, nsAtomList);
  NS_IF_COPY(mPseudoClassList, aCopy.mPseudoClassList, nsAtomList);
  NS_IF_COPY(mAttrList, aCopy.mAttrList, nsAttrSelector);
  mOperator     = aCopy.mOperator;
  NS_IF_COPY(mNegations, aCopy.mNegations, nsCSSSelector);

  NS_IF_ADDREF(mTag);
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
        (aOther->mOperator == mOperator)) {
      if (nsnull != mIDList) {
        if (PR_FALSE == mIDList->Equals(aOther->mIDList)) {
          return PR_FALSE;
        }
      }
      else {
        if (nsnull != aOther->mIDList) {
          return PR_FALSE;
        }
      }
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
      if (nsnull != mNegations) {
        if (PR_FALSE == mNegations->Equals(aOther->mNegations)) {
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
  NS_IF_DELETE(mIDList);
  NS_IF_DELETE(mClassList);
  NS_IF_DELETE(mPseudoClassList);
  NS_IF_DELETE(mAttrList);
  NS_IF_DELETE(mNegations);
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

void nsCSSSelector::AddID(const nsString& aID)
{
  if (0 < aID.Length()) {
    nsAtomList** list = &mIDList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAtomList(aID);
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

void nsCSSSelector::AddAttribute(PRInt32 aNameSpace, const nsString& aAttr)
{
  if (0 < aAttr.Length()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr);
  }
}

void nsCSSSelector::AddAttribute(PRInt32 aNameSpace, const nsString& aAttr, PRUint8 aFunc, 
                                 const nsString& aValue, PRBool aCaseSensitive)
{
  if (0 < aAttr.Length()) {
    nsAttrSelector** list = &mAttrList;
    while (nsnull != *list) {
      list = &((*list)->mNext);
    }
    *list = new nsAttrSelector(aNameSpace, aAttr, aFunc, aValue, aCaseSensitive);
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
  nsAtomList* list = mIDList;
  while (nsnull != list) {
    weight += 0x010000;
    list = list->mNext;
  }
  list = mClassList;
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
  if (nsnull != mNegations) {
    weight += mNegations->CalcWeight();
  }
  return weight;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as nsCSSSelector's size): 
*    1) sizeof(*this) + the size of the mTag 
*       + the size of the mIDList unique items 
*       + the size of the mClassList and mPseudoClassList unique items
*
*  Contained / Aggregated data (not reported as nsCSSSelector's size):
*    1) AttributeList is called out to seperately if it exists
*
*  Children / siblings / parents:
*    1) Recurses to mNext which is counted as it's own instance
*    
******************************************************************************/
void nsCSSSelector::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("nsCSSSelector"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  
  // now get the member-atoms and add them in
  if(mTag && uniqueItems->AddItem(mTag)){
    localSize = 0;
    mTag->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }


  // XXX ????



  // a couple of simple atom lists
  if(mIDList && uniqueItems->AddItem(mIDList)){
    aSize += sizeof(*mIDList);
    nsAtomList *pNext = nsnull;
    pNext = mIDList;    
    while(pNext){
      if(pNext->mAtom && uniqueItems->AddItem(pNext->mAtom)){
        localSize = 0;
        pNext->mAtom->SizeOf(aSizeOfHandler, &localSize);
        aSize += localSize;
      }
      pNext = pNext->mNext;
    }
  }
  if(mClassList && uniqueItems->AddItem(mClassList)){
    aSize += sizeof(*mClassList);
    nsAtomList *pNext = nsnull;
    pNext = mClassList;    
    while(pNext){
      if(pNext->mAtom && uniqueItems->AddItem(pNext->mAtom)){
        localSize = 0;
        pNext->mAtom->SizeOf(aSizeOfHandler, &localSize);
        aSize += localSize;
      }
      pNext = pNext->mNext;
    }
  }
  if(mPseudoClassList && uniqueItems->AddItem(mPseudoClassList)){
    nsAtomList *pNext = nsnull;
    pNext = mPseudoClassList;    
    while(pNext){
      if(pNext->mAtom && uniqueItems->AddItem(pNext->mAtom)){
        localSize = 0;
        pNext->mAtom->SizeOf(aSizeOfHandler, &localSize);
        aSize += localSize;
      }
      pNext = pNext->mNext;
    }
  }
  // done with undelegated sizes 
  aSizeOfHandler->AddSize(tag, aSize);

  // the AttributeList gets its own delegation-call
  if(mAttrList){
    localSize = 0;
    mAttrList->SizeOf(aSizeOfHandler, localSize);
  }

  // don't forget the negated selectors
  if(mNegations) {
    localSize = 0;
    mNegations->SizeOf(aSizeOfHandler, localSize);
  }
  
  // finally chain to the next...
  if(mNext){
    localSize = 0;
    mNext->SizeOf(aSizeOfHandler, localSize);
  }
}

// pseudo-elements are stored in the selectors' chain using fictional elements;
// these fictional elements have mTag starting with a colon
static PRBool IsPseudoElement(nsIAtom* aAtom)
{
  if (aAtom) {
    const PRUnichar *str;
    aAtom->GetUnicode(&str);
    return str && (*str == ':');
  }

  return PR_FALSE;
}

void nsCSSSelector::AppendNegationToString(nsAWritableString& aString)
{
  aString.Append(NS_LITERAL_STRING(":not("));
}

//
// Builds the textual representation of a selector. Called by DOM 2 CSS 
// StyleRule:selectorText
//
nsresult nsCSSSelector::ToString( nsAWritableString& aString, nsICSSStyleSheet* aSheet, PRBool aIsPseudoElem,
                                  PRInt8 aNegatedIndex) const
{
  const PRUnichar* temp;
  PRBool aIsNegated = PRBool(0 < aNegatedIndex);

  // selectors are linked from right-to-left, so the next selector in the linked list
  // actually precedes this one in the resulting string
  if (mNext) {
    mNext->ToString(aString, aSheet, IsPseudoElement(mTag), PR_FALSE);
    if (!aIsNegated && !IsPseudoElement(mTag)) {
      // don't add a leading whitespace if we have a pseudo-element
      // or a negated simple selector
      aString.Append(PRUnichar(' '));
    }
  }
  if (1 < aNegatedIndex) {
    // the first mNegations does not contain a negated type element selector
    // or a negated universal selector
    NS_IF_NEGATED_START(aIsNegated, aString)
  }

  // append the namespace prefix
  if (mNameSpace > 0) {
    nsCOMPtr<nsINameSpace> sheetNS;
    aSheet->GetNameSpace(*getter_AddRefs(sheetNS));
    nsCOMPtr<nsIAtom> prefixAtom;
    // will return null if namespace was the default
    sheetNS->FindNameSpacePrefix(mNameSpace, *getter_AddRefs(prefixAtom));
    if (prefixAtom) {
      const PRUnichar* prefix;
      prefixAtom->GetUnicode(&prefix);
      aString.Append(prefix);
      aString.Append(PRUnichar('|'));
    }
  }
  // smells like a universal selector
  if (!mTag && !mIDList && !mClassList) {
    if (1 != aNegatedIndex) {
      aString.Append(PRUnichar('*'));
    }
    if (1 < aNegatedIndex) {
      NS_IF_NEGATED_END(aIsNegated, aString)
    }
  } else {
    // Append the tag name, if there is one
    if (mTag) {
      mTag->GetUnicode(&temp);
      aString.Append(temp);
      NS_IF_NEGATED_END(aIsNegated, aString)
    }
    // Append the id, if there is one
    if (mIDList) {
      nsAtomList* list = mIDList;
      while (list != nsnull) {
        list->mAtom->GetUnicode(&temp);
        NS_IF_NEGATED_START(aIsNegated, aString)
        aString.Append(PRUnichar('#'));
        aString.Append(temp);
        NS_IF_NEGATED_END(aIsNegated, aString)
        list = list->mNext;
      }
    }
    // Append each class in the linked list
    if (mClassList) {
      nsAtomList* list = mClassList;
      while (list != nsnull) {
        list->mAtom->GetUnicode(&temp);
        NS_IF_NEGATED_START(aIsNegated, aString)
        aString.Append(PRUnichar('.'));
        aString.Append(temp);
        NS_IF_NEGATED_END(aIsNegated, aString)
        list = list->mNext;
      }
    }
  }

  // Append each attribute selector in the linked list
  if (mAttrList) {
    nsAttrSelector* list = mAttrList;
    while (list != nsnull) {
      NS_IF_NEGATED_START(aIsNegated, aString)
      aString.Append(PRUnichar('['));
      // Append the namespace prefix
      if (list->mNameSpace > 0) {
        nsCOMPtr<nsINameSpace> sheetNS;
        aSheet->GetNameSpace(*getter_AddRefs(sheetNS));
        nsCOMPtr<nsIAtom> prefixAtom;
        // will return null if namespace was the default
        sheetNS->FindNameSpacePrefix(list->mNameSpace, *getter_AddRefs(prefixAtom));
        if (prefixAtom) { 
          const PRUnichar* prefix;
          prefixAtom->GetUnicode(&prefix);
          aString.Append(prefix);
          aString.Append(PRUnichar('|'));
        }
      }
      // Append the attribute name
      list->mAttr->GetUnicode(&temp);
      aString.Append(temp);
      // Append the function
      if (list->mFunction == NS_ATTR_FUNC_EQUALS) {
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_INCLUDES) {
        aString.Append(PRUnichar('~'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_DASHMATCH) {
        aString.Append(PRUnichar('|'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_BEGINSMATCH) {
        aString.Append(PRUnichar('^'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_ENDSMATCH) {
        aString.Append(PRUnichar('$'));
        aString.Append(PRUnichar('='));
      } else if (list->mFunction == NS_ATTR_FUNC_CONTAINSMATCH) {
        aString.Append(PRUnichar('*'));
        aString.Append(PRUnichar('='));
      }
      // Append the value
      aString.Append(list->mValue);
      aString.Append(PRUnichar(']'));
      NS_IF_NEGATED_END(aIsNegated, aString)
      list = list->mNext;
    }
  }

  // Append each pseudo-class in the linked list
  if (mPseudoClassList) {
    nsAtomList* list = mPseudoClassList;
    while (list != nsnull) {
      list->mAtom->GetUnicode(&temp);
      NS_IF_NEGATED_START(aIsNegated, aString)
      aString.Append(temp);
      NS_IF_NEGATED_END(aIsNegated, aString)
      list = list->mNext;
    }
  }

  if (mNegations) {
    // chain all the negated selectors
    mNegations->ToString(aString, aSheet, PR_FALSE, aNegatedIndex + 1);
  }

  // Append the operator only if the selector is not negated and is not
  // a pseudo-element
  if (!aIsNegated && mOperator && !aIsPseudoElem) {
    aString.Append(PRUnichar(' '));
    aString.Append(mOperator);
  }
  return NS_OK;
}


// -- CSSImportantRule -------------------------------

static nscoord CalcLength(const nsCSSValue& aValue, const nsFont& aFont, 
                          nsIPresContext* aPresContext);
static PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                       const nsStyleCoord& aParentCoord,
                       PRInt32 aMask, const nsFont& aFont, 
                       nsIPresContext* aPresContext);

// New map helpers shared by both important and regular rules.
static nsresult MapFontForDeclaration(nsICSSDeclaration* aDecl, nsCSSFont& aFont);
static nsresult MapDisplayForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSDisplay& aDisplay);
static nsresult MapColorForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSColor& aColor);
static nsresult MapMarginForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSMargin& aMargin); 
static nsresult MapListForDeclaration(nsICSSDeclaration* aDecl, nsCSSList& aList);
static nsresult MapPositionForDeclaration(nsICSSDeclaration* aDecl, nsCSSPosition& aPosition);
static nsresult MapTableForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSTable& aTable);
static nsresult MapContentForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSContent& aContent);
static nsresult MapTextForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSText& aContent);
static nsresult MapUIForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSUserInterface& aContent);

#ifdef INCLUDE_XUL
static nsresult MapXULForDeclaration(nsICSSDeclaration* aDecl, nsCSSXUL& aXUL);
#endif

class CSSStyleRuleImpl;

class CSSImportantRule : public nsIStyleRule {
public:
  CSSImportantRule(nsICSSStyleSheet* aSheet, nsICSSDeclaration* aDeclaration);

  NS_DECL_ISUPPORTS

//  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
//  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

protected:
  virtual ~CSSImportantRule(void);

  nsICSSDeclaration*  mDeclaration;
  nsICSSStyleSheet*   mSheet;

friend class CSSStyleRuleImpl;
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

NS_IMPL_ISUPPORTS1(CSSImportantRule, nsIStyleRule)

#if 0
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
#endif

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
CSSImportantRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (!aRuleData)
    return NS_OK;

  if (aRuleData->mFontData)
    return MapFontForDeclaration(mDeclaration, *aRuleData->mFontData);
  else if (aRuleData->mDisplayData)
    return MapDisplayForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mDisplayData);
  else if (aRuleData->mColorData)
    return MapColorForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mColorData);
  else if (aRuleData->mMarginData)
    return MapMarginForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mMarginData);
  else if (aRuleData->mListData)
    return MapListForDeclaration(mDeclaration, *aRuleData->mListData);
  else if (aRuleData->mPositionData)
    return MapPositionForDeclaration(mDeclaration, *aRuleData->mPositionData);
  else if (aRuleData->mTableData)
    return MapTableForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mTableData);
  else if (aRuleData->mContentData)
    return MapContentForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mContentData);
  else if (aRuleData->mTextData)
    return MapTextForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mTextData);
  else if (aRuleData->mUIData)
    return MapUIForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mUIData);
#ifdef INCLUDE_XUL
  else if (aRuleData->mXULData)
    return MapXULForDeclaration(mDeclaration, *aRuleData->mXULData);
#endif

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

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSImportantRule's size): 
*    1) sizeof(*this) 
*
*  Contained / Aggregated data (not reported as CSSImportantRule's size):
*    1) mDeclaration is sized seperately
*    2) mSheet is sized seperately
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSImportantRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSImportantRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(CSSImportantRule);
  aSizeOfHandler->AddSize(tag,aSize);

  // now dump the mDeclaration and mSheet
  if(mDeclaration){
    mDeclaration->SizeOf(aSizeOfHandler, localSize);
  }
  if(mSheet){
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}

// -- nsDOMStyleRuleDeclaration -------------------------------

class DOMCSSDeclarationImpl : public nsDOMCSSDeclaration
{
public:
  DOMCSSDeclarationImpl(nsICSSStyleRule *aRule);
  ~DOMCSSDeclarationImpl(void);

  NS_IMETHOD RemoveProperty(const nsAReadableString& aPropertyName, 
                            nsAWritableString& aReturn);

  virtual void DropReference(void);
  virtual nsresult GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                     PRBool aAllocate);
  virtual nsresult SetCSSDeclaration(nsICSSDeclaration *aDecl);
  virtual nsresult ParseDeclaration(const nsAReadableString& aDecl,
                                    PRBool aParseOnlyOneDecl,
                                    PRBool aClearOldDecl);
  virtual nsresult GetParent(nsISupports **aParent);

protected:
  nsICSSStyleRule *mRule;
};

MOZ_DECL_CTOR_COUNTER(DOMCSSDeclarationImpl)

DOMCSSDeclarationImpl::DOMCSSDeclarationImpl(nsICSSStyleRule *aRule)
{
  MOZ_COUNT_CTOR(DOMCSSDeclarationImpl);

  // This reference is not reference-counted. The rule
  // object tells us when its about to go away.
  mRule = aRule;
}

DOMCSSDeclarationImpl::~DOMCSSDeclarationImpl(void)
{
  MOZ_COUNT_DTOR(DOMCSSDeclarationImpl);
}

NS_IMETHODIMP
DOMCSSDeclarationImpl::RemoveProperty(const nsAReadableString& aPropertyName, 
                                      nsAWritableString& aReturn)
{
  aReturn.Truncate();

  nsCOMPtr<nsICSSDeclaration> decl;
  nsresult rv = GetCSSDeclaration(getter_AddRefs(decl), PR_TRUE);

  if (NS_SUCCEEDED(rv) && decl) {
    nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);
    nsCSSValue val;

    rv = decl->RemoveProperty(prop, val);

    if (NS_SUCCEEDED(rv)) {
      // We pass in eCSSProperty_UNKNOWN here so that we don't get the
      // property name in the return string.
      val.ToString(aReturn, eCSSProperty_UNKNOWN);
    } else {
      // If we tried to remove an invalid property or a property that wasn't 
      //  set we simply return success and an empty string
      rv = NS_OK;
    }
  }

  return rv;
}

void 
DOMCSSDeclarationImpl::DropReference(void)
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
DOMCSSDeclarationImpl::SetCSSDeclaration(nsICSSDeclaration *aDecl)
{
  if (nsnull != mRule) {
    mRule->SetDeclaration(aDecl);
  }

  return NS_OK;
}

nsresult 
DOMCSSDeclarationImpl::ParseDeclaration(const nsAReadableString& aDecl,
                                        PRBool aParseOnlyOneDecl,
                                        PRBool aClearOldDecl)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_TRUE);

  if (NS_SUCCEEDED(result) && (decl)) {
    nsICSSLoader* cssLoader = nsnull;
    nsICSSParser* cssParser = nsnull;
    nsIURI* baseURI = nsnull;
    nsICSSStyleSheet* cssSheet = nsnull;
    nsIDocument*  owningDoc = nsnull;

    nsIStyleSheet* sheet = nsnull;
    if (mRule) {
      mRule->GetStyleSheet(sheet);
      if (sheet) {
        sheet->GetURL(baseURI);
        sheet->GetOwningDocument(owningDoc);
        sheet->QueryInterface(NS_GET_IID(nsICSSStyleSheet), (void**)&cssSheet);
        if (owningDoc) {
          nsIHTMLContentContainer* htmlContainer;
          result = owningDoc->QueryInterface(NS_GET_IID(nsIHTMLContentContainer), (void**)&htmlContainer);
          if (NS_SUCCEEDED(result)) {
            result = htmlContainer->GetCSSLoader(cssLoader);
            NS_RELEASE(htmlContainer);
          }
        }
        NS_RELEASE(sheet);
      }
    }
    if (cssLoader) {
      result = cssLoader->GetParserFor(nsnull, &cssParser);
    }
    else {
      result = NS_NewCSSParser(&cssParser);
    }

    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsICSSDeclaration> declClone;
      decl->Clone(*getter_AddRefs(declClone));
      NS_ENSURE_TRUE(declClone, NS_ERROR_OUT_OF_MEMORY);

      if (aClearOldDecl) {
        // This should be done with decl->Clear() once such a method exists.
        nsAutoString propName;
        PRUint32 count, i;

        decl->Count(&count);

        for (i = 0; i < count; i++) {
          decl->GetNthProperty(0, propName);

          nsCSSProperty prop = nsCSSProps::LookupProperty(propName);
          nsCSSValue val;
 
          decl->RemoveProperty(prop, val);
        }
      }

      PRInt32 hint;
      result = cssParser->ParseAndAppendDeclaration(aDecl, baseURI, decl,
                                                    aParseOnlyOneDecl, &hint);

      if (result == NS_CSS_PARSER_DROP_DECLARATION) {
        SetCSSDeclaration(declClone);
        result = NS_OK;
      } else if (NS_SUCCEEDED(result)) {
        if (cssSheet) {
          cssSheet->SetModified(PR_TRUE);
        }
        if (owningDoc) {
          owningDoc->StyleRuleChanged(cssSheet, mRule, hint);
        }
      }
      if (cssLoader) {
        cssLoader->RecycleParser(cssParser);
      }
      else {
        NS_RELEASE(cssParser);
      }
    }
    NS_IF_RELEASE(cssLoader);
    NS_IF_RELEASE(baseURI);
    NS_IF_RELEASE(cssSheet);
    NS_IF_RELEASE(owningDoc);
    NS_RELEASE(decl);
  }

  return result;
}



nsresult 
DOMCSSDeclarationImpl::GetParent(nsISupports **aParent)
{
  if (nsnull != mRule) {
    return mRule->QueryInterface(kISupportsIID, (void **)aParent);
  } else {
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;
  }

  return NS_OK;
}

// -- nsCSSStyleRule -------------------------------

class CSSStyleRuleImpl : public nsCSSRule,
                         public nsICSSStyleRule, 
                         public nsIDOMCSSStyleRule
{
public:
  CSSStyleRuleImpl(const nsCSSSelector& aSelector);
  CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy); 

  NS_DECL_ISUPPORTS_INHERITED

//  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
//  NS_IMETHOD HashValue(PRUint32& aValue) const;
  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  virtual nsCSSSelector* FirstSelector(void);
  virtual void AddSelector(const nsCSSSelector& aSelector);
  virtual void DeleteSelector(nsCSSSelector* aSelector);
  virtual void SetSourceSelectorText(const nsString& aSelectorText);
  virtual void GetSourceSelectorText(nsString& aSelectorText) const;

  virtual PRUint32 GetLineNumber(void) const;
  virtual void SetLineNumber(PRUint32 aLineNumber);

  virtual nsICSSDeclaration* GetDeclaration(void) const;
  virtual void SetDeclaration(nsICSSDeclaration* aDeclaration);

  virtual PRInt32 GetWeight(void) const;
  virtual void SetWeight(PRInt32 aWeight);

  virtual nsIStyleRule* GetImportantRule(void);

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);
  
  NS_IMETHOD SetParentRule(nsICSSGroupRule* aRule);

  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSStyleRule interface
  NS_DECL_NSIDOMCSSSTYLERULE

private: 
  // These are not supported and are not implemented! 
  CSSStyleRuleImpl& operator=(const CSSStyleRuleImpl& aCopy); 

protected:
  virtual ~CSSStyleRuleImpl(void);

protected:
  nsCSSSelector           mSelector;
  nsICSSDeclaration*      mDeclaration;
  PRInt32                 mWeight;
  CSSImportantRule*       mImportantRule;
  DOMCSSDeclarationImpl*  mDOMDeclaration;                          
  PRUint32                mLineNumber;
};

#ifdef DEBUG_REFS
PRUint32 gStyleRuleCount=0;
#endif

CSSStyleRuleImpl::CSSStyleRuleImpl(const nsCSSSelector& aSelector)
  : nsCSSRule(),
    mSelector(aSelector), mDeclaration(nsnull), 
    mWeight(0), mImportantRule(nsnull),
    mDOMDeclaration(nsnull)
{
#ifdef DEBUG_REFS
  gStyleRuleCount++;
  printf( "CSSStyleRuleImpl Instances (ctor): %ld\n", (long)gStyleRuleCount);
#endif
}

CSSStyleRuleImpl::CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mSelector(aCopy.mSelector),
    mDeclaration(nsnull),
    mWeight(aCopy.mWeight),
    mImportantRule(nsnull),
    mDOMDeclaration(nsnull)
{
#ifdef DEBUG_REFS
  gStyleRuleCount++;
  printf( "CSSStyleRuleImpl Instances (cp-ctor): %ld\n", (long)gStyleRuleCount);
#endif

  nsCSSSelector* copySel = aCopy.mSelector.mNext;
  nsCSSSelector* ourSel = &mSelector;

  while (copySel && ourSel) {
    ourSel->mNext = new nsCSSSelector(*copySel);
    ourSel = ourSel->mNext;
    copySel = copySel->mNext;
  }

  if (aCopy.mDeclaration) {
    aCopy.mDeclaration->Clone(mDeclaration);
  }
  // rest is constructed lazily on existing data
}


CSSStyleRuleImpl::~CSSStyleRuleImpl(void)
{
#ifdef DEBUG_REFS
  gStyleRuleCount--;
  printf( "CSSStyleRuleImpl Instances (dtor): %ld\n", (long)gStyleRuleCount);
#endif

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
}

// QueryInterface implementation for CSSStyleRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSStyleRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSStyleRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleRule)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF_INHERITED(CSSStyleRuleImpl, nsCSSRule);
NS_IMPL_RELEASE_INHERITED(CSSStyleRuleImpl, nsCSSRule);


#if 0
NS_IMETHODIMP CSSStyleRuleImpl::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  nsICSSStyleRule* iCSSRule;

  if (this == aRule) {
    aResult = PR_TRUE;
  }
  else {
    aResult = PR_FALSE;
    if ((nsnull != aRule) && 
        (NS_OK == ((nsIStyleRule*)aRule)->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void**) &iCSSRule))) {

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
#endif

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
    /* no need for set, since get recreates the string */
}

void CSSStyleRuleImpl::GetSourceSelectorText(nsString& aSelectorText) const
{
  mSelector.ToString( aSelectorText, mSheet, IsPseudoElement(mSelector.mTag),
                      0 );
}

PRUint32 CSSStyleRuleImpl::GetLineNumber(void) const
{
  return mLineNumber;
}

void CSSStyleRuleImpl::SetLineNumber(PRUint32 aLineNumber)
{
  mLineNumber = aLineNumber;
}

nsICSSDeclaration* CSSStyleRuleImpl::GetDeclaration(void) const
{
  nsICSSDeclaration* result = mDeclaration;
  NS_IF_ADDREF(result);
  return result;
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
  return nsCSSRule::GetStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  nsCSSRule::SetStyleSheet(aSheet);
  if (nsnull != mImportantRule) { // we're responsible for this guy too
    mImportantRule->mSheet = aSheet;
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::SetParentRule(nsICSSGroupRule* aRule)
{
  return nsCSSRule::SetParentRule(aRule);
}

NS_IMETHODIMP
CSSStyleRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::STYLE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSStyleRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSStyleRuleImpl* clone = new CSSStyleRuleImpl(*this);
  if (clone) {
    return clone->QueryInterface(NS_GET_IID(nsICSSRule), (void **)&aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
CSSStyleRuleImpl::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (!aRuleData)
    return NS_OK;

  if (aRuleData->mFontData)
    return MapFontForDeclaration(mDeclaration, *aRuleData->mFontData);
  else if (aRuleData->mDisplayData)
    return MapDisplayForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mDisplayData);
  else if (aRuleData->mColorData)
    return MapColorForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mColorData);
  else if (aRuleData->mMarginData)
    return MapMarginForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mMarginData);
  else if (aRuleData->mListData)
    return MapListForDeclaration(mDeclaration, *aRuleData->mListData);
  else if (aRuleData->mPositionData)
    return MapPositionForDeclaration(mDeclaration, *aRuleData->mPositionData);
  else if (aRuleData->mTableData)
    return MapTableForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mTableData);
  else if (aRuleData->mContentData)
    return MapContentForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mContentData);
  else if (aRuleData->mTextData)
    return MapTextForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mTextData);
  else if (aRuleData->mUIData)
    return MapUIForDeclaration(mDeclaration, aRuleData->mSID, *aRuleData->mUIData);
#ifdef INCLUDE_XUL
  else if (aRuleData->mXULData)
    return MapXULForDeclaration(mDeclaration, *aRuleData->mXULData);
#endif

  return NS_OK;
}

static nsresult 
MapFontForDeclaration(nsICSSDeclaration* aDecl, nsCSSFont& aFont)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSFont* ourFont;
  aDecl->GetData(kCSSFontSID, (nsCSSStruct**)&ourFont);
  if (!ourFont)
    return NS_OK; // We don't have any rules for fonts.

  if (eCSSUnit_Null == aFont.mFamily.GetUnit() && eCSSUnit_Null != ourFont->mFamily.GetUnit())
    aFont.mFamily = ourFont->mFamily;

  if (eCSSUnit_Null == aFont.mStyle.GetUnit() && eCSSUnit_Null != ourFont->mStyle.GetUnit())
    aFont.mStyle = ourFont->mStyle;

  if (eCSSUnit_Null == aFont.mVariant.GetUnit() && eCSSUnit_Null != ourFont->mVariant.GetUnit())
    aFont.mVariant = ourFont->mVariant;

  if (eCSSUnit_Null == aFont.mWeight.GetUnit() && eCSSUnit_Null != ourFont->mWeight.GetUnit())
    aFont.mWeight = ourFont->mWeight;

  if (eCSSUnit_Null == aFont.mSize.GetUnit() && eCSSUnit_Null != ourFont->mSize.GetUnit())
    aFont.mSize = ourFont->mSize;

  return NS_OK;
}

#ifdef INCLUDE_XUL
static nsresult 
MapXULForDeclaration(nsICSSDeclaration* aDecl, nsCSSXUL& aXUL)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSXUL* ourXUL;
  aDecl->GetData(kCSSXULSID, (nsCSSStruct**)&ourXUL);
  if (!ourXUL)
    return NS_OK; // We don't have any rules for XUL.

  // box-align: enum, inherit
  if (aXUL.mBoxAlign.GetUnit() == eCSSUnit_Null && ourXUL->mBoxAlign.GetUnit() != eCSSUnit_Null)
    aXUL.mBoxAlign = ourXUL->mBoxAlign;

  // box-direction: enum, inherit
  if (aXUL.mBoxDirection.GetUnit() == eCSSUnit_Null && ourXUL->mBoxDirection.GetUnit() != eCSSUnit_Null)
    aXUL.mBoxDirection = ourXUL->mBoxDirection;

  // box-flex: enum, inherit
  if (aXUL.mBoxFlex.GetUnit() == eCSSUnit_Null && ourXUL->mBoxFlex.GetUnit() != eCSSUnit_Null)
    aXUL.mBoxFlex = ourXUL->mBoxFlex;

  // box-orient: enum, inherit
  if (aXUL.mBoxOrient.GetUnit() == eCSSUnit_Null && ourXUL->mBoxOrient.GetUnit() != eCSSUnit_Null)
    aXUL.mBoxOrient = ourXUL->mBoxOrient;

  // box-pack: enum, inherit
  if (aXUL.mBoxPack.GetUnit() == eCSSUnit_Null && ourXUL->mBoxPack.GetUnit() != eCSSUnit_Null)
    aXUL.mBoxPack = ourXUL->mBoxPack;

  return NS_OK;
}
#endif

static nsresult 
MapPositionForDeclaration(nsICSSDeclaration* aDecl, nsCSSPosition& aPosition)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSPosition* ourPosition;
  aDecl->GetData(kCSSPositionSID, (nsCSSStruct**)&ourPosition);
  if (!ourPosition)
    return NS_OK; // We don't have any rules for position.

  // box offsets: length, percent, auto, inherit
  if (ourPosition->mOffset) {
    if (aPosition.mOffset->mLeft.GetUnit() == eCSSUnit_Null && ourPosition->mOffset->mLeft.GetUnit() != eCSSUnit_Null)
      aPosition.mOffset->mLeft = ourPosition->mOffset->mLeft;

    if (aPosition.mOffset->mRight.GetUnit() == eCSSUnit_Null && ourPosition->mOffset->mRight.GetUnit() != eCSSUnit_Null)
      aPosition.mOffset->mRight = ourPosition->mOffset->mRight;

    if (aPosition.mOffset->mTop.GetUnit() == eCSSUnit_Null && ourPosition->mOffset->mTop.GetUnit() != eCSSUnit_Null)
      aPosition.mOffset->mTop = ourPosition->mOffset->mTop;

    if (aPosition.mOffset->mBottom.GetUnit() == eCSSUnit_Null && ourPosition->mOffset->mBottom.GetUnit() != eCSSUnit_Null)
      aPosition.mOffset->mBottom = ourPosition->mOffset->mBottom;
  }

  // width/min-width/max-width
  if (aPosition.mWidth.GetUnit() == eCSSUnit_Null && ourPosition->mWidth.GetUnit() != eCSSUnit_Null)
    aPosition.mWidth = ourPosition->mWidth;
  if (aPosition.mMinWidth.GetUnit() == eCSSUnit_Null && ourPosition->mMinWidth.GetUnit() != eCSSUnit_Null)
    aPosition.mMinWidth = ourPosition->mMinWidth;
  if (aPosition.mMaxWidth.GetUnit() == eCSSUnit_Null && ourPosition->mMaxWidth.GetUnit() != eCSSUnit_Null)
    aPosition.mMaxWidth = ourPosition->mMaxWidth;

  // height/min-height/max-height
  if (aPosition.mHeight.GetUnit() == eCSSUnit_Null && ourPosition->mHeight.GetUnit() != eCSSUnit_Null)
    aPosition.mHeight = ourPosition->mHeight;
  if (aPosition.mMinHeight.GetUnit() == eCSSUnit_Null && ourPosition->mMinHeight.GetUnit() != eCSSUnit_Null)
    aPosition.mMinHeight = ourPosition->mMinHeight;
  if (aPosition.mMaxHeight.GetUnit() == eCSSUnit_Null && ourPosition->mMaxHeight.GetUnit() != eCSSUnit_Null)
    aPosition.mMaxHeight = ourPosition->mMaxHeight;

  // box-sizing: enum, inherit
  if (aPosition.mBoxSizing.GetUnit() == eCSSUnit_Null && ourPosition->mBoxSizing.GetUnit() != eCSSUnit_Null)
    aPosition.mBoxSizing = ourPosition->mBoxSizing;

  // z-index
  if (aPosition.mZIndex.GetUnit() == eCSSUnit_Null && ourPosition->mZIndex.GetUnit() != eCSSUnit_Null)
    aPosition.mZIndex = ourPosition->mZIndex;

  return NS_OK;
}

static nsresult 
MapListForDeclaration(nsICSSDeclaration* aDecl, nsCSSList& aList)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSList* ourList;
  aDecl->GetData(kCSSListSID, (nsCSSStruct**)&ourList);
  if (!ourList)
    return NS_OK; // We don't have any rules for lists.

  // list-style-type: enum, none, inherit
  if (aList.mType.GetUnit() == eCSSUnit_Null && ourList->mType.GetUnit() != eCSSUnit_Null)
    aList.mType = ourList->mType;

  // list-style-image: url, none, inherit
  if (aList.mImage.GetUnit() == eCSSUnit_Null && ourList->mImage.GetUnit() != eCSSUnit_Null)
    aList.mImage = ourList->mImage;
      
  // list-style-position: enum, inherit
  if (aList.mPosition.GetUnit() == eCSSUnit_Null && ourList->mPosition.GetUnit() != eCSSUnit_Null)
    aList.mPosition = ourList->mPosition;

  return NS_OK;
}
    
static nsresult
MapMarginForDeclaration(nsICSSDeclaration* aDeclaration, const nsStyleStructID& aSID, nsCSSMargin& aMargin)
{
  nsCSSMargin*  ourMargin;
  aDeclaration->GetData(kCSSMarginSID, (nsCSSStruct**)&ourMargin);
  if (!ourMargin)
    return NS_OK;

  // Margins
  if (aSID == eStyleStruct_Margin && ourMargin->mMargin) {
    if (eCSSUnit_Null == aMargin.mMargin->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mMargin->mLeft.GetUnit())
      aMargin.mMargin->mLeft = ourMargin->mMargin->mLeft;

    if (eCSSUnit_Null == aMargin.mMargin->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mMargin->mTop.GetUnit())
      aMargin.mMargin->mTop = ourMargin->mMargin->mTop;

    if (eCSSUnit_Null == aMargin.mMargin->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mMargin->mRight.GetUnit())
      aMargin.mMargin->mRight = ourMargin->mMargin->mRight;

    if (eCSSUnit_Null == aMargin.mMargin->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mMargin->mBottom.GetUnit())
      aMargin.mMargin->mBottom = ourMargin->mMargin->mBottom;
  }

  // Padding
  if (aSID == eStyleStruct_Padding && ourMargin->mPadding) {
    if (eCSSUnit_Null == aMargin.mPadding->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mPadding->mLeft.GetUnit())
      aMargin.mPadding->mLeft = ourMargin->mPadding->mLeft;

    if (eCSSUnit_Null == aMargin.mPadding->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mPadding->mTop.GetUnit())
      aMargin.mPadding->mTop = ourMargin->mPadding->mTop;

    if (eCSSUnit_Null == aMargin.mPadding->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mPadding->mRight.GetUnit())
      aMargin.mPadding->mRight = ourMargin->mPadding->mRight;

    if (eCSSUnit_Null == aMargin.mPadding->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mPadding->mBottom.GetUnit())
      aMargin.mPadding->mBottom = ourMargin->mPadding->mBottom;
  }

  // Borders
  if (aSID == eStyleStruct_Border) {
    // border-size
    if (ourMargin->mBorderWidth) {
      if (eCSSUnit_Null == aMargin.mBorderWidth->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mBorderWidth->mLeft.GetUnit())
        aMargin.mBorderWidth->mLeft = ourMargin->mBorderWidth->mLeft;

      if (eCSSUnit_Null == aMargin.mBorderWidth->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mBorderWidth->mTop.GetUnit())
        aMargin.mBorderWidth->mTop = ourMargin->mBorderWidth->mTop;

      if (eCSSUnit_Null == aMargin.mBorderWidth->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mBorderWidth->mRight.GetUnit())
        aMargin.mBorderWidth->mRight = ourMargin->mBorderWidth->mRight;

      if (eCSSUnit_Null == aMargin.mBorderWidth->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mBorderWidth->mBottom.GetUnit())
        aMargin.mBorderWidth->mBottom = ourMargin->mBorderWidth->mBottom;
    }

    // border-style
    if (ourMargin->mBorderStyle) {
      if (eCSSUnit_Null == aMargin.mBorderStyle->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mBorderStyle->mLeft.GetUnit())
        aMargin.mBorderStyle->mLeft = ourMargin->mBorderStyle->mLeft;

      if (eCSSUnit_Null == aMargin.mBorderStyle->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mBorderStyle->mTop.GetUnit())
        aMargin.mBorderStyle->mTop = ourMargin->mBorderStyle->mTop;

      if (eCSSUnit_Null == aMargin.mBorderStyle->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mBorderStyle->mRight.GetUnit())
        aMargin.mBorderStyle->mRight = ourMargin->mBorderStyle->mRight;

      if (eCSSUnit_Null == aMargin.mBorderStyle->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mBorderStyle->mBottom.GetUnit())
        aMargin.mBorderStyle->mBottom = ourMargin->mBorderStyle->mBottom;
    }

    // border-color
    if (ourMargin->mBorderColor) {
      if (eCSSUnit_Null == aMargin.mBorderColor->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mBorderColor->mLeft.GetUnit())
        aMargin.mBorderColor->mLeft = ourMargin->mBorderColor->mLeft;

      if (eCSSUnit_Null == aMargin.mBorderColor->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mBorderColor->mTop.GetUnit())
        aMargin.mBorderColor->mTop = ourMargin->mBorderColor->mTop;

      if (eCSSUnit_Null == aMargin.mBorderColor->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mBorderColor->mRight.GetUnit())
        aMargin.mBorderColor->mRight = ourMargin->mBorderColor->mRight;

      if (eCSSUnit_Null == aMargin.mBorderColor->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mBorderColor->mBottom.GetUnit())
        aMargin.mBorderColor->mBottom = ourMargin->mBorderColor->mBottom;
    }

    // -moz-border-radius
    if (ourMargin->mBorderRadius) {
      if (eCSSUnit_Null == aMargin.mBorderRadius->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mBorderRadius->mLeft.GetUnit())
        aMargin.mBorderRadius->mLeft = ourMargin->mBorderRadius->mLeft;

      if (eCSSUnit_Null == aMargin.mBorderRadius->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mBorderRadius->mTop.GetUnit())
        aMargin.mBorderRadius->mTop = ourMargin->mBorderRadius->mTop;

      if (eCSSUnit_Null == aMargin.mBorderRadius->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mBorderRadius->mRight.GetUnit())
        aMargin.mBorderRadius->mRight = ourMargin->mBorderRadius->mRight;

      if (eCSSUnit_Null == aMargin.mBorderRadius->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mBorderRadius->mBottom.GetUnit())
        aMargin.mBorderRadius->mBottom = ourMargin->mBorderRadius->mBottom;
    }

    // float-edge
    if (eCSSUnit_Null == aMargin.mFloatEdge.GetUnit() && eCSSUnit_Null != ourMargin->mFloatEdge.GetUnit())
      aMargin.mFloatEdge = ourMargin->mFloatEdge;
  }

  // Outline
  if (aSID == eStyleStruct_Outline) {
    // -moz-outline-radius
    if (ourMargin->mOutlineRadius) {
      if (eCSSUnit_Null == aMargin.mOutlineRadius->mLeft.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineRadius->mLeft.GetUnit())
        aMargin.mOutlineRadius->mLeft = ourMargin->mOutlineRadius->mLeft;

      if (eCSSUnit_Null == aMargin.mOutlineRadius->mTop.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineRadius->mTop.GetUnit())
        aMargin.mOutlineRadius->mTop = ourMargin->mOutlineRadius->mTop;

      if (eCSSUnit_Null == aMargin.mOutlineRadius->mRight.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineRadius->mRight.GetUnit())
        aMargin.mOutlineRadius->mRight = ourMargin->mOutlineRadius->mRight;

      if (eCSSUnit_Null == aMargin.mOutlineRadius->mBottom.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineRadius->mBottom.GetUnit())
        aMargin.mOutlineRadius->mBottom = ourMargin->mOutlineRadius->mBottom;
    }

    // outline-width
    if (eCSSUnit_Null == aMargin.mOutlineWidth.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineWidth.GetUnit())
      aMargin.mOutlineWidth = ourMargin->mOutlineWidth;

    // outline-color
    if (eCSSUnit_Null == aMargin.mOutlineColor.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineColor.GetUnit())
      aMargin.mOutlineColor = ourMargin->mOutlineColor;

    // outline-style
    if (eCSSUnit_Null == aMargin.mOutlineStyle.GetUnit() && eCSSUnit_Null != ourMargin->mOutlineStyle.GetUnit())
      aMargin.mOutlineStyle = ourMargin->mOutlineStyle;
  }

  return NS_OK;
}

static nsresult
MapColorForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSColor& aColor)
{
  if (!aDecl)
    return NS_OK;

  nsCSSColor* ourColor;
  aDecl->GetData(kCSSColorSID, (nsCSSStruct**)&ourColor);
  if (!ourColor)
    return NS_OK; // No rules for color or background.

  if (aID == eStyleStruct_Color) {
    // color: color, string, inherit
    if (aColor.mColor.GetUnit() == eCSSUnit_Null && ourColor->mColor.GetUnit() != eCSSUnit_Null)
      aColor.mColor = ourColor->mColor;
  }
  else if (aID == eStyleStruct_Background) {
    // background-color: color, string, enum (flags), inherit
    if (aColor.mBackColor.GetUnit() == eCSSUnit_Null && ourColor->mBackColor.GetUnit() != eCSSUnit_Null)
      aColor.mBackColor = ourColor->mBackColor;

    // background-image: url, none, inherit
    if (aColor.mBackImage.GetUnit() == eCSSUnit_Null && ourColor->mBackImage.GetUnit() != eCSSUnit_Null)
      aColor.mBackImage = ourColor->mBackImage;

    // background-repeat: enum, inherit
    if (aColor.mBackRepeat.GetUnit() == eCSSUnit_Null && ourColor->mBackRepeat.GetUnit() != eCSSUnit_Null)
      aColor.mBackRepeat = ourColor->mBackRepeat;

    // background-attachment: enum, inherit
    if (aColor.mBackAttachment.GetUnit() == eCSSUnit_Null && ourColor->mBackAttachment.GetUnit() != eCSSUnit_Null)
      aColor.mBackAttachment = ourColor->mBackAttachment;
      
    // background-position: enum, length, percent (flags), inherit
    if (aColor.mBackPositionX.GetUnit() == eCSSUnit_Null && ourColor->mBackPositionX.GetUnit() != eCSSUnit_Null)
      aColor.mBackPositionX = ourColor->mBackPositionX;
    if (aColor.mBackPositionY.GetUnit() == eCSSUnit_Null && ourColor->mBackPositionY.GetUnit() != eCSSUnit_Null)
      aColor.mBackPositionY = ourColor->mBackPositionY;
  }

  return NS_OK;
}

static nsresult 
MapTableForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSTable& aTable)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSTable* ourTable;
  aDecl->GetData(kCSSTableSID, (nsCSSStruct**)&ourTable);
  if (!ourTable)
    return NS_OK; // We don't have any rules for tables.

  if (aID == eStyleStruct_TableBorder) {
    // border-collapse: enum, inherit
    if (aTable.mBorderCollapse.GetUnit() == eCSSUnit_Null && ourTable->mBorderCollapse.GetUnit() != eCSSUnit_Null)
      aTable.mBorderCollapse = ourTable->mBorderCollapse;

    // border-spacing-x: length, inherit
    if (aTable.mBorderSpacingX.GetUnit() == eCSSUnit_Null && ourTable->mBorderSpacingX.GetUnit() != eCSSUnit_Null)
      aTable.mBorderSpacingX = ourTable->mBorderSpacingX;

    // border-spacing-y: length, inherit
    if (aTable.mBorderSpacingY.GetUnit() == eCSSUnit_Null && ourTable->mBorderSpacingY.GetUnit() != eCSSUnit_Null)
      aTable.mBorderSpacingY = ourTable->mBorderSpacingY;

    // caption-side: enum, inherit
    if (aTable.mCaptionSide.GetUnit() == eCSSUnit_Null && ourTable->mCaptionSide.GetUnit() != eCSSUnit_Null)
      aTable.mCaptionSide = ourTable->mCaptionSide;

    // empty-cells: enum, inherit
    if (aTable.mEmptyCells.GetUnit() == eCSSUnit_Null && ourTable->mEmptyCells.GetUnit() != eCSSUnit_Null)
      aTable.mEmptyCells = ourTable->mEmptyCells;
  }
  else if (aID == eStyleStruct_Table) {
    // table-layout: auto, enum, inherit
    if (aTable.mLayout.GetUnit() == eCSSUnit_Null && ourTable->mLayout.GetUnit() != eCSSUnit_Null)
      aTable.mLayout = ourTable->mLayout;
  }

  return NS_OK;
}

static nsresult 
MapContentForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSContent& aContent)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSContent* ourContent;
  aDecl->GetData(kCSSContentSID, (nsCSSStruct**)&ourContent);
  if (!ourContent)
    return NS_OK; // We don't have any rules for content.

  if (aID == eStyleStruct_Content) {
    if (!aContent.mContent && ourContent->mContent)
      aContent.mContent = ourContent->mContent;

    if (!aContent.mCounterIncrement && ourContent->mCounterIncrement)
      aContent.mCounterIncrement = ourContent->mCounterIncrement;

    if (!aContent.mCounterReset && ourContent->mCounterReset)
      aContent.mCounterReset = ourContent->mCounterReset;
    
    if (aContent.mMarkerOffset.GetUnit() == eCSSUnit_Null && ourContent->mMarkerOffset.GetUnit() != eCSSUnit_Null)
      aContent.mMarkerOffset = ourContent->mMarkerOffset;
  }
  else if (aID == eStyleStruct_Quotes) {
    if (!aContent.mQuotes && ourContent->mQuotes)
      aContent.mQuotes = ourContent->mQuotes;
  }

  return NS_OK;
}

static nsresult
MapTextForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSText& aText)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSText* ourText;
  aDecl->GetData(kCSSTextSID, (nsCSSStruct**)&ourText);
  if (!ourText)
    return NS_OK; // We don't have any rules for text.

  if (aID == eStyleStruct_Text) {
    if (aText.mLetterSpacing.GetUnit() == eCSSUnit_Null && ourText->mLetterSpacing.GetUnit() != eCSSUnit_Null)
      aText.mLetterSpacing = ourText->mLetterSpacing;

    if (aText.mLineHeight.GetUnit() == eCSSUnit_Null && ourText->mLineHeight.GetUnit() != eCSSUnit_Null)
      aText.mLineHeight = ourText->mLineHeight;

    if (aText.mTextIndent.GetUnit() == eCSSUnit_Null && ourText->mTextIndent.GetUnit() != eCSSUnit_Null)
      aText.mTextIndent = ourText->mTextIndent;

    if (aText.mTextTransform.GetUnit() == eCSSUnit_Null && ourText->mTextTransform.GetUnit() != eCSSUnit_Null)
      aText.mTextTransform = ourText->mTextTransform;

    if (aText.mTextAlign.GetUnit() == eCSSUnit_Null && ourText->mTextAlign.GetUnit() != eCSSUnit_Null)
      aText.mTextAlign = ourText->mTextAlign;

    if (aText.mWhiteSpace.GetUnit() == eCSSUnit_Null && ourText->mWhiteSpace.GetUnit() != eCSSUnit_Null)
      aText.mWhiteSpace = ourText->mWhiteSpace;

    if (aText.mWordSpacing.GetUnit() == eCSSUnit_Null && ourText->mWordSpacing.GetUnit() != eCSSUnit_Null)
      aText.mWordSpacing = ourText->mWordSpacing;
  }
  else if (aID == eStyleStruct_TextReset) {
    if (aText.mVerticalAlign.GetUnit() == eCSSUnit_Null && ourText->mVerticalAlign.GetUnit() != eCSSUnit_Null)
      aText.mVerticalAlign = ourText->mVerticalAlign;

    if (aText.mDecoration.GetUnit() == eCSSUnit_Null && ourText->mDecoration.GetUnit() != eCSSUnit_Null)
      aText.mDecoration = ourText->mDecoration;

#ifdef IBMBIDI
    if (aText.mUnicodeBidi.GetUnit() == eCSSUnit_Null && ourText->mUnicodeBidi.GetUnit() != eCSSUnit_Null)
      aText.mUnicodeBidi = ourText->mUnicodeBidi;
#endif
  }

  return NS_OK;

}

static nsresult 
MapDisplayForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSDisplay& aDisplay)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSDisplay* ourDisplay;
  aDecl->GetData(kCSSDisplaySID, (nsCSSStruct**)&ourDisplay);
  if (!ourDisplay)
    return NS_OK; // We don't have any rules for display.

  if (aID == eStyleStruct_Display) {
    // display: enum, none, inherit
    if (aDisplay.mDisplay.GetUnit() == eCSSUnit_Null && ourDisplay->mDisplay.GetUnit() != eCSSUnit_Null)
      aDisplay.mDisplay = ourDisplay->mDisplay;
    
    // binding: url, none, inherit
    if (aDisplay.mBinding.GetUnit() == eCSSUnit_Null && ourDisplay->mBinding.GetUnit() != eCSSUnit_Null)
      aDisplay.mBinding = ourDisplay->mBinding;

    // position: enum, inherit
    if (aDisplay.mPosition.GetUnit() == eCSSUnit_Null && ourDisplay->mPosition.GetUnit() != eCSSUnit_Null)
      aDisplay.mPosition = ourDisplay->mPosition;

    // clear: enum, none, inherit
    if (aDisplay.mClear.GetUnit() == eCSSUnit_Null && ourDisplay->mClear.GetUnit() != eCSSUnit_Null)
      aDisplay.mClear = ourDisplay->mClear;

    // float: enum, none, inherit
    if (aDisplay.mFloat.GetUnit() == eCSSUnit_Null && ourDisplay->mFloat.GetUnit() != eCSSUnit_Null)
      aDisplay.mFloat = ourDisplay->mFloat;

    // overflow: enum, auto, inherit
    if (aDisplay.mOverflow.GetUnit() == eCSSUnit_Null && ourDisplay->mOverflow.GetUnit() != eCSSUnit_Null)
      aDisplay.mOverflow = ourDisplay->mOverflow;
    
    // clip property: length, auto, inherit
    if (ourDisplay->mClip) {
      if (aDisplay.mClip->mLeft.GetUnit() == eCSSUnit_Null && ourDisplay->mClip->mLeft.GetUnit() != eCSSUnit_Null)
        aDisplay.mClip->mLeft = ourDisplay->mClip->mLeft;
      if (aDisplay.mClip->mRight.GetUnit() == eCSSUnit_Null && ourDisplay->mClip->mRight.GetUnit() != eCSSUnit_Null)
        aDisplay.mClip->mRight = ourDisplay->mClip->mRight;
      if (aDisplay.mClip->mTop.GetUnit() == eCSSUnit_Null && ourDisplay->mClip->mTop.GetUnit() != eCSSUnit_Null)
        aDisplay.mClip->mTop = ourDisplay->mClip->mTop;
      if (aDisplay.mClip->mBottom.GetUnit() == eCSSUnit_Null && ourDisplay->mClip->mBottom.GetUnit() != eCSSUnit_Null)
        aDisplay.mClip->mBottom = ourDisplay->mClip->mBottom;
    }
  }
  else if (aID == eStyleStruct_Visibility) {
    // opacity: factor, percent, inherit
    if (aDisplay.mOpacity.GetUnit() == eCSSUnit_Null && ourDisplay->mOpacity.GetUnit() != eCSSUnit_Null)
      aDisplay.mOpacity = ourDisplay->mOpacity;

    // direction: enum, inherit
    if (aDisplay.mDirection.GetUnit() == eCSSUnit_Null && ourDisplay->mDirection.GetUnit() != eCSSUnit_Null)
      aDisplay.mDirection = ourDisplay->mDirection;

    // visibility: enum, inherit
    if (aDisplay.mVisibility.GetUnit() == eCSSUnit_Null && ourDisplay->mVisibility.GetUnit() != eCSSUnit_Null)
      aDisplay.mVisibility = ourDisplay->mVisibility; 
  }

  return NS_OK;
}

static nsresult
MapUIForDeclaration(nsICSSDeclaration* aDecl, const nsStyleStructID& aID, nsCSSUserInterface& aUI)
{
  if (!aDecl)
    return NS_OK; // The rule must have a declaration.

  nsCSSUserInterface* ourUI;
  aDecl->GetData(kCSSUserInterfaceSID, (nsCSSStruct**)&ourUI);
  if (!ourUI)
    return NS_OK; // We don't have any rules for UI.

  if (aID == eStyleStruct_UserInterface) {
    if (aUI.mUserFocus.GetUnit() == eCSSUnit_Null && ourUI->mUserFocus.GetUnit() != eCSSUnit_Null)
      aUI.mUserFocus = ourUI->mUserFocus;
    
    if (aUI.mUserInput.GetUnit() == eCSSUnit_Null && ourUI->mUserInput.GetUnit() != eCSSUnit_Null)
      aUI.mUserInput = ourUI->mUserInput;

    if (aUI.mUserModify.GetUnit() == eCSSUnit_Null && ourUI->mUserModify.GetUnit() != eCSSUnit_Null)
      aUI.mUserModify = ourUI->mUserModify;

    if (!aUI.mCursor && ourUI->mCursor)
      aUI.mCursor = ourUI->mCursor;


  }
  else if (aID == eStyleStruct_UIReset) {
    if (aUI.mUserSelect.GetUnit() == eCSSUnit_Null && ourUI->mUserSelect.GetUnit() != eCSSUnit_Null)
      aUI.mUserSelect = ourUI->mUserSelect;
   
    if (!aUI.mKeyEquivalent && ourUI->mKeyEquivalent)
      aUI.mKeyEquivalent = ourUI->mKeyEquivalent;

    if (aUI.mResizer.GetUnit() == eCSSUnit_Null && ourUI->mResizer.GetUnit() != eCSSUnit_Null)
      aUI.mResizer = ourUI->mResizer;
  }

  return NS_OK;

}

NS_IMETHODIMP
CSSStyleRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;
  mSelector.ToString(buffer, mSheet, PR_FALSE, 0);

  buffer.AppendWithConversion(" weight: ");
  buffer.AppendInt(mWeight, 10);
  buffer.AppendWithConversion(" ");
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

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSStyleRuleImpl's size): 
*    1) sizeof(*this) 
*       + sizeof the DOMDeclaration if it exists and is unique
*
*  Contained / Aggregated data (not reported as CSSStyleRuleImpl's size):
*    1) mDeclaration if it exists
*    2) mImportantRule if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSStyleRuleImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSStyleRuleImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  // remove the sizeof the mSelector's class since we count it seperately below
  aSize -= sizeof(mSelector);

  // and add the size of the DOMDeclaration
  // XXX - investigate the size and quantity of these
  if(mDOMDeclaration && uniqueItems->AddItem(mDOMDeclaration)){
    aSize += sizeof(DOMCSSDeclarationImpl);
  }
  aSizeOfHandler->AddSize(tag,aSize);
  
  // now delegate to the Selector, Declaration, and ImportantRule
  mSelector.SizeOf(aSizeOfHandler, localSize);

  if(mDeclaration){
    mDeclaration->SizeOf(aSizeOfHandler, localSize);
  }
  if(mImportantRule){
    mImportantRule->SizeOf(aSizeOfHandler, localSize);
  }
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::STYLE_RULE;
  
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetCssText(nsAWritableString& aCssText)
{
  mSelector.ToString( aCssText, mSheet, IsPseudoElement(mSelector.mTag),
                      0 );
  aCssText.Append(PRUnichar(' '));
  aCssText.Append(PRUnichar('{'));
  aCssText.Append(PRUnichar(' '));
  if (mDeclaration)
  {
    nsAutoString   tempString;
    mDeclaration->ToString( tempString );
    aCssText.Append( tempString );
  }
  aCssText.Append(PRUnichar(' '));
  aCssText.Append(PRUnichar('}'));
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::SetCssText(const nsAReadableString& aCssText)
{
  // XXX TBI - need to re-parse rule & declaration
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (nsnull != mSheet) {
    return mSheet->QueryInterface(NS_GET_IID(nsIDOMCSSStyleSheet), (void**)aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    return CallQueryInterface(mParentRule, aParentRule);
  }
  *aParentRule = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::GetSelectorText(nsAWritableString& aSelectorText)
{
  mSelector.ToString( aSelectorText, mSheet, IsPseudoElement(mSelector.mTag), 0 );
  return NS_OK;
}

NS_IMETHODIMP    
CSSStyleRuleImpl::SetSelectorText(const nsAReadableString& aSelectorText)
{
  // XXX TBI - get a parser and re-parse the selectors, 
  // XXX then need to re-compute the cascade
  // XXX and dirty sheet
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
  return it->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void **) aInstancePtrResult);
}
