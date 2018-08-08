/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Storage of the children and attributes of a DOM node; storage for
 * the two is unified to minimize footprint.
 */

#include "AttrArray.h"

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

AttrArray::AttrArray()
  : mImpl(nullptr)
{
}

AttrArray::~AttrArray()
{
  if (!mImpl) {
    return;
  }

  Clear();

  free(mImpl);
}

uint32_t
AttrArray::AttrCount() const
{
  return NonMappedAttrCount() + MappedAttrCount();
}

const nsAttrValue*
AttrArray::GetAttr(nsAtom* aLocalName, int32_t aNamespaceID) const
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
AttrArray::GetAttr(const nsAString& aLocalName) const
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
AttrArray::GetAttr(const nsAString& aName,
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
AttrArray::AttrAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &ATTRS(mImpl)[aPos].mValue;
  }

  return mImpl->mMappedAttrs->AttrAt(aPos - nonmapped);
}

nsresult
AttrArray::SetAndSwapAttr(nsAtom* aLocalName, nsAttrValue& aValue,
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

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aLocalName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue();
  ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

  return NS_OK;
}

nsresult
AttrArray::SetAndSwapAttr(mozilla::dom::NodeInfo* aName,
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

  if (i == slotCount && !AddAttrSlot()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  new (&ATTRS(mImpl)[i].mName) nsAttrName(aName);
  new (&ATTRS(mImpl)[i].mValue) nsAttrValue();
  ATTRS(mImpl)[i].mValue.SwapValueWith(aValue);

  return NS_OK;
}

nsresult
AttrArray::RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue)
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
AttrArray::AttrInfoAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return BorrowedAttrInfo(&ATTRS(mImpl)[aPos].mName, &ATTRS(mImpl)[aPos].mValue);
  }

  return BorrowedAttrInfo(mImpl->mMappedAttrs->NameAt(aPos - nonmapped),
                    mImpl->mMappedAttrs->AttrAt(aPos - nonmapped));
}

const nsAttrName*
AttrArray::AttrNameAt(uint32_t aPos) const
{
  NS_ASSERTION(aPos < AttrCount(),
               "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &ATTRS(mImpl)[aPos].mName;
  }

  return mImpl->mMappedAttrs->NameAt(aPos - nonmapped);
}

const nsAttrName*
AttrArray::GetSafeAttrNameAt(uint32_t aPos) const
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
AttrArray::GetExistingAttrNameFromQName(const nsAString& aName) const
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
AttrArray::IndexOfAttr(nsAtom* aLocalName, int32_t aNamespaceID) const
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
AttrArray::SetAndSwapMappedAttr(nsAtom* aLocalName,
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
AttrArray::DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet)
{
  MOZ_ASSERT(mImpl && mImpl->mMappedAttrs,
                  "Should have mapped attrs here!");
  if (aSheet == mImpl->mMappedAttrs->GetStyleSheet()) {
    return NS_OK;
  }

  RefPtr<nsMappedAttributes> mapped =
    GetModifiableMapped(nullptr, nullptr, false);

  mapped->SetStyleSheet(aSheet);

  return MakeMappedUnique(mapped);
}

nsresult
AttrArray::DoUpdateMappedAttrRuleMapper(nsMappedAttributeElement& aElement)
{
  MOZ_ASSERT(mImpl && mImpl->mMappedAttrs, "Should have mapped attrs here!");

  // First two args don't matter if the assert holds.
  RefPtr<nsMappedAttributes> mapped =
    GetModifiableMapped(nullptr, nullptr, false);

  mapped->SetRuleMapper(aElement.GetAttributeMappingFunction());

  return MakeMappedUnique(mapped);
}

void
AttrArray::Compact()
{
  if (!mImpl) {
    return;
  }

  // First compress away empty attrslots
  uint32_t slotCount = AttrSlotCount();
  uint32_t attrCount = NonMappedAttrCount();

  if (attrCount < slotCount) {
    SetAttrSlotCount(attrCount);
  }

  // Then resize or free buffer
  uint32_t newSize = attrCount * ATTRSIZE;
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
AttrArray::Clear()
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

  SetAttrSlotCount(0);
}

uint32_t
AttrArray::NonMappedAttrCount() const
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
AttrArray::MappedAttrCount() const
{
  return mImpl && mImpl->mMappedAttrs ? (uint32_t)mImpl->mMappedAttrs->Count() : 0;
}

nsresult
AttrArray::ForceMapped(nsMappedAttributeElement* aContent, nsIDocument* aDocument)
{
  nsHTMLStyleSheet* sheet = aDocument->GetAttributeStyleSheet();
  RefPtr<nsMappedAttributes> mapped = GetModifiableMapped(aContent, sheet, false, 0);
  return MakeMappedUnique(mapped);
}

void
AttrArray::ClearMappedServoStyle()
{
  if (mImpl && mImpl->mMappedAttrs) {
    mImpl->mMappedAttrs->ClearServoStyle();
  }
}

nsMappedAttributes*
AttrArray::GetModifiableMapped(nsMappedAttributeElement* aContent,
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
AttrArray::MakeMappedUnique(nsMappedAttributes* aAttributes)
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
AttrArray::GetMapped() const
{
  return mImpl ? mImpl->mMappedAttrs : nullptr;
}

nsresult
AttrArray::EnsureCapacityToClone(const AttrArray& aOther)
{
  MOZ_ASSERT(!mImpl, "AttrArray::EnsureCapacityToClone requires the array be empty when called");

  uint32_t attrCount = aOther.NonMappedAttrCount();

  if (attrCount == 0) {
    return NS_OK;
  }

  // No need to use a CheckedUint32 because we are cloning. We know that we
  // have already allocated an AttrArray of this size.
  uint32_t size = attrCount;
  size *= ATTRSIZE;
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
  SetAttrSlotCount(attrCount);

  return NS_OK;
}

bool
AttrArray::GrowBy(uint32_t aGrowSize)
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
    SetAttrSlotCount(0);
  }

  mImpl->mBufferSize = size.value() - NS_IMPL_EXTRA_SIZE;

  return true;
}

bool
AttrArray::AddAttrSlot()
{
  uint32_t slotCount = AttrSlotCount();

  CheckedUint32 size = slotCount;
  size += 1;
  size *= ATTRSIZE;
  if (!size.isValid()) {
    return false;
  }

  // Grow buffer if needed
  if (!(mImpl && mImpl->mBufferSize >= size.value()) &&
      !GrowBy(ATTRSIZE)) {
    return false;
  }

  void** offset = mImpl->mBuffer + slotCount * ATTRSIZE;

  SetAttrSlotCount(slotCount + 1);
  memset(static_cast<void*>(offset), 0, sizeof(InternalAttr));

  return true;
}

size_t
AttrArray::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
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
