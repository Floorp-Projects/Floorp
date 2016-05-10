/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * compact representation of the property-value pairs within a CSS
 * declaration, and the code for expanding and compacting it
 */

#include "nsCSSDataBlock.h"

#include "CSSVariableImageTable.h"
#include "mozilla/css/Declaration.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/WritingModes.h"
#include "nsIDocument.h"
#include "nsRuleData.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"

using namespace mozilla;
using namespace mozilla::css;

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
  return aRuleData->mLevel != SheetType::Agent &&
         aRuleData->mLevel != SheetType::User &&
         !aRuleData->mPresContext->UseDocumentColors();
}

/**
 * Tries to call |nsCSSValue::StartImageLoad()| on an image source.
 * Image sources are specified by |url()| or |-moz-image-rect()| function.
 */
static void
TryToStartImageLoadOnValue(const nsCSSValue& aValue, nsIDocument* aDocument,
                           nsStyleContext* aContext, nsCSSProperty aProperty,
                           bool aForTokenStream)
{
  MOZ_ASSERT(aDocument);

  if (aValue.GetUnit() == eCSSUnit_URL) {
    aValue.StartImageLoad(aDocument);
    if (aForTokenStream && aContext) {
      CSSVariableImageTable::Add(aContext, aProperty,
                                 aValue.GetImageStructValue());
    }
  }
  else if (aValue.GetUnit() == eCSSUnit_Image) {
    // If we already have a request, see if this document needs to clone it.
    imgIRequest* request = aValue.GetImageValue(nullptr);

    if (request) {
      ImageValue* imageValue = aValue.GetImageStructValue();
      aDocument->StyleImageLoader()->MaybeRegisterCSSImage(imageValue);
      if (aForTokenStream && aContext) {
        CSSVariableImageTable::Add(aContext, aProperty, imageValue);
      }
    }
  }
  else if (aValue.EqualsFunction(eCSSKeyword__moz_image_rect)) {
    nsCSSValue::Array* arguments = aValue.GetArrayValue();
    MOZ_ASSERT(arguments->Count() == 6, "unexpected num of arguments");

    const nsCSSValue& image = arguments->Item(1);
    TryToStartImageLoadOnValue(image, aDocument, aContext, aProperty,
                               aForTokenStream);
  }
}

static void
TryToStartImageLoad(const nsCSSValue& aValue, nsIDocument* aDocument,
                    nsStyleContext* aContext, nsCSSProperty aProperty,
                    bool aForTokenStream)
{
  if (aValue.GetUnit() == eCSSUnit_List) {
    for (const nsCSSValueList* l = aValue.GetListValue(); l; l = l->mNext) {
      TryToStartImageLoad(l->mValue, aDocument, aContext, aProperty,
                          aForTokenStream);
    }
  } else if (nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0)) {
    if (aValue.GetUnit() == eCSSUnit_Array) {
      TryToStartImageLoadOnValue(aValue.GetArrayValue()->Item(0), aDocument,
                                 aContext, aProperty, aForTokenStream);
    }
  } else {
    TryToStartImageLoadOnValue(aValue, aDocument, aContext, aProperty,
                               aForTokenStream);
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

static void
MapSinglePropertyInto(nsCSSProperty aTargetProp,
                      const nsCSSValue* aSrcValue,
                      nsCSSValue* aTargetValue,
                      nsRuleData* aRuleData)
{
  MOZ_ASSERT(!nsCSSProps::PropHasFlags(aTargetProp, CSS_PROPERTY_LOGICAL),
             "Can't map into a logical property");
  MOZ_ASSERT(aSrcValue->GetUnit() != eCSSUnit_Null, "oops");

  // Although aTargetValue is the nsCSSValue we are going to write into,
  // we also look at its value before writing into it.  This is done
  // when aTargetValue is a token stream value, which is the case when we
  // have just re-parsed a property that had a variable reference (in
  // nsCSSParser::ParsePropertyWithVariableReferences).  TryToStartImageLoad
  // then records any resulting ImageValue objects in the
  // CSSVariableImageTable, to give them the appropriate lifetime.
  MOZ_ASSERT(aTargetValue->GetUnit() == eCSSUnit_TokenStream ||
             aTargetValue->GetUnit() == eCSSUnit_Null,
             "aTargetValue must only be a token stream (when re-parsing "
             "properties with variable references) or null");

  if (ShouldStartImageLoads(aRuleData, aTargetProp)) {
    nsIDocument* doc = aRuleData->mPresContext->Document();
    TryToStartImageLoad(*aSrcValue, doc, aRuleData->mStyleContext,
                        aTargetProp,
                        aTargetValue->GetUnit() == eCSSUnit_TokenStream);
  }
  *aTargetValue = *aSrcValue;
  if (nsCSSProps::PropHasFlags(aTargetProp,
        CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED) &&
      ShouldIgnoreColors(aRuleData))
  {
    if (aTargetProp == eCSSProperty_background_color) {
      // Force non-'transparent' background
      // colors to the user's default.
      if (aTargetValue->IsNonTransparentColor()) {
        aTargetValue->SetColorValue(aRuleData->mPresContext->
                                    DefaultBackgroundColor());
      }
    } else {
      // Ignore 'color', 'border-*-color', etc.
      *aTargetValue = nsCSSValue();
    }
  }
}

/**
 * If aProperty is a logical property, converts it to the equivalent physical
 * property based on writing mode information obtained from aRuleData's
 * style context.
 */
static inline void
EnsurePhysicalProperty(nsCSSProperty& aProperty, nsRuleData* aRuleData)
{
  bool isAxisProperty =
    nsCSSProps::PropHasFlags(aProperty, CSS_PROPERTY_LOGICAL_AXIS);
  bool isBlock =
    nsCSSProps::PropHasFlags(aProperty, CSS_PROPERTY_LOGICAL_BLOCK_AXIS);

  int index;

  if (isAxisProperty) {
    LogicalAxis logicalAxis = isBlock ? eLogicalAxisBlock : eLogicalAxisInline;
    uint8_t wm = aRuleData->mStyleContext->StyleVisibility()->mWritingMode;
    PhysicalAxis axis =
      WritingMode::PhysicalAxisForLogicalAxis(wm, logicalAxis);

    // We rely on physical axis constants values matching the order of the
    // physical properties in the logical group array.
    static_assert(eAxisVertical == 0 && eAxisHorizontal == 1,
                  "unexpected axis constant values");
    index = axis;
  } else {
    bool isEnd =
      nsCSSProps::PropHasFlags(aProperty, CSS_PROPERTY_LOGICAL_END_EDGE);

    LogicalEdge edge = isEnd ? eLogicalEdgeEnd : eLogicalEdgeStart;

    // We handle block axis logical properties separately to save a bit of
    // work that the WritingMode constructor does that is unnecessary
    // unless we have an inline axis property.
    mozilla::css::Side side;
    if (isBlock) {
      uint8_t wm = aRuleData->mStyleContext->StyleVisibility()->mWritingMode;
      side = WritingMode::PhysicalSideForBlockAxis(wm, edge);
    } else {
      WritingMode wm(aRuleData->mStyleContext);
      side = wm.PhysicalSideForInlineAxis(edge);
    }

    // We rely on the physical side constant values matching the order of
    // the physical properties in the logical group array.
    static_assert(NS_SIDE_TOP == 0 && NS_SIDE_RIGHT == 1 &&
                  NS_SIDE_BOTTOM == 2 && NS_SIDE_LEFT == 3,
                  "unexpected side constant values");
    index = side;
  }

  const nsCSSProperty* props = nsCSSProps::LogicalGroup(aProperty);
  size_t len = isAxisProperty ? 2 : 4;
#ifdef DEBUG
    for (size_t i = 0; i < len; i++) {
    MOZ_ASSERT(props[i] != eCSSProperty_UNKNOWN,
               "unexpected logical group length");
  }
  MOZ_ASSERT(props[len] == eCSSProperty_UNKNOWN,
             "unexpected logical group length");
#endif

  for (size_t i = 0; i < len; i++) {
    if (aRuleData->ValueFor(props[i])->GetUnit() == eCSSUnit_Null) {
      // A declaration of one of the logical properties in this logical
      // group (but maybe not aProperty) would be the winning
      // declaration in the cascade.  This means that it's reasonably
      // likely that this logical property could be the winning
      // declaration in the cascade for some values of writing-mode,
      // direction, and text-orientation.  (It doesn't mean that for
      // sure, though.  For example, if this is a block-start logical
      // property, and all but the bottom physical property were set.
      // But the common case we want to hit here is logical declarations
      // that are completely overridden by a shorthand.)
      //
      // If this logical property could be the winning declaration in
      // the cascade for some values of writing-mode, direction, and
      // text-orientation, then we have to fault the resulting style
      // struct out of the rule tree.  We can't cache anything on the
      // rule tree if it depends on data from the style context, since
      // data cached in the rule tree could be used with a style context
      // with a different value of the depended-upon data.
      uint8_t wm = WritingMode(aRuleData->mStyleContext).GetBits();
      aRuleData->mConditions.SetWritingModeDependency(wm);
      break;
    }
  }

  aProperty = props[index];
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

  // We process these in reverse order so that we end up mapping the
  // right property when one can be expressed using both logical and
  // physical property names.
  for (uint32_t i = mNumProps; i-- > 0; ) {
    nsCSSProperty iProp = PropertyAtIndex(i);
    if (nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]) &
        aRuleData->mSIDs) {
      if (nsCSSProps::PropHasFlags(iProp, CSS_PROPERTY_LOGICAL)) {
        EnsurePhysicalProperty(iProp, aRuleData);
      }
      nsCSSValue* target = aRuleData->ValueFor(iProp);
      if (target->GetUnit() == eCSSUnit_Null) {
        const nsCSSValue *val = ValueAtIndex(i);
        // In order for variable resolution to have the right information
        // about the stylesheet level of a value, that level needs to be
        // stored on the token stream. We can't do that at creation time
        // because the CSS parser (which creates the object) has no idea
        // about the stylesheet level, so we do it here instead, where
        // the rule walking will have just updated aRuleData.
        if (val->GetUnit() == eCSSUnit_TokenStream) {
          val->GetTokenStreamValue()->mLevel = aRuleData->mLevel;
        }
        MapSinglePropertyInto(iProp, val, target, aRuleData);
      }
    }
  }
}

const nsCSSValue*
nsCSSCompressedDataBlock::ValueFor(nsCSSProperty aProperty) const
{
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
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
  MOZ_ASSERT(newValue && newValue->GetUnit() != eCSSUnit_Null,
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
    MOZ_ASSERT(val->GetUnit() != eCSSUnit_Null, "oops");
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
    MOZ_ASSERT(!nsCSSProps::IsShorthand(iProp), "out of range");
    MOZ_ASSERT(!HasPropertyBit(iProp),
               "compressed block has property multiple times");
    SetPropertyBit(iProp);
    if (aImportant)
      SetImportantBit(iProp);

    const nsCSSValue* val = aBlock->ValueAtIndex(i);
    nsCSSValue* dest = PropertyAt(iProp);
    MOZ_ASSERT(val->GetUnit() != eCSSUnit_Null, "oops");
    MOZ_ASSERT(dest->GetUnit() == eCSSUnit_Null,
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
  MOZ_ASSERT(aNormalBlock, "unexpected null block");
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
      MOZ_ASSERT(!nsCSSProps::IsShorthand(iProp), "out of range");
      MOZ_ASSERT(PropertyAt(iProp)->GetUnit() != eCSSUnit_Null,
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
                                 nsCSSCompressedDataBlock **aImportantBlock,
                                 const nsTArray<uint32_t>& aOrder)
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
  for (size_t i = 0; i < aOrder.Length(); i++) {
    nsCSSProperty iProp = static_cast<nsCSSProperty>(aOrder[i]);
    if (iProp >= eCSSProperty_COUNT) {
      // a custom property
      continue;
    }
    MOZ_ASSERT(mPropertiesSet.HasProperty(iProp),
               "aOrder identifies a property not in the expanded "
               "data block");
    MOZ_ASSERT(!nsCSSProps::IsShorthand(iProp), "out of range");
    bool important = mPropertiesImportant.HasProperty(iProp);
    nsCSSCompressedDataBlock *result =
      important ? result_important : result_normal;
    uint32_t* ip = important ? &i_important : &i_normal;
    nsCSSValue* val = PropertyAt(iProp);
    MOZ_ASSERT(val->GetUnit() != eCSSUnit_Null,
               "Null value while compressing");
    result->SetPropertyAtIndex(*ip, iProp);
    result->RawCopyValueToIndex(*ip, val);
    new (val) nsCSSValue();
    (*ip)++;
    result->mStyleBits |=
      nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[iProp]);
  }

  MOZ_ASSERT(numPropsNormal == i_normal, "bad numProps");

  if (result_important) {
    MOZ_ASSERT(numPropsImportant == i_important, "bad numProps");
  }

#ifdef DEBUG
  {
    // assert that we didn't have any other properties on this expanded data
    // block that we didn't find in aOrder
    uint32_t numPropsInSet = 0;
    for (size_t iHigh = 0; iHigh < nsCSSPropertySet::kChunkCount; iHigh++) {
      if (!mPropertiesSet.HasPropertyInChunk(iHigh)) {
        continue;
      }
      for (size_t iLow = 0; iLow < nsCSSPropertySet::kBitsInChunk; iLow++) {
        if (mPropertiesSet.HasPropertyAt(iHigh, iLow)) {
          numPropsInSet++;
        }
      }
    }
    MOZ_ASSERT(numPropsNormal + numPropsImportant == numPropsInSet,
               "aOrder missing properties from the expanded data block");
  }
#endif

  ClearSets();
  AssertInitialState();
  *aNormalBlock = result_normal.forget();
  *aImportantBlock = result_important.forget();
}

void
nsCSSExpandedDataBlock::AddLonghandProperty(nsCSSProperty aProperty,
                                            const nsCSSValue& aValue)
{
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
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
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID,
                                         nsCSSProps::eIgnoreEnabledState) {
      ClearLonghandProperty(*p);
    }
  } else {
    ClearLonghandProperty(aPropID);
  }
}

void
nsCSSExpandedDataBlock::ClearLonghandProperty(nsCSSProperty aPropID)
{
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aPropID), "out of range");

  ClearPropertyBit(aPropID);
  ClearImportantBit(aPropID);
  PropertyAt(aPropID)->Reset();
}

bool
nsCSSExpandedDataBlock::TransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                                          nsCSSProperty aPropID,
                                          nsCSSProps::EnabledState aEnabledState,
                                          bool aIsImportant,
                                          bool aOverrideImportant,
                                          bool aMustCallValueAppended,
                                          css::Declaration* aDeclaration,
                                          nsIDocument* aSheetDocument)
{
  if (!nsCSSProps::IsShorthand(aPropID)) {
    return DoTransferFromBlock(aFromBlock, aPropID,
                               aIsImportant, aOverrideImportant,
                               aMustCallValueAppended, aDeclaration,
                               aSheetDocument);
  }

  // We can pass eIgnoreEnabledState (here, and in ClearProperty above) rather
  // than a value corresponding to whether we're parsing a UA style sheet or
  // certified app because we assert in nsCSSProps::AddRefTable that shorthand
  // properties available in these contexts also have all of their
  // subproperties available in these contexts.
  bool changed = false;
  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID, aEnabledState) {
    changed |= DoTransferFromBlock(aFromBlock, *p,
                                   aIsImportant, aOverrideImportant,
                                   aMustCallValueAppended, aDeclaration,
                                   aSheetDocument);
  }
  return changed;
}

bool
nsCSSExpandedDataBlock::DoTransferFromBlock(nsCSSExpandedDataBlock& aFromBlock,
                                            nsCSSProperty aPropID,
                                            bool aIsImportant,
                                            bool aOverrideImportant,
                                            bool aMustCallValueAppended,
                                            css::Declaration* aDeclaration,
                                            nsIDocument* aSheetDocument)
{
  bool changed = false;
  MOZ_ASSERT(aFromBlock.HasPropertyBit(aPropID), "oops");
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

  if (aSheetDocument) {
    UseCounter useCounter = nsCSSProps::UseCounterFor(aPropID);
    if (useCounter != eUseCounter_UNKNOWN) {
      aSheetDocument->SetDocumentAndPageUseCounter(useCounter);
    }
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

void
nsCSSExpandedDataBlock::MapRuleInfoInto(nsCSSProperty aPropID,
                                        nsRuleData* aRuleData) const
{
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aPropID));

  const nsCSSValue* src = PropertyAt(aPropID);
  MOZ_ASSERT(src->GetUnit() != eCSSUnit_Null);

  nsCSSProperty physicalProp = aPropID;
  if (nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_LOGICAL)) {
    EnsurePhysicalProperty(physicalProp, aRuleData);
  }

  nsCSSValue* dest = aRuleData->ValueFor(physicalProp);
  MOZ_ASSERT(dest->GetUnit() == eCSSUnit_TokenStream &&
             dest->GetTokenStreamValue()->mPropertyID == aPropID);

  CSSVariableImageTable::ReplaceAll(aRuleData->mStyleContext, aPropID, [=] {
    MapSinglePropertyInto(physicalProp, src, dest, aRuleData);
  });
}

#ifdef DEBUG
void
nsCSSExpandedDataBlock::DoAssertInitialState()
{
  mPropertiesSet.AssertIsEmpty("not initial state");
  mPropertiesImportant.AssertIsEmpty("not initial state");

  for (uint32_t i = 0; i < eCSSProperty_COUNT_no_shorthands; ++i) {
    nsCSSProperty prop = nsCSSProperty(i);
    MOZ_ASSERT(PropertyAt(prop)->GetUnit() == eCSSUnit_Null,
               "not initial state");
  }
}
#endif
