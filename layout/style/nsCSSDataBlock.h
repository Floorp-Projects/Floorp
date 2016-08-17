/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * compact representation of the property-value pairs within a CSS
 * declaration, and the code for expanding and compacting it
 */

#ifndef nsCSSDataBlock_h__
#define nsCSSDataBlock_h__

#include "mozilla/MemoryReporting.h"
#include "nsCSSProps.h"
#include "nsCSSPropertyIDSet.h"
#include "nsCSSValue.h"
#include "nsStyleStruct.h"
#include "imgRequestProxy.h"

struct nsRuleData;
class nsCSSExpandedDataBlock;
class nsIDocument;

namespace mozilla {
namespace css {
class Declaration;
} // namespace css
} // namespace mozilla

/**
 * An |nsCSSCompressedDataBlock| holds a usually-immutable chunk of
 * property-value data for a CSS declaration block (which we misname a
 * |css::Declaration|).  Mutation is accomplished through
 * |nsCSSExpandedDataBlock| or in some cases via direct slot access.
 */
class nsCSSCompressedDataBlock
{
private:
  friend class nsCSSExpandedDataBlock;

  // Only this class (via |CreateEmptyBlock|) or nsCSSExpandedDataBlock
  // (in |Compress|) can create compressed data blocks.
  explicit nsCSSCompressedDataBlock(uint32_t aNumProps)
    : mStyleBits(0), mNumProps(aNumProps)
  {}

public:
  ~nsCSSCompressedDataBlock();

  /**
   * Do what |nsIStyleRule::MapRuleInfoInto| needs to do for a style
   * rule using this block for storage.
   */
  void MapRuleInfoInto(nsRuleData *aRuleData) const;

  /**
   * Return the location at which the *value* for the property is
   * stored, or null if the block does not contain a value for the
   * property.
   *
   * Inefficient (by design).
   *
   * Must not be called for shorthands.
   */
  const nsCSSValue* ValueFor(nsCSSPropertyID aProperty) const;

  /**
   * Attempt to replace the value for |aProperty| stored in this block
   * with the matching value stored in |aFromBlock|.
   * This method will fail (returning false) if |aProperty| is not
   * already in this block.  It will set |aChanged| to true if it
   * actually made a change to the block, but regardless, if it
   * returns true, the value in |aFromBlock| was erased.
   */
  bool TryReplaceValue(nsCSSPropertyID aProperty,
                         nsCSSExpandedDataBlock& aFromBlock,
                         bool* aChanged);

  /**
   * Clone this block, or return null on out-of-memory.
   */
  nsCSSCompressedDataBlock* Clone() const;

  /**
   * Create a new nsCSSCompressedDataBlock holding no declarations.
   */
  static nsCSSCompressedDataBlock* CreateEmptyBlock();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  bool HasDefaultBorderImageSlice() const;
  bool HasDefaultBorderImageWidth() const;
  bool HasDefaultBorderImageOutset() const;
  bool HasDefaultBorderImageRepeat() const;

  bool HasInheritedStyleData() const
  {
    return mStyleBits & NS_STYLE_INHERITED_STRUCT_MASK;
  }

private:
  void* operator new(size_t aBaseSize, uint32_t aNumProps) {
    MOZ_ASSERT(aBaseSize == sizeof(nsCSSCompressedDataBlock),
               "unexpected size for nsCSSCompressedDataBlock");
    return ::operator new(aBaseSize + DataSize(aNumProps));
  }

public:
  // Ideally, |nsCSSPropertyID| would be |enum nsCSSPropertyID : int16_t|.  But
  // not all of the compilers we use are modern enough to support small
  // enums.  So we manually squeeze nsCSSPropertyID into 16 bits ourselves.
  // The static assertion below ensures it fits.
  typedef int16_t CompressedCSSProperty;
  static const size_t MaxCompressedCSSProperty = INT16_MAX;

private:
  static size_t DataSize(uint32_t aNumProps) {
    return size_t(aNumProps) *
           (sizeof(nsCSSValue) + sizeof(CompressedCSSProperty));
  }

  int32_t mStyleBits; // the structs for which we have data, according to
                      // |nsCachedStyleData::GetBitForSID|.
  uint32_t mNumProps;
  // nsCSSValue elements are stored after these fields, and
  // nsCSSPropertyID elements are stored -- each one compressed as a
  // CompressedCSSProperty -- after the nsCSSValue elements.  Space for them
  // is allocated in |operator new| above.  The static assertions following
  // this class make sure that the value and property elements are aligned
  // appropriately.

  nsCSSValue* Values() const {
    return (nsCSSValue*)(this + 1);
  }

  CompressedCSSProperty* CompressedProperties() const {
    return (CompressedCSSProperty*)(Values() + mNumProps);
  }

  nsCSSValue* ValueAtIndex(uint32_t i) const {
    MOZ_ASSERT(i < mNumProps, "value index out of range");
    return Values() + i;
  }

  nsCSSPropertyID PropertyAtIndex(uint32_t i) const {
    MOZ_ASSERT(i < mNumProps, "property index out of range");
    nsCSSPropertyID prop = (nsCSSPropertyID)CompressedProperties()[i];
    MOZ_ASSERT(!nsCSSProps::IsShorthand(prop), "out of range");
    return prop;
  }

  void CopyValueToIndex(uint32_t i, nsCSSValue* aValue) {
    new (ValueAtIndex(i)) nsCSSValue(*aValue);
  }

  void RawCopyValueToIndex(uint32_t i, nsCSSValue* aValue) {
    memcpy(ValueAtIndex(i), aValue, sizeof(nsCSSValue));
  }

  void SetPropertyAtIndex(uint32_t i, nsCSSPropertyID aProperty) {
    MOZ_ASSERT(i < mNumProps, "set property index out of range");
    CompressedProperties()[i] = (CompressedCSSProperty)aProperty;
  }

  void SetNumPropsToZero() {
    mNumProps = 0;
  }
};

// Make sure the values and properties are aligned appropriately.  (These
// assertions are stronger than necessary to keep them simple.)
static_assert(sizeof(nsCSSCompressedDataBlock) == 8,
              "nsCSSCompressedDataBlock's size has changed");
static_assert(NS_ALIGNMENT_OF(nsCSSValue) == 4 || NS_ALIGNMENT_OF(nsCSSValue) == 8,
              "nsCSSValue doesn't align with nsCSSCompressedDataBlock");
static_assert(NS_ALIGNMENT_OF(nsCSSCompressedDataBlock::CompressedCSSProperty) == 2,
              "CompressedCSSProperty doesn't align with nsCSSValue");

// Make sure that sizeof(CompressedCSSProperty) is big enough.
static_assert(eCSSProperty_COUNT_no_shorthands <=
              nsCSSCompressedDataBlock::MaxCompressedCSSProperty,
              "nsCSSPropertyID doesn't fit in StoredSizeOfCSSProperty");

class nsCSSExpandedDataBlock
{
  friend class nsCSSCompressedDataBlock;

public:
  nsCSSExpandedDataBlock();
  ~nsCSSExpandedDataBlock();

private:
  /* Property storage may not be accessed directly; use AddLonghandProperty
   * and friends.
   */
  nsCSSValue mValues[eCSSProperty_COUNT_no_shorthands];

public:
  /**
   * Transfer all of the state from a pair of compressed data blocks
   * to this expanded block.  This expanded block must be clear
   * beforehand.
   *
   * This method DELETES both of the compressed data blocks it is
   * passed.  (This is necessary because ownership of sub-objects
   * is transferred to the expanded block.)
   */
  void Expand(nsCSSCompressedDataBlock *aNormalBlock,
              nsCSSCompressedDataBlock *aImportantBlock);

  /**
   * Allocate new compressed blocks and transfer all of the state
   * from this expanded block to the new blocks, clearing this
   * expanded block.  A normal block will always be allocated, but
   * an important block will only be allocated if there are
   * !important properties in the expanded block; otherwise
   * |*aImportantBlock| will be set to null.
   *
   * aOrder is an array of nsCSSPropertyID values specifying the order
   * to store values in the two data blocks.
   */
  void Compress(nsCSSCompressedDataBlock **aNormalBlock,
                nsCSSCompressedDataBlock **aImportantBlock,
                const nsTArray<uint32_t>& aOrder);

  /**
   * Copy a value into this expanded block.  This does NOT destroy
   * the source value object.  |aProperty| cannot be a shorthand.
   */
  void AddLonghandProperty(nsCSSPropertyID aProperty, const nsCSSValue& aValue);

  /**
   * Clear the state of this expanded block.
   */
  void Clear();

  /**
   * Clear the data for the given property (including the set and
   * important bits).  Can be used with shorthand properties.
   */
  void ClearProperty(nsCSSPropertyID aPropID);

  /**
   * Same as ClearProperty, but faster and cannot be used with shorthands.
   */
  void ClearLonghandProperty(nsCSSPropertyID aPropID);

  /**
   * Transfer the state for |aPropID| (which may be a shorthand)
   * from |aFromBlock| to this block.  The property being transferred
   * is !important if |aIsImportant| is true, and should replace an
   * existing !important property regardless of its own importance
   * if |aOverrideImportant| is true.  |aEnabledState| is used to
   * determine which longhand components of |aPropID| (if it is a
   * shorthand) to transfer.
   *
   * Returns true if something changed, false otherwise.  Calls
   * |ValueAppended| on |aDeclaration| if the property was not
   * previously set, or in any case if |aMustCallValueAppended| is true.
   * Calls |SetDocumentAndPageUseCounter| on |aSheetDocument| if it is
   * non-null and |aPropID| has a use counter.
   */
  bool TransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                         nsCSSPropertyID aPropID,
                         mozilla::CSSEnabledState aEnabledState,
                         bool aIsImportant,
                         bool aOverrideImportant,
                         bool aMustCallValueAppended,
                         mozilla::css::Declaration* aDeclaration,
                         nsIDocument* aSheetDocument);

  /**
   * Copies the values for aPropID into the specified aRuleData object.
   *
   * This is used for copying parsed-at-computed-value-time properties
   * that had variable references.  aPropID must be a longhand property.
   */
  void MapRuleInfoInto(nsCSSPropertyID aPropID, nsRuleData* aRuleData) const;

  void AssertInitialState() {
#ifdef DEBUG
    DoAssertInitialState();
#endif
  }

private:
  /**
   * Compute the number of properties that will be present in the
   * result of |Compress|.
   */
  void ComputeNumProps(uint32_t* aNumPropsNormal,
                       uint32_t* aNumPropsImportant);

  void DoExpand(nsCSSCompressedDataBlock *aBlock, bool aImportant);

  /**
   * Worker for TransferFromBlock; cannot be used with shorthands.
   */
  bool DoTransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                             nsCSSPropertyID aPropID,
                             bool aIsImportant,
                             bool aOverrideImportant,
                             bool aMustCallValueAppended,
                             mozilla::css::Declaration* aDeclaration,
                             nsIDocument* aSheetDocument);

#ifdef DEBUG
  void DoAssertInitialState();
#endif

  /*
   * mPropertiesSet stores a bit for every property that is present,
   * to optimize compression of blocks with small numbers of
   * properties (the norm) and to allow quickly checking whether a
   * property is set in this block.
   */
  nsCSSPropertyIDSet mPropertiesSet;
  /*
   * mPropertiesImportant indicates which properties are '!important'.
   */
  nsCSSPropertyIDSet mPropertiesImportant;

  /*
   * Return the storage location within |this| of the value of the
   * property |aProperty|.
   */
  nsCSSValue* PropertyAt(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty &&
               aProperty < eCSSProperty_COUNT_no_shorthands,
               "property out of range");
    return &mValues[aProperty];
  }
  const nsCSSValue* PropertyAt(nsCSSPropertyID aProperty) const {
    MOZ_ASSERT(0 <= aProperty &&
               aProperty < eCSSProperty_COUNT_no_shorthands,
               "property out of range");
    return &mValues[aProperty];
  }

  void SetPropertyBit(nsCSSPropertyID aProperty) {
    mPropertiesSet.AddProperty(aProperty);
  }

  void ClearPropertyBit(nsCSSPropertyID aProperty) {
    mPropertiesSet.RemoveProperty(aProperty);
  }

  bool HasPropertyBit(nsCSSPropertyID aProperty) {
    return mPropertiesSet.HasProperty(aProperty);
  }

  void SetImportantBit(nsCSSPropertyID aProperty) {
    mPropertiesImportant.AddProperty(aProperty);
  }

  void ClearImportantBit(nsCSSPropertyID aProperty) {
    mPropertiesImportant.RemoveProperty(aProperty);
  }

  bool HasImportantBit(nsCSSPropertyID aProperty) {
    return mPropertiesImportant.HasProperty(aProperty);
  }

  void ClearSets() {
    mPropertiesSet.Empty();
    mPropertiesImportant.Empty();
  }
};

#endif /* !defined(nsCSSDataBlock_h__) */
