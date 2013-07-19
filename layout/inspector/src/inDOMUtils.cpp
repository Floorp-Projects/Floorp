/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inDOMUtils.h"
#include "inLayoutUtils.h"

#include "nsIServiceManager.h"
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
#include "nsEventStateManager.h"
#include "nsIAtom.h"
#include "nsRange.h"
#include "nsContentList.h"
#include "mozilla/dom/Element.h"
#include "nsCSSStyleSheet.h"
#include "nsRuleWalker.h"
#include "nsRuleProcessorData.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/InspectorUtilsBinding.h"
#include "nsCSSProps.h"
#include "nsColor.h"

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

NS_IMPL_ISUPPORTS1(inDOMUtils, inIDOMUtils)

///////////////////////////////////////////////////////////////////////////////
// inIDOMUtils

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
  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(aNode));
  nsCOMPtr<nsIDOMNode> parent;

  if (doc) {
    parent = inLayoutUtils::GetContainerFor(doc);
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
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  NS_ENSURE_STATE(content);
  nsRefPtr<nsStyleContext> styleContext;
  GetRuleNodeForContent(content, pseudoElt, getter_AddRefs(styleContext), &ruleNode);
  if (!ruleNode) {
    // This can fail for content nodes that are not in the document or
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
inDOMUtils::GetRuleLine(nsIDOMCSSStyleRule *aRule, uint32_t *_retval)
{
  ErrorResult rv;
  nsRefPtr<StyleRule> rule = GetRuleFromDOMRule(aRule, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  *_retval = rule->GetLineNumber();
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetRuleColumn(nsIDOMCSSStyleRule *aRule, uint32_t *_retval)
{
  ErrorResult rv;
  nsRefPtr<StyleRule> rule = GetRuleFromDOMRule(aRule, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
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
  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName,
                                                  nsCSSProps::eAny);
  if (prop == eCSSProperty_UNKNOWN) {
    *_retval = false;
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
                                PRUnichar*** aProps)
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

  PRUnichar** props =
    static_cast<PRUnichar**>(nsMemory::Alloc(maxCount * sizeof(PRUnichar*)));

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

static void GetKeywordsForProperty(const nsCSSProperty aProperty,
                                   nsTArray<nsString>& aArray)
{
  if (nsCSSProps::IsShorthand(aProperty)) {
    // Shorthand props have no keywords.
    return;
  }
  const int32_t *keywordTable = nsCSSProps::kKeywordTableTable[aProperty];
  if (keywordTable) {
    size_t i = 0;
    while (nsCSSKeyword(keywordTable[i]) != eCSSKeyword_UNKNOWN) {
      nsCSSKeyword word = nsCSSKeyword(keywordTable[i]);
      CopyASCIItoUTF16(nsCSSKeywords::GetStringValue(word),
          *aArray.AppendElement());
      // Increment counter by 2, because in this table every second
      // element is a nsCSSKeyword.
      i += 2;
    }
  }
}

static void GetColorsForProperty(const nsCSSProperty propertyID,
                                 nsTArray<nsString>& aArray)
{
  uint32_t propertyParserVariant = nsCSSProps::ParserVariant(propertyID);
  if (propertyParserVariant & VARIANT_COLOR) {
    size_t size;
    const char * const *allColorNames = NS_AllColorNames(&size);
    for (size_t i = 0; i < size; i++) {
      CopyASCIItoUTF16(allColorNames[i], *aArray.AppendElement());
    }
  }
  return;
}

NS_IMETHODIMP
inDOMUtils::GetCSSValuesForProperty(const nsAString& aProperty,
                                    uint32_t* aLength,
                                    PRUnichar*** aValues)
{
  nsCSSProperty propertyID = nsCSSProps::LookupProperty(aProperty,
                                                        nsCSSProps::eEnabled);
  if (propertyID == eCSSProperty_UNKNOWN) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsString> array;
  // All CSS properties take initial and inherit.
  array.AppendElement(NS_LITERAL_STRING("-moz-initial"));
  array.AppendElement(NS_LITERAL_STRING("inherit"));
  if (!nsCSSProps::IsShorthand(propertyID)) {
    // Property is longhand.
    GetKeywordsForProperty(propertyID, array);
    GetColorsForProperty(propertyID, array);
  } else {
    // Property is shorthand.
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subproperty, propertyID) {
      GetKeywordsForProperty(*subproperty, array);
      GetColorsForProperty(*subproperty, array);
    }
  }

  *aLength = array.Length();
  PRUnichar** ret =
    static_cast<PRUnichar**>(NS_Alloc(*aLength * sizeof(PRUnichar*)));
  for (uint32_t i = 0; i < *aLength; ++i) {
    ret[i] = ToNewUnicode(array[i]);
  }
  *aValues = ret;
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::ColorNameToRGB(const nsAString& aColorName, JSContext* aCx,
                           JS::Value* aValue)
{
  nscolor color;
  if (!NS_ColorNameToRGB(aColorName, &color)) {
    return NS_ERROR_INVALID_ARG;
  }

  InspectorRGBTriple triple;
  triple.mR = NS_GET_R(color);
  triple.mG = NS_GET_G(color);
  triple.mB = NS_GET_B(color);

  if (!triple.ToObject(aCx, JS::NullPtr(),
                       JS::MutableHandle<JS::Value>::fromMarkedLocation(aValue))) {
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
inDOMUtils::SetContentState(nsIDOMElement *aElement, nsEventStates::InternalType aState)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsRefPtr<nsEventStateManager> esm = inLayoutUtils::GetEventStateManagerFor(aElement);
  if (esm) {
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(aElement);

    // XXX Invalid cast of bool to nsresult (bug 778108)
    return (nsresult)esm->SetContentState(content, nsEventStates(aState));
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inDOMUtils::GetContentState(nsIDOMElement *aElement, nsEventStates::InternalType* aState)
{
  *aState = 0;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(content);

  // NOTE: if this method is removed,
  // please remove GetInternalValue from nsEventStates
  *aState = content->AsElement()->State().GetInternalValue();
  return NS_OK;
}

/* static */ nsresult
inDOMUtils::GetRuleNodeForContent(nsIContent* aContent,
                                  nsIAtom* aPseudo,
                                  nsStyleContext** aStyleContext,
                                  nsRuleNode** aRuleNode)
{
  *aRuleNode = nullptr;
  *aStyleContext = nullptr;

  if (!aContent->IsElement()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIDocument* doc = aContent->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  nsIPresShell *presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_UNEXPECTED);

  nsPresContext *presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_UNEXPECTED);

  bool safe = presContext->EnsureSafeToHandOutCSSRules();
  NS_ENSURE_TRUE(safe, NS_ERROR_OUT_OF_MEMORY);

  nsRefPtr<nsStyleContext> sContext =
    nsComputedDOMStyle::GetStyleContextForElement(aContent->AsElement(), aPseudo, presShell);
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

static nsEventStates
GetStatesForPseudoClass(const nsAString& aStatePseudo)
{
  // An array of the states that are relevant for various pseudoclasses.
  // XXXbz this duplicates code in nsCSSRuleProcessor
  static const nsEventStates sPseudoClassStates[] = {
#define CSS_PSEUDO_CLASS(_name, _value, _pref)	\
    nsEventStates(),
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _pref, _states)	\
    _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS

    // Add more entries for our fake values to make sure we can't
    // index out of bounds into this array no matter what.
    nsEventStates(),
    nsEventStates()
  };
  MOZ_STATIC_ASSERT(NS_ARRAY_LENGTH(sPseudoClassStates) ==
                    nsCSSPseudoClasses::ePseudoClass_NotPseudoClass + 1,
                    "Length of PseudoClassStates array is incorrect");

  nsCOMPtr<nsIAtom> atom = do_GetAtom(aStatePseudo);

  // Ignore :moz-any-link so we don't give the element simultaneous
  // visited and unvisited style state
  if (nsCSSPseudoClasses::GetPseudoType(atom) ==
      nsCSSPseudoClasses::ePseudoClass_mozAnyLink) {
    return nsEventStates();
  }
  // Our array above is long enough that indexing into it with
  // NotPseudoClass is ok.
  return sPseudoClassStates[nsCSSPseudoClasses::GetPseudoType(atom)];
}

NS_IMETHODIMP
inDOMUtils::AddPseudoClassLock(nsIDOMElement *aElement,
                               const nsAString &aPseudoClass)
{
  nsEventStates state = GetStatesForPseudoClass(aPseudoClass);
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
  nsEventStates state = GetStatesForPseudoClass(aPseudoClass);
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
  nsEventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    *_retval = false;
    return NS_OK;
  }

  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  nsEventStates locks = element->LockedStyleStates();

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
  nsRefPtr<nsCSSStyleSheet> sheet = do_QueryObject(aSheet);
  NS_ENSURE_ARG_POINTER(sheet);

  return sheet->ParseSheet(aInput);
}
