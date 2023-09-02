/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Storage of the attributes of a DOM node.
 */

#ifndef AttrArray_h___
#define AttrArray_h___

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Span.h"
#include "mozilla/dom/BorrowedAttrInfo.h"

#include "nscore.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsCaseTreatment.h"

namespace mozilla {
class AttributeStyles;
struct StyleLockedDeclarationBlock;
}  // namespace mozilla

class AttrArray {
  using BorrowedAttrInfo = mozilla::dom::BorrowedAttrInfo;

 public:
  AttrArray() = default;
  ~AttrArray() = default;

  bool HasAttrs() const { return !!AttrCount(); }

  uint32_t AttrCount() const { return mImpl ? mImpl->mAttrCount : 0; }

  const nsAttrValue* GetAttr(const nsAtom* aLocalName) const;

  const nsAttrValue* GetAttr(const nsAtom* aLocalName,
                             int32_t aNamespaceID) const;
  // As above but using a string attr name and always using
  // kNameSpaceID_None.  This is always case-sensitive.
  const nsAttrValue* GetAttr(const nsAString& aName) const;
  // Get an nsAttrValue by qualified name.  Can optionally do
  // ASCII-case-insensitive name matching.
  const nsAttrValue* GetAttr(const nsAString& aName,
                             nsCaseTreatment aCaseSensitive) const;
  const nsAttrValue* AttrAt(uint32_t aPos) const;
  // SetAndSwapAttr swaps the current attribute value with aValue.
  // If the attribute was unset, an empty value will be swapped into aValue
  // and aHadValue will be set to false. Otherwise, aHadValue will be set to
  // true.
  nsresult SetAndSwapAttr(nsAtom* aLocalName, nsAttrValue& aValue,
                          bool* aHadValue);
  nsresult SetAndSwapAttr(mozilla::dom::NodeInfo* aName, nsAttrValue& aValue,
                          bool* aHadValue);

  // This stores the argument and clears the pending mapped attribute evaluation
  // bit, so after calling this IsPendingMappedAttributeEvaluation() is
  // guaranteed to return false.
  void SetMappedDeclarationBlock(
      already_AddRefed<mozilla::StyleLockedDeclarationBlock>);

  bool IsPendingMappedAttributeEvaluation() const {
    return mImpl && mImpl->mMappedAttributeBits & 1;
  }

  mozilla::StyleLockedDeclarationBlock* GetMappedDeclarationBlock() const {
    return mImpl ? mImpl->GetMappedDeclarationBlock() : nullptr;
  }

  // Remove the attr at position aPos.  The value of the attr is placed in
  // aValue; any value that was already in aValue is destroyed.
  nsresult RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue);

  // Returns attribute name at given position, *not* out-of-bounds safe
  const nsAttrName* AttrNameAt(uint32_t aPos) const;

  // Returns the attribute info at a given position, *not* out-of-bounds safe
  BorrowedAttrInfo AttrInfoAt(uint32_t aPos) const;

  // Returns attribute name at given position or null if aPos is out-of-bounds
  const nsAttrName* GetSafeAttrNameAt(uint32_t aPos) const;

  const nsAttrName* GetExistingAttrNameFromQName(const nsAString& aName) const;
  int32_t IndexOfAttr(const nsAtom* aLocalName) const;
  int32_t IndexOfAttr(const nsAtom* aLocalName, int32_t aNamespaceID) const;

  void Compact();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Mark the element as pending mapped attribute evaluation. This should be
  // called when a mapped attribute is changed (regardless of connectedness).
  bool MarkAsPendingPresAttributeEvaluation() {
    // It'd be great to be able to assert that mImpl is non-null or we're the
    // <body> element.
    if (MOZ_UNLIKELY(!mImpl) && !GrowBy(1)) {
      return false;
    }
    InfallibleMarkAsPendingPresAttributeEvaluation();
    return true;
  }

  // See above.
  void InfallibleMarkAsPendingPresAttributeEvaluation() {
    MOZ_ASSERT(mImpl);
    mImpl->mMappedAttributeBits |= 1;
  }

  // Clear the servo declaration block on the mapped attributes, if any
  // Will assert off main thread
  void ClearMappedServoStyle();

  // Increases capacity (if necessary) to have enough space to accomodate the
  // unmapped attributes of |aOther|.
  nsresult EnsureCapacityToClone(const AttrArray& aOther);

  enum AttrValuesState { ATTR_MISSING = -1, ATTR_VALUE_NO_MATCH = -2 };
  using AttrValuesArray = nsStaticAtom* const;
  int32_t FindAttrValueIn(int32_t aNameSpaceID, const nsAtom* aName,
                          AttrValuesArray* aValues,
                          nsCaseTreatment aCaseSensitive) const;

  inline bool GetAttr(int32_t aNameSpaceID, const nsAtom* aName,
                      nsAString& aResult) const {
    MOZ_ASSERT(aResult.IsEmpty(), "Should have empty string coming in");
    const nsAttrValue* val = GetAttr(aName, aNameSpaceID);
    if (!val) {
      return false;
    }
    val->ToString(aResult);
    return true;
  }

  inline bool GetAttr(const nsAtom* aName, nsAString& aResult) const {
    MOZ_ASSERT(aResult.IsEmpty(), "Should have empty string coming in");
    const nsAttrValue* val = GetAttr(aName);
    if (!val) {
      return false;
    }
    val->ToString(aResult);
    return true;
  }

  inline bool HasAttr(const nsAtom* aName) const { return !!GetAttr(aName); }

  inline bool HasAttr(int32_t aNameSpaceID, const nsAtom* aName) const {
    return !!GetAttr(aName, aNameSpaceID);
  }

  inline bool AttrValueIs(int32_t aNameSpaceID, const nsAtom* aName,
                          const nsAString& aValue,
                          nsCaseTreatment aCaseSensitive) const {
    NS_ASSERTION(aName, "Must have attr name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
    const nsAttrValue* val = GetAttr(aName, aNameSpaceID);
    return val && val->Equals(aValue, aCaseSensitive);
  }

  inline bool AttrValueIs(int32_t aNameSpaceID, const nsAtom* aName,
                          const nsAtom* aValue,
                          nsCaseTreatment aCaseSensitive) const {
    NS_ASSERTION(aName, "Must have attr name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
    NS_ASSERTION(aValue, "Null value atom");

    const nsAttrValue* val = GetAttr(aName, aNameSpaceID);
    return val && val->Equals(aValue, aCaseSensitive);
  }

  struct InternalAttr {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  AttrArray(const AttrArray& aOther) = delete;
  AttrArray& operator=(const AttrArray& aOther) = delete;

  bool GrowBy(uint32_t aGrowSize);
  bool GrowTo(uint32_t aCapacity);

 private:
  // Tries to create an attribute, growing the buffer if needed, with the given
  // name and value.
  //
  // The value is moved from the argument.
  //
  // `Name` can be anything you construct a `nsAttrName` with (either an atom or
  // a NodeInfo pointer).
  template <typename Name>
  nsresult AddNewAttribute(Name*, nsAttrValue&);

  class Impl {
   public:
    constexpr static size_t AllocationSizeForAttributes(uint32_t aAttrCount) {
      return sizeof(Impl) + aAttrCount * sizeof(InternalAttr);
    }

    mozilla::StyleLockedDeclarationBlock* GetMappedDeclarationBlock() const {
      return reinterpret_cast<mozilla::StyleLockedDeclarationBlock*>(
          mMappedAttributeBits & ~uintptr_t(1));
    }

    auto Attrs() const {
      return mozilla::Span<const InternalAttr>{mBuffer, mAttrCount};
    }

    auto Attrs() { return mozilla::Span<InternalAttr>{mBuffer, mAttrCount}; }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    ~Impl();

    uint32_t mAttrCount;
    uint32_t mCapacity;  // In number of InternalAttrs

    // mMappedAttributeBits is a tagged pointer of a
    // StyleLockedDeclarationBlock, which holds the style information that our
    // attributes map to.
    //
    // If the lower bit is set, then our mapped attributes are dirty. This just
    // means that we might have mapped attributes (or used to and no longer
    // have), and are pending an update to recompute our declaration.
    uintptr_t mMappedAttributeBits = 0;

    // Allocated in the same buffer as `Impl`.
    InternalAttr mBuffer[0];
  };

  mozilla::Span<InternalAttr> Attrs() {
    return mImpl ? mImpl->Attrs() : mozilla::Span<InternalAttr>();
  }

  mozilla::Span<const InternalAttr> Attrs() const {
    return mImpl ? mImpl->Attrs() : mozilla::Span<const InternalAttr>();
  }

  mozilla::UniquePtr<Impl> mImpl;
};

#endif
