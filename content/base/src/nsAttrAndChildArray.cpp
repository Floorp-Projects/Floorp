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

#define ATTRS(_impl) \
  NS_REINTERPRET_CAST(InternalAttr*, (_impl)->mBuffer)
  

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

  Clear();

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
    PRUint32 attrCount = AttrCount();
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

void
nsAttrAndChildArray::ReplaceChildAt(nsIContent* aChild, PRUint32 aPos)
{
  NS_ASSERTION(aPos < ChildCount(), "out-of-bounds");
  void** pos = mImpl->mBuffer + AttrSlotsSize() + aPos;
  nsIContent* child = NS_STATIC_CAST(nsIContent*, *pos);
  NS_RELEASE(child);
  *pos = aChild;
  NS_ADDREF(aChild);
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
  if (!mImpl) {
    return 0;
  }

  PRUint32 count = AttrSlotCount();
  while (count > 0 && !mImpl->mBuffer[(count - 1) * ATTRSIZE]) {
    --count;
  }

  return count;
}

const nsAttrValue*
nsAttrAndChildArray::GetAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRInt32 i = IndexOfAttr(aLocalName, aNamespaceID);
  if (i < 0) {
    return nsnull;
  }

  return &ATTRS(mImpl)[i].mValue;
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
nsAttrAndChildArray::SetAttr(nsIAtom* aLocalName, nsHTMLValue* aValue)
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
nsAttrAndChildArray::SetAttr(nsINodeInfo* aName, const nsAString& aValue)
{
  PRInt32 namespaceID = aName->NamespaceID();
  nsIAtom* localName = aName->NameAtom();
  if (namespaceID == kNameSpaceID_None) {
    return SetAttr(localName, aValue);
  }

  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(localName, namespaceID)) {
      ATTRS(mImpl)[i].mName.SetTo(aName);
      ATTRS(mImpl)[i].mValue.SetTo(aValue);

      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(slotCount < ATTRCHILD_ARRAY_MAX_ATTR_COUNT,
                 NS_ERROR_FAILURE);

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue(aValue);

  return NS_OK;
}


void
nsAttrAndChildArray::RemoveAttrAt(PRUint32 aPos)
{
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds");

  ATTRS(mImpl)[aPos].~InternalAttr();

  PRUint32 slotCount = AttrSlotCount();
  memmove(&ATTRS(mImpl)[aPos],
          &ATTRS(mImpl)[aPos + 1],
          (slotCount - aPos - 1) * sizeof(InternalAttr));
  memset(&ATTRS(mImpl)[slotCount - 1], nsnull, sizeof(InternalAttr));
}

const nsAttrName*
nsAttrAndChildArray::GetSafeAttrNameAt(PRUint32 aPos) const
{
  // Warn here since we should make this non-bounds safe
  PRUint32 slotCount = AttrSlotCount();
  NS_ENSURE_TRUE(aPos < slotCount, nsnull);

  void** pos = mImpl->mBuffer + aPos * ATTRSIZE;
  NS_ENSURE_TRUE(*pos, nsnull);

  return &NS_REINTERPRET_CAST(InternalAttr*, pos)->mName;
}

const nsAttrName*
nsAttrAndChildArray::GetExistingAttrNameFromQName(const nsAString& aName) const
{
  NS_ConvertUTF16toUTF8 name(aName);
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
    if (ATTRS(mImpl)[i].mName.QualifiedNameEquals(name)) {
      return &ATTRS(mImpl)[i].mName;
    }
  }

  return nsnull;
}

PRInt32
nsAttrAndChildArray::IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const
{
  PRUint32 i, slotCount = AttrSlotCount();
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        return NS_STATIC_CAST(PRInt32, i);
      }
    }
  }
  else {
    for (i = 0; i < slotCount && mImpl->mBuffer[i * ATTRSIZE]; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName, aNamespaceID)) {
        return NS_STATIC_CAST(PRInt32, i);
      }
    }
  }

  return -1;
}

void
nsAttrAndChildArray::Compact()
{
  if (!mImpl) {
    return;
  }

  // First compress away empty attrslots
  PRUint32 slotCount = AttrSlotCount();
  PRUint32 attrCount = AttrCount();
  PRUint32 childCount = ChildCount();

  if (attrCount < slotCount) {
    memmove(mImpl->mBuffer + attrCount * ATTRSIZE,
            mImpl->mBuffer + slotCount * ATTRSIZE,
            childCount * sizeof(nsIContent*));
    SetAttrSlotCount(attrCount);
  }

  // Then resize or free buffer
  PRUint32 newSize = attrCount * ATTRSIZE + childCount;
  if (!newSize) {
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
  memmove(&ATTRS(mImpl)[slotCount + 1], &ATTRS(mImpl)[slotCount],
          childCount * sizeof(nsIContent*));

  SetAttrSlotCount(slotCount + 1);
  offset[0] = nsnull;
  offset[1] = nsnull;

  return PR_TRUE;
}
