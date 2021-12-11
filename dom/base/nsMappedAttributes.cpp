/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A unique per-element set of attributes that is used as an
 * nsIStyleRule; used to implement presentational attributes.
 */

#include "nsMappedAttributes.h"
#include "mozilla/Assertions.h"
#include "nsHTMLStyleSheet.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MappedDeclarations.h"
#include "mozilla/MemoryReporting.h"

using namespace mozilla;

bool nsMappedAttributes::sShuttingDown = false;
nsTArray<void*>* nsMappedAttributes::sCachedMappedAttributeAllocations =
    nullptr;

void nsMappedAttributes::Shutdown() {
  sShuttingDown = true;
  if (sCachedMappedAttributeAllocations) {
    for (uint32_t i = 0; i < sCachedMappedAttributeAllocations->Length(); ++i) {
      void* cachedValue = (*sCachedMappedAttributeAllocations)[i];
      ::operator delete(cachedValue);
    }
  }

  delete sCachedMappedAttributeAllocations;
  sCachedMappedAttributeAllocations = nullptr;
}

nsMappedAttributes::nsMappedAttributes(nsHTMLStyleSheet* aSheet,
                                       nsMapRuleToAttributesFunc aMapRuleFunc)
    : mAttrCount(0),
      mSheet(aSheet),
      mRuleMapper(aMapRuleFunc),
      mServoStyle(nullptr) {
  MOZ_ASSERT(mRefCnt == 0);  // Ensure caching works as expected.
}

nsMappedAttributes::nsMappedAttributes(const nsMappedAttributes& aCopy)
    : mAttrCount(aCopy.mAttrCount),
      mSheet(aCopy.mSheet),
      mRuleMapper(aCopy.mRuleMapper),
      // This is only called by ::Clone, which is used to create independent
      // nsMappedAttributes objects which should not share a DeclarationBlock
      mServoStyle(nullptr) {
  MOZ_ASSERT(mBufferSize >= aCopy.mAttrCount, "can't fit attributes");
  MOZ_ASSERT(mRefCnt == 0);  // Ensure caching works as expected.

  uint32_t i = 0;
  for (const InternalAttr& attr : aCopy.Attrs()) {
    new (&mBuffer[i++]) InternalAttr(attr);
  }
}

nsMappedAttributes::~nsMappedAttributes() {
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }

  for (InternalAttr& attr : Attrs()) {
    attr.~InternalAttr();
  }
}

nsMappedAttributes* nsMappedAttributes::Clone(bool aWillAddAttr) {
  uint32_t extra = aWillAddAttr ? 1 : 0;

  // This will call the overridden operator new
  return new (mAttrCount + extra) nsMappedAttributes(*this);
}

void* nsMappedAttributes::operator new(size_t aSize,
                                       uint32_t aAttrCount) noexcept(true) {
  size_t size = aSize + aAttrCount * sizeof(InternalAttr);

  if (sCachedMappedAttributeAllocations) {
    void* cached = sCachedMappedAttributeAllocations->SafeElementAt(aAttrCount);
    if (cached) {
      (*sCachedMappedAttributeAllocations)[aAttrCount] = nullptr;
      return cached;
    }
  }

  void* newAttrs = ::operator new(size);

#ifdef DEBUG
  static_cast<nsMappedAttributes*>(newAttrs)->mBufferSize = aAttrCount;
#endif
  return newAttrs;
}

void nsMappedAttributes::LastRelease() {
  if (!sShuttingDown) {
    if (!sCachedMappedAttributeAllocations) {
      sCachedMappedAttributeAllocations = new nsTArray<void*>();
    }

    // Ensure the cache array is at least mAttrCount + 1 long and
    // that each item is either null or pointing to a cached item.
    // The size of the array is capped because mapped attributes are defined
    // statically in element implementations.
    sCachedMappedAttributeAllocations->SetCapacity(mAttrCount + 1);
    for (uint32_t i = sCachedMappedAttributeAllocations->Length();
         i < (uint32_t(mAttrCount) + 1); ++i) {
      sCachedMappedAttributeAllocations->AppendElement(nullptr);
    }

    if (!(*sCachedMappedAttributeAllocations)[mAttrCount]) {
      void* memoryToCache = this;
      this->~nsMappedAttributes();
      (*sCachedMappedAttributeAllocations)[mAttrCount] = memoryToCache;
      return;
    }
  }

  delete this;
}

void nsMappedAttributes::SetAndSwapAttr(nsAtom* aAttrName, nsAttrValue& aValue,
                                        bool* aValueWasSet) {
  MOZ_ASSERT(aAttrName, "null name");
  *aValueWasSet = false;
  uint32_t i;
  for (i = 0; i < mAttrCount && !mBuffer[i].mName.IsSmaller(aAttrName); ++i) {
    if (mBuffer[i].mName.Equals(aAttrName)) {
      mBuffer[i].mValue.SwapValueWith(aValue);
      *aValueWasSet = true;
      return;
    }
  }

  MOZ_ASSERT(mBufferSize >= mAttrCount + 1, "can't fit attributes");

  if (mAttrCount != i) {
    memmove(&mBuffer[i + 1], &mBuffer[i],
            (mAttrCount - i) * sizeof(InternalAttr));
  }

  new (&mBuffer[i].mName) nsAttrName(aAttrName);
  new (&mBuffer[i].mValue) nsAttrValue();
  mBuffer[i].mValue.SwapValueWith(aValue);
  ++mAttrCount;
}

const nsAttrValue* nsMappedAttributes::GetAttr(const nsAtom* aAttrName) const {
  MOZ_ASSERT(aAttrName, "null name");
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Equals(aAttrName)) {
      return &attr.mValue;
    }
  }
  return nullptr;
}

const nsAttrValue* nsMappedAttributes::GetAttr(
    const nsAString& aAttrName) const {
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.Atom()->Equals(aAttrName)) {
      return &attr.mValue;
    }
  }
  return nullptr;
}

bool nsMappedAttributes::Equals(const nsMappedAttributes* aOther) const {
  if (this == aOther) {
    return true;
  }

  if (mRuleMapper != aOther->mRuleMapper || mAttrCount != aOther->mAttrCount) {
    return false;
  }

  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    if (!mBuffer[i].mName.Equals(aOther->mBuffer[i].mName) ||
        !mBuffer[i].mValue.Equals(aOther->mBuffer[i].mValue)) {
      return false;
    }
  }

  return true;
}

PLDHashNumber nsMappedAttributes::HashValue() const {
  PLDHashNumber hash = HashGeneric(mRuleMapper);
  for (const InternalAttr& attr : Attrs()) {
    hash = AddToHash(hash, attr.mName.HashValue(), attr.mValue.HashValue());
  }
  return hash;
}

void nsMappedAttributes::SetStyleSheet(nsHTMLStyleSheet* aSheet) {
  MOZ_ASSERT(!mSheet,
             "Should either drop the sheet reference manually, "
             "or drop the mapped attributes");
  mSheet = aSheet;  // not ref counted
}

void nsMappedAttributes::RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue) {
  mBuffer[aPos].mValue.SwapValueWith(aValue);
  mBuffer[aPos].~InternalAttr();
  memmove(&mBuffer[aPos], &mBuffer[aPos + 1],
          (mAttrCount - aPos - 1) * sizeof(InternalAttr));
  mAttrCount--;
}

const nsAttrName* nsMappedAttributes::GetExistingAttrNameFromQName(
    const nsAString& aName) const {
  for (const InternalAttr& attr : Attrs()) {
    if (attr.mName.IsAtom()) {
      if (attr.mName.Atom()->Equals(aName)) {
        return &attr.mName;
      }
    } else {
      if (attr.mName.NodeInfo()->QualifiedNameEquals(aName)) {
        return &attr.mName;
      }
    }
  }

  return nullptr;
}

int32_t nsMappedAttributes::IndexOfAttr(const nsAtom* aLocalName) const {
  for (uint32_t i = 0; i < mAttrCount; ++i) {
    if (mBuffer[i].mName.Equals(aLocalName)) {
      return i;
    }
  }
  return -1;
}

size_t nsMappedAttributes::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  MOZ_ASSERT(mBufferSize >= mAttrCount, "can't fit attributes");

  size_t n = aMallocSizeOf(this);
  for (const InternalAttr& attr : Attrs()) {
    n += attr.mValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

void nsMappedAttributes::LazilyResolveServoDeclaration(dom::Document* aDoc) {
  MOZ_ASSERT(!mServoStyle,
             "LazilyResolveServoDeclaration should not be called if "
             "mServoStyle is already set");
  if (mRuleMapper) {
    MappedDeclarations declarations(
        aDoc, Servo_DeclarationBlock_CreateEmpty().Consume());
    (*mRuleMapper)(this, declarations);
    mServoStyle = declarations.TakeDeclarationBlock();
  }
}
