/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MathMLElement.h"

#include "base/compiler_specific.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/StaticPrefs_mathml.h"
#include "mozilla/TextUtils.h"
#include "nsGkAtoms.h"
#include "nsIContentInlines.h"
#include "nsITableCellLayout.h"  // for MAX_COLSPAN / MAX_ROWSPAN
#include "nsCSSValue.h"
#include "nsMappedAttributes.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"
#include "mozAutoDocUpdate.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsIURI.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MappedDeclarations.h"
#include "mozilla/dom/MathMLElementBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ISUPPORTS_INHERITED(MathMLElement, MathMLElementBase, Link)

static nsresult ReportLengthParseError(const nsString& aValue,
                                       Document* aDocument) {
  AutoTArray<nsString, 1> arg = {aValue};
  return nsContentUtils::ReportToConsole(
      nsIScriptError::errorFlag, "MathML"_ns, aDocument,
      nsContentUtils::eMATHML_PROPERTIES, "LengthParsingError", arg);
}

static nsresult ReportParseErrorNoTag(const nsString& aValue, nsAtom* aAtom,
                                      Document* aDocument) {
  AutoTArray<nsString, 2> argv = {aValue, nsDependentAtomString(aAtom)};
  return nsContentUtils::ReportToConsole(
      nsIScriptError::errorFlag, "MathML"_ns, aDocument,
      nsContentUtils::eMATHML_PROPERTIES, "AttributeParsingErrorNoTag", argv);
}

MathMLElement::MathMLElement(
    already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : MathMLElementBase(std::move(aNodeInfo)),
      ALLOW_THIS_IN_INITIALIZER_LIST(Link(this)),
      mIncrementScriptLevel(false) {}

MathMLElement::MathMLElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : MathMLElementBase(std::move(aNodeInfo)),
      ALLOW_THIS_IN_INITIALIZER_LIST(Link(this)),
      mIncrementScriptLevel(false) {}

nsresult MathMLElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = MathMLElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // FIXME(emilio): Probably should be composed, this uses all the other link
  // infrastructure.
  if (Document* doc = aContext.GetUncomposedDoc()) {
    doc->RegisterPendingLinkUpdate(this);
  }

  // Set the bit in the document for telemetry.
  if (Document* doc = aContext.GetComposedDoc()) {
    doc->SetMathMLEnabled();
  }

  return rv;
}

void MathMLElement::UnbindFromTree(bool aNullParent) {
  // Without removing the link state we risk a dangling pointer
  // in the mStyledLinks hashtable
  Link::ResetLinkState(false, Link::ElementHasHref());

  MathMLElementBase::UnbindFromTree(aNullParent);
}

bool MathMLElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   nsAttrValue& aResult) {
  MOZ_ASSERT(IsMathMLElement());

  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::color || aAttribute == nsGkAtoms::mathcolor_ ||
        aAttribute == nsGkAtoms::background ||
        aAttribute == nsGkAtoms::mathbackground_) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::tabindex) {
      return aResult.ParseIntValue(aValue);
    }
    if (mNodeInfo->Equals(nsGkAtoms::mtd_)) {
      if (aAttribute == nsGkAtoms::columnspan_) {
        aResult.ParseClampedNonNegativeInt(aValue, 1, 1, MAX_COLSPAN);
        return true;
      }
      if (aAttribute == nsGkAtoms::rowspan) {
        aResult.ParseClampedNonNegativeInt(aValue, 1, 0, MAX_ROWSPAN);
        return true;
      }
    }
  }

  return MathMLElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                           aMaybeScriptedPrincipal, aResult);
}

// https://mathml-refresh.github.io/mathml-core/#global-attributes
static Element::MappedAttributeEntry sGlobalAttributes[] = {
    {nsGkAtoms::dir},           {nsGkAtoms::mathbackground_},
    {nsGkAtoms::mathcolor_},    {nsGkAtoms::mathsize_},
    {nsGkAtoms::mathvariant_},  {nsGkAtoms::scriptlevel_},
    {nsGkAtoms::displaystyle_}, {nullptr}};

static Element::MappedAttributeEntry sDeprecatedStyleAttributes[] = {
    {nsGkAtoms::background},
    {nsGkAtoms::color},
    {nsGkAtoms::fontfamily_},
    {nsGkAtoms::fontsize_},
    {nsGkAtoms::fontstyle_},
    {nsGkAtoms::fontweight_},
    {nullptr}};

bool MathMLElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  MOZ_ASSERT(IsMathMLElement());

  static const MappedAttributeEntry* const globalMap[] = {sGlobalAttributes};
  static const MappedAttributeEntry* const styleMap[] = {
      sDeprecatedStyleAttributes};

  return FindAttributeDependence(aAttribute, globalMap) ||
         (!StaticPrefs::mathml_deprecated_style_attributes_disabled() &&
          FindAttributeDependence(aAttribute, styleMap)) ||
         (!StaticPrefs::mathml_scriptminsize_attribute_disabled() &&
          aAttribute == nsGkAtoms::scriptminsize_) ||
         (!StaticPrefs::mathml_scriptsizemultiplier_attribute_disabled() &&
          aAttribute == nsGkAtoms::scriptsizemultiplier_) ||
         (mNodeInfo->Equals(nsGkAtoms::mtable_) &&
          aAttribute == nsGkAtoms::width);
}

nsMapRuleToAttributesFunc MathMLElement::GetAttributeMappingFunction() const {
  // It doesn't really matter what our tag is here, because only attributes
  // that satisfy IsAttributeMapped will be stored in the mapped attributes
  // list and available to the mapping function
  return &MapMathMLAttributesInto;
}

/* static */
bool MathMLElement::ParseNamedSpaceValue(const nsString& aString,
                                         nsCSSValue& aCSSValue, uint32_t aFlags,
                                         const Document& aDocument) {
  if (StaticPrefs::mathml_mathspace_names_disabled()) {
    return false;
  }
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
    aDocument.WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedMathSpaceValue);
    aCSSValue.SetFloatValue(float(i) / float(18), eCSSUnit_EM);
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
/* static */
// XXXfredw: Deprecate legacy MathML syntax and use the CSS parser instead.
// See https://github.com/mathml-refresh/mathml/issues/63
bool MathMLElement::ParseNumericValue(const nsString& aString,
                                      nsCSSValue& aCSSValue, uint32_t aFlags,
                                      Document* aDocument) {
  nsAutoString str(aString);
  str.CompressWhitespace();  // aString is const in this code...

  int32_t stringLength = str.Length();
  if (!stringLength) {
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }

  if (aDocument && ParseNamedSpaceValue(str, aCSSValue, aFlags, *aDocument)) {
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
  for (; i < stringLength; i++) {
    c = str[i];
    if (gotDot && c == '.') {
      if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
        ReportLengthParseError(aString, aDocument);
      }
      return false;  // two dots encountered
    } else if (c == '.')
      gotDot = true;
    else if (!IsAsciiDigit(c)) {
      str.Right(unit, stringLength - i);
      // some authors leave blanks before the unit, but that shouldn't
      // be allowed, so don't CompressWhitespace on 'unit'.
      break;
    }
    number.Append(c);
  }
  if (gotDot && str[i - 1] == '.') {
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;  // Number ending with a dot.
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
  } else if (unit.EqualsLiteral("%")) {
    aCSSValue.SetPercentValue(floatValue / 100.0f);
    return true;
  } else if (unit.LowerCaseEqualsLiteral("em"))
    cssUnit = eCSSUnit_EM;
  else if (unit.LowerCaseEqualsLiteral("ex"))
    cssUnit = eCSSUnit_XHeight;
  else if (unit.LowerCaseEqualsLiteral("px"))
    cssUnit = eCSSUnit_Pixel;
  else if (unit.LowerCaseEqualsLiteral("in"))
    cssUnit = eCSSUnit_Inch;
  else if (unit.LowerCaseEqualsLiteral("cm"))
    cssUnit = eCSSUnit_Centimeter;
  else if (unit.LowerCaseEqualsLiteral("mm"))
    cssUnit = eCSSUnit_Millimeter;
  else if (unit.LowerCaseEqualsLiteral("pt"))
    cssUnit = eCSSUnit_Point;
  else if (unit.LowerCaseEqualsLiteral("pc"))
    cssUnit = eCSSUnit_Pica;
  else if (unit.LowerCaseEqualsLiteral("q"))
    cssUnit = eCSSUnit_Quarter;
  else {  // unexpected unit
    if (!(aFlags & PARSE_SUPPRESS_WARNINGS)) {
      ReportLengthParseError(aString, aDocument);
    }
    return false;
  }

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return true;
}

void MathMLElement::MapMathMLAttributesInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
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
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty__moz_script_size_multiplier)) {
    aDecls.Document()->WarnOnceAbout(
        dom::DeprecatedOperations::
            eMathML_DeprecatedScriptsizemultiplierAttribute);
    auto str = value->GetStringValue();
    str.CompressWhitespace();
    // MathML numbers can't have leading '+'
    if (str.Length() > 0 && str.CharAt(0) != '+') {
      nsresult errorCode;
      float floatValue = str.ToFloat(&errorCode);
      // Negative scriptsizemultipliers are not parsed
      if (NS_SUCCEEDED(errorCode) && floatValue >= 0.0f) {
        aDecls.SetNumberValue(eCSSProperty__moz_script_size_multiplier,
                              floatValue);
      } else {
        ReportParseErrorNoTag(str, nsGkAtoms::scriptsizemultiplier_,
                              aDecls.Document());
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
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty__moz_script_min_size)) {
    aDecls.Document()->WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedScriptminsizeAttribute);
    nsCSSValue scriptMinSize;
    ParseNumericValue(value->GetStringValue(), scriptMinSize,
                      PARSE_ALLOW_UNITLESS | CONVERT_UNITLESS_TO_PERCENT,
                      aDecls.Document());

    if (scriptMinSize.GetUnit() == eCSSUnit_Percent) {
      scriptMinSize.SetFloatValue(8.0 * scriptMinSize.GetPercentValue(),
                                  eCSSUnit_Point);
    }
    if (scriptMinSize.GetUnit() != eCSSUnit_Null) {
      aDecls.SetLengthValue(eCSSProperty__moz_script_min_size, scriptMinSize);
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
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty_math_depth)) {
    auto str = value->GetStringValue();
    str.CompressWhitespace();
    if (str.Length() > 0) {
      nsresult errorCode;
      int32_t intValue = str.ToInteger(&errorCode);
      if (NS_SUCCEEDED(errorCode)) {
        char16_t ch = str.CharAt(0);
        bool isRelativeScriptLevel = (ch == '+' || ch == '-');
        aDecls.SetMathDepthValue(intValue, isRelativeScriptLevel);
      } else {
        ReportParseErrorNoTag(str, nsGkAtoms::scriptlevel_, aDecls.Document());
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
  bool parseSizeKeywords = !StaticPrefs::mathml_mathsize_names_disabled();
  value = aAttributes->GetAttr(nsGkAtoms::mathsize_);
  if (!value) {
    parseSizeKeywords = false;
    value = aAttributes->GetAttr(nsGkAtoms::fontsize_);
    if (value) {
      aDecls.Document()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
    }
  }
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty_font_size)) {
    auto str = value->GetStringValue();
    nsCSSValue fontSize;
    uint32_t flags = PARSE_ALLOW_UNITLESS | CONVERT_UNITLESS_TO_PERCENT;
    if (parseSizeKeywords) {
      // Do not warn for invalid value if mathsize keywords are accepted.
      flags |= PARSE_SUPPRESS_WARNINGS;
    }
    if (!ParseNumericValue(str, fontSize, flags, nullptr) &&
        parseSizeKeywords) {
      static const char sizes[3][7] = {"small", "normal", "big"};
      static const StyleFontSizeKeyword values[MOZ_ARRAY_LENGTH(sizes)] = {
          StyleFontSizeKeyword::Small, StyleFontSizeKeyword::Medium,
          StyleFontSizeKeyword::Large};
      str.CompressWhitespace();
      for (uint32_t i = 0; i < ArrayLength(sizes); ++i) {
        if (str.EqualsASCII(sizes[i])) {
          aDecls.Document()->WarnOnceAbout(
              dom::DeprecatedOperations::eMathML_DeprecatedMathSizeValue);
          aDecls.SetKeywordValue(eCSSProperty_font_size, values[i]);
          break;
        }
      }
    } else if (fontSize.GetUnit() == eCSSUnit_Percent) {
      aDecls.SetPercentValue(eCSSProperty_font_size,
                             fontSize.GetPercentValue());
    } else if (fontSize.GetUnit() != eCSSUnit_Null) {
      aDecls.SetLengthValue(eCSSProperty_font_size, fontSize);
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
  if (value) {
    aDecls.Document()->WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
  }
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty_font_family)) {
    aDecls.SetFontFamily(NS_ConvertUTF16toUTF8(value->GetStringValue()));
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
  value = aAttributes->GetAttr(nsGkAtoms::fontstyle_);
  if (value) {
    aDecls.Document()->WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
    if (value->Type() == nsAttrValue::eString &&
        !aDecls.PropertyIsSet(eCSSProperty_font_style)) {
      auto str = value->GetStringValue();
      str.CompressWhitespace();
      // FIXME(emilio): This should use FontSlantStyle or what not. Or even
      // better, it looks deprecated since forever, we should just kill it.
      if (str.EqualsASCII("normal")) {
        aDecls.SetKeywordValue(eCSSProperty_font_style, NS_FONT_STYLE_NORMAL);
      } else if (str.EqualsASCII("italic")) {
        aDecls.SetKeywordValue(eCSSProperty_font_style, NS_FONT_STYLE_ITALIC);
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
  value = aAttributes->GetAttr(nsGkAtoms::fontweight_);
  if (value) {
    aDecls.Document()->WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
    if (value->Type() == nsAttrValue::eString &&
        !aDecls.PropertyIsSet(eCSSProperty_font_weight)) {
      auto str = value->GetStringValue();
      str.CompressWhitespace();
      if (str.EqualsASCII("normal")) {
        aDecls.SetKeywordValue(eCSSProperty_font_weight,
                               FontWeight::Normal().ToFloat());
      } else if (str.EqualsASCII("bold")) {
        aDecls.SetKeywordValue(eCSSProperty_font_weight,
                               FontWeight::Bold().ToFloat());
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
  value = aAttributes->GetAttr(nsGkAtoms::mathvariant_);
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty__moz_math_variant)) {
    auto str = value->GetStringValue();
    str.CompressWhitespace();
    static const char sizes[19][23] = {"normal",
                                       "bold",
                                       "italic",
                                       "bold-italic",
                                       "script",
                                       "bold-script",
                                       "fraktur",
                                       "double-struck",
                                       "bold-fraktur",
                                       "sans-serif",
                                       "bold-sans-serif",
                                       "sans-serif-italic",
                                       "sans-serif-bold-italic",
                                       "monospace",
                                       "initial",
                                       "tailed",
                                       "looped",
                                       "stretched"};
    static const int32_t values[MOZ_ARRAY_LENGTH(sizes)] = {
        NS_MATHML_MATHVARIANT_NORMAL,
        NS_MATHML_MATHVARIANT_BOLD,
        NS_MATHML_MATHVARIANT_ITALIC,
        NS_MATHML_MATHVARIANT_BOLD_ITALIC,
        NS_MATHML_MATHVARIANT_SCRIPT,
        NS_MATHML_MATHVARIANT_BOLD_SCRIPT,
        NS_MATHML_MATHVARIANT_FRAKTUR,
        NS_MATHML_MATHVARIANT_DOUBLE_STRUCK,
        NS_MATHML_MATHVARIANT_BOLD_FRAKTUR,
        NS_MATHML_MATHVARIANT_SANS_SERIF,
        NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF,
        NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC,
        NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC,
        NS_MATHML_MATHVARIANT_MONOSPACE,
        NS_MATHML_MATHVARIANT_INITIAL,
        NS_MATHML_MATHVARIANT_TAILED,
        NS_MATHML_MATHVARIANT_LOOPED,
        NS_MATHML_MATHVARIANT_STRETCHED};
    for (uint32_t i = 0; i < ArrayLength(sizes); ++i) {
      if (str.LowerCaseEqualsASCII(sizes[i])) {
        aDecls.SetKeywordValue(eCSSProperty__moz_math_variant, values[i]);
        break;
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
  value = aAttributes->GetAttr(nsGkAtoms::mathbackground_);
  if (!value) {
    value = aAttributes->GetAttr(nsGkAtoms::background);
    if (value) {
      aDecls.Document()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
    }
  }
  if (value) {
    nscolor color;
    if (value->GetColorValue(color)) {
      aDecls.SetColorValueIfUnset(eCSSProperty_background_color, color);
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
  value = aAttributes->GetAttr(nsGkAtoms::mathcolor_);
  if (!value) {
    value = aAttributes->GetAttr(nsGkAtoms::color);
    if (value) {
      aDecls.Document()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedStyleAttribute);
    }
  }
  nscolor color;
  if (value && value->GetColorValue(color)) {
    aDecls.SetColorValueIfUnset(eCSSProperty_color, color);
  }

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
  if (!aDecls.PropertyIsSet(eCSSProperty_width)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
    nsCSSValue width;
    // This does not handle auto and unitless values
    if (value && value->Type() == nsAttrValue::eString) {
      ParseNumericValue(value->GetStringValue(), width, 0, aDecls.Document());
      if (width.GetUnit() == eCSSUnit_Percent) {
        aDecls.SetPercentValue(eCSSProperty_width, width.GetPercentValue());
      } else if (width.GetUnit() != eCSSUnit_Null) {
        aDecls.SetLengthValue(eCSSProperty_width, width);
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
  value = aAttributes->GetAttr(nsGkAtoms::dir);
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty_direction)) {
    auto str = value->GetStringValue();
    static const char dirs[][4] = {"ltr", "rtl"};
    static const StyleDirection dirValues[MOZ_ARRAY_LENGTH(dirs)] = {
        StyleDirection::Ltr, StyleDirection::Rtl};
    for (uint32_t i = 0; i < ArrayLength(dirs); ++i) {
      if (str.LowerCaseEqualsASCII(dirs[i])) {
        aDecls.SetKeywordValue(eCSSProperty_direction, dirValues[i]);
        break;
      }
    }
  }

  // displaystyle
  // https://mathml-refresh.github.io/mathml-core/#dfn-displaystyle
  value = aAttributes->GetAttr(nsGkAtoms::displaystyle_);
  if (value && value->Type() == nsAttrValue::eString &&
      !aDecls.PropertyIsSet(eCSSProperty_math_style)) {
    auto str = value->GetStringValue();
    static const char displaystyles[][6] = {"false", "true"};
    static const uint8_t mathStyle[MOZ_ARRAY_LENGTH(displaystyles)] = {
        NS_STYLE_MATH_STYLE_COMPACT, NS_STYLE_MATH_STYLE_NORMAL};
    for (uint32_t i = 0; i < ArrayLength(displaystyles); ++i) {
      if (str.LowerCaseEqualsASCII(displaystyles[i])) {
        aDecls.SetKeywordValue(eCSSProperty_math_style, mathStyle[i]);
        break;
      }
    }
  }
}

void MathMLElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  Element::GetEventTargetParent(aVisitor);

  GetEventTargetParentForLinks(aVisitor);
}

nsresult MathMLElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  return PostHandleEventForLinks(aVisitor);
}

NS_IMPL_ELEMENT_CLONE(MathMLElement)

EventStates MathMLElement::IntrinsicState() const {
  return Link::LinkState() | MathMLElementBase::IntrinsicState() |
         (mIncrementScriptLevel ? NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL
                                : EventStates());
}

bool MathMLElement::IsNodeOfType(uint32_t aFlags) const { return false; }

void MathMLElement::SetIncrementScriptLevel(bool aIncrementScriptLevel,
                                            bool aNotify) {
  if (aIncrementScriptLevel == mIncrementScriptLevel) return;
  mIncrementScriptLevel = aIncrementScriptLevel;

  NS_ASSERTION(aNotify, "We always notify!");

  UpdateState(true);
}

int32_t MathMLElement::TabIndexDefault() {
  nsCOMPtr<nsIURI> uri;
  return IsLink(getter_AddRefs(uri)) ? 0 : -1;
}

// XXX Bug 1586011: Share logic with other element classes.
bool MathMLElement::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) {
  if (!IsInComposedDoc() || IsInDesignMode()) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    return false;
  }

  int32_t tabIndex = TabIndex();
  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  nsCOMPtr<nsIURI> uri;
  if (!IsLink(getter_AddRefs(uri))) {
    // If a tabindex is specified at all we're focusable
    return GetTabIndexAttrValue().isSome();
  }

  if (!OwnerDoc()->LinkHandlingEnabled()) {
    return false;
  }

  // Links that are in an editable region should never be focusable, even if
  // they are in a contenteditable="false" region.
  if (nsContentUtils::IsNodeInEditableRegion(this)) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    return false;
  }

  if (aTabIndex && (sTabFocusModel & eTabFocus_linksMask) == 0) {
    *aTabIndex = -1;
  }

  return true;
}

bool MathMLElement::IsLink(nsIURI** aURI) const {
  bool hasHref = false;
  const nsAttrValue* href = mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_None);
  if (href) {
    // MathML href
    // The REC says: "When user agents encounter MathML elements with both href
    // and xlink:href attributes, the href attribute should take precedence."
    hasHref = true;
  } else if (!StaticPrefs::mathml_xlink_disabled()) {
    // To be a clickable XLink for styling and interaction purposes, we require:
    //
    //   xlink:href    - must be set
    //   xlink:type    - must be unset or set to "" or set to "simple"
    //   xlink:show    - must be unset or set to "", "new" or "replace"
    //   xlink:actuate - must be unset or set to "" or "onRequest"
    //
    // For any other values, we're either not a *clickable* XLink, or the end
    // result is poorly specified. Either way, we return false.

    static Element::AttrValuesArray sTypeVals[] = {nsGkAtoms::_empty,
                                                   nsGkAtoms::simple, nullptr};

    static Element::AttrValuesArray sShowVals[] = {
        nsGkAtoms::_empty, nsGkAtoms::_new, nsGkAtoms::replace, nullptr};

    static Element::AttrValuesArray sActuateVals[] = {
        nsGkAtoms::_empty, nsGkAtoms::onRequest, nullptr};

    // Optimization: check for href first for early return
    href = mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
    if (href &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::type, sTypeVals,
                        eCaseMatters) != Element::ATTR_VALUE_NO_MATCH &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show, sShowVals,
                        eCaseMatters) != Element::ATTR_VALUE_NO_MATCH &&
        FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::actuate, sActuateVals,
                        eCaseMatters) != Element::ATTR_VALUE_NO_MATCH) {
      OwnerDoc()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedXLinkAttribute);
      hasHref = true;
    }
  }

  if (hasHref) {
    // Get absolute URI
    nsAutoString hrefStr;
    href->ToString(hrefStr);
    nsContentUtils::NewURIWithDocumentCharset(aURI, hrefStr, OwnerDoc(),
                                              GetBaseURI());
    // must promise out param is non-null if we return true
    return !!*aURI;
  }

  *aURI = nullptr;
  return false;
}

void MathMLElement::GetLinkTarget(nsAString& aTarget) {
  if (StaticPrefs::mathml_xlink_disabled()) {
    MathMLElementBase::GetLinkTarget(aTarget);
    return;
  }

  const nsAttrValue* target =
      mAttrs.GetAttr(nsGkAtoms::target, kNameSpaceID_XLink);
  if (target) {
    OwnerDoc()->WarnOnceAbout(
        dom::DeprecatedOperations::eMathML_DeprecatedXLinkAttribute);
    target->ToString(aTarget);
  }

  if (aTarget.IsEmpty()) {
    static Element::AttrValuesArray sShowVals[] = {nsGkAtoms::_new,
                                                   nsGkAtoms::replace, nullptr};

    bool hasDeprecatedShowAttribute = true;
    switch (FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show, sShowVals,
                            eCaseMatters)) {
      case ATTR_MISSING:
        hasDeprecatedShowAttribute = false;
        break;
      case 0:
        aTarget.AssignLiteral("_blank");
        return;
      case 1:
        return;
    }
    if (hasDeprecatedShowAttribute) {
      OwnerDoc()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedXLinkAttribute);
    }
    OwnerDoc()->GetBaseTarget(aTarget);
  }
}

already_AddRefed<nsIURI> MathMLElement::GetHrefURI() const {
  nsCOMPtr<nsIURI> hrefURI;
  return IsLink(getter_AddRefs(hrefURI)) ? hrefURI.forget() : nullptr;
}

bool MathMLElement::IsEventAttributeNameInternal(nsAtom* aName) {
  // The intent is to align MathML event attributes on HTML5, so the flag
  // EventNameType_HTML is used here.
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_HTML);
}

nsresult MathMLElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                      const nsAttrValueOrString* aValue,
                                      bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (!aValue && IsEventAttributeName(aName)) {
      if (EventListenerManager* manager = GetExistingListenerManager()) {
        manager->RemoveEventHandler(GetEventNameForAttr(aName));
      }
    }
  }

  return MathMLElementBase::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
}

nsresult MathMLElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                     const nsAttrValue* aValue,
                                     const nsAttrValue* aOldValue,
                                     nsIPrincipal* aSubjectPrincipal,
                                     bool aNotify) {
  // It is important that this be done after the attribute is set/unset.
  // We will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aName == nsGkAtoms::href && (aNameSpaceID == kNameSpaceID_None ||
                                   (!StaticPrefs::mathml_xlink_disabled() &&
                                    aNameSpaceID == kNameSpaceID_XLink))) {
    if (aValue && aNameSpaceID == kNameSpaceID_XLink) {
      OwnerDoc()->WarnOnceAbout(
          dom::DeprecatedOperations::eMathML_DeprecatedXLinkAttribute);
    }
    // Note: When unsetting href, there may still be another href since there
    // are 2 possible namespaces.
    Link::ResetLinkState(aNotify, aValue || Link::ElementHasHref());
  }

  if (aNameSpaceID == kNameSpaceID_None) {
    if (IsEventAttributeName(aName) && aValue) {
      MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
                 "Expected string value for script body");
      SetEventHandler(GetEventNameForAttr(aName), aValue->GetStringValue());
    }
  }

  return MathMLElementBase::AfterSetAttr(aNameSpaceID, aName, aValue, aOldValue,
                                         aSubjectPrincipal, aNotify);
}

JSObject* MathMLElement::WrapNode(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return MathMLElement_Binding::Wrap(aCx, this, aGivenProto);
}
