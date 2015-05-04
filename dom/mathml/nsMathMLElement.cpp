/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLElement.h"
#include "base/compiler_specific.h"
#include "mozilla/ArrayUtils.h"
#include "nsGkAtoms.h"
#include "nsCRT.h"
#include "nsLayoutStylesheetCache.h"
#include "nsRuleData.h"
#include "nsCSSValue.h"
#include "nsCSSParser.h"
#include "nsMappedAttributes.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "mozAutoDocUpdate.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsIURI.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/ElementBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ISUPPORTS_INHERITED(nsMathMLElement, nsMathMLElementBase,
                            nsIDOMElement, nsIDOMNode, Link)

static nsresult
WarnDeprecated(const char16_t* aDeprecatedAttribute, 
               const char16_t* aFavoredAttribute, nsIDocument* aDocument)
{
  const char16_t *argv[] = 
    { aDeprecatedAttribute, aFavoredAttribute };
  return nsContentUtils::
          ReportToConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_CSTRING("MathML"), aDocument,
                          nsContentUtils::eMATHML_PROPERTIES,
                          "DeprecatedSupersededBy", argv, 2);
}

static nsresult 
ReportLengthParseError(const nsString& aValue, nsIDocument* aDocument)
{
  const char16_t *arg = aValue.get();
  return nsContentUtils::
         ReportToConsole(nsIScriptError::errorFlag,
                         NS_LITERAL_CSTRING("MathML"), aDocument,
                         nsContentUtils::eMATHML_PROPERTIES,
                         "LengthParsingError", &arg, 1);
}

static nsresult
ReportParseErrorNoTag(const nsString& aValue, 
                      nsIAtom*        aAtom,
                      nsIDocument*    aDocument)
{
  const char16_t *argv[] = 
    { aValue.get(), aAtom->GetUTF16String() };
  return nsContentUtils::
         ReportToConsole(nsIScriptError::errorFlag,
                         NS_LITERAL_CSTRING("MathML"), aDocument,
                         nsContentUtils::eMATHML_PROPERTIES,
                         "AttributeParsingErrorNoTag", argv, 2);
}

nsMathMLElement::nsMathMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
: nsMathMLElementBase(aNodeInfo),
  ALLOW_THIS_IN_INITIALIZER_LIST(Link(this)),
  mIncrementScriptLevel(false)
{
}

nsMathMLElement::nsMathMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
: nsMathMLElementBase(aNodeInfo),
  ALLOW_THIS_IN_INITIALIZER_LIST(Link(this)),
  mIncrementScriptLevel(false)
{
}

nsresult
nsMathMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = nsMathMLElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    aDocument->RegisterPendingLinkUpdate(this);
  }

  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    if (!doc->GetMathMLEnabled()) {
      // Enable MathML and setup the style sheet during binding, not element
      // construction, because we could move a MathML element from the document
      // that created it to another document.
      doc->SetMathMLEnabled();
      doc->
        EnsureOnDemandBuiltInUASheet(nsLayoutStylesheetCache::MathMLSheet());

      // Rebuild style data for the presshell, because style system
      // optimizations may have taken place assuming MathML was disabled.
      // (See nsRuleNode::CheckSpecifiedProperties.)
      nsCOMPtr<nsIPresShell> shell = doc->GetShell();
      if (shell) {
        shell->GetPresContext()->
          PostRebuildAllStyleDataEvent(nsChangeHint(0), eRestyle_Subtree);
      }
    }
  }

  return rv;
}

void
nsMathMLElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false, Link::ElementHasHref());
  
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    doc->UnregisterPendingLinkUpdate(this);
  }

  nsMathMLElementBase::UnbindFromTree(aDeep, aNullParent);
}

bool
nsMathMLElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (IsMathMLElement(nsGkAtoms::math) && aAttribute == nsGkAtoms::mode) {
      WarnDeprecated(nsGkAtoms::mode->GetUTF16String(),
                     nsGkAtoms::display->GetUTF16String(), OwnerDoc());
    }
    if (aAttribute == nsGkAtoms::color) {
      WarnDeprecated(nsGkAtoms::color->GetUTF16String(),
                     nsGkAtoms::mathcolor_->GetUTF16String(), OwnerDoc());
    }
    if (aAttribute == nsGkAtoms::color ||
        aAttribute == nsGkAtoms::mathcolor_ ||
        aAttribute == nsGkAtoms::background ||
        aAttribute == nsGkAtoms::mathbackground_) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsMathMLElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                             aValue, aResult);
}

static Element::MappedAttributeEntry sMtableStyles[] = {
  { &nsGkAtoms::width },
  { nullptr }
};

static Element::MappedAttributeEntry sTokenStyles[] = {
  { &nsGkAtoms::mathsize_ },
  { &nsGkAtoms::fontsize_ },
  { &nsGkAtoms::color },
  { &nsGkAtoms::fontfamily_ },
  { &nsGkAtoms::fontstyle_ },
  { &nsGkAtoms::fontweight_ },
  { &nsGkAtoms::mathvariant_},
  { nullptr }
};

static Element::MappedAttributeEntry sEnvironmentStyles[] = {
  { &nsGkAtoms::scriptlevel_ },
  { &nsGkAtoms::scriptminsize_ },
  { &nsGkAtoms::scriptsizemultiplier_ },
  { &nsGkAtoms::background },
  { nullptr }
};

static Element::MappedAttributeEntry sCommonPresStyles[] = {
  { &nsGkAtoms::mathcolor_ },
  { &nsGkAtoms::mathbackground_ },
  { nullptr }
};

static Element::MappedAttributeEntry sDirStyles[] = {
  { &nsGkAtoms::dir },
  { nullptr }
};

bool
nsMathMLElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const mtableMap[] = {
    sMtableStyles,
    sCommonPresStyles
  };
  static const MappedAttributeEntry* const tokenMap[] = {
    sTokenStyles,
    sCommonPresStyles,
    sDirStyles
  };
  static const MappedAttributeEntry* const mstyleMap[] = {
    sTokenStyles,
    sEnvironmentStyles,
    sCommonPresStyles,
    sDirStyles
  };
  static const MappedAttributeEntry* const commonPresMap[] = {
    sCommonPresStyles
  };
  static const MappedAttributeEntry* const mrowMap[] = {
    sCommonPresStyles,
    sDirStyles
  };

  // We don't support mglyph (yet).
  if (IsAnyOfMathMLElements(nsGkAtoms::ms_, nsGkAtoms::mi_, nsGkAtoms::mn_,
                            nsGkAtoms::mo_, nsGkAtoms::mtext_,
                            nsGkAtoms::mspace_))
    return FindAttributeDependence(aAttribute, tokenMap);
  if (IsAnyOfMathMLElements(nsGkAtoms::mstyle_, nsGkAtoms::math))
    return FindAttributeDependence(aAttribute, mstyleMap);

  if (IsMathMLElement(nsGkAtoms::mtable_))
    return FindAttributeDependence(aAttribute, mtableMap);

  if (IsMathMLElement(nsGkAtoms::mrow_))
    return FindAttributeDependence(aAttribute, mrowMap);

  if (IsAnyOfMathMLElements(nsGkAtoms::maction_,
                            nsGkAtoms::maligngroup_,
                            nsGkAtoms::malignmark_,
                            nsGkAtoms::menclose_,
                            nsGkAtoms::merror_,
                            nsGkAtoms::mfenced_,
                            nsGkAtoms::mfrac_,
                            nsGkAtoms::mover_,
                            nsGkAtoms::mpadded_,
                            nsGkAtoms::mphantom_,
                            nsGkAtoms::mprescripts_,
                            nsGkAtoms::mroot_,
                            nsGkAtoms::msqrt_,
                            nsGkAtoms::msub_,
                            nsGkAtoms::msubsup_,
                            nsGkAtoms::msup_,
                            nsGkAtoms::mtd_,
                            nsGkAtoms::mtr_,
                            nsGkAtoms::munder_,
                            nsGkAtoms::munderover_,
                            nsGkAtoms::none)) {
    return FindAttributeDependence(aAttribute, commonPresMap);
  }

  return false;
}

nsMapRuleToAttributesFunc
nsMathMLElement::GetAttributeMappingFunction() const
{
  // It doesn't really matter what our tag is here, because only attributes
  // that satisfy IsAttributeMapped will be stored in the mapped attributes
  // list and available to the mapping function
  return &MapMathMLAttributesInto;
}

/* static */ bool
nsMathMLElement::ParseNamedSpaceValue(const nsString& aString,
                                      nsCSSValue&     aCSSValue,
                                      uint32_t        aFlags)
{
   int32_t i = 0;
   // See if it is one of the 'namedspace' (ranging -7/18em, -6/18, ... 7/18em)
   if (aString.EqualsLiteral("veryverythinmathspace")) {
     i = 1;
   } else if (aString.EqualsLiteral("verythinmathspace")) {
     i = 2;
   } else if (aString.EqualsLiteral("thinmathspace")) {
     i = 3;
   } else if (aString.EqualsLiteral("mediummathspace")) {
     i = 4;
   } else if (aString.EqualsLiteral("thickmathspace")) {
     i = 5;
   } else if (aString.EqualsLiteral("verythickmathspace")) {
     i = 6;
   } else if (aString.EqualsLiteral("veryverythickmathspace")) {
     i = 7;
   } else if (aFlags & PARSE_ALLOW_NEGATIVE) {
     if (aString.EqualsLiteral("negativeveryverythinmathspace")) {
       i = -1;
     } else if (aString.EqualsLiteral("negativeverythinmathspace")) {
       i = -2;
     } else if (aString.EqualsLiteral("negativethinmathspace")) {
       i = -3;
     } else if (aString.EqualsLiteral("negativemediummathspace")) {
       i = -4;
     } else if (aString.EqualsLiteral("negativethickmathspace")) {
       i = -5;
     } else if (aString.EqualsLiteral("negativeverythickmathspace")) {
       i = -6;
     } else if (aString.EqualsLiteral("negativeveryverythickmathspace")) {
       i = -7;
     }
   }
   if (0 != i) { 
     aCSSValue.SetFloatValue(float(i)/float(18), eCSSUnit_EM);
     return true;
   }
   
   return false;
}
 
// The REC says:
//
// "Most presentation elements have attributes that accept values representing
// lengths to be used for size, spacing or similar properties. The syntax of a
// length is specified as
//
// number | number unit | namedspace
//
// There should be no space between the number and the unit of a length."
// 
// "A trailing '%' represents a percent of the default value. The default
// value, or how it is obtained, is listed in the table of attributes for each
// element. [...] A number without a unit is intepreted as a multiple of the
// default value."
//
// "The possible units in MathML are:
//  
// Unit Description
// em   an em (font-relative unit traditionally used for horizontal lengths)
// ex   an ex (font-relative unit traditionally used for vertical lengths)
// px   pixels, or size of a pixel in the current display
// in   inches (1 inch = 2.54 centimeters)
// cm   centimeters
// mm   millimeters
// pt   points (1 point = 1/72 inch)
// pc   picas (1 pica = 12 points)
// %    percentage of default value"
//
// The numbers are defined that way:
// - unsigned-number: "a string of decimal digits with up to one decimal point
//   (U+002E), representing a non-negative terminating decimal number (a type of
//   rational number)"
// - number: "an optional prefix of '-' (U+002D), followed by an unsigned
//   number, representing a terminating decimal number (a type of rational
//   number)"
//
/* static */ bool
nsMathMLElement::ParseNumericValue(const nsString& aString,
                                   nsCSSValue&     aCSSValue,
                                   uint32_t        aFlags,
                                   nsIDocument*    aDocument)
{
  nsAutoString str(aString);
  str.CompressWhitespace(); // aString is const in this code...

  int32_t stringLength = str.Length();
  if (!stringLength) {
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }

  if (ParseNamedSpaceValue(aString, aCSSValue, aFlags)) {
    return true;
  }

  nsAutoString number, unit;

  // see if the negative sign is there
  int32_t i = 0;
  char16_t c = str[0];
  if (c == '-') {
    number.Append(c);
    i++;
  }

  // Gather up characters that make up the number
  bool gotDot = false;
  for ( ; i < stringLength; i++) {
    c = str[i];
    if (gotDot && c == '.') {
      if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
        ReportLengthParseError(aString, aDocument);
      }
      return false;  // two dots encountered
    }
    else if (c == '.')
      gotDot = true;
    else if (!nsCRT::IsAsciiDigit(c)) {
      str.Right(unit, stringLength - i);
      // some authors leave blanks before the unit, but that shouldn't
      // be allowed, so don't CompressWhitespace on 'unit'.
      break;
    }
    number.Append(c);
  }

  // Convert number to floating point
  nsresult errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (NS_FAILED(errorCode)) {
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }
  if (floatValue < 0 && !(aFlags & PARSE_ALLOW_NEGATIVE)) {
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }

  nsCSSUnit cssUnit;
  if (unit.IsEmpty()) {
    if (aFlags & PARSE_ALLOW_UNITLESS) {
      // no explicit unit, this is a number that will act as a multiplier
      if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                        NS_LITERAL_CSTRING("MathML"), aDocument,
                                        nsContentUtils::eMATHML_PROPERTIES,
                                        "UnitlessValuesAreDeprecated");
      }
      if (aFlags & CONVERT_UNITLESS_TO_PERCENT) {
        aCSSValue.SetPercentValue(floatValue);
        return true;
      }
      else
        cssUnit = eCSSUnit_Number;
    } else {
      // We are supposed to have a unit, but there isn't one.
      // If the value is 0 we can just call it "pixels" otherwise
      // this is illegal.
      if (floatValue != 0.0) {
        if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
          ReportLengthParseError(aString, aDocument);
        }
        return false;
      }
      cssUnit = eCSSUnit_Pixel;
    }
  }
  else if (unit.EqualsLiteral("%")) {
    aCSSValue.SetPercentValue(floatValue / 100.0f);
    return true;
  }
  else if (unit.EqualsLiteral("em")) cssUnit = eCSSUnit_EM;
  else if (unit.EqualsLiteral("ex")) cssUnit = eCSSUnit_XHeight;
  else if (unit.EqualsLiteral("px")) cssUnit = eCSSUnit_Pixel;
  else if (unit.EqualsLiteral("in")) cssUnit = eCSSUnit_Inch;
  else if (unit.EqualsLiteral("cm")) cssUnit = eCSSUnit_Centimeter;
  else if (unit.EqualsLiteral("mm")) cssUnit = eCSSUnit_Millimeter;
  else if (unit.EqualsLiteral("pt")) cssUnit = eCSSUnit_Point;
  else if (unit.EqualsLiteral("pc")) cssUnit = eCSSUnit_Pica;
  else { // unexpected unit
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return true;
}

void
nsMathMLElement::MapMathMLAttributesInto(const nsMappedAttributes* aAttributes,
                                         nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    // scriptsizemultiplier
    //
    // "Specifies the multiplier to be used to adjust font size due to changes
    // in scriptlevel.
    //
    // values: number
    // default: 0.71
    //
    const nsAttrValue* value =
      aAttributes->GetAttr(nsGkAtoms::scriptsizemultiplier_);
    nsCSSValue* scriptSizeMultiplier =
      aData->ValueForScriptSizeMultiplier();
    if (value && value->Type() == nsAttrValue::eString &&
        scriptSizeMultiplier->GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      str.CompressWhitespace();
      // MathML numbers can't have leading '+'
      if (str.Length() > 0 && str.CharAt(0) != '+') {
        nsresult errorCode;
        float floatValue = str.ToFloat(&errorCode);
        // Negative scriptsizemultipliers are not parsed
        if (NS_SUCCEEDED(errorCode) && floatValue >= 0.0f) {
          scriptSizeMultiplier->SetFloatValue(floatValue, eCSSUnit_Number);
        } else {
          ReportParseErrorNoTag(str,
                                nsGkAtoms::scriptsizemultiplier_,
                                aData->mPresContext->Document());
        }
      }
    }

    // scriptminsize
    //
    // "Specifies the minimum font size allowed due to changes in scriptlevel.
    // Note that this does not limit the font size due to changes to mathsize."
    //
    // values: length
    // default: 8pt
    //
    // We don't allow negative values.
    // Unitless and percent values give a multiple of the default value.
    //
    value = aAttributes->GetAttr(nsGkAtoms::scriptminsize_);
    nsCSSValue* scriptMinSize = aData->ValueForScriptMinSize();
    if (value && value->Type() == nsAttrValue::eString &&
        scriptMinSize->GetUnit() == eCSSUnit_Null) {
      ParseNumericValue(value->GetStringValue(), *scriptMinSize,
                        PARSE_ALLOW_UNITLESS | CONVERT_UNITLESS_TO_PERCENT,
                        aData->mPresContext->Document());

      if (scriptMinSize->GetUnit() == eCSSUnit_Percent) {
        scriptMinSize->SetFloatValue(8.0 * scriptMinSize->GetPercentValue(),
                                     eCSSUnit_Point);
      }
    }

    // scriptlevel
    // 
    // "Changes the scriptlevel in effect for the children. When the value is
    // given without a sign, it sets scriptlevel to the specified value; when a
    // sign is given, it increments ("+") or decrements ("-") the current
    // value. (Note that large decrements can result in negative values of
    // scriptlevel, but these values are considered legal.)"
    //
    // values: ( "+" | "-" )? unsigned-integer
    // default: inherited
    //
    value = aAttributes->GetAttr(nsGkAtoms::scriptlevel_);
    nsCSSValue* scriptLevel = aData->ValueForScriptLevel();
    if (value && value->Type() == nsAttrValue::eString &&
        scriptLevel->GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      str.CompressWhitespace();
      if (str.Length() > 0) {
        nsresult errorCode;
        int32_t intValue = str.ToInteger(&errorCode);
        if (NS_SUCCEEDED(errorCode)) {
          // This is kind of cheesy ... if the scriptlevel has a sign,
          // then it's a relative value and we store the nsCSSValue as an
          // Integer to indicate that. Otherwise we store it as a Number
          // to indicate that the scriptlevel is absolute.
          char16_t ch = str.CharAt(0);
          if (ch == '+' || ch == '-') {
            scriptLevel->SetIntValue(intValue, eCSSUnit_Integer);
          } else {
            scriptLevel->SetFloatValue(intValue, eCSSUnit_Number);
          }
        } else {
          ReportParseErrorNoTag(str,
                                nsGkAtoms::scriptlevel_,
                                aData->mPresContext->Document());
        }
      }
    }

    // mathsize
    //
    // "Specifies the size to display the token content. The values 'small' and
    // 'big' choose a size smaller or larger than the current font size, but
    // leave the exact proportions unspecified; 'normal' is allowed for
    // completeness, but since it is equivalent to '100%' or '1em', it has no
    // effect."
    //
    // values: "small" | "normal" | "big" | length
    // default: inherited
    //
    // fontsize
    //
    // "Specified the size for the token. Deprecated in favor of mathsize."
    //
    // values: length
    // default: inherited
    //
    // In both cases, we don't allow negative values.
    // Unitless values give a multiple of the default value.
    //  
    bool parseSizeKeywords = true;
    value = aAttributes->GetAttr(nsGkAtoms::mathsize_);
    if (!value) {
      parseSizeKeywords = false;
      value = aAttributes->GetAttr(nsGkAtoms::fontsize_);
      if (value) {
        WarnDeprecated(nsGkAtoms::fontsize_->GetUTF16String(),
                       nsGkAtoms::mathsize_->GetUTF16String(),
                       aData->mPresContext->Document());
      }
    }
    nsCSSValue* fontSize = aData->ValueForFontSize();
    if (value && value->Type() == nsAttrValue::eString &&
        fontSize->GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      if (!ParseNumericValue(str, *fontSize, PARSE_SUPPRESS_WARNINGS |
                             PARSE_ALLOW_UNITLESS | CONVERT_UNITLESS_TO_PERCENT,
                             nullptr)
          && parseSizeKeywords) {
        static const char sizes[3][7] = { "small", "normal", "big" };
        static const int32_t values[MOZ_ARRAY_LENGTH(sizes)] = {
          NS_STYLE_FONT_SIZE_SMALL, NS_STYLE_FONT_SIZE_MEDIUM,
          NS_STYLE_FONT_SIZE_LARGE
        };
        str.CompressWhitespace();
        for (uint32_t i = 0; i < ArrayLength(sizes); ++i) {
          if (str.EqualsASCII(sizes[i])) {
            fontSize->SetIntValue(values[i], eCSSUnit_Enumerated);
            break;
          }
        }
      }
    }

    // fontfamily
    //
    // "Should be the name of a font that may be available to a MathML renderer,
    // or a CSS font specification; See Section 6.5 Using CSS with MathML and
    // CSS for more information. Deprecated in favor of mathvariant."
    //
    // values: string
    // 
    value = aAttributes->GetAttr(nsGkAtoms::fontfamily_);
    nsCSSValue* fontFamily = aData->ValueForFontFamily();
    if (value) {
      WarnDeprecated(nsGkAtoms::fontfamily_->GetUTF16String(),
                     nsGkAtoms::mathvariant_->GetUTF16String(),
                     aData->mPresContext->Document());
    }
    if (value && value->Type() == nsAttrValue::eString &&
        fontFamily->GetUnit() == eCSSUnit_Null) {
      nsCSSParser parser;
      parser.ParseFontFamilyListString(value->GetStringValue(),
                                       nullptr, 0, *fontFamily);
    }

    // fontstyle
    //
    // "Specified the font style to use for the token. Deprecated in favor of
    //  mathvariant."
    //
    // values: "normal" | "italic"
    // default:	normal (except on <mi>)
    //
    // Note that the font-style property is reset in layout/style/ when
    // -moz-math-variant is specified.
    nsCSSValue* fontStyle = aData->ValueForFontStyle();
    value = aAttributes->GetAttr(nsGkAtoms::fontstyle_);
    if (value) {
      WarnDeprecated(nsGkAtoms::fontstyle_->GetUTF16String(),
                       nsGkAtoms::mathvariant_->GetUTF16String(),
                       aData->mPresContext->Document());
      if (value->Type() == nsAttrValue::eString &&
          fontStyle->GetUnit() == eCSSUnit_Null) {
        nsAutoString str(value->GetStringValue());
        str.CompressWhitespace();
        if (str.EqualsASCII("normal")) {
          fontStyle->SetIntValue(NS_STYLE_FONT_STYLE_NORMAL,
                                eCSSUnit_Enumerated);
        } else if (str.EqualsASCII("italic")) {
          fontStyle->SetIntValue(NS_STYLE_FONT_STYLE_ITALIC,
                                eCSSUnit_Enumerated);
        }
      }
    }

    // fontweight
    //
    // "Specified the font weight for the token. Deprecated in favor of
    // mathvariant."
    //
    // values: "normal" | "bold"
    // default: normal
    //
    // Note that the font-weight property is reset in layout/style/ when
    // -moz-math-variant is specified.
    nsCSSValue* fontWeight = aData->ValueForFontWeight();
    value = aAttributes->GetAttr(nsGkAtoms::fontweight_);
    if (value) {
      WarnDeprecated(nsGkAtoms::fontweight_->GetUTF16String(),
                       nsGkAtoms::mathvariant_->GetUTF16String(),
                       aData->mPresContext->Document());
      if (value->Type() == nsAttrValue::eString &&
          fontWeight->GetUnit() == eCSSUnit_Null) {
        nsAutoString str(value->GetStringValue());
        str.CompressWhitespace();
        if (str.EqualsASCII("normal")) {
          fontWeight->SetIntValue(NS_STYLE_FONT_WEIGHT_NORMAL,
                                 eCSSUnit_Enumerated);
        } else if (str.EqualsASCII("bold")) {
          fontWeight->SetIntValue(NS_STYLE_FONT_WEIGHT_BOLD,
                                  eCSSUnit_Enumerated);
        }
      }
    }

    // mathvariant
    //
    // "Specifies the logical class of the token. Note that this class is more
    // than styling, it typically conveys semantic intent;"
    //
    // values: "normal" | "bold" | "italic" | "bold-italic" | "double-struck" |
    // "bold-fraktur" | "script" | "bold-script" | "fraktur" | "sans-serif" |
    // "bold-sans-serif" | "sans-serif-italic" | "sans-serif-bold-italic" |
    // "monospace" | "initial" | "tailed" | "looped" | "stretched"
    // default: normal (except on <mi>)
    //
    nsCSSValue* mathVariant = aData->ValueForMathVariant();
    value = aAttributes->GetAttr(nsGkAtoms::mathvariant_);
    if (value && value->Type() == nsAttrValue::eString &&
        mathVariant->GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      str.CompressWhitespace();
      static const char sizes[19][23] = {
        "normal", "bold", "italic", "bold-italic", "script", "bold-script",
        "fraktur", "double-struck", "bold-fraktur", "sans-serif",
        "bold-sans-serif", "sans-serif-italic", "sans-serif-bold-italic",
        "monospace", "initial", "tailed", "looped", "stretched"
      };
      static const int32_t values[MOZ_ARRAY_LENGTH(sizes)] = {
        NS_MATHML_MATHVARIANT_NORMAL, NS_MATHML_MATHVARIANT_BOLD,
        NS_MATHML_MATHVARIANT_ITALIC, NS_MATHML_MATHVARIANT_BOLD_ITALIC,
        NS_MATHML_MATHVARIANT_SCRIPT, NS_MATHML_MATHVARIANT_BOLD_SCRIPT,
        NS_MATHML_MATHVARIANT_FRAKTUR, NS_MATHML_MATHVARIANT_DOUBLE_STRUCK,
        NS_MATHML_MATHVARIANT_BOLD_FRAKTUR, NS_MATHML_MATHVARIANT_SANS_SERIF,
        NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF,
        NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC,
        NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC,
        NS_MATHML_MATHVARIANT_MONOSPACE, NS_MATHML_MATHVARIANT_INITIAL,
        NS_MATHML_MATHVARIANT_TAILED, NS_MATHML_MATHVARIANT_LOOPED,
        NS_MATHML_MATHVARIANT_STRETCHED
      };
      for (uint32_t i = 0; i < ArrayLength(sizes); ++i) {
        if (str.EqualsASCII(sizes[i])) {
          mathVariant->SetIntValue(values[i], eCSSUnit_Enumerated);
          break;
        }
      }
    }
  }

  // mathbackground
  // 
  // "Specifies the background color to be used to fill in the bounding box of
  // the element and its children. The default, 'transparent', lets the
  // background color, if any, used in the current rendering context to show
  // through."
  // 
  // values: color | "transparent" 
  // default: "transparent"
  //
  // background
  //
  // "Specified the background color to be used to fill in the bounding box of
  // the element and its children. Deprecated in favor of mathbackground."
  //
  // values: color | "transparent"
  // default: "transparent"
  //
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)) {
    const nsAttrValue* value =
      aAttributes->GetAttr(nsGkAtoms::mathbackground_);
    if (!value) {
      value = aAttributes->GetAttr(nsGkAtoms::background);
      if (value) {
        WarnDeprecated(nsGkAtoms::background->GetUTF16String(),
                       nsGkAtoms::mathbackground_->GetUTF16String(),
                       aData->mPresContext->Document());
      }
    }
    nsCSSValue* backgroundColor = aData->ValueForBackgroundColor();
    if (value && backgroundColor->GetUnit() == eCSSUnit_Null) {
      nscolor color;
      if (value->GetColorValue(color)) {
        backgroundColor->SetColorValue(color);
      }
    }
  }

  // mathcolor
  //
  // "Specifies the foreground color to use when drawing the components of this
  // element, such as the content for token elements or any lines, surds, or
  // other decorations. It also establishes the default mathcolor used for
  // child elements when used on a layout element."
  //
  // values: color
  // default: inherited
  //
  // color
  // 
  // "Specified the color for the token. Deprecated in favor of mathcolor." 
  //
  // values: color
  // default: inherited
  //
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::mathcolor_);
    if (!value) {
      value = aAttributes->GetAttr(nsGkAtoms::color);
      if (value) {
        WarnDeprecated(nsGkAtoms::color->GetUTF16String(),
                       nsGkAtoms::mathcolor_->GetUTF16String(),
                       aData->mPresContext->Document());
      }
    }
    nscolor color;
    nsCSSValue* colorValue = aData->ValueForColor();
    if (value && value->GetColorValue(color) &&
        colorValue->GetUnit() == eCSSUnit_Null) {
      colorValue->SetColorValue(color);
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width
    //
    // "Specifies the desired width of the entire table and is intended for
    // visual user agents. When the value is a percentage value, the value is
    // relative to the horizontal space a MathML renderer has available for the
    // math element. When the value is "auto", the MathML renderer should
    // calculate the table width from its contents using whatever layout
    // algorithm it chooses. "
    //
    // values: "auto" | length
    // default: auto
    //
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      // This does not handle auto and unitless values
      if (value && value->Type() == nsAttrValue::eString) {
        ParseNumericValue(value->GetStringValue(), *width, 0,
                          aData->mPresContext->Document());
      }
    }
  }

  // dir
  //
  // Overall Directionality of Mathematics Formulas:
  // "The overall directionality for a formula, basically the direction of the
  // Layout Schemata, is specified by the dir attribute on the containing math
  // element (see Section 2.2 The Top-Level math Element). The default is ltr.
  // [...] The overall directionality is usually set on the math, but may also 
  // be switched for individual subformula by using the dir attribute on mrow
  // or mstyle elements." 
  //
  // Bidirectional Layout in Token Elements:
  // "Specifies the initial directionality for text within the token:
  // ltr (Left To Right) or rtl (Right To Left). This attribute should only be
  // needed in rare cases involving weak or neutral characters;
  // see Section 3.1.5.1 Overall Directionality of Mathematics Formulas for
  // further discussion. It has no effect on mspace."
  //
  // values: "ltr" | "rtl"
  // default: inherited
  //
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Visibility)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::dir);
    nsCSSValue* direction = aData->ValueForDirection();
    if (value && value->Type() == nsAttrValue::eString &&
        direction->GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      static const char dirs[][4] = { "ltr", "rtl" };
      static const int32_t dirValues[MOZ_ARRAY_LENGTH(dirs)] = {
        NS_STYLE_DIRECTION_LTR, NS_STYLE_DIRECTION_RTL
      };
      for (uint32_t i = 0; i < ArrayLength(dirs); ++i) {
        if (str.EqualsASCII(dirs[i])) {
          direction->SetIntValue(dirValues[i], eCSSUnit_Enumerated);
          break;
        }
      }
    }
  }
}

nsresult
nsMathMLElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  nsresult rv = Element::PreHandleEvent(aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  return PreHandleEventForLinks(aVisitor);
}

nsresult
nsMathMLElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return PostHandleEventForLinks(aVisitor);
}

NS_IMPL_ELEMENT_CLONE(nsMathMLElement)

EventStates
nsMathMLElement::IntrinsicState() const
{
  return Link::LinkState() | nsMathMLElementBase::IntrinsicState() |
    (mIncrementScriptLevel ?
       NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL : EventStates());
}

bool
nsMathMLElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~eCONTENT);
}

void
nsMathMLElement::SetIncrementScriptLevel(bool aIncrementScriptLevel,
                                         bool aNotify)
{
  if (aIncrementScriptLevel == mIncrementScriptLevel)
    return;
  mIncrementScriptLevel = aIncrementScriptLevel;

  NS_ASSERTION(aNotify, "We always notify!");

  UpdateState(true);
}

bool
nsMathMLElement::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse)
{
  nsCOMPtr<nsIURI> uri;
  if (IsLink(getter_AddRefs(uri))) {
    if (aTabIndex) {
      *aTabIndex = ((sTabFocusModel & eTabFocus_linksMask) == 0 ? -1 : 0);
    }
    return true;
  }

  if (aTabIndex) {
    *aTabIndex = -1;
  }

  return false;
}

bool
nsMathMLElement::IsLink(nsIURI** aURI) const
{
  // http://www.w3.org/TR/2010/REC-MathML3-20101021/chapter6.html#interf.link
  // The REC says that the following elements should not be linking elements:
  if (IsAnyOfMathMLElements(nsGkAtoms::mprescripts_, nsGkAtoms::none,
                            nsGkAtoms::malignmark_, nsGkAtoms::maligngroup_)) {
    *aURI = nullptr;
    return false;
  }

  bool hasHref = false;
  const nsAttrValue* href = mAttrsAndChildren.GetAttr(nsGkAtoms::href,
                                                      kNameSpaceID_None);
  if (href) {
    // MathML href
    // The REC says: "When user agents encounter MathML elements with both href
    // and xlink:href attributes, the href attribute should take precedence."
    hasHref = true;
  } else {
    // To be a clickable XLink for styling and interaction purposes, we require:
    //
    //   xlink:href    - must be set
    //   xlink:type    - must be unset or set to "" or set to "simple"
    //   xlink:show    - must be unset or set to "", "new" or "replace"
    //   xlink:actuate - must be unset or set to "" or "onRequest"
    //
    // For any other values, we're either not a *clickable* XLink, or the end
    // result is poorly specified. Either way, we return false.
    
    static nsIContent::AttrValuesArray sTypeVals[] =
      { &nsGkAtoms::_empty, &nsGkAtoms::simple, nullptr };
    
    static nsIContent::AttrValuesArray sShowVals[] =
      { &nsGkAtoms::_empty, &nsGkAtoms::_new, &nsGkAtoms::replace, nullptr };
    
    static nsIContent::AttrValuesArray sActuateVals[] =
      { &nsGkAtoms::_empty, &nsGkAtoms::onRequest, nullptr };
    
    // Optimization: check for href first for early return
    href = mAttrsAndChildren.GetAttr(nsGkAtoms::href,
                                     kNameSpaceID_XLink);
    if (href &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::type,
                        sTypeVals, eCaseMatters) !=
        nsIContent::ATTR_VALUE_NO_MATCH &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show,
                        sShowVals, eCaseMatters) !=
        nsIContent::ATTR_VALUE_NO_MATCH &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::actuate,
                        sActuateVals, eCaseMatters) !=
        nsIContent::ATTR_VALUE_NO_MATCH) {
      hasHref = true;
    }
  }

  if (hasHref) {
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    // Get absolute URI
    nsAutoString hrefStr;
    href->ToString(hrefStr); 
    nsContentUtils::NewURIWithDocumentCharset(aURI, hrefStr,
                                              OwnerDoc(), baseURI);
    // must promise out param is non-null if we return true
    return !!*aURI;
  }

  *aURI = nullptr;
  return false;
}

void
nsMathMLElement::GetLinkTarget(nsAString& aTarget)
{
  const nsAttrValue* target = mAttrsAndChildren.GetAttr(nsGkAtoms::target,
                                                        kNameSpaceID_XLink);
  if (target) {
    target->ToString(aTarget);
  }

  if (aTarget.IsEmpty()) {

    static nsIContent::AttrValuesArray sShowVals[] =
      { &nsGkAtoms::_new, &nsGkAtoms::replace, nullptr };
    
    switch (FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show,
                            sShowVals, eCaseMatters)) {
    case 0:
      aTarget.AssignLiteral("_blank");
      return;
    case 1:
      return;
    }
    OwnerDoc()->GetBaseTarget(aTarget);
  }
}

already_AddRefed<nsIURI>
nsMathMLElement::GetHrefURI() const
{
  nsCOMPtr<nsIURI> hrefURI;
  return IsLink(getter_AddRefs(hrefURI)) ? hrefURI.forget() : nullptr;
}

nsresult
nsMathMLElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                         nsIAtom* aPrefix, const nsAString& aValue,
                         bool aNotify)
{
  nsresult rv = nsMathMLElementBase::SetAttr(aNameSpaceID, aName, aPrefix,
                                           aValue, aNotify);

  // The ordering of the parent class's SetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not set until SetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aName == nsGkAtoms::href &&
      (aNameSpaceID == kNameSpaceID_None ||
       aNameSpaceID == kNameSpaceID_XLink)) {
    if (aNameSpaceID == kNameSpaceID_XLink) {
      WarnDeprecated(MOZ_UTF16("xlink:href"),
                     MOZ_UTF16("href"), OwnerDoc());
    }
    Link::ResetLinkState(!!aNotify, true);
  }

  return rv;
}

nsresult
nsMathMLElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
                           bool aNotify)
{
  nsresult rv = nsMathMLElementBase::UnsetAttr(aNameSpaceID, aAttr, aNotify);

  // The ordering of the parent class's UnsetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not unset until UnsetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aAttr == nsGkAtoms::href &&
      (aNameSpaceID == kNameSpaceID_None ||
       aNameSpaceID == kNameSpaceID_XLink)) {
    // Note: just because we removed a single href attr doesn't mean there's no href,
    // since there are 2 possible namespaces.
    Link::ResetLinkState(!!aNotify, Link::ElementHasHref());
  }

  return rv;
}

JSObject*
nsMathMLElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ElementBinding::Wrap(aCx, this, aGivenProto);
}
