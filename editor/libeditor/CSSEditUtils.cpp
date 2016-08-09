/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CSSEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/ChangeStyleTransaction.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/Preferences.h"
#include "mozilla/css/Declaration.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsStringIterator.h"
#include "nsStyledElement.h"
#include "nsSubstringTuple.h"
#include "nsUnicharUtils.h"

namespace mozilla {

using namespace dom;

static
void ProcessBValue(const nsAString* aInputString,
                   nsAString& aOutputString,
                   const char* aDefaultValueString,
                   const char* aPrependString,
                   const char* aAppendString)
{
  if (aInputString && aInputString->EqualsLiteral("-moz-editor-invert-value")) {
      aOutputString.AssignLiteral("normal");
  }
  else {
    aOutputString.AssignLiteral("bold");
  }
}

static
void ProcessDefaultValue(const nsAString* aInputString,
                         nsAString& aOutputString,
                         const char* aDefaultValueString,
                         const char* aPrependString,
                         const char* aAppendString)
{
  CopyASCIItoUTF16(aDefaultValueString, aOutputString);
}

static
void ProcessSameValue(const nsAString* aInputString,
                      nsAString & aOutputString,
                      const char* aDefaultValueString,
                      const char* aPrependString,
                      const char* aAppendString)
{
  if (aInputString) {
    aOutputString.Assign(*aInputString);
  }
  else
    aOutputString.Truncate();
}

static
void ProcessExtendedValue(const nsAString* aInputString,
                          nsAString& aOutputString,
                          const char* aDefaultValueString,
                          const char* aPrependString,
                          const char* aAppendString)
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
void ProcessLengthValue(const nsAString* aInputString,
                        nsAString& aOutputString,
                        const char* aDefaultValueString,
                        const char* aPrependString,
                        const char* aAppendString)
{
  aOutputString.Truncate();
  if (aInputString) {
    aOutputString.Append(*aInputString);
    if (-1 == aOutputString.FindChar(char16_t('%'))) {
      aOutputString.AppendLiteral("px");
    }
  }
}

static
void ProcessListStyleTypeValue(const nsAString* aInputString,
                               nsAString& aOutputString,
                               const char* aDefaultValueString,
                               const char* aPrependString,
                               const char* aAppendString)
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
void ProcessMarginLeftValue(const nsAString* aInputString,
                            nsAString& aOutputString,
                            const char* aDefaultValueString,
                            const char* aPrependString,
                            const char* aAppendString)
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
void ProcessMarginRightValue(const nsAString* aInputString,
                             nsAString& aOutputString,
                             const char* aDefaultValueString,
                             const char* aPrependString,
                             const char* aAppendString)
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

const CSSEditUtils::CSSEquivTable boldEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_font_weight, ProcessBValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable italicEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_font_style, ProcessDefaultValue, "italic", nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable underlineEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_text_decoration, ProcessDefaultValue, "underline", nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable strikeEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_text_decoration, ProcessDefaultValue, "line-through", nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable ttEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_font_family, ProcessDefaultValue, "monospace", nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable fontColorEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_color, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable fontFaceEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_font_family, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable bgcolorEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_background_color, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable backgroundImageEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_background_image, ProcessExtendedValue, nullptr, "url(", ")", true, true },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable textColorEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_color, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable borderEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_border, ProcessExtendedValue, nullptr, nullptr, "px solid", true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable textAlignEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_text_align, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable captionAlignEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_caption_side, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable verticalAlignEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_vertical_align, ProcessSameValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable nowrapEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_whitespace, ProcessDefaultValue, "nowrap", nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable widthEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_width, ProcessLengthValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable heightEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_height, ProcessLengthValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable listStyleTypeEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_list_style_type, ProcessListStyleTypeValue, nullptr, nullptr, nullptr, true, true },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable tableAlignEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_text_align, ProcessDefaultValue, "left", nullptr, nullptr, false, false },
  { CSSEditUtils::eCSSEditableProperty_margin_left, ProcessMarginLeftValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_margin_right, ProcessMarginRightValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

const CSSEditUtils::CSSEquivTable hrAlignEquivTable[] = {
  { CSSEditUtils::eCSSEditableProperty_margin_left, ProcessMarginLeftValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_margin_right, ProcessMarginRightValue, nullptr, nullptr, nullptr, true, false },
  { CSSEditUtils::eCSSEditableProperty_NONE, 0 }
};

CSSEditUtils::CSSEditUtils(HTMLEditor* aHTMLEditor)
  : mHTMLEditor(aHTMLEditor)
  , mIsCSSPrefChecked(true)
{
  // let's retrieve the value of the "CSS editing" pref
  mIsCSSPrefChecked = Preferences::GetBool("editor.use_css", mIsCSSPrefChecked);
}

CSSEditUtils::~CSSEditUtils()
{
}

// Answers true if we have some CSS equivalence for the HTML style defined
// by aProperty and/or aAttribute for the node aNode
bool
CSSEditUtils::IsCSSEditableProperty(nsINode* aNode,
                                    nsIAtom* aProperty,
                                    const nsAString* aAttribute)
{
  MOZ_ASSERT(aNode);

  nsINode* node = aNode;
  // we need an element node here
  if (node->NodeType() == nsIDOMNode::TEXT_NODE) {
    node = node->GetParentNode();
    NS_ENSURE_TRUE(node, false);
  }

  // html inline styles B I TT U STRIKE and COLOR/FACE on FONT
  if (nsGkAtoms::b == aProperty ||
      nsGkAtoms::i == aProperty ||
      nsGkAtoms::tt == aProperty ||
      nsGkAtoms::u == aProperty ||
      nsGkAtoms::strike == aProperty ||
      (nsGkAtoms::font == aProperty && aAttribute &&
       (aAttribute->EqualsLiteral("color") ||
        aAttribute->EqualsLiteral("face")))) {
    return true;
  }

  // ALIGN attribute on elements supporting it
  if (aAttribute && (aAttribute->EqualsLiteral("align")) &&
      node->IsAnyOfHTMLElements(nsGkAtoms::div,
                                nsGkAtoms::p,
                                nsGkAtoms::h1,
                                nsGkAtoms::h2,
                                nsGkAtoms::h3,
                                nsGkAtoms::h4,
                                nsGkAtoms::h5,
                                nsGkAtoms::h6,
                                nsGkAtoms::td,
                                nsGkAtoms::th,
                                nsGkAtoms::table,
                                nsGkAtoms::hr,
                                // For the above, why not use
                                // HTMLEditUtils::SupportsAlignAttr?
                                // It also checks for tbody, tfoot, thead.
                                // Let's add the following elements here even
                                // if "align" has a different meaning for them
                                nsGkAtoms::legend,
                                nsGkAtoms::caption)) {
    return true;
  }

  if (aAttribute && (aAttribute->EqualsLiteral("valign")) &&
      node->IsAnyOfHTMLElements(nsGkAtoms::col,
                                nsGkAtoms::colgroup,
                                nsGkAtoms::tbody,
                                nsGkAtoms::td,
                                nsGkAtoms::th,
                                nsGkAtoms::tfoot,
                                nsGkAtoms::thead,
                                nsGkAtoms::tr)) {
    return true;
  }

  // attributes TEXT, BACKGROUND and BGCOLOR on BODY
  if (aAttribute && node->IsHTMLElement(nsGkAtoms::body) &&
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
  if (aAttribute &&
      node->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th) &&
      (aAttribute->EqualsLiteral("height")
       || aAttribute->EqualsLiteral("width")
       || aAttribute->EqualsLiteral("nowrap"))) {
    return true;
  }

  // attributes HEIGHT and WIDTH on TABLE
  if (aAttribute && node->IsHTMLElement(nsGkAtoms::table) &&
      (aAttribute->EqualsLiteral("height")
       || aAttribute->EqualsLiteral("width"))) {
    return true;
  }

  // attributes SIZE and WIDTH on HR
  if (aAttribute && node->IsHTMLElement(nsGkAtoms::hr) &&
      (aAttribute->EqualsLiteral("size")
       || aAttribute->EqualsLiteral("width"))) {
    return true;
  }

  // attribute TYPE on OL UL LI
  if (aAttribute &&
      node->IsAnyOfHTMLElements(nsGkAtoms::ol, nsGkAtoms::ul,
                                nsGkAtoms::li) &&
      aAttribute->EqualsLiteral("type")) {
    return true;
  }

  if (aAttribute && node->IsHTMLElement(nsGkAtoms::img) &&
      (aAttribute->EqualsLiteral("border")
       || aAttribute->EqualsLiteral("width")
       || aAttribute->EqualsLiteral("height"))) {
    return true;
  }

  // other elements that we can align using CSS even if they
  // can't carry the html ALIGN attribute
  if (aAttribute && aAttribute->EqualsLiteral("align") &&
      node->IsAnyOfHTMLElements(nsGkAtoms::ul,
                                nsGkAtoms::ol,
                                nsGkAtoms::dl,
                                nsGkAtoms::li,
                                nsGkAtoms::dd,
                                nsGkAtoms::dt,
                                nsGkAtoms::address,
                                nsGkAtoms::pre)) {
    return true;
  }

  return false;
}

// The lowest level above the transaction; adds the CSS declaration
// "aProperty : aValue" to the inline styles carried by aElement
nsresult
CSSEditUtils::SetCSSProperty(Element& aElement,
                             nsIAtom& aProperty,
                             const nsAString& aValue,
                             bool aSuppressTxn)
{
  RefPtr<ChangeStyleTransaction> transaction =
    CreateCSSPropertyTxn(aElement, aProperty, aValue,
                         ChangeStyleTransaction::eSet);
  if (aSuppressTxn) {
    return transaction->DoTransaction();
  }
  return mHTMLEditor->DoTransaction(transaction);
}

nsresult
CSSEditUtils::SetCSSPropertyPixels(Element& aElement,
                                   nsIAtom& aProperty,
                                   int32_t aIntValue)
{
  nsAutoString s;
  s.AppendInt(aIntValue);
  return SetCSSProperty(aElement, aProperty, s + NS_LITERAL_STRING("px"),
                        /* suppress txn */ false);
}

// The lowest level above the transaction; removes the value aValue from the
// list of values specified for the CSS property aProperty, or totally remove
// the declaration if this property accepts only one value
nsresult
CSSEditUtils::RemoveCSSProperty(Element& aElement,
                                nsIAtom& aProperty,
                                const nsAString& aValue,
                                bool aSuppressTxn)
{
  RefPtr<ChangeStyleTransaction> transaction =
    CreateCSSPropertyTxn(aElement, aProperty, aValue,
                         ChangeStyleTransaction::eRemove);
  if (aSuppressTxn) {
    return transaction->DoTransaction();
  }
  return mHTMLEditor->DoTransaction(transaction);
}

already_AddRefed<ChangeStyleTransaction>
CSSEditUtils::CreateCSSPropertyTxn(
                Element& aElement,
                nsIAtom& aAttribute,
                const nsAString& aValue,
                ChangeStyleTransaction::EChangeType aChangeType)
{
  RefPtr<ChangeStyleTransaction> transaction =
    new ChangeStyleTransaction(aElement, aAttribute, aValue, aChangeType);
  return transaction.forget();
}

nsresult
CSSEditUtils::GetSpecifiedProperty(nsINode& aNode,
                                   nsIAtom& aProperty,
                                   nsAString& aValue)
{
  return GetCSSInlinePropertyBase(&aNode, &aProperty, aValue, eSpecified);
}

nsresult
CSSEditUtils::GetComputedProperty(nsINode& aNode,
                                  nsIAtom& aProperty,
                                  nsAString& aValue)
{
  return GetCSSInlinePropertyBase(&aNode, &aProperty, aValue, eComputed);
}

nsresult
CSSEditUtils::GetCSSInlinePropertyBase(nsINode* aNode,
                                       nsIAtom* aProperty,
                                       nsAString& aValue,
                                       StyleType aStyleType)
{
  MOZ_ASSERT(aNode && aProperty);
  aValue.Truncate();

  nsCOMPtr<Element> element = GetElementContainerOrSelf(aNode);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  if (aStyleType == eComputed) {
    // Get the all the computed css styles attached to the element node
    RefPtr<nsComputedDOMStyle> cssDecl = GetComputedStyle(element);
    NS_ENSURE_STATE(cssDecl);

    // from these declarations, get the one we want and that one only
    MOZ_ALWAYS_SUCCEEDS(
      cssDecl->GetPropertyValue(nsDependentAtomString(aProperty), aValue));

    return NS_OK;
  }

  MOZ_ASSERT(aStyleType == eSpecified);
  RefPtr<css::Declaration> decl = element->GetInlineStyleDeclaration();
  if (!decl) {
    return NS_OK;
  }
  nsCSSPropertyID prop =
    nsCSSProps::LookupProperty(nsDependentAtomString(aProperty),
                               CSSEnabledState::eForAllContent);
  MOZ_ASSERT(prop != eCSSProperty_UNKNOWN);
  decl->GetValue(prop, aValue);

  return NS_OK;
}

already_AddRefed<nsComputedDOMStyle>
CSSEditUtils::GetComputedStyle(Element* aElement)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->GetUncomposedDoc();
  NS_ENSURE_TRUE(doc, nullptr);

  nsIPresShell* presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, nullptr);

  RefPtr<nsComputedDOMStyle> style =
    NS_NewComputedDOMStyle(aElement, EmptyString(), presShell);

  return style.forget();
}

// remove the CSS style "aProperty : aPropertyValue" and possibly remove the whole node
// if it is a span and if its only attribute is _moz_dirty
nsresult
CSSEditUtils::RemoveCSSInlineStyle(nsIDOMNode* aNode,
                                   nsIAtom* aProperty,
                                   const nsAString& aPropertyValue)
{
  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  NS_ENSURE_STATE(element);

  // remove the property from the style attribute
  nsresult res = RemoveCSSProperty(*element, *aProperty, aPropertyValue);
  NS_ENSURE_SUCCESS(res, res);

  if (!element->IsHTMLElement(nsGkAtoms::span) ||
      HTMLEditor::HasAttributes(element)) {
    return NS_OK;
  }

  return mHTMLEditor->RemoveContainer(element);
}

// Answers true if the property can be removed by setting a "none" CSS value
// on a node
bool
CSSEditUtils::IsCSSInvertible(nsIAtom& aProperty,
                              const nsAString* aAttribute)
{
  return nsGkAtoms::b == &aProperty;
}

// Get the default browser background color if we need it for GetCSSBackgroundColorState
void
CSSEditUtils::GetDefaultBackgroundColor(nsAString& aColor)
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
CSSEditUtils::GetDefaultLengthUnit(nsAString& aLengthUnit)
{
  nsresult rv =
    Preferences::GetString("editor.css.default_length_unit", &aLengthUnit);
  // XXX Why don't you validate the pref value?
  if (NS_FAILED(rv)) {
    aLengthUnit.AssignLiteral("px");
  }
}

// Unfortunately, CSSStyleDeclaration::GetPropertyCSSValue is not yet
// implemented... We need then a way to determine the number part and the unit
// from aString, aString being the result of a GetPropertyValue query...
void
CSSEditUtils::ParseLength(const nsAString& aString,
                          float* aValue,
                          nsIAtom** aUnit)
{
  if (aString.IsEmpty()) {
    *aValue = 0;
    *aUnit = NS_Atomize(aString).take();
    return;
  }

  nsAString::const_iterator iter;
  aString.BeginReading(iter);

  float a = 10.0f , b = 1.0f, value = 0;
  int8_t sign = 1;
  int32_t i = 0, j = aString.Length();
  char16_t c;
  bool floatingPointFound = false;
  c = *iter;
  if (char16_t('-') == c) { sign = -1; iter++; i++; }
  else if (char16_t('+') == c) { iter++; i++; }
  while (i < j) {
    c = *iter;
    if ((char16_t('0') == c) ||
        (char16_t('1') == c) ||
        (char16_t('2') == c) ||
        (char16_t('3') == c) ||
        (char16_t('4') == c) ||
        (char16_t('5') == c) ||
        (char16_t('6') == c) ||
        (char16_t('7') == c) ||
        (char16_t('8') == c) ||
        (char16_t('9') == c)) {
      value = (value * a) + (b * (c - char16_t('0')));
      b = b / 10 * a;
    }
    else if (!floatingPointFound && (char16_t('.') == c)) {
      floatingPointFound = true;
      a = 1.0f; b = 0.1f;
    }
    else break;
    iter++;
    i++;
  }
  *aValue = value * sign;
  *aUnit = NS_Atomize(StringTail(aString, j-i)).take();
}

void
CSSEditUtils::GetCSSPropertyAtom(nsCSSEditableProperty aProperty,
                                 nsIAtom** aAtom)
{
  *aAtom = nullptr;
  switch (aProperty) {
    case eCSSEditableProperty_background_color:
      *aAtom = nsGkAtoms::backgroundColor;
      break;
    case eCSSEditableProperty_background_image:
      *aAtom = nsGkAtoms::background_image;
      break;
    case eCSSEditableProperty_border:
      *aAtom = nsGkAtoms::border;
      break;
    case eCSSEditableProperty_caption_side:
      *aAtom = nsGkAtoms::caption_side;
      break;
    case eCSSEditableProperty_color:
      *aAtom = nsGkAtoms::color;
      break;
    case eCSSEditableProperty_float:
      *aAtom = nsGkAtoms::_float;
      break;
    case eCSSEditableProperty_font_family:
      *aAtom = nsGkAtoms::font_family;
      break;
    case eCSSEditableProperty_font_size:
      *aAtom = nsGkAtoms::font_size;
      break;
    case eCSSEditableProperty_font_style:
      *aAtom = nsGkAtoms::font_style;
      break;
    case eCSSEditableProperty_font_weight:
      *aAtom = nsGkAtoms::fontWeight;
      break;
    case eCSSEditableProperty_height:
      *aAtom = nsGkAtoms::height;
      break;
    case eCSSEditableProperty_list_style_type:
      *aAtom = nsGkAtoms::list_style_type;
      break;
    case eCSSEditableProperty_margin_left:
      *aAtom = nsGkAtoms::marginLeft;
      break;
    case eCSSEditableProperty_margin_right:
      *aAtom = nsGkAtoms::marginRight;
      break;
    case eCSSEditableProperty_text_align:
      *aAtom = nsGkAtoms::textAlign;
      break;
    case eCSSEditableProperty_text_decoration:
      *aAtom = nsGkAtoms::text_decoration;
      break;
    case eCSSEditableProperty_vertical_align:
      *aAtom = nsGkAtoms::vertical_align;
      break;
    case eCSSEditableProperty_whitespace:
      *aAtom = nsGkAtoms::white_space;
      break;
    case eCSSEditableProperty_width:
      *aAtom = nsGkAtoms::width;
      break;
    case eCSSEditableProperty_NONE:
      // intentionally empty
      break;
  }
}

// Populate aProperty and aValueArray with the CSS declarations equivalent to the
// value aValue according to the equivalence table aEquivTable
void
CSSEditUtils::BuildCSSDeclarations(nsTArray<nsIAtom*>& aPropertyArray,
                                   nsTArray<nsString>& aValueArray,
                                   const CSSEquivTable* aEquivTable,
                                   const nsAString* aValue,
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

  int8_t index = 0;
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
CSSEditUtils::GenerateCSSDeclarationsFromHTMLStyle(
                Element* aElement,
                nsIAtom* aHTMLProperty,
                const nsAString* aAttribute,
                const nsAString* aValue,
                nsTArray<nsIAtom*>& cssPropertyArray,
                nsTArray<nsString>& cssValueArray,
                bool aGetOrRemoveRequest)
{
  MOZ_ASSERT(aElement);
  const CSSEditUtils::CSSEquivTable* equivTable = nullptr;

  if (nsGkAtoms::b == aHTMLProperty) {
    equivTable = boldEquivTable;
  } else if (nsGkAtoms::i == aHTMLProperty) {
    equivTable = italicEquivTable;
  } else if (nsGkAtoms::u == aHTMLProperty) {
    equivTable = underlineEquivTable;
  } else if (nsGkAtoms::strike == aHTMLProperty) {
    equivTable = strikeEquivTable;
  } else if (nsGkAtoms::tt == aHTMLProperty) {
    equivTable = ttEquivTable;
  } else if (aAttribute) {
    if (nsGkAtoms::font == aHTMLProperty &&
        aAttribute->EqualsLiteral("color")) {
      equivTable = fontColorEquivTable;
    } else if (nsGkAtoms::font == aHTMLProperty &&
               aAttribute->EqualsLiteral("face")) {
      equivTable = fontFaceEquivTable;
    } else if (aAttribute->EqualsLiteral("bgcolor")) {
      equivTable = bgcolorEquivTable;
    } else if (aAttribute->EqualsLiteral("background")) {
      equivTable = backgroundImageEquivTable;
    } else if (aAttribute->EqualsLiteral("text")) {
      equivTable = textColorEquivTable;
    } else if (aAttribute->EqualsLiteral("border")) {
      equivTable = borderEquivTable;
    } else if (aAttribute->EqualsLiteral("align")) {
      if (aElement->IsHTMLElement(nsGkAtoms::table)) {
        equivTable = tableAlignEquivTable;
      } else if (aElement->IsHTMLElement(nsGkAtoms::hr)) {
        equivTable = hrAlignEquivTable;
      } else if (aElement->IsAnyOfHTMLElements(nsGkAtoms::legend,
                                               nsGkAtoms::caption)) {
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
               (aElement->IsHTMLElement(nsGkAtoms::hr) &&
                aAttribute->EqualsLiteral("size"))) {
      equivTable = heightEquivTable;
    } else if (aAttribute->EqualsLiteral("type") &&
               aElement->IsAnyOfHTMLElements(nsGkAtoms::ol,
                                             nsGkAtoms::ul,
                                             nsGkAtoms::li)) {
      equivTable = listStyleTypeEquivTable;
    }
  }
  if (equivTable) {
    BuildCSSDeclarations(cssPropertyArray, cssValueArray, equivTable,
                         aValue, aGetOrRemoveRequest);
  }
}

// Add to aNode the CSS inline style equivalent to HTMLProperty/aAttribute/
// aValue for the node, and return in aCount the number of CSS properties set
// by the call.  The Element version returns aCount instead.
int32_t
CSSEditUtils::SetCSSEquivalentToHTMLStyle(Element* aElement,
                                          nsIAtom* aProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue,
                                          bool aSuppressTransaction)
{
  MOZ_ASSERT(aElement && aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);
  int32_t count;
  // This can only fail if SetCSSProperty fails, which should only happen if
  // something is pretty badly wrong.  In this case we assert so that hopefully
  // someone will notice, but there's nothing more sensible to do than just
  // return the count and carry on.
  nsresult res = SetCSSEquivalentToHTMLStyle(aElement->AsDOMNode(),
                                             aProperty, aAttribute,
                                             aValue, &count,
                                             aSuppressTransaction);
  NS_ASSERTION(NS_SUCCEEDED(res), "SetCSSEquivalentToHTMLStyle failed");
  NS_ENSURE_SUCCESS(res, count);
  return count;
}

nsresult
CSSEditUtils::SetCSSEquivalentToHTMLStyle(nsIDOMNode* aNode,
                                          nsIAtom* aHTMLProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue,
                                          int32_t* aCount,
                                          bool aSuppressTransaction)
{
  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  *aCount = 0;
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
                                       false);

  // set the individual CSS inline styles
  *aCount = cssPropertyArray.Length();
  for (int32_t index = 0; index < *aCount; index++) {
    nsresult res = SetCSSProperty(*element, *cssPropertyArray[index],
                                  cssValueArray[index], aSuppressTransaction);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

// Remove from aNode the CSS inline style equivalent to HTMLProperty/aAttribute/aValue for the node
nsresult
CSSEditUtils::RemoveCSSEquivalentToHTMLStyle(nsIDOMNode* aNode,
                                             nsIAtom* aHTMLProperty,
                                             const nsAString* aAttribute,
                                             const nsAString* aValue,
                                             bool aSuppressTransaction)
{
  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(element, NS_OK);

  return RemoveCSSEquivalentToHTMLStyle(element, aHTMLProperty, aAttribute,
                                        aValue, aSuppressTransaction);
}

nsresult
CSSEditUtils::RemoveCSSEquivalentToHTMLStyle(Element* aElement,
                                             nsIAtom* aHTMLProperty,
                                             const nsAString* aAttribute,
                                             const nsAString* aValue,
                                             bool aSuppressTransaction)
{
  MOZ_ASSERT(aElement);

  if (!IsCSSEditableProperty(aElement, aHTMLProperty, aAttribute)) {
    return NS_OK;
  }

  // we can apply the styles only if the node is an element and if we have
  // an equivalence for the requested HTML style in this implementation

  // Find the CSS equivalence to the HTML style
  nsTArray<nsIAtom*> cssPropertyArray;
  nsTArray<nsString> cssValueArray;
  GenerateCSSDeclarationsFromHTMLStyle(aElement, aHTMLProperty, aAttribute,
                                       aValue, cssPropertyArray, cssValueArray,
                                       true);

  // remove the individual CSS inline styles
  int32_t count = cssPropertyArray.Length();
  for (int32_t index = 0; index < count; index++) {
    nsresult res = RemoveCSSProperty(*aElement,
                                     *cssPropertyArray[index],
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
CSSEditUtils::GetCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                   nsIAtom* aHTMLProperty,
                                                   const nsAString* aAttribute,
                                                   nsAString& aValueString,
                                                   StyleType aStyleType)
{
  aValueString.Truncate();
  nsCOMPtr<Element> theElement = GetElementContainerOrSelf(aNode);
  NS_ENSURE_TRUE(theElement, NS_ERROR_NULL_POINTER);

  if (!theElement || !IsCSSEditableProperty(theElement, aHTMLProperty, aAttribute)) {
    return NS_OK;
  }

  // Yes, the requested HTML style has a CSS equivalence in this implementation
  nsTArray<nsIAtom*> cssPropertyArray;
  nsTArray<nsString> cssValueArray;
  // get the CSS equivalence with last param true indicating we want only the
  // "gettable" properties
  GenerateCSSDeclarationsFromHTMLStyle(theElement, aHTMLProperty, aAttribute, nullptr,
                                       cssPropertyArray, cssValueArray, true);
  int32_t count = cssPropertyArray.Length();
  for (int32_t index = 0; index < count; index++) {
    nsAutoString valueString;
    // retrieve the specified/computed value of the property
    nsresult res = GetCSSInlinePropertyBase(theElement, cssPropertyArray[index],
                                            valueString, aStyleType);
    NS_ENSURE_SUCCESS(res, res);
    // append the value to aValueString (possibly with a leading whitespace)
    if (index) {
      aValueString.Append(char16_t(' '));
    }
    aValueString.Append(valueString);
  }
  return NS_OK;
}

// Does the node aNode (or its parent, if it's not an element node) have a CSS
// style equivalent to the HTML style aHTMLProperty/aHTMLAttribute/valueString?
// The value of aStyleType controls the styles we retrieve: specified or
// computed. The return value aIsSet is true if the CSS styles are set.
//
// The nsIContent variant returns aIsSet instead of using an out parameter, and
// does not modify aValue.
bool
CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                  nsIAtom* aProperty,
                                                  const nsAString* aAttribute,
                                                  const nsAString& aValue,
                                                  StyleType aStyleType)
{
  // Use aValue as only an in param, not in-out
  nsAutoString value(aValue);
  return IsCSSEquivalentToHTMLInlineStyleSet(aNode, aProperty, aAttribute,
                                             value, aStyleType);
}

bool
CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                  nsIAtom* aProperty,
                                                  const nsAString* aAttribute,
                                                  nsAString& aValue,
                                                  StyleType aStyleType)
{
  MOZ_ASSERT(aNode && aProperty);
  bool isSet;
  nsresult res = IsCSSEquivalentToHTMLInlineStyleSet(aNode->AsDOMNode(),
                                                     aProperty, aAttribute,
                                                     isSet, aValue, aStyleType);
  NS_ENSURE_SUCCESS(res, false);
  return isSet;
}

nsresult
CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                nsIDOMNode* aNode,
                nsIAtom* aHTMLProperty,
                const nsAString* aHTMLAttribute,
                bool& aIsSet,
                nsAString& valueString,
                StyleType aStyleType)
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

    if (nsGkAtoms::b == aHTMLProperty) {
      if (valueString.EqualsLiteral("bold")) {
        aIsSet = true;
      } else if (valueString.EqualsLiteral("normal")) {
        aIsSet = false;
      } else if (valueString.EqualsLiteral("bolder")) {
        aIsSet = true;
        valueString.AssignLiteral("bold");
      } else {
        int32_t weight = 0;
        nsresult errorCode;
        nsAutoString value(valueString);
        weight = value.ToInteger(&errorCode);
        if (400 < weight) {
          aIsSet = true;
          valueString.AssignLiteral("bold");
        } else {
          aIsSet = false;
          valueString.AssignLiteral("normal");
        }
      }
    } else if (nsGkAtoms::i == aHTMLProperty) {
      if (valueString.EqualsLiteral("italic") ||
          valueString.EqualsLiteral("oblique")) {
        aIsSet = true;
      }
    } else if (nsGkAtoms::u == aHTMLProperty) {
      nsAutoString val;
      val.AssignLiteral("underline");
      aIsSet = ChangeStyleTransaction::ValueIncludes(valueString, val);
    } else if (nsGkAtoms::strike == aHTMLProperty) {
      nsAutoString val;
      val.AssignLiteral("line-through");
      aIsSet = ChangeStyleTransaction::ValueIncludes(valueString, val);
    } else if (aHTMLAttribute &&
               ((nsGkAtoms::font == aHTMLProperty &&
                 aHTMLAttribute->EqualsLiteral("color")) ||
                aHTMLAttribute->EqualsLiteral("bgcolor"))) {
      if (htmlValueString.IsEmpty()) {
        aIsSet = true;
      } else {
        nscolor rgba;
        nsAutoString subStr;
        htmlValueString.Right(subStr, htmlValueString.Length() - 1);
        if (NS_ColorNameToRGB(htmlValueString, &rgba) ||
            NS_HexToRGBA(subStr, nsHexColorType::NoAlpha, &rgba)) {
          nsAutoString htmlColor, tmpStr;

          if (NS_GET_A(rgba) != 255) {
            // This should only be hit by the "transparent" keyword, which
            // currently serializes to "transparent" (not "rgba(0, 0, 0, 0)").
            MOZ_ASSERT(NS_GET_R(rgba) == 0 && NS_GET_G(rgba) == 0 &&
                       NS_GET_B(rgba) == 0 && NS_GET_A(rgba) == 0);
            htmlColor.AppendLiteral("transparent");
          } else {
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

            htmlColor.Append(char16_t(')'));
          }

          aIsSet = htmlColor.Equals(valueString,
                                    nsCaseInsensitiveStringComparator());
        } else {
          aIsSet = htmlValueString.Equals(valueString,
                                    nsCaseInsensitiveStringComparator());
        }
      }
    } else if (nsGkAtoms::tt == aHTMLProperty) {
      aIsSet = StringBeginsWith(valueString, NS_LITERAL_STRING("monospace"));
    } else if (nsGkAtoms::font == aHTMLProperty && aHTMLAttribute &&
               aHTMLAttribute->EqualsLiteral("face")) {
      if (!htmlValueString.IsEmpty()) {
        const char16_t commaSpace[] = { char16_t(','), char16_t(' '), 0 };
        const char16_t comma[] = { char16_t(','), 0 };
        htmlValueString.ReplaceSubstring(commaSpace, comma);
        nsAutoString valueStringNorm(valueString);
        valueStringNorm.ReplaceSubstring(commaSpace, comma);
        aIsSet = htmlValueString.Equals(valueStringNorm,
                                        nsCaseInsensitiveStringComparator());
      } else {
        aIsSet = true;
      }
      return NS_OK;
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

    if (htmlValueString.EqualsLiteral("-moz-editor-invert-value")) {
      aIsSet = !aIsSet;
    }

    if (nsGkAtoms::u == aHTMLProperty || nsGkAtoms::strike == aHTMLProperty) {
      // unfortunately, the value of the text-decoration property is not inherited.
      // that means that we have to look at ancestors of node to see if they are underlined
      node = node->GetParentElement();  // set to null if it's not a dom element
    }
  } while ((nsGkAtoms::u == aHTMLProperty ||
            nsGkAtoms::strike == aHTMLProperty) && !aIsSet && node);
  return NS_OK;
}

void
CSSEditUtils::SetCSSEnabled(bool aIsCSSPrefChecked)
{
  mIsCSSPrefChecked = aIsCSSPrefChecked;
}

bool
CSSEditUtils::IsCSSPrefChecked()
{
  return mIsCSSPrefChecked ;
}

// ElementsSameStyle compares two elements and checks if they have the same
// specified CSS declarations in the STYLE attribute
// The answer is always negative if at least one of them carries an ID or a class
bool
CSSEditUtils::ElementsSameStyle(nsIDOMNode* aFirstNode,
                                nsIDOMNode* aSecondNode)
{
  nsCOMPtr<Element> firstElement  = do_QueryInterface(aFirstNode);
  nsCOMPtr<Element> secondElement = do_QueryInterface(aSecondNode);

  NS_ASSERTION((firstElement && secondElement), "Non element nodes passed to ElementsSameStyle.");
  NS_ENSURE_TRUE(firstElement, false);
  NS_ENSURE_TRUE(secondElement, false);

  return ElementsSameStyle(firstElement, secondElement);
}

bool
CSSEditUtils::ElementsSameStyle(Element* aFirstElement,
                                Element* aSecondElement)
{
  MOZ_ASSERT(aFirstElement);
  MOZ_ASSERT(aSecondElement);

  if (aFirstElement->HasAttr(kNameSpaceID_None, nsGkAtoms::id) ||
      aSecondElement->HasAttr(kNameSpaceID_None, nsGkAtoms::id)) {
    // at least one of the spans carries an ID ; suspect a CSS rule applies to it and
    // refuse to merge the nodes
    return false;
  }

  nsAutoString firstClass, secondClass;
  bool isFirstClassSet = aFirstElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, firstClass);
  bool isSecondClassSet = aSecondElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, secondClass);
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
  } else if (isFirstClassSet || isSecondClassSet) {
    // one span only carries a class, early way out
    return false;
  }

  nsCOMPtr<nsIDOMCSSStyleDeclaration> firstCSSDecl, secondCSSDecl;
  uint32_t firstLength, secondLength;
  nsresult rv = GetInlineStyles(aFirstElement,  getter_AddRefs(firstCSSDecl),  &firstLength);
  if (NS_FAILED(rv) || !firstCSSDecl) {
    return false;
  }
  rv = GetInlineStyles(aSecondElement, getter_AddRefs(secondCSSDecl), &secondLength);
  if (NS_FAILED(rv) || !secondCSSDecl) {
    return false;
  }

  if (firstLength != secondLength) {
    // early way out if we can
    return false;
  }

  if (!firstLength) {
    // no inline style !
    return true;
  }

  nsAutoString propertyNameString;
  nsAutoString firstValue, secondValue;
  for (uint32_t i = 0; i < firstLength; i++) {
    firstCSSDecl->Item(i, propertyNameString);
    firstCSSDecl->GetPropertyValue(propertyNameString, firstValue);
    secondCSSDecl->GetPropertyValue(propertyNameString, secondValue);
    if (!firstValue.Equals(secondValue)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < secondLength; i++) {
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
CSSEditUtils::GetInlineStyles(Element* aElement,
                              nsIDOMCSSStyleDeclaration** aCssDecl,
                              uint32_t* aLength)
{
  return GetInlineStyles(static_cast<nsISupports*>(aElement), aCssDecl, aLength);
}

nsresult
CSSEditUtils::GetInlineStyles(nsIDOMElement* aElement,
                              nsIDOMCSSStyleDeclaration** aCssDecl,
                              uint32_t* aLength)
{
  return GetInlineStyles(static_cast<nsISupports*>(aElement), aCssDecl, aLength);
}

nsresult
CSSEditUtils::GetInlineStyles(nsISupports* aElement,
                              nsIDOMCSSStyleDeclaration** aCssDecl,
                              uint32_t* aLength)
{
  NS_ENSURE_TRUE(aElement && aLength, NS_ERROR_NULL_POINTER);
  *aLength = 0;
  nsCOMPtr<nsStyledElement> inlineStyles = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl =
    do_QueryInterface(inlineStyles->Style());
  MOZ_ASSERT(cssDecl);

  cssDecl.forget(aCssDecl);
  (*aCssDecl)->GetLength(aLength);
  return NS_OK;
}

already_AddRefed<nsIDOMElement>
CSSEditUtils::GetElementContainerOrSelf(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, nullptr);
  nsCOMPtr<nsIDOMElement> element =
    do_QueryInterface(GetElementContainerOrSelf(node));
  return element.forget();
}

Element*
CSSEditUtils::GetElementContainerOrSelf(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (nsIDOMNode::DOCUMENT_NODE == aNode->NodeType()) {
    return nullptr;
  }

  nsINode* node = aNode;
  // Loop until we find an element.
  while (node && !node->IsElement()) {
    node = node->GetParentNode();
  }

  NS_ENSURE_TRUE(node, nullptr);
  return node->AsElement();
}

nsresult
CSSEditUtils::SetCSSProperty(nsIDOMElement* aElement,
                             const nsAString& aProperty,
                             const nsAString& aValue)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  uint32_t length;
  nsresult res = GetInlineStyles(aElement, getter_AddRefs(cssDecl), &length);
  if (NS_FAILED(res) || !cssDecl) return res;

  return cssDecl->SetProperty(aProperty,
                              aValue,
                              EmptyString());
}

nsresult
CSSEditUtils::SetCSSPropertyPixels(nsIDOMElement* aElement,
                                   const nsAString& aProperty,
                                   int32_t aIntValue)
{
  nsAutoString s;
  s.AppendInt(aIntValue);
  return SetCSSProperty(aElement, aProperty, s + NS_LITERAL_STRING("px"));
}

} // namespace mozilla
