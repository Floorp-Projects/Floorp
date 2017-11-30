/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#include "nsCSSParser.h"

#include "mozilla/Attributes.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/Unused.h"

#include <algorithm> // for std::stable_sort
#include <limits> // for std::numeric_limits

#include "nsAlgorithm.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsCSSScanner.h"
#include "mozilla/css/ErrorReporter.h"
#include "mozilla/css/Loader.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/ImportRule.h"
#include "mozilla/css/URLMatchingFunction.h"
#include "nsCSSRules.h"
#include "nsCSSCounterStyleRule.h"
#include "nsCSSFontFaceRule.h"
#include "mozilla/css/NameSpaceRule.h"
#include "nsTArray.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Declaration.h"
#include "nsStyleConsts.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAtom.h"
#include "nsColor.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsNameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsError.h"
#include "nsMediaList.h"
#include "nsStyleUtil.h"
#include "nsIPrincipal.h"
#include "mozilla/Sprintf.h"
#include "nsContentUtils.h"
#include "nsAutoPtr.h"
#include "CSSCalc.h"
#include "nsMediaFeatures.h"
#include "nsLayoutUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/StylePrefs.h"
#include "nsRuleData.h"
#include "mozilla/CSSVariableValues.h"
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"
#include "mozilla/dom/URL.h"
#include "gfxFontFamilyList.h"

using namespace mozilla;
using namespace mozilla::css;

typedef nsCSSProps::KTableEntry KTableEntry;

// Maximum number of repetitions for the repeat() function
// in the grid-template-rows and grid-template-columns properties,
// to limit high memory usage from small stylesheets.
// Must be a positive integer. Should be large-ish.
#define GRID_TEMPLATE_MAX_REPETITIONS 10000

// End-of-array marker for mask arguments to ParseBitmaskValues
#define MASK_END_VALUE  (-1)

enum class CSSParseResult : int32_t {
  // Parsed something successfully:
  Ok,
  // Did not find what we were looking for, but did not consume any token:
  NotFound,
  // Unexpected token or token value, too late for UngetToken() to be enough:
  Error
};

enum class GridTrackSizeFlags {
  eDefaultTrackSize = 0x0,
  eFixedTrackSize   = 0x1, // parse a <fixed-size> instead of <track-size>
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(GridTrackSizeFlags)

enum class GridTrackListFlags {
  eDefaultTrackList  = 0x0, // parse a <track-list>
  eExplicitTrackList = 0x1, // parse an <explicit-track-list> instead
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(GridTrackListFlags)

namespace {

// Rule processing function
typedef void (* RuleAppendFunc) (css::Rule* aRule, void* aData);
static void AssignRuleToPointer(css::Rule* aRule, void* aPointer);
static void AppendRuleToSheet(css::Rule* aRule, void* aParser);

struct CSSParserInputState {
  nsCSSScannerPosition mPosition;
  nsCSSToken mToken;
  bool mHavePushBack;
};

static_assert(css::eAuthorSheetFeatures == 0 &&
              css::eUserSheetFeatures == 1 &&
              css::eAgentSheetFeatures == 2,
              "sheet parsing mode constants won't fit "
              "in CSSParserImpl::mParsingMode");

// Your basic top-down recursive descent style parser
// The exposed methods and members of this class are precisely those
// needed by nsCSSParser, far below.
class CSSParserImpl {
public:
  CSSParserImpl();
  ~CSSParserImpl();

  nsresult SetStyleSheet(CSSStyleSheet* aSheet);

  nsIDocument* GetDocument();

  nsresult SetQuirkMode(bool aQuirkMode);

  nsresult SetChildLoader(mozilla::css::Loader* aChildLoader);

  // Clears everything set by the above Set*() functions.
  void Reset();

  nsresult ParseSheet(const nsAString& aInput,
                      nsIURI*          aSheetURI,
                      nsIURI*          aBaseURI,
                      nsIPrincipal*    aSheetPrincipal,
                      uint32_t         aLineNumber,
                      css::LoaderReusableStyleSheets* aReusableSheets);

  already_AddRefed<css::Declaration>
           ParseStyleAttribute(const nsAString&  aAttributeValue,
                               nsIURI*           aDocURL,
                               nsIURI*           aBaseURL,
                               nsIPrincipal*     aNodePrincipal);

  nsresult ParseDeclarations(const nsAString&  aBuffer,
                             nsIURI*           aSheetURL,
                             nsIURI*           aBaseURL,
                             nsIPrincipal*     aSheetPrincipal,
                             css::Declaration* aDeclaration,
                             bool*           aChanged);

  nsresult ParseRule(const nsAString&        aRule,
                     nsIURI*                 aSheetURL,
                     nsIURI*                 aBaseURL,
                     nsIPrincipal*           aSheetPrincipal,
                     css::Rule**             aResult);

  void ParseProperty(const nsCSSPropertyID aPropID,
                     const nsAString& aPropValue,
                     nsIURI* aSheetURL,
                     nsIURI* aBaseURL,
                     nsIPrincipal* aSheetPrincipal,
                     css::Declaration* aDeclaration,
                     bool* aChanged,
                     bool aIsImportant,
                     bool aIsSVGMode);
  void ParseLonghandProperty(const nsCSSPropertyID aPropID,
                             const nsAString& aPropValue,
                             nsIURI* aSheetURL,
                             nsIURI* aBaseURL,
                             nsIPrincipal* aSheetPrincipal,
                             nsCSSValue& aValue);

  bool ParseTransformProperty(const nsAString& aPropValue,
                              bool aDisallowRelativeValues,
                              nsCSSValue& aResult);

  void ParseMediaList(const nsAString& aBuffer,
                      nsIURI* aURL, // for error reporting
                      uint32_t aLineNumber, // for error reporting
                      nsMediaList* aMediaList,
                      mozilla::dom::CallerType aCallerType);

  bool ParseSourceSizeList(const nsAString& aBuffer,
                           nsIURI* aURI, // for error reporting
                           uint32_t aLineNumber, // for error reporting
                           InfallibleTArray< nsAutoPtr<nsMediaQuery> >& aQueries,
                           InfallibleTArray<nsCSSValue>& aValues);

  void ParseVariable(const nsAString& aVariableName,
                     const nsAString& aPropValue,
                     nsIURI* aSheetURL,
                     nsIURI* aBaseURL,
                     nsIPrincipal* aSheetPrincipal,
                     css::Declaration* aDeclaration,
                     bool* aChanged,
                     bool aIsImportant);

  bool ParseFontFamilyListString(const nsAString& aBuffer,
                                 nsIURI* aURL, // for error reporting
                                 uint32_t aLineNumber, // for error reporting
                                 nsCSSValue& aValue);

  bool ParseColorString(const nsAString& aBuffer,
                        nsIURI* aURL, // for error reporting
                        uint32_t aLineNumber, // for error reporting
                        nsCSSValue& aValue,
                        bool aSuppressErrors /* false */);

  bool ParseMarginString(const nsAString& aBuffer,
                         nsIURI* aURL, // for error reporting
                         uint32_t aLineNumber, // for error reporting
                         nsCSSValue& aValue,
                         bool aSuppressErrors /* false */);

  nsresult ParseSelectorString(const nsAString& aSelectorString,
                               nsIURI* aURL, // for error reporting
                               uint32_t aLineNumber, // for error reporting
                               nsCSSSelectorList **aSelectorList);

  already_AddRefed<nsCSSKeyframeRule>
  ParseKeyframeRule(const nsAString& aBuffer,
                    nsIURI*            aURL,
                    uint32_t           aLineNumber);

  bool ParseKeyframeSelectorString(const nsAString& aSelectorString,
                                   nsIURI* aURL, // for error reporting
                                   uint32_t aLineNumber, // for error reporting
                                   InfallibleTArray<float>& aSelectorList);

  bool EvaluateSupportsDeclaration(const nsAString& aProperty,
                                   const nsAString& aValue,
                                   nsIURI* aDocURL,
                                   nsIURI* aBaseURL,
                                   nsIPrincipal* aDocPrincipal);

  bool EvaluateSupportsCondition(const nsAString& aCondition,
                                 nsIURI* aDocURL,
                                 nsIURI* aBaseURL,
                                 nsIPrincipal* aDocPrincipal,
                                 SupportsParsingSettings aSettings
                                  = SupportsParsingSettings::Normal);

  already_AddRefed<nsAtom> ParseCounterStyleName(const nsAString& aBuffer,
                                                  nsIURI* aURL);

  bool ParseCounterDescriptor(nsCSSCounterDesc aDescID,
                              const nsAString& aBuffer,
                              nsIURI* aSheetURL,
                              nsIURI* aBaseURL,
                              nsIPrincipal* aSheetPrincipal,
                              nsCSSValue& aValue);

  bool ParseFontFaceDescriptor(nsCSSFontDesc aDescID,
                               const nsAString& aBuffer,
                               nsIURI* aSheetURL,
                               nsIURI* aBaseURL,
                               nsIPrincipal* aSheetPrincipal,
                               nsCSSValue& aValue);

  bool IsValueValidForProperty(const nsCSSPropertyID aPropID,
                               const nsAString& aPropValue);

  typedef nsCSSParser::VariableEnumFunc VariableEnumFunc;

  /**
   * Parses a CSS token stream value and invokes a callback function for each
   * variable reference that is encountered.
   *
   * @param aPropertyValue The CSS token stream value.
   * @param aFunc The callback function to invoke; its parameters are the
   *   variable name found and the aData argument passed in to this function.
   * @param aData User data to pass in to the callback.
   * @return Whether aPropertyValue could be parsed as a valid CSS token stream
   *   value (e.g., without syntactic errors in variable references).
   */
  bool EnumerateVariableReferences(const nsAString& aPropertyValue,
                                   VariableEnumFunc aFunc,
                                   void* aData);

  /**
   * Parses aPropertyValue as a CSS token stream value and resolves any
   * variable references using the variables in aVariables.
   *
   * @param aPropertyValue The CSS token stream value.
   * @param aVariables The set of variable values to use when resolving variable
   *   references.
   * @param aResult Out parameter that gets the resolved value.
   * @param aFirstToken Out parameter that gets the type of the first token in
   *   aResult.
   * @param aLastToken Out parameter that gets the type of the last token in
   *   aResult.
   * @return Whether aResult could be parsed successfully and variable reference
   *   substitution succeeded.
   */
  bool ResolveVariableValue(const nsAString& aPropertyValue,
                            const CSSVariableValues* aVariables,
                            nsString& aResult,
                            nsCSSTokenSerializationType& aFirstToken,
                            nsCSSTokenSerializationType& aLastToken);

  /**
   * Parses a string as a CSS token stream value for particular property,
   * resolving any variable references.  The parsed property value is stored
   * in the specified nsRuleData object.  If aShorthandPropertyID has a value
   * other than eCSSProperty_UNKNOWN, this is the property that will be parsed;
   * otherwise, aPropertyID will be parsed.  Either way, only aPropertyID,
   * a longhand property, will be copied over to the rule data.
   *
   * If the property cannot be parsed, it will be treated as if 'initial' or
   * 'inherit' were specified, for non-inherited and inherited properties
   * respectively.
   *
   * @param aPropertyID The ID of the longhand property whose value is to be
   *   copied to the rule data.
   * @param aShorthandPropertyID The ID of the shorthand property to be parsed.
   *   If a longhand property is to be parsed, aPropertyID is that property,
   *   and aShorthandPropertyID must be eCSSProperty_UNKNOWN.
   * @param aValue The CSS token stream value.
   * @param aVariables The set of variable values to use when resolving variable
   *   references.
   * @param aRuleData The rule data object into which parsed property value for
   *   aPropertyID will be stored.
   */
  void ParsePropertyWithVariableReferences(nsCSSPropertyID aPropertyID,
                                           nsCSSPropertyID aShorthandPropertyID,
                                           const nsAString& aValue,
                                           const CSSVariableValues* aVariables,
                                           nsRuleData* aRuleData,
                                           nsIURI* aDocURL,
                                           nsIURI* aBaseURL,
                                           nsIPrincipal* aDocPrincipal,
                                           CSSStyleSheet* aSheet,
                                           uint32_t aLineNumber,
                                           uint32_t aLineOffset);

  bool AgentRulesEnabled() const {
    return mParsingMode == css::eAgentSheetFeatures;
  }
  bool ChromeRulesEnabled() const {
    return mIsChrome || mParsingMode == css::eUserSheetFeatures;
  }

  CSSEnabledState EnabledState() const {
    static_assert(int(CSSEnabledState::eForAllContent) == 0,
                  "CSSEnabledState::eForAllContent should be zero for "
                  "this bitfield to work");
    CSSEnabledState enabledState = CSSEnabledState::eForAllContent;
    if (AgentRulesEnabled()) {
      enabledState |= CSSEnabledState::eInUASheets;
    }
    if (ChromeRulesEnabled()) {
      enabledState |= CSSEnabledState::eInChrome;
    }
    return enabledState;
  }

  nsCSSPropertyID LookupEnabledProperty(const nsAString& aProperty) {
    return nsCSSProps::LookupProperty(aProperty, EnabledState());
  }

protected:
  class nsAutoParseCompoundProperty;
  friend class nsAutoParseCompoundProperty;

  class nsAutoFailingSupportsRule;
  friend class nsAutoFailingSupportsRule;

  class nsAutoSuppressErrors;
  friend class nsAutoSuppressErrors;

  void AppendRule(css::Rule* aRule);
  friend void AppendRuleToSheet(css::Rule*, void*); // calls AppendRule

  /**
   * This helper class automatically calls SetParsingCompoundProperty in its
   * constructor and takes care of resetting it to false in its destructor.
   */
  class nsAutoParseCompoundProperty {
    public:
      explicit nsAutoParseCompoundProperty(CSSParserImpl* aParser) : mParser(aParser)
      {
        NS_ASSERTION(!aParser->IsParsingCompoundProperty(),
                     "already parsing compound property");
        NS_ASSERTION(aParser, "Null parser?");
        aParser->SetParsingCompoundProperty(true);
      }

      ~nsAutoParseCompoundProperty()
      {
        mParser->SetParsingCompoundProperty(false);
      }
    private:
      CSSParserImpl* mParser;
  };

  /**
   * This helper class conditionally sets mInFailingSupportsRule to
   * true if aCondition = false, and resets it to its original value in its
   * destructor.  If we are already somewhere within a failing @supports
   * rule, passing in aCondition = true does not change mInFailingSupportsRule.
   */
  class nsAutoFailingSupportsRule {
    public:
      nsAutoFailingSupportsRule(CSSParserImpl* aParser,
                                bool aCondition)
        : mParser(aParser),
          mOriginalValue(aParser->mInFailingSupportsRule)
      {
        if (!aCondition) {
          mParser->mInFailingSupportsRule = true;
        }
      }

      ~nsAutoFailingSupportsRule()
      {
        mParser->mInFailingSupportsRule = mOriginalValue;
      }

    private:
      CSSParserImpl* mParser;
      bool mOriginalValue;
  };

  /**
   * Auto class to set aParser->mSuppressErrors to the specified value
   * and restore it to its original value later.
   */
  class nsAutoSuppressErrors {
    public:
      explicit nsAutoSuppressErrors(CSSParserImpl* aParser,
                                    bool aSuppressErrors = true)
        : mParser(aParser),
          mOriginalValue(aParser->mSuppressErrors)
      {
        mParser->mSuppressErrors = aSuppressErrors;
      }

      ~nsAutoSuppressErrors()
      {
        mParser->mSuppressErrors = mOriginalValue;
      }

    private:
      CSSParserImpl* mParser;
      bool mOriginalValue;
  };

  /**
   * RAII class to set aParser->mInSupportsCondition to true and restore it
   * to false later.
   */
  class MOZ_RAII nsAutoInSupportsCondition
  {
  public:
    explicit nsAutoInSupportsCondition(CSSParserImpl* aParser)
      : mParser(aParser)
    {
      MOZ_ASSERT(!aParser->mInSupportsCondition,
                 "nsAutoInSupportsCondition is not designed to be used "
                 "re-entrantly");
      mParser->mInSupportsCondition = true;
    }

    ~nsAutoInSupportsCondition()
    {
      mParser->mInSupportsCondition = false;
    }

  private:
    CSSParserImpl* const mParser;
  };

  // the caller must hold on to aString until parsing is done
  void InitScanner(nsCSSScanner& aScanner,
                   css::ErrorReporter& aReporter,
                   nsIURI* aSheetURI, nsIURI* aBaseURI,
                   nsIPrincipal* aSheetPrincipal,
                   dom::CallerType aCallerType = dom::CallerType::NonSystem);
  void ReleaseScanner(void);

  /**
   * Saves the current input state, which includes any currently pushed
   * back token, and the current position of the scanner.
   */
  void SaveInputState(CSSParserInputState& aState);

  /**
   * Restores the saved input state by pushing back any saved pushback
   * token and calling RestoreSavedPosition on the scanner.
   */
  void RestoreSavedInputState(const CSSParserInputState& aState);

  bool GetToken(bool aSkipWS);
  void UngetToken();
  bool GetNextTokenLocation(bool aSkipWS, uint32_t *linenum, uint32_t *colnum);
  void AssertNextTokenAt(uint32_t aLine, uint32_t aCol)
  {
    // Beware that this method will call GetToken/UngetToken (in
    // GetNextTokenLocation) in DEBUG builds, but not in non-DEBUG builds.
    DebugOnly<uint32_t> lineAfter, colAfter;
    MOZ_ASSERT(GetNextTokenLocation(true, &lineAfter, &colAfter) &&
               lineAfter == aLine && colAfter == aCol,
               "shouldn't have consumed any tokens");
  }

  bool ExpectSymbol(char16_t aSymbol, bool aSkipWS);
  bool ExpectEndProperty();
  bool CheckEndProperty();
  nsAString* NextIdent();

  // returns true when the stop symbol is found, and false for EOF
  bool SkipUntil(char16_t aStopSymbol);
  void SkipUntilOneOf(const char16_t* aStopSymbolChars);
  // For each character in aStopSymbolChars from the end of the array
  // to the start, calls SkipUntil with that character.
  typedef AutoTArray<char16_t, 16> StopSymbolCharStack;
  void SkipUntilAllOf(const StopSymbolCharStack& aStopSymbolChars);
  // returns true if the stop symbol or EOF is found, and false for an
  // unexpected ')', ']' or '}'; this not safe to call outside variable
  // resolution, as it doesn't handle mismatched content
  bool SkipBalancedContentUntil(char16_t aStopSymbol);

  void SkipRuleSet(bool aInsideBraces);
  bool SkipAtRule(bool aInsideBlock);
  MOZ_MUST_USE bool SkipDeclaration(bool aCheckForBraces);

  void PushGroup(css::GroupRule* aRule);
  void PopGroup();

  bool ParseRuleSet(RuleAppendFunc aAppendFunc, void* aProcessData,
                    bool aInsideBraces = false);
  bool ParseAtRule(RuleAppendFunc aAppendFunc, void* aProcessData,
                   bool aInAtRule);
  bool ParseCharsetRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseImportRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseURLOrString(nsString& aURL);
  bool GatherMedia(nsMediaList* aMedia, bool aInAtRule);

  enum eMediaQueryType { eMediaQueryNormal,
                         // Parsing an at rule
                         eMediaQueryAtRule,
                         // Attempt to consume a single media-condition and
                         // stop. Note that the spec defines "expression and/or
                         // expression" as one condition but "expression,
                         // expression" as two.
                         eMediaQuerySingleCondition };
  bool ParseMediaQuery(eMediaQueryType aMode, nsMediaQuery **aQuery,
                       bool *aHitStop);
  bool ParseMediaQueryExpression(nsMediaQuery* aQuery);
  void ProcessImport(const nsString& aURLSpec,
                     nsMediaList* aMedia,
                     RuleAppendFunc aAppendFunc,
                     void* aProcessData,
                     uint32_t aLineNumber,
                     uint32_t aColumnNumber);
  bool ParseGroupRule(css::GroupRule* aRule, RuleAppendFunc aAppendFunc,
                      void* aProcessData);
  bool ParseMediaRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseMozDocumentRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  void ProcessNameSpace(const nsString& aPrefix,
                        const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                        void* aProcessData,
                        uint32_t aLineNumber, uint32_t aColumnNumber);

  bool ParseFontFaceRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseFontFeatureValuesRule(RuleAppendFunc aAppendFunc,
                                  void* aProcessData);
  bool ParseFontFeatureValueSet(nsCSSFontFeatureValuesRule *aRule);
  bool ParseFontDescriptor(nsCSSFontFaceRule* aRule);
  bool ParseFontDescriptorValue(nsCSSFontDesc aDescID,
                                nsCSSValue& aValue);

  bool ParsePageRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseKeyframesRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  already_AddRefed<nsCSSKeyframeRule> ParseKeyframeRule();
  bool ParseKeyframeSelectorList(InfallibleTArray<float>& aSelectorList);

  bool ParseSupportsRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseSupportsCondition(bool& aConditionMet);
  bool ParseSupportsConditionNegation(bool& aConditionMet);
  bool ParseSupportsConditionInParens(bool& aConditionMet);
  bool ParseSupportsMozBoolPrefName(bool& aConditionMet);
  bool ParseSupportsConditionInParensInsideParens(bool& aConditionMet);
  bool ParseSupportsConditionTerms(bool& aConditionMet);
  enum SupportsConditionTermOperator { eAnd, eOr };
  bool ParseSupportsConditionTermsAfterOperator(
                                       bool& aConditionMet,
                                       SupportsConditionTermOperator aOperator);

  bool ParseCounterStyleRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  already_AddRefed<nsAtom> ParseCounterStyleName(bool aForDefinition);
  bool ParseCounterStyleNameValue(nsCSSValue& aValue);
  bool ParseCounterDescriptor(nsCSSCounterStyleRule *aRule);
  bool ParseCounterDescriptorValue(nsCSSCounterDesc aDescID,
                                   nsCSSValue& aValue);
  bool ParseCounterRange(nsCSSValuePair& aPair);

  /**
   * Parses the current input stream for a CSS token stream value and resolves
   * any variable references using the variables in aVariables.
   *
   * @param aVariables The set of variable values to use when resolving variable
   *   references.
   * @param aResult Out parameter that, if the function returns true, will be
   *   replaced with the resolved value.
   * @return Whether aResult could be parsed successfully and variable reference
   *   substitution succeeded.
   */
  bool ResolveValueWithVariableReferences(
                              const CSSVariableValues* aVariables,
                              nsString& aResult,
                              nsCSSTokenSerializationType& aResultFirstToken,
                              nsCSSTokenSerializationType& aResultLastToken);
  // Helper function for ResolveValueWithVariableReferences.
  bool ResolveValueWithVariableReferencesRec(
                             nsString& aResult,
                             nsCSSTokenSerializationType& aResultFirstToken,
                             nsCSSTokenSerializationType& aResultLastToken,
                             const CSSVariableValues* aVariables);

  enum nsSelectorParsingStatus {
    // we have parsed a selector and we saw a token that cannot be
    // part of a selector:
    eSelectorParsingStatus_Done,
    // we should continue parsing the selector:
    eSelectorParsingStatus_Continue,
    // we saw an unexpected token or token value,
    // or we saw end-of-file with an unfinished selector:
    eSelectorParsingStatus_Error
  };
  nsSelectorParsingStatus ParseIDSelector(int32_t&       aDataMask,
                                          nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseClassSelector(int32_t&       aDataMask,
                                             nsCSSSelector& aSelector);

  // aPseudoElement and aPseudoElementArgs are the location where
  // pseudo-elements (as opposed to pseudo-classes) are stored;
  // pseudo-classes are stored on aSelector.  aPseudoElement and
  // aPseudoElementArgs must be non-null iff !aIsNegated.
  nsSelectorParsingStatus ParsePseudoSelector(int32_t&       aDataMask,
                                              nsCSSSelector& aSelector,
                                              bool           aIsNegated,
                                              nsAtom**      aPseudoElement,
                                              nsAtomList**   aPseudoElementArgs,
                                              CSSPseudoElementType* aPseudoElementType);

  nsSelectorParsingStatus ParseAttributeSelector(int32_t&       aDataMask,
                                                 nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseTypeOrUniversalSelector(int32_t&       aDataMask,
                                                       nsCSSSelector& aSelector,
                                                       bool           aIsNegated);

  nsSelectorParsingStatus ParsePseudoClassWithIdentArg(nsCSSSelector& aSelector,
                                                       CSSPseudoClassType aType);

  nsSelectorParsingStatus ParsePseudoClassWithNthPairArg(nsCSSSelector& aSelector,
                                                         CSSPseudoClassType aType);

  nsSelectorParsingStatus ParsePseudoClassWithSelectorListArg(nsCSSSelector& aSelector,
                                                              CSSPseudoClassType aType);

  nsSelectorParsingStatus ParseNegatedSimpleSelector(int32_t&       aDataMask,
                                                     nsCSSSelector& aSelector);

  // If aStopChar is non-zero, the selector list is done when we hit
  // aStopChar.  Otherwise, it's done when we hit EOF.
  bool ParseSelectorList(nsCSSSelectorList*& aListHead,
                           char16_t aStopChar);
  bool ParseSelectorGroup(nsCSSSelectorList*& aListHead);
  bool ParseSelector(nsCSSSelectorList* aList, char16_t aPrevCombinator);

  enum {
    eParseDeclaration_InBraces           = 1 << 0,
    eParseDeclaration_AllowImportant     = 1 << 1
  };
  enum nsCSSContextType {
    eCSSContext_General,
    eCSSContext_Page
  };

  already_AddRefed<css::Declaration>
    ParseDeclarationBlock(uint32_t aFlags,
                          nsCSSContextType aContext = eCSSContext_General);
  bool ParseDeclaration(css::Declaration* aDeclaration,
                        uint32_t aFlags,
                        bool aMustCallValueAppended,
                        bool* aChanged,
                        nsCSSContextType aContext = eCSSContext_General);

  // A "prefix-aware" wrapper for nsCSSKeywords::LookupKeyword().
  // Use this instead of LookupKeyword() if you might be parsing an unprefixed
  // property (like "display") for which we emulate a vendor-prefixed value
  // (like "-webkit-box").
  nsCSSKeyword LookupKeywordPrefixAware(nsAString& aKeywordStr,
                                        const KTableEntry aKeywordTable[]);

  bool ParseProperty(nsCSSPropertyID aPropID);
  bool ParsePropertyByFunction(nsCSSPropertyID aPropID);
  CSSParseResult ParseSingleValueProperty(nsCSSValue& aValue,
                                          nsCSSPropertyID aPropID);
  bool ParseSingleValuePropertyByFunction(nsCSSValue& aValue,
                                          nsCSSPropertyID aPropID);

  // This is similar to ParseSingleValueProperty but only works for
  // properties that are parsed with ParseBoxProperties or
  // ParseGroupedBoxProperty.
  //
  // Only works with variants with the following flags:
  // A, C, H, K, L, N, P, CALC.
  CSSParseResult ParseBoxProperty(nsCSSValue& aValue,
                                  nsCSSPropertyID aPropID);

  enum PriorityParsingStatus {
    ePriority_None,
    ePriority_Important,
    ePriority_Error
  };
  PriorityParsingStatus ParsePriority();

#ifdef MOZ_XUL
  bool ParseTreePseudoElement(nsAtomList **aPseudoElementArgs);
#endif

  // Property specific parsing routines
  bool ParseImageLayers(const nsCSSPropertyID aTable[]);

  struct ImageLayersShorthandParseState {
    nsCSSValue&  mColor;
    nsCSSValueList* mImage;
    nsCSSValuePairList* mRepeat;
    nsCSSValueList* mAttachment;   // A property for background layer only
    nsCSSValueList* mClip;
    nsCSSValueList* mOrigin;
    nsCSSValueList* mPositionX;
    nsCSSValueList* mPositionY;
    nsCSSValuePairList* mSize;
    nsCSSValueList* mComposite;    // A property for mask layer only
    nsCSSValueList* mMode;         // A property for mask layer only
    ImageLayersShorthandParseState(
        nsCSSValue& aColor, nsCSSValueList* aImage, nsCSSValuePairList* aRepeat,
        nsCSSValueList* aAttachment, nsCSSValueList* aClip,
        nsCSSValueList* aOrigin,
        nsCSSValueList* aPositionX, nsCSSValueList* aPositionY,
        nsCSSValuePairList* aSize, nsCSSValueList* aComposite,
        nsCSSValueList* aMode) :
        mColor(aColor), mImage(aImage), mRepeat(aRepeat),
        mAttachment(aAttachment), mClip(aClip), mOrigin(aOrigin),
        mPositionX(aPositionX), mPositionY(aPositionY),
        mSize(aSize), mComposite(aComposite),
        mMode(aMode) {};
  };

  bool IsFunctionTokenValidForImageLayerImage(const nsCSSToken& aToken) const;
  bool ParseImageLayersItem(ImageLayersShorthandParseState& aState,
                            const nsCSSPropertyID aTable[]);

  bool ParseValueList(nsCSSPropertyID aPropID); // a single value prop-id
  bool ParseImageLayerRepeat(nsCSSPropertyID aPropID);
  bool ParseImageLayerRepeatValues(nsCSSValuePair& aValue);
  bool ParseImageLayerPosition(const nsCSSPropertyID aTable[]);
  bool ParseImageLayerPositionCoord(nsCSSPropertyID aPropID, bool aIsHorizontal);

  // ParseBoxPositionValues parses the CSS 2.1 background-position syntax,
  // which is still used by some properties. See ParsePositionValue
  // for the css3-background syntax.
  bool ParseBoxPositionValues(nsCSSValuePair& aOut, bool aAcceptsInherit,
                              bool aAllowExplicitCenter = true); // deprecated

  // ParsePositionValue parses a CSS <position> value, which is used by
  // the 'background-position' property.
  bool ParsePositionValue(nsCSSValue& aOut);
  bool ParsePositionValueSeparateCoords(nsCSSValue& aOutX, nsCSSValue& aOutY);

  bool ParseImageLayerPositionCoordItem(nsCSSValue& aOut, bool aIsHorizontal);
  bool ParseImageLayerSize(nsCSSPropertyID aPropID);
  bool ParseImageLayerSizeValues(nsCSSValuePair& aOut);
  bool ParseBorderColor();
  bool ParseBorderColors(nsCSSPropertyID aProperty);
  void SetBorderImageInitialValues();
  bool ParseBorderImageRepeat(bool aAcceptsInherit);
  // If ParseBorderImageSlice returns false, aConsumedTokens indicates
  // whether or not any tokens were consumed (in other words, was the property
  // in error or just not present).  If ParseBorderImageSlice returns true
  // aConsumedTokens is always true.
  bool ParseBorderImageSlice(bool aAcceptsInherit, bool* aConsumedTokens);
  bool ParseBorderImageWidth(bool aAcceptsInherit);
  bool ParseBorderImageOutset(bool aAcceptsInherit);
  bool ParseBorderImage();
  bool ParseBorderSpacing();
  bool ParseBorderSide(const nsCSSPropertyID aPropIDs[],
                         bool aSetAllSides);
  bool ParseBorderStyle();
  bool ParseBorderWidth();

  bool ParseCalc(nsCSSValue &aValue, uint32_t aVariantMask);
  bool ParseCalcAdditiveExpression(nsCSSValue& aValue,
                                   uint32_t& aVariantMask);
  bool ParseCalcMultiplicativeExpression(nsCSSValue& aValue,
                                         uint32_t& aVariantMask,
                                         bool *aHadFinalWS);
  bool ParseCalcTerm(nsCSSValue& aValue, uint32_t& aVariantMask);
  bool ParseContextProperties();
  bool RequireWhitespace();

  // For "flex" shorthand property, defined in CSS Flexbox spec
  bool ParseFlex();
  // For "flex-flow" shorthand property, defined in CSS Flexbox spec
  bool ParseFlexFlow();

  // CSS Grid
  bool ParseGridAutoFlow();

  // Parse a <line-names> expression.
  // If successful, either leave aValue untouched,
  // to indicate that we parsed the empty list,
  // or set it to a eCSSUnit_List of eCSSUnit_Ident.
  //
  // To parse an optional <line-names> (ie. if not finding an open bracket
  // is considered the same as an empty list),
  // treat CSSParseResult::NotFound the same as CSSParseResult::Ok.
  //
  // If aValue is already a eCSSUnit_List, append to that list.
  CSSParseResult ParseGridLineNames(nsCSSValue& aValue);
  bool ParseGridLineNameListRepeat(nsCSSValueList** aTailPtr);
  bool ParseOptionalLineNameListAfterSubgrid(nsCSSValue& aValue);

  CSSParseResult ParseGridTrackBreadth(nsCSSValue& aValue);
  // eFixedTrackSize in aFlags makes it parse a <fixed-size>.
  CSSParseResult ParseGridTrackSize(nsCSSValue& aValue,
    GridTrackSizeFlags aFlags = GridTrackSizeFlags::eDefaultTrackSize);

  bool ParseGridAutoColumnsRows(nsCSSPropertyID aPropID);
  bool ParseGridTrackListRepeat(nsCSSValueList** aTailPtr);
  bool ParseGridTrackRepeatIntro(bool            aForSubgrid,
                                 int32_t*        aRepetitions,
                                 Maybe<int32_t>* aRepeatAutoEnum);

  // Assuming a [ <line-names>? ] has already been parsed,
  // parse the rest of a <track-list>.
  //
  // This exists because [ <line-names>? ] is ambiguous in the 'grid-template'
  // shorthand: it can be either the start of a <track-list> (in
  // a <'grid-template-rows'>) or of the intertwined syntax that sets both
  // grid-template-rows and grid-template-areas.
  //
  // On success, |aValue| will be a list of odd length >= 3,
  // starting with a <line-names> (which is itself a list)
  // and alternating between that and <track-size>.
  bool ParseGridTrackListWithFirstLineNames(nsCSSValue& aValue,
    const nsCSSValue& aFirstLineNames,
    GridTrackListFlags aFlags = GridTrackListFlags::eDefaultTrackList);

  bool ParseGridTrackList(nsCSSPropertyID aPropID,
    GridTrackListFlags aFlags = GridTrackListFlags::eDefaultTrackList);
  bool ParseGridTemplateColumnsRows(nsCSSPropertyID aPropID);

  // |aAreaIndices| is a lookup table to help us parse faster,
  // mapping area names to indices in |aResult.mNamedAreas|.
  bool ParseGridTemplateAreasLine(const nsAutoString& aInput,
                                  css::GridTemplateAreasValue* aResult,
                                  nsDataHashtable<nsStringHashKey, uint32_t>& aAreaIndices);
  bool ParseGridTemplateAreas();
  bool ParseGridTemplateColumnsOrAutoFlow(bool aForGridShorthand);
  bool ParseGridTemplate(bool aForGridShorthand = false);
  bool ParseGridTemplateAfterString(const nsCSSValue& aFirstLineNames);
  bool ParseGrid();
  CSSParseResult ParseGridShorthandAutoProps(int32_t aAutoFlowAxis);
  bool ParseGridLine(nsCSSValue& aValue);
  bool ParseGridColumnRowStartEnd(nsCSSPropertyID aPropID);
  bool ParseGridColumnRow(nsCSSPropertyID aStartPropID,
                          nsCSSPropertyID aEndPropID);
  bool ParseGridArea();
  bool ParseGridGap();

  bool ParseInitialLetter();

  // parsing 'align/justify-items/self' from the css-align spec
  bool ParseAlignJustifyPosition(nsCSSValue& aResult,
                                 const KTableEntry aTable[]);
  bool ParseJustifyItems();
  bool ParseAlignItems();
  bool ParseAlignJustifySelf(nsCSSPropertyID aPropID);
  // parsing 'align/justify-content' from the css-align spec
  bool ParseAlignJustifyContent(nsCSSPropertyID aPropID);
  bool ParsePlaceContent();
  bool ParsePlaceItems();
  bool ParsePlaceSelf();

  // for 'clip' and '-moz-image-region'
  bool ParseRect(nsCSSPropertyID aPropID);
  bool ParseColumns();
  bool ParseContain(nsCSSValue& aValue);
  bool ParseContent();
  bool ParseCounterData(nsCSSPropertyID aPropID);
  bool ParseCursor();
  bool ParseFont();
  bool ParseFontSynthesis(nsCSSValue& aValue);
  bool ParseSingleAlternate(int32_t& aWhichFeature, nsCSSValue& aValue);
  bool ParseFontVariantAlternates(nsCSSValue& aValue);
  bool MergeBitmaskValue(int32_t aNewValue, const int32_t aMasks[],
                         int32_t& aMergedValue);
  bool ParseBitmaskValues(nsCSSValue& aValue,
                          const KTableEntry aKeywordTable[],
                          const int32_t aMasks[]);
  bool ParseFontVariantEastAsian(nsCSSValue& aValue);
  bool ParseFontVariantLigatures(nsCSSValue& aValue);
  bool ParseFontVariantNumeric(nsCSSValue& aValue);
  bool ParseFontVariant();
  bool ParseFontWeight(nsCSSValue& aValue);
  bool ParseOneFamily(nsAString& aFamily, bool& aOneKeyword, bool& aQuoted);
  bool ParseFamily(nsCSSValue& aValue);
  bool ParseFontFeatureSettings(nsCSSValue& aValue);
  bool ParseFontVariationSettings(nsCSSValue& aValue);
  bool ParseFontSrc(nsCSSValue& aValue);
  bool ParseFontSrcFormat(InfallibleTArray<nsCSSValue>& values);
  bool ParseFontRanges(nsCSSValue& aValue);
  bool ParseListStyle();
  bool ParseListStyleType(nsCSSValue& aValue);
  bool ParseMargin();
  bool ParseClipPath(nsCSSValue& aValue);
  bool ParseTransform(bool aIsPrefixed, nsCSSPropertyID aProperty,
                      bool aDisallowRelativeValues = false);
  bool ParseObjectPosition();
  bool ParseOutline();
  bool ParseOverflow();
  bool ParsePadding();
  bool ParseQuotes();
  bool ParseTextAlign(nsCSSValue& aValue,
                      const KTableEntry aTable[]);
  bool ParseTextAlign(nsCSSValue& aValue);
  bool ParseTextAlignLast(nsCSSValue& aValue);
  bool ParseTextDecoration();
  bool ParseTextDecorationLine(nsCSSValue& aValue);
  bool ParseTextEmphasis();
  bool ParseTextEmphasisPosition(nsCSSValue& aValue);
  bool ParseTextEmphasisStyle(nsCSSValue& aValue);
  bool ParseTextCombineUpright(nsCSSValue& aValue);
  bool ParseTextOverflow(nsCSSValue& aValue);
  bool ParseTouchAction(nsCSSValue& aValue);

  bool ParseShadowItem(nsCSSValue& aValue, bool aIsBoxShadow);
  bool ParseShadowList(nsCSSPropertyID aProperty);
  bool ParseShapeOutside(nsCSSValue& aValue);
  bool ParseTransitionProperty();
  bool ParseTransitionTimingFunctionValues(nsCSSValue& aValue);
  bool ParseTransitionTimingFunctionValueComponent(float& aComponent,
                                                     char aStop,
                                                     bool aIsXPoint);
  bool ParseTransitionStepTimingFunctionValues(nsCSSValue& aValue);
  bool ParseTransitionFramesTimingFunctionValues(nsCSSValue& aValue);
  enum ParseAnimationOrTransitionShorthandResult {
    eParseAnimationOrTransitionShorthand_Values,
    eParseAnimationOrTransitionShorthand_Inherit,
    eParseAnimationOrTransitionShorthand_Error
  };
  ParseAnimationOrTransitionShorthandResult
    ParseAnimationOrTransitionShorthand(const nsCSSPropertyID* aProperties,
                                        const nsCSSValue* aInitialValues,
                                        nsCSSValue* aValues,
                                        size_t aNumProperties);
  bool ParseTransition();
  bool ParseAnimation();
  bool ParseWillChange();

  bool ParsePaint(nsCSSPropertyID aPropID);
  bool ParseDasharray();
  bool ParseMarker();
  bool ParsePaintOrder();
  bool ParseAll();
  bool ParseScrollSnapType();
  bool ParseScrollSnapPoints(nsCSSValue& aValue, nsCSSPropertyID aPropID);
  bool ParseScrollSnapDestination(nsCSSValue& aValue);
  bool ParseScrollSnapCoordinate(nsCSSValue& aValue);
  bool ParseOverscrollBehavior();
  bool ParseWebkitTextStroke();

  /**
   * Parses a variable value from a custom property declaration.
   *
   * @param aType Out parameter into which will be stored the type of variable
   *   value, indicating whether the parsed value was a token stream or one of
   *   the CSS-wide keywords.
   * @param aValue Out parameter into which will be stored the token stream
   *   as a string, if the parsed custom property did take a token stream.
   * @return Whether parsing succeeded.
   */
  bool ParseVariableDeclaration(CSSVariableDeclarations::Type* aType,
                                nsString& aValue);

  /**
   * Parses a CSS variable value.  This could be 'initial', 'inherit', 'unset'
   * or a token stream, which may or may not include variable references.
   *
   * @param aType Out parameter into which the type of the variable value
   *   will be stored.
   * @param aDropBackslash Out parameter indicating whether during variable
   *   value parsing there was a trailing backslash before EOF that must
   *   be dropped when serializing the variable value.
   * @param aImpliedCharacters Out parameter appended to which will be any
   *   characters that were implied when encountering EOF and which
   *   must be included at the end of the serialized variable value.
   * @param aFunc A callback function to invoke when a variable reference
   *   is encountered.  May be null.  Arguments are the variable name
   *   and the aData argument passed in to this function.
   * @param User data to pass in to the callback.
   * @return Whether parsing succeeded.
   */
  bool ParseValueWithVariables(CSSVariableDeclarations::Type* aType,
                               bool* aDropBackslash,
                               nsString& aImpliedCharacters,
                               void (*aFunc)(const nsAString&, void*),
                               void* aData);

  /**
   * Returns whether the scanner dropped a backslash just before EOF.
   */
  bool BackslashDropped();

  /**
   * Calls AppendImpliedEOFCharacters on mScanner.
   */
  void AppendImpliedEOFCharacters(nsAString& aResult);

  // Reused utility parsing routines
  void AppendValue(nsCSSPropertyID aPropID, const nsCSSValue& aValue);
  bool ParseBoxProperties(const nsCSSPropertyID aPropIDs[]);
  bool ParseGroupedBoxProperty(int32_t aVariantMask,
                               nsCSSValue& aValue,
                               uint32_t aRestrictions);
  bool ParseBoxCornerRadius(const nsCSSPropertyID aPropID);
  bool ParseBoxCornerRadiiInternals(nsCSSValue array[]);
  bool ParseBoxCornerRadii(const nsCSSPropertyID aPropIDs[]);

  int32_t ParseChoice(nsCSSValue aValues[],
                      const nsCSSPropertyID aPropIDs[], int32_t aNumIDs);

  CSSParseResult ParseColor(nsCSSValue& aValue);

  template<typename ComponentType>
  bool ParseRGBColor(ComponentType& aR,
                     ComponentType& aG,
                     ComponentType& aB,
                     ComponentType& aA);
  bool ParseHSLColor(float& aHue, float& aSaturation, float& aLightness,
                     float& aOpacity);

  // The ParseColorOpacityAndCloseParen methods below attempt to parse an
  // optional [ separator <alpha-value> ] expression, followed by a
  // close-parenthesis, at the end of a css color function (e.g. "rgba()" or
  // "hsla()"). If these functions simply encounter a close-parenthesis (without
  // any [separator <alpha-value>]), they will still succeed (i.e. return true),
  // with outparam 'aOpacity' set to a default opacity value (fully-opaque).
  //
  // The range of opacity component is [0, 255], and the default opacity value
  // is 255 (fully-opaque) for this function.
  bool ParseColorOpacityAndCloseParen(uint8_t& aOpacity,
                                      char aSeparator);
  // Similar to the previous one, but the range of opacity component is
  // [0.0f, 1.0f] and the default opacity value is 1.0f (fully-opaque).
  bool ParseColorOpacityAndCloseParen(float& aOpacity,
                                      char aSeparator);

  // Parse a <number> color component. The range of color component is [0, 255].
  // If |aSeparator| is provided, this function will also attempt to parse that
  // character after parsing the color component.
  bool ParseColorComponent(uint8_t& aComponent, const Maybe<char>& aSeparator);
  // Similar to the previous one, but parse a <percentage> color component.
  // The range of color component is [0.0f, 1.0f].
  bool ParseColorComponent(float& aComponent, const Maybe<char>& aSeparator);

  // Parse a <hue> component.
  //   <hue> = <number> | <angle>
  // The unit of outparam 'aAngle' is degree. Assume the unit to be degree if an
  // unitless <number> is parsed.
  bool ParseHue(float& aAngle);

  bool ParseEnum(nsCSSValue& aValue,
                 const KTableEntry aKeywordTable[]);

  // A special ParseEnum for the CSS Box Alignment properties that have
  // 'baseline' values.  In addition to the keywords in aKeywordTable, it also
  // parses 'first baseline' and 'last baseline' as a single value.
  // (aKeywordTable must contain 'baseline')
  bool ParseAlignEnum(nsCSSValue& aValue, const KTableEntry aKeywordTable[]);

  // Variant parsing methods
  CSSParseResult ParseVariant(nsCSSValue& aValue,
                              uint32_t aVariantMask,
                              const KTableEntry aKeywordTable[]);
  CSSParseResult ParseVariantWithRestrictions(nsCSSValue& aValue,
                                              int32_t aVariantMask,
                                              const KTableEntry aKeywordTable[],
                                              uint32_t aRestrictions);
  CSSParseResult ParseNonNegativeVariant(nsCSSValue& aValue,
                                         int32_t aVariantMask,
                                         const KTableEntry aKeywordTable[]);
  CSSParseResult ParseOneOrLargerVariant(nsCSSValue& aValue,
                                         int32_t aVariantMask,
                                         const KTableEntry aKeywordTable[]);

  // Variant parsing methods that are guaranteed to UngetToken any token
  // consumed on failure

  MOZ_MUST_USE bool ParseSingleTokenVariant(nsCSSValue& aValue,
                                            int32_t aVariantMask,
                                            const KTableEntry aKeywordTable[])
  {
    MOZ_ASSERT(!(aVariantMask & VARIANT_MULTIPLE_TOKENS),
               "use ParseVariant for variants in VARIANT_MULTIPLE_TOKENS");
    CSSParseResult result = ParseVariant(aValue, aVariantMask, aKeywordTable);
    MOZ_ASSERT(result != CSSParseResult::Error);
    return result == CSSParseResult::Ok;
  }
  bool ParseSingleTokenNonNegativeVariant(nsCSSValue& aValue,
                                          int32_t aVariantMask,
                                          const KTableEntry aKeywordTable[])
  {
    MOZ_ASSERT(!(aVariantMask & VARIANT_MULTIPLE_TOKENS),
               "use ParseNonNegativeVariant for variants in "
               "VARIANT_MULTIPLE_TOKENS");
    CSSParseResult result =
      ParseNonNegativeVariant(aValue, aVariantMask, aKeywordTable);
    MOZ_ASSERT(result != CSSParseResult::Error);
    return result == CSSParseResult::Ok;
  }
  bool ParseSingleTokenOneOrLargerVariant(nsCSSValue& aValue,
                                          int32_t aVariantMask,
                                          const KTableEntry aKeywordTable[])
  {
    MOZ_ASSERT(!(aVariantMask & VARIANT_MULTIPLE_TOKENS),
               "use ParseOneOrLargerVariant for variants in "
               "VARIANT_MULTIPLE_TOKENS");
    CSSParseResult result =
      ParseOneOrLargerVariant(aValue, aVariantMask, aKeywordTable);
    MOZ_ASSERT(result != CSSParseResult::Error);
    return result == CSSParseResult::Ok;
  }

  // Helpers for some common ParseSingleTokenNonNegativeVariant calls.
  bool ParseNonNegativeInteger(nsCSSValue& aValue)
  {
    return ParseSingleTokenNonNegativeVariant(aValue, VARIANT_INTEGER, nullptr);
  }
  bool ParseNonNegativeNumber(nsCSSValue& aValue)
  {
    return ParseSingleTokenNonNegativeVariant(aValue, VARIANT_NUMBER, nullptr);
  }

  // Helpers for some common ParseSingleTokenOneOrLargerVariant calls.
  bool ParseOneOrLargerInteger(nsCSSValue& aValue)
  {
    return ParseSingleTokenOneOrLargerVariant(aValue, VARIANT_INTEGER, nullptr);
  }
  bool ParseOneOrLargerNumber(nsCSSValue& aValue)
  {
    return ParseSingleTokenOneOrLargerVariant(aValue, VARIANT_NUMBER, nullptr);
  }

  // http://dev.w3.org/csswg/css-values/#custom-idents
  // Parse an identifier that is none of:
  // * a CSS-wide keyword
  // * "default"
  // * a keyword in |aExcludedKeywords|
  // * a keyword in |aPropertyKTable|
  //
  // |aExcludedKeywords| is an array of nsCSSKeyword
  // that ends with a eCSSKeyword_UNKNOWN marker.
  //
  // |aPropertyKTable| can be used if some of the keywords to exclude
  // also appear in an existing nsCSSProps::KTableEntry,
  // to avoid duplicating them.
  bool ParseCustomIdent(nsCSSValue& aValue,
                        const nsAutoString& aIdentValue,
                        const nsCSSKeyword aExcludedKeywords[] = nullptr,
                        const nsCSSProps::KTableEntry aPropertyKTable[] = nullptr);
  bool ParseCounter(nsCSSValue& aValue);
  bool ParseAttr(nsCSSValue& aValue);
  bool ParseSymbols(nsCSSValue& aValue);
  bool SetValueToURL(nsCSSValue& aValue, const nsString& aURL);
  bool TranslateDimension(nsCSSValue& aValue, uint32_t aVariantMask,
                            float aNumber, const nsString& aUnit);
  bool ParseImageOrientation(nsCSSValue& aAngle);
  bool ParseImageRect(nsCSSValue& aImage);
  bool ParseElement(nsCSSValue& aValue);
  bool ParseColorStop(nsCSSValueGradient* aGradient);

  enum GradientParsingFlags {
    eGradient_Repeating    = 1 << 0, // repeating-{linear|radial}-gradient
    eGradient_MozLegacy    = 1 << 1, // -moz-{linear|radial}-gradient
    eGradient_WebkitLegacy = 1 << 2, // -webkit-{linear|radial}-gradient

    // Mask to catch both "legacy" flags:
    eGradient_AnyLegacy = eGradient_MozLegacy | eGradient_WebkitLegacy
  };
  bool ParseLinearGradient(nsCSSValue& aValue, uint8_t aFlags);
  bool ParseRadialGradient(nsCSSValue& aValue, uint8_t aFlags);
  bool IsLegacyGradientLine(const nsCSSTokenType& aType,
                            const nsString& aId);
  bool ParseGradientColorStops(nsCSSValueGradient* aGradient,
                               nsCSSValue& aValue);

  // For the ancient "-webkit-gradient(linear|radial, ...)" syntax:
  bool ParseWebkitGradientPointComponent(nsCSSValue& aComponent,
                                         bool aIsHorizontal);
  bool ParseWebkitGradientPoint(nsCSSValuePair& aPoint);
  bool ParseWebkitGradientRadius(float& aRadius);
  bool ParseWebkitGradientColorStop(nsCSSValueGradient* aGradient);
  bool ParseWebkitGradientColorStops(nsCSSValueGradient* aGradient);
  void FinalizeLinearWebkitGradient(nsCSSValueGradient* aGradient,
                                    const nsCSSValuePair& aStartPoint,
                                    const nsCSSValuePair& aSecondPoint);
  void FinalizeRadialWebkitGradient(nsCSSValueGradient* aGradient,
                                    const nsCSSValuePair& aFirstCenter,
                                    const nsCSSValuePair& aSecondCenter,
                                    const float aFirstRadius,
                                    const float aSecondRadius);
  bool ParseWebkitGradient(nsCSSValue& aValue);

  void SetParsingCompoundProperty(bool aBool) {
    mParsingCompoundProperty = aBool;
  }
  bool IsParsingCompoundProperty(void) const {
    return mParsingCompoundProperty;
  }

  /* Functions for basic shapes */
  bool ParseReferenceBoxAndBasicShape(nsCSSValue& aValue,
                                      const KTableEntry aBoxKeywordTable[]);
  bool ParseBasicShape(nsCSSValue& aValue, bool* aConsumedTokens);
  bool ParsePolygonFunction(nsCSSValue& aValue);
  bool ParseCircleOrEllipseFunction(nsCSSKeyword, nsCSSValue& aValue);
  bool ParseInsetFunction(nsCSSValue& aValue);
  // We parse position values differently for basic-shape, by expanding defaults
  // and replacing keywords with percentages
  bool ParsePositionValueForBasicShape(nsCSSValue& aOut);


  /* Functions for transform Parsing */
  bool ParseSingleTransform(bool aIsPrefixed, bool aDisallowRelativeValues,
                            nsCSSValue& aValue);
  bool ParseFunction(nsCSSKeyword aFunction, const uint32_t aAllowedTypes[],
                     uint32_t aVariantMaskAll, uint16_t aMinElems,
                     uint16_t aMaxElems, nsCSSValue &aValue);
  bool ParseFunctionInternals(const uint32_t aVariantMask[],
                              uint32_t aVariantMaskAll,
                              uint16_t aMinElems,
                              uint16_t aMaxElems,
                              InfallibleTArray<nsCSSValue>& aOutput);

  /* Functions for transform-origin/perspective-origin Parsing */
  bool ParseTransformOrigin(nsCSSPropertyID aProperty);

  /* Functions for filter parsing */
  bool ParseFilter();
  bool ParseSingleFilter(nsCSSValue* aValue);
  bool ParseDropShadow(nsCSSValue* aValue);

  /* Find and return the namespace ID associated with aPrefix.
     If aPrefix has not been declared in an @namespace rule, returns
     kNameSpaceID_Unknown. */
  int32_t GetNamespaceIdForPrefix(const nsString& aPrefix);

  /* Find the correct default namespace, and set it on aSelector. */
  void SetDefaultNamespaceOnSelector(nsCSSSelector& aSelector);

  // Current token. The value is valid after calling GetToken and invalidated
  // by UngetToken.
  nsCSSToken mToken;

  // Our scanner.
  nsCSSScanner* mScanner;

  // Our error reporter.
  css::ErrorReporter* mReporter;

  // The URI to be used as a base for relative URIs.
  nsCOMPtr<nsIURI> mBaseURI;

  // The URI to be used as an HTTP "Referer" and for error reporting.
  nsCOMPtr<nsIURI> mSheetURI;

  // The principal of the sheet involved
  nsCOMPtr<nsIPrincipal> mSheetPrincipal;

  // The sheet we're parsing into
  RefPtr<CSSStyleSheet> mSheet;

  // Used for @import rules
  css::Loader* mChildLoader; // not ref counted, it owns us

  // Any sheets we may reuse when parsing an @import.
  css::LoaderReusableStyleSheets* mReusableSheets;

  // Sheet section we're in.  This is used to enforce correct ordering of the
  // various rule types (eg the fact that a @charset rule must come before
  // anything else).  Note that there are checks of similar things in various
  // places in CSSStyleSheet.cpp (e.g in insertRule, RebuildChildList).
  enum nsCSSSection {
    eCSSSection_Charset,
    eCSSSection_Import,
    eCSSSection_NameSpace,
    eCSSSection_General
  };
  nsCSSSection  mSection;

  nsXMLNameSpaceMap *mNameSpaceMap;  // weak, mSheet owns it

  // After an UngetToken is done this flag is true. The next call to
  // GetToken clears the flag.
  bool mHavePushBack : 1;

  // True if we are in quirks mode; false in standards or almost standards mode
  bool          mNavQuirkMode : 1;

  // True when the hashless color quirk applies.
  bool mHashlessColorQuirk : 1;

  // True when the unitless length quirk applies.
  bool mUnitlessLengthQuirk : 1;

  // True if we are in parsing rules for the chrome.
  bool mIsChrome : 1;

  // True if we're parsing SVG presentation attributes
  // These attributes allow non-calc lengths to be unitless (mapping to px)
  bool mIsSVGMode : 1;

  // True if viewport units should be allowed.
  bool mViewportUnitsEnabled : 1;

  // This flag is set when parsing a non-box shorthand; it's used to not apply
  // some quirks during shorthand parsing
  bool          mParsingCompoundProperty : 1;

  // True if we are in the middle of parsing an @supports condition.
  // This is used to avoid recording the input stream when variable references
  // are encountered in a property declaration in the @supports condition.
  bool mInSupportsCondition : 1;

  // True if we are somewhere within a @supports rule whose condition is
  // false.
  bool mInFailingSupportsRule : 1;

  // True if we will suppress all parse errors (except unexpected EOFs).
  // This is used to prevent errors for declarations inside a failing
  // @supports rule.
  bool mSuppressErrors : 1;

  // True if any parsing of URL values requires a sheet principal to have
  // been passed in the nsCSSScanner constructor.  This is usually the case.
  // It can be set to false, for example, when we create an nsCSSParser solely
  // to parse a property value to test it for syntactic correctness.  When
  // false, an assertion that mSheetPrincipal is non-null is skipped.  Should
  // not be set to false if any nsCSSValues created during parsing can escape
  // out of the parser.
  bool mSheetPrincipalRequired;

  // Controls access to nonstandard style constructs that are not safe
  // for use on the public Web but necessary in UA sheets and/or
  // useful in user sheets.
  css::SheetParsingMode mParsingMode;

  // This enum helps us track whether we've unprefixed "display: -webkit-box"
  // (treating it as "display: flex") in an earlier declaration within a series
  // of declarations.  (This only impacts behavior if
  // sWebkitPrefixedAliasesEnabled is true.)
  enum WebkitBoxUnprefixState : uint8_t {
    eNotParsingDecls, // We are *not* currently parsing a sequence of
                      // CSS declarations. (default state)

    // The next two enum values indicate that we *are* currently parsing a
    // sequence of declarations (in ParseDeclarations or ParseDeclarationBlock)
    // and...
    eHaveNotUnprefixed, // ...we have not unprefixed 'display:-webkit-box' in
                        // this sequence of CSS declarations.
    eHaveUnprefixed // ...we *have* unprefixed 'display:-webkit-box' earlier in
                    // this sequence of CSS declarations.
  };
  WebkitBoxUnprefixState mWebkitBoxUnprefixState;

  // Stack of rule groups; used for @media and such.
  InfallibleTArray<RefPtr<css::GroupRule> > mGroupStack;

  // During the parsing of a property (which may be a shorthand), the data
  // are stored in |mTempData|.  (It is needed to ensure that parser
  // errors cause the data to be ignored, and to ensure that a
  // non-'!important' declaration does not override an '!important'
  // one.)
  nsCSSExpandedDataBlock mTempData;

  // All data from successfully parsed properties are placed into |mData|.
  nsCSSExpandedDataBlock mData;

public:
  // Used from nsCSSParser constructors and destructors
  CSSParserImpl* mNextFree;
};

static void AssignRuleToPointer(css::Rule* aRule, void* aPointer)
{
  css::Rule **pointer = static_cast<css::Rule**>(aPointer);
  NS_ADDREF(*pointer = aRule);
}

static void AppendRuleToSheet(css::Rule* aRule, void* aParser)
{
  CSSParserImpl* parser = (CSSParserImpl*) aParser;
  parser->AppendRule(aRule);
}

#define REPORT_UNEXPECTED(msg_) \
  { if (!mSuppressErrors) mReporter->ReportUnexpected(#msg_); }

#define REPORT_UNEXPECTED_P(msg_, param_) \
  { if (!mSuppressErrors) mReporter->ReportUnexpected(#msg_, param_); }

#define REPORT_UNEXPECTED_P_V(msg_, param_, value_) \
  { if (!mSuppressErrors) mReporter->ReportUnexpected(#msg_, param_, value_); }

#define REPORT_UNEXPECTED_TOKEN(msg_) \
  { if (!mSuppressErrors) mReporter->ReportUnexpected(#msg_, mToken); }

#define REPORT_UNEXPECTED_TOKEN_CHAR(msg_, ch_) \
  { if (!mSuppressErrors) mReporter->ReportUnexpected(#msg_, mToken, ch_); }

#define REPORT_UNEXPECTED_EOF(lf_) \
  mReporter->ReportUnexpectedEOF(#lf_)

#define REPORT_UNEXPECTED_EOF_CHAR(ch_) \
  mReporter->ReportUnexpectedEOF(ch_)

#define OUTPUT_ERROR() \
  mReporter->OutputError()

#define OUTPUT_ERROR_WITH_POSITION(linenum_, lineoff_) \
  mReporter->OutputError(linenum_, lineoff_)

#define CLEAR_ERROR() \
  mReporter->ClearError()

CSSParserImpl::CSSParserImpl()
  : mToken(),
    mScanner(nullptr),
    mReporter(nullptr),
    mChildLoader(nullptr),
    mReusableSheets(nullptr),
    mSection(eCSSSection_Charset),
    mNameSpaceMap(nullptr),
    mHavePushBack(false),
    mNavQuirkMode(false),
    mHashlessColorQuirk(false),
    mUnitlessLengthQuirk(false),
    mIsChrome(false),
    mIsSVGMode(false),
    mViewportUnitsEnabled(true),
    mParsingCompoundProperty(false),
    mInSupportsCondition(false),
    mInFailingSupportsRule(false),
    mSuppressErrors(false),
    mSheetPrincipalRequired(true),
    mParsingMode(css::eAuthorSheetFeatures),
    mWebkitBoxUnprefixState(eNotParsingDecls),
    mNextFree(nullptr)
{
}

CSSParserImpl::~CSSParserImpl()
{
  mData.AssertInitialState();
  mTempData.AssertInitialState();
}

nsresult
CSSParserImpl::SetStyleSheet(CSSStyleSheet* aSheet)
{
  if (aSheet != mSheet) {
    // Switch to using the new sheet, if any
    mGroupStack.Clear();
    mSheet = aSheet;
    if (mSheet) {
      mNameSpaceMap = mSheet->GetNameSpaceMap();
    } else {
      mNameSpaceMap = nullptr;
    }
  } else if (mSheet) {
    mNameSpaceMap = mSheet->GetNameSpaceMap();
  }

  return NS_OK;
}

nsIDocument*
CSSParserImpl::GetDocument()
{
  if (!mSheet) {
    return nullptr;
  }
  return mSheet->GetAssociatedDocument();
}

nsresult
CSSParserImpl::SetQuirkMode(bool aQuirkMode)
{
  mNavQuirkMode = aQuirkMode;
  return NS_OK;
}

nsresult
CSSParserImpl::SetChildLoader(mozilla::css::Loader* aChildLoader)
{
  mChildLoader = aChildLoader;  // not ref counted, it owns us
  return NS_OK;
}

void
CSSParserImpl::Reset()
{
  NS_ASSERTION(!mScanner, "resetting with scanner active");
  SetStyleSheet(nullptr);
  SetQuirkMode(false);
  SetChildLoader(nullptr);
}

void
CSSParserImpl::InitScanner(nsCSSScanner& aScanner,
                           css::ErrorReporter& aReporter,
                           nsIURI* aSheetURI, nsIURI* aBaseURI,
                           nsIPrincipal* aSheetPrincipal,
                           dom::CallerType aCallerType)
{
  NS_PRECONDITION(!mParsingCompoundProperty, "Bad initial state");
  NS_PRECONDITION(!mScanner, "already have scanner");

  mScanner = &aScanner;
  mReporter = &aReporter;
  mScanner->SetErrorReporter(mReporter);

  mBaseURI = aBaseURI;
  mSheetURI = aSheetURI;
  mSheetPrincipal = aSheetPrincipal;
  mHavePushBack = false;
  mIsChrome =
    aCallerType == dom::CallerType::System ||
    (aSheetURI && dom::IsChromeURI(aSheetURI));
}

void
CSSParserImpl::ReleaseScanner()
{
  mScanner = nullptr;
  mReporter = nullptr;
  mBaseURI = nullptr;
  mSheetURI = nullptr;
  mSheetPrincipal = nullptr;
  mIsChrome = false;
}

nsresult
CSSParserImpl::ParseSheet(const nsAString& aInput,
                          nsIURI*          aSheetURI,
                          nsIURI*          aBaseURI,
                          nsIPrincipal*    aSheetPrincipal,
                          uint32_t         aLineNumber,
                          css::LoaderReusableStyleSheets* aReusableSheets)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");
  NS_PRECONDITION(aSheetURI, "need sheet URI");
  NS_PRECONDITION(mSheet, "Must have sheet to parse into");
  NS_ENSURE_STATE(mSheet);

#ifdef DEBUG
  nsIURI* uri = mSheet->GetSheetURI();
  bool equal;
  NS_ASSERTION(NS_SUCCEEDED(aSheetURI->Equals(uri, &equal)) && equal,
               "Sheet URI does not match passed URI");
  NS_ASSERTION(NS_SUCCEEDED(mSheet->Principal()->Equals(aSheetPrincipal,
                                                        &equal)) &&
               equal,
               "Sheet principal does not match passed principal");
#endif

  nsCSSScanner scanner(aInput, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);

  int32_t ruleCount = mSheet->StyleRuleCount();
  if (0 < ruleCount) {
    const css::Rule* lastRule = mSheet->GetStyleRuleAt(ruleCount - 1);
    if (lastRule) {
      switch (lastRule->GetType()) {
        case css::Rule::CHARSET_RULE:
        case css::Rule::IMPORT_RULE:
          mSection = eCSSSection_Import;
          break;
        case css::Rule::NAMESPACE_RULE:
          mSection = eCSSSection_NameSpace;
          break;
        default:
          mSection = eCSSSection_General;
          break;
      }
    }
  }
  else {
    mSection = eCSSSection_Charset; // sheet is empty, any rules are fair
  }

  mParsingMode = mSheet->ParsingMode();
  mReusableSheets = aReusableSheets;

  nsCSSToken* tk = &mToken;
  for (;;) {
    // Get next non-whitespace token
    if (!GetToken(true)) {
      OUTPUT_ERROR();
      break;
    }
    if (eCSSToken_HTMLComment == tk->mType) {
      continue; // legal here only
    }
    if (eCSSToken_AtKeyword == tk->mType) {
      ParseAtRule(AppendRuleToSheet, this, false);
      continue;
    }
    UngetToken();
    if (ParseRuleSet(AppendRuleToSheet, this)) {
      mSection = eCSSSection_General;
    }
  }

  mSheet->SetSourceMapURLFromComment(scanner.GetSourceMapURL());
  mSheet->SetSourceURL(scanner.GetSourceURL());
  ReleaseScanner();

  mParsingMode = css::eAuthorSheetFeatures;
  mReusableSheets = nullptr;

  return NS_OK;
}

/**
 * Determines whether the identifier contained in the given string is a
 * vendor-specific identifier, as described in CSS 2.1 section 4.1.2.1.
 */
static bool
NonMozillaVendorIdentifier(const nsAString& ident)
{
  return (ident.First() == char16_t('-') &&
          !StringBeginsWith(ident, NS_LITERAL_STRING("-moz-"))) ||
         ident.First() == char16_t('_');

}

already_AddRefed<css::Declaration>
CSSParserImpl::ParseStyleAttribute(const nsAString& aAttributeValue,
                                   nsIURI*          aDocURI,
                                   nsIURI*          aBaseURI,
                                   nsIPrincipal*    aNodePrincipal)
{
  NS_PRECONDITION(aNodePrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");

  // XXX line number?
  nsCSSScanner scanner(aAttributeValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURI);
  InitScanner(scanner, reporter, aDocURI, aBaseURI, aNodePrincipal);

  mSection = eCSSSection_General;

  uint32_t parseFlags = eParseDeclaration_AllowImportant;

  RefPtr<css::Declaration> declaration = ParseDeclarationBlock(parseFlags);

  ReleaseScanner();

  return declaration.forget();
}

nsresult
CSSParserImpl::ParseDeclarations(const nsAString&  aBuffer,
                                 nsIURI*           aSheetURI,
                                 nsIURI*           aBaseURI,
                                 nsIPrincipal*     aSheetPrincipal,
                                 css::Declaration* aDeclaration,
                                 bool*           aChanged)
{
  *aChanged = false;

  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");

  nsCSSScanner scanner(aBuffer, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);

  MOZ_ASSERT(mWebkitBoxUnprefixState == eNotParsingDecls,
             "Someone forgot to clear mWebkitBoxUnprefixState!");
  AutoRestore<WebkitBoxUnprefixState> autoRestore(mWebkitBoxUnprefixState);
  mWebkitBoxUnprefixState = eHaveNotUnprefixed;

  mSection = eCSSSection_General;

  mData.AssertInitialState();
  aDeclaration->ClearData();
  // We could check if it was already empty, but...
  *aChanged = true;

  for (;;) {
    // If we cleared the old decl, then we want to be calling
    // ValueAppended as we parse.
    if (!ParseDeclaration(aDeclaration, eParseDeclaration_AllowImportant,
                          true, aChanged)) {
      if (!SkipDeclaration(false)) {
        break;
      }
    }
  }

  aDeclaration->CompressFrom(&mData);
  ReleaseScanner();
  return NS_OK;
}

nsresult
CSSParserImpl::ParseRule(const nsAString&        aRule,
                         nsIURI*                 aSheetURI,
                         nsIURI*                 aBaseURI,
                         nsIPrincipal*           aSheetPrincipal,
                         css::Rule**             aResult)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");

  *aResult = nullptr;

  nsCSSScanner scanner(aRule, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);

  mSection = eCSSSection_Charset; // callers are responsible for rejecting invalid rules.

  nsCSSToken* tk = &mToken;
  // Get first non-whitespace token
  nsresult rv = NS_OK;
  if (!GetToken(true)) {
    REPORT_UNEXPECTED(PEParseRuleWSOnly);
    OUTPUT_ERROR();
    rv = NS_ERROR_DOM_SYNTAX_ERR;
  } else {
    if (eCSSToken_AtKeyword == tk->mType) {
      // FIXME: perhaps aInsideBlock should be true when we are?
      ParseAtRule(AssignRuleToPointer, aResult, false);
    } else {
      UngetToken();
      ParseRuleSet(AssignRuleToPointer, aResult);
    }

    if (*aResult && GetToken(true)) {
      // garbage after rule
      REPORT_UNEXPECTED_TOKEN(PERuleTrailing);
      NS_RELEASE(*aResult);
    }

    if (!*aResult) {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
      OUTPUT_ERROR();
    }
  }

  ReleaseScanner();
  return rv;
}

void
CSSParserImpl::ParseLonghandProperty(const nsCSSPropertyID aPropID,
                                     const nsAString& aPropValue,
                                     nsIURI* aSheetURL,
                                     nsIURI* aBaseURL,
                                     nsIPrincipal* aSheetPrincipal,
                                     nsCSSValue& aValue)
{
  MOZ_ASSERT(aPropID < eCSSProperty_COUNT_no_shorthands,
             "ParseLonghandProperty must only take a longhand property");

  RefPtr<css::Declaration> declaration = new css::Declaration;
  declaration->InitializeEmpty();

  bool changed;
  ParseProperty(aPropID, aPropValue, aSheetURL, aBaseURL, aSheetPrincipal,
                declaration, &changed,
                /* aIsImportant */ false,
                /* aIsSVGMode */ false);

  if (changed) {
    aValue = *declaration->GetNormalBlock()->ValueFor(aPropID);
  } else {
    aValue.Reset();
  }
}

bool
CSSParserImpl::ParseTransformProperty(const nsAString& aPropValue,
                                      bool aDisallowRelativeValues,
                                      nsCSSValue& aValue)
{
  RefPtr<css::Declaration> declaration = new css::Declaration();
  declaration->InitializeEmpty();

  mData.AssertInitialState();
  mTempData.AssertInitialState();

  nsCSSScanner scanner(aPropValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, nullptr);
  InitScanner(scanner, reporter, nullptr, nullptr, nullptr);

  bool parsedOK = ParseTransform(false, eCSSProperty_transform,
                                 aDisallowRelativeValues);
  // We should now be at EOF
  if (parsedOK && GetToken(true)) {
    parsedOK = false;
    mTempData.ClearProperty(eCSSProperty_transform);
  }

  bool changed = false;
  if (parsedOK) {
    declaration->ExpandTo(&mData);
    changed = mData.TransferFromBlock(mTempData, eCSSProperty_transform,
                                      EnabledState(), false,
                                      true, false, declaration,
                                      GetDocument());
    declaration->CompressFrom(&mData);
  }

  if (changed) {
    aValue = *declaration->GetNormalBlock()->ValueFor(eCSSProperty_transform);
  } else {
    aValue.Reset();
  }

  mTempData.AssertInitialState();
  ReleaseScanner();

  return parsedOK;
}

void
CSSParserImpl::ParseProperty(const nsCSSPropertyID aPropID,
                             const nsAString& aPropValue,
                             nsIURI* aSheetURI,
                             nsIURI* aBaseURI,
                             nsIPrincipal* aSheetPrincipal,
                             css::Declaration* aDeclaration,
                             bool* aChanged,
                             bool aIsImportant,
                             bool aIsSVGMode)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");
  NS_PRECONDITION(aDeclaration, "Need declaration to parse into!");
  MOZ_ASSERT(aPropID != eCSSPropertyExtra_variable);

  mData.AssertInitialState();
  mTempData.AssertInitialState();
  aDeclaration->AssertMutable();

  nsCSSScanner scanner(aPropValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);
  mSection = eCSSSection_General;

  *aChanged = false;

  // Check for unknown or preffed off properties
  if (eCSSProperty_UNKNOWN == aPropID ||
      !nsCSSProps::IsEnabled(aPropID, EnabledState())) {
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    REPORT_UNEXPECTED_P(PEUnknownProperty, propName);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ReleaseScanner();
    return;
  }

  mIsSVGMode = aIsSVGMode;
  bool parsedOK = ParseProperty(aPropID);
  // We should now be at EOF
  if (parsedOK && GetToken(true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
    parsedOK = false;
  }

  if (!parsedOK) {
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    REPORT_UNEXPECTED_P(PEValueParsingError, propName);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    mTempData.ClearProperty(aPropID);
  } else {

    // We know we don't need to force a ValueAppended call for the new
    // value.  So if we are not processing a shorthand, and there's
    // already a value for this property in the declaration at the
    // same importance level, then we can just copy our parsed value
    // directly into the declaration without going through the whole
    // expand/compress thing.
    if (!aDeclaration->TryReplaceValue(aPropID, aIsImportant, mTempData,
                                       aChanged)) {
      // Do it the slow way
      aDeclaration->ExpandTo(&mData);
      *aChanged = mData.TransferFromBlock(mTempData, aPropID,
                                          EnabledState(), aIsImportant,
                                          true, false, aDeclaration,
                                          GetDocument());
      aDeclaration->CompressFrom(&mData);
    }
    CLEAR_ERROR();
  }

  mTempData.AssertInitialState();
  mIsSVGMode = false;

  ReleaseScanner();
}

void
CSSParserImpl::ParseVariable(const nsAString& aVariableName,
                             const nsAString& aPropValue,
                             nsIURI* aSheetURI,
                             nsIURI* aBaseURI,
                             nsIPrincipal* aSheetPrincipal,
                             css::Declaration* aDeclaration,
                             bool* aChanged,
                             bool aIsImportant)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");
  NS_PRECONDITION(aDeclaration, "Need declaration to parse into!");

  mData.AssertInitialState();
  mTempData.AssertInitialState();
  aDeclaration->AssertMutable();

  nsCSSScanner scanner(aPropValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);
  mSection = eCSSSection_General;

  *aChanged = false;

  CSSVariableDeclarations::Type variableType;
  nsString variableValue;

  bool parsedOK = ParseVariableDeclaration(&variableType, variableValue);

  // We should now be at EOF
  if (parsedOK && GetToken(true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
    parsedOK = false;
  }

  if (!parsedOK) {
    REPORT_UNEXPECTED_P(PEValueParsingError, NS_LITERAL_STRING("--") +
                                             aVariableName);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
  } else {
    CLEAR_ERROR();
    aDeclaration->AddVariable(aVariableName, variableType,
                              variableValue, aIsImportant, true);
    *aChanged = true;
  }

  mTempData.AssertInitialState();

  ReleaseScanner();
}

void
CSSParserImpl::ParseMediaList(const nsAString& aBuffer,
                              nsIURI* aURI, // for error reporting
                              uint32_t aLineNumber, // for error reporting
                              nsMediaList* aMediaList,
                              dom::CallerType aCallerType)
{
  // XXX Are there cases where the caller wants to keep what it already
  // has in case of parser error?  If GatherMedia ever changes to return
  // a value other than true, we probably should avoid modifying aMediaList.
  aMediaList->Clear();

  // fake base URI since media lists don't have URIs in them
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr, aCallerType);

  DebugOnly<bool> parsedOK = GatherMedia(aMediaList, false);
  NS_ASSERTION(parsedOK, "GatherMedia returned false; we probably want to avoid "
                         "trashing aMediaList");

  CLEAR_ERROR();
  ReleaseScanner();
}

//  <source-size-list> = <source-size>#?
//  <source-size> = <media-condition>? <length>
bool
CSSParserImpl::ParseSourceSizeList(const nsAString& aBuffer,
                                   nsIURI* aURI, // for error reporting
                                   uint32_t aLineNumber, // for error reporting
                                   InfallibleTArray< nsAutoPtr<nsMediaQuery> >& aQueries,
                                   InfallibleTArray<nsCSSValue>& aValues)
{
  aQueries.Clear();
  aValues.Clear();

  // fake base URI since media value lists don't have URIs in them
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  // https://html.spec.whatwg.org/multipage/embedded-content.html#parse-a-sizes-attribute
  bool hitEnd = false;
  do {
    bool hitError = false;
    // Parse single <media-condition> <source-size-value>
    do {
      nsAutoPtr<nsMediaQuery> query;
      nsCSSValue value;

      bool hitStop;
      if (!ParseMediaQuery(eMediaQuerySingleCondition, getter_Transfers(query),
                           &hitStop)) {
        NS_ASSERTION(!hitStop, "should return true when hit stop");
        hitError = true;
        break;
      }

      if (!query) {
        REPORT_UNEXPECTED_EOF(PEParseSourceSizeListEOF);
        NS_ASSERTION(hitStop,
                     "should return hitStop or an error if returning no query");
        hitError = true;
        break;
      }

      if (hitStop) {
        // Empty conditions (e.g. just a bare value) should be treated as always
        // matching (a query with no expressions fails to match, so a negated one
        // always matches.)
        query->SetNegated();
      }

      // https://html.spec.whatwg.org/multipage/embedded-content.html#source-size-value
      // Percentages are not allowed in a <source-size-value>, to avoid
      // confusion about what it would be relative to.
      if (ParseNonNegativeVariant(value, VARIANT_LCALC, nullptr) !=
          CSSParseResult::Ok) {
        hitError = true;
        break;
      }

      if (GetToken(true)) {
        if (!mToken.IsSymbol(',')) {
          REPORT_UNEXPECTED_TOKEN(PEParseSourceSizeListNotComma);
          hitError = true;
          break;
        }
      } else {
        hitEnd = true;
      }

      aQueries.AppendElement(query.forget());
      aValues.AppendElement(value);
    } while(0);

    if (hitError) {
      OUTPUT_ERROR();

      // Per spec, we just skip the current entry if there was a parse error.
      // Jumps to next entry of <source-size-list> which is a comma-separated list.
      if (!SkipUntil(',')) {
        hitEnd = true;
      }
    }
  } while (!hitEnd);

  CLEAR_ERROR();
  ReleaseScanner();

  return !aQueries.IsEmpty();
}

bool
CSSParserImpl::ParseColorString(const nsAString& aBuffer,
                                nsIURI* aURI, // for error reporting
                                uint32_t aLineNumber, // for error reporting
                                nsCSSValue& aValue,
                                bool aSuppressErrors /* false */)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  nsAutoSuppressErrors suppressErrors(this, aSuppressErrors);

  // Parse a color, and check that there's nothing else after it.
  bool colorParsed = ParseColor(aValue) == CSSParseResult::Ok &&
                     !GetToken(true);

  if (aSuppressErrors) {
    CLEAR_ERROR();
  } else {
    OUTPUT_ERROR();
  }

  ReleaseScanner();
  return colorParsed;
}

bool
CSSParserImpl::ParseMarginString(const nsAString& aBuffer,
                                 nsIURI* aURI, // for error reporting
                                 uint32_t aLineNumber, // for error reporting
                                 nsCSSValue& aValue,
                                 bool aSuppressErrors /* false */)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  nsAutoSuppressErrors suppressErrors(this, aSuppressErrors);

  // Parse a margin, and check that there's nothing else after it.
  bool marginParsed = ParseGroupedBoxProperty(VARIANT_LP, aValue, 0) && !GetToken(true);

  if (aSuppressErrors) {
    CLEAR_ERROR();
  } else {
    OUTPUT_ERROR();
  }

  ReleaseScanner();
  return marginParsed;
}

bool
CSSParserImpl::ParseFontFamilyListString(const nsAString& aBuffer,
                                         nsIURI* aURI, // for error reporting
                                         uint32_t aLineNumber, // for error reporting
                                         nsCSSValue& aValue)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  // Parse a font family list, and check that there's nothing else after it.
  bool familyParsed = ParseFamily(aValue) && !GetToken(true);
  OUTPUT_ERROR();
  ReleaseScanner();
  return familyParsed;
}

nsresult
CSSParserImpl::ParseSelectorString(const nsAString& aSelectorString,
                                   nsIURI* aURI, // for error reporting
                                   uint32_t aLineNumber, // for error reporting
                                   nsCSSSelectorList **aSelectorList)
{
  nsCSSScanner scanner(aSelectorString, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  bool success = ParseSelectorList(*aSelectorList, char16_t(0));

  // We deliberately do not call OUTPUT_ERROR here, because all our
  // callers map a failure return to a JS exception, and if that JS
  // exception is caught, people don't want to see parser diagnostics;
  // see e.g. http://bugs.jquery.com/ticket/7535
  // It would be nice to be able to save the parser diagnostics into
  // the exception, so that if it _isn't_ caught we can report them
  // along with the usual uncaught-exception message, but we don't
  // have any way to do that at present; see bug 631621.
  CLEAR_ERROR();
  ReleaseScanner();

  if (success) {
    NS_ASSERTION(*aSelectorList, "Should have list!");
    return NS_OK;
  }

  NS_ASSERTION(!*aSelectorList, "Shouldn't have list!");

  return NS_ERROR_DOM_SYNTAX_ERR;
}


already_AddRefed<nsCSSKeyframeRule>
CSSParserImpl::ParseKeyframeRule(const nsAString&  aBuffer,
                                 nsIURI*             aURI,
                                 uint32_t            aLineNumber)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  RefPtr<nsCSSKeyframeRule> result = ParseKeyframeRule();
  if (GetToken(true)) {
    // extra garbage at the end
    result = nullptr;
  }

  OUTPUT_ERROR();
  ReleaseScanner();

  return result.forget();
}

bool
CSSParserImpl::ParseKeyframeSelectorString(const nsAString& aSelectorString,
                                           nsIURI* aURI, // for error reporting
                                           uint32_t aLineNumber, // for error reporting
                                           InfallibleTArray<float>& aSelectorList)
{
  MOZ_ASSERT(aSelectorList.IsEmpty(), "given list should start empty");

  nsCSSScanner scanner(aSelectorString, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  bool success = ParseKeyframeSelectorList(aSelectorList) &&
                 // must consume entire input string
                 !GetToken(true);

  OUTPUT_ERROR();
  ReleaseScanner();

  if (success) {
    NS_ASSERTION(!aSelectorList.IsEmpty(), "should not be empty");
  } else {
    aSelectorList.Clear();
  }

  return success;
}

bool
CSSParserImpl::EvaluateSupportsDeclaration(const nsAString& aProperty,
                                           const nsAString& aValue,
                                           nsIURI* aDocURL,
                                           nsIURI* aBaseURL,
                                           nsIPrincipal* aDocPrincipal)
{
  nsCSSPropertyID propID = LookupEnabledProperty(aProperty);
  if (propID == eCSSProperty_UNKNOWN) {
    return false;
  }

  nsCSSScanner scanner(aValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURL);
  InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);
  nsAutoSuppressErrors suppressErrors(this);

  bool parsedOK;

  if (propID == eCSSPropertyExtra_variable) {
    MOZ_ASSERT(Substring(aProperty, 0,
                         CSS_CUSTOM_NAME_PREFIX_LENGTH).EqualsLiteral("--"));
    const nsDependentSubstring varName =
      Substring(aProperty, CSS_CUSTOM_NAME_PREFIX_LENGTH);  // remove '--'
    CSSVariableDeclarations::Type variableType;
    nsString variableValue;
    parsedOK = ParseVariableDeclaration(&variableType, variableValue) &&
               !GetToken(true);
  } else {
    parsedOK = ParseProperty(propID) && !GetToken(true);

    mTempData.ClearProperty(propID);
    mTempData.AssertInitialState();
  }

  CLEAR_ERROR();
  ReleaseScanner();

  return parsedOK;
}

bool
CSSParserImpl::EvaluateSupportsCondition(const nsAString& aDeclaration,
                                         nsIURI* aDocURL,
                                         nsIURI* aBaseURL,
                                         nsIPrincipal* aDocPrincipal,
                                         SupportsParsingSettings aSettings)
{
  nsCSSScanner scanner(aDeclaration, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURL);
  InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);
  nsAutoSuppressErrors suppressErrors(this);

  bool conditionMet;
  bool parsedOK;

  if (aSettings == SupportsParsingSettings::ImpliedParentheses) {
    parsedOK = ParseSupportsConditionInParensInsideParens(conditionMet) && !GetToken(true);
  } else {
    parsedOK = ParseSupportsCondition(conditionMet) && !GetToken(true);
  }

  CLEAR_ERROR();
  ReleaseScanner();

  return parsedOK && conditionMet;
}

bool
CSSParserImpl::EnumerateVariableReferences(const nsAString& aPropertyValue,
                                           VariableEnumFunc aFunc,
                                           void* aData)
{
  nsCSSScanner scanner(aPropertyValue, 0);
  css::ErrorReporter reporter(scanner, nullptr, nullptr, nullptr);
  InitScanner(scanner, reporter, nullptr, nullptr, nullptr);
  nsAutoSuppressErrors suppressErrors(this);

  CSSVariableDeclarations::Type type;
  bool dropBackslash;
  nsString impliedCharacters;
  bool result = ParseValueWithVariables(&type, &dropBackslash,
                                        impliedCharacters, aFunc, aData) &&
                !GetToken(true);

  ReleaseScanner();

  return result;
}

static bool
SeparatorRequiredBetweenTokens(nsCSSTokenSerializationType aToken1,
                               nsCSSTokenSerializationType aToken2)
{
  // The two lines marked with (*) do not correspond to entries in
  // the table in the css-syntax spec but which we need to handle,
  // as we treat them as whole tokens.
  switch (aToken1) {
    case eCSSTokenSerialization_Ident:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_URL_or_BadURL ||
             aToken2 == eCSSTokenSerialization_Symbol_Minus ||
             aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension ||
             aToken2 == eCSSTokenSerialization_URange ||
             aToken2 == eCSSTokenSerialization_CDC ||
             aToken2 == eCSSTokenSerialization_Symbol_OpenParen;
    case eCSSTokenSerialization_AtKeyword_or_Hash:
    case eCSSTokenSerialization_Dimension:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_URL_or_BadURL ||
             aToken2 == eCSSTokenSerialization_Symbol_Minus ||
             aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension ||
             aToken2 == eCSSTokenSerialization_URange ||
             aToken2 == eCSSTokenSerialization_CDC;
    case eCSSTokenSerialization_Symbol_Hash:
    case eCSSTokenSerialization_Symbol_Minus:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_URL_or_BadURL ||
             aToken2 == eCSSTokenSerialization_Symbol_Minus ||
             aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension ||
             aToken2 == eCSSTokenSerialization_URange;
    case eCSSTokenSerialization_Number:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_URL_or_BadURL ||
             aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension ||
             aToken2 == eCSSTokenSerialization_URange;
    case eCSSTokenSerialization_Symbol_At:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_URL_or_BadURL ||
             aToken2 == eCSSTokenSerialization_Symbol_Minus ||
             aToken2 == eCSSTokenSerialization_URange;
    case eCSSTokenSerialization_URange:
      return aToken2 == eCSSTokenSerialization_Ident ||
             aToken2 == eCSSTokenSerialization_Function ||
             aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension ||
             aToken2 == eCSSTokenSerialization_Symbol_Question;
    case eCSSTokenSerialization_Symbol_Dot_or_Plus:
      return aToken2 == eCSSTokenSerialization_Number ||
             aToken2 == eCSSTokenSerialization_Percentage ||
             aToken2 == eCSSTokenSerialization_Dimension;
    case eCSSTokenSerialization_Symbol_Assorted:
    case eCSSTokenSerialization_Symbol_Asterisk:
      return aToken2 == eCSSTokenSerialization_Symbol_Equals;
    case eCSSTokenSerialization_Symbol_Bar:
      return aToken2 == eCSSTokenSerialization_Symbol_Equals ||
             aToken2 == eCSSTokenSerialization_Symbol_Bar ||
             aToken2 == eCSSTokenSerialization_DashMatch;              // (*)
    case eCSSTokenSerialization_Symbol_Slash:
      return aToken2 == eCSSTokenSerialization_Symbol_Asterisk ||
             aToken2 == eCSSTokenSerialization_ContainsMatch;          // (*)
    default:
      MOZ_ASSERT(aToken1 == eCSSTokenSerialization_Nothing ||
                 aToken1 == eCSSTokenSerialization_Whitespace ||
                 aToken1 == eCSSTokenSerialization_Percentage ||
                 aToken1 == eCSSTokenSerialization_URL_or_BadURL ||
                 aToken1 == eCSSTokenSerialization_Function ||
                 aToken1 == eCSSTokenSerialization_CDC ||
                 aToken1 == eCSSTokenSerialization_Symbol_OpenParen ||
                 aToken1 == eCSSTokenSerialization_Symbol_Question ||
                 aToken1 == eCSSTokenSerialization_Symbol_Assorted ||
                 aToken1 == eCSSTokenSerialization_Symbol_Asterisk ||
                 aToken1 == eCSSTokenSerialization_Symbol_Equals ||
                 aToken1 == eCSSTokenSerialization_Symbol_Bar ||
                 aToken1 == eCSSTokenSerialization_Symbol_Slash ||
                 aToken1 == eCSSTokenSerialization_Other,
                 "unexpected nsCSSTokenSerializationType value");
      return false;
  }
}

/**
 * Appends aValue to aResult, possibly inserting an empty CSS
 * comment between the two to ensure that tokens from both strings
 * remain separated.
 */
static void
AppendTokens(nsAString& aResult,
             nsCSSTokenSerializationType& aResultFirstToken,
             nsCSSTokenSerializationType& aResultLastToken,
             nsCSSTokenSerializationType aValueFirstToken,
             nsCSSTokenSerializationType aValueLastToken,
             const nsAString& aValue)
{
  if (SeparatorRequiredBetweenTokens(aResultLastToken, aValueFirstToken)) {
    aResult.AppendLiteral("/**/");
  }
  aResult.Append(aValue);
  if (aResultFirstToken == eCSSTokenSerialization_Nothing) {
    aResultFirstToken = aValueFirstToken;
  }
  if (aValueLastToken != eCSSTokenSerialization_Nothing) {
    aResultLastToken = aValueLastToken;
  }
}

/**
 * Stops the given scanner recording, and appends the recorded result
 * to aResult, possibly inserting an empty CSS comment between the two to
 * ensure that tokens from both strings remain separated.
 */
static void
StopRecordingAndAppendTokens(nsString& aResult,
                             nsCSSTokenSerializationType& aResultFirstToken,
                             nsCSSTokenSerializationType& aResultLastToken,
                             nsCSSTokenSerializationType aValueFirstToken,
                             nsCSSTokenSerializationType aValueLastToken,
                             nsCSSScanner* aScanner)
{
  if (SeparatorRequiredBetweenTokens(aResultLastToken, aValueFirstToken)) {
    aResult.AppendLiteral("/**/");
  }
  aScanner->StopRecording(aResult);
  if (aResultFirstToken == eCSSTokenSerialization_Nothing) {
    aResultFirstToken = aValueFirstToken;
  }
  if (aValueLastToken != eCSSTokenSerialization_Nothing) {
    aResultLastToken = aValueLastToken;
  }
}

bool
CSSParserImpl::ResolveValueWithVariableReferencesRec(
                                     nsString& aResult,
                                     nsCSSTokenSerializationType& aResultFirstToken,
                                     nsCSSTokenSerializationType& aResultLastToken,
                                     const CSSVariableValues* aVariables)
{
  // This function assumes we are already recording, and will leave the scanner
  // recording when it returns.
  MOZ_ASSERT(mScanner->IsRecording());
  MOZ_ASSERT(aResult.IsEmpty());

  // Stack of closing characters for currently open constructs.
  AutoTArray<char16_t, 16> stack;

  // The resolved value for this ResolveValueWithVariableReferencesRec call.
  nsString value;

  // The length of the scanner's recording before the currently parsed token.
  // This is used so that when we encounter a "var(" token, we can strip
  // it off the end of the recording, regardless of how long the token was.
  // (With escapes, it could be longer than four characters.)
  uint32_t lengthBeforeVar = 0;

  // Tracking the type of token that appears at the start and end of |value|
  // and that appears at the start and end of the scanner recording.  These are
  // used to determine whether we need to insert "/**/" when pasting token
  // streams together.
  nsCSSTokenSerializationType valueFirstToken = eCSSTokenSerialization_Nothing,
                              valueLastToken  = eCSSTokenSerialization_Nothing,
                              recFirstToken   = eCSSTokenSerialization_Nothing,
                              recLastToken    = eCSSTokenSerialization_Nothing;

#define UPDATE_RECORDING_TOKENS(type)                    \
  if (recFirstToken == eCSSTokenSerialization_Nothing) { \
    recFirstToken = type;                                \
  }                                                      \
  recLastToken = type;

  while (GetToken(false)) {
    switch (mToken.mType) {
      case eCSSToken_Symbol: {
        nsCSSTokenSerializationType type = eCSSTokenSerialization_Other;
        if (mToken.mSymbol == '(') {
          stack.AppendElement(')');
          type = eCSSTokenSerialization_Symbol_OpenParen;
        } else if (mToken.mSymbol == '[') {
          stack.AppendElement(']');
        } else if (mToken.mSymbol == '{') {
          stack.AppendElement('}');
        } else if (mToken.mSymbol == ';') {
          if (stack.IsEmpty()) {
            // A ';' that is at the top level of the value or at the top level
            // of a variable reference's fallback is invalid.
            return false;
          }
        } else if (mToken.mSymbol == '!') {
          if (stack.IsEmpty()) {
            // An '!' that is at the top level of the value or at the top level
            // of a variable reference's fallback is invalid.
            return false;
          }
        } else if (mToken.mSymbol == ')' &&
                   stack.IsEmpty()) {
          // We're closing a "var(".
          nsString finalTokens;
          mScanner->StopRecording(finalTokens);
          MOZ_ASSERT(finalTokens[finalTokens.Length() - 1] == ')');
          finalTokens.Truncate(finalTokens.Length() - 1);
          aResult.Append(value);

          AppendTokens(aResult, valueFirstToken, valueLastToken,
                       recFirstToken, recLastToken, finalTokens);

          mScanner->StartRecording();
          UngetToken();
          aResultFirstToken = valueFirstToken;
          aResultLastToken = valueLastToken;
          return true;
        } else if (mToken.mSymbol == ')' ||
                   mToken.mSymbol == ']' ||
                   mToken.mSymbol == '}') {
          if (stack.IsEmpty() ||
              stack.LastElement() != mToken.mSymbol) {
            // A mismatched closing bracket is invalid.
            return false;
          }
          stack.TruncateLength(stack.Length() - 1);
        } else if (mToken.mSymbol == '#') {
          type = eCSSTokenSerialization_Symbol_Hash;
        } else if (mToken.mSymbol == '@') {
          type = eCSSTokenSerialization_Symbol_At;
        } else if (mToken.mSymbol == '.' ||
                   mToken.mSymbol == '+') {
          type = eCSSTokenSerialization_Symbol_Dot_or_Plus;
        } else if (mToken.mSymbol == '-') {
          type = eCSSTokenSerialization_Symbol_Minus;
        } else if (mToken.mSymbol == '?') {
          type = eCSSTokenSerialization_Symbol_Question;
        } else if (mToken.mSymbol == '$' ||
                   mToken.mSymbol == '^' ||
                   mToken.mSymbol == '~') {
          type = eCSSTokenSerialization_Symbol_Assorted;
        } else if (mToken.mSymbol == '=') {
          type = eCSSTokenSerialization_Symbol_Equals;
        } else if (mToken.mSymbol == '|') {
          type = eCSSTokenSerialization_Symbol_Bar;
        } else if (mToken.mSymbol == '/') {
          type = eCSSTokenSerialization_Symbol_Slash;
        } else if (mToken.mSymbol == '*') {
          type = eCSSTokenSerialization_Symbol_Asterisk;
        }
        UPDATE_RECORDING_TOKENS(type);
        break;
      }

      case eCSSToken_Function:
        if (mToken.mIdent.LowerCaseEqualsLiteral("var")) {
          // Save the tokens before the "var(" to our resolved value.
          nsString recording;
          mScanner->StopRecording(recording);
          recording.Truncate(lengthBeforeVar);
          AppendTokens(value, valueFirstToken, valueLastToken,
                       recFirstToken, recLastToken, recording);
          recFirstToken = eCSSTokenSerialization_Nothing;
          recLastToken = eCSSTokenSerialization_Nothing;

          if (!GetToken(true) ||
              mToken.mType != eCSSToken_Ident ||
              !nsCSSProps::IsCustomPropertyName(mToken.mIdent)) {
            // "var(" must be followed by an identifier, and it must be a
            // custom property name.
            return false;
          }

          // Turn the custom property name into a variable name by removing the
          // '--' prefix.
          MOZ_ASSERT(Substring(mToken.mIdent, 0,
                               CSS_CUSTOM_NAME_PREFIX_LENGTH).
                       EqualsLiteral("--"));
          nsDependentString variableName(mToken.mIdent,
                                         CSS_CUSTOM_NAME_PREFIX_LENGTH);

          // Get the value of the identified variable.  Note that we
          // check if the variable value is the empty string, as that means
          // that the variable was invalid at computed value time due to
          // unresolveable variable references or cycles.
          nsString variableValue;
          nsCSSTokenSerializationType varFirstToken, varLastToken;
          bool valid = aVariables->Get(variableName, variableValue,
                                       varFirstToken, varLastToken) &&
                       !variableValue.IsEmpty();

          if (!GetToken(true) ||
              mToken.IsSymbol(')')) {
            mScanner->StartRecording();
            if (!valid) {
              // Invalid variable with no fallback.
              return false;
            }
            // Valid variable with no fallback.
            AppendTokens(value, valueFirstToken, valueLastToken,
                         varFirstToken, varLastToken, variableValue);
          } else if (mToken.IsSymbol(',')) {
            mScanner->StartRecording();
            if (!GetToken(false) ||
                mToken.IsSymbol(')')) {
              // Comma must be followed by at least one fallback token.
              return false;
            }
            UngetToken();
            if (valid) {
              // Valid variable with ignored fallback.
              mScanner->StopRecording();
              AppendTokens(value, valueFirstToken, valueLastToken,
                           varFirstToken, varLastToken, variableValue);
              bool ok = SkipBalancedContentUntil(')');
              mScanner->StartRecording();
              if (!ok) {
                return false;
              }
            } else {
              nsString fallback;
              if (!ResolveValueWithVariableReferencesRec(fallback,
                                                         varFirstToken,
                                                         varLastToken,
                                                         aVariables)) {
                // Fallback value had invalid tokens or an invalid variable reference
                // that itself had no fallback.
                return false;
              }
              AppendTokens(value, valueFirstToken, valueLastToken,
                           varFirstToken, varLastToken, fallback);
              // Now we're either at the pushed back ')' that finished the
              // fallback or at EOF.
              DebugOnly<bool> gotToken = GetToken(false);
              MOZ_ASSERT(!gotToken || mToken.IsSymbol(')'));
            }
          } else {
            // Expected ',' or ')' after the variable name.
            mScanner->StartRecording();
            return false;
          }
        } else {
          stack.AppendElement(')');
          UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Function);
        }
        break;

      case eCSSToken_Bad_String:
      case eCSSToken_Bad_URL:
        return false;

      case eCSSToken_Whitespace:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Whitespace);
        break;

      case eCSSToken_AtKeyword:
      case eCSSToken_Hash:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_AtKeyword_or_Hash);
        break;

      case eCSSToken_Number:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Number);
        break;

      case eCSSToken_Dimension:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Dimension);
        break;

      case eCSSToken_Ident:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Ident);
        break;

      case eCSSToken_Percentage:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Percentage);
        break;

      case eCSSToken_URange:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_URange);
        break;

      case eCSSToken_URL:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_URL_or_BadURL);
        break;

      case eCSSToken_HTMLComment:
        if (mToken.mIdent[0] == '-') {
          UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_CDC);
        } else {
          UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Other);
        }
        break;

      case eCSSToken_Dashmatch:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_DashMatch);
        break;

      case eCSSToken_Containsmatch:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_ContainsMatch);
        break;

      default:
        MOZ_FALLTHROUGH_ASSERT("unexpected token type");
      case eCSSToken_ID:
      case eCSSToken_String:
      case eCSSToken_Includes:
      case eCSSToken_Beginsmatch:
      case eCSSToken_Endsmatch:
        UPDATE_RECORDING_TOKENS(eCSSTokenSerialization_Other);
        break;
    }

    lengthBeforeVar = mScanner->RecordingLength();
  }

#undef UPDATE_RECORDING_TOKENS

  aResult.Append(value);
  StopRecordingAndAppendTokens(aResult, valueFirstToken, valueLastToken,
                               recFirstToken, recLastToken, mScanner);

  // Append any implicitly closed brackets.
  if (!stack.IsEmpty()) {
    do {
      aResult.Append(stack.LastElement());
      stack.TruncateLength(stack.Length() - 1);
    } while (!stack.IsEmpty());
    valueLastToken = eCSSTokenSerialization_Other;
  }

  mScanner->StartRecording();
  aResultFirstToken = valueFirstToken;
  aResultLastToken = valueLastToken;
  return true;
}

bool
CSSParserImpl::ResolveValueWithVariableReferences(
                                        const CSSVariableValues* aVariables,
                                        nsString& aResult,
                                        nsCSSTokenSerializationType& aFirstToken,
                                        nsCSSTokenSerializationType& aLastToken)
{
  aResult.Truncate(0);

  // Start recording before we read the first token.
  mScanner->StartRecording();

  if (!GetToken(false)) {
    // Value was empty since we reached EOF.
    mScanner->StopRecording();
    return false;
  }

  UngetToken();

  nsString value;
  nsCSSTokenSerializationType firstToken, lastToken;
  bool ok = ResolveValueWithVariableReferencesRec(value, firstToken, lastToken, aVariables) &&
            !GetToken(true);

  mScanner->StopRecording();

  if (ok) {
    aResult = value;
    aFirstToken = firstToken;
    aLastToken = lastToken;
  }
  return ok;
}

bool
CSSParserImpl::ResolveVariableValue(const nsAString& aPropertyValue,
                                    const CSSVariableValues* aVariables,
                                    nsString& aResult,
                                    nsCSSTokenSerializationType& aFirstToken,
                                    nsCSSTokenSerializationType& aLastToken)
{
  nsCSSScanner scanner(aPropertyValue, 0);

  // At this point, we know that aPropertyValue is syntactically correct
  // for a token stream that has variable references.  We also won't be
  // interpreting any of the stream as we parse it, apart from expanding
  // var() references, so we don't need a base URL etc. or any useful
  // error reporting.
  css::ErrorReporter reporter(scanner, nullptr, nullptr, nullptr);
  InitScanner(scanner, reporter, nullptr, nullptr, nullptr);

  bool valid = ResolveValueWithVariableReferences(aVariables, aResult,
                                                  aFirstToken, aLastToken);

  ReleaseScanner();
  return valid;
}

void
CSSParserImpl::ParsePropertyWithVariableReferences(
                                            nsCSSPropertyID aPropertyID,
                                            nsCSSPropertyID aShorthandPropertyID,
                                            const nsAString& aValue,
                                            const CSSVariableValues* aVariables,
                                            nsRuleData* aRuleData,
                                            nsIURI* aDocURL,
                                            nsIURI* aBaseURL,
                                            nsIPrincipal* aDocPrincipal,
                                            CSSStyleSheet* aSheet,
                                            uint32_t aLineNumber,
                                            uint32_t aLineOffset)
{
  mTempData.AssertInitialState();

  bool valid;
  nsString expandedValue;

  // Resolve any variable references in the property value.
  {
    nsCSSScanner scanner(aValue, 0);
    css::ErrorReporter reporter(scanner, aSheet, mChildLoader, aDocURL);
    InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);

    nsCSSTokenSerializationType firstToken, lastToken;
    valid = ResolveValueWithVariableReferences(aVariables, expandedValue,
                                               firstToken, lastToken);
    if (!valid) {
      NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropertyID));
      REPORT_UNEXPECTED(PEInvalidVariableReference);
      REPORT_UNEXPECTED_P(PEValueParsingError, propName);
      if (nsCSSProps::IsInherited(aPropertyID)) {
        REPORT_UNEXPECTED(PEValueWithVariablesFallbackInherit);
      } else {
        REPORT_UNEXPECTED(PEValueWithVariablesFallbackInitial);
      }
      OUTPUT_ERROR_WITH_POSITION(aLineNumber, aLineOffset);
    }
    ReleaseScanner();
  }

  nsCSSPropertyID propertyToParse =
    aShorthandPropertyID != eCSSProperty_UNKNOWN ? aShorthandPropertyID :
                                                   aPropertyID;

  // Parse the property with that resolved value.
  if (valid) {
    nsCSSScanner scanner(expandedValue, 0);
    css::ErrorReporter reporter(scanner, aSheet, mChildLoader, aDocURL);
    InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);
    valid = ParseProperty(propertyToParse);
    if (valid && GetToken(true)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
      valid = false;
    }
    if (!valid) {
      NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(
                                                              propertyToParse));
      REPORT_UNEXPECTED_P_V(PEValueWithVariablesParsingErrorInValue,
                            propName, expandedValue);
      if (nsCSSProps::IsInherited(aPropertyID)) {
        REPORT_UNEXPECTED(PEValueWithVariablesFallbackInherit);
      } else {
        REPORT_UNEXPECTED(PEValueWithVariablesFallbackInitial);
      }
      OUTPUT_ERROR_WITH_POSITION(aLineNumber, aLineOffset);
    }
    ReleaseScanner();
  }

  // If the property could not be parsed with the resolved value, then we
  // treat it as if the value were 'initial' or 'inherit', depending on whether
  // the property is an inherited property.
  if (!valid) {
    nsCSSValue defaultValue;
    if (nsCSSProps::IsInherited(aPropertyID)) {
      defaultValue.SetInheritValue();
    } else {
      defaultValue.SetInitialValue();
    }
    mTempData.AddLonghandProperty(aPropertyID, defaultValue);
  }

  // Copy the property value into the rule data.
  mTempData.MapRuleInfoInto(aPropertyID, aRuleData);

  mTempData.ClearProperty(propertyToParse);
  mTempData.AssertInitialState();
}

already_AddRefed<nsAtom>
CSSParserImpl::ParseCounterStyleName(const nsAString& aBuffer, nsIURI* aURL)
{
  nsCSSScanner scanner(aBuffer, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURL);
  InitScanner(scanner, reporter, aURL, aURL, nullptr);

  RefPtr<nsAtom> name = ParseCounterStyleName(true);
  bool success = name && !GetToken(true);

  OUTPUT_ERROR();
  ReleaseScanner();

  return success ? name.forget() : nullptr;
}

bool
CSSParserImpl::ParseCounterDescriptor(nsCSSCounterDesc aDescID,
                                      const nsAString& aBuffer,
                                      nsIURI* aSheetURL,
                                      nsIURI* aBaseURL,
                                      nsIPrincipal* aSheetPrincipal,
                                      nsCSSValue& aValue)
{
  nsCSSScanner scanner(aBuffer, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURL);
  InitScanner(scanner, reporter, aSheetURL, aBaseURL, aSheetPrincipal);

  bool success = ParseCounterDescriptorValue(aDescID, aValue) &&
                 !GetToken(true);

  OUTPUT_ERROR();
  ReleaseScanner();

  return success;
}

bool
CSSParserImpl::ParseFontFaceDescriptor(nsCSSFontDesc aDescID,
                                       const nsAString& aBuffer,
                                       nsIURI* aSheetURL,
                                       nsIURI* aBaseURL,
                                       nsIPrincipal* aSheetPrincipal,
                                       nsCSSValue& aValue)
{
  nsCSSScanner scanner(aBuffer, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURL);
  InitScanner(scanner, reporter, aSheetURL, aBaseURL, aSheetPrincipal);

  bool success = ParseFontDescriptorValue(aDescID, aValue) &&
                 !GetToken(true);

  OUTPUT_ERROR();
  ReleaseScanner();

  return success;
}

//----------------------------------------------------------------------

bool
CSSParserImpl::GetToken(bool aSkipWS)
{
  if (mHavePushBack) {
    mHavePushBack = false;
    if (!aSkipWS || mToken.mType != eCSSToken_Whitespace) {
      return true;
    }
  }
  return mScanner->Next(mToken, aSkipWS ?
                        eCSSScannerExclude_WhitespaceAndComments :
                        eCSSScannerExclude_Comments);
}

void
CSSParserImpl::UngetToken()
{
  NS_PRECONDITION(!mHavePushBack, "double pushback");
  mHavePushBack = true;
}

bool
CSSParserImpl::GetNextTokenLocation(bool aSkipWS, uint32_t *linenum, uint32_t *colnum)
{
  // Peek at next token so that mScanner updates line and column vals
  if (!GetToken(aSkipWS)) {
    return false;
  }
  UngetToken();
  // The scanner uses one-indexing for line numbers but zero-indexing
  // for column numbers.
  *linenum = mScanner->GetLineNumber();
  *colnum = 1 + mScanner->GetColumnNumber();
  return true;
}

bool
CSSParserImpl::ExpectSymbol(char16_t aSymbol,
                            bool aSkipWS)
{
  if (!GetToken(aSkipWS)) {
    // CSS2.1 specifies that all "open constructs" are to be closed at
    // EOF.  It simplifies higher layers if we claim to have found an
    // ), ], }, or ; if we encounter EOF while looking for one of them.
    // Do still issue a diagnostic, to aid debugging.
    if (aSymbol == ')' || aSymbol == ']' ||
        aSymbol == '}' || aSymbol == ';') {
      REPORT_UNEXPECTED_EOF_CHAR(aSymbol);
      return true;
    }
    else
      return false;
  }
  if (mToken.IsSymbol(aSymbol)) {
    return true;
  }
  UngetToken();
  return false;
}

// Checks to see if we're at the end of a property.  If an error occurs during
// the check, does not signal a parse error.
bool
CSSParserImpl::CheckEndProperty()
{
  if (!GetToken(true)) {
    return true; // properties may end with eof
  }
  if ((eCSSToken_Symbol == mToken.mType) &&
      ((';' == mToken.mSymbol) ||
       ('!' == mToken.mSymbol) ||
       ('}' == mToken.mSymbol) ||
       (')' == mToken.mSymbol))) {
    // XXX need to verify that ! is only followed by "important [;|}]
    // XXX this requires a multi-token pushback buffer
    UngetToken();
    return true;
  }
  UngetToken();
  return false;
}

// Checks if we're at the end of a property, raising an error if we're not.
bool
CSSParserImpl::ExpectEndProperty()
{
  if (CheckEndProperty())
    return true;

  // If we're here, we read something incorrect, so we should report it.
  REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
  return false;
}

// Parses the priority suffix on a property, which at present may be
// either '!important' or nothing.
CSSParserImpl::PriorityParsingStatus
CSSParserImpl::ParsePriority()
{
  if (!GetToken(true)) {
    return ePriority_None; // properties may end with EOF
  }
  if (!mToken.IsSymbol('!')) {
    UngetToken();
    return ePriority_None; // dunno what it is, but it's not a priority
  }

  if (!GetToken(true)) {
    // EOF is not ok after !
    REPORT_UNEXPECTED_EOF(PEImportantEOF);
    return ePriority_Error;
  }

  if (mToken.mType != eCSSToken_Ident ||
      !mToken.mIdent.LowerCaseEqualsLiteral("important")) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedImportant);
    UngetToken();
    return ePriority_Error;
  }

  return ePriority_Important;
}

nsAString*
CSSParserImpl::NextIdent()
{
  // XXX Error reporting?
  if (!GetToken(true)) {
    return nullptr;
  }
  if (eCSSToken_Ident != mToken.mType) {
    UngetToken();
    return nullptr;
  }
  return &mToken.mIdent;
}

bool
CSSParserImpl::SkipAtRule(bool aInsideBlock)
{
  for (;;) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PESkipAtRuleEOF2);
      return false;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      char16_t symbol = mToken.mSymbol;
      if (symbol == ';') {
        break;
      }
      if (aInsideBlock && symbol == '}') {
        // The closing } doesn't belong to us.
        UngetToken();
        break;
      }
      if (symbol == '{') {
        SkipUntil('}');
        break;
      } else if (symbol == '(') {
        SkipUntil(')');
      } else if (symbol == '[') {
        SkipUntil(']');
      }
    } else if (eCSSToken_Function == mToken.mType ||
               eCSSToken_Bad_URL == mToken.mType) {
      SkipUntil(')');
    }
  }
  return true;
}

bool
CSSParserImpl::ParseAtRule(RuleAppendFunc aAppendFunc,
                           void* aData,
                           bool aInAtRule)
{

  nsCSSSection newSection;
  bool (CSSParserImpl::*parseFunc)(RuleAppendFunc, void*);

  if ((mSection <= eCSSSection_Charset) &&
      (mToken.mIdent.LowerCaseEqualsLiteral("charset"))) {
    parseFunc = &CSSParserImpl::ParseCharsetRule;
    newSection = eCSSSection_Import;  // only one charset allowed

  } else if ((mSection <= eCSSSection_Import) &&
             mToken.mIdent.LowerCaseEqualsLiteral("import")) {
    parseFunc = &CSSParserImpl::ParseImportRule;
    newSection = eCSSSection_Import;

  } else if ((mSection <= eCSSSection_NameSpace) &&
             mToken.mIdent.LowerCaseEqualsLiteral("namespace")) {
    parseFunc = &CSSParserImpl::ParseNameSpaceRule;
    newSection = eCSSSection_NameSpace;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("media")) {
    parseFunc = &CSSParserImpl::ParseMediaRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-document")) {
    parseFunc = &CSSParserImpl::ParseMozDocumentRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("font-face")) {
    parseFunc = &CSSParserImpl::ParseFontFaceRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("font-feature-values")) {
    parseFunc = &CSSParserImpl::ParseFontFeatureValuesRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("page")) {
    parseFunc = &CSSParserImpl::ParsePageRule;
    newSection = eCSSSection_General;

  } else if ((nsCSSProps::IsEnabled(eCSSPropertyAlias_MozAnimation,
                                    EnabledState()) &&
              mToken.mIdent.LowerCaseEqualsLiteral("-moz-keyframes")) ||
             (nsCSSProps::IsEnabled(eCSSPropertyAlias_WebkitAnimation) &&
              mToken.mIdent.LowerCaseEqualsLiteral("-webkit-keyframes")) ||
             mToken.mIdent.LowerCaseEqualsLiteral("keyframes")) {
    parseFunc = &CSSParserImpl::ParseKeyframesRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("supports")) {
    parseFunc = &CSSParserImpl::ParseSupportsRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("counter-style")) {
    parseFunc = &CSSParserImpl::ParseCounterStyleRule;
    newSection = eCSSSection_General;

  } else {
    if (!NonMozillaVendorIdentifier(mToken.mIdent)) {
      REPORT_UNEXPECTED_TOKEN(PEUnknownAtRule);
      OUTPUT_ERROR();
    }
    // Skip over unsupported at rule, don't advance section
    return SkipAtRule(aInAtRule);
  }

  // Inside of @-rules, only the rules that can occur anywhere
  // are allowed.
  bool unnestable = aInAtRule && newSection != eCSSSection_General;
  if (unnestable) {
    REPORT_UNEXPECTED_TOKEN(PEGroupRuleNestedAtRule);
  }

  if (unnestable || !(this->*parseFunc)(aAppendFunc, aData)) {
    // Skip over invalid at rule, don't advance section
    OUTPUT_ERROR();
    return SkipAtRule(aInAtRule);
  }

  // Nested @-rules don't affect the top-level rule chain requirement
  if (!aInAtRule) {
    mSection = newSection;
  }

  return true;
}

bool
CSSParserImpl::ParseCharsetRule(RuleAppendFunc aAppendFunc,
                                void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PECharsetRuleEOF);
    return false;
  }

  if (eCSSToken_String != mToken.mType) {
    UngetToken();
    REPORT_UNEXPECTED_TOKEN(PECharsetRuleNotString);
    return false;
  }

  nsAutoString charset = mToken.mIdent;

  if (!ExpectSymbol(';', true)) {
    return false;
  }

  // It's intentional that we don't create a rule object for @charset rules.

  return true;
}

bool
CSSParserImpl::ParseURLOrString(nsString& aURL)
{
  if (!GetToken(true)) {
    return false;
  }
  if (eCSSToken_String == mToken.mType || eCSSToken_URL == mToken.mType) {
    aURL = mToken.mIdent;
    return true;
  }
  UngetToken();
  return false;
}

bool
CSSParserImpl::ParseMediaQuery(eMediaQueryType aQueryType,
                               nsMediaQuery **aQuery,
                               bool *aHitStop)
{
  *aQuery = nullptr;
  *aHitStop = false;
  bool inAtRule = aQueryType == eMediaQueryAtRule;
  // Attempt to parse a single condition and stop
  bool singleCondition = aQueryType == eMediaQuerySingleCondition;

  // "If the comma-separated list is the empty list it is assumed to
  // specify the media query 'all'."  (css3-mediaqueries, section
  // "Media Queries")
  if (!GetToken(true)) {
    *aHitStop = true;
    // expected termination by EOF
    if (!inAtRule)
      return true;

    // unexpected termination by EOF
    REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
    return true;
  }

  if (eCSSToken_Symbol == mToken.mType && inAtRule &&
      (mToken.mSymbol == ';' || mToken.mSymbol == '{' || mToken.mSymbol == '}' )) {
    *aHitStop = true;
    UngetToken();
    return true;
  }
  UngetToken();

  nsMediaQuery* query = new nsMediaQuery;
  *aQuery = query;

  if (ExpectSymbol('(', true)) {
    // we got an expression without a media type
    UngetToken(); // so ParseMediaQueryExpression can handle it
    query->SetType(nsGkAtoms::all);
    query->SetTypeOmitted();
    // Just parse the first expression here.
    if (!ParseMediaQueryExpression(query)) {
      OUTPUT_ERROR();
      query->SetHadUnknownExpression();
    }
  } else if (singleCondition) {
    // Since we are only trying to consume a single condition, which precludes
    // media types and not/only, this should be the same as reaching immediate
    // EOF (no condition to parse)
    *aHitStop = true;
    return true;
  } else {
    RefPtr<nsAtom> mediaType;
    bool gotNotOrOnly = false;
    for (;;) {
      if (!GetToken(true)) {
        REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
        return false;
      }
      if (eCSSToken_Ident != mToken.mType) {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotIdent);
        UngetToken();
        return false;
      }
      // case insensitive from CSS - must be lower cased
      nsContentUtils::ASCIIToLower(mToken.mIdent);
      mediaType = NS_Atomize(mToken.mIdent);
      if (!gotNotOrOnly && mediaType == nsGkAtoms::_not) {
        gotNotOrOnly = true;
        query->SetNegated();
      } else if (!gotNotOrOnly && mediaType == nsGkAtoms::only) {
        gotNotOrOnly = true;
        query->SetHasOnly();
      } else if (mediaType == nsGkAtoms::_not ||
                 mediaType == nsGkAtoms::only ||
                 mediaType == nsGkAtoms::_and ||
                 mediaType == nsGkAtoms::_or) {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaReservedMediaType);
        UngetToken();
        return false;
      } else {
        // valid media type
        break;
      }
    }
    query->SetType(mediaType);
  }

  for (;;) {
    if (!GetToken(true)) {
      *aHitStop = true;
      // expected termination by EOF
      if (!inAtRule)
        break;

      // unexpected termination by EOF
      REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
      break;
    }

    if (eCSSToken_Symbol == mToken.mType && inAtRule &&
        (mToken.mSymbol == ';' || mToken.mSymbol == '{' || mToken.mSymbol == '}')) {
      *aHitStop = true;
      UngetToken();
      break;
    }
    if (!singleCondition &&
        eCSSToken_Symbol == mToken.mType && mToken.mSymbol == ',') {
      // Done with the expressions for this query
      break;
    }
    if (eCSSToken_Ident != mToken.mType ||
        !mToken.mIdent.LowerCaseEqualsLiteral("and")) {
      if (singleCondition) {
        // We have a condition at this point -- if we're not chained to other
        // conditions with and/or, we're done.
        UngetToken();
        break;
      } else {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotComma);
        UngetToken();
        return false;
      }
    }
    if (!ParseMediaQueryExpression(query)) {
      OUTPUT_ERROR();
      query->SetHadUnknownExpression();
    }
  }
  return true;
}

// Returns false only when there is a low-level error in the scanner
// (out-of-memory).
bool
CSSParserImpl::GatherMedia(nsMediaList* aMedia,
                           bool aInAtRule)
{
  eMediaQueryType type = aInAtRule ? eMediaQueryAtRule : eMediaQueryNormal;
  for (;;) {
    nsAutoPtr<nsMediaQuery> query;
    bool hitStop;
    if (!ParseMediaQuery(type, getter_Transfers(query), &hitStop)) {
      NS_ASSERTION(!hitStop, "should return true when hit stop");
      OUTPUT_ERROR();
      if (query) {
        query->SetHadUnknownExpression();
      }
      if (aInAtRule) {
        const char16_t stopChars[] =
          { char16_t(','), char16_t('{'), char16_t(';'), char16_t('}'), char16_t(0) };
        SkipUntilOneOf(stopChars);
      } else {
        SkipUntil(',');
      }
      // Rely on SkipUntilOneOf leaving mToken around as the last token read.
      if (mToken.mType == eCSSToken_Symbol && aInAtRule &&
          (mToken.mSymbol == '{' || mToken.mSymbol == ';'  || mToken.mSymbol == '}')) {
        UngetToken();
        hitStop = true;
      }
    }
    if (query) {
      aMedia->AppendQuery(query);
    }
    if (hitStop) {
      break;
    }
  }
  return true;
}

bool
CSSParserImpl::ParseMediaQueryExpression(nsMediaQuery* aQuery)
{
  if (!ExpectSymbol('(', true)) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedExpressionStart);
    return false;
  }
  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEMQExpressionEOF);
    return false;
  }
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedFeatureName);
    UngetToken();
    SkipUntil(')');
    return false;
  }

  nsMediaExpression *expr = aQuery->NewExpression();

  // case insensitive from CSS - must be lower cased
  nsContentUtils::ASCIIToLower(mToken.mIdent);
  nsDependentString featureString(mToken.mIdent, 0);
  uint8_t satisfiedReqFlags = 0;

  if (EnabledState() & (CSSEnabledState::eInUASheets |
                        CSSEnabledState::eInChrome)) {
    satisfiedReqFlags |= nsMediaFeature::eUserAgentAndChromeOnly;
  }

  // Strip off "-webkit-" prefix from featureString:
  if (StylePrefs::sWebkitPrefixedAliasesEnabled &&
      StringBeginsWith(featureString, NS_LITERAL_STRING("-webkit-"))) {
    satisfiedReqFlags |= nsMediaFeature::eHasWebkitPrefix;
    featureString.Rebind(featureString, 8);
  }
  if (StylePrefs::sWebkitDevicePixelRatioEnabled) {
    satisfiedReqFlags |= nsMediaFeature::eWebkitDevicePixelRatioPrefEnabled;
  }

  // Strip off "min-"/"max-" prefix from featureString:
  if (StringBeginsWith(featureString, NS_LITERAL_STRING("min-"))) {
    expr->mRange = nsMediaExpression::eMin;
    featureString.Rebind(featureString, 4);
  } else if (StringBeginsWith(featureString, NS_LITERAL_STRING("max-"))) {
    expr->mRange = nsMediaExpression::eMax;
    featureString.Rebind(featureString, 4);
  } else {
    expr->mRange = nsMediaExpression::eEqual;
  }

  RefPtr<nsAtom> mediaFeatureAtom = NS_Atomize(featureString);
  const nsMediaFeature *feature = nsMediaFeatures::features;
  for (; feature->mName; ++feature) {
    // See if name matches & all requirement flags are satisfied:
    // (We check requirements by turning off all of the flags that have been
    // satisfied, and then see if the result is 0.)
    if (*(feature->mName) == mediaFeatureAtom &&
        !(feature->mReqFlags & ~satisfiedReqFlags)) {
      break;
    }
  }
  if (!feature->mName ||
      (expr->mRange != nsMediaExpression::eEqual &&
       feature->mRangeType != nsMediaFeature::eMinMaxAllowed)) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedFeatureName);
    SkipUntil(')');
    return false;
  }
  expr->mFeature = feature;

  if (!GetToken(true) || mToken.IsSymbol(')')) {
    // Query expressions for any feature can be given without a value.
    // However, min/max prefixes are not allowed.
    if (expr->mRange != nsMediaExpression::eEqual) {
      REPORT_UNEXPECTED(PEMQNoMinMaxWithoutValue);
      return false;
    }
    expr->mValue.Reset();
    return true;
  }

  if (!mToken.IsSymbol(':')) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedFeatureNameEnd);
    UngetToken();
    SkipUntil(')');
    return false;
  }

  bool rv = false;
  switch (feature->mValueType) {
    case nsMediaFeature::eLength:
      rv = ParseSingleTokenNonNegativeVariant(expr->mValue, VARIANT_LENGTH,
                                              nullptr);
      break;
    case nsMediaFeature::eInteger:
    case nsMediaFeature::eBoolInteger:
      rv = ParseNonNegativeInteger(expr->mValue);
      // Enforce extra restrictions for eBoolInteger
      if (rv &&
          feature->mValueType == nsMediaFeature::eBoolInteger &&
          expr->mValue.GetIntValue() > 1)
        rv = false;
      break;
    case nsMediaFeature::eFloat:
      rv = ParseNonNegativeNumber(expr->mValue);
      break;
    case nsMediaFeature::eIntRatio:
      {
        // Two integers separated by '/', with optional whitespace on
        // either side of the '/'.
        RefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);
        expr->mValue.SetArrayValue(a, eCSSUnit_Array);
        // We don't bother with ParseNonNegativeVariant since we have to
        // check for != 0 as well; no need to worry about the UngetToken
        // since we're throwing out up to the next ')' anyway.
        rv = ParseSingleTokenVariant(a->Item(0), VARIANT_INTEGER, nullptr) &&
             a->Item(0).GetIntValue() > 0 &&
             ExpectSymbol('/', true) &&
             ParseSingleTokenVariant(a->Item(1), VARIANT_INTEGER, nullptr) &&
             a->Item(1).GetIntValue() > 0;
      }
      break;
    case nsMediaFeature::eResolution:
      rv = GetToken(true);
      if (!rv)
        break;
      rv = mToken.mType == eCSSToken_Dimension && mToken.mNumber > 0.0f;
      if (!rv) {
        UngetToken();
        break;
      }
      // No worries about whether unitless zero is allowed, since the
      // value must be positive (and we checked that above).
      NS_ASSERTION(!mToken.mIdent.IsEmpty(), "unit lied");
      if (mToken.mIdent.LowerCaseEqualsLiteral("dpi")) {
        expr->mValue.SetFloatValue(mToken.mNumber, eCSSUnit_Inch);
      } else if (mToken.mIdent.LowerCaseEqualsLiteral("dppx")) {
        expr->mValue.SetFloatValue(mToken.mNumber, eCSSUnit_Pixel);
      } else if (mToken.mIdent.LowerCaseEqualsLiteral("dpcm")) {
        expr->mValue.SetFloatValue(mToken.mNumber, eCSSUnit_Centimeter);
      } else {
        rv = false;
      }
      break;
    case nsMediaFeature::eEnumerated:
      rv = ParseSingleTokenVariant(expr->mValue, VARIANT_KEYWORD,
                                   feature->mData.mKeywordTable);
      break;
    case nsMediaFeature::eIdent:
      rv = ParseSingleTokenVariant(expr->mValue, VARIANT_IDENTIFIER, nullptr);
      break;
  }
  if (!rv || !ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED(PEMQExpectedFeatureValue);
    SkipUntil(')');
    return false;
  }

  return true;
}

// Parse a CSS2 import rule: "@import STRING | URL [medium [, medium]]"
bool
CSSParserImpl::ParseImportRule(RuleAppendFunc aAppendFunc, void* aData)
{
  RefPtr<nsMediaList> media = new nsMediaList();

  uint32_t linenum, colnum;
  nsAutoString url;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !ParseURLOrString(url)) {
    REPORT_UNEXPECTED_TOKEN(PEImportNotURI);
    return false;
  }

  if (!ExpectSymbol(';', true)) {
    if (!GatherMedia(media, true) ||
        !ExpectSymbol(';', true)) {
      REPORT_UNEXPECTED_TOKEN(PEImportUnexpected);
      // don't advance section, simply ignore invalid @import
      return false;
    }

    // Safe to assert this, since we ensured that there is something
    // other than the ';' coming after the @import's url() token.
    NS_ASSERTION(media->Length() != 0, "media list must be nonempty");
  }

  ProcessImport(url, media, aAppendFunc, aData, linenum, colnum);
  return true;
}

void
CSSParserImpl::ProcessImport(const nsString& aURLSpec,
                             nsMediaList* aMedia,
                             RuleAppendFunc aAppendFunc,
                             void* aData,
                             uint32_t aLineNumber,
                             uint32_t aColumnNumber)
{
  RefPtr<css::ImportRule> rule = new css::ImportRule(aMedia, aURLSpec,
                                                     aLineNumber,
                                                     aColumnNumber);
  (*aAppendFunc)(rule, aData);

  // Diagnose bad URIs even if we don't have a child loader.
  nsCOMPtr<nsIURI> url;
  // Charset will be deduced from mBaseURI, which is more or less correct.
  nsresult rv = NS_NewURI(getter_AddRefs(url), aURLSpec, nullptr, mBaseURI);

  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      // import url is bad
      REPORT_UNEXPECTED_P(PEImportBadURI, aURLSpec);
      OUTPUT_ERROR();
    }
    return;
  }

  if (mChildLoader) {
    mChildLoader->LoadChildSheet(mSheet, url, aMedia, rule, mReusableSheets);
  }
}

// Parse the {} part of an @media or @-moz-document rule.
bool
CSSParserImpl::ParseGroupRule(css::GroupRule* aRule,
                              RuleAppendFunc aAppendFunc,
                              void* aData)
{
  // XXXbz this could use better error reporting throughout the method
  if (!ExpectSymbol('{', true)) {
    return false;
  }

  // push rule on stack, loop over children
  PushGroup(aRule);
  nsCSSSection holdSection = mSection;
  mSection = eCSSSection_General;

  for (;;) {
    // Get next non-whitespace token
    if (! GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEGroupRuleEOF2);
      break;
    }
    if (mToken.IsSymbol('}')) { // done!
      UngetToken();
      break;
    }
    if (eCSSToken_AtKeyword == mToken.mType) {
      // Parse for nested rules
      ParseAtRule(aAppendFunc, aData, true);
      continue;
    }
    UngetToken();
    ParseRuleSet(AppendRuleToSheet, this, true);
  }
  PopGroup();

  if (!ExpectSymbol('}', true)) {
    mSection = holdSection;
    return false;
  }
  (*aAppendFunc)(aRule, aData);
  return true;
}

// Parse a CSS2 media rule: "@media medium [, medium] { ... }"
bool
CSSParserImpl::ParseMediaRule(RuleAppendFunc aAppendFunc, void* aData)
{
  RefPtr<nsMediaList> media = new nsMediaList();
  uint32_t linenum, colnum;
  if (GetNextTokenLocation(true, &linenum, &colnum) &&
      GatherMedia(media, true)) {
    // XXXbz this could use better error reporting throughout the method
    RefPtr<css::MediaRule> rule = new css::MediaRule(linenum, colnum);
    // Append first, so when we do SetMedia() the rule
    // knows what its stylesheet is.
    if (ParseGroupRule(rule, aAppendFunc, aData)) {
      rule->SetMedia(media);
      return true;
    }
  }

  return false;
}

// Parse a @-moz-document rule.  This is like an @media rule, but instead
// of a medium it has a nonempty list of items where each item is either
// url(), url-prefix(), or domain().
bool
CSSParserImpl::ParseMozDocumentRule(RuleAppendFunc aAppendFunc, void* aData)
{
  if (mParsingMode == css::eAuthorSheetFeatures &&
      !StylePrefs::sMozDocumentEnabledInContent) {
    return false;
  }

  css::DocumentRule::URL *urls = nullptr;
  css::DocumentRule::URL **next = &urls;

  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum)) {
    return false;
  }

  do {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEMozDocRuleEOF);
      delete urls;
      return false;
    }

    if (!(eCSSToken_URL == mToken.mType ||
          (eCSSToken_Function == mToken.mType &&
           (mToken.mIdent.LowerCaseEqualsLiteral("url-prefix") ||
            mToken.mIdent.LowerCaseEqualsLiteral("domain") ||
            mToken.mIdent.LowerCaseEqualsLiteral("regexp"))))) {
      REPORT_UNEXPECTED_TOKEN(PEMozDocRuleBadFunc2);
      UngetToken();
      delete urls;
      return false;
    }
    css::DocumentRule::URL *cur = *next = new css::DocumentRule::URL;
    next = &cur->next;
    if (mToken.mType == eCSSToken_URL) {
      cur->func = URLMatchingFunction::eURL;
      CopyUTF16toUTF8(mToken.mIdent, cur->url);
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("regexp")) {
      // regexp() is different from url-prefix() and domain() (but
      // probably the way they *should* have been* in that it requires a
      // string argument, and doesn't try to behave like url().
      cur->func = URLMatchingFunction::eRegExp;
      GetToken(true);
      // copy before we know it's valid (but before ExpectSymbol changes
      // mToken.mIdent)
      CopyUTF16toUTF8(mToken.mIdent, cur->url);
      if (eCSSToken_String != mToken.mType || !ExpectSymbol(')', true)) {
        REPORT_UNEXPECTED_TOKEN(PEMozDocRuleNotString);
        SkipUntil(')');
        delete urls;
        return false;
      }
    } else {
      if (mToken.mIdent.LowerCaseEqualsLiteral("url-prefix")) {
        cur->func = URLMatchingFunction::eURLPrefix;
      } else if (mToken.mIdent.LowerCaseEqualsLiteral("domain")) {
        cur->func = URLMatchingFunction::eDomain;
      }

      NS_ASSERTION(!mHavePushBack, "mustn't have pushback at this point");
      mScanner->NextURL(mToken);
      if (mToken.mType != eCSSToken_URL) {
        REPORT_UNEXPECTED_TOKEN(PEMozDocRuleNotURI);
        SkipUntil(')');
        delete urls;
        return false;
      }

      // We could try to make the URL (as long as it's not domain())
      // canonical and absolute with NS_NewURI and GetSpec, but I'm
      // inclined to think we shouldn't.
      CopyUTF16toUTF8(mToken.mIdent, cur->url);
    }
  } while (ExpectSymbol(',', true));

  RefPtr<css::DocumentRule> rule = new css::DocumentRule(linenum, colnum);
  rule->SetURLs(urls);

  return ParseGroupRule(rule, aAppendFunc, aData);
}

// Parse a CSS3 namespace rule: "@namespace [prefix] STRING | URL;"
bool
CSSParserImpl::ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEAtNSPrefixEOF);
    return false;
  }

  nsAutoString  prefix;
  nsAutoString  url;

  if (eCSSToken_Ident == mToken.mType) {
    prefix = mToken.mIdent;
    // user-specified identifiers are case-sensitive (bug 416106)
  } else {
    UngetToken();
  }

  if (!ParseURLOrString(url) || !ExpectSymbol(';', true)) {
    if (mHavePushBack) {
      REPORT_UNEXPECTED_TOKEN(PEAtNSUnexpected);
    } else {
      REPORT_UNEXPECTED_EOF(PEAtNSURIEOF);
    }
    return false;
  }

  ProcessNameSpace(prefix, url, aAppendFunc, aData, linenum, colnum);
  return true;
}

void
CSSParserImpl::ProcessNameSpace(const nsString& aPrefix,
                                const nsString& aURLSpec,
                                RuleAppendFunc aAppendFunc,
                                void* aData,
                                uint32_t aLineNumber,
                                uint32_t aColumnNumber)
{
  RefPtr<nsAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = NS_Atomize(aPrefix);
  }

  RefPtr<css::NameSpaceRule> rule = new css::NameSpaceRule(prefix, aURLSpec,
                                                             aLineNumber,
                                                             aColumnNumber);
  (*aAppendFunc)(rule, aData);

  // If this was the first namespace rule encountered, it will trigger
  // creation of a namespace map.
  if (!mNameSpaceMap) {
    mNameSpaceMap = mSheet->GetNameSpaceMap();
  }
}

// font-face-rule: '@font-face' '{' font-description '}'
// font-description: font-descriptor+
bool
CSSParserImpl::ParseFontFaceRule(RuleAppendFunc aAppendFunc, void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEBadFontBlockStart);
    return false;
  }

  RefPtr<nsCSSFontFaceRule> rule(new nsCSSFontFaceRule(linenum, colnum));

  for (;;) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEFontFaceEOF);
      break;
    }
    if (mToken.IsSymbol('}')) { // done!
      UngetToken();
      break;
    }

    // ignore extra semicolons
    if (mToken.IsSymbol(';'))
      continue;

    if (!ParseFontDescriptor(rule)) {
      REPORT_UNEXPECTED(PEDeclSkipped);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true))
        break;
    }
  }
  if (!ExpectSymbol('}', true)) {
    REPORT_UNEXPECTED_TOKEN(PEBadFontBlockEnd);
    return false;
  }
  (*aAppendFunc)(rule, aData);
  return true;
}

// font-descriptor: font-family-desc
//                | font-style-desc
//                | font-weight-desc
//                | font-stretch-desc
//                | font-src-desc
//                | unicode-range-desc
//
// All font-*-desc productions follow the pattern
//    IDENT ':' value ';'
//
// On entry to this function, mToken is the IDENT.

bool
CSSParserImpl::ParseFontDescriptor(nsCSSFontFaceRule* aRule)
{
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEFontDescExpected);
    return false;
  }

  nsString descName = mToken.mIdent;
  if (!ExpectSymbol(':', true)) {
    REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
    OUTPUT_ERROR();
    return false;
  }

  nsCSSFontDesc descID = nsCSSProps::LookupFontDesc(descName);
  nsCSSValue value;

  if (descID == eCSSFontDesc_UNKNOWN ||
      (descID == eCSSFontDesc_Display && !StylePrefs::sFontDisplayEnabled)) {
    if (NonMozillaVendorIdentifier(descName)) {
      // silently skip other vendors' extensions
      Unused << SkipDeclaration(true);
      return true;
    } else {
      REPORT_UNEXPECTED_P(PEUnknownFontDesc, descName);
      return false;
    }
  }

  if (!ParseFontDescriptorValue(descID, value)) {
    REPORT_UNEXPECTED_P(PEValueParsingError, descName);
    return false;
  }

  // Expect termination by ;, }, or EOF.
  if (GetToken(true)) {
    if (!mToken.IsSymbol(';') &&
        !mToken.IsSymbol('}')) {
      UngetToken();
      REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
      return false;
    }
    UngetToken();
  }

  aRule->SetDesc(descID, value);
  return true;
}

// @font-feature-values <font-family># {
//   @<feature-type> {
//     <feature-ident> : <feature-index>+;
//     <feature-ident> : <feature-index>+;
//     ...
//   }
//   ...
// }

bool
CSSParserImpl::ParseFontFeatureValuesRule(RuleAppendFunc aAppendFunc,
                                          void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum)) {
    return false;
  }

  RefPtr<nsCSSFontFeatureValuesRule>
               valuesRule(new nsCSSFontFeatureValuesRule(linenum, colnum));

  // parse family list
  nsCSSValue fontlistValue;

  if (!ParseFamily(fontlistValue) ||
      fontlistValue.GetUnit() != eCSSUnit_FontFamilyList)
  {
    REPORT_UNEXPECTED_TOKEN(PEFFVNoFamily);
    return false;
  }

  // add family to rule
  SharedFontList* fontlist = fontlistValue.GetFontFamilyListValue();

  // family list has generic ==> parse error
  if (fontlist->HasGeneric()) {
    REPORT_UNEXPECTED_TOKEN(PEFFVGenericInFamilyList);
    return false;
  }

  valuesRule->SetFamilyList(fontlist);

  // open brace
  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEFFVBlockStart);
    return false;
  }

  // list of sets of feature values, each set bound to a specific
  // feature-type (e.g. swash, annotation)
  for (;;) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEFFVUnexpectedEOF);
      break;
    }
    if (mToken.IsSymbol('}')) { // done!
      UngetToken();
      break;
    }

    if (!ParseFontFeatureValueSet(valuesRule)) {
      if (!SkipAtRule(false)) {
        break;
      }
    }
  }
  if (!ExpectSymbol('}', true)) {
    REPORT_UNEXPECTED_TOKEN(PEFFVUnexpectedBlockEnd);
    SkipUntil('}');
    return false;
  }

  (*aAppendFunc)(valuesRule, aData);
  return true;
}

#define NUMVALUES_NO_LIMIT  0xFFFF

// parse a single value set containing name-value pairs for a single feature type
//   @<feature-type> { [ <feature-ident> : <feature-index>+ ; ]* }
//   Ex: @swash { flowing: 1; delicate: 2; }
bool
CSSParserImpl::ParseFontFeatureValueSet(nsCSSFontFeatureValuesRule
                                                            *aFeatureValuesRule)
{
  // -- @keyword (e.g. swash, styleset)
  if (eCSSToken_AtKeyword != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEFontFeatureValuesNoAt);
    OUTPUT_ERROR();
    UngetToken();
    return false;
  }

  // which font-specific variant of font-variant-alternates
  int32_t whichVariant;
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
  if (!nsCSSProps::FindKeyword(keyword,
                               nsCSSProps::kFontVariantAlternatesFuncsKTable,
                               whichVariant))
  {
    if (!NonMozillaVendorIdentifier(mToken.mIdent)) {
      REPORT_UNEXPECTED_TOKEN(PEFFVUnknownFontVariantPropValue);
      OUTPUT_ERROR();
    }
    UngetToken();
    return false;
  }

  nsAutoString featureType(mToken.mIdent);

  // open brace
  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEFFVValueSetStart);
    return false;
  }

  // styleset and character-variant can be multi-valued, otherwise single value
  int32_t limitNumValues;

  switch (keyword) {
    case eCSSKeyword_styleset:
      limitNumValues = NUMVALUES_NO_LIMIT;
      break;
    case eCSSKeyword_character_variant:
      limitNumValues = 2;
      break;
    default:
      limitNumValues = 1;
      break;
  }

  // -- ident integer+ [, ident integer+]
  AutoTArray<gfxFontFeatureValueSet::ValueList, 5> values;

  // list of font-feature-values-declaration's
  for (;;) {
    nsAutoString valueId;

    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEFFVUnexpectedEOF);
      break;
    }

    // ignore extra semicolons
    if (mToken.IsSymbol(';')) {
      continue;
    }

    // close brace ==> done
    if (mToken.IsSymbol('}')) {
      break;
    }

    // ident
    if (eCSSToken_Ident != mToken.mType) {
      REPORT_UNEXPECTED_TOKEN(PEFFVExpectedIdent);
      if (!SkipDeclaration(true)) {
        break;
      }
      continue;
    }

    valueId.Assign(mToken.mIdent);

    // colon
    if (!ExpectSymbol(':', true)) {
      REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true)) {
        break;
      }
      continue;
    }

    // value list
    AutoTArray<uint32_t,4>   featureSelectors;

    nsCSSValue intValue;
    while (ParseNonNegativeInteger(intValue)) {
      featureSelectors.AppendElement(uint32_t(intValue.GetIntValue()));
    }

    int32_t numValues = featureSelectors.Length();

    if (numValues == 0) {
      REPORT_UNEXPECTED_TOKEN(PEFFVExpectedValue);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true)) {
        break;
      }
      continue;
    }

    if (numValues > limitNumValues) {
      REPORT_UNEXPECTED_P(PEFFVTooManyValues, featureType);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true)) {
        break;
      }
      continue;
    }

    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEFFVUnexpectedEOF);
      gfxFontFeatureValueSet::ValueList v(valueId, featureSelectors);
      values.AppendElement(v);
      break;
    }

    // ';' or '}' to end definition
    if (!mToken.IsSymbol(';') && !mToken.IsSymbol('}')) {
      REPORT_UNEXPECTED_TOKEN(PEFFVValueDefinitionTrailing);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true)) {
        break;
      }
      continue;
    }

    gfxFontFeatureValueSet::ValueList v(valueId, featureSelectors);
    values.AppendElement(v);

    if (mToken.IsSymbol('}')) {
      break;
    }
 }

  aFeatureValuesRule->AddValueList(whichVariant, values);
  return true;
}

bool
CSSParserImpl::ParseKeyframesRule(RuleAppendFunc aAppendFunc, void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEKeyframeNameEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Ident && mToken.mType != eCSSToken_String) {
    REPORT_UNEXPECTED_TOKEN(PEKeyframeBadName);
    UngetToken();
    return false;
  }

  if (mToken.mType == eCSSToken_Ident) {
    // Check for keywords that are not allowed as custom-ident for the
    // keyframes-name: standard CSS-wide keywords, plus 'none'.
    static const nsCSSKeyword excludedKeywords[] = {
      eCSSKeyword_none,
      eCSSKeyword_UNKNOWN
    };
    nsCSSValue value;
    if (!ParseCustomIdent(value, mToken.mIdent, excludedKeywords)) {
      REPORT_UNEXPECTED_TOKEN(PEKeyframeBadName);
      UngetToken();
      return false;
    }
  }

  nsString name(mToken.mIdent);

  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEKeyframeBrace);
    return false;
  }

  RefPtr<nsCSSKeyframesRule> rule =
    new nsCSSKeyframesRule(NS_Atomize(name), linenum, colnum);

  while (!ExpectSymbol('}', true)) {
    RefPtr<nsCSSKeyframeRule> kid = ParseKeyframeRule();
    if (kid) {
      rule->AppendStyleRule(kid);
    } else {
      OUTPUT_ERROR();
      SkipRuleSet(true);
    }
  }

  (*aAppendFunc)(rule, aData);
  return true;
}

bool
CSSParserImpl::ParsePageRule(RuleAppendFunc aAppendFunc, void* aData)
{
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum)) {
    return false;
  }

  // TODO: There can be page selectors after @page such as ":first", ":left".
  uint32_t parseFlags = eParseDeclaration_InBraces |
                        eParseDeclaration_AllowImportant;

  // Forbid viewport units in @page rules. See bug 811391.
  MOZ_ASSERT(mViewportUnitsEnabled,
             "Viewport units should be enabled outside of @page rules.");
  mViewportUnitsEnabled = false;
  RefPtr<css::Declaration> declaration =
    ParseDeclarationBlock(parseFlags, eCSSContext_Page);
  mViewportUnitsEnabled = true;

  if (!declaration) {
    return false;
  }

  RefPtr<nsCSSPageRule> rule =
    new nsCSSPageRule(declaration, linenum, colnum);

  (*aAppendFunc)(rule, aData);
  return true;
}

already_AddRefed<nsCSSKeyframeRule>
CSSParserImpl::ParseKeyframeRule()
{
  InfallibleTArray<float> selectorList;
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !ParseKeyframeSelectorList(selectorList)) {
    REPORT_UNEXPECTED(PEBadSelectorKeyframeRuleIgnored);
    return nullptr;
  }

  // Ignore !important in keyframe rules
  uint32_t parseFlags = eParseDeclaration_InBraces;
  RefPtr<css::Declaration> declaration(ParseDeclarationBlock(parseFlags));
  if (!declaration) {
    return nullptr;
  }

  // Takes ownership of declaration
  RefPtr<nsCSSKeyframeRule> rule =
    new nsCSSKeyframeRule(Move(selectorList), declaration.forget(),
                          linenum, colnum);
  return rule.forget();
}

bool
CSSParserImpl::ParseKeyframeSelectorList(InfallibleTArray<float>& aSelectorList)
{
  for (;;) {
    if (!GetToken(true)) {
      // The first time through the loop, this means we got an empty
      // list.  Otherwise, it means we have a trailing comma.
      return false;
    }
    float value;
    switch (mToken.mType) {
      case eCSSToken_Percentage:
        value = mToken.mNumber;
        break;
      case eCSSToken_Ident:
        if (mToken.mIdent.LowerCaseEqualsLiteral("from")) {
          value = 0.0f;
          break;
        }
        if (mToken.mIdent.LowerCaseEqualsLiteral("to")) {
          value = 1.0f;
          break;
        }
        MOZ_FALLTHROUGH;
      default:
        UngetToken();
        // The first time through the loop, this means we got an empty
        // list.  Otherwise, it means we have a trailing comma.
        return false;
    }
    aSelectorList.AppendElement(value);
    if (!ExpectSymbol(',', true)) {
      return true;
    }
  }
}

// supports_rule
//   : "@supports" supports_condition group_rule_body
//   ;
bool
CSSParserImpl::ParseSupportsRule(RuleAppendFunc aAppendFunc, void* aProcessData)
{
  bool conditionMet = false;
  nsString condition;

  mScanner->StartRecording();

  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum)) {
    return false;
  }

  bool parsed = ParseSupportsCondition(conditionMet);

  if (!parsed) {
    mScanner->StopRecording();
    return false;
  }

  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PESupportsGroupRuleStart);
    mScanner->StopRecording();
    return false;
  }

  UngetToken();
  mScanner->StopRecording(condition);

  // Remove the "{" that would follow the condition.
  if (condition.Length() != 0) {
    condition.Truncate(condition.Length() - 1);
  }

  // Remove spaces from the start and end of the recorded supports condition.
  condition.Trim(" ", true, true, false);

  // Record whether we are in a failing @supports, so that property parse
  // errors don't get reported.
  nsAutoFailingSupportsRule failing(this, conditionMet);

  RefPtr<css::GroupRule> rule =
    new mozilla::CSSSupportsRule(conditionMet, condition, linenum, colnum);
  return ParseGroupRule(rule, aAppendFunc, aProcessData);
}

// supports_condition
//   : supports_condition_in_parens supports_condition_terms
//   | supports_condition_negation
//   ;
bool
CSSParserImpl::ParseSupportsCondition(bool& aConditionMet)
{
  nsAutoInSupportsCondition aisc(this);

  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PESupportsConditionStartEOF2);
    return false;
  }

  UngetToken();

  mScanner->ClearSeenBadToken();

  if (mToken.IsSymbol('(') ||
      mToken.mType == eCSSToken_Function ||
      mToken.mType == eCSSToken_URL ||
      mToken.mType == eCSSToken_Bad_URL) {
    bool result = ParseSupportsConditionInParens(aConditionMet) &&
                  ParseSupportsConditionTerms(aConditionMet) &&
                  !mScanner->SeenBadToken();
    return result;
  }

  if (mToken.mType == eCSSToken_Ident &&
      mToken.mIdent.LowerCaseEqualsLiteral("not")) {
    bool result = ParseSupportsConditionNegation(aConditionMet) &&
                  !mScanner->SeenBadToken();
    return result;
  }

  REPORT_UNEXPECTED_TOKEN(PESupportsConditionExpectedStart);
  return false;
}

// supports_condition_negation
//   : 'not' supports_condition_in_parens
//   ;
bool
CSSParserImpl::ParseSupportsConditionNegation(bool& aConditionMet)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PESupportsConditionNotEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Ident ||
      !mToken.mIdent.LowerCaseEqualsLiteral("not")) {
    REPORT_UNEXPECTED_TOKEN(PESupportsConditionExpectedNot);
    return false;
  }

  if (ParseSupportsConditionInParens(aConditionMet)) {
    aConditionMet = !aConditionMet;
    return true;
  }

  return false;
}

// supports_condition_in_parens
//   : '(' S* supports_condition_in_parens_inside_parens ')' S*
//   | supports_condition_pref
//   | general_enclosed
//   ;
bool
CSSParserImpl::ParseSupportsConditionInParens(bool& aConditionMet)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PESupportsConditionInParensStartEOF);
    return false;
  }

  if (mToken.mType == eCSSToken_URL) {
    aConditionMet = false;
    return true;
  }

  if (AgentRulesEnabled() &&
      mToken.mType == eCSSToken_Function &&
      mToken.mIdent.LowerCaseEqualsLiteral("-moz-bool-pref")) {
    return ParseSupportsMozBoolPrefName(aConditionMet);
  }

  if (mToken.mType == eCSSToken_Function ||
      mToken.mType == eCSSToken_Bad_URL) {
    if (!SkipUntil(')')) {
      REPORT_UNEXPECTED_EOF(PESupportsConditionInParensEOF);
      return false;
    }
    aConditionMet = false;
    return true;
  }

  if (!mToken.IsSymbol('(')) {
    REPORT_UNEXPECTED_TOKEN(PESupportsConditionExpectedOpenParenOrFunction);
    UngetToken();
    return false;
  }

  if (!ParseSupportsConditionInParensInsideParens(aConditionMet)) {
    if (!SkipUntil(')')) {
      REPORT_UNEXPECTED_EOF(PESupportsConditionInParensEOF);
      return false;
    }
    aConditionMet = false;
    return true;
  }

  if (!(ExpectSymbol(')', true))) {
    SkipUntil(')');
    aConditionMet = false;
    return true;
  }

  return true;
}

// supports_condition_pref
//   : '-moz-bool-pref(' bool_pref_name ')'
//   ;
bool
CSSParserImpl::ParseSupportsMozBoolPrefName(bool& aConditionMet)
{
  if (!GetToken(true)) {
    return false;
  }

  if (mToken.mType != eCSSToken_String) {
    SkipUntil(')');
    return false;
  }

  aConditionMet = Preferences::GetBool(
    NS_ConvertUTF16toUTF8(mToken.mIdent).get());

  if (!ExpectSymbol(')', true)) {
    SkipUntil(')');
    return false;
  }

  return true;
}

// supports_condition_in_parens_inside_parens
//   : core_declaration
//   | supports_condition_negation
//   | supports_condition_in_parens supports_condition_terms
//   ;
bool
CSSParserImpl::ParseSupportsConditionInParensInsideParens(bool& aConditionMet)
{
  if (!GetToken(true)) {
    return false;
  }

  if (mToken.mType == eCSSToken_Ident) {
    if (!mToken.mIdent.LowerCaseEqualsLiteral("not")) {
      nsAutoString propertyName = mToken.mIdent;
      if (!ExpectSymbol(':', true)) {
        return false;
      }

      nsCSSPropertyID propID = LookupEnabledProperty(propertyName);
      if (propID == eCSSProperty_UNKNOWN) {
        if (ExpectSymbol(')', true)) {
          UngetToken();
          return false;
        }
        aConditionMet = false;
        SkipUntil(')');
        UngetToken();
      } else if (propID == eCSSPropertyExtra_variable) {
        if (ExpectSymbol(')', false)) {
          UngetToken();
          return false;
        }
        CSSVariableDeclarations::Type variableType;
        nsString variableValue;
        aConditionMet =
          ParseVariableDeclaration(&variableType, variableValue) &&
          ParsePriority() != ePriority_Error;
        if (!aConditionMet) {
          SkipUntil(')');
          UngetToken();
        }
      } else {
        if (ExpectSymbol(')', true)) {
          UngetToken();
          return false;
        }
        aConditionMet = ParseProperty(propID) &&
                        ParsePriority() != ePriority_Error;
        if (!aConditionMet) {
          SkipUntil(')');
          UngetToken();
        }
        mTempData.ClearProperty(propID);
        mTempData.AssertInitialState();
      }
      return true;
    }

    UngetToken();
    return ParseSupportsConditionNegation(aConditionMet);
  }

  UngetToken();
  return ParseSupportsConditionInParens(aConditionMet) &&
         ParseSupportsConditionTerms(aConditionMet);
}

// supports_condition_terms
//   : 'and' supports_condition_terms_after_operator('and')
//   | 'or' supports_condition_terms_after_operator('or')
//   |
//   ;
bool
CSSParserImpl::ParseSupportsConditionTerms(bool& aConditionMet)
{
  if (!GetToken(true)) {
    return true;
  }

  if (mToken.mType != eCSSToken_Ident) {
    UngetToken();
    return true;
  }

  if (mToken.mIdent.LowerCaseEqualsLiteral("and")) {
    return ParseSupportsConditionTermsAfterOperator(aConditionMet, eAnd);
  }

  if (mToken.mIdent.LowerCaseEqualsLiteral("or")) {
    return ParseSupportsConditionTermsAfterOperator(aConditionMet, eOr);
  }

  UngetToken();
  return true;
}

// supports_condition_terms_after_operator(operator)
//   : supports_condition_in_parens ( <operator> supports_condition_in_parens )*
//   ;
bool
CSSParserImpl::ParseSupportsConditionTermsAfterOperator(
                         bool& aConditionMet,
                         CSSParserImpl::SupportsConditionTermOperator aOperator)
{
  const char* token = aOperator == eAnd ? "and" : "or";
  for (;;) {
    bool termConditionMet = false;
    if (!ParseSupportsConditionInParens(termConditionMet)) {
      return false;
    }
    aConditionMet = aOperator == eAnd ? aConditionMet && termConditionMet :
                                        aConditionMet || termConditionMet;

    if (!GetToken(true)) {
      return true;
    }

    if (mToken.mType != eCSSToken_Ident ||
        !mToken.mIdent.LowerCaseEqualsASCII(token)) {
      UngetToken();
      return true;
    }
  }
}

bool
CSSParserImpl::ParseCounterStyleRule(RuleAppendFunc aAppendFunc, void* aData)
{
  RefPtr<nsAtom> name;
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !(name = ParseCounterStyleName(true))) {
    REPORT_UNEXPECTED_TOKEN(PECounterStyleNotIdent);
    return false;
  }

  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PECounterStyleBadBlockStart);
    return false;
  }

  RefPtr<nsCSSCounterStyleRule> rule = new nsCSSCounterStyleRule(name,
                                                                   linenum,
                                                                   colnum);
  for (;;) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PECounterStyleEOF);
      break;
    }
    if (mToken.IsSymbol('}')) {
      break;
    }
    if (mToken.IsSymbol(';')) {
      continue;
    }

    if (!ParseCounterDescriptor(rule)) {
      REPORT_UNEXPECTED(PEDeclSkipped);
      OUTPUT_ERROR();
      if (!SkipDeclaration(true)) {
        REPORT_UNEXPECTED_EOF(PECounterStyleEOF);
        break;
      }
    }
  }

  int32_t system = rule->GetSystem();
  bool isCorrect = false;
  switch (system) {
    case NS_STYLE_COUNTER_SYSTEM_CYCLIC:
    case NS_STYLE_COUNTER_SYSTEM_NUMERIC:
    case NS_STYLE_COUNTER_SYSTEM_ALPHABETIC:
    case NS_STYLE_COUNTER_SYSTEM_SYMBOLIC:
    case NS_STYLE_COUNTER_SYSTEM_FIXED: {
      // check whether symbols is set and the length is sufficient
      const nsCSSValue& symbols = rule->GetDesc(eCSSCounterDesc_Symbols);
      if (symbols.GetUnit() == eCSSUnit_List &&
          nsCSSCounterStyleRule::CheckDescValue(
              system, eCSSCounterDesc_Symbols, symbols)) {
        isCorrect = true;
      }
      break;
    }
    case NS_STYLE_COUNTER_SYSTEM_ADDITIVE: {
      // for additive system, additive-symbols must be set
      const nsCSSValue& symbols =
        rule->GetDesc(eCSSCounterDesc_AdditiveSymbols);
      if (symbols.GetUnit() == eCSSUnit_PairList) {
        isCorrect = true;
      }
      break;
    }
    case NS_STYLE_COUNTER_SYSTEM_EXTENDS: {
      // for extends system, symbols & additive-symbols must not be set
      const nsCSSValue& symbols = rule->GetDesc(eCSSCounterDesc_Symbols);
      const nsCSSValue& additiveSymbols =
        rule->GetDesc(eCSSCounterDesc_AdditiveSymbols);
      if (symbols.GetUnit() == eCSSUnit_Null &&
          additiveSymbols.GetUnit() == eCSSUnit_Null) {
        isCorrect = true;
      }
      break;
    }
    default:
      NS_NOTREACHED("unknown system");
  }

  if (isCorrect) {
    (*aAppendFunc)(rule, aData);
  }
  return true;
}

already_AddRefed<nsAtom>
CSSParserImpl::ParseCounterStyleName(bool aForDefinition)
{
  if (!GetToken(true)) {
    return nullptr;
  }

  if (mToken.mType != eCSSToken_Ident) {
    UngetToken();
    return nullptr;
  }

  static const nsCSSKeyword kReservedNames[] = {
    eCSSKeyword_none,
    eCSSKeyword_decimal,
    eCSSKeyword_disc,
    eCSSKeyword_UNKNOWN
  };

  nsCSSValue value; // we don't actually care about the value
  if (!ParseCustomIdent(value, mToken.mIdent,
                        aForDefinition ? kReservedNames : nullptr)) {
    REPORT_UNEXPECTED_TOKEN(PECounterStyleBadName);
    UngetToken();
    return nullptr;
  }

  nsString name = mToken.mIdent;
  if (nsCSSProps::IsPredefinedCounterStyle(name)) {
    ToLowerCase(name);
  }
  return NS_Atomize(name);
}

bool
CSSParserImpl::ParseCounterStyleNameValue(nsCSSValue& aValue)
{
  if (RefPtr<nsAtom> name = ParseCounterStyleName(false)) {
    aValue.SetAtomIdentValue(name.forget());
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParseCounterDescriptor(nsCSSCounterStyleRule* aRule)
{
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PECounterDescExpected);
    return false;
  }

  nsString descName = mToken.mIdent;
  if (!ExpectSymbol(':', true)) {
    REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
    OUTPUT_ERROR();
    return false;
  }

  nsCSSCounterDesc descID = nsCSSProps::LookupCounterDesc(descName);
  nsCSSValue value;

  if (descID == eCSSCounterDesc_UNKNOWN) {
    REPORT_UNEXPECTED_P(PEUnknownCounterDesc, descName);
    return false;
  }

  if (!ParseCounterDescriptorValue(descID, value)) {
    REPORT_UNEXPECTED_P(PEValueParsingError, descName);
    return false;
  }

  if (!ExpectEndProperty()) {
    return false;
  }

  aRule->SetDesc(descID, value);
  return true;
}

bool
CSSParserImpl::ParseCounterDescriptorValue(nsCSSCounterDesc aDescID,
                                           nsCSSValue& aValue)
{
  // Should also include VARIANT_IMAGE, but it is not supported currently.
  // See bug 1024179.
  static const int32_t VARIANT_COUNTER_SYMBOL =
    VARIANT_STRING | VARIANT_IDENTIFIER;

  switch (aDescID) {
    case eCSSCounterDesc_System: {
      nsCSSValue system;
      if (!ParseEnum(system, nsCSSProps::kCounterSystemKTable)) {
        return false;
      }
      switch (system.GetIntValue()) {
        case NS_STYLE_COUNTER_SYSTEM_FIXED: {
          nsCSSValue start;
          if (!ParseSingleTokenVariant(start, VARIANT_INTEGER, nullptr)) {
            start.SetIntValue(1, eCSSUnit_Integer);
          }
          aValue.SetPairValue(system, start);
          return true;
        }
        case NS_STYLE_COUNTER_SYSTEM_EXTENDS: {
          nsCSSValue name;
          if (!ParseCounterStyleNameValue(name)) {
            REPORT_UNEXPECTED_TOKEN(PECounterExtendsNotIdent);
            return false;
          }
          aValue.SetPairValue(system, name);
          return true;
        }
        default:
          aValue = system;
          return true;
      }
    }

    case eCSSCounterDesc_Negative: {
      nsCSSValue first, second;
      if (!ParseSingleTokenVariant(first, VARIANT_COUNTER_SYMBOL, nullptr)) {
        return false;
      }
      if (!ParseSingleTokenVariant(second, VARIANT_COUNTER_SYMBOL, nullptr)) {
        aValue = first;
      } else {
        aValue.SetPairValue(first, second);
      }
      return true;
    }

    case eCSSCounterDesc_Prefix:
    case eCSSCounterDesc_Suffix:
      return ParseSingleTokenVariant(aValue, VARIANT_COUNTER_SYMBOL, nullptr);

    case eCSSCounterDesc_Range: {
      if (ParseSingleTokenVariant(aValue, VARIANT_AUTO, nullptr)) {
        return true;
      }
      nsCSSValuePairList* item = aValue.SetPairListValue();
      for (;;) {
        nsCSSValuePair pair;
        if (!ParseCounterRange(pair)) {
          return false;
        }
        item->mXValue = pair.mXValue;
        item->mYValue = pair.mYValue;
        if (!ExpectSymbol(',', true)) {
          return true;
        }
        item->mNext = new nsCSSValuePairList;
        item = item->mNext;
      }
      // should always return in the loop
    }

    case eCSSCounterDesc_Pad: {
      nsCSSValue width, symbol;
      bool hasWidth = ParseNonNegativeInteger(width);
      if (!ParseSingleTokenVariant(symbol, VARIANT_COUNTER_SYMBOL, nullptr) ||
          (!hasWidth && !ParseNonNegativeInteger(width))) {
        return false;
      }
      aValue.SetPairValue(width, symbol);
      return true;
    }

    case eCSSCounterDesc_Fallback:
      return ParseCounterStyleNameValue(aValue);

    case eCSSCounterDesc_Symbols: {
      nsCSSValueList* item = nullptr;
      for (;;) {
        nsCSSValue value;
        if (!ParseSingleTokenVariant(value, VARIANT_COUNTER_SYMBOL, nullptr)) {
          return !!item;
        }
        if (!item) {
          item = aValue.SetListValue();
        } else {
          item->mNext = new nsCSSValueList;
          item = item->mNext;
        }
        item->mValue = value;
      }
      // should always return in the loop
    }

    case eCSSCounterDesc_AdditiveSymbols: {
      nsCSSValuePairList* item = nullptr;
      int32_t lastWeight = -1;
      for (;;) {
        nsCSSValue weight, symbol;
        bool hasWeight = ParseNonNegativeInteger(weight);
        if (!ParseSingleTokenVariant(symbol, VARIANT_COUNTER_SYMBOL, nullptr) ||
            (!hasWeight && !ParseNonNegativeInteger(weight))) {
          return false;
        }
        if (lastWeight != -1 && weight.GetIntValue() >= lastWeight) {
          REPORT_UNEXPECTED(PECounterASWeight);
          return false;
        }
        lastWeight = weight.GetIntValue();
        if (!item) {
          item = aValue.SetPairListValue();
        } else {
          item->mNext = new nsCSSValuePairList;
          item = item->mNext;
        }
        item->mXValue = weight;
        item->mYValue = symbol;
        if (!ExpectSymbol(',', true)) {
          return true;
        }
      }
      // should always return in the loop
    }

    case eCSSCounterDesc_SpeakAs:
      if (ParseSingleTokenVariant(aValue, VARIANT_AUTO | VARIANT_KEYWORD,
                                  nsCSSProps::kCounterSpeakAsKTable)) {
        if (aValue.GetUnit() == eCSSUnit_Enumerated &&
            aValue.GetIntValue() == NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT) {
          // Currently spell-out is not supported, so it is explicitly
          // rejected here rather than parsed as a custom identifier.
          // See bug 1024178.
          return false;
        }
        return true;
      }
      return ParseCounterStyleNameValue(aValue);

    default:
      NS_NOTREACHED("unknown descriptor");
      return false;
  }
}

bool
CSSParserImpl::ParseCounterRange(nsCSSValuePair& aPair)
{
  static const int32_t VARIANT_BOUND = VARIANT_INTEGER | VARIANT_KEYWORD;
  nsCSSValue lower, upper;
  if (!ParseSingleTokenVariant(lower, VARIANT_BOUND,
                               nsCSSProps::kCounterRangeKTable) ||
      !ParseSingleTokenVariant(upper, VARIANT_BOUND,
                               nsCSSProps::kCounterRangeKTable)) {
    return false;
  }
  if (lower.GetUnit() != eCSSUnit_Enumerated &&
      upper.GetUnit() != eCSSUnit_Enumerated &&
      lower.GetIntValue() > upper.GetIntValue()) {
    return false;
  }
  aPair = nsCSSValuePair(lower, upper);
  return true;
}

bool
CSSParserImpl::SkipUntil(char16_t aStopSymbol)
{
  nsCSSToken* tk = &mToken;
  AutoTArray<char16_t, 16> stack;
  stack.AppendElement(aStopSymbol);
  for (;;) {
    if (!GetToken(true)) {
      return false;
    }
    if (eCSSToken_Symbol == tk->mType) {
      char16_t symbol = tk->mSymbol;
      uint32_t stackTopIndex = stack.Length() - 1;
      if (symbol == stack.ElementAt(stackTopIndex)) {
        stack.RemoveElementAt(stackTopIndex);
        if (stackTopIndex == 0) {
          return true;
        }

      // Just handle out-of-memory by parsing incorrectly.  It's
      // highly unlikely we're dealing with a legitimate style sheet
      // anyway.
      } else if ('{' == symbol) {
        stack.AppendElement('}');
      } else if ('[' == symbol) {
        stack.AppendElement(']');
      } else if ('(' == symbol) {
        stack.AppendElement(')');
      }
    } else if (eCSSToken_Function == tk->mType ||
               eCSSToken_Bad_URL == tk->mType) {
      stack.AppendElement(')');
    }
  }
}

bool
CSSParserImpl::SkipBalancedContentUntil(char16_t aStopSymbol)
{
  nsCSSToken* tk = &mToken;
  AutoTArray<char16_t, 16> stack;
  stack.AppendElement(aStopSymbol);
  for (;;) {
    if (!GetToken(true)) {
      return true;
    }
    if (eCSSToken_Symbol == tk->mType) {
      char16_t symbol = tk->mSymbol;
      uint32_t stackTopIndex = stack.Length() - 1;
      if (symbol == stack.ElementAt(stackTopIndex)) {
        stack.RemoveElementAt(stackTopIndex);
        if (stackTopIndex == 0) {
          return true;
        }

      // Just handle out-of-memory by parsing incorrectly.  It's
      // highly unlikely we're dealing with a legitimate style sheet
      // anyway.
      } else if ('{' == symbol) {
        stack.AppendElement('}');
      } else if ('[' == symbol) {
        stack.AppendElement(']');
      } else if ('(' == symbol) {
        stack.AppendElement(')');
      } else if (')' == symbol ||
                 ']' == symbol ||
                 '}' == symbol) {
        UngetToken();
        return false;
      }
    } else if (eCSSToken_Function == tk->mType ||
               eCSSToken_Bad_URL == tk->mType) {
      stack.AppendElement(')');
    }
  }
}

void
CSSParserImpl::SkipUntilOneOf(const char16_t* aStopSymbolChars)
{
  nsCSSToken* tk = &mToken;
  nsDependentString stopSymbolChars(aStopSymbolChars);
  for (;;) {
    if (!GetToken(true)) {
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      char16_t symbol = tk->mSymbol;
      if (stopSymbolChars.FindChar(symbol) != -1) {
        break;
      } else if ('{' == symbol) {
        SkipUntil('}');
      } else if ('[' == symbol) {
        SkipUntil(']');
      } else if ('(' == symbol) {
        SkipUntil(')');
      }
    } else if (eCSSToken_Function == tk->mType ||
               eCSSToken_Bad_URL == tk->mType) {
      SkipUntil(')');
    }
  }
}

void
CSSParserImpl::SkipUntilAllOf(const StopSymbolCharStack& aStopSymbolChars)
{
  uint32_t i = aStopSymbolChars.Length();
  while (i--) {
    SkipUntil(aStopSymbolChars[i]);
  }
}

bool
CSSParserImpl::SkipDeclaration(bool aCheckForBraces)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(true)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF(PESkipDeclBraceEOF);
      }
      return false;
    }
    if (eCSSToken_Symbol == tk->mType) {
      char16_t symbol = tk->mSymbol;
      if (';' == symbol) {
        break;
      }
      if (aCheckForBraces) {
        if ('}' == symbol) {
          UngetToken();
          break;
        }
      }
      if ('{' == symbol) {
        SkipUntil('}');
      } else if ('(' == symbol) {
        SkipUntil(')');
      } else if ('[' == symbol) {
        SkipUntil(']');
      }
    } else if (eCSSToken_Function == tk->mType ||
               eCSSToken_Bad_URL == tk->mType) {
      SkipUntil(')');
    }
  }
  return true;
}

void
CSSParserImpl::SkipRuleSet(bool aInsideBraces)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PESkipRSBraceEOF);
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      char16_t symbol = tk->mSymbol;
      if ('}' == symbol && aInsideBraces) {
        // leave block closer for higher-level grammar to consume
        UngetToken();
        break;
      } else if ('{' == symbol) {
        SkipUntil('}');
        break;
      } else if ('(' == symbol) {
        SkipUntil(')');
      } else if ('[' == symbol) {
        SkipUntil(']');
      }
    } else if (eCSSToken_Function == tk->mType ||
               eCSSToken_Bad_URL == tk->mType) {
      SkipUntil(')');
    }
  }
}

void
CSSParserImpl::PushGroup(css::GroupRule* aRule)
{
  mGroupStack.AppendElement(aRule);
}

void
CSSParserImpl::PopGroup()
{
  uint32_t count = mGroupStack.Length();
  if (0 < count) {
    mGroupStack.RemoveElementAt(count - 1);
  }
}

void
CSSParserImpl::AppendRule(css::Rule* aRule)
{
  uint32_t count = mGroupStack.Length();
  if (0 < count) {
    mGroupStack[count - 1]->AppendStyleRule(aRule);
  }
  else {
    mSheet->AppendStyleRule(aRule);
  }
}

bool
CSSParserImpl::ParseRuleSet(RuleAppendFunc aAppendFunc, void* aData,
                            bool aInsideBraces)
{
  // First get the list of selectors for the rule
  nsCSSSelectorList* slist = nullptr;
  uint32_t linenum, colnum;
  if (!GetNextTokenLocation(true, &linenum, &colnum) ||
      !ParseSelectorList(slist, char16_t('{'))) {
    REPORT_UNEXPECTED(PEBadSelectorRSIgnored);
    OUTPUT_ERROR();
    SkipRuleSet(aInsideBraces);
    return false;
  }
  NS_ASSERTION(nullptr != slist, "null selector list");
  CLEAR_ERROR();

  // Next parse the declaration block
  uint32_t parseFlags = eParseDeclaration_InBraces |
                        eParseDeclaration_AllowImportant;
  RefPtr<css::Declaration> declaration = ParseDeclarationBlock(parseFlags);
  if (nullptr == declaration) {
    delete slist;
    return false;
  }

#if 0
  slist->Dump();
  fputs("{\n", stdout);
  declaration->List();
  fputs("}\n", stdout);
#endif

  // Translate the selector list and declaration block into style data

  RefPtr<css::StyleRule> rule = new css::StyleRule(slist, declaration,
                                                     linenum, colnum);
  (*aAppendFunc)(rule, aData);

  return true;
}

bool
CSSParserImpl::ParseSelectorList(nsCSSSelectorList*& aListHead,
                                 char16_t aStopChar)
{
  nsCSSSelectorList* list = nullptr;
  if (! ParseSelectorGroup(list)) {
    // must have at least one selector group
    aListHead = nullptr;
    return false;
  }
  NS_ASSERTION(nullptr != list, "no selector list");
  aListHead = list;

  // After that there must either be a "," or a "{" (the latter if
  // StopChar is nonzero)
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (! GetToken(true)) {
      if (aStopChar == char16_t(0)) {
        return true;
      }

      REPORT_UNEXPECTED_EOF(PESelectorListExtraEOF);
      break;
    }

    if (eCSSToken_Symbol == tk->mType) {
      if (',' == tk->mSymbol) {
        nsCSSSelectorList* newList = nullptr;
        // Another selector group must follow
        if (! ParseSelectorGroup(newList)) {
          break;
        }
        // add new list to the end of the selector list
        list->mNext = newList;
        list = newList;
        continue;
      } else if (aStopChar == tk->mSymbol && aStopChar != char16_t(0)) {
        UngetToken();
        return true;
      }
    }
    REPORT_UNEXPECTED_TOKEN(PESelectorListExtra);
    UngetToken();
    break;
  }

  delete aListHead;
  aListHead = nullptr;
  return false;
}

static bool IsUniversalSelector(const nsCSSSelector& aSelector)
{
  return bool((aSelector.mNameSpace == kNameSpaceID_Unknown) &&
                (aSelector.mLowercaseTag == nullptr) &&
                (aSelector.mIDList == nullptr) &&
                (aSelector.mClassList == nullptr) &&
                (aSelector.mAttrList == nullptr) &&
                (aSelector.mNegations == nullptr) &&
                (aSelector.mPseudoClassList == nullptr));
}

bool
CSSParserImpl::ParseSelectorGroup(nsCSSSelectorList*& aList)
{
  char16_t combinator = 0;
  nsAutoPtr<nsCSSSelectorList> list(new nsCSSSelectorList());

  for (;;) {
    if (!ParseSelector(list, combinator)) {
      return false;
    }

    // Look for a combinator.
    if (!GetToken(false)) {
      break; // EOF ok here
    }

    combinator = char16_t(0);
    if (mToken.mType == eCSSToken_Whitespace) {
      if (!GetToken(true)) {
        break; // EOF ok here
      }
      combinator = char16_t(' ');
    }

    if (mToken.mType != eCSSToken_Symbol) {
      UngetToken(); // not a combinator
    } else {
      char16_t symbol = mToken.mSymbol;
      if (symbol == '+' || symbol == '>' || symbol == '~') {
        combinator = mToken.mSymbol;
      } else {
        UngetToken(); // not a combinator
        if (symbol == ',' || symbol == '{' || symbol == ')') {
          break; // end of selector group
        }
      }
    }

    if (!combinator) {
      REPORT_UNEXPECTED_TOKEN(PESelectorListExtra);
      return false;
    }
  }

  aList = list.forget();
  return true;
}

#define SEL_MASK_NSPACE   0x01
#define SEL_MASK_ELEM     0x02
#define SEL_MASK_ID       0x04
#define SEL_MASK_CLASS    0x08
#define SEL_MASK_ATTRIB   0x10
#define SEL_MASK_PCLASS   0x20
#define SEL_MASK_PELEM    0x40

//
// Parses an ID selector #name
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseIDSelector(int32_t&       aDataMask,
                               nsCSSSelector& aSelector)
{
  NS_ASSERTION(!mToken.mIdent.IsEmpty(),
               "Empty mIdent in eCSSToken_ID token?");
  aDataMask |= SEL_MASK_ID;
  aSelector.AddID(mToken.mIdent);
  return eSelectorParsingStatus_Continue;
}

//
// Parses a class selector .name
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseClassSelector(int32_t&       aDataMask,
                                  nsCSSSelector& aSelector)
{
  if (! GetToken(false)) { // get ident
    REPORT_UNEXPECTED_EOF(PEClassSelEOF);
    return eSelectorParsingStatus_Error;
  }
  if (eCSSToken_Ident != mToken.mType) {  // malformed selector
    REPORT_UNEXPECTED_TOKEN(PEClassSelNotIdent);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }
  aDataMask |= SEL_MASK_CLASS;

  aSelector.AddClass(mToken.mIdent);

  return eSelectorParsingStatus_Continue;
}

//
// Parse a type element selector or a universal selector
// namespace|type or namespace|* or *|* or *
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseTypeOrUniversalSelector(int32_t&       aDataMask,
                                            nsCSSSelector& aSelector,
                                            bool           aIsNegated)
{
  nsAutoString buffer;
  if (mToken.IsSymbol('*')) {  // universal element selector, or universal namespace
    if (ExpectSymbol('|', false)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // namespace wildcard

      if (! GetToken(false)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;

        aSelector.SetTag(mToken.mIdent);
      }
      else if (mToken.IsSymbol('*')) {  // universal selector
        aDataMask |= SEL_MASK_ELEM;
        // don't set tag
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PETypeSelNotType);
        UngetToken();
        return eSelectorParsingStatus_Error;
      }
    }
    else {  // was universal element selector
      SetDefaultNamespaceOnSelector(aSelector);
      aDataMask |= SEL_MASK_ELEM;
      // don't set any tag in the selector
    }
    if (! GetToken(false)) {   // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) {    // element name or namespace name
    buffer = mToken.mIdent; // hang on to ident

    if (ExpectSymbol('|', false)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      int32_t nameSpaceID = GetNamespaceIdForPrefix(buffer);
      if (nameSpaceID == kNameSpaceID_Unknown) {
        return eSelectorParsingStatus_Error;
      }
      aSelector.SetNameSpace(nameSpaceID);

      if (! GetToken(false)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;
        aSelector.SetTag(mToken.mIdent);
      }
      else if (mToken.IsSymbol('*')) {  // universal selector
        aDataMask |= SEL_MASK_ELEM;
        // don't set tag
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PETypeSelNotType);
        UngetToken();
        return eSelectorParsingStatus_Error;
      }
    }
    else {  // was element name
      SetDefaultNamespaceOnSelector(aSelector);
      aSelector.SetTag(buffer);

      aDataMask |= SEL_MASK_ELEM;
    }
    if (! GetToken(false)) {   // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  else if (mToken.IsSymbol('|')) {  // No namespace
    aDataMask |= SEL_MASK_NSPACE;
    aSelector.SetNameSpace(kNameSpaceID_None);  // explicit NO namespace

    // get mandatory tag
    if (! GetToken(false)) {
      REPORT_UNEXPECTED_EOF(PETypeSelEOF);
      return eSelectorParsingStatus_Error;
    }
    if (eCSSToken_Ident == mToken.mType) {  // element name
      aDataMask |= SEL_MASK_ELEM;
      aSelector.SetTag(mToken.mIdent);
    }
    else if (mToken.IsSymbol('*')) {  // universal selector
      aDataMask |= SEL_MASK_ELEM;
      // don't set tag
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PETypeSelNotType);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
    if (! GetToken(false)) {   // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  else {
    SetDefaultNamespaceOnSelector(aSelector);
  }

  if (aIsNegated) {
    // restore last token read in case of a negated type selector
    UngetToken();
  }
  return eSelectorParsingStatus_Continue;
}

//
// Parse attribute selectors [attr], [attr=value], [attr|=value],
// [attr~=value], [attr^=value], [attr$=value] and [attr*=value]
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseAttributeSelector(int32_t&       aDataMask,
                                      nsCSSSelector& aSelector)
{
  if (! GetToken(true)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
    return eSelectorParsingStatus_Error;
  }

  int32_t nameSpaceID = kNameSpaceID_None;
  nsAutoString  attr;
  if (mToken.IsSymbol('*')) { // wildcard namespace
    nameSpaceID = kNameSpaceID_Unknown;
    if (ExpectSymbol('|', false)) {
      if (! GetToken(false)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        return eSelectorParsingStatus_Error;
       }
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttSelNoBar);
      return eSelectorParsingStatus_Error;
    }
  }
  else if (mToken.IsSymbol('|')) { // NO namespace
    if (! GetToken(false)) { // premature EOF
      REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
      return eSelectorParsingStatus_Error;
    }
    if (eCSSToken_Ident == mToken.mType) { // attr name
      attr = mToken.mIdent;
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) { // attr name or namespace
    attr = mToken.mIdent; // hang on to it
    if (ExpectSymbol('|', false)) {  // was a namespace
      nameSpaceID = GetNamespaceIdForPrefix(attr);
      if (nameSpaceID == kNameSpaceID_Unknown) {
        return eSelectorParsingStatus_Error;
      }
      if (! GetToken(false)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        return eSelectorParsingStatus_Error;
      }
    }
  }
  else {  // malformed
    REPORT_UNEXPECTED_TOKEN(PEAttributeNameOrNamespaceExpected);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  bool gotEOF = false;
  if (! GetToken(true)) { // premature EOF
    // Treat this just like we saw a ']', but do still output the
    // warning, similar to what ExpectSymbol does.
    REPORT_UNEXPECTED_EOF(PEAttSelInnerEOF);
    gotEOF = true;
  }
  if (gotEOF ||
      (eCSSToken_Symbol == mToken.mType) ||
      (eCSSToken_Includes == mToken.mType) ||
      (eCSSToken_Dashmatch == mToken.mType) ||
      (eCSSToken_Beginsmatch == mToken.mType) ||
      (eCSSToken_Endsmatch == mToken.mType) ||
      (eCSSToken_Containsmatch == mToken.mType)) {
    uint8_t func;
    // Important: Check the EOF/']' case first, since if gotEOF we
    // don't want to be examining mToken.
    if (gotEOF || ']' == mToken.mSymbol) {
      aDataMask |= SEL_MASK_ATTRIB;
      aSelector.AddAttribute(nameSpaceID, attr);
      func = NS_ATTR_FUNC_SET;
    }
    else if (eCSSToken_Includes == mToken.mType) {
      func = NS_ATTR_FUNC_INCLUDES;
    }
    else if (eCSSToken_Dashmatch == mToken.mType) {
      func = NS_ATTR_FUNC_DASHMATCH;
    }
    else if (eCSSToken_Beginsmatch == mToken.mType) {
      func = NS_ATTR_FUNC_BEGINSMATCH;
    }
    else if (eCSSToken_Endsmatch == mToken.mType) {
      func = NS_ATTR_FUNC_ENDSMATCH;
    }
    else if (eCSSToken_Containsmatch == mToken.mType) {
      func = NS_ATTR_FUNC_CONTAINSMATCH;
    }
    else if ('=' == mToken.mSymbol) {
      func = NS_ATTR_FUNC_EQUALS;
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttSelUnexpected);
      UngetToken(); // bad function
      return eSelectorParsingStatus_Error;
    }
    if (NS_ATTR_FUNC_SET != func) { // get value
      if (! GetToken(true)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttSelValueEOF);
        return eSelectorParsingStatus_Error;
      }
      if ((eCSSToken_Ident == mToken.mType) || (eCSSToken_String == mToken.mType)) {
        nsAutoString  value(mToken.mIdent);
        // Avoid duplicating the eof handling by just not checking for
        // the 'i' annotation if we got eof.
        typedef nsAttrSelector::ValueCaseSensitivity ValueCaseSensitivity;
        ValueCaseSensitivity valueCaseSensitivity =
          ValueCaseSensitivity::CaseSensitive;
        bool eof = !GetToken(true);
        if (!eof) {
          if (eCSSToken_Ident == mToken.mType &&
              mToken.mIdent.LowerCaseEqualsLiteral("i")) {
            valueCaseSensitivity = ValueCaseSensitivity::CaseInsensitive;
            eof = !GetToken(true);
          }
        }
        bool gotClosingBracket;
        if (eof) { // premature EOF
          // Report a warning, but then treat it as a closing bracket.
          REPORT_UNEXPECTED_EOF(PEAttSelCloseEOF);
          gotClosingBracket = true;
        } else {
          gotClosingBracket = mToken.IsSymbol(']');
        }
        if (gotClosingBracket) {
          // For cases when this style sheet is applied to an HTML
          // element in an HTML document, and the attribute selector is
          // for a non-namespaced attribute, then check to see if it's
          // one of the known attributes whose VALUE is
          // case-insensitive.
          if (valueCaseSensitivity != ValueCaseSensitivity::CaseInsensitive &&
              nameSpaceID == kNameSpaceID_None) {
            static const char* caseInsensitiveHTMLAttribute[] = {
              // list based on http://www.w3.org/TR/html4/
              "lang",
              "dir",
              "http-equiv",
              "text",
              "link",
              "vlink",
              "alink",
              "compact",
              "align",
              "frame",
              "rules",
              "valign",
              "scope",
              "axis",
              "nowrap",
              "hreflang",
              "rel",
              "rev",
              "charset",
              "codetype",
              "declare",
              "valuetype",
              "shape",
              "nohref",
              "media",
              "bgcolor",
              "clear",
              "color",
              "face",
              "noshade",
              "noresize",
              "scrolling",
              "target",
              "method",
              "enctype",
              "accept-charset",
              "accept",
              "checked",
              "multiple",
              "selected",
              "disabled",
              "readonly",
              "language",
              "defer",
              "type",
              // additional attributes not in HTML4
              "direction", // marquee
              nullptr
            };
            short i = 0;
            const char* htmlAttr;
            while ((htmlAttr = caseInsensitiveHTMLAttribute[i++])) {
              if (attr.LowerCaseEqualsASCII(htmlAttr)) {
                valueCaseSensitivity = ValueCaseSensitivity::CaseInsensitiveInHTML;
                break;
              }
            }
          }
          aDataMask |= SEL_MASK_ATTRIB;
          aSelector.AddAttribute(nameSpaceID, attr, func, value,
                                 valueCaseSensitivity);
        }
        else {
          REPORT_UNEXPECTED_TOKEN(PEAttSelNoClose);
          UngetToken();
          return eSelectorParsingStatus_Error;
        }
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttSelBadValue);
        UngetToken();
        return eSelectorParsingStatus_Error;
      }
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PEAttSelUnexpected);
    UngetToken(); // bad dog, no biscut!
    return eSelectorParsingStatus_Error;
   }
   return eSelectorParsingStatus_Continue;
}

//
// Parse pseudo-classes and pseudo-elements
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParsePseudoSelector(int32_t&       aDataMask,
                                   nsCSSSelector& aSelector,
                                   bool           aIsNegated,
                                   nsAtom**      aPseudoElement,
                                   nsAtomList**   aPseudoElementArgs,
                                   CSSPseudoElementType* aPseudoElementType)
{
  NS_ASSERTION(aIsNegated || (aPseudoElement && aPseudoElementArgs),
               "expected location to store pseudo element");
  NS_ASSERTION(!aIsNegated || (!aPseudoElement && !aPseudoElementArgs),
               "negated selectors shouldn't have a place to store "
               "pseudo elements");
  if (! GetToken(false)) { // premature eof
    REPORT_UNEXPECTED_EOF(PEPseudoSelEOF);
    return eSelectorParsingStatus_Error;
  }

  // First, find out whether we are parsing a CSS3 pseudo-element
  bool parsingPseudoElement = false;
  if (mToken.IsSymbol(':')) {
    parsingPseudoElement = true;
    if (! GetToken(false)) { // premature eof
      REPORT_UNEXPECTED_EOF(PEPseudoSelEOF);
      return eSelectorParsingStatus_Error;
    }
  }

  // Do some sanity-checking on the token
  if (eCSSToken_Ident != mToken.mType && eCSSToken_Function != mToken.mType) {
    // malformed selector
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelBadName);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  // OK, now we know we have an mIdent.  Atomize it.  All the atoms, for
  // pseudo-classes as well as pseudo-elements, start with a single ':'.
  nsAutoString buffer;
  buffer.Append(char16_t(':'));
  buffer.Append(mToken.mIdent);
  nsContentUtils::ASCIIToLower(buffer);
  RefPtr<nsAtom> pseudo = NS_Atomize(buffer);

  // stash away some info about this pseudo so we only have to get it once.
  bool isTreePseudo = false;
  CSSEnabledState enabledState = EnabledState();
  CSSPseudoElementType pseudoElementType =
    nsCSSPseudoElements::GetPseudoType(pseudo, enabledState);
  CSSPseudoClassType pseudoClassType =
    nsCSSPseudoClasses::GetPseudoType(pseudo, enabledState);
  bool pseudoClassIsUserAction =
    nsCSSPseudoClasses::IsUserActionPseudoClass(pseudoClassType);

  if (nsCSSAnonBoxes::IsNonElement(pseudo)) {
    // Non-element anonymous boxes should not match any rule.
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelUnknown);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  // We currently allow :-moz-placeholder and ::-moz-placeholder and
  // ::placeholder. We have to be a bit stricter regarding the
  // pseudo-element parsing rules.
  if (pseudoElementType == CSSPseudoElementType::placeholder &&
      pseudoClassType == CSSPseudoClassType::mozPlaceholder) {
    if (parsingPseudoElement) {
      pseudoClassType = CSSPseudoClassType::NotPseudo;
    } else {
      pseudoElementType = CSSPseudoElementType::NotPseudo;
    }
  }

#ifdef MOZ_XUL
  isTreePseudo = (pseudoElementType == CSSPseudoElementType::XULTree);
  // If a tree pseudo-element is using the function syntax, it will
  // get isTree set here and will pass the check below that only
  // allows functions if they are in our list of things allowed to be
  // functions.  If it is _not_ using the function syntax, isTree will
  // be false, and it will still pass that check.  So the tree
  // pseudo-elements are allowed to be either functions or not, as
  // desired.
  bool isTree = (eCSSToken_Function == mToken.mType) && isTreePseudo;
#endif
  bool isPseudoElement = (pseudoElementType < CSSPseudoElementType::Count);
  // anonymous boxes are only allowed if they're the tree boxes or we have
  // enabled agent rules
  bool isAnonBox = isTreePseudo ||
    ((pseudoElementType == CSSPseudoElementType::InheritingAnonBox ||
      pseudoElementType == CSSPseudoElementType::NonInheritingAnonBox) &&
     AgentRulesEnabled());
  bool isPseudoClass =
    (pseudoClassType != CSSPseudoClassType::NotPseudo);

  NS_ASSERTION(!isPseudoClass ||
               pseudoElementType == CSSPseudoElementType::NotPseudo,
               "Why is this atom both a pseudo-class and a pseudo-element?");
  NS_ASSERTION(isPseudoClass + isPseudoElement + isAnonBox <= 1,
               "Shouldn't be more than one of these");

  if (!isPseudoClass && !isPseudoElement && !isAnonBox) {
    // Not a pseudo-class, not a pseudo-element.... forget it
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelUnknown);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  // If it's a function token, it better be on our "ok" list, and if the name
  // is that of a function pseudo it better be a function token
  if ((eCSSToken_Function == mToken.mType) !=
      (
#ifdef MOZ_XUL
       isTree ||
#endif
       CSSPseudoClassType::negation == pseudoClassType ||
       nsCSSPseudoClasses::HasStringArg(pseudoClassType) ||
       nsCSSPseudoClasses::HasNthPairArg(pseudoClassType) ||
       nsCSSPseudoClasses::HasSelectorListArg(pseudoClassType))) {
    // There are no other function pseudos
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelNonFunc);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  // If it starts with "::", it better be a pseudo-element
  if (parsingPseudoElement &&
      !isPseudoElement &&
      !isAnonBox) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelNotPE);
    UngetToken();
    return eSelectorParsingStatus_Error;
  }

  if (aSelector.IsPseudoElement()) {
    CSSPseudoElementType type = aSelector.PseudoType();
    if (type >= CSSPseudoElementType::Count ||
        !nsCSSPseudoElements::PseudoElementSupportsUserActionState(type)) {
      // We only allow user action pseudo-classes on certain pseudo-elements.
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelNoUserActionPC);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
    if (!isPseudoClass || !pseudoClassIsUserAction) {
      // CSS 4 Selectors says that pseudo-elements can only be followed by
      // a user action pseudo-class.
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassNotUserAction);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
  }

  if (!parsingPseudoElement &&
      CSSPseudoClassType::negation == pseudoClassType) {
    if (aIsNegated) { // :not() can't be itself negated
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelDoubleNot);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
    // CSS 3 Negation pseudo-class takes one simple selector as argument
    nsSelectorParsingStatus parsingStatus =
      ParseNegatedSimpleSelector(aDataMask, aSelector);
    if (eSelectorParsingStatus_Continue != parsingStatus) {
      return parsingStatus;
    }
  }
  else if (!parsingPseudoElement && isPseudoClass) {
    aDataMask |= SEL_MASK_PCLASS;
    if (eCSSToken_Function == mToken.mType) {
      nsSelectorParsingStatus parsingStatus;
      if (nsCSSPseudoClasses::HasStringArg(pseudoClassType)) {
        parsingStatus =
          ParsePseudoClassWithIdentArg(aSelector, pseudoClassType);
      }
      else if (nsCSSPseudoClasses::HasNthPairArg(pseudoClassType)) {
        parsingStatus =
          ParsePseudoClassWithNthPairArg(aSelector, pseudoClassType);
      }
      else {
        MOZ_ASSERT(nsCSSPseudoClasses::HasSelectorListArg(pseudoClassType),
                   "unexpected pseudo with function token");
        parsingStatus = ParsePseudoClassWithSelectorListArg(aSelector,
                                                            pseudoClassType);
      }
      if (eSelectorParsingStatus_Continue != parsingStatus) {
        if (eSelectorParsingStatus_Error == parsingStatus) {
          SkipUntil(')');
        }
        return parsingStatus;
      }
    }
    else {
      aSelector.AddPseudoClass(pseudoClassType);
    }
  }
  else if (isPseudoElement || isAnonBox) {
    // Pseudo-element.  Make some more sanity checks.

    if (aIsNegated) { // pseudo-elements can't be negated
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelPEInNot);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
    // CSS2 pseudo-elements and -moz-tree-* pseudo-elements are allowed
    // to have a single ':' on them.  Others (CSS3+ pseudo-elements and
    // various -moz-* pseudo-elements) must have |parsingPseudoElement|
    // set.
    if (!parsingPseudoElement &&
        !nsCSSPseudoElements::IsCSS2PseudoElement(pseudo)
#ifdef MOZ_XUL
        && !isTreePseudo
#endif
        ) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelNewStyleOnly);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }

    if (0 == (aDataMask & SEL_MASK_PELEM)) {
      aDataMask |= SEL_MASK_PELEM;
      NS_ADDREF(*aPseudoElement = pseudo);
      *aPseudoElementType = pseudoElementType;

#ifdef MOZ_XUL
      if (isTree) {
        // We have encountered a pseudoelement of the form
        // -moz-tree-xxxx(a,b,c).  We parse (a,b,c) and add each
        // item in the list to the pseudoclass list.  They will be pulled
        // from the list later along with the pseudo-element.
        if (!ParseTreePseudoElement(aPseudoElementArgs)) {
          return eSelectorParsingStatus_Error;
        }
      }
#endif

      // Pseudo-elements can only be followed by user action pseudo-classes
      // or be the end of the selector.  So the next non-whitespace token must
      // be ':', '{' or ',' or EOF.
      if (!GetToken(true)) { // premature eof is ok (here!)
        return eSelectorParsingStatus_Done;
      }
      if (parsingPseudoElement && mToken.IsSymbol(':')) {
        UngetToken();
        return eSelectorParsingStatus_Continue;
      }
      if ((mToken.IsSymbol('{') || mToken.IsSymbol(','))) {
        UngetToken();
        return eSelectorParsingStatus_Done;
      }
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelEndOrUserActionPC);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
    else {  // multiple pseudo elements, not legal
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelMultiplePE);
      UngetToken();
      return eSelectorParsingStatus_Error;
    }
  }
#ifdef DEBUG
  else {
    // We should never end up here.  Indeed, if we ended up here, we know (from
    // the current if/else cascade) that !isPseudoElement and !isAnonBox.  But
    // then due to our earlier check we know that isPseudoClass.  Since we
    // didn't fall into the isPseudoClass case in this cascade, we must have
    // parsingPseudoElement.  But we've already checked the
    // parsingPseudoElement && !isPseudoClass && !isAnonBox case and bailed if
    // it's happened.
    NS_NOTREACHED("How did this happen?");
  }
#endif
  return eSelectorParsingStatus_Continue;
}

//
// Parse the argument of a negation pseudo-class :not()
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseNegatedSimpleSelector(int32_t&       aDataMask,
                                          nsCSSSelector& aSelector)
{
  if (! GetToken(true)) { // premature eof
    REPORT_UNEXPECTED_EOF(PENegationEOF);
    return eSelectorParsingStatus_Error;
  }

  if (mToken.IsSymbol(')')) {
    REPORT_UNEXPECTED_TOKEN(PENegationBadArg);
    return eSelectorParsingStatus_Error;
  }

  // Create a new nsCSSSelector and add it to the end of
  // aSelector.mNegations.
  // Given the current parsing rules, every selector in mNegations
  // contains only one simple selector (css3 definition) within it.
  // This could easily change in future versions of CSS, and the only
  // thing we need to change to support that is this parsing code and the
  // serialization code for nsCSSSelector.
  nsCSSSelector *newSel = new nsCSSSelector();
  nsCSSSelector* negations = &aSelector;
  while (negations->mNegations) {
    negations = negations->mNegations;
  }
  negations->mNegations = newSel;

  nsSelectorParsingStatus parsingStatus;
  if (eCSSToken_ID == mToken.mType) { // #id
    parsingStatus = ParseIDSelector(aDataMask, *newSel);
  }
  else if (mToken.IsSymbol('.')) {    // .class
    parsingStatus = ParseClassSelector(aDataMask, *newSel);
  }
  else if (mToken.IsSymbol(':')) {    // :pseudo
    parsingStatus = ParsePseudoSelector(aDataMask, *newSel, true,
                                        nullptr, nullptr, nullptr);
  }
  else if (mToken.IsSymbol('[')) {    // [attribute
    parsingStatus = ParseAttributeSelector(aDataMask, *newSel);
    if (eSelectorParsingStatus_Error == parsingStatus) {
      // Skip forward to the matching ']'
      SkipUntil(']');
    }
  }
  else {
    // then it should be a type element or universal selector
    parsingStatus = ParseTypeOrUniversalSelector(aDataMask, *newSel, true);
  }
  if (eSelectorParsingStatus_Error == parsingStatus) {
    REPORT_UNEXPECTED_TOKEN(PENegationBadInner);
    SkipUntil(')');
    return parsingStatus;
  }
  // close the parenthesis
  if (!ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED_TOKEN(PENegationNoClose);
    SkipUntil(')');
    return eSelectorParsingStatus_Error;
  }

  NS_ASSERTION(newSel->mNameSpace == kNameSpaceID_Unknown ||
               (!newSel->mIDList && !newSel->mClassList &&
                !newSel->mPseudoClassList && !newSel->mAttrList),
               "Need to fix the serialization code to deal with this");

  return eSelectorParsingStatus_Continue;
}

//
// Parse the argument of a pseudo-class that has an ident arg
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParsePseudoClassWithIdentArg(nsCSSSelector& aSelector,
                                            CSSPseudoClassType aType)
{
  if (! GetToken(true)) { // premature eof
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }
  // We expect an identifier with a language abbreviation
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotIdent);
    UngetToken();
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
  }

  // -moz-locale-dir and dir take an identifier argument.  While
  // only 'ltr' and 'rtl' (case-insensitively) will match anything, any
  // other identifier is still valid.
  if (aType == CSSPseudoClassType::mozLocaleDir ||
      aType == CSSPseudoClassType::dir) {
    nsContentUtils::ASCIIToLower(mToken.mIdent); // case insensitive
  }

  // Add the pseudo with the language parameter
  aSelector.AddPseudoClass(aType, mToken.mIdent.get());

  // close the parenthesis
  if (!ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoClose);
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
  }

  return eSelectorParsingStatus_Continue;
}

CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParsePseudoClassWithNthPairArg(nsCSSSelector& aSelector,
                                              CSSPseudoClassType aType)
{
  int32_t numbers[2] = { 0, 0 };
  bool lookForB = true;
  bool onlyN = false;
  bool hasSign = false;
  int sign = 1;

  // Follow the whitespace rules as proposed in
  // http://lists.w3.org/Archives/Public/www-style/2008Mar/0121.html

  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }

  // A helper function that checks if the token starts with literal string
  // |aStr| using a case-insensitive match.
  auto TokenBeginsWith = [this] (const nsLiteralString& aStr) {
    return StringBeginsWith(mToken.mIdent, aStr,
                            nsASCIICaseInsensitiveStringComparator());
  };

  if (mToken.IsSymbol('+')) {
    // This can only be +n, since +an, -an, +a, -a will all
    // parse a number as the first token, and -n is an ident token.
    numbers[0] = 1;
    onlyN = true;

    // consume the `n`
    // We do not allow whitespace here
    // https://drafts.csswg.org/css-syntax-3/#the-anb-type
    if (! GetToken(false)) {
      REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
  }

  if (eCSSToken_Ident == mToken.mType || eCSSToken_Dimension == mToken.mType) {
    // The CSS tokenization doesn't handle :nth-child() containing - well:
    //   2n-1 is a dimension
    //   n-1 is an identifier
    // The easiest way to deal with that is to push everything from the
    // minus on back onto the scanner's pushback buffer.
    uint32_t truncAt = 0;
    if (TokenBeginsWith(NS_LITERAL_STRING("n-"))) {
      truncAt = 1;
    } else if (TokenBeginsWith(NS_LITERAL_STRING("-n-"))) {
      truncAt = 2;
    }
    if (truncAt != 0) {
      mScanner->Backup(mToken.mIdent.Length() - truncAt);
      mToken.mIdent.Truncate(truncAt);
    }
  }

  if (onlyN) {
    // If we parsed a + or -, check that the truncated
    // token is an "n"
    if (eCSSToken_Ident != mToken.mType || !mToken.mIdent.LowerCaseEqualsLiteral("n")) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      return eSelectorParsingStatus_Error;
    }
  } else {
    if (eCSSToken_Ident == mToken.mType) {
      if (mToken.mIdent.LowerCaseEqualsLiteral("odd")) {
        numbers[0] = 2;
        numbers[1] = 1;
        lookForB = false;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("even")) {
        numbers[0] = 2;
        numbers[1] = 0;
        lookForB = false;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("n")) {
          numbers[0] = 1;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-n")) {
        numbers[0] = -1;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
        return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
      }
    }
    else if (eCSSToken_Number == mToken.mType) {
      if (!mToken.mIntegerValid) {
        REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
        return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
      }

      numbers[1] = mToken.mInteger;
      lookForB = false;
    }
    else if (eCSSToken_Dimension == mToken.mType) {
      if (!mToken.mIntegerValid || !mToken.mIdent.LowerCaseEqualsLiteral("n")) {
        REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
        return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
      }
      numbers[0] = mToken.mInteger;
    }
    // XXX If it's a ')', is that valid?  (as 0n+0)
    else {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      UngetToken();
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
  }

  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }
  if (lookForB && !mToken.IsSymbol(')')) {
    // The '+' or '-' sign can optionally be separated by whitespace.
    // If it is separated by whitespace from what follows it, it appears
    // as a separate token rather than part of the number token.
    if (mToken.IsSymbol('+') || mToken.IsSymbol('-')) {
      hasSign = true;
      if (mToken.IsSymbol('-')) {
        sign = -1;
      }
      if (! GetToken(true)) {
        REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
        return eSelectorParsingStatus_Error;
      }
    }
    if (eCSSToken_Number != mToken.mType ||
        !mToken.mIntegerValid || mToken.mHasSign == hasSign) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      UngetToken();
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
    numbers[1] = mToken.mInteger * sign;
    if (! GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
      return eSelectorParsingStatus_Error;
    }
  }
  if (!mToken.IsSymbol(')')) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoClose);
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
  }
  aSelector.AddPseudoClass(aType, numbers);
  return eSelectorParsingStatus_Continue;
}

//
// Parse the argument of a pseudo-class that has a selector list argument.
// Such selector lists cannot contain combinators, but can contain
// anything that goes between a pair of combinators.
//
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParsePseudoClassWithSelectorListArg(nsCSSSelector& aSelector,
                                                   CSSPseudoClassType aType)
{
  nsAutoPtr<nsCSSSelectorList> slist;
  if (! ParseSelectorList(*getter_Transfers(slist), char16_t(')'))) {
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
  }

  // Check that none of the selectors in the list have combinators or
  // pseudo-elements.
  for (nsCSSSelectorList *l = slist; l; l = l->mNext) {
    nsCSSSelector *s = l->mSelectors;
    if (s->mNext || s->IsPseudoElement()) {
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
  }

  // Add the pseudo with the selector list parameter
  aSelector.AddPseudoClass(aType, slist.forget());

  // close the parenthesis
  if (!ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoClose);
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
  }

  return eSelectorParsingStatus_Continue;
}


/**
 * This is the format for selectors:
 * operator? [[namespace |]? element_name]? [ ID | class | attrib | pseudo ]*
 */
bool
CSSParserImpl::ParseSelector(nsCSSSelectorList* aList,
                             char16_t aPrevCombinator)
{
  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PESelectorEOF);
    return false;
  }

  nsCSSSelector* selector = aList->AddSelector(aPrevCombinator);
  RefPtr<nsAtom> pseudoElement;
  nsAutoPtr<nsAtomList> pseudoElementArgs;
  CSSPseudoElementType pseudoElementType = CSSPseudoElementType::NotPseudo;

  int32_t dataMask = 0;
  nsSelectorParsingStatus parsingStatus =
    ParseTypeOrUniversalSelector(dataMask, *selector, false);

  while (parsingStatus == eSelectorParsingStatus_Continue) {
    if (mToken.IsSymbol(':')) {    // :pseudo
      parsingStatus = ParsePseudoSelector(dataMask, *selector, false,
                                          getter_AddRefs(pseudoElement),
                                          getter_Transfers(pseudoElementArgs),
                                          &pseudoElementType);
      if (pseudoElement &&
          pseudoElementType != CSSPseudoElementType::InheritingAnonBox &&
          pseudoElementType != CSSPseudoElementType::NonInheritingAnonBox) {
        // Pseudo-elements other than anonymous boxes are represented with
        // a special ':' combinator.

        aList->mWeight += selector->CalcWeight();

        selector = aList->AddSelector(':');

        selector->mLowercaseTag.swap(pseudoElement);
        selector->mClassList = pseudoElementArgs.forget();
        selector->SetPseudoType(pseudoElementType);
      }
    } else if (selector->IsPseudoElement()) {
      // Once we parsed a pseudo-element, we can only parse
      // pseudo-classes (and only a limited set, which
      // ParsePseudoSelector knows how to handle).
      parsingStatus = eSelectorParsingStatus_Done;
      UngetToken();
      break;
    } else if (eCSSToken_ID == mToken.mType) { // #id
      parsingStatus = ParseIDSelector(dataMask, *selector);
    } else if (mToken.IsSymbol('.')) {    // .class
      parsingStatus = ParseClassSelector(dataMask, *selector);
    }
    else if (mToken.IsSymbol('[')) {    // [attribute
      parsingStatus = ParseAttributeSelector(dataMask, *selector);
      if (eSelectorParsingStatus_Error == parsingStatus) {
        SkipUntil(']');
      }
    }
    else {  // not a selector token, we're done
      parsingStatus = eSelectorParsingStatus_Done;
      UngetToken();
      break;
    }

    if (parsingStatus != eSelectorParsingStatus_Continue) {
      break;
    }

    if (! GetToken(false)) { // premature eof is ok (here!)
      parsingStatus = eSelectorParsingStatus_Done;
      break;
    }
  }

  if (parsingStatus == eSelectorParsingStatus_Error) {
    return false;
  }

  if (!dataMask) {
    if (selector->mNext) {
      REPORT_UNEXPECTED(PESelectorGroupExtraCombinator);
    } else {
      REPORT_UNEXPECTED(PESelectorGroupNoSelector);
    }
    return false;
  }

  if (pseudoElementType == CSSPseudoElementType::InheritingAnonBox ||
      pseudoElementType == CSSPseudoElementType::NonInheritingAnonBox) {
    // We got an anonymous box pseudo-element; it must be the only
    // thing in this selector group.
    if (selector->mNext || !IsUniversalSelector(*selector)) {
      REPORT_UNEXPECTED(PEAnonBoxNotAlone);
      return false;
    }

    // Rewrite the current selector as this pseudo-element.
    // It does not contribute to selector weight.
    selector->mLowercaseTag.swap(pseudoElement);
    selector->mClassList = pseudoElementArgs.forget();
    selector->SetPseudoType(pseudoElementType);
    return true;
  }

  aList->mWeight += selector->CalcWeight();

  return true;
}

already_AddRefed<css::Declaration>
CSSParserImpl::ParseDeclarationBlock(uint32_t aFlags, nsCSSContextType aContext)
{
  bool checkForBraces = (aFlags & eParseDeclaration_InBraces) != 0;

  MOZ_ASSERT(mWebkitBoxUnprefixState == eNotParsingDecls,
             "Someone forgot to clear mWebkitBoxUnprefixState!");
  AutoRestore<WebkitBoxUnprefixState> autoRestore(mWebkitBoxUnprefixState);
  mWebkitBoxUnprefixState = eHaveNotUnprefixed;

  if (checkForBraces) {
    if (!ExpectSymbol('{', true)) {
      REPORT_UNEXPECTED_TOKEN(PEBadDeclBlockStart);
      OUTPUT_ERROR();
      return nullptr;
    }
  }
  RefPtr<css::Declaration> declaration = new css::Declaration();
  mData.AssertInitialState();
  for (;;) {
    bool changed = false;
    if (!ParseDeclaration(declaration, aFlags, true, &changed, aContext)) {
      if (!SkipDeclaration(checkForBraces)) {
        break;
      }
      if (checkForBraces) {
        if (ExpectSymbol('}', true)) {
          break;
        }
      }
      // Since the skipped declaration didn't end the block we parse
      // the next declaration.
    }
  }
  declaration->CompressFrom(&mData);
  return declaration.forget();
}

static Maybe<int32_t>
GetEnumColorValue(nsCSSKeyword aKeyword, bool aIsChrome)
{
  int32_t value;
  if (!nsCSSProps::FindKeyword(aKeyword, nsCSSProps::kColorKTable, value)) {
    // Unknown color keyword.
    return Nothing();
  }
  if (value < 0) {
    // Known special color keyword handled by style system,
    // e.g. NS_COLOR_CURRENTCOLOR. See nsStyleConsts.h.
    return Some(value);
  }
  nscolor color;
  auto colorID = static_cast<LookAndFeel::ColorID>(value);
  if (NS_FAILED(LookAndFeel::GetColor(colorID, !aIsChrome, &color))) {
    // Known LookAndFeel::ColorID, but this platform's LookAndFeel impl
    // doesn't map it to a color. (This might be a platform-specific
    // ColorID, which only makes sense on another platform.)
    return Nothing();
  }
  // Known color provided by LookAndFeel.
  return Some(value);
}

/// Returns the number of digits in a positive number
/// assuming it has <= 6 digits
static uint32_t
CountNumbersForHashlessColor(uint32_t number) {
  /// Just use a simple match instead of calculating a log
  /// or dividing in a loop to be more efficient.
  if (number < 10) {
    return 1;
  } else if (number < 100) {
    return 2;
  } else if (number < 1000) {
    return 3;
  } else if (number < 10000) {
    return 4;
  } else if (number < 100000) {
    return 5;
  } else if (number < 1000000) {
    return 6;
  } else {
    // we don't care about numbers with more than 6 digits other
    // than the fact that they have more than 6 digits, so just return something
    // larger than 6 here. This is incorrect in the general case.
    return 100;
  }
}


CSSParseResult
CSSParserImpl::ParseColor(nsCSSValue& aValue)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorEOF);
    return CSSParseResult::NotFound;
  }

  nsCSSToken* tk = &mToken;
  nscolor rgba;
  switch (tk->mType) {
    case eCSSToken_ID:
    case eCSSToken_Hash:
      // #rgb, #rrggbb, #rgba, #rrggbbaa
      if (NS_HexToRGBA(tk->mIdent, nsHexColorType::AllowAlpha, &rgba)) {
        nsCSSUnit unit;
        switch (tk->mIdent.Length()) {
          case 3:
            unit = eCSSUnit_ShortHexColor;
            break;
          case 4:
            unit = eCSSUnit_ShortHexColorAlpha;
            break;
          case 6:
            unit = eCSSUnit_HexColor;
            break;
          default:
            MOZ_FALLTHROUGH_ASSERT("unexpected hex color length");
          case 8:
            unit = eCSSUnit_HexColorAlpha;
            break;
        }
        aValue.SetIntegerColorValue(rgba, unit);
        return CSSParseResult::Ok;
      }
      break;

    case eCSSToken_Ident: {
      if (NS_ColorNameToRGB(tk->mIdent, &rgba)) {
        // Lowercase color name, since keyword values should be
        // serialized in lowercase.
        nsContentUtils::ASCIIToLower(tk->mIdent);
        aValue.SetStringValue(tk->mIdent, eCSSUnit_Ident);
        return CSSParseResult::Ok;
      }
      nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
      if (Maybe<int32_t> value = GetEnumColorValue(keyword, mIsChrome)) {
        aValue.SetIntValue(value.value(), eCSSUnit_EnumColor);
        return CSSParseResult::Ok;
      }
      break;
    }
    case eCSSToken_Function: {
      bool isRGB;
      bool isHSL;
      if ((isRGB = mToken.mIdent.LowerCaseEqualsLiteral("rgb")) ||
          mToken.mIdent.LowerCaseEqualsLiteral("rgba")) {
        // rgb() = rgb( <percentage>{3} [ / <alpha-value> ]? ) |
        //         rgb( <number>{3} [ / <alpha-value> ]? ) |
        //         rgb( <percentage>#{3} , <alpha-value>? ) |
        //         rgb( <number>#{3} , <alpha-value>? )
        // <alpha-value> = <number> | <percentage>
        // rgba is an alias of rgb.

        if (GetToken(true)) {
          UngetToken();
        }
        if (mToken.mType == eCSSToken_Number) { // <number>
          uint8_t r, g, b, a;

          if (ParseRGBColor(r, g, b, a)) {
            aValue.SetIntegerColorValue(NS_RGBA(r, g, b, a),
                isRGB ? eCSSUnit_RGBColor : eCSSUnit_RGBAColor);
            return CSSParseResult::Ok;
          }
        } else {  // <percentage>
          float r, g, b, a;

          if (ParseRGBColor(r, g, b, a)) {
            aValue.SetFloatColorValue(r, g, b, a,
                isRGB ? eCSSUnit_PercentageRGBColor : eCSSUnit_PercentageRGBAColor);
            return CSSParseResult::Ok;
          }
        }
        SkipUntil(')');
        return CSSParseResult::Error;
      }
      else if ((isHSL = mToken.mIdent.LowerCaseEqualsLiteral("hsl")) ||
               mToken.mIdent.LowerCaseEqualsLiteral("hsla")) {
        // hsl() = hsl( <hue> <percentage> <percentage> [ / <alpha-value> ]? ) ||
        //         hsl( <hue>, <percentage>, <percentage>, <alpha-value>? )
        // <hue> = <number> | <angle>
        // hsla is an alias of hsl.

        float h, s, l, a;

        if (ParseHSLColor(h, s, l, a)) {
          aValue.SetFloatColorValue(h, s, l, a,
              isHSL ? eCSSUnit_HSLColor : eCSSUnit_HSLAColor);
          return CSSParseResult::Ok;
        }
        SkipUntil(')');
        return CSSParseResult::Error;
      }
      break;
    }
    default:
      break;
  }

  // try 'xxyyzz' without '#' prefix for compatibility with IE and Nav4x (bug 23236 and 45804)
  if (mHashlessColorQuirk) {
    // https://quirks.spec.whatwg.org/#the-hashless-hex-color-quirk
    //
    // - If the string starts with 'a-f', the nsCSSScanner builds the
    //   token as a eCSSToken_Ident and we can parse the string as a
    //   'xxyyzz' RGB color.
    // - If it only contains up to six '0-9' digits, the token is a
    //   eCSSToken_Number and it must be converted back to a 6
    //   characters string to be parsed as a RGB color. The number cannot
    //   be specified as more than six digits.
    // - If it starts with '0-9' and contains any 'a-f', the token is a
    //   eCSSToken_Dimension, the mNumber part must be converted back to
    //   a string and the mIdent part must be appended to that string so
    //   that the resulting string has 6 characters. The combined
    //   dimension cannot be longer than 6 characters.
    // Note: This is a hack for Nav compatibility.  Do not attempt to
    // simplify it by hacking into the ncCSSScanner.  This would be very
    // bad.
    nsAutoString str;
    char buffer[20];
    switch (tk->mType) {
      case eCSSToken_Ident:
        str.Assign(tk->mIdent);
        break;

      case eCSSToken_Number:
        if (tk->mIntegerValid && tk->mInteger < 1000000 && tk->mInteger >= 0) {
          SprintfLiteral(buffer, "%06d", tk->mInteger);
          CopyASCIItoUTF16(buffer, str);
        }
        break;

      case eCSSToken_Dimension:
        if (tk->mIntegerValid &&
            tk->mIdent.Length() + CountNumbersForHashlessColor(tk->mInteger) <= 6 &&
            tk->mInteger >= 0) {
          SprintfLiteral(buffer, "%06d", tk->mInteger);
          nsAutoString temp;
          CopyASCIItoUTF16(buffer, temp);
          temp.Right(str, 6 - tk->mIdent.Length());
          str.Append(tk->mIdent);
        }
        break;
      default:
        // There is a whole bunch of cases that are
        // not handled by this switch.  Ignore them.
        break;
    }
    // The hashless color quirk does not support 4 & 8 digit colors with alpha.
    if (NS_HexToRGBA(str, nsHexColorType::NoAlpha, &rgba)) {
      aValue.SetIntegerColorValue(rgba, eCSSUnit_HexColor);
      return CSSParseResult::Ok;
    }
  }

  // It's not a color
  REPORT_UNEXPECTED_TOKEN(PEColorNotColor);
  UngetToken();
  return CSSParseResult::NotFound;
}

bool
CSSParserImpl::ParseColorComponent(uint8_t& aComponent, const Maybe<char>& aSeparator)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorComponentEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return false;
  }

  float value = mToken.mNumber;

  if (aSeparator && !ExpectSymbol(*aSeparator, true)) {
    REPORT_UNEXPECTED_TOKEN_CHAR(PEColorComponentBadTerm, *aSeparator);
    return false;
  }

  if (value < 0.0f) value = 0.0f;
  if (value > 255.0f) value = 255.0f;

  aComponent = NSToIntRound(value);
  return true;
}

bool
CSSParserImpl::ParseColorComponent(float& aComponent, const Maybe<char>& aSeparator)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorComponentEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
    UngetToken();
    return false;
  }

  float value = mToken.mNumber;

  if (aSeparator && !ExpectSymbol(*aSeparator, true)) {
    REPORT_UNEXPECTED_TOKEN_CHAR(PEColorComponentBadTerm, *aSeparator);
    return false;
  }

  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;

  aComponent = value;
  return true;
}

bool
CSSParserImpl::ParseHue(float& aAngle)
{
  nsCSSValue value;
  // <hue> = <number> | <angle>
  // https://drafts.csswg.org/css-color/#typedef-hue
  if (!ParseSingleTokenVariant(value,
                               VARIANT_NUMBER | VARIANT_ANGLE,
                               nullptr)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumberOrAngle);
    return false;
  }

  float unclampedResult;
  if (value.GetUnit() == eCSSUnit_Number) {
    unclampedResult = value.GetFloatValue();
  } else {
    // Convert double value of GetAngleValueInDegrees() to float.
    unclampedResult = value.GetAngleValueInDegrees();
  }

  // Clamp it as finite values in float.
  aAngle = mozilla::clamped(unclampedResult,
                            -std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max());
  return true;
}

bool
CSSParserImpl::ParseHSLColor(float& aHue, float& aSaturation, float& aLightness,
                             float& aOpacity)
{
  // comma-less expression:
  // hsl() = hsl( <hue> <saturation> <lightness> [ / <alpha-value> ]? )
  // the expression with comma:
  // hsl() = hsl( <hue>, <saturation>, <lightness>, <alpha-value>? )

  const char commaSeparator = ',';

  // Parse hue.
  // <hue> = <number> | <angle>
  float degreeAngle;
  if (!ParseHue(degreeAngle)) {
    return false;
  }
  aHue = degreeAngle / 360.0f;
  // hue values are wraparound
  aHue = aHue - floor(aHue);
  // Look for a comma separator after "hue" component to determine if the
  // expression is comma-less or not.
  bool hasComma = ExpectSymbol(commaSeparator, true);

  // Parse saturation, lightness and opacity.
  // The saturation and lightness are <percentage>, so reuse the float version
  // of ParseColorComponent function for them. No need to check the separator
  // after 'lightness'. It will be checked in opacity value parsing.
  const char separatorBeforeAlpha = hasComma ? commaSeparator : '/';
  if (ParseColorComponent(aSaturation, hasComma ? Some(commaSeparator) : Nothing()) &&
      ParseColorComponent(aLightness, Nothing()) &&
      ParseColorOpacityAndCloseParen(aOpacity, separatorBeforeAlpha)) {
    return true;
  }

  return false;
}


bool
CSSParserImpl::ParseColorOpacityAndCloseParen(uint8_t& aOpacity,
                                              char aSeparator)
{
  float floatOpacity;
  if (!ParseColorOpacityAndCloseParen(floatOpacity, aSeparator)) {
    return false;
  }

  uint8_t value = nsStyleUtil::FloatToColorComponent(floatOpacity);
  // Need to compare to something slightly larger
  // than 0.5 due to floating point inaccuracies.
  NS_ASSERTION(fabs(255.0f * floatOpacity - value) <= 0.51f,
               "FloatToColorComponent did something weird");

  aOpacity = value;
  return true;
}

bool
CSSParserImpl::ParseColorOpacityAndCloseParen(float& aOpacity,
                                              char aSeparator)
{
  if (ExpectSymbol(')', true)) {
    // The optional [separator <alpha-value>] was omitted, so set the opacity
    // to a fully-opaque value '1.0f' and return success.
    aOpacity = 1.0f;
    return true;
  }

  if (!ExpectSymbol(aSeparator, true)) {
    REPORT_UNEXPECTED_TOKEN_CHAR(PEColorComponentBadTerm, aSeparator);
    return false;
  }

  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorOpacityEOF);
    return false;
  }

  // eCSSToken_Number or eCSSToken_Percentage.
  if (mToken.mType != eCSSToken_Number && mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumberOrPercent);
    UngetToken();
    return false;
  }

  if (!ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
    return false;
  }

  if (mToken.mNumber < 0.0f) {
    mToken.mNumber = 0.0f;
  } else if (mToken.mNumber > 1.0f) {
    mToken.mNumber = 1.0f;
  }

  aOpacity = mToken.mNumber;
  return true;
}

template<typename ComponentType>
bool
CSSParserImpl::ParseRGBColor(ComponentType& aR,
                             ComponentType& aG,
                             ComponentType& aB,
                             ComponentType& aA)
{
  // comma-less expression:
  // rgb() = rgb( component{3} [ / <alpha-value> ]? )
  // the expression with comma:
  // rgb() = rgb( component#{3} , <alpha-value>? )

  const char commaSeparator = ',';

  // Parse R.
  if (!ParseColorComponent(aR, Nothing())) {
    return false;
  }
  // Look for a comma separator after "r" component to determine if the
  // expression is comma-less or not.
  bool hasComma = ExpectSymbol(commaSeparator, true);

  // Parse G, B and A.
  // No need to check the separator after 'B'. It will be checked in 'A' value
  // parsing.
  const char separatorBeforeAlpha = hasComma ? commaSeparator : '/';
  if (ParseColorComponent(aG, hasComma ? Some(commaSeparator) : Nothing()) &&
      ParseColorComponent(aB, Nothing()) &&
      ParseColorOpacityAndCloseParen(aA, separatorBeforeAlpha)) {
    return true;
  }

  return false;
}

#ifdef MOZ_XUL
bool
CSSParserImpl::ParseTreePseudoElement(nsAtomList **aPseudoElementArgs)
{
  // The argument to a tree pseudo-element is a sequence of identifiers
  // that are either space- or comma-separated.  (Was the intent to
  // allow only comma-separated?  That's not what was done.)
  nsCSSSelector fakeSelector; // so we can reuse AddPseudoClass

  while (!ExpectSymbol(')', true)) {
    if (!GetToken(true)) {
      return false;
    }
    if (eCSSToken_Ident == mToken.mType) {
      fakeSelector.AddClass(mToken.mIdent);
    }
    else if (!mToken.IsSymbol(',')) {
      UngetToken();
      SkipUntil(')');
      return false;
    }
  }
  *aPseudoElementArgs = fakeSelector.mClassList;
  fakeSelector.mClassList = nullptr;
  return true;
}
#endif

nsCSSKeyword
CSSParserImpl::LookupKeywordPrefixAware(nsAString& aKeywordStr,
                                        const KTableEntry aKeywordTable[])
{
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aKeywordStr);

  if (!StylePrefs::sWebkitPrefixedAliasesEnabled) {
    // Not accepting webkit-prefixed keywords -> don't do anything special.
    return keyword;
  }

  if (aKeywordTable == nsCSSProps::kDisplayKTable) {
    if ((keyword == eCSSKeyword__webkit_box ||
         keyword == eCSSKeyword__webkit_inline_box)) {
      // Make a note that we're accepting some "-webkit-{inline-}box" styling,
      // so we can give special treatment to subsequent "-moz-{inline}-box".
      // (See special treatment below.)
      if (mWebkitBoxUnprefixState == eHaveNotUnprefixed) {
        mWebkitBoxUnprefixState = eHaveUnprefixed;
      }
    } else if (mWebkitBoxUnprefixState == eHaveUnprefixed &&
               (keyword == eCSSKeyword__moz_box ||
                keyword == eCSSKeyword__moz_inline_box)) {
      // If we've seen "display: -webkit-box" (or "-webkit-inline-box") in an
      // earlier declaration and we honored it, then we have to watch out for
      // later "display: -moz-box" (and "-moz-inline-box") declarations; they're
      // likely just a halfhearted attempt at compatibility, and they actually
      // end up stomping on our emulation of the earlier -webkit-box
      // display-value, via the CSS cascade. To prevent this problem, we treat
      // "display: -moz-box" & "-moz-inline-box" as if they were simply a
      // repetition of the webkit equivalent that we already parsed.
      MOZ_ASSERT(StylePrefs::sWebkitPrefixedAliasesEnabled,
                 "The only way mWebkitBoxUnprefixState can be eHaveUnprefixed "
                 "is if we're supporting webkit-prefixed aliases");
      return (keyword == eCSSKeyword__moz_box) ?
        eCSSKeyword__webkit_box : eCSSKeyword__webkit_inline_box;
    }
  }

  return keyword;
}

//----------------------------------------------------------------------

bool
CSSParserImpl::ParseDeclaration(css::Declaration* aDeclaration,
                                uint32_t aFlags,
                                bool aMustCallValueAppended,
                                bool* aChanged,
                                nsCSSContextType aContext)
{
  NS_PRECONDITION(aContext == eCSSContext_General ||
                  aContext == eCSSContext_Page,
                  "Must be page or general context");

  bool checkForBraces = (aFlags & eParseDeclaration_InBraces) != 0;

  mTempData.AssertInitialState();

  // Get property name
  nsCSSToken* tk = &mToken;
  nsAutoString propertyName;
  for (;;) {
    if (!GetToken(true)) {
      if (checkForBraces) {
        REPORT_UNEXPECTED_EOF(PEDeclEndEOF);
      }
      return false;
    }
    if (eCSSToken_Ident == tk->mType) {
      propertyName = tk->mIdent;
      // grab the ident before the ExpectSymbol trashes the token
      if (!ExpectSymbol(':', true)) {
        REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
        REPORT_UNEXPECTED(PEDeclDropped);
        OUTPUT_ERROR();
        return false;
      }
      break;
    }
    if (tk->IsSymbol(';')) {
      // dangling semicolons are skipped
      continue;
    }

    if (!tk->IsSymbol('}')) {
      REPORT_UNEXPECTED_TOKEN(PEParseDeclarationDeclExpected);
      REPORT_UNEXPECTED(PEDeclSkipped);
      OUTPUT_ERROR();

      if (eCSSToken_AtKeyword == tk->mType) {
        SkipAtRule(checkForBraces);
        return true;  // Not a declaration, but don't skip until ';'
      }
    }
    // Not a declaration...
    UngetToken();
    return false;
  }

  // Don't report property parse errors if we're inside a failing @supports
  // rule.
  nsAutoSuppressErrors suppressErrors(this, mInFailingSupportsRule);

  // Information about a parsed non-custom property.
  nsCSSPropertyID propID;

  // Information about a parsed custom property.
  CSSVariableDeclarations::Type variableType;
  nsString variableValue;

  // Check if the property name is a custom property.
  bool customProperty = nsCSSProps::IsCustomPropertyName(propertyName) &&
                        aContext == eCSSContext_General;

  if (customProperty) {
    if (!ParseVariableDeclaration(&variableType, variableValue)) {
      REPORT_UNEXPECTED_P(PEValueParsingError, propertyName);
      REPORT_UNEXPECTED(PEDeclDropped);
      OUTPUT_ERROR();
      return false;
    }
  } else {
    // Map property name to its ID.
    propID = LookupEnabledProperty(propertyName);
    if (eCSSProperty_UNKNOWN == propID ||
        eCSSPropertyExtra_variable == propID ||
        (aContext == eCSSContext_Page &&
         !nsCSSProps::PropHasFlags(propID,
                                   CSS_PROPERTY_APPLIES_TO_PAGE_RULE))) { // unknown property
      if (!NonMozillaVendorIdentifier(propertyName)) {
        REPORT_UNEXPECTED_P(PEUnknownProperty, propertyName);
        REPORT_UNEXPECTED(PEDeclDropped);
        OUTPUT_ERROR();
      }
      return false;
    }
    // Then parse the property.
    if (!ParseProperty(propID)) {
      // XXX Much better to put stuff in the value parsers instead...
      REPORT_UNEXPECTED_P(PEValueParsingError, propertyName);
      REPORT_UNEXPECTED(PEDeclDropped);
      OUTPUT_ERROR();
      mTempData.ClearProperty(propID);
      mTempData.AssertInitialState();
      return false;
    }
  }

  CLEAR_ERROR();

  // Look for "!important".
  PriorityParsingStatus status;
  if ((aFlags & eParseDeclaration_AllowImportant) != 0) {
    status = ParsePriority();
  } else {
    status = ePriority_None;
  }

  // Look for a semicolon or close brace.
  if (status != ePriority_Error) {
    if (!GetToken(true)) {
      // EOF is always ok
    } else if (mToken.IsSymbol(';')) {
      // semicolon is always ok
    } else if (mToken.IsSymbol('}')) {
      // brace is ok if checkForBraces, but don't eat it
      UngetToken();
      if (!checkForBraces) {
        status = ePriority_Error;
      }
    } else {
      UngetToken();
      status = ePriority_Error;
    }
  }

  if (status == ePriority_Error) {
    if (checkForBraces) {
      REPORT_UNEXPECTED_TOKEN(PEBadDeclOrRuleEnd2);
    } else {
      REPORT_UNEXPECTED_TOKEN(PEBadDeclEnd);
    }
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    if (!customProperty) {
      mTempData.ClearProperty(propID);
    }
    mTempData.AssertInitialState();
    return false;
  }

  if (customProperty) {
    MOZ_ASSERT(Substring(propertyName, 0,
                         CSS_CUSTOM_NAME_PREFIX_LENGTH).EqualsLiteral("--"));
    // remove '--'
    nsDependentString varName(propertyName, CSS_CUSTOM_NAME_PREFIX_LENGTH);
    aDeclaration->AddVariable(varName, variableType, variableValue,
                              status == ePriority_Important, false);
  } else {
    *aChanged |= mData.TransferFromBlock(mTempData, propID, EnabledState(),
                                         status == ePriority_Important,
                                         false, aMustCallValueAppended,
                                         aDeclaration, GetDocument());
  }

  return true;
}

static const nsCSSPropertyID kBorderTopIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color
};
static const nsCSSPropertyID kBorderRightIDs[] = {
  eCSSProperty_border_right_width,
  eCSSProperty_border_right_style,
  eCSSProperty_border_right_color
};
static const nsCSSPropertyID kBorderBottomIDs[] = {
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_color
};
static const nsCSSPropertyID kBorderLeftIDs[] = {
  eCSSProperty_border_left_width,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_color
};
static const nsCSSPropertyID kBorderInlineStartIDs[] = {
  eCSSProperty_border_inline_start_width,
  eCSSProperty_border_inline_start_style,
  eCSSProperty_border_inline_start_color
};
static const nsCSSPropertyID kBorderInlineEndIDs[] = {
  eCSSProperty_border_inline_end_width,
  eCSSProperty_border_inline_end_style,
  eCSSProperty_border_inline_end_color
};
static const nsCSSPropertyID kBorderBlockStartIDs[] = {
  eCSSProperty_border_block_start_width,
  eCSSProperty_border_block_start_style,
  eCSSProperty_border_block_start_color
};
static const nsCSSPropertyID kBorderBlockEndIDs[] = {
  eCSSProperty_border_block_end_width,
  eCSSProperty_border_block_end_style,
  eCSSProperty_border_block_end_color
};
static const nsCSSPropertyID kColumnRuleIDs[] = {
  eCSSProperty_column_rule_width,
  eCSSProperty_column_rule_style,
  eCSSProperty_column_rule_color
};

bool
CSSParserImpl::ParseEnum(nsCSSValue& aValue,
                         const KTableEntry aKeywordTable[])
{
  nsAString* ident = NextIdent();
  if (nullptr == ident) {
    return false;
  }
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(*ident);
  int32_t value;
  if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
    aValue.SetIntValue(value, eCSSUnit_Enumerated);
    return true;
  }

  // Put the unknown identifier back and return
  UngetToken();
  return false;
}

bool
CSSParserImpl::ParseAlignEnum(nsCSSValue& aValue,
                              const KTableEntry aKeywordTable[])
{
  MOZ_ASSERT(nsCSSProps::ValueToKeywordEnum(NS_STYLE_ALIGN_BASELINE,
                                            aKeywordTable) !=
               eCSSKeyword_UNKNOWN,
             "Please use ParseEnum instead");
  nsAString* ident = NextIdent();
  if (!ident) {
    return false;
  }
  nsCSSKeyword baselinePrefix = eCSSKeyword_first;
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(*ident);
  if (keyword == eCSSKeyword_first || keyword == eCSSKeyword_last) {
    baselinePrefix = keyword;
    ident = NextIdent();
    if (!ident) {
      return false;
    }
    keyword = nsCSSKeywords::LookupKeyword(*ident);
  }
  int32_t value;
  if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
    if (baselinePrefix == eCSSKeyword_last &&
        keyword == eCSSKeyword_baseline) {
      value = NS_STYLE_ALIGN_LAST_BASELINE;
    }
    aValue.SetIntValue(value, eCSSUnit_Enumerated);
    return true;
  }

  // Put the unknown identifier back and return
  UngetToken();
  return false;
}

struct UnitInfo {
  char name[6];  // needs to be long enough for the longest unit, with
                 // terminating null.
  uint32_t length;
  nsCSSUnit unit;
  int32_t type;
};

#define STR_WITH_LEN(_str) \
  _str, sizeof(_str) - 1

const UnitInfo UnitData[] = {
  { STR_WITH_LEN("px"), eCSSUnit_Pixel, VARIANT_LENGTH },
  { STR_WITH_LEN("em"), eCSSUnit_EM, VARIANT_LENGTH },
  { STR_WITH_LEN("ex"), eCSSUnit_XHeight, VARIANT_LENGTH },
  { STR_WITH_LEN("pt"), eCSSUnit_Point, VARIANT_LENGTH },
  { STR_WITH_LEN("in"), eCSSUnit_Inch, VARIANT_LENGTH },
  { STR_WITH_LEN("cm"), eCSSUnit_Centimeter, VARIANT_LENGTH },
  { STR_WITH_LEN("ch"), eCSSUnit_Char, VARIANT_LENGTH },
  { STR_WITH_LEN("rem"), eCSSUnit_RootEM, VARIANT_LENGTH },
  { STR_WITH_LEN("mm"), eCSSUnit_Millimeter, VARIANT_LENGTH },
  { STR_WITH_LEN("vw"), eCSSUnit_ViewportWidth, VARIANT_LENGTH },
  { STR_WITH_LEN("vh"), eCSSUnit_ViewportHeight, VARIANT_LENGTH },
  { STR_WITH_LEN("vmin"), eCSSUnit_ViewportMin, VARIANT_LENGTH },
  { STR_WITH_LEN("vmax"), eCSSUnit_ViewportMax, VARIANT_LENGTH },
  { STR_WITH_LEN("pc"), eCSSUnit_Pica, VARIANT_LENGTH },
  { STR_WITH_LEN("q"), eCSSUnit_Quarter, VARIANT_LENGTH },
  { STR_WITH_LEN("deg"), eCSSUnit_Degree, VARIANT_ANGLE },
  { STR_WITH_LEN("grad"), eCSSUnit_Grad, VARIANT_ANGLE },
  { STR_WITH_LEN("rad"), eCSSUnit_Radian, VARIANT_ANGLE },
  { STR_WITH_LEN("turn"), eCSSUnit_Turn, VARIANT_ANGLE },
  { STR_WITH_LEN("hz"), eCSSUnit_Hertz, VARIANT_FREQUENCY },
  { STR_WITH_LEN("khz"), eCSSUnit_Kilohertz, VARIANT_FREQUENCY },
  { STR_WITH_LEN("s"), eCSSUnit_Seconds, VARIANT_TIME },
  { STR_WITH_LEN("ms"), eCSSUnit_Milliseconds, VARIANT_TIME }
};

#undef STR_WITH_LEN

bool
CSSParserImpl::TranslateDimension(nsCSSValue& aValue,
                                  uint32_t aVariantMask,
                                  float aNumber,
                                  const nsString& aUnit)
{
  nsCSSUnit units;
  int32_t   type = 0;
  if (!aUnit.IsEmpty()) {
    uint32_t i;
    for (i = 0; i < ArrayLength(UnitData); ++i) {
      if (aUnit.LowerCaseEqualsASCII(UnitData[i].name,
                                     UnitData[i].length)) {
        units = UnitData[i].unit;
        type = UnitData[i].type;
        break;
      }
    }

    if (i == ArrayLength(UnitData)) {
      // Unknown unit
      return false;
    }

    if (!mViewportUnitsEnabled &&
        (eCSSUnit_ViewportWidth == units  ||
         eCSSUnit_ViewportHeight == units ||
         eCSSUnit_ViewportMin == units    ||
         eCSSUnit_ViewportMax == units)) {
      // Viewport units aren't allowed right now, probably because we're
      // inside an @page declaration. Fail.
      return false;
    }

    if ((VARIANT_ABSOLUTE_DIMENSION & aVariantMask) != 0 &&
        !nsCSSValue::IsPixelLengthUnit(units)) {
      return false;
    }
  } else {
    // Must be a zero number...
    NS_ASSERTION(0 == aNumber, "numbers without units must be 0");
    if ((VARIANT_LENGTH & aVariantMask) != 0) {
      units = eCSSUnit_Pixel;
      type = VARIANT_LENGTH;
    }
    else if ((VARIANT_ANGLE & aVariantMask) != 0) {
      NS_ASSERTION(aVariantMask & VARIANT_ZERO_ANGLE,
                   "must have allowed zero angle");
      units = eCSSUnit_Degree;
      type = VARIANT_ANGLE;
    }
    else {
      NS_ERROR("Variant mask does not include dimension; why were we called?");
      return false;
    }
  }
  if ((type & aVariantMask) != 0) {
    aValue.SetFloatValue(aNumber, units);
    return true;
  }
  return false;
}

// Note that this does include VARIANT_CALC, which is numeric.  This is
// because calc() parsing, as proposed, drops range restrictions inside
// the calc() expression and clamps the result of the calculation to the
// range.
#define VARIANT_ALL_NONNUMERIC \
  VARIANT_KEYWORD | \
  VARIANT_COLOR | \
  VARIANT_URL | \
  VARIANT_STRING | \
  VARIANT_COUNTER | \
  VARIANT_ATTR | \
  VARIANT_IDENTIFIER | \
  VARIANT_IDENTIFIER_NO_INHERIT | \
  VARIANT_AUTO | \
  VARIANT_INHERIT | \
  VARIANT_NONE | \
  VARIANT_NORMAL | \
  VARIANT_SYSFONT | \
  VARIANT_GRADIENT | \
  VARIANT_TIMING_FUNCTION | \
  VARIANT_ALL | \
  VARIANT_CALC | \
  VARIANT_OPENTYPE_SVG_KEYWORD

CSSParseResult
CSSParserImpl::ParseVariantWithRestrictions(nsCSSValue& aValue,
                                            int32_t aVariantMask,
                                            const KTableEntry aKeywordTable[],
                                            uint32_t aRestrictions)
{
  switch (aRestrictions) {
    default:
      MOZ_FALLTHROUGH_ASSERT("should not be reached");
    case 0:
      return ParseVariant(aValue, aVariantMask, aKeywordTable);
    case CSS_PROPERTY_VALUE_NONNEGATIVE:
      return ParseNonNegativeVariant(aValue, aVariantMask, aKeywordTable);
    case CSS_PROPERTY_VALUE_AT_LEAST_ONE:
      return ParseOneOrLargerVariant(aValue, aVariantMask, aKeywordTable);
  }
}

// Note that callers passing VARIANT_CALC in aVariantMask will get
// full-range parsing inside the calc() expression, and the code that
// computes the calc will be required to clamp the resulting value to an
// appropriate range.
CSSParseResult
CSSParserImpl::ParseNonNegativeVariant(nsCSSValue& aValue,
                                       int32_t aVariantMask,
                                       const KTableEntry aKeywordTable[])
{
  // The variant mask must only contain non-numeric variants or the ones
  // that we specifically handle.
  MOZ_ASSERT((aVariantMask & ~(VARIANT_ALL_NONNUMERIC |
                               VARIANT_NUMBER |
                               VARIANT_LENGTH |
                               VARIANT_PERCENT |
                               VARIANT_INTEGER)) == 0,
             "need to update code below to handle additional variants");

  CSSParseResult result = ParseVariant(aValue, aVariantMask, aKeywordTable);
  if (result == CSSParseResult::Ok) {
    if (eCSSUnit_Number == aValue.GetUnit() ||
        aValue.IsLengthUnit()){
      if (aValue.GetFloatValue() < 0) {
        UngetToken();
        return CSSParseResult::NotFound;
      }
    }
    else if (aValue.GetUnit() == eCSSUnit_Percent) {
      if (aValue.GetPercentValue() < 0) {
        UngetToken();
        return CSSParseResult::NotFound;
      }
    } else if (aValue.GetUnit() == eCSSUnit_Integer) {
      if (aValue.GetIntValue() < 0) {
        UngetToken();
        return CSSParseResult::NotFound;
      }
    }
  }
  return result;
}

// Note that callers passing VARIANT_CALC in aVariantMask will get
// full-range parsing inside the calc() expression, and the code that
// computes the calc will be required to clamp the resulting value to an
// appropriate range.
CSSParseResult
CSSParserImpl::ParseOneOrLargerVariant(nsCSSValue& aValue,
                                       int32_t aVariantMask,
                                       const KTableEntry aKeywordTable[])
{
  // The variant mask must only contain non-numeric variants or the ones
  // that we specifically handle.
  MOZ_ASSERT((aVariantMask & ~(VARIANT_ALL_NONNUMERIC |
                               VARIANT_NUMBER |
                               VARIANT_INTEGER)) == 0,
             "need to update code below to handle additional variants");

  CSSParseResult result = ParseVariant(aValue, aVariantMask, aKeywordTable);
  if (result == CSSParseResult::Ok) {
    if (aValue.GetUnit() == eCSSUnit_Integer) {
      if (aValue.GetIntValue() < 1) {
        UngetToken();
        return CSSParseResult::NotFound;
      }
    } else if (eCSSUnit_Number == aValue.GetUnit()) {
      if (aValue.GetFloatValue() < 1.0f) {
        UngetToken();
        return CSSParseResult::NotFound;
      }
    }
  }
  return result;
}

static bool
IsCSSTokenCalcFunction(const nsCSSToken& aToken)
{
  return aToken.mType == eCSSToken_Function &&
         aToken.mIdent.LowerCaseEqualsLiteral("calc");
}

// Assigns to aValue iff it returns CSSParseResult::Ok.
CSSParseResult
CSSParserImpl::ParseVariant(nsCSSValue& aValue,
                            uint32_t aVariantMask,
                            const KTableEntry aKeywordTable[])
{
  NS_ASSERTION(!(mHashlessColorQuirk && (aVariantMask & VARIANT_COLOR)) ||
               !(aVariantMask & VARIANT_NUMBER),
               "can't distinguish colors from numbers");
  NS_ASSERTION(!(mHashlessColorQuirk && (aVariantMask & VARIANT_COLOR)) ||
               !(mUnitlessLengthQuirk && (aVariantMask & VARIANT_LENGTH)),
               "can't distinguish colors from lengths");
  NS_ASSERTION(!(mUnitlessLengthQuirk && (aVariantMask & VARIANT_LENGTH)) ||
               !(aVariantMask & VARIANT_NUMBER),
               "can't distinguish lengths from numbers");
  MOZ_ASSERT(!(aVariantMask & VARIANT_IDENTIFIER) ||
             !(aVariantMask & VARIANT_IDENTIFIER_NO_INHERIT),
             "must not set both VARIANT_IDENTIFIER and "
             "VARIANT_IDENTIFIER_NO_INHERIT");

  uint32_t lineBefore, colBefore;
  if (!GetNextTokenLocation(true, &lineBefore, &colBefore) ||
      !GetToken(true)) {
    // Must be at EOF.
    return CSSParseResult::NotFound;
  }

  nsCSSToken* tk = &mToken;
  if (((aVariantMask & (VARIANT_AHK | VARIANT_NORMAL | VARIANT_NONE | VARIANT_ALL)) != 0) &&
      (eCSSToken_Ident == tk->mType)) {
    nsCSSKeyword keyword = LookupKeywordPrefixAware(tk->mIdent,
                                                    aKeywordTable);

    if (eCSSKeyword_UNKNOWN < keyword) { // known keyword
      if ((aVariantMask & VARIANT_AUTO) != 0) {
        if (eCSSKeyword_auto == keyword) {
          aValue.SetAutoValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_INHERIT) != 0) {
        // XXX Should we check IsParsingCompoundProperty, or do all
        // callers handle it?  (Not all callers set it, though, since
        // they want the quirks that are disabled by setting it.)

        // IMPORTANT: If new keywords are added here,
        // they probably need to be added in ParseCustomIdent as well.
        if (eCSSKeyword_inherit == keyword) {
          aValue.SetInheritValue();
          return CSSParseResult::Ok;
        }
        else if (eCSSKeyword_initial == keyword) {
          aValue.SetInitialValue();
          return CSSParseResult::Ok;
        }
        else if (eCSSKeyword_unset == keyword &&
                 nsLayoutUtils::UnsetValueEnabled()) {
          aValue.SetUnsetValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_NONE) != 0) {
        if (eCSSKeyword_none == keyword) {
          aValue.SetNoneValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_ALL) != 0) {
        if (eCSSKeyword_all == keyword) {
          aValue.SetAllValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_NORMAL) != 0) {
        if (eCSSKeyword_normal == keyword) {
          aValue.SetNormalValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_SYSFONT) != 0) {
        if (eCSSKeyword__moz_use_system_font == keyword &&
            !IsParsingCompoundProperty()) {
          aValue.SetSystemFontValue();
          return CSSParseResult::Ok;
        }
      }
      if ((aVariantMask & VARIANT_OPENTYPE_SVG_KEYWORD) != 0) {
        if (StylePrefs::sOpentypeSVGEnabled) {
          aVariantMask |= VARIANT_KEYWORD;
        }
      }
      if ((aVariantMask & VARIANT_KEYWORD) != 0) {
        int32_t value;
        if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
          aValue.SetIntValue(value, eCSSUnit_Enumerated);
          return CSSParseResult::Ok;
        }
      }
    }
  }
  // Check VARIANT_NUMBER and VARIANT_INTEGER before VARIANT_LENGTH or
  // VARIANT_ZERO_ANGLE.
  if (((aVariantMask & VARIANT_NUMBER) != 0) &&
      (eCSSToken_Number == tk->mType)) {
    aValue.SetFloatValue(tk->mNumber, eCSSUnit_Number);
    return CSSParseResult::Ok;
  }
  if (((aVariantMask & VARIANT_INTEGER) != 0) &&
      (eCSSToken_Number == tk->mType) && tk->mIntegerValid) {
    aValue.SetIntValue(tk->mInteger, eCSSUnit_Integer);
    return CSSParseResult::Ok;
  }
  if (((aVariantMask & (VARIANT_LENGTH | VARIANT_ANGLE |
                        VARIANT_FREQUENCY | VARIANT_TIME)) != 0 &&
       eCSSToken_Dimension == tk->mType) ||
      ((aVariantMask & (VARIANT_LENGTH | VARIANT_ZERO_ANGLE)) != 0 &&
       eCSSToken_Number == tk->mType &&
       tk->mNumber == 0.0f)) {
    if ((aVariantMask & VARIANT_NONNEGATIVE_DIMENSION) != 0 &&
        tk->mNumber < 0.0) {
        UngetToken();
        AssertNextTokenAt(lineBefore, colBefore);
        return CSSParseResult::NotFound;
    }
    if (TranslateDimension(aValue, aVariantMask, tk->mNumber, tk->mIdent)) {
      return CSSParseResult::Ok;
    }
    // Put the token back; we didn't parse it, so we shouldn't consume it
    UngetToken();
    AssertNextTokenAt(lineBefore, colBefore);
    return CSSParseResult::NotFound;
  }
  if (((aVariantMask & VARIANT_PERCENT) != 0) &&
      (eCSSToken_Percentage == tk->mType)) {
    aValue.SetPercentValue(tk->mNumber);
    return CSSParseResult::Ok;
  }
  if (mUnitlessLengthQuirk) { // NONSTANDARD: Nav interprets unitless numbers as px
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return CSSParseResult::Ok;
    }
  }

  if (mIsSVGMode && !IsParsingCompoundProperty()) {
    // STANDARD: SVG Spec states that lengths and coordinates can be unitless
    // in which case they default to user-units (1 px = 1 user unit)
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return CSSParseResult::Ok;
    }
  }

  if (((aVariantMask & VARIANT_URL) != 0) &&
      eCSSToken_URL == tk->mType) {
    SetValueToURL(aValue, tk->mIdent);
    return CSSParseResult::Ok;
  }
  if ((aVariantMask & VARIANT_GRADIENT) != 0 &&
      eCSSToken_Function == tk->mType) {
    // a generated gradient
    nsDependentString tmp(tk->mIdent, 0);
    uint8_t gradientFlags = 0;
    if (StylePrefs::sMozGradientsEnabled &&
        StringBeginsWith(tmp, NS_LITERAL_STRING("-moz-"))) {
      tmp.Rebind(tmp, 5);
      gradientFlags |= eGradient_MozLegacy;
    } else if (StylePrefs::sWebkitPrefixedAliasesEnabled &&
               StringBeginsWith(tmp, NS_LITERAL_STRING("-webkit-"))) {
      tmp.Rebind(tmp, 8);
      gradientFlags |= eGradient_WebkitLegacy;
    }
    if (StringBeginsWith(tmp, NS_LITERAL_STRING("repeating-"))) {
      tmp.Rebind(tmp, 10);
      gradientFlags |= eGradient_Repeating;
    }

    if (tmp.LowerCaseEqualsLiteral("linear-gradient")) {
      if (!ParseLinearGradient(aValue, gradientFlags)) {
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
    if (tmp.LowerCaseEqualsLiteral("radial-gradient")) {
      if (!ParseRadialGradient(aValue, gradientFlags)) {
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
    if ((gradientFlags == eGradient_WebkitLegacy) &&
        tmp.LowerCaseEqualsLiteral("gradient")) {
      // Note: we check gradientFlags using '==' to select *exactly*
      // eGradient_WebkitLegacy -- and exclude eGradient_Repeating -- because
      // we don't want to accept -webkit-repeating-gradient() expressions.
      // (This is not a recognized syntax.)
      if (!ParseWebkitGradient(aValue)) {
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
  }
  if ((aVariantMask & VARIANT_IMAGE_RECT) != 0 &&
      eCSSToken_Function == tk->mType &&
      tk->mIdent.LowerCaseEqualsLiteral("-moz-image-rect")) {
    if (!ParseImageRect(aValue)) {
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }
  if ((aVariantMask & VARIANT_ELEMENT) != 0 &&
      eCSSToken_Function == tk->mType &&
      tk->mIdent.LowerCaseEqualsLiteral("-moz-element")) {
    if (!ParseElement(aValue)) {
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }
  if ((aVariantMask & VARIANT_COLOR) != 0) {
    if (mHashlessColorQuirk || // NONSTANDARD: Nav interprets 'xxyyzz' values even without '#' prefix
        (eCSSToken_ID == tk->mType) ||
        (eCSSToken_Hash == tk->mType) ||
        (eCSSToken_Ident == tk->mType) ||
        ((eCSSToken_Function == tk->mType) &&
         (tk->mIdent.LowerCaseEqualsLiteral("rgb") ||
          tk->mIdent.LowerCaseEqualsLiteral("hsl") ||
          tk->mIdent.LowerCaseEqualsLiteral("rgba") ||
          tk->mIdent.LowerCaseEqualsLiteral("hsla"))))
    {
      // Put token back so that parse color can get it
      UngetToken();
      return ParseColor(aValue);
    }
  }
  if (((aVariantMask & VARIANT_STRING) != 0) &&
      (eCSSToken_String == tk->mType)) {
    nsAutoString  buffer;
    buffer.Append(tk->mIdent);
    aValue.SetStringValue(buffer, eCSSUnit_String);
    return CSSParseResult::Ok;
  }
  if (((aVariantMask &
        (VARIANT_IDENTIFIER | VARIANT_IDENTIFIER_NO_INHERIT)) != 0) &&
      (eCSSToken_Ident == tk->mType) &&
      ((aVariantMask & VARIANT_IDENTIFIER) != 0 ||
       !(tk->mIdent.LowerCaseEqualsLiteral("inherit") ||
         tk->mIdent.LowerCaseEqualsLiteral("initial") ||
         (tk->mIdent.LowerCaseEqualsLiteral("unset") &&
          nsLayoutUtils::UnsetValueEnabled())))) {
    aValue.SetStringValue(tk->mIdent, eCSSUnit_Ident);
    return CSSParseResult::Ok;
  }
  if (((aVariantMask & VARIANT_COUNTER) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      (tk->mIdent.LowerCaseEqualsLiteral("counter") ||
       tk->mIdent.LowerCaseEqualsLiteral("counters"))) {
    if (!ParseCounter(aValue)) {
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }
  if (((aVariantMask & VARIANT_ATTR) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      tk->mIdent.LowerCaseEqualsLiteral("attr")) {
    if (!ParseAttr(aValue)) {
      SkipUntil(')');
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }
  if (((aVariantMask & VARIANT_TIMING_FUNCTION) != 0) &&
      (eCSSToken_Function == tk->mType)) {
    if (tk->mIdent.LowerCaseEqualsLiteral("cubic-bezier")) {
      if (!ParseTransitionTimingFunctionValues(aValue)) {
        SkipUntil(')');
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
    if (tk->mIdent.LowerCaseEqualsLiteral("steps")) {
      if (!ParseTransitionStepTimingFunctionValues(aValue)) {
        SkipUntil(')');
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
    if (StylePrefs::sFramesTimingFunctionEnabled &&
        tk->mIdent.LowerCaseEqualsLiteral("frames")) {
      if (!ParseTransitionFramesTimingFunctionValues(aValue)) {
        SkipUntil(')');
        return CSSParseResult::Error;
      }
      return CSSParseResult::Ok;
    }
  }
  if ((aVariantMask & VARIANT_CALC) &&
      IsCSSTokenCalcFunction(*tk)) {
    // calc() currently allows only lengths, percents, numbers, and integers.
    //
    // Note that VARIANT_NUMBER can be mixed with VARIANT_LENGTH and
    // VARIANT_PERCENTAGE in the list of allowed types (numbers can be used as
    // coefficients).
    // However, the the resulting type is not a mixed type with number.
    // VARIANT_INTEGER can't be mixed with anything else.
    if (!ParseCalc(aValue, aVariantMask & (VARIANT_LPN | VARIANT_INTEGER))) {
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }

  UngetToken();
  AssertNextTokenAt(lineBefore, colBefore);
  return CSSParseResult::NotFound;
}

bool
CSSParserImpl::ParseCustomIdent(nsCSSValue& aValue,
                                const nsAutoString& aIdentValue,
                                const nsCSSKeyword aExcludedKeywords[],
                                const nsCSSProps::KTableEntry aPropertyKTable[])
{
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aIdentValue);
  if (keyword == eCSSKeyword_UNKNOWN) {
    // Fast path for identifiers that are not known CSS keywords:
    aValue.SetStringValue(mToken.mIdent, eCSSUnit_Ident);
    return true;
  }
  if (keyword == eCSSKeyword_inherit ||
      keyword == eCSSKeyword_initial ||
      keyword == eCSSKeyword_unset ||
      keyword == eCSSKeyword_default ||
      (aPropertyKTable &&
        nsCSSProps::FindIndexOfKeyword(keyword, aPropertyKTable) >= 0)) {
    return false;
  }
  if (aExcludedKeywords) {
    for (uint32_t i = 0;; i++) {
      nsCSSKeyword excludedKeyword = aExcludedKeywords[i];
      if (excludedKeyword == eCSSKeyword_UNKNOWN) {
        break;
      }
      if (excludedKeyword == keyword) {
        return false;
      }
    }
  }
  aValue.SetStringValue(mToken.mIdent, eCSSUnit_Ident);
  return true;
}

bool
CSSParserImpl::ParseCounter(nsCSSValue& aValue)
{
  nsCSSUnit unit = (mToken.mIdent.LowerCaseEqualsLiteral("counter") ?
                    eCSSUnit_Counter : eCSSUnit_Counters);

  // A non-iterative for loop to break out when an error occurs.
  for (;;) {
    if (!GetToken(true)) {
      break;
    }
    if (eCSSToken_Ident != mToken.mType) {
      UngetToken();
      break;
    }

    RefPtr<nsCSSValue::Array> val =
      nsCSSValue::Array::Create(unit == eCSSUnit_Counter ? 2 : 3);

    val->Item(0).SetStringValue(mToken.mIdent, eCSSUnit_Ident);

    if (eCSSUnit_Counters == unit) {
      // must have a comma and then a separator string
      if (!ExpectSymbol(',', true) || !GetToken(true)) {
        break;
      }
      if (eCSSToken_String != mToken.mType) {
        UngetToken();
        break;
      }
      val->Item(1).SetStringValue(mToken.mIdent, eCSSUnit_String);
    }

    // get optional type
    int32_t typeItem = eCSSUnit_Counters == unit ? 2 : 1;
    nsCSSValue& type = val->Item(typeItem);
    if (ExpectSymbol(',', true)) {
      if (!ParseCounterStyleNameValue(type) && !ParseSymbols(type)) {
        break;
      }
    } else {
      type.SetAtomIdentValue(
        do_AddRef(static_cast<nsAtom*>(nsGkAtoms::decimal)));
    }

    if (!ExpectSymbol(')', true)) {
      break;
    }

    aValue.SetArrayValue(val, unit);
    return true;
  }

  SkipUntil(')');
  return false;
}

bool
CSSParserImpl::ParseContextProperties()
{
  nsCSSValue listValue;
  nsCSSValueList* currentListValue = listValue.SetListValue();
  bool first = true;
  for (;;) {
    const uint32_t variantMask = VARIANT_IDENTIFIER |
                                 VARIANT_INHERIT |
                                 VARIANT_NONE;
    nsCSSValue value;
    if (!ParseSingleTokenVariant(value, variantMask, nullptr)) {
      return false;
    }

    if (value.GetUnit() != eCSSUnit_Ident) {
      if (first) {
        AppendValue(eCSSProperty__moz_context_properties, value);
        return true;
      } else {
        return false;
      }
    }

    value.AtomizeIdentValue();
    nsAtom* atom = value.GetAtomValue();
    if (atom == nsGkAtoms::_default) {
      return false;
    }

    currentListValue->mValue = Move(value);

    if (!ExpectSymbol(',', true)) {
      break;
    }
    currentListValue->mNext = new nsCSSValueList;
    currentListValue = currentListValue->mNext;
    first = false;
  }

  AppendValue(eCSSProperty__moz_context_properties, listValue);
  return true;
}

bool
CSSParserImpl::ParseAttr(nsCSSValue& aValue)
{
  if (!GetToken(true)) {
    return false;
  }

  nsAutoString attr;
  if (eCSSToken_Ident == mToken.mType) {  // attr name or namespace
    nsAutoString  holdIdent(mToken.mIdent);
    if (ExpectSymbol('|', false)) {  // namespace
      int32_t nameSpaceID = GetNamespaceIdForPrefix(holdIdent);
      if (nameSpaceID == kNameSpaceID_Unknown) {
        return false;
      }
      attr.AppendInt(nameSpaceID, 10);
      attr.Append(char16_t('|'));
      if (! GetToken(false)) {
        REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
        return false;
      }
      if (eCSSToken_Ident == mToken.mType) {
        attr.Append(mToken.mIdent);
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        return false;
      }
    }
    else {  // no namespace
      attr = holdIdent;
    }
  }
  else if (mToken.IsSymbol('*')) {  // namespace wildcard
    // Wildcard namespace makes no sense here and is not allowed
    REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
    UngetToken();
    return false;
  }
  else if (mToken.IsSymbol('|')) {  // explicit NO namespace
    if (! GetToken(false)) {
      REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
      return false;
    }
    if (eCSSToken_Ident == mToken.mType) {
      attr.Append(mToken.mIdent);
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
      UngetToken();
      return false;
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PEAttributeNameOrNamespaceExpected);
    UngetToken();
    return false;
  }
  if (!ExpectSymbol(')', true)) {
    return false;
  }
  aValue.SetStringValue(attr, eCSSUnit_Attr);
  return true;
}

bool
CSSParserImpl::ParseSymbols(nsCSSValue& aValue)
{
  if (!GetToken(true)) {
    return false;
  }
  if (mToken.mType != eCSSToken_Function &&
      !mToken.mIdent.LowerCaseEqualsLiteral("symbols")) {
    UngetToken();
    return false;
  }

  RefPtr<nsCSSValue::Array> params = nsCSSValue::Array::Create(2);
  nsCSSValue& type = params->Item(0);
  nsCSSValue& symbols = params->Item(1);

  if (!ParseEnum(type, nsCSSProps::kCounterSymbolsSystemKTable)) {
    type.SetIntValue(NS_STYLE_COUNTER_SYSTEM_SYMBOLIC, eCSSUnit_Enumerated);
  }

  bool first = true;
  nsCSSValueList* item = symbols.SetListValue();
  for (;;) {
    // FIXME Should also include VARIANT_IMAGE. See bug 1071436.
    if (!ParseSingleTokenVariant(item->mValue, VARIANT_STRING, nullptr)) {
      break;
    }
    if (ExpectSymbol(')', true)) {
      if (first) {
        switch (type.GetIntValue()) {
          case NS_STYLE_COUNTER_SYSTEM_NUMERIC:
          case NS_STYLE_COUNTER_SYSTEM_ALPHABETIC:
            // require at least two symbols
            return false;
        }
      }
      aValue.SetArrayValue(params, eCSSUnit_Symbols);
      return true;
    }
    item->mNext = new nsCSSValueList;
    item = item->mNext;
    first = false;
  }

  SkipUntil(')');
  return false;
}

bool
CSSParserImpl::SetValueToURL(nsCSSValue& aValue, const nsString& aURL)
{
  if (!mSheetPrincipal) {
    if (!mSheetPrincipalRequired) {
      /* Pretend to succeed.  */
      return true;
    }

    NS_NOTREACHED("Codepaths that expect to parse URLs MUST pass in an "
                  "origin principal");
    return false;
  }

  mozilla::css::URLValue *urlVal =
    new mozilla::css::URLValue(aURL, mBaseURI, mSheetURI, mSheetPrincipal);
  aValue.SetURLValue(urlVal);
  return true;
}

/**
 * Parse the image-orientation property, which has the grammar:
 * <angle> flip? | flip | from-image
 */
bool
CSSParserImpl::ParseImageOrientation(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT, nullptr)) {
    // 'inherit', 'initial' and 'unset' must be alone
    return true;
  }

  // Check for an angle with optional 'flip'.
  nsCSSValue angle;
  if (ParseSingleTokenVariant(angle, VARIANT_ANGLE, nullptr)) {
    nsCSSValue flip;

    if (ParseSingleTokenVariant(flip, VARIANT_KEYWORD,
                                nsCSSProps::kImageOrientationFlipKTable)) {
      RefPtr<nsCSSValue::Array> array = nsCSSValue::Array::Create(2);
      array->Item(0) = angle;
      array->Item(1) = flip;
      aValue.SetArrayValue(array, eCSSUnit_Array);
    } else {
      aValue = angle;
    }

    return true;
  }

  // The remaining possibilities (bare 'flip' and 'from-image') are both
  // keywords, so we can handle them at the same time.
  nsCSSValue keyword;
  if (ParseSingleTokenVariant(keyword, VARIANT_KEYWORD,
                              nsCSSProps::kImageOrientationKTable)) {
    aValue = keyword;
    return true;
  }

  // All possibilities failed.
  return false;
}

/**
 * Parse the arguments of -moz-image-rect() function.
 * -moz-image-rect(<uri>, <top>, <right>, <bottom>, <left>)
 */
bool
CSSParserImpl::ParseImageRect(nsCSSValue& aImage)
{
  // A non-iterative for loop to break out when an error occurs.
  for (;;) {
    nsCSSValue newFunction;
    static const uint32_t kNumArgs = 5;
    nsCSSValue::Array* func =
      newFunction.InitFunction(eCSSKeyword__moz_image_rect, kNumArgs);

    // func->Item(0) is reserved for the function name.
    nsCSSValue& url    = func->Item(1);
    nsCSSValue& top    = func->Item(2);
    nsCSSValue& right  = func->Item(3);
    nsCSSValue& bottom = func->Item(4);
    nsCSSValue& left   = func->Item(5);

    nsAutoString urlString;
    if (!ParseURLOrString(urlString) ||
        !SetValueToURL(url, urlString) ||
        !ExpectSymbol(',', true)) {
      break;
    }

    static const int32_t VARIANT_SIDE = VARIANT_NUMBER | VARIANT_PERCENT;
    if (!ParseSingleTokenNonNegativeVariant(top, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseSingleTokenNonNegativeVariant(right, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseSingleTokenNonNegativeVariant(bottom, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseSingleTokenNonNegativeVariant(left, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(')', true))
      break;

    aImage = newFunction;
    return true;
  }

  SkipUntil(')');
  return false;
}

// <element>: -moz-element(# <element_id> )
bool
CSSParserImpl::ParseElement(nsCSSValue& aValue)
{
  // A non-iterative for loop to break out when an error occurs.
  for (;;) {
    if (!GetToken(true))
      break;

    if (mToken.mType == eCSSToken_ID) {
      aValue.SetStringValue(mToken.mIdent, eCSSUnit_Element);
    } else {
      UngetToken();
      break;
    }

    if (!ExpectSymbol(')', true))
      break;

    return true;
  }

  // If we detect a syntax error, we must match the opening parenthesis of the
  // function with the closing parenthesis and skip all the tokens in between.
  SkipUntil(')');
  return false;
}

// flex: none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
bool
CSSParserImpl::ParseFlex()
{
  // First check for inherit / initial / unset
  nsCSSValue tmpVal;
  if (ParseSingleTokenVariant(tmpVal, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_flex_grow, tmpVal);
    AppendValue(eCSSProperty_flex_shrink, tmpVal);
    AppendValue(eCSSProperty_flex_basis, tmpVal);
    return true;
  }

  // Next, check for 'none' == '0 0 auto'
  if (ParseSingleTokenVariant(tmpVal, VARIANT_NONE, nullptr)) {
    AppendValue(eCSSProperty_flex_grow, nsCSSValue(0.0f, eCSSUnit_Number));
    AppendValue(eCSSProperty_flex_shrink, nsCSSValue(0.0f, eCSSUnit_Number));
    AppendValue(eCSSProperty_flex_basis, nsCSSValue(eCSSUnit_Auto));
    return true;
  }

  // OK, try parsing our value as individual per-subproperty components:
  //   [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]

  // Each subproperty has a default value that it takes when it's omitted in a
  // "flex" shorthand value. These default values are *only* for the shorthand
  // syntax -- they're distinct from the subproperties' own initial values.  We
  // start with each subproperty at its default, as if we had "flex: 1 1 0%".
  nsCSSValue flexGrow(1.0f, eCSSUnit_Number);
  nsCSSValue flexShrink(1.0f, eCSSUnit_Number);
  nsCSSValue flexBasis(0.0f, eCSSUnit_Percent);

  // OVERVIEW OF PARSING STRATEGY:
  // =============================
  // a) Parse the first component as either flex-basis or flex-grow.
  // b) If it wasn't flex-grow, parse the _next_ component as flex-grow.
  // c) Now we've just parsed flex-grow -- so try parsing the next thing as
  //    flex-shrink.
  // d) Finally: If we didn't get flex-basis at the beginning, try to parse
  //    it now, at the end.
  //
  // More details in each section below.

  uint32_t flexBasisVariantMask =
    (nsCSSProps::ParserVariant(eCSSProperty_flex_basis) & ~(VARIANT_INHERIT));

  // (a) Parse first component. It can be either be a 'flex-basis' value or a
  // 'flex-grow' value, so we use the flex-basis-specific variant mask, along
  //  with VARIANT_NUMBER to accept 'flex-grow' values.
  //
  // NOTE: if we encounter unitless 0 here, we *must* interpret it as a
  // 'flex-grow' value (a number), *not* as a 'flex-basis' value (a length).
  // Conveniently, that's the behavior this combined variant-mask gives us --
  // it'll treat unitless 0 as a number. The flexbox spec requires this:
  // "a unitless zero that is not already preceded by two flex factors must be
  //  interpreted as a flex factor.
  if (ParseNonNegativeVariant(tmpVal, flexBasisVariantMask | VARIANT_NUMBER,
                              nsCSSProps::kWidthKTable) != CSSParseResult::Ok) {
    // First component was not a valid flex-basis or flex-grow value. Fail.
    return false;
  }

  // Record what we just parsed as either flex-basis or flex-grow:
  bool wasFirstComponentFlexBasis = (tmpVal.GetUnit() != eCSSUnit_Number);
  (wasFirstComponentFlexBasis ? flexBasis : flexGrow) = tmpVal;

  // (b) If we didn't get flex-grow yet, parse _next_ component as flex-grow.
  bool doneParsing = false;
  if (wasFirstComponentFlexBasis) {
    if (ParseNonNegativeNumber(tmpVal)) {
      flexGrow = tmpVal;
    } else {
      // Failed to parse anything after our flex-basis -- that's fine. We can
      // skip the remaining parsing.
      doneParsing = true;
    }
  }

  if (!doneParsing) {
    // (c) OK -- the last thing we parsed was flex-grow, so look for a
    //     flex-shrink in the next position.
    if (ParseNonNegativeNumber(tmpVal)) {
      flexShrink = tmpVal;
    }

    // d) Finally: If we didn't get flex-basis at the beginning, try to parse
    //    it now, at the end.
    //
    // NOTE: If we encounter unitless 0 in this final position, we'll parse it
    // as a 'flex-basis' value.  That's OK, because we know it must have
    // been "preceded by 2 flex factors" (justification below), which gets us
    // out of the spec's requirement of otherwise having to treat unitless 0
    // as a flex factor.
    //
    // JUSTIFICATION: How do we know that a unitless 0 here must have been
    // preceded by 2 flex factors? Well, suppose we had a unitless 0 that
    // was preceded by only 1 flex factor.  Then, we would have already
    // accepted this unitless 0 as the 'flex-shrink' value, up above (since
    // it's a valid flex-shrink value), and we'd have moved on to the next
    // token (if any). And of course, if we instead had a unitless 0 preceded
    // by *no* flex factors (if it were the first token), we would've already
    // parsed it in our very first call to ParseNonNegativeVariant().  So, any
    // unitless 0 encountered here *must* have been preceded by 2 flex factors.
    if (!wasFirstComponentFlexBasis) {
      CSSParseResult result =
        ParseNonNegativeVariant(tmpVal, flexBasisVariantMask,
                                nsCSSProps::kWidthKTable);
      if (result == CSSParseResult::Error) {
        return false;
      } else if (result == CSSParseResult::Ok) {
        flexBasis = tmpVal;
      }
    }
  }

  AppendValue(eCSSProperty_flex_grow,   flexGrow);
  AppendValue(eCSSProperty_flex_shrink, flexShrink);
  AppendValue(eCSSProperty_flex_basis,  flexBasis);

  return true;
}

// flex-flow: <flex-direction> || <flex-wrap>
bool
CSSParserImpl::ParseFlexFlow()
{
  static const nsCSSPropertyID kFlexFlowSubprops[] = {
    eCSSProperty_flex_direction,
    eCSSProperty_flex_wrap
  };
  const size_t numProps = MOZ_ARRAY_LENGTH(kFlexFlowSubprops);
  nsCSSValue values[numProps];

  int32_t found = ParseChoice(values, kFlexFlowSubprops, numProps);

  // Bail if we didn't successfully parse anything
  if (found < 1) {
    return false;
  }

  // If either property didn't get an explicit value, use its initial value.
  if ((found & 1) == 0) {
    values[0].SetIntValue(NS_STYLE_FLEX_DIRECTION_ROW, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) {
    values[1].SetIntValue(NS_STYLE_FLEX_WRAP_NOWRAP, eCSSUnit_Enumerated);
  }

  // Store these values and declare success!
  for (size_t i = 0; i < numProps; i++) {
    AppendValue(kFlexFlowSubprops[i], values[i]);
  }
  return true;
}

bool
CSSParserImpl::ParseGridAutoFlow()
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_grid_auto_flow, value);
    return true;
  }

  static const int32_t mask[] = {
    NS_STYLE_GRID_AUTO_FLOW_ROW | NS_STYLE_GRID_AUTO_FLOW_COLUMN,
    MASK_END_VALUE
  };
  if (!ParseBitmaskValues(value, nsCSSProps::kGridAutoFlowKTable, mask)) {
    return false;
  }
  int32_t bitField = value.GetIntValue();

  // If neither row nor column is provided, row is assumed.
  if (!(bitField & (NS_STYLE_GRID_AUTO_FLOW_ROW |
                    NS_STYLE_GRID_AUTO_FLOW_COLUMN))) {
    value.SetIntValue(bitField | NS_STYLE_GRID_AUTO_FLOW_ROW,
                      eCSSUnit_Enumerated);
  }

  AppendValue(eCSSProperty_grid_auto_flow, value);
  return true;
}

static const nsCSSKeyword kGridLineKeywords[] = {
  eCSSKeyword_span,
  eCSSKeyword_UNKNOWN  // End-of-array marker
};

CSSParseResult
CSSParserImpl::ParseGridLineNames(nsCSSValue& aValue)
{
  if (!ExpectSymbol('[', true)) {
    return CSSParseResult::NotFound;
  }
  if (!GetToken(true) || mToken.IsSymbol(']')) {
    return CSSParseResult::Ok;
  }
  // 'return' so far leaves aValue untouched, to represent an empty list.

  nsCSSValueList* item;
  if (aValue.GetUnit() == eCSSUnit_List) {
    // Find the end of an existing list.
    // The grid-template shorthand uses this, at most once for a given list.

    // NOTE: we could avoid this traversal by somehow keeping around
    // a pointer to the last item from the previous call.
    // It's not yet clear if this is worth the additional code complexity.
    item = aValue.GetListValue();
    while (item->mNext) {
      item = item->mNext;
    }
    item->mNext = new nsCSSValueList;
    item = item->mNext;
  } else {
    MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Null, "Unexpected unit");
    item = aValue.SetListValue();
  }
  for (;;) {
    if (!(eCSSToken_Ident == mToken.mType &&
          ParseCustomIdent(item->mValue, mToken.mIdent, kGridLineKeywords))) {
      UngetToken();
      SkipUntil(']');
      return CSSParseResult::Error;
    }
    if (!GetToken(true) || mToken.IsSymbol(']')) {
      return CSSParseResult::Ok;
    }
    item->mNext = new nsCSSValueList;
    item = item->mNext;
  }
}

// Assuming the 'repeat(' function token has already been consumed,
// parse the rest of repeat(<positive-integer> | auto-fill, <line-names>+)
// Append to the linked list whose end is given by |aTailPtr|,
// and update |aTailPtr| to point to the new end of the list.
bool
CSSParserImpl::ParseGridLineNameListRepeat(nsCSSValueList** aTailPtr)
{
  int32_t repetitions;
  Maybe<int32_t> repeatAutoEnum;
  if (!ParseGridTrackRepeatIntro(true, &repetitions, &repeatAutoEnum)) {
    return false;
  }
  if (repeatAutoEnum.isSome()) {
    // Parse exactly one <line-names>.
    nsCSSValue listValue;
    nsCSSValueList* list = listValue.SetListValue();
    if (ParseGridLineNames(list->mValue) != CSSParseResult::Ok) {
      return false;
    }
    if (!ExpectSymbol(')', true)) {
      return false;
    }
    // Instead of hooking up this list into the flat name list as usual,
    // we create a pair(Int, List) where the first value is the auto-fill
    // keyword and the second is the name list to repeat.
    nsCSSValue kwd;
    kwd.SetIntValue(repeatAutoEnum.value(), eCSSUnit_Enumerated);
    *aTailPtr = (*aTailPtr)->mNext = new nsCSSValueList;
    (*aTailPtr)->mValue.SetPairValue(kwd, listValue);
    return true;
  }

  // Parse at least one <line-names>
  nsCSSValueList* tail = *aTailPtr;
  do {
    tail->mNext = new nsCSSValueList;
    tail = tail->mNext;
    if (ParseGridLineNames(tail->mValue) != CSSParseResult::Ok) {
      return false;
    }
  } while (!ExpectSymbol(')', true));
  nsCSSValueList* firstRepeatedItem = (*aTailPtr)->mNext;
  nsCSSValueList* lastRepeatedItem = tail;

  // Our repeated items are already in the target list once,
  // so they need to be repeated |repetitions - 1| more times.
  MOZ_ASSERT(repetitions > 0, "Expected positive repetitions");
  while (--repetitions) {
    nsCSSValueList* repeatedItem = firstRepeatedItem;
    for (;;) {
      tail->mNext = new nsCSSValueList;
      tail = tail->mNext;
      tail->mValue = repeatedItem->mValue;
      if (repeatedItem == lastRepeatedItem) {
        break;
      }
      repeatedItem = repeatedItem->mNext;
    }
  }
  *aTailPtr = tail;
  return true;
}

// Assuming a 'subgrid' keyword was already consumed, parse <line-name-list>?
bool
CSSParserImpl::ParseOptionalLineNameListAfterSubgrid(nsCSSValue& aValue)
{
  nsCSSValueList* item = aValue.SetListValue();
  // This marker distinguishes the value from a <track-list>.
  item->mValue.SetIntValue(NS_STYLE_GRID_TEMPLATE_SUBGRID,
                           eCSSUnit_Enumerated);
  bool haveRepeatAuto = false;
  for (;;) {
    // First try to parse <name-repeat>, i.e.
    // repeat(<positive-integer> | auto-fill, <line-names>+)
    if (!GetToken(true)) {
      return true;
    }
    if (mToken.mType == eCSSToken_Function &&
        mToken.mIdent.LowerCaseEqualsLiteral("repeat")) {
      nsCSSValueList* startOfRepeat = item;
      if (!ParseGridLineNameListRepeat(&item)) {
        SkipUntil(')');
        return false;
      }
      if (startOfRepeat->mNext->mValue.GetUnit() == eCSSUnit_Pair) {
        if (haveRepeatAuto) {
          REPORT_UNEXPECTED(PEMoreThanOneGridRepeatAutoFillInNameList);
          return false;
        }
        haveRepeatAuto = true;
      }
    } else {
      UngetToken();

      // This was not a repeat() function. Try to parse <line-names>.
      nsCSSValue lineNames;
      CSSParseResult result = ParseGridLineNames(lineNames);
      if (result == CSSParseResult::NotFound) {
        return true;
      }
      if (result == CSSParseResult::Error) {
        return false;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
      item->mValue = lineNames;
    }
  }
}

CSSParseResult
CSSParserImpl::ParseGridTrackBreadth(nsCSSValue& aValue)
{
  CSSParseResult result = ParseNonNegativeVariant(aValue,
                            VARIANT_AUTO | VARIANT_LPCALC | VARIANT_KEYWORD,
                            nsCSSProps::kGridTrackBreadthKTable);

  if (result == CSSParseResult::Ok ||
      result == CSSParseResult::Error) {
    return result;
  }

  // Attempt to parse <flex> (a dimension with the "fr" unit).
  if (!GetToken(true)) {
    return CSSParseResult::NotFound;
  }
  if (!(eCSSToken_Dimension == mToken.mType &&
        mToken.mIdent.LowerCaseEqualsLiteral("fr") &&
        mToken.mNumber >= 0)) {
    UngetToken();
    return CSSParseResult::NotFound;
  }
  aValue.SetFloatValue(mToken.mNumber, eCSSUnit_FlexFraction);
  return CSSParseResult::Ok;
}

// Parse a <track-size>, or <fixed-size> when aFlags has eFixedTrackSize.
CSSParseResult
CSSParserImpl::ParseGridTrackSize(nsCSSValue& aValue,
                                  GridTrackSizeFlags aFlags)
{
  const bool requireFixedSize =
    !!(aFlags & GridTrackSizeFlags::eFixedTrackSize);
  // Attempt to parse a single <track-breadth>.
  CSSParseResult result = ParseGridTrackBreadth(aValue);
  if (requireFixedSize && result == CSSParseResult::Ok &&
      !aValue.IsLengthPercentCalcUnit()) {
    result = CSSParseResult::Error;
  }
  if (result == CSSParseResult::Error) {
    return result;
  }
  if (result == CSSParseResult::Ok) {
    if (aValue.GetUnit() == eCSSUnit_FlexFraction) {
      // Single value <flex> is represented internally as minmax(auto, <flex>).
      nsCSSValue minmax;
      nsCSSValue::Array* func = minmax.InitFunction(eCSSKeyword_minmax, 2);
      func->Item(1).SetAutoValue();
      func->Item(2) = aValue;
      aValue = minmax;
    }
    return result;
  }

  // Attempt to parse a minmax() or fit-content() function.
  if (!GetToken(true)) {
    return CSSParseResult::NotFound;
  }
  if (eCSSToken_Function != mToken.mType) {
    UngetToken();
    return CSSParseResult::NotFound;
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("fit-content")) {
    if (requireFixedSize) {
      UngetToken();
      return CSSParseResult::Error;
    }
    nsCSSValue::Array* func = aValue.InitFunction(eCSSKeyword_fit_content, 1);
    if (ParseGridTrackBreadth(func->Item(1)) == CSSParseResult::Ok &&
        func->Item(1).IsLengthPercentCalcUnit() &&
        ExpectSymbol(')', true)) {
      return CSSParseResult::Ok;
    }
    SkipUntil(')');
    return CSSParseResult::Error;
  }
  if (!mToken.mIdent.LowerCaseEqualsLiteral("minmax")) {
    UngetToken();
    return CSSParseResult::NotFound;
  }
  nsCSSValue::Array* func = aValue.InitFunction(eCSSKeyword_minmax, 2);
  if (ParseGridTrackBreadth(func->Item(1)) == CSSParseResult::Ok &&
      ExpectSymbol(',', true) &&
      ParseGridTrackBreadth(func->Item(2)) == CSSParseResult::Ok &&
      ExpectSymbol(')', true)) {
    if (requireFixedSize &&
        !func->Item(1).IsLengthPercentCalcUnit() &&
        !func->Item(2).IsLengthPercentCalcUnit()) {
      return CSSParseResult::Error;
    }
    // Reject <flex> min-sizing.
    if (func->Item(1).GetUnit() == eCSSUnit_FlexFraction) {
      return CSSParseResult::Error;
    }
    return CSSParseResult::Ok;
  }
  SkipUntil(')');
  return CSSParseResult::Error;
}

bool
CSSParserImpl::ParseGridAutoColumnsRows(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr) ||
      ParseGridTrackSize(value) == CSSParseResult::Ok) {
    AppendValue(aPropID, value);
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParseGridTrackListWithFirstLineNames(nsCSSValue& aValue,
                                                    const nsCSSValue& aFirstLineNames,
                                                    GridTrackListFlags aFlags)
{
  nsCSSValueList* firstLineNamesItem = aValue.SetListValue();
  firstLineNamesItem->mValue = aFirstLineNames;

  // This function is trying to parse <track-list>, which is
  //   [ <line-names>? [ <track-size> | <repeat()> ] ]+ <line-names>?
  // and we're already past the first "<line-names>?".
  // If aFlags contains eExplicitTrackList then <repeat()> is disallowed.
  //
  // Each iteration of the following loop attempts to parse either a
  // repeat() or a <track-size> expression, and then an (optional)
  // <line-names> expression.
  //
  // The only successful exit point from this loop is the ::NotFound
  // case after ParseGridTrackSize(); i.e. we'll greedily parse
  // repeat()/<track-size> until we can't find one.
  nsCSSValueList* item = firstLineNamesItem;
  bool haveRepeatAuto = false;
  for (;;) {
    // First try to parse repeat()
    if (!GetToken(true)) {
      break;
    }
    if (!(aFlags & GridTrackListFlags::eExplicitTrackList) &&
        mToken.mType == eCSSToken_Function &&
        mToken.mIdent.LowerCaseEqualsLiteral("repeat")) {
      nsCSSValueList* startOfRepeat = item;
      if (!ParseGridTrackListRepeat(&item)) {
        SkipUntil(')');
        return false;
      }
      auto firstRepeat = startOfRepeat->mNext;
      if (firstRepeat->mValue.GetUnit() == eCSSUnit_Pair) {
        if (haveRepeatAuto) {
          REPORT_UNEXPECTED(PEMoreThanOneGridRepeatAutoFillFitInTrackList);
          return false;
        }
        haveRepeatAuto = true;
        // We're parsing an <auto-track-list>, which requires that all tracks
        // are <fixed-size>, so we need to check the ones we've parsed already.
        for (nsCSSValueList* list = firstLineNamesItem->mNext;
             list != firstRepeat; list = list->mNext) {
          if (list->mValue.GetUnit() == eCSSUnit_Function) {
            nsCSSValue::Array* func = list->mValue.GetArrayValue();
            auto funcName = func->Item(0).GetKeywordValue();
            if (funcName == eCSSKeyword_minmax) {
              if (!func->Item(1).IsLengthPercentCalcUnit() &&
                  !func->Item(2).IsLengthPercentCalcUnit()) {
                return false;
              }
            } else {
              MOZ_ASSERT(funcName == eCSSKeyword_fit_content,
                         "Expected minmax() or fit-content() function");
              return false; // fit-content() is not a <fixed-size>
            }
          } else if (!list->mValue.IsLengthPercentCalcUnit()) {
            return false;
          }
          list = list->mNext; // skip line names
        }
      }
    } else {
      UngetToken();

      // Not a repeat() function; try to parse <track-size> | <fixed-size>.
      nsCSSValue trackSize;
      GridTrackSizeFlags flags = haveRepeatAuto
        ? GridTrackSizeFlags::eFixedTrackSize
        : GridTrackSizeFlags::eDefaultTrackSize;
      CSSParseResult result = ParseGridTrackSize(trackSize, flags);
      if (result == CSSParseResult::Error) {
        return false;
      }
      if (result == CSSParseResult::NotFound) {
        // What we've parsed so far is a valid <track-list>
        // (modulo the "at least one <track-size>" check below.)
        // Stop here.
        break;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
      item->mValue = trackSize;

      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
    if (ParseGridLineNames(item->mValue) == CSSParseResult::Error) {
      return false;
    }
  }

  // Require at least one <track-size>.
  if (item == firstLineNamesItem) {
    return false;
  }

  MOZ_ASSERT(aValue.GetListValue() &&
             aValue.GetListValue()->mNext &&
             aValue.GetListValue()->mNext->mNext,
             "<track-list> should have a minimum length of 3");
  return true;
}

// Takes ownership of |aSecond|
static void
ConcatLineNames(nsCSSValue& aFirst, nsCSSValue& aSecond)
{
  if (aSecond.GetUnit() == eCSSUnit_Null) {
    // Nothing to do.
    return;
  }
  if (aFirst.GetUnit() == eCSSUnit_Null) {
    // Empty or omitted <line-names>. Replace it.
    aFirst = aSecond;
    return;
  }

  // Join the two <line-names> lists.
  nsCSSValueList* source = aSecond.GetListValue();
  nsCSSValueList* target = aFirst.GetListValue();
  // Find the end:
  while (target->mNext) {
    target = target->mNext;
  }
  // Copy the first name. We can't take ownership of it
  // as it'll be destroyed when |aSecond| goes out of scope.
  target->mNext = new nsCSSValueList;
  target = target->mNext;
  target->mValue = source->mValue;
  // Move the rest of the linked list.
  target->mNext = source->mNext;
  source->mNext = nullptr;
}

// Assuming the 'repeat(' function token has already been consumed,
// parse "repeat( <positive-integer> | auto-fill | auto-fit ,"
// (or "repeat( <positive-integer> | auto-fill ," when aForSubgrid is true)
// and stop after the comma.  Return true when parsing succeeds,
// with aRepetitions set to the number of repetitions and aRepeatAutoEnum set
// to an enum value for auto-fill | auto-fit (it's not set at all when
// <positive-integer> was parsed).
bool
CSSParserImpl::ParseGridTrackRepeatIntro(bool            aForSubgrid,
                                         int32_t*        aRepetitions,
                                         Maybe<int32_t>* aRepeatAutoEnum)
{
  if (!GetToken(true)) {
    return false;
  }
  if (mToken.mType == eCSSToken_Ident) {
    if (mToken.mIdent.LowerCaseEqualsLiteral("auto-fill")) {
      aRepeatAutoEnum->emplace(NS_STYLE_GRID_REPEAT_AUTO_FILL);
    } else if (!aForSubgrid &&
               mToken.mIdent.LowerCaseEqualsLiteral("auto-fit")) {
      aRepeatAutoEnum->emplace(NS_STYLE_GRID_REPEAT_AUTO_FIT);
    } else {
      return false;
    }
    *aRepetitions = 1;
  } else if (mToken.mType == eCSSToken_Number) {
    if (!(mToken.mIntegerValid &&
          mToken.mInteger > 0)) {
      return false;
    }
    *aRepetitions = std::min(mToken.mInteger, GRID_TEMPLATE_MAX_REPETITIONS);
  } else {
    return false;
  }

  if (!ExpectSymbol(',', true)) {
    return false;
  }
  return true;
}

// Assuming the 'repeat(' function token has already been consumed,
// parse the rest of
// repeat( <positive-integer> | auto-fill | auto-fit ,
//         [ <line-names>? <track-size> ]+ <line-names>? )
// Append to the linked list whose end is given by |aTailPtr|,
// and update |aTailPtr| to point to the new end of the list.
// Note: only one <track-size> is allowed for auto-fill/fit
bool
CSSParserImpl::ParseGridTrackListRepeat(nsCSSValueList** aTailPtr)
{
  int32_t repetitions;
  Maybe<int32_t> repeatAutoEnum;
  if (!ParseGridTrackRepeatIntro(false, &repetitions, &repeatAutoEnum)) {
    return false;
  }

  // Parse [ <line-names>? <track-size> ]+ <line-names>?
  // but keep the first and last <line-names> separate
  // because they'll need to be joined.
  // http://dev.w3.org/csswg/css-grid/#repeat-notation
  nsCSSValue firstLineNames;
  nsCSSValue trackSize;
  nsCSSValue lastLineNames;
  // Optional
  if (ParseGridLineNames(firstLineNames) == CSSParseResult::Error) {
    return false;
  }
  // Required
  GridTrackSizeFlags flags = repeatAutoEnum.isSome()
    ? GridTrackSizeFlags::eFixedTrackSize
    : GridTrackSizeFlags::eDefaultTrackSize;
  if (ParseGridTrackSize(trackSize, flags) != CSSParseResult::Ok) {
    return false;
  }
  // Use nsAutoPtr to free the list in case of early return.
  nsAutoPtr<nsCSSValueList> firstTrackSizeItemAuto(new nsCSSValueList);
  firstTrackSizeItemAuto->mValue = trackSize;

  nsCSSValueList* item = firstTrackSizeItemAuto;
  for (;;) {
    // Optional
    if (ParseGridLineNames(lastLineNames) == CSSParseResult::Error) {
      return false;
    }

    if (ExpectSymbol(')', true)) {
      break;
    }

    // <auto-repeat> only accepts a single track size:
    // <line-names>? <fixed-size> <line-names>?
    if (repeatAutoEnum.isSome()) {
      REPORT_UNEXPECTED(PEMoreThanOneGridRepeatTrackSize);
      return false;
    }

    // Required
    if (ParseGridTrackSize(trackSize) != CSSParseResult::Ok) {
      return false;
    }

    item->mNext = new nsCSSValueList;
    item = item->mNext;
    item->mValue = lastLineNames;
    // Do not append to this list at the next iteration.
    lastLineNames.Reset();

    item->mNext = new nsCSSValueList;
    item = item->mNext;
    item->mValue = trackSize;
  }
  nsCSSValueList* lastTrackSizeItem = item;

  // [ <line-names>? <track-size> ]+ <line-names>?  is now parsed into:
  // * firstLineNames: the first <line-names>
  // * a linked list of odd length >= 1, from firstTrackSizeItem
  //   (the first <track-size>) to lastTrackSizeItem (the last),
  //   with the <line-names> sublists in between
  // * lastLineNames: the last <line-names>

  if (repeatAutoEnum.isSome()) {
    // Instead of hooking up this list into the flat track/name list as usual,
    // we create a pair(Int, List) where the first value is the auto-fill/fit
    // keyword and the second is the list to repeat.  There are three items
    // in this list, the first is the list of line names before the track size,
    // the second item is the track size, and the last item is the list of line
    // names after the track size.  Note that the line names are NOT merged
    // with any line names before/after the repeat() itself.
    nsCSSValue listValue;
    nsCSSValueList* list = listValue.SetListValue();
    list->mValue = firstLineNames;
    list = list->mNext = new nsCSSValueList;
    list->mValue = trackSize;
    list = list->mNext = new nsCSSValueList;
    list->mValue = lastLineNames;
    nsCSSValue kwd;
    kwd.SetIntValue(repeatAutoEnum.value(), eCSSUnit_Enumerated);
    *aTailPtr = (*aTailPtr)->mNext = new nsCSSValueList;
    (*aTailPtr)->mValue.SetPairValue(kwd, listValue);
    // Append an empty list since the caller expects that to represent the names
    // that follows the repeat() function.
    *aTailPtr = (*aTailPtr)->mNext = new nsCSSValueList;
    return true;
  }

  // Join the last and first <line-names> (in that order.)
  // For example, repeat(3, (a) 100px (b) 200px (c)) results in
  // (a) 100px (b) 200px (c a) 100px (b) 200px (c a) 100px (b) 200px (c)
  // This is (c a).
  // Make deep copies: the originals will be moved.
  nsCSSValue joinerLineNames;
  {
    nsCSSValueList* target = nullptr;
    if (lastLineNames.GetUnit() != eCSSUnit_Null) {
      target = joinerLineNames.SetListValue();
      nsCSSValueList* source = lastLineNames.GetListValue();
      for (;;) {
        target->mValue = source->mValue;
        source = source->mNext;
        if (!source) {
          break;
        }
        target->mNext = new nsCSSValueList;
        target = target->mNext;
      }
    }

    if (firstLineNames.GetUnit() != eCSSUnit_Null) {
      if (target) {
        target->mNext = new nsCSSValueList;
        target = target->mNext;
      } else {
        target = joinerLineNames.SetListValue();
      }
      nsCSSValueList* source = firstLineNames.GetListValue();
      for (;;) {
        target->mValue = source->mValue;
        source = source->mNext;
        if (!source) {
          break;
        }
        target->mNext = new nsCSSValueList;
        target = target->mNext;
      }
    }
  }

  // Join our first <line-names> with the one before repeat().
  // (a) repeat(1, (b) 20px) expands to (a b) 20px
  nsCSSValueList* previousItemBeforeRepeat = *aTailPtr;
  ConcatLineNames(previousItemBeforeRepeat->mValue, firstLineNames);

  // Move our linked list
  // (first to last <track-size>, with the <line-names> sublists in between).
  // This is the first repetition.
  NS_ASSERTION(previousItemBeforeRepeat->mNext == nullptr,
               "Expected the end of a linked list");
  previousItemBeforeRepeat->mNext = firstTrackSizeItemAuto.forget();
  nsCSSValueList* firstTrackSizeItem = previousItemBeforeRepeat->mNext;
  nsCSSValueList* tail = lastTrackSizeItem;

  // Repeat |repetitions - 1| more times:
  // * the joiner <line-names>
  // * the linked list
  //   (first to last <track-size>, with the <line-names> sublists in between)
  MOZ_ASSERT(repetitions > 0, "Expected positive repetitions");
  while (--repetitions) {
    tail->mNext = new nsCSSValueList;
    tail = tail->mNext;
    tail->mValue = joinerLineNames;

    nsCSSValueList* repeatedItem = firstTrackSizeItem;
    for (;;) {
      tail->mNext = new nsCSSValueList;
      tail = tail->mNext;
      tail->mValue = repeatedItem->mValue;
      if (repeatedItem == lastTrackSizeItem) {
        break;
      }
      repeatedItem = repeatedItem->mNext;
    }
  }

  // Finally, move our last <line-names>.
  // Any <line-names> immediately after repeat() will append to it.
  tail->mNext = new nsCSSValueList;
  tail = tail->mNext;
  tail->mValue = lastLineNames;

  *aTailPtr = tail;
  return true;
}

bool
CSSParserImpl::ParseGridTrackList(nsCSSPropertyID aPropID,
                                  GridTrackListFlags aFlags)
{
  nsCSSValue value;
  nsCSSValue firstLineNames;
  if (ParseGridLineNames(firstLineNames) == CSSParseResult::Error ||
      !ParseGridTrackListWithFirstLineNames(value, firstLineNames, aFlags)) {
    return false;
  }
  AppendValue(aPropID, value);
  return true;
}

bool
CSSParserImpl::ParseGridTemplateColumnsRows(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    AppendValue(aPropID, value);
    return true;
  }

  nsAString* ident = NextIdent();
  if (ident) {
    if (ident->LowerCaseEqualsLiteral("subgrid")) {
      if (!nsLayoutUtils::IsGridTemplateSubgridValueEnabled()) {
        REPORT_UNEXPECTED(PESubgridNotSupported);
        return false;
      }
      if (!ParseOptionalLineNameListAfterSubgrid(value)) {
        return false;
      }
      AppendValue(aPropID, value);
      return true;
    }
    UngetToken();
  }

  return ParseGridTrackList(aPropID);
}

bool
CSSParserImpl::ParseGridTemplateAreasLine(const nsAutoString& aInput,
                                          css::GridTemplateAreasValue* aAreas,
                                          nsDataHashtable<nsStringHashKey, uint32_t>& aAreaIndices)
{
  aAreas->mTemplates.AppendElement(mToken.mIdent);

  nsCSSGridTemplateAreaScanner scanner(aInput);
  nsCSSGridTemplateAreaToken token;
  css::GridNamedArea* currentArea = nullptr;
  uint32_t row = aAreas->NRows();
  // Column numbers starts at 1, but we might not have any, eg
  // grid-template-areas:""; which will result in mNColumns == 0.
  uint32_t column = 0;
  while (scanner.Next(token)) {
    ++column;
    if (token.isTrash) {
      return false;
    }
    if (currentArea) {
      if (token.mName == currentArea->mName) {
        if (currentArea->mRowStart == row) {
          // Next column in the first row of this named area.
          currentArea->mColumnEnd++;
        }
        continue;
      }
      // We're exiting |currentArea|, so currentArea is ending at |column|.
      // Make sure that this is consistent with currentArea on previous rows:
      if (currentArea->mColumnEnd != column) {
        NS_ASSERTION(currentArea->mRowStart != row,
                     "Inconsistent column end for the first row of a named area.");
        // Not a rectangle
        return false;
      }
      currentArea = nullptr;
    }
    if (!token.mName.IsEmpty()) {
      // Named cell that doesn't have a cell with the same name on its left.

      // Check if this is the continuation of an existing named area:
      uint32_t index;
      if (aAreaIndices.Get(token.mName, &index)) {
        MOZ_ASSERT(index < aAreas->mNamedAreas.Length(),
                   "Invalid aAreaIndices hash table");
        currentArea = &aAreas->mNamedAreas[index];
        if (currentArea->mColumnStart != column ||
            currentArea->mRowEnd != row) {
          // Existing named area, but not forming a rectangle
          return false;
        }
        // Next row of an existing named area
        currentArea->mRowEnd++;
      } else {
        // New named area
        aAreaIndices.Put(token.mName, aAreas->mNamedAreas.Length());
        currentArea = aAreas->mNamedAreas.AppendElement();
        currentArea->mName = token.mName;
        // For column or row N (starting at 1),
        // the start line is N, the end line is N + 1
        currentArea->mColumnStart = column;
        currentArea->mColumnEnd = column + 1;
        currentArea->mRowStart = row;
        currentArea->mRowEnd = row + 1;
      }
    }
  }
  if (currentArea && currentArea->mColumnEnd != column + 1) {
    NS_ASSERTION(currentArea->mRowStart != row,
                 "Inconsistent column end for the first row of a named area.");
    // Not a rectangle
    return false;
  }

  // On the first row, set the number of columns
  // that grid-template-areas contributes to the explicit grid.
  // On other rows, check that the number of columns is consistent
  // between rows.
  if (row == 1) {
    aAreas->mNColumns = column;
  } else if (aAreas->mNColumns != column) {
    return false;
  }
  return true;
}

bool
CSSParserImpl::ParseGridTemplateAreas()
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    AppendValue(eCSSProperty_grid_template_areas, value);
    return true;
  }

  RefPtr<css::GridTemplateAreasValue> areas =
    new css::GridTemplateAreasValue();
  nsDataHashtable<nsStringHashKey, uint32_t> areaIndices;
  for (;;) {
    if (!GetToken(true)) {
      break;
    }
    if (eCSSToken_String != mToken.mType) {
      UngetToken();
      break;
    }
    if (!ParseGridTemplateAreasLine(mToken.mIdent, areas, areaIndices)) {
      return false;
    }
  }

  if (areas->NRows() == 0) {
    return false;
  }

  AppendValue(eCSSProperty_grid_template_areas, nsCSSValue(areas));
  return true;
}

// [ auto-flow && dense? ] <'grid-auto-columns'>? |
// <'grid-template-columns'>
bool
CSSParserImpl::ParseGridTemplateColumnsOrAutoFlow(bool aForGridShorthand)
{
  if (aForGridShorthand) {
    auto res = ParseGridShorthandAutoProps(NS_STYLE_GRID_AUTO_FLOW_COLUMN);
    if (res == CSSParseResult::Error) {
      return false;
    }
    if (res == CSSParseResult::Ok) {
      nsCSSValue value(eCSSUnit_None);
      AppendValue(eCSSProperty_grid_template_columns, value);
      return true;
    }
  }
  return ParseGridTemplateColumnsRows(eCSSProperty_grid_template_columns);
}

bool
CSSParserImpl::ParseGridTemplate(bool aForGridShorthand)
{
  // none |
  // subgrid |
  // <'grid-template-rows'> / <'grid-template-columns'> |
  // [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <explicit-track-list>]?
  // or additionally when aForGridShorthand is true:
  // <'grid-template-rows'> / [ auto-flow && dense? ] <'grid-auto-columns'>?
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_grid_template_areas, value);
    AppendValue(eCSSProperty_grid_template_rows, value);
    AppendValue(eCSSProperty_grid_template_columns, value);
    return true;
  }

  // 'none' can appear either by itself,
  // or as the beginning of <'grid-template-rows'> / <'grid-template-columns'>
  if (ParseSingleTokenVariant(value, VARIANT_NONE, nullptr)) {
    AppendValue(eCSSProperty_grid_template_rows, value);
    AppendValue(eCSSProperty_grid_template_areas, value);
    if (ExpectSymbol('/', true)) {
      return ParseGridTemplateColumnsOrAutoFlow(aForGridShorthand);
    }
    AppendValue(eCSSProperty_grid_template_columns, value);
    return true;
  }

  // 'subgrid' can appear either by itself,
  // or as the beginning of <'grid-template-rows'> / <'grid-template-columns'>
  nsAString* ident = NextIdent();
  if (ident) {
    if (ident->LowerCaseEqualsLiteral("subgrid")) {
      if (!nsLayoutUtils::IsGridTemplateSubgridValueEnabled()) {
        REPORT_UNEXPECTED(PESubgridNotSupported);
        return false;
      }
      if (!ParseOptionalLineNameListAfterSubgrid(value)) {
        return false;
      }
      AppendValue(eCSSProperty_grid_template_rows, value);
      AppendValue(eCSSProperty_grid_template_areas, nsCSSValue(eCSSUnit_None));
      if (ExpectSymbol('/', true)) {
        return ParseGridTemplateColumnsOrAutoFlow(aForGridShorthand);
      }
      if (value.GetListValue()->mNext) {
        // Non-empty <line-name-list> after 'subgrid'.
        // This is only valid as part of <'grid-template-rows'>,
        // which must be followed by a slash.
        return false;
      }
      // 'subgrid' by itself sets both grid-template-rows/columns.
      AppendValue(eCSSProperty_grid_template_columns, value);
      return true;
    }
    UngetToken();
  }

  // [ <line-names>? ] here is ambiguous:
  // it can be either the start of a <track-list> (in a <'grid-template-rows'>),
  // or the start of [ <line-names>? <string> <track-size>? <line-names>? ]+
  nsCSSValue firstLineNames;
  if (ParseGridLineNames(firstLineNames) == CSSParseResult::Error ||
      !GetToken(true)) {
    return false;
  }
  if (mToken.mType == eCSSToken_String) {
    // It's the [ <line-names>? <string> <track-size>? <line-names>? ]+ case.
    if (!ParseGridTemplateAfterString(firstLineNames)) {
      return false;
    }
    // Parse an optional [ / <explicit-track-list> ] as the columns value.
    if (ExpectSymbol('/', true)) {
      return ParseGridTrackList(eCSSProperty_grid_template_columns,
                                GridTrackListFlags::eExplicitTrackList);
    }
    value.SetNoneValue(); // absent means 'none'
    AppendValue(eCSSProperty_grid_template_columns, value);
    return true;
  }
  UngetToken();

  // Finish parsing <'grid-template-rows'> with the |firstLineNames| we have,
  // and then parse a mandatory [ / <'grid-template-columns'> ].
  if (!ParseGridTrackListWithFirstLineNames(value, firstLineNames) ||
      !ExpectSymbol('/', true)) {
    return false;
  }
  AppendValue(eCSSProperty_grid_template_rows, value);
  value.SetNoneValue();
  AppendValue(eCSSProperty_grid_template_areas, value);
  return ParseGridTemplateColumnsOrAutoFlow(aForGridShorthand);
}

// Helper for parsing the 'grid-template' shorthand:
// Parse [ <line-names>? <string> <track-size>? <line-names>? ]+
// with a <line-names>? already consumed, stored in |aFirstLineNames|,
// and the current token a <string>
bool
CSSParserImpl::ParseGridTemplateAfterString(const nsCSSValue& aFirstLineNames)
{
  MOZ_ASSERT(mToken.mType == eCSSToken_String,
             "ParseGridTemplateAfterString called with a non-string token");

  nsCSSValue rowsValue;
  RefPtr<css::GridTemplateAreasValue> areas =
    new css::GridTemplateAreasValue();
  nsDataHashtable<nsStringHashKey, uint32_t> areaIndices;
  nsCSSValueList* rowsItem = rowsValue.SetListValue();
  rowsItem->mValue = aFirstLineNames;

  for (;;) {
    if (!ParseGridTemplateAreasLine(mToken.mIdent, areas, areaIndices)) {
      return false;
    }

    rowsItem->mNext = new nsCSSValueList;
    rowsItem = rowsItem->mNext;
    CSSParseResult result = ParseGridTrackSize(rowsItem->mValue);
    if (result == CSSParseResult::Error) {
      return false;
    }
    if (result == CSSParseResult::NotFound) {
      rowsItem->mValue.SetAutoValue();
    }

    rowsItem->mNext = new nsCSSValueList;
    rowsItem = rowsItem->mNext;
    result = ParseGridLineNames(rowsItem->mValue);
    if (result == CSSParseResult::Error) {
      return false;
    }
    if (result == CSSParseResult::Ok) {
      // Append to the same list as the previous call to ParseGridLineNames.
      result = ParseGridLineNames(rowsItem->mValue);
      if (result == CSSParseResult::Error) {
        return false;
      }
      if (result == CSSParseResult::Ok) {
        // Parsed <line-name> twice.
        // The property value can not end here, we expect a string next.
        if (!GetToken(true)) {
          return false;
        }
        if (eCSSToken_String != mToken.mType) {
          UngetToken();
          return false;
        }
        continue;
      }
    }

    // Did not find a <line-names>.
    // Next, we expect either a string or the end of the property value.
    if (!GetToken(true)) {
      break;
    }
    if (eCSSToken_String != mToken.mType) {
      UngetToken();
      break;
    }
  }

  AppendValue(eCSSProperty_grid_template_areas, nsCSSValue(areas));
  AppendValue(eCSSProperty_grid_template_rows, rowsValue);
  return true;
}

// <'grid-template'> |
// <'grid-template-rows'> / [ auto-flow && dense? ] <'grid-auto-columns'>? |
// [ auto-flow && dense? ] <'grid-auto-rows'>? / <'grid-template-columns'>
bool
CSSParserImpl::ParseGrid()
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    for (const nsCSSPropertyID* subprops =
           nsCSSProps::SubpropertyEntryFor(eCSSProperty_grid);
         *subprops != eCSSProperty_UNKNOWN; ++subprops) {
      AppendValue(*subprops, value);
    }
    return true;
  }

  // [ auto-flow && dense? ] <'grid-auto-rows'>? / <'grid-template-columns'>
  auto res = ParseGridShorthandAutoProps(NS_STYLE_GRID_AUTO_FLOW_ROW);
  if (res == CSSParseResult::Error) {
    return false;
  }
  if (res == CSSParseResult::Ok) {
    value.SetAutoValue();
    AppendValue(eCSSProperty_grid_auto_columns, value);
    nsCSSValue none(eCSSUnit_None);
    AppendValue(eCSSProperty_grid_template_areas, none);
    AppendValue(eCSSProperty_grid_template_rows, none);
    if (!ExpectSymbol('/', true)) {
      return false;
    }
    return ParseGridTemplateColumnsRows(eCSSProperty_grid_template_columns);
  }

  // Set remaining subproperties that might not be set by ParseGridTemplate to
  // their initial values and then parse <'grid-template'> |
  // <'grid-template-rows'> / [ auto-flow && dense? ] <'grid-auto-columns'>? .
  value.SetIntValue(NS_STYLE_GRID_AUTO_FLOW_ROW, eCSSUnit_Enumerated);
  AppendValue(eCSSProperty_grid_auto_flow, value);
  value.SetAutoValue();
  AppendValue(eCSSProperty_grid_auto_rows, value);
  AppendValue(eCSSProperty_grid_auto_columns, value);
  return ParseGridTemplate(true);
}

// Parse [ auto-flow && dense? ] <'grid-auto-[rows|columns]'>? for the 'grid'
// shorthand.  If aAutoFlowAxis == NS_STYLE_GRID_AUTO_FLOW_ROW then we're
// parsing row values, otherwise column values.
CSSParseResult
CSSParserImpl::ParseGridShorthandAutoProps(int32_t aAutoFlowAxis)
{
  MOZ_ASSERT(aAutoFlowAxis == NS_STYLE_GRID_AUTO_FLOW_ROW ||
             aAutoFlowAxis == NS_STYLE_GRID_AUTO_FLOW_COLUMN);
  if (!GetToken(true)) {
    return CSSParseResult::NotFound;
  }
  // [ auto-flow && dense? ]
  int32_t autoFlowValue = 0;
  if (mToken.mType == eCSSToken_Ident) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    if (keyword == eCSSKeyword_auto_flow) {
      autoFlowValue = aAutoFlowAxis;
      if (GetToken(true)) {
        if (mToken.mType == eCSSToken_Ident &&
            nsCSSKeywords::LookupKeyword(mToken.mIdent) == eCSSKeyword_dense) {
          autoFlowValue |= NS_STYLE_GRID_AUTO_FLOW_DENSE;
        } else {
          UngetToken();
        }
      }
    } else if (keyword == eCSSKeyword_dense) {
      if (!GetToken(true)) {
        return CSSParseResult::Error;
      }
      if (mToken.mType != eCSSToken_Ident ||
          nsCSSKeywords::LookupKeyword(mToken.mIdent) != eCSSKeyword_auto_flow) {
        UngetToken();
        return CSSParseResult::Error;
      }
      autoFlowValue = aAutoFlowAxis | NS_STYLE_GRID_AUTO_FLOW_DENSE;
    }
  }
  if (autoFlowValue) {
    nsCSSValue value;
    value.SetIntValue(autoFlowValue, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_grid_auto_flow, value);
  } else {
    UngetToken();
    return CSSParseResult::NotFound;
  }

  // <'grid-auto-[rows|columns]'>?
  nsCSSValue autoTrackValue;
  CSSParseResult result = ParseGridTrackSize(autoTrackValue);
  if (result == CSSParseResult::Error) {
    return result;
  }
  if (result == CSSParseResult::NotFound) {
    autoTrackValue.SetAutoValue();
  }
  AppendValue(aAutoFlowAxis == NS_STYLE_GRID_AUTO_FLOW_ROW ?
                eCSSProperty_grid_auto_rows : eCSSProperty_grid_auto_columns,
              autoTrackValue);
  return CSSParseResult::Ok;
}

// Parse a <grid-line>.
// If successful, set aValue to eCSSUnit_Auto,
// or a eCSSUnit_List containing, in that order:
//
// * An optional eCSSUnit_Enumerated marking a "span" keyword.
// * An optional eCSSUnit_Integer
// * An optional eCSSUnit_Ident
//
// At least one of eCSSUnit_Integer or eCSSUnit_Ident is present.
bool
CSSParserImpl::ParseGridLine(nsCSSValue& aValue)
{
  //  <grid-line> =
  //    auto |
  //    <custom-ident> |
  //    [ <integer> && <custom-ident>? ] |
  //    [ span && [ <integer> || <custom-ident> ] ]
  //
  // Syntactically, this simplifies to:
  //
  //  <grid-line> =
  //    auto |
  //    [ span? && [ <integer> || <custom-ident> ] ]

  if (ParseSingleTokenVariant(aValue, VARIANT_AUTO, nullptr)) {
    return true;
  }

  bool hasSpan = false;
  bool hasIdent = false;
  Maybe<int32_t> integer;
  nsCSSValue ident;

#ifdef MOZ_VALGRIND
  // Make the contained value be defined even though we really want a
  // Nothing here.  This works around an otherwise difficult to avoid
  // Memcheck false positive when this is compiled by gcc-5.3 -O2.
  // See bug 1301856.
  integer.emplace(0);
  integer.reset();
#endif

  if (!GetToken(true)) {
    return false;
  }
  if (mToken.mType == eCSSToken_Ident &&
      mToken.mIdent.LowerCaseEqualsLiteral("span")) {
    hasSpan = true;
    if (!GetToken(true)) {
      return false;
    }
  }

  do {
    if (!hasIdent &&
        mToken.mType == eCSSToken_Ident &&
        ParseCustomIdent(ident, mToken.mIdent, kGridLineKeywords)) {
      hasIdent = true;
    } else if (integer.isNothing() &&
               mToken.mType == eCSSToken_Number &&
               mToken.mIntegerValid &&
               mToken.mInteger != 0) {
      integer.emplace(mToken.mInteger);
    } else {
      UngetToken();
      break;
    }
  } while (!(integer.isSome() && hasIdent) && GetToken(true));

  // Require at least one of <integer> or <custom-ident>
  if (!(integer.isSome() || hasIdent)) {
    return false;
  }

  if (!hasSpan && GetToken(true)) {
    if (mToken.mType == eCSSToken_Ident &&
        mToken.mIdent.LowerCaseEqualsLiteral("span")) {
      hasSpan = true;
    } else {
      UngetToken();
    }
  }

  nsCSSValueList* item = aValue.SetListValue();
  if (hasSpan) {
    // Given "span", a negative <integer> is invalid.
    if (integer.isSome() && integer.ref() < 0) {
      return false;
    }
    // '1' here is a dummy value.
    // The mere presence of eCSSUnit_Enumerated indicates a "span" keyword.
    item->mValue.SetIntValue(1, eCSSUnit_Enumerated);
    item->mNext = new nsCSSValueList;
    item = item->mNext;
  }
  if (integer.isSome()) {
    item->mValue.SetIntValue(integer.ref(), eCSSUnit_Integer);
    if (hasIdent) {
      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
  }
  if (hasIdent) {
    item->mValue = ident;
  }
  return true;
}

bool
CSSParserImpl::ParseGridColumnRowStartEnd(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr) ||
      ParseGridLine(value)) {
    AppendValue(aPropID, value);
    return true;
  }
  return false;
}

// If |aFallback| is a List containing a single Ident, set |aValue| to that.
// Otherwise, set |aValue| to Auto.
// Used with |aFallback| from ParseGridLine()
static void
HandleGridLineFallback(const nsCSSValue& aFallback, nsCSSValue& aValue)
{
  if (aFallback.GetUnit() == eCSSUnit_List &&
      aFallback.GetListValue()->mValue.GetUnit() == eCSSUnit_Ident &&
      !aFallback.GetListValue()->mNext) {
    aValue = aFallback;
  } else {
    aValue.SetAutoValue();
  }
}

bool
CSSParserImpl::ParseGridColumnRow(nsCSSPropertyID aStartPropID,
                                  nsCSSPropertyID aEndPropID)
{
  nsCSSValue value;
  nsCSSValue secondValue;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    AppendValue(aStartPropID, value);
    AppendValue(aEndPropID, value);
    return true;
  }

  if (!ParseGridLine(value)) {
    return false;
  }
  if (GetToken(true)) {
    if (mToken.IsSymbol('/')) {
      if (ParseGridLine(secondValue)) {
        AppendValue(aStartPropID, value);
        AppendValue(aEndPropID, secondValue);
        return true;
      } else {
        return false;
      }
    }
    UngetToken();
  }

  // A single <custom-ident> is repeated to both properties,
  // anything else sets the grid-{column,row}-end property to 'auto'.
  HandleGridLineFallback(value, secondValue);

  AppendValue(aStartPropID, value);
  AppendValue(aEndPropID, secondValue);
  return true;
}

bool
CSSParserImpl::ParseGridArea()
{
  nsCSSValue values[4];
  if (ParseSingleTokenVariant(values[0], VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_grid_row_start, values[0]);
    AppendValue(eCSSProperty_grid_column_start, values[0]);
    AppendValue(eCSSProperty_grid_row_end, values[0]);
    AppendValue(eCSSProperty_grid_column_end, values[0]);
    return true;
  }

  int32_t i = 0;
  for (;;) {
    if (!ParseGridLine(values[i])) {
      return false;
    }
    if (++i == 4 || !GetToken(true)) {
      break;
    }
    if (!mToken.IsSymbol('/')) {
      UngetToken();
      break;
    }
  }

  MOZ_ASSERT(i >= 1, "should have parsed at least one grid-line (or returned)");
  if (i < 2) {
    HandleGridLineFallback(values[0], values[1]);
  }
  if (i < 3) {
    HandleGridLineFallback(values[0], values[2]);
  }
  if (i < 4) {
    HandleGridLineFallback(values[1], values[3]);
  }

  AppendValue(eCSSProperty_grid_row_start, values[0]);
  AppendValue(eCSSProperty_grid_column_start, values[1]);
  AppendValue(eCSSProperty_grid_row_end, values[2]);
  AppendValue(eCSSProperty_grid_column_end, values[3]);
  return true;
}

bool
CSSParserImpl::ParseGridGap()
{
  nsCSSValue first;
  if (ParseSingleTokenVariant(first, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_grid_row_gap, first);
    AppendValue(eCSSProperty_grid_column_gap, first);
    return true;
  }
  if (ParseNonNegativeVariant(first, VARIANT_LPCALC, nullptr) !=
        CSSParseResult::Ok) {
    return false;
  }
  nsCSSValue second;
  auto result = ParseNonNegativeVariant(second, VARIANT_LPCALC, nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  }
  AppendValue(eCSSProperty_grid_row_gap, first);
  AppendValue(eCSSProperty_grid_column_gap,
              result == CSSParseResult::NotFound ? first : second);
  return true;
}

// normal | [<number> <integer>?]
bool
CSSParserImpl::ParseInitialLetter()
{
  nsCSSValue value;
  // 'inherit', 'initial', 'unset', 'none', and 'normal' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NORMAL,
                               nullptr)) {
    nsCSSValue first, second;
    if (!ParseOneOrLargerNumber(first)) {
      return false;
    }

    if (!ParseOneOrLargerInteger(second)) {
      AppendValue(eCSSProperty_initial_letter, first);
      return true;
    } else {
      RefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(2);
      val->Item(0) = first;
      val->Item(1) = second;
      value.SetArrayValue(val, eCSSUnit_Array);
    }
  }
  AppendValue(eCSSProperty_initial_letter, value);
  return true;
}

// [ $aTable && <overflow-position>? ] ?
// $aTable is for <content-position> or <self-position>
bool
CSSParserImpl::ParseAlignJustifyPosition(nsCSSValue& aResult,
                                         const KTableEntry aTable[])
{
  nsCSSValue pos, overflowPos;
  int32_t value = 0;
  if (ParseEnum(pos, aTable)) {
    value = pos.GetIntValue();
    if (ParseEnum(overflowPos, nsCSSProps::kAlignOverflowPosition)) {
      value |= overflowPos.GetIntValue();
    }
    aResult.SetIntValue(value, eCSSUnit_Enumerated);
    return true;
  }
  if (ParseEnum(overflowPos, nsCSSProps::kAlignOverflowPosition)) {
    if (ParseEnum(pos, aTable)) {
      aResult.SetIntValue(pos.GetIntValue() | overflowPos.GetIntValue(),
                          eCSSUnit_Enumerated);
      return true;
    }
    return false; // <overflow-position> must be followed by a value in $table
  }
  return true;
}

// auto | normal | stretch | <baseline-position> |
// [ <self-position> && <overflow-position>? ] |
// [ legacy && [ left | right | center ] ]
bool
CSSParserImpl::ParseJustifyItems()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    if (MOZ_UNLIKELY(ParseEnum(value, nsCSSProps::kAlignLegacy))) {
      nsCSSValue legacy;
      if (!ParseEnum(legacy, nsCSSProps::kAlignLegacyPosition)) {
        return false; // leading 'legacy' not followed by 'left' etc is an error
      }
      value.SetIntValue(value.GetIntValue() | legacy.GetIntValue(),
                        eCSSUnit_Enumerated);
    } else {
      if (!ParseAlignEnum(value, nsCSSProps::kAlignAutoNormalStretchBaseline)) {
        if (!ParseAlignJustifyPosition(value, nsCSSProps::kAlignSelfPosition) ||
            value.GetUnit() == eCSSUnit_Null) {
          return false;
        }
        // check for a trailing 'legacy' after 'left' etc
        auto val = value.GetIntValue();
        if (val == NS_STYLE_JUSTIFY_CENTER ||
            val == NS_STYLE_JUSTIFY_LEFT   ||
            val == NS_STYLE_JUSTIFY_RIGHT) {
          nsCSSValue legacy;
          if (ParseEnum(legacy, nsCSSProps::kAlignLegacy)) {
            value.SetIntValue(val | legacy.GetIntValue(), eCSSUnit_Enumerated);
          }
        }
      }
    }
  }
  AppendValue(eCSSProperty_justify_items, value);
  return true;
}

// normal | stretch | <baseline-position> |
// [ <overflow-position>? && <self-position> ]
bool
CSSParserImpl::ParseAlignItems()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    if (!ParseAlignEnum(value, nsCSSProps::kAlignNormalStretchBaseline)) {
      if (!ParseAlignJustifyPosition(value, nsCSSProps::kAlignSelfPosition) ||
          value.GetUnit() == eCSSUnit_Null) {
        return false;
      }
    }
  }
  AppendValue(eCSSProperty_align_items, value);
  return true;
}

// auto | normal | stretch | <baseline-position> |
// [ <overflow-position>? && <self-position> ]
bool
CSSParserImpl::ParseAlignJustifySelf(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    if (!ParseAlignEnum(value, nsCSSProps::kAlignAutoNormalStretchBaseline)) {
      if (!ParseAlignJustifyPosition(value, nsCSSProps::kAlignSelfPosition) ||
          value.GetUnit() == eCSSUnit_Null) {
        return false;
      }
    }
  }
  AppendValue(aPropID, value);
  return true;
}

// normal | <baseline-position> | [ <content-distribution> ||
//   [ <overflow-position>? && <content-position> ] ]
// (the part after the || is called <*-position> below)
bool
CSSParserImpl::ParseAlignJustifyContent(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    if (!ParseAlignEnum(value, nsCSSProps::kAlignNormalBaseline)) {
      nsCSSValue fallbackValue;
      if (!ParseEnum(value, nsCSSProps::kAlignContentDistribution)) {
        if (!ParseAlignJustifyPosition(fallbackValue,
                                       nsCSSProps::kAlignContentPosition) ||
            fallbackValue.GetUnit() == eCSSUnit_Null) {
          return false;
        }
        // optional <content-distribution> after <*-position> ...
        if (!ParseEnum(value, nsCSSProps::kAlignContentDistribution)) {
          // ... is missing so the <*-position> is the value, not the fallback
          value = fallbackValue;
          fallbackValue.Reset();
        }
      } else {
        // any optional <*-position> is a fallback value
        if (!ParseAlignJustifyPosition(fallbackValue,
                                       nsCSSProps::kAlignContentPosition)) {
          return false;
        }
      }
      if (fallbackValue.GetUnit() != eCSSUnit_Null) {
        auto fallback = fallbackValue.GetIntValue();
        value.SetIntValue(value.GetIntValue() |
                            (fallback << NS_STYLE_ALIGN_ALL_SHIFT),
                          eCSSUnit_Enumerated);
      }
    }
  }
  AppendValue(aPropID, value);
  return true;
}

// place-content: [ normal | <baseline-position> | <content-distribution> |
//                  <content-position> ]{1,2}
bool
CSSParserImpl::ParsePlaceContent()
{
  nsCSSValue first;
  if (ParseSingleTokenVariant(first, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_align_content, first);
    AppendValue(eCSSProperty_justify_content, first);
    return true;
  }
  if (!ParseAlignEnum(first, nsCSSProps::kAlignNormalBaseline) &&
      !ParseEnum(first, nsCSSProps::kAlignContentDistribution) &&
      !ParseEnum(first, nsCSSProps::kAlignContentPosition)) {
    return false;
  }
  AppendValue(eCSSProperty_align_content, first);
  nsCSSValue second;
  if (!ParseAlignEnum(second, nsCSSProps::kAlignNormalBaseline) &&
      !ParseEnum(second, nsCSSProps::kAlignContentDistribution) &&
      !ParseEnum(second, nsCSSProps::kAlignContentPosition)) {
    AppendValue(eCSSProperty_justify_content, first);
  } else {
    AppendValue(eCSSProperty_justify_content, second);
  }
  return true;
}

// place-items:  <x> [ auto | <x> ]?
// <x> = [ normal | stretch | <baseline-position> | <self-position> ]
bool
CSSParserImpl::ParsePlaceItems()
{
  nsCSSValue first;
  if (ParseSingleTokenVariant(first, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_align_items, first);
    AppendValue(eCSSProperty_justify_items, first);
    return true;
  }
  if (!ParseAlignEnum(first, nsCSSProps::kAlignNormalStretchBaseline) &&
      !ParseEnum(first, nsCSSProps::kAlignSelfPosition)) {
    return false;
  }
  AppendValue(eCSSProperty_align_items, first);
  nsCSSValue second;
  // Note: 'auto' is valid for justify-items, but not align-items.
  if (!ParseAlignEnum(second, nsCSSProps::kAlignAutoNormalStretchBaseline) &&
      !ParseEnum(second, nsCSSProps::kAlignSelfPosition)) {
    AppendValue(eCSSProperty_justify_items, first);
  } else {
    AppendValue(eCSSProperty_justify_items, second);
  }
  return true;
}

// place-self: [ auto | normal | stretch | <baseline-position> |
//               <self-position> ]{1,2}
bool
CSSParserImpl::ParsePlaceSelf()
{
  nsCSSValue first;
  if (ParseSingleTokenVariant(first, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_align_self, first);
    AppendValue(eCSSProperty_justify_self, first);
    return true;
  }
  if (!ParseAlignEnum(first, nsCSSProps::kAlignAutoNormalStretchBaseline) &&
      !ParseEnum(first, nsCSSProps::kAlignSelfPosition)) {
    return false;
  }
  AppendValue(eCSSProperty_align_self, first);
  nsCSSValue second;
  if (!ParseAlignEnum(second, nsCSSProps::kAlignAutoNormalStretchBaseline) &&
      !ParseEnum(second, nsCSSProps::kAlignSelfPosition)) {
    AppendValue(eCSSProperty_justify_self, first);
  } else {
    AppendValue(eCSSProperty_justify_self, second);
  }
  return true;
}

// <color-stop> : <color> [ <percentage> | <length> ]?
bool
CSSParserImpl::ParseColorStop(nsCSSValueGradient* aGradient)
{
  nsCSSValueGradientStop* stop = aGradient->mStops.AppendElement();
  CSSParseResult result = ParseVariant(stop->mColor, VARIANT_COLOR, nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::NotFound) {
    stop->mIsInterpolationHint = true;
  }

  // Stop positions do not have to fall between the starting-point and
  // ending-point, so we don't use ParseNonNegativeVariant.
  result = ParseVariant(stop->mLocation, VARIANT_LP | VARIANT_CALC, nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::NotFound) {
    if (stop->mIsInterpolationHint) {
      return false;
    }
    stop->mLocation.SetNoneValue();
  }
  return true;
}

// Helper for ParseLinearGradient -- returns true iff aPosition represents a
// box-position value which was parsed with only edge keywords.
// e.g. "left top", or "bottom", but not "left 10px"
//
// (NOTE: Even though callers may want to exclude explicit "center", we still
// need to allow for _CENTER here, because omitted position-values (e.g. the
// x-component of a value like "top") will have been parsed as being *implicit*
// center. The correct way to disallow *explicit* center is to pass "false" for
// ParseBoxPositionValues()'s "aAllowExplicitCenter" parameter, before you
// call this function.)
static bool
IsBoxPositionStrictlyEdgeKeywords(nsCSSValuePair& aPosition)
{
  const nsCSSValue& xValue = aPosition.mXValue;
  const nsCSSValue& yValue = aPosition.mYValue;
  return (xValue.GetUnit() == eCSSUnit_Enumerated &&
          (xValue.GetIntValue() & (NS_STYLE_IMAGELAYER_POSITION_LEFT |
                                   NS_STYLE_IMAGELAYER_POSITION_CENTER |
                                   NS_STYLE_IMAGELAYER_POSITION_RIGHT)) &&
          yValue.GetUnit() == eCSSUnit_Enumerated &&
          (yValue.GetIntValue() & (NS_STYLE_IMAGELAYER_POSITION_TOP |
                                   NS_STYLE_IMAGELAYER_POSITION_CENTER |
                                   NS_STYLE_IMAGELAYER_POSITION_BOTTOM)));
}

// <gradient>
//    : linear-gradient( <linear-gradient-line>? <color-stops> ')'
//    | radial-gradient( <radial-gradient-line>? <color-stops> ')'
//
// <linear-gradient-line> : [ to [left | right] || [top | bottom] ] ,
//                        | <legacy-gradient-line>
// <radial-gradient-line> : [ <shape> || <size> ] [ at <position> ]? ,
//                        | [ at <position> ] ,
//                        | <legacy-gradient-line>? <legacy-shape-size>?
// <shape> : circle | ellipse
// <size> : closest-side | closest-corner | farthest-side | farthest-corner
//        | <length> | [<length> | <percentage>]{2}
//
// <legacy-gradient-line> : [ <position> || <angle>] ,
//
// <legacy-shape-size> : [ <shape> || <legacy-size> ] ,
// <legacy-size> : closest-side | closest-corner | farthest-side
//               | farthest-corner | contain | cover
//
// <color-stops> : <color-stop> , <color-stop> [, <color-stop>]*
bool
CSSParserImpl::ParseLinearGradient(nsCSSValue& aValue,
                                   uint8_t aFlags)
{
  RefPtr<nsCSSValueGradient> cssGradient
    = new nsCSSValueGradient(false, aFlags & eGradient_Repeating);

  if (!GetToken(true)) {
    return false;
  }

  // Check for "to" syntax (but not if parsing a -webkit-linear-gradient)
  if (!(aFlags & eGradient_WebkitLegacy) &&
      mToken.mType == eCSSToken_Ident &&
      mToken.mIdent.LowerCaseEqualsLiteral("to")) {

    // "to" syntax doesn't allow explicit "center"
    if (!ParseBoxPositionValues(cssGradient->mBgPos, false, false)) {
      SkipUntil(')');
      return false;
    }

    // [ to [left | right] || [top | bottom] ] ,
    if (!IsBoxPositionStrictlyEdgeKeywords(cssGradient->mBgPos)) {
      SkipUntil(')');
      return false;
    }

    if (!ExpectSymbol(',', true)) {
      SkipUntil(')');
      return false;
    }

    return ParseGradientColorStops(cssGradient, aValue);
  }

  if (!(aFlags & eGradient_AnyLegacy)) {
    // We're parsing an unprefixed linear-gradient, and we tried & failed to
    // parse a 'to' token above. Put the token back & try to re-parse our
    // expression as <angle>? <color-stop-list>
    UngetToken();

    // <angle> ,
    if (ParseSingleTokenVariant(cssGradient->mAngle,
                                VARIANT_ANGLE_OR_ZERO, nullptr) &&
        !ExpectSymbol(',', true)) {
      SkipUntil(')');
      return false;
    }

    return ParseGradientColorStops(cssGradient, aValue);
  }

  // If we get here, we're parsing a prefixed linear-gradient expression.  Put
  // back the first token (which we may have checked for "to" above) and try to
  // parse expression as <legacy-gradient-line>? <color-stop-list>
  bool haveGradientLine = IsLegacyGradientLine(mToken.mType, mToken.mIdent);
  UngetToken();

  if (haveGradientLine) {
    // Parse a <legacy-gradient-line>
    cssGradient->mIsLegacySyntax = true;
    cssGradient->mIsMozLegacySyntax = (aFlags & eGradient_MozLegacy);
    // In -webkit-linear-gradient expressions (handled below), we need to accept
    // unitless 0 for angles, to match WebKit/Blink.
    int32_t angleFlags = (aFlags & eGradient_WebkitLegacy) ?
      VARIANT_ANGLE | VARIANT_ZERO_ANGLE :
      VARIANT_ANGLE;

    bool haveAngle =
      ParseSingleTokenVariant(cssGradient->mAngle, angleFlags, nullptr);

    // If we got an angle, we might now have a comma, ending the gradient-line.
    bool haveAngleComma = haveAngle && ExpectSymbol(',', true);

    // And in fact, if we're -webkit-linear-gradient and got an angle, there
    // *must* be a comma after the angle. (Though -moz-linear-gradient is more
    // permissive because it will allow box-position before the comma.)
    if (aFlags & eGradient_WebkitLegacy && haveAngle && !haveAngleComma) {
      SkipUntil(')');
      return false;
    }

    // If we're prefixed & didn't get an angle + comma, then proceed to parse a
    // box-position. (Note that due to the above webkit-specific early-return,
    // this will only parse an *initial* box-position inside of
    // -webkit-linear-gradient, vs. an initial OR post-angle box-position
    // inside of -moz-linear-gradient.)
    if (((aFlags & eGradient_AnyLegacy) && !haveAngleComma)) {
      // (Note: 3rd arg controls whether the "center" keyword is allowed.
      // -moz-linear-gradient allows it; -webkit-linear-gradient does not.)
      if (!ParseBoxPositionValues(cssGradient->mBgPos, false,
                                  (aFlags & eGradient_MozLegacy))) {
        SkipUntil(')');
        return false;
      }

      // -webkit-linear-gradient only supports edge keywords here.
      if ((aFlags & eGradient_WebkitLegacy) &&
          !IsBoxPositionStrictlyEdgeKeywords(cssGradient->mBgPos)) {
        SkipUntil(')');
        return false;
      }

      if (!ExpectSymbol(',', true) &&
          // If we didn't already get an angle, and we're not -webkit prefixed,
          // we can parse an angle+comma now.  Otherwise it's an error.
          (haveAngle ||
           (aFlags & eGradient_WebkitLegacy) ||
           !ParseSingleTokenVariant(cssGradient->mAngle, VARIANT_ANGLE,
                                    nullptr) ||
           // now we better have a comma
           !ExpectSymbol(',', true))) {
        SkipUntil(')');
        return false;
      }
    }
  }

  return ParseGradientColorStops(cssGradient, aValue);
}

bool
CSSParserImpl::ParseRadialGradient(nsCSSValue& aValue,
                                   uint8_t aFlags)
{
  RefPtr<nsCSSValueGradient> cssGradient
    = new nsCSSValueGradient(true, aFlags & eGradient_Repeating);

  // [ <shape> || <size> ]
  bool haveShape =
    ParseSingleTokenVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                            nsCSSProps::kRadialGradientShapeKTable);

  bool haveSize =
    ParseSingleTokenVariant(cssGradient->GetRadialSize(), VARIANT_KEYWORD,
                            (aFlags & eGradient_AnyLegacy) ?
                            nsCSSProps::kRadialGradientLegacySizeKTable :
                            nsCSSProps::kRadialGradientSizeKTable);
  if (haveSize) {
    if (!haveShape) {
      // <size> <shape>
      haveShape =
        ParseSingleTokenVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                                nsCSSProps::kRadialGradientShapeKTable);
    }
  } else if (!(aFlags & eGradient_AnyLegacy)) {
    // Save RadialShape before parsing RadiusX because RadialShape and
    // RadiusX share the storage.
    int32_t shape =
      cssGradient->GetRadialShape().GetUnit() == eCSSUnit_Enumerated ?
      cssGradient->GetRadialShape().GetIntValue() : -1;
    // <length> | [<length> | <percentage>]{2}
    cssGradient->mIsExplicitSize = true;
    haveSize =
      ParseSingleTokenNonNegativeVariant(cssGradient->GetRadiusX(), VARIANT_LP,
                                         nullptr);
    if (!haveSize) {
      // It was not an explicit size after all.
      // Note that ParseNonNegativeVariant may have put something
      // invalid into our storage, but only in the case where it was
      // rejected only for being negative.  Since this means the token
      // was a length or a percentage, we know it's not valid syntax
      // (which must be a comma, the 'at' keyword, or a color), so we
      // know this value will be dropped.  This means it doesn't matter
      // that we have something invalid in our storage.
      cssGradient->mIsExplicitSize = false;
    } else {
      // vertical extent is optional
      bool haveYSize =
        ParseSingleTokenNonNegativeVariant(cssGradient->GetRadiusY(),
                                           VARIANT_LP, nullptr);
      if (!haveShape) {
        nsCSSValue shapeValue;
        haveShape =
          ParseSingleTokenVariant(shapeValue, VARIANT_KEYWORD,
                                  nsCSSProps::kRadialGradientShapeKTable);
        if (haveShape) {
          shape = shapeValue.GetIntValue();
        }
      }
      if (haveYSize
            ? shape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR
            : cssGradient->GetRadiusX().GetUnit() == eCSSUnit_Percent ||
              shape == NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL) {
        SkipUntil(')');
        return false;
      }
    }
  }

  if ((haveShape || haveSize) && ExpectSymbol(',', true)) {
    // [ <shape> || <size> ] ,
    return ParseGradientColorStops(cssGradient, aValue);
  }

  if (!GetToken(true)) {
    return false;
  }

  if (!(aFlags & eGradient_AnyLegacy)) {
    if (mToken.mType == eCSSToken_Ident &&
        mToken.mIdent.LowerCaseEqualsLiteral("at")) {
      // [ <shape> || <size> ]? at <position> ,
      if (!ParseBoxPositionValues(cssGradient->mBgPos, false) ||
          !ExpectSymbol(',', true)) {
        SkipUntil(')');
        return false;
      }

      return ParseGradientColorStops(cssGradient, aValue);
    }

    // <color-stops> only
    UngetToken();
    return ParseGradientColorStops(cssGradient, aValue);
  }
  MOZ_ASSERT(!cssGradient->mIsExplicitSize);

  nsCSSTokenType ty = mToken.mType;
  nsString id = mToken.mIdent;
  UngetToken();

  // <legacy-gradient-line>
  bool haveGradientLine = false;
  // if we already encountered a shape or size,
  // we can not have a gradient-line in legacy syntax
  if (!haveShape && !haveSize) {
      haveGradientLine = IsLegacyGradientLine(ty, id);
  }
  if (haveGradientLine) {
    // Note: -webkit-radial-gradient() doesn't accept angles.
    bool haveAngle = (aFlags & eGradient_WebkitLegacy)
      ? false
      : ParseSingleTokenVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr);

    // If we got an angle, we might now have a comma, ending the gradient-line
    if (!haveAngle || !ExpectSymbol(',', true)) {
      if (!ParseBoxPositionValues(cssGradient->mBgPos, false)) {
        SkipUntil(')');
        return false;
      }

      if (!ExpectSymbol(',', true) &&
          // If we didn't already get an angle, and we're not -webkit prefixed,
          // can parse an angle+comma now.  Otherwise it's an error.
          (haveAngle ||
           (aFlags & eGradient_WebkitLegacy) ||
           !ParseSingleTokenVariant(cssGradient->mAngle, VARIANT_ANGLE,
                                    nullptr) ||
           // now we better have a comma
           !ExpectSymbol(',', true))) {
        SkipUntil(')');
        return false;
      }
    }

    if (cssGradient->mAngle.GetUnit() != eCSSUnit_None) {
      cssGradient->mIsLegacySyntax = true;
      cssGradient->mIsMozLegacySyntax = (aFlags & eGradient_MozLegacy);
    }
  }

  // radial gradients might have a shape and size here for legacy syntax
  if (!haveShape && !haveSize) {
    haveShape =
      ParseSingleTokenVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                              nsCSSProps::kRadialGradientShapeKTable);
    haveSize =
      ParseSingleTokenVariant(cssGradient->GetRadialSize(), VARIANT_KEYWORD,
                              nsCSSProps::kRadialGradientLegacySizeKTable);

    // could be in either order
    if (!haveShape) {
      haveShape =
        ParseSingleTokenVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                                nsCSSProps::kRadialGradientShapeKTable);
    }
  }

  if ((haveShape || haveSize) && !ExpectSymbol(',', true)) {
    SkipUntil(')');
    return false;
  }

  return ParseGradientColorStops(cssGradient, aValue);
}

bool
CSSParserImpl::IsLegacyGradientLine(const nsCSSTokenType& aType,
                                    const nsString& aId)
{
  // N.B. ParseBoxPositionValues is not guaranteed to put back
  // everything it scanned if it fails, so we must only call it
  // if there is no alternative to consuming a <box-position>.
  // ParseVariant, as used here, will either succeed and consume
  // a single token, or fail and consume none, so we can be more
  // cavalier about calling it.

  bool haveGradientLine = false;
  switch (aType) {
  case eCSSToken_Percentage:
  case eCSSToken_Number:
  case eCSSToken_Dimension:
    haveGradientLine = true;
    break;

  case eCSSToken_Function:
    if (aId.LowerCaseEqualsLiteral("calc")) {
      haveGradientLine = true;
      break;
    }
    MOZ_FALLTHROUGH;
  case eCSSToken_ID:
  case eCSSToken_Hash:
    // this is a color
    break;

  case eCSSToken_Ident: {
    // This is only a gradient line if it's a box position keyword.
    nsCSSKeyword kw = nsCSSKeywords::LookupKeyword(aId);
    int32_t junk;
    if (nsCSSProps::FindKeyword(kw, nsCSSProps::kImageLayerPositionKTable,
                                junk)) {
      haveGradientLine = true;
    }
    break;
  }

  default:
    // error
    break;
  }

  return haveGradientLine;
}

bool
CSSParserImpl::ParseGradientColorStops(nsCSSValueGradient* aGradient,
                                       nsCSSValue& aValue)
{
  // At least two color stops are required
  if (!ParseColorStop(aGradient) ||
      !ExpectSymbol(',', true) ||
      !ParseColorStop(aGradient)) {
    SkipUntil(')');
    return false;
  }

  // Additional color stops
  while (ExpectSymbol(',', true)) {
    if (!ParseColorStop(aGradient)) {
      SkipUntil(')');
      return false;
    }
  }

  if (!ExpectSymbol(')', true)) {
    SkipUntil(')');
    return false;
  }

  // Check if interpolation hints are in the correct location
  bool previousPointWasInterpolationHint = true;
  for (size_t x = 0; x < aGradient->mStops.Length(); x++) {
    bool isInterpolationHint = aGradient->mStops[x].mIsInterpolationHint;
    if (isInterpolationHint && previousPointWasInterpolationHint) {
      return false;
    }
    previousPointWasInterpolationHint = isInterpolationHint;
  }

  if (previousPointWasInterpolationHint) {
    return false;
  }

  aValue.SetGradientValue(aGradient);
  return true;
}

// Parses the x or y component of a -webkit-gradient() <point> expression.
// See ParseWebkitGradientPoint() documentation for more.
bool
CSSParserImpl::ParseWebkitGradientPointComponent(nsCSSValue& aComponent,
                                                 bool aIsHorizontal)
{
  // Attempts to use ParseVariant to process the token as a number (representing
  // pixels), or a percent, or a calc expression of purely one or the other of
  // those (we enforce this pureness via ComputeCalc below). If ParseVariant
  // fails, the token may instead be a keyword or unknown token, in which case
  // case we execute the rest of the function.
  CSSParseResult status = ParseVariant(aComponent, VARIANT_PN | VARIANT_CALC,
                                       nullptr);
  if (status == CSSParseResult::Error) {
    return false;
  }
  if (status == CSSParseResult::Ok) {
    switch (aComponent.GetUnit()) {
      case eCSSUnit_Number:
        aComponent.SetFloatValue(aComponent.GetFloatValue(), eCSSUnit_Pixel);
        return true;
      case eCSSUnit_Calc: {
        float result;
        ReduceCalcOps<float, eCSSUnit_Number> opsNumber;
        if (ComputeCalc(result, aComponent, opsNumber)) {
          aComponent.SetFloatValue(result, eCSSUnit_Pixel);
          return true;
        }
        ReduceCalcOps<float, eCSSUnit_Percent> opsPercent;
        if (ComputeCalc(result, aComponent, opsPercent)) {
          aComponent.SetPercentValue(result);
          return true;
        }
        return false;
      }
      case eCSSUnit_Percent:
        return true;
      default:
        MOZ_ASSERT(false, "ParseVariant returned value with unexpected unit");
        return false;
    }
  }

  if (!GetToken(true)) {
    return false;
  }

  // Keyword tables to use for keyword-matching
  // (Keyword order is important; we assume the index can be multiplied by 50%
  // to convert to a percent-valued component.)
  static const nsCSSKeyword kHorizKeywords[] = {
    eCSSKeyword_left,   //   0%
    eCSSKeyword_center, //  50%
    eCSSKeyword_right   // 100%
  };
  static const nsCSSKeyword kVertKeywords[] = {
    eCSSKeyword_top,     //   0%
    eCSSKeyword_center,  //  50%
    eCSSKeyword_bottom   // 100%
  };
  static const size_t kNumKeywords = MOZ_ARRAY_LENGTH(kHorizKeywords);
  static_assert(kNumKeywords == MOZ_ARRAY_LENGTH(kVertKeywords),
                "Horizontal & vertical keyword tables must have same count");

  // Try to parse the component as a number, or a percent, or a
  // keyword-converted-to-percent.
  if (mToken.mType == eCSSToken_Number) {
    aComponent.SetFloatValue(mToken.mNumber, eCSSUnit_Pixel);
  } else if (mToken.mType == eCSSToken_Percentage) {
    aComponent.SetPercentValue(mToken.mNumber);
  } else if (mToken.mType == eCSSToken_Ident) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    if (keyword == eCSSKeyword_UNKNOWN) {
      return false;
    }
    // Choose our keyword table:
    const nsCSSKeyword* kwTable = aIsHorizontal ? kHorizKeywords : kVertKeywords;
    // Convert keyword to percent value (0%, 50%, or 100%)
    bool didAcceptKeyword = false;
    for (size_t i = 0; i < kNumKeywords; i++) {
      if (keyword == kwTable[i]) {
        // 0%, 50%, or 100%:
        aComponent.SetPercentValue(i * 0.5);
        didAcceptKeyword = true;
        break;
      }
    }
    if (!didAcceptKeyword) {
      return false;
    }
  } else {
    // Unrecognized token type. Put it back. (It might be a closing-paren of an
    // invalid -webkit-gradient(...) expression, and we need to be sure caller
    // can see it & stops parsing at that point.)
    UngetToken();
    return false;
  }

  MOZ_ASSERT(aComponent.GetUnit() == eCSSUnit_Pixel ||
             aComponent.GetUnit() == eCSSUnit_Percent,
             "If we get here, we should've successfully parsed a number (as a "
             "pixel length), a percent, or a keyword (converted to percent)");
  return true;
}

// This function parses a "<point>" expression for -webkit-gradient(...)
// Quoting https://www.webkit.org/blog/175/introducing-css-gradients/ :
//   "A point is a pair of space-separated values.
//    The syntax supports numbers, percentages or
//    the keywords top, bottom, left and right
//    for point values."
//
// Two additional notes:
//  - WebKit also accepts the "center" keyword (not listed in the text above).
//  - WebKit only accepts horizontal-flavored keywords (left/center/right) in
//    the first ("x") component, and vertical-flavored keywords
//    (top/center/bottom) in the second ("y") component. (This is different
//    from the standard gradient syntax, which accepts both orderings, e.g.
//    "top left" as well as "left top".)
bool
CSSParserImpl::ParseWebkitGradientPoint(nsCSSValuePair& aPoint)
{
  return ParseWebkitGradientPointComponent(aPoint.mXValue, true) &&
    ParseWebkitGradientPointComponent(aPoint.mYValue, false);
}

// Parse the next token as a <number> (for a <radius> in a -webkit-gradient
// expresison).  Returns true on success; returns false & puts back
// whatever it parsed on failure.
bool
CSSParserImpl::ParseWebkitGradientRadius(float& aRadius)
{
  nsCSSValue parseResult;
  CSSParseResult status = ParseVariant(parseResult,
                                       VARIANT_NUMBER | VARIANT_CALC, nullptr);
  if (status != CSSParseResult::Ok) {
    return false;
  }
  switch (parseResult.GetUnit()) {
    case eCSSUnit_Number:
      aRadius = parseResult.GetFloatValue();
      return true;
    case eCSSUnit_Calc: {
      ReduceCalcOps<float, eCSSUnit_Number> ops;
      if (!ComputeCalc(aRadius, parseResult, ops)) {
        MOZ_ASSERT_UNREACHABLE("unexpected unit");
      }
      return true;
    }
    default:
      MOZ_ASSERT(false, "ParseVariant returned value with unexpected unit");
      return false;
  }
}

// Parse one of:
//  color-stop(number|percent, color)
//  from(color)
//  to(color)
//
// Quoting https://www.webkit.org/blog/175/introducing-css-gradients/ :
//   A stop is a function, color-stop, that takes two arguments, the stop value
//   (either a percentage or a number between 0 and 1.0), and a color (any
//   valid CSS color). In addition the shorthand functions from and to are
//   supported. These functions only require a color argument and are
//   equivalent to color-stop(0, ...) and color-stop(1.0, ...) respectively.
bool
CSSParserImpl::ParseWebkitGradientColorStop(nsCSSValueGradient* aGradient)
{
  MOZ_ASSERT(aGradient, "null gradient");

  if (!GetToken(true)) {
    return false;
  }

  // We're expecting color-stop(...), from(...), or to(...) which are all
  // functions. Bail if we got anything else.
  if (mToken.mType != eCSSToken_Function) {
    UngetToken();
    return false;
  }

  nsCSSValueGradientStop* stop = aGradient->mStops.AppendElement();

  // Parse color-stop location (or infer it, for shorthands "from"/"to"):
  if (mToken.mIdent.LowerCaseEqualsLiteral("color-stop")) {
    // Parse stop location, followed by comma.
    if (!ParseSingleTokenVariant(stop->mLocation,
                                 VARIANT_NUMBER | VARIANT_PERCENT,
                                 nullptr) ||
        !ExpectSymbol(',', true)) {
      SkipUntil(')'); // Skip to end of color-stop(...) expression.
      return false;
    }

    // If we got a <number>, convert it to percentage for consistency:
    if (stop->mLocation.GetUnit() == eCSSUnit_Number) {
      stop->mLocation.SetPercentValue(stop->mLocation.GetFloatValue());
    }
  } else if (mToken.mIdent.LowerCaseEqualsLiteral("from")) {
    // Shorthand for color-stop(0%, ...)
    stop->mLocation.SetPercentValue(0.0f);
  } else if (mToken.mIdent.LowerCaseEqualsLiteral("to")) {
    // Shorthand for color-stop(100%, ...)
    stop->mLocation.SetPercentValue(1.0f);
  } else {
    // Unrecognized function name (invalid for a -webkit-gradient color stop).
    UngetToken();
    return false;
  }

  CSSParseResult result = ParseVariant(stop->mColor, VARIANT_COLOR, nullptr);
  if (result != CSSParseResult::Ok ||
      (stop->mColor.GetUnit() == eCSSUnit_EnumColor &&
       stop->mColor.GetIntValue() == NS_COLOR_CURRENTCOLOR)) {
    // Parse failure, or parsed "currentColor" which is forbidden in
    // -webkit-gradient for some reason.
    SkipUntil(')');
    return false;
  }

  // Parse color-stop function close-paren
  if (!ExpectSymbol(')', true)) {
    SkipUntil(')');
    return false;
  }

  MOZ_ASSERT(stop->mLocation.GetUnit() == eCSSUnit_Percent,
             "Should produce only percent-valued stop-locations. "
             "(Caller depends on this when sorting color stops.)");

  return true;
}

// Comparatison function to use for sorting -webkit-gradient() stops by
// location. This function assumes stops have percent-valued locations (and
// CSSParserImpl::ParseWebkitGradientColorStop should enforce this).
static bool
IsColorStopPctLocationLessThan(const nsCSSValueGradientStop& aStop1,
                               const nsCSSValueGradientStop& aStop2) {
  return (aStop1.mLocation.GetPercentValue() <
          aStop2.mLocation.GetPercentValue());
}

// This function parses a list of comma-separated color-stops for a
// -webkit-gradient(...) expression, and then pads & sorts the list as-needed.
bool
CSSParserImpl::ParseWebkitGradientColorStops(nsCSSValueGradient* aGradient)
{
  MOZ_ASSERT(aGradient, "null gradient");

  // Parse any number of ", <color-stop>" expressions. (0 or more)
  // Note: This is different from unprefixed gradient syntax, which
  // requires at least 2 stops.
  while (ExpectSymbol(',', true)) {
    if (!ParseWebkitGradientColorStop(aGradient)) {
      return false;
    }
  }

  // Pad up to 2 stops as-needed:
  // (Modern gradient expressions are required to have at least 2 stops, so we
  // depend on this internally -- e.g. we have an assertion about this in
  // nsCSSRendering.cpp. -webkit-gradient syntax allows 0 stops or 1 stop,
  // though, so we just pad up to 2 stops in this case).

  // If we have no stops, pad with transparent-black:
  if (aGradient->mStops.IsEmpty()) {
    nsCSSValueGradientStop* stop1 = aGradient->mStops.AppendElement();
    stop1->mColor.SetIntegerColorValue(NS_RGBA(0, 0, 0, 0),
                                       eCSSUnit_RGBAColor);
    stop1->mLocation.SetPercentValue(0.0f);

    nsCSSValueGradientStop* stop2 = aGradient->mStops.AppendElement();
    stop2->mColor.SetIntegerColorValue(NS_RGBA(0, 0, 0, 0),
                                       eCSSUnit_RGBAColor);
    stop2->mLocation.SetPercentValue(1.0f);
  } else if (aGradient->mStops.Length() == 1) {
    // Copy whatever the author provided in the first stop:
    nsCSSValueGradientStop* stop = aGradient->mStops.AppendElement();
    *stop = aGradient->mStops[0];
  } else {
    // We have >2 stops. Sort them in order of increasing location.
    std::stable_sort(aGradient->mStops.begin(),
                     aGradient->mStops.end(),
                     IsColorStopPctLocationLessThan);
  }
  return true;
}

// Compares aStartCoord to aEndCoord, and returns true iff they share the same
// unit (both pixel, or both percent) and aStartCoord is larger.
static bool
IsWebkitGradientCoordLarger(const nsCSSValue& aStartCoord,
                            const nsCSSValue& aEndCoord)
{
  if (aStartCoord.GetUnit() == eCSSUnit_Percent &&
      aEndCoord.GetUnit() == eCSSUnit_Percent) {
    return aStartCoord.GetPercentValue() > aEndCoord.GetPercentValue();
  }

  if (aStartCoord.GetUnit() == eCSSUnit_Pixel &&
      aEndCoord.GetUnit() == eCSSUnit_Pixel) {
    return aStartCoord.GetFloatValue() > aEndCoord.GetFloatValue();
  }

  // We can't compare them, since their units differ. Returning false suggests
  // that aEndCoord is larger, which is probably a decent guess anyway.
  return false;
}

// Finalize our internal representation of a -webkit-gradient(linear, ...)
// expression, given the parsed points.  (The parsed color stops
// should already be hanging off of the passed-in nsCSSValueGradient.)
//
// Note: linear gradients progress along a line between two points.  The
// -webkit-gradient(linear, ...) syntax lets the author precisely specify the
// starting and ending point. However, our internal gradient structures only
// store ONE point (which for modern linear-gradient() expressions is a side or
// corner & represents the ending point of the gradient -- and the starting
// point is implicitly the opposite side/corner).
//
// In this function, we analyze the start & end points from a
// -webkit-gradient(linear, ...) expression, and we choose an appropriate
// target point to produce a modern linear-gradient() representation that has
// the same rough trajectory.  If we can't determine a target point for some
// reason, we just fall back to the default top-to-bottom linear-gradient()
// directionality.
void
CSSParserImpl::FinalizeLinearWebkitGradient(nsCSSValueGradient* aGradient,
                                            const nsCSSValuePair& aStartPoint,
                                            const nsCSSValuePair& aEndPoint)
{
  MOZ_ASSERT(!aGradient->mIsRadial, "passed-in gradient must be linear");

  if (aStartPoint == aEndPoint ||
      aStartPoint.mXValue.GetUnit() != aEndPoint.mXValue.GetUnit() ||
      aStartPoint.mYValue.GetUnit() != aEndPoint.mYValue.GetUnit()) {
    // Start point & end point are the same, OR they use different units for at
    // least one coordinate.  Either way, this isn't something we can represent
    // using an unprefixed linear-gradient expression, so instead we'll just
    // arbitrarily pretend this is a top-to-bottom gradient (which is the
    // default for modern linear-gradient if a direction is unspecified).
    aGradient->mBgPos.mYValue.SetIntValue(NS_STYLE_IMAGELAYER_POSITION_BOTTOM,
                                          eCSSUnit_Enumerated);
    aGradient->mBgPos.mXValue.SetIntValue(NS_STYLE_IMAGELAYER_POSITION_CENTER,
                                          eCSSUnit_Enumerated);
    return;
  }

  // Set the target point to one of the box's corners (or the center of an
  // edge), by comparing aStartPoint and aEndPoint to extract a general
  // direction.

  int32_t targetX;
  if (aStartPoint.mXValue == aEndPoint.mXValue) {
    targetX = NS_STYLE_IMAGELAYER_POSITION_CENTER;
  } else if (IsWebkitGradientCoordLarger(aStartPoint.mXValue,
                                         aEndPoint.mXValue)) {
    targetX = NS_STYLE_IMAGELAYER_POSITION_LEFT;
  } else {
    MOZ_ASSERT(IsWebkitGradientCoordLarger(aEndPoint.mXValue,
                                           aStartPoint.mXValue),
               "IsWebkitGradientCoordLarger returning inconsistent results?");
    targetX = NS_STYLE_IMAGELAYER_POSITION_RIGHT;
  }

  int32_t targetY;
  if (aStartPoint.mYValue == aEndPoint.mYValue) {
    targetY = NS_STYLE_IMAGELAYER_POSITION_CENTER;
  } else if (IsWebkitGradientCoordLarger(aStartPoint.mYValue,
                                         aEndPoint.mYValue)) {
    targetY = NS_STYLE_IMAGELAYER_POSITION_TOP;
  } else {
    MOZ_ASSERT(IsWebkitGradientCoordLarger(aEndPoint.mYValue,
                                           aStartPoint.mYValue),
               "IsWebkitGradientCoordLarger returning inconsistent results?");
    targetY = NS_STYLE_IMAGELAYER_POSITION_BOTTOM;
  }

  aGradient->mBgPos.mXValue.SetIntValue(targetX, eCSSUnit_Enumerated);
  aGradient->mBgPos.mYValue.SetIntValue(targetY, eCSSUnit_Enumerated);
}

// Finalize our internal representation of a -webkit-gradient(radial, ...)
// expression, given the parsed points & radii.  (The parsed color-stops
// should already be hanging off of the passed-in nsCSSValueGradient).
void
CSSParserImpl::FinalizeRadialWebkitGradient(nsCSSValueGradient* aGradient,
                                            const nsCSSValuePair& aFirstCenter,
                                            const nsCSSValuePair& aSecondCenter,
                                            const float aFirstRadius,
                                            const float aSecondRadius)
{
  MOZ_ASSERT(aGradient->mIsRadial, "passed-in gradient must be radial");

  // NOTE: -webkit-gradient(radial, ...) has *two arbitrary circles*, with the
  // gradient stretching between the circles' edges.  In contrast, the standard
  // syntax (and hence our data structures) can only represent *one* circle,
  // with the gradient going from its center to its edge.  To bridge this gap
  // in expressiveness, we'll just see which of our two circles is smaller, and
  // we'll treat that circle as if it were zero-sized and located at the center
  // of the larger circle. Then, we'll be able to use the same data structures
  // that we use for the standard radial-gradient syntax.
  if (aSecondRadius >= aFirstRadius) {
    // Second circle is larger.
    aGradient->mBgPos = aSecondCenter;
    aGradient->mIsExplicitSize = true;
    aGradient->GetRadiusX().SetFloatValue(aSecondRadius, eCSSUnit_Pixel);
    return;
  }

  // First circle is larger, so we'll have it be the outer circle.
  aGradient->mBgPos = aFirstCenter;
  aGradient->mIsExplicitSize = true;
  aGradient->GetRadiusX().SetFloatValue(aFirstRadius, eCSSUnit_Pixel);

  // For this to work properly (with the earlier color stops attached to the
  // first circle), we need to also reverse the color-stop list, so that
  // e.g. the author's "from" color is attached to the outer edge (the first
  // circle), rather than attached to the center (the collapsed second circle).
  std::reverse(aGradient->mStops.begin(), aGradient->mStops.end());

  // And now invert the stop locations:
  for (nsCSSValueGradientStop& colorStop : aGradient->mStops) {
    float origLocation = colorStop.mLocation.GetPercentValue();
    colorStop.mLocation.SetPercentValue(1.0f - origLocation);
  }
}

bool
CSSParserImpl::ParseWebkitGradient(nsCSSValue& aValue)
{
  // Parse type of gradient
  if (!GetToken(true)) {
    return false;
  }

  if (mToken.mType != eCSSToken_Ident) {
    UngetToken(); // Important; the token might be ")", which we're about to
                  // seek to.
    SkipUntil(')');
    return false;
  }

  bool isRadial;
  if (mToken.mIdent.LowerCaseEqualsLiteral("radial")) {
    isRadial = true;
  } else if (mToken.mIdent.LowerCaseEqualsLiteral("linear")) {
    isRadial = false;
  } else {
    // Unrecognized gradient type.
    SkipUntil(')');
    return false;
  }

  // Parse a comma + first point:
  nsCSSValuePair firstPoint;
  if (!ExpectSymbol(',', true) ||
      !ParseWebkitGradientPoint(firstPoint)) {
    SkipUntil(')');
    return false;
  }

  // If radial, parse comma + first radius:
  float firstRadius;
  if (isRadial) {
    if (!ExpectSymbol(',', true) ||
        !ParseWebkitGradientRadius(firstRadius)) {
      SkipUntil(')');
      return false;
    }
  }

  // Parse a comma + second point:
  nsCSSValuePair secondPoint;
  if (!ExpectSymbol(',', true) ||
      !ParseWebkitGradientPoint(secondPoint)) {
    SkipUntil(')');
    return false;
  }

  // If radial, parse comma + second radius:
  float secondRadius;
  if (isRadial) {
    if (!ExpectSymbol(',', true) ||
        !ParseWebkitGradientRadius(secondRadius)) {
      SkipUntil(')');
      return false;
    }
  }

  // Construct a nsCSSValueGradient object, and parse color stops into it:
  RefPtr<nsCSSValueGradient> cssGradient =
    new nsCSSValueGradient(isRadial, false /* aIsRepeating */);

  if (!ParseWebkitGradientColorStops(cssGradient) ||
      !ExpectSymbol(')', true)) {
    // Failed to parse color-stops, or found trailing junk between them & ')'.
    SkipUntil(')');
    return false;
  }

  // Finish building cssGradient, based on our parsed positioning/sizing info:
  if (isRadial) {
    FinalizeRadialWebkitGradient(cssGradient, firstPoint, secondPoint,
                           firstRadius, secondRadius);
  } else {
    FinalizeLinearWebkitGradient(cssGradient, firstPoint, secondPoint);
  }

  aValue.SetGradientValue(cssGradient);
  return true;
}

bool
CSSParserImpl::ParseWebkitTextStroke()
{
  static const nsCSSPropertyID kWebkitTextStrokeIDs[] = {
    eCSSProperty__webkit_text_stroke_width,
    eCSSProperty__webkit_text_stroke_color
  };

  const size_t numProps = MOZ_ARRAY_LENGTH(kWebkitTextStrokeIDs);
  nsCSSValue values[numProps];

  int32_t found = ParseChoice(values, kWebkitTextStrokeIDs, numProps);
  if (found < 1) {
    return false;
  }

  if (!(found & 1)) { // Provide default -webkit-text-stroke-width
    values[0].SetFloatValue(0, eCSSUnit_Pixel);
  }

  if (!(found & 2)) { // Provide default -webkit-text-stroke-color
    values[1].SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }

  for (size_t index = 0; index < numProps; ++index) {
    AppendValue(kWebkitTextStrokeIDs[index], values[index]);
  }

  return true;
}

  int32_t
CSSParserImpl::ParseChoice(nsCSSValue aValues[],
                           const nsCSSPropertyID aPropIDs[], int32_t aNumIDs)
{
  int32_t found = 0;
  nsAutoParseCompoundProperty compound(this);

  int32_t loop;
  for (loop = 0; loop < aNumIDs; loop++) {
    // Try each property parser in order
    int32_t hadFound = found;
    int32_t index;
    for (index = 0; index < aNumIDs; index++) {
      int32_t bit = 1 << index;
      if ((found & bit) == 0) {
        CSSParseResult result =
          ParseSingleValueProperty(aValues[index], aPropIDs[index]);
        if (result == CSSParseResult::Error) {
          return -1;
        }
        if (result == CSSParseResult::Ok) {
          found |= bit;
          // It's more efficient to break since it will reset |hadFound|
          // to |found|.  Furthermore, ParseListStyle depends on our going
          // through the properties in order for each value..
          break;
        }
      }
    }
    if (found == hadFound) {  // found nothing new
      break;
    }
  }
  if (0 < found) {
    if (1 == found) { // only first property
      if (eCSSUnit_Inherit == aValues[0].GetUnit()) { // one inherit, all inherit
        for (loop = 1; loop < aNumIDs; loop++) {
          aValues[loop].SetInheritValue();
        }
        found = ((1 << aNumIDs) - 1);
      }
      else if (eCSSUnit_Initial == aValues[0].GetUnit()) { // one initial, all initial
        for (loop = 1; loop < aNumIDs; loop++) {
          aValues[loop].SetInitialValue();
        }
        found = ((1 << aNumIDs) - 1);
      }
      else if (eCSSUnit_Unset == aValues[0].GetUnit()) { // one unset, all unset
        for (loop = 1; loop < aNumIDs; loop++) {
          aValues[loop].SetUnsetValue();
        }
        found = ((1 << aNumIDs) - 1);
      }
    }
    else {  // more than one value, verify no inherits, initials or unsets
      for (loop = 0; loop < aNumIDs; loop++) {
        if (eCSSUnit_Inherit == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
        else if (eCSSUnit_Initial == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
        else if (eCSSUnit_Unset == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
      }
    }
  }
  return found;
}

void
CSSParserImpl::AppendValue(nsCSSPropertyID aPropID, const nsCSSValue& aValue)
{
  mTempData.AddLonghandProperty(aPropID, aValue);
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values.
 */
bool
CSSParserImpl::ParseBoxProperties(const nsCSSPropertyID aPropIDs[])
{
  // Get up to four values for the property
  int32_t count = 0;
  nsCSSRect result;
  NS_FOR_CSS_SIDES (index) {
    CSSParseResult parseResult =
      ParseBoxProperty(result.*(nsCSSRect::sides[index]), aPropIDs[index]);
    if (parseResult == CSSParseResult::NotFound) {
      break;
    }
    if (parseResult == CSSParseResult::Error) {
      return false;
    }
    count++;
  }
  if (count == 0) {
    return false;
  }

  if (1 < count) { // verify no more than single inherit, initial or unset
    NS_FOR_CSS_SIDES (index) {
      nsCSSUnit unit = (result.*(nsCSSRect::sides[index])).GetUnit();
      if (eCSSUnit_Inherit == unit ||
          eCSSUnit_Initial == unit ||
          eCSSUnit_Unset == unit) {
        return false;
      }
    }
  }

  // Provide missing values by replicating some of the values found
  switch (count) {
    case 1: // Make right == top
      result.mRight = result.mTop;
      MOZ_FALLTHROUGH;
    case 2: // Make bottom == top
      result.mBottom = result.mTop;
      MOZ_FALLTHROUGH;
    case 3: // Make left == right
      result.mLeft = result.mRight;
  }

  NS_FOR_CSS_SIDES (index) {
    AppendValue(aPropIDs[index], result.*(nsCSSRect::sides[index]));
  }
  return true;
}

// Similar to ParseBoxProperties, except there is only one property
// with the result as its value, not four.
bool
CSSParserImpl::ParseGroupedBoxProperty(int32_t aVariantMask,
                                       /** outparam */ nsCSSValue& aValue,
                                       uint32_t aRestrictions)
{
  nsCSSRect& result = aValue.SetRectValue();

  int32_t count = 0;
  NS_FOR_CSS_SIDES (index) {
    CSSParseResult parseResult =
      ParseVariantWithRestrictions(result.*(nsCSSRect::sides[index]),
                                   aVariantMask, nullptr,
                                   aRestrictions);
    if (parseResult == CSSParseResult::NotFound) {
      break;
    }
    if (parseResult == CSSParseResult::Error) {
      return false;
    }
    count++;
  }

  if (count == 0) {
    return false;
  }

  // Provide missing values by replicating some of the values found
  switch (count) {
    case 1: // Make right == top
      result.mRight = result.mTop;
      MOZ_FALLTHROUGH;
    case 2: // Make bottom == top
      result.mBottom = result.mTop;
      MOZ_FALLTHROUGH;
    case 3: // Make left == right
      result.mLeft = result.mRight;
  }

  return true;
}

bool
CSSParserImpl::ParseBoxCornerRadius(nsCSSPropertyID aPropID)
{
  nsCSSValue dimenX, dimenY;
  // required first value
  if (ParseNonNegativeVariant(dimenX, VARIANT_HLP | VARIANT_CALC, nullptr) !=
      CSSParseResult::Ok) {
    return false;
  }

  // optional second value (forbidden if first value is inherit/initial/unset)
  if (dimenX.GetUnit() != eCSSUnit_Inherit &&
      dimenX.GetUnit() != eCSSUnit_Initial &&
      dimenX.GetUnit() != eCSSUnit_Unset) {
    if (ParseNonNegativeVariant(dimenY, VARIANT_LP | VARIANT_CALC, nullptr) ==
        CSSParseResult::Error) {
      return false;
    }
  }

  if (dimenX == dimenY || dimenY.GetUnit() == eCSSUnit_Null) {
    AppendValue(aPropID, dimenX);
  } else {
    nsCSSValue value;
    value.SetPairValue(dimenX, dimenY);
    AppendValue(aPropID, value);
  }
  return true;
}

bool
CSSParserImpl::ParseBoxCornerRadiiInternals(nsCSSValue array[])
{
  // Rectangles are used as scratch storage.
  // top => top-left, right => top-right,
  // bottom => bottom-right, left => bottom-left.
  nsCSSRect dimenX, dimenY;
  int32_t countX = 0, countY = 0;

  NS_FOR_CSS_SIDES (side) {
    CSSParseResult result =
      ParseNonNegativeVariant(dimenX.*nsCSSRect::sides[side],
                              (side > 0 ? 0 : VARIANT_INHERIT) |
                                VARIANT_LP | VARIANT_CALC,
                              nullptr);
    if (result == CSSParseResult::Error) {
      return false;
    } else if (result == CSSParseResult::NotFound) {
      break;
    }
    countX++;
  }
  if (countX == 0)
    return false;

  if (ExpectSymbol('/', true)) {
    NS_FOR_CSS_SIDES (side) {
      CSSParseResult result =
        ParseNonNegativeVariant(dimenY.*nsCSSRect::sides[side],
                                VARIANT_LP | VARIANT_CALC, nullptr);
      if (result == CSSParseResult::Error) {
        return false;
      } else if (result == CSSParseResult::NotFound) {
        break;
      }
      countY++;
    }
    if (countY == 0)
      return false;
  }

  // if 'initial', 'inherit' or 'unset' was used, it must be the only value
  if (countX > 1 || countY > 0) {
    nsCSSUnit unit = dimenX.mTop.GetUnit();
    if (eCSSUnit_Inherit == unit ||
        eCSSUnit_Initial == unit ||
        eCSSUnit_Unset == unit)
      return false;
  }

  // if we have no Y-values, use the X-values
  if (countY == 0) {
    dimenY = dimenX;
    countY = countX;
  }

  // Provide missing values by replicating some of the values found
  switch (countX) {
    case 1: // Make top-right same as top-left
      dimenX.mRight = dimenX.mTop;
      MOZ_FALLTHROUGH;
    case 2: // Make bottom-right same as top-left
      dimenX.mBottom = dimenX.mTop;
      MOZ_FALLTHROUGH;
    case 3: // Make bottom-left same as top-right
      dimenX.mLeft = dimenX.mRight;
  }

  switch (countY) {
    case 1: // Make top-right same as top-left
      dimenY.mRight = dimenY.mTop;
      MOZ_FALLTHROUGH;
    case 2: // Make bottom-right same as top-left
      dimenY.mBottom = dimenY.mTop;
      MOZ_FALLTHROUGH;
    case 3: // Make bottom-left same as top-right
      dimenY.mLeft = dimenY.mRight;
  }

  NS_FOR_CSS_SIDES(side) {
    nsCSSValue& x = dimenX.*nsCSSRect::sides[side];
    nsCSSValue& y = dimenY.*nsCSSRect::sides[side];

    if (x == y) {
      array[side] = x;
    } else {
      nsCSSValue pair;
      pair.SetPairValue(x, y);
      array[side] = pair;
    }
  }
  return true;
}

bool
CSSParserImpl::ParseBoxCornerRadii(const nsCSSPropertyID aPropIDs[])
{
  nsCSSValue value[4];
  if (!ParseBoxCornerRadiiInternals(value)) {
    return false;
  }

  NS_FOR_CSS_SIDES(side) {
    AppendValue(aPropIDs[side], value[side]);
  }
  return true;
}

// These must be in CSS order (top,right,bottom,left) for indexing to work
static const nsCSSPropertyID kBorderStyleIDs[] = {
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style
};
static const nsCSSPropertyID kBorderWidthIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width
};
static const nsCSSPropertyID kBorderColorIDs[] = {
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color
};
static const nsCSSPropertyID kBorderRadiusIDs[] = {
  eCSSProperty_border_top_left_radius,
  eCSSProperty_border_top_right_radius,
  eCSSProperty_border_bottom_right_radius,
  eCSSProperty_border_bottom_left_radius
};
static const nsCSSPropertyID kOutlineRadiusIDs[] = {
  eCSSProperty__moz_outline_radius_topleft,
  eCSSProperty__moz_outline_radius_topright,
  eCSSProperty__moz_outline_radius_bottomright,
  eCSSProperty__moz_outline_radius_bottomleft
};

void
CSSParserImpl::SaveInputState(CSSParserInputState& aState)
{
  aState.mToken = mToken;
  aState.mHavePushBack = mHavePushBack;
  mScanner->SavePosition(aState.mPosition);
}

void
CSSParserImpl::RestoreSavedInputState(const CSSParserInputState& aState)
{
  mToken = aState.mToken;
  mHavePushBack = aState.mHavePushBack;
  mScanner->RestoreSavedPosition(aState.mPosition);
}

bool
CSSParserImpl::ParseProperty(nsCSSPropertyID aPropID)
{
  // Can't use AutoRestore<bool> because it's a bitfield.
  MOZ_ASSERT(!mHashlessColorQuirk,
             "hashless color quirk should not be set");
  MOZ_ASSERT(!mUnitlessLengthQuirk,
             "unitless length quirk should not be set");
  MOZ_ASSERT(aPropID != eCSSPropertyExtra_variable);

  if (mNavQuirkMode) {
    mHashlessColorQuirk =
      nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_HASHLESS_COLOR_QUIRK);
    mUnitlessLengthQuirk =
      nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_UNITLESS_LENGTH_QUIRK);
  }

  // Save the current input state so that we can restore it later if we
  // have to re-parse the property value as a variable-reference-containing
  // token stream.
  CSSParserInputState stateBeforeProperty;
  SaveInputState(stateBeforeProperty);
  mScanner->ClearSeenVariableReference();

  NS_ASSERTION(aPropID < eCSSProperty_COUNT, "index out of range");
  bool allowVariables = true;
  bool result;
  switch (nsCSSProps::PropertyParseType(aPropID)) {
    case CSS_PROPERTY_PARSE_INACCESSIBLE: {
      // The user can't use these
      REPORT_UNEXPECTED(PEInaccessibleProperty2);
      allowVariables = false;
      result = false;
      break;
    }
    case CSS_PROPERTY_PARSE_FUNCTION: {
      result = ParsePropertyByFunction(aPropID);
      break;
    }
    case CSS_PROPERTY_PARSE_VALUE: {
      result = false;
      nsCSSValue value;
      if (ParseSingleValueProperty(value, aPropID) == CSSParseResult::Ok) {
        AppendValue(aPropID, value);
        result = true;
      }
      // XXX Report errors?
      break;
    }
    case CSS_PROPERTY_PARSE_VALUE_LIST: {
      result = ParseValueList(aPropID);
      break;
    }
    default: {
      result = false;
      allowVariables = false;
      MOZ_ASSERT(false,
                 "Property's flags field in nsCSSPropList.h is missing "
                 "one of the CSS_PROPERTY_PARSE_* constants");
      break;
    }
  }

  if (result) {
    // We need to call ExpectEndProperty() to decide whether to reparse
    // with variables.  This is needed because the property parsing may
    // have stopped upon finding a variable (e.g., 'margin: 1px var(a)')
    // in a way that future variable substitutions will be valid, or
    // because it parsed everything that's possible but we still want to
    // act as though the property contains variables even though we know
    // the substitution will never work (e.g., for 'margin: 1px 2px 3px
    // 4px 5px var(a)').
    //
    // It would be nice to find a better solution here
    // (and for the SkipUntilOneOf below), though, that doesn't depend
    // on using what we don't accept for doing parsing correctly.
    if (!ExpectEndProperty()) {
      result = false;
    }
  }

  bool seenVariable = mScanner->SeenVariableReference() ||
    (stateBeforeProperty.mHavePushBack &&
     stateBeforeProperty.mToken.mType == eCSSToken_Function &&
     stateBeforeProperty.mToken.mIdent.LowerCaseEqualsLiteral("var"));
  bool parseAsTokenStream;

  if (!result && allowVariables) {
    parseAsTokenStream = true;
    if (!seenVariable) {
      // We might have stopped parsing the property before its end and before
      // finding a variable reference.  Keep checking until the end of the
      // property.
      CSSParserInputState stateAtError;
      SaveInputState(stateAtError);

      const char16_t stopChars[] = { ';', '!', '}', ')', 0 };
      SkipUntilOneOf(stopChars);
      UngetToken();
      parseAsTokenStream = mScanner->SeenVariableReference();

      if (!parseAsTokenStream) {
        // If we parsed to the end of the propery and didn't find any variable
        // references, then the real position we want to report the error at
        // is |stateAtError|.
        RestoreSavedInputState(stateAtError);
      }
    }
  } else {
    parseAsTokenStream = false;
  }

  if (parseAsTokenStream) {
    // Go back to the start of the property value and parse it to make sure
    // its variable references are syntactically valid and is otherwise
    // balanced.
    RestoreSavedInputState(stateBeforeProperty);

    if (!mInSupportsCondition) {
      mScanner->StartRecording();
    }

    CSSVariableDeclarations::Type type;
    bool dropBackslash;
    nsString impliedCharacters;
    nsCSSValue value;
    if (ParseValueWithVariables(&type, &dropBackslash, impliedCharacters,
                                nullptr, nullptr)) {
      MOZ_ASSERT(type == CSSVariableDeclarations::eTokenStream,
                 "a non-custom property reparsed since it contained variable "
                 "references should not have been 'initial' or 'inherit'");

      nsString propertyValue;

      if (!mInSupportsCondition) {
        // If we are in an @supports condition, we don't need to store the
        // actual token stream on the nsCSSValue.
        mScanner->StopRecording(propertyValue);
        if (dropBackslash) {
          MOZ_ASSERT(!propertyValue.IsEmpty() &&
                     propertyValue[propertyValue.Length() - 1] == '\\');
          propertyValue.Truncate(propertyValue.Length() - 1);
        }
        propertyValue.Append(impliedCharacters);
      }

      if (mHavePushBack) {
        // If we came to the end of a property value that had a variable
        // reference and a token was pushed back, then it would have been
        // ended by '!', ')', ';', ']' or '}'.  We should remove it from the
        // recorded property value.
        MOZ_ASSERT(mToken.IsSymbol('!') ||
                   mToken.IsSymbol(')') ||
                   mToken.IsSymbol(';') ||
                   mToken.IsSymbol(']') ||
                   mToken.IsSymbol('}'));
        if (!mInSupportsCondition) {
          MOZ_ASSERT(!propertyValue.IsEmpty());
          MOZ_ASSERT(propertyValue[propertyValue.Length() - 1] ==
                     mToken.mSymbol);
          propertyValue.Truncate(propertyValue.Length() - 1);
        }
      }

      if (!mInSupportsCondition) {
        if (nsCSSProps::IsShorthand(aPropID)) {
          // If this is a shorthand property, we store the token stream on each
          // of its corresponding longhand properties.
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID, EnabledState()) {
            nsCSSValueTokenStream* tokenStream = new nsCSSValueTokenStream;
            tokenStream->mPropertyID = *p;
            tokenStream->mShorthandPropertyID = aPropID;
            tokenStream->mTokenStream = propertyValue;
            tokenStream->mBaseURI = mBaseURI;
            tokenStream->mSheetURI = mSheetURI;
            tokenStream->mSheetPrincipal = mSheetPrincipal;
            // XXX Should store sheet here (see bug 952338).
            // tokenStream->mSheet = mSheet;
            tokenStream->mLineNumber = stateBeforeProperty.mPosition.LineNumber();
            tokenStream->mLineOffset = stateBeforeProperty.mPosition.LineOffset();
            value.SetTokenStreamValue(tokenStream);
            AppendValue(*p, value);
          }
        } else {
          nsCSSValueTokenStream* tokenStream = new nsCSSValueTokenStream;
          tokenStream->mPropertyID = aPropID;
          tokenStream->mTokenStream = propertyValue;
          tokenStream->mBaseURI = mBaseURI;
          tokenStream->mSheetURI = mSheetURI;
          tokenStream->mSheetPrincipal = mSheetPrincipal;
          // XXX Should store sheet here (see bug 952338).
          // tokenStream->mSheet = mSheet;
          tokenStream->mLineNumber = stateBeforeProperty.mPosition.LineNumber();
          tokenStream->mLineOffset = stateBeforeProperty.mPosition.LineOffset();
          value.SetTokenStreamValue(tokenStream);
          AppendValue(aPropID, value);
        }
      }
      result = true;
    } else {
      if (!mInSupportsCondition) {
        mScanner->StopRecording();
      }
    }
  }

  if (mNavQuirkMode) {
    mHashlessColorQuirk = false;
    mUnitlessLengthQuirk = false;
  }

  return result;
}

bool
CSSParserImpl::ParsePropertyByFunction(nsCSSPropertyID aPropID)
{
  switch (aPropID) {  // handle shorthand or multiple properties
  case eCSSProperty_place_content:
    return ParsePlaceContent();
  case eCSSProperty_place_items:
    return ParsePlaceItems();
  case eCSSProperty_place_self:
    return ParsePlaceSelf();
  case eCSSProperty_background:
    return ParseImageLayers(nsStyleImageLayers::kBackgroundLayerTable);
  case eCSSProperty_background_repeat:
    return ParseImageLayerRepeat(eCSSProperty_background_repeat);
  case eCSSProperty_background_position:
    return ParseImageLayerPosition(nsStyleImageLayers::kBackgroundLayerTable);
  case eCSSProperty_background_position_x:
  case eCSSProperty_background_position_y:
    return ParseImageLayerPositionCoord(aPropID,
               aPropID == eCSSProperty_background_position_x);
  case eCSSProperty_background_size:
    return ParseImageLayerSize(eCSSProperty_background_size);
  case eCSSProperty_border:
    return ParseBorderSide(kBorderTopIDs, true);
  case eCSSProperty_border_color:
    return ParseBorderColor();
  case eCSSProperty_border_spacing:
    return ParseBorderSpacing();
  case eCSSProperty_border_style:
    return ParseBorderStyle();
  case eCSSProperty_border_block_end:
    return ParseBorderSide(kBorderBlockEndIDs, false);
  case eCSSProperty_border_block_start:
    return ParseBorderSide(kBorderBlockStartIDs, false);
  case eCSSProperty_border_bottom:
    return ParseBorderSide(kBorderBottomIDs, false);
  case eCSSProperty_border_inline_end:
    return ParseBorderSide(kBorderInlineEndIDs, false);
  case eCSSProperty_border_inline_start:
    return ParseBorderSide(kBorderInlineStartIDs, false);
  case eCSSProperty_border_left:
    return ParseBorderSide(kBorderLeftIDs, false);
  case eCSSProperty_border_right:
    return ParseBorderSide(kBorderRightIDs, false);
  case eCSSProperty_border_top:
    return ParseBorderSide(kBorderTopIDs, false);
  case eCSSProperty__moz_border_bottom_colors:
  case eCSSProperty__moz_border_left_colors:
  case eCSSProperty__moz_border_right_colors:
  case eCSSProperty__moz_border_top_colors:
    return ParseBorderColors(aPropID);
  case eCSSProperty_border_image_slice:
    return ParseBorderImageSlice(true, nullptr);
  case eCSSProperty_border_image_width:
    return ParseBorderImageWidth(true);
  case eCSSProperty_border_image_outset:
    return ParseBorderImageOutset(true);
  case eCSSProperty_border_image_repeat:
    return ParseBorderImageRepeat(true);
  case eCSSProperty_border_image:
    return ParseBorderImage();
  case eCSSProperty_border_width:
    return ParseBorderWidth();
  case eCSSProperty_border_radius:
    return ParseBoxCornerRadii(kBorderRadiusIDs);
  case eCSSProperty__moz_outline_radius:
    return ParseBoxCornerRadii(kOutlineRadiusIDs);

  case eCSSProperty_border_top_left_radius:
  case eCSSProperty_border_top_right_radius:
  case eCSSProperty_border_bottom_right_radius:
  case eCSSProperty_border_bottom_left_radius:
  case eCSSProperty__moz_outline_radius_topleft:
  case eCSSProperty__moz_outline_radius_topright:
  case eCSSProperty__moz_outline_radius_bottomright:
  case eCSSProperty__moz_outline_radius_bottomleft:
    return ParseBoxCornerRadius(aPropID);

  case eCSSProperty_box_shadow:
  case eCSSProperty_text_shadow:
    return ParseShadowList(aPropID);

  case eCSSProperty_clip:
    return ParseRect(eCSSProperty_clip);
  case eCSSProperty_columns:
    return ParseColumns();
  case eCSSProperty_column_rule:
    return ParseBorderSide(kColumnRuleIDs, false);
  case eCSSProperty_content:
    return ParseContent();
  case eCSSProperty__moz_context_properties:
    return ParseContextProperties();
  case eCSSProperty_counter_increment:
  case eCSSProperty_counter_reset:
    return ParseCounterData(aPropID);
  case eCSSProperty_cursor:
    return ParseCursor();
  case eCSSProperty_filter:
    return ParseFilter();
  case eCSSProperty_flex:
    return ParseFlex();
  case eCSSProperty_flex_flow:
    return ParseFlexFlow();
  case eCSSProperty_font:
    return ParseFont();
  case eCSSProperty_font_variant:
    return ParseFontVariant();
  case eCSSProperty_grid_auto_flow:
    return ParseGridAutoFlow();
  case eCSSProperty_grid_auto_columns:
  case eCSSProperty_grid_auto_rows:
    return ParseGridAutoColumnsRows(aPropID);
  case eCSSProperty_grid_template_areas:
    return ParseGridTemplateAreas();
  case eCSSProperty_grid_template_columns:
  case eCSSProperty_grid_template_rows:
    return ParseGridTemplateColumnsRows(aPropID);
  case eCSSProperty_grid_template:
    return ParseGridTemplate();
  case eCSSProperty_grid:
    return ParseGrid();
  case eCSSProperty_grid_column_start:
  case eCSSProperty_grid_column_end:
  case eCSSProperty_grid_row_start:
  case eCSSProperty_grid_row_end:
    return ParseGridColumnRowStartEnd(aPropID);
  case eCSSProperty_grid_column:
    return ParseGridColumnRow(eCSSProperty_grid_column_start,
                              eCSSProperty_grid_column_end);
  case eCSSProperty_grid_row:
    return ParseGridColumnRow(eCSSProperty_grid_row_start,
                              eCSSProperty_grid_row_end);
  case eCSSProperty_grid_area:
    return ParseGridArea();
  case eCSSProperty_grid_gap:
    return ParseGridGap();
  case eCSSProperty__moz_image_region:
    return ParseRect(eCSSProperty__moz_image_region);
  case eCSSProperty_align_content:
  case eCSSProperty_justify_content:
    return ParseAlignJustifyContent(aPropID);
  case eCSSProperty_align_items:
    return ParseAlignItems();
  case eCSSProperty_align_self:
  case eCSSProperty_justify_self:
    return ParseAlignJustifySelf(aPropID);
  case eCSSProperty_initial_letter:
    return ParseInitialLetter();
  case eCSSProperty_justify_items:
    return ParseJustifyItems();
  case eCSSProperty_list_style:
    return ParseListStyle();
  case eCSSProperty_margin:
    return ParseMargin();
  case eCSSProperty_object_position:
    return ParseObjectPosition();
  case eCSSProperty_outline:
    return ParseOutline();
  case eCSSProperty_overflow:
    return ParseOverflow();
  case eCSSProperty_padding:
    return ParsePadding();
  case eCSSProperty_quotes:
    return ParseQuotes();
  case eCSSProperty_text_decoration:
    return ParseTextDecoration();
  case eCSSProperty_text_emphasis:
    return ParseTextEmphasis();
  case eCSSProperty_will_change:
    return ParseWillChange();
  case eCSSProperty_transform:
  case eCSSProperty__moz_window_transform:
    return ParseTransform(false, aPropID);
  case eCSSProperty__moz_transform:
    return ParseTransform(true, eCSSProperty_transform);
  case eCSSProperty_transform_origin:
  case eCSSProperty_perspective_origin:
  case eCSSProperty__moz_window_transform_origin:
    return ParseTransformOrigin(aPropID);
  case eCSSProperty_transition:
    return ParseTransition();
  case eCSSProperty_animation:
    return ParseAnimation();
  case eCSSProperty_transition_property:
    return ParseTransitionProperty();
  case eCSSProperty_fill:
  case eCSSProperty_stroke:
    return ParsePaint(aPropID);
  case eCSSProperty_stroke_dasharray:
    return ParseDasharray();
  case eCSSProperty_marker:
    return ParseMarker();
  case eCSSProperty_paint_order:
    return ParsePaintOrder();
  case eCSSProperty_overscroll_behavior:
    return ParseOverscrollBehavior();
  case eCSSProperty_scroll_snap_type:
    return ParseScrollSnapType();
  case eCSSProperty_mask:
    return ParseImageLayers(nsStyleImageLayers::kMaskLayerTable);
  case eCSSProperty_mask_repeat:
    return ParseImageLayerRepeat(eCSSProperty_mask_repeat);
  case eCSSProperty_mask_position:
    return ParseImageLayerPosition(nsStyleImageLayers::kMaskLayerTable);
  case eCSSProperty_mask_position_x:
  case eCSSProperty_mask_position_y:
    return ParseImageLayerPositionCoord(aPropID,
               aPropID == eCSSProperty_mask_position_x);
  case eCSSProperty_mask_size:
    return ParseImageLayerSize(eCSSProperty_mask_size);
  case eCSSProperty__webkit_text_stroke:
    return ParseWebkitTextStroke();
  case eCSSProperty_all:
    return ParseAll();
  default:
    MOZ_ASSERT(false, "should not be called");
    return false;
  }
}

// Bits used in determining which background position info we have
#define BG_CENTER  NS_STYLE_IMAGELAYER_POSITION_CENTER
#define BG_TOP     NS_STYLE_IMAGELAYER_POSITION_TOP
#define BG_BOTTOM  NS_STYLE_IMAGELAYER_POSITION_BOTTOM
#define BG_LEFT    NS_STYLE_IMAGELAYER_POSITION_LEFT
#define BG_RIGHT   NS_STYLE_IMAGELAYER_POSITION_RIGHT
#define BG_CTB    (BG_CENTER | BG_TOP | BG_BOTTOM)
#define BG_TB     (BG_TOP | BG_BOTTOM)
#define BG_CLR    (BG_CENTER | BG_LEFT | BG_RIGHT)
#define BG_LR     (BG_LEFT | BG_RIGHT)

CSSParseResult
CSSParserImpl::ParseBoxProperty(nsCSSValue& aValue,
                                nsCSSPropertyID aPropID)
{
  if (aPropID < 0 || aPropID >= eCSSProperty_COUNT_no_shorthands) {
    MOZ_ASSERT(false, "must only be called for longhand properties");
    return CSSParseResult::NotFound;
  }

  MOZ_ASSERT(!nsCSSProps::PropHasFlags(aPropID,
                                       CSS_PROPERTY_VALUE_PARSER_FUNCTION),
             "must only be called for non-function-parsed properties");

  uint32_t variant = nsCSSProps::ParserVariant(aPropID);
  if (variant == 0) {
    MOZ_ASSERT(false, "must only be called for variant-parsed properties");
    return CSSParseResult::NotFound;
  }

  if (variant & ~(VARIANT_AHKLP | VARIANT_COLOR | VARIANT_CALC)) {
    MOZ_ASSERT(false, "must only be called for properties that take certain "
                      "variants");
    return CSSParseResult::NotFound;
  }

  const KTableEntry* kwtable = nsCSSProps::kKeywordTableTable[aPropID];
  uint32_t restrictions = nsCSSProps::ValueRestrictions(aPropID);

  return ParseVariantWithRestrictions(aValue, variant, kwtable, restrictions);
}

bool
CSSParserImpl::ParseSingleValuePropertyByFunction(nsCSSValue& aValue,
                                                  nsCSSPropertyID aPropID)
{
  switch (aPropID) {
    case eCSSProperty_clip_path:
      return ParseClipPath(aValue);
    case eCSSProperty_contain:
      return ParseContain(aValue);
    case eCSSProperty_font_family:
      return ParseFamily(aValue);
    case eCSSProperty_font_synthesis:
      return ParseFontSynthesis(aValue);
    case eCSSProperty_font_variant_alternates:
      return ParseFontVariantAlternates(aValue);
    case eCSSProperty_font_variant_east_asian:
      return ParseFontVariantEastAsian(aValue);
    case eCSSProperty_font_variant_ligatures:
      return ParseFontVariantLigatures(aValue);
    case eCSSProperty_font_variant_numeric:
      return ParseFontVariantNumeric(aValue);
    case eCSSProperty_font_feature_settings:
      return ParseFontFeatureSettings(aValue);
    case eCSSProperty_font_variation_settings:
      return ParseFontVariationSettings(aValue);
    case eCSSProperty_font_weight:
      return ParseFontWeight(aValue);
    case eCSSProperty_image_orientation:
      return ParseImageOrientation(aValue);
    case eCSSProperty_list_style_type:
      return ParseListStyleType(aValue);
    case eCSSProperty_scroll_snap_points_x:
      return ParseScrollSnapPoints(aValue, eCSSProperty_scroll_snap_points_x);
    case eCSSProperty_scroll_snap_points_y:
      return ParseScrollSnapPoints(aValue, eCSSProperty_scroll_snap_points_y);
    case eCSSProperty_scroll_snap_destination:
      return ParseScrollSnapDestination(aValue);
    case eCSSProperty_scroll_snap_coordinate:
      return ParseScrollSnapCoordinate(aValue);
    case eCSSProperty_shape_outside:
      return ParseShapeOutside(aValue);
    case eCSSProperty_text_align:
      return ParseTextAlign(aValue);
    case eCSSProperty_text_align_last:
      return ParseTextAlignLast(aValue);
    case eCSSProperty_text_decoration_line:
      return ParseTextDecorationLine(aValue);
    case eCSSProperty_text_combine_upright:
      return ParseTextCombineUpright(aValue);
    case eCSSProperty_text_emphasis_position:
      return ParseTextEmphasisPosition(aValue);
    case eCSSProperty_text_emphasis_style:
      return ParseTextEmphasisStyle(aValue);
    case eCSSProperty_text_overflow:
      return ParseTextOverflow(aValue);
    case eCSSProperty_touch_action:
      return ParseTouchAction(aValue);
    default:
      MOZ_ASSERT(false, "should not reach here");
      return false;
  }
}

CSSParseResult
CSSParserImpl::ParseSingleValueProperty(nsCSSValue& aValue,
                                        nsCSSPropertyID aPropID)
{
  if (aPropID == eCSSPropertyExtra_x_none_value) {
    return ParseVariant(aValue, VARIANT_NONE | VARIANT_INHERIT, nullptr);
  }

  if (aPropID == eCSSPropertyExtra_x_auto_value) {
    return ParseVariant(aValue, VARIANT_AUTO | VARIANT_INHERIT, nullptr);
  }

  if (aPropID < 0 || aPropID >= eCSSProperty_COUNT_no_shorthands) {
    MOZ_ASSERT(false, "not a single value property");
    return CSSParseResult::NotFound;
  }

  if (nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_VALUE_PARSER_FUNCTION)) {
    uint32_t lineBefore, colBefore;
    if (!GetNextTokenLocation(true, &lineBefore, &colBefore)) {
      // We're at EOF before parsing.
      return CSSParseResult::NotFound;
    }

    if (ParseSingleValuePropertyByFunction(aValue, aPropID)) {
      return CSSParseResult::Ok;
    }

    uint32_t lineAfter, colAfter;
    if (!GetNextTokenLocation(true, &lineAfter, &colAfter) ||
        lineAfter != lineBefore ||
        colAfter != colBefore) {
      // Any single token value that was invalid will have been pushed back,
      // so GetNextTokenLocation encountering EOF means we failed while
      // parsing a multi-token value.
      return CSSParseResult::Error;
    }

    return CSSParseResult::NotFound;
  }

  uint32_t variant = nsCSSProps::ParserVariant(aPropID);
  if (variant == 0) {
    MOZ_ASSERT(false, "not a single value property");
    return CSSParseResult::NotFound;
  }

  const KTableEntry* kwtable = nsCSSProps::kKeywordTableTable[aPropID];
  uint32_t restrictions = nsCSSProps::ValueRestrictions(aPropID);
  return ParseVariantWithRestrictions(aValue, variant, kwtable, restrictions);
}

// font-descriptor: descriptor ':' value ';'
// caller has advanced mToken to point at the descriptor
bool
CSSParserImpl::ParseFontDescriptorValue(nsCSSFontDesc aDescID,
                                        nsCSSValue& aValue)
{
  switch (aDescID) {
    // These four are similar to the properties of the same name,
    // possibly with more restrictions on the values they can take.
  case eCSSFontDesc_Family: {
    nsCSSValue value;
    if (!ParseFamily(value) ||
        value.GetUnit() != eCSSUnit_FontFamilyList)
      return false;

    // name can only be a single, non-generic name
    const SharedFontList* f = value.GetFontFamilyListValue();
    const nsTArray<FontFamilyName>& fontlist = f->mNames;

    if (fontlist.Length() != 1 || !fontlist[0].IsNamed()) {
      return false;
    }

    aValue.SetStringValue(fontlist[0].mName, eCSSUnit_String);
    return true;
  }

  case eCSSFontDesc_Style:
    // property is VARIANT_HMK|VARIANT_SYSFONT
    return ParseSingleTokenVariant(aValue, VARIANT_KEYWORD | VARIANT_NORMAL,
                                   nsCSSProps::kFontStyleKTable);

  case eCSSFontDesc_Display:
    return ParseSingleTokenVariant(aValue, VARIANT_KEYWORD,
                                   nsCSSProps::kFontDisplayKTable);

  case eCSSFontDesc_Weight:
    return (ParseFontWeight(aValue) &&
            aValue.GetUnit() != eCSSUnit_Inherit &&
            aValue.GetUnit() != eCSSUnit_Initial &&
            aValue.GetUnit() != eCSSUnit_Unset &&
            (aValue.GetUnit() != eCSSUnit_Enumerated ||
             (aValue.GetIntValue() != NS_STYLE_FONT_WEIGHT_BOLDER &&
              aValue.GetIntValue() != NS_STYLE_FONT_WEIGHT_LIGHTER)));

  case eCSSFontDesc_Stretch:
    // property is VARIANT_HK|VARIANT_SYSFONT
    return ParseSingleTokenVariant(aValue, VARIANT_KEYWORD,
                                   nsCSSProps::kFontStretchKTable);

    // These two are unique to @font-face and have their own special grammar.
  case eCSSFontDesc_Src:
    return ParseFontSrc(aValue);

  case eCSSFontDesc_UnicodeRange:
    return ParseFontRanges(aValue);

  case eCSSFontDesc_FontFeatureSettings:
    return ParseFontFeatureSettings(aValue);

  case eCSSFontDesc_FontLanguageOverride:
    return ParseSingleTokenVariant(aValue, VARIANT_NORMAL | VARIANT_STRING,
                                   nullptr);

  case eCSSFontDesc_UNKNOWN:
  case eCSSFontDesc_COUNT:
    NS_NOTREACHED("bad nsCSSFontDesc code");
  }
  // explicitly do NOT have a default case to let the compiler
  // help find missing descriptors
  return false;
}

static nsCSSValue
BoxPositionMaskToCSSValue(int32_t aMask, bool isX)
{
  int32_t val = NS_STYLE_IMAGELAYER_POSITION_CENTER;
  if (isX) {
    if (aMask & BG_LEFT) {
      val = NS_STYLE_IMAGELAYER_POSITION_LEFT;
    }
    else if (aMask & BG_RIGHT) {
      val = NS_STYLE_IMAGELAYER_POSITION_RIGHT;
    }
  }
  else {
    if (aMask & BG_TOP) {
      val = NS_STYLE_IMAGELAYER_POSITION_TOP;
    }
    else if (aMask & BG_BOTTOM) {
      val = NS_STYLE_IMAGELAYER_POSITION_BOTTOM;
    }
  }

  return nsCSSValue(val, eCSSUnit_Enumerated);
}

bool
CSSParserImpl::ParseImageLayers(const nsCSSPropertyID aTable[])
{
  nsAutoParseCompoundProperty compound(this);

  // background-color can only be set once, so it's not a list.
  nsCSSValue color;

  // Check first for inherit/initial/unset.
  if (ParseSingleTokenVariant(color, VARIANT_INHERIT, nullptr)) {
    // must be alone
    for (const nsCSSPropertyID* subprops =
           nsCSSProps::SubpropertyEntryFor(aTable[nsStyleImageLayers::shorthand]);
         *subprops != eCSSProperty_UNKNOWN; ++subprops) {
      AppendValue(*subprops, color);
    }
    return true;
  }

  nsCSSValue image, repeat, attachment, clip, origin, positionX, positionY, size,
             composite, maskMode;
  ImageLayersShorthandParseState state(color, image.SetListValue(),
                                       repeat.SetPairListValue(),
                                       attachment.SetListValue(), clip.SetListValue(),
                                       origin.SetListValue(),
                                       positionX.SetListValue(), positionY.SetListValue(),
                                       size.SetPairListValue(), composite.SetListValue(),
                                       maskMode.SetListValue());

  for (;;) {
    if (!ParseImageLayersItem(state, aTable)) {
      return false;
    }

    // If we saw a color, this must be the last item.
    if (color.GetUnit() != eCSSUnit_Null) {
      MOZ_ASSERT(aTable[nsStyleImageLayers::color] != eCSSProperty_UNKNOWN);
      break;
    }

    // If there's a comma, expect another item.
    if (!ExpectSymbol(',', true)) {
      break;
    }

#define APPENDNEXT(propID_, propMember_, propType_) \
  if (aTable[propID_] != eCSSProperty_UNKNOWN) { \
    propMember_->mNext = new propType_; \
    propMember_ = propMember_->mNext; \
  }
    // Chain another entry on all the lists.
    APPENDNEXT(nsStyleImageLayers::image, state.mImage,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::repeat, state.mRepeat,
               nsCSSValuePairList);
    APPENDNEXT(nsStyleImageLayers::clip, state.mClip,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::origin, state.mOrigin,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::positionX, state.mPositionX,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::positionY, state.mPositionY,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::size, state.mSize,
               nsCSSValuePairList);
    APPENDNEXT(nsStyleImageLayers::attachment, state.mAttachment,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::maskMode, state.mMode,
               nsCSSValueList);
    APPENDNEXT(nsStyleImageLayers::composite, state.mComposite,
               nsCSSValueList);
#undef APPENDNEXT
  }

  // If we get to this point without seeing a color, provide a default.
 if (aTable[nsStyleImageLayers::color] != eCSSProperty_UNKNOWN) {
    if (color.GetUnit() == eCSSUnit_Null) {
      color.SetIntegerColorValue(NS_RGBA(0,0,0,0), eCSSUnit_RGBAColor);
    }
  }

#define APPENDVALUE(propID_, propValue_) \
  if (propID_ != eCSSProperty_UNKNOWN) { \
    AppendValue(propID_,  propValue_); \
  }

  APPENDVALUE(aTable[nsStyleImageLayers::image],      image);
  APPENDVALUE(aTable[nsStyleImageLayers::repeat],     repeat);
  APPENDVALUE(aTable[nsStyleImageLayers::clip],       clip);
  APPENDVALUE(aTable[nsStyleImageLayers::origin],     origin);
  APPENDVALUE(aTable[nsStyleImageLayers::positionX],  positionX);
  APPENDVALUE(aTable[nsStyleImageLayers::positionY],  positionY);
  APPENDVALUE(aTable[nsStyleImageLayers::size],       size);
  APPENDVALUE(aTable[nsStyleImageLayers::color],      color);
  APPENDVALUE(aTable[nsStyleImageLayers::attachment], attachment);
  APPENDVALUE(aTable[nsStyleImageLayers::maskMode],   maskMode);
  APPENDVALUE(aTable[nsStyleImageLayers::composite],  composite);

#undef APPENDVALUE

  return true;
}

// Helper for ParseImageLayersItem. Returns true if the passed-in nsCSSToken is
// a function which is accepted for background-image.
bool
CSSParserImpl::IsFunctionTokenValidForImageLayerImage(
  const nsCSSToken& aToken) const
{
  MOZ_ASSERT(aToken.mType == eCSSToken_Function,
             "Should only be called for function-typed tokens");

  const nsAString& funcName = aToken.mIdent;

  return funcName.LowerCaseEqualsLiteral("linear-gradient") ||
    funcName.LowerCaseEqualsLiteral("radial-gradient") ||
    funcName.LowerCaseEqualsLiteral("repeating-linear-gradient") ||
    funcName.LowerCaseEqualsLiteral("repeating-radial-gradient") ||
    funcName.LowerCaseEqualsLiteral("-moz-linear-gradient") ||
    funcName.LowerCaseEqualsLiteral("-moz-radial-gradient") ||
    funcName.LowerCaseEqualsLiteral("-moz-repeating-linear-gradient") ||
    funcName.LowerCaseEqualsLiteral("-moz-repeating-radial-gradient") ||
    funcName.LowerCaseEqualsLiteral("-moz-image-rect") ||
    funcName.LowerCaseEqualsLiteral("-moz-element") ||
    (StylePrefs::sWebkitPrefixedAliasesEnabled &&
     (funcName.LowerCaseEqualsLiteral("-webkit-gradient") ||
      funcName.LowerCaseEqualsLiteral("-webkit-linear-gradient") ||
      funcName.LowerCaseEqualsLiteral("-webkit-radial-gradient") ||
      funcName.LowerCaseEqualsLiteral("-webkit-repeating-linear-gradient") ||
      funcName.LowerCaseEqualsLiteral("-webkit-repeating-radial-gradient")));
}

// Parse one item of the background shorthand property.
bool
CSSParserImpl::ParseImageLayersItem(
  CSSParserImpl::ImageLayersShorthandParseState& aState,
  const nsCSSPropertyID aTable[])
{
  // Fill in the values that the shorthand will set if we don't find
  // other values.
  aState.mImage->mValue.SetNoneValue();
  aState.mAttachment->mValue.SetIntValue(NS_STYLE_IMAGELAYER_ATTACHMENT_SCROLL,
                                         eCSSUnit_Enumerated);
  aState.mClip->mValue.SetEnumValue(StyleGeometryBox::BorderBox);

  aState.mRepeat->mXValue.SetIntValue(uint8_t(StyleImageLayerRepeat::Repeat),
                                      eCSSUnit_Enumerated);
  aState.mRepeat->mYValue.Reset();

  RefPtr<nsCSSValue::Array> positionXArr = nsCSSValue::Array::Create(2);
  RefPtr<nsCSSValue::Array> positionYArr = nsCSSValue::Array::Create(2);
  aState.mPositionX->mValue.SetArrayValue(positionXArr, eCSSUnit_Array);
  aState.mPositionY->mValue.SetArrayValue(positionYArr, eCSSUnit_Array);

  if (eCSSProperty_mask == aTable[nsStyleImageLayers::shorthand]) {
    aState.mOrigin->mValue.SetEnumValue(StyleGeometryBox::BorderBox);
  } else {
    aState.mOrigin->mValue.SetEnumValue(StyleGeometryBox::PaddingBox);
  }
  positionXArr->Item(1).SetPercentValue(0.0f);
  positionYArr->Item(1).SetPercentValue(0.0f);

  aState.mSize->mXValue.SetAutoValue();
  aState.mSize->mYValue.SetAutoValue();
  aState.mComposite->mValue.SetIntValue(NS_STYLE_MASK_COMPOSITE_ADD,
                                        eCSSUnit_Enumerated);
  aState.mMode->mValue.SetIntValue(NS_STYLE_MASK_MODE_MATCH_SOURCE,
                                   eCSSUnit_Enumerated);
  bool haveColor = false,
       haveImage = false,
       haveRepeat = false,
       haveAttach = false,
       havePositionAndSize = false,
       haveOrigin = false,
       haveClip = false,
       haveComposite = false,
       haveMode = false,
       haveSomething = false;

  const KTableEntry* originTable =
    nsCSSProps::kKeywordTableTable[aTable[nsStyleImageLayers::origin]];
  const KTableEntry* clipTable =
    nsCSSProps::kKeywordTableTable[aTable[nsStyleImageLayers::clip]];

  while (GetToken(true)) {
    nsCSSTokenType tt = mToken.mType;
    UngetToken(); // ...but we'll still cheat and use mToken
    if (tt == eCSSToken_Symbol) {
      // ExpectEndProperty only looks for symbols, and nothing else will
      // show up as one.
      break;
    }

    if (tt == eCSSToken_Ident) {
      nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
      int32_t dummy;
      if (keyword == eCSSKeyword_inherit ||
          keyword == eCSSKeyword_initial ||
          keyword == eCSSKeyword_unset) {
        return false;
      } else if (!haveImage && keyword == eCSSKeyword_none) {
        haveImage = true;
        if (ParseSingleValueProperty(aState.mImage->mValue,
                                     aTable[nsStyleImageLayers::image]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (!haveAttach &&
                 aTable[nsStyleImageLayers::attachment] !=
                   eCSSProperty_UNKNOWN &&
                 nsCSSProps::FindKeyword(
                   keyword, nsCSSProps::kImageLayerAttachmentKTable, dummy)) {
        haveAttach = true;
        if (ParseSingleValueProperty(aState.mAttachment->mValue,
                                     aTable[nsStyleImageLayers::attachment]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (!haveRepeat &&
                 nsCSSProps::FindKeyword(
                   keyword, nsCSSProps::kImageLayerRepeatKTable, dummy)) {
        haveRepeat = true;
        nsCSSValuePair scratch;
        if (!ParseImageLayerRepeatValues(scratch)) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
        aState.mRepeat->mXValue = scratch.mXValue;
        aState.mRepeat->mYValue = scratch.mYValue;
      } else if (!havePositionAndSize &&
                 nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kImageLayerPositionKTable, dummy)) {
        havePositionAndSize = true;

        if (!ParsePositionValueSeparateCoords(aState.mPositionX->mValue,
                                              aState.mPositionY->mValue)) {
          return false;
        }
        if (ExpectSymbol('/', true)) {
          nsCSSValuePair scratch;
          if (!ParseImageLayerSizeValues(scratch)) {
            return false;
          }
          aState.mSize->mXValue = scratch.mXValue;
          aState.mSize->mYValue = scratch.mYValue;
        }
      } else if (!haveOrigin &&
                 nsCSSProps::FindKeyword(keyword, originTable, dummy)) {
        haveOrigin = true;
        if (ParseSingleValueProperty(aState.mOrigin->mValue,
                                     aTable[nsStyleImageLayers::origin]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
        // Set clip value to origin if clip is not set yet.
        // Note that we don't set haveClip here so that it can be
        // overridden if we see it later.
        if (!haveClip) {
#ifdef DEBUG
          for (size_t i = 0; originTable[i].mValue != -1; i++) {
            // For each keyword & value in kOriginKTable, ensure that
            // kBackgroundKTable has a matching entry at the same position.
            MOZ_ASSERT(originTable[i].mKeyword == clipTable[i].mKeyword);
            MOZ_ASSERT(originTable[i].mValue == clipTable[i].mValue);
          }
#endif
          aState.mClip->mValue = aState.mOrigin->mValue;
        }
      } else if (!haveClip &&
                 nsCSSProps::FindKeyword(keyword, clipTable, dummy)) {
        // It is important that we try parsing clip later than origin
        // because if there are two <box> / <geometry-box> values, the
        // first should be origin, and the second should be clip.
        haveClip = true;
        if (ParseSingleValueProperty(aState.mClip->mValue,
                                     aTable[nsStyleImageLayers::clip]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (!haveComposite &&
                 aTable[nsStyleImageLayers::composite] !=
                   eCSSProperty_UNKNOWN &&
                 nsCSSProps::FindKeyword(
                   keyword, nsCSSProps::kImageLayerCompositeKTable, dummy)) {
        haveComposite = true;
        if (ParseSingleValueProperty(aState.mComposite->mValue,
                                     aTable[nsStyleImageLayers::composite]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (!haveMode &&
                 aTable[nsStyleImageLayers::maskMode] != eCSSProperty_UNKNOWN &&
                 nsCSSProps::FindKeyword(
                   keyword, nsCSSProps::kImageLayerModeKTable, dummy)) {
        haveMode = true;
        if (ParseSingleValueProperty(aState.mMode->mValue,
                                     aTable[nsStyleImageLayers::maskMode]) !=
            CSSParseResult::Ok) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (!haveColor &&
                 aTable[nsStyleImageLayers::color] != eCSSProperty_UNKNOWN) {
        haveColor = true;
        if (ParseSingleValueProperty(aState.mColor,
                                     aTable[nsStyleImageLayers::color]) !=
                                       CSSParseResult::Ok) {
          return false;
        }
      } else {
        return false;
      }
    } else if (tt == eCSSToken_URL ||
               (tt == eCSSToken_Function &&
                IsFunctionTokenValidForImageLayerImage(mToken))) {
      if (haveImage)
        return false;
      haveImage = true;
      if (ParseSingleValueProperty(aState.mImage->mValue,
                                   aTable[nsStyleImageLayers::image]) !=
          CSSParseResult::Ok) {
        return false;
      }
    } else if (tt == eCSSToken_Dimension ||
               tt == eCSSToken_Number ||
               tt == eCSSToken_Percentage ||
               (tt == eCSSToken_Function &&
                mToken.mIdent.LowerCaseEqualsLiteral("calc"))) {
      if (havePositionAndSize)
        return false;
      havePositionAndSize = true;
      if (!ParsePositionValueSeparateCoords(aState.mPositionX->mValue,
                                            aState.mPositionY->mValue)) {
        return false;
      }
      if (ExpectSymbol('/', true)) {
        nsCSSValuePair scratch;
        if (!ParseImageLayerSizeValues(scratch)) {
          return false;
        }
        aState.mSize->mXValue = scratch.mXValue;
        aState.mSize->mYValue = scratch.mYValue;
      }
    } else if (aTable[nsStyleImageLayers::color] != eCSSProperty_UNKNOWN) {
      if (haveColor)
        return false;
      haveColor = true;
      // Note: This parses 'inherit', 'initial' and 'unset', but
      // we've already checked for them, so it's ok.
      if (ParseSingleValueProperty(aState.mColor,
                                   aTable[nsStyleImageLayers::color]) !=
                                     CSSParseResult::Ok) {
        return false;
      }
    } else {
      return false;
    }

    haveSomething = true;
  }

  return haveSomething;
}

// This function is very similar to ParseScrollSnapCoordinate,
// ParseImageLayerPosition, and ParseImageLayersSize.
bool
CSSParserImpl::ParseValueList(nsCSSPropertyID aPropID)
{
  // aPropID is a single value prop-id
  nsCSSValue value;
  // 'initial', 'inherit' and 'unset' stand alone, no list permitted.
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    nsCSSValueList* item = value.SetListValue();
    for (;;) {
      if (ParseSingleValueProperty(item->mValue, aPropID) !=
          CSSParseResult::Ok) {
        return false;
      }
      if (!ExpectSymbol(',', true)) {
        break;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
  }
  AppendValue(aPropID, value);
  return true;
}

bool
CSSParserImpl::ParseImageLayerRepeat(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  // 'initial', 'inherit' and 'unset' stand alone, no list permitted.
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    nsCSSValuePair valuePair;
    if (!ParseImageLayerRepeatValues(valuePair)) {
      return false;
    }
    nsCSSValuePairList* item = value.SetPairListValue();
    for (;;) {
      item->mXValue = valuePair.mXValue;
      item->mYValue = valuePair.mYValue;
      if (!ExpectSymbol(',', true)) {
        break;
      }
      if (!ParseImageLayerRepeatValues(valuePair)) {
        return false;
      }
      item->mNext = new nsCSSValuePairList;
      item = item->mNext;
    }
  }

  AppendValue(aPropID, value);
  return true;
}

bool
CSSParserImpl::ParseImageLayerRepeatValues(nsCSSValuePair& aValue)
{
  nsCSSValue& xValue = aValue.mXValue;
  nsCSSValue& yValue = aValue.mYValue;

  if (ParseEnum(xValue, nsCSSProps::kImageLayerRepeatKTable)) {
    int32_t value = xValue.GetIntValue();
    // For single values set yValue as eCSSUnit_Null.
    if (value == uint8_t(StyleImageLayerRepeat::RepeatX) ||
        value == uint8_t(StyleImageLayerRepeat::RepeatY) ||
        !ParseEnum(yValue, nsCSSProps::kImageLayerRepeatPartKTable)) {
      // the caller will fail cases like "repeat-x no-repeat"
      // by expecting a list separator or an end property.
      yValue.Reset();
    }
    return true;
  }

  return false;
}

bool
CSSParserImpl::ParseImageLayerPosition(const nsCSSPropertyID aTable[])
{
  // 'initial', 'inherit' and 'unset' stand alone, no list permitted.
  nsCSSValue position;
  if (ParseSingleTokenVariant(position, VARIANT_INHERIT, nullptr)) {
    AppendValue(aTable[nsStyleImageLayers::positionX], position);
    AppendValue(aTable[nsStyleImageLayers::positionY], position);
    return true;
  }

  nsCSSValue itemValueX;
  nsCSSValue itemValueY;
  if (!ParsePositionValueSeparateCoords(itemValueX, itemValueY)) {
    return false;
  }

  nsCSSValue valueX;
  nsCSSValue valueY;
  nsCSSValueList* itemX = valueX.SetListValue();
  nsCSSValueList* itemY = valueY.SetListValue();
  for (;;) {
    itemX->mValue = itemValueX;
    itemY->mValue = itemValueY;
    if (!ExpectSymbol(',', true)) {
      break;
    }
    if (!ParsePositionValueSeparateCoords(itemValueX, itemValueY)) {
      return false;
    }
    itemX->mNext = new nsCSSValueList;
    itemY->mNext = new nsCSSValueList;
    itemX = itemX->mNext;
    itemY = itemY->mNext;
  }
  AppendValue(aTable[nsStyleImageLayers::positionX], valueX);
  AppendValue(aTable[nsStyleImageLayers::positionY], valueY);
  return true;
}

bool
CSSParserImpl::ParseImageLayerPositionCoord(nsCSSPropertyID aPropID, bool aIsHorizontal)
{
  nsCSSValue value;
  // 'initial', 'inherit' and 'unset' stand alone, no list permitted.
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    nsCSSValue itemValue;
    if (!ParseImageLayerPositionCoordItem(itemValue, aIsHorizontal)) {
      return false;
    }
    nsCSSValueList* item = value.SetListValue();
    for (;;) {
      item->mValue = itemValue;
      if (!ExpectSymbol(',', true)) {
        break;
      }
      if (!ParseImageLayerPositionCoordItem(itemValue, aIsHorizontal)) {
        return false;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
  }
  AppendValue(aPropID, value);
  return true;
}

/**
 * BoxPositionMaskToCSSValue and ParseBoxPositionValues are used
 * for parsing the CSS 2.1 background-position syntax (which has at
 * most two values).  (Compare to the css3-background syntax which
 * takes up to four values.)  Some current CSS specifications that
 * use background-position-like syntax still use this old syntax.
 **
 * Parses two values that correspond to positions in a box.  These can be
 * values corresponding to percentages of the box, raw offsets, or keywords
 * like "top," "left center," etc.
 *
 * @param aOut The nsCSSValuePair in which to place the result.
 * @param aAcceptsInherit If true, 'inherit', 'initial' and 'unset' are
 *   legal values
 * @param aAllowExplicitCenter If true, 'center' is a legal value
 * @return Whether or not the operation succeeded.
 */
bool CSSParserImpl::ParseBoxPositionValues(nsCSSValuePair &aOut,
                                           bool aAcceptsInherit,
                                           bool aAllowExplicitCenter)
{
  // First try a percentage or a length value
  nsCSSValue &xValue = aOut.mXValue,
             &yValue = aOut.mYValue;
  int32_t variantMask =
    (aAcceptsInherit ? VARIANT_INHERIT : 0) | VARIANT_LP | VARIANT_CALC;
  CSSParseResult result = ParseVariant(xValue, variantMask, nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::Ok) {
    if (eCSSUnit_Inherit == xValue.GetUnit() ||
        eCSSUnit_Initial == xValue.GetUnit() ||
        eCSSUnit_Unset == xValue.GetUnit()) {  // both are inherit, initial or unset
      yValue = xValue;
      return true;
    }
    // We have one percentage/length/calc. Get the optional second
    // percentage/length/calc/keyword.
    result = ParseVariant(yValue, VARIANT_LP | VARIANT_CALC, nullptr);
    if (result == CSSParseResult::Error) {
      return false;
    } else if (result == CSSParseResult::Ok) {
      // We have two numbers
      return true;
    }

    if (ParseEnum(yValue, nsCSSProps::kImageLayerPositionKTable)) {
      int32_t yVal = yValue.GetIntValue();
      if (!(yVal & BG_CTB)) {
        // The second keyword can only be 'center', 'top', or 'bottom'
        return false;
      }
      yValue = BoxPositionMaskToCSSValue(yVal, false);
      return true;
    }

    // If only one percentage or length value is given, it sets the
    // horizontal position only, and the vertical position will be 50%.
    yValue.SetPercentValue(0.5f);
    return true;
  }

  // Now try keywords. We do this manually to allow for the first
  // appearance of "center" to apply to the either the x or y
  // position (it's ambiguous so we have to disambiguate). Each
  // allowed keyword value is assigned it's own bit. We don't allow
  // any duplicate keywords other than center. We try to get two
  // keywords but it's okay if there is only one.
  int32_t mask = 0;
  if (ParseEnum(xValue, nsCSSProps::kImageLayerPositionKTable)) {
    int32_t bit = xValue.GetIntValue();
    mask |= bit;
    if (ParseEnum(xValue, nsCSSProps::kImageLayerPositionKTable)) {
      bit = xValue.GetIntValue();
      if (mask & (bit & ~BG_CENTER)) {
        // Only the 'center' keyword can be duplicated.
        return false;
      }
      mask |= bit;
    }
    else {
      // Only one keyword.  See if we have a length, percentage, or calc.
      result = ParseVariant(yValue, VARIANT_LP | VARIANT_CALC, nullptr);
      if (result == CSSParseResult::Error) {
        return false;
      } else if (result == CSSParseResult::Ok) {
        if (!(mask & BG_CLR)) {
          // The first keyword can only be 'center', 'left', or 'right'
          return false;
        }

        xValue = BoxPositionMaskToCSSValue(mask, true);
        return true;
      }
    }
  }

  // Check for bad input. Bad input consists of no matching keywords,
  // or pairs of x keywords or pairs of y keywords.
  if ((mask == 0) || (mask == (BG_TOP | BG_BOTTOM)) ||
      (mask == (BG_LEFT | BG_RIGHT)) ||
      (!aAllowExplicitCenter && (mask & BG_CENTER))) {
    return false;
  }

  // Create style values
  xValue = BoxPositionMaskToCSSValue(mask, true);
  yValue = BoxPositionMaskToCSSValue(mask, false);
  return true;
}

// Parses a CSS <position> value, for e.g. the 'background-position' property.
// Spec reference: http://www.w3.org/TR/css3-background/#ltpositiongt
// Invariants:
//  - Always produces a four-value array on a successful parse.
//  - The values are: X edge, X offset, Y edge, Y offset.
//  - Edges are always keywords or null.
//  - A |center| edge will not have an offset.
bool
CSSParserImpl::ParsePositionValue(nsCSSValue& aOut)
{
  RefPtr<nsCSSValue::Array> value = nsCSSValue::Array::Create(4);
  aOut.SetArrayValue(value, eCSSUnit_Array);

  // The following clarifies organisation of the array.
  nsCSSValue &xEdge   = value->Item(0),
             &xOffset = value->Item(1),
             &yEdge   = value->Item(2),
             &yOffset = value->Item(3);

  // Parse all the values into the array.
  uint32_t valueCount = 0;
  for (int32_t i = 0; i < 4; i++) {
    CSSParseResult result =
      ParseVariant(value->Item(i), VARIANT_LPCALC | VARIANT_KEYWORD,
                   nsCSSProps::kImageLayerPositionKTable);
    if (result == CSSParseResult::Error) {
      return false;
    } else if (result == CSSParseResult::NotFound) {
      break;
    }
    ++valueCount;
  }

  switch (valueCount) {
    case 4:
      // "If three or four values are given, then each <percentage> or <length>
      // represents an offset and must be preceded by a keyword, which specifies
      // from which edge the offset is given."
      if (eCSSUnit_Enumerated != xEdge.GetUnit() ||
          BG_CENTER == xEdge.GetIntValue() ||
          eCSSUnit_Enumerated == xOffset.GetUnit() ||
          eCSSUnit_Enumerated != yEdge.GetUnit() ||
          BG_CENTER == yEdge.GetIntValue() ||
          eCSSUnit_Enumerated == yOffset.GetUnit()) {
        return false;
      }
      break;
    case 3:
      // "If three or four values are given, then each <percentage> or<length>
      // represents an offset and must be preceded by a keyword, which specifies
      // from which edge the offset is given." ... "If three values are given,
      // the missing offset is assumed to be zero."
      if (eCSSUnit_Enumerated != value->Item(1).GetUnit()) {
        // keyword offset keyword
        // Second value is non-keyword, thus first value must be a non-center
        // keyword.
        if (eCSSUnit_Enumerated != value->Item(0).GetUnit() ||
            BG_CENTER == value->Item(0).GetIntValue()) {
          return false;
        }

        // Remaining value must be a keyword.
        if (eCSSUnit_Enumerated != value->Item(2).GetUnit()) {
          return false;
        }

        yOffset.Reset(); // Everything else is in the correct position.
      } else if (eCSSUnit_Enumerated != value->Item(2).GetUnit()) {
        // keyword keyword offset
        // Third value is non-keyword, thus second value must be non-center
        // keyword.
        if (BG_CENTER == value->Item(1).GetIntValue()) {
          return false;
        }

        // Remaining value must be a keyword.
        if (eCSSUnit_Enumerated != value->Item(0).GetUnit()) {
          return false;
        }

        // Move the values to the correct position in the array.
        value->Item(3) = value->Item(2); // yOffset
        value->Item(2) = value->Item(1); // yEdge
        value->Item(1).Reset(); // xOffset
      } else {
        return false;
      }
      break;
    case 2:
      // "If two values are given and at least one value is not a keyword, then
      // the first value represents the horizontal position (or offset) and the
      // second represents the vertical position (or offset)"
      if (eCSSUnit_Enumerated == value->Item(0).GetUnit()) {
        if (eCSSUnit_Enumerated == value->Item(1).GetUnit()) {
          // keyword keyword
          value->Item(2) = value->Item(1); // move yEdge to correct position
          xOffset.Reset();
          yOffset.Reset();
        } else {
          // keyword offset
          // First value must represent horizontal position.
          if ((BG_TOP | BG_BOTTOM) & value->Item(0).GetIntValue()) {
            return false;
          }
          value->Item(3) = value->Item(1); // move yOffset to correct position
          xOffset.Reset();
          yEdge.Reset();
        }
      } else {
        if (eCSSUnit_Enumerated == value->Item(1).GetUnit()) {
          // offset keyword
          // Second value must represent vertical position.
          if ((BG_LEFT | BG_RIGHT) & value->Item(1).GetIntValue()) {
            return false;
          }
          value->Item(2) = value->Item(1); // move yEdge to correct position
          value->Item(1) = value->Item(0); // move xOffset to correct position
          xEdge.Reset();
          yOffset.Reset();
        } else {
          // offset offset
          value->Item(3) = value->Item(1); // move yOffset to correct position
          value->Item(1) = value->Item(0); // move xOffset to correct position
          xEdge.Reset();
          yEdge.Reset();
        }
      }
      break;
    case 1:
      // "If only one value is specified, the second value is assumed to be
      // center."
      if (eCSSUnit_Enumerated == value->Item(0).GetUnit()) {
        xOffset.Reset();
      } else {
        value->Item(1) = value->Item(0); // move xOffset to correct position
        xEdge.Reset();
      }
      yEdge.SetIntValue(NS_STYLE_IMAGELAYER_POSITION_CENTER, eCSSUnit_Enumerated);
      yOffset.Reset();
      break;
    default:
      return false;
  }

  // For compatibility with CSS2.1 code the edges can be unspecified.
  // Unspecified edges are recorded as nullptr.
  NS_ASSERTION((eCSSUnit_Enumerated == xEdge.GetUnit()  ||
                eCSSUnit_Null       == xEdge.GetUnit()) &&
               (eCSSUnit_Enumerated == yEdge.GetUnit()  ||
                eCSSUnit_Null       == yEdge.GetUnit()) &&
                eCSSUnit_Enumerated != xOffset.GetUnit()  &&
                eCSSUnit_Enumerated != yOffset.GetUnit(),
                "Unexpected units");

  // Keywords in first and second pairs can not both be vertical or
  // horizontal keywords. (eg. left right, bottom top). Additionally,
  // non-center keyword can not be duplicated (eg. left left).
  int32_t xEdgeEnum =
          xEdge.GetUnit() == eCSSUnit_Enumerated ? xEdge.GetIntValue() : 0;
  int32_t yEdgeEnum =
          yEdge.GetUnit() == eCSSUnit_Enumerated ? yEdge.GetIntValue() : 0;
  if ((xEdgeEnum | yEdgeEnum) == (BG_LEFT | BG_RIGHT) ||
      (xEdgeEnum | yEdgeEnum) == (BG_TOP | BG_BOTTOM) ||
      (xEdgeEnum & yEdgeEnum & ~BG_CENTER)) {
    return false;
  }

  // The values could be in an order that is different than expected.
  // eg. x contains vertical information, y contains horizontal information.
  // Swap if incorrect order.
  if (xEdgeEnum & (BG_TOP | BG_BOTTOM) ||
      yEdgeEnum & (BG_LEFT | BG_RIGHT)) {
    nsCSSValue swapEdge = xEdge;
    nsCSSValue swapOffset = xOffset;
    xEdge = yEdge;
    xOffset = yOffset;
    yEdge = swapEdge;
    yOffset = swapOffset;
  }

  return true;
}

static void
AdjustEdgeOffsetPairForBasicShape(nsCSSValue& aEdge,
                                  nsCSSValue& aOffset,
                                  uint8_t aDefaultEdge)
{
  // 0 length offsets are 0%
  if (aOffset.IsLengthUnit() && aOffset.GetFloatValue() == 0.0) {
    aOffset.SetPercentValue(0);
  }

  // Default edge is top/left in the 4-value case
  // In case of 1 or 0 values, the default is center,
  // but ParsePositionValue already handles this case
  if (eCSSUnit_Null == aEdge.GetUnit()) {
    aEdge.SetIntValue(aDefaultEdge, eCSSUnit_Enumerated);
  }
  // Default offset is 0%
  if (eCSSUnit_Null == aOffset.GetUnit()) {
    aOffset.SetPercentValue(0.0);
  }
  if (eCSSUnit_Enumerated == aEdge.GetUnit() &&
      eCSSUnit_Percent == aOffset.GetUnit()) {
    switch (aEdge.GetIntValue()) {
      case NS_STYLE_IMAGELAYER_POSITION_CENTER:
        aEdge.SetIntValue(aDefaultEdge, eCSSUnit_Enumerated);
        MOZ_ASSERT(aOffset.GetPercentValue() == 0.0,
                   "center cannot be used with an offset");
        aOffset.SetPercentValue(0.5);
        break;
      case NS_STYLE_IMAGELAYER_POSITION_BOTTOM:
        MOZ_ASSERT(aDefaultEdge == NS_STYLE_IMAGELAYER_POSITION_TOP);
        aEdge.SetIntValue(aDefaultEdge, eCSSUnit_Enumerated);
        aOffset.SetPercentValue(1 - aOffset.GetPercentValue());
        break;
      case NS_STYLE_IMAGELAYER_POSITION_RIGHT:
        MOZ_ASSERT(aDefaultEdge == NS_STYLE_IMAGELAYER_POSITION_LEFT);
        aEdge.SetIntValue(aDefaultEdge, eCSSUnit_Enumerated);
        aOffset.SetPercentValue(1 - aOffset.GetPercentValue());
    }
  }
}

// https://drafts.csswg.org/css-shapes/#basic-shape-serialization
// We set values to defaults while parsing for basic shapes
// Invariants:
//  - Always produces a four-value array on a successful parse.
//  - The values are: X edge, X offset, Y edge, Y offset
//  - Edges are always keywords (not including center)
//  - Offsets are nonnull
//  - Percentage offsets have keywords folded into them,
//    so "bottom 40%" or "right 20%" will not exist.
bool
CSSParserImpl::ParsePositionValueForBasicShape(nsCSSValue& aOut)
{
  if (!ParsePositionValue(aOut)) {
    return false;
  }
  nsCSSValue::Array* value = aOut.GetArrayValue();
  nsCSSValue& xEdge   = value->Item(0);
  nsCSSValue& xOffset = value->Item(1);
  nsCSSValue& yEdge   = value->Item(2);
  nsCSSValue& yOffset = value->Item(3);
  // A keyword edge + percent offset pair can be contracted
  // into the percentage with the default value in the edge.
  // Offset lengths which are 0 can also be rewritten as 0%
  AdjustEdgeOffsetPairForBasicShape(xEdge, xOffset,
                                    NS_STYLE_IMAGELAYER_POSITION_LEFT);
  AdjustEdgeOffsetPairForBasicShape(yEdge, yOffset,
                                    NS_STYLE_IMAGELAYER_POSITION_TOP);
  return true;
}

bool
CSSParserImpl::ParsePositionValueSeparateCoords(nsCSSValue& aOutX, nsCSSValue& aOutY)
{
  nsCSSValue scratch;
  if (!ParsePositionValue(scratch)) {
    return false;
  }

  // Separate the four values into two pairs of two values for X and Y.
  RefPtr<nsCSSValue::Array> valueX = nsCSSValue::Array::Create(2);
  RefPtr<nsCSSValue::Array> valueY = nsCSSValue::Array::Create(2);
  aOutX.SetArrayValue(valueX, eCSSUnit_Array);
  aOutY.SetArrayValue(valueY, eCSSUnit_Array);

  RefPtr<nsCSSValue::Array> value = scratch.GetArrayValue();
  valueX->Item(0) = value->Item(0);
  valueX->Item(1) = value->Item(1);
  valueY->Item(0) = value->Item(2);
  valueY->Item(1) = value->Item(3);
  return true;
}

// Parses one item in a list of values for the 'background-position-x' or
// 'background-position-y' property. Does not support the start/end keywords.
// Spec reference: https://drafts.csswg.org/css-backgrounds-4/#propdef-background-position-x
bool
CSSParserImpl::ParseImageLayerPositionCoordItem(nsCSSValue& aOut, bool aIsHorizontal)
{
  RefPtr<nsCSSValue::Array> value = nsCSSValue::Array::Create(2);
  aOut.SetArrayValue(value, eCSSUnit_Array);

  nsCSSValue &edge   = value->Item(0),
             &offset = value->Item(1);

  nsCSSValue edgeOrOffset;
  CSSParseResult result =
    ParseVariant(edgeOrOffset, VARIANT_LPCALC | VARIANT_KEYWORD,
                 nsCSSProps::kImageLayerPositionKTable);
  if (result != CSSParseResult::Ok) {
    return false;
  }

  if (edgeOrOffset.GetUnit() == eCSSUnit_Enumerated) {
    edge = edgeOrOffset;

    // The edge can be followed by an optional offset.
    result = ParseVariant(offset, VARIANT_LPCALC, nullptr);
    if (result == CSSParseResult::Error) {
      return false;
    }
  } else {
    offset = edgeOrOffset;
  }

  // Keywords for horizontal properties cannot be vertical keywords, and
  // keywords for vertical properties cannot be horizontal keywords.
  // Also, if an offset is specified, the edge cannot be center.
  int32_t edgeEnum =
          edge.GetUnit() == eCSSUnit_Enumerated ? edge.GetIntValue() : 0;
  int32_t allowedKeywords =
    (aIsHorizontal ? (BG_LEFT | BG_RIGHT) : (BG_TOP | BG_BOTTOM)) |
    (offset.GetUnit() == eCSSUnit_Null ? BG_CENTER : 0);
  if (edgeEnum & ~allowedKeywords) {
    return false;
  }

  NS_ASSERTION((eCSSUnit_Enumerated == edge.GetUnit() ||
                eCSSUnit_Null       == edge.GetUnit()) &&
               eCSSUnit_Enumerated != offset.GetUnit(),
               "Unexpected units");

  return true;
}

// This function is very similar to ParseScrollSnapCoordinate,
// ParseImageLayers, and ParseImageLayerPosition.
bool
CSSParserImpl::ParseImageLayerSize(nsCSSPropertyID aPropID)
{
  nsCSSValue value;
  // 'initial', 'inherit' and 'unset' stand alone, no list permitted.
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    nsCSSValuePair valuePair;
    if (!ParseImageLayerSizeValues(valuePair)) {
      return false;
    }
    nsCSSValuePairList* item = value.SetPairListValue();
    for (;;) {
      item->mXValue = valuePair.mXValue;
      item->mYValue = valuePair.mYValue;
      if (!ExpectSymbol(',', true)) {
        break;
      }
      if (!ParseImageLayerSizeValues(valuePair)) {
        return false;
      }
      item->mNext = new nsCSSValuePairList;
      item = item->mNext;
    }
  }
  AppendValue(aPropID, value);
  return true;
}

/**
 * Parses two values that correspond to lengths for the background-size
 * property.  These can be one or two lengths (or the 'auto' keyword) or
 * percentages corresponding to the element's dimensions or the single keywords
 * 'contain' or 'cover'.  'initial', 'inherit' and 'unset' must be handled by
 * the caller if desired.
 *
 * @param aOut The nsCSSValuePair in which to place the result.
 * @return Whether or not the operation succeeded.
 */
#define BG_SIZE_VARIANT (VARIANT_LP | VARIANT_AUTO | VARIANT_CALC)
bool CSSParserImpl::ParseImageLayerSizeValues(nsCSSValuePair &aOut)
{
  // First try a percentage or a length value
  nsCSSValue &xValue = aOut.mXValue,
             &yValue = aOut.mYValue;
  CSSParseResult result =
    ParseNonNegativeVariant(xValue, BG_SIZE_VARIANT, nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::Ok) {
    // We have one percentage/length/calc/auto. Get the optional second
    // percentage/length/calc/keyword.
    result = ParseNonNegativeVariant(yValue, BG_SIZE_VARIANT, nullptr);
    if (result == CSSParseResult::Error) {
      return false;
    } else if (result == CSSParseResult::Ok) {
      // We have a second percentage/length/calc/auto.
      return true;
    }

    // If only one percentage or length value is given, it sets the
    // horizontal size only, and the vertical size will be as if by 'auto'.
    yValue.SetAutoValue();
    return true;
  }

  // Now address 'contain' and 'cover'.
  if (!ParseEnum(xValue, nsCSSProps::kImageLayerSizeKTable))
    return false;
  yValue.Reset();
  return true;
}

#undef BG_SIZE_VARIANT

bool
CSSParserImpl::ParseBorderColor()
{
  return ParseBoxProperties(kBorderColorIDs);
}

void
CSSParserImpl::SetBorderImageInitialValues()
{
  // border-image-source: none
  nsCSSValue source;
  source.SetNoneValue();
  AppendValue(eCSSProperty_border_image_source, source);

  // border-image-slice: 100%
  nsCSSValue sliceBoxValue;
  nsCSSRect& sliceBox = sliceBoxValue.SetRectValue();
  sliceBox.SetAllSidesTo(nsCSSValue(1.0f, eCSSUnit_Percent));
  nsCSSValue slice;
  nsCSSValueList* sliceList = slice.SetListValue();
  sliceList->mValue = sliceBoxValue;
  AppendValue(eCSSProperty_border_image_slice, slice);

  // border-image-width: 1
  nsCSSValue width;
  nsCSSRect& widthBox = width.SetRectValue();
  widthBox.SetAllSidesTo(nsCSSValue(1.0f, eCSSUnit_Number));
  AppendValue(eCSSProperty_border_image_width, width);

  // border-image-outset: 0
  nsCSSValue outset;
  nsCSSRect& outsetBox = outset.SetRectValue();
  outsetBox.SetAllSidesTo(nsCSSValue(0.0f, eCSSUnit_Number));
  AppendValue(eCSSProperty_border_image_outset, outset);

  // border-image-repeat: repeat
  nsCSSValue repeat;
  nsCSSValuePair repeatPair;
  repeatPair.SetBothValuesTo(nsCSSValue(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH,
                                        eCSSUnit_Enumerated));
  repeat.SetPairValue(&repeatPair);
  AppendValue(eCSSProperty_border_image_repeat, repeat);
}

bool
CSSParserImpl::ParseBorderImageSlice(bool aAcceptsInherit,
                                     bool* aConsumedTokens)
{
  // border-image-slice: initial | [<number>|<percentage>]{1,4} && fill?
  nsCSSValue value;

  if (aConsumedTokens) {
    *aConsumedTokens = true;
  }

  if (aAcceptsInherit &&
      ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keywords "inherit", "initial" and "unset" can not be mixed, so we
    // are done.
    AppendValue(eCSSProperty_border_image_slice, value);
    return true;
  }

  // Try parsing "fill" value.
  nsCSSValue imageSliceFillValue;
  bool hasFill = ParseEnum(imageSliceFillValue,
                           nsCSSProps::kBorderImageSliceKTable);

  // Parse the box dimensions.
  nsCSSValue imageSliceBoxValue;
  if (!ParseGroupedBoxProperty(VARIANT_PN, imageSliceBoxValue,
                               CSS_PROPERTY_VALUE_NONNEGATIVE)) {
    if (!hasFill && aConsumedTokens) {
      *aConsumedTokens = false;
    }

    return false;
  }

  // Try parsing "fill" keyword again if the first time failed because keyword
  // and slice dimensions can be in any order.
  if (!hasFill) {
    hasFill = ParseEnum(imageSliceFillValue,
                        nsCSSProps::kBorderImageSliceKTable);
  }

  nsCSSValueList* borderImageSlice = value.SetListValue();
  // Put the box value into the list.
  borderImageSlice->mValue = imageSliceBoxValue;

  if (hasFill) {
    // Put the "fill" value into the list.
    borderImageSlice->mNext = new nsCSSValueList;
    borderImageSlice->mNext->mValue = imageSliceFillValue;
  }

  AppendValue(eCSSProperty_border_image_slice, value);
  return true;
}

bool
CSSParserImpl::ParseBorderImageWidth(bool aAcceptsInherit)
{
  // border-image-width: initial | [<length>|<number>|<percentage>|auto]{1,4}
  nsCSSValue value;

  if (aAcceptsInherit &&
      ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keywords "inherit", "initial" and "unset" can not be mixed, so we
    // are done.
    AppendValue(eCSSProperty_border_image_width, value);
    return true;
  }

  // Parse the box dimensions.
  if (!ParseGroupedBoxProperty(VARIANT_ALPN, value, CSS_PROPERTY_VALUE_NONNEGATIVE)) {
    return false;
  }

  AppendValue(eCSSProperty_border_image_width, value);
  return true;
}

bool
CSSParserImpl::ParseBorderImageOutset(bool aAcceptsInherit)
{
  // border-image-outset: initial | [<length>|<number>]{1,4}
  nsCSSValue value;

  if (aAcceptsInherit &&
      ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keywords "inherit", "initial" and "unset" can not be mixed, so we
    // are done.
    AppendValue(eCSSProperty_border_image_outset, value);
    return true;
  }

  // Parse the box dimensions.
  if (!ParseGroupedBoxProperty(VARIANT_LN, value, CSS_PROPERTY_VALUE_NONNEGATIVE)) {
    return false;
  }

  AppendValue(eCSSProperty_border_image_outset, value);
  return true;
}

bool
CSSParserImpl::ParseBorderImageRepeat(bool aAcceptsInherit)
{
  nsCSSValue value;
  if (aAcceptsInherit &&
      ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keywords "inherit", "initial" and "unset" can not be mixed, so we
    // are done.
    AppendValue(eCSSProperty_border_image_repeat, value);
    return true;
  }

  nsCSSValuePair result;
  if (!ParseEnum(result.mXValue, nsCSSProps::kBorderImageRepeatKTable)) {
    return false;
  }

  // optional second keyword, defaults to first
  if (!ParseEnum(result.mYValue, nsCSSProps::kBorderImageRepeatKTable)) {
    result.mYValue = result.mXValue;
  }

  value.SetPairValue(&result);
  AppendValue(eCSSProperty_border_image_repeat, value);
  return true;
}

bool
CSSParserImpl::ParseBorderImage()
{
  nsAutoParseCompoundProperty compound(this);

  // border-image: inherit | initial |
  // <border-image-source> ||
  // <border-image-slice>
  //   [ / <border-image-width> |
  //     / <border-image-width>? / <border-image-outset> ]? ||
  // <border-image-repeat>

  nsCSSValue value;
  if (ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_border_image_source, value);
    AppendValue(eCSSProperty_border_image_slice, value);
    AppendValue(eCSSProperty_border_image_width, value);
    AppendValue(eCSSProperty_border_image_outset, value);
    AppendValue(eCSSProperty_border_image_repeat, value);
    // Keywords "inherit", "initial" and "unset" can't be mixed, so we are done.
    return true;
  }

  // No empty property.
  if (CheckEndProperty()) {
    return false;
  }

  // Shorthand properties are required to set everything they can.
  SetBorderImageInitialValues();

  bool foundSource = false;
  bool foundSliceWidthOutset = false;
  bool foundRepeat = false;

  // This loop is used to handle the parsing of border-image properties which
  // can appear in any order.
  nsCSSValue imageSourceValue;
  while (!CheckEndProperty()) {
    // <border-image-source>
    if (!foundSource) {
      CSSParseResult result =
        ParseVariant(imageSourceValue, VARIANT_IMAGE, nullptr);
      if (result == CSSParseResult::Error) {
        return false;
      } else if (result == CSSParseResult::Ok) {
        AppendValue(eCSSProperty_border_image_source, imageSourceValue);
        foundSource = true;
        continue;
      }
    }

    // <border-image-slice>
    // ParseBorderImageSlice is weird.  It may consume tokens and then return
    // false, because it parses a property with two required components that
    // can appear in either order.  Since the tokens that were consumed cannot
    // parse as anything else we care about, this isn't a problem.
    if (!foundSliceWidthOutset) {
      bool sliceConsumedTokens = false;
      if (ParseBorderImageSlice(false, &sliceConsumedTokens)) {
        foundSliceWidthOutset = true;

        // [ / <border-image-width>?
        if (ExpectSymbol('/', true)) {
          bool foundBorderImageWidth = ParseBorderImageWidth(false);

          // [ / <border-image-outset>
          if (ExpectSymbol('/', true)) {
            if (!ParseBorderImageOutset(false)) {
              return false;
            }
          } else if (!foundBorderImageWidth) {
            // If this part has an trailing slash, the whole declaration is
            // invalid.
            return false;
          }
        }

        continue;
      } else {
        // If we consumed some tokens for <border-image-slice> but did not
        // successfully parse it, we have an error.
        if (sliceConsumedTokens) {
          return false;
        }
      }
    }

    // <border-image-repeat>
    if (!foundRepeat && ParseBorderImageRepeat(false)) {
      foundRepeat = true;
      continue;
    }

    return false;
  }

  return true;
}

bool
CSSParserImpl::ParseBorderSpacing()
{
  nsCSSValue xValue, yValue;
  if (ParseNonNegativeVariant(xValue, VARIANT_HL | VARIANT_CALC, nullptr) !=
      CSSParseResult::Ok) {
    return false;
  }

  // If we have one length, get the optional second length.
  // set the second value equal to the first.
  if (xValue.IsLengthUnit() || xValue.IsCalcUnit()) {
    if (ParseNonNegativeVariant(yValue, VARIANT_LENGTH | VARIANT_CALC,
                                nullptr) == CSSParseResult::Error) {
      return false;
    }
  }

  if (yValue == xValue || yValue.GetUnit() == eCSSUnit_Null) {
    AppendValue(eCSSProperty_border_spacing, xValue);
  } else {
    nsCSSValue pair;
    pair.SetPairValue(xValue, yValue);
    AppendValue(eCSSProperty_border_spacing, pair);
  }
  return true;
}

bool
CSSParserImpl::ParseBorderSide(const nsCSSPropertyID aPropIDs[],
                               bool aSetAllSides)
{
  const int32_t numProps = 3;
  nsCSSValue  values[numProps];

  int32_t found = ParseChoice(values, aPropIDs, numProps);
  if (found < 1) {
    return false;
  }

  if ((found & 1) == 0) { // Provide default border-width
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default border-style
    values[1].SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) { // text color will be used
    values[2].SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }

  if (aSetAllSides) {
    // Parsing "border" shorthand; set all four sides to the same thing
    for (int32_t index = 0; index < 4; index++) {
      NS_ASSERTION(numProps == 3, "This code needs updating");
      AppendValue(kBorderWidthIDs[index], values[0]);
      AppendValue(kBorderStyleIDs[index], values[1]);
      AppendValue(kBorderColorIDs[index], values[2]);
    }

    static const nsCSSPropertyID kBorderColorsProps[] = {
      eCSSProperty__moz_border_top_colors,
      eCSSProperty__moz_border_right_colors,
      eCSSProperty__moz_border_bottom_colors,
      eCSSProperty__moz_border_left_colors
    };

    // Set the other properties that the border shorthand sets to their
    // initial values.
    nsCSSValue extraValue;
    switch (values[0].GetUnit()) {
    case eCSSUnit_Inherit:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      extraValue = values[0];
      // Set value of border-image properties to initial/inherit/unset
      AppendValue(eCSSProperty_border_image_source, extraValue);
      AppendValue(eCSSProperty_border_image_slice, extraValue);
      AppendValue(eCSSProperty_border_image_width, extraValue);
      AppendValue(eCSSProperty_border_image_outset, extraValue);
      AppendValue(eCSSProperty_border_image_repeat, extraValue);
      break;
    default:
      extraValue.SetNoneValue();
      SetBorderImageInitialValues();
      break;
    }
    NS_FOR_CSS_SIDES(side) {
      AppendValue(kBorderColorsProps[side], extraValue);
    }
  }
  else {
    // Just set our one side
    for (int32_t index = 0; index < numProps; index++) {
      AppendValue(aPropIDs[index], values[index]);
    }
  }
  return true;
}

bool
CSSParserImpl::ParseBorderStyle()
{
  return ParseBoxProperties(kBorderStyleIDs);
}

bool
CSSParserImpl::ParseBorderWidth()
{
  return ParseBoxProperties(kBorderWidthIDs);
}

bool
CSSParserImpl::ParseBorderColors(nsCSSPropertyID aProperty)
{
  nsCSSValue value;
  // 'inherit', 'initial', 'unset' and 'none' are only allowed on their own
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    nsCSSValueList *cur = value.SetListValue();
    for (;;) {
      if (ParseVariant(cur->mValue, VARIANT_COLOR, nullptr) !=
          CSSParseResult::Ok) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(aProperty, value);
  return true;
}

// Parse the top level of a calc() expression.
bool
CSSParserImpl::ParseCalc(nsCSSValue &aValue, uint32_t aVariantMask)
{
  // Parsing calc expressions requires, in a number of cases, looking
  // for a token that is *either* a value of the property or a number.
  // This can be done without lookahead when we assume that the property
  // values cannot themselves be numbers.
  MOZ_ASSERT(aVariantMask != 0, "unexpected variant mask");
  MOZ_ASSERT(!(aVariantMask & VARIANT_LPN) != !(aVariantMask & VARIANT_INTEGER),
             "variant mask must intersect with exactly one of VARIANT_LPN "
             "or VARIANT_INTEGER");

  bool oldUnitlessLengthQuirk = mUnitlessLengthQuirk;
  mUnitlessLengthQuirk = false;

  // One-iteration loop so we can break to the error-handling case.
  do {
    // The toplevel of a calc() is always an nsCSSValue::Array of length 1.
    RefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(1);

    if (!ParseCalcAdditiveExpression(arr->Item(0), aVariantMask))
      break;

    if (!ExpectSymbol(')', true))
      break;

    aValue.SetArrayValue(arr, eCSSUnit_Calc);
    mUnitlessLengthQuirk = oldUnitlessLengthQuirk;
    return true;
  } while (false);

  SkipUntil(')');
  mUnitlessLengthQuirk = oldUnitlessLengthQuirk;
  return false;
}

// We optimize away the <value-expression> production given that
// ParseVariant consumes initial whitespace and we call
// ExpectSymbol(')') with true for aSkipWS.
//  * If aVariantMask is VARIANT_NUMBER, this function parses the
//    <number-additive-expression> production.
//  * If aVariantMask does not contain VARIANT_NUMBER, this function
//    parses the <value-additive-expression> production.
//  * Otherwise (VARIANT_NUMBER and other bits) this function parses
//    whichever one of the productions matches ***and modifies
//    aVariantMask*** to reflect which one it has parsed by either
//    removing VARIANT_NUMBER or removing all other bits.
// It does so iteratively, but builds the correct recursive
// data structure.
bool
CSSParserImpl::ParseCalcAdditiveExpression(nsCSSValue& aValue,
                                           uint32_t& aVariantMask)
{
  MOZ_ASSERT(aVariantMask != 0, "unexpected variant mask");
  nsCSSValue *storage = &aValue;
  for (;;) {
    bool haveWS;
    if (!ParseCalcMultiplicativeExpression(*storage, aVariantMask, &haveWS))
      return false;

    if (!haveWS || !GetToken(false))
      return true;
    nsCSSUnit unit;
    if (mToken.IsSymbol('+')) {
      unit = eCSSUnit_Calc_Plus;
    } else if (mToken.IsSymbol('-')) {
      unit = eCSSUnit_Calc_Minus;
    } else {
      UngetToken();
      return true;
    }
    if (!RequireWhitespace())
      return false;

    RefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(2);
    arr->Item(0) = aValue;
    storage = &arr->Item(1);
    aValue.SetArrayValue(arr, unit);
  }
}

//  * If aVariantMask is VARIANT_NUMBER, this function parses the
//    <number-multiplicative-expression> production.
//  * If aVariantMask does not contain VARIANT_NUMBER, this function
//    parses the <value-multiplicative-expression> production.
//  * Otherwise (VARIANT_NUMBER and other bits) this function parses
//    whichever one of the productions matches ***and modifies
//    aVariantMask*** to reflect which one it has parsed by either
//    removing VARIANT_NUMBER or removing all other bits.
// It does so iteratively, but builds the correct recursive data
// structure.
// This function always consumes *trailing* whitespace when it returns
// true; whether there was any such whitespace is returned in the
// aHadFinalWS parameter.
bool
CSSParserImpl::ParseCalcMultiplicativeExpression(nsCSSValue& aValue,
                                                 uint32_t& aVariantMask,
                                                 bool *aHadFinalWS)
{
  MOZ_ASSERT(aVariantMask != 0, "unexpected variant mask");
  bool gotValue = false; // already got the part with the unit
  bool afterDivision = false;

  nsCSSValue *storage = &aValue;
  for (;;) {
    uint32_t variantMask;
    if (aVariantMask & VARIANT_INTEGER) {
      MOZ_ASSERT(aVariantMask == VARIANT_INTEGER,
                 "integers in calc expressions can't be mixed with anything "
                 "else.");
      variantMask = aVariantMask;
    } else if (afterDivision || gotValue) {
      // At this point in the calc expression, we expect a coefficient or a
      // divisor, which must be a number. (Not a length/%/etc.)
      variantMask = VARIANT_NUMBER;
    } else {
      // At this point in the calc expression, we'll accept a coefficient
      // (a number) or a value of whatever type |aVariantMask| specifies.
      variantMask = aVariantMask | VARIANT_NUMBER;
    }
    if (!ParseCalcTerm(*storage, variantMask))
      return false;
    MOZ_ASSERT(variantMask != 0,
               "ParseCalcTerm did not set variantMask appropriately");
    MOZ_ASSERT(!(variantMask & VARIANT_NUMBER) ||
               !(variantMask & ~int32_t(VARIANT_NUMBER)),
               "ParseCalcTerm did not set variantMask appropriately");

    if (variantMask & VARIANT_NUMBER) {
      // Simplify the value immediately so we can check for division by
      // zero.
      float number;
      mozilla::css::ReduceCalcOps<float, eCSSUnit_Number> ops;
      if (!mozilla::css::ComputeCalc(number, *storage, ops)) {
        MOZ_ASSERT_UNREACHABLE("unexpected unit");
      }
      if (number == 0.0 && afterDivision)
        return false;
      storage->SetFloatValue(number, eCSSUnit_Number);
    } else {
      gotValue = true;

      if (storage != &aValue) {
        // Simplify any numbers in the Times_L position (which are
        // not simplified by the check above).
        MOZ_ASSERT(storage == &aValue.GetArrayValue()->Item(1),
                   "unexpected relationship to current storage");
        nsCSSValue &leftValue = aValue.GetArrayValue()->Item(0);
        if (variantMask & VARIANT_INTEGER) {
          int integer;
          mozilla::css::ReduceCalcOps<int, eCSSUnit_Integer> ops;
          if (!mozilla::css::ComputeCalc(integer, leftValue, ops)) {
            MOZ_ASSERT_UNREACHABLE("unexpected unit");
          }
          leftValue.SetIntValue(integer, eCSSUnit_Integer);
        } else {
          float number;
          mozilla::css::ReduceCalcOps<float, eCSSUnit_Number> ops;
          if (!mozilla::css::ComputeCalc(number, leftValue, ops)) {
            MOZ_ASSERT_UNREACHABLE("unexpected unit");
          }
          leftValue.SetFloatValue(number, eCSSUnit_Number);
        }
      }
    }

    bool hadWS = RequireWhitespace();
    if (!GetToken(false)) {
      *aHadFinalWS = hadWS;
      break;
    }
    nsCSSUnit unit;
    if (mToken.IsSymbol('*')) {
      unit = gotValue ? eCSSUnit_Calc_Times_R : eCSSUnit_Calc_Times_L;
      afterDivision = false;
    } else if (mToken.IsSymbol('/')) {
      if (variantMask & VARIANT_INTEGER) {
        // Integers aren't mixed with anything else (see the assert at the top
        // of CSSParserImpl::ParseCalc).
        // We don't allow division at all in calc()s for expressions where an
        // integer is expected, because calc() division can't be resolved to
        // an integer, as implied by spec text about '/' here:
        // https://drafts.csswg.org/css-values-3/#calc-type-checking
        // We've consumed the '/' token, but it doesn't matter as we're in an
        // error-handling situation where we've already consumed a lot of
        // other tokens (e.g. the token before the '/'). ParseVariant will
        // indicate this with CSSParseResult::Error.
        return false;
      }
      unit = eCSSUnit_Calc_Divided;
      afterDivision = true;
    } else {
      UngetToken();
      *aHadFinalWS = hadWS;
      break;
    }

    RefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(2);
    arr->Item(0) = aValue;
    storage = &arr->Item(1);
    aValue.SetArrayValue(arr, unit);
  }

  // Adjust aVariantMask (see comments above function) to reflect which
  // option we took.
  if (aVariantMask & VARIANT_NUMBER) {
    if (gotValue) {
      aVariantMask &= ~int32_t(VARIANT_NUMBER);
    } else {
      aVariantMask = VARIANT_NUMBER;
    }
  } else {
    if (!gotValue) {
      // We had to find a value, but we didn't.
      return false;
    }
  }

  return true;
}

//  * If aVariantMask is VARIANT_NUMBER, this function parses the
//    <number-term> production.
//  * If aVariantMask does not contain VARIANT_NUMBER, this function
//    parses the <value-term> production.
//  * Otherwise (VARIANT_NUMBER and other bits) this function parses
//    whichever one of the productions matches ***and modifies
//    aVariantMask*** to reflect which one it has parsed by either
//    removing VARIANT_NUMBER or removing all other bits.
bool
CSSParserImpl::ParseCalcTerm(nsCSSValue& aValue, uint32_t& aVariantMask)
{
  MOZ_ASSERT(aVariantMask != 0, "unexpected variant mask");
  if (!GetToken(true))
    return false;
  // Either an additive expression in parentheses...
  if (mToken.IsSymbol('(') ||
      // Treat nested calc() as plain parenthesis.
      IsCSSTokenCalcFunction(mToken)) {
    if (!ParseCalcAdditiveExpression(aValue, aVariantMask) ||
        !ExpectSymbol(')', true)) {
      SkipUntil(')');
      return false;
    }
    return true;
  }
  // ... or just a value
  UngetToken();
  if (aVariantMask & VARIANT_INTEGER) {
    // Integers aren't mixed with anything else (see the assert at the
    // top of CSSParserImpl::ParseCalc).
    if (ParseVariant(aValue, aVariantMask, nullptr) != CSSParseResult::Ok) {
      return false;
    }
  } else {
    // Always pass VARIANT_NUMBER to ParseVariant so that unitless zero
    // always gets picked up (we want to catch unitless zeroes using
    // VARIANT_NUMBER and then error out)
    if (ParseVariant(aValue, aVariantMask | VARIANT_NUMBER, nullptr) !=
        CSSParseResult::Ok) {
      return false;
    }
    // ...and do the VARIANT_NUMBER check ourselves.
    if (!(aVariantMask & VARIANT_NUMBER) && aValue.GetUnit() == eCSSUnit_Number) {
      return false;
    }
  }
  // If we did the value parsing, we need to adjust aVariantMask to
  // reflect which option we took (see above).
  if (aVariantMask & VARIANT_NUMBER) {
    if (aValue.GetUnit() == eCSSUnit_Number) {
      aVariantMask = VARIANT_NUMBER;
    } else {
      aVariantMask &= ~int32_t(VARIANT_NUMBER);
    }
  }
  return true;
}

// This function consumes all consecutive whitespace and returns whether
// there was any.
bool
CSSParserImpl::RequireWhitespace()
{
  if (!GetToken(false))
    return false;
  if (mToken.mType != eCSSToken_Whitespace) {
    UngetToken();
    return false;
  }
  // Skip any additional whitespace tokens.
  if (GetToken(true)) {
    UngetToken();
  }
  return true;
}

bool
CSSParserImpl::ParseRect(nsCSSPropertyID aPropID)
{
  nsCSSValue val;
  if (ParseSingleTokenVariant(val, VARIANT_INHERIT | VARIANT_AUTO, nullptr)) {
    AppendValue(aPropID, val);
    return true;
  }

  if (! GetToken(true)) {
    return false;
  }

  if (mToken.mType == eCSSToken_Function &&
      mToken.mIdent.LowerCaseEqualsLiteral("rect")) {
    nsCSSRect& rect = val.SetRectValue();
    bool useCommas;
    NS_FOR_CSS_SIDES(side) {
      if (!ParseSingleTokenVariant(rect.*(nsCSSRect::sides[side]),
                                   VARIANT_AL, nullptr)) {
        return false;
      }
      if (side == 0) {
        useCommas = ExpectSymbol(',', true);
      } else if (useCommas && side < 3) {
        // Skip optional commas between elements, but only if the first
        // separator was a comma.
        if (!ExpectSymbol(',', true)) {
          return false;
        }
      }
    }
    if (!ExpectSymbol(')', true)) {
      return false;
    }
  } else {
    UngetToken();
    return false;
  }

  AppendValue(aPropID, val);
  return true;
}

bool
CSSParserImpl::ParseColumns()
{
  // We use a similar "fake value" hack to ParseListStyle, because
  // "auto" is acceptable for both column-count and column-width.
  // If the fake "auto" value is found, and one of the real values isn't,
  // that means the fake auto value is meant for the real value we didn't
  // find.
  static const nsCSSPropertyID columnIDs[] = {
    eCSSPropertyExtra_x_auto_value,
    eCSSProperty_column_count,
    eCSSProperty_column_width
  };
  const int32_t numProps = MOZ_ARRAY_LENGTH(columnIDs);

  nsCSSValue values[numProps];
  int32_t found = ParseChoice(values, columnIDs, numProps);
  if (found < 1) {
    return false;
  }
  if ((found & (1|2|4)) == (1|2|4) &&
      values[0].GetUnit() ==  eCSSUnit_Auto) {
    // We filled all 3 values, which is invalid
    return false;
  }

  if ((found & 2) == 0) {
    // Provide auto column-count
    values[1].SetAutoValue();
  }
  if ((found & 4) == 0) {
    // Provide auto column-width
    values[2].SetAutoValue();
  }

  // Start at index 1 to skip the fake auto value.
  for (int32_t index = 1; index < numProps; index++) {
    AppendValue(columnIDs[index], values[index]);
  }
  return true;
}

#define VARIANT_CONTENT (VARIANT_STRING | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR | \
                         VARIANT_KEYWORD)
bool
CSSParserImpl::ParseContent()
{
  // We need to divide the 'content' keywords into two classes for
  // ParseVariant's sake, so we can't just use nsCSSProps::kContentKTable.
  static const KTableEntry kContentListKWs[] = {
    { eCSSKeyword_open_quote, uint8_t(StyleContent::OpenQuote) },
    { eCSSKeyword_close_quote, uint8_t(StyleContent::CloseQuote) },
    { eCSSKeyword_no_open_quote, uint8_t(StyleContent::NoOpenQuote) },
    { eCSSKeyword_no_close_quote, uint8_t(StyleContent::NoCloseQuote) },
    { eCSSKeyword_UNKNOWN, -1 }
  };

  static const KTableEntry kContentSolitaryKWs[] = {
    { eCSSKeyword__moz_alt_content, uint8_t(StyleContent::AltContent) },
    { eCSSKeyword_UNKNOWN, -1 }
  };

  // Verify that these two lists add up to the size of
  // nsCSSProps::kContentKTable.
  MOZ_ASSERT(nsCSSProps::kContentKTable[
               ArrayLength(kContentListKWs) +
               ArrayLength(kContentSolitaryKWs) - 2].mKeyword ==
                 eCSSKeyword_UNKNOWN &&
             nsCSSProps::kContentKTable[
               ArrayLength(kContentListKWs) +
               ArrayLength(kContentSolitaryKWs) - 2].mValue == -1,
             "content keyword tables out of sync");

  nsCSSValue value;
  // 'inherit', 'initial', 'unset', 'normal', 'none', and 'alt-content' must
  // be alone
  if (!ParseSingleTokenVariant(value, VARIANT_HMK | VARIANT_NONE,
                               kContentSolitaryKWs)) {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (ParseVariant(cur->mValue, VARIANT_CONTENT, kContentListKWs) !=
          CSSParseResult::Ok) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_content, value);
  return true;
}

bool
CSSParserImpl::ParseCounterData(nsCSSPropertyID aPropID)
{
  static const nsCSSKeyword kCounterDataKTable[] = {
    eCSSKeyword_none,
    eCSSKeyword_UNKNOWN
  };
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    if (!GetToken(true)) {
      return false;
    }
    if (mToken.mType != eCSSToken_Ident) {
      UngetToken();
      return false;
    }

    nsCSSValuePairList *cur = value.SetPairListValue();
    for (;;) {
      if (!ParseCustomIdent(cur->mXValue, mToken.mIdent, kCounterDataKTable)) {
        return false;
      }
      int32_t value = aPropID == eCSSProperty_counter_increment ? 1 : 0;
      if (GetToken(true)) {
        if (mToken.mType == eCSSToken_Number && mToken.mIntegerValid) {
          value = mToken.mInteger;
        } else {
          UngetToken();
        }
      }
      cur->mYValue.SetIntValue(value, eCSSUnit_Integer);
      if (!GetToken(true)) {
        break;
      }
      if (mToken.mType != eCSSToken_Ident) {
        UngetToken();
        break;
      }
      cur->mNext = new nsCSSValuePairList;
      cur = cur->mNext;
    }
  }
  AppendValue(aPropID, value);
  return true;
}

bool
CSSParserImpl::ParseCursor()
{
  nsCSSValue value;
  // 'inherit', 'initial' and 'unset' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseSingleTokenVariant(cur->mValue, VARIANT_UK,
                                   nsCSSProps::kCursorKTable)) {
        return false;
      }
      if (cur->mValue.GetUnit() != eCSSUnit_URL) { // keyword must be last
        break;
      }

      // We have a URL, so make a value array with three values.
      RefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(3);
      val->Item(0) = cur->mValue;

      // Parse optional x and y position of cursor hotspot (css3-ui).
      if (ParseSingleTokenVariant(val->Item(1), VARIANT_NUMBER, nullptr)) {
        // If we have one number, we must have two.
        if (!ParseSingleTokenVariant(val->Item(2), VARIANT_NUMBER, nullptr)) {
          return false;
        }
      }
      cur->mValue.SetArrayValue(val, eCSSUnit_Array);

      if (!ExpectSymbol(',', true)) { // url must not be last
        return false;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_cursor, value);
  return true;
}


bool
CSSParserImpl::ParseFont()
{
  nsCSSValue  family;
  if (ParseSingleTokenVariant(family, VARIANT_HK, nsCSSProps::kFontKTable)) {
    if (eCSSUnit_Inherit == family.GetUnit() ||
        eCSSUnit_Initial == family.GetUnit() ||
        eCSSUnit_Unset == family.GetUnit()) {
      AppendValue(eCSSProperty__x_system_font, nsCSSValue(eCSSUnit_None));
      AppendValue(eCSSProperty_font_family, family);
      AppendValue(eCSSProperty_font_style, family);
      AppendValue(eCSSProperty_font_weight, family);
      AppendValue(eCSSProperty_font_size, family);
      AppendValue(eCSSProperty_line_height, family);
      AppendValue(eCSSProperty_font_stretch, family);
      AppendValue(eCSSProperty_font_size_adjust, family);
      AppendValue(eCSSProperty_font_feature_settings, family);
      AppendValue(eCSSProperty_font_language_override, family);
      AppendValue(eCSSProperty_font_kerning, family);
      AppendValue(eCSSProperty_font_variant_alternates, family);
      AppendValue(eCSSProperty_font_variant_caps, family);
      AppendValue(eCSSProperty_font_variant_east_asian, family);
      AppendValue(eCSSProperty_font_variant_ligatures, family);
      AppendValue(eCSSProperty_font_variant_numeric, family);
      AppendValue(eCSSProperty_font_variant_position, family);
    }
    else {
      AppendValue(eCSSProperty__x_system_font, family);
      nsCSSValue systemFont(eCSSUnit_System_Font);
      AppendValue(eCSSProperty_font_family, systemFont);
      AppendValue(eCSSProperty_font_style, systemFont);
      AppendValue(eCSSProperty_font_weight, systemFont);
      AppendValue(eCSSProperty_font_size, systemFont);
      AppendValue(eCSSProperty_line_height, systemFont);
      AppendValue(eCSSProperty_font_stretch, systemFont);
      AppendValue(eCSSProperty_font_size_adjust, systemFont);
      AppendValue(eCSSProperty_font_feature_settings, systemFont);
      AppendValue(eCSSProperty_font_language_override, systemFont);
      AppendValue(eCSSProperty_font_kerning, systemFont);
      AppendValue(eCSSProperty_font_variant_alternates, systemFont);
      AppendValue(eCSSProperty_font_variant_caps, systemFont);
      AppendValue(eCSSProperty_font_variant_east_asian, systemFont);
      AppendValue(eCSSProperty_font_variant_ligatures, systemFont);
      AppendValue(eCSSProperty_font_variant_numeric, systemFont);
      AppendValue(eCSSProperty_font_variant_position, systemFont);
    }
    return true;
  }

  // Get optional font-style, font-variant, font-weight, font-stretch
  // (in any order)

  // Indexes into fontIDs[] and values[] arrays.
  const int kFontStyleIndex = 0;
  const int kFontVariantIndex = 1;
  const int kFontWeightIndex = 2;
  const int kFontStretchIndex = 3;

  // The order of the initializers here must match the order of the indexes
  // defined above!
  static const nsCSSPropertyID fontIDs[] = {
    eCSSProperty_font_style,
    eCSSProperty_font_variant_caps,
    eCSSProperty_font_weight,
    eCSSProperty_font_stretch
  };

  const int32_t numProps = MOZ_ARRAY_LENGTH(fontIDs);
  nsCSSValue  values[numProps];
  int32_t found = ParseChoice(values, fontIDs, numProps);
  if (found < 0 ||
      eCSSUnit_Inherit == values[kFontStyleIndex].GetUnit() ||
      eCSSUnit_Initial == values[kFontStyleIndex].GetUnit() ||
      eCSSUnit_Unset == values[kFontStyleIndex].GetUnit()) { // illegal data
    return false;
  }
  if ((found & (1 << kFontStyleIndex)) == 0) {
    // Provide default font-style
    values[kFontStyleIndex].SetIntValue(NS_FONT_STYLE_NORMAL,
                                        eCSSUnit_Enumerated);
  }
  if ((found & (1 << kFontVariantIndex)) == 0) {
    // Provide default font-variant
    values[kFontVariantIndex].SetNormalValue();
  } else {
    if (values[kFontVariantIndex].GetUnit() == eCSSUnit_Enumerated &&
        values[kFontVariantIndex].GetIntValue() !=
        NS_FONT_VARIANT_CAPS_SMALLCAPS) {
      // only normal or small-caps is allowed in font shorthand
      // this also assumes other values for font-variant-caps never overlap
      // possible values for style or weight
      return false;
    }
  }
  if ((found & (1 << kFontWeightIndex)) == 0) {
    // Provide default font-weight
    values[kFontWeightIndex].SetIntValue(NS_FONT_WEIGHT_NORMAL,
                                         eCSSUnit_Enumerated);
  }
  if ((found & (1 << kFontStretchIndex)) == 0) {
    // Provide default font-stretch
    values[kFontStretchIndex].SetIntValue(NS_FONT_STRETCH_NORMAL,
                                          eCSSUnit_Enumerated);
  }

  // Get mandatory font-size
  nsCSSValue  size;
  if (!ParseSingleTokenNonNegativeVariant(size, VARIANT_KEYWORD | VARIANT_LP,
                                          nsCSSProps::kFontSizeKTable)) {
    return false;
  }

  // Get optional "/" line-height
  nsCSSValue  lineHeight;
  if (ExpectSymbol('/', true)) {
    if (ParseNonNegativeVariant(lineHeight,
                                VARIANT_NUMBER | VARIANT_LP |
                                  VARIANT_NORMAL | VARIANT_CALC,
                                nullptr) != CSSParseResult::Ok) {
      return false;
    }
  }
  else {
    lineHeight.SetNormalValue();
  }

  // Get final mandatory font-family
  nsAutoParseCompoundProperty compound(this);
  if (ParseFamily(family)) {
    if (eCSSUnit_Inherit != family.GetUnit() &&
        eCSSUnit_Initial != family.GetUnit() &&
        eCSSUnit_Unset != family.GetUnit()) {
      AppendValue(eCSSProperty__x_system_font, nsCSSValue(eCSSUnit_None));
      AppendValue(eCSSProperty_font_family, family);
      AppendValue(eCSSProperty_font_style, values[kFontStyleIndex]);
      AppendValue(eCSSProperty_font_variant_caps, values[kFontVariantIndex]);
      AppendValue(eCSSProperty_font_weight, values[kFontWeightIndex]);
      AppendValue(eCSSProperty_font_size, size);
      AppendValue(eCSSProperty_line_height, lineHeight);
      AppendValue(eCSSProperty_font_stretch, values[kFontStretchIndex]);
      AppendValue(eCSSProperty_font_size_adjust, nsCSSValue(eCSSUnit_None));
      AppendValue(eCSSProperty_font_feature_settings, nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_language_override, nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_kerning,
                  nsCSSValue(NS_FONT_KERNING_AUTO, eCSSUnit_Enumerated));
      AppendValue(eCSSProperty_font_variant_alternates,
                  nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_variant_east_asian,
                  nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_variant_ligatures,
                  nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_variant_numeric,
                  nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_variant_position,
                  nsCSSValue(eCSSUnit_Normal));
      return true;
    }
  }
  return false;
}

bool
CSSParserImpl::ParseFontSynthesis(nsCSSValue& aValue)
{
  if (!ParseSingleTokenVariant(aValue, VARIANT_HK | VARIANT_NONE,
                               nsCSSProps::kFontSynthesisKTable)) {
    return false;
  }

  // first value 'none' ==> done
  if (eCSSUnit_None == aValue.GetUnit() ||
      eCSSUnit_Initial == aValue.GetUnit() ||
      eCSSUnit_Inherit == aValue.GetUnit() ||
      eCSSUnit_Unset == aValue.GetUnit())
  {
    return true;
  }

  // look for a second value
  int32_t intValue = aValue.GetIntValue();
  nsCSSValue nextValue;

  if (ParseEnum(nextValue, nsCSSProps::kFontSynthesisKTable)) {
    int32_t nextIntValue = nextValue.GetIntValue();
    if (nextIntValue & intValue) {
      return false;
    }
    aValue.SetIntValue(nextIntValue | intValue, eCSSUnit_Enumerated);
  }

  return true;
}

// font-variant-alternates allows for a combination of multiple
// simple enumerated values and functional values.  Functional values have
// parameter lists with one or more idents which are later resolved
// based on values defined in @font-feature-value rules.
//
// font-variant-alternates: swash(flowing) historical-forms styleset(alt-g, alt-m);
//
// So for this the nsCSSValue is set to a pair value, with one
// value for a bitmask of both simple and functional property values
// and another value containing a ValuePairList with lists of idents
// for each functional property value.
//
// pairValue
//   o intValue
//       NS_FONT_VARIANT_ALTERNATES_SWASH |
//       NS_FONT_VARIANT_ALTERNATES_STYLESET
//   o valuePairList, each element with
//     - intValue - indicates which alternate
//     - string or valueList of strings
//
// Note: when only 'historical-forms' is specified, there are no
// functional values to store, in which case the valuePairList is a
// single element dummy list.  In all other cases, the length of the
// list will match the number of functional values.

#define MAX_ALLOWED_FEATURES 512

static uint16_t
MaxElementsForAlternateType(nsCSSKeyword keyword)
{
  uint16_t maxElems = 1;
  if (keyword == eCSSKeyword_styleset ||
      keyword == eCSSKeyword_character_variant) {
    maxElems = MAX_ALLOWED_FEATURES;
  }
  return maxElems;
}

bool
CSSParserImpl::ParseSingleAlternate(int32_t& aWhichFeature,
                                    nsCSSValue& aValue)
{
  if (!GetToken(true)) {
    return false;
  }

  bool isIdent = (mToken.mType == eCSSToken_Ident);
  if (mToken.mType != eCSSToken_Function && !isIdent) {
    UngetToken();
    return false;
  }

  // ident ==> simple enumerated prop val (e.g. historical-forms)
  // function ==> e.g. swash(flowing) styleset(alt-g, alt-m)

  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
  if (!nsCSSProps::FindKeyword(keyword,
                               (isIdent ?
                                nsCSSProps::kFontVariantAlternatesKTable :
                                nsCSSProps::kFontVariantAlternatesFuncsKTable),
                               aWhichFeature))
  {
    // failed, pop token
    UngetToken();
    return false;
  }

  if (isIdent) {
    aValue.SetIntValue(aWhichFeature, eCSSUnit_Enumerated);
    return true;
  }

  return ParseFunction(keyword, nullptr, VARIANT_IDENTIFIER,
                       1, MaxElementsForAlternateType(keyword), aValue);
}

bool
CSSParserImpl::ParseFontVariantAlternates(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL,
                              nullptr)) {
    return true;
  }

  // iterate through parameters
  nsCSSValue listValue;
  int32_t feature, featureFlags = 0;

  // if no functional values, this may be a list with a single, unused element
  listValue.SetListValue();

  nsCSSValueList* list = nullptr;
  nsCSSValue value;
  while (ParseSingleAlternate(feature, value)) {

    // check to make sure value not already set
    if (feature == 0 ||
        feature & featureFlags) {
      return false;
    }

    featureFlags |= feature;

    // if function, need to add to the list of functions
    if (value.GetUnit() == eCSSUnit_Function) {
      if (!list) {
        list = listValue.GetListValue();
      } else {
        list->mNext = new nsCSSValueList;
        list = list->mNext;
      }
      list->mValue = value;
    }
  }

  if (featureFlags == 0) {
    // ParseSingleAlternate failed the first time through the loop.
    return false;
  }

  nsCSSValue featureValue;
  featureValue.SetIntValue(featureFlags, eCSSUnit_Enumerated);
  aValue.SetPairValue(featureValue, listValue);

  return true;
}

bool
CSSParserImpl::MergeBitmaskValue(int32_t aNewValue,
                                 const int32_t aMasks[],
                                 int32_t& aMergedValue)
{
  // check to make sure value not already set
  if (aNewValue & aMergedValue) {
    return false;
  }

  const int32_t *m = aMasks;
  int32_t c = 0;

  while (*m != MASK_END_VALUE) {
    if (*m & aNewValue) {
      c = aMergedValue & *m;
      break;
    }
    m++;
  }

  if (c) {
    return false;
  }

  aMergedValue |= aNewValue;
  return true;
}

// aMasks - array of masks for mutually-exclusive property values,
//          e.g. proportial-nums, tabular-nums

bool
CSSParserImpl::ParseBitmaskValues(nsCSSValue& aValue,
                                  const KTableEntry aKeywordTable[],
                                  const int32_t aMasks[])
{
  // Parse at least one keyword
  if (!ParseEnum(aValue, aKeywordTable)) {
    return false;
  }

  // look for more values
  nsCSSValue nextValue;
  int32_t mergedValue = aValue.GetIntValue();

  while (ParseEnum(nextValue, aKeywordTable))
  {
    if (!MergeBitmaskValue(nextValue.GetIntValue(), aMasks, mergedValue)) {
      return false;
    }
  }

  aValue.SetIntValue(mergedValue, eCSSUnit_Enumerated);

  return true;
}

static const int32_t maskEastAsian[] = {
  NS_FONT_VARIANT_EAST_ASIAN_VARIANT_MASK,
  NS_FONT_VARIANT_EAST_ASIAN_WIDTH_MASK,
  MASK_END_VALUE
};

bool
CSSParserImpl::ParseFontVariantEastAsian(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL,
                              nullptr)) {
    return true;
  }

  NS_ASSERTION(maskEastAsian[ArrayLength(maskEastAsian) - 1] ==
                 MASK_END_VALUE,
               "incorrectly terminated array");

  return ParseBitmaskValues(aValue, nsCSSProps::kFontVariantEastAsianKTable,
                            maskEastAsian);
}

bool
CSSParserImpl::ParseContain(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NONE,
                              nullptr)) {
    return true;
  }
  static const int32_t maskContain[] = { MASK_END_VALUE };
  if (!ParseBitmaskValues(aValue, nsCSSProps::kContainKTable, maskContain)) {
    return false;
  }
  if (aValue.GetIntValue() & NS_STYLE_CONTAIN_STRICT) {
    if (aValue.GetIntValue() != NS_STYLE_CONTAIN_STRICT) {
      // Disallow any other keywords in combination with 'strict'.
      return false;
    }
    // Strict implies layout, style, and paint.
    // However, for serialization purposes, we keep the strict bit around.
    aValue.SetIntValue(NS_STYLE_CONTAIN_STRICT |
        NS_STYLE_CONTAIN_ALL_BITS, eCSSUnit_Enumerated);
  }
  return true;
}

static const int32_t maskLigatures[] = {
  NS_FONT_VARIANT_LIGATURES_COMMON_MASK,
  NS_FONT_VARIANT_LIGATURES_DISCRETIONARY_MASK,
  NS_FONT_VARIANT_LIGATURES_HISTORICAL_MASK,
  NS_FONT_VARIANT_LIGATURES_CONTEXTUAL_MASK,
  MASK_END_VALUE
};

bool
CSSParserImpl::ParseFontVariantLigatures(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue,
                              VARIANT_INHERIT | VARIANT_NORMAL | VARIANT_NONE,
                              nullptr)) {
    return true;
  }

  NS_ASSERTION(maskLigatures[ArrayLength(maskLigatures) - 1] ==
                 MASK_END_VALUE,
               "incorrectly terminated array");

  return ParseBitmaskValues(aValue, nsCSSProps::kFontVariantLigaturesKTable,
                            maskLigatures);
}

static const int32_t maskNumeric[] = {
  NS_FONT_VARIANT_NUMERIC_FIGURE_MASK,
  NS_FONT_VARIANT_NUMERIC_SPACING_MASK,
  NS_FONT_VARIANT_NUMERIC_FRACTION_MASK,
  MASK_END_VALUE
};

bool
CSSParserImpl::ParseFontVariantNumeric(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL,
                              nullptr)) {
    return true;
  }

  NS_ASSERTION(maskNumeric[ArrayLength(maskNumeric) - 1] ==
                 MASK_END_VALUE,
               "incorrectly terminated array");

  return ParseBitmaskValues(aValue, nsCSSProps::kFontVariantNumericKTable,
                            maskNumeric);
}

bool
CSSParserImpl::ParseFontVariant()
{
  // parse single values - normal/inherit/none
  nsCSSValue value;
  nsCSSValue normal(eCSSUnit_Normal);

  if (ParseSingleTokenVariant(value,
                              VARIANT_INHERIT | VARIANT_NORMAL | VARIANT_NONE,
                              nullptr)) {
    AppendValue(eCSSProperty_font_variant_ligatures, value);
    if (eCSSUnit_None == value.GetUnit()) {
      // 'none' applies the value 'normal' to all properties other
      // than 'font-variant-ligatures'
      value.SetNormalValue();
    }
    AppendValue(eCSSProperty_font_variant_alternates, value);
    AppendValue(eCSSProperty_font_variant_caps, value);
    AppendValue(eCSSProperty_font_variant_east_asian, value);
    AppendValue(eCSSProperty_font_variant_numeric, value);
    AppendValue(eCSSProperty_font_variant_position, value);
    return true;
  }

  // set each of the individual subproperties
  int32_t altFeatures = 0, capsFeatures = 0, eastAsianFeatures = 0,
          ligFeatures = 0, numericFeatures = 0, posFeatures = 0;
  nsCSSValue altListValue;
  nsCSSValueList* altList = nullptr;

  // if no functional values, this may be a list with a single, unused element
  altListValue.SetListValue();

  bool foundValid = false; // found at least one proper value
  while (GetToken(true)) {
    // only an ident or a function at this point
    bool isFunction = (mToken.mType == eCSSToken_Function);
    if (mToken.mType != eCSSToken_Ident && !isFunction) {
      UngetToken();
      break;
    }

    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    if (keyword == eCSSKeyword_UNKNOWN) {
      UngetToken();
      return false;
    }

    int32_t feature;

    // function? ==> font-variant-alternates
    if (isFunction) {
      if (!nsCSSProps::FindKeyword(keyword,
                                   nsCSSProps::kFontVariantAlternatesFuncsKTable,
                                   feature) ||
          (feature & altFeatures)) {
        UngetToken();
        return false;
      }

      altFeatures |= feature;
      nsCSSValue funcValue;
      if (!ParseFunction(keyword, nullptr, VARIANT_IDENTIFIER, 1,
                         MaxElementsForAlternateType(keyword), funcValue) ||
          funcValue.GetUnit() != eCSSUnit_Function) {
        UngetToken();
        return false;
      }

      if (!altList) {
        altList = altListValue.GetListValue();
      } else {
        altList->mNext = new nsCSSValueList;
        altList = altList->mNext;
      }
      altList->mValue = funcValue;
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantCapsKTable,
                                       feature)) {
      if (capsFeatures != 0) {
        // multiple values for font-variant-caps
        UngetToken();
        return false;
      }
      capsFeatures = feature;
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantAlternatesKTable,
                                       feature)) {
      if (feature & altFeatures) {
        // same value repeated
        UngetToken();
        return false;
      }
      altFeatures |= feature;
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantEastAsianKTable,
                                       feature)) {
      if (!MergeBitmaskValue(feature, maskEastAsian, eastAsianFeatures)) {
        // multiple mutually exclusive values
        UngetToken();
        return false;
      }
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantLigaturesKTable,
                                       feature)) {
      if (keyword == eCSSKeyword_none ||
          !MergeBitmaskValue(feature, maskLigatures, ligFeatures)) {
        // none or multiple mutually exclusive values
        UngetToken();
        return false;
      }
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantNumericKTable,
                                       feature)) {
      if (!MergeBitmaskValue(feature, maskNumeric, numericFeatures)) {
        // multiple mutually exclusive values
        UngetToken();
        return false;
      }
    } else if (nsCSSProps::FindKeyword(keyword,
                                       nsCSSProps::kFontVariantPositionKTable,
                                       feature)) {
      if (posFeatures != 0) {
        // multiple values for font-variant-caps
        UngetToken();
        return false;
      }
      posFeatures = feature;
    } else {
      // bogus keyword, bail...
      UngetToken();
      return false;
    }

    foundValid = true;
  }

  if (!foundValid) {
    return false;
  }

  if (altFeatures) {
    nsCSSValue featureValue;
    featureValue.SetIntValue(altFeatures, eCSSUnit_Enumerated);
    value.SetPairValue(featureValue, altListValue);
    AppendValue(eCSSProperty_font_variant_alternates, value);
  } else {
    AppendValue(eCSSProperty_font_variant_alternates, normal);
  }

  if (capsFeatures) {
    value.SetIntValue(capsFeatures, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_font_variant_caps, value);
  } else {
    AppendValue(eCSSProperty_font_variant_caps, normal);
  }

  if (eastAsianFeatures) {
    value.SetIntValue(eastAsianFeatures, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_font_variant_east_asian, value);
  } else {
    AppendValue(eCSSProperty_font_variant_east_asian, normal);
  }

  if (ligFeatures) {
    value.SetIntValue(ligFeatures, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_font_variant_ligatures, value);
  } else {
    AppendValue(eCSSProperty_font_variant_ligatures, normal);
  }

  if (numericFeatures) {
    value.SetIntValue(numericFeatures, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_font_variant_numeric, value);
  } else {
    AppendValue(eCSSProperty_font_variant_numeric, normal);
  }

  if (posFeatures) {
    value.SetIntValue(posFeatures, eCSSUnit_Enumerated);
    AppendValue(eCSSProperty_font_variant_position, value);
  } else {
    AppendValue(eCSSProperty_font_variant_position, normal);
  }

  return true;
}

bool
CSSParserImpl::ParseFontWeight(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_HKI | VARIANT_SYSFONT,
                              nsCSSProps::kFontWeightKTable)) {
    if (eCSSUnit_Integer == aValue.GetUnit()) { // ensure unit value
      int32_t intValue = aValue.GetIntValue();
      if ((100 <= intValue) &&
          (intValue <= 900) &&
          (0 == (intValue % 100))) {
        return true;
      } else {
        UngetToken();
        return false;
      }
    }
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParseOneFamily(nsAString& aFamily,
                              bool& aOneKeyword,
                              bool& aQuoted)
{
  if (!GetToken(true))
    return false;

  nsCSSToken* tk = &mToken;

  aOneKeyword = false;
  aQuoted = false;
  if (eCSSToken_Ident == tk->mType) {
    aOneKeyword = true;
    aFamily.Append(tk->mIdent);
    for (;;) {
      if (!GetToken(false))
        break;

      if (eCSSToken_Ident == tk->mType) {
        aOneKeyword = false;
        // We had at least another keyword before.
        // "If a sequence of identifiers is given as a font family name,
        //  the computed value is the name converted to a string by joining
        //  all the identifiers in the sequence by single spaces."
        // -- CSS 2.1, section 15.3
        // Whitespace tokens do not actually matter,
        // identifier tokens can be separated by comments.
        aFamily.Append(char16_t(' '));
        aFamily.Append(tk->mIdent);
      } else if (eCSSToken_Whitespace != tk->mType) {
        UngetToken();
        break;
      }
    }
    return true;

  } else if (eCSSToken_String == tk->mType) {
    aQuoted = true;
    aFamily.Append(tk->mIdent); // XXX What if it had escaped quotes?
    return true;

  } else {
    UngetToken();
    return false;
  }
}


static bool
AppendGeneric(nsCSSKeyword aKeyword, nsTArray<FontFamilyName>& aFamilyList)
{
  switch (aKeyword) {
    case eCSSKeyword_serif:
      aFamilyList.AppendElement(FontFamilyName(eFamily_serif));
      return true;
    case eCSSKeyword_sans_serif:
      aFamilyList.AppendElement(FontFamilyName(eFamily_sans_serif));
      return true;
    case eCSSKeyword_monospace:
      aFamilyList.AppendElement(FontFamilyName(eFamily_monospace));
      return true;
    case eCSSKeyword_cursive:
      aFamilyList.AppendElement(FontFamilyName(eFamily_cursive));
      return true;
    case eCSSKeyword_fantasy:
      aFamilyList.AppendElement(FontFamilyName(eFamily_fantasy));
      return true;
    case eCSSKeyword__moz_fixed:
      aFamilyList.AppendElement(FontFamilyName(eFamily_moz_fixed));
      return true;
    default:
      break;
  }

  return false;
}

bool
CSSParserImpl::ParseFamily(nsCSSValue& aValue)
{
  nsTArray<FontFamilyName> families;
  nsAutoString family;
  bool single, quoted;

  // keywords only have meaning in the first position
  if (!ParseOneFamily(family, single, quoted))
    return false;

  // check for keywords, but only when keywords appear by themselves
  // i.e. not in compounds such as font-family: default blah;
  bool foundGeneric = false;
  if (single) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(family);
    switch (keyword) {
      case eCSSKeyword_inherit:
        aValue.SetInheritValue();
        return true;
      case eCSSKeyword_default:
        // 605231 - don't parse unquoted 'default' reserved keyword
        return false;
      case eCSSKeyword_initial:
        aValue.SetInitialValue();
        return true;
      case eCSSKeyword_unset:
        if (nsLayoutUtils::UnsetValueEnabled()) {
          aValue.SetUnsetValue();
          return true;
        }
        break;
      case eCSSKeyword__moz_use_system_font:
        if (!IsParsingCompoundProperty()) {
          aValue.SetSystemFontValue();
          return true;
        }
        break;
      default:
        foundGeneric = AppendGeneric(keyword, families);
    }
  }

  if (!foundGeneric) {
    families.AppendElement(
      FontFamilyName(family, (quoted ? eQuotedName : eUnquotedName)));
  }

  for (;;) {
    if (!ExpectSymbol(',', true))
      break;

    nsAutoString nextFamily;
    if (!ParseOneFamily(nextFamily, single, quoted))
      return false;

    // at this point unquoted keywords are not allowed
    // as font family names but can appear within names
    foundGeneric = false;
    if (single) {
      nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(nextFamily);
      switch (keyword) {
        case eCSSKeyword_inherit:
        case eCSSKeyword_initial:
        case eCSSKeyword_default:
        case eCSSKeyword__moz_use_system_font:
          return false;
        case eCSSKeyword_unset:
          if (nsLayoutUtils::UnsetValueEnabled()) {
            return false;
          }
          break;
        default:
          foundGeneric = AppendGeneric(keyword, families);
          break;
      }
    }

    if (!foundGeneric) {
      families.AppendElement(
        FontFamilyName(nextFamily, (quoted ? eQuotedName : eUnquotedName)));
    }
  }

  if (families.IsEmpty()) {
    return false;
  }

  RefPtr<SharedFontList> familyList =
    new SharedFontList(Move(families));
  aValue.SetFontFamilyListValue(familyList.forget());
  return true;
}

// src: ( uri-src | local-src ) (',' ( uri-src | local-src ) )*
// uri-src: uri [ 'format(' string ( ',' string )* ')' ]
// local-src: 'local(' ( string | ident ) ')'

bool
CSSParserImpl::ParseFontSrc(nsCSSValue& aValue)
{
  // could we maybe turn nsCSSValue::Array into InfallibleTArray<nsCSSValue>?
  InfallibleTArray<nsCSSValue> values;
  nsCSSValue cur;
  for (;;) {
    if (!GetToken(true))
      break;

    if (mToken.mType == eCSSToken_URL) {
      SetValueToURL(cur, mToken.mIdent);
      values.AppendElement(cur);
      if (!ParseFontSrcFormat(values))
        return false;

    } else if (mToken.mType == eCSSToken_Function &&
               mToken.mIdent.LowerCaseEqualsLiteral("local")) {
      // css3-fonts does not specify a formal grammar for local().
      // The text permits both unquoted identifiers and quoted
      // strings.  We resolve this ambiguity in the spec by
      // assuming that the appropriate production is a single
      // <family-name>, possibly surrounded by whitespace.

      nsAutoString family;
      bool single, quoted;
      if (!ParseOneFamily(family, single, quoted)) {
        SkipUntil(')');
        return false;
      }
      if (!ExpectSymbol(')', true)) {
        SkipUntil(')');
        return false;
      }

      // reject generics
      if (single) {
        nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(family);
        switch (keyword) {
          case eCSSKeyword_serif:
          case eCSSKeyword_sans_serif:
          case eCSSKeyword_monospace:
          case eCSSKeyword_cursive:
          case eCSSKeyword_fantasy:
          case eCSSKeyword__moz_fixed:
            return false;
          default:
            break;
        }
      }

      cur.SetStringValue(family, eCSSUnit_Local_Font);
      values.AppendElement(cur);
    } else {
      // We don't know what to do with this token; unget it and error out
      UngetToken();
      return false;
    }

    if (!ExpectSymbol(',', true))
      break;
  }

  if (values.Length() == 0)
    return false;

  RefPtr<nsCSSValue::Array> srcVals
    = nsCSSValue::Array::Create(values.Length());

  uint32_t i;
  for (i = 0; i < values.Length(); i++)
    srcVals->Item(i) = values[i];
  aValue.SetArrayValue(srcVals, eCSSUnit_Array);
  return true;
}

bool
CSSParserImpl::ParseFontSrcFormat(InfallibleTArray<nsCSSValue> & values)
{
  if (!GetToken(true))
    return true; // EOF harmless here
  if (mToken.mType != eCSSToken_Function ||
      !mToken.mIdent.LowerCaseEqualsLiteral("format")) {
    UngetToken();
    return true;
  }

  do {
    if (!GetToken(true))
      return false; // EOF - no need for SkipUntil

    if (mToken.mType != eCSSToken_String) {
      UngetToken();
      SkipUntil(')');
      return false;
    }

    nsCSSValue cur(mToken.mIdent, eCSSUnit_Font_Format);
    values.AppendElement(cur);
  } while (ExpectSymbol(',', true));

  if (!ExpectSymbol(')', true)) {
    SkipUntil(')');
    return false;
  }

  return true;
}

// font-ranges: urange ( ',' urange )*
bool
CSSParserImpl::ParseFontRanges(nsCSSValue& aValue)
{
  InfallibleTArray<uint32_t> ranges;
  for (;;) {
    if (!GetToken(true))
      break;

    if (mToken.mType != eCSSToken_URange) {
      UngetToken();
      break;
    }

    // An invalid range token is a parsing error, causing the entire
    // descriptor to be ignored.
    if (!mToken.mIntegerValid)
      return false;

    uint32_t low = mToken.mInteger;
    uint32_t high = mToken.mInteger2;

    // A range that descends, or high end exceeds the current range of
    // Unicode (U+0-10FFFF) invalidates the descriptor.
    if (low > high || high > 0x10FFFF) {
      return false;
    }
    ranges.AppendElement(low);
    ranges.AppendElement(high);

    if (!ExpectSymbol(',', true))
      break;
  }

  if (ranges.Length() == 0)
    return false;

  RefPtr<nsCSSValue::Array> srcVals
    = nsCSSValue::Array::Create(ranges.Length());

  for (uint32_t i = 0; i < ranges.Length(); i++)
    srcVals->Item(i).SetIntValue(ranges[i], eCSSUnit_Integer);
  aValue.SetArrayValue(srcVals, eCSSUnit_Array);
  return true;
}

// font-feature-settings: normal | <feature-tag-value> [, <feature-tag-value>]*
// <feature-tag-value> = <string> [ <integer> | on | off ]?

// minimum - "tagx", "tagy", "tagz"
// edge error case - "tagx" on 1, "tagx" "tagy", "tagx" -1, "tagx" big

// pair value is always x = string, y = int

// font feature tags must be four ASCII characters
#define FEATURE_TAG_LENGTH   4

static bool
ValidFontFeatureTag(const nsString& aTag)
{
  if (aTag.Length() != FEATURE_TAG_LENGTH) {
    return false;
  }
  uint32_t i;
  for (i = 0; i < FEATURE_TAG_LENGTH; i++) {
    uint32_t ch = aTag[i];
    if (ch < 0x20 || ch > 0x7e) {
      return false;
    }
  }
  return true;
}

bool
CSSParserImpl::ParseFontFeatureSettings(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL,
                              nullptr)) {
    return true;
  }

  auto resultHead = MakeUnique<nsCSSValuePairList>();
  nsCSSValuePairList* cur = resultHead.get();

  for (;;) {
    // feature tag
    if (!GetToken(true)) {
      return false;
    }

    if (mToken.mType != eCSSToken_String ||
        !ValidFontFeatureTag(mToken.mIdent)) {
      UngetToken();
      return false;
    }
    cur->mXValue.SetStringValue(mToken.mIdent, eCSSUnit_String);

    if (!GetToken(true)) {
      cur->mYValue.SetIntValue(1, eCSSUnit_Integer);
      break;
    }

    // optional value or on/off keyword
    if (mToken.mType == eCSSToken_Number && mToken.mIntegerValid &&
        mToken.mInteger >= 0) {
      cur->mYValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
    } else if (mToken.mType == eCSSToken_Ident &&
               mToken.mIdent.LowerCaseEqualsLiteral("on")) {
      cur->mYValue.SetIntValue(1, eCSSUnit_Integer);
    } else if (mToken.mType == eCSSToken_Ident &&
               mToken.mIdent.LowerCaseEqualsLiteral("off")) {
      cur->mYValue.SetIntValue(0, eCSSUnit_Integer);
    } else {
      // something other than value/on/off, set default value
      cur->mYValue.SetIntValue(1, eCSSUnit_Integer);
      UngetToken();
    }

    if (!ExpectSymbol(',', true)) {
      break;
    }

    cur->mNext = new nsCSSValuePairList;
    cur = cur->mNext;
  }

  aValue.AdoptPairListValue(Move(resultHead));

  return true;
}

bool
CSSParserImpl::ParseFontVariationSettings(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL,
                              nullptr)) {
    return true;
  }

  auto resultHead = MakeUnique<nsCSSValuePairList>();
  nsCSSValuePairList* cur = resultHead.get();

  for (;;) {
    // variation tag
    if (!GetToken(true)) {
      return false;
    }

    // variation tags are subject to the same validation as feature tags
    if (mToken.mType != eCSSToken_String ||
        !ValidFontFeatureTag(mToken.mIdent)) {
      UngetToken();
      return false;
    }
    cur->mXValue.SetStringValue(mToken.mIdent, eCSSUnit_String);

    if (!GetToken(true)) {
      return false;
    }

    if (mToken.mType == eCSSToken_Number) {
      cur->mYValue.SetFloatValue(mToken.mNumber, eCSSUnit_Number);
    } else {
      UngetToken();
      return false;
    }

    if (!ExpectSymbol(',', true)) {
      break;
    }

    cur->mNext = new nsCSSValuePairList;
    cur = cur->mNext;
  }

  aValue.AdoptPairListValue(Move(resultHead));

  return true;
}

bool
CSSParserImpl::ParseListStyle()
{
  // 'list-style' can accept 'none' for two different subproperties,
  // 'list-style-type' and 'list-style-image'.  In order to accept
  // 'none' as the value of either but still allow another value for
  // either, we need to ensure that the first 'none' we find gets
  // allocated to a dummy property instead. Since parse function for
  // 'list-style-type' could accept values for 'list-style-position',
  // we put position in front of type.
  static const nsCSSPropertyID listStyleIDs[] = {
    eCSSPropertyExtra_x_none_value,
    eCSSProperty_list_style_position,
    eCSSProperty_list_style_type,
    eCSSProperty_list_style_image
  };

  nsCSSValue values[MOZ_ARRAY_LENGTH(listStyleIDs)];
  int32_t found =
    ParseChoice(values, listStyleIDs, ArrayLength(listStyleIDs));
  if (found < 1) {
    return false;
  }

  if ((found & (1|4|8)) == (1|4|8)) {
    if (values[0].GetUnit() == eCSSUnit_None) {
      // We found a 'none' plus another value for both of
      // 'list-style-type' and 'list-style-image'.  This is a parse
      // error, since the 'none' has to count for at least one of them.
      return false;
    } else {
      NS_ASSERTION(found == (1|2|4|8) && values[0] == values[1] &&
                   values[0] == values[2] && values[0] == values[3],
                   "should be a special value");
    }
  }

  if ((found & 2) == 0) {
    values[1].SetIntValue(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
                          eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) {
    // Provide default values
    nsAtom* type = (found & 1) ? nsGkAtoms::none : nsGkAtoms::disc;
    values[2].SetAtomIdentValue(do_AddRef(type));
  }
  if ((found & 8) == 0) {
    values[3].SetNoneValue();
  }

  // Start at 1 to avoid appending fake value.
  for (uint32_t index = 1; index < ArrayLength(listStyleIDs); ++index) {
    AppendValue(listStyleIDs[index], values[index]);
  }
  return true;
}

bool
CSSParserImpl::ParseListStyleType(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_STRING,
                              nullptr)) {
    return true;
  }

  if (ParseCounterStyleNameValue(aValue) || ParseSymbols(aValue)) {
    return true;
  }

  return false;
}

bool
CSSParserImpl::ParseMargin()
{
  static const nsCSSPropertyID kMarginSideIDs[] = {
    eCSSProperty_margin_top,
    eCSSProperty_margin_right,
    eCSSProperty_margin_bottom,
    eCSSProperty_margin_left
  };

  return ParseBoxProperties(kMarginSideIDs);
}

bool
CSSParserImpl::ParseObjectPosition()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr) &&
      !ParsePositionValue(value)) {
    return false;
  }
  AppendValue(eCSSProperty_object_position, value);
  return true;
}

bool
CSSParserImpl::ParseOutline()
{
  const int32_t numProps = 3;
  static const nsCSSPropertyID kOutlineIDs[] = {
    eCSSProperty_outline_color,
    eCSSProperty_outline_style,
    eCSSProperty_outline_width
  };

  nsCSSValue  values[numProps];
  int32_t found = ParseChoice(values, kOutlineIDs, numProps);
  if (found < 1) {
    return false;
  }

  // Provide default values
  if ((found & 1) == 0) { // Provide default outline-color
    values[0].SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }
  if ((found & 2) == 0) { // Provide default outline-style
    values[1].SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) { // Provide default outline-width
    values[2].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }

  int32_t index;
  for (index = 0; index < numProps; index++) {
    AppendValue(kOutlineIDs[index], values[index]);
  }
  return true;
}

bool
CSSParserImpl::ParseOverflow()
{
  nsCSSValue overflow;
  if (!ParseSingleTokenVariant(overflow, VARIANT_HK,
                               nsCSSProps::kOverflowKTable)) {
    return false;
  }

  nsCSSValue overflowX(overflow);
  nsCSSValue overflowY(overflow);
  if (eCSSUnit_Enumerated == overflow.GetUnit())
    switch(overflow.GetIntValue()) {
      case NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL:
        overflowX.SetIntValue(NS_STYLE_OVERFLOW_SCROLL, eCSSUnit_Enumerated);
        overflowY.SetIntValue(NS_STYLE_OVERFLOW_HIDDEN, eCSSUnit_Enumerated);
        break;
      case NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL:
        overflowX.SetIntValue(NS_STYLE_OVERFLOW_HIDDEN, eCSSUnit_Enumerated);
        overflowY.SetIntValue(NS_STYLE_OVERFLOW_SCROLL, eCSSUnit_Enumerated);
        break;
    }
  AppendValue(eCSSProperty_overflow_x, overflowX);
  AppendValue(eCSSProperty_overflow_y, overflowY);
  return true;
}

bool
CSSParserImpl::ParsePadding()
{
  static const nsCSSPropertyID kPaddingSideIDs[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left
  };

  return ParseBoxProperties(kPaddingSideIDs);
}

bool
CSSParserImpl::ParseQuotes()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_HOS, nullptr)) {
    return false;
  }
  if (value.GetUnit() == eCSSUnit_String) {
    nsCSSValue open = value;
    nsCSSValuePairList* quotes = value.SetPairListValue();
    for (;;) {
      quotes->mXValue = open;
      // get mandatory close
      if (!ParseSingleTokenVariant(quotes->mYValue, VARIANT_STRING, nullptr)) {
        return false;
      }
      // look for another open
      if (!ParseSingleTokenVariant(open, VARIANT_STRING, nullptr)) {
        break;
      }
      quotes->mNext = new nsCSSValuePairList;
      quotes = quotes->mNext;
    }
  }
  AppendValue(eCSSProperty_quotes, value);
  return true;
}

bool
CSSParserImpl::ParseTextDecoration()
{
  static const nsCSSPropertyID kTextDecorationIDs[] = {
    eCSSProperty_text_decoration_line,
    eCSSProperty_text_decoration_style,
    eCSSProperty_text_decoration_color
  };
  const int32_t numProps = MOZ_ARRAY_LENGTH(kTextDecorationIDs);
  nsCSSValue values[numProps];

  int32_t found = ParseChoice(values, kTextDecorationIDs, numProps);
  if (found < 1) {
    return false;
  }

  // Provide default values
  if ((found & 1) == 0) { // Provide default text-decoration-line
    values[0].SetIntValue(NS_STYLE_TEXT_DECORATION_LINE_NONE,
                          eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default text-decoration-style
    values[1].SetIntValue(NS_STYLE_TEXT_DECORATION_STYLE_SOLID,
                          eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) { // Provide default text-decoration-color
    values[2].SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }

  for (int32_t index = 0; index < numProps; index++) {
    AppendValue(kTextDecorationIDs[index], values[index]);
  }
  return true;
}

bool
CSSParserImpl::ParseTextEmphasis()
{
  static constexpr nsCSSPropertyID kTextEmphasisIDs[] = {
    eCSSProperty_text_emphasis_style,
    eCSSProperty_text_emphasis_color
  };
  constexpr int32_t numProps = MOZ_ARRAY_LENGTH(kTextEmphasisIDs);
  nsCSSValue values[numProps];

  int32_t found = ParseChoice(values, kTextEmphasisIDs, numProps);
  if (found < 1) {
    return false;
  }

  if (!(found & 1)) { // Provide default text-emphasis-style
    values[0].SetNoneValue();
  }
  if (!(found & 2)) { // Provide default text-emphasis-color
    values[1].SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }

  for (int32_t index = 0; index < numProps; index++) {
    AppendValue(kTextEmphasisIDs[index], values[index]);
  }
  return true;
}

bool
CSSParserImpl::ParseTextEmphasisPosition(nsCSSValue& aValue)
{
  static_assert((NS_STYLE_TEXT_EMPHASIS_POSITION_OVER ^
                 NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER ^
                 NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT ^
                 NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT) ==
                (NS_STYLE_TEXT_EMPHASIS_POSITION_OVER |
                 NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER |
                 NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT |
                 NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT),
                "text-emphasis-position constants should be bitmasks");

  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT, nullptr)) {
    return true;
  }

  nsCSSValue first, second;
  const auto& kTable = nsCSSProps::kTextEmphasisPositionKTable;
  if (!ParseSingleTokenVariant(first, VARIANT_KEYWORD, kTable) ||
      !ParseSingleTokenVariant(second, VARIANT_KEYWORD, kTable)) {
    return false;
  }

  auto firstValue = first.GetIntValue();
  auto secondValue = second.GetIntValue();
  if ((firstValue == NS_STYLE_TEXT_EMPHASIS_POSITION_OVER ||
      firstValue == NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER) ==
      (secondValue == NS_STYLE_TEXT_EMPHASIS_POSITION_OVER ||
      secondValue == NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER)) {
    return false;
  }

  aValue.SetIntValue(firstValue | secondValue, eCSSUnit_Enumerated);
  return true;
}

bool
CSSParserImpl::ParseTextEmphasisStyle(nsCSSValue& aValue)
{
  static_assert((NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK ^
                 NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK) ==
                (NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK |
                 NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK),
                "text-emphasis-style shape and fill constants "
                "should not intersect");
  static_assert(NS_STYLE_TEXT_EMPHASIS_STYLE_FILLED == 0,
                "Making 'filled' zero ensures that if neither 'filled' nor "
                "'open' is specified, we compute it to 'filled' per spec");

  if (ParseSingleTokenVariant(aValue, VARIANT_HOS, nullptr)) {
    return true;
  }

  nsCSSValue first, second;
  const auto& fillKTable = nsCSSProps::kTextEmphasisStyleFillKTable;
  const auto& shapeKTable = nsCSSProps::kTextEmphasisStyleShapeKTable;

  // Parse a fill value and/or a shape value, in either order.
  // (Require at least one of them, and treat the second as optional.)
  if (ParseSingleTokenVariant(first, VARIANT_KEYWORD, fillKTable)) {
    Unused << ParseSingleTokenVariant(second, VARIANT_KEYWORD, shapeKTable);
  } else if (ParseSingleTokenVariant(first, VARIANT_KEYWORD, shapeKTable)) {
    Unused << ParseSingleTokenVariant(second, VARIANT_KEYWORD, fillKTable);
  } else {
    return false;
  }

  auto value = first.GetIntValue();
  if (second.GetUnit() == eCSSUnit_Enumerated) {
    value |= second.GetIntValue();
  }
  aValue.SetIntValue(value, eCSSUnit_Enumerated);
  return true;
}

bool
CSSParserImpl::ParseTextAlign(nsCSSValue& aValue, const KTableEntry aTable[])
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT, nullptr)) {
    // 'inherit', 'initial' and 'unset' must be alone
    return true;
  }

  nsCSSValue left;
  if (!ParseSingleTokenVariant(left, VARIANT_KEYWORD, aTable)) {
    return false;
  }

  if (!nsLayoutUtils::IsTextAlignUnsafeValueEnabled()) {
    aValue = left;
    return true;
  }

  nsCSSValue right;
  if (ParseSingleTokenVariant(right, VARIANT_KEYWORD, aTable)) {
    // 'true' must be combined with some other value than 'true'.
    if (left.GetIntValue() == NS_STYLE_TEXT_ALIGN_UNSAFE &&
        right.GetIntValue() == NS_STYLE_TEXT_ALIGN_UNSAFE) {
      return false;
    }
    aValue.SetPairValue(left, right);
  } else {
    // Single value 'true' is not allowed.
    if (left.GetIntValue() == NS_STYLE_TEXT_ALIGN_UNSAFE) {
      return false;
    }
    aValue = left;
  }
  return true;
}

bool
CSSParserImpl::ParseTextAlign(nsCSSValue& aValue)
{
  return ParseTextAlign(aValue, nsCSSProps::kTextAlignKTable);
}

bool
CSSParserImpl::ParseTextAlignLast(nsCSSValue& aValue)
{
  return ParseTextAlign(aValue, nsCSSProps::kTextAlignLastKTable);
}

bool
CSSParserImpl::ParseTextDecorationLine(nsCSSValue& aValue)
{
  static_assert((NS_STYLE_TEXT_DECORATION_LINE_NONE ^
                 NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE ^
                 NS_STYLE_TEXT_DECORATION_LINE_OVERLINE ^
                 NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH ^
                 NS_STYLE_TEXT_DECORATION_LINE_BLINK) ==
                (NS_STYLE_TEXT_DECORATION_LINE_NONE |
                 NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE |
                 NS_STYLE_TEXT_DECORATION_LINE_OVERLINE |
                 NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH |
                 NS_STYLE_TEXT_DECORATION_LINE_BLINK),
                "text decoration constants need to be bitmasks");
  if (ParseSingleTokenVariant(aValue, VARIANT_HK,
                              nsCSSProps::kTextDecorationLineKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      int32_t intValue = aValue.GetIntValue();
      if (intValue != NS_STYLE_TEXT_DECORATION_LINE_NONE) {
        // look for more keywords
        nsCSSValue  keyword;
        int32_t index;
        for (index = 0; index < 3; index++) {
          if (ParseEnum(keyword, nsCSSProps::kTextDecorationLineKTable)) {
            int32_t newValue = keyword.GetIntValue();
            if (newValue == NS_STYLE_TEXT_DECORATION_LINE_NONE ||
                newValue & intValue) {
              // 'none' keyword in conjuction with others is not allowed, and
              // duplicate keyword is not allowed.
              return false;
            }
            intValue |= newValue;
          }
          else {
            break;
          }
        }
        aValue.SetIntValue(intValue, eCSSUnit_Enumerated);
      }
    }
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParseTextOverflow(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT, nullptr)) {
    // 'inherit', 'initial' and 'unset' must be alone
    return true;
  }

  nsCSSValue left;
  if (!ParseSingleTokenVariant(left, VARIANT_KEYWORD | VARIANT_STRING,
                               nsCSSProps::kTextOverflowKTable))
    return false;

  nsCSSValue right;
  if (ParseSingleTokenVariant(right, VARIANT_KEYWORD | VARIANT_STRING,
                              nsCSSProps::kTextOverflowKTable))
    aValue.SetPairValue(left, right);
  else {
    aValue = left;
  }
  return true;
}

bool
CSSParserImpl::ParseTouchAction(nsCSSValue& aValue)
{
  // Avaliable values of property touch-action:
  // auto | none | [pan-x || pan-y] | manipulation

  if (!ParseSingleTokenVariant(aValue, VARIANT_HK,
                               nsCSSProps::kTouchActionKTable)) {
    return false;
  }

  // Auto and None keywords aren't allowed in conjunction with others.
  // Also inherit, initial and unset values are available.
  if (eCSSUnit_Enumerated != aValue.GetUnit()) {
    return true;
  }

  int32_t intValue = aValue.GetIntValue();
  nsCSSValue nextValue;
  if (ParseEnum(nextValue, nsCSSProps::kTouchActionKTable)) {
    int32_t nextIntValue = nextValue.GetIntValue();

    // duplicates aren't allowed.
    if (nextIntValue & intValue) {
      return false;
    }

    // Auto and None and Manipulation is not allowed in conjunction with others.
    if ((intValue | nextIntValue) & (NS_STYLE_TOUCH_ACTION_NONE |
                                     NS_STYLE_TOUCH_ACTION_AUTO |
                                     NS_STYLE_TOUCH_ACTION_MANIPULATION)) {
      return false;
    }

    aValue.SetIntValue(nextIntValue | intValue, eCSSUnit_Enumerated);
  }

  return true;
}

bool
CSSParserImpl::ParseTextCombineUpright(nsCSSValue& aValue)
{
  if (!ParseSingleTokenVariant(aValue, VARIANT_HK,
                               nsCSSProps::kTextCombineUprightKTable)) {
    return false;
  }

  // if 'digits', need to check for an explicit number [2, 3, 4]
  if (eCSSUnit_Enumerated == aValue.GetUnit() &&
      aValue.GetIntValue() == NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2) {
    if (!nsLayoutUtils::TextCombineUprightDigitsEnabled()) {
      return false;
    }
    if (!GetToken(true)) {
      return true;
    }
    if (mToken.mType == eCSSToken_Number && mToken.mIntegerValid) {
      switch (mToken.mInteger) {
        case 2:  // already set, nothing to do
          break;
        case 3:
          aValue.SetIntValue(NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_3,
                             eCSSUnit_Enumerated);
          break;
        case 4:
          aValue.SetIntValue(NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_4,
                             eCSSUnit_Enumerated);
          break;
        default:
          // invalid digits value
          return false;
      }
    } else {
      UngetToken();
    }
  }
  return true;
}

///////////////////////////////////////////////////////
// transform Parsing Implementation

/* Reads a function list of arguments and consumes the closing parenthesis.
 * Do not call this function directly; it's meant to be called from
 * ParseFunction.
 */
bool
CSSParserImpl::ParseFunctionInternals(const uint32_t aVariantMask[],
                                      uint32_t aVariantMaskAll,
                                      uint16_t aMinElems,
                                      uint16_t aMaxElems,
                                      InfallibleTArray<nsCSSValue> &aOutput)
{
  NS_ASSERTION((aVariantMask && !aVariantMaskAll) ||
               (!aVariantMask && aVariantMaskAll),
               "only one of the two variant mask parameters can be set");

  for (uint16_t index = 0; index < aMaxElems; ++index) {
    nsCSSValue newValue;
    uint32_t m = aVariantMaskAll ? aVariantMaskAll : aVariantMask[index];
    if (ParseVariant(newValue, m, nullptr) != CSSParseResult::Ok) {
      break;
    }

    if (nsCSSValue::IsFloatUnit(newValue.GetUnit())) {
      // Clamp infinity or -infinity values to max float or -max float to avoid
      // calculations with infinity.
      newValue.SetFloatValue(
        mozilla::clamped(newValue.GetFloatValue(),
                         -std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max()),
        newValue.GetUnit());
    }

    aOutput.AppendElement(newValue);

    if (ExpectSymbol(',', true)) {
      // Move on to the next argument if we see a comma.
      continue;
    }

    if (ExpectSymbol(')', true)) {
      // Make sure we've read enough symbols if we see a closing parenthesis.
      return (index + 1) >= aMinElems;
    }

    // Only a comma or a closing parenthesis is valid after an argument.
    break;
  }

  // If we're here, we've hit an error without seeing a closing parenthesis or
  // we've read too many elements without seeing a closing parenthesis.
  SkipUntil(')');
  return false;
}

/* Parses a function [ input of the form (a [, b]*) ] and stores it
 * as an nsCSSValue that holds a function of the form
 * function-name arg1 arg2 ... argN
 *
 * On error, the return value is false.
 *
 * @param aFunction The name of the function that we're reading.
 * @param aAllowedTypes An array of values corresponding to the legal
 *        types for each element in the function.  The zeroth element in the
 *        array corresponds to the first function parameter, etc.  The length
 *        of this array _must_ be greater than or equal to aMaxElems or the
 *        behavior is undefined.  If not null, aAllowTypesAll must be 0.
 * @param aAllowedTypesAll If set, every element tested for these types
 * @param aMinElems Minimum number of elements to read.  Reading fewer than
 *        this many elements will result in the function failing.
 * @param aMaxElems Maximum number of elements to read.  Reading more than
 *        this many elements will result in the function failing.
 * @param aValue (out) The value that was parsed.
 */
bool
CSSParserImpl::ParseFunction(nsCSSKeyword aFunction,
                             const uint32_t aAllowedTypes[],
                             uint32_t aAllowedTypesAll,
                             uint16_t aMinElems, uint16_t aMaxElems,
                             nsCSSValue &aValue)
{
  NS_ASSERTION((aAllowedTypes && !aAllowedTypesAll) ||
               (!aAllowedTypes && aAllowedTypesAll),
               "only one of the two allowed type parameter can be set");
  typedef InfallibleTArray<nsCSSValue>::size_type arrlen_t;

  /* 2^16 - 2, so that if we have 2^16 - 2 transforms, we have 2^16 - 1
   * elements stored in the the nsCSSValue::Array.
   */
  static const arrlen_t MAX_ALLOWED_ELEMS = 0xFFFE;

  /* Read in a list of values as an array, failing if we can't or if
   * it's out of bounds.
   *
   * We reserve 16 entries in the foundValues array in order to avoid
   * having to resize the array dynamically when parsing some well-formed
   * functions.  The number 16 is coming from the number of arguments that
   * matrix3d() accepts.
   */
  AutoTArray<nsCSSValue, 16> foundValues;
  if (!ParseFunctionInternals(aAllowedTypes, aAllowedTypesAll, aMinElems,
                              aMaxElems, foundValues)) {
    return false;
  }

  /*
   * In case the user has given us more than 2^16 - 2 arguments,
   * we'll truncate them at 2^16 - 2 arguments.
   */
  uint16_t numArgs = std::min(foundValues.Length(), MAX_ALLOWED_ELEMS);
  RefPtr<nsCSSValue::Array> convertedArray =
    aValue.InitFunction(aFunction, numArgs);

  /* Copy things over. */
  for (uint16_t index = 0; index < numArgs; ++index)
    convertedArray->Item(index + 1) = foundValues[static_cast<arrlen_t>(index)];

  /* Return it! */
  return true;
}

/**
 * Given a token, determines the minimum and maximum number of function
 * parameters to read, along with the mask that should be used to read
 * those function parameters.  If the token isn't a transform function,
 * returns an error.
 *
 * @param aToken The token identifying the function.
 * @param aIsPrefixed If true, parse matrices using the matrix syntax
 *   for -moz-transform.
 * @param aDisallowRelativeValues If true, only allow variants that are
 *   numbers or have non-relative dimensions.
 * @param aMinElems [out] The minimum number of elements to read.
 * @param aMaxElems [out] The maximum number of elements to read
 * @param aVariantMask [out] The variant mask to use during parsing
 * @return Whether the information was loaded successfully.
 */
static bool GetFunctionParseInformation(nsCSSKeyword aToken,
                                        bool aIsPrefixed,
                                        bool aDisallowRelativeValues,
                                        uint16_t &aMinElems,
                                        uint16_t &aMaxElems,
                                        const uint32_t *& aVariantMask)
{
/* These types represent the common variant masks that will be used to
   * parse out the individual functions.  The order in the enumeration
   * must match the order in which the masks are declared.
   */
  enum { eLengthPercentCalc,
         eLengthCalc,
         eAbsoluteLengthCalc,
         eTwoLengthPercentCalcs,
         eTwoAbsoluteLengthCalcs,
         eTwoLengthPercentCalcsOneLengthCalc,
         eThreeAbsoluteLengthCalc,
         eAngle,
         eTwoAngles,
         eNumber,
         eNonNegativeLength,
         eNonNegativeAbsoluteLength,
         eTwoNumbers,
         eThreeNumbers,
         eThreeNumbersOneAngle,
         eMatrix,
         eMatrixPrefixed,
         eMatrix3d,
         eMatrix3dPrefixed,
         eNumVariantMasks };
  static const int32_t kMaxElemsPerFunction = 16;
  static const uint32_t kVariantMasks[eNumVariantMasks][kMaxElemsPerFunction] = {
    {VARIANT_LPCALC},
    {VARIANT_LCALC},
    {VARIANT_LB},
    {VARIANT_LPCALC, VARIANT_LPCALC},
    {VARIANT_LBCALC, VARIANT_LBCALC},
    {VARIANT_LPCALC, VARIANT_LPCALC, VARIANT_LCALC},
    {VARIANT_LBCALC, VARIANT_LBCALC, VARIANT_LBCALC},
    {VARIANT_ANGLE_OR_ZERO},
    {VARIANT_ANGLE_OR_ZERO, VARIANT_ANGLE_OR_ZERO},
    {VARIANT_NUMBER},
    {VARIANT_LENGTH|VARIANT_NONNEGATIVE_DIMENSION},
    {VARIANT_LB|VARIANT_NONNEGATIVE_DIMENSION},
    {VARIANT_NUMBER, VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_ANGLE_OR_ZERO},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_LPNCALC, VARIANT_LPNCALC},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_LPNCALC, VARIANT_LPNCALC, VARIANT_LNCALC, VARIANT_NUMBER}};
  // Map from a mask to a congruent mask that excludes relative variants.
  static const int32_t kNonRelativeVariantMap[eNumVariantMasks] = {
    eAbsoluteLengthCalc,
    eAbsoluteLengthCalc,
    eAbsoluteLengthCalc,
    eTwoAbsoluteLengthCalcs,
    eTwoAbsoluteLengthCalcs,
    eThreeAbsoluteLengthCalc,
    eThreeAbsoluteLengthCalc,
    eAngle,
    eTwoAngles,
    eNumber,
    eNonNegativeAbsoluteLength,
    eNonNegativeAbsoluteLength,
    eTwoNumbers,
    eThreeNumbers,
    eThreeNumbersOneAngle,
    eMatrix,
    eMatrix,
    eMatrix3d,
    eMatrix3d };

#ifdef DEBUG
  static const uint8_t kVariantMaskLengths[eNumVariantMasks] =
    {1, 1, 1, 2, 2, 3, 3, 1, 2, 1, 1, 1, 2, 3, 4, 6, 6, 16, 16};
#endif

  int32_t variantIndex = eNumVariantMasks;

  switch (aToken) {
  case eCSSKeyword_translatex:
  case eCSSKeyword_translatey:
    /* Exactly one length or percent. */
    variantIndex = eLengthPercentCalc;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_translatez:
    /* Exactly one length */
    variantIndex = eLengthCalc;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_translate3d:
    /* Exactly two lengthds or percents and a number */
    variantIndex = eTwoLengthPercentCalcsOneLengthCalc;
    aMinElems = 3U;
    aMaxElems = 3U;
    break;
  case eCSSKeyword_scalez:
  case eCSSKeyword_scalex:
  case eCSSKeyword_scaley:
    /* Exactly one scale factor. */
    variantIndex = eNumber;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_scale3d:
    /* Exactly three scale factors. */
    variantIndex = eThreeNumbers;
    aMinElems = 3U;
    aMaxElems = 3U;
    break;
  case eCSSKeyword_rotatex:
  case eCSSKeyword_rotatey:
  case eCSSKeyword_rotate:
  case eCSSKeyword_rotatez:
    /* Exactly one angle. */
    variantIndex = eAngle;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_rotate3d:
    variantIndex = eThreeNumbersOneAngle;
    aMinElems = 4U;
    aMaxElems = 4U;
    break;
  case eCSSKeyword_translate:
    /* One or two lengths or percents. */
    variantIndex = eTwoLengthPercentCalcs;
    aMinElems = 1U;
    aMaxElems = 2U;
    break;
  case eCSSKeyword_skew:
    /* Exactly one or two angles. */
    variantIndex = eTwoAngles;
    aMinElems = 1U;
    aMaxElems = 2U;
    break;
  case eCSSKeyword_scale:
    /* One or two scale factors. */
    variantIndex = eTwoNumbers;
    aMinElems = 1U;
    aMaxElems = 2U;
    break;
  case eCSSKeyword_skewx:
    /* Exactly one angle. */
    variantIndex = eAngle;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_skewy:
    /* Exactly one angle. */
    variantIndex = eAngle;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_matrix:
    /* Six values, all numbers. */
    variantIndex = aIsPrefixed ? eMatrixPrefixed : eMatrix;
    aMinElems = 6U;
    aMaxElems = 6U;
    break;
  case eCSSKeyword_matrix3d:
    /* 16 matrix values, all numbers */
    variantIndex = aIsPrefixed ? eMatrix3dPrefixed : eMatrix3d;
    aMinElems = 16U;
    aMaxElems = 16U;
    break;
  case eCSSKeyword_perspective:
    /* Exactly one scale number. */
    variantIndex = eNonNegativeLength;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  default:
    /* Oh dear, we didn't match.  Report an error. */
    return false;
  }

  if (aDisallowRelativeValues) {
    variantIndex = kNonRelativeVariantMap[variantIndex];
  }

  NS_ASSERTION(aMinElems > 0, "Didn't update minimum elements!");
  NS_ASSERTION(aMaxElems > 0, "Didn't update maximum elements!");
  NS_ASSERTION(aMinElems <= aMaxElems, "aMinElems > aMaxElems!");
  NS_ASSERTION(variantIndex >= 0, "Invalid variant mask!");
  NS_ASSERTION(variantIndex < eNumVariantMasks, "Invalid variant mask!");
#ifdef DEBUG
  NS_ASSERTION(aMaxElems <= kVariantMaskLengths[variantIndex],
               "Invalid aMaxElems for this variant mask.");
#endif

  // Convert the index into a mask.
  aVariantMask = kVariantMasks[variantIndex];

  return true;
}

bool CSSParserImpl::ParseWillChange()
{
  nsCSSValue listValue;
  nsCSSValueList* currentListValue = listValue.SetListValue();
  bool first = true;
  for (;;) {
    const uint32_t variantMask = VARIANT_IDENTIFIER |
                                 VARIANT_INHERIT |
                                 VARIANT_NONE |
                                 VARIANT_ALL |
                                 VARIANT_AUTO;
    nsCSSValue value;
    if (!ParseSingleTokenVariant(value, variantMask, nullptr)) {
      return false;
    }

    if (value.GetUnit() == eCSSUnit_None ||
        value.GetUnit() == eCSSUnit_All)
    {
      return false;
    }

    if (value.GetUnit() != eCSSUnit_Ident) {
      if (first) {
        AppendValue(eCSSProperty_will_change, value);
        return true;
      } else {
        return false;
      }
    }

    value.AtomizeIdentValue();
    nsAtom* atom = value.GetAtomValue();
    if (atom == nsGkAtoms::_default || atom == nsGkAtoms::willChange) {
      return false;
    }

    currentListValue->mValue = Move(value);

    if (!ExpectSymbol(',', true)) {
      break;
    }
    currentListValue->mNext = new nsCSSValueList;
    currentListValue = currentListValue->mNext;
    first = false;
  }

  AppendValue(eCSSProperty_will_change, listValue);
  return true;
}

/* Reads a single transform function from the tokenizer stream, reporting an
 * error if something goes wrong.
 */
bool
CSSParserImpl::ParseSingleTransform(bool aIsPrefixed,
                                    bool aDisallowRelativeValues,
                                    nsCSSValue& aValue)
{
  if (!GetToken(true))
    return false;

  if (mToken.mType != eCSSToken_Function) {
    UngetToken();
    return false;
  }

  const uint32_t* variantMask;
  uint16_t minElems, maxElems;
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);

  if (!GetFunctionParseInformation(keyword, aIsPrefixed,
                                   aDisallowRelativeValues,
                                   minElems, maxElems,
                                   variantMask))
    return false;

  return ParseFunction(keyword, variantMask, 0, minElems, maxElems, aValue);
}

/* Parses a transform property list by continuously reading in properties
 * and constructing a matrix from it.
 * aProperty can be transform or -moz-window-transform.
 * FIXME: For -moz-window-transform, it would be nice to reject non-2d
 * transforms at parse time, because the implementation only supports 2d
 * transforms. Instead, at the moment, non-2d transforms are treated as the
 * identity transform very late in the pipeline.
 */
bool
CSSParserImpl::ParseTransform(bool aIsPrefixed, nsCSSPropertyID aProperty,
                              bool aDisallowRelativeValues)
{
  nsCSSValue value;
  // 'inherit', 'initial', 'unset' and 'none' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    nsCSSValueSharedList* list = new nsCSSValueSharedList;
    value.SetSharedListValue(list);
    list->mHead = new nsCSSValueList;
    nsCSSValueList* cur = list->mHead;
    for (;;) {
      if (!ParseSingleTransform(aIsPrefixed, aDisallowRelativeValues,
                                cur->mValue)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(aProperty, value);
  return true;
}

/* Reads a polygon function's argument list.
 */
bool
CSSParserImpl::ParsePolygonFunction(nsCSSValue& aValue)
{
  uint16_t numArgs = 1;

  nsCSSValue fillRuleValue;
  if (ParseEnum(fillRuleValue, nsCSSProps::kFillRuleKTable)) {
    numArgs++;

    // The fill-rule must be comma separated from the polygon points.
    if (!ExpectSymbol(',', true)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
      SkipUntil(')');
      return false;
    }
  }

  nsCSSValue coordinates;
  nsCSSValuePairList* item = coordinates.SetPairListValue();
  for (;;) {
    nsCSSValue xValue, yValue;
    if (ParseVariant(xValue, VARIANT_LPCALC, nullptr) != CSSParseResult::Ok ||
        ParseVariant(yValue, VARIANT_LPCALC, nullptr) != CSSParseResult::Ok) {
      REPORT_UNEXPECTED_TOKEN(PECoordinatePair);
      SkipUntil(')');
      return false;
    }
    item->mXValue = xValue;
    item->mYValue = yValue;

    // See whether to continue or whether to look for end of function.
    if (!ExpectSymbol(',', true)) {
      // We need to read the closing parenthesis.
      if (!ExpectSymbol(')', true)) {
        REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
        SkipUntil(')');
        return false;
      }
      break;
    }
    item->mNext = new nsCSSValuePairList;
    item = item->mNext;
  }

  RefPtr<nsCSSValue::Array> functionArray =
    aValue.InitFunction(eCSSKeyword_polygon, numArgs);
  functionArray->Item(numArgs) = coordinates;
  if (numArgs > 1) {
    functionArray->Item(1) = fillRuleValue;
  }

  return true;
}

bool
CSSParserImpl::ParseCircleOrEllipseFunction(nsCSSKeyword aKeyword,
                                            nsCSSValue& aValue)
{
  nsCSSValue radiusX, radiusY, position;
  bool hasRadius = false, hasPosition = false;

  int32_t mask = VARIANT_LPCALC | VARIANT_NONNEGATIVE_DIMENSION |
                 VARIANT_KEYWORD;
  CSSParseResult result =
    ParseVariant(radiusX, mask, nsCSSProps::kShapeRadiusKTable);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::Ok) {
    if (aKeyword == eCSSKeyword_ellipse) {
      if (ParseVariant(radiusY, mask, nsCSSProps::kShapeRadiusKTable) !=
          CSSParseResult::Ok) {
        REPORT_UNEXPECTED_TOKEN(PEExpectedRadius);
        SkipUntil(')');
        return false;
      }
    }
    hasRadius = true;
  }

  if (!ExpectSymbol(')', true)) {
    if (!GetToken(true)) {
      REPORT_UNEXPECTED_EOF(PEPositionEOF);
      return false;
    }

    if (mToken.mType != eCSSToken_Ident ||
        !mToken.mIdent.LowerCaseEqualsLiteral("at") ||
        !ParsePositionValueForBasicShape(position)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedPosition);
      SkipUntil(')');
      return false;
    }
    if (!ExpectSymbol(')', true)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
      SkipUntil(')');
      return false;
    }
    hasPosition = true;
  }

  size_t count = aKeyword == eCSSKeyword_circle ? 2 : 3;
  RefPtr<nsCSSValue::Array> functionArray =
    aValue.InitFunction(aKeyword, count);
  if (hasRadius) {
    functionArray->Item(1) = radiusX;
    if (aKeyword == eCSSKeyword_ellipse) {
      functionArray->Item(2) = radiusY;
    }
  }
  if (hasPosition) {
    functionArray->Item(count) = position;
  }

  return true;
}

bool
CSSParserImpl::ParseInsetFunction(nsCSSValue& aValue)
{
  RefPtr<nsCSSValue::Array> functionArray =
    aValue.InitFunction(eCSSKeyword_inset, 5);

  int count = 0;
  while (count < 4) {
    CSSParseResult result =
      ParseVariant(functionArray->Item(count + 1), VARIANT_LPCALC, nullptr);
    if (result == CSSParseResult::Error) {
      count = 0;
      break;
    } else if (result == CSSParseResult::NotFound) {
      break;
    }
    ++count;
  }

  if (count == 0) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedShapeArg);
    SkipUntil(')');
    return false;
  }

  if (!ExpectSymbol(')', true)) {
    if (!GetToken(true)) {
      NS_NOTREACHED("ExpectSymbol should have returned true");
      return false;
    }

    RefPtr<nsCSSValue::Array> radiusArray = nsCSSValue::Array::Create(4);
    functionArray->Item(5).SetArrayValue(radiusArray, eCSSUnit_Array);
    if (mToken.mType != eCSSToken_Ident ||
        !mToken.mIdent.LowerCaseEqualsLiteral("round") ||
        !ParseBoxCornerRadiiInternals(radiusArray->ItemStorage())) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedRadius);
      SkipUntil(')');
      return false;
    }

    if (!ExpectSymbol(')', true)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
      SkipUntil(')');
      return false;
    }
  }

  return true;
}

bool
CSSParserImpl::ParseBasicShape(nsCSSValue& aValue, bool* aConsumedTokens)
{
  if (!GetToken(true)) {
    return false;
  }

  if (mToken.mType != eCSSToken_Function) {
    UngetToken();
    return false;
  }

  // Specific shape function parsing always consumes tokens.
  *aConsumedTokens = true;
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
  switch (keyword) {
  case eCSSKeyword_polygon:
    return ParsePolygonFunction(aValue);
  case eCSSKeyword_circle:
  case eCSSKeyword_ellipse:
    return ParseCircleOrEllipseFunction(keyword, aValue);
  case eCSSKeyword_inset:
    return ParseInsetFunction(aValue);
  default:
    return false;
  }
}

bool
CSSParserImpl::ParseReferenceBoxAndBasicShape(
  nsCSSValue& aValue,
  const KTableEntry aBoxKeywordTable[])
{
  nsCSSValue referenceBox;
  bool hasBox = ParseEnum(referenceBox, aBoxKeywordTable);

  const bool boxCameFirst = hasBox;

  nsCSSValue basicShape;
  bool basicShapeConsumedTokens = false;
  bool hasShape = ParseBasicShape(basicShape, &basicShapeConsumedTokens);

  // Parsing wasn't successful if ParseBasicShape consumed tokens but failed
  // or if the token was neither a reference box nor a basic shape.
  if ((!hasShape && basicShapeConsumedTokens) || (!hasBox && !hasShape)) {
    return false;
  }

  // Check if the second argument is a reference box if the first wasn't.
  if (!hasBox) {
    hasBox = ParseEnum(referenceBox, aBoxKeywordTable);
  }

  RefPtr<nsCSSValue::Array> fullValue =
    nsCSSValue::Array::Create((hasBox && hasShape) ? 2 : 1);

  if (hasBox && hasShape) {
    fullValue->Item(boxCameFirst ? 0 : 1) = referenceBox;
    fullValue->Item(boxCameFirst ? 1 : 0) = basicShape;
  } else if (hasBox) {
    fullValue->Item(0) = referenceBox;
  } else {
    MOZ_ASSERT(hasShape, "should've bailed if we got neither box nor shape");
    fullValue->Item(0) = basicShape;
  }

  aValue.SetArrayValue(fullValue, eCSSUnit_Array);
  return true;
}

// Parse a clip-path url to a <clipPath> element or a basic shape.
bool
CSSParserImpl::ParseClipPath(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_HUO, nullptr)) {
    return true;
  }

  return ParseReferenceBoxAndBasicShape(
    aValue, nsCSSProps::kClipPathGeometryBoxKTable);
}

// none | [ <basic-shape> || <shape-box> ] | <image>
bool
CSSParserImpl::ParseShapeOutside(nsCSSValue& aValue)
{
  CSSParseResult result =
    ParseVariant(aValue, VARIANT_IMAGE | VARIANT_INHERIT, nullptr);

  if (result == CSSParseResult::Error) {
    return false;
  }

  if (result == CSSParseResult::Ok) {
    // 'inherit', 'initial', 'unset', 'none', and <image> (<url> or
    // <gradient>) must be alone.
    return true;
  }

  return ParseReferenceBoxAndBasicShape(
    aValue, nsCSSProps::kShapeOutsideShapeBoxKTable);
}

bool CSSParserImpl::ParseTransformOrigin(nsCSSPropertyID aProperty)
{
  nsCSSValuePair position;
  if (!ParseBoxPositionValues(position, true))
    return false;

  // Unlike many other uses of pairs, this position should always be stored
  // as a pair, even if the values are the same, so it always serializes as
  // a pair, and to keep the computation code simple.
  if (position.mXValue.GetUnit() == eCSSUnit_Inherit ||
      position.mXValue.GetUnit() == eCSSUnit_Initial ||
      position.mXValue.GetUnit() == eCSSUnit_Unset) {
    MOZ_ASSERT(position.mXValue == position.mYValue,
               "inherit/initial/unset only half?");
    AppendValue(aProperty, position.mXValue);
  } else {
    nsCSSValue value;
    if (aProperty != eCSSProperty_transform_origin) {
      value.SetPairValue(position.mXValue, position.mYValue);
    } else {
      nsCSSValue depth;
      CSSParseResult result =
        ParseVariant(depth, VARIANT_LENGTH | VARIANT_CALC, nullptr);
      if (result == CSSParseResult::Error) {
        return false;
      } else if (result == CSSParseResult::NotFound) {
        depth.SetFloatValue(0.0f, eCSSUnit_Pixel);
      }
      value.SetTripletValue(position.mXValue, position.mYValue, depth);
    }

    AppendValue(aProperty, value);
  }
  return true;
}

/**
 * Reads a drop-shadow value. At the moment the Filter Effects specification
 * just expects one shadow item. Should this ever change to a list of shadow
 * items, use ParseShadowList instead.
 */
bool
CSSParserImpl::ParseDropShadow(nsCSSValue* aValue)
{
  // Use nsCSSValueList to reuse the shadow resolving code in
  // nsRuleNode and nsComputedDOMStyle.
  nsCSSValue shadow;
  nsCSSValueList* cur = shadow.SetListValue();
  if (!ParseShadowItem(cur->mValue, false))
    return false;

  if (!ExpectSymbol(')', true))
    return false;

  nsCSSValue::Array* dropShadow = aValue->InitFunction(eCSSKeyword_drop_shadow, 1);

  // Copy things over.
  dropShadow->Item(1) = shadow;

  return true;
}

/**
 * Reads a single url or filter function from the tokenizer stream, reporting an
 * error if something goes wrong.
 */
bool
CSSParserImpl::ParseSingleFilter(nsCSSValue* aValue)
{
  if (ParseSingleTokenVariant(*aValue, VARIANT_URL, nullptr)) {
    return true;
  }

  if (!nsLayoutUtils::CSSFiltersEnabled()) {
    // With CSS Filters disabled, we should only accept an SVG reference filter.
    REPORT_UNEXPECTED_TOKEN(PEExpectedNoneOrURL);
    return false;
  }

  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEFilterEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Function) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNoneOrURLOrFilterFunction);
    UngetToken();
    return false;
  }

  nsCSSKeyword functionName = nsCSSKeywords::LookupKeyword(mToken.mIdent);
  // Parse drop-shadow independently of the other filter functions
  // because of its more complex characteristics.
  if (functionName == eCSSKeyword_drop_shadow) {
    if (ParseDropShadow(aValue)) {
      return true;
    } else {
      // Unrecognized filter function.
      REPORT_UNEXPECTED_TOKEN(PEExpectedNoneOrURLOrFilterFunction);
      SkipUntil(')');
      return false;
    }
  }

  // Set up the parsing rules based on the filter function.
  uint32_t variantMask = VARIANT_PN;
  bool rejectNegativeArgument = true;
  bool clampArgumentToOne = false;
  switch (functionName) {
    case eCSSKeyword_blur:
      variantMask = VARIANT_LCALC | VARIANT_NONNEGATIVE_DIMENSION;
      // VARIANT_NONNEGATIVE_DIMENSION will already reject negative lengths.
      rejectNegativeArgument = false;
      break;
    case eCSSKeyword_brightness:
    case eCSSKeyword_contrast:
    case eCSSKeyword_saturate:
      break;
    case eCSSKeyword_grayscale:
    case eCSSKeyword_invert:
    case eCSSKeyword_sepia:
    case eCSSKeyword_opacity:
      clampArgumentToOne = true;
      break;
    case eCSSKeyword_hue_rotate:
      variantMask = VARIANT_ANGLE;
      rejectNegativeArgument = false;
      break;
    default:
      // Unrecognized filter function.
      REPORT_UNEXPECTED_TOKEN(PEExpectedNoneOrURLOrFilterFunction);
      SkipUntil(')');
      return false;
  }

  // Parse the function.
  uint16_t minElems = 1U;
  uint16_t maxElems = 1U;
  uint32_t allVariants = 0;
  if (!ParseFunction(functionName, &variantMask, allVariants,
                     minElems, maxElems, *aValue)) {
    REPORT_UNEXPECTED(PEFilterFunctionArgumentsParsingError);
    return false;
  }

  // Get the first and only argument to the filter function.
  MOZ_ASSERT(aValue->GetUnit() == eCSSUnit_Function,
             "expected a filter function");
  MOZ_ASSERT(aValue->UnitHasArrayValue(),
             "filter function should be an array");
  MOZ_ASSERT(aValue->GetArrayValue()->Count() == 2,
             "filter function should have exactly one argument");
  nsCSSValue& arg = aValue->GetArrayValue()->Item(1);

  if (rejectNegativeArgument &&
      ((arg.GetUnit() == eCSSUnit_Percent && arg.GetPercentValue() < 0.0f) ||
       (arg.GetUnit() == eCSSUnit_Number && arg.GetFloatValue() < 0.0f))) {
    REPORT_UNEXPECTED(PEExpectedNonnegativeNP);
    return false;
  }

  if (clampArgumentToOne) {
    if (arg.GetUnit() == eCSSUnit_Number &&
        arg.GetFloatValue() > 1.0f) {
      arg.SetFloatValue(1.0f, arg.GetUnit());
    } else if (arg.GetUnit() == eCSSUnit_Percent &&
               arg.GetPercentValue() > 1.0f) {
      arg.SetPercentValue(1.0f);
    }
  }

  return true;
}

/**
 * Parses a filter property value by continuously reading in urls and/or filter
 * functions and constructing a list.
 *
 * When CSS Filters are enabled, the filter property accepts one or more SVG
 * reference filters and/or CSS filter functions.
 * e.g. filter: url(#my-filter-1) blur(3px) url(#my-filter-2) grayscale(50%);
 *
 * When CSS Filters are disabled, the filter property only accepts one SVG
 * reference filter.
 * e.g. filter: url(#my-filter);
 */
bool
CSSParserImpl::ParseFilter()
{
  nsCSSValue value;
  // 'inherit', 'initial', 'unset' and 'none' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    nsCSSValueList* cur = value.SetListValue();
    while (cur) {
      if (!ParseSingleFilter(&cur->mValue)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      if (!nsLayoutUtils::CSSFiltersEnabled()) {
        // With CSS Filters disabled, we should only accept one SVG reference
        // filter.
        REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
        return false;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_filter, value);
  return true;
}

bool
CSSParserImpl::ParseTransitionProperty()
{
  nsCSSValue value;
  // 'inherit', 'initial', 'unset' and 'none' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    // Accept a list of arbitrary identifiers.  They should be
    // CSS properties, but we want to accept any so that we
    // accept properties that we don't know about yet, e.g.
    // transition-property: invalid-property, left, opacity;
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseSingleTokenVariant(cur->mValue,
                                   VARIANT_IDENTIFIER | VARIANT_ALL,
                                   nullptr)) {
        return false;
      }
      if (cur->mValue.GetUnit() == eCSSUnit_Ident) {
        nsDependentString str(cur->mValue.GetStringBufferValue());
        // Exclude 'none', 'inherit', 'initial' and 'unset' according to the
        // same rules as for 'counter-reset' in CSS 2.1.
        if (str.LowerCaseEqualsLiteral("none") ||
            str.LowerCaseEqualsLiteral("inherit") ||
            str.LowerCaseEqualsLiteral("initial") ||
            (str.LowerCaseEqualsLiteral("unset") &&
             nsLayoutUtils::UnsetValueEnabled())) {
          return false;
        }
      }
      if (!ExpectSymbol(',', true)) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_transition_property, value);
  return true;
}

bool
CSSParserImpl::ParseTransitionTimingFunctionValues(nsCSSValue& aValue)
{
  NS_ASSERTION(!mHavePushBack &&
               mToken.mType == eCSSToken_Function &&
               mToken.mIdent.LowerCaseEqualsLiteral("cubic-bezier"),
               "unexpected initial state");

  RefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(4);

  float x1, x2, y1, y2;
  if (!ParseTransitionTimingFunctionValueComponent(x1, ',', true) ||
      !ParseTransitionTimingFunctionValueComponent(y1, ',', false) ||
      !ParseTransitionTimingFunctionValueComponent(x2, ',', true) ||
      !ParseTransitionTimingFunctionValueComponent(y2, ')', false)) {
    return false;
  }

  val->Item(0).SetFloatValue(x1, eCSSUnit_Number);
  val->Item(1).SetFloatValue(y1, eCSSUnit_Number);
  val->Item(2).SetFloatValue(x2, eCSSUnit_Number);
  val->Item(3).SetFloatValue(y2, eCSSUnit_Number);

  aValue.SetArrayValue(val, eCSSUnit_Cubic_Bezier);

  return true;
}

bool
CSSParserImpl::ParseTransitionTimingFunctionValueComponent(float& aComponent,
                                                           char aStop,
                                                           bool aIsXPoint)
{
  if (!GetToken(true)) {
    return false;
  }
  nsCSSToken* tk = &mToken;
  if (tk->mType == eCSSToken_Number) {
    float num = tk->mNumber;

    // Clamp infinity or -infinity values to max float or -max float to avoid
    // calculations with infinity.
    num = mozilla::clamped(num, -std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max());

    // X control point should be inside [0, 1] range.
    if (aIsXPoint && (num < 0.0 || num > 1.0)) {
      return false;
    }
    aComponent = num;
    if (ExpectSymbol(aStop, true)) {
      return true;
    }
  }
  return false;
}

bool
CSSParserImpl::ParseTransitionStepTimingFunctionValues(nsCSSValue& aValue)
{
  NS_ASSERTION(!mHavePushBack &&
               mToken.mType == eCSSToken_Function &&
               mToken.mIdent.LowerCaseEqualsLiteral("steps"),
               "unexpected initial state");

  RefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(2);

  if (!ParseSingleTokenOneOrLargerVariant(val->Item(0), VARIANT_INTEGER,
                                          nullptr)) {
    return false;
  }

  int32_t type = -1;  // indicates an implicit end value
  if (ExpectSymbol(',', true)) {
    if (!GetToken(true)) {
      return false;
    }
    if (mToken.mType == eCSSToken_Ident) {
      if (mToken.mIdent.LowerCaseEqualsLiteral("start")) {
        type = NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START;
      } else if (mToken.mIdent.LowerCaseEqualsLiteral("end")) {
        type = NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END;
      }
    }
    if (type == -1) {
      UngetToken();
      return false;
    }
  }
  val->Item(1).SetIntValue(type, eCSSUnit_Enumerated);

  if (!ExpectSymbol(')', true)) {
    return false;
  }

  aValue.SetArrayValue(val, eCSSUnit_Steps);
  return true;
}

bool
CSSParserImpl::ParseTransitionFramesTimingFunctionValues(nsCSSValue& aValue)
{
  NS_ASSERTION(!mHavePushBack &&
               mToken.mType == eCSSToken_Function &&
               mToken.mIdent.LowerCaseEqualsLiteral("frames"),
               "unexpected initial state");

  nsCSSKeyword functionName = nsCSSKeywords::LookupKeyword(mToken.mIdent);
  MOZ_ASSERT(functionName == eCSSKeyword_frames);

  nsCSSValue frameNumber;
  if (!ParseSingleTokenOneOrLargerVariant(frameNumber, VARIANT_INTEGER,
                                          nullptr)) {
    return false;
  }
  MOZ_ASSERT(frameNumber.GetIntValue() >= 1,
             "Parsing function should've enforced OneOrLarger, per its name");

  // The number of frames must be a positive integer greater than one.
  if (frameNumber.GetIntValue() == 1) {
    return false;
  }

  if (!ExpectSymbol(')', true)) {
    return false;
  }

  RefPtr<nsCSSValue::Array> val = aValue.InitFunction(functionName, 1);
  val->Item(1) = frameNumber;
  return true;
}

static nsCSSValueList*
AppendValueToList(nsCSSValue& aContainer,
                  nsCSSValueList* aTail,
                  const nsCSSValue& aValue)
{
  nsCSSValueList* entry;
  if (aContainer.GetUnit() == eCSSUnit_Null) {
    MOZ_ASSERT(!aTail, "should not have an entry");
    entry = aContainer.SetListValue();
  } else {
    MOZ_ASSERT(!aTail->mNext, "should not have a next entry");
    MOZ_ASSERT(aContainer.GetUnit() == eCSSUnit_List, "not a list");
    entry = new nsCSSValueList;
    aTail->mNext = entry;
  }
  entry->mValue = aValue;
  return entry;
}

CSSParserImpl::ParseAnimationOrTransitionShorthandResult
CSSParserImpl::ParseAnimationOrTransitionShorthand(
                 const nsCSSPropertyID* aProperties,
                 const nsCSSValue* aInitialValues,
                 nsCSSValue* aValues,
                 size_t aNumProperties)
{
  nsCSSValue tempValue;
  // first see if 'inherit', 'initial' or 'unset' is specified.  If one is,
  // it can be the only thing specified, so don't attempt to parse any
  // additional properties
  if (ParseSingleTokenVariant(tempValue, VARIANT_INHERIT, nullptr)) {
    for (uint32_t i = 0; i < aNumProperties; ++i) {
      AppendValue(aProperties[i], tempValue);
    }
    return eParseAnimationOrTransitionShorthand_Inherit;
  }

  static const size_t maxNumProperties = 8;
  MOZ_ASSERT(aNumProperties <= maxNumProperties,
             "can't handle this many properties");
  nsCSSValueList *cur[maxNumProperties];
  bool parsedProperty[maxNumProperties];

  for (size_t i = 0; i < aNumProperties; ++i) {
    cur[i] = nullptr;
  }
  bool atEOP = false; // at end of property?
  for (;;) { // loop over comma-separated transitions or animations
    // whether a particular subproperty was specified for this
    // transition or animation
    bool haveAnyProperty = false;
    for (size_t i = 0; i < aNumProperties; ++i) {
      parsedProperty[i] = false;
    }
    for (;;) { // loop over values within a transition or animation
      bool foundProperty = false;
      // check to see if we're at the end of one full transition or
      // animation definition (either because we hit a comma or because
      // we hit the end of the property definition)
      if (ExpectSymbol(',', true))
        break;
      if (CheckEndProperty()) {
        atEOP = true;
        break;
      }

      // else, try to parse the next transition or animation sub-property
      for (uint32_t i = 0; !foundProperty && i < aNumProperties; ++i) {
        if (!parsedProperty[i]) {
          // if we haven't found this property yet, try to parse it
          CSSParseResult result =
            ParseSingleValueProperty(tempValue, aProperties[i]);
          if (result == CSSParseResult::Error) {
            return eParseAnimationOrTransitionShorthand_Error;
          }
          if (result == CSSParseResult::Ok) {
            parsedProperty[i] = true;
            cur[i] = AppendValueToList(aValues[i], cur[i], tempValue);
            foundProperty = true;
            haveAnyProperty = true;
            break; // out of inner loop; continue looking for next sub-property
          }
        }
      }
      if (!foundProperty) {
        // We're not at a ',' or at the end of the property, but we couldn't
        // parse any of the sub-properties, so the declaration is invalid.
        return eParseAnimationOrTransitionShorthand_Error;
      }
    }

    if (!haveAnyProperty) {
      // Got an empty item.
      return eParseAnimationOrTransitionShorthand_Error;
    }

    // We hit the end of the property or the end of one transition
    // or animation definition, add its components to the list.
    for (uint32_t i = 0; i < aNumProperties; ++i) {
      // If all of the subproperties were not explicitly specified, fill
      // in the missing ones with initial values.
      if (!parsedProperty[i]) {
        cur[i] = AppendValueToList(aValues[i], cur[i], aInitialValues[i]);
      }
    }

    if (atEOP)
      break;
    // else we just hit a ',' so continue parsing the next compound transition
  }

  return eParseAnimationOrTransitionShorthand_Values;
}

bool
CSSParserImpl::ParseTransition()
{
  static const nsCSSPropertyID kTransitionProperties[] = {
    eCSSProperty_transition_duration,
    eCSSProperty_transition_timing_function,
    // Must check 'transition-delay' after 'transition-duration', since
    // that's our assumption about what the spec means for the shorthand
    // syntax (the first time given is the duration, and the second
    // given is the delay).
    eCSSProperty_transition_delay,
    // Must check 'transition-property' after
    // 'transition-timing-function' since 'transition-property' accepts
    // any keyword.
    eCSSProperty_transition_property
  };
  static const uint32_t numProps = MOZ_ARRAY_LENGTH(kTransitionProperties);
  // this is a shorthand property that accepts -property, -delay,
  // -duration, and -timing-function with some components missing.
  // there can be multiple transitions, separated with commas

  nsCSSValue initialValues[numProps];
  initialValues[0].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[1].SetIntValue(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE,
                               eCSSUnit_Enumerated);
  initialValues[2].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[3].SetAllValue();

  nsCSSValue values[numProps];

  ParseAnimationOrTransitionShorthandResult spres =
    ParseAnimationOrTransitionShorthand(kTransitionProperties,
                                        initialValues, values, numProps);
  if (spres != eParseAnimationOrTransitionShorthand_Values) {
    return spres != eParseAnimationOrTransitionShorthand_Error;
  }

  // Make two checks on the list for 'transition-property':
  //   + If there is more than one item, then none of the items can be
  //     'none'.
  //   + None of the items can be 'inherit', 'initial' or 'unset'.
  {
    MOZ_ASSERT(kTransitionProperties[3] == eCSSProperty_transition_property,
               "array index mismatch");
    nsCSSValueList *l = values[3].GetListValue();
    bool multipleItems = !!l->mNext;
    do {
      const nsCSSValue& val = l->mValue;
      if (val.GetUnit() == eCSSUnit_None) {
        if (multipleItems) {
          // This is a syntax error.
          return false;
        }

        // Unbox a solitary 'none'.
        values[3].SetNoneValue();
        break;
      }
      if (val.GetUnit() == eCSSUnit_Ident) {
        nsDependentString str(val.GetStringBufferValue());
        if (str.EqualsLiteral("inherit") ||
            str.EqualsLiteral("initial") ||
            (str.EqualsLiteral("unset") &&
             nsLayoutUtils::UnsetValueEnabled())) {
          return false;
        }
      }
    } while ((l = l->mNext));
  }

  // Save all parsed transition sub-properties in mTempData
  for (uint32_t i = 0; i < numProps; ++i) {
    AppendValue(kTransitionProperties[i], values[i]);
  }
  return true;
}

bool
CSSParserImpl::ParseAnimation()
{
  static const nsCSSPropertyID kAnimationProperties[] = {
    eCSSProperty_animation_duration,
    eCSSProperty_animation_timing_function,
    // Must check 'animation-delay' after 'animation-duration', since
    // that's our assumption about what the spec means for the shorthand
    // syntax (the first time given is the duration, and the second
    // given is the delay).
    eCSSProperty_animation_delay,
    eCSSProperty_animation_direction,
    eCSSProperty_animation_fill_mode,
    eCSSProperty_animation_iteration_count,
    eCSSProperty_animation_play_state,
    // Must check 'animation-name' after 'animation-timing-function',
    // 'animation-direction', 'animation-fill-mode',
    // 'animation-iteration-count', and 'animation-play-state' since
    // 'animation-name' accepts any keyword.
    eCSSProperty_animation_name
  };
  static const uint32_t numProps = MOZ_ARRAY_LENGTH(kAnimationProperties);
  // this is a shorthand property that accepts -property, -delay,
  // -duration, and -timing-function with some components missing.
  // there can be multiple animations, separated with commas

  nsCSSValue initialValues[numProps];
  initialValues[0].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[1].SetIntValue(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE,
                               eCSSUnit_Enumerated);
  initialValues[2].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[3].SetIntValue(static_cast<int32_t>(dom::PlaybackDirection::Normal),
                               eCSSUnit_Enumerated);
  initialValues[4].SetIntValue(static_cast<int32_t>(dom::FillMode::None),
                               eCSSUnit_Enumerated);
  initialValues[5].SetFloatValue(1.0f, eCSSUnit_Number);
  initialValues[6].SetIntValue(NS_STYLE_ANIMATION_PLAY_STATE_RUNNING, eCSSUnit_Enumerated);
  initialValues[7].SetNoneValue();

  nsCSSValue values[numProps];

  ParseAnimationOrTransitionShorthandResult spres =
    ParseAnimationOrTransitionShorthand(kAnimationProperties,
                                        initialValues, values, numProps);
  if (spres != eParseAnimationOrTransitionShorthand_Values) {
    return spres != eParseAnimationOrTransitionShorthand_Error;
  }

  // Save all parsed animation sub-properties in mTempData
  for (uint32_t i = 0; i < numProps; ++i) {
    AppendValue(kAnimationProperties[i], values[i]);
  }
  return true;
}

bool
CSSParserImpl::ParseShadowItem(nsCSSValue& aValue, bool aIsBoxShadow)
{
  // A shadow list item is an array, with entries in this sequence:
  enum {
    IndexX,
    IndexY,
    IndexRadius,
    IndexSpread,  // only for box-shadow
    IndexColor,
    IndexInset    // only for box-shadow
  };

  RefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(6);

  if (aIsBoxShadow) {
    // Optional inset keyword (ignore errors)
    Unused << ParseSingleTokenVariant(val->Item(IndexInset), VARIANT_KEYWORD,
                                      nsCSSProps::kBoxShadowTypeKTable);
  }

  nsCSSValue xOrColor;
  bool haveColor = false;
  if (ParseVariant(xOrColor, VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC,
                   nullptr) != CSSParseResult::Ok) {
    return false;
  }
  if (xOrColor.IsLengthUnit() || xOrColor.IsCalcUnit()) {
    val->Item(IndexX) = xOrColor;
  } else {
    // Must be a color (as string or color value)
    NS_ASSERTION(xOrColor.GetUnit() == eCSSUnit_Ident ||
                 xOrColor.GetUnit() == eCSSUnit_EnumColor ||
                 xOrColor.IsNumericColorUnit(),
                 "Must be a color value");
    val->Item(IndexColor) = xOrColor;
    haveColor = true;

    // X coordinate mandatory after color
    if (ParseVariant(val->Item(IndexX), VARIANT_LENGTH | VARIANT_CALC,
                     nullptr) != CSSParseResult::Ok) {
      return false;
    }
  }

  // Y coordinate; mandatory
  if (ParseVariant(val->Item(IndexY), VARIANT_LENGTH | VARIANT_CALC,
                   nullptr) != CSSParseResult::Ok) {
    return false;
  }

  // Optional radius. Ignore errors except if they pass a negative
  // value which we must reject. If we use ParseNonNegativeVariant
  // we can't tell the difference between an unspecified radius
  // and a negative radius.
  CSSParseResult result =
    ParseVariant(val->Item(IndexRadius), VARIANT_LENGTH | VARIANT_CALC,
                 nullptr);
  if (result == CSSParseResult::Error) {
    return false;
  } else if (result == CSSParseResult::Ok) {
    if (val->Item(IndexRadius).IsLengthUnit() &&
        val->Item(IndexRadius).GetFloatValue() < 0) {
      return false;
    }
  }

  if (aIsBoxShadow) {
    // Optional spread
    if (ParseVariant(val->Item(IndexSpread), VARIANT_LENGTH | VARIANT_CALC,
                     nullptr) == CSSParseResult::Error) {
      return false;
    }
  }

  if (!haveColor) {
    // Optional color
    if (ParseVariant(val->Item(IndexColor), VARIANT_COLOR, nullptr) ==
        CSSParseResult::Error) {
      return false;
    }
  }

  if (aIsBoxShadow && val->Item(IndexInset).GetUnit() == eCSSUnit_Null) {
    // Optional inset keyword (ignore errors)
    Unused << ParseSingleTokenVariant(val->Item(IndexInset), VARIANT_KEYWORD,
                                      nsCSSProps::kBoxShadowTypeKTable);
  }

  aValue.SetArrayValue(val, eCSSUnit_Array);
  return true;
}

bool
CSSParserImpl::ParseShadowList(nsCSSPropertyID aProperty)
{
  nsAutoParseCompoundProperty compound(this);
  bool isBoxShadow = aProperty == eCSSProperty_box_shadow;

  nsCSSValue value;
  // 'inherit', 'initial', 'unset' and 'none' must be alone
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE,
                               nullptr)) {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseShadowItem(cur->mValue, isBoxShadow)) {
        return false;
      }
      if (!ExpectSymbol(',', true)) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(aProperty, value);
  return true;
}

int32_t
CSSParserImpl::GetNamespaceIdForPrefix(const nsString& aPrefix)
{
  NS_PRECONDITION(!aPrefix.IsEmpty(), "Must have a prefix here");

  int32_t nameSpaceID = kNameSpaceID_Unknown;
  if (mNameSpaceMap) {
    // user-specified identifiers are case-sensitive (bug 416106)
    RefPtr<nsAtom> prefix = NS_Atomize(aPrefix);
    nameSpaceID = mNameSpaceMap->FindNameSpaceID(prefix);
  }
  // else no declared namespaces

  if (nameSpaceID == kNameSpaceID_Unknown) {   // unknown prefix, dump it
    REPORT_UNEXPECTED_P(PEUnknownNamespacePrefix, aPrefix);
  }

  return nameSpaceID;
}

void
CSSParserImpl::SetDefaultNamespaceOnSelector(nsCSSSelector& aSelector)
{
  if (mNameSpaceMap) {
    aSelector.SetNameSpace(mNameSpaceMap->FindNameSpaceID(nullptr));
  } else {
    aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
  }
}

bool
CSSParserImpl::ParsePaint(nsCSSPropertyID aPropID)
{
  nsCSSValue x, y;

  if (ParseVariant(x, VARIANT_HC | VARIANT_NONE | VARIANT_URL | VARIANT_KEYWORD,
                   nsCSSProps::kContextPatternKTable) != CSSParseResult::Ok) {
    return false;
  }

  bool hasFallback = false;
  bool canHaveFallback = x.GetUnit() == eCSSUnit_URL ||
                         x.GetUnit() == eCSSUnit_Enumerated;
  if (canHaveFallback) {
    CSSParseResult result =
      ParseVariant(y, VARIANT_COLOR | VARIANT_NONE, nullptr);
    if (result == CSSParseResult::Error) {
      return false;
    }
    hasFallback = (result != CSSParseResult::NotFound);
  }

  if (hasFallback) {
    nsCSSValue val;
    val.SetPairValue(x, y);
    AppendValue(aPropID, val);
  } else {
    AppendValue(aPropID, x);
  }
  return true;
}

bool
CSSParserImpl::ParseDasharray()
{
  nsCSSValue value;

  // 'inherit', 'initial', 'unset' and 'none' are only allowed on their own
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT | VARIANT_NONE |
                                      VARIANT_OPENTYPE_SVG_KEYWORD,
                               nsCSSProps::kStrokeContextValueKTable)) {
    nsCSSValueList *cur = value.SetListValue();
    for (;;) {
      if (!ParseSingleTokenNonNegativeVariant(cur->mValue, VARIANT_LPN,
                                              nullptr)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      // skip optional commas between elements
      (void)ExpectSymbol(',', true);

      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_stroke_dasharray, value);
  return true;
}

bool
CSSParserImpl::ParseMarker()
{
  nsCSSValue marker;
  if (ParseSingleValueProperty(marker, eCSSProperty_marker_end) ==
      CSSParseResult::Ok) {
    AppendValue(eCSSProperty_marker_end, marker);
    AppendValue(eCSSProperty_marker_mid, marker);
    AppendValue(eCSSProperty_marker_start, marker);
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParsePaintOrder()
{
  static_assert
    ((1 << NS_STYLE_PAINT_ORDER_BITWIDTH) > NS_STYLE_PAINT_ORDER_LAST_VALUE,
     "bitfield width insufficient for paint-order constants");

  static const KTableEntry kPaintOrderKTable[] = {
    { eCSSKeyword_normal,  NS_STYLE_PAINT_ORDER_NORMAL },
    { eCSSKeyword_fill,    NS_STYLE_PAINT_ORDER_FILL },
    { eCSSKeyword_stroke,  NS_STYLE_PAINT_ORDER_STROKE },
    { eCSSKeyword_markers, NS_STYLE_PAINT_ORDER_MARKERS },
    { eCSSKeyword_UNKNOWN, -1 }
  };

  static_assert(MOZ_ARRAY_LENGTH(kPaintOrderKTable) ==
                  NS_STYLE_PAINT_ORDER_LAST_VALUE + 2,
                "missing paint-order values in kPaintOrderKTable");

  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_HK, kPaintOrderKTable)) {
    return false;
  }

  uint32_t seen = 0;
  uint32_t order = 0;
  uint32_t position = 0;

  // Ensure that even cast to a signed int32_t when stored in CSSValue,
  // we have enough space for the entire paint-order value.
  static_assert
    (NS_STYLE_PAINT_ORDER_BITWIDTH * NS_STYLE_PAINT_ORDER_LAST_VALUE < 32,
     "seen and order not big enough");

  if (value.GetUnit() == eCSSUnit_Enumerated) {
    uint32_t component = static_cast<uint32_t>(value.GetIntValue());
    if (component != NS_STYLE_PAINT_ORDER_NORMAL) {
      bool parsedOK = true;
      for (;;) {
        if (seen & (1 << component)) {
          // Already seen this component.
          UngetToken();
          parsedOK = false;
          break;
        }
        seen |= (1 << component);
        order |= (component << position);
        position += NS_STYLE_PAINT_ORDER_BITWIDTH;
        if (!ParseEnum(value, kPaintOrderKTable)) {
          break;
        }
        component = value.GetIntValue();
        if (component == NS_STYLE_PAINT_ORDER_NORMAL) {
          // Can't have "normal" in the middle of the list of paint components.
          UngetToken();
          parsedOK = false;
          break;
        }
      }

      // Fill in the remaining paint-order components in the order of their
      // constant values.
      if (parsedOK) {
        for (component = 1;
             component <= NS_STYLE_PAINT_ORDER_LAST_VALUE;
             component++) {
          if (!(seen & (1 << component))) {
            order |= (component << position);
            position += NS_STYLE_PAINT_ORDER_BITWIDTH;
          }
        }
      }
    }

    static_assert(NS_STYLE_PAINT_ORDER_NORMAL == 0,
                  "unexpected value for NS_STYLE_PAINT_ORDER_NORMAL");
    value.SetIntValue(static_cast<int32_t>(order), eCSSUnit_Enumerated);
  }

  AppendValue(eCSSProperty_paint_order, value);
  return true;
}

bool
CSSParserImpl::BackslashDropped()
{
  return mScanner->GetEOFCharacters() &
         nsCSSScanner::eEOFCharacters_DropBackslash;
}

void
CSSParserImpl::AppendImpliedEOFCharacters(nsAString& aResult)
{
  nsCSSScanner::AppendImpliedEOFCharacters(mScanner->GetEOFCharacters(),
                                           aResult);
}

bool
CSSParserImpl::ParseAll()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_INHERIT, nullptr)) {
    return false;
  }

  // It's unlikely we'll want to use 'all' from within a UA style sheet, so
  // instead of computing the correct EnabledState value we just expand out
  // to all content-visible properties.
  CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, eCSSProperty_all,
                                       CSSEnabledState::eForAllContent) {
    AppendValue(*p, value);
  }
  return true;
}

bool
CSSParserImpl::ParseVariableDeclaration(CSSVariableDeclarations::Type* aType,
                                        nsString& aValue)
{
  CSSVariableDeclarations::Type type;
  nsString variableValue;
  bool dropBackslash;
  nsString impliedCharacters;

  // Record the token stream while parsing a variable value.
  if (!mInSupportsCondition) {
    mScanner->StartRecording();
  }
  if (!ParseValueWithVariables(&type, &dropBackslash, impliedCharacters,
                               nullptr, nullptr)) {
    if (!mInSupportsCondition) {
      mScanner->StopRecording();
    }
    return false;
  }

  if (!mInSupportsCondition) {
    if (type == CSSVariableDeclarations::eTokenStream) {
      // This was indeed a token stream value, so store it in variableValue.
      mScanner->StopRecording(variableValue);
      if (dropBackslash) {
        MOZ_ASSERT(!variableValue.IsEmpty() &&
                   variableValue[variableValue.Length() - 1] == '\\');
        variableValue.Truncate(variableValue.Length() - 1);
      }
      variableValue.Append(impliedCharacters);
    } else {
      // This was either 'inherit' or 'initial'; we don't need the recorded
      // input.
      mScanner->StopRecording();
    }
  }

  if (mHavePushBack && type == CSSVariableDeclarations::eTokenStream) {
    // If we came to the end of a valid variable declaration and a token was
    // pushed back, then it would have been ended by '!', ')', ';', ']' or '}'.
    // We need to remove it from the recorded variable value.
    MOZ_ASSERT(mToken.IsSymbol('!') ||
               mToken.IsSymbol(')') ||
               mToken.IsSymbol(';') ||
               mToken.IsSymbol(']') ||
               mToken.IsSymbol('}'));
    if (!mInSupportsCondition) {
      MOZ_ASSERT(!variableValue.IsEmpty());
      MOZ_ASSERT(variableValue[variableValue.Length() - 1] == mToken.mSymbol);
      variableValue.Truncate(variableValue.Length() - 1);
    }
  }

  *aType = type;
  aValue = variableValue;
  return true;
}

bool
CSSParserImpl::ParseOverscrollBehavior()
{
  static const nsCSSPropertyID ids[] = {
    eCSSProperty_overscroll_behavior_x,
    eCSSProperty_overscroll_behavior_y
  };
  const int32_t numProps = MOZ_ARRAY_LENGTH(ids);

  nsCSSValue values[numProps];
  int32_t found = ParseChoice(values, ids, numProps);
  if (found < 1) {
    return false;
  }

  // If only one value is specified, it's used for both axes.
  if (found == 1) {
    values[1] = values[0];
  }

  AppendValue(eCSSProperty_overscroll_behavior_x, values[0]);
  AppendValue(eCSSProperty_overscroll_behavior_y, values[1]);
  return true;
}

bool
CSSParserImpl::ParseScrollSnapType()
{
  nsCSSValue value;
  if (!ParseSingleTokenVariant(value, VARIANT_HK,
                               nsCSSProps::kScrollSnapTypeKTable)) {
    return false;
  }
  AppendValue(eCSSProperty_scroll_snap_type_x, value);
  AppendValue(eCSSProperty_scroll_snap_type_y, value);
  return true;
}

bool
CSSParserImpl::ParseScrollSnapPoints(nsCSSValue& aValue, nsCSSPropertyID aPropID)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NONE,
                              nullptr)) {
    return true;
  }
  if (!GetToken(true)) {
    return false;
  }
  if (mToken.mType == eCSSToken_Function &&
      nsCSSKeywords::LookupKeyword(mToken.mIdent) == eCSSKeyword_repeat) {
    nsCSSValue lengthValue;
    if (ParseNonNegativeVariant(lengthValue,
                                VARIANT_LENGTH | VARIANT_PERCENT | VARIANT_CALC,
                                nullptr) != CSSParseResult::Ok) {
      REPORT_UNEXPECTED(PEExpectedNonnegativeNP);
      SkipUntil(')');
      return false;
    }
    if (!ExpectSymbol(')', true)) {
      REPORT_UNEXPECTED(PEExpectedCloseParen);
      SkipUntil(')');
      return false;
    }
    RefPtr<nsCSSValue::Array> functionArray =
      aValue.InitFunction(eCSSKeyword_repeat, 1);
    functionArray->Item(1) = lengthValue;
    return true;
  }
  UngetToken();
  return false;
}


bool
CSSParserImpl::ParseScrollSnapDestination(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT, nullptr)) {
    return true;
  }
  nsCSSValue itemValue;
  if (!ParsePositionValue(aValue)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPosition);
    return false;
  }
  return true;
}

// This function is very similar to ParseImageLayerPosition, and ParseImageLayerSize.
bool
CSSParserImpl::ParseScrollSnapCoordinate(nsCSSValue& aValue)
{
  if (ParseSingleTokenVariant(aValue, VARIANT_INHERIT | VARIANT_NONE,
                              nullptr)) {
    return true;
  }
  nsCSSValue itemValue;
  if (!ParsePositionValue(itemValue)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPosition);
    return false;
  }
  nsCSSValueList* item = aValue.SetListValue();
  for (;;) {
    item->mValue = itemValue;
    if (!ExpectSymbol(',', true)) {
      break;
    }
    if (!ParsePositionValue(itemValue)) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedPosition);
      return false;
    }
    item->mNext = new nsCSSValueList;
    item = item->mNext;
  }
  return true;
}

bool
CSSParserImpl::ParseValueWithVariables(CSSVariableDeclarations::Type* aType,
                                       bool* aDropBackslash,
                                       nsString& aImpliedCharacters,
                                       void (*aFunc)(const nsAString&, void*),
                                       void* aData)
{
  // A property value is invalid if it contains variable references and also:
  //
  //   * has unbalanced parens, brackets or braces
  //   * has any BAD_STRING or BAD_URL tokens
  //   * has any ';' or '!' tokens at the top level of a variable reference's
  //     fallback
  //
  // If the property is a custom property (i.e. a variable declaration), then
  // it is also invalid if it consists of no tokens, such as:
  //
  //   --invalid:;
  //
  // Note that is valid for a custom property to have a value that consists
  // solely of white space, such as:
  //
  //   --valid: ;

  // Stack of closing characters for currently open constructs.
  StopSymbolCharStack stack;

  // Indexes into ')' characters in |stack| that correspond to "var(".  This
  // is used to stop parsing when we encounter a '!' or ';' at the top level
  // of a variable reference's fallback.
  AutoTArray<uint32_t, 16> references;

  if (!GetToken(false)) {
    // Variable value was empty since we reached EOF.
    REPORT_UNEXPECTED_EOF(PEVariableEOF);
    return false;
  }

  if (mToken.mType == eCSSToken_Symbol &&
      (mToken.mSymbol == '!' ||
       mToken.mSymbol == ')' ||
       mToken.mSymbol == ';' ||
       mToken.mSymbol == ']' ||
       mToken.mSymbol == '}')) {
    // Variable value was empty since we reached the end of the construct.
    UngetToken();
    REPORT_UNEXPECTED_TOKEN(PEVariableEmpty);
    return false;
  }

  if (mToken.mType == eCSSToken_Whitespace) {
    if (!GetToken(true)) {
      // Variable value was white space only.  This is valid.
      MOZ_ASSERT(!BackslashDropped());
      *aType = CSSVariableDeclarations::eTokenStream;
      *aDropBackslash = false;
      AppendImpliedEOFCharacters(aImpliedCharacters);
      return true;
    }
  }

  // Look for 'initial', 'inherit' or 'unset' as the first non-white space
  // token.
  CSSVariableDeclarations::Type type = CSSVariableDeclarations::eTokenStream;
  if (mToken.mType == eCSSToken_Ident) {
    if (mToken.mIdent.LowerCaseEqualsLiteral("initial")) {
      type = CSSVariableDeclarations::eInitial;
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("inherit")) {
      type = CSSVariableDeclarations::eInherit;
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("unset")) {
      type = CSSVariableDeclarations::eUnset;
    }
  }

  if (type != CSSVariableDeclarations::eTokenStream) {
    if (!GetToken(true)) {
      // Variable value was 'initial' or 'inherit' followed by EOF.
      MOZ_ASSERT(!BackslashDropped());
      *aType = type;
      *aDropBackslash = false;
      AppendImpliedEOFCharacters(aImpliedCharacters);
      return true;
    }
    UngetToken();
    if (mToken.mType == eCSSToken_Symbol &&
        (mToken.mSymbol == '!' ||
         mToken.mSymbol == ')' ||
         mToken.mSymbol == ';' ||
         mToken.mSymbol == ']' ||
         mToken.mSymbol == '}')) {
      // Variable value was 'initial' or 'inherit' followed by the end
      // of the declaration.
      MOZ_ASSERT(!BackslashDropped());
      *aType = type;
      *aDropBackslash = false;
      return true;
    }
  }

  do {
    switch (mToken.mType) {
      case eCSSToken_Symbol:
        if (mToken.mSymbol == '(') {
          stack.AppendElement(')');
        } else if (mToken.mSymbol == '[') {
          stack.AppendElement(']');
        } else if (mToken.mSymbol == '{') {
          stack.AppendElement('}');
        } else if (mToken.mSymbol == ';' ||
                   mToken.mSymbol == '!') {
          if (stack.IsEmpty()) {
            UngetToken();
            MOZ_ASSERT(!BackslashDropped());
            *aType = CSSVariableDeclarations::eTokenStream;
            *aDropBackslash = false;
            return true;
          } else if (!references.IsEmpty() &&
                     references.LastElement() == stack.Length() - 1) {
            REPORT_UNEXPECTED_TOKEN(PEInvalidVariableTokenFallback);
            SkipUntilAllOf(stack);
            return false;
          }
        } else if (mToken.mSymbol == ')' ||
                   mToken.mSymbol == ']' ||
                   mToken.mSymbol == '}') {
          for (;;) {
            if (stack.IsEmpty()) {
              UngetToken();
              MOZ_ASSERT(!BackslashDropped());
              *aType = CSSVariableDeclarations::eTokenStream;
              *aDropBackslash = false;
              return true;
            }
            char16_t c = stack.LastElement();
            stack.TruncateLength(stack.Length() - 1);
            if (!references.IsEmpty() &&
                references.LastElement() == stack.Length()) {
              references.TruncateLength(references.Length() - 1);
            }
            if (mToken.mSymbol == c) {
              break;
            }
          }
        }
        break;

      case eCSSToken_Function:
        if (mToken.mIdent.LowerCaseEqualsLiteral("var")) {
          if (!GetToken(true)) {
            // EOF directly after "var(".
            REPORT_UNEXPECTED_EOF(PEExpectedVariableNameEOF);
            return false;
          }
          if (mToken.mType != eCSSToken_Ident ||
              !nsCSSProps::IsCustomPropertyName(mToken.mIdent)) {
            // There must be an identifier directly after the "var(" and
            // it must be a custom property name.
            UngetToken();
            REPORT_UNEXPECTED_TOKEN(PEExpectedVariableName);
            SkipUntil(')');
            SkipUntilAllOf(stack);
            return false;
          }
          if (aFunc) {
            MOZ_ASSERT(Substring(mToken.mIdent, 0,
                                 CSS_CUSTOM_NAME_PREFIX_LENGTH).
                         EqualsLiteral("--"));
            // remove '--'
            const nsDependentSubstring varName =
              Substring(mToken.mIdent, CSS_CUSTOM_NAME_PREFIX_LENGTH);
            aFunc(varName, aData);
          }
          if (!GetToken(true)) {
            // EOF right after "var(<ident>".
            stack.AppendElement(')');
          } else if (mToken.IsSymbol(',')) {
            // Variable reference with fallback.
            if (!GetToken(false) || mToken.IsSymbol(')')) {
              // Comma must be followed by at least one fallback token.
              REPORT_UNEXPECTED(PEExpectedVariableFallback);
              SkipUntilAllOf(stack);
              return false;
            }
            UngetToken();
            references.AppendElement(stack.Length());
            stack.AppendElement(')');
          } else if (mToken.IsSymbol(')')) {
            // Correctly closed variable reference.
          } else {
            // Malformed variable reference.
            REPORT_UNEXPECTED_TOKEN(PEExpectedVariableCommaOrCloseParen);
            SkipUntil(')');
            SkipUntilAllOf(stack);
            return false;
          }
        } else {
          stack.AppendElement(')');
        }
        break;

      case eCSSToken_Bad_String:
        SkipUntilAllOf(stack);
        return false;

      case eCSSToken_Bad_URL:
        SkipUntil(')');
        SkipUntilAllOf(stack);
        return false;

      default:
        break;
    }
  } while (GetToken(true));

  // Append any implied closing characters.
  *aDropBackslash = BackslashDropped();
  AppendImpliedEOFCharacters(aImpliedCharacters);
  uint32_t i = stack.Length();
  while (i--) {
    aImpliedCharacters.Append(stack[i]);
  }

  *aType = type;
  return true;
}

bool
CSSParserImpl::IsValueValidForProperty(const nsCSSPropertyID aPropID,
                                       const nsAString& aPropValue)
{
  mData.AssertInitialState();
  mTempData.AssertInitialState();

  nsCSSScanner scanner(aPropValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, nullptr);
  InitScanner(scanner, reporter, nullptr, nullptr, nullptr);

  // We normally would need to pass in a sheet principal to InitScanner,
  // because we might parse a URL value.  However, we will never use the
  // parsed nsCSSValue (and so whether we have a sheet principal or not
  // doesn't really matter), so to avoid failing the assertion in
  // SetValueToURL, we set mSheetPrincipalRequired to false to declare
  // that it's safe to skip the assertion.
  AutoRestore<bool> autoRestore(mSheetPrincipalRequired);
  mSheetPrincipalRequired = false;

  nsAutoSuppressErrors suppressErrors(this);

  mSection = eCSSSection_General;

  // Check for unknown properties
  if (eCSSProperty_UNKNOWN == aPropID) {
    ReleaseScanner();
    return false;
  }

  // Check that the property and value parse successfully
  bool parsedOK = ParseProperty(aPropID);

  // Check for priority
  parsedOK = parsedOK && ParsePriority() != ePriority_Error;

  // We should now be at EOF
  parsedOK = parsedOK && !GetToken(true);

  mTempData.ClearProperty(aPropID);
  mTempData.AssertInitialState();
  mData.AssertInitialState();

  CLEAR_ERROR();
  ReleaseScanner();

  return parsedOK;
}

} // namespace

// Recycling of parser implementation objects

static CSSParserImpl* gFreeList = nullptr;

nsCSSParser::nsCSSParser(mozilla::css::Loader* aLoader,
                         CSSStyleSheet* aSheet)
{
  CSSParserImpl *impl = gFreeList;
  if (impl) {
    gFreeList = impl->mNextFree;
    impl->mNextFree = nullptr;
  } else {
    impl = new CSSParserImpl();
  }

  if (aLoader) {
    impl->SetChildLoader(aLoader);
    impl->SetQuirkMode(aLoader->GetCompatibilityMode() ==
                       eCompatibility_NavQuirks);
  }
  if (aSheet) {
    impl->SetStyleSheet(aSheet);
  }

  mImpl = static_cast<void*>(impl);
}

nsCSSParser::~nsCSSParser()
{
  CSSParserImpl *impl = static_cast<CSSParserImpl*>(mImpl);
  impl->Reset();
  impl->mNextFree = gFreeList;
  gFreeList = impl;
}

/* static */ void
nsCSSParser::Shutdown()
{
  CSSParserImpl *tofree = gFreeList;
  CSSParserImpl *next;
  while (tofree)
    {
      next = tofree->mNextFree;
      delete tofree;
      tofree = next;
    }
}

// Wrapper methods

nsresult
nsCSSParser::ParseSheet(const nsAString& aInput,
                        nsIURI*          aSheetURI,
                        nsIURI*          aBaseURI,
                        nsIPrincipal*    aSheetPrincipal,
                        uint32_t         aLineNumber,
                        css::LoaderReusableStyleSheets* aReusableSheets)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseSheet(aInput, aSheetURI, aBaseURI, aSheetPrincipal, aLineNumber,
               aReusableSheets);
}

already_AddRefed<css::Declaration>
nsCSSParser::ParseStyleAttribute(const nsAString&  aAttributeValue,
                                 nsIURI*           aDocURI,
                                 nsIURI*           aBaseURI,
                                 nsIPrincipal*     aNodePrincipal)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseStyleAttribute(aAttributeValue, aDocURI, aBaseURI, aNodePrincipal);
}

nsresult
nsCSSParser::ParseDeclarations(const nsAString&  aBuffer,
                               nsIURI*           aSheetURI,
                               nsIURI*           aBaseURI,
                               nsIPrincipal*     aSheetPrincipal,
                               css::Declaration* aDeclaration,
                               bool*           aChanged)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseDeclarations(aBuffer, aSheetURI, aBaseURI, aSheetPrincipal,
                      aDeclaration, aChanged);
}

nsresult
nsCSSParser::ParseRule(const nsAString&        aRule,
                       nsIURI*                 aSheetURI,
                       nsIURI*                 aBaseURI,
                       nsIPrincipal*           aSheetPrincipal,
                       css::Rule**             aResult)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseRule(aRule, aSheetURI, aBaseURI, aSheetPrincipal, aResult);
}

void
nsCSSParser::ParseProperty(const nsCSSPropertyID aPropID,
                           const nsAString&    aPropValue,
                           nsIURI*             aSheetURI,
                           nsIURI*             aBaseURI,
                           nsIPrincipal*       aSheetPrincipal,
                           css::Declaration*   aDeclaration,
                           bool*               aChanged,
                           bool                aIsImportant,
                           bool                aIsSVGMode)
{
  static_cast<CSSParserImpl*>(mImpl)->
    ParseProperty(aPropID, aPropValue, aSheetURI, aBaseURI,
                  aSheetPrincipal, aDeclaration, aChanged,
                  aIsImportant, aIsSVGMode);
}

void
nsCSSParser::ParseLonghandProperty(const nsCSSPropertyID aPropID,
                                   const nsAString&    aPropValue,
                                   nsIURI*             aSheetURI,
                                   nsIURI*             aBaseURI,
                                   nsIPrincipal*       aSheetPrincipal,
                                   nsCSSValue&         aResult)
{
  static_cast<CSSParserImpl*>(mImpl)->
    ParseLonghandProperty(aPropID, aPropValue, aSheetURI, aBaseURI,
                          aSheetPrincipal, aResult);
}

bool
nsCSSParser::ParseTransformProperty(const nsAString& aPropValue,
                                    bool             aDisallowRelativeValues,
                                    nsCSSValue&      aResult)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseTransformProperty(aPropValue, aDisallowRelativeValues, aResult);
}

void
nsCSSParser::ParseVariable(const nsAString&    aVariableName,
                           const nsAString&    aPropValue,
                           nsIURI*             aSheetURI,
                           nsIURI*             aBaseURI,
                           nsIPrincipal*       aSheetPrincipal,
                           css::Declaration*   aDeclaration,
                           bool*               aChanged,
                           bool                aIsImportant)
{
  static_cast<CSSParserImpl*>(mImpl)->
    ParseVariable(aVariableName, aPropValue, aSheetURI, aBaseURI,
                  aSheetPrincipal, aDeclaration, aChanged, aIsImportant);
}

void
nsCSSParser::ParseMediaList(const nsAString& aBuffer,
                            nsIURI*            aURI,
                            uint32_t           aLineNumber,
                            nsMediaList*       aMediaList,
                            dom::CallerType aCallerType)
{
  static_cast<CSSParserImpl*>(mImpl)->
    ParseMediaList(aBuffer, aURI, aLineNumber, aMediaList, aCallerType);
}

bool
nsCSSParser::ParseSourceSizeList(const nsAString& aBuffer,
                                 nsIURI* aURI,
                                 uint32_t aLineNumber,
                                 InfallibleTArray< nsAutoPtr<nsMediaQuery> >& aQueries,
                                 InfallibleTArray<nsCSSValue>& aValues)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseSourceSizeList(aBuffer, aURI, aLineNumber, aQueries, aValues);
}

bool
nsCSSParser::ParseFontFamilyListString(const nsAString& aBuffer,
                                       nsIURI*            aURI,
                                       uint32_t           aLineNumber,
                                       nsCSSValue&        aValue)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseFontFamilyListString(aBuffer, aURI, aLineNumber, aValue);
}

bool
nsCSSParser::ParseColorString(const nsAString& aBuffer,
                              nsIURI*            aURI,
                              uint32_t           aLineNumber,
                              nsCSSValue&        aValue,
                              bool               aSuppressErrors /* false */)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseColorString(aBuffer, aURI, aLineNumber, aValue, aSuppressErrors);
}

bool
nsCSSParser::ParseMarginString(const nsAString& aBuffer,
                               nsIURI*            aURI,
                               uint32_t           aLineNumber,
                               nsCSSValue&        aValue,
                               bool               aSuppressErrors /* false */)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseMarginString(aBuffer, aURI, aLineNumber, aValue, aSuppressErrors);
}

nsresult
nsCSSParser::ParseSelectorString(const nsAString&  aSelectorString,
                                 nsIURI*             aURI,
                                 uint32_t            aLineNumber,
                                 nsCSSSelectorList** aSelectorList)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseSelectorString(aSelectorString, aURI, aLineNumber, aSelectorList);
}

already_AddRefed<nsCSSKeyframeRule>
nsCSSParser::ParseKeyframeRule(const nsAString& aBuffer,
                               nsIURI*            aURI,
                               uint32_t           aLineNumber)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseKeyframeRule(aBuffer, aURI, aLineNumber);
}

bool
nsCSSParser::ParseKeyframeSelectorString(const nsAString& aSelectorString,
                                         nsIURI*            aURI,
                                         uint32_t           aLineNumber,
                                         InfallibleTArray<float>& aSelectorList)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseKeyframeSelectorString(aSelectorString, aURI, aLineNumber,
                                aSelectorList);
}

bool
nsCSSParser::EvaluateSupportsDeclaration(const nsAString& aProperty,
                                         const nsAString& aValue,
                                         nsIURI* aDocURL,
                                         nsIURI* aBaseURL,
                                         nsIPrincipal* aDocPrincipal)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    EvaluateSupportsDeclaration(aProperty, aValue, aDocURL, aBaseURL,
                                aDocPrincipal);
}

bool
nsCSSParser::EvaluateSupportsCondition(const nsAString& aCondition,
                                       nsIURI* aDocURL,
                                       nsIURI* aBaseURL,
                                       nsIPrincipal* aDocPrincipal,
                                       SupportsParsingSettings aSettings)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    EvaluateSupportsCondition(aCondition, aDocURL, aBaseURL,
                              aDocPrincipal, aSettings);
}

bool
nsCSSParser::EnumerateVariableReferences(const nsAString& aPropertyValue,
                                         VariableEnumFunc aFunc,
                                         void* aData)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    EnumerateVariableReferences(aPropertyValue, aFunc, aData);
}

bool
nsCSSParser::ResolveVariableValue(const nsAString& aPropertyValue,
                                  const CSSVariableValues* aVariables,
                                  nsString& aResult,
                                  nsCSSTokenSerializationType& aFirstToken,
                                  nsCSSTokenSerializationType& aLastToken)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ResolveVariableValue(aPropertyValue, aVariables,
                         aResult, aFirstToken, aLastToken);
}

void
nsCSSParser::ParsePropertyWithVariableReferences(
                                            nsCSSPropertyID aPropertyID,
                                            nsCSSPropertyID aShorthandPropertyID,
                                            const nsAString& aValue,
                                            const CSSVariableValues* aVariables,
                                            nsRuleData* aRuleData,
                                            nsIURI* aDocURL,
                                            nsIURI* aBaseURL,
                                            nsIPrincipal* aDocPrincipal,
                                            CSSStyleSheet* aSheet,
                                            uint32_t aLineNumber,
                                            uint32_t aLineOffset)
{
  static_cast<CSSParserImpl*>(mImpl)->
    ParsePropertyWithVariableReferences(aPropertyID, aShorthandPropertyID,
                                        aValue, aVariables, aRuleData, aDocURL,
                                        aBaseURL, aDocPrincipal, aSheet,
                                        aLineNumber, aLineOffset);
}

already_AddRefed<nsAtom>
nsCSSParser::ParseCounterStyleName(const nsAString& aBuffer, nsIURI* aURL)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseCounterStyleName(aBuffer, aURL);
}

bool
nsCSSParser::ParseCounterDescriptor(nsCSSCounterDesc aDescID,
                                    const nsAString& aBuffer,
                                    nsIURI* aSheetURL,
                                    nsIURI* aBaseURL,
                                    nsIPrincipal* aSheetPrincipal,
                                    nsCSSValue& aValue)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseCounterDescriptor(aDescID, aBuffer,
                           aSheetURL, aBaseURL, aSheetPrincipal, aValue);
}

bool
nsCSSParser::ParseFontFaceDescriptor(nsCSSFontDesc aDescID,
                                     const nsAString& aBuffer,
                                     nsIURI* aSheetURL,
                                     nsIURI* aBaseURL,
                                     nsIPrincipal* aSheetPrincipal,
                                     nsCSSValue& aValue)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseFontFaceDescriptor(aDescID, aBuffer,
                           aSheetURL, aBaseURL, aSheetPrincipal, aValue);
}

bool
nsCSSParser::IsValueValidForProperty(const nsCSSPropertyID aPropID,
                                     const nsAString&    aPropValue)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    IsValueValidForProperty(aPropID, aPropValue);
}
