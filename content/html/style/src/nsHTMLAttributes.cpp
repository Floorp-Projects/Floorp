/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsVoidArray.h"
#include "nsISizeOfHandler.h"
#include "nsCOMPtr.h"

#include "nsIStyleSet.h"
#include "nsRuleWalker.h"

MOZ_DECL_CTOR_COUNTER(HTMLAttribute)

struct HTMLAttribute {
  HTMLAttribute(void)
    : mAttribute(nsnull),
      mValue(),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
  }

  HTMLAttribute(nsIAtom* aAttribute, const nsAReadableString& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    NS_IF_ADDREF(mAttribute);
  }

  HTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    NS_IF_ADDREF(mAttribute);
  }

  HTMLAttribute(const HTMLAttribute& aCopy)
    : mAttribute(aCopy.mAttribute),
      mValue(aCopy.mValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    NS_IF_ADDREF(mAttribute);
  }

  ~HTMLAttribute(void)
  {
    MOZ_COUNT_DTOR(HTMLAttribute);
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
    return NS_PTR_TO_INT32(mAttribute) ^ mValue.HashValue();
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

  void Set(nsIAtom* aAttribute, const nsAReadableString& aValue)
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
        aBuffer.AppendWithConversion(" = ");
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.AppendWithConversion("null");
    }
  }

  void ToString(nsString& aBuffer) const
  {
    if (nsnull != mAttribute) {
      mAttribute->ToString(aBuffer);
      if (eHTMLUnit_Null != mValue.GetUnit()) {
        aBuffer.AppendWithConversion(" = ");
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.AssignWithConversion("null");
    }
  }

  static void
  CopyHTMLAttributes(HTMLAttribute* aSource, HTMLAttribute** aDest)
  {
    while (aSource && aDest) {
      (*aDest) = new HTMLAttribute(*aSource);
      if (! *aDest) {
        break;
      }
      aDest = &((*aDest)->mNext);
      aSource = aSource->mNext;
    }
  }

  static void
  DeleteHTMLAttributes(HTMLAttribute* aAttr)
  {
    while (aAttr) {
      HTMLAttribute* deadBeef = aAttr;
      aAttr = aAttr->mNext;
      delete deadBeef;
    }
  }

  static HTMLAttribute*
  FindHTMLAttribute(nsIAtom* aAttrName, HTMLAttribute* aAttr)
  {
    while (aAttr) {
      if (aAttrName == aAttr->mAttribute) {
        return aAttr;
      }
      aAttr = aAttr->mNext;
    }
    return nsnull;
  }

  static const HTMLAttribute*
  FindHTMLAttribute(nsIAtom* aAttrName, const HTMLAttribute* aAttr)
  {
    while (aAttr) {
      if (aAttrName == aAttr->mAttribute) {
        return aAttr;
      }
      aAttr = aAttr->mNext;
    }
    return nsnull;
  }

  static PRBool
  RemoveHTMLAttribute(nsIAtom* aAttrName, HTMLAttribute** aAttr)
  {
    while (*aAttr) {
      if ((*aAttr)->mAttribute == aAttrName) {
        HTMLAttribute*  attr = *aAttr;
        *aAttr = (*aAttr)->mNext;
        delete attr;
        return PR_TRUE;
      }
      aAttr = &((*aAttr)->mNext);
    }
    return PR_FALSE;
  }

#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    if (!aResult) {
      return NS_ERROR_NULL_POINTER;
    }
    *aResult = sizeof(*this);
    return NS_OK;
  }
#endif

  nsIAtom*        mAttribute;
  nsHTMLValue     mValue;
  HTMLAttribute*  mNext;
};

// ----------------

MOZ_DECL_CTOR_COUNTER(nsClassList)

struct nsClassList {
  nsClassList(nsIAtom* aAtom)
    : mAtom(aAtom), // take ref
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(nsClassList);
  }

  nsClassList(const nsClassList& aCopy)
    : mAtom(aCopy.mAtom),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(nsClassList);
    NS_IF_ADDREF(mAtom);
    if (aCopy.mNext) {
      mNext = new nsClassList(*(aCopy.mNext));
    }
  }

  ~nsClassList(void)
  {
    MOZ_COUNT_DTOR(nsClassList);
    Reset();
  }

  void Reset(void)
  {
    NS_IF_RELEASE(mAtom);
    if (mNext) {
      delete mNext;
      mNext = nsnull;
    }
  }

  nsIAtom*      mAtom;
  nsClassList*  mNext;
};

// ----------------

class nsHTMLMappedAttributes : public nsIHTMLMappedAttributes, public nsIStyleRule {
public:
  nsHTMLMappedAttributes(void);
  nsHTMLMappedAttributes(const nsHTMLMappedAttributes& aCopy);
  virtual ~nsHTMLMappedAttributes(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIHTMLStyleSheet* aSheet,
                  nsMapRuleToAttributesFunc aMapRuleFunc);
  NS_IMETHOD Clone(nsHTMLMappedAttributes** aInstancePtrResult) const;
  NS_IMETHOD Reset(void);
  NS_IMETHOD SetMappingFunction(nsMapRuleToAttributesFunc aMapRuleFunc);

  NS_IMETHOD SetAttribute(nsIAtom* aAttrName, const nsAReadableString& aValue);
  NS_IMETHOD SetAttribute(nsIAtom* aAttrName, const nsHTMLValue& aValue);
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttrName, PRInt32& aAttrCount);

  NS_IMETHOD GetAttribute(nsIAtom* aAttrName,
                          nsHTMLValue& aValue) const;
  NS_IMETHOD GetAttribute(nsIAtom* aAttrName,
                          const nsHTMLValue** aValue) const;

  NS_IMETHOD GetAttributeCount(PRInt32& aCount) const;

  NS_IMETHOD Equals(const nsIHTMLMappedAttributes* aAttributes, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD AddUse(void);
  NS_IMETHOD ReleaseUse(void);
  NS_IMETHOD GetUseCount(PRInt32& aUseCount) const;

  NS_IMETHOD SetUniqued(PRBool aUniqued);
  NS_IMETHOD GetUniqued(PRBool& aUniqued);
  NS_IMETHOD DropStyleSheetReference(void);

  // nsIStyleRule 
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD SetStyleSheet(nsIHTMLStyleSheet* aSheet);
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;
  
  // The new mapping functions.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

  void SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult);
#endif

  nsIHTMLStyleSheet*  mSheet;
  PRInt32             mUseCount;
  PRInt32             mAttrCount;
  HTMLAttribute       mFirst;
  nsMapRuleToAttributesFunc mRuleMapper;
  PRBool              mUniqued;
};

nsHTMLMappedAttributes::nsHTMLMappedAttributes(void)
  : mSheet(nsnull),
    mUseCount(0),
    mAttrCount(0),
    mFirst(),
    mRuleMapper(nsnull),
    mUniqued(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsHTMLMappedAttributes::nsHTMLMappedAttributes(const nsHTMLMappedAttributes& aCopy)
  : mSheet(aCopy.mSheet),
    mUseCount(0),
    mAttrCount(aCopy.mAttrCount),
    mFirst(aCopy.mFirst),
    mRuleMapper(aCopy.mRuleMapper),
    mUniqued(PR_FALSE)
{
  NS_INIT_ISUPPORTS();

  HTMLAttribute::CopyHTMLAttributes(aCopy.mFirst.mNext, &(mFirst.mNext));
}

nsHTMLMappedAttributes::~nsHTMLMappedAttributes(void)
{
  Reset();
}

NS_IMPL_ADDREF(nsHTMLMappedAttributes);
NS_IMPL_RELEASE(nsHTMLMappedAttributes);

nsresult 
nsHTMLMappedAttributes::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(NS_GET_IID(nsIHTMLMappedAttributes))) {
    *aInstancePtrResult = (void*) ((nsIHTMLMappedAttributes*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleRule))) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsIHTMLMappedAttributes*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP
nsHTMLMappedAttributes::Init(nsIHTMLStyleSheet* aSheet,
                             nsMapRuleToAttributesFunc aMapRuleFunc)
{
  mSheet = aSheet;
  mRuleMapper = aMapRuleFunc;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::Clone(nsHTMLMappedAttributes** aInstancePtrResult) const
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  nsHTMLMappedAttributes* clone = new nsHTMLMappedAttributes(*this);

  if (! clone) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(clone);
  *aInstancePtrResult = clone;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::Reset(void)
{
  mAttrCount = 0;
  mFirst.Reset();
  HTMLAttribute::DeleteHTMLAttributes(mFirst.mNext);
  mUniqued = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::SetMappingFunction(nsMapRuleToAttributesFunc aMapRuleFunc)
{
  mRuleMapper = aMapRuleFunc;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::SetAttribute(nsIAtom* aAttrName, const nsAReadableString& aValue)
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mFirst.mAttribute) {  // do we already have any?
    HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
    if (attr) {
      attr->mValue.SetStringValue(aValue);
    }
    else {
      // add new attribute
      // keep these arbitrarily sorted so they'll equal regardless of set order
      if (aAttrName < mFirst.mAttribute) {  // before first, move first down one
        attr = new HTMLAttribute(mFirst);
        if (attr) {
          attr->mNext = mFirst.mNext;
          mFirst.mNext = attr;
          mFirst.Set(aAttrName, aValue);
        }
        else {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        attr = new HTMLAttribute(aAttrName, aValue);
        if (attr) {
          HTMLAttribute* prev = &mFirst;
          while (prev->mNext && (prev->mNext->mAttribute < aAttrName)) {
            prev = prev->mNext;
          }
          attr->mNext = prev->mNext;
          prev->mNext = attr;
        }
        else {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mAttrCount++;
    }
  }
  else {
    mFirst.Set(aAttrName, aValue);
    mAttrCount++;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::SetAttribute(nsIAtom* aAttrName, const nsHTMLValue& aValue)
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mFirst.mAttribute) {  // do we already have any?
    HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
    if (attr) {
      attr->mValue = aValue;
    }
    else {
      // add new attribute
      // keep these arbitrarily sorted so they'll hash regardless of set order
      if (aAttrName < mFirst.mAttribute) {  // before first, move first down one
        attr = new HTMLAttribute(mFirst);
        if (attr) {
          attr->mNext = mFirst.mNext;
          mFirst.mNext = attr;
          mFirst.Set(aAttrName, aValue);
        }
        else {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        attr = new HTMLAttribute(aAttrName, aValue);
        if (attr) {
          HTMLAttribute* prev = &mFirst;
          while (prev->mNext && (prev->mNext->mAttribute < aAttrName)) {
            prev = prev->mNext;
          }
          attr->mNext = prev->mNext;
          prev->mNext = attr;
        }
        else {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mAttrCount++;
    }
  }
  else {
    mFirst.Set(aAttrName, aValue);
    mAttrCount++;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::UnsetAttribute(nsIAtom* aAttrName, PRInt32& aAttrCount)
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aAttrName == mFirst.mAttribute) {
    if (mFirst.mNext) {
      HTMLAttribute*  attr = mFirst.mNext;
      mFirst = *attr;
      mFirst.mNext = attr->mNext;
      delete attr;
    }
    else {
      mFirst.Reset();
    }
    mAttrCount--;
  }
  else {
    if (HTMLAttribute::RemoveHTMLAttribute(aAttrName, &(mFirst.mNext))) {
      mAttrCount--;
    }
  }
  aAttrCount = mAttrCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetAttribute(nsIAtom* aAttrName,
                                     nsHTMLValue& aValue) const
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  const HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
  if (attr) {
    aValue = attr->mValue;
    return ((attr->mValue.GetUnit() == eHTMLUnit_Null) ?
            NS_CONTENT_ATTR_NO_VALUE :
            NS_CONTENT_ATTR_HAS_VALUE);
  }
  aValue.Reset();
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetAttribute(nsIAtom* aAttrName,
                                     const nsHTMLValue** aValue) const
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  const HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
  if (attr) {
    *aValue = &attr->mValue;
    return ((attr->mValue.GetUnit() == eHTMLUnit_Null) ?
            NS_CONTENT_ATTR_NO_VALUE :
            NS_CONTENT_ATTR_HAS_VALUE);
  }
  *aValue = nsnull;
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetAttributeCount(PRInt32& aCount) const
{
  aCount = mAttrCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::Equals(const nsIHTMLMappedAttributes* aOther, PRBool& aResult) const
{
  const nsHTMLMappedAttributes* other = (const nsHTMLMappedAttributes*)aOther;

  if (this == other) {
    aResult = PR_TRUE;
  }
  else {
    aResult = PR_FALSE;
    if ((mRuleMapper == other->mRuleMapper) &&
        (mAttrCount == other->mAttrCount)) {
      const HTMLAttribute* attr = &mFirst;
      const HTMLAttribute* otherAttr = &(other->mFirst);

      aResult = PR_TRUE;
      while (attr) {
        if (! ((*attr) == (*otherAttr))) {
          aResult = PR_FALSE;
          break;
        }
        attr = attr->mNext;
        otherAttr = otherAttr->mNext;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::HashValue(PRUint32& aValue) const
{
  aValue = NS_PTR_TO_INT32(mRuleMapper);
  
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
nsHTMLMappedAttributes::AddUse(void)
{
  mUseCount++;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::ReleaseUse(void)
{
  mUseCount--;

  if (mSheet && (0 == mUseCount) && mUniqued) {
    mSheet->DropMappedAttributes(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetUseCount(PRInt32& aUseCount) const
{
  aUseCount = mUseCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::SetUniqued(PRBool aUniqued)
{
  mUniqued = aUniqued;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetUniqued(PRBool& aUniqued)
{
  aUniqued = mUniqued;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::DropStyleSheetReference(void)
{
  mSheet = nsnull;
  mUniqued = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLMappedAttributes::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  aSheet = mSheet;
  NS_IF_ADDREF(aSheet);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::SetStyleSheet(nsIHTMLStyleSheet* aSheet)
{
  if (mSheet && mUniqued) {
    mSheet->DropMappedAttributes(this);
  }
  mSheet = aSheet;  // not ref counted
  mUniqued = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMappedAttributes::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (0 < mAttrCount) {
    if (mRuleMapper) {
      (*mRuleMapper)(this, aRuleData);
    }
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLMappedAttributes::List(FILE* out, PRInt32 aIndent) const
{
  const HTMLAttribute*  attr = &mFirst;
  nsAutoString buffer;

  while (nsnull != attr) {
    PRInt32 index;
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
    attr->ToString(buffer);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
    fputs("\n", out);
    attr = attr->mNext;
  }
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as nsHTMLMappedAttributes's size): 
*    1) sizeof(*this)
*
*  Contained / Aggregated data (not reported as nsHTMLMappedAttributes's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void nsHTMLMappedAttributes::SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult)
{
  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    // this is already accounted for
    return;
  }
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("HTMLMappedAttributes"));
  aResult = sizeof(*this);
  aSizer->AddSize(tag, aResult);
}
#endif

//--------------------

const PRInt32 kNameBufferSize = 4;

class HTMLAttributesImpl: public nsIHTMLAttributes {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLAttributesImpl(void);
  HTMLAttributesImpl(const HTMLAttributesImpl& aCopy);
  virtual ~HTMLAttributesImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIHTMLAttributes
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttrName, const nsHTMLValue& aValue,
                             PRBool aMappedToStyle, 
                             nsIHTMLContent* aContent,
                             nsIHTMLStyleSheet* aSheet,
                             PRInt32& aAttrCount);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttrName, const nsAReadableString& aValue,
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent,
                             nsIHTMLStyleSheet* aSheet);
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttrName, 
                               nsIHTMLContent* aContent,
                               nsIHTMLStyleSheet* aSheet,
                               PRInt32& aAttrCount);

  NS_IMETHOD GetAttribute(nsIAtom* aAttrName,
                          nsHTMLValue& aValue) const;
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          const nsHTMLValue** aValue) const;

  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                nsIAtom*& aName) const;

  NS_IMETHOD GetAttributeCount(PRInt32& aCount) const;

  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;

  NS_IMETHOD Clone(nsIHTMLAttributes** aInstancePtrResult) const;

  NS_IMETHOD SetStyleSheet(nsIHTMLStyleSheet* aSheet);

  NS_IMETHOD WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker) const;

#ifdef UNIQUE_ATTR_SUPPORT
  NS_IMETHOD AddContentRef(void);
  NS_IMETHOD ReleaseContentRef(void);
  NS_IMETHOD GetContentRefCount(PRInt32& aCount) const;
#endif
  NS_IMETHOD Reset(void);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  void SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult);
#endif

protected:
  virtual nsresult SetAttributeName(nsIAtom* aAttrName, PRBool& aFound);
  virtual nsresult UnsetAttributeName(nsIAtom* aAttrName, PRBool& aFound);
  virtual nsresult EnsureSingleMappedFor(nsIHTMLContent* aContent, 
                                         nsIHTMLStyleSheet* aSheet,
                                         PRBool aCreate);
  virtual nsresult UniqueMapped(nsIHTMLStyleSheet* aSheet);

private:
  HTMLAttributesImpl& operator=(const HTMLAttributesImpl& aCopy);
  PRBool operator==(const HTMLAttributesImpl& aCopy) const;

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  NS_DECL_OWNINGTHREAD // for thread-safety checking

  nsIAtom**               mAttrNames;
  PRInt32                 mAttrCount;
  PRInt32                 mAttrSize;
  HTMLAttribute*          mFirstUnmapped;
  nsHTMLMappedAttributes* mMapped;
  nsIAtom*                mID;
  nsClassList             mFirstClass;

  nsIAtom*                mNameBuffer[kNameBufferSize];
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
      ::operator delete(ptr);
    }
  }
}

HTMLAttributesImpl::HTMLAttributesImpl(void)
  : mAttrNames(mNameBuffer),
    mAttrCount(0),
    mAttrSize(kNameBufferSize),
    mFirstUnmapped(nsnull),
    mMapped(nsnull),
    mID(nsnull),
    mFirstClass(nsnull)
{
  NS_INIT_REFCNT();
}

HTMLAttributesImpl::HTMLAttributesImpl(const HTMLAttributesImpl& aCopy)
  : mAttrNames(mNameBuffer),
    mAttrCount(aCopy.mAttrCount),
    mAttrSize(kNameBufferSize),
    mFirstUnmapped(nsnull),
    mMapped(aCopy.mMapped),
    mID(aCopy.mID),
    mFirstClass(aCopy.mFirstClass)
{
  NS_INIT_REFCNT();

  if (mAttrCount) {
    if (mAttrSize < mAttrCount) {
      mAttrNames = new nsIAtom*[mAttrCount];
      if (mAttrNames) {
        mAttrSize = mAttrCount;
      }
      else {  // new buffer failed, deal with it
        mAttrNames = mNameBuffer;
        mAttrCount = 0;
      }
    }
    PRInt32 index = mAttrCount;
    while (0 < index--) {
      mAttrNames[index] = aCopy.mAttrNames[index];
      NS_ADDREF(mAttrNames[index]);
    }
  }

  HTMLAttribute::CopyHTMLAttributes(aCopy.mFirstUnmapped, &mFirstUnmapped);
  if (mMapped) {
    mMapped->AddUse();
    NS_ADDREF(mMapped);
  }

  NS_IF_ADDREF(mID);
}

HTMLAttributesImpl::~HTMLAttributesImpl(void)
{
  Reset();
}

NS_IMPL_ISUPPORTS1(HTMLAttributesImpl, nsIHTMLAttributes)

const PRUnichar kNullCh = PRUnichar('\0');

static void ParseClasses(const nsAReadableString& aClassString, nsClassList& aClassList)
{
  nsAutoString  classStr(aClassString);  // copy to work buffer
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr.get();
  PRUnichar* end   = start;

  nsClassList*  list = &aClassList;
  while (list && (kNullCh != *start)) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      if (! list->mAtom) {
        list->mAtom = NS_NewAtom(start);
      }
      else {
        list->mNext = new nsClassList(NS_NewAtom(start));
        list = list->mNext;
      }
    }

    start = ++end;
  }
}

nsresult
HTMLAttributesImpl::SetAttributeName(nsIAtom* aAttrName, PRBool& aFound)
{
  PRInt32 index = mAttrCount;
  while (0 < index--) {
    if (mAttrNames[index] == aAttrName) {
      aFound = PR_TRUE;
      return NS_OK;
    }
  }
  aFound = PR_FALSE;
  if (mAttrCount == mAttrSize) {  // no more room, grow buffer
    nsIAtom** buffer = new nsIAtom*[mAttrSize + 4]; 
    if (buffer) {
      nsCRT::memcpy(buffer, mAttrNames, sizeof(nsIAtom*) * mAttrCount);
      mAttrSize += 4;
      if (mAttrNames != mNameBuffer) {
        delete [] mAttrNames;
      }
      mAttrNames = buffer;
    }
    else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  mAttrNames[mAttrCount++] = aAttrName;
  NS_ADDREF(aAttrName);
  return NS_OK;
}

nsresult
HTMLAttributesImpl::UnsetAttributeName(nsIAtom* aAttrName, PRBool& aFound)
{
  PRInt32 index = mAttrCount;
  while (0 < index--) {
    if (mAttrNames[index] == aAttrName) {
      mAttrCount--;
      if ((mAttrNames != mNameBuffer) && (mAttrCount <= (kNameBufferSize / 2))) {
        // go back to using internal buffer
        if (0 < index) {
          nsCRT::memcpy(mNameBuffer, mAttrNames, index * sizeof(nsIAtom*));
        }
        if (index < mAttrCount) {
          nsCRT::memcpy(&(mNameBuffer[index]), &(mAttrNames[index + 1]), 
                        (mAttrCount - index) * sizeof(nsIAtom*));
        }
        delete [] mAttrNames;
        mAttrNames = mNameBuffer;
        mAttrSize = kNameBufferSize;
      }
      else {
        if (index < mAttrCount) {
          nsCRT::memmove(&(mAttrNames[index]), &(mAttrNames[index + 1]), 
                         (mAttrCount - index) * sizeof(nsIAtom*));
        }
      }
      NS_RELEASE(aAttrName);
      aFound = PR_TRUE;
      return NS_OK;
    }
  }
  aFound = PR_FALSE;
  return NS_OK;
}

nsresult
HTMLAttributesImpl::EnsureSingleMappedFor(nsIHTMLContent* aContent,
                                          nsIHTMLStyleSheet* aSheet,
                                          PRBool aCreate)
{
  nsresult result = NS_OK;
  if (mMapped) {
    nsHTMLMappedAttributes* single;
    result = mMapped->Clone(&single);
    if (NS_SUCCEEDED(result)) {
      mMapped->ReleaseUse();
      NS_RELEASE(mMapped);
      mMapped = single;
      mMapped->AddUse();
    }
  }
  else if (aCreate) {  // create one
    mMapped = new nsHTMLMappedAttributes();
    if (mMapped) {
      NS_ADDREF(mMapped);
      mMapped->AddUse();
      if (aContent) {
        nsMapRuleToAttributesFunc mapRuleFunc;
        aContent->GetAttributeMappingFunction(mapRuleFunc);
        result = mMapped->Init(aSheet, mapRuleFunc);
      }
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return result;
}

nsresult
HTMLAttributesImpl::UniqueMapped(nsIHTMLStyleSheet* aSheet)
{
  nsresult result = NS_OK;

  if (aSheet) {
    nsIHTMLMappedAttributes*  mapped;
    result = aSheet->UniqueMappedAttributes(mMapped, mapped);
    if (NS_SUCCEEDED(result)) {
      if (mapped != mMapped) {
        mMapped->ReleaseUse();
        NS_RELEASE(mMapped);
        mMapped = (nsHTMLMappedAttributes*)mapped; // take ref
        mMapped->AddUse();
      }
      else {
        NS_RELEASE(mapped);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetAttributeFor(nsIAtom* aAttrName, const nsAReadableString& aValue,
                                    PRBool aMappedToStyle, nsIHTMLContent* aContent,
                                    nsIHTMLStyleSheet* aSheet)
{
  nsresult result = NS_OK;

  if (nsHTMLAtoms::id == aAttrName) {
    NS_IF_RELEASE(mID);
    mID = NS_NewAtom(aValue);
  }
  else if (nsHTMLAtoms::kClass == aAttrName) {
    mFirstClass.Reset();
    ParseClasses(aValue, mFirstClass);
  }

  PRBool  haveAttr;
  result = SetAttributeName(aAttrName, haveAttr);
  if (NS_SUCCEEDED(result)) {
    if (aMappedToStyle) {
      result = EnsureSingleMappedFor(aContent, aSheet, PR_TRUE);
      if (mMapped) {
        result = mMapped->SetAttribute(aAttrName, aValue);
        UniqueMapped(aSheet);
      }
    }
    else {
      if (haveAttr) {
        HTMLAttribute*  attr = HTMLAttribute::FindHTMLAttribute(aAttrName, mFirstUnmapped);
        NS_ASSERTION(attr, "failed to find attribute");
        if (attr) {
          attr->mValue.SetStringValue(aValue);
        }
      }
      else {
        HTMLAttribute*  attr = new HTMLAttribute(aAttrName, aValue);
        if (attr) {
          attr->mNext = mFirstUnmapped;
          mFirstUnmapped = attr;
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::SetAttributeFor(nsIAtom* aAttrName,
                                    const nsHTMLValue& aValue,
                                    PRBool aMappedToStyle,
                                    nsIHTMLContent* aContent,
                                    nsIHTMLStyleSheet* aSheet,
                                    PRInt32& aCount)
{
  nsresult result = NS_OK;

  if (eHTMLUnit_Null == aValue.GetUnit()) {
    return UnsetAttributeFor(aAttrName, aContent, aSheet, aCount);
  }
  
  if (nsHTMLAtoms::id == aAttrName) {
    NS_IF_RELEASE(mID);
    if (eHTMLUnit_String == aValue.GetUnit()) {
      nsAutoString  buffer;
      mID = NS_NewAtom(aValue.GetStringValue(buffer));
    }
  }
  else if (nsHTMLAtoms::kClass == aAttrName) {
    mFirstClass.Reset();
    if (eHTMLUnit_String == aValue.GetUnit()) {
      nsAutoString  buffer;
      aValue.GetStringValue(buffer);
      ParseClasses(buffer, mFirstClass);
    }
  }

  PRBool  haveAttr;
  result = SetAttributeName(aAttrName, haveAttr);
  if (NS_SUCCEEDED(result)) {
    if (aMappedToStyle) {
      result = EnsureSingleMappedFor(aContent, aSheet, PR_TRUE);
      if (mMapped) {
        result = mMapped->SetAttribute(aAttrName, aValue);
        UniqueMapped(aSheet);
      }
    }
    else {
      if (haveAttr) {
        HTMLAttribute*  attr = HTMLAttribute::FindHTMLAttribute(aAttrName, mFirstUnmapped);
        NS_ASSERTION(attr, "failed to find attribute");
        if (attr) {
          attr->mValue = aValue;
        }
      }
      else {
        HTMLAttribute*  attr = new HTMLAttribute(aAttrName, aValue);
        if (attr) {
          attr->mNext = mFirstUnmapped;
          mFirstUnmapped = attr;
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }
  aCount = mAttrCount;

  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::UnsetAttributeFor(nsIAtom* aAttrName,
                                      nsIHTMLContent* aContent,
                                      nsIHTMLStyleSheet* aSheet,
                                      PRInt32& aCount)
{
  nsresult result = NS_OK;
  if (nsHTMLAtoms::id == aAttrName) {
    NS_IF_RELEASE(mID);
  }
  else if (nsHTMLAtoms::kClass == aAttrName) {
    mFirstClass.Reset();
  }

  PRBool haveAttr;
  result = UnsetAttributeName(aAttrName, haveAttr);
  if (NS_SUCCEEDED(result) && haveAttr) {
    if (! HTMLAttribute::RemoveHTMLAttribute(aAttrName, &mFirstUnmapped)) {
      // must be mapped
      if (mMapped) {
        EnsureSingleMappedFor(aContent, aSheet, PR_FALSE);
        PRInt32 mappedCount = 0;
        mMapped->UnsetAttribute(aAttrName, mappedCount);
        if (! mappedCount) {  // toss it when empty
          mMapped->ReleaseUse();
          NS_RELEASE(mMapped);
        }
        else {
          UniqueMapped(aSheet);
        }
      }
    }
  }

  aCount = mAttrCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttribute(nsIAtom* aAttrName,
                                 nsHTMLValue& aValue) const
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;

  if (mMapped) {
    result = mMapped->GetAttribute(aAttrName, aValue);
  }

  if (NS_CONTENT_ATTR_NOT_THERE == result) {
    const HTMLAttribute*  attr = HTMLAttribute::FindHTMLAttribute(aAttrName, mFirstUnmapped);

    if (attr) {
      aValue = attr->mValue;
      result = ((attr->mValue.GetUnit() == eHTMLUnit_Null) ? 
                NS_CONTENT_ATTR_NO_VALUE : 
                NS_CONTENT_ATTR_HAS_VALUE);
    }
    else {
      aValue.Reset();
    }
  }

  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttribute(nsIAtom* aAttrName,
                                 const nsHTMLValue** aValue) const
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;

  if (mMapped) {
    result = mMapped->GetAttribute(aAttrName, aValue);
  }

  if (NS_CONTENT_ATTR_NOT_THERE == result) {
    const HTMLAttribute*  attr = HTMLAttribute::FindHTMLAttribute(aAttrName, mFirstUnmapped);

    if (attr) {
      *aValue = &attr->mValue;
      result = ((attr->mValue.GetUnit() == eHTMLUnit_Null) ? 
                NS_CONTENT_ATTR_NO_VALUE : 
                NS_CONTENT_ATTR_HAS_VALUE);
    }
    else {
      *aValue = nsnull;
    }
  }

  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttributeNameAt(PRInt32 aIndex,
                                       nsIAtom*& aName) const
{
  nsresult result = NS_ERROR_ILLEGAL_VALUE;

  if ((0 <= aIndex) && (aIndex < mAttrCount)) {
    aName = mAttrNames[aIndex];
    NS_ADDREF(aName);
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetAttributeCount(PRInt32& aCount) const
{
  aCount = mAttrCount;
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetID(nsIAtom*& aResult) const
{
  aResult = mID;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  const nsClassList* classList = &mFirstClass;
  while (classList && classList->mAtom) {
    aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
    classList = classList->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const
{
  const nsClassList* classList = &mFirstClass;
  while (classList) {
    if (aCaseSensitive) {
      if (classList->mAtom == aClass) {
        return NS_OK;
      }
    }
    else {
      nsAutoString s1;
      nsAutoString s2;
      if (classList->mAtom) {
        classList->mAtom->ToString(s1);
      }
      if (aClass) {
        aClass->ToString(s2);
      }
      if (s1.EqualsIgnoreCase(s2)) {
        return NS_OK;
      }
    }
    classList = classList->mNext;
  }
  return NS_COMFALSE;
}

#ifdef UNIQUE_ATTR_SUPPORT
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
#endif

NS_IMETHODIMP
HTMLAttributesImpl::Clone(nsIHTMLAttributes** aInstancePtrResult) const
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLAttributesImpl* clone = new HTMLAttributesImpl(*this);

  if (nsnull == clone) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return clone->QueryInterface(NS_GET_IID(nsIHTMLAttributes), (void **) aInstancePtrResult);
}

NS_IMETHODIMP
HTMLAttributesImpl::SetStyleSheet(nsIHTMLStyleSheet* aSheet)
{
  if (mMapped && (aSheet != mMapped->mSheet)) {
    mMapped->SetStyleSheet(aSheet);
    return UniqueMapped(aSheet);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker) const
{
  if (aRuleWalker && mMapped)
    aRuleWalker->Forward((nsIStyleRule*)mMapped);
  return NS_OK;
}

NS_IMETHODIMP
HTMLAttributesImpl::Reset(void)
{
  // Release atoms first, then the table if it was malloc'd
  PRInt32 i, n = mAttrCount;
  for (i = 0; i < n; i++) {
    NS_IF_RELEASE(mAttrNames[i]);
  }
  if (mAttrNames != mNameBuffer) {
    delete [] mAttrNames;
    mAttrNames = mNameBuffer;
    mAttrSize = kNameBufferSize;
  }
  mAttrCount = 0;

  if (mFirstUnmapped) {
    HTMLAttribute::DeleteHTMLAttributes(mFirstUnmapped);
  }

  if (mMapped) {
    mMapped->ReleaseUse();
    NS_RELEASE(mMapped);
  }

  NS_IF_RELEASE(mID);
  mFirstClass.Reset();
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
HTMLAttributesImpl::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 index = 0;
  while (index < mAttrCount) {
    PRInt32 indent;
    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

    nsHTMLValue value;
    GetAttribute(mAttrNames[index], value);
    nsAutoString  buffer;
    mAttrNames[index]->ToString(buffer);
    if (eHTMLUnit_Null != value.GetUnit()) {
      buffer.AppendWithConversion(" = ");
      value.AppendToString(buffer);
    }
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
  }
  return NS_OK;
}

void HTMLAttributesImpl::SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult)
{
  PRUint32 sum = 0;

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    // this is already accounted for
    return;
  }

  // XXX step through this again
  sum = sizeof(*this);
  if (mAttrNames != mNameBuffer) {
    sum += sizeof(*mAttrNames) * mAttrSize;
  }
  if (mFirstUnmapped) {
    HTMLAttribute* ha = mFirstUnmapped;
    while (ha) {
      PRUint32 asum = 0;
      ha->SizeOf(aSizer, &asum);  // XXX Unique???
      sum += asum;
      ha = ha->mNext;
    }
  }
  if (mMapped) {
    PRBool recorded;
    aSizer->RecordObject((void*)mMapped, &recorded);
    if (!recorded) {
      PRUint32 asum = 0;
      mMapped->SizeOf(aSizer, asum);  // XXX Unique???
      sum += asum;
    }
  }

  aResult = sum;

  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("HTMLAttributesImpl"));
  aSizer->AddSize(tag, aResult);
}
#endif

nsresult
NS_NewHTMLAttributes(nsIHTMLAttributes** aInstancePtrResult)
{
  *aInstancePtrResult = new HTMLAttributesImpl();
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


