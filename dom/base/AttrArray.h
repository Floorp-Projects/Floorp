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

class nsINode;
class nsIContent;
class nsMappedAttributes;
class nsHTMLStyleSheet;
class nsRuleWalker;
class nsMappedAttributeElement;

class AttrArray
{
  typedef mozilla::dom::BorrowedAttrInfo BorrowedAttrInfo;
public:
  AttrArray() = default;
  ~AttrArray() = default;

  bool HasAttrs() const
  {
    return NonMappedAttrCount() || MappedAttrCount();
  }

  uint32_t AttrCount() const
  {
    return NonMappedAttrCount() + MappedAttrCount();
  }

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
  // unmapped attributes of |aOther|.
  nsresult EnsureCapacityToClone(const AttrArray& aOther);

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

private:
  AttrArray(const AttrArray& aOther) = delete;
  AttrArray& operator=(const AttrArray& aOther) = delete;

  uint32_t NonMappedAttrCount() const
  {
    return mImpl ? mImpl->mAttrCount : 0;
  }

  uint32_t MappedAttrCount() const
  {
    return mImpl && mImpl->mMappedAttrs ? DoGetMappedAttrCount() : 0;
  }

  uint32_t DoGetMappedAttrCount() const;

  // Returns a non-null zero-refcount object.
  nsMappedAttributes*
  GetModifiableMapped(nsMappedAttributeElement* aContent,
                      nsHTMLStyleSheet* aSheet,
                      bool aWillAddAttr,
                      int32_t aAttrCount = 1);
  nsresult MakeMappedUnique(nsMappedAttributes* aAttributes);

  bool GrowBy(uint32_t aGrowSize);

  // Tries to create an attribute, growing the buffer if needed, with the given
  // name and value.
  //
  // The value is moved from the argument.
  //
  // `Name` can be anything you construct a `nsAttrName` with (either an atom or
  // a NodeInfo pointer).
  template<typename Name>
  nsresult AddNewAttribute(Name*, nsAttrValue&);

  /**
   * Guts of SetMappedAttrStyleSheet for the rare case when we have mapped attrs
   */
  nsresult DoSetMappedAttrStyleSheet(nsHTMLStyleSheet* aSheet);

  /**
   * Guts of UpdateMappedAttrRuleMapper for the case  when we have mapped attrs.
   */
  nsresult DoUpdateMappedAttrRuleMapper(nsMappedAttributeElement& aElement);

#ifdef _MSC_VER
// Disable MSVC warning 'nonstandard extension used: zero-sized array in struct/union'
#pragma warning(push)
#pragma warning(disable:4200)
#endif
  class Impl
  {
  public:

    constexpr static size_t AllocationSizeForAttributes(uint32_t aAttrCount)
    {
      return sizeof(Impl) + aAttrCount * sizeof(InternalAttr);
    }

    mozilla::Span<const InternalAttr> NonMappedAttrs() const
    {
      return mozilla::MakeSpan(static_cast<const InternalAttr*>(mBuffer), mAttrCount);
    }

    mozilla::Span<InternalAttr> NonMappedAttrs()
    {
      return mozilla::MakeSpan(static_cast<InternalAttr*>(mBuffer), mAttrCount);
    }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    ~Impl();

    uint32_t mAttrCount;
    uint32_t mCapacity; // In number of InternalAttrs

    // Manually refcounted.
    nsMappedAttributes* mMappedAttrs;

    // Allocated in the same buffer as `Impl`.
    InternalAttr mBuffer[0];
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif


  mozilla::Span<InternalAttr> NonMappedAttrs()
  {
    return mImpl ? mImpl->NonMappedAttrs() : mozilla::Span<InternalAttr>();
  }

  mozilla::Span<const InternalAttr> NonMappedAttrs() const
  {
    return mImpl ? mImpl->NonMappedAttrs() : mozilla::Span<const InternalAttr>();
  }

  mozilla::UniquePtr<Impl> mImpl;
};

#endif
