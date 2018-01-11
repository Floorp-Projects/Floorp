/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"

#include "inDOMUtils.h"
#include "inLayoutUtils.h"

#include "nsArray.h"
#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIContentInlines.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCharacterData.h"
#include "nsRuleNode.h"
#include "nsIStyleRule.h"
#include "mozilla/css/StyleRule.h"
#include "nsICSSStyleRuleDOMWrapper.h"
#include "nsIDOMWindow.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIMutableArray.h"
#include "nsBindingManager.h"
#include "ChildIterator.h"
#include "nsComputedDOMStyle.h"
#include "mozilla/EventStateManager.h"
#include "nsAtom.h"
#include "nsRange.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/Element.h"
#include "nsRuleWalker.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/CSSLexer.h"
#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsColor.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsStyleUtil.h"
#include "nsQueryObject.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleRule.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/dom/InspectorUtils.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

extern const char* const kCSSRawProperties[];

///////////////////////////////////////////////////////////////////////////////

inDOMUtils::inDOMUtils()
{
}

inDOMUtils::~inDOMUtils()
{
}

NS_IMPL_ISUPPORTS(inDOMUtils, inIDOMUtils)

///////////////////////////////////////////////////////////////////////////////
// inIDOMUtils

namespace mozilla {
namespace dom {

/* static */ void
InspectorUtils::GetAllStyleSheets(GlobalObject& aGlobalObject,
                                  nsIDocument& aDocument,
                                  nsTArray<RefPtr<StyleSheet>>& aResult)
{
  // Get the agent, then user and finally xbl sheets in the style set.
  nsIPresShell* presShell = aDocument.GetShell();

  if (presShell) {
    StyleSetHandle styleSet = presShell->StyleSet();
    SheetType sheetType = SheetType::Agent;
    for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
      aResult.AppendElement(styleSet->StyleSheetAt(sheetType, i));
    }
    sheetType = SheetType::User;
    for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
      aResult.AppendElement(styleSet->StyleSheetAt(sheetType, i));
    }

    AutoTArray<StyleSheet*, 32> xblSheetArray;
    styleSet->AppendAllXBLStyleSheets(xblSheetArray);

    // The XBL stylesheet array will quite often be full of duplicates. Cope:
    nsTHashtable<nsPtrHashKey<StyleSheet>> sheetSet;
    for (StyleSheet* sheet : xblSheetArray) {
      if (!sheetSet.Contains(sheet)) {
        sheetSet.PutEntry(sheet);
        aResult.AppendElement(sheet);
      }
    }
  }

  // Get the document sheets.
  for (size_t i = 0; i < aDocument.SheetCount(); i++) {
    aResult.AppendElement(aDocument.SheetAt(i));
  }
}

} // namespace dom
} // namespace mozilla

NS_IMETHODIMP
inDOMUtils::IsIgnorableWhitespace(nsIDOMCharacterData *aDataNode,
                                  bool *aReturn)
{
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  NS_ENSURE_ARG_POINTER(aDataNode);

  *aReturn = false;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aDataNode);
  NS_ASSERTION(content, "Does not implement nsIContent!");

  if (!content->TextIsOnlyWhitespace()) {
    return NS_OK;
  }

  // Okay.  We have only white space.  Let's check the white-space
  // property now and make sure that this isn't preformatted text...
  nsIFrame* frame = content->GetPrimaryFrame();
  if (frame) {
    const nsStyleText* text = frame->StyleText();
    *aReturn = !text->WhiteSpaceIsSignificant();
  }
  else {
    // empty inter-tag text node without frame, e.g., in between <table>\n<tr>
    *aReturn = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetParentForNode(nsIDOMNode* aNode,
                             bool aShowingAnonymousContent,
                             nsIDOMNode** aParent)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // First do the special cases -- document nodes and anonymous content
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aNode));
  nsCOMPtr<nsIDOMNode> parent;

  if (doc) {
    parent = inLayoutUtils::GetContainerFor(*doc);
  } else if (aShowingAnonymousContent) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      nsIContent* bparent = content->GetFlattenedTreeParent();
      parent = do_QueryInterface(bparent);
    }
  }

  if (!parent) {
    // Ok, just get the normal DOM parent node
    aNode->GetParentNode(getter_AddRefs(parent));
  }

  NS_IF_ADDREF(*aParent = parent);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetChildrenForNode(nsIDOMNode* aNode,
                               bool aShowingAnonymousContent,
                               nsIDOMNodeList** aChildren)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_PRECONDITION(aChildren, "Must have an out parameter");

  nsCOMPtr<nsIDOMNodeList> kids;

  if (aShowingAnonymousContent) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      kids = content->GetChildren(nsIContent::eAllChildren);
    }
  }

  if (!kids) {
    aNode->GetChildNodes(getter_AddRefs(kids));
  }

  kids.forget(aChildren);
  return NS_OK;
}

namespace mozilla {
namespace dom {

/* static */ void
InspectorUtils::GetCSSStyleRules(GlobalObject& aGlobalObject,
                                 Element& aElement,
                                 const nsAString& aPseudo,
                                 nsTArray<RefPtr<css::Rule>>& aResult)
{
  RefPtr<nsAtom> pseudoElt;
  if (!aPseudo.IsEmpty()) {
    pseudoElt = NS_Atomize(aPseudo);
  }

  RefPtr<nsStyleContext> styleContext =
    GetCleanStyleContextForElement(&aElement, pseudoElt);
  if (!styleContext) {
    // This can fail for elements that are not in the document or
    // if the document they're in doesn't have a presshell.  Bail out.
    return;
  }


  if (auto gecko = styleContext->GetAsGecko()) {
    nsRuleNode* ruleNode = gecko->RuleNode();
    if (!ruleNode) {
      return;
    }

    AutoTArray<nsRuleNode*, 16> ruleNodes;
    while (!ruleNode->IsRoot()) {
      ruleNodes.AppendElement(ruleNode);
      ruleNode = ruleNode->GetParent();
    }

    for (nsRuleNode* ruleNode : Reversed(ruleNodes)) {
      RefPtr<Declaration> decl = do_QueryObject(ruleNode->GetRule());
      if (decl) {
        css::Rule* owningRule = decl->GetOwningRule();
        if (owningRule) {
          aResult.AppendElement(owningRule);
        }
      }
    }
  } else {
    nsIDocument* doc = aElement.OwnerDoc();
    nsIPresShell* shell = doc->GetShell();
    if (!shell) {
      return;
    }

    ServoStyleContext* servo = styleContext->AsServo();
    nsTArray<const RawServoStyleRule*> rawRuleList;
    Servo_ComputedValues_GetStyleRuleList(servo, &rawRuleList);

    AutoTArray<ServoStyleRuleMap*, 1> maps;
    {
      ServoStyleSet* styleSet = shell->StyleSet()->AsServo();
      ServoStyleRuleMap* map = styleSet->StyleRuleMap();
      map->EnsureTable();
      maps.AppendElement(map);
    }

    // Collect style rule maps for bindings.
    for (nsIContent* bindingContent = &aElement; bindingContent;
         bindingContent = bindingContent->GetBindingParent()) {
      for (nsXBLBinding* binding = bindingContent->GetXBLBinding();
           binding; binding = binding->GetBaseBinding()) {
        if (ServoStyleSet* styleSet = binding->GetServoStyleSet()) {
          ServoStyleRuleMap* map = styleSet->StyleRuleMap();
          map->EnsureTable();
          maps.AppendElement(map);
        }
      }
      // Note that we intentionally don't cut off here, unlike when we
      // do styling, because even if style rules from parent binding
      // do not apply to the element directly in those cases, their
      // rules may still show up in the list we get above due to the
      // inheritance in cascading.
    }

    // Find matching rules in the table.
    for (const RawServoStyleRule* rawRule : Reversed(rawRuleList)) {
      ServoStyleRule* rule = nullptr;
      for (ServoStyleRuleMap* map : maps) {
        rule = map->Lookup(rawRule);
        if (rule) {
          break;
        }
      }
      if (rule) {
        aResult.AppendElement(rule);
      } else {
        MOZ_ASSERT_UNREACHABLE("We should be able to map a raw rule to a rule");
      }
    }
  }
}

/* static */ uint32_t
InspectorUtils::GetRuleLine(GlobalObject& aGlobal, css::Rule& aRule)
{
  return aRule.GetLineNumber();
}

/* static */ uint32_t
InspectorUtils::GetRuleColumn(GlobalObject& aGlobal, css::Rule& aRule)
{
  return aRule.GetColumnNumber();
}

/* static */ uint32_t
InspectorUtils::GetRelativeRuleLine(GlobalObject& aGlobal, css::Rule& aRule)
{
  uint32_t lineNumber = aRule.GetLineNumber();
  StyleSheet* sheet = aRule.GetStyleSheet();
  if (sheet && lineNumber != 0) {
    nsINode* owningNode = sheet->GetOwnerNode();
    if (owningNode) {
      nsCOMPtr<nsIStyleSheetLinkingElement> link =
        do_QueryInterface(owningNode);
      if (link) {
        lineNumber -= link->GetLineNumber() - 1;
      }
    }
  }

  return lineNumber;
}

/* static */ CSSLexer*
InspectorUtils::GetCSSLexer(GlobalObject& aGlobal, const nsAString& aText)
{
  return new CSSLexer(aText);
}

/* static */ uint32_t
InspectorUtils::GetSelectorCount(GlobalObject& aGlobal,
                                 BindingStyleRule& aRule)
{
  return aRule.GetSelectorCount();
}

/* static */ void
InspectorUtils::GetSelectorText(GlobalObject& aGlobal,
                                BindingStyleRule& aRule,
                                uint32_t aSelectorIndex,
                                nsString& aText,
                                ErrorResult& aRv)
{
  aRv = aRule.GetSelectorText(aSelectorIndex, aText);
}

/* static */ uint64_t
InspectorUtils::GetSpecificity(GlobalObject& aGlobal,
                               BindingStyleRule& aRule,
                               uint32_t aSelectorIndex,
                               ErrorResult& aRv)
{
  uint64_t s;
  aRv = aRule.GetSpecificity(aSelectorIndex, &s);
  return s;
}

/* static */ bool
InspectorUtils::SelectorMatchesElement(GlobalObject& aGlobalObject,
                                       Element& aElement,
                                       BindingStyleRule& aRule,
                                       uint32_t aSelectorIndex,
                                       const nsAString& aPseudo,
                                       ErrorResult& aRv)
{
  bool result = false;
  aRv = aRule.SelectorMatchesElement(&aElement, aSelectorIndex, aPseudo,
                                     &result);
  return result;
}

/* static */ bool
InspectorUtils::IsInheritedProperty(GlobalObject& aGlobalObject,
                                    const nsAString& aPropertyName)
{
  nsCSSPropertyID prop = nsCSSProps::
    LookupProperty(aPropertyName, CSSEnabledState::eIgnoreEnabledState);
  if (prop == eCSSProperty_UNKNOWN) {
    return false;
  }

  if (prop == eCSSPropertyExtra_variable) {
    return true;
  }

  if (nsCSSProps::IsShorthand(prop)) {
    prop = nsCSSProps::SubpropertyEntryFor(prop)[0];
  }

  nsStyleStructID sid = nsCSSProps::kSIDTable[prop];
  return !nsStyleContext::IsReset(sid);
}

/* static */ void
InspectorUtils::GetCSSPropertyNames(GlobalObject& aGlobalObject,
                                    const PropertyNamesOptions& aOptions,
                                    nsTArray<nsString>& aResult)
{
#define DO_PROP(_prop)                                                      \
  PR_BEGIN_MACRO                                                            \
    nsCSSPropertyID cssProp = nsCSSPropertyID(_prop);                       \
    if (nsCSSProps::IsEnabled(cssProp, CSSEnabledState::eForAllContent)) {  \
      nsDependentCString name(kCSSRawProperties[_prop]);                    \
      aResult.AppendElement(NS_ConvertASCIItoUTF16(name));                  \
    }                                                                       \
  PR_END_MACRO

  uint32_t prop = 0;
  for ( ; prop < eCSSProperty_COUNT_no_shorthands; ++prop) {
    if (nsCSSProps::PropertyParseType(nsCSSPropertyID(prop)) !=
        CSS_PROPERTY_PARSE_INACCESSIBLE) {
      DO_PROP(prop);
    }
  }

  for ( ; prop < eCSSProperty_COUNT; ++prop) {
    // Some shorthands are also aliases
    if (aOptions.mIncludeAliases ||
        !nsCSSProps::PropHasFlags(nsCSSPropertyID(prop),
                                  CSS_PROPERTY_IS_ALIAS)) {
      DO_PROP(prop);
    }
  }

  if (aOptions.mIncludeAliases) {
    for (prop = eCSSProperty_COUNT; prop < eCSSProperty_COUNT_with_aliases; ++prop) {
      DO_PROP(prop);
    }
  }

#undef DO_PROP
}

} // namespace dom
} // namespace mozilla

static void InsertNoDuplicates(nsTArray<nsString>& aArray,
                               const nsAString& aString)
{
  size_t i = aArray.IndexOfFirstElementGt(aString);
  if (i > 0 && aArray[i-1].Equals(aString)) {
    return;
  }
  aArray.InsertElementAt(i, aString);
}

static void GetKeywordsForProperty(const nsCSSPropertyID aProperty,
                                   nsTArray<nsString>& aArray)
{
  if (nsCSSProps::IsShorthand(aProperty)) {
    // Shorthand props have no keywords.
    return;
  }
  const nsCSSProps::KTableEntry* keywordTable =
    nsCSSProps::kKeywordTableTable[aProperty];
  if (keywordTable) {
    for (size_t i = 0; keywordTable[i].mKeyword != eCSSKeyword_UNKNOWN; ++i) {
      nsCSSKeyword word = keywordTable[i].mKeyword;

      // These are extra -moz values which are added while rebuilding
      // the properties db. These values are not relevant and are not
      // documented on MDN, so filter these out
      if (word != eCSSKeyword__moz_zoom_in && word != eCSSKeyword__moz_zoom_out &&
          word != eCSSKeyword__moz_grab && word != eCSSKeyword__moz_grabbing) {
          InsertNoDuplicates(aArray,
                  NS_ConvertASCIItoUTF16(nsCSSKeywords::GetStringValue(word)));
      }
    }
  }
}

static void GetColorsForProperty(const uint32_t aParserVariant,
                                 nsTArray<nsString>& aArray)
{
  if (aParserVariant & VARIANT_COLOR) {
    // GetKeywordsForProperty and GetOtherValuesForProperty assume aArray is sorted,
    // and if aArray is not empty here, then it's not going to be sorted coming out.
    MOZ_ASSERT(aArray.Length() == 0);
    size_t size;
    const char * const *allColorNames = NS_AllColorNames(&size);
    nsString* utf16Names = aArray.AppendElements(size);
    for (size_t i = 0; i < size; i++) {
      CopyASCIItoUTF16(allColorNames[i], utf16Names[i]);
    }
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("currentColor"));
  }
}

static void GetOtherValuesForProperty(const uint32_t aParserVariant,
                                      nsTArray<nsString>& aArray)
{
  if (aParserVariant & VARIANT_AUTO) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("auto"));
  }
  if (aParserVariant & VARIANT_NORMAL) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("normal"));
  }
  if(aParserVariant & VARIANT_ALL) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("all"));
  }
  if (aParserVariant & VARIANT_NONE) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("none"));
  }
  if (aParserVariant & VARIANT_ELEMENT) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-element"));
  }
  if (aParserVariant & VARIANT_IMAGE_RECT) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-image-rect"));
  }
  if (aParserVariant & VARIANT_COLOR) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("rgb"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("hsl"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("rgba"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("hsla"));
  }
  if (aParserVariant & VARIANT_TIMING_FUNCTION) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("cubic-bezier"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("steps"));
  }
  if (aParserVariant & VARIANT_CALC) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("calc"));
  }
  if (aParserVariant & VARIANT_URL) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("url"));
  }
  if (aParserVariant & VARIANT_GRADIENT) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("linear-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("radial-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("repeating-linear-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("repeating-radial-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-linear-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-radial-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-repeating-linear-gradient"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-repeating-radial-gradient"));
  }
}

NS_IMETHODIMP
inDOMUtils::GetSubpropertiesForCSSProperty(const nsAString& aProperty,
                                           uint32_t* aLength,
                                           char16_t*** aValues)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);

  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  if (propertyID == eCSSPropertyExtra_variable) {
    *aValues = static_cast<char16_t**>(moz_xmalloc(sizeof(char16_t*)));
    (*aValues)[0] = ToNewUnicode(aProperty);
    *aLength = 1;
    return NS_OK;
  }

  if (!nsCSSProps::IsShorthand(propertyID)) {
    *aValues = static_cast<char16_t**>(moz_xmalloc(sizeof(char16_t*)));
    (*aValues)[0] = ToNewUnicode(nsCSSProps::GetStringValue(propertyID));
    *aLength = 1;
    return NS_OK;
  }

  // Count up how many subproperties we have.
  size_t subpropCount = 0;
  for (const nsCSSPropertyID *props = nsCSSProps::SubpropertyEntryFor(propertyID);
       *props != eCSSProperty_UNKNOWN; ++props) {
    ++subpropCount;
  }

  *aValues =
    static_cast<char16_t**>(moz_xmalloc(subpropCount * sizeof(char16_t*)));
  *aLength = subpropCount;
  for (const nsCSSPropertyID *props = nsCSSProps::SubpropertyEntryFor(propertyID),
                           *props_start = props;
       *props != eCSSProperty_UNKNOWN; ++props) {
    (*aValues)[props-props_start] = ToNewUnicode(nsCSSProps::GetStringValue(*props));
  }
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::CssPropertyIsShorthand(const nsAString& aProperty, bool *_retval)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  if (propertyID == eCSSPropertyExtra_variable) {
    *_retval = false;
  } else {
    *_retval = nsCSSProps::IsShorthand(propertyID);
  }
  return NS_OK;
}

// A helper function that determines whether the given property
// supports the given type.
static bool
PropertySupportsVariant(nsCSSPropertyID aPropertyID, uint32_t aVariant)
{
  if (nsCSSProps::IsShorthand(aPropertyID)) {
    // We need a special case for border here, because while it resets
    // border-image, it can't actually parse an image.
    if (aPropertyID == eCSSProperty_border) {
      return (aVariant & (VARIANT_COLOR | VARIANT_LENGTH)) != 0;
    }

    for (const nsCSSPropertyID* props = nsCSSProps::SubpropertyEntryFor(aPropertyID);
         *props != eCSSProperty_UNKNOWN; ++props) {
      if (PropertySupportsVariant(*props, aVariant)) {
        return true;
      }
    }
    return false;
  }

  // Properties that are parsed by functions must have their
  // attributes hand-maintained here.
  if (nsCSSProps::PropHasFlags(aPropertyID, CSS_PROPERTY_VALUE_PARSER_FUNCTION) ||
      nsCSSProps::PropertyParseType(aPropertyID) == CSS_PROPERTY_PARSE_FUNCTION) {
    // These must all be special-cased.
    uint32_t supported;
    switch (aPropertyID) {
      case eCSSProperty_border_image_slice:
      case eCSSProperty_grid_template:
      case eCSSProperty_grid:
        supported = VARIANT_PN;
        break;

      case eCSSProperty_border_image_outset:
        supported = VARIANT_LN;
        break;

      case eCSSProperty_border_image_width:
      case eCSSProperty_stroke_dasharray:
        supported = VARIANT_LPN;
        break;

      case eCSSProperty_border_top_left_radius:
      case eCSSProperty_border_top_right_radius:
      case eCSSProperty_border_bottom_left_radius:
      case eCSSProperty_border_bottom_right_radius:
      case eCSSProperty_background_position:
      case eCSSProperty_background_position_x:
      case eCSSProperty_background_position_y:
      case eCSSProperty_background_size:
      case eCSSProperty_mask_position:
      case eCSSProperty_mask_position_x:
      case eCSSProperty_mask_position_y:
      case eCSSProperty_mask_size:
      case eCSSProperty_grid_auto_columns:
      case eCSSProperty_grid_auto_rows:
      case eCSSProperty_grid_template_columns:
      case eCSSProperty_grid_template_rows:
      case eCSSProperty_object_position:
      case eCSSProperty_scroll_snap_coordinate:
      case eCSSProperty_scroll_snap_destination:
      case eCSSProperty_transform_origin:
      case eCSSProperty_perspective_origin:
      case eCSSProperty__moz_outline_radius_topleft:
      case eCSSProperty__moz_outline_radius_topright:
      case eCSSProperty__moz_outline_radius_bottomleft:
      case eCSSProperty__moz_outline_radius_bottomright:
      case eCSSProperty__moz_window_transform_origin:
        supported = VARIANT_LP;
        break;

      case eCSSProperty__moz_border_bottom_colors:
      case eCSSProperty__moz_border_left_colors:
      case eCSSProperty__moz_border_right_colors:
      case eCSSProperty__moz_border_top_colors:
        supported = VARIANT_COLOR;
        break;

      case eCSSProperty_text_shadow:
      case eCSSProperty_box_shadow:
        supported = VARIANT_LENGTH | VARIANT_COLOR;
        break;

      case eCSSProperty_border_spacing:
        supported = VARIANT_LENGTH;
        break;

      case eCSSProperty_content:
      case eCSSProperty_cursor:
      case eCSSProperty_clip_path:
        supported = VARIANT_URL;
        break;

      case eCSSProperty_shape_outside:
        supported = VARIANT_IMAGE;
        break;

      case eCSSProperty_fill:
      case eCSSProperty_stroke:
        supported = VARIANT_COLOR | VARIANT_URL;
        break;

      case eCSSProperty_image_orientation:
        supported = VARIANT_ANGLE;
        break;

      case eCSSProperty_filter:
        supported = VARIANT_URL;
        break;

      case eCSSProperty_grid_column_start:
      case eCSSProperty_grid_column_end:
      case eCSSProperty_grid_row_start:
      case eCSSProperty_grid_row_end:
      case eCSSProperty_font_weight:
      case eCSSProperty_initial_letter:
        supported = VARIANT_NUMBER;
        break;

      default:
        supported = 0;
        break;
    }

    return (supported & aVariant) != 0;
  }

  return (nsCSSProps::ParserVariant(aPropertyID) & aVariant) != 0;
}

NS_IMETHODIMP
inDOMUtils::CssPropertySupportsType(const nsAString& aProperty, uint32_t aType,
                                    bool *_retval)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  if (propertyID >= eCSSProperty_COUNT) {
    *_retval = false;
    return NS_OK;
  }

  uint32_t variant;
  switch (aType) {
  case TYPE_LENGTH:
    variant = VARIANT_LENGTH;
    break;
  case TYPE_PERCENTAGE:
    variant = VARIANT_PERCENT;
    break;
  case TYPE_COLOR:
    variant = VARIANT_COLOR;
    break;
  case TYPE_URL:
    variant = VARIANT_URL;
    break;
  case TYPE_ANGLE:
    variant = VARIANT_ANGLE;
    break;
  case TYPE_FREQUENCY:
    variant = VARIANT_FREQUENCY;
    break;
  case TYPE_TIME:
    variant = VARIANT_TIME;
    break;
  case TYPE_GRADIENT:
    variant = VARIANT_GRADIENT;
    break;
  case TYPE_TIMING_FUNCTION:
    variant = VARIANT_TIMING_FUNCTION;
    break;
  case TYPE_IMAGE_RECT:
    variant = VARIANT_IMAGE_RECT;
    break;
  case TYPE_NUMBER:
    // Include integers under "number"?
    variant = VARIANT_NUMBER | VARIANT_INTEGER;
    break;
  default:
    // Unknown type
    return NS_ERROR_NOT_AVAILABLE;
  }

  *_retval = PropertySupportsVariant(propertyID, variant);
  return NS_OK;
}

namespace mozilla {
namespace dom {

/* static */ void
InspectorUtils::GetCSSValuesForProperty(GlobalObject& aGlobalObject,
                                        const nsAString& aProperty,
                                        nsTArray<nsString>& aResult,
                                        ErrorResult& aRv)
{
  nsCSSPropertyID propertyID = nsCSSProps::
    LookupProperty(aProperty, CSSEnabledState::eForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // We start collecting the values, BUT colors need to go in first, because aResult
  // needs to stay sorted, and the colors are sorted, so we just append them.
  if (propertyID == eCSSPropertyExtra_variable) {
    // No other values we can report.
  } else if (!nsCSSProps::IsShorthand(propertyID)) {
    // Property is longhand.
    uint32_t propertyParserVariant = nsCSSProps::ParserVariant(propertyID);
    // Get colors first.
    GetColorsForProperty(propertyParserVariant, aResult);
    GetKeywordsForProperty(propertyID, aResult);
    GetOtherValuesForProperty(propertyParserVariant, aResult);
  } else {
    // Property is shorthand.
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subproperty, propertyID,
                                         CSSEnabledState::eForAllContent) {
      // Get colors (once) first.
      uint32_t propertyParserVariant = nsCSSProps::ParserVariant(*subproperty);
      if (propertyParserVariant & VARIANT_COLOR) {
        GetColorsForProperty(propertyParserVariant, aResult);
        break;
      }
    }
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subproperty, propertyID,
                                         CSSEnabledState::eForAllContent) {
      uint32_t propertyParserVariant = nsCSSProps::ParserVariant(*subproperty);
      GetKeywordsForProperty(*subproperty, aResult);
      GetOtherValuesForProperty(propertyParserVariant, aResult);
    }
  }
  // All CSS properties take initial, inherit and unset.
  InsertNoDuplicates(aResult, NS_LITERAL_STRING("initial"));
  InsertNoDuplicates(aResult, NS_LITERAL_STRING("inherit"));
  InsertNoDuplicates(aResult, NS_LITERAL_STRING("unset"));
}

/* static */ void
InspectorUtils::RgbToColorName(GlobalObject& aGlobalObject,
                               uint8_t aR, uint8_t aG, uint8_t aB,
                               nsAString& aColorName,
                               ErrorResult& aRv)
{
  const char* color = NS_RGBToColorName(NS_RGB(aR, aG, aB));
  if (!color) {
    aColorName.Truncate();
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aColorName.AssignASCII(color);
}

/* static */ void
InspectorUtils::ColorToRGBA(GlobalObject& aGlobalObject,
                            const nsAString& aColorString,
                            Nullable<InspectorRGBATuple>& aResult)
{
  nscolor color = NS_RGB(0, 0, 0);

#ifdef MOZ_STYLO
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), aColorString,
                                    &color)) {
    aResult.SetNull();
    return;
  }
#else
  nsCSSParser cssParser;
  nsCSSValue cssValue;

  if (!cssParser.ParseColorString(aColorString, nullptr, 0, cssValue, true)) {
    aResult.SetNull();
    return;
  }

  nsRuleNode::ComputeColor(cssValue, nullptr, nullptr, color);
#endif

  InspectorRGBATuple& tuple = aResult.SetValue();
  tuple.mR = NS_GET_R(color);
  tuple.mG = NS_GET_G(color);
  tuple.mB = NS_GET_B(color);
  tuple.mA = nsStyleUtil::ColorComponentToFloat(NS_GET_A(color));
}

} // namespace dom
} // namespace mozilla

NS_IMETHODIMP
inDOMUtils::IsValidCSSColor(const nsAString& aColorString, bool *_retval)
{
#ifdef MOZ_STYLO
  *_retval = ServoCSSParser::IsValidCSSColor(aColorString);
#else
  nsCSSParser cssParser;
  nsCSSValue cssValue;
  *_retval = cssParser.ParseColorString(aColorString, nullptr, 0, cssValue, true);
#endif
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetBindingURLs(nsIDOMElement *aElement, nsIArray **_retval)
{
  NS_ENSURE_ARG_POINTER(aElement);

  *_retval = nullptr;

  nsCOMPtr<nsIMutableArray> urls = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!urls)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(content);

  nsXBLBinding *binding = content->GetXBLBinding();

  while (binding) {
    urls->AppendElement(binding->PrototypeBinding()->BindingURI());
    binding = binding->GetBaseBinding();
  }

  urls.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::SetContentState(nsIDOMElement* aElement,
                            EventStates::InternalType aState,
                            bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aElement);

  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  NS_ENSURE_TRUE(esm, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIContent> content;
  content = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(content, NS_ERROR_INVALID_ARG);

  *aRetVal = esm->SetContentState(content, EventStates(aState));
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::RemoveContentState(nsIDOMElement* aElement,
                               EventStates::InternalType aState,
                               bool aClearActiveDocument,
                               bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aElement);

  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  NS_ENSURE_TRUE(esm, NS_ERROR_INVALID_ARG);

  *aRetVal = esm->SetContentState(nullptr, EventStates(aState));

  if (aClearActiveDocument && EventStates(aState) == NS_EVENT_STATE_ACTIVE) {
    EventStateManager* activeESM = static_cast<EventStateManager*>(
      EventStateManager::GetActiveEventStateManager());
    if (activeESM == esm) {
      EventStateManager::ClearGlobalActiveContent(nullptr);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetContentState(nsIDOMElement* aElement,
                            EventStates::InternalType* aState)
{
  *aState = 0;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(content);

  // NOTE: if this method is removed,
  // please remove GetInternalValue from EventStates
  *aState = content->AsElement()->State().GetInternalValue();
  return NS_OK;
}

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<nsStyleContext>
InspectorUtils::GetCleanStyleContextForElement(dom::Element* aElement,
                                               nsAtom* aPseudo)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsIPresShell *presShell = doc->GetShell();
  if (!presShell) {
    return nullptr;
  }

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext) {
    return nullptr;
  }

  presContext->EnsureSafeToHandOutCSSRules();

  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContext(aElement, aPseudo, presShell);
  return styleContext.forget();
}

} // namespace dom
} // namespace mozilla

NS_IMETHODIMP
inDOMUtils::GetUsedFontFaces(nsIDOMRange* aRange,
                             nsIDOMFontFaceList** aFontFaceList)
{
  return static_cast<nsRange*>(aRange)->GetUsedFontFaces(aFontFaceList);
}

static EventStates
GetStatesForPseudoClass(const nsAString& aStatePseudo)
{
  // An array of the states that are relevant for various pseudoclasses.
  // XXXbz this duplicates code in nsCSSRuleProcessor
  static const EventStates sPseudoClassStates[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
    EventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _flags, _pref, _states) \
    _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS

    // Add more entries for our fake values to make sure we can't
    // index out of bounds into this array no matter what.
    EventStates(),
    EventStates()
  };
  static_assert(MOZ_ARRAY_LENGTH(sPseudoClassStates) ==
                static_cast<size_t>(CSSPseudoClassType::MAX),
                "Length of PseudoClassStates array is incorrect");

  RefPtr<nsAtom> atom = NS_Atomize(aStatePseudo);
  CSSPseudoClassType type = nsCSSPseudoClasses::
    GetPseudoType(atom, CSSEnabledState::eIgnoreEnabledState);

  // Ignore :any-link so we don't give the element simultaneous
  // visited and unvisited style state
  if (type == CSSPseudoClassType::anyLink ||
      type == CSSPseudoClassType::mozAnyLink) {
    return EventStates();
  }
  // Our array above is long enough that indexing into it with
  // NotPseudo is ok.
  return sPseudoClassStates[static_cast<CSSPseudoClassTypeBase>(type)];
}

NS_IMETHODIMP
inDOMUtils::GetCSSPseudoElementNames(uint32_t* aLength, char16_t*** aNames)
{
  nsTArray<nsAtom*> array;

  const CSSPseudoElementTypeBase pseudoCount =
    static_cast<CSSPseudoElementTypeBase>(CSSPseudoElementType::Count);
  for (CSSPseudoElementTypeBase i = 0; i < pseudoCount; ++i) {
    CSSPseudoElementType type = static_cast<CSSPseudoElementType>(i);
    if (nsCSSPseudoElements::IsEnabled(type, CSSEnabledState::eForAllContent)) {
      nsAtom* atom = nsCSSPseudoElements::GetPseudoAtom(type);
      array.AppendElement(atom);
    }
  }

  *aLength = array.Length();
  char16_t** ret =
    static_cast<char16_t**>(moz_xmalloc(*aLength * sizeof(char16_t*)));
  for (uint32_t i = 0; i < *aLength; ++i) {
    ret[i] = ToNewUnicode(nsDependentAtomString(array[i]));
  }
  *aNames = ret;
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::AddPseudoClassLock(nsIDOMElement *aElement,
                               const nsAString &aPseudoClass,
                               bool aEnabled,
                               uint8_t aArgc)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  element->LockStyleStates(state, aArgc > 0 ? aEnabled : true);

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::RemovePseudoClassLock(nsIDOMElement *aElement,
                                  const nsAString &aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  element->UnlockStyleStates(state);

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::HasPseudoClassLock(nsIDOMElement *aElement,
                               const nsAString &aPseudoClass,
                               bool *_retval)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    *_retval = false;
    return NS_OK;
  }

  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  EventStates locks = element->LockedStyleStates().mLocks;

  *_retval = locks.HasAllStates(state);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::ClearPseudoClassLocks(nsIDOMElement *aElement)
{
  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  element->ClearStyleStateLocks();

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::ParseStyleSheet(nsIDOMCSSStyleSheet *aSheet,
                            const nsAString& aInput)
{
  RefPtr<CSSStyleSheet> geckoSheet = do_QueryObject(aSheet);
  if (geckoSheet) {
    NS_ENSURE_ARG_POINTER(geckoSheet);
    return geckoSheet->ReparseSheet(aInput);
  }

  RefPtr<ServoStyleSheet> servoSheet = do_QueryObject(aSheet);
  if (servoSheet) {
    NS_ENSURE_ARG_POINTER(servoSheet);
    return servoSheet->ReparseSheet(aInput);
  }

  return NS_ERROR_INVALID_POINTER;
}

NS_IMETHODIMP
inDOMUtils::ScrollElementIntoView(nsIDOMElement *aElement)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(content);

  nsIPresShell* presShell = content->OwnerDoc()->GetShell();
  if (!presShell) {
    return NS_OK;
  }

  presShell->ScrollContentIntoView(content,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);

  return NS_OK;
}
