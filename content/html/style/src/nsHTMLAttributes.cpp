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
#include "nsHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsIArena.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsVoidArray.h"
#include "nsISizeOfHandler.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"

#include "nsIStyleSet.h"
#include "nsRuleWalker.h"

MOZ_DECL_CTOR_COUNTER(HTMLAttribute)

struct HTMLAttribute {
  HTMLAttribute(void)
    : mValue(),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    mAttribute.mAtom = nsnull;
  }

  HTMLAttribute(nsIAtom* aAttribute, const nsAString& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    mAttribute.Addref();
  }

  HTMLAttribute(nsHTMLAttrName aAttribute, const nsHTMLValue& aValue)
    : mAttribute(aAttribute),
      mValue(aValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    mAttribute.Addref();
  }

  HTMLAttribute(const HTMLAttribute& aCopy)
    : mAttribute(aCopy.mAttribute),
      mValue(aCopy.mValue),
      mNext(nsnull)
  {
    MOZ_COUNT_CTOR(HTMLAttribute);
    mAttribute.Addref();
  }

  ~HTMLAttribute(void)
  {
    MOZ_COUNT_DTOR(HTMLAttribute);
    mAttribute.Release();
  }

  HTMLAttribute& operator=(const HTMLAttribute& aCopy)
  {
    mAttribute.Release();
    mAttribute = aCopy.mAttribute;
    mAttribute.Addref();
    mValue = aCopy.mValue;
    return *this;
  }

  PRBool operator==(const HTMLAttribute& aOther) const
  {
    return PRBool((mAttribute.mBits == aOther.mAttribute.mBits) &&
                  (mValue == aOther.mValue));
  }

  PRUint32 HashValue(void) const
  {
    return mAttribute.mBits ^ mValue.HashValue();
  }

  void Reset(void)
  {
    mAttribute.Release();
    mValue.Reset();
  }

  void Set(nsIAtom* aAttribute, const nsHTMLValue& aValue)
  {
    mAttribute.Release();
    mAttribute.mAtom = aAttribute;
    mAttribute.Addref();
    mValue = aValue;
  }

  void Set(nsIAtom* aAttribute, const nsAString& aValue)
  {
    mAttribute.Release();
    mAttribute.mAtom = aAttribute;
    mAttribute.Addref();
    mValue.SetStringValue(aValue);
  }

  void Set(nsINodeInfo* aAttribute, const nsAString& aValue)
  {
    mAttribute.Release();
    mAttribute.SetNodeInfo(aAttribute);
    mAttribute.Addref();
    mValue.SetStringValue(aValue);
  }

#ifdef DEBUG
  void AppendToString(nsString& aBuffer) const
  {
    if (mAttribute.mBits) {
      nsAutoString temp;
      if (mAttribute.IsAtom())
          mAttribute.mAtom->ToString(temp);
      else
          mAttribute.GetNodeInfo()->GetQualifiedName(temp);
      aBuffer.Append(temp);
      if (eHTMLUnit_Null != mValue.GetUnit()) {
        aBuffer.Append(NS_LITERAL_STRING(" = "));
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.Append(NS_LITERAL_STRING("null"));
    }
  }

  void ToString(nsString& aBuffer) const
  {
    if (mAttribute.mBits) {
      if (mAttribute.IsAtom())
          mAttribute.mAtom->ToString(aBuffer);
      else
          mAttribute.GetNodeInfo()->GetQualifiedName(aBuffer);
      if (eHTMLUnit_Null != mValue.GetUnit()) {
        aBuffer.Append(NS_LITERAL_STRING(" = "));
        mValue.AppendToString(aBuffer);
      }
    }
    else {
      aBuffer.Assign(NS_LITERAL_STRING("null"));
    }
  }
#endif // DEBUG

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
      if (aAttrName == aAttr->mAttribute.mAtom) {
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
      if (aAttrName == aAttr->mAttribute.mAtom) {
        return aAttr;
      }
      aAttr = aAttr->mNext;
    }
    return nsnull;
  }

  static HTMLAttribute*
  FindHTMLAttribute(nsIAtom* aAttrName, PRInt32 aNamespaceID,
                    HTMLAttribute* aAttr)
  {
    // Just a precaution
    if (aNamespaceID == kNameSpaceID_None)
      return FindHTMLAttribute(aAttrName, aAttr);

    while (aAttr) {
      if (!aAttr->mAttribute.IsAtom() &&
          aAttr->mAttribute.GetNodeInfo()->Equals(aAttrName, aNamespaceID)) {
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
      if ((*aAttr)->mAttribute.mAtom == aAttrName) {
        HTMLAttribute*  attr = *aAttr;
        *aAttr = (*aAttr)->mNext;
        delete attr;
        return PR_TRUE;
      }
      aAttr = &((*aAttr)->mNext);
    }
    return PR_FALSE;
  }

  static PRBool
  RemoveHTMLAttribute(nsIAtom* aAttrName, PRInt32 aNamespaceID,
                      HTMLAttribute** aAttr)
  {
    if (aNamespaceID == kNameSpaceID_None)
      return RemoveHTMLAttribute(aAttrName, aAttr);

    while (*aAttr) {
      if (!(*aAttr)->mAttribute.IsAtom() &&
          (*aAttr)->mAttribute.GetNodeInfo()->Equals(aAttrName,
                                                     aNamespaceID)) {
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

  nsHTMLAttrName  mAttribute;
  nsHTMLValue     mValue;
  HTMLAttribute*  mNext;
};

// ----------------

MOZ_DECL_CTOR_COUNTER(nsHTMLClassList)

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

  NS_IMETHOD SetAttribute(nsIAtom* aAttrName, const nsAString& aValue);
  NS_IMETHOD SetAttribute(nsIAtom* aAttrName, const nsHTMLValue& aValue);
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttrName, PRInt32& aAttrCount);

  NS_IMETHOD GetAttribute(nsIAtom* aAttrName,
                          nsHTMLValue& aValue) const;
  NS_IMETHOD GetAttribute(nsIAtom* aAttrName,
                          const nsHTMLValue** aValue) const;
  NS_IMETHOD_(PRBool) HasAttribute(nsIAtom* aAttrName) const;

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
}

nsHTMLMappedAttributes::nsHTMLMappedAttributes(const nsHTMLMappedAttributes& aCopy)
  : mSheet(aCopy.mSheet),
    mUseCount(0),
    mAttrCount(aCopy.mAttrCount),
    mFirst(aCopy.mFirst),
    mRuleMapper(aCopy.mRuleMapper),
    mUniqued(PR_FALSE)
{

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
nsHTMLMappedAttributes::SetAttribute(nsIAtom* aAttrName, const nsAString& aValue)
{
  if (! aAttrName) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mFirst.mAttribute.mBits) {  // do we already have any?
    HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
    if (attr) {
      attr->mValue.SetStringValue(aValue);
    }
    else {
      // add new attribute
      // keep these arbitrarily sorted so they'll equal regardless of set order
      if (aAttrName < mFirst.mAttribute.mAtom) {  // before first, move first down one
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
          while (prev->mNext && (prev->mNext->mAttribute.mAtom < aAttrName)) {
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

  if (mFirst.mAttribute.mBits) {  // do we already have any?
    HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
    if (attr) {
      attr->mValue = aValue;
    }
    else {
      // add new attribute
      // keep these arbitrarily sorted so they'll hash regardless of set order
      if (aAttrName < mFirst.mAttribute.mAtom) {  // before first, move first down one
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
          while (prev->mNext && (prev->mNext->mAttribute.mAtom < aAttrName)) {
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

  if (aAttrName == mFirst.mAttribute.mAtom) {
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

NS_IMETHODIMP_(PRBool)
nsHTMLMappedAttributes::HasAttribute(nsIAtom* aAttrName) const
{
  if (!aAttrName)
    return PR_FALSE;

  const HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName, &mFirst);
  return attr != nsnull;
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
    if (attr->mAttribute.mAtom) {
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
  tag = do_GetAtom("HTMLMappedAttributes");
  aResult = sizeof(*this);
  aSizer->AddSize(tag, aResult);
}
#endif

//--------------------

nsHTMLAttributes::nsHTMLAttributes(void)
  : mAttrNames(mNameBuffer),
    mAttrCount(0),
    mAttrSize(kHTMLAttrNameBufferSize),
    mFirstUnmapped(nsnull),
    mMapped(nsnull),
    mID(nsnull),
    mFirstClass(nsnull)
{
}

nsHTMLAttributes::nsHTMLAttributes(const nsHTMLAttributes& aCopy)
  : mAttrNames(mNameBuffer),
    mAttrCount(aCopy.mAttrCount),
    mAttrSize(kHTMLAttrNameBufferSize),
    mFirstUnmapped(nsnull),
    mMapped(aCopy.mMapped),
    mID(aCopy.mID),
    mFirstClass(aCopy.mFirstClass)
{
  if (mAttrCount) {
    if (mAttrSize < mAttrCount) {
      mAttrNames = new nsHTMLAttrName[mAttrCount];
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
      mAttrNames[index].Addref();
    }
  }

  HTMLAttribute::CopyHTMLAttributes(aCopy.mFirstUnmapped, &mFirstUnmapped);
  if (mMapped) {
    mMapped->AddUse();
    NS_ADDREF(mMapped);
  }

  NS_IF_ADDREF(mID);
}

nsHTMLAttributes::~nsHTMLAttributes(void)
{
  Reset();
}

const PRUnichar kNullCh = PRUnichar('\0');

static void ParseClasses(const nsAString& aClassString, nsHTMLClassList& aClassList)
{
  nsAutoString  classStr(aClassString);  // copy to work buffer
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr.get();
  PRUnichar* end   = start;

  nsHTMLClassList*  list = &aClassList;
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
        list->mNext = new nsHTMLClassList(NS_NewAtom(start));
        list = list->mNext;
      }
    }

    start = ++end;
  }
}

nsresult
nsHTMLAttributes::SetAttributeName(nsHTMLAttrName aAttrName, PRBool& aFound)
{
  PRInt32 index = mAttrCount;
  if (aAttrName.IsAtom()) {
    while (0 < index--) {
      if (mAttrNames[index].mAtom == aAttrName.mAtom) {
        aFound = PR_TRUE;
        return NS_OK;
      }
    }
  }
  else {
    nsINodeInfo* ni = aAttrName.GetNodeInfo();
    while (0 < index--) {
      if (!mAttrNames[index].IsAtom() && 
          ni->NameAndNamespaceEquals(mAttrNames[index].GetNodeInfo())) {
        aFound = PR_TRUE;
        return NS_OK;
      }
    }
  }
  aFound = PR_FALSE;
  if (mAttrCount == mAttrSize) {  // no more room, grow buffer
    nsHTMLAttrName* buffer = new nsHTMLAttrName[mAttrSize + 4]; 
    if (buffer) {
      memcpy(buffer, mAttrNames, sizeof(nsHTMLAttrName) * mAttrCount);
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
  mAttrNames[mAttrCount] = aAttrName;
  mAttrNames[mAttrCount++].Addref();
  return NS_OK;
}

nsresult
nsHTMLAttributes::UnsetAttributeName(nsIAtom* aAttrName, PRBool& aFound)
{
  PRInt32 index = mAttrCount;
  while (0 < index--) {
    if (mAttrNames[index].mAtom == aAttrName) {
      mAttrCount--;
      if (mAttrNames != mNameBuffer &&
          mAttrCount <= (kHTMLAttrNameBufferSize / 2)) {
        // go back to using internal buffer
        if (0 < index) {
          memcpy(mNameBuffer, mAttrNames, index * sizeof(nsIAtom*));
        }
        if (index < mAttrCount) {
          memcpy(&mNameBuffer[index], &mAttrNames[index + 1], 
                 (mAttrCount - index) * sizeof(nsIAtom*));
        }
        delete [] mAttrNames;
        mAttrNames = mNameBuffer;
        mAttrSize = kHTMLAttrNameBufferSize;
      }
      else {
        if (index < mAttrCount) {
          memmove(&(mAttrNames[index]), &(mAttrNames[index + 1]), 
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
nsHTMLAttributes::UnsetAttributeName(nsIAtom* aAttrName, PRInt32 aNamespaceID,
                                     PRBool& aFound)
{
  NS_ASSERTION(aNamespaceID != kNameSpaceID_None,
               "namespaced UnsetAttributeName called with null namespace");
  PRInt32 index = mAttrCount;
  while (0 < index--) {
    if (!mAttrNames[index].IsAtom() &&
        mAttrNames[index].GetNodeInfo()->Equals(aAttrName, aNamespaceID)) {
      mAttrNames[index].Release();
      mAttrCount--;
      if (mAttrNames != mNameBuffer &&
          mAttrCount <= (kHTMLAttrNameBufferSize / 2)) {
        // go back to using internal buffer
        if (0 < index) {
          memcpy(mNameBuffer, mAttrNames, index * sizeof(nsIAtom*));
        }
        if (index < mAttrCount) {
          memcpy(&mNameBuffer[index], &mAttrNames[index + 1], 
                 (mAttrCount - index) * sizeof(nsIAtom*));
        }
        delete [] mAttrNames;
        mAttrNames = mNameBuffer;
        mAttrSize = kHTMLAttrNameBufferSize;
      }
      else {
        if (index < mAttrCount) {
          memmove(&(mAttrNames[index]), &(mAttrNames[index + 1]),
                  (mAttrCount - index) * sizeof(nsIAtom*));
        }
      }
      aFound = PR_TRUE;
      return NS_OK;
    }
  }
  aFound = PR_FALSE;
  return NS_OK;
}

nsresult
nsHTMLAttributes::EnsureSingleMappedFor(nsIHTMLContent* aContent,
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
nsHTMLAttributes::UniqueMapped(nsIHTMLStyleSheet* aSheet)
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
nsHTMLAttributes::SetAttributeFor(nsIAtom* aAttrName, const nsAString& aValue,
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
nsHTMLAttributes::SetAttributeFor(nsIAtom* aAttrName,
                                  const nsHTMLValue& aValue,
                                  PRBool aMappedToStyle,
                                  nsIHTMLContent* aContent,
                                  nsIHTMLStyleSheet* aSheet,
                                  PRInt32& aCount)
{
  nsresult result = NS_OK;

  if (eHTMLUnit_Null == aValue.GetUnit()) {
    return UnsetAttributeFor(aAttrName, kNameSpaceID_None, aContent,
                             aSheet, aCount);
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
nsHTMLAttributes::UnsetAttributeFor(nsIAtom* aAttrName,
                                    PRInt32 aNamespaceID,
                                    nsIHTMLContent* aContent,
                                    nsIHTMLStyleSheet* aSheet,
                                    PRInt32& aCount)
{
  nsresult result = NS_OK;
  PRBool haveAttr;

  if (aNamespaceID == kNameSpaceID_None) {
    if (nsHTMLAtoms::id == aAttrName) {
      NS_IF_RELEASE(mID);
    }
    else if (nsHTMLAtoms::kClass == aAttrName) {
      mFirstClass.Reset();
    }
    result = UnsetAttributeName(aAttrName, haveAttr);
  }
  else {
    result = UnsetAttributeName(aAttrName, aNamespaceID, haveAttr);
  }

  if (NS_SUCCEEDED(result) && haveAttr) {
    if (!HTMLAttribute::RemoveHTMLAttribute(aAttrName, aNamespaceID,
                                            &mFirstUnmapped) &&
        aNamespaceID == kNameSpaceID_None) {
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
nsHTMLAttributes::GetAttribute(nsIAtom* aAttrName,
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
nsHTMLAttributes::GetAttribute(nsIAtom* aAttrName,
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

NS_IMETHODIMP_(PRBool)
nsHTMLAttributes::HasAttribute(nsIAtom* aAttrName, PRInt32 aNamespaceID) const
{
  if (mMapped && aNamespaceID == kNameSpaceID_None &&
      mMapped->HasAttribute(aAttrName))
    return PR_TRUE;

  return (PRBool)(HTMLAttribute::FindHTMLAttribute(aAttrName, aNamespaceID, mFirstUnmapped) != nsnull);
}

NS_IMETHODIMP
nsHTMLAttributes::SetAttributeFor(nsINodeInfo* aAttrName,
                                  const nsAString& aValue)
{
  NS_ENSURE_ARG_POINTER(aAttrName);
  nsresult rv;
#ifdef DEBUG
  {
    PRInt32 namespaceID;
    aAttrName->GetNamespaceID(namespaceID);
    NS_ASSERTION(namespaceID != kNameSpaceID_None, "namespace is null in SetAttributeFor");
  }
#endif

  PRBool haveAttr;
  rv = SetAttributeName(aAttrName, haveAttr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (haveAttr) {
    PRInt32 namespaceID;
    nsCOMPtr<nsIAtom> localName;
    aAttrName->GetNamespaceID(namespaceID);
    aAttrName->GetNameAtom(*getter_AddRefs(localName));
    HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(localName,
                                                           namespaceID,
                                                           mFirstUnmapped);
    NS_ASSERTION(attr, "failed to find attribute");
    if (!attr)
      return NS_ERROR_FAILURE;
    attr->mValue.SetStringValue(aValue);
  }
  else {
    HTMLAttribute* attr = new HTMLAttribute(aAttrName, aValue);
    NS_ENSURE_TRUE(attr, NS_ERROR_OUT_OF_MEMORY);
    attr->mNext = mFirstUnmapped;
    mFirstUnmapped = attr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::GetAttribute(nsIAtom* aAttrName, PRInt32 aNamespaceID,
                               nsIAtom*& aPrefix,
                               const nsHTMLValue** aValue) const
{
  NS_ASSERTION(aNamespaceID != kNameSpaceID_None, "namespace is null in UnsetAttributeFor");

  const HTMLAttribute* attr = HTMLAttribute::FindHTMLAttribute(aAttrName,
                                                               aNamespaceID,
                                                               mFirstUnmapped);
  if (!attr) {
    *aValue = nsnull;
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  *aValue = &attr->mValue;
  attr->mAttribute.GetNodeInfo()->GetPrefixAtom(aPrefix);
  return attr->mValue.GetUnit() == eHTMLUnit_Null ? 
         NS_CONTENT_ATTR_NO_VALUE : 
         NS_CONTENT_ATTR_HAS_VALUE;
}


NS_IMETHODIMP
nsHTMLAttributes::GetAttributeNameAt(PRInt32 aIndex,
                                     PRInt32& aNamespaceID,
                                     nsIAtom*& aName,
                                     nsIAtom*& aPrefix) const
{
  nsresult result = NS_ERROR_ILLEGAL_VALUE;

  if ((0 <= aIndex) && (aIndex < mAttrCount)) {
    if (mAttrNames[aIndex].IsAtom()) {
        aNamespaceID = kNameSpaceID_None;
        aName = mAttrNames[aIndex].mAtom;
        NS_ADDREF(aName);
        aPrefix = nsnull;
    }
    else {
        nsINodeInfo* ni = mAttrNames[aIndex].GetNodeInfo();
        ni->GetNamespaceID(aNamespaceID);
        ni->GetNameAtom(aName);
        ni->GetPrefixAtom(aPrefix);
    }
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
nsHTMLAttributes::GetAttributeCount(PRInt32& aCount) const
{
  aCount = mAttrCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::GetID(nsIAtom*& aResult) const
{
  aResult = mID;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  const nsHTMLClassList* classList = &mFirstClass;
  while (classList && classList->mAtom) {
    aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
    classList = classList->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsHTMLAttributes::HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const
{
  NS_PRECONDITION(aClass, "unexpected null pointer");
  if (mFirstClass.mAtom) {
    const nsHTMLClassList* classList = &mFirstClass;
    if (aCaseSensitive) {
      do {
        if (classList->mAtom == aClass)
          return PR_TRUE;
        classList = classList->mNext;
      } while (classList);
    } else {
      const PRUnichar* class1Buf;
      aClass->GetUnicode(&class1Buf);
      // This length calculation (and the |aCaseSensitive| check above) could
      // theoretically be pulled out of another loop by creating a separate
      // |HasClassCI| function.
      nsDependentString class1(class1Buf);
      do {
        const PRUnichar* class2Buf;
        classList->mAtom->GetUnicode(&class2Buf);
        nsDependentString class2(class2Buf);
        if (class1.Equals(class2, nsCaseInsensitiveStringComparator()))
          return PR_TRUE;
        classList = classList->mNext;
      } while (classList);
    }
  }
  return PR_FALSE;
}

#ifdef UNIQUE_ATTR_SUPPORT
NS_IMETHODIMP
nsHTMLAttributes::AddContentRef(void)
{
  ++mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::ReleaseContentRef(void)
{
  --mContentRefCount;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::GetContentRefCount(PRInt32& aCount) const
{
  aCount = mContentRefCount;
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsHTMLAttributes::Clone(nsHTMLAttributes** aInstancePtrResult) const
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  nsHTMLAttributes* clone = new nsHTMLAttributes(*this);

  if (nsnull == clone) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aInstancePtrResult = clone;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::SetStyleSheet(nsIHTMLStyleSheet* aSheet)
{
  if (mMapped && (aSheet != mMapped->mSheet)) {
    mMapped->SetStyleSheet(aSheet);
    return UniqueMapped(aSheet);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker) const
{
  if (aRuleWalker && mMapped)
    aRuleWalker->Forward((nsIStyleRule*)mMapped);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAttributes::Reset(void)
{
  // Release atoms first, then the table if it was malloc'd
  PRInt32 i, n = mAttrCount;
  for (i = 0; i < n; i++) {
    mAttrNames[i].Release();
  }
  if (mAttrNames != mNameBuffer) {
    delete [] mAttrNames;
    mAttrNames = mNameBuffer;
    mAttrSize = kHTMLAttrNameBufferSize;
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
nsHTMLAttributes::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 index = 0;
  while (index < mAttrCount) {
    PRInt32 indent;
    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

    nsHTMLValue value;
    nsAutoString  buffer;
    if (mAttrNames[index].IsAtom()) {
      GetAttribute(mAttrNames[index].mAtom, value);
      mAttrNames[index].mAtom->ToString(buffer);
    }
    else {
      nsINodeInfo* ni = mAttrNames[index].GetNodeInfo();
      PRInt32 namespaceID;
      nsCOMPtr<nsIAtom> localName, prefix;
      ni->GetNameAtom(*getter_AddRefs(localName));
      ni->GetNamespaceID(namespaceID);
      const nsHTMLValue *tmp;
      GetAttribute(localName, namespaceID, *getter_AddRefs(prefix), &tmp);
      value = *tmp;
      ni->GetQualifiedName(buffer);
    }
    if (eHTMLUnit_Null != value.GetUnit()) {
      buffer.Append(NS_LITERAL_STRING(" = "));
      value.AppendToString(buffer);
    }
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
  }
  return NS_OK;
}

void nsHTMLAttributes::SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult)
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
  tag = do_GetAtom("nsHTMLAttributes");
  aSizer->AddSize(tag, aResult);
}
#endif

nsresult
NS_NewHTMLAttributes(nsHTMLAttributes** aInstancePtrResult)
{
  *aInstancePtrResult = new nsHTMLAttributes();
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


