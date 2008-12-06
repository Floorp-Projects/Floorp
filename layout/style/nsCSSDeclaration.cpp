/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * representation of a declaration block (or style attribute) in a CSS
 * stylesheet
 */

#include "nscore.h"
#include "prlog.h"
#include "nsCSSDeclaration.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsFont.h"
#include "nsReadableUtils.h"
#include "nsStyleUtil.h"

#include "nsStyleConsts.h"

#include "nsCOMPtr.h"

#define B_BORDER_TOP_STYLE    0x001
#define B_BORDER_LEFT_STYLE   0x002
#define B_BORDER_RIGHT_STYLE  0x004
#define B_BORDER_BOTTOM_STYLE 0x008
#define B_BORDER_TOP_COLOR    0x010
#define B_BORDER_LEFT_COLOR   0x020
#define B_BORDER_RIGHT_COLOR  0x040
#define B_BORDER_BOTTOM_COLOR 0x080
#define B_BORDER_TOP_WIDTH    0x100
#define B_BORDER_LEFT_WIDTH   0x200
#define B_BORDER_RIGHT_WIDTH  0x400
#define B_BORDER_BOTTOM_WIDTH 0x800

#define B_BORDER_STYLE        0x00f
#define B_BORDER_COLOR        0x0f0
#define B_BORDER_WIDTH        0xf00

#define B_BORDER_TOP          0x111
#define B_BORDER_LEFT         0x222
#define B_BORDER_RIGHT        0x444
#define B_BORDER_BOTTOM       0x888

#define B_BORDER              0xfff

nsCSSDeclaration::nsCSSDeclaration() 
  : mData(nsnull),
    mImportantData(nsnull)
{
  // check that we can fit all the CSS properties into a PRUint8
  // for the mOrder array - if not, might need to use PRUint16!
  PR_STATIC_ASSERT(eCSSProperty_COUNT_no_shorthands - 1 <= PR_UINT8_MAX);

  MOZ_COUNT_CTOR(nsCSSDeclaration);
}

nsCSSDeclaration::nsCSSDeclaration(const nsCSSDeclaration& aCopy)
  : mOrder(aCopy.mOrder),
    mData(aCopy.mData ? aCopy.mData->Clone() : nsnull),
    mImportantData(aCopy.mImportantData ? aCopy.mImportantData->Clone()
                                         : nsnull)
{
  MOZ_COUNT_CTOR(nsCSSDeclaration);
}

nsCSSDeclaration::~nsCSSDeclaration(void)
{
  if (mData) {
    mData->Destroy();
  }
  if (mImportantData) {
    mImportantData->Destroy();
  }

  MOZ_COUNT_DTOR(nsCSSDeclaration);
}

nsresult
nsCSSDeclaration::ValueAppended(nsCSSProperty aProperty)
{
  // order IS important for CSS, so remove and add to the end
  if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
      mOrder.RemoveElement(*p);
      mOrder.AppendElement(*p);
    }
  } else {
    mOrder.RemoveElement(aProperty);
    mOrder.AppendElement(aProperty);
  }
  return NS_OK;
}

nsresult
nsCSSDeclaration::RemoveProperty(nsCSSProperty aProperty)
{
  nsCSSExpandedDataBlock data;
  data.Expand(&mData, &mImportantData);
  NS_ASSERTION(!mData && !mImportantData, "Expand didn't null things out");

  if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
      data.ClearProperty(*p);
      mOrder.RemoveElement(*p);
    }
  } else {
    data.ClearProperty(aProperty);
    mOrder.RemoveElement(aProperty);
  }

  data.Compress(&mData, &mImportantData);
  return NS_OK;
}

nsresult
nsCSSDeclaration::AppendComment(const nsAString& aComment)
{
  return /* NS_ERROR_NOT_IMPLEMENTED, or not any longer that is */ NS_OK;
}

nsresult
nsCSSDeclaration::GetValueOrImportantValue(nsCSSProperty aProperty, nsCSSValue& aValue) const
{
  aValue.Reset();

  NS_ASSERTION(aProperty >= 0, "out of range");
  if (aProperty >= eCSSProperty_COUNT_no_shorthands ||
      nsCSSProps::kTypeTable[aProperty] != eCSSType_Value) {
    NS_ERROR("can't query for shorthand properties");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                     ? mImportantData : mData;
  const void *storage = data->StorageFor(aProperty);
  if (!storage)
    return NS_OK;
  aValue = *static_cast<const nsCSSValue*>(storage);
  return NS_OK;
}

PRBool nsCSSDeclaration::AppendValueToString(nsCSSProperty aProperty, nsAString& aResult) const
{
  nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                      ? mImportantData : mData;
  const void *storage = data->StorageFor(aProperty);
  if (storage) {
    switch (nsCSSProps::kTypeTable[aProperty]) {
      case eCSSType_Value: {
        const nsCSSValue *val = static_cast<const nsCSSValue*>(storage);
        AppendCSSValueToString(aProperty, *val, aResult);
      } break;
      case eCSSType_Rect: {
        const nsCSSRect *rect = static_cast<const nsCSSRect*>(storage);
        if (rect->mTop.GetUnit() == eCSSUnit_Inherit ||
            rect->mTop.GetUnit() == eCSSUnit_Initial) {
          NS_ASSERTION(rect->mRight.GetUnit() == rect->mTop.GetUnit(),
                       "Top inherit or initial, right isn't.  Fix the parser!");
          NS_ASSERTION(rect->mBottom.GetUnit() == rect->mTop.GetUnit(),
                       "Top inherit or initial, bottom isn't.  Fix the parser!");
          NS_ASSERTION(rect->mLeft.GetUnit() == rect->mTop.GetUnit(),
                       "Top inherit or initial, left isn't.  Fix the parser!");
          AppendCSSValueToString(aProperty, rect->mTop, aResult);
        } else {
          aResult.AppendLiteral("rect(");
          AppendCSSValueToString(aProperty, rect->mTop, aResult);
          NS_NAMED_LITERAL_STRING(comma, ", ");
          aResult.Append(comma);
          AppendCSSValueToString(aProperty, rect->mRight, aResult);
          aResult.Append(comma);
          AppendCSSValueToString(aProperty, rect->mBottom, aResult);
          aResult.Append(comma);
          AppendCSSValueToString(aProperty, rect->mLeft, aResult);
          aResult.Append(PRUnichar(')'));
        }
      } break;
      case eCSSType_ValuePair: {
        const nsCSSValuePair *pair = static_cast<const nsCSSValuePair*>(storage);
        AppendCSSValueToString(aProperty, pair->mXValue, aResult);
        if (pair->mYValue != pair->mXValue ||
            ((aProperty == eCSSProperty_background_position ||
              aProperty == eCSSProperty__moz_transform_origin) &&
             pair->mXValue.GetUnit() != eCSSUnit_Inherit &&
             pair->mXValue.GetUnit() != eCSSUnit_Initial)) {
          // Only output a Y value if it's different from the X value
          // or if it's a background-position value other than 'initial'
          // or 'inherit' or if it's a -moz-transform-origin value other
          // than 'initial' or 'inherit'.
          aResult.Append(PRUnichar(' '));
          AppendCSSValueToString(aProperty, pair->mYValue, aResult);
        }
      } break;
      case eCSSType_ValueList: {
        const nsCSSValueList* val =
            *static_cast<nsCSSValueList*const*>(storage);
        do {
          AppendCSSValueToString(aProperty, val->mValue, aResult);
          val = val->mNext;
          if (val) {
            if (nsCSSProps::PropHasFlags(aProperty,
                                         CSS_PROPERTY_VALUE_LIST_USES_COMMAS))
              aResult.Append(PRUnichar(','));
            aResult.Append(PRUnichar(' '));
          }
        } while (val);
      } break;
      case eCSSType_ValuePairList: {
        const nsCSSValuePairList* item =
            *static_cast<nsCSSValuePairList*const*>(storage);
        do {
          NS_ASSERTION(item->mXValue.GetUnit() != eCSSUnit_Null,
                       "unexpected null unit");
          AppendCSSValueToString(aProperty, item->mXValue, aResult);
          if (item->mYValue.GetUnit() != eCSSUnit_Null) {
            aResult.Append(PRUnichar(' '));
            AppendCSSValueToString(aProperty, item->mYValue, aResult);
          }
          item = item->mNext;
          if (item) {
            aResult.Append(PRUnichar(' '));
          }
        } while (item);
      } break;
    }
  }
  return storage != nsnull;
}

/* static */ PRBool
nsCSSDeclaration::AppendCSSValueToString(nsCSSProperty aProperty,
                                         const nsCSSValue& aValue,
                                         nsAString& aResult)
{
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Null == unit) {
    return PR_FALSE;
  }

  if (eCSSUnit_String <= unit && unit <= eCSSUnit_Attr) {
    if (unit == eCSSUnit_Attr) {
      aResult.AppendLiteral("attr(");
    }
    nsAutoString  buffer;
    aValue.GetStringValue(buffer);
    aResult.Append(buffer);
  }
  else if (eCSSUnit_Array <= unit && unit <= eCSSUnit_Counters) {
    switch (unit) {
      case eCSSUnit_Counter:  aResult.AppendLiteral("counter(");  break;
      case eCSSUnit_Counters: aResult.AppendLiteral("counters("); break;
      default: break;
    }

    nsCSSValue::Array *array = aValue.GetArrayValue();
    PRBool mark = PR_FALSE;
    for (PRUint16 i = 0, i_end = array->Count(); i < i_end; ++i) {
      if (aProperty == eCSSProperty_border_image && i >= 5) {
        if (array->Item(i).GetUnit() == eCSSUnit_Null) {
          continue;
        }
        if (i == 5) {
          aResult.AppendLiteral(" /");
        }
      }
      if (mark && array->Item(i).GetUnit() != eCSSUnit_Null) {
        if (unit == eCSSUnit_Array)
          aResult.AppendLiteral(" ");
        else
          aResult.AppendLiteral(", ");
      }
      nsCSSProperty prop =
        ((eCSSUnit_Counter <= unit && unit <= eCSSUnit_Counters) &&
         i == array->Count() - 1)
        ? eCSSProperty_list_style_type : aProperty;
      if (AppendCSSValueToString(prop, array->Item(i), aResult)) {
        mark = PR_TRUE;
      }
    }
  }
  /* Although Function is backed by an Array, we'll handle it separately
   * because it's a bit quirky.
   */
  else if (eCSSUnit_Function == unit) {
    const nsCSSValue::Array* array = aValue.GetArrayValue();
    NS_ASSERTION(array->Count() >= 1, "Functions must have at least one element for the name.");

    /* Append the function name. */
    AppendCSSValueToString(aProperty, array->Item(0), aResult);
    aResult.AppendLiteral("(");

    /* Now, step through the function contents, writing each of them as we go. */
    for (PRUint16 index = 1; index < array->Count(); ++index) {
      AppendCSSValueToString(aProperty, array->Item(index), aResult);

      /* If we're not at the final element, append a comma. */
      if (index + 1 != array->Count())
        aResult.AppendLiteral(", ");
    }

    /* Finally, append the closing parenthesis. */
    aResult.AppendLiteral(")");
  }
  else if (eCSSUnit_Integer == unit) {
    nsAutoString tmpStr;
    tmpStr.AppendInt(aValue.GetIntValue(), 10);
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Enumerated == unit) {
    if (eCSSProperty_text_decoration == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if (NS_STYLE_TEXT_DECORATION_NONE != intValue) {
        PRInt32 mask;
        for (mask = NS_STYLE_TEXT_DECORATION_UNDERLINE;
             mask <= NS_STYLE_TEXT_DECORATION_BLINK; 
             mask <<= 1) {
          if ((mask & intValue) == mask) {
            AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, mask), aResult);
            intValue &= ~mask;
            if (0 != intValue) { // more left
              aResult.Append(PRUnichar(' '));
            }
          }
        }
      }
      else {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_TEXT_DECORATION_NONE), aResult);
      }
    }
    else if (eCSSProperty_azimuth == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, (intValue & ~NS_STYLE_AZIMUTH_BEHIND)), aResult);
      if ((NS_STYLE_AZIMUTH_BEHIND & intValue) != 0) {
        aResult.Append(PRUnichar(' '));
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_AZIMUTH_BEHIND), aResult);
      }
    }
    else if (eCSSProperty_marks == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PAGE_MARKS_CROP), aResult);
      }
      if ((NS_STYLE_PAGE_MARKS_REGISTER & intValue) != 0) {
        if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
          aResult.Append(PRUnichar(' '));
        }
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PAGE_MARKS_REGISTER), aResult);
      }
    }
    else {
      const nsAFlatCString& name = nsCSSProps::LookupPropertyValue(aProperty, aValue.GetIntValue());
      AppendASCIItoUTF16(name, aResult);
    }
  }
  else if (eCSSUnit_EnumColor == unit) {
    // we can lookup the property in the ColorTable and then
    // get a string mapping the name
    nsCAutoString str;
    if (nsCSSProps::GetColorName(aValue.GetIntValue(), str)){
      AppendASCIItoUTF16(str, aResult);
    } else {
      NS_NOTREACHED("bad color value");
    }
  }
  else if (eCSSUnit_Color == unit) {
    nscolor color = aValue.GetColorValue();
    if (color == NS_RGBA(0, 0, 0, 0)) {
      // Use the strictest match for 'transparent' so we do correct
      // round-tripping of all other rgba() values.
      aResult.AppendLiteral("transparent");
    } else {
      nsAutoString tmpStr;
      PRUint8 a = NS_GET_A(color);
      if (a < 255) {
        tmpStr.AppendLiteral("rgba(");
      } else {
        tmpStr.AppendLiteral("rgb(");
      }

      NS_NAMED_LITERAL_STRING(comma, ", ");

      tmpStr.AppendInt(NS_GET_R(color), 10);
      tmpStr.Append(comma);
      tmpStr.AppendInt(NS_GET_G(color), 10);
      tmpStr.Append(comma);
      tmpStr.AppendInt(NS_GET_B(color), 10);
      if (a < 255) {
        tmpStr.Append(comma);
        tmpStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(a));
      }
      tmpStr.Append(PRUnichar(')'));

      aResult.Append(tmpStr);
    }
  }
  else if (eCSSUnit_URL == unit || eCSSUnit_Image == unit) {
    aResult.Append(NS_LITERAL_STRING("url(") +
                   nsDependentString(aValue.GetOriginalURLValue()) +
                   NS_LITERAL_STRING(")"));
  }
  else if (eCSSUnit_Percent == unit) {
    nsAutoString tmpStr;
    tmpStr.AppendFloat(aValue.GetPercentValue() * 100.0f);
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    nsAutoString tmpStr;
    tmpStr.AppendFloat(aValue.GetFloatValue());
    aResult.Append(tmpStr);
  }

  switch (unit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aResult.AppendLiteral("auto");     break;
    case eCSSUnit_Inherit:      aResult.AppendLiteral("inherit");  break;
    case eCSSUnit_Initial:      aResult.AppendLiteral("-moz-initial"); break;
    case eCSSUnit_None:         aResult.AppendLiteral("none");     break;
    case eCSSUnit_Normal:       aResult.AppendLiteral("normal");   break;
    case eCSSUnit_System_Font:  aResult.AppendLiteral("-moz-use-system-font"); break;
    case eCSSUnit_Dummy:        break;

    case eCSSUnit_String:       break;
    case eCSSUnit_URL:          break;
    case eCSSUnit_Image:        break;
    case eCSSUnit_Array:        break;
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(PRUnichar(')'));    break;
    case eCSSUnit_Local_Font:   break;
    case eCSSUnit_Font_Format:  break;
    case eCSSUnit_Function:     break;
    case eCSSUnit_Integer:      break;
    case eCSSUnit_Enumerated:   break;
    case eCSSUnit_EnumColor:    break;
    case eCSSUnit_Color:        break;
    case eCSSUnit_Percent:      aResult.Append(PRUnichar('%'));    break;
    case eCSSUnit_Number:       break;

    case eCSSUnit_Inch:         aResult.AppendLiteral("in");   break;
    case eCSSUnit_Foot:         aResult.AppendLiteral("ft");   break;
    case eCSSUnit_Mile:         aResult.AppendLiteral("mi");   break;
    case eCSSUnit_Millimeter:   aResult.AppendLiteral("mm");   break;
    case eCSSUnit_Centimeter:   aResult.AppendLiteral("cm");   break;
    case eCSSUnit_Meter:        aResult.AppendLiteral("m");    break;
    case eCSSUnit_Kilometer:    aResult.AppendLiteral("km");   break;
    case eCSSUnit_Point:        aResult.AppendLiteral("pt");   break;
    case eCSSUnit_Pica:         aResult.AppendLiteral("pc");   break;
    case eCSSUnit_Didot:        aResult.AppendLiteral("dt");   break;
    case eCSSUnit_Cicero:       aResult.AppendLiteral("cc");   break;

    case eCSSUnit_EM:           aResult.AppendLiteral("em");   break;
    case eCSSUnit_XHeight:      aResult.AppendLiteral("ex");   break;
    case eCSSUnit_Char:         aResult.AppendLiteral("ch");   break;

    case eCSSUnit_Pixel:        aResult.AppendLiteral("px");   break;

    case eCSSUnit_Degree:       aResult.AppendLiteral("deg");  break;
    case eCSSUnit_Grad:         aResult.AppendLiteral("grad"); break;
    case eCSSUnit_Radian:       aResult.AppendLiteral("rad");  break;

    case eCSSUnit_Hertz:        aResult.AppendLiteral("Hz");   break;
    case eCSSUnit_Kilohertz:    aResult.AppendLiteral("kHz");  break;

    case eCSSUnit_Seconds:      aResult.Append(PRUnichar('s'));    break;
    case eCSSUnit_Milliseconds: aResult.AppendLiteral("ms");   break;
  }

  return PR_TRUE;
}

nsresult
nsCSSDeclaration::GetValue(nsCSSProperty aProperty,
                           nsAString& aValue) const
{
  aValue.Truncate(0);

  // simple properties are easy.
  if (!nsCSSProps::IsShorthand(aProperty)) {
    AppendValueToString(aProperty, aValue);
    return NS_OK;
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
  PRUint32 totalCount = 0, importantCount = 0,
           initialCount = 0, inheritCount = 0;
  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
    if (*p == eCSSProperty__x_system_font ||
         nsCSSProps::PropHasFlags(*p, CSS_PROPERTY_DIRECTIONAL_SOURCE)) {
      // The system-font subproperty and the *-source properties don't count.
      continue;
    }
    ++totalCount;
    const void *storage = mData->StorageFor(*p);
    NS_ASSERTION(!storage || !mImportantData || !mImportantData->StorageFor(*p),
                 "can't be in both blocks");
    if (!storage && mImportantData) {
      ++importantCount;
      storage = mImportantData->StorageFor(*p);
    }
    if (!storage) {
      // Case (1) above: some subproperties not specified.
      return NS_OK;
    }
    nsCSSUnit unit;
    switch (nsCSSProps::kTypeTable[*p]) {
      case eCSSType_Value: {
        const nsCSSValue *val = static_cast<const nsCSSValue*>(storage);
        unit = val->GetUnit();
      } break;
      case eCSSType_Rect: {
        const nsCSSRect *rect = static_cast<const nsCSSRect*>(storage);
        unit = rect->mTop.GetUnit();
      } break;
      case eCSSType_ValuePair: {
        const nsCSSValuePair *pair = static_cast<const nsCSSValuePair*>(storage);
        unit = pair->mXValue.GetUnit();
      } break;
      case eCSSType_ValueList: {
        const nsCSSValueList* item =
            *static_cast<nsCSSValueList*const*>(storage);
        if (item) {
          unit = item->mValue.GetUnit();
        } else {
          unit = eCSSUnit_Null;
        }
      } break;
      case eCSSType_ValuePairList: {
        const nsCSSValuePairList* item =
            *static_cast<nsCSSValuePairList*const*>(storage);
        if (item) {
          unit = item->mXValue.GetUnit();
        } else {
          unit = eCSSUnit_Null;
        }
      } break;
    }
    if (unit == eCSSUnit_Inherit) {
      ++inheritCount;
    } else if (unit == eCSSUnit_Initial) {
      ++initialCount;
    }
  }
  if (importantCount != 0 && importantCount != totalCount) {
    // Case (3), no consistent importance.
    return NS_OK;
  }
  if (initialCount == totalCount) {
    // Simplify serialization below by serializing initial up-front.
    AppendCSSValueToString(eCSSProperty_UNKNOWN, nsCSSValue(eCSSUnit_Initial),
                           aValue);
    return NS_OK;
  }
  if (inheritCount == totalCount) {
    // Simplify serialization below by serializing inherit up-front.
    AppendCSSValueToString(eCSSProperty_UNKNOWN, nsCSSValue(eCSSUnit_Inherit),
                           aValue);
    return NS_OK;
  }
  if (initialCount != 0 || inheritCount != 0) {
    // Case (2): partially initial or inherit.
    return NS_OK;
  }


  // XXX What about checking the consistency of '!important'?
  // XXX What about checking that we don't serialize inherit,
  // -moz-initial, or other illegal values?
  // XXXldb Can we share shorthand logic with ToString?
  switch (aProperty) {
    case eCSSProperty_margin: 
    case eCSSProperty_padding: 
    case eCSSProperty_border_color: 
    case eCSSProperty_border_style: 
    case eCSSProperty_border_width: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ASSERTION(nsCSSProps::kTypeTable[subprops[0]] == eCSSType_Value &&
                   nsCSSProps::kTypeTable[subprops[1]] == eCSSType_Value &&
                   nsCSSProps::kTypeTable[subprops[2]] == eCSSType_Value &&
                   nsCSSProps::kTypeTable[subprops[3]] == eCSSType_Value,
                   "type mismatch");
      if (!AppendValueToString(subprops[0], aValue) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[1], aValue)) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[2], aValue)) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[3], aValue))) {
        aValue.Truncate();
      }
      break;
    }
    case eCSSProperty__moz_border_radius: 
    case eCSSProperty__moz_outline_radius: {
      const nsCSSProperty* subprops =
        nsCSSProps::SubpropertyEntryFor(aProperty);
      NS_ASSERTION(nsCSSProps::kTypeTable[subprops[0]] == eCSSType_ValuePair &&
                   nsCSSProps::kTypeTable[subprops[1]] == eCSSType_ValuePair &&
                   nsCSSProps::kTypeTable[subprops[2]] == eCSSType_ValuePair &&
                   nsCSSProps::kTypeTable[subprops[3]] == eCSSType_ValuePair,
                   "type mismatch");
      nsCSSCompressedDataBlock *data = GetValueIsImportant(aProperty)
                                     ? mImportantData : mData;
      const nsCSSValuePair* vals[4] = {
        static_cast<const nsCSSValuePair*>(data->StorageFor(subprops[0])),
        static_cast<const nsCSSValuePair*>(data->StorageFor(subprops[1])),
        static_cast<const nsCSSValuePair*>(data->StorageFor(subprops[2])),
        static_cast<const nsCSSValuePair*>(data->StorageFor(subprops[3]))
      };

      AppendCSSValueToString(aProperty, vals[0]->mXValue, aValue);
      aValue.Append(PRUnichar(' '));
      AppendCSSValueToString(aProperty, vals[1]->mXValue, aValue);
      aValue.Append(PRUnichar(' '));
      AppendCSSValueToString(aProperty, vals[2]->mXValue, aValue);
      aValue.Append(PRUnichar(' '));
      AppendCSSValueToString(aProperty, vals[3]->mXValue, aValue);
        
      // For compatibility, only write a slash and the y-values
      // if they're not identical to the x-values.
      if (vals[0]->mXValue != vals[0]->mYValue ||
          vals[1]->mXValue != vals[1]->mYValue ||
          vals[2]->mXValue != vals[2]->mYValue ||
          vals[3]->mXValue != vals[3]->mYValue) {
        aValue.AppendLiteral(" / ");
        AppendCSSValueToString(aProperty, vals[0]->mYValue, aValue);
        aValue.Append(PRUnichar(' '));
        AppendCSSValueToString(aProperty, vals[1]->mYValue, aValue);
        aValue.Append(PRUnichar(' '));
        AppendCSSValueToString(aProperty, vals[2]->mYValue, aValue);
        aValue.Append(PRUnichar(' '));
        AppendCSSValueToString(aProperty, vals[3]->mYValue, aValue);
      }
      break;
    }
    case eCSSProperty_border:
      // XXX More consistency checking needed before falling through.
      aProperty = eCSSProperty_border_top;
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
      NS_ASSERTION(nsCSSProps::kTypeTable[subprops[0]] == eCSSType_Value &&
                   nsCSSProps::kTypeTable[subprops[1]] == eCSSType_Value &&
                   nsCSSProps::kTypeTable[subprops[2]] == eCSSType_Value,
                   "type mismatch");
      if (!AppendValueToString(subprops[0], aValue) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[1], aValue)) ||
          !(aValue.Append(PRUnichar(' ')),
            AppendValueToString(subprops[2], aValue))) {
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
      NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
                   "not box property with physical vs. logical cascading");
      AppendValueToString(subprops[0], aValue);
      break;
    }
    case eCSSProperty_background: {
      PRBool appendedSomething = PR_FALSE;
      if (AppendValueToString(eCSSProperty_background_color, aValue)) {
        appendedSomething = PR_TRUE;
        aValue.Append(PRUnichar(' '));
      }
      if (AppendValueToString(eCSSProperty_background_image, aValue)) {
        aValue.Append(PRUnichar(' '));
        appendedSomething = PR_TRUE;
      }
      if (AppendValueToString(eCSSProperty_background_repeat, aValue)) {
        aValue.Append(PRUnichar(' '));
        appendedSomething = PR_TRUE;
      }
      if (AppendValueToString(eCSSProperty_background_attachment, aValue)) {
        aValue.Append(PRUnichar(' '));
        appendedSomething = PR_TRUE;
      }
      if (!AppendValueToString(eCSSProperty_background_position, aValue) &&
          appendedSomething) {
        NS_ASSERTION(!aValue.IsEmpty() && aValue.Last() == PRUnichar(' '),
                     "We appended a space before!");
        // We appended an extra space.  Let's get rid of it
        aValue.Truncate(aValue.Length() - 1);
      }
      break;
    }
    case eCSSProperty_cue: {
      if (AppendValueToString(eCSSProperty_cue_before, aValue)) {
        aValue.Append(PRUnichar(' '));
        if (!AppendValueToString(eCSSProperty_cue_after, aValue))
          aValue.Truncate();
      }
      break;
    }
    case eCSSProperty_font: {
      nsCSSValue style, variant, weight, size, lh, family, systemFont;
      GetValueOrImportantValue(eCSSProperty__x_system_font, systemFont);
      GetValueOrImportantValue(eCSSProperty_font_style, style);
      GetValueOrImportantValue(eCSSProperty_font_variant, variant);
      GetValueOrImportantValue(eCSSProperty_font_weight, weight);
      GetValueOrImportantValue(eCSSProperty_font_size, size);
      GetValueOrImportantValue(eCSSProperty_line_height, lh);
      GetValueOrImportantValue(eCSSProperty_font_family, family);

      if (systemFont.GetUnit() != eCSSUnit_None &&
          systemFont.GetUnit() != eCSSUnit_Null) {
        AppendCSSValueToString(eCSSProperty__x_system_font, systemFont, aValue);
      } else {
        if (style.GetUnit() != eCSSUnit_Normal) {
          AppendCSSValueToString(eCSSProperty_font_style, style, aValue);
          aValue.Append(PRUnichar(' '));
        }
        if (variant.GetUnit() != eCSSUnit_Normal) {
          AppendCSSValueToString(eCSSProperty_font_variant, variant, aValue);
          aValue.Append(PRUnichar(' '));
        }
        if (weight.GetUnit() != eCSSUnit_Normal) {
          AppendCSSValueToString(eCSSProperty_font_weight, weight, aValue);
          aValue.Append(PRUnichar(' '));
        }
        AppendCSSValueToString(eCSSProperty_font_size, size, aValue);
        if (lh.GetUnit() != eCSSUnit_Normal) {
          aValue.Append(PRUnichar('/'));
          AppendCSSValueToString(eCSSProperty_line_height, lh, aValue);
        }
        aValue.Append(PRUnichar(' '));
        AppendCSSValueToString(eCSSProperty_font_family, family, aValue);
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
      nsCSSValue xValue, yValue;
      GetValueOrImportantValue(eCSSProperty_overflow_x, xValue);
      GetValueOrImportantValue(eCSSProperty_overflow_y, yValue);
      if (xValue == yValue)
        AppendValueToString(eCSSProperty_overflow_x, aValue);
      break;
    }
    case eCSSProperty_pause: {
      if (AppendValueToString(eCSSProperty_pause_before, aValue)) {
        aValue.Append(PRUnichar(' '));
        if (!AppendValueToString(eCSSProperty_pause_after, aValue))
          aValue.Truncate();
      }
      break;
    }
#ifdef MOZ_SVG
    case eCSSProperty_marker: {
      nsCSSValue endValue, midValue, startValue;
      GetValueOrImportantValue(eCSSProperty_marker_end, endValue);
      GetValueOrImportantValue(eCSSProperty_marker_mid, midValue);
      GetValueOrImportantValue(eCSSProperty_marker_start, startValue);
      if (endValue == midValue && midValue == startValue)
        AppendValueToString(eCSSProperty_marker_end, aValue);
      break;
    }
#endif
    default:
      NS_NOTREACHED("no other shorthands");
      break;
  }
  return NS_OK;
}

PRBool
nsCSSDeclaration::GetValueIsImportant(const nsAString& aProperty) const
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty);
  return GetValueIsImportant(propID);
}

PRBool
nsCSSDeclaration::GetValueIsImportant(nsCSSProperty aProperty) const
{
  if (!mImportantData)
    return PR_FALSE;

  // Calling StorageFor is inefficient, but we can assume '!important'
  // is rare.

  if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProperty) {
      if (*p == eCSSProperty__x_system_font) {
        // The system_font subproperty doesn't count.
        continue;
      }
      if (!mImportantData->StorageFor(*p)) {
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }

  return mImportantData->StorageFor(aProperty) != nsnull;
}

// XXXldb Bug 376075 All callers of AllPropertiesSameImportance also
// need to check for 'inherit' and 'initial' values, since you can't
// output a mix of either mixed with other values in the same shorthand!
PRBool
nsCSSDeclaration::AllPropertiesSameImportance(PRInt32 aFirst, PRInt32 aSecond,
                                              PRInt32 aThird, PRInt32 aFourth,
                                              PRInt32 aFifth,
                                              PRBool & aImportance) const
{
  aImportance = GetValueIsImportant(OrderValueAt(aFirst-1));
  if ((aSecond && aImportance != GetValueIsImportant(OrderValueAt(aSecond-1))) ||
      (aThird && aImportance != GetValueIsImportant(OrderValueAt(aThird-1))) ||
      (aFourth && aImportance != GetValueIsImportant(OrderValueAt(aFourth-1))) ||
      (aFifth && aImportance != GetValueIsImportant(OrderValueAt(aFifth-1)))) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsCSSDeclaration::AllPropertiesSameValue(PRInt32 aFirst, PRInt32 aSecond,
                                         PRInt32 aThird, PRInt32 aFourth) const
{
  nsCSSValue firstValue, otherValue;
  // TryBorderShorthand does the bounds-checking for us; valid values there
  // are > 0; 0 is a flag for "not set".  We here are passed the actual
  // index, which comes from finding the value in the mOrder property array.
  // Of course, re-getting the mOrder value here is pretty silly.
  GetValueOrImportantValue(OrderValueAt(aFirst-1), firstValue);
  GetValueOrImportantValue(OrderValueAt(aSecond-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  GetValueOrImportantValue(OrderValueAt(aThird-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  GetValueOrImportantValue(OrderValueAt(aFourth-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/* static */ void
nsCSSDeclaration::AppendImportanceToString(PRBool aIsImportant,
                                           nsAString& aString)
{
  if (aIsImportant) {
   aString.AppendLiteral(" ! important");
  }
}

void
nsCSSDeclaration::AppendPropertyAndValueToString(nsCSSProperty aProperty,
                                                 nsCSSProperty aPropertyName,
                                                 nsAString& aResult) const
{
  NS_ASSERTION(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
               "property enum out of range");
  AppendASCIItoUTF16(nsCSSProps::GetStringValue(aPropertyName), aResult);
  aResult.AppendLiteral(": ");
  AppendValueToString(aProperty, aResult);
  PRBool  isImportant = GetValueIsImportant(aProperty);
  AppendImportanceToString(isImportant, aResult);
  aResult.AppendLiteral("; ");
}

PRBool
nsCSSDeclaration::TryBorderShorthand(nsAString & aString, PRUint32 aPropertiesSet,
                                     PRInt32 aBorderTopWidth,
                                     PRInt32 aBorderTopStyle,
                                     PRInt32 aBorderTopColor,
                                     PRInt32 aBorderBottomWidth,
                                     PRInt32 aBorderBottomStyle,
                                     PRInt32 aBorderBottomColor,
                                     PRInt32 aBorderLeftWidth,
                                     PRInt32 aBorderLeftStyle,
                                     PRInt32 aBorderLeftColor,
                                     PRInt32 aBorderRightWidth,
                                     PRInt32 aBorderRightStyle,
                                     PRInt32 aBorderRightColor) const
{
  PRBool border = PR_FALSE, isImportant = PR_FALSE;
  // 0 means not in the mOrder array; otherwise it's index+1
  if (B_BORDER == aPropertiesSet
      && AllPropertiesSameValue(aBorderTopWidth, aBorderBottomWidth,
                                aBorderLeftWidth, aBorderRightWidth)
      && AllPropertiesSameValue(aBorderTopStyle, aBorderBottomStyle,
                                aBorderLeftStyle, aBorderRightStyle)
      && AllPropertiesSameValue(aBorderTopColor, aBorderBottomColor,
                                aBorderLeftColor, aBorderRightColor)) {
    border = PR_TRUE;
  }
  if (border) {
    border = PR_FALSE;
    PRBool  isWidthImportant, isStyleImportant, isColorImportant;
    if (AllPropertiesSameImportance(aBorderTopWidth, aBorderBottomWidth,
                                    aBorderLeftWidth, aBorderRightWidth,
                                    0,
                                    isWidthImportant) &&
        AllPropertiesSameImportance(aBorderTopStyle, aBorderBottomStyle,
                                    aBorderLeftStyle, aBorderRightStyle,
                                    0,
                                    isStyleImportant) &&
        AllPropertiesSameImportance(aBorderTopColor, aBorderBottomColor,
                                    aBorderLeftColor, aBorderRightColor,
                                    0,
                                    isColorImportant)) {
      if (isWidthImportant == isStyleImportant && isWidthImportant == isColorImportant) {
        border = PR_TRUE;
        isImportant = isWidthImportant;
      }
    }
  }
  if (border) {
    AppendASCIItoUTF16(nsCSSProps::GetStringValue(eCSSProperty_border), aString);
    aString.AppendLiteral(": ");

    AppendValueToString(eCSSProperty_border_top_width, aString);
    aString.Append(PRUnichar(' '));

    AppendValueToString(eCSSProperty_border_top_style, aString);
    aString.Append(PRUnichar(' '));

    nsAutoString valueString;
    AppendValueToString(eCSSProperty_border_top_color, valueString);
    if (!valueString.EqualsLiteral("-moz-use-text-color")) {
      /* don't output this value, it's proprietary Mozilla and  */
      /* not intended to be exposed ; we can remove it from the */
      /* values of the shorthand since this value represents the */
      /* initial value of border-*-color */
      aString.Append(valueString);
    }
    AppendImportanceToString(isImportant, aString);
    aString.AppendLiteral("; ");
  }
  return border;
}

PRBool
nsCSSDeclaration::TryBorderSideShorthand(nsAString & aString,
                                         nsCSSProperty aShorthand,
                                         PRInt32 aBorderWidth,
                                         PRInt32 aBorderStyle,
                                         PRInt32 aBorderColor) const
{
  PRBool isImportant;
  if (AllPropertiesSameImportance(aBorderWidth, aBorderStyle, aBorderColor,
                                  0, 0,
                                  isImportant)) {
    AppendASCIItoUTF16(nsCSSProps::GetStringValue(aShorthand), aString);
    aString.AppendLiteral(": ");

    AppendValueToString(OrderValueAt(aBorderWidth-1), aString);

    aString.Append(PRUnichar(' '));
    AppendValueToString(OrderValueAt(aBorderStyle-1), aString);

    nsAutoString valueString;
    AppendValueToString(OrderValueAt(aBorderColor-1), valueString);
    if (!valueString.EqualsLiteral("-moz-use-text-color")) {
      aString.AppendLiteral(" ");
      aString.Append(valueString);
    }
    AppendImportanceToString(isImportant, aString);
    aString.AppendLiteral("; ");
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsCSSDeclaration::TryFourSidesShorthand(nsAString & aString,
                                        nsCSSProperty aShorthand,
                                        PRInt32 & aTop,
                                        PRInt32 & aBottom,
                                        PRInt32 & aLeft,
                                        PRInt32 & aRight,
                                        PRBool aClearIndexes) const
{
  // 0 means not in the mOrder array; otherwise it's index+1
  PRBool isImportant;
  if (aTop && aBottom && aLeft && aRight &&
      AllPropertiesSameImportance(aTop, aBottom, aLeft, aRight,
                                  0,
                                  isImportant)) {
    // all 4 properties are set, we can output a shorthand
    AppendASCIItoUTF16(nsCSSProps::GetStringValue(aShorthand), aString);
    aString.AppendLiteral(": ");
    nsCSSValue topValue, bottomValue, leftValue, rightValue;
    nsCSSProperty topProp    = OrderValueAt(aTop-1);
    nsCSSProperty bottomProp = OrderValueAt(aBottom-1);
    nsCSSProperty leftProp   = OrderValueAt(aLeft-1);
    nsCSSProperty rightProp  = OrderValueAt(aRight-1);
    GetValueOrImportantValue(topProp,    topValue);
    GetValueOrImportantValue(bottomProp, bottomValue);
    GetValueOrImportantValue(leftProp,   leftValue);
    GetValueOrImportantValue(rightProp,  rightValue);
    AppendCSSValueToString(topProp, topValue, aString);
    if (topValue != rightValue || topValue != leftValue || topValue != bottomValue) {
      aString.Append(PRUnichar(' '));
      AppendCSSValueToString(rightProp, rightValue, aString);
      if (topValue != bottomValue || rightValue != leftValue) {
        aString.Append(PRUnichar(' '));
        AppendCSSValueToString(bottomProp, bottomValue, aString);
        if (rightValue != leftValue) {
          aString.Append(PRUnichar(' '));
          AppendCSSValueToString(leftProp, leftValue, aString);
        }
      }
    }
    if (aClearIndexes) {
      aTop = 0; aBottom = 0; aLeft = 0; aRight = 0;
    }
    AppendImportanceToString(isImportant, aString);
    aString.AppendLiteral("; ");
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsCSSDeclaration::TryBackgroundShorthand(nsAString & aString,
                                         PRInt32 & aBgColor,
                                         PRInt32 & aBgImage,
                                         PRInt32 & aBgRepeat,
                                         PRInt32 & aBgAttachment,
                                         PRInt32 & aBgPosition) const
{
  // 0 means not in the mOrder array; otherwise it's index+1
  // check if we have at least two properties set; otherwise, no need to
  // use a shorthand
  PRBool isImportant;
  if (aBgColor && aBgImage && aBgRepeat && aBgAttachment && aBgPosition &&
      AllPropertiesSameImportance(aBgColor, aBgImage, aBgRepeat, aBgAttachment,
                                  aBgPosition, isImportant)) {
    AppendASCIItoUTF16(nsCSSProps::GetStringValue(eCSSProperty_background), aString);
    aString.AppendLiteral(": ");

    AppendValueToString(eCSSProperty_background_color, aString);
    aBgColor = 0;

    aString.Append(PRUnichar(' '));
    AppendValueToString(eCSSProperty_background_image, aString);
    aBgImage = 0;

    aString.Append(PRUnichar(' '));
    AppendValueToString(eCSSProperty_background_repeat, aString);
    aBgRepeat = 0;

    aString.Append(PRUnichar(' '));
    AppendValueToString(eCSSProperty_background_attachment, aString);
    aBgAttachment = 0;

    aString.Append(PRUnichar(' '));
    AppendValueToString(eCSSProperty_background_position, aString);
    aBgPosition = 0;

    AppendImportanceToString(isImportant, aString);
    aString.AppendLiteral("; ");
  }
}

void
nsCSSDeclaration::TryOverflowShorthand(nsAString & aString,
                                       PRInt32 & aOverflowX,
                                       PRInt32 & aOverflowY) const
{
  PRBool isImportant;
  if (aOverflowX && aOverflowY &&
      AllPropertiesSameImportance(aOverflowX, aOverflowY,
                                  0, 0, 0, isImportant)) {
    nsCSSValue xValue, yValue;
    GetValueOrImportantValue(eCSSProperty_overflow_x, xValue);
    GetValueOrImportantValue(eCSSProperty_overflow_y, yValue);
    if (xValue == yValue) {
      AppendASCIItoUTF16(nsCSSProps::GetStringValue(eCSSProperty_overflow),
                         aString);
      aString.AppendLiteral(": ");

      AppendCSSValueToString(eCSSProperty_overflow_x, xValue, aString);
      AppendImportanceToString(isImportant, aString);
      aString.AppendLiteral("; ");
      aOverflowX = aOverflowY = 0;
    }
  }
}

#ifdef MOZ_SVG
void
nsCSSDeclaration::TryMarkerShorthand(nsAString & aString,
                                     PRInt32 & aMarkerEnd,
                                     PRInt32 & aMarkerMid,
                                     PRInt32 & aMarkerStart) const
{
  PRBool isImportant;
  if (aMarkerEnd && aMarkerMid && aMarkerEnd &&
      AllPropertiesSameImportance(aMarkerEnd, aMarkerMid, aMarkerStart,
                                  0, 0, isImportant)) {
    nsCSSValue endValue, midValue, startValue;
    GetValueOrImportantValue(eCSSProperty_marker_end, endValue);
    GetValueOrImportantValue(eCSSProperty_marker_mid, midValue);
    GetValueOrImportantValue(eCSSProperty_marker_start, startValue);
    if (endValue == midValue && midValue == startValue) {
      AppendASCIItoUTF16(nsCSSProps::GetStringValue(eCSSProperty_marker),
                         aString);
      aString.AppendLiteral(": ");

      AppendCSSValueToString(eCSSProperty_marker_end, endValue, aString);
      AppendImportanceToString(isImportant, aString);
      aString.AppendLiteral("; ");
      aMarkerEnd = aMarkerMid = aMarkerStart = 0;
    }
  }
}
#endif

#define NS_CASE_OUTPUT_PROPERTY_VALUE(_prop, _index) \
case _prop: \
          if (_index) { \
            AppendPropertyAndValueToString(property, aString); \
            _index = 0; \
          } \
          break;

#define NS_CASE_OUTPUT_PROPERTY_VALUE_AS(_prop, _propas, _index) \
case _prop: \
          if (_index) { \
            AppendPropertyAndValueToString(property, _propas, aString); \
            _index = 0; \
          } \
          break;

#define NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(_condition, _prop, _index) \
case _prop: \
          if ((_condition) && _index) { \
            AppendPropertyAndValueToString(property, aString); \
            _index = 0; \
          } \
          break;

#define NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(_condition, _prop, _propas, _index) \
case _prop: \
          if ((_condition) && _index) { \
            AppendPropertyAndValueToString(property, _propas, aString); \
            _index = 0; \
          } \
          break;

void nsCSSDeclaration::PropertyIsSet(PRInt32 & aPropertyIndex, PRInt32 aIndex, PRUint32 & aSet, PRUint32 aValue) const
{
  aPropertyIndex = aIndex + 1;
  aSet |= aValue;
}

nsresult
nsCSSDeclaration::ToString(nsAString& aString) const
{
  PRInt32 count = mOrder.Length();
  PRInt32 index;
  // 0 means not in the mOrder array; otherwise it's index+1
  PRInt32 borderTopWidth = 0, borderTopStyle = 0, borderTopColor = 0;
  PRInt32 borderBottomWidth = 0, borderBottomStyle = 0, borderBottomColor = 0;
  PRInt32 borderLeftWidth = 0, borderLeftStyle = 0, borderLeftColor = 0;
  PRInt32 borderRightWidth = 0, borderRightStyle = 0, borderRightColor = 0;
  PRInt32 borderStartWidth = 0, borderStartStyle = 0, borderStartColor = 0;
  PRInt32 borderEndWidth = 0, borderEndStyle = 0, borderEndColor = 0;
  PRInt32 marginTop = 0,  marginBottom = 0,  marginLeft = 0,  marginRight = 0;
  PRInt32 paddingTop = 0, paddingBottom = 0, paddingLeft = 0, paddingRight = 0;
  PRInt32 bgColor = 0, bgImage = 0, bgRepeat = 0, bgAttachment = 0;
  PRInt32 bgPosition = 0;
  PRInt32 overflowX = 0, overflowY = 0;
  PRInt32 columnRuleWidth = 0, columnRuleStyle = 0, columnRuleColor = 0;
  PRUint32 borderPropertiesSet = 0, finalBorderPropertiesToSet = 0;
#ifdef MOZ_SVG
  PRInt32 markerEnd = 0, markerMid = 0, markerStart = 0;
#endif

  for (index = 0; index < count; index++) {
    nsCSSProperty property = OrderValueAt(index);
    switch (property) {
      case eCSSProperty_border_top_width:
        PropertyIsSet(borderTopWidth, index, borderPropertiesSet, B_BORDER_TOP_WIDTH);
        break;
      case eCSSProperty_border_bottom_width:
        PropertyIsSet(borderBottomWidth, index, borderPropertiesSet, B_BORDER_BOTTOM_WIDTH);
        break;
      case eCSSProperty_border_left_width_value:
        PropertyIsSet(borderLeftWidth, index, borderPropertiesSet, B_BORDER_LEFT_WIDTH);
        break;
      case eCSSProperty_border_right_width_value:
        PropertyIsSet(borderRightWidth, index, borderPropertiesSet, B_BORDER_RIGHT_WIDTH);
        break;
      case eCSSProperty_border_start_width_value:
        borderStartWidth = index+1;
        break;
      case eCSSProperty_border_end_width_value:
        borderEndWidth = index+1;
        break;

      case eCSSProperty_border_top_style:
        PropertyIsSet(borderTopStyle, index, borderPropertiesSet, B_BORDER_TOP_STYLE);
        break;
      case eCSSProperty_border_bottom_style:
        PropertyIsSet(borderBottomStyle, index, borderPropertiesSet, B_BORDER_BOTTOM_STYLE);
        break;
      case eCSSProperty_border_left_style_value:
        PropertyIsSet(borderLeftStyle, index, borderPropertiesSet, B_BORDER_LEFT_STYLE);
        break;
      case eCSSProperty_border_right_style_value:
        PropertyIsSet(borderRightStyle, index, borderPropertiesSet, B_BORDER_RIGHT_STYLE);
        break;
      case eCSSProperty_border_start_style_value:
        borderStartStyle = index+1;
        break;
      case eCSSProperty_border_end_style_value:
        borderEndStyle = index+1;
        break;

      case eCSSProperty_border_top_color:
        PropertyIsSet(borderTopColor, index, borderPropertiesSet, B_BORDER_TOP_COLOR);
        break;
      case eCSSProperty_border_bottom_color:
        PropertyIsSet(borderBottomColor, index, borderPropertiesSet, B_BORDER_BOTTOM_COLOR);
        break;
      case eCSSProperty_border_left_color_value:
        PropertyIsSet(borderLeftColor, index, borderPropertiesSet, B_BORDER_LEFT_COLOR);
        break;
      case eCSSProperty_border_right_color_value:
        PropertyIsSet(borderRightColor, index, borderPropertiesSet, B_BORDER_RIGHT_COLOR);
        break;
      case eCSSProperty_border_start_color_value:
        borderStartColor = index+1;
        break;
      case eCSSProperty_border_end_color_value:
        borderEndColor = index+1;
        break;

      case eCSSProperty_margin_top:            marginTop     = index+1; break;
      case eCSSProperty_margin_bottom:         marginBottom  = index+1; break;
      case eCSSProperty_margin_left_value:     marginLeft    = index+1; break;
      case eCSSProperty_margin_right_value:    marginRight   = index+1; break;

      case eCSSProperty_padding_top:           paddingTop    = index+1; break;
      case eCSSProperty_padding_bottom:        paddingBottom = index+1; break;
      case eCSSProperty_padding_left_value:    paddingLeft   = index+1; break;
      case eCSSProperty_padding_right_value:   paddingRight  = index+1; break;

      case eCSSProperty_background_color:      bgColor       = index+1; break;
      case eCSSProperty_background_image:      bgImage       = index+1; break;
      case eCSSProperty_background_repeat:     bgRepeat      = index+1; break;
      case eCSSProperty_background_attachment: bgAttachment  = index+1; break;
      case eCSSProperty_background_position:   bgPosition    = index+1; break;

      case eCSSProperty_overflow_x:            overflowX     = index+1; break;
      case eCSSProperty_overflow_y:            overflowY     = index+1; break;

      case eCSSProperty__moz_column_rule_width: columnRuleWidth = index+1; break;
      case eCSSProperty__moz_column_rule_style: columnRuleStyle = index+1; break;
      case eCSSProperty__moz_column_rule_color: columnRuleColor = index+1; break;

#ifdef MOZ_SVG
      case eCSSProperty_marker_end:            markerEnd     = index+1; break;
      case eCSSProperty_marker_mid:            markerMid     = index+1; break;
      case eCSSProperty_marker_start:          markerStart   = index+1; break;
#endif

      default: break;
    }
  }

  if (!TryBorderShorthand(aString, borderPropertiesSet,
                          borderTopWidth, borderTopStyle, borderTopColor,
                          borderBottomWidth, borderBottomStyle, borderBottomColor,
                          borderLeftWidth, borderLeftStyle, borderLeftColor,
                          borderRightWidth, borderRightStyle, borderRightColor)) {
    PRUint32 borderPropertiesToSet = 0;
    if ((borderPropertiesSet & B_BORDER_STYLE) != B_BORDER_STYLE ||
        !TryFourSidesShorthand(aString, eCSSProperty_border_style,
                               borderTopStyle, borderBottomStyle,
                               borderLeftStyle, borderRightStyle,
                               PR_FALSE)) {
      borderPropertiesToSet |= B_BORDER_STYLE;
    }
    if ((borderPropertiesSet & B_BORDER_COLOR) != B_BORDER_COLOR ||
        !TryFourSidesShorthand(aString, eCSSProperty_border_color,
                               borderTopColor, borderBottomColor,
                               borderLeftColor, borderRightColor,
                               PR_FALSE)) {
      borderPropertiesToSet |= B_BORDER_COLOR;
    }
    if ((borderPropertiesSet & B_BORDER_WIDTH) != B_BORDER_WIDTH ||
        !TryFourSidesShorthand(aString, eCSSProperty_border_width,
                               borderTopWidth, borderBottomWidth,
                               borderLeftWidth, borderRightWidth,
                               PR_FALSE)) {
      borderPropertiesToSet |= B_BORDER_WIDTH;
    }
    borderPropertiesToSet &= borderPropertiesSet;
    if (borderPropertiesToSet) {
      if ((borderPropertiesSet & B_BORDER_TOP) != B_BORDER_TOP ||
          !TryBorderSideShorthand(aString, eCSSProperty_border_top,
                                  borderTopWidth, borderTopStyle, borderTopColor)) {
        finalBorderPropertiesToSet |= B_BORDER_TOP;
      }
      if ((borderPropertiesSet & B_BORDER_LEFT) != B_BORDER_LEFT ||
          !TryBorderSideShorthand(aString, eCSSProperty_border_left,
                                  borderLeftWidth, borderLeftStyle, borderLeftColor)) {
        finalBorderPropertiesToSet |= B_BORDER_LEFT;
      }
      if ((borderPropertiesSet & B_BORDER_RIGHT) != B_BORDER_RIGHT ||
          !TryBorderSideShorthand(aString, eCSSProperty_border_right,
                                  borderRightWidth, borderRightStyle, borderRightColor)) {
        finalBorderPropertiesToSet |= B_BORDER_RIGHT;
      }
      if ((borderPropertiesSet & B_BORDER_BOTTOM) != B_BORDER_BOTTOM ||
          !TryBorderSideShorthand(aString, eCSSProperty_border_bottom,
                                  borderBottomWidth, borderBottomStyle, borderBottomColor)) {
        finalBorderPropertiesToSet |= B_BORDER_BOTTOM;
      }
      finalBorderPropertiesToSet &= borderPropertiesToSet;
    }
  }

  TryFourSidesShorthand(aString, eCSSProperty_margin,
                        marginTop, marginBottom,
                        marginLeft, marginRight,
                        PR_TRUE);
  TryFourSidesShorthand(aString, eCSSProperty_padding,
                        paddingTop, paddingBottom,
                        paddingLeft, paddingRight,
                        PR_TRUE);
  TryBackgroundShorthand(aString,
                         bgColor, bgImage, bgRepeat, bgAttachment,
                         bgPosition);
  TryOverflowShorthand(aString, overflowX, overflowY);
#ifdef MOZ_SVG
  TryMarkerShorthand(aString, markerEnd, markerMid, markerStart);
#endif

  if (columnRuleColor && columnRuleStyle && columnRuleWidth) {
    TryBorderSideShorthand(aString, eCSSProperty__moz_column_rule,
                           columnRuleWidth, columnRuleStyle, columnRuleColor);
    columnRuleWidth = columnRuleStyle = columnRuleColor = 0;
  }

  // FIXME The order of the declarations should depend on the *-source
  // properties.
  if (borderStartWidth && borderStartStyle && borderStartColor &&
      TryBorderSideShorthand(aString, eCSSProperty_border_start,
                             borderStartWidth, borderStartStyle, borderStartColor))
    borderStartWidth = borderStartStyle = borderStartColor = 0;
  if (borderEndWidth && borderEndStyle && borderEndColor &&
      TryBorderSideShorthand(aString, eCSSProperty_border_end,
                             borderEndWidth, borderEndStyle, borderEndColor))
    borderEndWidth = borderEndStyle = borderEndColor = 0;

  for (index = 0; index < count; index++) {
    nsCSSProperty property = OrderValueAt(index);
    switch (property) {

      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_STYLE,
                                                eCSSProperty_border_top_style, borderTopStyle)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_LEFT_STYLE,
                                                   eCSSProperty_border_left_style_value,
                                                   eCSSProperty_border_left_style, borderLeftStyle)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_RIGHT_STYLE,
                                                   eCSSProperty_border_right_style_value,
                                                   eCSSProperty_border_right_style, borderRightStyle)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_STYLE,
                                                eCSSProperty_border_bottom_style, borderBottomStyle)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_start_style_value,
                                       eCSSProperty_border_start_style, borderStartStyle)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_end_style_value,
                                       eCSSProperty_border_end_style, borderEndStyle)

      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_COLOR,
                                                eCSSProperty_border_top_color, borderTopColor)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_LEFT_COLOR,
                                                   eCSSProperty_border_left_color_value,
                                                   eCSSProperty_border_left_color, borderLeftColor)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_RIGHT_COLOR,
                                                   eCSSProperty_border_right_color_value,
                                                   eCSSProperty_border_right_color, borderRightColor)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_COLOR,
                                                eCSSProperty_border_bottom_color, borderBottomColor)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_start_color_value,
                                       eCSSProperty_border_start_color, borderStartColor)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_end_color_value,
                                       eCSSProperty_border_end_color, borderEndColor)

      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_WIDTH,
                                                eCSSProperty_border_top_width, borderTopWidth)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_LEFT_WIDTH,
                                                   eCSSProperty_border_left_width_value,
                                                   eCSSProperty_border_left_width, borderLeftWidth)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE_AS(finalBorderPropertiesToSet & B_BORDER_RIGHT_WIDTH,
                                                   eCSSProperty_border_right_width_value,
                                                   eCSSProperty_border_right_width, borderRightWidth)
      NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_WIDTH,
                                                eCSSProperty_border_bottom_width, borderBottomWidth)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_start_width_value,
                                       eCSSProperty_border_start_width, borderStartWidth)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_border_end_width_value,
                                       eCSSProperty_border_end_width, borderEndWidth)

      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_top, marginTop)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_bottom, marginBottom)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_margin_left_value,
                                       eCSSProperty_margin_left, marginLeft)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_margin_right_value,
                                       eCSSProperty_margin_right, marginRight)

      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_top, paddingTop)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_bottom, paddingBottom)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_padding_left_value,
                                       eCSSProperty_padding_left, paddingLeft)
      NS_CASE_OUTPUT_PROPERTY_VALUE_AS(eCSSProperty_padding_right_value,
                                       eCSSProperty_padding_right, paddingRight)

      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_color, bgColor)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_image, bgImage)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_repeat, bgRepeat)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_attachment, bgAttachment)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_position, bgPosition)

      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_overflow_x, overflowX)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_overflow_y, overflowY)

#ifdef MOZ_SVG
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_marker_end, markerEnd)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_marker_mid, markerMid)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_marker_start, markerStart)
#endif

      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty__moz_column_rule_width, columnRuleWidth)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty__moz_column_rule_style, columnRuleStyle)
      NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty__moz_column_rule_color, columnRuleColor)

      case eCSSProperty_margin_left_ltr_source:
      case eCSSProperty_margin_left_rtl_source:
      case eCSSProperty_margin_right_ltr_source:
      case eCSSProperty_margin_right_rtl_source:
      case eCSSProperty_padding_left_ltr_source:
      case eCSSProperty_padding_left_rtl_source:
      case eCSSProperty_padding_right_ltr_source:
      case eCSSProperty_padding_right_rtl_source:
      case eCSSProperty_border_left_color_ltr_source:
      case eCSSProperty_border_left_color_rtl_source:
      case eCSSProperty_border_left_style_ltr_source:
      case eCSSProperty_border_left_style_rtl_source:
      case eCSSProperty_border_left_width_ltr_source:
      case eCSSProperty_border_left_width_rtl_source:
      case eCSSProperty_border_right_color_ltr_source:
      case eCSSProperty_border_right_color_rtl_source:
      case eCSSProperty_border_right_style_ltr_source:
      case eCSSProperty_border_right_style_rtl_source:
      case eCSSProperty_border_right_width_ltr_source:
      case eCSSProperty_border_right_width_rtl_source:
        break;

      case eCSSProperty_margin_start_value:
        AppendPropertyAndValueToString(property, eCSSProperty_margin_start,
                                       aString);
        break;
      case eCSSProperty_margin_end_value:
        AppendPropertyAndValueToString(property, eCSSProperty_margin_end,
                                       aString);
        break;
      case eCSSProperty_padding_start_value:
        AppendPropertyAndValueToString(property, eCSSProperty_padding_start,
                                       aString);
        break;
      case eCSSProperty_padding_end_value:
        AppendPropertyAndValueToString(property, eCSSProperty_padding_end,
                                       aString);
        break;

      default:
        if (0 <= property) {
          AppendPropertyAndValueToString(property, aString);
        }
        break;
    }
  }
  if (! aString.IsEmpty()) {
    // if the string is not empty, we have a trailing whitespace we should remove
    aString.Truncate(aString.Length() - 1);
  }
  return NS_OK;
}

#ifdef DEBUG
void nsCSSDeclaration::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);
  nsAutoString s;
  ToString(s);
  fputs(NS_ConvertUTF16toUTF8(s).get(), out);
  fputs("}", out);
}
#endif

nsresult
nsCSSDeclaration::GetNthProperty(PRUint32 aIndex, nsAString& aReturn) const
{
  aReturn.Truncate();
  if (aIndex < mOrder.Length()) {
    nsCSSProperty property = OrderValueAt(aIndex);
    if (0 <= property) {
      AppendASCIItoUTF16(nsCSSProps::GetStringValue(property), aReturn);
    }
  }
  
  return NS_OK;
}

nsCSSDeclaration*
nsCSSDeclaration::Clone() const
{
  return new nsCSSDeclaration(*this);
}

PRBool
nsCSSDeclaration::InitializeEmpty()
{
  NS_ASSERTION(!mData && !mImportantData, "already initialized");
  mData = nsCSSCompressedDataBlock::CreateEmptyBlock();
  return mData != nsnull;
}
