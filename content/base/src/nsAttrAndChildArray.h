/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nscore.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"

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
    (~PRUint32(0) >> ATTRCHILD_ARRAY_ATTR_SLOTS_BITS)

#define ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK \
    ((1 << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS) - 1)


#define ATTRSIZE (sizeof(InternalAttr) / sizeof(void*))

class nsAttrAndChildArray
{
public:
  nsAttrAndChildArray();
  ~nsAttrAndChildArray();

  PRUint32 ChildCount() const
  {
    return mImpl ? (mImpl->mAttrAndChildCount >> ATTRCHILD_ARRAY_ATTR_SLOTS_BITS) : 0;
  }
  nsIContent* ChildAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < ChildCount(), "out-of-bounds access in nsAttrAndChildArray");
    return reinterpret_cast<nsIContent*>(mImpl->mBuffer[AttrSlotsSize() + aPos]);
  }
  nsIContent* GetSafeChildAt(PRUint32 aPos) const;
  nsIContent * const * GetChildArray(PRUint32* aChildCount) const;
  nsresult AppendChild(nsIContent* aChild)
  {
    return InsertChildAt(aChild, ChildCount());
  }
  nsresult InsertChildAt(nsIContent* aChild, PRUint32 aPos);
  void RemoveChildAt(PRUint32 aPos);
  // Like RemoveChildAt but hands the reference to the child being
  // removed back to the caller instead of just releasing it.
  already_AddRefed<nsIContent> TakeChildAt(PRUint32 aPos);
  PRInt32 IndexOfChild(nsINode* aPossibleChild) const;

  PRUint32 AttrCount() const;
  const nsAttrValue* GetAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID = kNameSpaceID_None) const;
  const nsAttrValue* AttrAt(PRUint32 aPos) const;
  nsresult SetAndTakeAttr(nsIAtom* aLocalName, nsAttrValue& aValue);
  nsresult SetAndTakeAttr(nsINodeInfo* aName, nsAttrValue& aValue);

  // Remove the attr at position aPos.  The value of the attr is placed in
  // aValue; any value that was already in aValue is destroyed.
  nsresult RemoveAttrAt(PRUint32 aPos, nsAttrValue& aValue);

  // Returns attribute name at given position, *not* out-of-bounds safe
  const nsAttrName* AttrNameAt(PRUint32 aPos) const;

  // Returns attribute name at given position or null if aPos is out-of-bounds
  const nsAttrName* GetSafeAttrNameAt(PRUint32 aPos) const;

  const nsAttrName* GetExistingAttrNameFromQName(const nsAString& aName) const;
  PRInt32 IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID = kNameSpaceID_None) const;

  nsresult SetAndTakeMappedAttr(nsIAtom* aLocalName, nsAttrValue& aValue,
                                nsMappedAttributeElement* aContent,
                                nsHTMLStyleSheet* aSheet);
  nsresult SetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet) {
    if (!mImpl || !mImpl->mMappedAttrs) {
      return NS_OK;
    }
    return DoSetMappedAttrStyleSheet(aSheet);
  }
  void WalkMappedAttributeStyleRules(nsRuleWalker* aRuleWalker);

  void Compact();

  bool CanFitMoreAttrs() const
  {
    return AttrSlotCount() < ATTRCHILD_ARRAY_MAX_ATTR_COUNT ||
           !AttrSlotIsTaken(ATTRCHILD_ARRAY_MAX_ATTR_COUNT - 1);
  }

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;
  bool HasMappedAttrs() const
  {
    return MappedAttrCount();
  }

private:
  nsAttrAndChildArray(const nsAttrAndChildArray& aOther) MOZ_DELETE;
  nsAttrAndChildArray& operator=(const nsAttrAndChildArray& aOther) MOZ_DELETE;

  void Clear();

  PRUint32 NonMappedAttrCount() const;
  PRUint32 MappedAttrCount() const;

  nsresult GetModifiableMapped(nsMappedAttributeElement* aContent,
                               nsHTMLStyleSheet* aSheet,
                               bool aWillAddAttr,
                               nsMappedAttributes** aModifiable);
  nsresult MakeMappedUnique(nsMappedAttributes* aAttributes);

  PRUint32 AttrSlotsSize() const
  {
    return AttrSlotCount() * ATTRSIZE;
  }

  PRUint32 AttrSlotCount() const
  {
    return mImpl ? mImpl->mAttrAndChildCount & ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK : 0;
  }

  bool AttrSlotIsTaken(PRUint32 aSlot) const
  {
    NS_PRECONDITION(aSlot < AttrSlotCount(), "out-of-bounds");
    return mImpl->mBuffer[aSlot * ATTRSIZE];
  }

  void SetChildCount(PRUint32 aCount)
  {
    mImpl->mAttrAndChildCount = 
        (mImpl->mAttrAndChildCount & ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK) |
        (aCount << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS);
  }

  void SetAttrSlotCount(PRUint32 aCount)
  {
    mImpl->mAttrAndChildCount =
        (mImpl->mAttrAndChildCount & ~ATTRCHILD_ARRAY_ATTR_SLOTS_COUNT_MASK) |
        aCount;
  }

  void SetAttrSlotAndChildCount(PRUint32 aSlotCount, PRUint32 aChildCount)
  {
    mImpl->mAttrAndChildCount = aSlotCount |
      (aChildCount << ATTRCHILD_ARRAY_ATTR_SLOTS_BITS);
  }

  bool GrowBy(PRUint32 aGrowSize);
  bool AddAttrSlot();

  /**
   * Set *aPos to aChild and update sibling pointers as needed.  aIndex is the
   * index at which aChild is actually being inserted.  aChildCount is the
   * number of kids we had before the insertion.
   */
  inline void SetChildAtPos(void** aPos, nsIContent* aChild, PRUint32 aIndex,
                            PRUint32 aChildCount);

  /**
   * Guts of SetMappedAttrStyleSheet for the rare case when we have mapped attrs
   */
  nsresult DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet);

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  struct Impl {
    PRUint32 mAttrAndChildCount;
    PRUint32 mBufferSize;
    nsMappedAttributes* mMappedAttrs;
    void* mBuffer[1];
  };

  Impl* mImpl;
};

#endif
