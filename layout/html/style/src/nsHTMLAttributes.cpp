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
#include "nsISizeOfHandler.h"

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

  void SizeOf(nsISizeOfHandler* aHandler) const;

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

void
HTMLAttribute::SizeOf(nsISizeOfHandler* aHandler) const
{
  if (!aHandler->HaveSeen(mAttribute)) {
    mAttribute->SizeOf(aHandler);
  }
  aHandler->Add(sizeof(*this));
  aHandler->Add((size_t) (- ((PRInt32) sizeof(mValue)) ) );
  mValue.SizeOf(aHandler);
  if (!aHandler->HaveSeen(mNext)) {
    mNext->SizeOf(aHandler);
  }
}

// ----------------

class HTMLAttributesImpl: public nsIHTMLAttributes, public nsIStyleRule {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLAttributesImpl(nsIHTMLStyleSheet* aSheet, nsMapAttributesFunc aMapFunc);
  HTMLAttributesImpl(const HTMLAttributesImpl& aCopy);
  ~HTMLAttributesImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIHTMLAttributes
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRInt32& aCount);
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                          PRInt32& aCount);
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute, PRInt32& aCount);

  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aValue) const;

  NS_IMETHOD GetAllAttributeNames(nsISupportsArray* aArray,
                                  PRInt32& aCount) const;

  NS_IMETHOD Count(PRInt32& aCount) const;
  NS_IMETHOD Equals(const nsIHTMLAttributes* aAttributes, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD SetID(nsIAtom* aID, PRInt32& aCount);
  NS_IMETHOD GetID(nsIAtom*& aResult) const;

  NS_IMETHOD SetClass(nsIAtom* aClass, PRInt32& aCount);  // XXX this will have to change for CSS2
  NS_IMETHOD GetClass(nsIAtom*& aResult) const;  // XXX this will have to change for CSS2

  NS_IMETHOD AddContentRef(void);
  NS_IMETHOD ReleaseContentRef(void);
  NS_IMETHOD GetContentRefCount(PRInt32& aCount);

  NS_IMETHOD Clone(nsIHTMLAttributes** aInstancePtrResult);
  NS_IMETHOD Reset(void);
  NS_IMETHOD SetMappingFunction(nsMapAttributesFunc aMapFunc);

  // nsIStyleRule 
  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsIHTMLStyleSheet* aSheet);
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength);
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  /**
   * Add this object's size information to the sizeof handler.
   */
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

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
  nsIAtom*            mClass;
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
      ::delete ptr;
    }
  }
}


HTMLAttributesImpl::HTMLAttributesImpl(nsIHTMLStyleSheet* aSheet, 
                                       nsMapAttributesFunc aMapFunc)
  : mSheet(aSheet),
    mFirst(),
    mCount(0),
    mID(nsnull),
    mClass(nsnull),
    mContentRefCount(0),
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
    mClass(aCopy.mClass),
    mContentRefCount(0),
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
  NS_IF_ADDREF(mClass);

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
      if ((mID == other->mID) && (mClass == other->mClass)) {
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
  aValue = PRUint32(mID) ^ PRUint32(mClass);

  const HTMLAttribute* attr = &mFirst;
  while (nsnull != attr) {
    if (nsnull != attr->mAttribute) {
      aValue = aValue ^ attr->HashValue();
    }
    attr = attr->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                                 PRInt32& aCount)
{
  if (nsHTMLAtoms::id == aAttribute) {
    nsIAtom* id = NS_NewAtom(aValue);
    nsresult rv = SetID(id, aCount);
    NS_RELEASE(id);
    return rv;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsIAtom* classA = NS_NewAtom(aValue);
    nsresult rv = SetClass(classA, aCount);
    NS_RELEASE(classA);
    return rv;
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
    nsAutoString  buffer;
    nsIAtom* id = NS_NewAtom(aValue.GetStringValue(buffer));
    nsresult rv = SetID(id, aCount);
    NS_RELEASE(id);
    return rv;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsAutoString  buffer;
    nsIAtom* classA = NS_NewAtom(aValue.GetStringValue(buffer));
    nsresult rv = SetClass(classA, aCount);
    NS_RELEASE(classA);
    return rv;
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
    return SetID (nsnull, aCount);
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    return SetClass (nsnull, aCount);
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
  if (nsHTMLAtoms::id == aAttribute) {
    nsIAtom* id;
    GetID(id);
    if (nsnull != id) {
      nsAutoString buffer;
      id->ToString(buffer);
      aValue.SetStringValue(buffer);
      NS_RELEASE(id);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsIAtom* classA;
    GetClass(classA);
    if (nsnull != classA) {
      nsAutoString buffer;
      classA->ToString(buffer);
      aValue.SetStringValue(buffer);
      NS_RELEASE(classA);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    return NS_CONTENT_ATTR_NOT_THERE;
  }

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
HTMLAttributesImpl::GetAllAttributeNames(nsISupportsArray* aArray,
                                         PRInt32& aCount) const
{
  NS_ASSERTION(nsnull != aArray, "null arg");

  if (nsnull == aArray) {
    aCount = 0;
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull != mID) {
    aArray->AppendElement((nsIAtom*)nsHTMLAtoms::id);
  }
  if (nsnull != mClass) {
    aArray->AppendElement((nsIAtom*)nsHTMLAtoms::kClass);
  }

  const HTMLAttribute*  attr = &mFirst;

  while (nsnull != attr) {
    if (nsnull != attr->mAttribute) {
      aArray->AppendElement(attr->mAttribute);
    }
    attr = attr->mNext;
  }
  aCount = mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::Count(PRInt32& aCount) const
{
  aCount = mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetID(nsIAtom* aID, PRInt32& aCount)
{
  if (aID != mID) {
    if (nsnull != mID) {
      NS_RELEASE(mID);
      mCount--;
    }
    mID = aID;
    if (nsnull != mID) {
      NS_ADDREF(mID);
      mCount++;
    }
  }
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
HTMLAttributesImpl::SetClass(nsIAtom* aClass, PRInt32& aCount)
{
  if (aClass != mClass) {
    if (nsnull != mClass) {
      NS_RELEASE(mClass);
      mCount--;
    }
    mClass = aClass;
    if (nsnull != mClass) {
      NS_ADDREF(mClass);
      mCount++;
    }
  }
  aCount = mCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetClass(nsIAtom*& aResult) const
{
  NS_IF_ADDREF(mClass);
  aResult = mClass;
  return NS_OK;
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
HTMLAttributesImpl::GetContentRefCount(PRInt32& aCount)
{
  aCount = mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::Clone(nsIHTMLAttributes** aInstancePtrResult)
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
  NS_IF_RELEASE(mClass);
  mMapper = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetMappingFunction(nsMapAttributesFunc aMapFunc)
{
  mMapper = aMapFunc;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  NS_ASSERTION(nsnull != mMapper, "no mapping function");
  if (nsnull != mMapper) {
    (*mMapper)(this, aContext, aPresContext);
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
HTMLAttributesImpl::GetStrength(PRInt32& aStrength)
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  // XXX mID
  // XXX mClass
  if (!aHandler->HaveSeen(mFirst.mNext)) {
    mFirst.mNext->SizeOf(aHandler);
  }
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
                       nsMapAttributesFunc aMapFunc)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLAttributesImpl  *it = new HTMLAttributesImpl(aSheet, aMapFunc);

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
