/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsCSSPropertySet.h"
#include "nsCSSValue.h"
#include "imgRequestProxy.h"

struct nsRuleData;
class nsCSSExpandedDataBlock;

namespace mozilla {
namespace css {
class Declaration;
}
}

/**
 * An |nsCSSCompressedDataBlock| holds a usually-immutable chunk of
 * property-value data for a CSS declaration block (which we misname a
 * |css::Declaration|).  Mutation is accomplished through
 * |nsCSSExpandedDataBlock| or in some cases via direct slot access.
 */
class nsCSSCompressedDataBlock {
private:
    friend class nsCSSExpandedDataBlock;

    // Only this class (via |CreateEmptyBlock|) or nsCSSExpandedDataBlock
    // (in |Compress|) can create compressed data blocks.
    nsCSSCompressedDataBlock(uint32_t aNumProps)
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
    const nsCSSValue* ValueFor(nsCSSProperty aProperty) const;

    /**
     * Attempt to replace the value for |aProperty| stored in this block
     * with the matching value stored in |aFromBlock|.
     * This method will fail (returning false) if |aProperty| is not
     * already in this block.  It will set |aChanged| to true if it
     * actually made a change to the block, but regardless, if it
     * returns true, the value in |aFromBlock| was erased.
     */
    bool TryReplaceValue(nsCSSProperty aProperty,
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

private:
    void* operator new(size_t aBaseSize, uint32_t aNumProps) {
        NS_ABORT_IF_FALSE(aBaseSize == sizeof(nsCSSCompressedDataBlock),
                          "unexpected size for nsCSSCompressedDataBlock");
        return ::operator new(aBaseSize + DataSize(aNumProps));
    }

public:
    // Ideally, |nsCSSProperty| would be |enum nsCSSProperty : int16_t|.  But
    // not all of the compilers we use are modern enough to support small
    // enums.  So we manually squeeze nsCSSProperty into 16 bits ourselves.
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
    // nsCSSProperty elements are stored -- each one compressed as a
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
        NS_ABORT_IF_FALSE(i < mNumProps, "value index out of range");
        return Values() + i;
    }

    nsCSSProperty PropertyAtIndex(uint32_t i) const {
        NS_ABORT_IF_FALSE(i < mNumProps, "property index out of range");
        nsCSSProperty prop = (nsCSSProperty)CompressedProperties()[i];
        NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(prop), "out of range");
        return prop;
    }

    void CopyValueToIndex(uint32_t i, nsCSSValue* aValue) {
        new (ValueAtIndex(i)) nsCSSValue(*aValue);
    }

    void RawCopyValueToIndex(uint32_t i, nsCSSValue* aValue) {
        memcpy(ValueAtIndex(i), aValue, sizeof(nsCSSValue));
    }

    void SetPropertyAtIndex(uint32_t i, nsCSSProperty aProperty) {
        NS_ABORT_IF_FALSE(i < mNumProps, "set property index out of range");
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
              "nsCSSProperty doesn't fit in StoredSizeOfCSSProperty");

class nsCSSExpandedDataBlock {
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
     */
    void Compress(nsCSSCompressedDataBlock **aNormalBlock,
                  nsCSSCompressedDataBlock **aImportantBlock);

    /**
     * Copy a value into this expanded block.  This does NOT destroy
     * the source value object.  |aProperty| cannot be a shorthand.
     */
    void AddLonghandProperty(nsCSSProperty aProperty, const nsCSSValue& aValue);

    /**
     * Clear the state of this expanded block.
     */
    void Clear();

    /**
     * Clear the data for the given property (including the set and
     * important bits).  Can be used with shorthand properties.
     */
    void ClearProperty(nsCSSProperty aPropID);

    /**
     * Same as ClearProperty, but faster and cannot be used with shorthands.
     */
    void ClearLonghandProperty(nsCSSProperty aPropID);

    /**
     * Transfer the state for |aPropID| (which may be a shorthand)
     * from |aFromBlock| to this block.  The property being transferred
     * is !important if |aIsImportant| is true, and should replace an
     * existing !important property regardless of its own importance
     * if |aOverrideImportant| is true.
     *
     * Returns true if something changed, false otherwise.  Calls
     * |ValueAppended| on |aDeclaration| if the property was not
     * previously set, or in any case if |aMustCallValueAppended| is true.
     */
    bool TransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                             nsCSSProperty aPropID,
                             bool aIsImportant,
                             bool aOverrideImportant,
                             bool aMustCallValueAppended,
                             mozilla::css::Declaration* aDeclaration);

    /**
     * Copies the values for aPropID into the specified aRuleData object.
     *
     * This is used for copying parsed-at-computed-value-time properties
     * that had variable references.  aPropID must be a longhand property.
     */
    void MapRuleInfoInto(nsCSSProperty aPropID, nsRuleData* aRuleData) const;

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
                               nsCSSProperty aPropID,
                               bool aIsImportant,
                               bool aOverrideImportant,
                               bool aMustCallValueAppended,
                               mozilla::css::Declaration* aDeclaration);

#ifdef DEBUG
    void DoAssertInitialState();
#endif

    /*
     * mPropertiesSet stores a bit for every property that is present,
     * to optimize compression of blocks with small numbers of
     * properties (the norm) and to allow quickly checking whether a
     * property is set in this block.
     */
    nsCSSPropertySet mPropertiesSet;
    /*
     * mPropertiesImportant indicates which properties are '!important'.
     */
    nsCSSPropertySet mPropertiesImportant;

    /*
     * Return the storage location within |this| of the value of the
     * property |aProperty|.
     */
    nsCSSValue* PropertyAt(nsCSSProperty aProperty) {
        NS_ABORT_IF_FALSE(0 <= aProperty &&
                          aProperty < eCSSProperty_COUNT_no_shorthands,
                          "property out of range");
        return &mValues[aProperty];
    }
    const nsCSSValue* PropertyAt(nsCSSProperty aProperty) const {
        NS_ABORT_IF_FALSE(0 <= aProperty &&
                          aProperty < eCSSProperty_COUNT_no_shorthands,
                          "property out of range");
        return &mValues[aProperty];
    }

    void SetPropertyBit(nsCSSProperty aProperty) {
        mPropertiesSet.AddProperty(aProperty);
    }

    void ClearPropertyBit(nsCSSProperty aProperty) {
        mPropertiesSet.RemoveProperty(aProperty);
    }

    bool HasPropertyBit(nsCSSProperty aProperty) {
        return mPropertiesSet.HasProperty(aProperty);
    }

    void SetImportantBit(nsCSSProperty aProperty) {
        mPropertiesImportant.AddProperty(aProperty);
    }

    void ClearImportantBit(nsCSSProperty aProperty) {
        mPropertiesImportant.RemoveProperty(aProperty);
    }

    bool HasImportantBit(nsCSSProperty aProperty) {
        return mPropertiesImportant.HasProperty(aProperty);
    }

    void ClearSets() {
        mPropertiesSet.Empty();
        mPropertiesImportant.Empty();
    }
};

#endif /* !defined(nsCSSDataBlock_h__) */
