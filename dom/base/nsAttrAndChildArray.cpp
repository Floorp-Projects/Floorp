/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Storage of the children and attributes of a DOM node; storage for
 * the two is unified to minimize footprint.
 */

#include "nsAttrAndChildArray.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"

#include "nsMappedAttributeElement.h"
#include "nsString.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h" // nsAutoScriptBlocker

using mozilla::CheckedUint32;

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

[*] sizeof(Element) + 4 bytes for nsIDOMNode vtable pointer.
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
  int32_t index;
};

// This is inited to all zeroes since it's static. Though even if it wasn't
// the worst thing that'd happen is a small inefficency if you'd get a false
// positive cachehit.
static IndexCacheSlot indexCache[CACHE_NUM_SLOTS];

static
inline
void
AddIndexToCache(const nsAttrAndChildArray* aArray, int32_t aIndex)
{
  uint32_t ix = CACHE_GET_INDEX(aArray);
  indexCache[ix].array = aArray;
  indexCache[ix].index = aIndex;
}

static
inline
int32_t
GetIndexFromCache(const nsAttrAndChildArray* aArray)
{
  uint32_t ix = CACHE_GET_INDEX(aArray);
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
  : mImpl(nullptr)
{
}

nsAttrAndChildArray::~nsAttrAndChildArray()
{
  if (!mImpl) {
    return;
  }

  Clear();

  free(mImpl);
}

nsIContent*
nsAttrAndChildArray::GetSafeChildAt(uint32_t aPos) const
{
  if (aPos < ChildCount()) {
    return ChildAt(aPos);
  }

  return nullptr;
}

nsIContent * const *
nsAttrAndChildArray::GetChildArray(uint32_t* aChildCount) const
{
  *aChildCount = ChildCount();

  if (!*aChildCount) {
    return nullptr;
  }

  return reinterpret_cast<nsIContent**>(mImpl->mBuffer + AttrSlotsSize());
}

nsresult
nsAttrAndChildArray::InsertChildAt(nsIContent* aChild, uint32_t aPos)
{
  NS_ASSERTION(aChild, "nullchild");
  NS_ASSERTION(aPos <= ChildCount(), "out-of-bounds");

  uint32_t offset = AttrSlotsSize();
  uint32_t childCount = ChildCount();

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
    uint32_t attrCount = NonMappedAttrCount();
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
nsAttrAndChildArray::RemoveChildAt(uint32_t aPos)
{
  // Just store the return value of TakeChildAt in an nsCOMPtr to
  // trigger a release.
  nsCOMPtr<nsIContent> child = TakeChildAt(aPos);
}

already_AddRefed<nsIContent>
nsAttrAndChildArray::TakeChildAt(uint32_t aPos)
{
  NS_ASSERTION(aPos < ChildCount(), "out-of-bounds");

  uint32_t childCount = ChildCount();
  void** pos = mImpl->mBuffer + AttrSlotsSize() + aPos;
  nsIContent* child = static_cast<nsIContent*>(*pos);
  if (child->mPreviousSibling) {
    child->mPreviousSibling->mNextSibling = child->mNextSibling;
  }
  if (child->mNextSibling) {
    child->mNextSibling->mPreviousSibling = child->mPreviousSibling;
  }
  child->mPreviousSibling = child->mNextSibling = nullptr;

  memmove(pos, pos + 1, (childCount - aPos - 1) * sizeof(nsIContent*));
  SetChildCount(childCount - 1);

  return dont_AddRef(child);
}

int32_t
nsAttrAndChildArray::IndexOfChild(const nsINode* aPossibleChild) const
{
  if (!mImpl) {
    return -1;
  }
  void** children = mImpl->mBuffer + AttrSlotsSize();
  // Use signed here since we compare count to cursor which has to be signed
  int32_t i, count = ChildCount();

  if (count >= CACHE_CHILD_LIMIT) {
    int32_t cursor = GetIndexFromCache(this);
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
    int32_t inc = 1, sign = 1;
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

          return static_cast<int32_t>(cursor);
        }
      }
    }
    else {
      for (; cursor >= 0; --cursor) {
        if (children[cursor] == aPossibleChild) {
          AddIndexToCache(this, cursor);

          return static_cast<int32_t>(cursor);
        }
      }
    }

    // The child wasn't even in the remaining children
    return -1;
  }

  for (i = 0; i < count; ++i) {
    if (children[i] == aPossibleChild) {
      return static_cast<int32_t>(i);
    }
  }

  return -1;
}

uint32_t
nsAttrAndChildArray::AttrCount() const
{
  return NonMappedAttrCount() + MappedAttrCount();
}

const nsAttrValue*
nsAttrAndChildArray::GetAttr(nsAtom* aLocalName, int32_t aNamespaceID) const
{
  uint32_t i, slotCount = AttrSlotCount();
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

  return nullptr;
}

const nsAttrValue*
nsAttrAndChildArray::GetAttr(const nsAString& aLocalName) const
{
  uint32_t i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
      return &ATTRS(mImpl)[i].mValue;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetAttr(aLocalName);
  }

  return nullptr;
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

  uint32_t i, slotCount = AttrSlotCount();
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

  return nullptr;
}

const nsAttrValue*
nsAttrAndChildArray::AttrAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in nsAttrAndChildArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &ATTRS(mImpl)[aPos].mValue;
  }

  return mImpl->mMappedAttrs->AttrAt(aPos - nonmapped);
}

nsresult
nsAttrAndChildArray::SetAndSwapAttr(nsAtom* aLocalName, nsAttrValue& aValue,
                                    bool* aHadValue)
{
  *aHadValue = false;
  uint32_t i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
      ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);
      *aHadValue = true;
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
nsAttrAndChildArray::SetAndSwapAttr(mozilla::dom::NodeInfo* aName,
                                    nsAttrValue& aValue, bool* aHadValue)
{
  int32_t namespaceID = aName->NamespaceID();
  nsAtom* localName = aName->NameAtom();
  if (namespaceID == kNameSpaceID_None) {
    return SetAndSwapAttr(localName, aValue, aHadValue);
  }

  *aHadValue = false;
  uint32_t i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    if (ATTRS(mImpl)[i].mName.Equals(localName, namespaceID)) {
      ATTRS(mImpl)[i].mName.SetTo(aName);
      ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);
      *aHadValue = true;
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
nsAttrAndChildArray::RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue)
{
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    ATTRS(mImpl)[aPos].mValue.SwapValueWith(aValue);
    ATTRS(mImpl)[aPos].~InternalAttr();

    uint32_t slotCount = AttrSlotCount();
    memmove(&ATTRS(mImpl)[aPos],
            &ATTRS(mImpl)[aPos + 1],
            (slotCount - aPos - 1) * sizeof(InternalAttr));
    memset(&ATTRS(mImpl)[slotCount - 1], 0, sizeof(InternalAttr));

    return NS_OK;
  }

  if (MappedAttrCount() == 1) {
    // We're removing the last mapped attribute.  Can't swap in this
    // case; have to copy.
    aValue.SetTo(*mImpl->mMappedAttrs->AttrAt(0));
    NS_RELEASE(mImpl->mMappedAttrs);

    return NS_OK;
  }

  RefPtr<nsMappedAttributes> mapped =
    GetModifiableMapped(nullptr, nullptr, false);

  mapped->RemoveAttrAt(aPos - nonmapped, aValue);

  return MakeMappedUnique(mapped);
}

mozilla::dom::BorrowedAttrInfo
nsAttrAndChildArray::AttrInfoAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in nsAttrAndChildArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return BorrowedAttrInfo(&ATTRS(mImpl)[aPos].mName, &ATTRS(mImpl)[aPos].mValue);
  }

  return BorrowedAttrInfo(mImpl->mMappedAttrs->NameAt(aPos - nonmapped),
                    mImpl->mMappedAttrs->AttrAt(aPos - nonmapped));
}

const nsAttrName*
nsAttrAndChildArray::AttrNameAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in nsAttrAndChildArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &ATTRS(mImpl)[aPos].mName;
  }

  return mImpl->mMappedAttrs->NameAt(aPos - nonmapped);
}

const nsAttrName*
nsAttrAndChildArray::GetSafeAttrNameAt(uint32_t aPos) const
{
  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    void** pos = mImpl->mBuffer + aPos * ATTRSIZE;
    if (!*pos) {
      return nullptr;
    }

    return &reinterpret_cast<InternalAttr*>(pos)->mName;
  }

  if (aPos >= AttrCount()) {
    return nullptr;
  }

  return mImpl->mMappedAttrs->NameAt(aPos - nonmapped);
}

const nsAttrName*
nsAttrAndChildArray::GetExistingAttrNameFromQName(const nsAString& aName) const
{
  uint32_t i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    if (ATTRS(mImpl)[i].mName.QualifiedNameEquals(aName)) {
      return &ATTRS(mImpl)[i].mName;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetExistingAttrNameFromQName(aName);
  }

  return nullptr;
}

int32_t
nsAttrAndChildArray::IndexOfAttr(nsAtom* aLocalName, int32_t aNamespaceID) const
{
  int32_t idx;
  if (mImpl && mImpl->mMappedAttrs && aNamespaceID == kNameSpaceID_None) {
    idx = mImpl->mMappedAttrs->IndexOfAttr(aLocalName);
    if (idx >= 0) {
      return NonMappedAttrCount() + idx;
    }
  }

  uint32_t i;
  uint32_t slotCount = AttrSlotCount();
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    // Note that here we don't check for AttrSlotIsTaken() in the loop
    // condition for the sake of performance because comparing aLocalName
    // against null would fail in the loop body (since Equals() just compares
    // the raw pointer value of aLocalName to what AttrSlotIsTaken() would be
    // checking.
    for (i = 0; i < slotCount; ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName)) {
        MOZ_ASSERT(AttrSlotIsTaken(i), "sanity check");
        return i;
      }
    }
  }
  else {
    for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      if (ATTRS(mImpl)[i].mName.Equals(aLocalName, aNamespaceID)) {
        return i;
      }
    }
  }

  return -1;
}

nsresult
nsAttrAndChildArray::SetAndSwapMappedAttr(nsAtom* aLocalName,
                                          nsAttrValue& aValue,
                                          nsMappedAttributeElement* aContent,
                                          nsHTMLStyleSheet* aSheet,
                                          bool* aHadValue)
{
  bool willAdd = true;
  if (mImpl && mImpl->mMappedAttrs) {
    willAdd = !mImpl->mMappedAttrs->GetAttr(aLocalName);
  }

  RefPtr<nsMappedAttributes> mapped =
    GetModifiableMapped(aContent, aSheet, willAdd);

  mapped->SetAndSwapAttr(aLocalName, aValue, aHadValue);

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

  RefPtr<nsMappedAttributes> mapped =
    GetModifiableMapped(nullptr, nullptr, false);

  mapped->SetStyleSheet(aSheet);

  return MakeMappedUnique(mapped);
}


void
nsAttrAndChildArray::Compact()
{
  if (!mImpl) {
    return;
  }

  // First compress away empty attrslots
  uint32_t slotCount = AttrSlotCount();
  uint32_t attrCount = NonMappedAttrCount();
  uint32_t childCount = ChildCount();

  if (attrCount < slotCount) {
    memmove(mImpl->mBuffer + attrCount * ATTRSIZE,
            mImpl->mBuffer + slotCount * ATTRSIZE,
            childCount * sizeof(nsIContent*));
    SetAttrSlotCount(attrCount);
  }

  // Then resize or free buffer
  uint32_t newSize = attrCount * ATTRSIZE + childCount;
  if (!newSize && !mImpl->mMappedAttrs) {
    free(mImpl);
    mImpl = nullptr;
  }
  else if (newSize < mImpl->mBufferSize) {
    mImpl = static_cast<Impl*>(realloc(mImpl, (newSize + NS_IMPL_EXTRA_SIZE) * sizeof(nsIContent*)));
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

  uint32_t i, slotCount = AttrSlotCount();
  for (i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
    ATTRS(mImpl)[i].~InternalAttr();
  }

  nsAutoScriptBlocker scriptBlocker;
  uint32_t end = slotCount * ATTRSIZE + ChildCount();
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
    child->mPreviousSibling = child->mNextSibling = nullptr;
    NS_RELEASE(child);
  }

  SetAttrSlotAndChildCount(0, 0);
}

uint32_t
nsAttrAndChildArray::NonMappedAttrCount() const
{
  if (!mImpl) {
    return 0;
  }

  uint32_t count = AttrSlotCount();
  while (count > 0 && !mImpl->mBuffer[(count - 1) * ATTRSIZE]) {
    --count;
  }

  return count;
}

uint32_t
nsAttrAndChildArray::MappedAttrCount() const
{
  return mImpl && mImpl->mMappedAttrs ? (uint32_t)mImpl->mMappedAttrs->Count() : 0;
}

nsresult
nsAttrAndChildArray::ForceMapped(nsMappedAttributeElement* aContent, nsIDocument* aDocument)
{
  nsHTMLStyleSheet* sheet = aDocument->GetAttributeStyleSheet();
  RefPtr<nsMappedAttributes> mapped = GetModifiableMapped(aContent, sheet, false, 0);
  return MakeMappedUnique(mapped);
}

void
nsAttrAndChildArray::ClearMappedServoStyle() {
  if (mImpl && mImpl->mMappedAttrs) {
    mImpl->mMappedAttrs->ClearServoStyle();
  }
}

nsMappedAttributes*
nsAttrAndChildArray::GetModifiableMapped(nsMappedAttributeElement* aContent,
                                         nsHTMLStyleSheet* aSheet,
                                         bool aWillAddAttr,
                                         int32_t aAttrCount)
{
  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->Clone(aWillAddAttr);
  }

  MOZ_ASSERT(aContent, "Trying to create modifiable without content");

  nsMapRuleToAttributesFunc mapRuleFunc =
    aContent->GetAttributeMappingFunction();
  return new (aAttrCount) nsMappedAttributes(aSheet, mapRuleFunc);
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

    RefPtr<nsMappedAttributes> mapped(aAttributes);
    mapped.swap(mImpl->mMappedAttrs);

    return NS_OK;
  }

  RefPtr<nsMappedAttributes> mapped =
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

const nsMappedAttributes*
nsAttrAndChildArray::GetMapped() const
{
  return mImpl ? mImpl->mMappedAttrs : nullptr;
}

nsresult nsAttrAndChildArray::EnsureCapacityToClone(const nsAttrAndChildArray& aOther,
                                                    bool aAllocateChildren)
{
  NS_PRECONDITION(!mImpl, "nsAttrAndChildArray::EnsureCapacityToClone requires the array be empty when called");

  uint32_t attrCount = aOther.NonMappedAttrCount();
  uint32_t childCount = 0;
  if (aAllocateChildren) {
    childCount = aOther.ChildCount();
  }

  if (attrCount == 0 && childCount == 0) {
    return NS_OK;
  }

  // No need to use a CheckedUint32 because we are cloning. We know that we
  // have already allocated an nsAttrAndChildArray of this size.
  uint32_t size = attrCount;
  size *= ATTRSIZE;
  size += childCount;
  uint32_t totalSize = size;
  totalSize += NS_IMPL_EXTRA_SIZE;

  mImpl = static_cast<Impl*>(malloc(totalSize * sizeof(void*)));
  NS_ENSURE_TRUE(mImpl, NS_ERROR_OUT_OF_MEMORY);

  mImpl->mMappedAttrs = nullptr;
  mImpl->mBufferSize = size;

  // The array is now the right size, but we should reserve the correct
  // number of slots for attributes so that children don't get written into
  // that part of the array (which will then need to be moved later).
  memset(static_cast<void*>(mImpl->mBuffer), 0, sizeof(InternalAttr) * attrCount);
  SetAttrSlotAndChildCount(attrCount, 0);

  return NS_OK;
}

bool
nsAttrAndChildArray::GrowBy(uint32_t aGrowSize)
{
  CheckedUint32 size = 0;
  if (mImpl) {
    size += mImpl->mBufferSize;
    size += NS_IMPL_EXTRA_SIZE;
    if (!size.isValid()) {
      return false;
    }
  }

  CheckedUint32 minSize = size.value();
  minSize += aGrowSize;
  if (!minSize.isValid()) {
    return false;
  }

  if (minSize.value() <= ATTRCHILD_ARRAY_LINEAR_THRESHOLD) {
    do {
      size += ATTRCHILD_ARRAY_GROWSIZE;
      if (!size.isValid()) {
        return false;
      }
    } while (size.value() < minSize.value());
  }
  else {
    uint32_t shift = mozilla::CeilingLog2(minSize.value());
    if (shift >= 32) {
      return false;
    }

    size = 1u << shift;
  }

  bool needToInitialize = !mImpl;
  CheckedUint32 neededSize = size;
  neededSize *= sizeof(void*);
  if (!neededSize.isValid()) {
    return false;
  }

  Impl* newImpl = static_cast<Impl*>(realloc(mImpl, neededSize.value()));
  NS_ENSURE_TRUE(newImpl, false);

  mImpl = newImpl;

  // Set initial counts if we didn't have a buffer before
  if (needToInitialize) {
    mImpl->mMappedAttrs = nullptr;
    SetAttrSlotAndChildCount(0, 0);
  }

  mImpl->mBufferSize = size.value() - NS_IMPL_EXTRA_SIZE;

  return true;
}

bool
nsAttrAndChildArray::AddAttrSlot()
{
  uint32_t slotCount = AttrSlotCount();
  uint32_t childCount = ChildCount();

  CheckedUint32 size = slotCount;
  size += 1;
  size *= ATTRSIZE;
  size += childCount;
  if (!size.isValid()) {
    return false;
  }

  // Grow buffer if needed
  if (!(mImpl && mImpl->mBufferSize >= size.value()) &&
      !GrowBy(ATTRSIZE)) {
    return false;
  }

  void** offset = mImpl->mBuffer + slotCount * ATTRSIZE;

  if (childCount > 0) {
    memmove(&ATTRS(mImpl)[slotCount + 1], &ATTRS(mImpl)[slotCount],
            childCount * sizeof(nsIContent*));
  }

  SetAttrSlotCount(slotCount + 1);
  memset(static_cast<void*>(offset), 0, sizeof(InternalAttr));

  return true;
}

inline void
nsAttrAndChildArray::SetChildAtPos(void** aPos, nsIContent* aChild,
                                   uint32_t aIndex, uint32_t aChildCount)
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
nsAttrAndChildArray::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  if (mImpl) {
    // Don't add the size taken by *mMappedAttrs because it's shared.

    n += aMallocSizeOf(mImpl);

    uint32_t slotCount = AttrSlotCount();
    for (uint32_t i = 0; i < slotCount && AttrSlotIsTaken(i); ++i) {
      nsAttrValue* value = &ATTRS(mImpl)[i].mValue;
      n += value->SizeOfExcludingThis(aMallocSizeOf);
    }
  }

  return n;
}

