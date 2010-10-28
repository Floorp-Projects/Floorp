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
 * The Original Code is MathML DOM code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vlad Sukhoy <vladimir.sukhoy@gmail.com> (original developer)
 *    Daniel Kraft <d@domob.eu> (nsMathMLElement patch, attachment 262925)
 *    Frederic Wang <fred.wang@free.fr>
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

#include "nsMathMLElement.h"
#include "nsDOMClassInfoID.h" // for eDOMClassInfo_MathElement_id.
#include "nsGkAtoms.h"
#include "nsCRT.h"
#include "nsRuleData.h"
#include "nsCSSValue.h"
#include "nsMappedAttributes.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsIEventStateManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsDOMClassInfoID.h"
#include "mozAutoDocUpdate.h"

//----------------------------------------------------------------------
// nsISupports methods:

DOMCI_NODE_DATA(MathMLElement, nsMathMLElement)

NS_INTERFACE_TABLE_HEAD(nsMathMLElement)
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsMathMLElement)
    NS_INTERFACE_TABLE_ENTRY(nsMathMLElement, nsIDOMNode)
    NS_INTERFACE_TABLE_ENTRY(nsMathMLElement, nsIDOMElement)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MathMLElement)
NS_ELEMENT_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(nsMathMLElement, nsMathMLElementBase)
NS_IMPL_RELEASE_INHERITED(nsMathMLElement, nsMathMLElementBase)

nsresult
nsMathMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            PRBool aCompileEventHandlers)
{
  static const char kMathMLStyleSheetURI[] = "resource://gre-resources/mathml.css";

  nsresult rv = nsMathMLElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument && !aDocument->GetMathMLEnabled()) {
    // Enable MathML and setup the style sheet during binding, not element
    // construction, because we could move a MathML element from the document
    // that created it to another document.
    aDocument->SetMathMLEnabled();
    aDocument->EnsureCatalogStyleSheet(kMathMLStyleSheetURI);

    // Rebuild style data for the presshell, because style system
    // optimizations may have taken place assuming MathML was disabled.
    // (See nsRuleNode::CheckSpecifiedProperties.)
    nsCOMPtr<nsIPresShell> shell = aDocument->GetShell();
    if (shell) {
      shell->GetPresContext()->PostRebuildAllStyleDataEvent(nsChangeHint(0));
    }
  }

  return rv;
}

PRBool
nsMathMLElement::ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
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

static nsGenericElement::MappedAttributeEntry sTokenStyles[] = {
  { &nsGkAtoms::mathsize_ },
  { &nsGkAtoms::fontsize_ },
  { &nsGkAtoms::color },
  { &nsGkAtoms::fontfamily_ },
  { nsnull }
};

static nsGenericElement::MappedAttributeEntry sEnvironmentStyles[] = {
  { &nsGkAtoms::scriptlevel_ },
  { &nsGkAtoms::scriptminsize_ },
  { &nsGkAtoms::scriptsizemultiplier_ },
  { &nsGkAtoms::background },
  { nsnull }
};

static nsGenericElement::MappedAttributeEntry sCommonPresStyles[] = {
  { &nsGkAtoms::mathcolor_ },
  { &nsGkAtoms::mathbackground_ },
  { nsnull }
};

PRBool
nsMathMLElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const tokenMap[] = {
    sTokenStyles,
    sCommonPresStyles
  };
  static const MappedAttributeEntry* const mstyleMap[] = {
    sTokenStyles,
    sEnvironmentStyles,
    sCommonPresStyles
  };
  static const MappedAttributeEntry* const commonPresMap[] = {
    sCommonPresStyles
  };
  
  // We don't support mglyph (yet).
  nsIAtom* tag = Tag();
  if (tag == nsGkAtoms::ms_ || tag == nsGkAtoms::mi_ ||
      tag == nsGkAtoms::mn_ || tag == nsGkAtoms::mo_ ||
      tag == nsGkAtoms::mtext_ || tag == nsGkAtoms::mspace_)
    return FindAttributeDependence(aAttribute, tokenMap,
                                   NS_ARRAY_LENGTH(tokenMap));
  if (tag == nsGkAtoms::mstyle_)
    return FindAttributeDependence(aAttribute, mstyleMap,
                                   NS_ARRAY_LENGTH(mstyleMap));

  if (tag == nsGkAtoms::maction_ ||
      tag == nsGkAtoms::maligngroup_ ||
      tag == nsGkAtoms::malignmark_ ||
      tag == nsGkAtoms::math ||
      tag == nsGkAtoms::menclose_ ||
      tag == nsGkAtoms::merror_ ||
      tag == nsGkAtoms::mfenced_ ||
      tag == nsGkAtoms::mfrac_ ||
      tag == nsGkAtoms::mover_ ||
      tag == nsGkAtoms::mpadded_ ||
      tag == nsGkAtoms::mphantom_ ||
      tag == nsGkAtoms::mprescripts_ ||
      tag == nsGkAtoms::mroot_ ||
      tag == nsGkAtoms::mrow_ ||
      tag == nsGkAtoms::msqrt_ ||
      tag == nsGkAtoms::msub_ ||
      tag == nsGkAtoms::msubsup_ ||
      tag == nsGkAtoms::msup_ ||
      tag == nsGkAtoms::mtable_ ||
      tag == nsGkAtoms::mtd_ ||
      tag == nsGkAtoms::mtr_ ||
      tag == nsGkAtoms::munder_ ||
      tag == nsGkAtoms::munderover_ ||
      tag == nsGkAtoms::none) {
    return FindAttributeDependence(aAttribute, commonPresMap,
                                   NS_ARRAY_LENGTH(commonPresMap));
  }

  return PR_FALSE;
}

nsMapRuleToAttributesFunc
nsMathMLElement::GetAttributeMappingFunction() const
{
  // It doesn't really matter what our tag is here, because only attributes
  // that satisfy IsAttributeMapped will be stored in the mapped attributes
  // list and available to the mapping function
  return &MapMathMLAttributesInto;
}

// ================
// Utilities for parsing and retrieving numeric values

/*
The REC says:
  An explicit plus sign ('+') is not allowed as part of a numeric value
  except when it is specifically listed in the syntax (as a quoted '+'
  or "+"),

  Units allowed
  ID  Description
  em  ems (font-relative unit traditionally used for horizontal lengths)
  ex  exs (font-relative unit traditionally used for vertical lengths)
  px  pixels, or pixel size of a "typical computer display"
  in  inches (1 inch = 2.54 centimeters)
  cm  centimeters
  mm  millimeters
  pt  points (1 point = 1/72 inch)
  pc  picas (1 pica = 12 points)
  %   percentage of default value

Implementation here:
  The numeric value is valid only if it is of the form [-] nnn.nnn
  [h/v-unit]
*/

/* static */ PRBool
nsMathMLElement::ParseNumericValue(const nsString& aString,
                                   nsCSSValue&     aCSSValue,
                                   PRUint32        aFlags)
{
  nsAutoString str(aString);
  str.CompressWhitespace(); // aString is const in this code...

  PRInt32 stringLength = str.Length();
  if (!stringLength)
    return PR_FALSE;

  nsAutoString number, unit;

  // see if the negative sign is there
  PRInt32 i = 0;
  PRUnichar c = str[0];
  if (c == '-') {
    number.Append(c);
    i++;

    // skip any space after the negative sign
    if (i < stringLength && nsCRT::IsAsciiSpace(str[i]))
      i++;
  }

  // Gather up characters that make up the number
  PRBool gotDot = PR_FALSE;
  for ( ; i < stringLength; i++) {
    c = str[i];
    if (gotDot && c == '.')
      return PR_FALSE;  // two dots encountered
    else if (c == '.')
      gotDot = PR_TRUE;
    else if (!nsCRT::IsAsciiDigit(c)) {
      str.Right(unit, stringLength - i);
      // some authors leave blanks before the unit, but that shouldn't
      // be allowed, so don't CompressWhitespace on 'unit'.
      break;
    }
    number.Append(c);
  }

  // Convert number to floating point
  PRInt32 errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (NS_FAILED(errorCode))
    return PR_FALSE;
  if (floatValue < 0 && !(aFlags & PARSE_ALLOW_NEGATIVE))
    return PR_FALSE;

  nsCSSUnit cssUnit;
  if (unit.IsEmpty()) {
    if (aFlags & PARSE_ALLOW_UNITLESS) {
      // no explicit unit, this is a number that will act as a multiplier
      cssUnit = eCSSUnit_Number;
    } else {
      // We are supposed to have a unit, but there isn't one.
      // If the value is 0 we can just call it "pixels" otherwise
      // this is illegal.
      if (floatValue != 0.0)
        return PR_FALSE;
      cssUnit = eCSSUnit_Pixel;
    }
  }
  else if (unit.EqualsLiteral("%")) {
    aCSSValue.SetPercentValue(floatValue / 100.0f);
    return PR_TRUE;
  }
  else if (unit.EqualsLiteral("em")) cssUnit = eCSSUnit_EM;
  else if (unit.EqualsLiteral("ex")) cssUnit = eCSSUnit_XHeight;
  else if (unit.EqualsLiteral("px")) cssUnit = eCSSUnit_Pixel;
  else if (unit.EqualsLiteral("in")) cssUnit = eCSSUnit_Inch;
  else if (unit.EqualsLiteral("cm")) cssUnit = eCSSUnit_Centimeter;
  else if (unit.EqualsLiteral("mm")) cssUnit = eCSSUnit_Millimeter;
  else if (unit.EqualsLiteral("pt")) cssUnit = eCSSUnit_Point;
  else if (unit.EqualsLiteral("pc")) cssUnit = eCSSUnit_Pica;
  else // unexpected unit
    return PR_FALSE;

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return PR_TRUE;
}

void
nsMathMLElement::MapMathMLAttributesInto(const nsMappedAttributes* aAttributes,
                                         nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    const nsAttrValue* value =
      aAttributes->GetAttr(nsGkAtoms::scriptsizemultiplier_);
    if (value && value->Type() == nsAttrValue::eString &&
        aData->mFontData->mScriptSizeMultiplier.GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      str.CompressWhitespace();
      // MathML numbers can't have leading '+'
      if (str.Length() > 0 && str.CharAt(0) != '+') {
        PRInt32 errorCode;
        float floatValue = str.ToFloat(&errorCode);
        // Negative scriptsizemultipliers are not parsed
        if (NS_SUCCEEDED(errorCode) && floatValue >= 0.0f) {
          aData->mFontData->mScriptSizeMultiplier.
            SetFloatValue(floatValue, eCSSUnit_Number);
        }
      }
    }

    value = aAttributes->GetAttr(nsGkAtoms::scriptminsize_);
    if (value && value->Type() == nsAttrValue::eString &&
        aData->mFontData->mScriptMinSize.GetUnit() == eCSSUnit_Null) {
      ParseNumericValue(value->GetStringValue(),
                        aData->mFontData->mScriptMinSize, 0);
    }

    value = aAttributes->GetAttr(nsGkAtoms::scriptlevel_);
    if (value && value->Type() == nsAttrValue::eString &&
        aData->mFontData->mScriptLevel.GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      str.CompressWhitespace();
      if (str.Length() > 0) {
        PRInt32 errorCode;
        PRInt32 intValue = str.ToInteger(&errorCode);
        if (NS_SUCCEEDED(errorCode)) {
          // This is kind of cheesy ... if the scriptlevel has a sign,
          // then it's a relative value and we store the nsCSSValue as an
          // Integer to indicate that. Otherwise we store it as a Number
          // to indicate that the scriptlevel is absolute.
          PRUnichar ch = str.CharAt(0);
          if (ch == '+' || ch == '-') {
            aData->mFontData->mScriptLevel.SetIntValue(intValue, eCSSUnit_Integer);
          } else {
            aData->mFontData->mScriptLevel.SetFloatValue(intValue, eCSSUnit_Number);
          }
        }
      }
    }

    PRBool parseSizeKeywords = PR_TRUE;
    value = aAttributes->GetAttr(nsGkAtoms::mathsize_);
    if (!value) {
      parseSizeKeywords = PR_FALSE;
      value = aAttributes->GetAttr(nsGkAtoms::fontsize_);
    }
    if (value && value->Type() == nsAttrValue::eString &&
        aData->mFontData->mSize.GetUnit() == eCSSUnit_Null) {
      nsAutoString str(value->GetStringValue());
      if (!ParseNumericValue(str, aData->mFontData->mSize, 0) &&
          parseSizeKeywords) {
        static const char sizes[3][7] = { "small", "normal", "big" };
        static const PRInt32 values[NS_ARRAY_LENGTH(sizes)] = {
          NS_STYLE_FONT_SIZE_SMALL, NS_STYLE_FONT_SIZE_MEDIUM,
          NS_STYLE_FONT_SIZE_LARGE
        };
        str.CompressWhitespace();
        for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sizes); ++i) {
          if (str.EqualsASCII(sizes[i])) {
            aData->mFontData->mSize.SetIntValue(values[i], eCSSUnit_Enumerated);
            break;
          }
        }
      }
    }

    value = aAttributes->GetAttr(nsGkAtoms::fontfamily_);
    if (value && value->Type() == nsAttrValue::eString &&
        aData->mFontData->mFamily.GetUnit() == eCSSUnit_Null) {
      aData->mFontData->mFamily.SetStringValue(value->GetStringValue(),
                                               eCSSUnit_Families);
      aData->mFontData->mFamilyFromHTML = PR_FALSE;
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)) {
    const nsAttrValue* value =
      aAttributes->GetAttr(nsGkAtoms::mathbackground_);
    if (!value) {
      value = aAttributes->GetAttr(nsGkAtoms::background);
    }
    if (value && aData->mColorData->mBackColor.GetUnit() == eCSSUnit_Null) {
      nscolor color;
      if (value->GetColorValue(color)) {
        aData->mColorData->mBackColor.SetColorValue(color);
      }
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::mathcolor_);
    if (!value) {
      value = aAttributes->GetAttr(nsGkAtoms::color);
    }
    nscolor color;
    if (value && value->GetColorValue(color) &&
        aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      aData->mColorData->mColor.SetColorValue(color);
    }
  }
}

NS_IMPL_ELEMENT_CLONE(nsMathMLElement)

nsEventStates
nsMathMLElement::IntrinsicState() const
{
  return nsMathMLElementBase::IntrinsicState() |
    (mIncrementScriptLevel ? NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL : nsEventStates());
}

PRBool
nsMathMLElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eCONTENT);
}

void
nsMathMLElement::SetIncrementScriptLevel(PRBool aIncrementScriptLevel,
                                         PRBool aNotify)
{
  if (aIncrementScriptLevel == mIncrementScriptLevel)
    return;
  mIncrementScriptLevel = aIncrementScriptLevel;

  NS_ASSERTION(aNotify, "We always notify!");

  nsIDocument* doc = GetCurrentDoc();
  if (!doc)
    return;

  mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
  doc->ContentStatesChanged(this, nsnull,
                            NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL);
}
