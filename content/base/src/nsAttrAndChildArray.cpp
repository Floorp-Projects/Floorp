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
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#include "nsAttrAndChildArray.h"
#include "nsIContent.h"
#include "prmem.h"
#include "prbit.h"
#include "nsString.h"
#include "nsHTMLStyleSheet.h"
#include "nsRuleWalker.h"
#include "nsMappedAttributes.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"

/**
 * Due to a compiler bug in VisualAge C++ for AIX, we need to return the 
 * address of the first index into mBuffer here, instead of simply returning 
 * mBuffer itself.
 *
 * See Bug 231104 for more information.
 */
#define ATTRS(_impl) \
  NS_REINTERPRET_CAST(InternalAttr*, &((_impl)->mBuffer[0]))
  

#define NS_IMPL_EXTRA_SIZE \
  ((sizeof(Impl) - sizeof(mImpl->mBuffer)) / sizeof(void*))

nsAttrAndChildArray::nsAttrAndChildArray()
  : mImpl(nsnull)
{
}

nsAttrAndChildArray::~nsAttrAndChildArray()
{
  if (!mImpl) {
    return;
  }

  NS_ASSERTION(!mImpl->mMappedAttrs &&
               mImpl->mAttrAndChildCount == 0,
               "Call nsAttrAndChildArray::Clear() before destruction.");

  PR_Free(mImpl);
}

nsIContent*
nsAttrAndChildArray::GetSafeChildAt(PRUint32 aPos) const
{
  if (aPos < ChildCount()) {
    return ChildAt(aPos);
  }
  
  return nsnull;
}

nsresult
nsAttrAndChildArray::InsertChildAt(nsIContent* aChild, PRUint32 aPos)
{
  NS_ASSERTION(aChild, "nullchild");
  NS_ASSERTION(aPos <= ChildCount(), "out-of-bounds");

  PRUint32 offset = AttrSlotsSize();
  PRUint32 childCount = ChildCount();

  NS_ENSURE_TRUE(childCount < ATTRCHILD_ARRAY_MAX_CHILD_COUNT,
                 NS_ERROR_FAILURE);

  // First try to fit new child in existing childlist
  if (mImpl && offset + childCount < mImpl->mBufferSize) {
    void** pos = mImpl->mBuffer + offset + aPos;
    if (childCount != aPos) {
      memmove(pos + 1, pos, (childCount - aPos) * sizeof(nsIContent*));
    }
    *pos = aChild;
    NS_ADDREF(aChild);

    SetChildCount(childCount + 1);

    return NS_OK;
  }

  // Try to fit new child in existing buffer by compressing attrslots
  if (offset && !mImpl->mBuffer[offset - ATTRSIZE]) {
    // Compress away all empty slots while we're at it. This might not be the
    // optimal thing to do.
    PRUint32 attrCount = NonMappedAttrCount();
    void** newStart = mImpl->mBuffer + attrCount * ATTRSIZE;
    void** oldStart = mImpl->mBuffer + offset;
    memmove(newStart, oldStart, aPos * sizeof(nsIContent*));
    newStart[aPos] = aChild;
    memmove(&newStart[aPos + 1], &oldStart[aPos],
            (childCount - aPos) * sizeof(nsIContent*));
    NS_ADDREF(aChild);

    SetAttrSlotAndChildCount(attrCount, childCount + 1);

    return NS_OK;
  }

  // We can't fit in current buffer, Realloc time!
  if (!GrowBy(1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  void** pos = mImpl->mBuffer + offset + aPos;
  if (childCount != aPos) {
    memmove(pos + 1, pos, (childCount - aPos) * sizeof(nsIContent*));
  }
  *pos = aChild;
  NS_ADDREF(aChild);

  SetChildCount(childCount + 1);
  
  return NS_OK;
}

void
nsAttrAndChildArray::RemoveChildAt(PRUint32 aPos)
{
  NS_ASSERTION(aPos < ChildCount(), "out-of-bounds");

  PRUint32 childCount = ChildCount();
  void** pos = mImpl->mBuffer + AttrSlotsSize() + aPos;
  nsIContent* child = NS_STATIC_CAST(nsIContent*, *pos);
  NS_RELEASE(child);
  memmove(pos, pos + 1, (childCount - aPos - 1) * sizeof(nsIContent*));
  SetChildCount(childCount - 1);
}

PRInt32
nsAttrAndChildArray::IndexOfChild(nsIContent* aPossibleChild) const
{
  if (!mImpl) {
    return -1;
  }
  void** children = mImpl->mBuffer + AttrSlotsSize();
  PRUint32 i, count = ChildCount();
  for (i = 0; i < count; ++i) {
    if (children[i] == aPossibleChild) {
      return NS_STATIC_CAST(PRInt32, i);
    }
  }

  return -1;
}

PRUint32
nsAttrAndChildArray::AttrCount() const
{
  return NonMappedAttrCount() + MappedAttrCount();
}

const nsAttrValue*
nsAttrAndChildArray::GetAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRUint32 i, slotCount = AttrSlotCount();
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        return &ATTRS(mImpl)[i].mValue;
      }
    }

    if (mImpl && mImpl->mMappedAttrs) {
      return mImpl->mMappedAttrs->GetAttr(aLocalName);
    }
  }
  else {
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName, aNamespaceID)) {
        return &ATTRS(mImpl)[i].mValue;
      }
    }
  }

  return nsnull;
}

const nsAttrValue*
nsAttrAndChildArray::AttrAt(PRUint32 aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in nsAttrAndChildArray");

  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    return mImpl->mMappedAttrs->AttrAt(aPos);
  }

  return &ATTRS(mImpl)[aPos - mapped].mValue;
}

nsresult
nsAttrAndChildArray::SetAttr(nsIAtom* aLocalName, const nsAString& aValue)
{
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
      ATTRS(mImpl)[i].mValue.SetTo(aValue);

      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(slotCount < ATTRCHILD_ARRAY_MAX_ATTR_COUNT,
                 NS_ERROR_FAILURE);

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aLocalName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue(aValue);

  return NS_OK;
}

nsresult
nsAttrAndChildArray::SetAndTakeAttr(nsIAtom* aLocalName, nsAttrValue& aValue)
{
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
      ATTRS(mImpl)[i].mValue.Reset();
      ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(i < ATTRCHILD_ARRAY_MAX_ATTR_COUNT,
                 NS_ERROR_FAILURE);

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aLocalName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue();
  ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

  return NS_OK;
}

nsresult
nsAttrAndChildArray::SetAndTakeAttr(nsINodeInfo* aName, nsAttrValue& aValue)
{
  PRInt32 namespaceID = aName->NamespaceID();
  nsIAtom* localName = aName->NameAtom();
  if (namespaceID == kNameSpaceID_None) {
    return SetAndTakeAttr(localName, aValue);
  }

  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(localName, namespaceID)) {
      ATTRS(mImpl)[i].mName.SetTo(aName);
      ATTRS(mImpl)[i].mValue.Reset();
      ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(i < ATTRCHILD_ARRAY_MAX_ATTR_COUNT,
                 NS_ERROR_FAILURE);

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue();
  ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

  return NS_OK;
}


nsresult
nsAttrAndChildArray::RemoveAttrAt(PRUint32 aPos)
{
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds");

  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    if (mapped == 1) {
      // We're removing the last mapped attribute.
      NS_RELEASE(mImpl->mMappedAttrs);

      return NS_OK;
    }

    nsRefPtr<nsMappedAttributes> mapped;
    nsresult rv = GetModifiableMapped(nsnull, nsnull, PR_FALSE,
                                      getter_AddRefs(mapped));
    NS_ENSURE_SUCCESS(rv, rv);

    mapped->RemoveAttrAt(aPos);

    return MakeMappedUnique(mapped);
  }

  aPos -= mapped;
  ATTRS(mImpl)[aPos].~InternalAttr();

  PRUint32 slotCount = AttrSlotCount();
  memmove(&ATTRS(mImpl)[aPos],
          &ATTRS(mImpl)[aPos + 1],
          (slotCount - aPos - 1) * sizeof(InternalAttr));
  memset(&ATTRS(mImpl)[slotCount - 1], nsnull, sizeof(InternalAttr));

  return NS_OK;
}

const nsAttrName*
nsAttrAndChildArray::GetSafeAttrNameAt(PRUint32 aPos) const
{
  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    return mImpl->mMappedAttrs->NameAt(aPos);
  }

  // Warn here since we should make this non-bounds safe
  aPos -= mapped;
  PRUint32 slotCount = AttrSlotCount();
  NS_ENSURE_TRUE(aPos < slotCount, nsnull);

  void** pos = mImpl->mBuffer + aPos * ATTRSIZE;
  NS_ENSURE_TRUE(*pos, nsnull);

  return &NS_REINTERPRET_CAST(InternalAttr*, pos)->mName;
}

const nsAttrName*
nsAttrAndChildArray::GetExistingAttrNameFromQName(const nsACString& aName) const
{
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.QualifiedNameEquals(aName)) {
      return &ATTRS(mImpl)[i].mName;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetExistingAttrNameFromQName(aName);
  }

  return nsnull;
}

PRInt32
nsAttrAndChildArray::IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRInt32 idx;
  if (mImpl && mImpl->mMappedAttrs) {
    idx = mImpl->mMappedAttrs->IndexOfAttr(aLocalName, aNamespaceID);
    if (idx >= 0) {
      return idx;
    }
  }

  PRUint32 i;
  PRUint32 mapped = MappedAttrCount();
  PRUint32 slotCount = AttrSlotCount();
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        return i + mapped;
      }
    }
  }
  else {
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName, aNamespaceID)) {
        return i + mapped;
      }
    }
  }

  return -1;
}

nsresult
nsAttrAndChildArray::SetAndTakeMappedAttr(nsIAtom* aLocalName,
                                          nsAttrValue& aValue,
                                          nsIHTMLContent* aContent,
                                          nsHTMLStyleSheet* aSheet)
{
  nsRefPtr<nsMappedAttributes> mapped;
  nsresult rv = GetModifiableMapped(aContent, aSheet, PR_TRUE,
                                    getter_AddRefs(mapped));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mapped->SetAndTakeAttr(aLocalName, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return MakeMappedUnique(mapped);
}

nsresult
nsAttrAndChildArray::SetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet)
{
  if (!mImpl || !mImpl->mMappedAttrs ||
      aSheet == mImpl->mMappedAttrs->GetStyleSheet()) {
    return NS_OK;
  }

  nsRefPtr<nsMappedAttributes> mapped;
  nsresult rv = GetModifiableMapped(nsnull, nsnull, PR_FALSE, 
                                    getter_AddRefs(mapped));
  NS_ENSURE_SUCCESS(rv, rv);

  mapped->SetStyleSheet(aSheet);

  return MakeMappedUnique(mapped);
}

void
nsAttrAndChildArray::WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker)
{
  if (mImpl && mImpl->mMappedAttrs && aRuleWalker) {
    aRuleWalker->Forward(mImpl->mMappedAttrs);
  }
}

void
nsAttrAndChildArray::Compact()
{
  if (!mImpl) {
    return;
  }

  // First compress away empty attrslots
  PRUint32 slotCount = AttrSlotCount();
  PRUint32 attrCount = NonMappedAttrCount();
  PRUint32 childCount = ChildCount();

  if (attrCount < slotCount) {
    memmove(mImpl->mBuffer + attrCount * ATTRSIZE,
            mImpl->mBuffer + slotCount * ATTRSIZE,
            childCount * sizeof(nsIContent*));
    SetAttrSlotCount(attrCount);
  }

  // Then resize or free buffer
  PRUint32 newSize = attrCount * ATTRSIZE + childCount;
  if (!newSize && !mImpl->mMappedAttrs) {
    PR_Free(mImpl);
    mImpl = nsnull;
  }
  else if (newSize < mImpl->mBufferSize) {
    mImpl = NS_STATIC_CAST(Impl*, PR_Realloc(mImpl, (newSize + NS_IMPL_EXTRA_SIZE) * sizeof(nsIContent*)));
    NS_ASSERTION(mImpl, "failed to reallocate to smaller buffer");

    mImpl->mBufferSize = newSize;
  }
}

void
nsAttrAndChildArray::Clear()
{
  if (!mImpl) {
    return;
  }

  if (mImpl->mMappedAttrs) {
    NS_RELEASE(mImpl->mMappedAttrs);
  }

  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    ATTRS(mImpl)[i].~InternalAttr();
  }

  PRUint32 end = slotCount * ATTRSIZE + ChildCount();
  for (i = slotCount * ATTRSIZE; i < end; ++i) {
    nsIContent* child = NS_STATIC_CAST(nsIContent*, mImpl->mBuffer[i]);
    child->SetParent(nsnull); // XXX is it better to let the owner do this?
    NS_RELEASE(child);
  }

  SetAttrSlotAndChildCount(0, 0);
}

PRUint32
nsAttrAndChildArray::NonMappedAttrCount() const
{
  if (!mImpl) {
    return 0;
  }

  PRUint32 count = AttrSlotCount();
  while (count > 0 && !mImpl->mBuffer[(count - 1) * ATTRSIZE]) {
    --count;
  }

  return count;
}

PRUint32
nsAttrAndChildArray::MappedAttrCount() const
{
  return mImpl && mImpl->mMappedAttrs ? (PRUint32)mImpl->mMappedAttrs->Count() : 0;
}

nsresult
nsAttrAndChildArray::GetModifiableMapped(nsIHTMLContent* aContent,
                                         nsHTMLStyleSheet* aSheet,
                                         PRBool aWillAddAttr,
                                         nsMappedAttributes** aModifiable)
{
  *aModifiable = nsnull;

  if (mImpl && mImpl->mMappedAttrs) {
    *aModifiable = mImpl->mMappedAttrs->Clone(aWillAddAttr);
    NS_ENSURE_TRUE(*aModifiable, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aModifiable);
    
    return NS_OK;
  }

  NS_ASSERTION(aContent, "Trying to create modifiable without content");

  nsMapRuleToAttributesFunc mapRuleFunc;
  aContent->GetAttributeMappingFunction(mapRuleFunc);
  *aModifiable = new nsMappedAttributes(aSheet, mapRuleFunc);
  NS_ENSURE_TRUE(*aModifiable, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aModifiable);

  return NS_OK;
}

nsresult
nsAttrAndChildArray::MakeMappedUnique(nsMappedAttributes* aAttributes)
{
  NS_ASSERTION(aAttributes, "missing attributes");

  if (!mImpl && !GrowBy(1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!aAttributes->GetStyleSheet()) {
    // This doesn't currently happen, but it could if we do loading right

    nsRefPtr<nsMappedAttributes> mapped(aAttributes);
    mapped.swap(mImpl->mMappedAttrs);

    return NS_OK;
  }

  nsRefPtr<nsMappedAttributes> mapped =
    aAttributes->GetStyleSheet()->UniqueMappedAttributes(aAttributes);
  NS_ENSURE_TRUE(mapped, NS_ERROR_OUT_OF_MEMORY);

  if (mapped != aAttributes) {
    // Reset the stylesheet of aAttributes so that it doesn't spend time
    // trying to remove itself from the hash. There is no risk that aAttributes
    // is in the hash since it will always have come from GetModifiableMapped,
    // which never returns maps that are in the hash (such hashes are by
    // nature not modifiable).
    aAttributes->DropStyleSheetReference();
  }
  mapped.swap(mImpl->mMappedAttrs);

  return NS_OK;
}


PRBool
nsAttrAndChildArray::GrowBy(PRUint32 aGrowSize)
{
  PRUint32 size = mImpl ? mImpl->mBufferSize + NS_IMPL_EXTRA_SIZE : 0;
  PRUint32 minSize = size + aGrowSize;

  if (minSize <= ATTRCHILD_ARRAY_LINEAR_THRESHOLD) {
    do {
      size += ATTRCHILD_ARRAY_GROWSIZE;
    } while (size < minSize);
  }
  else {
    size = PR_BIT(PR_CeilingLog2(minSize));
  }

  Impl* newImpl = NS_STATIC_CAST(Impl*,
      mImpl ? PR_Realloc(mImpl, size * sizeof(void*)) :
              PR_Malloc(size * sizeof(void*)));
  NS_ENSURE_TRUE(newImpl, PR_FALSE);

  Impl* oldImpl = mImpl;
  mImpl = newImpl;

  // Set initial counts if we didn't have a buffer before
  if (!oldImpl) {
    mImpl->mMappedAttrs = nsnull;
    SetAttrSlotAndChildCount(0, 0);
  }

  mImpl->mBufferSize = size - NS_IMPL_EXTRA_SIZE;

  return PR_TRUE;
}

PRBool
nsAttrAndChildArray::AddAttrSlot()
{
  PRUint32 slotCount = AttrSlotCount();
  PRUint32 childCount = ChildCount();

  // Grow buffer if needed
  if (!(mImpl && mImpl->mBufferSize >= (slotCount + 1) * ATTRSIZE + childCount) &&
      !GrowBy(ATTRSIZE)) {
    return PR_FALSE;
  }
  void** offset = mImpl->mBuffer + slotCount * ATTRSIZE;

  if (childCount > 0) {
    memmove(&ATTRS(mImpl)[slotCount + 1], &ATTRS(mImpl)[slotCount],
            childCount * sizeof(nsIContent*));
  }

  SetAttrSlotCount(slotCount + 1);
  offset[0] = nsnull;
  offset[1] = nsnull;

  return PR_TRUE;
}
