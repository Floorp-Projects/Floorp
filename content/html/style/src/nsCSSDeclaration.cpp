/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsICSSDeclaration.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsCSSPropIDs.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"

#include "nsStyleConsts.h"

//#define DEBUG_REFS


struct CSSColorEntry{
	PRUint8 r;
	PRUint8 g;
	PRUint8 b;
	char *name;
}; 

static CSSColorEntry css_rgb_table[] = 
{
  {  0,   0,   0,  "black" },
  {  0,   0, 128,  "navy" },
  {  0,   0, 139,  "darkblue" },
  {  0,   0, 205,  "mediumblue" },
  {  0,   0, 255,  "blue" },
  {  0, 100,   0,  "darkgreen" },
  {  0, 128,   0,  "green" },
  {  0, 128, 128,  "teal" },
  {  0, 139, 139,  "darkcyan" },
  {  0, 191, 255,  "deepskyblue" },
  {  0, 206, 209,  "darkturquoise" },
  {  0, 250, 154,  "mediumspringgreen" },
  {  0, 255,   0,  "lime" },
  {  0, 255, 127,  "springgreen" },
  {  0, 255, 255,  "aqua" },
  {  0, 255, 255,  "cyan" },
  { 25,  25, 112,  "midnightblue" },
  { 30, 144, 255,  "dodgerblue" },
  { 32, 178, 170,  "lightseagreen" },
  { 34, 139,  34,  "forestgreen" },
  { 46, 139,  87,  "seagreen" },
  { 47,  79,  79,  "darkslategray" },
  { 50, 205,  50,  "limegreen" },
  { 60, 179, 113,  "mediumseagreen" },
  { 64, 224, 208,  "turquoise" },
  { 65, 105, 225,  "royalblue" },
  { 70, 130, 180,  "steelblue" },
  { 72,  61, 139,  "darkslateblue" },
  { 72, 209, 204,  "mediumturquoise" },
  { 75,   0, 130,  "indigo" },
  { 85, 107,  47,  "darkolivegreen" },
  { 95, 158, 160,  "cadetblue" },
  {100, 149, 237,  "cornflowerblue" },
  {102, 205, 170,  "mediumaquamarine" },
  {105, 105, 105,  "dimgray" },
  {106,  90, 205,  "slateblue" },
  {107, 142,  35,  "olivedrab" },
  {112, 128, 144,  "slategray" },
  {119, 136, 153,  "lightslategray" },
  {123, 104, 238,  "mediumslateblue" },
  {124, 252,   0,  "lawngreen" },
  {127, 255,   0,  "chartreuse" },
  {127, 255, 212,  "aquamarine" },
  {128,   0,   0,  "maroon" },
  {128,   0, 128,  "purple" },
  {128, 128,   0,  "olive" },
  {128, 128, 128,  "gray" },
  {135, 206, 235,  "skyblue" },
  {135, 206, 250,  "lightskyblue" },
  {138,  43, 226,  "blueviolet" },
  {139,   0,   0,  "darkred" },
  {139,   0, 139,  "darkmagenta" },
  {139,  69,  19,  "saddlebrown" },
  {143, 188, 143,  "darkseagreen" },
  {144, 238, 144,  "lightgreen" },
  {147, 112, 219,  "mediumpurple" },
  {148,   0, 211,  "darkviolet" },
  {152, 251, 152,  "palegreen" },
  {153,  50, 204,  "darkorchid" },
  {154, 205,  50,  "yellowgreen" },
  {160,  82,  45,  "sienna" },
  {165,  42,  42,  "brown" },
  {169, 169, 169,  "darkgray" },
  {173, 216, 230,  "lightblue" },
  {173, 255,  47,  "greenyellow" },
  {175, 238, 238,  "paleturquoise" },
  {176, 196, 222,  "lightsteelblue" },
  {176, 224, 230,  "powderblue" },
  {178,  34,  34,  "firebrick" },
  {184, 134,  11,  "darkgoldenrod" },
  {186,  85, 211,  "mediumorchid" },
  {188, 143, 143,  "rosybrown" },
  {189, 183, 107,  "darkkhaki" },
  {192, 192, 192,  "silver" },
  {199,  21, 133,  "mediumvioletred" },
  {205,  92,  92,  "indianred" },
  {205, 133,  63,  "peru" },
  {210, 105,  30,  "chocolate" },
  {210, 180, 140,  "tan" },
  {211, 211, 211,  "lightgrey" },
  {216, 191, 216,  "thistle" },
  {218, 112, 214,  "orchid" },
  {218, 165,  32,  "goldenrod" },
  {219, 112, 147,  "palevioletred" },
  {220,  20,  60,  "crimson" },
  {220, 220, 220,  "gainsboro" },
  {221, 160, 221,  "plum" },
  {222, 184, 135,  "burlywood" },
  {224, 255, 255,  "lightcyan" },
  {230, 230, 250,  "lavender" },
  {233, 150, 122,  "darksalmon" },
  {238, 130, 238,  "violet" },
  {238, 232, 170,  "palegoldenrod" },
  {240, 128, 128,  "lightcoral" },
  {240, 230, 140,  "khaki" },
  {240, 248, 255,  "aliceblue" },
  {240, 255, 240,  "honeydew" },
  {240, 255, 255,  "azure" },
  {244, 164,  96,  "sandybrown" },
  {245, 222, 179,  "wheat" },
  {245, 245, 220,  "beige" },
  {245, 245, 245,  "whitesmoke" },
  {245, 255, 250,  "mintcream" },
  {248, 248, 255,  "ghostwhite" },
  {250, 128, 114,  "salmon" },
  {250, 235, 215,  "antiquewhite" },
  {250, 240, 230,  "linen" },
  {250, 250, 210,  "lightgoldenrodyellow" },
  {253, 245, 230,  "oldlace" },
  {255,   0,   0,  "red" },
  {255,   0, 255,  "fuchsia" },
  {255,   0, 255,  "magenta" },
  {255,  20, 147,  "deeppink" },
  {255,  69,   0,  "orangered" },
  {255,  99,  71,  "tomato" },
  {255, 105, 180,  "hotpink" },
  {255, 127,  80,  "coral" },
  {255, 140,   0,  "darkorange" },
  {255, 160, 122,  "lightsalmon" },
  {255, 165,   0,  "orange" },
  {255, 182, 193,  "lightpink" },
  {255, 192, 203,  "pink" },
  {255, 215,   0,  "gold" },
  {255, 218, 185,  "peachpuff" },
  {255, 222, 173,  "navajowhite" },
  {255, 228, 181,  "moccasin" },
  {255, 228, 196,  "bisque" },
  {255, 228, 225,  "mistyrose" },
  {255, 235, 205,  "blanchedalmond" },
  {255, 239, 213,  "papayawhip" },
  {255, 240, 245,  "lavenderblush" },
  {255, 245, 238,  "seashell" },
  {255, 248, 220,  "cornsilk" },
  {255, 250, 205,  "lemonchiffon" },
  {255, 250, 240,  "floralwhite" },
  {255, 250, 250,  "snow" },
  {255, 255,   0,  "yellow" },
  {255, 255, 224,  "lightyellow" },
  {255, 255, 240,  "ivory" },
  {255, 255, 255,  "white" },
};



static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);


nsCSSValue::nsCSSValue(nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(mUnit <= eCSSUnit_Normal, "not a valueless unit");
  if (aUnit > eCSSUnit_Normal) {
    mUnit = eCSSUnit_Null;
  }
  mValue.mInt = 0;
}

nsCSSValue::nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eCSSUnit_Integer == aUnit) ||
               (eCSSUnit_Enumerated == aUnit), "not an int value");
  if ((eCSSUnit_Integer == aUnit) ||
      (eCSSUnit_Enumerated == aUnit)) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsString& aValue)
  : mUnit(eCSSUnit_String)
{
  mValue.mString = aValue.ToNewString();
}

nsCSSValue::nsCSSValue(nscolor aValue)
  : mUnit(eCSSUnit_Color)
{
  mValue.mColor = aValue;
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if (eCSSUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
    }
    else {
      mValue.mString = nsnull;
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
}

nsCSSValue::~nsCSSValue(void)
{
  Reset();
}

// XXX shouldn't this get moved to color code?
static const char* RGBToCSSString(nscolor aColor)
{
  const char* result = nsnull;

  PRInt32 r = NS_GET_R(aColor);
  PRInt32 g = NS_GET_G(aColor);
  PRInt32 b = NS_GET_B(aColor);

  PRInt32         index = 0;
  PRInt32         count = sizeof(css_rgb_table)/sizeof(CSSColorEntry);
  CSSColorEntry*  entry = nsnull;
  
  for (index = 0; index < count; index++)
  {
    entry = &css_rgb_table[index];
    if (entry->r == r)
    {
      if (entry->g == g && entry->b == b)
      {
        result = entry->name;
        break;
      }
    }
    else if (entry->r > r)
    {
      break;
    }
  }
  return result;
}


void
nsCSSValue::ValueToString(nsString& aBuffer, const nsCSSValue& aValue, 
                          PRInt32 aPropID)
{
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Null == unit) {
    return;
  }

  if (eCSSUnit_String == unit) {
    nsAutoString  buffer;
    aValue.GetStringValue(buffer);
    aBuffer.Append(buffer);
  }
  else if (eCSSUnit_Integer == unit) {
    aBuffer.Append(aValue.GetIntValue(), 10);
  }
  else if (eCSSUnit_Enumerated == unit) {
    const char* name = nsCSSProps::LookupProperty(aPropID, aValue.GetIntValue());
    if (name != nsnull) {
      aBuffer.Append(name);
    }
  }
  else if (eCSSUnit_Color == unit){
    nscolor color = aValue.GetColorValue();
    const char* name = RGBToCSSString(color);

    if (name != nsnull)
      aBuffer.Append(name);
    else
    {
      aBuffer.Append("rgb(");
      aBuffer.Append(NS_GET_R(color), 10);
      aBuffer.Append(",");
      aBuffer.Append(NS_GET_G(color), 10);
      aBuffer.Append(",");
      aBuffer.Append(NS_GET_B(color), 10);
      aBuffer.Append(')');
    }
  }
  else if (eCSSUnit_Percent == unit) {
    aBuffer.Append(aValue.GetPercentValue() * 100.0f);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    aBuffer.Append(aValue.GetFloatValue());
  }

  switch (unit) {
    case eCSSUnit_Null:       break;
    case eCSSUnit_Auto:       aBuffer.Append("auto"); break;
    case eCSSUnit_Inherit:    aBuffer.Append("inherit"); break;
    case eCSSUnit_None:       aBuffer.Append("none"); break;
    case eCSSUnit_Normal:     aBuffer.Append("normal"); break;
    case eCSSUnit_String:     break;
    case eCSSUnit_Integer:    break;
    case eCSSUnit_Enumerated: break;
    case eCSSUnit_Color:      break;
    case eCSSUnit_Percent:    aBuffer.Append("%");    break;
    case eCSSUnit_Number:     break;
    case eCSSUnit_Inch:       aBuffer.Append("in");   break;
    case eCSSUnit_Foot:       aBuffer.Append("ft");   break;
    case eCSSUnit_Mile:       aBuffer.Append("mi");   break;
    case eCSSUnit_Millimeter: aBuffer.Append("mm");   break;
    case eCSSUnit_Centimeter: aBuffer.Append("cm");   break;
    case eCSSUnit_Meter:      aBuffer.Append("m");    break;
    case eCSSUnit_Kilometer:  aBuffer.Append("km");   break;
    case eCSSUnit_Point:      aBuffer.Append("pt");   break;
    case eCSSUnit_Pica:       aBuffer.Append("pc");   break;
    case eCSSUnit_Didot:      aBuffer.Append("dt");   break;
    case eCSSUnit_Cicero:     aBuffer.Append("cc");   break;
    case eCSSUnit_EM:         aBuffer.Append("em");   break;
    case eCSSUnit_EN:         aBuffer.Append("en");   break;
    case eCSSUnit_XHeight:    aBuffer.Append("ex");   break;
    case eCSSUnit_CapHeight:  aBuffer.Append("cap");  break;
    case eCSSUnit_Pixel:      aBuffer.Append("px");   break;
  }
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  Reset();
  mUnit = aCopy.mUnit;
  if (eCSSUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  return *this;
}

PRBool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if (eCSSUnit_String == mUnit) {
      if (nsnull == mValue.mString) {
        if (nsnull == aOther.mValue.mString) {
          return PR_TRUE;
        }
      }
      else if (nsnull != aOther.mValue.mString) {
        return mValue.mString->Equals(*(aOther.mValue.mString));
      }
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
    else if (eCSSUnit_Color == mUnit){
      return PRBool(mValue.mColor == aOther.mValue.mColor);
    }
    else {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
  }
  return PR_FALSE;
}

nscoord nsCSSValue::GetLengthTwips(void) const
{
  NS_ASSERTION(IsFixedLengthUnit(), "not a fixed length unit");

  if (IsFixedLengthUnit()) {
    switch (mUnit) {
    case eCSSUnit_Inch:        
      return NS_INCHES_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Foot:        
      return NS_FEET_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Mile:        
      return NS_MILES_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Millimeter:
      return NS_MILLIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Centimeter:
      return NS_CENTIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Meter:
      return NS_METERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Kilometer:
      return NS_KILOMETERS_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Point:
      return NSFloatPointsToTwips(mValue.mFloat);
    case eCSSUnit_Pica:
      return NS_PICAS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Didot:
      return NS_DIDOTS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Cicero:
      return NS_CICEROS_TO_TWIPS(mValue.mFloat);
    }
  }
  return 0;
}

void nsCSSValue::Reset(void)
{
  if ((eCSSUnit_String == mUnit) && (nsnull != mValue.mString)) {
    delete mValue.mString;
  }
  mUnit = eCSSUnit_Null;
  mValue.mInt = 0;
};

void nsCSSValue::SetIntValue(PRInt32 aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION((eCSSUnit_Integer == aUnit) ||
               (eCSSUnit_Enumerated == aUnit), "not an int value");
  Reset();
  if ((eCSSUnit_Integer == aUnit) ||
      (eCSSUnit_Enumerated == aUnit)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION(eCSSUnit_Number <= aUnit, "not a float value");
  Reset();
  if (eCSSUnit_Number <= aUnit) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  }
}

void nsCSSValue::SetStringValue(const nsString& aValue)
{
  Reset();
  mUnit = eCSSUnit_String;
  mValue.mString = aValue.ToNewString();
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetAutoValue(void)
{
  Reset();
  mUnit = eCSSUnit_Auto;
}

void nsCSSValue::SetInheritValue(void)
{
  Reset();
  mUnit = eCSSUnit_Inherit;
}

void nsCSSValue::SetNoneValue(void)
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetNormalValue(void)
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::AppendToString(nsString& aBuffer, PRInt32 aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    aBuffer.Append(nsCSSProps::kNameTable[aPropID].name);
    aBuffer.Append(": ");
  }

  if (eCSSUnit_String == mUnit) {
    if (nsnull != mValue.mString) {
      aBuffer.Append('"');
      aBuffer.Append(*(mValue.mString));
      aBuffer.Append('"');
    }
    else {
      aBuffer.Append("null str");
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    aBuffer.Append(mValue.mInt, 10);
    aBuffer.Append("[0x");
    aBuffer.Append(mValue.mInt, 16);
    aBuffer.Append(']');
  }
  else if (eCSSUnit_Color == mUnit){
    aBuffer.Append("(0x");
    aBuffer.Append(NS_GET_R(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_G(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_B(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_A(mValue.mColor), 16);
    aBuffer.Append(')');
  }
  else if (eCSSUnit_Percent == mUnit) {
    aBuffer.Append(mValue.mFloat * 100.0f);
  }
  else if (eCSSUnit_Percent < mUnit) {
    aBuffer.Append(mValue.mFloat);
  }

  switch (mUnit) {
    case eCSSUnit_Null:       break;
    case eCSSUnit_Auto:       aBuffer.Append("auto"); break;
    case eCSSUnit_Inherit:    aBuffer.Append("inherit"); break;
    case eCSSUnit_None:       aBuffer.Append("none"); break;
    case eCSSUnit_String:     break;
    case eCSSUnit_Integer:    aBuffer.Append("int");  break;
    case eCSSUnit_Enumerated: aBuffer.Append("enum"); break;
    case eCSSUnit_Color:      aBuffer.Append("rbga"); break;
    case eCSSUnit_Percent:    aBuffer.Append("%");    break;
    case eCSSUnit_Number:     aBuffer.Append("#");    break;
    case eCSSUnit_Inch:       aBuffer.Append("in");   break;
    case eCSSUnit_Foot:       aBuffer.Append("ft");   break;
    case eCSSUnit_Mile:       aBuffer.Append("mi");   break;
    case eCSSUnit_Millimeter: aBuffer.Append("mm");   break;
    case eCSSUnit_Centimeter: aBuffer.Append("cm");   break;
    case eCSSUnit_Meter:      aBuffer.Append("m");    break;
    case eCSSUnit_Kilometer:  aBuffer.Append("km");   break;
    case eCSSUnit_Point:      aBuffer.Append("pt");   break;
    case eCSSUnit_Pica:       aBuffer.Append("pc");   break;
    case eCSSUnit_Didot:      aBuffer.Append("dt");   break;
    case eCSSUnit_Cicero:     aBuffer.Append("cc");   break;
    case eCSSUnit_EM:         aBuffer.Append("em");   break;
    case eCSSUnit_EN:         aBuffer.Append("en");   break;
    case eCSSUnit_XHeight:    aBuffer.Append("ex");   break;
    case eCSSUnit_CapHeight:  aBuffer.Append("cap");  break;
    case eCSSUnit_Pixel:      aBuffer.Append("px");   break;
  }
  aBuffer.Append(' ');
}

void nsCSSValue::ToString(nsString& aBuffer, PRInt32 aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

const nsID& nsCSSFont::GetID(void)
{
  return kCSSFontSID;
}

void nsCSSFont::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mFamily.AppendToString(buffer, PROP_FONT_FAMILY);
  mStyle.AppendToString(buffer, PROP_FONT_STYLE);
  mVariant.AppendToString(buffer, PROP_FONT_VARIANT);
  mWeight.AppendToString(buffer, PROP_FONT_WEIGHT);
  mSize.AppendToString(buffer, PROP_FONT_SIZE);
  fputs(buffer, out);
}


const nsID& nsCSSColor::GetID(void)
{
  return kCSSColorSID;
}

void nsCSSColor::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mColor.AppendToString(buffer, PROP_COLOR);
  mBackColor.AppendToString(buffer, PROP_BACKGROUND_COLOR);
  mBackImage.AppendToString(buffer, PROP_BACKGROUND_IMAGE);
  mBackRepeat.AppendToString(buffer, PROP_BACKGROUND_REPEAT);
  mBackAttachment.AppendToString(buffer, PROP_BACKGROUND_ATTACHMENT);
  mBackPositionX.AppendToString(buffer, PROP_BACKGROUND_X_POSITION);
  mBackPositionY.AppendToString(buffer, PROP_BACKGROUND_Y_POSITION);
  mBackFilter.AppendToString(buffer, PROP_BACKGROUND_FILTER);
  mCursor.AppendToString(buffer, PROP_CURSOR);
  mCursorImage.AppendToString(buffer, PROP_CURSOR_IMAGE);
  mOpacity.AppendToString(buffer, PROP_OPACITY);
  fputs(buffer, out);
}

const nsID& nsCSSText::GetID(void)
{
  return kCSSTextSID;
}

void nsCSSText::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mWordSpacing.AppendToString(buffer, PROP_WORD_SPACING);
  mLetterSpacing.AppendToString(buffer, PROP_LETTER_SPACING);
  mDecoration.AppendToString(buffer, PROP_TEXT_DECORATION);
  mVerticalAlign.AppendToString(buffer, PROP_VERTICAL_ALIGN);
  mTextTransform.AppendToString(buffer, PROP_TEXT_TRANSFORM);
  mTextAlign.AppendToString(buffer, PROP_TEXT_ALIGN);
  mTextIndent.AppendToString(buffer, PROP_TEXT_INDENT);
  mLineHeight.AppendToString(buffer, PROP_LINE_HEIGHT);
  mWhiteSpace.AppendToString(buffer, PROP_WHITE_SPACE);
  fputs(buffer, out);
}

void nsCSSRect::List(FILE* out, PRInt32 aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (-1 < aPropID) {
    buffer.Append(nsCSSProps::kNameTable[aPropID].name);
    buffer.Append(": ");
  }

  mTop.AppendToString(buffer);
  mRight.AppendToString(buffer);
  mBottom.AppendToString(buffer); 
  mLeft.AppendToString(buffer);
  fputs(buffer, out);
}

void nsCSSRect::List(FILE* out, PRInt32 aIndent, PRIntn aTRBL[]) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSUnit_Null != mTop.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[0]].name);
    buffer.Append(": ");
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[1]].name);
    buffer.Append(": ");
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[2]].name);
    buffer.Append(": ");
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[3]].name);
    buffer.Append(": ");
    mLeft.AppendToString(buffer);
  }

  fputs(buffer, out);
}

nsCSSDisplay::nsCSSDisplay(void)
  : mClip(nsnull)
{
}

nsCSSDisplay::~nsCSSDisplay(void)
{
  if (nsnull != mClip) {
    delete mClip;
  }
}

const nsID& nsCSSDisplay::GetID(void)
{
  return kCSSDisplaySID;
}

void nsCSSDisplay::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mDirection.AppendToString(buffer, PROP_DIRECTION);
  mDisplay.AppendToString(buffer, PROP_DISPLAY);
  mFloat.AppendToString(buffer, PROP_FLOAT);
  mClear.AppendToString(buffer, PROP_CLEAR);
  mVisibility.AppendToString(buffer, PROP_VISIBILITY);
  mFilter.AppendToString(buffer, PROP_FILTER);
  fputs(buffer, out);
  if (nsnull != mClip) {
    mClip->List(out, PROP_CLIP);
  }
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, PROP_OVERFLOW);
  fputs(buffer, out);
}

nsCSSMargin::nsCSSMargin(void)
  : mMargin(nsnull), mPadding(nsnull), mBorder(nsnull), mColor(nsnull), mStyle(nsnull)
{
}

nsCSSMargin::~nsCSSMargin(void)
{
  if (nsnull != mMargin) {
    delete mMargin;
  }
  if (nsnull != mPadding) {
    delete mPadding;
  }
  if (nsnull != mBorder) {
    delete mBorder;
  }
  if (nsnull != mColor) {
    delete mColor;
  }
  if (nsnull != mStyle) {
    delete mStyle;
  }
}

const nsID& nsCSSMargin::GetID(void)
{
  return kCSSMarginSID;
}

void nsCSSMargin::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  if (nsnull != mMargin) {
    static PRIntn trbl[] = {
      PROP_MARGIN_TOP,
      PROP_MARGIN_RIGHT,
      PROP_MARGIN_BOTTOM,
      PROP_MARGIN_LEFT
    };
    mMargin->List(out, aIndent, trbl);
  }
  if (nsnull != mPadding) {
    static PRIntn trbl[] = {
      PROP_PADDING_TOP,
      PROP_PADDING_RIGHT,
      PROP_PADDING_BOTTOM,
      PROP_PADDING_LEFT
    };
    mPadding->List(out, aIndent, trbl);
  }
  if (nsnull != mBorder) {
    static PRIntn trbl[] = {
      PROP_BORDER_TOP_WIDTH,
      PROP_BORDER_RIGHT_WIDTH,
      PROP_BORDER_BOTTOM_WIDTH,
      PROP_BORDER_LEFT_WIDTH
    };
    mBorder->List(out, aIndent, trbl);
  }
  if (nsnull != mColor) {
    mColor->List(out, PROP_BORDER_COLOR, aIndent);
  }
  if (nsnull != mStyle) {
    mStyle->List(out, PROP_BORDER_STYLE, aIndent);
  }
}

const nsID& nsCSSPosition::GetID(void)
{
  return kCSSPositionSID;
}

void nsCSSPosition::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mPosition.AppendToString(buffer, PROP_POSITION);
  mWidth.AppendToString(buffer, PROP_WIDTH);
  mHeight.AppendToString(buffer, PROP_HEIGHT);
  mLeft.AppendToString(buffer, PROP_LEFT);
  mTop.AppendToString(buffer, PROP_TOP);
  mZIndex.AppendToString(buffer, PROP_Z_INDEX);
  fputs(buffer, out);
}

const nsID& nsCSSList::GetID(void)
{
  return kCSSListSID;
}

void nsCSSList::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mType.AppendToString(buffer, PROP_LIST_STYLE_TYPE);
  mImage.AppendToString(buffer, PROP_LIST_STYLE_IMAGE);
  mPosition.AppendToString(buffer, PROP_LIST_STYLE_POSITION);
  fputs(buffer, out);
}



class CSSDeclarationImpl : public nsICSSDeclaration {
public:
  void* operator new(size_t size);
  
  CSSDeclarationImpl(void);
  ~CSSDeclarationImpl(void);

  NS_DECL_ISUPPORTS

  nsresult  GetData(const nsID& aSID, nsCSSStruct** aData);
  nsresult  EnsureData(const nsID& aSID, nsCSSStruct** aData);

  nsresult AppendValue(const char* aProperty, const nsCSSValue& aValue);
  nsresult AppendValue(PRInt32 aProperty, const nsCSSValue& aValue);
  nsresult SetValueImportant(const char* aProperty);
  nsresult SetValueImportant(PRInt32 aProperty);
  nsresult AppendComment(const nsString& aComment);

  nsresult GetValue(const char* aProperty, nsCSSValue& aValue);
  nsresult GetValue(PRInt32 aProperty, nsCSSValue& aValue);

  nsresult GetImportantValues(nsICSSDeclaration*& aResult);
  nsresult GetValueIsImportant(const char *aProperty, PRBool& aIsImportant);

  virtual nsresult ToString(nsString& aString);

  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  
  nsresult Count(PRUint32* aCount);
  nsresult GetNthProperty(PRUint32 aIndex, nsString& aReturn);

private:
  CSSDeclarationImpl(const CSSDeclarationImpl& aCopy);
  CSSDeclarationImpl& operator=(const CSSDeclarationImpl& aCopy);
  PRBool operator==(const CSSDeclarationImpl& aCopy) const;

protected:
  nsCSSFont*      mFont;
  nsCSSColor*     mColor;
  nsCSSText*      mText;
  nsCSSMargin*    mMargin;
  nsCSSPosition*  mPosition;
  nsCSSList*      mList;
  nsCSSDisplay*   mDisplay;

  CSSDeclarationImpl* mImportant;

  nsVoidArray*    mOrder;
  nsVoidArray*    mComments;
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif


void* CSSDeclarationImpl::operator new(size_t size)
{
  void* result = new char[size];

  nsCRT::zero(result, size);
  return result;
}

CSSDeclarationImpl::CSSDeclarationImpl(void)
{
  NS_INIT_REFCNT();
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSDeclaration\n", gInstanceCount);
#endif
}

CSSDeclarationImpl::~CSSDeclarationImpl(void)
{
  if (nsnull != mFont) {
    delete mFont;
  }
  if (nsnull != mColor) {
    delete mColor;
  }
  if (nsnull != mText) {
    delete mText;
  }
  if (nsnull != mMargin) {
    delete mMargin;
  }
  if (nsnull != mPosition) {
    delete mPosition;
  }
  if (nsnull != mList) {
    delete mList;
  }
  if (nsnull != mDisplay) {
    delete mDisplay;
  }
  NS_IF_RELEASE(mImportant);
  if (nsnull != mOrder) {
    delete mOrder;
  }
  if (nsnull != mComments) {
    PRInt32 index = mComments->Count();
    while (0 < --index) {
      nsString* comment = (nsString*)mComments->ElementAt(index);
      delete comment;
    }
    delete mComments;
  }

#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSDeclaration\n", gInstanceCount);
#endif
}

NS_IMPL_ISUPPORTS(CSSDeclarationImpl, kICSSDeclarationIID);

nsresult CSSDeclarationImpl::GetData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aSID.Equals(kCSSFontSID)) {
    *aDataPtr = mFont;
  }
  else if (aSID.Equals(kCSSColorSID)) {
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSDisplaySID)) {
    *aDataPtr = mDisplay;
  }
  else if (aSID.Equals(kCSSTextSID)) {
    *aDataPtr = mText;
  }
  else if (aSID.Equals(kCSSMarginSID)) {
    *aDataPtr = mMargin;
  }
  else if (aSID.Equals(kCSSPositionSID)) {
    *aDataPtr = mPosition;
  }
  else if (aSID.Equals(kCSSListSID)) {
    *aDataPtr = mList;
  }
  else {
    return NS_NOINTERFACE;
  }
  return NS_OK;
}

nsresult CSSDeclarationImpl::EnsureData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aSID.Equals(kCSSFontSID)) {
    if (nsnull == mFont) {
      mFont = new nsCSSFont();
    }
    *aDataPtr = mFont;
  }
  else if (aSID.Equals(kCSSColorSID)) {
    if (nsnull == mColor) {
      mColor = new nsCSSColor();
    }
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSDisplaySID)) {
    if (nsnull == mDisplay) {
      mDisplay = new nsCSSDisplay();
    }
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSTextSID)) {
    if (nsnull == mText) {
      mText = new nsCSSText();
    }
    *aDataPtr = mText;
  }
  else if (aSID.Equals(kCSSMarginSID)) {
    if (nsnull == mMargin) {
      mMargin = new nsCSSMargin();
    }
    *aDataPtr = mMargin;
  }
  else if (aSID.Equals(kCSSPositionSID)) {
    if (nsnull == mPosition) {
      mPosition = new nsCSSPosition();
    }
    *aDataPtr = mPosition;
  }
  else if (aSID.Equals(kCSSListSID)) {
    if (nsnull == mList) {
      mList = new nsCSSList();
    }
    *aDataPtr = mList;
  }
  else {
    return NS_NOINTERFACE;
  }
  if (nsnull == *aDataPtr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult CSSDeclarationImpl::AppendValue(const char* aProperty, const nsCSSValue& aValue)
{
  return AppendValue(nsCSSProps::LookupName(aProperty), aValue);
}

nsresult CSSDeclarationImpl::AppendValue(PRInt32 aProperty, const nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
      if (nsnull == mFont) {
        mFont = new nsCSSFont();
      }
      if (nsnull != mFont) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:  mFont->mFamily = aValue;   break;
          case PROP_FONT_STYLE:   mFont->mStyle = aValue;    break;
          case PROP_FONT_VARIANT: mFont->mVariant = aValue;  break;
          case PROP_FONT_WEIGHT:  mFont->mWeight = aValue;   break;
          case PROP_FONT_SIZE:    mFont->mSize = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_CURSOR_IMAGE:
    case PROP_OPACITY:
      if (nsnull == mColor) {
        mColor = new nsCSSColor();
      }
      if (nsnull != mColor) {
        switch (aProperty) {
          case PROP_COLOR:                  mColor->mColor = aValue;           break;
          case PROP_BACKGROUND_COLOR:       mColor->mBackColor = aValue;       break;
          case PROP_BACKGROUND_IMAGE:       mColor->mBackImage = aValue;       break;
          case PROP_BACKGROUND_REPEAT:      mColor->mBackRepeat = aValue;      break;
          case PROP_BACKGROUND_ATTACHMENT:  mColor->mBackAttachment = aValue;  break;
          case PROP_BACKGROUND_X_POSITION:  mColor->mBackPositionX = aValue;   break;
          case PROP_BACKGROUND_Y_POSITION:  mColor->mBackPositionY = aValue;   break;
          case PROP_BACKGROUND_FILTER:      mColor->mBackFilter = aValue;      break;
          case PROP_CURSOR:                 mColor->mCursor = aValue;          break;
          case PROP_CURSOR_IMAGE:           mColor->mCursorImage = aValue;     break;
          case PROP_OPACITY:                mColor->mOpacity = aValue;         break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      if (nsnull == mText) {
        mText = new nsCSSText();
      }
      if (nsnull != mText) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     mText->mWordSpacing = aValue;    break;
          case PROP_LETTER_SPACING:   mText->mLetterSpacing = aValue;  break;
          case PROP_TEXT_DECORATION:  mText->mDecoration = aValue;     break;
          case PROP_VERTICAL_ALIGN:   mText->mVerticalAlign = aValue;  break;
          case PROP_TEXT_TRANSFORM:   mText->mTextTransform = aValue;  break;
          case PROP_TEXT_ALIGN:       mText->mTextAlign = aValue;      break;
          case PROP_TEXT_INDENT:      mText->mTextIndent = aValue;     break;
          case PROP_LINE_HEIGHT:      mText->mLineHeight = aValue;     break;
          case PROP_WHITE_SPACE:      mText->mWhiteSpace = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mMargin) {
          mMargin->mMargin = new nsCSSRect();
        }
        if (nsnull != mMargin->mMargin) {
          switch (aProperty) {
            case PROP_MARGIN_TOP:     mMargin->mMargin->mTop = aValue;     break;
            case PROP_MARGIN_RIGHT:   mMargin->mMargin->mRight = aValue;   break;
            case PROP_MARGIN_BOTTOM:  mMargin->mMargin->mBottom = aValue;  break;
            case PROP_MARGIN_LEFT:    mMargin->mMargin->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mPadding) {
          mMargin->mPadding = new nsCSSRect();
        }
        if (nsnull != mMargin->mPadding) {
          switch (aProperty) {
            case PROP_PADDING_TOP:    mMargin->mPadding->mTop = aValue;    break;
            case PROP_PADDING_RIGHT:  mMargin->mPadding->mRight = aValue;  break;
            case PROP_PADDING_BOTTOM: mMargin->mPadding->mBottom = aValue; break;
            case PROP_PADDING_LEFT:   mMargin->mPadding->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mBorder) {
          mMargin->mBorder = new nsCSSRect();
        }
        if (nsnull != mMargin->mBorder) {
          switch (aProperty) {
            case PROP_BORDER_TOP_WIDTH:     mMargin->mBorder->mTop = aValue;     break;
            case PROP_BORDER_RIGHT_WIDTH:   mMargin->mBorder->mRight = aValue;   break;
            case PROP_BORDER_BOTTOM_WIDTH:  mMargin->mBorder->mBottom = aValue;  break;
            case PROP_BORDER_LEFT_WIDTH:    mMargin->mBorder->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mColor) {
          mMargin->mColor = new nsCSSRect();
        }
        if (nsnull != mMargin->mColor) {
          switch (aProperty) {
            case PROP_BORDER_TOP_COLOR:     mMargin->mColor->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_COLOR:   mMargin->mColor->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_COLOR:  mMargin->mColor->mBottom = aValue; break;
            case PROP_BORDER_LEFT_COLOR:    mMargin->mColor->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mStyle) {
          mMargin->mStyle = new nsCSSRect();
        }
        if (nsnull != mMargin->mStyle) {
          switch (aProperty) {
            case PROP_BORDER_TOP_STYLE:     mMargin->mStyle->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_STYLE:   mMargin->mStyle->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_STYLE:  mMargin->mStyle->mBottom = aValue; break;
            case PROP_BORDER_LEFT_STYLE:    mMargin->mStyle->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_LEFT:
    case PROP_TOP:
    case PROP_Z_INDEX:
      if (nsnull == mPosition) {
        mPosition = new nsCSSPosition();
      }
      if (nsnull != mPosition) {
        switch (aProperty) {
          case PROP_POSITION:   mPosition->mPosition = aValue;   break;
          case PROP_WIDTH:      mPosition->mWidth = aValue;      break;
          case PROP_HEIGHT:     mPosition->mHeight = aValue;     break;
          case PROP_LEFT:       mPosition->mLeft = aValue;       break;
          case PROP_TOP:        mPosition->mTop = aValue;        break;
          case PROP_Z_INDEX:    mPosition->mZIndex = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      if (nsnull == mList) {
        mList = new nsCSSList();
      }
      if (nsnull != mList) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      mList->mType = aValue;     break;
          case PROP_LIST_STYLE_IMAGE:     mList->mImage = aValue;    break;
          case PROP_LIST_STYLE_POSITION:  mList->mPosition = aValue; break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
    case PROP_VISIBILITY:
    case PROP_OVERFLOW:
    case PROP_FILTER:
      if (nsnull == mDisplay) {
        mDisplay = new nsCSSDisplay();
      }
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case PROP_FLOAT:      mDisplay->mFloat = aValue;      break;
          case PROP_CLEAR:      mDisplay->mClear = aValue;      break;
          case PROP_DISPLAY:    mDisplay->mDisplay = aValue;    break;
          case PROP_DIRECTION:  mDisplay->mDirection = aValue;  break;
          case PROP_VISIBILITY: mDisplay->mVisibility = aValue; break;
          case PROP_OVERFLOW:   mDisplay->mOverflow = aValue;   break;
          case PROP_FILTER:     mDisplay->mFilter = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      if (nsnull == mDisplay) {
        mDisplay = new nsCSSDisplay();
      }
      if (nsnull != mDisplay) {
        if (nsnull == mDisplay->mClip) {
          mDisplay->mClip = new nsCSSRect();
        }
        if (nsnull != mDisplay->mClip) {
          switch(aProperty) {
            case PROP_CLIP_TOP:     mDisplay->mClip->mTop = aValue;     break;
            case PROP_CLIP_RIGHT:   mDisplay->mClip->mRight = aValue;   break;
            case PROP_CLIP_BOTTOM:  mDisplay->mClip->mBottom = aValue;  break;
            case PROP_CLIP_LEFT:    mDisplay->mClip->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_CLIP:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_PADDING:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = new nsVoidArray();
    }
    if (nsnull != mOrder) {
      PRInt32 index = mOrder->IndexOf((void*)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
      if (eCSSUnit_Null != aValue.GetUnit()) {
        mOrder->AppendElement((void*)aProperty);
      }
    }
  }
  return result;
}

nsresult CSSDeclarationImpl::SetValueImportant(const char* aProperty)
{
  return SetValueImportant(nsCSSProps::LookupName(aProperty));
}

nsresult CSSDeclarationImpl::SetValueImportant(PRInt32 aProperty)
{
  nsresult result = NS_OK;

  if (nsnull == mImportant) {
    mImportant = new CSSDeclarationImpl();
    if (nsnull != mImportant) {
      NS_ADDREF(mImportant);
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (NS_OK == result) {
    switch (aProperty) {
      // nsCSSFont
      case PROP_FONT_FAMILY:
      case PROP_FONT_STYLE:
      case PROP_FONT_VARIANT:
      case PROP_FONT_WEIGHT:
      case PROP_FONT_SIZE:
        if (nsnull != mFont) {
          if (nsnull == mImportant->mFont) {
            mImportant->mFont = new nsCSSFont();
          }
          if (nsnull != mImportant->mFont) {
            switch (aProperty) {
              case PROP_FONT_FAMILY:  mImportant->mFont->mFamily =  mFont->mFamily;   
                                      mFont->mFamily.Reset();       break;
              case PROP_FONT_STYLE:   mImportant->mFont->mStyle =   mFont->mStyle;
                                      mFont->mStyle.Reset();        break;
              case PROP_FONT_VARIANT: mImportant->mFont->mVariant = mFont->mVariant;
                                      mFont->mVariant.Reset();      break;
              case PROP_FONT_WEIGHT:  mImportant->mFont->mWeight =  mFont->mWeight;
                                      mFont->mWeight.Reset();       break;
              case PROP_FONT_SIZE:    mImportant->mFont->mSize =    mFont->mSize;
                                      mFont->mSize.Reset();         break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

      // nsCSSColor
      case PROP_COLOR:
      case PROP_BACKGROUND_COLOR:
      case PROP_BACKGROUND_IMAGE:
      case PROP_BACKGROUND_REPEAT:
      case PROP_BACKGROUND_ATTACHMENT:
      case PROP_BACKGROUND_X_POSITION:
      case PROP_BACKGROUND_Y_POSITION:
      case PROP_BACKGROUND_FILTER:
      case PROP_CURSOR:
      case PROP_CURSOR_IMAGE:
      case PROP_OPACITY:
        if (nsnull != mColor) {
          if (nsnull == mImportant->mColor) {
            mImportant->mColor = new nsCSSColor();
          }
          if (nsnull != mImportant->mColor) {
            switch (aProperty) {
              case PROP_COLOR:                  mImportant->mColor->mColor =          mColor->mColor;
                                                mColor->mColor.Reset();               break;
              case PROP_BACKGROUND_COLOR:       mImportant->mColor->mBackColor =      mColor->mBackColor;
                                                mColor->mBackColor.Reset();           break;
              case PROP_BACKGROUND_IMAGE:       mImportant->mColor->mBackImage =      mColor->mBackImage;
                                                mColor->mBackImage.Reset();           break;
              case PROP_BACKGROUND_REPEAT:      mImportant->mColor->mBackRepeat =     mColor->mBackRepeat;
                                                mColor->mBackRepeat.Reset();          break;
              case PROP_BACKGROUND_ATTACHMENT:  mImportant->mColor->mBackAttachment = mColor->mBackAttachment;
                                                mColor->mBackAttachment.Reset();      break;
              case PROP_BACKGROUND_X_POSITION:  mImportant->mColor->mBackPositionX =  mColor->mBackPositionX;
                                                mColor->mBackPositionX.Reset();       break;
              case PROP_BACKGROUND_Y_POSITION:  mImportant->mColor->mBackPositionY =  mColor->mBackPositionY;
                                                mColor->mBackPositionY.Reset();       break;
              case PROP_BACKGROUND_FILTER:      mImportant->mColor->mBackFilter =     mColor->mBackFilter;
                                                mColor->mBackFilter.Reset();          break;
              case PROP_CURSOR:                 mImportant->mColor->mCursor =         mColor->mCursor;
                                                mColor->mCursor.Reset();              break;
              case PROP_CURSOR_IMAGE:           mImportant->mColor->mCursorImage =    mColor->mCursorImage;
                                                mColor->mCursorImage.Reset();         break;
              case PROP_OPACITY:                mImportant->mColor->mOpacity =        mColor->mOpacity;
                                                mColor->mOpacity.Reset();             break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

      // nsCSSText
      case PROP_WORD_SPACING:
      case PROP_LETTER_SPACING:
      case PROP_TEXT_DECORATION:
      case PROP_VERTICAL_ALIGN:
      case PROP_TEXT_TRANSFORM:
      case PROP_TEXT_ALIGN:
      case PROP_TEXT_INDENT:
      case PROP_LINE_HEIGHT:
      case PROP_WHITE_SPACE:
        if (nsnull != mText) {
          if (nsnull == mImportant->mText) {
            mImportant->mText = new nsCSSText();
          }
          if (nsnull != mImportant->mText) {
            switch (aProperty) {
              case PROP_WORD_SPACING:     mImportant->mText->mWordSpacing =   mText->mWordSpacing;
                                          mText->mWordSpacing.Reset();        break;
              case PROP_LETTER_SPACING:   mImportant->mText->mLetterSpacing = mText->mLetterSpacing;
                                          mText->mLetterSpacing.Reset();      break;
              case PROP_TEXT_DECORATION:  mImportant->mText->mDecoration =    mText->mDecoration;
                                          mText->mDecoration.Reset();         break;
              case PROP_VERTICAL_ALIGN:   mImportant->mText->mVerticalAlign = mText->mVerticalAlign;
                                          mText->mVerticalAlign.Reset();      break;
              case PROP_TEXT_TRANSFORM:   mImportant->mText->mTextTransform = mText->mTextTransform;
                                          mText->mTextTransform.Reset();      break;
              case PROP_TEXT_ALIGN:       mImportant->mText->mTextAlign =     mText->mTextAlign;
                                          mText->mTextAlign.Reset();          break;
              case PROP_TEXT_INDENT:      mImportant->mText->mTextIndent =    mText->mTextIndent;
                                          mText->mTextIndent.Reset();         break;
              case PROP_LINE_HEIGHT:      mImportant->mText->mLineHeight =    mText->mLineHeight;
                                          mText->mLineHeight.Reset();         break;
              case PROP_WHITE_SPACE:      mImportant->mText->mWhiteSpace =    mText->mWhiteSpace;
                                          mText->mWhiteSpace.Reset();         break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

      // nsCSSMargin
      case PROP_MARGIN_TOP:
      case PROP_MARGIN_RIGHT:
      case PROP_MARGIN_BOTTOM:
      case PROP_MARGIN_LEFT:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mMargin) {
            if (nsnull == mImportant->mMargin) {
              mImportant->mMargin = new nsCSSMargin();
            }
            if (nsnull != mImportant->mMargin) {
              if (nsnull == mImportant->mMargin->mMargin) {
                mImportant->mMargin->mMargin = new nsCSSRect();
              }
              if (nsnull != mImportant->mMargin->mMargin) {
                switch (aProperty) {
                  case PROP_MARGIN_TOP:     mImportant->mMargin->mMargin->mTop =    mMargin->mMargin->mTop;
                                            mMargin->mMargin->mTop.Reset();         break;
                  case PROP_MARGIN_RIGHT:   mImportant->mMargin->mMargin->mRight =  mMargin->mMargin->mRight;
                                            mMargin->mMargin->mRight.Reset();       break;
                  case PROP_MARGIN_BOTTOM:  mImportant->mMargin->mMargin->mBottom = mMargin->mMargin->mBottom;
                                            mMargin->mMargin->mBottom.Reset();      break;
                  case PROP_MARGIN_LEFT:    mImportant->mMargin->mMargin->mLeft =   mMargin->mMargin->mLeft;
                                            mMargin->mMargin->mLeft.Reset();        break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      case PROP_PADDING_TOP:
      case PROP_PADDING_RIGHT:
      case PROP_PADDING_BOTTOM:
      case PROP_PADDING_LEFT:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mPadding) {
            if (nsnull == mImportant->mMargin) {
              mImportant->mMargin = new nsCSSMargin();
            }
            if (nsnull != mImportant->mMargin) {
              if (nsnull == mImportant->mMargin->mPadding) {
                mImportant->mMargin->mPadding = new nsCSSRect();
              }
              if (nsnull != mImportant->mMargin->mPadding) {
                switch (aProperty) {
                  case PROP_PADDING_TOP:    mImportant->mMargin->mPadding->mTop =     mMargin->mPadding->mTop;
                                            mMargin->mPadding->mTop.Reset();          break;
                  case PROP_PADDING_RIGHT:  mImportant->mMargin->mPadding->mRight =   mMargin->mPadding->mRight;
                                            mMargin->mPadding->mRight.Reset();        break;
                  case PROP_PADDING_BOTTOM: mImportant->mMargin->mPadding->mBottom =  mMargin->mPadding->mBottom;
                                            mMargin->mPadding->mBottom.Reset();       break;
                  case PROP_PADDING_LEFT:   mImportant->mMargin->mPadding->mLeft =    mMargin->mPadding->mLeft;
                                            mMargin->mPadding->mLeft.Reset();         break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      case PROP_BORDER_TOP_WIDTH:
      case PROP_BORDER_RIGHT_WIDTH:
      case PROP_BORDER_BOTTOM_WIDTH:
      case PROP_BORDER_LEFT_WIDTH:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorder) {
            if (nsnull == mImportant->mMargin) {
              mImportant->mMargin = new nsCSSMargin();
            }
            if (nsnull != mImportant->mMargin) {
              if (nsnull == mImportant->mMargin->mBorder) {
                mImportant->mMargin->mBorder = new nsCSSRect();
              }
              if (nsnull != mImportant->mMargin->mBorder) {
                switch (aProperty) {
                  case PROP_BORDER_TOP_WIDTH:     mImportant->mMargin->mBorder->mTop =    mMargin->mBorder->mTop;
                                                  mMargin->mBorder->mTop.Reset();         break;
                  case PROP_BORDER_RIGHT_WIDTH:   mImportant->mMargin->mBorder->mRight =  mMargin->mBorder->mRight;
                                                  mMargin->mBorder->mRight.Reset();       break;
                  case PROP_BORDER_BOTTOM_WIDTH:  mImportant->mMargin->mBorder->mBottom = mMargin->mBorder->mBottom;
                                                  mMargin->mBorder->mBottom.Reset();      break;
                  case PROP_BORDER_LEFT_WIDTH:    mImportant->mMargin->mBorder->mLeft =   mMargin->mBorder->mLeft;
                                                  mMargin->mBorder->mLeft.Reset();        break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      case PROP_BORDER_TOP_COLOR:
      case PROP_BORDER_RIGHT_COLOR:
      case PROP_BORDER_BOTTOM_COLOR:
      case PROP_BORDER_LEFT_COLOR:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mColor) {
            if (nsnull == mImportant->mMargin) {
              mImportant->mMargin = new nsCSSMargin();
            }
            if (nsnull != mImportant->mMargin) {
              if (nsnull == mImportant->mMargin->mColor) {
                mImportant->mMargin->mColor = new nsCSSRect();
              }
              if (nsnull != mImportant->mMargin->mColor) {
                switch (aProperty) {
                  case PROP_BORDER_TOP_COLOR:     mImportant->mMargin->mColor->mTop   =   mMargin->mColor->mTop;
                                                  mMargin->mColor->mTop.Reset();          break;
                  case PROP_BORDER_RIGHT_COLOR:   mImportant->mMargin->mColor->mRight =   mMargin->mColor->mRight;
                                                  mMargin->mColor->mRight.Reset();        break;
                  case PROP_BORDER_BOTTOM_COLOR:  mImportant->mMargin->mColor->mBottom =  mMargin->mColor->mBottom;
                                                  mMargin->mColor->mBottom.Reset();       break;
                  case PROP_BORDER_LEFT_COLOR:    mImportant->mMargin->mColor->mLeft =    mMargin->mColor->mLeft;
                                                  mMargin->mColor->mLeft.Reset();         break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      case PROP_BORDER_TOP_STYLE:
      case PROP_BORDER_RIGHT_STYLE:
      case PROP_BORDER_BOTTOM_STYLE:
      case PROP_BORDER_LEFT_STYLE:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mStyle) {
            if (nsnull == mImportant->mMargin) {
              mImportant->mMargin = new nsCSSMargin();
            }
            if (nsnull != mImportant->mMargin) {
              if (nsnull == mImportant->mMargin->mStyle) {
                mImportant->mMargin->mStyle = new nsCSSRect();
              }
              if (nsnull != mImportant->mMargin->mStyle) {
                switch (aProperty) {
                  case PROP_BORDER_TOP_STYLE:     mImportant->mMargin->mStyle->mTop =     mMargin->mStyle->mTop;
                                                  mMargin->mStyle->mTop.Reset();          break;
                  case PROP_BORDER_RIGHT_STYLE:   mImportant->mMargin->mStyle->mRight =   mMargin->mStyle->mRight;
                                                  mMargin->mStyle->mRight.Reset();        break;
                  case PROP_BORDER_BOTTOM_STYLE:  mImportant->mMargin->mStyle->mBottom =  mMargin->mStyle->mBottom;
                                                  mMargin->mStyle->mBottom.Reset();       break;
                  case PROP_BORDER_LEFT_STYLE:    mImportant->mMargin->mStyle->mLeft =    mMargin->mStyle->mLeft;
                                                  mMargin->mStyle->mLeft.Reset();         break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      // nsCSSPosition
      case PROP_POSITION:
      case PROP_WIDTH:
      case PROP_HEIGHT:
      case PROP_LEFT:
      case PROP_TOP:
      case PROP_Z_INDEX:
        if (nsnull != mPosition) {
          if (nsnull == mImportant->mPosition) {
            mImportant->mPosition = new nsCSSPosition();
          }
          if (nsnull != mImportant->mPosition) {
            switch (aProperty) {
              case PROP_POSITION:   mImportant->mPosition->mPosition =  mPosition->mPosition;
                                    mPosition->mPosition.Reset();       break;
              case PROP_WIDTH:      mImportant->mPosition->mWidth =     mPosition->mWidth;
                                    mPosition->mWidth.Reset();          break;
              case PROP_HEIGHT:     mImportant->mPosition->mHeight =    mPosition->mHeight;
                                    mPosition->mHeight.Reset();         break;
              case PROP_LEFT:       mImportant->mPosition->mLeft =      mPosition->mLeft;
                                    mPosition->mLeft.Reset();           break;
              case PROP_TOP:        mImportant->mPosition->mTop =       mPosition->mTop;
                                    mPosition->mTop.Reset();            break;
              case PROP_Z_INDEX:    mImportant->mPosition->mZIndex =    mPosition->mZIndex;
                                    mPosition->mZIndex.Reset();         break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

        // nsCSSList
      case PROP_LIST_STYLE_TYPE:
      case PROP_LIST_STYLE_IMAGE:
      case PROP_LIST_STYLE_POSITION:
        if (nsnull != mList) {
          if (nsnull == mImportant->mList) {
            mImportant->mList = new nsCSSList();
          }
          if (nsnull != mImportant->mList) {
            switch (aProperty) {
              case PROP_LIST_STYLE_TYPE:      mImportant->mList->mType =      mList->mType;
                                              mList->mType.Reset();           break;
              case PROP_LIST_STYLE_IMAGE:     mImportant->mList->mImage =     mList->mImage;
                                              mList->mImage.Reset();          break;
              case PROP_LIST_STYLE_POSITION:  mImportant->mList->mPosition =  mList->mPosition;
                                              mList->mPosition.Reset();       break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

        // nsCSSDisplay
      case PROP_FLOAT:
      case PROP_CLEAR:
      case PROP_DISPLAY:
      case PROP_DIRECTION:
      case PROP_VISIBILITY:
      case PROP_OVERFLOW:
      case PROP_FILTER:
        if (nsnull != mDisplay) {
          if (nsnull == mImportant->mDisplay) {
            mImportant->mDisplay = new nsCSSDisplay();
          }
          if (nsnull != mImportant->mDisplay) {
            switch (aProperty) {
              case PROP_FLOAT:      mImportant->mDisplay->mFloat =      mDisplay->mFloat;
                                    mDisplay->mFloat.Reset();           break;
              case PROP_CLEAR:      mImportant->mDisplay->mClear =      mDisplay->mClear;
                                    mDisplay->mClear.Reset();           break;
              case PROP_DISPLAY:    mImportant->mDisplay->mDisplay =    mDisplay->mDisplay;
                                    mDisplay->mDisplay.Reset();         break;
              case PROP_DIRECTION:  mImportant->mDisplay->mDirection =  mDisplay->mDirection;
                                    mDisplay->mDirection.Reset();       break;
              case PROP_VISIBILITY: mImportant->mDisplay->mVisibility = mDisplay->mVisibility;
                                    mDisplay->mVisibility.Reset();      break;
              case PROP_OVERFLOW:   mImportant->mDisplay->mOverflow =   mDisplay->mOverflow;
                                    mDisplay->mOverflow.Reset();        break;
              case PROP_FILTER:     mImportant->mDisplay->mFilter =     mDisplay->mFilter;
                                    mDisplay->mFilter.Reset();          break;
            }
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;

      case PROP_CLIP_TOP:
      case PROP_CLIP_RIGHT:
      case PROP_CLIP_BOTTOM:
      case PROP_CLIP_LEFT:
        if (nsnull != mDisplay) {
          if (nsnull != mDisplay->mClip) {
            if (nsnull == mImportant->mDisplay) {
              mImportant->mDisplay = new nsCSSDisplay();
            }
            if (nsnull != mImportant->mDisplay) {
              if (nsnull == mImportant->mDisplay->mClip) {
                mImportant->mDisplay->mClip = new nsCSSRect();
              }
              if (nsnull != mImportant->mDisplay->mClip) {
                switch(aProperty) {
                  case PROP_CLIP_TOP:     mImportant->mDisplay->mClip->mTop =     mDisplay->mClip->mTop;
                                          mDisplay->mClip->mTop.Reset();          break;
                  case PROP_CLIP_RIGHT:   mImportant->mDisplay->mClip->mRight =   mDisplay->mClip->mRight;
                                          mDisplay->mClip->mRight.Reset();        break;
                  case PROP_CLIP_BOTTOM:  mImportant->mDisplay->mClip->mBottom =  mDisplay->mClip->mBottom;
                                          mDisplay->mClip->mBottom.Reset();       break;
                  case PROP_CLIP_LEFT:    mImportant->mDisplay->mClip->mLeft =    mDisplay->mClip->mLeft;
                                          mDisplay->mClip->mLeft.Reset();         break;
                }
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
        }
        break;

      case PROP_BACKGROUND:
        SetValueImportant(PROP_BACKGROUND_COLOR);
        SetValueImportant(PROP_BACKGROUND_IMAGE);
        SetValueImportant(PROP_BACKGROUND_REPEAT);
        SetValueImportant(PROP_BACKGROUND_ATTACHMENT);
        SetValueImportant(PROP_BACKGROUND_X_POSITION);
        SetValueImportant(PROP_BACKGROUND_Y_POSITION);
        SetValueImportant(PROP_BACKGROUND_FILTER);
        break;
      case PROP_BORDER:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_CLIP:
        SetValueImportant(PROP_CLIP_TOP);
        SetValueImportant(PROP_CLIP_RIGHT);
        SetValueImportant(PROP_CLIP_BOTTOM);
        SetValueImportant(PROP_CLIP_LEFT);
        break;
      case PROP_FONT:
        SetValueImportant(PROP_FONT_FAMILY);
        SetValueImportant(PROP_FONT_STYLE);
        SetValueImportant(PROP_FONT_VARIANT);
        SetValueImportant(PROP_FONT_WEIGHT);
        SetValueImportant(PROP_FONT_SIZE);
        SetValueImportant(PROP_LINE_HEIGHT);
        break;
      case PROP_LIST_STYLE:
        SetValueImportant(PROP_LIST_STYLE_TYPE);
        SetValueImportant(PROP_LIST_STYLE_IMAGE);
        SetValueImportant(PROP_LIST_STYLE_POSITION);
        break;
      case PROP_MARGIN:
        SetValueImportant(PROP_MARGIN_TOP);
        SetValueImportant(PROP_MARGIN_RIGHT);
        SetValueImportant(PROP_MARGIN_BOTTOM);
        SetValueImportant(PROP_MARGIN_LEFT);
        break;
      case PROP_PADDING:
        SetValueImportant(PROP_PADDING_TOP);
        SetValueImportant(PROP_PADDING_RIGHT);
        SetValueImportant(PROP_PADDING_BOTTOM);
        SetValueImportant(PROP_PADDING_LEFT);
        break;
      case PROP_BACKGROUND_POSITION:
        SetValueImportant(PROP_BACKGROUND_X_POSITION);
        SetValueImportant(PROP_BACKGROUND_Y_POSITION);
        break;
      case PROP_BORDER_TOP:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        break;
      case PROP_BORDER_RIGHT:
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        break;
      case PROP_BORDER_BOTTOM:
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        break;
      case PROP_BORDER_LEFT:
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_BORDER_COLOR:
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_BORDER_STYLE:
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        break;
      case PROP_BORDER_WIDTH:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        break;
      default:
        result = NS_ERROR_ILLEGAL_VALUE;
        break;
    }
  }
  return result;
}

nsresult CSSDeclarationImpl::AppendComment(const nsString& aComment)
{
  nsresult result = NS_ERROR_OUT_OF_MEMORY;

  if (nsnull == mOrder) {
    mOrder = new nsVoidArray();
  }
  if (nsnull == mComments) {
    mComments = new nsVoidArray();
  }
  if ((nsnull != mComments) && (nsnull != mOrder)) {
    mComments->AppendElement(new nsString(aComment));
    mOrder->AppendElement((void*)-mComments->Count());
    result = NS_OK;
  }
  return result;
}

nsresult CSSDeclarationImpl::GetValue(const char* aProperty, nsCSSValue& aValue)
{
  return GetValue(nsCSSProps::LookupName(aProperty), aValue);
}

nsresult CSSDeclarationImpl::GetValue(PRInt32 aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
      if (nsnull != mFont) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:  aValue = mFont->mFamily;   break;
          case PROP_FONT_STYLE:   aValue = mFont->mStyle;    break;
          case PROP_FONT_VARIANT: aValue = mFont->mVariant;  break;
          case PROP_FONT_WEIGHT:  aValue = mFont->mWeight;   break;
          case PROP_FONT_SIZE:    aValue = mFont->mSize;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_CURSOR_IMAGE:
    case PROP_OPACITY:
      if (nsnull != mColor) {
        switch (aProperty) {
          case PROP_COLOR:                  aValue = mColor->mColor;           break;
          case PROP_BACKGROUND_COLOR:       aValue = mColor->mBackColor;       break;
          case PROP_BACKGROUND_IMAGE:       aValue = mColor->mBackImage;       break;
          case PROP_BACKGROUND_REPEAT:      aValue = mColor->mBackRepeat;      break;
          case PROP_BACKGROUND_ATTACHMENT:  aValue = mColor->mBackAttachment;  break;
          case PROP_BACKGROUND_X_POSITION:  aValue = mColor->mBackPositionX;   break;
          case PROP_BACKGROUND_Y_POSITION:  aValue = mColor->mBackPositionY;   break;
          case PROP_BACKGROUND_FILTER:      aValue = mColor->mBackFilter;      break;
          case PROP_CURSOR:                 aValue = mColor->mCursor;          break;
          case PROP_CURSOR_IMAGE:           aValue = mColor->mCursorImage;     break;
          case PROP_OPACITY:                aValue = mColor->mOpacity;         break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      if (nsnull != mText) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     aValue = mText->mWordSpacing;    break;
          case PROP_LETTER_SPACING:   aValue = mText->mLetterSpacing;  break;
          case PROP_TEXT_DECORATION:  aValue = mText->mDecoration;     break;
          case PROP_VERTICAL_ALIGN:   aValue = mText->mVerticalAlign;  break;
          case PROP_TEXT_TRANSFORM:   aValue = mText->mTextTransform;  break;
          case PROP_TEXT_ALIGN:       aValue = mText->mTextAlign;      break;
          case PROP_TEXT_INDENT:      aValue = mText->mTextIndent;     break;
          case PROP_LINE_HEIGHT:      aValue = mText->mLineHeight;     break;
          case PROP_WHITE_SPACE:      aValue = mText->mWhiteSpace;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mMargin)) {
        switch (aProperty) {
          case PROP_MARGIN_TOP:     aValue = mMargin->mMargin->mTop;     break;
          case PROP_MARGIN_RIGHT:   aValue = mMargin->mMargin->mRight;   break;
          case PROP_MARGIN_BOTTOM:  aValue = mMargin->mMargin->mBottom;  break;
          case PROP_MARGIN_LEFT:    aValue = mMargin->mMargin->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mPadding)) {
        switch (aProperty) {
          case PROP_PADDING_TOP:    aValue = mMargin->mPadding->mTop;    break;
          case PROP_PADDING_RIGHT:  aValue = mMargin->mPadding->mRight;  break;
          case PROP_PADDING_BOTTOM: aValue = mMargin->mPadding->mBottom; break;
          case PROP_PADDING_LEFT:   aValue = mMargin->mPadding->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorder)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_WIDTH:     aValue = mMargin->mBorder->mTop;     break;
          case PROP_BORDER_RIGHT_WIDTH:   aValue = mMargin->mBorder->mRight;   break;
          case PROP_BORDER_BOTTOM_WIDTH:  aValue = mMargin->mBorder->mBottom;  break;
          case PROP_BORDER_LEFT_WIDTH:    aValue = mMargin->mBorder->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      if ((nsnull != mMargin) && (nsnull != mMargin->mColor)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_COLOR:     aValue = mMargin->mColor->mTop;    break;
          case PROP_BORDER_RIGHT_COLOR:   aValue = mMargin->mColor->mRight;  break;
          case PROP_BORDER_BOTTOM_COLOR:  aValue = mMargin->mColor->mBottom; break;
          case PROP_BORDER_LEFT_COLOR:    aValue = mMargin->mColor->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      if ((nsnull != mMargin) && (nsnull != mMargin->mStyle)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_STYLE:     aValue = mMargin->mStyle->mTop;    break;
          case PROP_BORDER_RIGHT_STYLE:   aValue = mMargin->mStyle->mRight;  break;
          case PROP_BORDER_BOTTOM_STYLE:  aValue = mMargin->mStyle->mBottom; break;
          case PROP_BORDER_LEFT_STYLE:    aValue = mMargin->mStyle->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_LEFT:
    case PROP_TOP:
    case PROP_Z_INDEX:
      if (nsnull != mPosition) {
        switch (aProperty) {
          case PROP_POSITION:   aValue = mPosition->mPosition;   break;
          case PROP_WIDTH:      aValue = mPosition->mWidth;      break;
          case PROP_HEIGHT:     aValue = mPosition->mHeight;     break;
          case PROP_LEFT:       aValue = mPosition->mLeft;       break;
          case PROP_TOP:        aValue = mPosition->mTop;        break;
          case PROP_Z_INDEX:    aValue = mPosition->mZIndex;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      if (nsnull != mList) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      aValue = mList->mType;     break;
          case PROP_LIST_STYLE_IMAGE:     aValue = mList->mImage;    break;
          case PROP_LIST_STYLE_POSITION:  aValue = mList->mPosition; break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
    case PROP_VISIBILITY:
    case PROP_OVERFLOW:
    case PROP_FILTER:
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case PROP_FLOAT:      aValue = mDisplay->mFloat;      break;
          case PROP_CLEAR:      aValue = mDisplay->mClear;      break;
          case PROP_DISPLAY:    aValue = mDisplay->mDisplay;    break;
          case PROP_DIRECTION:  aValue = mDisplay->mDirection;  break;
          case PROP_VISIBILITY: aValue = mDisplay->mVisibility; break;
          case PROP_OVERFLOW:   aValue = mDisplay->mOverflow;   break;
          case PROP_FILTER:     aValue = mDisplay->mFilter;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      if ((nsnull != mDisplay) && (nsnull != mDisplay->mClip)) {
        switch(aProperty) {
          case PROP_CLIP_TOP:     aValue = mDisplay->mClip->mTop;     break;
          case PROP_CLIP_RIGHT:   aValue = mDisplay->mClip->mRight;   break;
          case PROP_CLIP_BOTTOM:  aValue = mDisplay->mClip->mBottom;  break;
          case PROP_CLIP_LEFT:    aValue = mDisplay->mClip->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_CLIP:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_PADDING:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return result;
}

nsresult CSSDeclarationImpl::GetImportantValues(nsICSSDeclaration*& aResult)
{
  if (nsnull != mImportant) {
    aResult = mImportant;
    NS_ADDREF(aResult);
  }
  else {
    aResult = nsnull;
  }
  return NS_OK;
}

nsresult CSSDeclarationImpl::GetValueIsImportant(const char *aProperty,
                                                 PRBool& aIsImportant)
{
  nsCSSValue val;

  if (nsnull != mImportant) {
    mImportant->GetValue(aProperty, val);
    if (eCSSUnit_Null != val.GetUnit()) {
      aIsImportant = PR_TRUE;
    }
    else {
      aIsImportant = PR_FALSE;
    }
  }
  else {
    aIsImportant = PR_FALSE;
  }

  return NS_OK;
}

nsresult CSSDeclarationImpl::ToString(nsString& aString)
{
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      PRInt32 property = (PRInt32)mOrder->ElementAt(index);
      if (0 <= property) {
        nsCSSValue  value;
        PRBool  important = PR_FALSE;
        // check for important value
        if (nsnull != mImportant) {
          mImportant->GetValue(property, value);
          if (eCSSUnit_Null != value.GetUnit()) {
            important = PR_TRUE;
          }
          else {
            GetValue(property, value);
          }
        }
        else {
          GetValue(property, value);
        }
        aString.Append(nsCSSProps::kNameTable[property].name);
        aString.Append(": ");
        nsCSSValue::ValueToString(aString, value, property);
        if (PR_TRUE == important) {
          aString.Append(" ! important");
        }
        aString.Append("; ");
      }
      else {  // is comment
        aString.Append("/* ");
        nsString* comment = (nsString*)mComments->ElementAt((-1) - property);
        aString.Append(*comment);
        aString.Append(" */ ");
      }
    }
  }
  return NS_OK;
}

void CSSDeclarationImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);

  if (nsnull != mFont) {
    mFont->List(out);
  }
  if (nsnull != mColor) {
    mColor->List(out);
  }
  if (nsnull != mText) {
    mText->List(out);
  }
  if (nsnull != mMargin) {
    mMargin->List(out);
  }
  if (nsnull != mPosition) {
    mPosition->List(out);
  }
  if (nsnull != mList) {
    mList->List(out);
  }
  if (nsnull != mDisplay) {
    mDisplay->List(out);
  }

  fputs("}", out);

  if (nsnull != mImportant) {
    fputs(" ! important ", out);
    mImportant->List(out, 0);
  }
}

nsresult
CSSDeclarationImpl::Count(PRUint32* aCount)
{
  if (nsnull != mOrder) {
    *aCount = (PRUint32)mOrder->Count();
  }
  else {
    *aCount = 0;
  }
  
  return NS_OK;
}

nsresult
CSSDeclarationImpl::GetNthProperty(PRUint32 aIndex, nsString& aReturn)
{
  aReturn.SetLength(0);
  if (nsnull != mOrder) {
    PRInt32 property = (PRInt32)mOrder->ElementAt(aIndex);
    if (0 <= property) {
      aReturn.Append(nsCSSProps::kNameTable[property].name);
    }
  }
  
  return NS_OK;
}


NS_HTML nsresult
  NS_NewCSSDeclaration(nsICSSDeclaration** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSDeclarationImpl  *it = new CSSDeclarationImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSDeclarationIID, (void **) aInstancePtrResult);
}




/*
font 
===========
font-family: string (list)
font-style: enum
font-variant: enum (ie: small caps)
font-weight: enum
font-size: abs, pct, enum, +-1


color/background
=============
color: color
background-color: color
background-image: url(string)
background-repeat: enum 
background-attachment: enum
background-position-x -y: abs, pct, enum (left/top center right/bottom (pct?))
cursor: enum
cursor-image: url(string)


text
=======
word-spacing: abs, "normal"
letter-spacing: abs, "normal"
text-decoration: enum
vertical-align: enum, pct
text-transform: enum
text-align: enum
text-indent: abs, pct
line-height: "normal", abs, pct, number-factor
white-space: enum

margin
=======
margin-top -right -bottom -left: "auto", abs, pct
padding-top -right -bottom -left: abs, pct
border-top -right -bottom -left-width: enum, abs
border-top -right -bottom -left-color: color
border-top -right -bottom -left-style: enum

size
=======
position: enum
width: abs, pct, "auto"
height: abs, pct, "auto"
left: abs, pct, "auto"
top: abs, pct, "auto"
clip: shape, "auto" (shape: rect - abs, auto)
overflow: enum
z-index: int, auto
visibity: enum

display
=======
float: enum
clear: enum
direction: enum
display: enum

filter: string

list
========
list-style-type: enum
list-style-image: url, "none"
list-style-position: enum (bool? in/out)

*/



