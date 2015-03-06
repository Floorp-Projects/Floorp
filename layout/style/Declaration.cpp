/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of a declaration block (or style attribute) in a CSS
 * stylesheet
 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/MemoryReporting.h"

#include "mozilla/css/Declaration.h"
#include "nsPrintfCString.h"
#include "gfxFontConstants.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace css {

Declaration::Declaration()
  : mImmutable(false)
{
  MOZ_COUNT_CTOR(mozilla::css::Declaration);
}

Declaration::Declaration(const Declaration& aCopy)
  : mOrder(aCopy.mOrder),
    mVariableOrder(aCopy.mVariableOrder),
    mData(aCopy.mData ? aCopy.mData->Clone() : nullptr),
    mImportantData(aCopy.mImportantData ?
                     aCopy.mImportantData->Clone() : nullptr),
    mVariables(aCopy.mVariables ?
        new CSSVariableDeclarations(*aCopy.mVariables) :
        nullptr),
    mImportantVariables(aCopy.mImportantVariables ?
        new CSSVariableDeclarations(*aCopy.mImportantVariables) :
        nullptr),
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
  MOZ_ASSERT(!mData && !mImportantData,
             "should only be called while expanded");
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
             "shorthands forbidden");
  // order IS important for CSS, so remove and add to the end
  mOrder.RemoveElement(static_cast<uint32_t>(aProperty));
  mOrder.AppendElement(static_cast<uint32_t>(aProperty));
}

void
Declaration::RemoveProperty(nsCSSProperty aProperty)
{
  MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT);

  nsCSSExpandedDataBlock data;
  ExpandTo(&data);
  MOZ_ASSERT(!mData && !mImportantData, "Expand didn't null things out");

  if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty,
                                         nsCSSProps::eEnabledForAllContent) {
      data.ClearLonghandProperty(*p);
      mOrder.RemoveElement(static_cast<uint32_t>(*p));
    }
  } else {
    data.ClearLonghandProperty(aProperty);
    mOrder.RemoveElement(static_cast<uint32_t>(aProperty));
  }

  CompressFrom(&data);
}

bool
Declaration::HasProperty(nsCSSProperty aProperty) const
{
  MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
             "property ID out of range");

  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                      ? mImportantData : mData;
  const nsCSSValue *val = data->ValueFor(aProperty);
  return !!val;
}

bool
Declaration::AppendValueToString(nsCSSProperty aProperty,
                                 nsAString& aResult,
                                 nsCSSValue::Serialization aSerialization) const
{
  MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
             "property ID out of range");

  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                      ? mImportantData : mData;
  const nsCSSValue *val = data->ValueFor(aProperty);
  if (!val) {
    return false;
  }

  val->AppendToString(aProperty, aResult, aSerialization);
  return true;
}

void
Declaration::GetValue(nsCSSProperty aProperty, nsAString& aValue) const
{
  GetValue(aProperty, aValue, nsCSSValue::eNormalized);
}

void
Declaration::GetAuthoredValue(nsCSSProperty aProperty, nsAString& aValue) const
{
  GetValue(aProperty, aValue, nsCSSValue::eAuthorSpecified);
}

void
Declaration::GetValue(nsCSSProperty aProperty, nsAString& aValue,
                      nsCSSValue::Serialization aSerialization) const
{
  aValue.Truncate(0);

  // simple properties are easy.
  if (!nsCSSProps::IsShorthand(aProperty)) {
    AppendValueToString(aProperty, aValue, aSerialization);
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
  //   (2) Since 'inherit', 'initial' and 'unset' can only be specified
  //       as the values for entire properties, we need to return the
  //       empty string if some but not all of the subproperties have one
  //       of those values.
  //   (3) Since a single value only makes sense with or without
  //       !important, we return the empty string if some values are
  //       !important and some are not.
  // Since we're doing this check for 'inherit' and 'initial' up front,
  // we can also simplify the property serialization code by serializing
  // those values up front as well.
  //
  // Additionally, if a shorthand property was set using a value with a
  // variable reference and none of its component longhand properties were
  // then overridden on the declaration, we return the token stream
  // assigned to the shorthand.
  const nsCSSValue* tokenStream = nullptr;
  uint32_t totalCount = 0, importantCount = 0,
           initialCount = 0, inheritCount = 0, unsetCount = 0,
           matchingTokenStreamCount = 0, nonMatchingTokenStreamCount = 0;
  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty,
                                       nsCSSProps::eEnabledForAllContent) {
    if (*p == eCSSProperty__x_system_font) {
      // The system-font subproperty doesn't count.
      continue;
    }
    ++totalCount;
    const nsCSSValue *val = mData->ValueFor(*p);
    MOZ_ASSERT(!val || !mImportantData || !mImportantData->ValueFor(*p),
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
    } else if (val->GetUnit() == eCSSUnit_Unset) {
      ++unsetCount;
    } else if (val->GetUnit() == eCSSUnit_TokenStream) {
      if (val->GetTokenStreamValue()->mShorthandPropertyID == aProperty) {
        tokenStream = val;
        ++matchingTokenStreamCount;
      } else {
        ++nonMatchingTokenStreamCount;
      }
    }
  }
  if (importantCount != 0 && importantCount != totalCount) {
    // Case (3), no consistent importance.
    return;
  }
  if (initialCount == totalCount) {
    // Simplify serialization below by serializing initial up-front.
    nsCSSValue(eCSSUnit_Initial).AppendToString(eCSSProperty_UNKNOWN, aValue,
                                                nsCSSValue::eNormalized);
    return;
  }
  if (inheritCount == totalCount) {
    // Simplify serialization below by serializing inherit up-front.
    nsCSSValue(eCSSUnit_Inherit).AppendToString(eCSSProperty_UNKNOWN, aValue,
                                                nsCSSValue::eNormalized);
    return;
  }
  if (unsetCount == totalCount) {
    // Simplify serialization below by serializing unset up-front.
    nsCSSValue(eCSSUnit_Unset).AppendToString(eCSSProperty_UNKNOWN, aValue,
                                              nsCSSValue::eNormalized);
    return;
  }
  if (initialCount != 0 || inheritCount != 0 ||
      unsetCount != 0 || nonMatchingTokenStreamCount != 0) {
    // Case (2): partially initial, inherit, unset or token stream.
    return;
  }
  if (tokenStream) {
    if (matchingTokenStreamCount == totalCount) {
      // Shorthand was specified using variable references and all of its
      // longhand components were set by the shorthand.
      aValue.Append(tokenStream->GetTokenStreamValue()->mTokenStream);
    } else {
      // In all other cases, serialize to the empty string.
    }
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
      MOZ_ASSERT(nsCSSProps::GetStringValue(subprops[0]).Find("-top") !=
                 kNotFound, "first subprop must be top");
      MOZ_ASSERT(nsCSSProps::GetStringValue(subprops[1]).Find("-right") !=
                 kNotFound, "second subprop must be right");
      MOZ_ASSERT(nsCSSProps::GetStringValue(subprops[2]).Find("-bottom") !=
                 kNotFound, "third subprop must be bottom");
      MOZ_ASSERT(nsCSSProps::GetStringValue(subprops[3]).Find("-left") !=
                 kNotFound, "fourth subprop must be left");
      const nsCSSValue* vals[4] = {
        data->ValueFor(subprops[0]),
        data->ValueFor(subprops[1]),
        data->ValueFor(subprops[2]),
        data->ValueFor(subprops[3])
      };
      nsCSSValue::AppendSidesShorthandToString(subprops, vals, aValue,
                                               aSerialization);
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
      nsCSSValue::AppendBasicShapeRadiusToString(subprops, vals, aValue,
                                                 aSerialization);
      break;
    }
    case eCSSProperty_border_image: {
      // Even though there are some cases where we could omit
      // 'border-image-source' (when it's none), it's probably not a
      // good idea since it's likely to be confusing.  It would also
      // require adding the extra check that we serialize *something*.
      AppendValueToString(eCSSProperty_border_image_source, aValue,
                          aSerialization);

      bool sliceDefault = data->HasDefaultBorderImageSlice();
      bool widthDefault = data->HasDefaultBorderImageWidth();
      bool outsetDefault = data->HasDefaultBorderImageOutset();

      if (!sliceDefault || !widthDefault || !outsetDefault) {
        aValue.Append(char16_t(' '));
        AppendValueToString(eCSSProperty_border_image_slice, aValue,
                            aSerialization);
        if (!widthDefault || !outsetDefault) {
          aValue.AppendLiteral(" /");
          if (!widthDefault) {
            aValue.Append(char16_t(' '));
            AppendValueToString(eCSSProperty_border_image_width, aValue,
                                aSerialization);
          }
          if (!outsetDefault) {
            aValue.AppendLiteral(" / ");
            AppendValueToString(eCSSProperty_border_image_outset, aValue,
                                aSerialization);
          }
        }
      }

      bool repeatDefault = data->HasDefaultBorderImageRepeat();
      if (!repeatDefault) {
        aValue.Append(char16_t(' '));
        AppendValueToString(eCSSProperty_border_image_repeat, aValue,
                            aSerialization);
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
    case eCSSProperty_border_block_start:
    case eCSSProperty_border_block_end:
    case eCSSProperty__moz_column_rule:
    case eCSSProperty_outline: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      MOZ_ASSERT(StringEndsWith(nsCSSProps::GetStringValue(subprops[2]),
                                NS_LITERAL_CSTRING("-color")),
                 "third subprop must be the color property");
      const nsCSSValue *colorValue = data->ValueFor(subprops[2]);
      bool isMozUseTextColor =
        colorValue->GetUnit() == eCSSUnit_Enumerated &&
        colorValue->GetIntValue() == NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR;
      if (!AppendValueToString(subprops[0], aValue, aSerialization) ||
          !(aValue.Append(char16_t(' ')),
            AppendValueToString(subprops[1], aValue, aSerialization)) ||
          // Don't output a third value when it's -moz-use-text-color.
          !(isMozUseTextColor ||
            (aValue.Append(char16_t(' ')),
             AppendValueToString(subprops[2], aValue, aSerialization)))) {
        aValue.Truncate();
      }
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
        // Serialize background-color at the beginning of the last item.
        if (!image->mNext) {
          AppendValueToString(eCSSProperty_background_color, aValue,
                              aSerialization);
          aValue.Append(char16_t(' '));
        }

        image->mValue.AppendToString(eCSSProperty_background_image, aValue,
                                     aSerialization);
        aValue.Append(char16_t(' '));
        repeat->mXValue.AppendToString(eCSSProperty_background_repeat, aValue,
                                       aSerialization);
        if (repeat->mYValue.GetUnit() != eCSSUnit_Null) {
          repeat->mYValue.AppendToString(eCSSProperty_background_repeat, aValue,
                                         aSerialization);
        }
        aValue.Append(char16_t(' '));
        attachment->mValue.AppendToString(eCSSProperty_background_attachment,
                                          aValue, aSerialization);
        aValue.Append(char16_t(' '));
        position->mValue.AppendToString(eCSSProperty_background_position,
                                        aValue, aSerialization);
        
        if (size->mXValue.GetUnit() != eCSSUnit_Auto ||
            size->mYValue.GetUnit() != eCSSUnit_Auto) {
          aValue.Append(char16_t(' '));
          aValue.Append(char16_t('/'));
          aValue.Append(char16_t(' '));
          size->mXValue.AppendToString(eCSSProperty_background_size, aValue,
                                       aSerialization);
          aValue.Append(char16_t(' '));
          size->mYValue.AppendToString(eCSSProperty_background_size, aValue,
                                       aSerialization);
        }

        MOZ_ASSERT(clip->mValue.GetUnit() == eCSSUnit_Enumerated &&
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
          static_assert(NS_STYLE_BG_CLIP_BORDER ==
                        NS_STYLE_BG_ORIGIN_BORDER &&
                        NS_STYLE_BG_CLIP_PADDING ==
                        NS_STYLE_BG_ORIGIN_PADDING &&
                        NS_STYLE_BG_CLIP_CONTENT ==
                        NS_STYLE_BG_ORIGIN_CONTENT,
                        "bg-clip and bg-origin style constants must agree");
          aValue.Append(char16_t(' '));
          origin->mValue.AppendToString(eCSSProperty_background_origin, aValue,
                                        aSerialization);

          if (clip->mValue != origin->mValue) {
            aValue.Append(char16_t(' '));
            clip->mValue.AppendToString(eCSSProperty_background_clip, aValue,
                                        aSerialization);
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
        aValue.Append(char16_t(','));
        aValue.Append(char16_t(' '));
      }
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

      if (systemFont &&
          systemFont->GetUnit() != eCSSUnit_None &&
          systemFont->GetUnit() != eCSSUnit_Null) {
        if (style->GetUnit() != eCSSUnit_System_Font ||
            weight->GetUnit() != eCSSUnit_System_Font ||
            size->GetUnit() != eCSSUnit_System_Font ||
            lh->GetUnit() != eCSSUnit_System_Font ||
            family->GetUnit() != eCSSUnit_System_Font ||
            stretch->GetUnit() != eCSSUnit_System_Font ||
            sizeAdjust->GetUnit() != eCSSUnit_System_Font ||
            featureSettings->GetUnit() != eCSSUnit_System_Font ||
            languageOverride->GetUnit() != eCSSUnit_System_Font ||
            fontKerning->GetUnit() != eCSSUnit_System_Font ||
            fontSynthesis->GetUnit() != eCSSUnit_System_Font ||
            fontVariantAlternates->GetUnit() != eCSSUnit_System_Font ||
            fontVariantCaps->GetUnit() != eCSSUnit_System_Font ||
            fontVariantEastAsian->GetUnit() != eCSSUnit_System_Font ||
            fontVariantLigatures->GetUnit() != eCSSUnit_System_Font ||
            fontVariantNumeric->GetUnit() != eCSSUnit_System_Font ||
            fontVariantPosition->GetUnit() != eCSSUnit_System_Font) {
          // This can't be represented as a shorthand.
          return;
        }
        systemFont->AppendToString(eCSSProperty__x_system_font, aValue,
                                   aSerialization);
      } else {
        // properties reset by this shorthand property to their
        // initial values but not represented in its syntax
        if (stretch->GetUnit() != eCSSUnit_Enumerated ||
            stretch->GetIntValue() != NS_STYLE_FONT_STRETCH_NORMAL ||
            sizeAdjust->GetUnit() != eCSSUnit_None ||
            featureSettings->GetUnit() != eCSSUnit_Normal ||
            languageOverride->GetUnit() != eCSSUnit_Normal ||
            fontKerning->GetIntValue() != NS_FONT_KERNING_AUTO ||
            fontSynthesis->GetUnit() != eCSSUnit_Enumerated ||
            fontSynthesis->GetIntValue() !=
              (NS_FONT_SYNTHESIS_WEIGHT | NS_FONT_SYNTHESIS_STYLE) ||
            fontVariantAlternates->GetUnit() != eCSSUnit_Normal ||
            fontVariantEastAsian->GetUnit() != eCSSUnit_Normal ||
            fontVariantLigatures->GetUnit() != eCSSUnit_Normal ||
            fontVariantNumeric->GetUnit() != eCSSUnit_Normal ||
            fontVariantPosition->GetUnit() != eCSSUnit_Normal) {
          return;
        }

        // only a normal or small-caps values of font-variant-caps can
        // be represented in the font shorthand
        if (fontVariantCaps->GetUnit() != eCSSUnit_Normal &&
            (fontVariantCaps->GetUnit() != eCSSUnit_Enumerated ||
             fontVariantCaps->GetIntValue() != NS_FONT_VARIANT_CAPS_SMALLCAPS)) {
          return;
        }

        if (style->GetUnit() != eCSSUnit_Enumerated ||
            style->GetIntValue() != NS_FONT_STYLE_NORMAL) {
          style->AppendToString(eCSSProperty_font_style, aValue,
                                aSerialization);
          aValue.Append(char16_t(' '));
        }
        if (fontVariantCaps->GetUnit() != eCSSUnit_Normal) {
          fontVariantCaps->AppendToString(eCSSProperty_font_variant_caps, aValue,
                                  aSerialization);
          aValue.Append(char16_t(' '));
        }
        if (weight->GetUnit() != eCSSUnit_Enumerated ||
            weight->GetIntValue() != NS_FONT_WEIGHT_NORMAL) {
          weight->AppendToString(eCSSProperty_font_weight, aValue,
                                 aSerialization);
          aValue.Append(char16_t(' '));
        }
        size->AppendToString(eCSSProperty_font_size, aValue, aSerialization);
        if (lh->GetUnit() != eCSSUnit_Normal) {
          aValue.Append(char16_t('/'));
          lh->AppendToString(eCSSProperty_line_height, aValue, aSerialization);
        }
        aValue.Append(char16_t(' '));
        family->AppendToString(eCSSProperty_font_family, aValue,
                               aSerialization);
      }
      break;
    }
    case eCSSProperty_font_variant: {
      const nsCSSProperty *subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      const nsCSSValue *fontVariantLigatures =
        data->ValueFor(eCSSProperty_font_variant_ligatures);

      // all subproperty values normal? system font?
      bool normalLigs = true, normalNonLigs = true, systemFont = true,
           hasSystem = false;
      for (const nsCSSProperty *sp = subprops; *sp != eCSSProperty_UNKNOWN; sp++) {
        const nsCSSValue *spVal = data->ValueFor(*sp);
        bool isNormal = (spVal->GetUnit() == eCSSUnit_Normal);
        if (*sp == eCSSProperty_font_variant_ligatures) {
          normalLigs = normalLigs && isNormal;
        } else {
          normalNonLigs = normalNonLigs && isNormal;
        }
        bool isSystem = (spVal->GetUnit() == eCSSUnit_System_Font);
        systemFont = systemFont && isSystem;
        hasSystem = hasSystem || isSystem;
      }

      bool ligsNone =
        fontVariantLigatures->GetUnit() == eCSSUnit_None;

      // normal, none, or system font ==> single value
      if ((normalLigs && normalNonLigs) ||
          (normalNonLigs && ligsNone) ||
          systemFont) {
        fontVariantLigatures->AppendToString(eCSSProperty_font_variant_ligatures,
                                             aValue,
                                             aSerialization);
      } else if (ligsNone || hasSystem) {
        // ligatures none but other values are non-normal ==> empty
        // at least one but not all values are system font ==> empty
        return;
      } else {
        // iterate over and append non-normal values
        bool appendSpace = false;
        for (const nsCSSProperty *sp = subprops;
             *sp != eCSSProperty_UNKNOWN; sp++) {
          const nsCSSValue *spVal = data->ValueFor(*sp);
          if (spVal && spVal->GetUnit() != eCSSUnit_Normal) {
            if (appendSpace) {
              aValue.Append(char16_t(' '));
            } else {
              appendSpace = true;
            }
            spVal->AppendToString(*sp, aValue, aSerialization);
          }
        }
      }
      break;
    }
    case eCSSProperty_list_style:
      if (AppendValueToString(eCSSProperty_list_style_position, aValue,
                              aSerialization)) {
        aValue.Append(char16_t(' '));
      }
      if (AppendValueToString(eCSSProperty_list_style_image, aValue,
                              aSerialization)) {
        aValue.Append(char16_t(' '));
      }
      AppendValueToString(eCSSProperty_list_style_type, aValue,
                          aSerialization);
      break;
    case eCSSProperty_overflow: {
      const nsCSSValue &xValue =
        *data->ValueFor(eCSSProperty_overflow_x);
      const nsCSSValue &yValue =
        *data->ValueFor(eCSSProperty_overflow_y);
      if (xValue == yValue)
        xValue.AppendToString(eCSSProperty_overflow_x, aValue, aSerialization);
      break;
    }
    case eCSSProperty_text_decoration: {
      const nsCSSValue *decorationColor =
        data->ValueFor(eCSSProperty_text_decoration_color);
      const nsCSSValue *decorationStyle =
        data->ValueFor(eCSSProperty_text_decoration_style);

      MOZ_ASSERT(decorationStyle->GetUnit() == eCSSUnit_Enumerated,
                 "bad text-decoration-style unit");

      AppendValueToString(eCSSProperty_text_decoration_line, aValue,
                          aSerialization);
      if (decorationStyle->GetIntValue() !=
            NS_STYLE_TEXT_DECORATION_STYLE_SOLID) {
        aValue.Append(char16_t(' '));
        AppendValueToString(eCSSProperty_text_decoration_style, aValue,
                            aSerialization);
      }
      if (decorationColor->GetUnit() != eCSSUnit_Enumerated ||
          decorationColor->GetIntValue() != NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR) {
        aValue.Append(char16_t(' '));
        AppendValueToString(eCSSProperty_text_decoration_color, aValue,
                            aSerialization);
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

      MOZ_ASSERT(transDuration->GetUnit() == eCSSUnit_List ||
                 transDuration->GetUnit() == eCSSUnit_ListDep,
                 "bad t-duration unit");
      MOZ_ASSERT(transTiming->GetUnit() == eCSSUnit_List ||
                 transTiming->GetUnit() == eCSSUnit_ListDep,
                 "bad t-timing unit");
      MOZ_ASSERT(transDelay->GetUnit() == eCSSUnit_List ||
                 transDelay->GetUnit() == eCSSUnit_ListDep,
                 "bad t-delay unit");

      const nsCSSValueList* dur = transDuration->GetListValue();
      const nsCSSValueList* tim = transTiming->GetListValue();
      const nsCSSValueList* del = transDelay->GetListValue();

      if (transProp->GetUnit() == eCSSUnit_None ||
          transProp->GetUnit() == eCSSUnit_All) {
        // If any of the other three lists has more than one element,
        // we can't use the shorthand.
        if (!dur->mNext && !tim->mNext && !del->mNext) {
          transProp->AppendToString(eCSSProperty_transition_property, aValue,
                                    aSerialization);
          aValue.Append(char16_t(' '));
          dur->mValue.AppendToString(eCSSProperty_transition_duration,aValue,
                                     aSerialization);
          aValue.Append(char16_t(' '));
          tim->mValue.AppendToString(eCSSProperty_transition_timing_function,
                                     aValue, aSerialization);
          aValue.Append(char16_t(' '));
          del->mValue.AppendToString(eCSSProperty_transition_delay, aValue,
                                     aSerialization);
          aValue.Append(char16_t(' '));
        } else {
          aValue.Truncate();
        }
      } else {
        MOZ_ASSERT(transProp->GetUnit() == eCSSUnit_List ||
                   transProp->GetUnit() == eCSSUnit_ListDep,
                   "bad t-prop unit");
        const nsCSSValueList* pro = transProp->GetListValue();
        for (;;) {
          pro->mValue.AppendToString(eCSSProperty_transition_property,
                                        aValue, aSerialization);
          aValue.Append(char16_t(' '));
          dur->mValue.AppendToString(eCSSProperty_transition_duration,
                                        aValue, aSerialization);
          aValue.Append(char16_t(' '));
          tim->mValue.AppendToString(eCSSProperty_transition_timing_function,
                                        aValue, aSerialization);
          aValue.Append(char16_t(' '));
          del->mValue.AppendToString(eCSSProperty_transition_delay,
                                        aValue, aSerialization);
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
      static const size_t numProps = 8;
      MOZ_ASSERT(subprops[numProps] == eCSSProperty_UNKNOWN,
                 "unexpected number of subproperties");
      const nsCSSValue* values[numProps];
      const nsCSSValueList* lists[numProps];

      for (uint32_t i = 0; i < numProps; ++i) {
        values[i] = data->ValueFor(subprops[i]);
        MOZ_ASSERT(values[i]->GetUnit() == eCSSUnit_List ||
                   values[i]->GetUnit() == eCSSUnit_ListDep,
                   "bad a-duration unit");
        lists[i] = values[i]->GetListValue();
      }

      for (;;) {
        // We must serialize 'animation-name' last in case it has
        // a value that conflicts with one of the other keyword properties.
        MOZ_ASSERT(subprops[numProps - 1] == eCSSProperty_animation_name,
                   "animation-name must be last");
        bool done = false;
        for (uint32_t i = 0;;) {
          lists[i]->mValue.AppendToString(subprops[i], aValue, aSerialization);
          lists[i] = lists[i]->mNext;
          if (!lists[i]) {
            done = true;
          }
          if (++i == numProps) {
            break;
          }
          aValue.Append(char16_t(' '));
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
        AppendValueToString(eCSSProperty_marker_end, aValue, aSerialization);
      break;
    }
    case eCSSProperty__moz_columns: {
      // Two values, column-count and column-width, separated by a space.
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      AppendValueToString(subprops[0], aValue, aSerialization);
      aValue.Append(char16_t(' '));
      AppendValueToString(subprops[1], aValue, aSerialization);
      break;
    }
    case eCSSProperty_flex: {
      // flex-grow, flex-shrink, flex-basis, separated by single space
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);

      AppendValueToString(subprops[0], aValue, aSerialization);
      aValue.Append(char16_t(' '));
      AppendValueToString(subprops[1], aValue, aSerialization);
      aValue.Append(char16_t(' '));
      AppendValueToString(subprops[2], aValue, aSerialization);
      break;
    }
    case eCSSProperty_flex_flow: {
      // flex-direction, flex-wrap, separated by single space
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      MOZ_ASSERT(subprops[2] == eCSSProperty_UNKNOWN,
                 "must have exactly two subproperties");

      AppendValueToString(subprops[0], aValue, aSerialization);
      aValue.Append(char16_t(' '));
      AppendValueToString(subprops[1], aValue, aSerialization);
      break;
    }
    case eCSSProperty_grid_row:
    case eCSSProperty_grid_column: {
      // grid-{row,column}-start, grid-{row,column}-end, separated by a slash
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      MOZ_ASSERT(subprops[2] == eCSSProperty_UNKNOWN,
                 "must have exactly two subproperties");

      // TODO: should we simplify when possible?
      AppendValueToString(subprops[0], aValue, aSerialization);
      aValue.AppendLiteral(" / ");
      AppendValueToString(subprops[1], aValue, aSerialization);
      break;
    }
    case eCSSProperty_grid_area: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      MOZ_ASSERT(subprops[4] == eCSSProperty_UNKNOWN,
                 "must have exactly four subproperties");

      // TODO: should we simplify when possible?
      AppendValueToString(subprops[0], aValue, aSerialization);
      aValue.AppendLiteral(" / ");
      AppendValueToString(subprops[1], aValue, aSerialization);
      aValue.AppendLiteral(" / ");
      AppendValueToString(subprops[2], aValue, aSerialization);
      aValue.AppendLiteral(" / ");
      AppendValueToString(subprops[3], aValue, aSerialization);
      break;
    }

    // This can express either grid-template-{areas,columns,rows}
    // or grid-auto-{flow,columns,rows}, but not both.
    case eCSSProperty_grid: {
      const nsCSSValue& areasValue =
        *data->ValueFor(eCSSProperty_grid_template_areas);
      const nsCSSValue& columnsValue =
        *data->ValueFor(eCSSProperty_grid_template_columns);
      const nsCSSValue& rowsValue =
        *data->ValueFor(eCSSProperty_grid_template_rows);

      const nsCSSValue& autoFlowValue =
        *data->ValueFor(eCSSProperty_grid_auto_flow);
      const nsCSSValue& autoColumnsValue =
        *data->ValueFor(eCSSProperty_grid_auto_columns);
      const nsCSSValue& autoRowsValue =
        *data->ValueFor(eCSSProperty_grid_auto_rows);

      if (areasValue.GetUnit() == eCSSUnit_None &&
          columnsValue.GetUnit() == eCSSUnit_None &&
          rowsValue.GetUnit() == eCSSUnit_None) {
        AppendValueToString(eCSSProperty_grid_auto_flow,
                            aValue, aSerialization);
        aValue.Append(char16_t(' '));
        AppendValueToString(eCSSProperty_grid_auto_columns,
                            aValue, aSerialization);
        aValue.AppendLiteral(" / ");
        AppendValueToString(eCSSProperty_grid_auto_rows,
                            aValue, aSerialization);
        break;
      } else if (!(autoFlowValue.GetUnit() == eCSSUnit_Enumerated &&
                   autoFlowValue.GetIntValue() == NS_STYLE_GRID_AUTO_FLOW_ROW &&
                   autoColumnsValue.GetUnit() == eCSSUnit_Auto &&
                   autoRowsValue.GetUnit() == eCSSUnit_Auto)) {
        // Not serializable, bail.
        return;
      }
      // Fall through to eCSSProperty_grid_template
    }
    case eCSSProperty_grid_template: {
      const nsCSSValue& areasValue =
        *data->ValueFor(eCSSProperty_grid_template_areas);
      const nsCSSValue& columnsValue =
        *data->ValueFor(eCSSProperty_grid_template_columns);
      const nsCSSValue& rowsValue =
        *data->ValueFor(eCSSProperty_grid_template_rows);
      if (areasValue.GetUnit() == eCSSUnit_None) {
        AppendValueToString(eCSSProperty_grid_template_columns,
                            aValue, aSerialization);
        aValue.AppendLiteral(" / ");
        AppendValueToString(eCSSProperty_grid_template_rows,
                            aValue, aSerialization);
        break;
      }
      if (columnsValue.GetUnit() == eCSSUnit_List ||
          columnsValue.GetUnit() == eCSSUnit_ListDep) {
        const nsCSSValueList* columnsItem = columnsValue.GetListValue();
        if (columnsItem->mValue.GetUnit() == eCSSUnit_Enumerated &&
            columnsItem->mValue.GetIntValue() == NS_STYLE_GRID_TEMPLATE_SUBGRID) {
          // We have "grid-template-areas:[something]; grid-template-columns:subgrid"
          // which isn't a value that the shorthand can express. Bail.
          return;
        }
      }
      if (rowsValue.GetUnit() != eCSSUnit_List &&
          rowsValue.GetUnit() != eCSSUnit_ListDep) {
        // We have "grid-template-areas:[something]; grid-template-rows:none"
        // which isn't a value that the shorthand can express. Bail.
        return;
      }
      const nsCSSValueList* rowsItem = rowsValue.GetListValue();
      if (rowsItem->mValue.GetUnit() == eCSSUnit_Enumerated &&
          rowsItem->mValue.GetIntValue() == NS_STYLE_GRID_TEMPLATE_SUBGRID) {
        // We have "grid-template-areas:[something]; grid-template-rows:subgrid"
        // which isn't a value that the shorthand can express. Bail.
        return;
      }
      const GridTemplateAreasValue* areas = areasValue.GetGridTemplateAreas();
      uint32_t nRowItems = 0;
      while (rowsItem) {
        nRowItems++;
        rowsItem = rowsItem->mNext;
      }
      MOZ_ASSERT(nRowItems % 2 == 1, "expected an odd number of items");
      if ((nRowItems - 1) / 2 != areas->NRows()) {
        // Not serializable, bail.
        return;
      }
      if (columnsValue.GetUnit() != eCSSUnit_None) {
        AppendValueToString(eCSSProperty_grid_template_columns,
                            aValue, aSerialization);
        aValue.AppendLiteral(" / ");
      }
      rowsItem = rowsValue.GetListValue();
      uint32_t row = 0;
      for (;;) {
        bool addSpaceSeparator = true;
        nsCSSUnit unit = rowsItem->mValue.GetUnit();

        if (unit == eCSSUnit_Null) {
          // Empty or omitted <line-names>. Serializes to nothing.
          addSpaceSeparator = false;  // Avoid a double space.

        } else if (unit == eCSSUnit_List || unit == eCSSUnit_ListDep) {
          // Non-empty <line-names>
          aValue.Append('(');
          rowsItem->mValue.AppendToString(eCSSProperty_grid_template_rows,
                                          aValue, aSerialization);
          aValue.Append(')');

        } else {
          nsStyleUtil::AppendEscapedCSSString(areas->mTemplates[row++], aValue);
          aValue.Append(char16_t(' '));

          // <track-size>
          rowsItem->mValue.AppendToString(eCSSProperty_grid_template_rows,
                                          aValue, aSerialization);
          if (rowsItem->mNext &&
              rowsItem->mNext->mValue.GetUnit() == eCSSUnit_Null &&
              !rowsItem->mNext->mNext) {
            // Break out of the loop early to avoid a trailing space.
            break;
          }
        }

        rowsItem = rowsItem->mNext;
        if (!rowsItem) {
          break;
        }

        if (addSpaceSeparator) {
          aValue.Append(char16_t(' '));
        }
      }
      break;
    }
    case eCSSProperty__moz_transform: {
      // shorthands that are just aliases with different parsing rules
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      MOZ_ASSERT(subprops[1] == eCSSProperty_UNKNOWN,
                 "must have exactly one subproperty");
      AppendValueToString(subprops[0], aValue, aSerialization);
      break;
    }
    case eCSSProperty_scroll_snap_type: {
      const nsCSSValue& xValue =
        *data->ValueFor(eCSSProperty_scroll_snap_type_x);
      const nsCSSValue& yValue =
        *data->ValueFor(eCSSProperty_scroll_snap_type_y);
      if (xValue == yValue) {
        AppendValueToString(eCSSProperty_scroll_snap_type_x, aValue,
                            aSerialization);
      }
      // If scroll-snap-type-x and scroll-snap-type-y are not equal,
      // we don't have a shorthand that can express. Bail.
      break;
    }
    case eCSSProperty_all:
      // If we got here, then we didn't have all "inherit" or "initial" or
      // "unset" values for all of the longhand property components of 'all'.
      // There is no other possible value that is valid for all properties,
      // so serialize as the empty string.
      break;
    default:
      MOZ_ASSERT(false, "no other shorthands");
      break;
  }
}

bool
Declaration::GetValueIsImportant(const nsAString& aProperty) const
{
  nsCSSProperty propID =
    nsCSSProps::LookupProperty(aProperty, nsCSSProps::eIgnoreEnabledState);
  if (propID == eCSSProperty_UNKNOWN) {
    return false;
  }
  if (propID == eCSSPropertyExtra_variable) {
    const nsSubstring& variableName =
      Substring(aProperty, CSS_CUSTOM_NAME_PREFIX_LENGTH);
    return GetVariableValueIsImportant(variableName);
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

  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty,
                                       nsCSSProps::eEnabledForAllContent) {
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
  MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
             "property enum out of range");
  MOZ_ASSERT((aProperty < eCSSProperty_COUNT_no_shorthands) == aValue.IsEmpty(),
             "aValue should be given for shorthands but not longhands");
  AppendASCIItoUTF16(nsCSSProps::GetStringValue(aProperty), aResult);
  aResult.AppendLiteral(": ");
  if (aValue.IsEmpty())
    AppendValueToString(aProperty, aResult, nsCSSValue::eNormalized);
  else
    aResult.Append(aValue);
  if (GetValueIsImportant(aProperty)) {
    aResult.AppendLiteral(" ! important");
  }
  aResult.AppendLiteral("; ");
}

void
Declaration::AppendVariableAndValueToString(const nsAString& aName,
                                            nsAString& aResult) const
{
  aResult.AppendLiteral("--");
  aResult.Append(aName);
  CSSVariableDeclarations::Type type;
  nsString value;
  bool important;

  if (mImportantVariables && mImportantVariables->Get(aName, type, value)) {
    important = true;
  } else {
    MOZ_ASSERT(mVariables);
    MOZ_ASSERT(mVariables->Has(aName));
    mVariables->Get(aName, type, value);
    important = false;
  }

  switch (type) {
    case CSSVariableDeclarations::eTokenStream:
      if (value.IsEmpty()) {
        aResult.Append(':');
      } else {
        aResult.AppendLiteral(": ");
        aResult.Append(value);
      }
      break;

    case CSSVariableDeclarations::eInitial:
      aResult.AppendLiteral("initial");
      break;

    case CSSVariableDeclarations::eInherit:
      aResult.AppendLiteral("inherit");
      break;

    case CSSVariableDeclarations::eUnset:
      aResult.AppendLiteral("unset");
      break;

    default:
      MOZ_ASSERT(false, "unexpected variable value type");
  }

  if (important) {
    aResult.AppendLiteral("! important");
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
    nsCSSProperty property = GetPropertyAt(index);

    if (property == eCSSPropertyExtra_variable) {
      uint32_t variableIndex = mOrder[index] - eCSSProperty_COUNT;
      AppendVariableAndValueToString(mVariableOrder[variableIndex], aString);
      continue;
    }

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

      GetValue(shorthand, value);

      // in the system font case, skip over font-variant shorthand, since all
      // subproperties are already dealt with via the font shorthand
      if (shorthand == eCSSProperty_font_variant &&
          value.EqualsLiteral("-moz-use-system-font")) {
        continue;
      }

      // If GetValue gives us a non-empty string back, we can use that
      // value; otherwise it's not possible to use this shorthand.
      if (!value.IsEmpty()) {
        AppendPropertyAndValueToString(shorthand, value, aString);
        shorthandsUsed.AppendElement(shorthand);
        doneProperty = true;
        break;
      }

      if (shorthand == eCSSProperty_font) {
        if (haveSystemFont && !didSystemFont) {
          // Output the shorthand font declaration that we will
          // partially override later.  But don't add it to
          // |shorthandsUsed|, since we will have to override it.
          systemFont->AppendToString(eCSSProperty__x_system_font, value,
                                     nsCSSValue::eNormalized);
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
          break;
        }
      }
    }
    if (doneProperty)
      continue;

    MOZ_ASSERT(value.IsEmpty(), "value should be empty now");
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
  nsAutoCString str;
  for (int32_t index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("{ ");
  nsAutoString s;
  ToString(s);
  AppendUTF16toUTF8(s, str);
  str.AppendLiteral("}\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

bool
Declaration::GetNthProperty(uint32_t aIndex, nsAString& aReturn) const
{
  aReturn.Truncate();
  if (aIndex < mOrder.Length()) {
    nsCSSProperty property = GetPropertyAt(aIndex);
    if (property == eCSSPropertyExtra_variable) {
      GetCustomPropertyNameAt(aIndex, aReturn);
      return true;
    }
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
  MOZ_ASSERT(!mData && !mImportantData, "already initialized");
  mData = nsCSSCompressedDataBlock::CreateEmptyBlock();
}

Declaration*
Declaration::EnsureMutable()
{
  MOZ_ASSERT(mData, "should only be called when not expanded");
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
  if (mVariables) {
    n += mVariables->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mImportantVariables) {
    n += mImportantVariables->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

bool
Declaration::HasVariableDeclaration(const nsAString& aName) const
{
  return (mVariables && mVariables->Has(aName)) ||
         (mImportantVariables && mImportantVariables->Has(aName));
}

void
Declaration::GetVariableDeclaration(const nsAString& aName,
                                    nsAString& aValue) const
{
  aValue.Truncate();

  CSSVariableDeclarations::Type type;
  nsString value;

  if ((mImportantVariables && mImportantVariables->Get(aName, type, value)) ||
      (mVariables && mVariables->Get(aName, type, value))) {
    switch (type) {
      case CSSVariableDeclarations::eTokenStream:
        aValue.Append(value);
        break;

      case CSSVariableDeclarations::eInitial:
        aValue.AppendLiteral("initial");
        break;

      case CSSVariableDeclarations::eInherit:
        aValue.AppendLiteral("inherit");
        break;

      case CSSVariableDeclarations::eUnset:
        aValue.AppendLiteral("unset");
        break;

      default:
        MOZ_ASSERT(false, "unexpected variable value type");
    }
  }
}

void
Declaration::AddVariableDeclaration(const nsAString& aName,
                                    CSSVariableDeclarations::Type aType,
                                    const nsString& aValue,
                                    bool aIsImportant,
                                    bool aOverrideImportant)
{
  MOZ_ASSERT(IsMutable());

  nsTArray<nsString>::index_type index = mVariableOrder.IndexOf(aName);
  if (index == nsTArray<nsString>::NoIndex) {
    index = mVariableOrder.Length();
    mVariableOrder.AppendElement(aName);
  }

  if (!aIsImportant && !aOverrideImportant &&
      mImportantVariables && mImportantVariables->Has(aName)) {
    return;
  }

  CSSVariableDeclarations* variables;
  if (aIsImportant) {
    if (mVariables) {
      mVariables->Remove(aName);
    }
    if (!mImportantVariables) {
      mImportantVariables = new CSSVariableDeclarations;
    }
    variables = mImportantVariables;
  } else {
    if (mImportantVariables) {
      mImportantVariables->Remove(aName);
    }
    if (!mVariables) {
      mVariables = new CSSVariableDeclarations;
    }
    variables = mVariables;
  }

  switch (aType) {
    case CSSVariableDeclarations::eTokenStream:
      variables->PutTokenStream(aName, aValue);
      break;

    case CSSVariableDeclarations::eInitial:
      MOZ_ASSERT(aValue.IsEmpty());
      variables->PutInitial(aName);
      break;

    case CSSVariableDeclarations::eInherit:
      MOZ_ASSERT(aValue.IsEmpty());
      variables->PutInherit(aName);
      break;

    case CSSVariableDeclarations::eUnset:
      MOZ_ASSERT(aValue.IsEmpty());
      variables->PutUnset(aName);
      break;

    default:
      MOZ_ASSERT(false, "unexpected aType value");
  }

  uint32_t propertyIndex = index + eCSSProperty_COUNT;
  mOrder.RemoveElement(propertyIndex);
  mOrder.AppendElement(propertyIndex);
}

void
Declaration::RemoveVariableDeclaration(const nsAString& aName)
{
  if (mVariables) {
    mVariables->Remove(aName);
  }
  if (mImportantVariables) {
    mImportantVariables->Remove(aName);
  }
  nsTArray<nsString>::index_type index = mVariableOrder.IndexOf(aName);
  if (index != nsTArray<nsString>::NoIndex) {
    mOrder.RemoveElement(index + eCSSProperty_COUNT);
  }
}

bool
Declaration::GetVariableValueIsImportant(const nsAString& aName) const
{
  return mImportantVariables && mImportantVariables->Has(aName);
}

} // namespace mozilla::css
} // namespace mozilla
