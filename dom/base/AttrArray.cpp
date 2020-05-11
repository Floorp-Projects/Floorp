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
#include "nsContentUtils.h"  // nsAutoScriptBlocker

using mozilla::CheckedUint32;
using mozilla::dom::Document;

AttrArray::Impl::~Impl() {
  for (InternalAttr& attr : NonMappedAttrs()) {
    attr.~InternalAttr();
  }

  NS_IF_RELEASE(mMappedAttrs);
}

const nsAttrValue* AttrArray::GetAttr(const nsAtom* aLocalName,
                                      int32_t aNamespaceID) const {
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    for (const InternalAttr& attr : NonMappedAttrs()) {
      if (attr.mName.Equals(aLocalName)) {
        return &attr.mValue;
      }
    }

    if (mImpl && mImpl->mMappedAttrs) {
      return mImpl->mMappedAttrs->GetAttr(aLocalName);
    }
  } else {
    for (const InternalAttr& attr : NonMappedAttrs()) {
      if (attr.mName.Equals(aLocalName, aNamespaceID)) {
        return &attr.mValue;
      }
    }
  }

  return nullptr;
}

const nsAttrValue* AttrArray::GetAttr(const nsAString& aLocalName) const {
  for (const InternalAttr& attr : NonMappedAttrs()) {
    if (attr.mName.Equals(aLocalName)) {
      return &attr.mValue;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetAttr(aLocalName);
  }

  return nullptr;
}

const nsAttrValue* AttrArray::GetAttr(const nsAString& aName,
                                      nsCaseTreatment aCaseSensitive) const {
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

  for (const InternalAttr& attr : NonMappedAttrs()) {
    if (attr.mName.QualifiedNameEquals(aName)) {
      return &attr.mValue;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetAttr(aName);
  }

  return nullptr;
}

const nsAttrValue* AttrArray::AttrAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &mImpl->NonMappedAttrs()[aPos].mValue;
  }

  return mImpl->mMappedAttrs->AttrAt(aPos - nonmapped);
}

template <typename Name>
inline nsresult AttrArray::AddNewAttribute(Name* aName, nsAttrValue& aValue) {
  MOZ_ASSERT(!mImpl || mImpl->mCapacity >= mImpl->mAttrCount);
  if (!mImpl || mImpl->mCapacity == mImpl->mAttrCount) {
    if (!GrowBy(1)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  InternalAttr& attr = mImpl->mBuffer[mImpl->mAttrCount++];
  new (&attr.mName) nsAttrName(aName);
  new (&attr.mValue) nsAttrValue();
  attr.mValue.SwapValueWith(aValue);
  return NS_OK;
}

nsresult AttrArray::SetAndSwapAttr(nsAtom* aLocalName, nsAttrValue& aValue,
                                   bool* aHadValue) {
  *aHadValue = false;

  for (InternalAttr& attr : NonMappedAttrs()) {
    if (attr.mName.Equals(aLocalName)) {
      attr.mValue.SwapValueWith(aValue);
      *aHadValue = true;
      return NS_OK;
    }
  }

  return AddNewAttribute(aLocalName, aValue);
}

nsresult AttrArray::SetAndSwapAttr(mozilla::dom::NodeInfo* aName,
                                   nsAttrValue& aValue, bool* aHadValue) {
  int32_t namespaceID = aName->NamespaceID();
  nsAtom* localName = aName->NameAtom();
  if (namespaceID == kNameSpaceID_None) {
    return SetAndSwapAttr(localName, aValue, aHadValue);
  }

  *aHadValue = false;
  for (InternalAttr& attr : NonMappedAttrs()) {
    if (attr.mName.Equals(localName, namespaceID)) {
      attr.mName.SetTo(aName);
      attr.mValue.SwapValueWith(aValue);
      *aHadValue = true;
      return NS_OK;
    }
  }

  return AddNewAttribute(aName, aValue);
}

nsresult AttrArray::RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue) {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    mImpl->mBuffer[aPos].mValue.SwapValueWith(aValue);
    mImpl->mBuffer[aPos].~InternalAttr();

    memmove(mImpl->mBuffer + aPos, mImpl->mBuffer + aPos + 1,
            (mImpl->mAttrCount - aPos - 1) * sizeof(InternalAttr));

    --mImpl->mAttrCount;

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

mozilla::dom::BorrowedAttrInfo AttrArray::AttrInfoAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    InternalAttr& attr = mImpl->mBuffer[aPos];
    return BorrowedAttrInfo(&attr.mName, &attr.mValue);
  }

  return BorrowedAttrInfo(mImpl->mMappedAttrs->NameAt(aPos - nonmapped),
                          mImpl->mMappedAttrs->AttrAt(aPos - nonmapped));
}

const nsAttrName* AttrArray::AttrNameAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");

  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &mImpl->mBuffer[aPos].mName;
  }

  return mImpl->mMappedAttrs->NameAt(aPos - nonmapped);
}

const nsAttrName* AttrArray::GetSafeAttrNameAt(uint32_t aPos) const {
  uint32_t nonmapped = NonMappedAttrCount();
  if (aPos < nonmapped) {
    return &mImpl->mBuffer[aPos].mName;
  }

  if (aPos >= AttrCount()) {
    return nullptr;
  }

  return mImpl->mMappedAttrs->NameAt(aPos - nonmapped);
}

const nsAttrName* AttrArray::GetExistingAttrNameFromQName(
    const nsAString& aName) const {
  for (const InternalAttr& attr : NonMappedAttrs()) {
    if (attr.mName.QualifiedNameEquals(aName)) {
      return &attr.mName;
    }
  }

  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->GetExistingAttrNameFromQName(aName);
  }

  return nullptr;
}

int32_t AttrArray::IndexOfAttr(const nsAtom* aLocalName,
                               int32_t aNamespaceID) const {
  if (!mImpl) {
    return -1;
  }

  int32_t idx;
  if (mImpl->mMappedAttrs && aNamespaceID == kNameSpaceID_None) {
    idx = mImpl->mMappedAttrs->IndexOfAttr(aLocalName);
    if (idx >= 0) {
      return NonMappedAttrCount() + idx;
    }
  }

  uint32_t i = 0;
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets make an optimized loop
    // Note that here we don't check for AttrSlotIsTaken() in the loop
    // condition for the sake of performance because comparing aLocalName
    // against null would fail in the loop body (since Equals() just compares
    // the raw pointer value of aLocalName to what AttrSlotIsTaken() would be
    // checking.
    for (const InternalAttr& attr : NonMappedAttrs()) {
      if (attr.mName.Equals(aLocalName)) {
        return i;
      }
      ++i;
    }
  } else {
    for (const InternalAttr& attr : NonMappedAttrs()) {
      if (attr.mName.Equals(aLocalName, aNamespaceID)) {
        return i;
      }
      ++i;
    }
  }

  return -1;
}

nsresult AttrArray::SetAndSwapMappedAttr(nsAtom* aLocalName,
                                         nsAttrValue& aValue,
                                         nsMappedAttributeElement* aContent,
                                         nsHTMLStyleSheet* aSheet,
                                         bool* aHadValue) {
  bool willAdd = true;
  if (mImpl && mImpl->mMappedAttrs) {
    willAdd = !mImpl->mMappedAttrs->GetAttr(aLocalName);
  }

  RefPtr<nsMappedAttributes> mapped =
      GetModifiableMapped(aContent, aSheet, willAdd);

  mapped->SetAndSwapAttr(aLocalName, aValue, aHadValue);

  return MakeMappedUnique(mapped);
}

nsresult AttrArray::DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet) {
  MOZ_ASSERT(mImpl && mImpl->mMappedAttrs, "Should have mapped attrs here!");
  if (aSheet == mImpl->mMappedAttrs->GetStyleSheet()) {
    return NS_OK;
  }

  RefPtr<nsMappedAttributes> mapped =
      GetModifiableMapped(nullptr, nullptr, false);

  mapped->DropStyleSheetReference();
  mapped->SetStyleSheet(aSheet);

  return MakeMappedUnique(mapped);
}

nsresult AttrArray::DoUpdateMappedAttrRuleMapper(
    nsMappedAttributeElement& aElement) {
  MOZ_ASSERT(mImpl && mImpl->mMappedAttrs, "Should have mapped attrs here!");

  // First two args don't matter if the assert holds.
  RefPtr<nsMappedAttributes> mapped =
      GetModifiableMapped(nullptr, nullptr, false);

  mapped->SetRuleMapper(aElement.GetAttributeMappingFunction());

  return MakeMappedUnique(mapped);
}

void AttrArray::Compact() {
  if (!mImpl) {
    return;
  }

  if (!mImpl->mAttrCount && !mImpl->mMappedAttrs) {
    mImpl.reset();
    return;
  }

  // Nothing to do.
  if (mImpl->mAttrCount == mImpl->mCapacity) {
    return;
  }

  Impl* impl = mImpl.release();
  impl = static_cast<Impl*>(
      realloc(impl, Impl::AllocationSizeForAttributes(impl->mAttrCount)));
  MOZ_ASSERT(impl, "failed to reallocate to a smaller buffer!");
  impl->mCapacity = impl->mAttrCount;
  mImpl.reset(impl);
}

uint32_t AttrArray::DoGetMappedAttrCount() const {
  MOZ_ASSERT(mImpl && mImpl->mMappedAttrs);
  return static_cast<uint32_t>(mImpl->mMappedAttrs->Count());
}

nsresult AttrArray::ForceMapped(nsMappedAttributeElement* aContent,
                                Document* aDocument) {
  nsHTMLStyleSheet* sheet = aDocument->GetAttributeStyleSheet();
  RefPtr<nsMappedAttributes> mapped =
      GetModifiableMapped(aContent, sheet, false, 0);
  return MakeMappedUnique(mapped);
}

void AttrArray::ClearMappedServoStyle() {
  if (mImpl && mImpl->mMappedAttrs) {
    mImpl->mMappedAttrs->ClearServoStyle();
  }
}

nsMappedAttributes* AttrArray::GetModifiableMapped(
    nsMappedAttributeElement* aContent, nsHTMLStyleSheet* aSheet,
    bool aWillAddAttr, int32_t aAttrCount) {
  if (mImpl && mImpl->mMappedAttrs) {
    return mImpl->mMappedAttrs->Clone(aWillAddAttr);
  }

  MOZ_ASSERT(aContent, "Trying to create modifiable without content");

  nsMapRuleToAttributesFunc mapRuleFunc =
      aContent->GetAttributeMappingFunction();
  return new (aAttrCount) nsMappedAttributes(aSheet, mapRuleFunc);
}

nsresult AttrArray::MakeMappedUnique(nsMappedAttributes* aAttributes) {
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

const nsMappedAttributes* AttrArray::GetMapped() const {
  return mImpl ? mImpl->mMappedAttrs : nullptr;
}

nsresult AttrArray::EnsureCapacityToClone(const AttrArray& aOther) {
  MOZ_ASSERT(!mImpl,
             "AttrArray::EnsureCapacityToClone requires the array be empty "
             "when called");

  uint32_t attrCount = aOther.NonMappedAttrCount();
  if (!attrCount) {
    return NS_OK;
  }

  // No need to use a CheckedUint32 because we are cloning. We know that we
  // have already allocated an AttrArray of this size.
  mImpl.reset(
      static_cast<Impl*>(malloc(Impl::AllocationSizeForAttributes(attrCount))));
  NS_ENSURE_TRUE(mImpl, NS_ERROR_OUT_OF_MEMORY);

  mImpl->mMappedAttrs = nullptr;
  mImpl->mCapacity = attrCount;
  mImpl->mAttrCount = 0;

  return NS_OK;
}

bool AttrArray::GrowBy(uint32_t aGrowSize) {
  const uint32_t kLinearThreshold = 16;
  const uint32_t kLinearGrowSize = 4;

  CheckedUint32 capacity = mImpl ? mImpl->mCapacity : 0;
  CheckedUint32 minCapacity = capacity;
  minCapacity += aGrowSize;
  if (!minCapacity.isValid()) {
    return false;
  }

  if (capacity.value() <= kLinearThreshold) {
    do {
      capacity += kLinearGrowSize;
      if (!capacity.isValid()) {
        return false;
      }
    } while (capacity.value() < minCapacity.value());
  } else {
    uint32_t shift = mozilla::CeilingLog2(minCapacity.value());
    if (shift >= 32) {
      return false;
    }
    capacity = 1u << shift;
  }

  CheckedUint32 sizeInBytes = capacity.value();
  sizeInBytes *= sizeof(InternalAttr);
  if (!sizeInBytes.isValid()) {
    return false;
  }

  sizeInBytes += sizeof(Impl);
  if (!sizeInBytes.isValid()) {
    return false;
  }

  MOZ_ASSERT(sizeInBytes.value() ==
             Impl::AllocationSizeForAttributes(capacity.value()));

  const bool needToInitialize = !mImpl;
  Impl* newImpl =
      static_cast<Impl*>(realloc(mImpl.release(), sizeInBytes.value()));
  NS_ENSURE_TRUE(newImpl, false);

  mImpl.reset(newImpl);

  // Set initial counts if we didn't have a buffer before
  if (needToInitialize) {
    mImpl->mMappedAttrs = nullptr;
    mImpl->mAttrCount = 0;
  }

  mImpl->mCapacity = capacity.value();
  return true;
}

size_t AttrArray::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  if (mImpl) {
    // Don't add the size taken by *mMappedAttrs because it's shared.

    n += aMallocSizeOf(mImpl.get());

    for (const InternalAttr& attr : NonMappedAttrs()) {
      n += attr.mValue.SizeOfExcludingThis(aMallocSizeOf);
    }
  }

  return n;
}
