/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Storage of the children and attributes of a DOM node; storage for
 * the two is unified to minimize footprint.
 */

#ifndef nsAttrAndChildArray_h___
#define nsAttrAndChildArray_h___

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BorrowedAttrInfo.h"

#include "nscore.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsCaseTreatment.h"

class nsINode;
class nsIContent;
class nsMappedAttributes;
class nsHTMLStyleSheet;
class nsRuleWalker;
class nsMappedAttributeElement;

#define ATTRCHILD_ARRAY_GROWSIZE 8
#define ATTRCHILD_ARRAY_LINEAR_THRESHOLD 32

#define ATTRCHILD_ARRAY_ATTR_SLOTS_BITS 10

#define ATTRCHILD_ARRAY_MAX_ATTR_COUNT \
    ((1 << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS) - 1)

#define ATTRCHILD_ARRAY_MAX_CHILD_COUNT \
    (~uint32_t(0) >> ATTRCHILD_ARRAY_ATTR_SLOTS_BITS)

#define ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK \
    ((1 << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS) - 1)


#define ATTRSIZE (sizeof(InternalAttr) / sizeof(void*))

class nsAttrAndChildArray
{
  typedef mozilla::dom::BorrowedAttrInfo BorrowedAttrInfo;
public:
  nsAttrAndChildArray();
  ~nsAttrAndChildArray();

  uint32_t ChildCount() const
  {
    return mImpl ? (mImpl->mAttrAndChildCount >> ATTRCHILD_ARRAY_ATTR_SLOTS_BITS) : 0;
  }
  nsIContent* ChildAt(uint32_t aPos) const
  {
    NS_ASSERTION(aPos < ChildCount(), "out-of-bounds access in nsAttrAndChildArray");
    return reinterpret_cast<nsIContent*>(mImpl->mBuffer[AttrSlotsSize() + aPos]);
  }
  nsIContent* GetSafeChildAt(uint32_t aPos) const;
  nsIContent * const * GetChildArray(uint32_t* aChildCount) const;
  nsresult InsertChildAt(nsIContent* aChild, uint32_t aPos);
  void RemoveChildAt(uint32_t aPos);
  // Like RemoveChildAt but hands the reference to the child being
  // removed back to the caller instead of just releasing it.
  already_AddRefed<nsIContent> TakeChildAt(uint32_t aPos);
  int32_t IndexOfChild(const nsINode* aPossibleChild) const;

  bool HasAttrs() const
  {
    return MappedAttrCount() || (AttrSlotCount() && AttrSlotIsTaken(0));
  }

  uint32_t AttrCount() const;
  const nsAttrValue* GetAttr(nsAtom* aLocalName,
                             int32_t aNamespaceID = kNameSpaceID_None) const;
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
  int32_t IndexOfAttr(nsAtom* aLocalName, int32_t aNamespaceID = kNameSpaceID_None) const;

  // SetAndSwapMappedAttr swaps the current attribute value with aValue.
  // If the attribute was unset, an empty value will be swapped into aValue
  // and aHadValue will be set to false. Otherwise, aHadValue will be set to
  // true.
  nsresult SetAndSwapMappedAttr(nsAtom* aLocalName, nsAttrValue& aValue,
                                nsMappedAttributeElement* aContent,
                                nsHTMLStyleSheet* aSheet,
                                bool* aHadValue);
  nsresult SetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet) {
    if (!mImpl || !mImpl->mMappedAttrs) {
      return NS_OK;
    }
    return DoSetMappedAttrStyleSheet(aSheet);
  }

  // Update the rule mapping function on our mapped attributes, if we have any.
  // We take a nsMappedAttributeElement, not a nsMapRuleToAttributesFunc,
  // because the latter is defined in a header we can't include here.
  nsresult UpdateMappedAttrRuleMapper(nsMappedAttributeElement& aElement)
  {
    if (!mImpl || !mImpl->mMappedAttrs) {
      return NS_OK;
    }
    return DoUpdateMappedAttrRuleMapper(aElement);
  }

  void Compact();

  bool CanFitMoreAttrs() const
  {
    return AttrSlotCount() < ATTRCHILD_ARRAY_MAX_ATTR_COUNT ||
           !AttrSlotIsTaken(ATTRCHILD_ARRAY_MAX_ATTR_COUNT - 1);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  bool HasMappedAttrs() const
  {
    return MappedAttrCount();
  }
  const nsMappedAttributes* GetMapped() const;

  // Force this to have mapped attributes, even if those attributes are empty.
  nsresult ForceMapped(nsMappedAttributeElement* aContent, nsIDocument* aDocument);

  // Clear the servo declaration block on the mapped attributes, if any
  // Will assert off main thread
  void ClearMappedServoStyle();

  // Increases capacity (if necessary) to have enough space to accomodate the
  // unmapped attributes and children of |aOther|. If |aAllocateChildren| is not
  // true, only enough space for unmapped attributes will be reserved.
  // It is REQUIRED that this function be called ONLY when the array is empty.
  nsresult EnsureCapacityToClone(const nsAttrAndChildArray& aOther,
                                 bool aAllocateChildren);

private:
  nsAttrAndChildArray(const nsAttrAndChildArray& aOther) = delete;
  nsAttrAndChildArray& operator=(const nsAttrAndChildArray& aOther) = delete;

  void Clear();

  uint32_t NonMappedAttrCount() const;
  uint32_t MappedAttrCount() const;

  // Returns a non-null zero-refcount object.
  nsMappedAttributes*
  GetModifiableMapped(nsMappedAttributeElement* aContent,
                      nsHTMLStyleSheet* aSheet,
                      bool aWillAddAttr,
                      int32_t aAttrCount = 1);
  nsresult MakeMappedUnique(nsMappedAttributes* aAttributes);

  uint32_t AttrSlotsSize() const
  {
    return AttrSlotCount() * ATTRSIZE;
  }

  uint32_t AttrSlotCount() const
  {
    return mImpl ? mImpl->mAttrAndChildCount & ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK : 0;
  }

  bool AttrSlotIsTaken(uint32_t aSlot) const
  {
    MOZ_ASSERT(aSlot < AttrSlotCount(), "out-of-bounds");
    return mImpl->mBuffer[aSlot * ATTRSIZE];
  }

  void SetChildCount(uint32_t aCount)
  {
    mImpl->mAttrAndChildCount =
        (mImpl->mAttrAndChildCount & ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK) |
        (aCount << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS);
  }

  void SetAttrSlotCount(uint32_t aCount)
  {
    mImpl->mAttrAndChildCount =
        (mImpl->mAttrAndChildCount & ~ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK) |
        aCount;
  }

  void SetAttrSlotAndChildCount(uint32_t aSlotCount, uint32_t aChildCount)
  {
    mImpl->mAttrAndChildCount = aSlotCount |
      (aChildCount << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS);
  }

  bool GrowBy(uint32_t aGrowSize);
  bool AddAttrSlot();

  /**
   * Set *aPos to aChild and update sibling pointers as needed.  aIndex is the
   * index at which aChild is actually being inserted.  aChildCount is the
   * number of kids we had before the insertion.
   */
  inline void SetChildAtPos(void** aPos, nsIContent* aChild, uint32_t aIndex,
                            uint32_t aChildCount);

  /**
   * Guts of SetMappedAttrStyleSheet for the rare case when we have mapped attrs
   */
  nsresult DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet);

  /**
   * Guts of UpdateMappedAttrRuleMapper for the case  when we have mapped attrs.
   */
  nsresult DoUpdateMappedAttrRuleMapper(nsMappedAttributeElement& aElement);

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  struct Impl {
    uint32_t mAttrAndChildCount;
    uint32_t mBufferSize;
    nsMappedAttributes* mMappedAttrs;
    void* mBuffer[1];
  };

  Impl* mImpl;
};

#endif
