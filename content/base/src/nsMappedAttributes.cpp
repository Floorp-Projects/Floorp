/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMappedAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsRuleWalker.h"
#include "prmem.h"

nsMappedAttributes::nsMappedAttributes(nsIHTMLStyleSheet* aSheet,
                                       nsMapRuleToAttributesFunc aMapRuleFunc)
  : mSheet(aSheet),
    mUseCount(0),
    mAttrCount(0),
    mBufferSize(0),
    mRuleMapper(aMapRuleFunc),
    mAttrs(nsnull)
{
}

nsMappedAttributes::nsMappedAttributes(const nsMappedAttributes& aCopy)
  : mSheet(aCopy.mSheet),
    mUseCount(0),
    mAttrCount(0),
    mBufferSize(0),
    mRuleMapper(aCopy.mRuleMapper),
    mAttrs(nsnull)
{
  if (!EnsureBufferSize(aCopy.mAttrCount + 1)) {
    return;
  }

  mAttrCount = aCopy.mAttrCount;

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    new (&mAttrs[i]) InternalAttr(aCopy.mAttrs[i]);
  }
}

nsMappedAttributes::~nsMappedAttributes()
{
  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    mAttrs[i].~InternalAttr();
  }

  PR_FREEIF(mAttrs);
}

NS_IMPL_ISUPPORTS1(nsMappedAttributes,
                   nsIStyleRule)

nsresult
nsMappedAttributes::SetAndTakeAttr(nsIAtom* aAttrName, nsAttrValue& aValue)
{
  NS_PRECONDITION(aAttrName, "null name");

  PRUint32 i;
  for (i = 0; i < mAttrCount && !mAttrs[i].mName.IsSmaller(aAttrName); ++i) {
    if (mAttrs[i].mName.Equals(aAttrName)) {
      mAttrs[i].mValue.Reset();
      mAttrs[i].mValue.SwapValueWith(aValue);

      return NS_OK;
    }
  }

  if (!EnsureBufferSize(mAttrCount + 1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mAttrCount != i) {
    memmove(&mAttrs[i + 1], &mAttrs[i], (mAttrCount - i) * sizeof(InternalAttr));
  }

  new (&mAttrs[i].mName) nsAttrName(aAttrName);
  new (&mAttrs[i].mValue) nsAttrValue();
  mAttrs[i].mValue.SwapValueWith(aValue);
  ++mAttrCount;

  return NS_OK;
}

nsresult
nsMappedAttributes::GetAttribute(nsIAtom* aAttrName,
                                 nsHTMLValue& aValue) const
{
  NS_PRECONDITION(aAttrName, "null name");

  const nsAttrValue* val = GetAttr(aAttrName);

  if (!val) {
    aValue.Reset();
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  if (val->GetType() == nsAttrValue::eHTMLValue) {
    aValue = *val->GetHTMLValue();
  }
  else {
    nsAutoString strVal;
    val->ToString(strVal);
    aValue.SetStringValue(strVal);
  }

  return NS_CONTENT_ATTR_HAS_VALUE;

}

const nsAttrValue*
nsMappedAttributes::GetAttr(nsIAtom* aAttrName) const
{
  NS_PRECONDITION(aAttrName, "null name");

  PRInt32 i = IndexOfAttr(aAttrName, kNameSpaceID_None);
  if (i >= 0) {
    return &mAttrs[i].mValue;
  }

  return nsnull;
}

PRBool
nsMappedAttributes::Equals(const nsMappedAttributes* aOther) const
{
  if (this == aOther) {
    return PR_TRUE;
  }

  if (mRuleMapper != aOther->mRuleMapper || mAttrCount != aOther->mAttrCount) {
    return PR_FALSE;
  }

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (!mAttrs[i].mName.Equals(aOther->mAttrs[i].mName) ||
        !mAttrs[i].mValue.EqualsIgnoreCase(aOther->mAttrs[i].mValue)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

PRUint32
nsMappedAttributes::HashValue() const
{
  PRUint32 value = NS_PTR_TO_INT32(mRuleMapper);

  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    value ^= mAttrs[i].mName.HashValue() ^ mAttrs[i].mValue.HashValue();
  }

  return value;
}

void
nsMappedAttributes::AddUse()
{
  mUseCount++;
}

void
nsMappedAttributes::ReleaseUse()
{
  mUseCount--;

  if (mSheet && !mUseCount) {
    mSheet->DropMappedAttributes(this);
  }
}

NS_IMETHODIMP
nsMappedAttributes::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  aSheet = mSheet;
  NS_IF_ADDREF(aSheet);
  return NS_OK;
}

void
nsMappedAttributes::SetStyleSheet(nsIHTMLStyleSheet* aSheet)
{
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }
  mSheet = aSheet;  // not ref counted
}

NS_IMETHODIMP
nsMappedAttributes::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (mRuleMapper) {
    (*mRuleMapper)(this, aRuleData);
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsMappedAttributes::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;
  PRUint32 i;

  for (i = 0; i < mAttrCount; ++i) {
    PRInt32 indent;
    for (indent = aIndent; indent > 0; --indent)
      fputs("  ", out);

    if (mAttrs[i].mName.IsAtom()) {
      mAttrs[i].mName.Atom()->ToString(buffer);
    }
    else {
      mAttrs[i].mName.NodeInfo()->GetQualifiedName(buffer);
    }
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);

    mAttrs[i].mValue.ToString(buffer);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
    fputs("\n", out);
  }

  return NS_OK;
}
#endif

void
nsMappedAttributes::RemoveAttrAt(PRUint32 aPos)
{
  mAttrs[aPos].~InternalAttr();
  memmove(&mAttrs[aPos], &mAttrs[aPos + 1],
          (mAttrCount - aPos - 1) * sizeof(InternalAttr));
  mAttrCount--;
}

const nsAttrName*
nsMappedAttributes::GetExistingAttrNameFromQName(const nsACString& aName) const
{
  PRUint32 i;
  for (i = 0; i < mAttrCount; ++i) {
    if (mAttrs[i].mName.IsAtom()) {
      if (mAttrs[i].mName.Atom()->EqualsUTF8(aName)) {
        return &mAttrs[i].mName;
      }
    }
    else {
      if (mAttrs[i].mName.NodeInfo()->QualifiedNameEquals(aName)) {
        return &mAttrs[i].mName;
      }
    }
  }

  return nsnull;
}

PRInt32
nsMappedAttributes::IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRUint32 i;
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < mAttrCount; ++i) {
      if (mAttrs[i].mName.Equals(aLocalName)) {
        return i;
      }
    }
  }
  else {
    for (i = 0; i < mAttrCount; ++i) {
      if (mAttrs[i].mName.Equals(aLocalName, aNamespaceID)) {
        return i;
      }
    }
  }

  return -1;
}

PRBool
nsMappedAttributes::EnsureBufferSize(PRUint32 aSize)
{
  if (mBufferSize >= aSize) {
    return PR_TRUE;
  }

  InternalAttr* buffer = NS_STATIC_CAST(InternalAttr*,
      mAttrs ? PR_Realloc(mAttrs, aSize * sizeof(InternalAttr)) :
               PR_Malloc(aSize * sizeof(InternalAttr)));
  NS_ENSURE_TRUE(buffer, PR_FALSE);

  mAttrs = buffer;
  mBufferSize = aSize;
  
  return PR_TRUE;
}
