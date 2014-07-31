/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"

#include "inDOMUtils.h"
#include "inLayoutUtils.h"

#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsString.h"
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
#include "nsIAtom.h"
#include "nsRange.h"
#include "nsContentList.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/dom/Element.h"
#include "nsRuleWalker.h"
#include "nsRuleProcessorData.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsColor.h"
#include "nsStyleSet.h"
#include "nsStyleUtil.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

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

NS_IMETHODIMP
inDOMUtils::GetAllStyleSheets(nsIDOMDocument *aDocument, uint32_t *aLength,
                              nsISupports ***aSheets)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  nsCOMArray<nsISupports> sheets;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
  MOZ_ASSERT(document);

  // Get the agent, then user sheets in the style set.
  nsIPresShell* presShell = document->GetShell();
  if (presShell) {
    nsStyleSet* styleSet = presShell->StyleSet();
    nsStyleSet::sheetType sheetType = nsStyleSet::eAgentSheet;
    for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
      sheets.AppendElement(styleSet->StyleSheetAt(sheetType, i));
    }
    sheetType = nsStyleSet::eUserSheet;
    for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
      sheets.AppendElement(styleSet->StyleSheetAt(sheetType, i));
    }
  }

  // Get the document sheets.
  for (int32_t i = 0; i < document->GetNumberOfStyleSheets(); i++) {
    sheets.AppendElement(document->GetStyleSheetAt(i));
  }

  nsISupports** ret = static_cast<nsISupports**>(NS_Alloc(sheets.Count() *
                                                 sizeof(nsISupports*)));

  for (int32_t i = 0; i < sheets.Count(); i++) {
    NS_ADDREF(ret[i] = sheets[i]);
  }

  *aLength = sheets.Count();
  *aSheets = ret;

  return NS_OK;
}

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
      nsIContent* bparent = content->GetXBLInsertionParent();
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

NS_IMETHODIMP
inDOMUtils::GetCSSStyleRules(nsIDOMElement *aElement,
                             const nsAString& aPseudo,
                             nsISupportsArray **_retval)
{
  NS_ENSURE_ARG_POINTER(aElement);

  *_retval = nullptr;

  nsCOMPtr<nsIAtom> pseudoElt;
  if (!aPseudo.IsEmpty()) {
    pseudoElt = do_GetAtom(aPseudo);
  }

  nsRuleNode* ruleNode = nullptr;
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element);
  nsRefPtr<nsStyleContext> styleContext;
  GetRuleNodeForElement(element, pseudoElt, getter_AddRefs(styleContext), &ruleNode);
  if (!ruleNode) {
    // This can fail for elements that are not in the document or
    // if the document they're in doesn't have a presshell.  Bail out.
    return NS_OK;
  }

  nsCOMPtr<nsISupportsArray> rules;
  NS_NewISupportsArray(getter_AddRefs(rules));
  if (!rules) return NS_ERROR_OUT_OF_MEMORY;

  nsRefPtr<mozilla::css::StyleRule> cssRule;
  for ( ; !ruleNode->IsRoot(); ruleNode = ruleNode->GetParent()) {
    cssRule = do_QueryObject(ruleNode->GetRule());
    if (cssRule) {
      nsCOMPtr<nsIDOMCSSRule> domRule = cssRule->GetDOMRule();
      if (domRule)
        rules->InsertElementAt(domRule, 0);
    }
  }

  *_retval = rules;
  NS_ADDREF(*_retval);

  return NS_OK;
}

static already_AddRefed<StyleRule>
GetRuleFromDOMRule(nsIDOMCSSStyleRule *aRule, ErrorResult& rv)
{
  nsCOMPtr<nsICSSStyleRuleDOMWrapper> rule = do_QueryInterface(aRule);
  if (!rule) {
    rv.Throw(NS_ERROR_INVALID_POINTER);
    return nullptr;
  }

  nsRefPtr<StyleRule> cssrule;
  rv = rule->GetCSSStyleRule(getter_AddRefs(cssrule));
  if (rv.Failed()) {
    return nullptr;
  }

  if (!cssrule) {
    rv.Throw(NS_ERROR_FAILURE);
  }
  return cssrule.forget();
}

NS_IMETHODIMP
inDOMUtils::GetRuleLine(nsIDOMCSSRule* aRule, uint32_t* _retval)
{
  NS_ENSURE_ARG_POINTER(aRule);

  Rule* rule = aRule->GetCSSRule();
  if (!rule) {
    return NS_ERROR_FAILURE;
  }

  *_retval = rule->GetLineNumber();
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetRuleColumn(nsIDOMCSSRule* aRule, uint32_t* _retval)
{
  NS_ENSURE_ARG_POINTER(aRule);

  Rule* rule = aRule->GetCSSRule();
  if (!rule) {
    return NS_ERROR_FAILURE;
  }

  *_retval = rule->GetColumnNumber();
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetSelectorCount(nsIDOMCSSStyleRule* aRule, uint32_t *aCount)
{
  ErrorResult rv;
  nsRefPtr<StyleRule> rule = GetRuleFromDOMRule(aRule, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  uint32_t count = 0;
  for (nsCSSSelectorList* sel = rule->Selector(); sel; sel = sel->mNext) {
    ++count;
  }
  *aCount = count;
  return NS_OK;
}

static nsCSSSelectorList*
GetSelectorAtIndex(nsIDOMCSSStyleRule* aRule, uint32_t aIndex, ErrorResult& rv)
{
  nsRefPtr<StyleRule> rule = GetRuleFromDOMRule(aRule, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  for (nsCSSSelectorList* sel = rule->Selector(); sel;
       sel = sel->mNext, --aIndex) {
    if (aIndex == 0) {
      return sel;
    }
  }

  // Ran out of selectors
  rv.Throw(NS_ERROR_INVALID_ARG);
  return nullptr;
}

NS_IMETHODIMP
inDOMUtils::GetSelectorText(nsIDOMCSSStyleRule* aRule,
                            uint32_t aSelectorIndex,
                            nsAString& aText)
{
  ErrorResult rv;
  nsCSSSelectorList* sel = GetSelectorAtIndex(aRule, aSelectorIndex, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  nsRefPtr<StyleRule> rule = GetRuleFromDOMRule(aRule, rv);
  MOZ_ASSERT(!rv.Failed(), "How could we get a selector but not a rule?");

  sel->mSelectors->ToString(aText, rule->GetStyleSheet(), false);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetSpecificity(nsIDOMCSSStyleRule* aRule,
                            uint32_t aSelectorIndex,
                            uint64_t* aSpecificity)
{
  ErrorResult rv;
  nsCSSSelectorList* sel = GetSelectorAtIndex(aRule, aSelectorIndex, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  *aSpecificity = sel->mWeight;
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::SelectorMatchesElement(nsIDOMElement* aElement,
                                   nsIDOMCSSStyleRule* aRule,
                                   uint32_t aSelectorIndex,
                                   const nsAString& aPseudo,
                                   bool* aMatches)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  ErrorResult rv;
  nsCSSSelectorList* tail = GetSelectorAtIndex(aRule, aSelectorIndex, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  // We want just the one list item, not the whole list tail
  nsAutoPtr<nsCSSSelectorList> sel(tail->Clone(false));

  // Do not attempt to match if a pseudo element is requested and this is not
  // a pseudo element selector, or vice versa.
  if (aPseudo.IsEmpty() == sel->mSelectors->IsPseudoElement()) {
    *aMatches = false;
    return NS_OK;
  }

  if (!aPseudo.IsEmpty()) {
    // We need to make sure that the requested pseudo element type
    // matches the selector pseudo element type before proceeding.
    nsCOMPtr<nsIAtom> pseudoElt = do_GetAtom(aPseudo);
    if (sel->mSelectors->PseudoType() !=
        nsCSSPseudoElements::GetPseudoType(pseudoElt)) {
      *aMatches = false;
      return NS_OK;
    }

    // We have a matching pseudo element, now remove it so we can compare
    // directly against |element| when proceeding into SelectorListMatches.
    // It's OK to do this - we just cloned sel and nothing else is using it.
    sel->RemoveRightmostSelector();
  }

  element->OwnerDoc()->FlushPendingLinkUpdates();
  // XXXbz what exactly should we do with visited state here?
  TreeMatchContext matchingContext(false,
                                   nsRuleWalker::eRelevantLinkUnvisited,
                                   element->OwnerDoc(),
                                   TreeMatchContext::eNeverMatchVisited);
  *aMatches = nsCSSRuleProcessor::SelectorListMatches(element, matchingContext,
                                                      sel);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::IsInheritedProperty(const nsAString &aPropertyName, bool *_retval)
{
  nsCSSProperty prop =
    nsCSSProps::LookupProperty(aPropertyName, nsCSSProps::eIgnoreEnabledState);
  if (prop == eCSSProperty_UNKNOWN) {
    *_retval = false;
    return NS_OK;
  }

  if (prop == eCSSPropertyExtra_variable) {
    *_retval = true;
    return NS_OK;
  }

  if (nsCSSProps::IsShorthand(prop)) {
    prop = nsCSSProps::SubpropertyEntryFor(prop)[0];
  }

  nsStyleStructID sid = nsCSSProps::kSIDTable[prop];
  *_retval = !nsCachedStyleData::IsReset(sid);
  return NS_OK;
}

extern const char* const kCSSRawProperties[];

NS_IMETHODIMP
inDOMUtils::GetCSSPropertyNames(uint32_t aFlags, uint32_t* aCount,
                                char16_t*** aProps)
{
  // maxCount is the largest number of properties we could have; our actual
  // number might be smaller because properties might be disabled.
  uint32_t maxCount;
  if (aFlags & EXCLUDE_SHORTHANDS) {
    maxCount = eCSSProperty_COUNT_no_shorthands;
  } else {
    maxCount = eCSSProperty_COUNT;
  }

  if (aFlags & INCLUDE_ALIASES) {
    maxCount += (eCSSProperty_COUNT_with_aliases - eCSSProperty_COUNT);
  }

  char16_t** props =
    static_cast<char16_t**>(nsMemory::Alloc(maxCount * sizeof(char16_t*)));

#define DO_PROP(_prop)                                                  \
  PR_BEGIN_MACRO                                                        \
    nsCSSProperty cssProp = nsCSSProperty(_prop);                       \
    if (nsCSSProps::IsEnabled(cssProp)) {                               \
      props[propCount] =                                                \
        ToNewUnicode(nsDependentCString(kCSSRawProperties[_prop]));     \
      ++propCount;                                                      \
    }                                                                   \
  PR_END_MACRO

  // prop is the property id we're considering; propCount is how many properties
  // we've put into props so far.
  uint32_t prop = 0, propCount = 0;
  for ( ; prop < eCSSProperty_COUNT_no_shorthands; ++prop) {
    if (nsCSSProps::PropertyParseType(nsCSSProperty(prop)) !=
        CSS_PROPERTY_PARSE_INACCESSIBLE) {
      DO_PROP(prop);
    }
  }

  if (!(aFlags & EXCLUDE_SHORTHANDS)) {
    for ( ; prop < eCSSProperty_COUNT; ++prop) {
      // Some shorthands are also aliases
      if ((aFlags & INCLUDE_ALIASES) ||
          !nsCSSProps::PropHasFlags(nsCSSProperty(prop),
                                    CSS_PROPERTY_IS_ALIAS)) {
        DO_PROP(prop);
      }
    }
  }

  if (aFlags & INCLUDE_ALIASES) {
    for (prop = eCSSProperty_COUNT; prop < eCSSProperty_COUNT_with_aliases; ++prop) {
      DO_PROP(prop);
    }
  }

#undef DO_PROP

  *aCount = propCount;
  *aProps = props;

  return NS_OK;
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

static void GetKeywordsForProperty(const nsCSSProperty aProperty,
                                   nsTArray<nsString>& aArray)
{
  if (nsCSSProps::IsShorthand(aProperty)) {
    // Shorthand props have no keywords.
    return;
  }
  const nsCSSProps::KTableValue *keywordTable =
    nsCSSProps::kKeywordTableTable[aProperty];
  if (keywordTable && keywordTable != nsCSSProps::kBoxPropSourceKTable) {
    size_t i = 0;
    while (nsCSSKeyword(keywordTable[i]) != eCSSKeyword_UNKNOWN) {
      nsCSSKeyword word = nsCSSKeyword(keywordTable[i]);
      InsertNoDuplicates(aArray,
                         NS_ConvertASCIItoUTF16(nsCSSKeywords::GetStringValue(word)));
      // Increment counter by 2, because in this table every second
      // element is a nsCSSKeyword.
      i += 2;
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
    for (size_t i = 0; i < size; i++) {
      CopyASCIItoUTF16(allColorNames[i], *aArray.AppendElement());
    }
  }
  return;
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
    InsertNoDuplicates(aArray, NS_LITERAL_STRING("-moz-calc"));
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
  nsCSSProperty propertyID =
    nsCSSProps::LookupProperty(aProperty, nsCSSProps::eEnabledForAllContent);

  if (propertyID == eCSSProperty_UNKNOWN ||
      propertyID == eCSSPropertyExtra_variable) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsString> array;
  if (!nsCSSProps::IsShorthand(propertyID)) {
    *aValues = static_cast<char16_t**>(nsMemory::Alloc(sizeof(char16_t*)));
    (*aValues)[0] = ToNewUnicode(nsCSSProps::GetStringValue(propertyID));
    *aLength = 1;
    return NS_OK;
  }

  // Count up how many subproperties we have.
  size_t subpropCount = 0;
  for (const nsCSSProperty *props = nsCSSProps::SubpropertyEntryFor(propertyID);
       *props != eCSSProperty_UNKNOWN; ++props) {
    ++subpropCount;
  }

  *aValues =
    static_cast<char16_t**>(nsMemory::Alloc(subpropCount * sizeof(char16_t*)));
  *aLength = subpropCount;
  for (const nsCSSProperty *props = nsCSSProps::SubpropertyEntryFor(propertyID),
                           *props_start = props;
       *props != eCSSProperty_UNKNOWN; ++props) {
    (*aValues)[props-props_start] = ToNewUnicode(nsCSSProps::GetStringValue(*props));
  }
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::CssPropertyIsShorthand(const nsAString& aProperty, bool *_retval)
{
  nsCSSProperty propertyID =
    nsCSSProps::LookupProperty(aProperty, nsCSSProps::eEnabledForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  *_retval = nsCSSProps::IsShorthand(propertyID);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::CssPropertySupportsType(const nsAString& aProperty, uint32_t aType,
                                    bool *_retval)
{
  nsCSSProperty propertyID =
    nsCSSProps::LookupProperty(aProperty, nsCSSProps::eEnabledForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
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

  if (!nsCSSProps::IsShorthand(propertyID)) {
    *_retval = nsCSSProps::ParserVariant(propertyID) & variant;
    return NS_OK;
  }

  for (const nsCSSProperty* props = nsCSSProps::SubpropertyEntryFor(propertyID);
       *props != eCSSProperty_UNKNOWN; ++props) {
    if (nsCSSProps::ParserVariant(*props) & variant) {
      *_retval = true;
      return NS_OK;
    }
  }

  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetCSSValuesForProperty(const nsAString& aProperty,
                                    uint32_t* aLength,
                                    char16_t*** aValues)
{
  nsCSSProperty propertyID = nsCSSProps::LookupProperty(aProperty,
                                                        nsCSSProps::eEnabledForAllContent);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsString> array;
  // We start collecting the values, BUT colors need to go in first, because array
  // needs to stay sorted, and the colors are sorted, so we just append them.
  if (propertyID == eCSSPropertyExtra_variable) {
    // No other values we can report.
  } else if (!nsCSSProps::IsShorthand(propertyID)) {
    // Property is longhand.
    uint32_t propertyParserVariant = nsCSSProps::ParserVariant(propertyID);
    // Get colors first.
    GetColorsForProperty(propertyParserVariant, array);
    if (propertyParserVariant & VARIANT_KEYWORD) {
      GetKeywordsForProperty(propertyID, array);
    }
    GetOtherValuesForProperty(propertyParserVariant, array);
  } else {
    // Property is shorthand.
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subproperty, propertyID) {
      // Get colors (once) first.
      uint32_t propertyParserVariant = nsCSSProps::ParserVariant(*subproperty);
      if (propertyParserVariant & VARIANT_COLOR) {
        GetColorsForProperty(propertyParserVariant, array);
        break;
      }
    }
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subproperty, propertyID) {
      uint32_t propertyParserVariant = nsCSSProps::ParserVariant(*subproperty);
      if (propertyParserVariant & VARIANT_KEYWORD) {
        GetKeywordsForProperty(*subproperty, array);
      }
      GetOtherValuesForProperty(propertyParserVariant, array);
    }
  }
  // All CSS properties take initial, inherit and unset.
  InsertNoDuplicates(array, NS_LITERAL_STRING("initial"));
  InsertNoDuplicates(array, NS_LITERAL_STRING("inherit"));
  InsertNoDuplicates(array, NS_LITERAL_STRING("unset"));

  *aLength = array.Length();
  char16_t** ret =
    static_cast<char16_t**>(NS_Alloc(*aLength * sizeof(char16_t*)));
  for (uint32_t i = 0; i < *aLength; ++i) {
    ret[i] = ToNewUnicode(array[i]);
  }
  *aValues = ret;
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::ColorNameToRGB(const nsAString& aColorName, JSContext* aCx,
                           JS::MutableHandle<JS::Value> aValue)
{
  nscolor color;
  if (!NS_ColorNameToRGB(aColorName, &color)) {
    return NS_ERROR_INVALID_ARG;
  }

  InspectorRGBTriple triple;
  triple.mR = NS_GET_R(color);
  triple.mG = NS_GET_G(color);
  triple.mB = NS_GET_B(color);

  if (!ToJSValue(aCx, triple, aValue)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::RgbToColorName(uint8_t aR, uint8_t aG, uint8_t aB,
                           nsAString& aColorName)
{
  const char* color = NS_RGBToColorName(NS_RGB(aR, aG, aB));
  if (!color) {
    aColorName.Truncate();
    return NS_ERROR_INVALID_ARG;
  }

  aColorName.AssignASCII(color);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::ColorToRGBA(const nsAString& aColorString, JSContext* aCx,
                        JS::MutableHandle<JS::Value> aValue)
{
  nscolor color = 0;
  nsCSSParser cssParser;
  nsCSSValue cssValue;

  bool isColor = cssParser.ParseColorString(aColorString, nullptr, 0,
                                            cssValue, true);

  if (!isColor) {
    aValue.setNull();
    return NS_OK;
  }

  nsRuleNode::ComputeColor(cssValue, nullptr, nullptr, color);

  InspectorRGBATuple tuple;
  tuple.mR = NS_GET_R(color);
  tuple.mG = NS_GET_G(color);
  tuple.mB = NS_GET_B(color);
  tuple.mA = nsStyleUtil::ColorComponentToFloat(NS_GET_A(color));

  if (!ToJSValue(aCx, tuple, aValue)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::IsValidCSSColor(const nsAString& aColorString, bool *_retval)
{
  nsCSSParser cssParser;
  nsCSSValue cssValue;
  *_retval = cssParser.ParseColorString(aColorString, nullptr, 0, cssValue, true);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::CssPropertyIsValid(const nsAString& aPropertyName,
                               const nsAString& aPropertyValue,
                               bool *_retval)
{
  nsCSSProperty propertyID =
    nsCSSProps::LookupProperty(aPropertyName, nsCSSProps::eIgnoreEnabledState);

  if (propertyID == eCSSProperty_UNKNOWN) {
    *_retval = false;
    return NS_OK;
  }

  // Get a parser, parse the property.
  nsCSSParser parser;
  *_retval = parser.IsValueValidForProperty(propertyID, aPropertyValue);

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
    urls->AppendElement(binding->PrototypeBinding()->BindingURI(), false);
    binding = binding->GetBaseBinding();
  }

  NS_ADDREF(*_retval = urls);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::SetContentState(nsIDOMElement* aElement,
                            EventStates::InternalType aState)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsRefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  if (esm) {
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(aElement);

    // XXX Invalid cast of bool to nsresult (bug 778108)
    return (nsresult)esm->SetContentState(content, EventStates(aState));
  }

  return NS_ERROR_FAILURE;
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

/* static */ nsresult
inDOMUtils::GetRuleNodeForElement(dom::Element* aElement,
                                  nsIAtom* aPseudo,
                                  nsStyleContext** aStyleContext,
                                  nsRuleNode** aRuleNode)
{
  MOZ_ASSERT(aElement);

  *aRuleNode = nullptr;
  *aStyleContext = nullptr;

  nsIDocument* doc = aElement->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  nsIPresShell *presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_UNEXPECTED);

  nsPresContext *presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_UNEXPECTED);

  presContext->EnsureSafeToHandOutCSSRules();

  nsRefPtr<nsStyleContext> sContext =
    nsComputedDOMStyle::GetStyleContextForElement(aElement, aPseudo, presShell);
  if (sContext) {
    *aRuleNode = sContext->RuleNode();
    sContext.forget(aStyleContext);
  }
  return NS_OK;
}

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
#define CSS_PSEUDO_CLASS(_name, _value, _pref)	\
    EventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _pref, _states)	\
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
                nsCSSPseudoClasses::ePseudoClass_NotPseudoClass + 1,
                "Length of PseudoClassStates array is incorrect");

  nsCOMPtr<nsIAtom> atom = do_GetAtom(aStatePseudo);

  // Ignore :moz-any-link so we don't give the element simultaneous
  // visited and unvisited style state
  if (nsCSSPseudoClasses::GetPseudoType(atom) ==
      nsCSSPseudoClasses::ePseudoClass_mozAnyLink) {
    return EventStates();
  }
  // Our array above is long enough that indexing into it with
  // NotPseudoClass is ok.
  return sPseudoClassStates[nsCSSPseudoClasses::GetPseudoType(atom)];
}

NS_IMETHODIMP
inDOMUtils::AddPseudoClassLock(nsIDOMElement *aElement,
                               const nsAString &aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  element->LockStyleStates(state);

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

  EventStates locks = element->LockedStyleStates();

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
  nsRefPtr<CSSStyleSheet> sheet = do_QueryObject(aSheet);
  NS_ENSURE_ARG_POINTER(sheet);

  return sheet->ParseSheet(aInput);
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
