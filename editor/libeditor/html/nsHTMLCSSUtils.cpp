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
 *   Original Author: Daniel Glazman <glazman@netscape.com>
 *   Ms2ger <ms2ger@gmail.com>
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

#include "nsHTMLEditor.h"
#include "nsCOMPtr.h"
#include "nsHTMLEditUtils.h"
#include "nsEditProperty.h"
#include "ChangeCSSInlineStyleTxn.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsTextEditUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsHTMLCSSUtils.h"
#include "nsColor.h"
#include "nsAttrName.h"
#include "nsAutoPtr.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

using namespace mozilla;

static
void ProcessBValue(const nsAString * aInputString, nsAString & aOutputString,
                   const char * aDefaultValueString,
                   const char * aPrependString, const char* aAppendString)
{
  if (aInputString && aInputString->EqualsLiteral("-moz-editor-invert-value")) {
      aOutputString.AssignLiteral("normal");
  }
  else {
    aOutputString.AssignLiteral("bold");
  }
}

static
void ProcessDefaultValue(const nsAString * aInputString, nsAString & aOutputString,
                         const char * aDefaultValueString,
                         const char * aPrependString, const char* aAppendString)
{
  CopyASCIItoUTF16(aDefaultValueString, aOutputString);
}

static
void ProcessSameValue(const nsAString * aInputString, nsAString & aOutputString,
                      const char * aDefaultValueString,
                      const char * aPrependString, const char* aAppendString)
{
  if (aInputString) {
    aOutputString.Assign(*aInputString);
  }
  else
    aOutputString.Truncate();
}

static
void ProcessExtendedValue(const nsAString * aInputString, nsAString & aOutputString,
                          const char * aDefaultValueString,
                          const char * aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    if (aPrependString) {
      AppendASCIItoUTF16(aPrependString, aOutputString);
    }
    aOutputString.Append(*aInputString);
    if (aAppendString) {
      AppendASCIItoUTF16(aAppendString, aOutputString);
    }
  }
}

static
void ProcessLengthValue(const nsAString * aInputString, nsAString & aOutputString,
                        const char * aDefaultValueString,
                        const char * aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    aOutputString.Append(*aInputString);
    if (-1 == aOutputString.FindChar(PRUnichar('%'))) {
      aOutputString.AppendLiteral("px");
    }
  }
}

static
void ProcessListStyleTypeValue(const nsAString * aInputString, nsAString & aOutputString,
                               const char * aDefaultValueString,
                               const char * aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    if (aInputString->EqualsLiteral("1")) {
      aOutputString.AppendLiteral("decimal");
    }
    else if (aInputString->EqualsLiteral("a")) {
      aOutputString.AppendLiteral("lower-alpha");
    }
    else if (aInputString->EqualsLiteral("A")) {
      aOutputString.AppendLiteral("upper-alpha");
    }
    else if (aInputString->EqualsLiteral("i")) {
      aOutputString.AppendLiteral("lower-roman");
    }
    else if (aInputString->EqualsLiteral("I")) {
      aOutputString.AppendLiteral("upper-roman");
    }
    else if (aInputString->EqualsLiteral("square")
             || aInputString->EqualsLiteral("circle")
             || aInputString->EqualsLiteral("disc")) {
      aOutputString.Append(*aInputString);
    }
  }
}

static
void ProcessMarginLeftValue(const nsAString * aInputString, nsAString & aOutputString,
                            const char * aDefaultValueString,
                            const char * aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    if (aInputString->EqualsLiteral("center") ||
        aInputString->EqualsLiteral("-moz-center")) {
      aOutputString.AppendLiteral("auto"); 
    }
    else if (aInputString->EqualsLiteral("right") ||
             aInputString->EqualsLiteral("-moz-right")) {
      aOutputString.AppendLiteral("auto"); 
    }
    else {
      aOutputString.AppendLiteral("0px"); 
    }
  }
}

static
void ProcessMarginRightValue(const nsAString * aInputString, nsAString & aOutputString,
                             const char * aDefaultValueString,
                             const char * aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    if (aInputString->EqualsLiteral("center") ||
        aInputString->EqualsLiteral("-moz-center")) {
      aOutputString.AppendLiteral("auto"); 
    }
    else if (aInputString->EqualsLiteral("left") ||
             aInputString->EqualsLiteral("-moz-left")) {
      aOutputString.AppendLiteral("auto"); 
    }
    else {
      aOutputString.AppendLiteral("0px"); 
    }
  }
}

static
void ProcessFontSizeValue(const nsAString* aInputString, nsAString& aOutputString,
                          const char* aDefaultValueString,
                          const char* aPrependString, const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    PRInt32 size = nsContentUtils::ParseLegacyFontSize(*aInputString);
    switch (size) {
      case 0:
        // Didn't parse
        return;
      case 1:
        aOutputString.AssignLiteral("x-small");
        return;
      case 2:
        aOutputString.AssignLiteral("small");
        return;
      case 3:
        aOutputString.AssignLiteral("medium");
        return;
      case 4:
        aOutputString.AssignLiteral("large");
        return;
      case 5:
        aOutputString.AssignLiteral("x-large");
        return;
      case 6:
        aOutputString.AssignLiteral("xx-large");
        return;
      case 7:
        // No corresponding CSS size
        return;
      default:
        NS_NOTREACHED("Unexpected return value from ParseLegacyFontSize");
    }
  }
}

const nsHTMLCSSUtils::CSSEquivTable boldEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_font_weight, ProcessBValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable italicEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_font_style, ProcessDefaultValue, "italic", nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable underlineEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_text_decoration, ProcessDefaultValue, "underline", nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable strikeEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_text_decoration, ProcessDefaultValue, "line-through", nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable ttEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_font_family, ProcessDefaultValue, "monospace", nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable fontColorEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_color, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable fontFaceEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_font_family, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable fontSizeEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_font_size, ProcessFontSizeValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable bgcolorEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_background_color, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable backgroundImageEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_background_image, ProcessExtendedValue, nsnull, "url(", ")", true, true },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable textColorEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_color, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable borderEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_border, ProcessExtendedValue, nsnull, nsnull, "px solid", true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable textAlignEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_text_align, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable captionAlignEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_caption_side, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable verticalAlignEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_vertical_align, ProcessSameValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable nowrapEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_whitespace, ProcessDefaultValue, "nowrap", nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable widthEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_width, ProcessLengthValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable heightEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_height, ProcessLengthValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable listStyleTypeEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_list_style_type, ProcessListStyleTypeValue, nsnull, nsnull, nsnull, true, true },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable tableAlignEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_text_align, ProcessDefaultValue, "left", nsnull, nsnull, false, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_margin_left, ProcessMarginLeftValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_margin_right, ProcessMarginRightValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

const nsHTMLCSSUtils::CSSEquivTable hrAlignEquivTable[] = {
  { nsHTMLCSSUtils::eCSSEditableProperty_margin_left, ProcessMarginLeftValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_margin_right, ProcessMarginRightValue, nsnull, nsnull, nsnull, true, false },
  { nsHTMLCSSUtils::eCSSEditableProperty_NONE, 0 }
};

nsHTMLCSSUtils::nsHTMLCSSUtils(nsHTMLEditor* aEditor)
  : mHTMLEditor(aEditor)
  , mIsCSSPrefChecked(true)
{
  // let's retrieve the value of the "CSS editing" pref
  mIsCSSPrefChecked = Preferences::GetBool("editor.use_css", mIsCSSPrefChecked);
}

nsHTMLCSSUtils::~nsHTMLCSSUtils()
{
}

// Answers true if we have some CSS equivalence for the HTML style defined
// by aProperty and/or aAttribute for the node aNode
bool
nsHTMLCSSUtils::IsCSSEditableProperty(nsIDOMNode* aNode,
                                      nsIAtom* aProperty,
                                      const nsAString* aAttribute,
                                      const nsAString* aValue)
{
  NS_ASSERTION(aNode, "Shouldn't you pass aNode? - Bug 214025");

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, false);
  return IsCSSEditableProperty(content, aProperty, aAttribute, aValue);
}

bool
nsHTMLCSSUtils::IsCSSEditableProperty(nsIContent* aNode,
                                      nsIAtom* aProperty,
                                      const nsAString* aAttribute,
                                      const nsAString* aValue)
{
  MOZ_ASSERT(aNode);

  nsIContent* content = aNode;
  // we need an element node here
  if (content->NodeType() == nsIDOMNode::TEXT_NODE) {
    content = content->GetParent();
    NS_ENSURE_TRUE(content, false);
  }

  nsIAtom *tagName = content->Tag();
  // brade: shouldn't some of the above go below the next block?

  // html inline styles B I TT U STRIKE and COLOR/FACE on FONT
  if (nsEditProperty::b == aProperty
      || nsEditProperty::i == aProperty
      || nsEditProperty::tt == aProperty
      || nsEditProperty::u == aProperty
      || nsEditProperty::strike == aProperty
      || ((nsEditProperty::font == aProperty) && aAttribute &&
           (aAttribute->EqualsLiteral("color") ||
            aAttribute->EqualsLiteral("face")))) {
    return true;
  }

  // FONT SIZE doesn't work if the value is 7
  if (nsEditProperty::font == aProperty && aAttribute &&
      aAttribute->EqualsLiteral("size")) {
    if (!aValue || aValue->IsEmpty()) {
      return true;
    }
    PRInt32 size = nsContentUtils::ParseLegacyFontSize(*aValue);
    return size && size != 7;
  }

  // ALIGN attribute on elements supporting it
  if (aAttribute && (aAttribute->EqualsLiteral("align")) &&
      (nsEditProperty::div == tagName
       || nsEditProperty::p   == tagName
       || nsEditProperty::h1  == tagName
       || nsEditProperty::h2  == tagName
       || nsEditProperty::h3  == tagName
       || nsEditProperty::h4  == tagName
       || nsEditProperty::h5  == tagName
       || nsEditProperty::h6  == tagName
       || nsEditProperty::td  == tagName
       || nsEditProperty::th  == tagName
       || nsEditProperty::table  == tagName
       || nsEditProperty::hr  == tagName
       // brade: for the above, why not use nsHTMLEditUtils::SupportsAlignAttr
       // brade: but it also checks for tbody, tfoot, thead
       // Let's add the following elements here even if ALIGN has not
       // the same meaning for them
       || nsEditProperty::legend  == tagName
       || nsEditProperty::caption == tagName)) {
    return true;
  }

  if (aAttribute && (aAttribute->EqualsLiteral("valign")) &&
      (nsEditProperty::col == tagName
       || nsEditProperty::colgroup   == tagName
       || nsEditProperty::tbody  == tagName
       || nsEditProperty::td  == tagName
       || nsEditProperty::th  == tagName
       || nsEditProperty::tfoot  == tagName
       || nsEditProperty::thead  == tagName
       || nsEditProperty::tr  == tagName)) {
    return true;
  }

  // attributes TEXT, BACKGROUND and BGCOLOR on BODY
  if (aAttribute && (nsEditProperty::body == tagName) &&
      (aAttribute->EqualsLiteral("text")
       || aAttribute->EqualsLiteral("background")
       || aAttribute->EqualsLiteral("bgcolor"))) {
    return true;
  }

  // attribute BGCOLOR on other elements
  if (aAttribute && aAttribute->EqualsLiteral("bgcolor")) {
    return true;
  }

  // attributes HEIGHT, WIDTH and NOWRAP on TD and TH
  if (aAttribute && ((nsEditProperty::td == tagName)
                      || (nsEditProperty::th == tagName)) &&
      (aAttribute->EqualsLiteral("height")
       || aAttribute->EqualsLiteral("width")
       || aAttribute->EqualsLiteral("nowrap"))) {
    return true;
  }

  // attributes HEIGHT and WIDTH on TABLE
  if (aAttribute && (nsEditProperty::table == tagName) &&
      (aAttribute->EqualsLiteral("height")
       || aAttribute->EqualsLiteral("width"))) {
    return true;
  }

  // attributes SIZE and WIDTH on HR
  if (aAttribute && (nsEditProperty::hr == tagName) &&
      (aAttribute->EqualsLiteral("size")
       || aAttribute->EqualsLiteral("width"))) {
    return true;
  }

  // attribute TYPE on OL UL LI
  if (aAttribute && (nsEditProperty::ol == tagName
                     || nsEditProperty::ul == tagName
                     || nsEditProperty::li == tagName) &&
      aAttribute->EqualsLiteral("type")) {
    return true;
  }

  if (aAttribute && nsEditProperty::img == tagName &&
      (aAttribute->EqualsLiteral("border")
       || aAttribute->EqualsLiteral("width")
       || aAttribute->EqualsLiteral("height"))) {
    return true;
  }

  // other elements that we can align using CSS even if they
  // can't carry the html ALIGN attribute
  if (aAttribute && aAttribute->EqualsLiteral("align") &&
      (nsEditProperty::ul == tagName
       || nsEditProperty::ol == tagName
       || nsEditProperty::dl == tagName
       || nsEditProperty::li == tagName
       || nsEditProperty::dd == tagName
       || nsEditProperty::dt == tagName
       || nsEditProperty::address == tagName
       || nsEditProperty::pre == tagName
       || nsEditProperty::ul == tagName)) {
    return true;
  }

  return false;
}

// the lowest level above the transaction; adds the css declaration "aProperty : aValue" to
// the inline styles carried by aElement
nsresult
nsHTMLCSSUtils::SetCSSProperty(nsIDOMElement *aElement, nsIAtom * aProperty, const nsAString & aValue,
                               bool aSuppressTransaction)
{
  nsRefPtr<ChangeCSSInlineStyleTxn> txn;
  nsresult result = CreateCSSPropertyTxn(aElement, aProperty, aValue,
                                         getter_AddRefs(txn), false);
  if (NS_SUCCEEDED(result))  {
    if (aSuppressTransaction) {
      result = txn->DoTransaction();
    }
    else {
      result = mHTMLEditor->DoTransaction(txn);
    }
  }
  return result;
}

nsresult
nsHTMLCSSUtils::SetCSSPropertyPixels(nsIDOMElement *aElement,
                                     nsIAtom *aProperty,
                                     PRInt32 aIntValue,
                                     bool aSuppressTransaction)
{
  nsAutoString s;
  s.AppendInt(aIntValue);
  return SetCSSProperty(aElement, aProperty, s + NS_LITERAL_STRING("px"),
                        aSuppressTransaction);
}

// the lowest level above the transaction; removes the value aValue from the list of values
// specified for the CSS property aProperty, or totally remove the declaration if this
// property accepts only one value
nsresult
nsHTMLCSSUtils::RemoveCSSProperty(nsIDOMElement *aElement, nsIAtom * aProperty, const nsAString & aValue,
                                  bool aSuppressTransaction)
{
  nsRefPtr<ChangeCSSInlineStyleTxn> txn;
  nsresult result = CreateCSSPropertyTxn(aElement, aProperty, aValue,
                                         getter_AddRefs(txn), true);
  if (NS_SUCCEEDED(result))  {
    if (aSuppressTransaction) {
      result = txn->DoTransaction();
    }
    else {
      result = mHTMLEditor->DoTransaction(txn);
    }
  }
  return result;
}

nsresult 
nsHTMLCSSUtils::CreateCSSPropertyTxn(nsIDOMElement *aElement, 
                                     nsIAtom * aAttribute,
                                     const nsAString& aValue,
                                     ChangeCSSInlineStyleTxn ** aTxn,
                                     bool aRemoveProperty)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  *aTxn = new ChangeCSSInlineStyleTxn();
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aTxn);
  return (*aTxn)->Init(mHTMLEditor, aElement, aAttribute, aValue, aRemoveProperty);
}

nsresult
nsHTMLCSSUtils::GetSpecifiedProperty(nsIDOMNode *aNode, nsIAtom *aProperty,
                                     nsAString & aValue)
{
  return GetCSSInlinePropertyBase(aNode, aProperty, aValue, nsnull, SPECIFIED_STYLE_TYPE);
}

nsresult
nsHTMLCSSUtils::GetComputedProperty(nsIDOMNode *aNode, nsIAtom *aProperty,
                                    nsAString & aValue)
{
  nsCOMPtr<nsIDOMWindow> window;
  nsresult res = GetDefaultViewCSS(aNode, getter_AddRefs(window));
  NS_ENSURE_SUCCESS(res, res);

  return GetCSSInlinePropertyBase(aNode, aProperty, aValue, window, COMPUTED_STYLE_TYPE);
}

nsresult
nsHTMLCSSUtils::GetCSSInlinePropertyBase(nsINode* aNode, nsIAtom* aProperty,
                                         nsAString& aValue,
                                         nsIDOMWindow* aWindow,
                                         PRUint8 aStyleType)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  return GetCSSInlinePropertyBase(node, aProperty, aValue, aWindow, aStyleType);
}

nsresult
nsHTMLCSSUtils::GetCSSInlinePropertyBase(nsIDOMNode *aNode, nsIAtom *aProperty,
                                         nsAString& aValue,
                                         nsIDOMWindow* aWindow,
                                         PRUint8 aStyleType)
{
  aValue.Truncate();
  NS_ENSURE_TRUE(aProperty, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> element = GetElementContainerOrSelf(aNode);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  switch (aStyleType) {
    case COMPUTED_STYLE_TYPE:
      if (element && aWindow) {
        nsAutoString value, propString;
        nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
        aProperty->ToString(propString);
        // Get the all the computed css styles attached to the element node
        nsresult res = aWindow->GetComputedStyle(element, EmptyString(), getter_AddRefs(cssDecl));
        if (NS_FAILED(res) || !cssDecl)
          return res;
        // from these declarations, get the one we want and that one only
        res = cssDecl->GetPropertyValue(propString, value);
        NS_ENSURE_SUCCESS(res, res);
        aValue.Assign(value);
      }
      break;
    case SPECIFIED_STYLE_TYPE:
      if (element) {
        nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
        PRUint32 length;
        nsresult res = GetInlineStyles(element, getter_AddRefs(cssDecl), &length);
        if (NS_FAILED(res) || !cssDecl) return res;
        nsAutoString value, propString;
        aProperty->ToString(propString);
        res = cssDecl->GetPropertyValue(propString, value);
        NS_ENSURE_SUCCESS(res, res);
        aValue.Assign(value);
      }
      break;
  }
  return NS_OK;
}

nsresult
nsHTMLCSSUtils::GetDefaultViewCSS(nsIDOMNode *aNode, nsIDOMWindow **aViewCSS)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  return GetDefaultViewCSS(node, aViewCSS);
}

nsresult
nsHTMLCSSUtils::GetDefaultViewCSS(nsINode* aNode, nsIDOMWindow** aViewCSS)
{
  MOZ_ASSERT(aNode);
  dom::Element* element = GetElementContainerOrSelf(aNode);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMWindow> window = element->OwnerDoc()->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  window.forget(aViewCSS);
  return NS_OK;
}

// remove the CSS style "aProperty : aPropertyValue" and possibly remove the whole node
// if it is a span and if its only attribute is _moz_dirty
nsresult
nsHTMLCSSUtils::RemoveCSSInlineStyle(nsIDOMNode *aNode, nsIAtom *aProperty, const nsAString & aPropertyValue)
{
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);

  // remove the property from the style attribute
  nsresult res = RemoveCSSProperty(elem, aProperty, aPropertyValue, false);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (!element || !element->IsHTML(nsGkAtoms::span) ||
      nsHTMLEditor::HasAttributes(element)) {
    return NS_OK;
  }

  return mHTMLEditor->RemoveContainer(aNode);
}

// Answers true is the property can be removed by setting a "none" CSS value
// on a node
bool
nsHTMLCSSUtils::IsCSSInvertable(nsIAtom *aProperty, const nsAString *aAttribute)
{
  return bool(nsEditProperty::b == aProperty);
}

// Get the default browser background color if we need it for GetCSSBackgroundColorState
void
nsHTMLCSSUtils::GetDefaultBackgroundColor(nsAString & aColor)
{
  if (Preferences::GetBool("editor.use_custom_colors", false)) {
    nsresult rv = Preferences::GetString("editor.background_color", &aColor);
    // XXX Why don't you validate the pref value?
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to get editor.background_color");
      aColor.AssignLiteral("#ffffff");  // Default to white
    }
    return;
  }

  if (Preferences::GetBool("browser.display.use_system_colors", false)) {
    return;
  }

  nsresult rv =
    Preferences::GetString("browser.display.background_color", &aColor);
  // XXX Why don't you validate the pref value?
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to get browser.display.background_color");
    aColor.AssignLiteral("#ffffff");  // Default to white
  }
}

// Get the default length unit used for CSS Indent/Outdent
void
nsHTMLCSSUtils::GetDefaultLengthUnit(nsAString & aLengthUnit)
{
  nsresult rv =
    Preferences::GetString("editor.css.default_length_unit", &aLengthUnit);
  // XXX Why don't you validate the pref value?
  if (NS_FAILED(rv)) {
    aLengthUnit.AssignLiteral("px");
  }
}

// Unfortunately, CSSStyleDeclaration::GetPropertyCSSValue is not yet implemented...
// We need then a way to determine the number part and the unit from aString, aString
// being the result of a GetPropertyValue query...
void
nsHTMLCSSUtils::ParseLength(const nsAString & aString, float * aValue, nsIAtom ** aUnit)
{
  nsAString::const_iterator iter;
  aString.BeginReading(iter);

  float a = 10.0f , b = 1.0f, value = 0;
  PRInt8 sign = 1;
  PRInt32 i = 0, j = aString.Length();
  PRUnichar c;
  bool floatingPointFound = false;
  c = *iter;
  if (PRUnichar('-') == c) { sign = -1; iter++; i++; }
  else if (PRUnichar('+') == c) { iter++; i++; }
  while (i < j) {
    c = *iter;
    if ((PRUnichar('0') == c) ||
        (PRUnichar('1') == c) ||
        (PRUnichar('2') == c) ||
        (PRUnichar('3') == c) ||
        (PRUnichar('4') == c) ||
        (PRUnichar('5') == c) ||
        (PRUnichar('6') == c) ||
        (PRUnichar('7') == c) ||
        (PRUnichar('8') == c) ||
        (PRUnichar('9') == c)) {
      value = (value * a) + (b * (c - PRUnichar('0')));
      b = b / 10 * a;
    }
    else if (!floatingPointFound && (PRUnichar('.') == c)) {
      floatingPointFound = true;
      a = 1.0f; b = 0.1f;
    }
    else break;
    iter++;
    i++;
  }
  *aValue = value * sign;
  *aUnit = NS_NewAtom(StringTail(aString, j-i)); 
}

void
nsHTMLCSSUtils::GetCSSPropertyAtom(nsCSSEditableProperty aProperty, nsIAtom ** aAtom)
{
  *aAtom = nsnull;
  switch (aProperty) {
    case eCSSEditableProperty_background_color:
      *aAtom = nsEditProperty::cssBackgroundColor;
      break;
    case eCSSEditableProperty_background_image:
      *aAtom = nsEditProperty::cssBackgroundImage;
      break;
    case eCSSEditableProperty_border:
      *aAtom = nsEditProperty::cssBorder;
      break;
    case eCSSEditableProperty_caption_side:
      *aAtom = nsEditProperty::cssCaptionSide;
      break;
    case eCSSEditableProperty_color:
      *aAtom = nsEditProperty::cssColor;
      break;
    case eCSSEditableProperty_float:
      *aAtom = nsEditProperty::cssFloat;
      break;
    case eCSSEditableProperty_font_family:
      *aAtom = nsEditProperty::cssFontFamily;
      break;
    case eCSSEditableProperty_font_size:
      *aAtom = nsEditProperty::cssFontSize;
      break;
    case eCSSEditableProperty_font_style:
      *aAtom = nsEditProperty::cssFontStyle;
      break;
    case eCSSEditableProperty_font_weight:
      *aAtom = nsEditProperty::cssFontWeight;
      break;
    case eCSSEditableProperty_height:
      *aAtom = nsEditProperty::cssHeight;
      break;
    case eCSSEditableProperty_list_style_type:
      *aAtom = nsEditProperty::cssListStyleType;
      break;
    case eCSSEditableProperty_margin_left:
      *aAtom = nsEditProperty::cssMarginLeft;
      break;
    case eCSSEditableProperty_margin_right:
      *aAtom = nsEditProperty::cssMarginRight;
      break;
    case eCSSEditableProperty_text_align:
      *aAtom = nsEditProperty::cssTextAlign;
      break;
    case eCSSEditableProperty_text_decoration:
      *aAtom = nsEditProperty::cssTextDecoration;
      break;
    case eCSSEditableProperty_vertical_align:
      *aAtom = nsEditProperty::cssVerticalAlign;
      break;
    case eCSSEditableProperty_whitespace:
      *aAtom = nsEditProperty::cssWhitespace;
      break;
    case eCSSEditableProperty_width:
      *aAtom = nsEditProperty::cssWidth;
      break;
    case eCSSEditableProperty_NONE:
      // intentionally empty
      break;
  }
}

// Populate aProperty and aValueArray with the CSS declarations equivalent to the
// value aValue according to the equivalence table aEquivTable
void
nsHTMLCSSUtils::BuildCSSDeclarations(nsTArray<nsIAtom*> & aPropertyArray,
                                     nsTArray<nsString> & aValueArray,
                                     const CSSEquivTable * aEquivTable,
                                     const nsAString * aValue,
                                     bool aGetOrRemoveRequest)
{
  // clear arrays
  aPropertyArray.Clear();
  aValueArray.Clear();

  // if we have an input value, let's use it
  nsAutoString value, lowerCasedValue;
  if (aValue) {
    value.Assign(*aValue);
    lowerCasedValue.Assign(*aValue);
    ToLowerCase(lowerCasedValue);
  }

  PRInt8 index = 0;
  nsCSSEditableProperty cssProperty = aEquivTable[index].cssProperty;
  while (cssProperty) {
    if (!aGetOrRemoveRequest|| aEquivTable[index].gettable) {
      nsAutoString cssValue, cssPropertyString;
      nsIAtom * cssPropertyAtom;
      // find the equivalent css value for the index-th property in
      // the equivalence table
      (*aEquivTable[index].processValueFunctor) ((!aGetOrRemoveRequest || aEquivTable[index].caseSensitiveValue) ? &value : &lowerCasedValue,
                                                 cssValue,
                                                 aEquivTable[index].defaultValue,
                                                 aEquivTable[index].prependValue,
                                                 aEquivTable[index].appendValue);
      GetCSSPropertyAtom(cssProperty, &cssPropertyAtom);
      aPropertyArray.AppendElement(cssPropertyAtom);
      aValueArray.AppendElement(cssValue);
    }
    index++;
    cssProperty = aEquivTable[index].cssProperty;
  }
}

// Populate cssPropertyArray and cssValueArray with the declarations equivalent
// to aHTMLProperty/aAttribute/aValue for the node aNode
void
nsHTMLCSSUtils::GenerateCSSDeclarationsFromHTMLStyle(dom::Element* aElement,
                                                     nsIAtom* aHTMLProperty,
                                                     const nsAString* aAttribute,
                                                     const nsAString* aValue,
                                                     nsTArray<nsIAtom*>& cssPropertyArray,
                                                     nsTArray<nsString>& cssValueArray,
                                                     bool aGetOrRemoveRequest)
{
  MOZ_ASSERT(aElement);
  nsIAtom* tagName = aElement->Tag();
  const nsHTMLCSSUtils::CSSEquivTable* equivTable = nsnull;

  if (nsEditProperty::b == aHTMLProperty) {
    equivTable = boldEquivTable;
  } else if (nsEditProperty::i == aHTMLProperty) {
    equivTable = italicEquivTable;
  } else if (nsEditProperty::u == aHTMLProperty) {
    equivTable = underlineEquivTable;
  } else if (nsEditProperty::strike == aHTMLProperty) {
    equivTable = strikeEquivTable;
  } else if (nsEditProperty::tt == aHTMLProperty) {
    equivTable = ttEquivTable;
  } else if (aAttribute) {
    if (nsEditProperty::font == aHTMLProperty &&
        aAttribute->EqualsLiteral("color")) {
      equivTable = fontColorEquivTable;
    } else if (nsEditProperty::font == aHTMLProperty &&
               aAttribute->EqualsLiteral("face")) {
      equivTable = fontFaceEquivTable;
    } else if (nsEditProperty::font == aHTMLProperty &&
               aAttribute->EqualsLiteral("size")) {
      equivTable = fontSizeEquivTable;
    } else if (aAttribute->EqualsLiteral("bgcolor")) {
      equivTable = bgcolorEquivTable;
    } else if (aAttribute->EqualsLiteral("background")) {
      equivTable = backgroundImageEquivTable;
    } else if (aAttribute->EqualsLiteral("text")) {
      equivTable = textColorEquivTable;
    } else if (aAttribute->EqualsLiteral("border")) {
      equivTable = borderEquivTable;
    } else if (aAttribute->EqualsLiteral("align")) {
      if (nsEditProperty::table  == tagName) {
        equivTable = tableAlignEquivTable;
      } else if (nsEditProperty::hr  == tagName) {
        equivTable = hrAlignEquivTable;
      } else if (nsEditProperty::legend  == tagName ||
               nsEditProperty::caption == tagName) {
        equivTable = captionAlignEquivTable;
      } else {
        equivTable = textAlignEquivTable;
      }
    } else if (aAttribute->EqualsLiteral("valign")) {
      equivTable = verticalAlignEquivTable;
    } else if (aAttribute->EqualsLiteral("nowrap")) {
      equivTable = nowrapEquivTable;
    } else if (aAttribute->EqualsLiteral("width")) {
      equivTable = widthEquivTable;
    } else if (aAttribute->EqualsLiteral("height") ||
               (nsEditProperty::hr == tagName &&
                aAttribute->EqualsLiteral("size"))) {
      equivTable = heightEquivTable;
    } else if (aAttribute->EqualsLiteral("type") &&
               (nsEditProperty::ol == tagName
                || nsEditProperty::ul == tagName
                || nsEditProperty::li == tagName)) {
      equivTable = listStyleTypeEquivTable;
    }
  }
  if (equivTable) {
    BuildCSSDeclarations(cssPropertyArray, cssValueArray, equivTable,
                         aValue, aGetOrRemoveRequest);
  }
}

// Add to aNode the CSS inline style equivalent to HTMLProperty/aAttribute/aValue for the node,
// and return in aCount the number of CSS properties set by the call
nsresult
nsHTMLCSSUtils::SetCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                            nsIAtom *aHTMLProperty,
                                            const nsAString *aAttribute,
                                            const nsAString *aValue,
                                            PRInt32 * aCount,
                                            bool aSuppressTransaction)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  *aCount = 0;
  if (!element || !IsCSSEditableProperty(element, aHTMLProperty,
                                         aAttribute, aValue)) {
    return NS_OK;
  }

  // we can apply the styles only if the node is an element and if we have
  // an equivalence for the requested HTML style in this implementation

  // Find the CSS equivalence to the HTML style
  nsTArray<nsIAtom*> cssPropertyArray;
  nsTArray<nsString> cssValueArray;
  GenerateCSSDeclarationsFromHTMLStyle(element, aHTMLProperty, aAttribute,
                                       aValue, cssPropertyArray, cssValueArray,
                                       false);

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(element);
  // set the individual CSS inline styles
  *aCount = cssPropertyArray.Length();
  for (PRInt32 index = 0; index < *aCount; index++) {
    nsresult res = SetCSSProperty(domElement, cssPropertyArray[index],
                                  cssValueArray[index], aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

// Remove from aNode the CSS inline style equivalent to HTMLProperty/aAttribute/aValue for the node
nsresult
nsHTMLCSSUtils::RemoveCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                               nsIAtom *aHTMLProperty,
                                               const nsAString *aAttribute,
                                               const nsAString *aValue,
                                               bool aSuppressTransaction)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (!element || !IsCSSEditableProperty(element, aHTMLProperty, aAttribute)) {
    return NS_OK;
  }

  // we can apply the styles only if the node is an element and if we have
  // an equivalence for the requested HTML style in this implementation

  // Find the CSS equivalence to the HTML style
  nsTArray<nsIAtom*> cssPropertyArray;
  nsTArray<nsString> cssValueArray;
  GenerateCSSDeclarationsFromHTMLStyle(element, aHTMLProperty, aAttribute,
                                       aValue, cssPropertyArray, cssValueArray,
                                       true);

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(element);
  // remove the individual CSS inline styles
  PRInt32 count = cssPropertyArray.Length();
  for (PRInt32 index = 0; index < count; index++) {
    nsresult res = RemoveCSSProperty(domElement,
                                     cssPropertyArray[index],
                                     cssValueArray[index],
                                     aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

// returns in aValueString the list of values for the CSS equivalences to
// the HTML style aHTMLProperty/aAttribute/aValueString for the node aNode;
// the value of aStyleType controls the styles we retrieve : specified or
// computed.
nsresult
nsHTMLCSSUtils::GetCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                     nsIAtom *aHTMLProperty,
                                                     const nsAString *aAttribute,
                                                     nsAString & aValueString,
                                                     PRUint8 aStyleType)
{
  aValueString.Truncate();
  nsCOMPtr<dom::Element> theElement = GetElementContainerOrSelf(aNode);
  NS_ENSURE_TRUE(theElement, NS_ERROR_NULL_POINTER);

  if (!theElement || !IsCSSEditableProperty(theElement, aHTMLProperty,
                                            aAttribute, &aValueString)) {
    return NS_OK;
  }

  // Yes, the requested HTML style has a CSS equivalence in this implementation
  // Retrieve the default ViewCSS if we are asked for computed styles
  nsCOMPtr<nsIDOMWindow> window;
  if (COMPUTED_STYLE_TYPE == aStyleType) {
    nsresult res = GetDefaultViewCSS(theElement, getter_AddRefs(window));
    NS_ENSURE_SUCCESS(res, res);
  }
  nsTArray<nsIAtom*> cssPropertyArray;
  nsTArray<nsString> cssValueArray;
  // get the CSS equivalence with last param true indicating we want only the
  // "gettable" properties
  GenerateCSSDeclarationsFromHTMLStyle(theElement, aHTMLProperty, aAttribute, nsnull,
                                       cssPropertyArray, cssValueArray, true);
  PRInt32 count = cssPropertyArray.Length();
  for (PRInt32 index = 0; index < count; index++) {
    nsAutoString valueString;
    // retrieve the specified/computed value of the property
    nsresult res = GetCSSInlinePropertyBase(theElement, cssPropertyArray[index],
                                            valueString, window, aStyleType);
    NS_ENSURE_SUCCESS(res, res);
    // append the value to aValueString (possibly with a leading whitespace)
    if (index) {
      aValueString.Append(PRUnichar(' '));
    }
    aValueString.Append(valueString);
  }
  return NS_OK;
}

// Does the node aNode (or its parent, if it's not an element node) have a CSS
// style equivalent to the HTML style aHTMLProperty/aHTMLAttribute/valueString?
// The value of aStyleType controls the styles we retrieve: specified or
// computed. The return value aIsSet is true if the CSS styles are set.
nsresult
nsHTMLCSSUtils::IsCSSEquivalentToHTMLInlineStyleSet(nsIDOMNode *aNode,
                                                    nsIAtom *aHTMLProperty,
                                                    const nsAString *aHTMLAttribute,
                                                    bool& aIsSet,
                                                    nsAString& valueString,
                                                    PRUint8 aStyleType)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsAutoString htmlValueString(valueString);
  aIsSet = false;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  do {
    valueString.Assign(htmlValueString);
    // get the value of the CSS equivalent styles
    nsresult res = GetCSSEquivalentToHTMLInlineStyleSet(node, aHTMLProperty, aHTMLAttribute,
                                                        valueString, aStyleType);
    NS_ENSURE_SUCCESS(res, res);

    // early way out if we can
    if (valueString.IsEmpty()) {
      return NS_OK;
    }

    if (nsEditProperty::b == aHTMLProperty) {
      if (valueString.EqualsLiteral("bold")) {
        aIsSet = true;
      } else if (valueString.EqualsLiteral("normal")) {
        aIsSet = false;
      } else if (valueString.EqualsLiteral("bolder")) {
        aIsSet = true;
        valueString.AssignLiteral("bold");
      } else {
        PRInt32 weight = 0;
        PRInt32 errorCode;
        nsAutoString value(valueString);
        weight = value.ToInteger(&errorCode, 10);
        if (400 < weight) {
          aIsSet = true;
          valueString.AssignLiteral("bold");
        } else {
          aIsSet = false;
          valueString.AssignLiteral("normal");
        }
      }
    } else if (nsEditProperty::i == aHTMLProperty) {
      if (valueString.EqualsLiteral("italic") ||
          valueString.EqualsLiteral("oblique")) {
        aIsSet = true;
      }
    } else if (nsEditProperty::u == aHTMLProperty) {
      nsAutoString val;
      val.AssignLiteral("underline");
      aIsSet = bool(ChangeCSSInlineStyleTxn::ValueIncludes(valueString, val, false));
    } else if (nsEditProperty::strike == aHTMLProperty) {
      nsAutoString val;
      val.AssignLiteral("line-through");
      aIsSet = bool(ChangeCSSInlineStyleTxn::ValueIncludes(valueString, val, false));
    } else if (aHTMLAttribute &&
               ((nsEditProperty::font == aHTMLProperty &&
                 aHTMLAttribute->EqualsLiteral("color")) ||
                aHTMLAttribute->EqualsLiteral("bgcolor"))) {
      if (htmlValueString.IsEmpty()) {
        aIsSet = true;
      } else {
        nscolor rgba;
        nsAutoString subStr;
        htmlValueString.Right(subStr, htmlValueString.Length() - 1);
        if (NS_ColorNameToRGB(htmlValueString, &rgba) ||
            NS_HexToRGB(subStr, &rgba)) {
          nsAutoString htmlColor, tmpStr;
          htmlColor.AppendLiteral("rgb(");

          NS_NAMED_LITERAL_STRING(comma, ", ");

          tmpStr.AppendInt(NS_GET_R(rgba), 10);
          htmlColor.Append(tmpStr + comma);

          tmpStr.Truncate();
          tmpStr.AppendInt(NS_GET_G(rgba), 10);
          htmlColor.Append(tmpStr + comma);

          tmpStr.Truncate();
          tmpStr.AppendInt(NS_GET_B(rgba), 10);
          htmlColor.Append(tmpStr);

          htmlColor.Append(PRUnichar(')'));
          aIsSet = htmlColor.Equals(valueString,
                                    nsCaseInsensitiveStringComparator());
        } else {
          aIsSet = htmlValueString.Equals(valueString,
                                    nsCaseInsensitiveStringComparator());
        }
      }
    } else if (nsEditProperty::tt == aHTMLProperty) {
      aIsSet = StringBeginsWith(valueString, NS_LITERAL_STRING("monospace"));
    } else if (nsEditProperty::font == aHTMLProperty && aHTMLAttribute &&
               aHTMLAttribute->EqualsLiteral("face")) {
      if (!htmlValueString.IsEmpty()) {
        const PRUnichar commaSpace[] = { PRUnichar(','), PRUnichar(' '), 0 };
        const PRUnichar comma[] = { PRUnichar(','), 0 };
        htmlValueString.ReplaceSubstring(commaSpace, comma);
        nsAutoString valueStringNorm(valueString);
        valueStringNorm.ReplaceSubstring(commaSpace, comma);
        aIsSet = htmlValueString.Equals(valueStringNorm,
                                        nsCaseInsensitiveStringComparator());
      } else {
        // ignore this, it's TT or our default
        nsAutoString valueStringLower;
        ToLowerCase(valueString, valueStringLower);
        aIsSet = !valueStringLower.EqualsLiteral("monospace") &&
                 !valueStringLower.EqualsLiteral("serif");
      }
      return NS_OK;
    } else if (nsEditProperty::font == aHTMLProperty && aHTMLAttribute &&
               aHTMLAttribute->EqualsLiteral("size")) {
      if (htmlValueString.IsEmpty()) {
        aIsSet = true;
      } else {
        PRInt32 size = nsContentUtils::ParseLegacyFontSize(htmlValueString);
        aIsSet = (size == 1 && valueString.EqualsLiteral("x-small")) ||
                 (size == 2 && valueString.EqualsLiteral("small")) ||
                 (size == 3 && valueString.EqualsLiteral("medium")) ||
                 (size == 4 && valueString.EqualsLiteral("large")) ||
                 (size == 5 && valueString.EqualsLiteral("x-large")) ||
                 (size == 6 && valueString.EqualsLiteral("xx-large"));
      }
    } else if (aHTMLAttribute && aHTMLAttribute->EqualsLiteral("align")) {
      aIsSet = true;
    } else {
      aIsSet = false;
      return NS_OK;
    }

    if (!htmlValueString.IsEmpty() &&
        htmlValueString.Equals(valueString,
                               nsCaseInsensitiveStringComparator())) {
      aIsSet = true;
    }

    if (nsEditProperty::u == aHTMLProperty || nsEditProperty::strike == aHTMLProperty) {
      // unfortunately, the value of the text-decoration property is not inherited.
      // that means that we have to look at ancestors of node to see if they are underlined
      node = node->GetElementParent();  // set to null if it's not a dom element
    }
  } while ((nsEditProperty::u == aHTMLProperty || nsEditProperty::strike == aHTMLProperty) &&
           !aIsSet && node);
  return NS_OK;
}

nsresult
nsHTMLCSSUtils::SetCSSEnabled(bool aIsCSSPrefChecked)
{
  mIsCSSPrefChecked = aIsCSSPrefChecked;
  return NS_OK;
}

bool
nsHTMLCSSUtils::IsCSSPrefChecked()
{
  return mIsCSSPrefChecked ;
}

// ElementsSameStyle compares two elements and checks if they have the same
// specified CSS declarations in the STYLE attribute 
// The answer is always negative if at least one of them carries an ID or a class
bool
nsHTMLCSSUtils::ElementsSameStyle(nsIDOMNode *aFirstNode, nsIDOMNode *aSecondNode)
{
  nsresult res;
  nsCOMPtr<nsIDOMElement> firstElement  = do_QueryInterface(aFirstNode);
  nsCOMPtr<nsIDOMElement> secondElement = do_QueryInterface(aSecondNode);

  NS_ASSERTION((firstElement && secondElement), "Non element nodes passed to ElementsSameStyle.");

  nsAutoString firstID, secondID;
  bool isFirstIDSet, isSecondIDSet;
  res = mHTMLEditor->GetAttributeValue(firstElement,  NS_LITERAL_STRING("id"), firstID,  &isFirstIDSet);
  res = mHTMLEditor->GetAttributeValue(secondElement, NS_LITERAL_STRING("id"), secondID, &isSecondIDSet);
  if (isFirstIDSet || isSecondIDSet) {
    // at least one of the spans carries an ID ; suspect a CSS rule applies to it and
    // refuse to merge the nodes
    return false;
  }

  nsAutoString firstClass, secondClass;
  bool isFirstClassSet, isSecondClassSet;
  res = mHTMLEditor->GetAttributeValue(firstElement,  NS_LITERAL_STRING("class"), firstClass,  &isFirstClassSet);
  res = mHTMLEditor->GetAttributeValue(secondElement, NS_LITERAL_STRING("class"), secondClass, &isSecondClassSet);
  if (isFirstClassSet && isSecondClassSet) {
    // both spans carry a class, let's compare them
    if (!firstClass.Equals(secondClass)) {
      // WARNING : technically, the comparison just above is questionable :
      // from a pure HTML/CSS point of view class="a b" is NOT the same than
      // class="b a" because a CSS rule could test the exact value of the class
      // attribute to be "a b" for instance ; from a user's point of view, a
      // wysiwyg editor should probably NOT make any difference. CSS people
      // need to discuss this issue before any modification.
      return false;
    }
  }
  else if (isFirstClassSet || isSecondClassSet) {
    // one span only carries a class, early way out
    return false;
  }

  nsCOMPtr<nsIDOMCSSStyleDeclaration> firstCSSDecl, secondCSSDecl;
  PRUint32 firstLength, secondLength;
  res = GetInlineStyles(firstElement,  getter_AddRefs(firstCSSDecl),  &firstLength);
  if (NS_FAILED(res) || !firstCSSDecl) return false;
  res = GetInlineStyles(secondElement, getter_AddRefs(secondCSSDecl), &secondLength);
  if (NS_FAILED(res) || !secondCSSDecl) return false;

  if (firstLength != secondLength) {
    // early way out if we can
    return false;
  }
  else if (0 == firstLength) {
    // no inline style !
    return true;
  }

  PRUint32 i;
  nsAutoString propertyNameString;
  nsAutoString firstValue, secondValue;
  for (i=0; i<firstLength; i++) {
    firstCSSDecl->Item(i, propertyNameString);
    firstCSSDecl->GetPropertyValue(propertyNameString, firstValue);
    secondCSSDecl->GetPropertyValue(propertyNameString, secondValue);
    if (!firstValue.Equals(secondValue)) {
      return false;
    }
  }
  for (i=0; i<secondLength; i++) {
    secondCSSDecl->Item(i, propertyNameString);
    secondCSSDecl->GetPropertyValue(propertyNameString, secondValue);
    firstCSSDecl->GetPropertyValue(propertyNameString, firstValue);
    if (!firstValue.Equals(secondValue)) {
      return false;
    }
  }

  return true;
}

nsresult
nsHTMLCSSUtils::GetInlineStyles(nsIDOMElement *aElement,
                                nsIDOMCSSStyleDeclaration **aCssDecl,
                                PRUint32 *aLength)
{
  NS_ENSURE_TRUE(aElement && aLength, NS_ERROR_NULL_POINTER);
  *aLength = 0;
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyles = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);
  nsresult res = inlineStyles->GetStyle(aCssDecl);
  if (NS_FAILED(res) || !aCssDecl) return NS_ERROR_NULL_POINTER;
  (*aCssDecl)->GetLength(aLength);
  return NS_OK;
}

already_AddRefed<nsIDOMElement>
nsHTMLCSSUtils::GetElementContainerOrSelf(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, nsnull);
  nsCOMPtr<nsIDOMElement> element =
    do_QueryInterface(GetElementContainerOrSelf(node));
  return element.forget();
}

dom::Element*
nsHTMLCSSUtils::GetElementContainerOrSelf(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (nsIDOMNode::DOCUMENT_NODE == aNode->NodeType()) {
    return nsnull;
  }

  nsINode* node = aNode;
  // Loop until we find an element.
  while (node && !node->IsElement()) {
    node = node->GetNodeParent();
  }

  NS_ENSURE_TRUE(node, nsnull);
  return node->AsElement();
}

nsresult
nsHTMLCSSUtils::SetCSSProperty(nsIDOMElement * aElement,
                               const nsAString & aProperty,
                               const nsAString & aValue)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  PRUint32 length;
  nsresult res = GetInlineStyles(aElement, getter_AddRefs(cssDecl), &length);
  if (NS_FAILED(res) || !cssDecl) return res;

  return cssDecl->SetProperty(aProperty,
                              aValue,
                              EmptyString());
}

nsresult
nsHTMLCSSUtils::SetCSSPropertyPixels(nsIDOMElement * aElement,
                                     const nsAString & aProperty,
                                     PRInt32 aIntValue)
{
  nsAutoString s;
  s.AppendInt(aIntValue);
  return SetCSSProperty(aElement, aProperty, s + NS_LITERAL_STRING("px"));
}
