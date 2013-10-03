/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * compact representation of the property-value pairs within a CSS
 * declaration, and the code for expanding and compacting it
 */

#include "nsCSSDataBlock.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/Declaration.h"
#include "mozilla/css/ImageLoader.h"
#include "nsRuleData.h"
#include "nsStyleSet.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"

namespace css = mozilla::css;

/**
 * Does a fast move of aSource to aDest.  The previous value in
 * aDest is cleanly destroyed, and aSource is cleared.  Returns
 * true if, before the copy, the value at aSource compared unequal
 * to the value at aDest; false otherwise.
 */
static bool
MoveValue(nsCSSValue* aSource, nsCSSValue* aDest)
{
    bool changed = (*aSource != *aDest);
    aDest->~nsCSSValue();
    memcpy(aDest, aSource, sizeof(nsCSSValue));
    new (aSource) nsCSSValue();
    return changed;
}

static bool
ShouldIgnoreColors(nsRuleData *aRuleData)
{
    return aRuleData->mLevel != nsStyleSet::eAgentSheet &&
           aRuleData->mLevel != nsStyleSet::eUserSheet &&
           !aRuleData->mPresContext->UseDocumentColors();
}

/**
 * Tries to call |nsCSSValue::StartImageLoad()| on an image source.
 * Image sources are specified by |url()| or |-moz-image-rect()| function.
 */
static void
TryToStartImageLoadOnValue(const nsCSSValue& aValue, nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  if (aValue.GetUnit() == eCSSUnit_URL) {
    aValue.StartImageLoad(aDocument);
  }
  else if (aValue.GetUnit() == eCSSUnit_Image) {
    // If we already have a request, see if this document needs to clone it.
    imgIRequest* request = aValue.GetImageValue(nullptr);

    if (request) {
      aDocument->StyleImageLoader()->MaybeRegisterCSSImage(aValue.GetImageStructValue());
    }
  }
  else if (aValue.EqualsFunction(eCSSKeyword__moz_image_rect)) {
    nsCSSValue::Array* arguments = aValue.GetArrayValue();
    NS_ABORT_IF_FALSE(arguments->Count() == 6, "unexpected num of arguments");

    const nsCSSValue& image = arguments->Item(1);
    TryToStartImageLoadOnValue(image, aDocument);
  }
}

static void
TryToStartImageLoad(const nsCSSValue& aValue, nsIDocument* aDocument,
                    nsCSSProperty aProperty)
{
  if (aValue.GetUnit() == eCSSUnit_List) {
    for (const nsCSSValueList* l = aValue.GetListValue(); l; l = l->mNext) {
      TryToStartImageLoad(l->mValue, aDocument, aProperty);
    }
  } else if (nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0)) {
    if (aValue.GetUnit() == eCSSUnit_Array) {
      TryToStartImageLoadOnValue(aValue.GetArrayValue()->Item(0), aDocument);
    }
  } else {
    TryToStartImageLoadOnValue(aValue, aDocument);
  }
}

static inline bool
ShouldStartImageLoads(nsRuleData *aRuleData, nsCSSProperty aProperty)
{
  // Don't initiate image loads for if-visited styles.  This is
  // important because:
  //  (1) it's a waste of CPU and bandwidth
  //  (2) in some cases we'd start the image load on a style change
  //      where we wouldn't have started the load initially, which makes
  //      which links are visited detectable to Web pages (see bug
  //      557287)
  return !aRuleData->mStyleContext->IsStyleIfVisited() &&
         nsCSSProps::PropHasFlags(aProperty, CSS_PROPERTY_START_IMAGE_LOADS);
}

void
nsCSSCompressedDataBlock::MapRuleInfoInto(nsRuleData *aRuleData) const
{
    // If we have no data for these structs, then return immediately.
    // This optimization should make us return most of the time, so we
    // have to worry much less (although still some) about the speed of
    // the rest of the function.
    if (!(aRuleData->mSIDs & mStyleBits))
        return;

    nsIDocument* doc = aRuleData->mPresContext->Document();

    for (uint32_t i = 0; i < mNumProps; i++) {
        nsCSSProperty iProp = PropertyAtIndex(i);
        if (nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]) &
            aRuleData->mSIDs) {
            nsCSSValue* target = aRuleData->ValueFor(iProp);
            if (target->GetUnit() == eCSSUnit_Null) {
                const nsCSSValue *val = ValueAtIndex(i);
                NS_ABORT_IF_FALSE(val->GetUnit() != eCSSUnit_Null, "oops");
                if (ShouldStartImageLoads(aRuleData, iProp)) {
                    TryToStartImageLoad(*val, doc, iProp);
                }
                *target = *val;
                if (nsCSSProps::PropHasFlags(iProp,
                        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED) &&
                    ShouldIgnoreColors(aRuleData))
                {
                    if (iProp == eCSSProperty_background_color) {
                        // Force non-'transparent' background
                        // colors to the user's default.
                        if (target->IsNonTransparentColor()) {
                            target->SetColorValue(aRuleData->mPresContext->
                                                  DefaultBackgroundColor());
                        }
                    } else {
                        // Ignore 'color', 'border-*-color', etc.
                        *target = nsCSSValue();
                    }
                }
            }
        }
    }
}

const nsCSSValue*
nsCSSCompressedDataBlock::ValueFor(nsCSSProperty aProperty) const
{
    NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                      "Don't call for shorthands");

    // If we have no data for this struct, then return immediately.
    // This optimization should make us return most of the time, so we
    // have to worry much less (although still some) about the speed of
    // the rest of the function.
    if (!(nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]) &
          mStyleBits))
        return nullptr;

    for (uint32_t i = 0; i < mNumProps; i++) {
        if (PropertyAtIndex(i) == aProperty) {
            return ValueAtIndex(i);
        }
    }

    return nullptr;
}

bool
nsCSSCompressedDataBlock::TryReplaceValue(nsCSSProperty aProperty,
                                          nsCSSExpandedDataBlock& aFromBlock,
                                          bool *aChanged)
{
    nsCSSValue* newValue = aFromBlock.PropertyAt(aProperty);
    NS_ABORT_IF_FALSE(newValue && newValue->GetUnit() != eCSSUnit_Null,
                      "cannot replace with empty value");

    const nsCSSValue* oldValue = ValueFor(aProperty);
    if (!oldValue) {
        *aChanged = false;
        return false;
    }

    *aChanged = MoveValue(newValue, const_cast<nsCSSValue*>(oldValue));
    aFromBlock.ClearPropertyBit(aProperty);
    return true;
}

nsCSSCompressedDataBlock*
nsCSSCompressedDataBlock::Clone() const
{
    nsAutoPtr<nsCSSCompressedDataBlock>
        result(new(mNumProps) nsCSSCompressedDataBlock(mNumProps));

    result->mStyleBits = mStyleBits;

    for (uint32_t i = 0; i < mNumProps; i++) {
        result->SetPropertyAtIndex(i, PropertyAtIndex(i));
        result->CopyValueToIndex(i, ValueAtIndex(i));
    }

    return result.forget();
}

nsCSSCompressedDataBlock::~nsCSSCompressedDataBlock()
{
    for (uint32_t i = 0; i < mNumProps; i++) {
#ifdef DEBUG
        (void)PropertyAtIndex(i);   // this checks the property is in range
#endif
        const nsCSSValue* val = ValueAtIndex(i);
        NS_ABORT_IF_FALSE(val->GetUnit() != eCSSUnit_Null, "oops");
        val->~nsCSSValue();
    }
}

/* static */ nsCSSCompressedDataBlock*
nsCSSCompressedDataBlock::CreateEmptyBlock()
{
    nsCSSCompressedDataBlock *result = new(0) nsCSSCompressedDataBlock(0);
    return result;
}

size_t
nsCSSCompressedDataBlock::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t n = aMallocSizeOf(this);
    for (uint32_t i = 0; i < mNumProps; i++) {
        n += ValueAtIndex(i)->SizeOfExcludingThis(aMallocSizeOf);
    }
    return n;
}

bool
nsCSSCompressedDataBlock::HasDefaultBorderImageSlice() const
{
  const nsCSSValueList *slice =
    ValueFor(eCSSProperty_border_image_slice)->GetListValue();
  return !slice->mNext &&
         slice->mValue.GetRectValue().AllSidesEqualTo(
           nsCSSValue(1.0f, eCSSUnit_Percent));
}

bool
nsCSSCompressedDataBlock::HasDefaultBorderImageWidth() const
{
  const nsCSSRect &width =
    ValueFor(eCSSProperty_border_image_width)->GetRectValue();
  return width.AllSidesEqualTo(nsCSSValue(1.0f, eCSSUnit_Number));
}

bool
nsCSSCompressedDataBlock::HasDefaultBorderImageOutset() const
{
  const nsCSSRect &outset =
    ValueFor(eCSSProperty_border_image_outset)->GetRectValue();
  return outset.AllSidesEqualTo(nsCSSValue(0.0f, eCSSUnit_Number));
}

bool
nsCSSCompressedDataBlock::HasDefaultBorderImageRepeat() const
{
  const nsCSSValuePair &repeat =
    ValueFor(eCSSProperty_border_image_repeat)->GetPairValue();
  return repeat.BothValuesEqualTo(
    nsCSSValue(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH, eCSSUnit_Enumerated));
}

/*****************************************************************************/

nsCSSExpandedDataBlock::nsCSSExpandedDataBlock()
{
    AssertInitialState();
}

nsCSSExpandedDataBlock::~nsCSSExpandedDataBlock()
{
    AssertInitialState();
}

void
nsCSSExpandedDataBlock::DoExpand(nsCSSCompressedDataBlock *aBlock,
                                 bool aImportant)
{
    /*
     * Save needless copying and allocation by copying the memory
     * corresponding to the stored data in the compressed block.
     */
    for (uint32_t i = 0; i < aBlock->mNumProps; i++) {
        nsCSSProperty iProp = aBlock->PropertyAtIndex(i);
        NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(iProp), "out of range");
        NS_ABORT_IF_FALSE(!HasPropertyBit(iProp),
                          "compressed block has property multiple times");
        SetPropertyBit(iProp);
        if (aImportant)
            SetImportantBit(iProp);

        const nsCSSValue* val = aBlock->ValueAtIndex(i);
        nsCSSValue* dest = PropertyAt(iProp);
        NS_ABORT_IF_FALSE(val->GetUnit() != eCSSUnit_Null, "oops");
        NS_ABORT_IF_FALSE(dest->GetUnit() == eCSSUnit_Null,
                          "expanding into non-empty block");
#ifdef NS_BUILD_REFCNT_LOGGING
        dest->~nsCSSValue();
#endif
        memcpy(dest, val, sizeof(nsCSSValue));
    }

    // Set the number of properties to zero so that we don't destroy the
    // remnants of what we just copied.
    aBlock->SetNumPropsToZero();
    delete aBlock;
}

void
nsCSSExpandedDataBlock::Expand(nsCSSCompressedDataBlock *aNormalBlock,
                               nsCSSCompressedDataBlock *aImportantBlock)
{
    NS_ABORT_IF_FALSE(aNormalBlock, "unexpected null block");
    AssertInitialState();

    DoExpand(aNormalBlock, false);
    if (aImportantBlock) {
        DoExpand(aImportantBlock, true);
    }
}

void
nsCSSExpandedDataBlock::ComputeNumProps(uint32_t* aNumPropsNormal,
                                        uint32_t* aNumPropsImportant)
{
    *aNumPropsNormal = *aNumPropsImportant = 0;
    for (size_t iHigh = 0; iHigh < nsCSSPropertySet::kChunkCount; ++iHigh) {
        if (!mPropertiesSet.HasPropertyInChunk(iHigh))
            continue;
        for (size_t iLow = 0; iLow < nsCSSPropertySet::kBitsInChunk; ++iLow) {
            if (!mPropertiesSet.HasPropertyAt(iHigh, iLow))
                continue;
#ifdef DEBUG
            nsCSSProperty iProp = nsCSSPropertySet::CSSPropertyAt(iHigh, iLow);
#endif
            NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(iProp), "out of range");
            NS_ABORT_IF_FALSE(PropertyAt(iProp)->GetUnit() != eCSSUnit_Null,
                              "null value while computing size");
            if (mPropertiesImportant.HasPropertyAt(iHigh, iLow))
                (*aNumPropsImportant)++;
            else
                (*aNumPropsNormal)++;
        }
    }
}

void
nsCSSExpandedDataBlock::Compress(nsCSSCompressedDataBlock **aNormalBlock,
                                 nsCSSCompressedDataBlock **aImportantBlock)
{
    nsAutoPtr<nsCSSCompressedDataBlock> result_normal, result_important;
    uint32_t i_normal = 0, i_important = 0;

    uint32_t numPropsNormal, numPropsImportant;
    ComputeNumProps(&numPropsNormal, &numPropsImportant);

    result_normal =
        new(numPropsNormal) nsCSSCompressedDataBlock(numPropsNormal);

    if (numPropsImportant != 0) {
        result_important =
            new(numPropsImportant) nsCSSCompressedDataBlock(numPropsImportant);
    } else {
        result_important = nullptr;
    }

    /*
     * Save needless copying and allocation by copying the memory
     * corresponding to the stored data in the expanded block, and then
     * clearing the data in the expanded block.
     */
    for (size_t iHigh = 0; iHigh < nsCSSPropertySet::kChunkCount; ++iHigh) {
        if (!mPropertiesSet.HasPropertyInChunk(iHigh))
            continue;
        for (size_t iLow = 0; iLow < nsCSSPropertySet::kBitsInChunk; ++iLow) {
            if (!mPropertiesSet.HasPropertyAt(iHigh, iLow))
                continue;
            nsCSSProperty iProp = nsCSSPropertySet::CSSPropertyAt(iHigh, iLow);
            NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(iProp), "out of range");
            bool important =
                mPropertiesImportant.HasPropertyAt(iHigh, iLow);
            nsCSSCompressedDataBlock *result =
                important ? result_important : result_normal;
            uint32_t* ip = important ? &i_important : &i_normal;
            nsCSSValue* val = PropertyAt(iProp);
            NS_ABORT_IF_FALSE(val->GetUnit() != eCSSUnit_Null,
                              "Null value while compressing");
            result->SetPropertyAtIndex(*ip, iProp);
            result->RawCopyValueToIndex(*ip, val);
            new (val) nsCSSValue();
            (*ip)++;
            result->mStyleBits |=
                nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]);
        }
    }

    NS_ABORT_IF_FALSE(numPropsNormal == i_normal, "bad numProps");

    if (result_important) {
        NS_ABORT_IF_FALSE(numPropsImportant == i_important, "bad numProps");
    }

    ClearSets();
    AssertInitialState();
    *aNormalBlock = result_normal.forget();
    *aImportantBlock = result_important.forget();
}

void
nsCSSExpandedDataBlock::AddLonghandProperty(nsCSSProperty aProperty,
                                            const nsCSSValue& aValue)
{
    NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                      "property out of range");
    nsCSSValue& storage = *static_cast<nsCSSValue*>(PropertyAt(aProperty));
    storage = aValue;
    SetPropertyBit(aProperty);
}

void
nsCSSExpandedDataBlock::Clear()
{
    for (size_t iHigh = 0; iHigh < nsCSSPropertySet::kChunkCount; ++iHigh) {
        if (!mPropertiesSet.HasPropertyInChunk(iHigh))
            continue;
        for (size_t iLow = 0; iLow < nsCSSPropertySet::kBitsInChunk; ++iLow) {
            if (!mPropertiesSet.HasPropertyAt(iHigh, iLow))
                continue;
            nsCSSProperty iProp = nsCSSPropertySet::CSSPropertyAt(iHigh, iLow);
            ClearLonghandProperty(iProp);
        }
    }

    AssertInitialState();
}

void
nsCSSExpandedDataBlock::ClearProperty(nsCSSProperty aPropID)
{
  if (nsCSSProps::IsShorthand(aPropID)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID) {
      ClearLonghandProperty(*p);
    }
  } else {
    ClearLonghandProperty(aPropID);
  }
}

void
nsCSSExpandedDataBlock::ClearLonghandProperty(nsCSSProperty aPropID)
{
    NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aPropID), "out of range");

    ClearPropertyBit(aPropID);
    ClearImportantBit(aPropID);
    PropertyAt(aPropID)->Reset();
}

bool
nsCSSExpandedDataBlock::TransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                                          nsCSSProperty aPropID,
                                          bool aIsImportant,
                                          bool aOverrideImportant,
                                          bool aMustCallValueAppended,
                                          css::Declaration* aDeclaration)
{
    if (!nsCSSProps::IsShorthand(aPropID)) {
        return DoTransferFromBlock(aFromBlock, aPropID,
                                   aIsImportant, aOverrideImportant,
                                   aMustCallValueAppended, aDeclaration);
    }

    bool changed = false;
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID) {
        changed |= DoTransferFromBlock(aFromBlock, *p,
                                       aIsImportant, aOverrideImportant,
                                       aMustCallValueAppended, aDeclaration);
    }
    return changed;
}

bool
nsCSSExpandedDataBlock::DoTransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                                            nsCSSProperty aPropID,
                                            bool aIsImportant,
                                            bool aOverrideImportant,
                                            bool aMustCallValueAppended,
                                            css::Declaration* aDeclaration)
{
  bool changed = false;
  NS_ABORT_IF_FALSE(aFromBlock.HasPropertyBit(aPropID), "oops");
  if (aIsImportant) {
    if (!HasImportantBit(aPropID))
      changed = true;
    SetImportantBit(aPropID);
  } else {
    if (HasImportantBit(aPropID)) {
      // When parsing a declaration block, an !important declaration
      // is not overwritten by an ordinary declaration of the same
      // property later in the block.  However, CSSOM manipulations
      // come through here too, and in that case we do want to
      // overwrite the property.
      if (!aOverrideImportant) {
        aFromBlock.ClearLonghandProperty(aPropID);
        return false;
      }
      changed = true;
      ClearImportantBit(aPropID);
    }
  }

  if (aMustCallValueAppended || !HasPropertyBit(aPropID)) {
    aDeclaration->ValueAppended(aPropID);
  }

  SetPropertyBit(aPropID);
  aFromBlock.ClearPropertyBit(aPropID);

  /*
   * Save needless copying and allocation by calling the destructor in
   * the destination, copying memory directly, and then using placement
   * new.
   */
  changed |= MoveValue(aFromBlock.PropertyAt(aPropID), PropertyAt(aPropID));
  return changed;
}

#ifdef DEBUG
void
nsCSSExpandedDataBlock::DoAssertInitialState()
{
    mPropertiesSet.AssertIsEmpty("not initial state");
    mPropertiesImportant.AssertIsEmpty("not initial state");

    for (uint32_t i = 0; i < eCSSProperty_COUNT_no_shorthands; ++i) {
        nsCSSProperty prop = nsCSSProperty(i);
        NS_ABORT_IF_FALSE(PropertyAt(prop)->GetUnit() == eCSSUnit_Null,
                          "not initial state");
    }
}
#endif
