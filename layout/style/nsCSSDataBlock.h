/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsCSSDataBlock.h.
 *
 * The Initial Developer of the Original Code is L. David Baron.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCSSDataBlock_h__
#define nsCSSDataBlock_h__

#include "nsCSSStruct.h"

struct nsRuleData;

class nsCSSExpandedDataBlock;

/**
 * An |nsCSSCompressedDataBlock| holds an immutable chunk of
 * property-value data for a CSS declaration block (which we misname a
 * |nsCSSDeclaration|).  Mutation is accomplished through
 * |nsCSSExpandedDataBlock|.
 */
class nsCSSCompressedDataBlock {
public:
    friend class nsCSSExpandedDataBlock;

    /**
     * Do what |nsIStyleRule::MapRuleInfoInto| needs to do for a style
     * rule using this block for storage.
     */
    nsresult MapRuleInfoInto(nsRuleData *aRuleData) const;

    /**
     * Return the location at which the *value* for the property is
     * stored, or null if the block does not contain a value for the
     * property.  This is either an |nsCSSValue*|, |nsCSSRect*|, or an
     * |nsCSSValueList**|, etc.
     *
     * Inefficient (by design).
     */
    const void* StorageFor(nsCSSProperty aProperty) const;

    /**
     * Clone this block, or return null on out-of-memory.
     */
    nsCSSCompressedDataBlock* Clone() const;

    /**
     * Delete all the data stored in this block, and the block itself.
     */
    void Destroy();

    /**
     * Create a new nsCSSCompressedDataBlock holding no declarations.
     */
    static nsCSSCompressedDataBlock* CreateEmptyBlock();

private:
    PRInt32 mStyleBits; // the structs for which we have data, according to
                        // |nsCachedStyleData::GetBitForSID|.

    enum { block_chars = 4 }; // put 4 chars in the definition of the class
                              // to ensure size not inflated by alignment

    void* operator new(size_t aBaseSize, size_t aDataSize) {
        // subtract off the extra size to store |mBlock_|
        return ::operator new(aBaseSize + aDataSize -
                              sizeof(char) * block_chars);
    }

    nsCSSCompressedDataBlock() : mStyleBits(0) {}

    // Only this class (through |Destroy|) or nsCSSExpandedDataBlock (in
    // |Expand|) can delete compressed data blocks.
    ~nsCSSCompressedDataBlock() { }

    char* mBlockEnd; // the byte after the last valid byte
    char mBlock_[block_chars]; // must be the last member!

    char* Block() { return mBlock_; }
    char* BlockEnd() { return mBlockEnd; }
    const char* Block() const { return mBlock_; }
    const char* BlockEnd() const { return mBlockEnd; }
    ptrdiff_t DataSize() const { return BlockEnd() - Block(); }
};

class nsCSSExpandedDataBlock {
public:
    nsCSSExpandedDataBlock();
    ~nsCSSExpandedDataBlock();
    /*
     * When setting properties in an |nsCSSExpandedDataBlock|, callers
     * must make the appropriate |AddPropertyBit| call.
     */

    nsCSSFont mFont;
    nsCSSDisplay mDisplay;
    nsCSSMargin mMargin;
    nsCSSList mList;
    nsCSSPosition mPosition;
    nsCSSTable mTable;
    nsCSSColor mColor;
    nsCSSContent mContent;
    nsCSSText mText;
    nsCSSUserInterface mUserInterface;
    nsCSSAural mAural;
    nsCSSPage mPage;
    nsCSSBreaks mBreaks;
    nsCSSXUL mXUL;
#ifdef MOZ_SVG
    nsCSSSVG mSVG;
#endif
    nsCSSColumn mColumn;

    /**
     * Transfer all of the state from the compressed block to this
     * expanded block.  The state of this expanded block must be clear
     * beforehand.
     *
     * The compressed block passed in IS DESTROYED by this method and
     * set to null, and thus cannot be used again.  (This is necessary
     * because ownership of sub-objects is transferred to the expanded
     * block.)
     */
    void Expand(nsCSSCompressedDataBlock **aNormalBlock,
                nsCSSCompressedDataBlock **aImportantBlock);

    /**
     * Allocate a new compressed block and transfer all of the state
     * from this expanded block to the new compressed block, clearing
     * the state of this expanded block.
     */
    void Compress(nsCSSCompressedDataBlock **aNormalBlock,
                  nsCSSCompressedDataBlock **aImportantBlock);

    /**
     * Clear (and thus destroy) the state of this expanded block.
     */
    void Clear();

    /**
     * Clear the data for the given property (including the set and
     * important bits).
     */
    void ClearProperty(nsCSSProperty aPropID);

    void AssertInitialState() {
#ifdef DEBUG
        DoAssertInitialState();
#endif
    }

private:
    /**
     * Compute the size that will be occupied by the result of
     * |Compress|.
     */
    struct ComputeSizeResult {
        PRUint32 normal, important;
    };
    ComputeSizeResult ComputeSize();

    void DoExpand(nsCSSCompressedDataBlock *aBlock, PRBool aImportant);

#ifdef DEBUG
    void DoAssertInitialState();
#endif

    struct PropertyOffsetInfo {
        // XXX These could probably be pointer-to-member, if the casting can
        // be done correctly.
        size_t block_offset; // offset of value in nsCSSExpandedDataBlock
        size_t ruledata_struct_offset; // offset of nsRuleData* in nsRuleData
        size_t ruledata_member_offset; // offset of value in nsRuleData*
    };

    static const PropertyOffsetInfo kOffsetTable[];

    typedef PRUint8 property_set_type;
    enum { kPropertiesSetChunkSize = 8 }; // number of bits in
                                          // |property_set_type|.
    // number of |property_set_type|s in the set
    enum { kPropertiesSetChunkCount =
             (eCSSProperty_COUNT_no_shorthands + (kPropertiesSetChunkSize-1)) /
             kPropertiesSetChunkSize };
    /*
     * mPropertiesSet stores a bit for every property that may be
     * present, to optimize compression of blocks with small numbers of
     * properties (the norm).  The code does not rely on it to be exact;
     * it is allowable, although slower, if a bit is erroneously set
     * even though the property is not present.
     */
    property_set_type mPropertiesSet[kPropertiesSetChunkCount];
    /*
     * mPropertiesImportant indicates which properties are '!important'.
     */
    property_set_type mPropertiesImportant[kPropertiesSetChunkCount];

public:
    /*
     * Return the storage location within |this| of the value of the
     * property (i.e., either an |nsCSSValue*|, |nsCSSRect*|, or
     * |nsCSSValueList**| (etc.).
     */
    void* PropertyAt(nsCSSProperty aProperty) {
        const PropertyOffsetInfo& offsets =
            nsCSSExpandedDataBlock::kOffsetTable[aProperty];
        return NS_REINTERPRET_CAST(void*, NS_REINTERPRET_CAST(char*, this) +
                                          offsets.block_offset);
    }

    /*
     * Return the storage location within |aRuleData| of the value of
     * the property (i.e., either an |nsCSSValue*|, |nsCSSRect*|, or
     * |nsCSSValueList**| (etc.).
     */
    static void* RuleDataPropertyAt(nsRuleData *aRuleData,
                                    nsCSSProperty aProperty) {
        const PropertyOffsetInfo& offsets =
            nsCSSExpandedDataBlock::kOffsetTable[aProperty];
        NS_ASSERTION(offsets.ruledata_struct_offset != size_t(-1),
                     "property should not use CSS_PROP_BACKENDONLY");
        char* cssstruct = *NS_REINTERPRET_CAST(char**,
                              NS_REINTERPRET_CAST(char*, aRuleData) +
                              offsets.ruledata_struct_offset);
        return NS_REINTERPRET_CAST(void*,
                                   cssstruct + offsets.ruledata_member_offset);
    }

    void AssertInSetRange(nsCSSProperty aProperty) {
        NS_ASSERTION(0 <= aProperty &&
                     aProperty < eCSSProperty_COUNT_no_shorthands,
                     "out of bounds");
    }

    void SetPropertyBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        mPropertiesSet[aProperty / kPropertiesSetChunkSize] |=
            property_set_type(1 << (aProperty % kPropertiesSetChunkSize));
    }

    void ClearPropertyBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        mPropertiesSet[aProperty / kPropertiesSetChunkSize] &=
            ~property_set_type(1 << (aProperty % kPropertiesSetChunkSize));
    }

    PRBool HasPropertyBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        return (mPropertiesSet[aProperty / kPropertiesSetChunkSize] &
                (1 << (aProperty % kPropertiesSetChunkSize))) != 0;
    }

    void SetImportantBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        mPropertiesImportant[aProperty / kPropertiesSetChunkSize] |=
            property_set_type(1 << (aProperty % kPropertiesSetChunkSize));
    }

    void ClearImportantBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        mPropertiesImportant[aProperty / kPropertiesSetChunkSize] &=
            ~property_set_type(1 << (aProperty % kPropertiesSetChunkSize));
    }

    PRBool HasImportantBit(nsCSSProperty aProperty) {
        AssertInSetRange(aProperty);
        return (mPropertiesImportant[aProperty / kPropertiesSetChunkSize] &
                (1 << (aProperty % kPropertiesSetChunkSize))) != 0;
    }

    void ClearSets() {
        memset(mPropertiesSet, 0, sizeof(mPropertiesSet));
        memset(mPropertiesImportant, 0, sizeof(mPropertiesImportant));
    }
};

#endif /* !defined(nsCSSDataBlock_h__) */
