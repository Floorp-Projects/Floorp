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

#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsVoidArray.h"

static NS_DEFINE_IID(kIHTMLAttributesIID, NS_IHTML_ATTRIBUTES_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);

//#define DEBUG_REFS

struct HTMLAttribute {
  HTMLAttribute(void)
    : mAttribute(nsnull),
      mValue(),
      mNext(nsnull)
  {
  }

  HTMLAttribute(nsIAtom* aAttribute, const nsString& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    NS_IF_ADDREF(mAttribute);
  }

  HTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    NS_IF_ADDREF(mAttribute);
  }

  HTMLAttribute(const HTMLAttribute& aCopy)
    : mAttribute(aCopy.mAttribute),
      mValue(aCopy.mValue),
      mNext(nsnull)
  {
    NS_IF_ADDREF(mAttribute);
  }

  ~HTMLAttribute(void)
  {
    NS_IF_RELEASE(mAttribute);
  }

  HTMLAttribute& operator=(const HTMLAttribute& aCopy)
  {
    NS_IF_RELEASE(mAttribute);
    mAttribute = aCopy.mAttribute;
    NS_IF_ADDREF(mAttribute);
    mValue = aCopy.mValue;
    return *this;
  }

  PRBool operator==(const HTMLAttribute& aOther) const
  {
    return PRBool((mAttribute == aOther.mAttribute) &&
                  (mValue == aOther.mValue));
  }

  PRUint32 HashValue(void) const
  {
    return PRUint32(mAttribute) ^ mValue.HashValue();
  }

  void Reset(void)
  {
    NS_IF_RELEASE(mAttribute);
    mValue.Reset();
  }

  void Set(nsIAtom* aAttribute, const nsHTMLValue& aValue)
  {
    NS_IF_RELEASE(mAttribute);
    mAttribute = aAttribute;
    NS_IF_ADDREF(mAttribute);
    mValue = aValue;
  }

  void Set(nsIAtom* aAttribute, const nsString& aValue)
  {
    NS_IF_RELEASE(mAttribute);
    mAttribute = aAttribute;
    NS_IF_ADDREF(mAttribute);
    mValue.SetStringValue(aValue);
  }

  void AppendToString(nsString& aBuffer) const
  {
    if (nsnull != mAttribute) {
      nsAutoString temp;
      mAttribute->ToString(temp);
      aBuffer.Append(temp);
      if (eHTMLUnit_Null != mValue.GetUnit()) {
        aBuffer.Append(" = ");
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.Append("null");
    }
  }

  void ToString(nsString& aBuffer) const
  {
    if (nsnull != mAttribute) {
      mAttribute->ToString(aBuffer);
      if (eHTMLUnit_Null != mValue.GetUnit()) {
        aBuffer.Append(" = ");
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.Truncate();
      aBuffer.Append("null");
    }
  }

  nsIAtom*        mAttribute;
  nsHTMLValue     mValue;
  HTMLAttribute*  mNext;
};

// ----------------

struct nsClassList {
  nsClassList(nsIAtom* aAtom)
    : mAtom(aAtom),
      mNext(nsnull)
  {
  }
  nsClassList(const nsClassList& aCopy)
    : mAtom(aCopy.mAtom),
      mNext(nsnull)
  {
    NS_ADDREF(mAtom);
    if (nsnull != aCopy.mNext) {
      mNext = new nsClassList(*(aCopy.mNext));
    }
  }
  ~nsClassList(void)
  {
    NS_RELEASE(mAtom);
    if (nsnull != mNext) {
      delete mNext;
    }
  }

  nsIAtom*      mAtom;
  nsClassList*  mNext;
};

class HTMLAttributesImpl: public nsIHTMLAttributes, public nsIStyleRule {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLAttributesImpl(nsIHTMLStyleSheet* aSheet, 
                     nsMapAttributesFunc aFontMapFunc,
                     nsMapAttributesFunc aMapFunc);
  HTMLAttributesImpl(const HTMLAttributesImpl& aCopy);
  ~HTMLAttributesImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIHTMLAttributes
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRInt32& aAttrCount);
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                          PRInt32& aAttrCount);
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute, PRInt32& aAttrCount);

  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aValue) const;

  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                nsIAtom*& aName) const;

  NS_IMETHOD GetAttributeCount(PRInt32& aCount) const;
  NS_IMETHOD Equals(const nsIHTMLAttributes* aAttributes, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD HasClass(nsIAtom* aClass) const;

  NS_IMETHOD AddContentRef(void);
  NS_IMETHOD ReleaseContentRef(void);
  NS_IMETHOD GetContentRefCount(PRInt32& aRefCount) const;

  NS_IMETHOD Clone(nsIHTMLAttributes** aInstancePtrResult) const;
  NS_IMETHOD Reset(void);
  NS_IMETHOD SetMappingFunctions(nsMapAttributesFunc aFontMapFunc, nsMapAttributesFunc aMapFunc);

  // nsIStyleRule 
  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsIHTMLStyleSheet* aSheet);
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;
  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

private:
  HTMLAttributesImpl& operator=(const HTMLAttributesImpl& aCopy);
  PRBool operator==(const HTMLAttributesImpl& aCopy) const;

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIHTMLStyleSheet*  mSheet;
  PRInt32             mContentRefCount;
  PRInt32             mCount;
  HTMLAttribute       mFirst;
  nsIAtom*            mID;
  nsClassList*        mClassList;
  nsMapAttributesFunc mFontMapper;
  nsMapAttributesFunc mMapper;

#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};


#ifdef DEBUG_REFS
static PRInt32  gInstanceCount = 0;
static PRInt32  gInstrument = 4;
#endif


void* HTMLAttributesImpl::operator new(size_t size)
{
  HTMLAttributesImpl* rv = (HTMLAttributesImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* HTMLAttributesImpl::operator new(size_t size, nsIArena* aArena)
{
  HTMLAttributesImpl* rv = (HTMLAttributesImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void HTMLAttributesImpl::operator delete(void* ptr)
{
  HTMLAttributesImpl* attr = (HTMLAttributesImpl*) ptr;
  if (nsnull != attr) {
    if (attr->mInHeap) {
      ::operator delete(ptr);
    }
  }
}


HTMLAttributesImpl::HTMLAttributesImpl(nsIHTMLStyleSheet* aSheet, 
                                       nsMapAttributesFunc aFontMapFunc,
                                       nsMapAttributesFunc aMapFunc)
  : mSheet(aSheet),
    mFirst(),
    mCount(0),
    mID(nsnull),
    mClassList(nsnull),
    mContentRefCount(0),
    mFontMapper(aFontMapFunc),
    mMapper(aMapFunc)
{
  NS_INIT_REFCNT();

#ifdef DEBUG_REFS
  mInstance = ++gInstanceCount;
  fprintf(stdout, "%d of %d + HTMLAttributes\n", mInstance, gInstanceCount);
#endif
}

HTMLAttributesImpl::HTMLAttributesImpl(const HTMLAttributesImpl& aCopy)
  : mSheet(aCopy.mSheet),
    mFirst(aCopy.mFirst),
    mCount(aCopy.mCount),
    mID(aCopy.mID),
    mClassList(nsnull),
    mContentRefCount(0),
    mFontMapper(aCopy.mFontMapper),
    mMapper(aCopy.mMapper)
{
  NS_INIT_REFCNT();

  HTMLAttribute* next = aCopy.mFirst.mNext;
  HTMLAttribute* last = &mFirst;
  while (nsnull != next) {
    HTMLAttribute* attr = new HTMLAttribute(*next);
    last->mNext = attr;
    last = attr;
    next = next->mNext;
  }

  NS_IF_ADDREF(mID);
  if (nsnull != aCopy.mClassList) {
    mClassList = new nsClassList(*(aCopy.mClassList));
  }

#ifdef DEBUG_REFS
  mInstance = ++gInstanceCount;
  fprintf(stdout, "%d of %d + HTMLAttributes\n", mInstance, gInstanceCount);
#endif
}

HTMLAttributesImpl::~HTMLAttributesImpl(void)
{
  Reset();

#ifdef DEBUG_REFS
  fprintf(stdout, "%d of %d - HTMLAttribtues\n", mInstance, gInstanceCount);
  --gInstanceCount;
#endif
}

#ifdef DEBUG_REFS
nsrefcnt HTMLAttributesImpl::AddRef(void)                                
{                                    
  if ((gInstrument == -1) || (mInstance == gInstrument)) {
    fprintf(stdout, "%d AddRef HTMLAttributes %d\n", mRefCnt + 1, mInstance);
  }
  return ++mRefCnt;                                          
}

nsrefcnt HTMLAttributesImpl::Release(void)                         
{                                                      
  if ((gInstrument == -1) || (mInstance == gInstrument)) {
    fprintf(stdout, "%d Release HTMLAttributes %d\n", mRefCnt - 1, mInstance);
  }
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}
#else
NS_IMPL_ADDREF(HTMLAttributesImpl)
NS_IMPL_RELEASE(HTMLAttributesImpl)
#endif


nsresult HTMLAttributesImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIHTMLAttributesIID)) {
    *aInstancePtrResult = (void*) ((nsIHTMLAttributes*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)(nsIHTMLAttributes*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP
HTMLAttributesImpl::Equals(const nsIHTMLAttributes* aAttributes, PRBool& aResult) const
{
  if (this == aAttributes) {
    aResult = PR_TRUE;
  }
  else {
    const HTMLAttributesImpl* other = (HTMLAttributesImpl*)aAttributes;
    aResult = PR_FALSE;

    if (mCount == other->mCount) {
      if (mID == other->mID) {
        const HTMLAttribute* attr = &mFirst;
        const HTMLAttribute* otherAttr = (&other->mFirst);

        aResult = PR_TRUE;
        while (nsnull != attr) {
          if (! ((*attr) == (*otherAttr))) {
            aResult = PR_FALSE;
            break;
          }
          attr = attr->mNext;
          otherAttr = otherAttr->mNext;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  nsIHTMLAttributes* iHTMLAttr;

  if (this == aRule) {
    aResult = PR_TRUE;
  }
  else {
    aResult = PR_FALSE;
    if ((nsnull != aRule) && 
        (NS_OK == ((nsIStyleRule*)aRule)->QueryInterface(kIHTMLAttributesIID, (void**) &iHTMLAttr))) {

      Equals(iHTMLAttr, aResult);

      NS_RELEASE(iHTMLAttr);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::HashValue(PRUint32& aValue) const
{
  aValue = 0;

  const HTMLAttribute* attr = &mFirst;
  while (nsnull != attr) {
    if (nsnull != attr->mAttribute) {
      aValue = aValue ^ attr->HashValue();
    }
    attr = attr->mNext;
  }
  return NS_OK;
}

const PRUnichar kNullCh = PRUnichar('\0');

static void ParseClasses(const nsString& aClassString, nsClassList** aClassList)
{
  NS_ASSERTION(nsnull == *aClassList, "non null start list");

  nsAutoString  classStr(aClassString);  // copy to work buffer
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr;
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
      *aClassList = new nsClassList(NS_NewAtom(start));
      aClassList = &((*aClassList)->mNext);
    }

    start = ++end;
  }
}


NS_IMETHODIMP
HTMLAttributesImpl::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                                 PRInt32& aCount)
{
  if (nsHTMLAtoms::id == aAttribute) {
    NS_IF_RELEASE(mID);
    mID = NS_NewAtom(aValue);
  }
  else if (nsHTMLAtoms::kClass == aAttribute) {
    if (nsnull != mClassList) {
      delete mClassList;
      mClassList = nsnull;
    }
    ParseClasses(aValue, &mClassList);
  }

  HTMLAttribute*  last = nsnull;
  HTMLAttribute*  attr = &mFirst;

  if (nsnull != mFirst.mAttribute) {
    while (nsnull != attr) {
      if (attr->mAttribute == aAttribute) {
        attr->mValue.SetStringValue(aValue);
        aCount = mCount;
        return NS_OK;
      }
      last = attr;
      attr = attr->mNext;
    }
  }

  if (nsnull == last) {
    mFirst.Set(aAttribute, aValue);
  }
  else {
    attr = new HTMLAttribute(aAttribute, aValue);
    if (nsnull == attr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    last->mNext = attr;
  }
  aCount = ++mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetAttribute(nsIAtom* aAttribute,
                                 const nsHTMLValue& aValue,
                                 PRInt32& aCount)
{
  if (eHTMLUnit_Null == aValue.GetUnit()) {
    return UnsetAttribute(aAttribute, aCount);
  }
  
  if (nsHTMLAtoms::id == aAttribute) {
    NS_IF_RELEASE(mID);
    if (eHTMLUnit_String == aValue.GetUnit()) {
      nsAutoString  buffer;
      mID = NS_NewAtom(aValue.GetStringValue(buffer));
    }
  }
  else if (nsHTMLAtoms::kClass == aAttribute) {
    if (nsnull != mClassList) {
      delete mClassList;
      mClassList = nsnull;
    }
    if (eHTMLUnit_String == aValue.GetUnit()) {
      nsAutoString  buffer;
      aValue.GetStringValue(buffer);
      ParseClasses(buffer, &mClassList);
    }
  }

  HTMLAttribute*  last = nsnull;
  HTMLAttribute*  attr = &mFirst;

  if (nsnull != mFirst.mAttribute) {
    while (nsnull != attr) {
      if (attr->mAttribute == aAttribute) {
        attr->mValue = aValue;
        aCount = mCount;
        return NS_OK;
      }
      last = attr;
      attr = attr->mNext;
    }
  }

  if (nsnull == last) {
    mFirst.Set(aAttribute, aValue);
  }
  else {
    attr = new HTMLAttribute(aAttribute, aValue);
    if (nsnull == attr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    last->mNext = attr;
  }
  aCount = ++mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::UnsetAttribute(nsIAtom* aAttribute,
                                   PRInt32& aCount)
{
  if (nsHTMLAtoms::id == aAttribute) {
    NS_IF_RELEASE(mID);
  }
  else if (nsHTMLAtoms::kClass == aAttribute) {
    if (nsnull != mClassList) {
      delete mClassList;
      mClassList = nsnull;
    }
  }

  HTMLAttribute*  prev = nsnull;
  HTMLAttribute*  attr = &mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      if (nsnull == prev) {
        if (nsnull != mFirst.mNext) {
          attr = mFirst.mNext;
          mFirst = *(mFirst.mNext);
          mFirst.mNext = mFirst.mNext->mNext;
          delete attr;
        }
        else {
          mFirst.Reset();
        }
      }
      else {
        prev->mNext = attr->mNext;
        delete attr;
      }
      mCount--;
      break;
    }
    prev = attr;
    attr = attr->mNext;
  }
  aCount = mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttribute(nsIAtom* aAttribute,
                                 nsHTMLValue& aValue) const
{
  aValue.Reset();

  const HTMLAttribute*  attr = &mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      aValue = attr->mValue;
      return (attr->mValue.GetUnit() == eHTMLUnit_Null)
        ? NS_CONTENT_ATTR_NO_VALUE
        : NS_CONTENT_ATTR_HAS_VALUE;
    }
    attr = attr->mNext;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttributeNameAt(PRInt32 aIndex,
                                       nsIAtom*& aName) const
{
  nsresult result = NS_ERROR_ILLEGAL_VALUE;

  if ((0 <= aIndex) && (aIndex < mCount)) {
    PRInt32 index = 0;
    const HTMLAttribute*  attr = &mFirst;

    while (nsnull != attr) {
      if (nsnull != attr->mAttribute) {
        if (aIndex == index++) {
          aName = attr->mAttribute;
          NS_ADDREF(aName);
          result = NS_OK;
          break;
        }
      }
      attr = attr->mNext;
    }
  }
  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttributeCount(PRInt32& aCount) const
{
  aCount = mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetID(nsIAtom*& aResult) const
{
  NS_IF_ADDREF(mID);
  aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
    classList = classList->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::HasClass(nsIAtom* aClass) const
{
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    if (classList->mAtom == aClass) {
      return NS_OK;
    }
    classList = classList->mNext;
  }
  return NS_COMFALSE;
}

NS_IMETHODIMP
HTMLAttributesImpl::AddContentRef(void)
{
  ++mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::ReleaseContentRef(void)
{
  --mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetContentRefCount(PRInt32& aCount) const
{
  aCount = mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::Clone(nsIHTMLAttributes** aInstancePtrResult) const
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLAttributesImpl* clown = new HTMLAttributesImpl(*this);

  if (nsnull == clown) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return clown->QueryInterface(kIHTMLAttributesIID, (void **) aInstancePtrResult);
}

NS_IMETHODIMP
HTMLAttributesImpl::Reset(void)
{
  mCount = 0;
  mFirst.Reset();
  HTMLAttribute*  next = mFirst.mNext;
  while (nsnull != next) {
    HTMLAttribute*  attr = next;
    next = next->mNext;
    delete attr;
  }
  mFirst.mNext = nsnull;
  NS_IF_RELEASE(mID);
  if (nsnull != mClassList) {
    delete mClassList;
    mClassList = nsnull;
  }
  mFontMapper = nsnull;
  mMapper = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetMappingFunctions(nsMapAttributesFunc aFontMapFunc, nsMapAttributesFunc aMapFunc)
{
  mFontMapper = aFontMapFunc;
  mMapper = aMapFunc;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (0 < mCount) {
    if (nsnull != mFontMapper) {
      (*mFontMapper)(this, aContext, aPresContext);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (0 < mCount) {
    NS_ASSERTION(mMapper || mFontMapper, "no mapping function");
    if (nsnull != mMapper) {
      (*mMapper)(this, aContext, aPresContext);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetStyleSheet(nsIHTMLStyleSheet* aSheet)
{ // this is not ref-counted, sheet sets to null when it goes away
  mSheet = aSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
HTMLAttributesImpl::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::List(FILE* out, PRInt32 aIndent) const
{
  const HTMLAttribute*  attr = &mFirst;
  nsAutoString buffer;

  while (nsnull != attr) {
    for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
    attr->ToString(buffer);
    fputs(buffer, out);
    fputs("\n", out);
    attr = attr->mNext;
  }
  return NS_OK;
}

extern NS_HTML nsresult
  NS_NewHTMLAttributes(nsIHTMLAttributes** aInstancePtrResult, nsIHTMLStyleSheet* aSheet,
                       nsMapAttributesFunc aFontMapFunc, nsMapAttributesFunc aMapFunc)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLAttributesImpl  *it = new HTMLAttributesImpl(aSheet, aFontMapFunc, aMapFunc);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIHTMLAttributesIID, (void **) aInstancePtrResult);
}


#if 0


marginwidth; marginheight:  px

border: px

align(DIV): left, right, center, justify
align: left, right, center, abscenter
valign: baseline, top, bottom, middle, center
align(image): abscenter, left, right, texttop, absbottom, baseline, center, bottom, top, middle, absmiddle

dir: rtl, ltr

width:  px, %
height: px, %

vspace; hspace: px

type(list): none, disc, (circle | round), square, decimal, lower-roman, upper-roman, lower-alpha, upper-alpha

href: url
target: string
src: url(string?)
color: color
face: string
suppress: bool

style: string
class: string (list)
id: string
name: string

//a
link; vlink; alink: color

//body
background: url
bgcolor: color
text: color

//frameset
bordercolor: color

//layer
background: url
bgcolor: color

//q
lang: string

//table
background: url
bgcolor: color
bordercolor: color
cellspacing: px
cellpadding: px
toppadding; leftpadding; bottompadding; rightpadding: px
cols: int

//td
background: url
bgcolor: color
nowrap: bool
colspan; rowspan: int

//tr
background: url
bgcolor: color

//colgroup
span: int

//col
repeat: int

//ul;ol
compact: bool
start: int

//li
value: int

//hr;spacer
size: px

//multicol
cols: int
gutter: px

//input
type: string
value: string

//factory methods:
GetStringData
GetBoolData
GetInvBoolData
GetPixelIntData
GetValueOrPctData
GetValueData

//tag methods
getAttribute
getIntAttribute
getKeywordAttribute
getSignedAttribute
getValueOrPctAttribute


//notes
nsIAtom->value table

#endif
