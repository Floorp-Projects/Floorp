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
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"

static NS_DEFINE_IID(kIHTMLAttributesIID, NS_IHTML_ATTRIBUTES_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);


struct HTMLAttribute {
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

  void Reset(void)
  {
    NS_IF_RELEASE(mAttribute);
    mValue.Reset();
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
      aBuffer.SetLength(0);
      aBuffer.Append("null");
    }
  }

  nsIAtom*        mAttribute;
  nsHTMLValue     mValue;
  HTMLAttribute*  mNext;
};

// ----------------


class HTMLAttributesImpl: public nsIHTMLAttributes, public nsIStyleRule {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLAttributesImpl(nsIHTMLContent* aContent);
  ~HTMLAttributesImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRBool Equals(const nsIStyleRule* aRule) const;
  virtual PRUint32 HashValue(void) const;

  virtual PRInt32 SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual PRInt32 SetAttribute(nsIAtom* aAttribute, 
                               const nsHTMLValue& aValue = nsHTMLValue::kNull);
  virtual PRInt32 UnsetAttribute(nsIAtom* aAttribute);
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const;
  virtual PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const;
  virtual PRInt32 Count(void) const;

  virtual PRInt32   SetID(nsIAtom* aID);
  virtual nsIAtom*  GetID(void) const;
  virtual PRInt32   SetClass(nsIAtom* aClass);  // XXX this will have to change for CSS2
  virtual nsIAtom*  GetClass(void) const;  // XXX this will have to change for CSS2

  virtual void MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private:
  HTMLAttributesImpl(const HTMLAttributesImpl& aCopy);
  HTMLAttributesImpl& operator=(const HTMLAttributesImpl& aCopy);
  PRBool operator==(const HTMLAttributesImpl& aCopy) const;

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIHTMLContent* mContent;
  PRInt32         mCount;
  HTMLAttribute*  mFirst;
  nsIAtom*        mID;
  nsIAtom*        mClass;
};

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


HTMLAttributesImpl::HTMLAttributesImpl(nsIHTMLContent* aContent)
  : mContent(aContent), // weak ref
    mFirst(nsnull),
    mCount(0),
    mID(nsnull),
    mClass(nsnull)
{
  NS_INIT_REFCNT();
}

HTMLAttributesImpl::~HTMLAttributesImpl(void)
{
  HTMLAttribute* next = mFirst;
  while (nsnull != next) {
    HTMLAttribute* attr = next;
    next = next->mNext;
    delete attr;
  }
  NS_IF_RELEASE(mID);
  NS_IF_RELEASE(mClass);
}

NS_IMPL_ADDREF(HTMLAttributesImpl)
NS_IMPL_RELEASE(HTMLAttributesImpl)

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
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)(nsIHTMLAttributes*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


PRBool HTMLAttributesImpl::Equals(const nsIStyleRule* aRule) const
{
  nsIHTMLAttributes* iHTMLAttr;

  if (this == aRule) {
    return PR_TRUE;
  }

  if ((nsnull != aRule) && 
      (NS_OK == ((nsIStyleRule*)aRule)->QueryInterface(kIHTMLAttributesIID, (void**) &iHTMLAttr))) {

    const HTMLAttributesImpl* other = (HTMLAttributesImpl*)iHTMLAttr;
    PRBool  result = PR_FALSE;

    if (mCount == other->mCount) {
      if ((mID == other->mID) && (mClass == other->mClass)) {
        const HTMLAttribute* attr = mFirst;
        const HTMLAttribute* otherAttr = other->mFirst;

        result = PR_TRUE;
        while (nsnull != attr) {
          if (! ((*attr) == (*otherAttr))) {
            result = PR_FALSE;
            break;
          }
          attr = attr->mNext;
          otherAttr = otherAttr->mNext;
        }
      }
    }

    NS_RELEASE(iHTMLAttr);
    return result;
  }
  return PR_FALSE;
}

PRUint32 HTMLAttributesImpl::HashValue(void) const
{
  return (PRUint32)this;
}

PRInt32 HTMLAttributesImpl::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (nsHTMLAtoms::id == aAttribute) {
    nsIAtom* id = NS_NewAtom(aValue);
    PRInt32 result = SetID (id);
    NS_RELEASE(id);
    return result;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsIAtom* classA = NS_NewAtom(aValue);
    PRInt32 result = SetClass (classA);
    NS_RELEASE(classA);
    return result;
  }

  HTMLAttribute*  last = nsnull;
  HTMLAttribute*  attr = mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      attr->mValue.Set (aValue);
      return mCount;
    }
    last = attr;
    attr = attr->mNext;
  }

  attr = new HTMLAttribute(aAttribute, aValue);
  if (nsnull == last) {
    mFirst = attr;
  }
  else {
    last->mNext = attr;
  }
  return ++mCount;
}

PRInt32 HTMLAttributesImpl::SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue)
{
  if (nsHTMLAtoms::id == aAttribute) {
    nsAutoString  buffer;
    nsIAtom* id = NS_NewAtom(aValue.GetStringValue(buffer));
    PRInt32 result = SetID (id);
    NS_RELEASE(id);
    return result;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsAutoString  buffer;
    nsIAtom* classA = NS_NewAtom(aValue.GetStringValue(buffer));
    PRInt32 result = SetClass (classA);
    NS_RELEASE(classA);
    return result;
  }

  HTMLAttribute*  last = nsnull;
  HTMLAttribute*  attr = mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      attr->mValue = aValue;
      return mCount;
    }
    last = attr;
    attr = attr->mNext;
  }

  attr = new HTMLAttribute(aAttribute, aValue);
  if (nsnull == last) {
    mFirst = attr;
  }
  else {
    last->mNext = attr;
  }
  return ++mCount;
}

PRInt32 HTMLAttributesImpl::UnsetAttribute(nsIAtom* aAttribute)
{
  if (nsHTMLAtoms::id == aAttribute) {
    return SetID (nsnull);
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    return SetClass (nsnull);
  }

  HTMLAttribute*  prev = nsnull;
  HTMLAttribute*  attr = mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      if (nsnull == prev) {
        mFirst = attr->mNext;
      }
      else {
        prev->mNext = attr->mNext;
      }
      delete attr;
      mCount--;
      break;
    }
    prev = attr;
    attr = attr->mNext;
  }
  return mCount;
}

nsContentAttr HTMLAttributesImpl::GetAttribute(nsIAtom* aAttribute,
                                               nsHTMLValue& aValue) const
{
  aValue.Reset();
  if (nsHTMLAtoms::id == aAttribute) {
    nsIAtom* id = GetID();
    if (nsnull != id) {
      nsAutoString buffer;
      id->ToString(buffer);
      aValue.Set(buffer);
      NS_RELEASE(id);
      return eContentAttr_HasValue;
    }
    return eContentAttr_NotThere;
  }
  if (nsHTMLAtoms::kClass == aAttribute) {
    nsIAtom* classA = GetClass();
    if (nsnull != classA) {
      nsAutoString buffer;
      classA->ToString(buffer);
      aValue.Set(buffer);
      NS_RELEASE(classA);
      return eContentAttr_HasValue;
    }
    return eContentAttr_NotThere;
  }

  HTMLAttribute*  attr = mFirst;

  while (nsnull != attr) {
    if (attr->mAttribute == aAttribute) {
      aValue = attr->mValue;
      return ((attr->mValue.GetUnit() == eHTMLUnit_Null) ? eContentAttr_NoValue : eContentAttr_HasValue);
    }
    attr = attr->mNext;
  }
  return eContentAttr_NotThere;
}

PRInt32 HTMLAttributesImpl::GetAllAttributeNames(nsISupportsArray* aArray) const
{
  NS_ASSERTION(nsnull != aArray, "null arg");

  if (nsnull == aArray) {
    return 0;
  }

  if (nsnull != mID) {
    aArray->AppendElement((nsIAtom*)nsHTMLAtoms::id);
  }
  if (nsnull != mClass) {
    aArray->AppendElement((nsIAtom*)nsHTMLAtoms::kClass);
  }

  HTMLAttribute*  attr = mFirst;

  while (nsnull != attr) {
    aArray->AppendElement(attr->mAttribute);
    attr = attr->mNext;
  }
  return mCount;
}

PRInt32 HTMLAttributesImpl::Count(void) const
{
  return mCount;
}

PRInt32 HTMLAttributesImpl::SetID(nsIAtom* aID)
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
  return mCount;
}

nsIAtom* HTMLAttributesImpl::GetID(void) const
{
  NS_IF_ADDREF(mID);
  return mID;
}

PRInt32 HTMLAttributesImpl::SetClass(nsIAtom* aClass)
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
  return mCount;
}

nsIAtom* HTMLAttributesImpl::GetClass(void) const
{
  NS_IF_ADDREF(mClass);
  return mClass;
}

void HTMLAttributesImpl::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (nsnull != mContent) {
    mContent->MapAttributesInto(aContext, aPresContext);
  }
}


void HTMLAttributesImpl::List(FILE* out, PRInt32 aIndent) const
{
  HTMLAttribute*  attr = mFirst;
  nsString buffer;

  while (nsnull != attr) {
    fputs("\n", out);
    for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
    attr->ToString(buffer);
    fputs(buffer, out);
    attr = attr->mNext;
  }
}

extern NS_HTML nsresult
  NS_NewHTMLAttributes(nsIHTMLAttributes** aInstancePtrResult, nsIHTMLContent* aContent)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLAttributesImpl  *it = new HTMLAttributesImpl(aContent);

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
