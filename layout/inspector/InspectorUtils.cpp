/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"

#include "inLayoutUtils.h"

#include "nsArray.h"
#include "nsAutoPtr.h"
#include "nsGenericDOMDataNode.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIContentInlines.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCharacterData.h"
#ifdef MOZ_OLD_STYLE
#include "nsRuleNode.h"
#include "nsIStyleRule.h"
#include "mozilla/css/StyleRule.h"
#endif
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
#ifdef MOZ_OLD_STYLE
#include "nsRuleWalker.h"
#endif
#include "nsCSSPseudoClasses.h"
#ifdef MOZ_OLD_STYLE
#include "nsCSSRuleProcessor.h"
#endif
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
#include "mozilla/dom/InspectorFontFace.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

extern const char* const kCSSRawProperties[];

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

bool
InspectorUtils::IsIgnorableWhitespace(nsGenericDOMDataNode& aDataNode)
{
  if (!aDataNode.TextIsOnlyWhitespace()) {
    return false;
  }

  // Okay.  We have only white space.  Let's check the white-space
  // property now and make sure that this isn't preformatted text...
  if (nsIFrame* frame = aDataNode.GetPrimaryFrame()) {
    return !frame->StyleText()->WhiteSpaceIsSignificant();
  }

  // empty inter-tag text node without frame, e.g., in between <table>\n<tr>
  return true;
}

/* static */ nsINode*
InspectorUtils::GetParentForNode(nsINode& aNode,
                                 bool aShowingAnonymousContent)
{
  // First do the special cases -- document nodes and anonymous content
  nsINode* parent = nullptr;

  if (aNode.IsNodeOfType(nsINode::eDOCUMENT)) {
    auto& doc = static_cast<nsIDocument&>(aNode);
    parent = inLayoutUtils::GetContainerFor(doc);
  } else if (aShowingAnonymousContent) {
    if (aNode.IsContent()) {
      parent = aNode.AsContent()->GetFlattenedTreeParent();
    }
  }

  if (!parent) {
    // Ok, just get the normal DOM parent node
    return aNode.GetParentNode();
  }

  return parent;
}

/* static */ already_AddRefed<nsINodeList>
InspectorUtils::GetChildrenForNode(nsINode& aNode,
                                   bool aShowingAnonymousContent)
{
  nsCOMPtr<nsINodeList> kids;

  if (aShowingAnonymousContent) {
    if (aNode.IsContent()) {
      kids = aNode.AsContent()->GetChildren(nsIContent::eAllChildren);
    }
  }

  if (!kids) {
    kids = aNode.ChildNodes();
  }

  return kids.forget();
}

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


  if (styleContext->IsGecko()) {
#ifdef MOZ_OLD_STYLE
    auto gecko = styleContext->AsGecko();
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
#else
    MOZ_CRASH("old style system disabled");
#endif
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

  // If aRule was parsed along with its stylesheet, then it will
  // have an absolute lineNumber that we need to remap to its
  // containing node. But if aRule was added via CSSOM after parsing,
  // then it has a sort-of relative line number already:
  // Gecko gives all rules a 0 lineNumber.
  // Servo gives the first line of a rule a 0 lineNumber, and then
  //   counts up from there.

  // The Servo behavior is arguably more correct, but harder to
  // interpret for purposes of deciding whether a lineNumber is
  // relative or absolute.

  // Since most of the time, inserted rules are single line and
  // therefore have 0 lineNumbers in both Gecko and Servo, we use
  // that to detect that a lineNumber is already relative.

  // There is one ugly edge case that we avoid: if an inserted rule
  // is multi-line, then Servo will give it 0+ lineNumbers. If we
  // do relative number mapping on those line numbers, we could get
  // negative underflow. So we check for underflow and instead report
  // a 0 lineNumber.
  StyleSheet* sheet = aRule.GetStyleSheet();
  if (sheet && lineNumber != 0) {
    nsINode* owningNode = sheet->GetOwnerNode();
    if (owningNode) {
      nsCOMPtr<nsIStyleSheetLinkingElement> link =
        do_QueryInterface(owningNode);
      if (link) {
        // Check for underflow, which is one indication that we're
        // trying to remap an already relative lineNumber.
        uint32_t linkLineIndex0 = link->GetLineNumber() - 1;
        if (linkLineIndex0 > lineNumber ) {
          lineNumber = 0;
        } else {
          lineNumber -= linkLineIndex0;
        }
      }
    }
  }

  return lineNumber;
}

/* static */ bool
InspectorUtils::HasRulesModifiedByCSSOM(GlobalObject& aGlobal, StyleSheet& aSheet)
{
  return aSheet.HasModifiedRules();
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

  // Special cases where nsCSSPropList.h doesn't hold the table.
  if (keywordTable == nullptr) {
    if (aProperty == eCSSProperty_clip_path) {
      keywordTable = nsCSSProps::kClipPathGeometryBoxKTable;
    }
  }

  if (keywordTable) {
    for (size_t i = 0; !keywordTable[i].IsSentinel(); ++i) {
      nsCSSKeyword word = keywordTable[i].mKeyword;

      // These are extra -moz values which are added while rebuilding
      // the properties db. These values are not relevant and are not
      // documented on MDN, so filter these out
      // eCSSKeyword_UNKNOWN is ignored because it indicates an
      // invalid entry; but can still be seen in a table, see bug 1430616.
      if (word != eCSSKeyword__moz_zoom_in && word != eCSSKeyword__moz_zoom_out &&
          word != eCSSKeyword__moz_grab && word != eCSSKeyword__moz_grabbing &&
          word != eCSSKeyword_UNKNOWN) {
          InsertNoDuplicates(aArray,
                  NS_ConvertASCIItoUTF16(nsCSSKeywords::GetStringValue(word)));
      }
    }
  }

  // More special cases.
  if (aProperty == eCSSProperty_clip_path) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("circle"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("ellipse"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("inset"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("polygon"));
  } else if (aProperty == eCSSProperty_clip) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("rect"));
  } else if (aProperty == eCSSProperty_list_style_type) {
    int32_t length;
    const char* const* values = nsCSSProps::GetListStyleTypes(&length);
    for (int32_t i = 0; i < length; ++i) {
      InsertNoDuplicates(aArray, NS_ConvertASCIItoUTF16(values[i]));
    }
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("symbols"));
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
  if (aParserVariant & VARIANT_ATTR) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("attr"));
  }
  if (aParserVariant & VARIANT_COUNTER) {
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("counter"));
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("counters"));
  }
}

/* static */ void
InspectorUtils::GetSubpropertiesForCSSProperty(GlobalObject& aGlobal,
                                               const nsAString& aProperty,
                                               nsTArray<nsString>& aResult,
                                               ErrorResult& aRv)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);

  if (propertyID == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (propertyID == eCSSPropertyExtra_variable) {
    aResult.AppendElement(aProperty);
    return;
  }

  if (!nsCSSProps::IsShorthand(propertyID)) {
    nsString* name = aResult.AppendElement();
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(propertyID), *name);
    return;
  }

  for (const nsCSSPropertyID* props = nsCSSProps::SubpropertyEntryFor(propertyID);
       *props != eCSSProperty_UNKNOWN; ++props) {
    nsString* name = aResult.AppendElement();
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(*props), *name);
  }
}

/* static */ bool
InspectorUtils::CssPropertyIsShorthand(GlobalObject& aGlobalObject,
                                       const nsAString& aProperty,
                                       ErrorResult& aRv)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (propertyID == eCSSPropertyExtra_variable) {
    return false;
  }

  return nsCSSProps::IsShorthand(propertyID);
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

  uint32_t supported = nsCSSProps::ParserVariant(aPropertyID);

  // For the time being, properties that are parsed by functions must
  // have some of their attributes hand-maintained here.
  if (nsCSSProps::PropHasFlags(aPropertyID, CSS_PROPERTY_VALUE_PARSER_FUNCTION) ||
      nsCSSProps::PropertyParseType(aPropertyID) == CSS_PROPERTY_PARSE_FUNCTION) {
    // These must all be special-cased.
    switch (aPropertyID) {
      case eCSSProperty_border_image_slice:
      case eCSSProperty_grid_template:
      case eCSSProperty_grid:
        supported |= VARIANT_PN;
        break;

      case eCSSProperty_border_image_outset:
        supported |= VARIANT_LN;
        break;

      case eCSSProperty_border_image_width:
      case eCSSProperty_stroke_dasharray:
        supported |= VARIANT_LPN;
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
      case eCSSProperty_translate:
      case eCSSProperty_perspective_origin:
      case eCSSProperty__moz_outline_radius_topleft:
      case eCSSProperty__moz_outline_radius_topright:
      case eCSSProperty__moz_outline_radius_bottomleft:
      case eCSSProperty__moz_outline_radius_bottomright:
      case eCSSProperty__moz_window_transform_origin:
        supported |= VARIANT_LP;
        break;

      case eCSSProperty_text_shadow:
      case eCSSProperty_box_shadow:
        supported |= VARIANT_LENGTH | VARIANT_COLOR;
        break;

      case eCSSProperty_border_spacing:
        supported |= VARIANT_LENGTH;
        break;

      case eCSSProperty_cursor:
        supported |= VARIANT_URL;
        break;

      case eCSSProperty_shape_outside:
        supported |= VARIANT_IMAGE;
        break;

      case eCSSProperty_fill:
      case eCSSProperty_stroke:
        supported |= VARIANT_COLOR | VARIANT_URL;
        break;

      case eCSSProperty_image_orientation:
      case eCSSProperty_rotate:
        supported |= VARIANT_ANGLE;
        break;

      case eCSSProperty_filter:
        supported |= VARIANT_URL;
        break;

      case eCSSProperty_grid_column_start:
      case eCSSProperty_grid_column_end:
      case eCSSProperty_grid_row_start:
      case eCSSProperty_grid_row_end:
      case eCSSProperty_font_weight:
      case eCSSProperty_initial_letter:
      case eCSSProperty_scale:
        supported |= VARIANT_NUMBER;
        break;

      default:
        break;
    }
  }

  return (supported & aVariant) != 0;
}

bool
InspectorUtils::CssPropertySupportsType(GlobalObject& aGlobalObject,
                                        const nsAString& aProperty,
                                        uint32_t aType,
                                        ErrorResult& aRv)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (propertyID >= eCSSProperty_COUNT) {
    return false;
  }

  uint32_t variant;
  switch (aType) {
  case InspectorUtilsBinding::TYPE_LENGTH:
    variant = VARIANT_LENGTH;
    break;
  case InspectorUtilsBinding::TYPE_PERCENTAGE:
    variant = VARIANT_PERCENT;
    break;
  case InspectorUtilsBinding::TYPE_COLOR:
    variant = VARIANT_COLOR;
    break;
  case InspectorUtilsBinding::TYPE_URL:
    variant = VARIANT_URL;
    break;
  case InspectorUtilsBinding::TYPE_ANGLE:
    variant = VARIANT_ANGLE;
    break;
  case InspectorUtilsBinding::TYPE_FREQUENCY:
    variant = VARIANT_FREQUENCY;
    break;
  case InspectorUtilsBinding::TYPE_TIME:
    variant = VARIANT_TIME;
    break;
  case InspectorUtilsBinding::TYPE_GRADIENT:
    variant = VARIANT_GRADIENT;
    break;
  case InspectorUtilsBinding::TYPE_TIMING_FUNCTION:
    variant = VARIANT_TIMING_FUNCTION;
    break;
  case InspectorUtilsBinding::TYPE_IMAGE_RECT:
    variant = VARIANT_IMAGE_RECT;
    break;
  case InspectorUtilsBinding::TYPE_NUMBER:
    // Include integers under "number"?
    variant = VARIANT_NUMBER | VARIANT_INTEGER;
    break;
  default:
    // Unknown type
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  return PropertySupportsVariant(propertyID, variant);
}

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
  } else if (propertyID == eCSSProperty_all) {
    // We don't want to pick up everything from gAllSubpropTable, so
    // special-case this here.
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

/* static */ bool
InspectorUtils::IsValidCSSColor(GlobalObject& aGlobalObject,
                                const nsAString& aColorString)
{
#ifdef MOZ_STYLO
  return ServoCSSParser::IsValidCSSColor(aColorString);
#else
  nsCSSParser cssParser;
  nsCSSValue cssValue;
  return cssParser.ParseColorString(aColorString, nullptr, 0, cssValue, true);
#endif
}

void
InspectorUtils::GetBindingURLs(GlobalObject& aGlobalObject,
                               Element& aElement,
                               nsTArray<nsString>& aResult)
{
  nsXBLBinding* binding = aElement.GetXBLBinding();

  while (binding) {
    nsCString spec;
    nsCOMPtr<nsIURI> bindingURI = binding->PrototypeBinding()->BindingURI();
    bindingURI->GetSpec(spec);
    nsString* resultURI = aResult.AppendElement();
    CopyASCIItoUTF16(spec, *resultURI);
    binding = binding->GetBaseBinding();
  }
}

/* static */ bool
InspectorUtils::SetContentState(GlobalObject& aGlobalObject,
                                Element& aElement,
                                uint64_t aState,
                                ErrorResult& aRv)
{
  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  if (!esm) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return esm->SetContentState(&aElement, EventStates(aState));
}

/* static */ bool
InspectorUtils::RemoveContentState(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   uint64_t aState,
                                   bool aClearActiveDocument,
                                   ErrorResult& aRv)
{
  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  if (!esm) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  bool result = esm->SetContentState(nullptr, EventStates(aState));

  if (aClearActiveDocument && EventStates(aState) == NS_EVENT_STATE_ACTIVE) {
    EventStateManager* activeESM = static_cast<EventStateManager*>(
      EventStateManager::GetActiveEventStateManager());
    if (activeESM == esm) {
      EventStateManager::ClearGlobalActiveContent(nullptr);
    }
  }

  return result;
}

/* static */ uint64_t
InspectorUtils::GetContentState(GlobalObject& aGlobalObject,
                                Element& aElement)
{
  // NOTE: if this method is removed,
  // please remove GetInternalValue from EventStates
  return aElement.State().GetInternalValue();
}

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

/* static */ void
InspectorUtils::GetUsedFontFaces(GlobalObject& aGlobalObject,
                                 nsRange& aRange,
                                 uint32_t aMaxRanges,
                                 nsTArray<nsAutoPtr<InspectorFontFace>>& aResult,
                                 ErrorResult& aRv)
{
  nsresult rv = aRange.GetUsedFontFaces(aResult, aMaxRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
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

/* static */ void
InspectorUtils::GetCSSPseudoElementNames(GlobalObject& aGlobalObject,
                                         nsTArray<nsString>& aResult)
{
  const CSSPseudoElementTypeBase pseudoCount =
    static_cast<CSSPseudoElementTypeBase>(CSSPseudoElementType::Count);
  for (CSSPseudoElementTypeBase i = 0; i < pseudoCount; ++i) {
    CSSPseudoElementType type = static_cast<CSSPseudoElementType>(i);
    if (nsCSSPseudoElements::IsEnabled(type, CSSEnabledState::eForAllContent)) {
      nsAtom* atom = nsCSSPseudoElements::GetPseudoAtom(type);
      aResult.AppendElement(nsDependentAtomString(atom));
    }
  }
}

/* static */ void
InspectorUtils::AddPseudoClassLock(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   const nsAString& aPseudoClass,
                                   bool aEnabled)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return;
  }

  aElement.LockStyleStates(state, aEnabled);
}

/* static */ void
InspectorUtils::RemovePseudoClassLock(GlobalObject& aGlobal,
                                      Element& aElement,
                                      const nsAString& aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return;
  }

  aElement.UnlockStyleStates(state);
}

/* static */ bool
InspectorUtils::HasPseudoClassLock(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   const nsAString& aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return false;
  }

  EventStates locks = aElement.LockedStyleStates().mLocks;
  return locks.HasAllStates(state);
}

/* static */ void
InspectorUtils::ClearPseudoClassLocks(GlobalObject& aGlobalObject,
                                      Element& aElement)
{
  aElement.ClearStyleStateLocks();
}

/* static */ void
InspectorUtils::ParseStyleSheet(GlobalObject& aGlobalObject,
                                StyleSheet& aSheet,
                                const nsAString& aInput,
                                ErrorResult& aRv)
{
#ifdef MOZ_OLD_STYLE
  RefPtr<CSSStyleSheet> geckoSheet = do_QueryObject(&aSheet);
  if (geckoSheet) {
    nsresult rv = geckoSheet->ReparseSheet(aInput);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    }
    return;
  }
#endif

  RefPtr<ServoStyleSheet> servoSheet = do_QueryObject(&aSheet);
  if (servoSheet) {
    nsresult rv = servoSheet->ReparseSheet(aInput);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    }
    return;
  }

  aRv.Throw(NS_ERROR_INVALID_POINTER);
}

void
InspectorUtils::ScrollElementIntoView(GlobalObject& aGlobalObject,
                                      Element& aElement)
{
  nsIPresShell* presShell = aElement.OwnerDoc()->GetShell();
  if (!presShell) {
    return;
  }

  presShell->ScrollContentIntoView(&aElement,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

} // namespace dom
} // namespace mozilla
