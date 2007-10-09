/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is nsCSSDataBlock.cpp.
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

/*
 * compact representation of the property-value pairs within a CSS
 * declaration, and the code for expanding and compacting it
 */

#include "nsCSSDataBlock.h"
#include "nsCSSProps.h"
#include "nsRuleData.h"
#include "nsRuleNode.h"
#include "nsStyleSet.h"

/*
 * nsCSSCompressedDataBlock holds property-value pairs corresponding to
 * CSS declaration blocks.  The value is stored in one of the six CSS
 * data types.  These six types are nsCSSValue, nsCSSRect,
 * nsCSSValueList, nsCSSCounterData, nsCSSQuotes, and nsCSSValuePair, and
 * each correspond to a value of the nsCSSType enumeration.
 *
 * The storage strategy uses the CDB*Storage structs below to help
 * ensure that all the types remain properly aligned.  nsCSSValue's
 * alignment requirements cannot be weaker than any others, since it
 * contains a pointer and an enumeration.
 *
 * The simple types, nsCSSValue and nsCSSRect have the nsCSSValue or
 * nsCSSRect objects stored in the block.  The list types have only a
 * pointer to the first element in the list stored in the block.
 */

struct CDBValueStorage {
    nsCSSProperty property;
    nsCSSValue value;
};

struct CDBRectStorage {
    nsCSSProperty property;
    nsCSSRect value;

};

struct CDBValuePairStorage {
    nsCSSProperty property;
    nsCSSValuePair value;
};

struct CDBPointerStorage {
    nsCSSProperty property;
    void *value;
};

enum {
    CDBValueStorage_advance = sizeof(CDBValueStorage),
    CDBRectStorage_advance = sizeof(CDBRectStorage),
    CDBValuePairStorage_advance = sizeof(CDBValuePairStorage),
    // round up using the closest estimate we can get of the alignment
    // requirements of nsCSSValue:
    CDBPointerStorage_advance = PR_ROUNDUP(sizeof(CDBPointerStorage),
                                sizeof(CDBValueStorage) - sizeof(nsCSSValue))
};

/*
 * Define a bunch of utility functions for getting the property or any
 * of the value types when the cursor is at the beginning of the storage
 * for the property-value pair.  The versions taking a non-const cursor
 * argument return a reference so that the caller can assign into the
 * result.
 */

inline nsCSSProperty& PropertyAtCursor(char *aCursor) {
    return *reinterpret_cast<nsCSSProperty*>(aCursor);
}

inline nsCSSProperty PropertyAtCursor(const char *aCursor) {
    return *reinterpret_cast<const nsCSSProperty*>(aCursor);
}

inline nsCSSValue* ValueAtCursor(char *aCursor) {
    return & reinterpret_cast<CDBValueStorage*>(aCursor)->value;
}

inline const nsCSSValue* ValueAtCursor(const char *aCursor) {
    return & reinterpret_cast<const CDBValueStorage*>(aCursor)->value;
}

inline nsCSSRect* RectAtCursor(char *aCursor) {
    return & reinterpret_cast<CDBRectStorage*>(aCursor)->value;
}

inline const nsCSSRect* RectAtCursor(const char *aCursor) {
    return & reinterpret_cast<const CDBRectStorage*>(aCursor)->value;
}

inline nsCSSValuePair* ValuePairAtCursor(char *aCursor) {
  return & reinterpret_cast<CDBValuePairStorage*>(aCursor)->value;
}

inline const nsCSSValuePair* ValuePairAtCursor(const char *aCursor) {
  return & reinterpret_cast<const CDBValuePairStorage*>(aCursor)->value;
}

inline void*& PointerAtCursor(char *aCursor) {
    return reinterpret_cast<CDBPointerStorage*>(aCursor)->value;
}

inline void* PointerAtCursor(const char *aCursor) {
    return reinterpret_cast<const CDBPointerStorage*>(aCursor)->value;
}

inline nsCSSValueList*& ValueListAtCursor(char *aCursor) {
    return * reinterpret_cast<nsCSSValueList**>
                             (& reinterpret_cast<CDBPointerStorage*>(aCursor)->value);
}

inline nsCSSValueList* ValueListAtCursor(const char *aCursor) {
    return static_cast<nsCSSValueList*>
                      (reinterpret_cast<const CDBPointerStorage*>(aCursor)->value);
}

inline nsCSSCounterData*& CounterDataAtCursor(char *aCursor) {
    return * reinterpret_cast<nsCSSCounterData**>
                             (& reinterpret_cast<CDBPointerStorage*>(aCursor)->value);
}

inline nsCSSCounterData* CounterDataAtCursor(const char *aCursor) {
    return static_cast<nsCSSCounterData*>
                      (reinterpret_cast<const CDBPointerStorage*>(aCursor)->value);
}

inline nsCSSQuotes*& QuotesAtCursor(char *aCursor) {
    return * reinterpret_cast<nsCSSQuotes**>
                             (& reinterpret_cast<CDBPointerStorage*>(aCursor)->value);
}

inline nsCSSQuotes* QuotesAtCursor(const char *aCursor) {
    return static_cast<nsCSSQuotes*>
                      (reinterpret_cast<const CDBPointerStorage*>(aCursor)->value);
}

static PRBool
ShouldIgnoreColors(nsRuleData *aRuleData)
{
    nsPresContext *presContext = aRuleData->mPresContext;
    return aRuleData->mLevel != nsStyleSet::eAgentSheet &&
           aRuleData->mLevel != nsStyleSet::eUserSheet &&
           !presContext->GetCachedBoolPref(kPresContext_UseDocumentColors) &&
           !presContext->IsChrome();
}

nsresult
nsCSSCompressedDataBlock::MapRuleInfoInto(nsRuleData *aRuleData) const
{
    // If we have no data for these structs, then return immediately.
    // This optimization should make us return most of the time, so we
    // have to worry much less (although still some) about the speed of
    // the rest of the function.
    if (!(aRuleData->mSIDs & mStyleBits))
        return NS_OK;

    const char* cursor = Block();
    const char* cursor_end = BlockEnd();
    while (cursor < cursor_end) {
        nsCSSProperty iProp = PropertyAtCursor(cursor);
        NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                     "out of range");
        if (nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]) &
            aRuleData->mSIDs) {
            void *prop =
                nsCSSExpandedDataBlock::RuleDataPropertyAt(aRuleData, iProp);
            switch (nsCSSProps::kTypeTable[iProp]) {
                case eCSSType_Value: {
                    nsCSSValue* target = static_cast<nsCSSValue*>(prop);
                    if (target->GetUnit() == eCSSUnit_Null) {
                        const nsCSSValue *val = ValueAtCursor(cursor);
                        NS_ASSERTION(val->GetUnit() != eCSSUnit_Null, "oops");
                        if ((iProp == eCSSProperty_background_image ||
                             iProp == eCSSProperty_list_style_image) &&
                            val->GetUnit() == eCSSUnit_URL) {
                            val->StartImageLoad(aRuleData->mPresContext->Document());
                        }
                        *target = *val;
                        if (iProp == eCSSProperty_font_family) {
                            // XXX Are there other things like this?
                            aRuleData->mFontData->mFamilyFromHTML = PR_FALSE;
                        }
                        else if (iProp == eCSSProperty_color ||
                                 iProp == eCSSProperty_background_color ||
                                 iProp == eCSSProperty_background_image ||
                                 iProp == eCSSProperty_border_top_color ||
                                 iProp == eCSSProperty_border_right_color ||
                                 iProp == eCSSProperty_border_bottom_color ||
                                 iProp == eCSSProperty_border_left_color ||
                                 iProp == eCSSProperty_outline_color) {
                            if (ShouldIgnoreColors(aRuleData)) {
                                if (iProp == eCSSProperty_background_color) {
                                    // Force non-'transparent' background
                                    // colors to the user's default.
                                    nsCSSUnit u = target->GetUnit();
                                    if (u != eCSSUnit_Enumerated &&
                                        u != eCSSUnit_Inherit &&
                                        u != eCSSUnit_Initial) {
                                        target->SetColorValue(aRuleData->
                                            mPresContext->
                                            DefaultBackgroundColor());
                                    }
                                } else {
                                    // Ignore 'color', 'border-*-color', and
                                    // 'background-image'
                                    *target = nsCSSValue();
                                }
                            }
                        }
                    }
                    cursor += CDBValueStorage_advance;
                } break;

                case eCSSType_Rect: {
                    const nsCSSRect* val = RectAtCursor(cursor);
                    NS_ASSERTION(val->HasValue(), "oops");
                    nsCSSRect* target = static_cast<nsCSSRect*>(prop);
                    if (target->mTop.GetUnit() == eCSSUnit_Null)
                        target->mTop = val->mTop;
                    if (target->mRight.GetUnit() == eCSSUnit_Null)
                        target->mRight = val->mRight;
                    if (target->mBottom.GetUnit() == eCSSUnit_Null)
                        target->mBottom = val->mBottom;
                    if (target->mLeft.GetUnit() == eCSSUnit_Null)
                        target->mLeft = val->mLeft;
                    cursor += CDBRectStorage_advance;
                } break;

                case eCSSType_ValuePair: {
                    const nsCSSValuePair* val = ValuePairAtCursor(cursor);
                    NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                                 val->mYValue.GetUnit() != eCSSUnit_Null, "oops");
                    nsCSSValuePair* target = static_cast<nsCSSValuePair*>(prop);
                    if (target->mXValue.GetUnit() == eCSSUnit_Null)
                        target->mXValue = val->mXValue;
                    if (target->mYValue.GetUnit() == eCSSUnit_Null)
                        target->mYValue = val->mYValue;
                    cursor += CDBValuePairStorage_advance;
                } break;

                case eCSSType_ValueList:
                    if (iProp == eCSSProperty_content) {
                        for (nsCSSValueList* l = ValueListAtCursor(cursor);
                             l; l = l->mNext)
                            if (l->mValue.GetUnit() == eCSSUnit_URL)
                                l->mValue.StartImageLoad(
                                    aRuleData->mPresContext->Document());
                    } else if (iProp == eCSSProperty_cursor) {
                        for (nsCSSValueList* l = ValueListAtCursor(cursor);
                             l; l = l->mNext)
                            if (l->mValue.GetUnit() == eCSSUnit_Array) {
                                // Don't try to restart loads we've already
                                // started
                                nsCSSValue& val =
                                    l->mValue.GetArrayValue()->Item(0);
                                if (val.GetUnit() == eCSSUnit_URL)
                                    val.StartImageLoad(
                                      aRuleData->mPresContext->Document());
                            }
                    }
                // fall through
                case eCSSType_CounterData:
                case eCSSType_Quotes: {
                    void** target = static_cast<void**>(prop);
                    if (!*target) {
                        void* val = PointerAtCursor(cursor);
                        NS_ASSERTION(val, "oops");
                        *target = val;

                        if (iProp == eCSSProperty_border_top_colors ||
                            iProp == eCSSProperty_border_right_colors ||
                            iProp == eCSSProperty_border_bottom_colors ||
                            iProp == eCSSProperty_border_left_colors) {
                            if (ShouldIgnoreColors(aRuleData)) {
                                *target = nsnull;
                            }
                        }
                    }
                    cursor += CDBPointerStorage_advance;
                } break;
            }
        } else {
            switch (nsCSSProps::kTypeTable[iProp]) {
                case eCSSType_Value: {
                    cursor += CDBValueStorage_advance;
                } break;

                case eCSSType_Rect: {
                    cursor += CDBRectStorage_advance;
                } break;

                case eCSSType_ValuePair: {
                    cursor += CDBValuePairStorage_advance;
                } break;

                case eCSSType_ValueList:
                case eCSSType_CounterData:
                case eCSSType_Quotes: {
                    cursor += CDBPointerStorage_advance;
                } break;
            }
        }
    }
    NS_ASSERTION(cursor == cursor_end, "inconsistent data");

    return NS_OK;
}

const void*
nsCSSCompressedDataBlock::StorageFor(nsCSSProperty aProperty) const
{
    // If we have no data for this struct, then return immediately.
    // This optimization should make us return most of the time, so we
    // have to worry much less (although still some) about the speed of
    // the rest of the function.
    if (!(nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]) &
          mStyleBits))
        return nsnull;

    const char* cursor = Block();
    const char* cursor_end = BlockEnd();
    while (cursor < cursor_end) {
        nsCSSProperty iProp = PropertyAtCursor(cursor);
        NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                     "out of range");
        if (iProp == aProperty) {
            switch (nsCSSProps::kTypeTable[iProp]) {
                case eCSSType_Value: {
                    return ValueAtCursor(cursor);
                }
                case eCSSType_Rect: {
                    return RectAtCursor(cursor);
                }
                case eCSSType_ValuePair: {
                    return ValuePairAtCursor(cursor);
                }
                case eCSSType_ValueList:
                case eCSSType_CounterData:
                case eCSSType_Quotes: {
                    return &PointerAtCursor(const_cast<char*>(cursor));
                }
            }
        }
        switch (nsCSSProps::kTypeTable[iProp]) {
            case eCSSType_Value: {
                cursor += CDBValueStorage_advance;
            } break;

            case eCSSType_Rect: {
                cursor += CDBRectStorage_advance;
            } break;

            case eCSSType_ValuePair: {
                cursor += CDBValuePairStorage_advance;
            } break;

            case eCSSType_ValueList:
            case eCSSType_CounterData:
            case eCSSType_Quotes: {
                cursor += CDBPointerStorage_advance;
            } break;
        }
    }
    NS_ASSERTION(cursor == cursor_end, "inconsistent data");

    return nsnull;
}

nsCSSCompressedDataBlock*
nsCSSCompressedDataBlock::Clone() const
{
    const char *cursor = Block(), *cursor_end = BlockEnd();
    char *result_cursor;

    nsCSSCompressedDataBlock *result =
        new(cursor_end - cursor) nsCSSCompressedDataBlock();
    if (!result)
        return nsnull;
    result_cursor = result->Block();

    while (cursor < cursor_end) {
        nsCSSProperty iProp = PropertyAtCursor(cursor);
        NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                     "out of range");
        PropertyAtCursor(result_cursor) = iProp;

        switch (nsCSSProps::kTypeTable[iProp]) {
            case eCSSType_Value: {
                const nsCSSValue* val = ValueAtCursor(cursor);
                NS_ASSERTION(val->GetUnit() != eCSSUnit_Null, "oops");
                nsCSSValue *result_val = ValueAtCursor(result_cursor);
                new (result_val) nsCSSValue(*val);
                cursor += CDBValueStorage_advance;
                result_cursor +=  CDBValueStorage_advance;
            } break;

            case eCSSType_Rect: {
                const nsCSSRect* val = RectAtCursor(cursor);
                NS_ASSERTION(val->HasValue(), "oops");
                nsCSSRect* result_val = RectAtCursor(result_cursor);
                new (result_val) nsCSSRect(*val);
                cursor += CDBRectStorage_advance;
                result_cursor += CDBRectStorage_advance;
            } break;

            case eCSSType_ValuePair: {
                const nsCSSValuePair* val = ValuePairAtCursor(cursor);
                NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                             val->mYValue.GetUnit() != eCSSUnit_Null, "oops");
                nsCSSValuePair* result_val = ValuePairAtCursor(result_cursor);
                new (result_val) nsCSSValuePair(*val);
                cursor += CDBValuePairStorage_advance;
                result_cursor += CDBValuePairStorage_advance;
            } break;

            case eCSSType_ValueList:
            case eCSSType_CounterData:
            case eCSSType_Quotes: {
                void *copy;
                NS_ASSERTION(PointerAtCursor(cursor), "oops");
                switch (nsCSSProps::kTypeTable[iProp]) {
                    default:
                        NS_NOTREACHED("unreachable");
                        // fall through to keep gcc's uninitialized
                        // variable warning quiet
                    case eCSSType_ValueList:
                        copy = new nsCSSValueList(*ValueListAtCursor(cursor));
                        break;
                    case eCSSType_CounterData:
                        copy =
                            new nsCSSCounterData(*CounterDataAtCursor(cursor));
                        break;
                    case eCSSType_Quotes:
                        copy = new nsCSSQuotes(*QuotesAtCursor(cursor));
                        break;
                }
                if (!copy) {
                    result->mBlockEnd = result_cursor;
                    result->Destroy();
                    return nsnull;
                }
                PointerAtCursor(result_cursor) = copy;
                cursor += CDBPointerStorage_advance;
                result_cursor += CDBPointerStorage_advance;
            } break;
        }
    }
    NS_ASSERTION(cursor == cursor_end, "inconsistent data");

    result->mBlockEnd = result_cursor;
    result->mStyleBits = mStyleBits;
    NS_ASSERTION(result->DataSize() == DataSize(), "wrong size");
    return result;
}

void
nsCSSCompressedDataBlock::Destroy()
{
    const char* cursor = Block();
    const char* cursor_end = BlockEnd();
    while (cursor < cursor_end) {
        nsCSSProperty iProp = PropertyAtCursor(cursor);
        NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                     "out of range");

        switch (nsCSSProps::kTypeTable[iProp]) {
            case eCSSType_Value: {
                const nsCSSValue* val = ValueAtCursor(cursor);
                NS_ASSERTION(val->GetUnit() != eCSSUnit_Null, "oops");
                val->~nsCSSValue();
                cursor += CDBValueStorage_advance;
            } break;

            case eCSSType_Rect: {
                const nsCSSRect* val = RectAtCursor(cursor);
                NS_ASSERTION(val->HasValue(), "oops");
                val->~nsCSSRect();
                cursor += CDBRectStorage_advance;
            } break;

            case eCSSType_ValuePair: {
                const nsCSSValuePair* val = ValuePairAtCursor(cursor);
                NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                             val->mYValue.GetUnit() != eCSSUnit_Null, "oops");
                val->~nsCSSValuePair();
                cursor += CDBValuePairStorage_advance;
            } break;

            case eCSSType_ValueList: {
                nsCSSValueList* val = ValueListAtCursor(cursor);
                NS_ASSERTION(val, "oops");
                delete val;
                cursor += CDBPointerStorage_advance;
            } break;

            case eCSSType_CounterData: {
                nsCSSCounterData* val = CounterDataAtCursor(cursor);
                NS_ASSERTION(val, "oops");
                delete val;
                cursor += CDBPointerStorage_advance;
            } break;

            case eCSSType_Quotes: {
                nsCSSQuotes* val = QuotesAtCursor(cursor);
                NS_ASSERTION(val, "oops");
                delete val;
                cursor += CDBPointerStorage_advance;
            } break;
        }
    }
    NS_ASSERTION(cursor == cursor_end, "inconsistent data");
    delete this;
}

/* static */ nsCSSCompressedDataBlock*
nsCSSCompressedDataBlock::CreateEmptyBlock()
{
    nsCSSCompressedDataBlock *result = new(0) nsCSSCompressedDataBlock();
    if (!result)
        return nsnull;
    result->mBlockEnd = result->Block();
    return result;
}

/*****************************************************************************/

nsCSSExpandedDataBlock::nsCSSExpandedDataBlock()
{
    ClearSets();
    AssertInitialState();
}

nsCSSExpandedDataBlock::~nsCSSExpandedDataBlock()
{
    AssertInitialState();
}

const nsCSSExpandedDataBlock::PropertyOffsetInfo
nsCSSExpandedDataBlock::kOffsetTable[eCSSProperty_COUNT_no_shorthands] = {
    #define CSS_PROP_BACKENDONLY(name_, id_, method_, datastruct_, member_, type_, kwtable_) \
        { offsetof(nsCSSExpandedDataBlock, m##datastruct_.member_),           \
          size_t(-1),                                                         \
          size_t(-1) },
    #define CSS_PROP(name_, id_, method_, datastruct_, member_, type_, kwtable_) \
        { offsetof(nsCSSExpandedDataBlock, m##datastruct_.member_),           \
          offsetof(nsRuleData, m##datastruct_##Data),                         \
          offsetof(nsRuleData##datastruct_, member_) },
    #include "nsCSSPropList.h"
    #undef CSS_PROP
    #undef CSS_PROP_BACKENDONLY
};

void
nsCSSExpandedDataBlock::DoExpand(nsCSSCompressedDataBlock *aBlock,
                                 PRBool aImportant)
{
    NS_PRECONDITION(aBlock, "unexpected null block");

    /*
     * Save needless copying and allocation by copying the memory
     * corresponding to the stored data in the compressed block, and
     * then, to avoid destructors, deleting the compressed block by
     * calling |delete| instead of using its |Destroy| method.
     */
    const char* cursor = aBlock->Block();
    const char* cursor_end = aBlock->BlockEnd();
    while (cursor < cursor_end) {
        nsCSSProperty iProp = PropertyAtCursor(cursor);
        NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                     "out of range");
        NS_ASSERTION(!HasPropertyBit(iProp),
                     "compressed block has property multiple times");
        SetPropertyBit(iProp);
        if (aImportant)
            SetImportantBit(iProp);
        void *prop = PropertyAt(iProp);

        switch (nsCSSProps::kTypeTable[iProp]) {
            case eCSSType_Value: {
                const nsCSSValue* val = ValueAtCursor(cursor);
                nsCSSValue* dest = static_cast<nsCSSValue*>(prop);
                NS_ASSERTION(val->GetUnit() != eCSSUnit_Null, "oops");
                NS_ASSERTION(dest->GetUnit() == eCSSUnit_Null,
                             "expanding into non-empty block");
#ifdef NS_BUILD_REFCNT_LOGGING
                dest->~nsCSSValue();
#endif
                memcpy(dest, val, sizeof(nsCSSValue));
                cursor += CDBValueStorage_advance;
            } break;

            case eCSSType_Rect: {
                const nsCSSRect* val = RectAtCursor(cursor);
                nsCSSRect* dest = static_cast<nsCSSRect*>(prop);
                NS_ASSERTION(val->HasValue(), "oops");
                NS_ASSERTION(!dest->HasValue(),
                             "expanding into non-empty block");
#ifdef NS_BUILD_REFCNT_LOGGING
                dest->~nsCSSRect();
#endif
                memcpy(dest, val, sizeof(nsCSSRect));
                cursor += CDBRectStorage_advance;
            } break;

            case eCSSType_ValuePair: {
                const nsCSSValuePair* val = ValuePairAtCursor(cursor);
                nsCSSValuePair* dest = static_cast<nsCSSValuePair*>(prop);
                NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                             val->mYValue.GetUnit() != eCSSUnit_Null, "oops");
                NS_ASSERTION(dest->mXValue.GetUnit() == eCSSUnit_Null &&
                             dest->mYValue.GetUnit() == eCSSUnit_Null,
                             "expanding into non-empty block");
#ifdef NS_BUILD_REFCNT_LOGGING
                dest->~nsCSSValuePair();
#endif
                memcpy(dest, val, sizeof(nsCSSValuePair));
                cursor += CDBValuePairStorage_advance;
            } break;

            case eCSSType_ValueList:
            case eCSSType_CounterData:
            case eCSSType_Quotes: {
                void* val = PointerAtCursor(cursor);
                void** dest = static_cast<void**>(prop);
                NS_ASSERTION(val, "oops");
                NS_ASSERTION(!*dest, "expanding into non-empty block");
                *dest = val;
                cursor += CDBPointerStorage_advance;
            } break;
        }
    }
    NS_ASSERTION(cursor == cursor_end, "inconsistent data");

    delete aBlock;
}

void
nsCSSExpandedDataBlock::Expand(nsCSSCompressedDataBlock **aNormalBlock,
                               nsCSSCompressedDataBlock **aImportantBlock)
{
    NS_PRECONDITION(*aNormalBlock, "unexpected null block");
    AssertInitialState();

    DoExpand(*aNormalBlock, PR_FALSE);
    *aNormalBlock = nsnull;
    if (*aImportantBlock) {
        DoExpand(*aImportantBlock, PR_TRUE);
        *aImportantBlock = nsnull;
    }
}

nsCSSExpandedDataBlock::ComputeSizeResult
nsCSSExpandedDataBlock::ComputeSize()
{
    ComputeSizeResult result = {0, 0};
    for (PRUint32 iHigh = 0; iHigh < NS_ARRAY_LENGTH(mPropertiesSet); ++iHigh) {
        if (mPropertiesSet[iHigh] == 0)
            continue;
        for (PRInt32 iLow = 0; iLow < kPropertiesSetChunkSize; ++iLow) {
            if ((mPropertiesSet[iHigh] & (1 << iLow)) == 0)
                continue;
            nsCSSProperty iProp =
                nsCSSProperty(iHigh * kPropertiesSetChunkSize + iLow);
            NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                         "out of range");
            void *prop = PropertyAt(iProp);
            PRUint32 increment = 0;
            switch (nsCSSProps::kTypeTable[iProp]) {
                case eCSSType_Value: {
#ifdef DEBUG
                    nsCSSValue* val = static_cast<nsCSSValue*>(prop);
                    NS_ASSERTION(val->GetUnit() != eCSSUnit_Null,
                                 "null value while computing size");
#endif
                    increment = CDBValueStorage_advance;
                } break;

                case eCSSType_Rect: {
#ifdef DEBUG
                    nsCSSRect* val = static_cast<nsCSSRect*>(prop);
                    NS_ASSERTION(val->HasValue(),
                                 "Valueless rect while computing size");
#endif
                    increment = CDBRectStorage_advance;
                } break;

                case eCSSType_ValuePair: {
#ifdef DEBUG
                    nsCSSValuePair* val = static_cast<nsCSSValuePair*>(prop);
                    NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                                 val->mYValue.GetUnit() != eCSSUnit_Null,
                                 "Valueless pair while computing size");
#endif
                    increment = CDBValuePairStorage_advance;
                } break;

                case eCSSType_ValueList:
                case eCSSType_CounterData:
                case eCSSType_Quotes: {
#ifdef DEBUG
                    void* val = *static_cast<void**>(prop);
                    NS_ASSERTION(val, "Null pointer while computing size");
#endif
                    increment = CDBPointerStorage_advance;
                } break;
            }
            if ((mPropertiesImportant[iHigh] & (1 << iLow)) == 0)
                result.normal += increment;
            else
                result.important += increment;
        }
    }
    return result;
}

void
nsCSSExpandedDataBlock::Compress(nsCSSCompressedDataBlock **aNormalBlock,
                                 nsCSSCompressedDataBlock **aImportantBlock)
{
    nsCSSCompressedDataBlock *result_normal, *result_important;
    char *cursor_normal, *cursor_important;

    ComputeSizeResult size = ComputeSize();
    
    result_normal = new(size.normal) nsCSSCompressedDataBlock();
    if (!result_normal) {
        *aNormalBlock = nsnull;
        *aImportantBlock = nsnull;
        return;
    }
    cursor_normal = result_normal->Block();

    if (size.important != 0) {
        result_important = new(size.important) nsCSSCompressedDataBlock();
        if (!result_important) {
            delete result_normal;
            *aNormalBlock = nsnull;
            *aImportantBlock = nsnull;
            return;
        }
        cursor_important = result_important->Block();
    } else {
        result_important = nsnull;
    }

    /*
     * Save needless copying and allocation by copying the memory
     * corresponding to the stored data in the expanded block, and then
     * clearing the data in the expanded block.
     */
    for (PRUint32 iHigh = 0; iHigh < NS_ARRAY_LENGTH(mPropertiesSet); ++iHigh) {
        if (mPropertiesSet[iHigh] == 0)
            continue;
        for (PRInt32 iLow = 0; iLow < kPropertiesSetChunkSize; ++iLow) {
            if ((mPropertiesSet[iHigh] & (1 << iLow)) == 0)
                continue;
            nsCSSProperty iProp =
                nsCSSProperty(iHigh * kPropertiesSetChunkSize + iLow);
            NS_ASSERTION(0 <= iProp && iProp < eCSSProperty_COUNT_no_shorthands,
                         "out of range");
            void *prop = PropertyAt(iProp);
            PRBool important =
                (mPropertiesImportant[iHigh] & (1 << iLow)) != 0;
            char *&cursor = important ? cursor_important : cursor_normal;
            nsCSSCompressedDataBlock *result =
                important ? result_important : result_normal;
            switch (nsCSSProps::kTypeTable[iProp]) {
                case eCSSType_Value: {
                    nsCSSValue* val = static_cast<nsCSSValue*>(prop);
                    NS_ASSERTION(val->GetUnit() != eCSSUnit_Null,
                                 "Null value while compressing");
                    CDBValueStorage *storage =
                        reinterpret_cast<CDBValueStorage*>(cursor);
                    storage->property = iProp;
                    memcpy(&storage->value, val, sizeof(nsCSSValue));
                    new (val) nsCSSValue();
                    cursor += CDBValueStorage_advance;
                } break;

                case eCSSType_Rect: {
                    nsCSSRect* val = static_cast<nsCSSRect*>(prop);
                    NS_ASSERTION(val->HasValue(),
                                 "Valueless rect while compressing");
                    CDBRectStorage *storage =
                        reinterpret_cast<CDBRectStorage*>(cursor);
                    storage->property = iProp;
                    memcpy(&storage->value, val, sizeof(nsCSSRect));
                    new (val) nsCSSRect();
                    cursor += CDBRectStorage_advance;
                } break;

                case eCSSType_ValuePair: {
                    nsCSSValuePair* val = static_cast<nsCSSValuePair*>(prop);
                    NS_ASSERTION(val->mXValue.GetUnit() != eCSSUnit_Null ||
                                 val->mYValue.GetUnit() != eCSSUnit_Null,
                                 "Valueless pair while compressing");
                    CDBValuePairStorage *storage =
                        reinterpret_cast<CDBValuePairStorage*>(cursor);
                    storage->property = iProp;
                    memcpy(&storage->value, val, sizeof(nsCSSValuePair));
                    new (val) nsCSSValuePair();
                    cursor += CDBValuePairStorage_advance;
                } break;

                case eCSSType_ValueList:
                case eCSSType_CounterData:
                case eCSSType_Quotes: {
                    void*& val = *static_cast<void**>(prop);
                    NS_ASSERTION(val, "Null pointer while compressing");
                    CDBPointerStorage *storage =
                        reinterpret_cast<CDBPointerStorage*>(cursor);
                    storage->property = iProp;
                    storage->value = val;
                    val = nsnull;
                    cursor += CDBPointerStorage_advance;
                } break;
            }
            result->mStyleBits |=
                nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]);
        }
    }

    result_normal->mBlockEnd = cursor_normal;
    NS_ASSERTION(result_normal->DataSize() == ptrdiff_t(size.normal),
                 "size miscalculation");
    if (result_important) {
        result_important->mBlockEnd = cursor_important;
        NS_ASSERTION(result_important->DataSize() == ptrdiff_t(size.important),
                     "size miscalculation");
    }

    ClearSets();
    AssertInitialState();
    *aNormalBlock = result_normal;
    *aImportantBlock = result_important;
}

void
nsCSSExpandedDataBlock::Clear()
{
    for (PRUint32 iHigh = 0; iHigh < NS_ARRAY_LENGTH(mPropertiesSet); ++iHigh) {
        if (mPropertiesSet[iHigh] == 0)
            continue;
        for (PRInt32 iLow = 0; iLow < kPropertiesSetChunkSize; ++iLow) {
            if ((mPropertiesSet[iHigh] & (1 << iLow)) == 0)
                continue;
            nsCSSProperty iProp =
                nsCSSProperty(iHigh * kPropertiesSetChunkSize + iLow);
            ClearProperty(iProp);
        }
    }

    AssertInitialState();
}

void
nsCSSExpandedDataBlock::ClearProperty(nsCSSProperty aPropID)
{
    NS_ASSERTION(0 <= aPropID && aPropID < eCSSProperty_COUNT_no_shorthands,
                 "out of range");

    ClearPropertyBit(aPropID);
    ClearImportantBit(aPropID);

    void *prop = PropertyAt(aPropID);
    switch (nsCSSProps::kTypeTable[aPropID]) {
        case eCSSType_Value: {
            nsCSSValue* val = static_cast<nsCSSValue*>(prop);
            val->Reset();
        } break;

        case eCSSType_Rect: {
            nsCSSRect* val = static_cast<nsCSSRect*>(prop);
            val->Reset();
        } break;

        case eCSSType_ValuePair: {
            nsCSSValuePair* val = static_cast<nsCSSValuePair*>(prop);
            val->mXValue.Reset();
            val->mYValue.Reset();
        } break;

        case eCSSType_ValueList: {
            nsCSSValueList*& val = *static_cast<nsCSSValueList**>(prop);
            if (val) {
                delete val;
                val = nsnull;
            }
        } break;

        case eCSSType_CounterData: {
            nsCSSCounterData*& val =
                *static_cast<nsCSSCounterData**>(prop);
            if (val) {
                delete val;
                val = nsnull;
            }
        } break;

        case eCSSType_Quotes: {
            nsCSSQuotes*& val = *static_cast<nsCSSQuotes**>(prop);
            if (val) {
                delete val;
                val = nsnull;
            }
        } break;
    }
}

#ifdef DEBUG
void
nsCSSExpandedDataBlock::DoAssertInitialState()
{
    PRUint32 i;
    for (i = 0; i < NS_ARRAY_LENGTH(mPropertiesSet); ++i) {
        NS_ASSERTION(mPropertiesSet[i] == 0, "not initial state");
    }
    for (i = 0; i < NS_ARRAY_LENGTH(mPropertiesImportant); ++i) {
        NS_ASSERTION(mPropertiesImportant[i] == 0, "not initial state");
    }

    for (i = 0; i < eCSSProperty_COUNT_no_shorthands; ++i) {
        void *prop = PropertyAt(nsCSSProperty(i));
        switch (nsCSSProps::kTypeTable[i]) {
            case eCSSType_Value: {
                nsCSSValue* val = static_cast<nsCSSValue*>(prop);
                NS_ASSERTION(val->GetUnit() == eCSSUnit_Null,
                             "not initial state");
            } break;

            case eCSSType_Rect: {
                nsCSSRect* val = static_cast<nsCSSRect*>(prop);
                NS_ASSERTION(val->mTop.GetUnit() == eCSSUnit_Null,
                             "not initial state");
                NS_ASSERTION(val->mRight.GetUnit() == eCSSUnit_Null,
                             "not initial state");
                NS_ASSERTION(val->mBottom.GetUnit() == eCSSUnit_Null,
                             "not initial state");
                NS_ASSERTION(val->mLeft.GetUnit() == eCSSUnit_Null,
                             "not initial state");
            } break;

            case eCSSType_ValuePair: {
                nsCSSValuePair* val = static_cast<nsCSSValuePair*>(prop);
                NS_ASSERTION(val->mXValue.GetUnit() == eCSSUnit_Null,
                             "not initial state");
                NS_ASSERTION(val->mYValue.GetUnit() == eCSSUnit_Null,
                             "not initial state");
            } break;

            case eCSSType_ValueList: {
                nsCSSValueList* val = *static_cast<nsCSSValueList**>(prop);
                NS_ASSERTION(val == nsnull, "not initial state");
            } break;

            case eCSSType_CounterData: {
                nsCSSCounterData* val =
                    *static_cast<nsCSSCounterData**>(prop);
                NS_ASSERTION(val == nsnull, "not initial state");
            } break;

            case eCSSType_Quotes: {
                nsCSSQuotes* val = *static_cast<nsCSSQuotes**>(prop);
                NS_ASSERTION(val == nsnull, "not initial state");
            } break;
        }
    }
}
#endif
