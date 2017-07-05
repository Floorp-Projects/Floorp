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
#include "nsHTMLStyleSheet.h"
#include "nsRuleData.h"
#include "nsRuleWalker.h"
#include "mozilla/GenericSpecifiedValues.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoDeclarationBlock.h"
#include "mozilla/ServoSpecifiedValues.h"

using namespace mozilla;

bool
nsMappedAttributes::sShuttingDown = false;
nsTArray<void*>*
nsMappedAttributes::sCachedMappedAttributeAllocations = nullptr;

void
nsMappedAttributes::Shutdown()
{
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
    mServoStyle(nullptr)
{
  MOZ_ASSERT(mRefCnt == 0); // Ensure caching works as expected.
}

nsMappedAttributes::nsMappedAttributes(const nsMappedAttributes& aCopy)
  : mAttrCount(aCopy.mAttrCount),
    mSheet(aCopy.mSheet),
    mRuleMapper(aCopy.mRuleMapper),
    // This is only called by ::Clone, which is used to create independent
    // nsMappedAttributes objects which should not share a ServoDeclarationBlock
    mServoStyle(nullptr)
{
  NS_ASSERTION(mBufferSize >= aCopy.mAttrCount, "can't fit attributes");
  MOZ_ASSERT(mRefCnt == 0); // Ensure caching works as expected.

  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    new (&Attrs()[i]) InternalAttr(aCopy.Attrs()[i]);
  }
}

nsMappedAttributes::~nsMappedAttributes()
{
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }

  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    Attrs()[i].~InternalAttr();
  }
}


nsMappedAttributes*
nsMappedAttributes::Clone(bool aWillAddAttr)
{
  uint32_t extra = aWillAddAttr ? 1 : 0;

  // This will call the overridden operator new
  return new (mAttrCount + extra) nsMappedAttributes(*this);
}

void* nsMappedAttributes::operator new(size_t aSize, uint32_t aAttrCount) CPP_THROW_NEW
{

  size_t size = aSize + aAttrCount * sizeof(InternalAttr);

  // aSize will include the mAttrs buffer so subtract that.
  // We don't want to under-allocate, however, so do not subtract
  // if we have zero attributes. The zero attribute case only happens
  // for <body>'s mapped attributes
  if (aAttrCount != 0) {
    size -= sizeof(void*[1]);
  }

  if (sCachedMappedAttributeAllocations) {
    void* cached =
      sCachedMappedAttributeAllocations->SafeElementAt(aAttrCount);
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

void
nsMappedAttributes::LastRelease()
{
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

NS_IMPL_ADDREF(nsMappedAttributes)
NS_IMPL_RELEASE_WITH_DESTROY(nsMappedAttributes, LastRelease())

NS_IMPL_QUERY_INTERFACE(nsMappedAttributes,
                        nsIStyleRule)

void
nsMappedAttributes::SetAndSwapAttr(nsIAtom* aAttrName, nsAttrValue& aValue,
                                   bool* aValueWasSet)
{
  NS_PRECONDITION(aAttrName, "null name");
  *aValueWasSet = false;
  uint32_t i;
  for (i = 0; i < mAttrCount && !Attrs()[i].mName.IsSmaller(aAttrName); ++i) {
    if (Attrs()[i].mName.Equals(aAttrName)) {
      Attrs()[i].mValue.SwapValueWith(aValue);
      *aValueWasSet = true;
      return;
    }
  }

  NS_ASSERTION(mBufferSize >= mAttrCount + 1, "can't fit attributes");

  if (mAttrCount != i) {
    memmove(&Attrs()[i + 1], &Attrs()[i], (mAttrCount - i) * sizeof(InternalAttr));
  }

  new (&Attrs()[i].mName) nsAttrName(aAttrName);
  new (&Attrs()[i].mValue) nsAttrValue();
  Attrs()[i].mValue.SwapValueWith(aValue);
  ++mAttrCount;
}

const nsAttrValue*
nsMappedAttributes::GetAttr(nsIAtom* aAttrName) const
{
  NS_PRECONDITION(aAttrName, "null name");

  for (uint32_t i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Equals(aAttrName)) {
      return &Attrs()[i].mValue;
    }
  }

  return nullptr;
}

const nsAttrValue*
nsMappedAttributes::GetAttr(const nsAString& aAttrName) const
{
  for (uint32_t i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Atom()->Equals(aAttrName)) {
      return &Attrs()[i].mValue;
    }
  }

  return nullptr;
}

bool
nsMappedAttributes::Equals(const nsMappedAttributes* aOther) const
{
  if (this == aOther) {
    return true;
  }

  if (mRuleMapper != aOther->mRuleMapper || mAttrCount != aOther->mAttrCount) {
    return false;
  }

  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    if (!Attrs()[i].mName.Equals(aOther->Attrs()[i].mName) ||
        !Attrs()[i].mValue.Equals(aOther->Attrs()[i].mValue)) {
      return false;
    }
  }

  return true;
}

PLDHashNumber
nsMappedAttributes::HashValue() const
{
  PLDHashNumber hash = HashGeneric(mRuleMapper);

  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    hash = AddToHash(hash,
                     Attrs()[i].mName.HashValue(),
                     Attrs()[i].mValue.HashValue());
  }

  return hash;
}

void
nsMappedAttributes::SetStyleSheet(nsHTMLStyleSheet* aSheet)
{
  if (mSheet) {
    mSheet->DropMappedAttributes(this);
  }
  mSheet = aSheet;  // not ref counted
}

/* virtual */ void
nsMappedAttributes::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (mRuleMapper) {
    (*mRuleMapper)(this, aRuleData);
  }
}

/* virtual */ bool
nsMappedAttributes::MightMapInheritedStyleData()
{
  // Just assume that we do, rather than adding checks to all of the different
  // kinds of attribute mapping functions we have.
  return true;
}

/* virtual */ bool
nsMappedAttributes::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                                  nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

#ifdef DEBUG
/* virtual */ void
nsMappedAttributes::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  nsAutoString tmp;
  uint32_t i;

  for (i = 0; i < mAttrCount; ++i) {
    int32_t indent;
    for (indent = aIndent; indent > 0; --indent) {
      str.AppendLiteral("  ");
    }

    Attrs()[i].mName.GetQualifiedName(tmp);
    LossyAppendUTF16toASCII(tmp, str);

    Attrs()[i].mValue.ToString(tmp);
    LossyAppendUTF16toASCII(tmp, str);
    str.Append('\n');
    fprintf_stderr(out, "%s", str.get());
  }
}
#endif

void
nsMappedAttributes::RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue)
{
  Attrs()[aPos].mValue.SwapValueWith(aValue);
  Attrs()[aPos].~InternalAttr();
  memmove(&Attrs()[aPos], &Attrs()[aPos + 1],
          (mAttrCount - aPos - 1) * sizeof(InternalAttr));
  mAttrCount--;
}

const nsAttrName*
nsMappedAttributes::GetExistingAttrNameFromQName(const nsAString& aName) const
{
  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.IsAtom()) {
      if (Attrs()[i].mName.Atom()->Equals(aName)) {
        return &Attrs()[i].mName;
      }
    }
    else {
      if (Attrs()[i].mName.NodeInfo()->QualifiedNameEquals(aName)) {
        return &Attrs()[i].mName;
      }
    }
  }

  return nullptr;
}

int32_t
nsMappedAttributes::IndexOfAttr(nsIAtom* aLocalName) const
{
  uint32_t i;
  for (i = 0; i < mAttrCount; ++i) {
    if (Attrs()[i].mName.Equals(aLocalName)) {
      return i;
    }
  }

  return -1;
}

size_t
nsMappedAttributes::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  NS_ASSERTION(mAttrCount == mBufferSize,
               "mBufferSize and mAttrCount are expected to be the same.");

  size_t n = aMallocSizeOf(this);
  for (uint16_t i = 0; i < mAttrCount; ++i) {
    n += Attrs()[i].mValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

void
nsMappedAttributes::LazilyResolveServoDeclaration(nsPresContext* aContext)
{

  MOZ_ASSERT(!mServoStyle,
             "LazilyResolveServoDeclaration should not be called if mServoStyle is already set");
  if (mRuleMapper) {
    mServoStyle = Servo_DeclarationBlock_CreateEmpty().Consume();
    ServoSpecifiedValues servo = ServoSpecifiedValues(aContext, mServoStyle.get());
    (*mRuleMapper)(this, &servo);
  }
}
