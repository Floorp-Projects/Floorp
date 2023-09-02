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

#include "mozilla/AttributeStyles.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindings.h"

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"  // nsAutoScriptBlocker

using mozilla::CheckedUint32;

AttrArray::Impl::~Impl() {
  for (InternalAttr& attr : Attrs()) {
    attr.~InternalAttr();
  }
  if (auto* decl = GetMappedDeclarationBlock()) {
    Servo_DeclarationBlock_Release(decl);
    mMappedAttributeBits = 0;
  }
}

void AttrArray::SetMappedDeclarationBlock(
    already_AddRefed<mozilla::StyleLockedDeclarationBlock> aBlock) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(IsPendingMappedAttributeEvaluation());
  if (auto* decl = GetMappedDeclarationBlock()) {
    Servo_DeclarationBlock_Release(decl);
  }
  mImpl->mMappedAttributeBits = reinterpret_cast<uintptr_t>(aBlock.take());
  MOZ_ASSERT(!IsPendingMappedAttributeEvaluation());
}

const nsAttrValue* AttrArray::GetAttr(const nsAtom* aLocalName) const {
  NS_ASSERTION(aLocalName, "Must have attr name");
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aLocalName)) {
      return &attr.mValue;
    }
  }
  return nullptr;
}

const nsAttrValue* AttrArray::GetAttr(const nsAtom* aLocalName,
                                      int32_t aNamespaceID) const {
  NS_ASSERTION(aLocalName, "Must have attr name");
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown, "Must have namespace");
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets use the optimized loop
    return GetAttr(aLocalName);
  }
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aLocalName, aNamespaceID)) {
      return &attr.mValue;
    }
  }
  return nullptr;
}

const nsAttrValue* AttrArray::GetAttr(const nsAString& aLocalName) const {
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aLocalName)) {
      return &attr.mValue;
    }
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

  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.QualifiedNameEquals(aName)) {
      return &attr.mValue;
    }
  }

  return nullptr;
}

const nsAttrValue* AttrArray::AttrAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");
  return &mImpl->Attrs()[aPos].mValue;
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

  for (InternalAttr& attr : Attrs()) {
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
  for (InternalAttr& attr : Attrs()) {
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

  mImpl->mBuffer[aPos].mValue.SwapValueWith(aValue);
  mImpl->mBuffer[aPos].~InternalAttr();

  memmove(mImpl->mBuffer + aPos, mImpl->mBuffer + aPos + 1,
          (mImpl->mAttrCount - aPos - 1) * sizeof(InternalAttr));

  --mImpl->mAttrCount;
  return NS_OK;
}

mozilla::dom::BorrowedAttrInfo AttrArray::AttrInfoAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");
  InternalAttr& attr = mImpl->mBuffer[aPos];
  return BorrowedAttrInfo(&attr.mName, &attr.mValue);
}

const nsAttrName* AttrArray::AttrNameAt(uint32_t aPos) const {
  NS_ASSERTION(aPos < AttrCount(), "out-of-bounds access in AttrArray");
  return &mImpl->mBuffer[aPos].mName;
}

const nsAttrName* AttrArray::GetSafeAttrNameAt(uint32_t aPos) const {
  if (aPos >= AttrCount()) {
    return nullptr;
  }
  return &mImpl->mBuffer[aPos].mName;
}

const nsAttrName* AttrArray::GetExistingAttrNameFromQName(
    const nsAString& aName) const {
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.QualifiedNameEquals(aName)) {
      return &attr.mName;
    }
  }
  return nullptr;
}

int32_t AttrArray::IndexOfAttr(const nsAtom* aLocalName) const {
  int32_t i = 0;
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aLocalName)) {
      return i;
    }
    ++i;
  }
  return -1;
}

int32_t AttrArray::IndexOfAttr(const nsAtom* aLocalName,
                               int32_t aNamespaceID) const {
  if (aNamespaceID == kNameSpaceID_None) {
    // This should be the common case so lets use the optimized loop
    return IndexOfAttr(aLocalName);
  }
  int32_t i = 0;
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aLocalName, aNamespaceID)) {
      return i;
    }
    ++i;
  }
  return -1;
}

void AttrArray::Compact() {
  if (!mImpl) {
    return;
  }

  if (!mImpl->mAttrCount && !mImpl->mMappedAttributeBits) {
    mImpl.reset();
    return;
  }

  // Nothing to do.
  if (mImpl->mAttrCount == mImpl->mCapacity) {
    return;
  }

  Impl* oldImpl = mImpl.release();
  Impl* impl = static_cast<Impl*>(
      realloc(oldImpl, Impl::AllocationSizeForAttributes(oldImpl->mAttrCount)));
  if (!impl) {
    mImpl.reset(oldImpl);
    return;
  }
  impl->mCapacity = impl->mAttrCount;
  mImpl.reset(impl);
}

nsresult AttrArray::EnsureCapacityToClone(const AttrArray& aOther) {
  MOZ_ASSERT(!mImpl,
             "AttrArray::EnsureCapacityToClone requires the array be empty "
             "when called");

  uint32_t attrCount = aOther.AttrCount();
  if (!attrCount) {
    return NS_OK;
  }

  // No need to use a CheckedUint32 because we are cloning. We know that we
  // have already allocated an AttrArray of this size.
  mImpl.reset(
      static_cast<Impl*>(malloc(Impl::AllocationSizeForAttributes(attrCount))));
  NS_ENSURE_TRUE(mImpl, NS_ERROR_OUT_OF_MEMORY);

  mImpl->mMappedAttributeBits = 0;
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

  return GrowTo(capacity.value());
}

bool AttrArray::GrowTo(uint32_t aCapacity) {
  uint32_t oldCapacity = mImpl ? mImpl->mCapacity : 0;
  if (aCapacity <= oldCapacity) {
    return true;
  }

  CheckedUint32 sizeInBytes = aCapacity;
  sizeInBytes *= sizeof(InternalAttr);
  if (!sizeInBytes.isValid()) {
    return false;
  }

  sizeInBytes += sizeof(Impl);
  if (!sizeInBytes.isValid()) {
    return false;
  }

  MOZ_ASSERT(sizeInBytes.value() ==
             Impl::AllocationSizeForAttributes(aCapacity));

  const bool needToInitialize = !mImpl;
  Impl* oldImpl = mImpl.release();
  Impl* newImpl = static_cast<Impl*>(realloc(oldImpl, sizeInBytes.value()));
  if (!newImpl) {
    mImpl.reset(oldImpl);
    return false;
  }

  mImpl.reset(newImpl);

  // Set initial counts if we didn't have a buffer before
  if (needToInitialize) {
    mImpl->mMappedAttributeBits = 0;
    mImpl->mAttrCount = 0;
  }

  mImpl->mCapacity = aCapacity;
  return true;
}

size_t AttrArray::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  if (!mImpl) {
    return 0;
  }
  size_t n = aMallocSizeOf(mImpl.get());
  for (const InternalAttr& attr : Attrs()) {
    n += attr.mValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

int32_t AttrArray::FindAttrValueIn(int32_t aNameSpaceID, const nsAtom* aName,
                                   AttrValuesArray* aValues,
                                   nsCaseTreatment aCaseSensitive) const {
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");

  const nsAttrValue* val = GetAttr(aName, aNameSpaceID);
  if (val) {
    for (int32_t i = 0; aValues[i]; ++i) {
      if (val->Equals(aValues[i], aCaseSensitive)) {
        return i;
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }
  return ATTR_MISSING;
}
