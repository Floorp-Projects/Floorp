/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of a declaration block (or style attribute) in a CSS
 * stylesheet
 */

#include "mozilla/MemoryReporting.h"
#include "mozilla/Util.h"

#include "mozilla/css/Declaration.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace css {

// check that we can fit all the CSS properties into a uint16_t
// for the mOrder array
MOZ_STATIC_ASSERT(eCSSProperty_COUNT_no_shorthands - 1 <= UINT16_MAX,
                  "CSS longhand property numbers no longer fit in a uint16_t");

Declaration::Declaration()
  : mImmutable(false)
{
  MOZ_COUNT_CTOR(mozilla::css::Declaration);
}

Declaration::Declaration(const Declaration& aCopy)
  : mOrder(aCopy.mOrder),
    mData(aCopy.mData ? aCopy.mData->Clone() : nullptr),
    mImportantData(aCopy.mImportantData
                   ? aCopy.mImportantData->Clone() : nullptr),
    mImmutable(false)
{
  MOZ_COUNT_CTOR(mozilla::css::Declaration);
}

Declaration::~Declaration()
{
  MOZ_COUNT_DTOR(mozilla::css::Declaration);
}

void
Declaration::ValueAppended(nsCSSProperty aProperty)
{
  NS_ABORT_IF_FALSE(!mData && !mImportantData,
                    "should only be called while expanded");
  NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                    "shorthands forbidden");
  // order IS important for CSS, so remove and add to the end
  mOrder.RemoveElement(aProperty);
  mOrder.AppendElement(aProperty);
}

void
Declaration::RemoveProperty(nsCSSProperty aProperty)
{
  nsCSSExpandedDataBlock data;
  ExpandTo(&data);
  NS_ABORT_IF_FALSE(!mData && !mImportantData, "Expand didn't null things out");

  if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
      data.ClearLonghandProperty(*p);
      mOrder.RemoveElement(*p);
    }
  } else {
    data.ClearLonghandProperty(aProperty);
    mOrder.RemoveElement(aProperty);
  }

  CompressFrom(&data);
}

bool
Declaration::HasProperty(nsCSSProperty aProperty) const
{
  NS_ABORT_IF_FALSE(0 <= aProperty &&
                    aProperty < eCSSProperty_COUNT_no_shorthands,
                    "property ID out of range");

  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                      ? mImportantData : mData;
  const nsCSSValue *val = data->ValueFor(aProperty);
  return !!val;
}

bool
Declaration::AppendValueToString(nsCSSProperty aProperty,
                                 nsAString& aResult) const
{
  NS_ABORT_IF_FALSE(0 <= aProperty &&
                    aProperty < eCSSProperty_COUNT_no_shorthands,
                    "property ID out of range");

  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                      ? mImportantData : mData;
  const nsCSSValue *val = data->ValueFor(aProperty);
  if (!val) {
    return false;
  }

  val->AppendToString(aProperty, aResult);
  return true;
}

// Helper to append |aString| with the shorthand sides notation used in e.g.
// 'padding'. |aProperties| and |aValues| are expected to have 4 elements.
static void
AppendSidesShorthandToString(const nsCSSProperty aProperties[],
                             const nsCSSValue* aValues[],
                             nsAString& aString)
{
  const nsCSSValue& value1 = *aValues[0];
  const nsCSSValue& value2 = *aValues[1];
  const nsCSSValue& value3 = *aValues[2];
  const nsCSSValue& value4 = *aValues[3];

  NS_ABORT_IF_FALSE(value1.GetUnit() != eCSSUnit_Null, "null value 1");
  value1.AppendToString(aProperties[0], aString);
  if (value1 != value2 || value1 != value3 || value1 != value4) {
    aString.Append(PRUnichar(' '));
    NS_ABORT_IF_FALSE(value2.GetUnit() != eCSSUnit_Null, "null value 2");
    value2.AppendToString(aProperties[1], aString);
    if (value1 != value3 || value2 != value4) {
      aString.Append(PRUnichar(' '));
      NS_ABORT_IF_FALSE(value3.GetUnit() != eCSSUnit_Null, "null value 3");
      value3.AppendToString(aProperties[2], aString);
      if (value2 != value4) {
        aString.Append(PRUnichar(' '));
        NS_ABORT_IF_FALSE(value4.GetUnit() != eCSSUnit_Null, "null value 4");
        value4.AppendToString(aProperties[3], aString);
      }
    }
  }
}

void
Declaration::GetValue(nsCSSProperty aProperty, nsAString& aValue) const
{
  aValue.Truncate(0);

  // simple properties are easy.
  if (!nsCSSProps::IsShorthand(aProperty)) {
    AppendValueToString(aProperty, aValue);
    return;
  }

  // DOM Level 2 Style says (when describing CSS2Properties, although
  // not CSSStyleDeclaration.getPropertyValue):
  //   However, if there is no shorthand declaration that could be added
  //   to the ruleset without changing in any way the rules already
  //   declared in the ruleset (i.e., by adding longhand rules that were
  //   previously not declared in the ruleset), then the empty string
  //   should be returned for the shorthand property.
  // This means we need to check a number of cases:
  //   (1) Since a shorthand sets all sub-properties, if some of its
  //       subproperties were not specified, we must return the empty
  //       string.
  //   (2) Since 'inherit' and 'initial' can only be specified as the
  //       values for entire properties, we need to return the empty
  //       string if some but not all of the subproperties have one of
  //       those values.
  //   (3) Since a single value only makes sense with or without
  //       !important, we return the empty string if some values are
  //       !important and some are not.
  // Since we're doing this check for 'inherit' and 'initial' up front,
  // we can also simplify the property serialization code by serializing
  // those values up front as well.
  uint32_t totalCount = 0, importantCount = 0,
           initialCount = 0, inheritCount = 0;
  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
    if (*p == eCSSProperty__x_system_font ||
         nsCSSProps::PropHasFlags(*p, CSS_PROPERTY_DIRECTIONAL_SOURCE)) {
      // The system-font subproperty and the *-source properties don't count.
      continue;
    }
    if (!nsCSSProps::IsEnabled(*p)) {
      continue;
    }
    ++totalCount;
    const nsCSSValue *val = mData->ValueFor(*p);
    NS_ABORT_IF_FALSE(!val || !mImportantData || !mImportantData->ValueFor(*p),
                      "can't be in both blocks");
    if (!val && mImportantData) {
      ++importantCount;
      val = mImportantData->ValueFor(*p);
    }
    if (!val) {
      // Case (1) above: some subproperties not specified.
      return;
    }
    if (val->GetUnit() == eCSSUnit_Inherit) {
      ++inheritCount;
    } else if (val->GetUnit() == eCSSUnit_Initial) {
      ++initialCount;
    }
  }
  if (importantCount != 0 && importantCount != totalCount) {
    // Case (3), no consistent importance.
    return;
  }
  if (initialCount == totalCount) {
    // Simplify serialization below by serializing initial up-front.
    nsCSSValue(eCSSUnit_Initial).AppendToString(eCSSProperty_UNKNOWN, aValue);
    return;
  }
  if (inheritCount == totalCount) {
    // Simplify serialization below by serializing inherit up-front.
    nsCSSValue(eCSSUnit_Inherit).AppendToString(eCSSProperty_UNKNOWN, aValue);
    return;
  }
  if (initialCount != 0 || inheritCount != 0) {
    // Case (2): partially initial or inherit.
    return;
  }

  nsCSSCompressedDataBlock *data = importantCount ? mImportantData : mData;
  switch (aProperty) {
    case eCSSProperty_margin: 
    case eCSSProperty_padding: 
    case eCSSProperty_border_color: 
    case eCSSProperty_border_style: 
    case eCSSProperty_border_width: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ABORT_IF_FALSE(nsCSSProps::GetStringValue(subprops[0]).Find("-top") !=
                        kNotFound, "first subprop must be top");
      NS_ABORT_IF_FALSE(nsCSSProps::GetStringValue(subprops[1]).Find("-right") !=
                        kNotFound, "second subprop must be right");
      NS_ABORT_IF_FALSE(nsCSSProps::GetStringValue(subprops[2]).Find("-bottom") !=
                        kNotFound, "third subprop must be bottom");
      NS_ABORT_IF_FALSE(nsCSSProps::GetStringValue(subprops[3]).Find("-left") !=
                        kNotFound, "fourth subprop must be left");
      const nsCSSValue* vals[4] = {
        data->ValueFor(subprops[0]),
        data->ValueFor(subprops[1]),
        data->ValueFor(subprops[2]),
        data->ValueFor(subprops[3])
      };
      AppendSidesShorthandToString(subprops, vals, aValue);
      break;
    }
    case eCSSProperty_border_radius:
    case eCSSProperty__moz_outline_radius: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      const nsCSSValue* vals[4] = {
        data->ValueFor(subprops[0]),
        data->ValueFor(subprops[1]),
        data->ValueFor(subprops[2]),
        data->ValueFor(subprops[3])
      };

      // For compatibility, only write a slash and the y-values
      // if they're not identical to the x-values.
      bool needY = false;
      const nsCSSValue* xVals[4];
      const nsCSSValue* yVals[4];
      for (int i = 0; i < 4; i++) {
        if (vals[i]->GetUnit() == eCSSUnit_Pair) {
          needY = true;
          xVals[i] = &vals[i]->GetPairValue().mXValue;
          yVals[i] = &vals[i]->GetPairValue().mYValue;
        } else {
          xVals[i] = yVals[i] = vals[i];
        }
      }

      AppendSidesShorthandToString(subprops, xVals, aValue);
      if (needY) {
        aValue.AppendLiteral(" / ");
        AppendSidesShorthandToString(subprops, yVals, aValue);
      }
      break;
    }
    case eCSSProperty_border_image: {
      // Even though there are some cases where we could omit
      // 'border-image-source' (when it's none), it's probably not a
      // good idea since it's likely to be confusing.  It would also
      // require adding the extra check that we serialize *something*.
      AppendValueToString(eCSSProperty_border_image_source, aValue);

      bool sliceDefault = data->HasDefaultBorderImageSlice();
      bool widthDefault = data->HasDefaultBorderImageWidth();
      bool outsetDefault = data->HasDefaultBorderImageOutset();

      if (!sliceDefault || !widthDefault || !outsetDefault) {
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_image_slice, aValue);
        if (!widthDefault || !outsetDefault) {
          aValue.Append(NS_LITERAL_STRING(" /"));
          if (!widthDefault) {
            aValue.Append(PRUnichar(' '));
            AppendValueToString(eCSSProperty_border_image_width, aValue);
          }
          if (!outsetDefault) {
            aValue.Append(NS_LITERAL_STRING(" / "));
            AppendValueToString(eCSSProperty_border_image_outset, aValue);
          }
        }
      }

      bool repeatDefault = data->HasDefaultBorderImageRepeat();
      if (!repeatDefault) {
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_image_repeat, aValue);
      }
      break;
    }
    case eCSSProperty_border: {
      // If we have a non-default value for any of the properties that
      // this shorthand sets but cannot specify, we have to return the
      // empty string.
      if (data->ValueFor(eCSSProperty_border_image_source)->GetUnit() !=
            eCSSUnit_None ||
          !data->HasDefaultBorderImageSlice() ||
          !data->HasDefaultBorderImageWidth() ||
          !data->HasDefaultBorderImageOutset() ||
          !data->HasDefaultBorderImageRepeat() ||
          data->ValueFor(eCSSProperty_border_top_colors)->GetUnit() !=
            eCSSUnit_None ||
          data->ValueFor(eCSSProperty_border_right_colors)->GetUnit() !=
            eCSSUnit_None ||
          data->ValueFor(eCSSProperty_border_bottom_colors)->GetUnit() !=
            eCSSUnit_None ||
          data->ValueFor(eCSSProperty_border_left_colors)->GetUnit() !=
            eCSSUnit_None) {
        break;
      }

      const nsCSSProperty* subproptables[3] = {
        nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color),
        nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_style),
        nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_width)
      };
      bool match = true;
      for (const nsCSSProperty** subprops = subproptables,
               **subprops_end = ArrayEnd(subproptables);
           subprops < subprops_end; ++subprops) {
        // Check only the first four subprops in each table, since the
        // others are extras for dimensional box properties.
        const nsCSSValue *firstSide = data->ValueFor((*subprops)[0]);
        for (int32_t side = 1; side < 4; ++side) {
          const nsCSSValue *otherSide =
            data->ValueFor((*subprops)[side]);
          if (*firstSide != *otherSide)
            match = false;
        }
      }
      if (!match) {
        // We can't express what we have in the border shorthand
        break;
      }
      // tweak aProperty and fall through
      aProperty = eCSSProperty_border_top;
    }
    case eCSSProperty_border_top:
    case eCSSProperty_border_right:
    case eCSSProperty_border_bottom:
    case eCSSProperty_border_left:
    case eCSSProperty_border_start:
    case eCSSProperty_border_end:
    case eCSSProperty__moz_column_rule:
    case eCSSProperty_outline: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ABORT_IF_FALSE(StringEndsWith(nsCSSProps::GetStringValue(subprops[2]),
                                       NS_LITERAL_CSTRING("-color")) ||
                        StringEndsWith(nsCSSProps::GetStringValue(subprops[2]),
                                       NS_LITERAL_CSTRING("-color-value")),
                        "third subprop must be the color property");
      const nsCSSValue *colorValue = data->ValueFor(subprops[2]);
      bool isMozUseTextColor =
        colorValue->GetUnit() == eCSSUnit_Enumerated &&
        colorValue->GetIntValue() == NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR;
      if (!AppendValueToString(subprops[0], aValue) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[1], aValue)) ||
          // Don't output a third value when it's -moz-use-text-color.
          !(isMozUseTextColor ||
            (aValue.Append(PRUnichar(' ')),
             AppendValueToString(subprops[2], aValue)))) {
        aValue.Truncate();
      }
      break;
    }
    case eCSSProperty_margin_left:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_start:
    case eCSSProperty_margin_end:
    case eCSSProperty_padding_left:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_start:
    case eCSSProperty_padding_end:
    case eCSSProperty_border_left_color:
    case eCSSProperty_border_left_style:
    case eCSSProperty_border_left_width:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_start_color:
    case eCSSProperty_border_start_style:
    case eCSSProperty_border_start_width:
    case eCSSProperty_border_end_color:
    case eCSSProperty_border_end_style:
    case eCSSProperty_border_end_width: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ABORT_IF_FALSE(subprops[3] == eCSSProperty_UNKNOWN,
                        "not box property with physical vs. logical cascading");
      AppendValueToString(subprops[0], aValue);
      break;
    }
    case eCSSProperty_background: {
      // We know from above that all subproperties were specified.
      // However, we still can't represent that in the shorthand unless
      // they're all lists of the same length.  So if they're different
      // lengths, we need to bail out.
      // We also need to bail out if an item has background-clip and
      // background-origin that are different and not the default
      // values.  (We omit them if they're both default.)
      const nsCSSValueList *image =
        data->ValueFor(eCSSProperty_background_image)->
        GetListValue();
      const nsCSSValuePairList *repeat =
        data->ValueFor(eCSSProperty_background_repeat)->
        GetPairListValue();
      const nsCSSValueList *attachment =
        data->ValueFor(eCSSProperty_background_attachment)->
        GetListValue();
      const nsCSSValueList *position =
        data->ValueFor(eCSSProperty_background_position)->
        GetListValue();
      const nsCSSValueList *clip =
        data->ValueFor(eCSSProperty_background_clip)->
        GetListValue();
      const nsCSSValueList *origin =
        data->ValueFor(eCSSProperty_background_origin)->
        GetListValue();
      const nsCSSValuePairList *size =
        data->ValueFor(eCSSProperty_background_size)->
        GetPairListValue();
      for (;;) {
        image->mValue.AppendToString(eCSSProperty_background_image, aValue);
        aValue.Append(PRUnichar(' '));
        repeat->mXValue.AppendToString(eCSSProperty_background_repeat, aValue);
        if (repeat->mYValue.GetUnit() != eCSSUnit_Null) {
          repeat->mYValue.AppendToString(eCSSProperty_background_repeat, aValue);
        }
        aValue.Append(PRUnichar(' '));
        attachment->mValue.AppendToString(eCSSProperty_background_attachment,
                                          aValue);
        aValue.Append(PRUnichar(' '));
        position->mValue.AppendToString(eCSSProperty_background_position,
                                        aValue);
        
        if (size->mXValue.GetUnit() != eCSSUnit_Auto ||
            size->mYValue.GetUnit() != eCSSUnit_Auto) {
          aValue.Append(PRUnichar(' '));
          aValue.Append(PRUnichar('/'));
          aValue.Append(PRUnichar(' '));
          size->mXValue.AppendToString(eCSSProperty_background_size, aValue);
          aValue.Append(PRUnichar(' '));
          size->mYValue.AppendToString(eCSSProperty_background_size, aValue);
        }

        NS_ABORT_IF_FALSE(clip->mValue.GetUnit() == eCSSUnit_Enumerated &&
                          origin->mValue.GetUnit() == eCSSUnit_Enumerated,
                          "should not have inherit/initial within list");

        if (clip->mValue.GetIntValue() != NS_STYLE_BG_CLIP_BORDER ||
            origin->mValue.GetIntValue() != NS_STYLE_BG_ORIGIN_PADDING) {
          MOZ_ASSERT(nsCSSProps::kKeywordTableTable[
                       eCSSProperty_background_origin] ==
                     nsCSSProps::kBackgroundOriginKTable);
          MOZ_ASSERT(nsCSSProps::kKeywordTableTable[
                       eCSSProperty_background_clip] ==
                     nsCSSProps::kBackgroundOriginKTable);
          MOZ_STATIC_ASSERT(NS_STYLE_BG_CLIP_BORDER ==
                            NS_STYLE_BG_ORIGIN_BORDER &&
                            NS_STYLE_BG_CLIP_PADDING ==
                            NS_STYLE_BG_ORIGIN_PADDING &&
                            NS_STYLE_BG_CLIP_CONTENT ==
                            NS_STYLE_BG_ORIGIN_CONTENT,
                            "bg-clip and bg-origin style constants must agree");
          aValue.Append(PRUnichar(' '));
          origin->mValue.AppendToString(eCSSProperty_background_origin, aValue);

          if (clip->mValue != origin->mValue) {
            aValue.Append(PRUnichar(' '));
            clip->mValue.AppendToString(eCSSProperty_background_clip, aValue);
          }
        }

        image = image->mNext;
        repeat = repeat->mNext;
        attachment = attachment->mNext;
        position = position->mNext;
        clip = clip->mNext;
        origin = origin->mNext;
        size = size->mNext;

        if (!image) {
          if (repeat || attachment || position || clip || origin || size) {
            // Uneven length lists, so can't be serialized as shorthand.
            aValue.Truncate();
            return;
          }
          break;
        }
        if (!repeat || !attachment || !position || !clip || !origin || !size) {
          // Uneven length lists, so can't be serialized as shorthand.
          aValue.Truncate();
          return;
        }
        aValue.Append(PRUnichar(','));
        aValue.Append(PRUnichar(' '));
      }

      aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_background_color, aValue);
      break;
    }
    case eCSSProperty_font: {
      // systemFont might not be present; other values are guaranteed to be
      // available based on the shorthand check at the beginning of the
      // function, as long as the prop is enabled
      const nsCSSValue *systemFont =
        data->ValueFor(eCSSProperty__x_system_font);
      const nsCSSValue *style =
        data->ValueFor(eCSSProperty_font_style);
      const nsCSSValue *variant =
        data->ValueFor(eCSSProperty_font_variant);
      const nsCSSValue *weight =
        data->ValueFor(eCSSProperty_font_weight);
      const nsCSSValue *size =
        data->ValueFor(eCSSProperty_font_size);
      const nsCSSValue *lh =
        data->ValueFor(eCSSProperty_line_height);
      const nsCSSValue *family =
        data->ValueFor(eCSSProperty_font_family);
      const nsCSSValue *stretch =
        data->ValueFor(eCSSProperty_font_stretch);
      const nsCSSValue *sizeAdjust =
        data->ValueFor(eCSSProperty_font_size_adjust);
      const nsCSSValue *featureSettings =
        data->ValueFor(eCSSProperty_font_feature_settings);
      const nsCSSValue *languageOverride =
        data->ValueFor(eCSSProperty_font_language_override);
      const nsCSSValue *fontKerning =
        data->ValueFor(eCSSProperty_font_kerning);
      const nsCSSValue *fontSynthesis =
        data->ValueFor(eCSSProperty_font_synthesis);
      const nsCSSValue *fontVariantAlternates =
        data->ValueFor(eCSSProperty_font_variant_alternates);
      const nsCSSValue *fontVariantCaps =
        data->ValueFor(eCSSProperty_font_variant_caps);
      const nsCSSValue *fontVariantEastAsian =
        data->ValueFor(eCSSProperty_font_variant_east_asian);
      const nsCSSValue *fontVariantLigatures =
        data->ValueFor(eCSSProperty_font_variant_ligatures);
      const nsCSSValue *fontVariantNumeric =
        data->ValueFor(eCSSProperty_font_variant_numeric);
      const nsCSSValue *fontVariantPosition =
        data->ValueFor(eCSSProperty_font_variant_position);

      // if font features are not enabled, pointers for fontVariant
      // values above may be null since the shorthand check ignores them
      bool fontFeaturesEnabled =
        mozilla::Preferences::GetBool("layout.css.font-features.enabled");

      if (systemFont &&
          systemFont->GetUnit() != eCSSUnit_None &&
          systemFont->GetUnit() != eCSSUnit_Null) {
        if (style->GetUnit() != eCSSUnit_System_Font ||
            variant->GetUnit() != eCSSUnit_System_Font ||
            weight->GetUnit() != eCSSUnit_System_Font ||
            size->GetUnit() != eCSSUnit_System_Font ||
            lh->GetUnit() != eCSSUnit_System_Font ||
            family->GetUnit() != eCSSUnit_System_Font ||
            stretch->GetUnit() != eCSSUnit_System_Font ||
            sizeAdjust->GetUnit() != eCSSUnit_System_Font ||
            featureSettings->GetUnit() != eCSSUnit_System_Font ||
            languageOverride->GetUnit() != eCSSUnit_System_Font ||
            (fontFeaturesEnabled &&
             (fontKerning->GetUnit() != eCSSUnit_System_Font ||
              fontSynthesis->GetUnit() != eCSSUnit_System_Font ||
              fontVariantAlternates->GetUnit() != eCSSUnit_System_Font ||
              fontVariantCaps->GetUnit() != eCSSUnit_System_Font ||
              fontVariantEastAsian->GetUnit() != eCSSUnit_System_Font ||
              fontVariantLigatures->GetUnit() != eCSSUnit_System_Font ||
              fontVariantNumeric->GetUnit() != eCSSUnit_System_Font ||
              fontVariantPosition->GetUnit() != eCSSUnit_System_Font))) {
          // This can't be represented as a shorthand.
          return;
        }
        systemFont->AppendToString(eCSSProperty__x_system_font, aValue);
      } else {
        // properties reset by this shorthand property to their
        // initial values but not represented in its syntax
        if (stretch->GetUnit() != eCSSUnit_Enumerated ||
            stretch->GetIntValue() != NS_STYLE_FONT_STRETCH_NORMAL ||
            sizeAdjust->GetUnit() != eCSSUnit_None ||
            featureSettings->GetUnit() != eCSSUnit_Normal ||
            languageOverride->GetUnit() != eCSSUnit_Normal ||
            (fontFeaturesEnabled &&
             (fontKerning->GetIntValue() != NS_FONT_KERNING_AUTO ||
              fontSynthesis->GetUnit() != eCSSUnit_Enumerated ||
              fontSynthesis->GetIntValue() !=
                (NS_FONT_SYNTHESIS_WEIGHT | NS_FONT_SYNTHESIS_STYLE) ||
              fontVariantAlternates->GetUnit() != eCSSUnit_Normal ||
              fontVariantCaps->GetUnit() != eCSSUnit_Normal ||
              fontVariantEastAsian->GetUnit() != eCSSUnit_Normal ||
              fontVariantLigatures->GetUnit() != eCSSUnit_Normal ||
              fontVariantNumeric->GetUnit() != eCSSUnit_Normal ||
              fontVariantPosition->GetUnit() != eCSSUnit_Normal))) {
          return;
        }

        if (style->GetUnit() != eCSSUnit_Enumerated ||
            style->GetIntValue() != NS_FONT_STYLE_NORMAL) {
          style->AppendToString(eCSSProperty_font_style, aValue);
          aValue.Append(PRUnichar(' '));
        }
        if (variant->GetUnit() != eCSSUnit_Enumerated ||
            variant->GetIntValue() != NS_FONT_VARIANT_NORMAL) {
          variant->AppendToString(eCSSProperty_font_variant, aValue);
          aValue.Append(PRUnichar(' '));
        }
        if (weight->GetUnit() != eCSSUnit_Enumerated ||
            weight->GetIntValue() != NS_FONT_WEIGHT_NORMAL) {
          weight->AppendToString(eCSSProperty_font_weight, aValue);
          aValue.Append(PRUnichar(' '));
        }
        size->AppendToString(eCSSProperty_font_size, aValue);
        if (lh->GetUnit() != eCSSUnit_Normal) {
          aValue.Append(PRUnichar('/'));
          lh->AppendToString(eCSSProperty_line_height, aValue);
        }
        aValue.Append(PRUnichar(' '));
        family->AppendToString(eCSSProperty_font_family, aValue);
      }
      break;
    }
    case eCSSProperty_list_style:
      if (AppendValueToString(eCSSProperty_list_style_type, aValue))
        aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_list_style_position, aValue))
        aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_list_style_image, aValue);
      break;
    case eCSSProperty_overflow: {
      const nsCSSValue &xValue =
        *data->ValueFor(eCSSProperty_overflow_x);
      const nsCSSValue &yValue =
        *data->ValueFor(eCSSProperty_overflow_y);
      if (xValue == yValue)
        xValue.AppendToString(eCSSProperty_overflow_x, aValue);
      break;
    }
    case eCSSProperty_text_decoration: {
      // If text-decoration-color or text-decoration-style isn't initial value,
      // we cannot serialize the text-decoration shorthand value.
      const nsCSSValue *decorationColor =
        data->ValueFor(eCSSProperty_text_decoration_color);
      const nsCSSValue *decorationStyle =
        data->ValueFor(eCSSProperty_text_decoration_style);

      NS_ABORT_IF_FALSE(decorationStyle->GetUnit() == eCSSUnit_Enumerated,
                        nsPrintfCString("bad text-decoration-style unit %d",
                                        decorationStyle->GetUnit()).get());

      if (decorationColor->GetUnit() != eCSSUnit_Enumerated ||
          decorationColor->GetIntValue() != NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR ||
          decorationStyle->GetIntValue() !=
            NS_STYLE_TEXT_DECORATION_STYLE_SOLID) {
        return;
      }

      const nsCSSValue *textBlink =
        data->ValueFor(eCSSProperty_text_blink);
      const nsCSSValue *decorationLine =
        data->ValueFor(eCSSProperty_text_decoration_line);

      NS_ABORT_IF_FALSE(textBlink->GetUnit() == eCSSUnit_Enumerated,
                        nsPrintfCString("bad text-blink unit %d",
                                        textBlink->GetUnit()).get());
      NS_ABORT_IF_FALSE(decorationLine->GetUnit() == eCSSUnit_Enumerated,
                        nsPrintfCString("bad text-decoration-line unit %d",
                                        decorationLine->GetUnit()).get());

      bool blinkNone = (textBlink->GetIntValue() == NS_STYLE_TEXT_BLINK_NONE);
      bool lineNone =
        (decorationLine->GetIntValue() == NS_STYLE_TEXT_DECORATION_LINE_NONE);

      if (blinkNone && lineNone) {
        AppendValueToString(eCSSProperty_text_decoration_line, aValue);
      } else {
        if (!blinkNone) {
          AppendValueToString(eCSSProperty_text_blink, aValue);
        }
        if (!lineNone) {
          if (!aValue.IsEmpty()) {
            aValue.Append(PRUnichar(' '));
          }
          AppendValueToString(eCSSProperty_text_decoration_line, aValue);
        }
      }
      break;
    }
    case eCSSProperty_transition: {
      const nsCSSValue *transProp =
        data->ValueFor(eCSSProperty_transition_property);
      const nsCSSValue *transDuration =
        data->ValueFor(eCSSProperty_transition_duration);
      const nsCSSValue *transTiming =
        data->ValueFor(eCSSProperty_transition_timing_function);
      const nsCSSValue *transDelay =
        data->ValueFor(eCSSProperty_transition_delay);

      NS_ABORT_IF_FALSE(transDuration->GetUnit() == eCSSUnit_List ||
                        transDuration->GetUnit() == eCSSUnit_ListDep,
                        nsPrintfCString("bad t-duration unit %d",
                                        transDuration->GetUnit()).get());
      NS_ABORT_IF_FALSE(transTiming->GetUnit() == eCSSUnit_List ||
                        transTiming->GetUnit() == eCSSUnit_ListDep,
                        nsPrintfCString("bad t-timing unit %d",
                                        transTiming->GetUnit()).get());
      NS_ABORT_IF_FALSE(transDelay->GetUnit() == eCSSUnit_List ||
                        transDelay->GetUnit() == eCSSUnit_ListDep,
                        nsPrintfCString("bad t-delay unit %d",
                                        transDelay->GetUnit()).get());

      const nsCSSValueList* dur = transDuration->GetListValue();
      const nsCSSValueList* tim = transTiming->GetListValue();
      const nsCSSValueList* del = transDelay->GetListValue();

      if (transProp->GetUnit() == eCSSUnit_None ||
          transProp->GetUnit() == eCSSUnit_All) {
        // If any of the other three lists has more than one element,
        // we can't use the shorthand.
        if (!dur->mNext && !tim->mNext && !del->mNext) {
          transProp->AppendToString(eCSSProperty_transition_property, aValue);
          aValue.Append(PRUnichar(' '));
          dur->mValue.AppendToString(eCSSProperty_transition_duration,aValue);
          aValue.Append(PRUnichar(' '));
          tim->mValue.AppendToString(eCSSProperty_transition_timing_function,
                                     aValue);
          aValue.Append(PRUnichar(' '));
          del->mValue.AppendToString(eCSSProperty_transition_delay, aValue);
          aValue.Append(PRUnichar(' '));
        } else {
          aValue.Truncate();
        }
      } else {
        NS_ABORT_IF_FALSE(transProp->GetUnit() == eCSSUnit_List ||
                          transProp->GetUnit() == eCSSUnit_ListDep,
                          nsPrintfCString("bad t-prop unit %d",
                                          transProp->GetUnit()).get());
        const nsCSSValueList* pro = transProp->GetListValue();
        for (;;) {
          pro->mValue.AppendToString(eCSSProperty_transition_property,
                                        aValue);
          aValue.Append(PRUnichar(' '));
          dur->mValue.AppendToString(eCSSProperty_transition_duration,
                                        aValue);
          aValue.Append(PRUnichar(' '));
          tim->mValue.AppendToString(eCSSProperty_transition_timing_function,
                                        aValue);
          aValue.Append(PRUnichar(' '));
          del->mValue.AppendToString(eCSSProperty_transition_delay,
                                        aValue);
          pro = pro->mNext;
          dur = dur->mNext;
          tim = tim->mNext;
          del = del->mNext;
          if (!pro || !dur || !tim || !del) {
            break;
          }
          aValue.AppendLiteral(", ");
        }
        if (pro || dur || tim || del) {
          // Lists not all the same length, can't use shorthand.
          aValue.Truncate();
        }
      }
      break;
    }
    case eCSSProperty_animation: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(eCSSProperty_animation);
      static const size_t numProps = 7;
      NS_ABORT_IF_FALSE(subprops[numProps] == eCSSProperty_UNKNOWN,
                        "unexpected number of subproperties");
      const nsCSSValue* values[numProps];
      const nsCSSValueList* lists[numProps];

      for (uint32_t i = 0; i < numProps; ++i) {
        values[i] = data->ValueFor(subprops[i]);
        NS_ABORT_IF_FALSE(values[i]->GetUnit() == eCSSUnit_List ||
                          values[i]->GetUnit() == eCSSUnit_ListDep,
                          nsPrintfCString("bad a-duration unit %d",
                                          values[i]->GetUnit()).get());
        lists[i] = values[i]->GetListValue();
      }

      for (;;) {
        // We must serialize 'animation-name' last in case it has
        // a value that conflicts with one of the other keyword properties.
        NS_ABORT_IF_FALSE(subprops[numProps - 1] ==
                            eCSSProperty_animation_name,
                          "animation-name must be last");
        bool done = false;
        for (uint32_t i = 0;;) {
          lists[i]->mValue.AppendToString(subprops[i], aValue);
          lists[i] = lists[i]->mNext;
          if (!lists[i]) {
            done = true;
          }
          if (++i == numProps) {
            break;
          }
          aValue.Append(PRUnichar(' '));
        }
        if (done) {
          break;
        }
        aValue.AppendLiteral(", ");
      }
      for (uint32_t i = 0; i < numProps; ++i) {
        if (lists[i]) {
          // Lists not all the same length, can't use shorthand.
          aValue.Truncate();
          break;
        }
      }
      break;
    }
    case eCSSProperty_marker: {
      const nsCSSValue &endValue =
        *data->ValueFor(eCSSProperty_marker_end);
      const nsCSSValue &midValue =
        *data->ValueFor(eCSSProperty_marker_mid);
      const nsCSSValue &startValue =
        *data->ValueFor(eCSSProperty_marker_start);
      if (endValue == midValue && midValue == startValue)
        AppendValueToString(eCSSProperty_marker_end, aValue);
      break;
    }
    case eCSSProperty__moz_columns: {
      // Two values, column-count and column-width, separated by a space.
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      AppendValueToString(subprops[0], aValue);
      aValue.Append(PRUnichar(' '));
      AppendValueToString(subprops[1], aValue);
      break;
    }
    case eCSSProperty_flex: {
      // flex-grow, flex-shrink, flex-basis, separated by single space
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);

      AppendValueToString(subprops[0], aValue);
      aValue.Append(PRUnichar(' '));
      AppendValueToString(subprops[1], aValue);
      aValue.Append(PRUnichar(' '));
      AppendValueToString(subprops[2], aValue);
      break;
    }
    case eCSSProperty__moz_transform: {
      // shorthands that are just aliases with different parsing rules
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ABORT_IF_FALSE(subprops[1] == eCSSProperty_UNKNOWN,
                        "must have exactly one subproperty");
      AppendValueToString(subprops[0], aValue);
      break;
    }
    default:
      NS_ABORT_IF_FALSE(false, "no other shorthands");
      break;
  }
}

bool
Declaration::GetValueIsImportant(const nsAString& aProperty) const
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty, nsCSSProps::eAny);
  if (propID == eCSSProperty_UNKNOWN) {
    return false;
  }
  return GetValueIsImportant(propID);
}

bool
Declaration::GetValueIsImportant(nsCSSProperty aProperty) const
{
  if (!mImportantData)
    return false;

  // Calling ValueFor is inefficient, but we can assume '!important' is rare.

  if (!nsCSSProps::IsShorthand(aProperty)) {
    return mImportantData->ValueFor(aProperty) != nullptr;
  }

  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
    if (*p == eCSSProperty__x_system_font) {
      // The system_font subproperty doesn't count.
      continue;
    }
    if (!mImportantData->ValueFor(*p)) {
      return false;
    }
  }
  return true;
}

void
Declaration::AppendPropertyAndValueToString(nsCSSProperty aProperty,
                                            nsAutoString& aValue,
                                            nsAString& aResult) const
{
  NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                    "property enum out of range");
  NS_ABORT_IF_FALSE((aProperty < eCSSProperty_COUNT_no_shorthands) ==
                    aValue.IsEmpty(),
                    "aValue should be given for shorthands but not longhands");
  AppendASCIItoUTF16(nsCSSProps::GetStringValue(aProperty), aResult);
  aResult.AppendLiteral(": ");
  if (aValue.IsEmpty())
    AppendValueToString(aProperty, aResult);
  else
    aResult.Append(aValue);
  if (GetValueIsImportant(aProperty)) {
    aResult.AppendLiteral(" ! important");
  }
  aResult.AppendLiteral("; ");
}

void
Declaration::ToString(nsAString& aString) const
{
  // Someone cares about this declaration's contents, so don't let it
  // change from under them.  See e.g. bug 338679.
  SetImmutable();

  nsCSSCompressedDataBlock *systemFontData =
    GetValueIsImportant(eCSSProperty__x_system_font) ? mImportantData : mData;
  const nsCSSValue *systemFont =
    systemFontData->ValueFor(eCSSProperty__x_system_font);
  const bool haveSystemFont = systemFont &&
                                systemFont->GetUnit() != eCSSUnit_None &&
                                systemFont->GetUnit() != eCSSUnit_Null;
  bool didSystemFont = false;

  int32_t count = mOrder.Length();
  int32_t index;
  nsAutoTArray<nsCSSProperty, 16> shorthandsUsed;
  for (index = 0; index < count; index++) {
    nsCSSProperty property = OrderValueAt(index);
    if (!nsCSSProps::IsEnabled(property)) {
      continue;
    }
    bool doneProperty = false;

    // If we already used this property in a shorthand, skip it.
    if (shorthandsUsed.Length() > 0) {
      for (const nsCSSProperty *shorthands =
             nsCSSProps::ShorthandsContaining(property);
           *shorthands != eCSSProperty_UNKNOWN; ++shorthands) {
        if (shorthandsUsed.Contains(*shorthands)) {
          doneProperty = true;
          break;
        }
      }
      if (doneProperty)
        continue;
    }

    // Try to use this property in a shorthand.
    nsAutoString value;
    for (const nsCSSProperty *shorthands =
           nsCSSProps::ShorthandsContaining(property);
         *shorthands != eCSSProperty_UNKNOWN; ++shorthands) {
      // ShorthandsContaining returns the shorthands in order from those
      // that contain the most subproperties to those that contain the
      // least, which is exactly the order we want to test them.
      nsCSSProperty shorthand = *shorthands;

      // If GetValue gives us a non-empty string back, we can use that
      // value; otherwise it's not possible to use this shorthand.
      GetValue(shorthand, value);
      if (!value.IsEmpty()) {
        AppendPropertyAndValueToString(shorthand, value, aString);
        shorthandsUsed.AppendElement(shorthand);
        doneProperty = true;
        break;
      }

      NS_ABORT_IF_FALSE(shorthand != eCSSProperty_font ||
                        *(shorthands + 1) == eCSSProperty_UNKNOWN,
                        "font should always be the only containing shorthand");
      if (shorthand == eCSSProperty_font) {
        if (haveSystemFont && !didSystemFont) {
          // Output the shorthand font declaration that we will
          // partially override later.  But don't add it to
          // |shorthandsUsed|, since we will have to override it.
          systemFont->AppendToString(eCSSProperty__x_system_font, value);
          AppendPropertyAndValueToString(eCSSProperty_font, value, aString);
          value.Truncate();
          didSystemFont = true;
        }

        // That we output the system font is enough for this property if:
        //   (1) it's the hidden system font subproperty (which either
        //       means we output it or we don't have it), or
        //   (2) its value is the hidden system font value and it matches
        //       the hidden system font subproperty in importance, and
        //       we output the system font subproperty.
        const nsCSSValue *val = systemFontData->ValueFor(property);
        if (property == eCSSProperty__x_system_font ||
            (haveSystemFont && val && val->GetUnit() == eCSSUnit_System_Font)) {
          doneProperty = true;
        }
      }
    }
    if (doneProperty)
      continue;

    NS_ABORT_IF_FALSE(value.IsEmpty(), "value should be empty now");
    AppendPropertyAndValueToString(property, value, aString);
  }
  if (! aString.IsEmpty()) {
    // if the string is not empty, we have trailing whitespace we
    // should remove
    aString.Truncate(aString.Length() - 1);
  }
}

#ifdef DEBUG
void
Declaration::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);
  nsAutoString s;
  ToString(s);
  fputs(NS_ConvertUTF16toUTF8(s).get(), out);
  fputs("}", out);
}
#endif

bool
Declaration::GetNthProperty(uint32_t aIndex, nsAString& aReturn) const
{
  aReturn.Truncate();
  if (aIndex < mOrder.Length()) {
    nsCSSProperty property = OrderValueAt(aIndex);
    if (0 <= property) {
      AppendASCIItoUTF16(nsCSSProps::GetStringValue(property), aReturn);
      return true;
    }
  }
  return false;
}

void
Declaration::InitializeEmpty()
{
  NS_ABORT_IF_FALSE(!mData && !mImportantData, "already initialized");
  mData = nsCSSCompressedDataBlock::CreateEmptyBlock();
}

Declaration*
Declaration::EnsureMutable()
{
  NS_ABORT_IF_FALSE(mData, "should only be called when not expanded");
  if (!IsMutable()) {
    return new Declaration(*this);
  } else {
    return this;
  }
}

size_t
Declaration::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mOrder.SizeOfExcludingThis(aMallocSizeOf);
  n += mData          ? mData         ->SizeOfIncludingThis(aMallocSizeOf) : 0;
  n += mImportantData ? mImportantData->SizeOfIncludingThis(aMallocSizeOf) : 0;
  return n;
}

} // namespace mozilla::css
} // namespace mozilla
