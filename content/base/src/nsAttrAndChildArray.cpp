/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Storage of the children and attributes of a DOM node; storage for
 * the two is unified to minimize footprint.
 */

#include "nsAttrAndChildArray.h"
#include "nsMappedAttributeElement.h"
#include "prmem.h"
#include "prbit.h"
#include "nsString.h"
#include "nsHTMLStyleSheet.h"
#include "nsRuleWalker.h"
#include "nsMappedAttributes.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h" // nsAutoScriptBlocker

/*
CACHE_POINTER_SHIFT indicates how many steps to downshift the |this| pointer.
It should be small enough to not cause collisions between adjecent arrays, and
large enough to make sure that all indexes are used. The size below is based
on the size of the smallest possible element (currently 24[*] bytes) which is
the smallest distance between two nsAttrAndChildArray. 24/(2^_5_) is 0.75.
This means that two adjacent nsAttrAndChildArrays will overlap one in 4 times.
However not all elements will have enough children to get cached. And any
allocator that doesn't return addresses aligned to 64 bytes will ensure that
any index will get used.

[*] sizeof(nsGenericElement) + 4 bytes for nsIDOMElement vtable pointer.
*/

#define CACHE_POINTER_SHIFT 5
#define CACHE_NUM_SLOTS 128
#define CACHE_CHILD_LIMIT 10

#define CACHE_GET_INDEX(_array) \
  ((NS_PTR_TO_INT32(_array) >> CACHE_POINTER_SHIFT) & \
   (CACHE_NUM_SLOTS - 1))

struct IndexCacheSlot
{
  const nsAttrAndChildArray* array;
  PRInt32 index;
};

// This is inited to all zeroes since it's static. Though even if it wasn't
// the worst thing that'd happen is a small inefficency if you'd get a false
// positive cachehit.
static IndexCacheSlot indexCache[CACHE_NUM_SLOTS];

static
inline
void
AddIndexToCache(const nsAttrAndChildArray* aArray, PRInt32 aIndex)
{
  PRUint32 ix = CACHE_GET_INDEX(aArray);
  indexCache[ix].array = aArray;
  indexCache[ix].index = aIndex;
}

static
inline
PRInt32
GetIndexFromCache(const nsAttrAndChildArray* aArray)
{
  PRUint32 ix = CACHE_GET_INDEX(aArray);
  return indexCache[ix].array == aArray ? indexCache[ix].index : -1;
}


/**
 * Due to a compiler bug in VisualAge C++ for AIX, we need to return the 
 * address of the first index into mBuffer here, instead of simply returning 
 * mBuffer itself.
 *
 * See Bug 231104 for more information.
 */
#define ATTRS(_impl) \
  reinterpret_cast<InternalAttr*>(&((_impl)->mBuffer[0]))
  

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

nsIContent * const *
nsAttrAndChildArray::GetChildArray(PRUint32* aChildCount) const
{
  *aChildCount = ChildCount();
  
  if (!*aChildCount) {
    return nsnull;
  }
  
  return reinterpret_cast<nsIContent**>(mImpl->mBuffer + AttrSlotsSize());
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
    SetChildAtPos(pos, aChild, aPos, childCount);

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
    memmove(&newStart[aPos + 1], &oldStart[aPos],
            (childCount - aPos) * sizeof(nsIContent*));
    SetChildAtPos(newStart + aPos, aChild, aPos, childCount);

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
  SetChildAtPos(pos, aChild, aPos, childCount);

  SetChildCount(childCount + 1);
  
  return NS_OK;
}

void
nsAttrAndChildArray::RemoveChildAt(PRUint32 aPos)
{
  // Just store the return value of TakeChildAt in an nsCOMPtr to
  // trigger a release.
  nsCOMPtr<nsIContent> child = TakeChildAt(aPos);
}

already_AddRefed<nsIContent>
nsAttrAndChildArray::TakeChildAt(PRUint32 aPos)
{
  NS_ASSERTION(aPos < ChildCount(), "out-of-bounds");

  PRUint32 childCount = ChildCount();
  void** pos = mImpl->mBuffer + AttrSlotsSize() + aPos;
  nsIContent* child = static_cast<nsIContent*>(*pos);
  if (child->mPreviousSibling) {
    child->mPreviousSibling->mNextSibling = child->mNextSibling;
  }
  if (child->mNextSibling) {
    child->mNextSibling->mPreviousSibling = child->mPreviousSibling;
  }
  child->mPreviousSibling = child->mNextSibling = nsnull;

  memmove(pos, pos + 1, (childCount - aPos - 1) * sizeof(nsIContent*));
  SetChildCount(childCount - 1);

  return child;
}

PRInt32
nsAttrAndChildArray::IndexOfChild(nsINode* aPossibleChild) const
{
  if (!mImpl) {
    return -1;
  }
  void** children = mImpl->mBuffer + AttrSlotsSize();
  // Use signed here since we compare count to cursor which has to be signed
  PRInt32 i, count = ChildCount();

  if (count >= CACHE_CHILD_LIMIT) {
    PRInt32 cursor = GetIndexFromCache(this);
    // Need to compare to count here since we may have removed children since
    // the index was added to the cache.
    // We're also relying on that GetIndexFromCache returns -1 if no cached
    // index was found.
    if (cursor >= count) {
      cursor = -1;
    }

    // Seek outward from the last found index. |inc| will change sign every
    // run through the loop. |sign| just exists to make sure the absolute
    // value of |inc| increases each time through.
    PRInt32 inc = 1, sign = 1;
    while (cursor >= 0 && cursor < count) {
      if (children[cursor] == aPossibleChild) {
        AddIndexToCache(this, cursor);

        return cursor;
      }

      cursor += inc;
      inc = -inc - sign;
      sign = -sign;
    }

    // We ran into one 'edge'. Add inc to cursor once more to get back to
    // the 'side' where we still need to search, then step in the |sign|
    // direction.
    cursor += inc;

    if (sign > 0) {
      for (; cursor < count; ++cursor) {
        if (children[cursor] == aPossibleChild) {
          AddIndexToCache(this, cursor);

          return static_cast<PRInt32>(cursor);
        }
      }
    }
    else {
      for (; cursor >= 0; --cursor) {
        if (children[cursor] == aPossibleChild) {
          AddIndexToCache(this, cursor);

          return static_cast<PRInt32>(cursor);
        }
      }
    }

    // The child wasn't even in the remaining children
    return -1;
  }

  for (i = 0; i < count; ++i) {
    if (children[i] == aPossibleChild) {
      return static_cast<PRInt32>(i);
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
    for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        return &ATTRS(mImpl)[i].mValue;
      }
    }

    if (mImpl && mImpl->mMappedAttrs) {
      return mImpl->mMappedAttrs->GetAttr(aLocalName);
    }
  }
  else {
    for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName, aNamespaceID)) {
        return &ATTRS(mImpl)[i].mValue;
      }
    }
  }

  return nsnull;
}

const nsAttrValue*
nsAttrAndChildArray::GetAttr(const nsAString& aName,
                             nsCaseTreatment aCaseSensitive) const
{
  // Check whether someone is being silly and passing non-lowercase
  // attr names.
  if (aCaseSensitive == eIgnoreCase &&
      nsContentUtils::StringContainsASCIIUpper(aName)) {
    // Try again with a lowercased name, but make sure we can't reenter this
    // block by passing eCaseSensitive for aCaseSensitive.
    nsAutoString lowercase;
    nsContentUtils::ASCIIToLower(aName, lowercase);
    return GetAttr(lowercase, eCaseMatters);
  }

  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    if (ATTRS(mImpl)[i].mName.QualifiedNameEquals(aName)) {
      return &ATTRS(mImpl)[i].mValue;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    const nsAttrValue* val =
      mImpl->mMappedAttrs->GetAttr(aName);
    if (val) {
      return val;
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
nsAttrAndChildArray::SetAndTakeAttr(nsIAtom* aLocalName, nsAttrValue& aValue)
{
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
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
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
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
nsAttrAndChildArray::RemoveAttrAt(PRUint32 aPos, nsAttrValue& aValue)
{
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds");

  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    if (mapped == 1) {
      // We're removing the last mapped attribute.  Can't swap in this
      // case; have to copy.
      aValue.SetTo(*mImpl->mMappedAttrs->AttrAt(0));
      NS_RELEASE(mImpl->mMappedAttrs);

      return NS_OK;
    }

    nsRefPtr<nsMappedAttributes> mapped;
    nsresult rv = GetModifiableMapped(nsnull, nsnull, false,
                                      getter_AddRefs(mapped));
    NS_ENSURE_SUCCESS(rv, rv);

    mapped->RemoveAttrAt(aPos, aValue);

    return MakeMappedUnique(mapped);
  }

  aPos -= mapped;
  ATTRS(mImpl)[aPos].mValue.SwapValueWith(aValue);
  ATTRS(mImpl)[aPos].~InternalAttr();

  PRUint32 slotCount = AttrSlotCount();
  memmove(&ATTRS(mImpl)[aPos],
          &ATTRS(mImpl)[aPos + 1],
          (slotCount - aPos - 1) * sizeof(InternalAttr));
  memset(&ATTRS(mImpl)[slotCount - 1], nsnull, sizeof(InternalAttr));

  return NS_OK;
}

const nsAttrName*
nsAttrAndChildArray::AttrNameAt(PRUint32 aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in nsAttrAndChildArray");

  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    return mImpl->mMappedAttrs->NameAt(aPos);
  }

  return &ATTRS(mImpl)[aPos - mapped].mName;
}

const nsAttrName*
nsAttrAndChildArray::GetSafeAttrNameAt(PRUint32 aPos) const
{
  PRUint32 mapped = MappedAttrCount();
  if (aPos < mapped) {
    return mImpl->mMappedAttrs->NameAt(aPos);
  }

  aPos -= mapped;
  if (aPos >= AttrSlotCount()) {
    return nsnull;
  }

  void** pos = mImpl->mBuffer + aPos * ATTRSIZE;
  if (!*pos) {
    return nsnull;
  }

  return &reinterpret_cast<InternalAttr*>(pos)->mName;
}

const nsAttrName*
nsAttrAndChildArray::GetExistingAttrNameFromQName(const nsAString& aName) const
{
  PRUint32 i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
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
  if (mImpl && mImpl->mMappedAttrs && aNamespaceID == kNameSpaceID_None) {
    idx = mImpl->mMappedAttrs->IndexOfAttr(aLocalName);
    if (idx >= 0) {
      return idx;
    }
  }

  PRUint32 i;
  PRUint32 mapped = MappedAttrCount();
  PRUint32 slotCount = AttrSlotCount();
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        return i + mapped;
      }
    }
  }
  else {
    for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
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
                                          nsMappedAttributeElement* aContent,
                                          nsHTMLStyleSheet* aSheet)
{
  nsRefPtr<nsMappedAttributes> mapped;

  bool willAdd = true;
  if (mImpl && mImpl->mMappedAttrs) {
    willAdd = mImpl->mMappedAttrs->GetAttr(aLocalName) == nsnull;
  }

  nsresult rv = GetModifiableMapped(aContent, aSheet, willAdd,
                                    getter_AddRefs(mapped));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mapped->SetAndTakeAttr(aLocalName, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return MakeMappedUnique(mapped);
}

nsresult
nsAttrAndChildArray::DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet)
{
  NS_PRECONDITION(mImpl && mImpl->mMappedAttrs,
                  "Should have mapped attrs here!");
  if (aSheet == mImpl->mMappedAttrs->GetStyleSheet()) {
    return NS_OK;
  }

  nsRefPtr<nsMappedAttributes> mapped;
  nsresult rv = GetModifiableMapped(nsnull, nsnull, false, 
                                    getter_AddRefs(mapped));
  NS_ENSURE_SUCCESS(rv, rv);

  mapped->SetStyleSheet(aSheet);

  return MakeMappedUnique(mapped);
}

void
nsAttrAndChildArray::WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker)
{
  if (mImpl && mImpl->mMappedAttrs) {
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
    mImpl = static_cast<Impl*>(PR_Realloc(mImpl, (newSize + NS_IMPL_EXTRA_SIZE) * sizeof(nsIContent*)));
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
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    ATTRS(mImpl)[i].~InternalAttr();
  }

  nsAutoScriptBlocker scriptBlocker;
  PRUint32 end = slotCount * ATTRSIZE + ChildCount();
  for (i = slotCount * ATTRSIZE; i < end; ++i) {
    nsIContent* child = static_cast<nsIContent*>(mImpl->mBuffer[i]);
    // making this false so tree teardown doesn't end up being
    // O(N*D) (number of nodes times average depth of tree).
    child->UnbindFromTree(false); // XXX is it better to let the owner do this?
    // Make sure to unlink our kids from each other, since someone
    // else could stil be holding references to some of them.

    // XXXbz We probably can't push this assignment down into the |aNullParent|
    // case of UnbindFromTree because we still need the assignment in
    // RemoveChildAt.  In particular, ContentRemoved fires between
    // RemoveChildAt and UnbindFromTree, and in ContentRemoved the sibling
    // chain needs to be correct.  Though maybe we could set the prev and next
    // to point to each other but keep the kid being removed pointing to them
    // through ContentRemoved so consumers can find where it used to be in the
    // list?
    child->mPreviousSibling = child->mNextSibling = nsnull;
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
nsAttrAndChildArray::GetModifiableMapped(nsMappedAttributeElement* aContent,
                                         nsHTMLStyleSheet* aSheet,
                                         bool aWillAddAttr,
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

  nsMapRuleToAttributesFunc mapRuleFunc =
    aContent->GetAttributeMappingFunction();
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


bool
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

  bool needToInitialize = !mImpl;
  Impl* newImpl = static_cast<Impl*>(PR_Realloc(mImpl, size * sizeof(void*)));
  NS_ENSURE_TRUE(newImpl, false);

  mImpl = newImpl;

  // Set initial counts if we didn't have a buffer before
  if (needToInitialize) {
    mImpl->mMappedAttrs = nsnull;
    SetAttrSlotAndChildCount(0, 0);
  }

  mImpl->mBufferSize = size - NS_IMPL_EXTRA_SIZE;

  return true;
}

bool
nsAttrAndChildArray::AddAttrSlot()
{
  PRUint32 slotCount = AttrSlotCount();
  PRUint32 childCount = ChildCount();

  // Grow buffer if needed
  if (!(mImpl && mImpl->mBufferSize >= (slotCount + 1) * ATTRSIZE + childCount) &&
      !GrowBy(ATTRSIZE)) {
    return false;
  }
  void** offset = mImpl->mBuffer + slotCount * ATTRSIZE;

  if (childCount > 0) {
    memmove(&ATTRS(mImpl)[slotCount + 1], &ATTRS(mImpl)[slotCount],
            childCount * sizeof(nsIContent*));
  }

  SetAttrSlotCount(slotCount + 1);
  offset[0] = nsnull;
  offset[1] = nsnull;

  return true;
}

inline void
nsAttrAndChildArray::SetChildAtPos(void** aPos, nsIContent* aChild,
                                   PRUint32 aIndex, PRUint32 aChildCount)
{
  NS_PRECONDITION(!aChild->GetNextSibling(), "aChild with next sibling?");
  NS_PRECONDITION(!aChild->GetPreviousSibling(), "aChild with prev sibling?");

  *aPos = aChild;
  NS_ADDREF(aChild);
  if (aIndex != 0) {
    nsIContent* previous = static_cast<nsIContent*>(*(aPos - 1));
    aChild->mPreviousSibling = previous;
    previous->mNextSibling = aChild;
  }
  if (aIndex != aChildCount) {
    nsIContent* next = static_cast<nsIContent*>(*(aPos + 1));
    aChild->mNextSibling = next;
    next->mPreviousSibling = aChild;
  }
}

size_t
nsAttrAndChildArray::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = 0;
  if (mImpl) {
    // Don't add the size taken by *mMappedAttrs because it's shared.

    n += aMallocSizeOf(mImpl);

    PRUint32 slotCount = AttrSlotCount();
    for (PRUint32 i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      nsAttrValue* value = &ATTRS(mImpl)[i].mValue;
      n += value->SizeOfExcludingThis(aMallocSizeOf);
    }
  }

  return n;
}

