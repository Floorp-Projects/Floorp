/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#include "mozilla/DebugOnly.h"

#include "nsCSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsCSSScanner.h"
#include "mozilla/css/ErrorReporter.h"
#include "mozilla/css/Loader.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/ImportRule.h"
#include "nsCSSRules.h"
#include "mozilla/css/NameSpaceRule.h"
#include "nsTArray.h"
#include "nsCSSStyleSheet.h"
#include "mozilla/css/Declaration.h"
#include "nsStyleConsts.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIAtom.h"
#include "nsColor.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsINameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsError.h"
#include "nsIMediaList.h"
#include "nsStyleUtil.h"
#include "nsIPrincipal.h"
#include "prprf.h"
#include "nsContentUtils.h"
#include "nsAutoPtr.h"
#include "CSSCalc.h"
#include "nsMediaFeatures.h"
#include "nsLayoutUtils.h"

using namespace mozilla;

const uint32_t
nsCSSProps::kParserVariantTable[eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  parsevariant_,
#include "nsCSSPropList.h"
#undef CSS_PROP
};

namespace {

// Rule processing function
typedef void (* RuleAppendFunc) (css::Rule* aRule, void* aData);
static void AssignRuleToPointer(css::Rule* aRule, void* aPointer);
static void AppendRuleToSheet(css::Rule* aRule, void* aParser);

// Your basic top-down recursive descent style parser
// The exposed methods and members of this class are precisely those
// needed by nsCSSParser, far below.
class CSSParserImpl {
public:
  CSSParserImpl();
  ~CSSParserImpl();

  nsresult SetStyleSheet(nsCSSStyleSheet* aSheet);

  nsresult SetQuirkMode(bool aQuirkMode);

  nsresult SetChildLoader(mozilla::css::Loader* aChildLoader);

  // Clears everything set by the above Set*() functions.
  void Reset();

  nsresult ParseSheet(const nsAString& aInput,
                      nsIURI*          aSheetURI,
                      nsIURI*          aBaseURI,
                      nsIPrincipal*    aSheetPrincipal,
                      uint32_t         aLineNumber,
                      bool             aAllowUnsafeRules);

  nsresult ParseStyleAttribute(const nsAString&  aAttributeValue,
                               nsIURI*           aDocURL,
                               nsIURI*           aBaseURL,
                               nsIPrincipal*     aNodePrincipal,
                               css::StyleRule**  aResult);

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

  nsresult ParseProperty(const nsCSSProperty aPropID,
                         const nsAString& aPropValue,
                         nsIURI* aSheetURL,
                         nsIURI* aBaseURL,
                         nsIPrincipal* aSheetPrincipal,
                         css::Declaration* aDeclaration,
                         bool* aChanged,
                         bool aIsImportant,
                         bool aIsSVGMode);

  nsresult ParseMediaList(const nsSubstring& aBuffer,
                          nsIURI* aURL, // for error reporting
                          uint32_t aLineNumber, // for error reporting
                          nsMediaList* aMediaList,
                          bool aHTMLMode);

  bool ParseColorString(const nsSubstring& aBuffer,
                        nsIURI* aURL, // for error reporting
                        uint32_t aLineNumber, // for error reporting
                        nsCSSValue& aValue);

  nsresult ParseSelectorString(const nsSubstring& aSelectorString,
                               nsIURI* aURL, // for error reporting
                               uint32_t aLineNumber, // for error reporting
                               nsCSSSelectorList **aSelectorList);

  already_AddRefed<nsCSSKeyframeRule>
  ParseKeyframeRule(const nsSubstring& aBuffer,
                    nsIURI*            aURL,
                    uint32_t           aLineNumber);

  bool ParseKeyframeSelectorString(const nsSubstring& aSelectorString,
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
                                 nsIPrincipal* aDocPrincipal);

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
      nsAutoParseCompoundProperty(CSSParserImpl* aParser) : mParser(aParser)
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
      nsAutoSuppressErrors(CSSParserImpl* aParser,
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

  // the caller must hold on to aString until parsing is done
  void InitScanner(nsCSSScanner& aScanner,
                   css::ErrorReporter& aReporter,
                   nsIURI* aSheetURI, nsIURI* aBaseURI,
                   nsIPrincipal* aSheetPrincipal);
  void ReleaseScanner(void);
  bool IsSVGMode() const {
    return mScanner->IsSVGMode();
  }

  bool GetToken(bool aSkipWS);
  void UngetToken();
  bool GetNextTokenLocation(bool aSkipWS, uint32_t *linenum, uint32_t *colnum);

  bool ExpectSymbol(PRUnichar aSymbol, bool aSkipWS);
  bool ExpectEndProperty();
  bool CheckEndProperty();
  nsSubstring* NextIdent();

  // returns true when the stop symbol is found, and false for EOF
  bool SkipUntil(PRUnichar aStopSymbol);
  void SkipUntilOneOf(const PRUnichar* aStopSymbolChars);

  void SkipRuleSet(bool aInsideBraces);
  bool SkipAtRule(bool aInsideBlock);
  bool SkipDeclaration(bool aCheckForBraces);

  void PushGroup(css::GroupRule* aRule);
  void PopGroup();

  bool ParseRuleSet(RuleAppendFunc aAppendFunc, void* aProcessData,
                    bool aInsideBraces = false);
  bool ParseAtRule(RuleAppendFunc aAppendFunc, void* aProcessData,
                   bool aInAtRule);
  bool ParseCharsetRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseImportRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseURLOrString(nsString& aURL);
  bool GatherMedia(nsMediaList* aMedia,
                   bool aInAtRule);
  bool ParseMediaQuery(bool aInAtRule, nsMediaQuery **aQuery,
                       bool *aHitStop);
  bool ParseMediaQueryExpression(nsMediaQuery* aQuery);
  void ProcessImport(const nsString& aURLSpec,
                     nsMediaList* aMedia,
                     RuleAppendFunc aAppendFunc,
                     void* aProcessData);
  bool ParseGroupRule(css::GroupRule* aRule, RuleAppendFunc aAppendFunc,
                      void* aProcessData);
  bool ParseMediaRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseMozDocumentRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  bool ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  void ProcessNameSpace(const nsString& aPrefix,
                        const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                        void* aProcessData);

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
  bool ParseSupportsConditionInParensInsideParens(bool& aConditionMet);
  bool ParseSupportsConditionTerms(bool& aConditionMet);
  enum SupportsConditionTermOperator { eAnd, eOr };
  bool ParseSupportsConditionTermsAfterOperator(
                                       bool& aConditionMet,
                                       SupportsConditionTermOperator aOperator);

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
                                              nsIAtom**      aPseudoElement,
                                              nsAtomList**   aPseudoElementArgs,
                                              nsCSSPseudoElements::Type* aPseudoElementType);

  nsSelectorParsingStatus ParseAttributeSelector(int32_t&       aDataMask,
                                                 nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseTypeOrUniversalSelector(int32_t&       aDataMask,
                                                       nsCSSSelector& aSelector,
                                                       bool           aIsNegated);

  nsSelectorParsingStatus ParsePseudoClassWithIdentArg(nsCSSSelector& aSelector,
                                                       nsCSSPseudoClasses::Type aType);

  nsSelectorParsingStatus ParsePseudoClassWithNthPairArg(nsCSSSelector& aSelector,
                                                         nsCSSPseudoClasses::Type aType);

  nsSelectorParsingStatus ParsePseudoClassWithSelectorListArg(nsCSSSelector& aSelector,
                                                              nsCSSPseudoClasses::Type aType);

  nsSelectorParsingStatus ParseNegatedSimpleSelector(int32_t&       aDataMask,
                                                     nsCSSSelector& aSelector);

  // If aStopChar is non-zero, the selector list is done when we hit
  // aStopChar.  Otherwise, it's done when we hit EOF.
  bool ParseSelectorList(nsCSSSelectorList*& aListHead,
                           PRUnichar aStopChar);
  bool ParseSelectorGroup(nsCSSSelectorList*& aListHead);
  bool ParseSelector(nsCSSSelectorList* aList, PRUnichar aPrevCombinator);

  enum {
    eParseDeclaration_InBraces       = 1 << 0,
    eParseDeclaration_AllowImportant = 1 << 1
  };
  enum nsCSSContextType {
    eCSSContext_General,
    eCSSContext_Page
  };

  css::Declaration* ParseDeclarationBlock(uint32_t aFlags,
                                          nsCSSContextType aContext = eCSSContext_General);
  bool ParseDeclaration(css::Declaration* aDeclaration,
                        uint32_t aFlags,
                        bool aMustCallValueAppended,
                        bool* aChanged,
                        nsCSSContextType aContext = eCSSContext_General);

  bool ParseProperty(nsCSSProperty aPropID);
  bool ParsePropertyByFunction(nsCSSProperty aPropID);
  bool ParseSingleValueProperty(nsCSSValue& aValue,
                                  nsCSSProperty aPropID);

  enum PriorityParsingStatus {
    ePriority_None,
    ePriority_Important,
    ePriority_Error
  };
  PriorityParsingStatus ParsePriority();

#ifdef MOZ_XUL
  bool ParseTreePseudoElement(nsAtomList **aPseudoElementArgs);
#endif

  void InitBoxPropsAsPhysical(const nsCSSProperty *aSourceProperties);

  // Property specific parsing routines
  bool ParseBackground();

  struct BackgroundParseState {
    nsCSSValue&  mColor;
    nsCSSValueList* mImage;
    nsCSSValuePairList* mRepeat;
    nsCSSValueList* mAttachment;
    nsCSSValueList* mClip;
    nsCSSValueList* mOrigin;
    nsCSSValueList* mPosition;
    nsCSSValuePairList* mSize;
    BackgroundParseState(
        nsCSSValue& aColor, nsCSSValueList* aImage, nsCSSValuePairList* aRepeat,
        nsCSSValueList* aAttachment, nsCSSValueList* aClip,
        nsCSSValueList* aOrigin, nsCSSValueList* aPosition,
        nsCSSValuePairList* aSize) :
        mColor(aColor), mImage(aImage), mRepeat(aRepeat),
        mAttachment(aAttachment), mClip(aClip), mOrigin(aOrigin),
        mPosition(aPosition), mSize(aSize) {};
  };

  bool ParseBackgroundItem(BackgroundParseState& aState);

  bool ParseValueList(nsCSSProperty aPropID); // a single value prop-id
  bool ParseBackgroundRepeat();
  bool ParseBackgroundRepeatValues(nsCSSValuePair& aValue);
  bool ParseBackgroundPosition();

  // ParseBoxPositionValues parses the CSS 2.1 background-position syntax,
  // which is still used by some properties. See ParseBackgroundPositionValues
  // for the css3-background syntax.
  bool ParseBoxPositionValues(nsCSSValuePair& aOut, bool aAcceptsInherit,
                              bool aAllowExplicitCenter = true); // deprecated
  bool ParseBackgroundPositionValues(nsCSSValue& aOut, bool aAcceptsInherit);

  bool ParseBackgroundSize();
  bool ParseBackgroundSizeValues(nsCSSValuePair& aOut);
  bool ParseBorderColor();
  bool ParseBorderColors(nsCSSProperty aProperty);
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
  bool ParseBorderSide(const nsCSSProperty aPropIDs[],
                         bool aSetAllSides);
  bool ParseDirectionalBorderSide(const nsCSSProperty aPropIDs[],
                                    int32_t aSourceType);
  bool ParseBorderStyle();
  bool ParseBorderWidth();

  bool ParseCalc(nsCSSValue &aValue, int32_t aVariantMask);
  bool ParseCalcAdditiveExpression(nsCSSValue& aValue,
                                     int32_t& aVariantMask);
  bool ParseCalcMultiplicativeExpression(nsCSSValue& aValue,
                                           int32_t& aVariantMask,
                                           bool *aHadFinalWS);
  bool ParseCalcTerm(nsCSSValue& aValue, int32_t& aVariantMask);
  bool RequireWhitespace();

  // For "flex" shorthand property, defined in CSS3 Flexbox
  bool ParseFlex();

  // for 'clip' and '-moz-image-region'
  bool ParseRect(nsCSSProperty aPropID);
  bool ParseColumns();
  bool ParseContent();
  bool ParseCounterData(nsCSSProperty aPropID);
  bool ParseCursor();
  bool ParseFont();
  bool ParseFontSynthesis(nsCSSValue& aValue);
  bool ParseSingleAlternate(int32_t& aWhichFeature, nsCSSValue& aValue);
  bool ParseFontVariantAlternates(nsCSSValue& aValue);
  bool ParseBitmaskValues(nsCSSValue& aValue, const int32_t aKeywordTable[],
                          const int32_t aMasks[]);
  bool ParseFontVariantEastAsian(nsCSSValue& aValue);
  bool ParseFontVariantLigatures(nsCSSValue& aValue);
  bool ParseFontVariantNumeric(nsCSSValue& aValue);
  bool ParseFontWeight(nsCSSValue& aValue);
  bool ParseOneFamily(nsAString& aFamily, bool& aOneKeyword);
  bool ParseFamily(nsCSSValue& aValue);
  bool ParseFontFeatureSettings(nsCSSValue& aValue);
  bool ParseFontSrc(nsCSSValue& aValue);
  bool ParseFontSrcFormat(InfallibleTArray<nsCSSValue>& values);
  bool ParseFontRanges(nsCSSValue& aValue);
  bool ParseListStyle();
  bool ParseMargin();
  bool ParseMarks(nsCSSValue& aValue);
  bool ParseTransform(bool aIsPrefixed);
  bool ParseOutline();
  bool ParseOverflow();
  bool ParsePadding();
  bool ParseQuotes();
  bool ParseSize();
  bool ParseTextDecoration();
  bool ParseTextDecorationLine(nsCSSValue& aValue);
  bool ParseTextOverflow(nsCSSValue& aValue);

  bool ParseShadowItem(nsCSSValue& aValue, bool aIsBoxShadow);
  bool ParseShadowList(nsCSSProperty aProperty);
  bool ParseTransitionProperty();
  bool ParseTransitionTimingFunctionValues(nsCSSValue& aValue);
  bool ParseTransitionTimingFunctionValueComponent(float& aComponent,
                                                     char aStop,
                                                     bool aCheckRange);
  bool ParseTransitionStepTimingFunctionValues(nsCSSValue& aValue);
  enum ParseAnimationOrTransitionShorthandResult {
    eParseAnimationOrTransitionShorthand_Values,
    eParseAnimationOrTransitionShorthand_Inherit,
    eParseAnimationOrTransitionShorthand_Error
  };
  ParseAnimationOrTransitionShorthandResult
    ParseAnimationOrTransitionShorthand(const nsCSSProperty* aProperties,
                                        const nsCSSValue* aInitialValues,
                                        nsCSSValue* aValues,
                                        size_t aNumProperties);
  bool ParseTransition();
  bool ParseAnimation();

  bool ParsePaint(nsCSSProperty aPropID);
  bool ParseDasharray();
  bool ParseMarker();
  bool ParsePaintOrder();

  // Reused utility parsing routines
  void AppendValue(nsCSSProperty aPropID, const nsCSSValue& aValue);
  bool ParseBoxProperties(const nsCSSProperty aPropIDs[]);
  bool ParseGroupedBoxProperty(int32_t aVariantMask,
                               nsCSSValue& aValue);
  bool ParseDirectionalBoxProperty(nsCSSProperty aProperty,
                                     int32_t aSourceType);
  bool ParseBoxCornerRadius(const nsCSSProperty aPropID);
  bool ParseBoxCornerRadii(const nsCSSProperty aPropIDs[]);
  int32_t ParseChoice(nsCSSValue aValues[],
                      const nsCSSProperty aPropIDs[], int32_t aNumIDs);
  bool ParseColor(nsCSSValue& aValue);
  bool ParseColorComponent(uint8_t& aComponent,
                             int32_t& aType, char aStop);
  // ParseHSLColor parses everything starting with the opening '('
  // up through and including the aStop char.
  bool ParseHSLColor(nscolor& aColor, char aStop);
  // ParseColorOpacity will enforce that the color ends with a ')'
  // after the opacity
  bool ParseColorOpacity(uint8_t& aOpacity);
  bool ParseEnum(nsCSSValue& aValue, const int32_t aKeywordTable[]);
  bool ParseVariant(nsCSSValue& aValue,
                      int32_t aVariantMask,
                      const int32_t aKeywordTable[]);
  bool ParseNonNegativeVariant(nsCSSValue& aValue,
                                 int32_t aVariantMask,
                                 const int32_t aKeywordTable[]);
  bool ParseOneOrLargerVariant(nsCSSValue& aValue,
                                 int32_t aVariantMask,
                                 const int32_t aKeywordTable[]);
  bool ParseCounter(nsCSSValue& aValue);
  bool ParseAttr(nsCSSValue& aValue);
  bool SetValueToURL(nsCSSValue& aValue, const nsString& aURL);
  bool TranslateDimension(nsCSSValue& aValue, int32_t aVariantMask,
                            float aNumber, const nsString& aUnit);
  bool ParseImageRect(nsCSSValue& aImage);
  bool ParseElement(nsCSSValue& aValue);
  bool ParseColorStop(nsCSSValueGradient* aGradient);
  bool ParseLinearGradient(nsCSSValue& aValue, bool aIsRepeating,
                           bool aIsLegacy);
  bool ParseRadialGradient(nsCSSValue& aValue, bool aIsRepeating,
                           bool aIsLegacy);
  bool IsLegacyGradientLine(const nsCSSTokenType& aType,
                            const nsString& aId);
  bool ParseGradientColorStops(nsCSSValueGradient* aGradient,
                               nsCSSValue& aValue);

  void SetParsingCompoundProperty(bool aBool) {
    mParsingCompoundProperty = aBool;
  }
  bool IsParsingCompoundProperty(void) const {
    return mParsingCompoundProperty;
  }

  /* Functions for transform Parsing */
  bool ParseSingleTransform(bool aIsPrefixed, nsCSSValue& aValue);
  bool ParseFunction(nsCSSKeyword aFunction, const int32_t aAllowedTypes[],
                     int32_t aVariantMaskAll, uint16_t aMinElems,
                     uint16_t aMaxElems, nsCSSValue &aValue);
  bool ParseFunctionInternals(const int32_t aVariantMask[],
                              int32_t aVariantMaskAll,
                              uint16_t aMinElems,
                              uint16_t aMaxElems,
                              InfallibleTArray<nsCSSValue>& aOutput);

  /* Functions for transform-origin/perspective-origin Parsing */
  bool ParseTransformOrigin(bool aPerspective);

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
  nsRefPtr<nsCSSStyleSheet> mSheet;

  // Used for @import rules
  mozilla::css::Loader* mChildLoader; // not ref counted, it owns us

  // Sheet section we're in.  This is used to enforce correct ordering of the
  // various rule types (eg the fact that a @charset rule must come before
  // anything else).  Note that there are checks of similar things in various
  // places in nsCSSStyleSheet.cpp (e.g in insertRule, RebuildChildList).
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

  // True if unsafe rules should be allowed
  bool mUnsafeRulesEnabled : 1;

  // True if viewport units should be allowed.
  bool mViewportUnitsEnabled : 1;

  // True for parsing media lists for HTML attributes, where we have to
  // ignore CSS comments.
  bool mHTMLMediaMode : 1;

  // This flag is set when parsing a non-box shorthand; it's used to not apply
  // some quirks during shorthand parsing
  bool          mParsingCompoundProperty : 1;

  // True if we are somewhere within a @supports rule whose condition is
  // false.
  bool mInFailingSupportsRule : 1;

  // True if we will suppress all parse errors (except unexpected EOFs).
  // This is used to prevent errors for declarations inside a failing
  // @supports rule.
  bool mSuppressErrors : 1;

  // Stack of rule groups; used for @media and such.
  InfallibleTArray<nsRefPtr<css::GroupRule> > mGroupStack;

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

#define CLEAR_ERROR() \
  mReporter->ClearError()

CSSParserImpl::CSSParserImpl()
  : mToken(),
    mScanner(nullptr),
    mReporter(nullptr),
    mChildLoader(nullptr),
    mSection(eCSSSection_Charset),
    mNameSpaceMap(nullptr),
    mHavePushBack(false),
    mNavQuirkMode(false),
    mHashlessColorQuirk(false),
    mUnitlessLengthQuirk(false),
    mUnsafeRulesEnabled(false),
    mViewportUnitsEnabled(true),
    mHTMLMediaMode(false),
    mParsingCompoundProperty(false),
    mInFailingSupportsRule(false),
    mSuppressErrors(false),
    mNextFree(nullptr)
{
}

CSSParserImpl::~CSSParserImpl()
{
  mData.AssertInitialState();
  mTempData.AssertInitialState();
}

nsresult
CSSParserImpl::SetStyleSheet(nsCSSStyleSheet* aSheet)
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
                           nsIPrincipal* aSheetPrincipal)
{
  NS_PRECONDITION(!mHTMLMediaMode, "Bad initial state");
  NS_PRECONDITION(!mParsingCompoundProperty, "Bad initial state");
  NS_PRECONDITION(!mScanner, "already have scanner");

  mScanner = &aScanner;
  mReporter = &aReporter;
  mScanner->SetErrorReporter(mReporter);

  mBaseURI = aBaseURI;
  mSheetURI = aSheetURI;
  mSheetPrincipal = aSheetPrincipal;
  mHavePushBack = false;
}

void
CSSParserImpl::ReleaseScanner()
{
  mScanner = nullptr;
  mReporter = nullptr;
  mBaseURI = nullptr;
  mSheetURI = nullptr;
  mSheetPrincipal = nullptr;
}

nsresult
CSSParserImpl::ParseSheet(const nsAString& aInput,
                          nsIURI*          aSheetURI,
                          nsIURI*          aBaseURI,
                          nsIPrincipal*    aSheetPrincipal,
                          uint32_t         aLineNumber,
                          bool             aAllowUnsafeRules)
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

  mUnsafeRulesEnabled = aAllowUnsafeRules;

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
  ReleaseScanner();

  mUnsafeRulesEnabled = false;

  // XXX check for low level errors
  return NS_OK;
}

/**
 * Determines whether the identifier contained in the given string is a
 * vendor-specific identifier, as described in CSS 2.1 section 4.1.2.1.
 */
static bool
NonMozillaVendorIdentifier(const nsAString& ident)
{
  return (ident.First() == PRUnichar('-') &&
          !StringBeginsWith(ident, NS_LITERAL_STRING("-moz-"))) ||
         ident.First() == PRUnichar('_');

}

nsresult
CSSParserImpl::ParseStyleAttribute(const nsAString& aAttributeValue,
                                   nsIURI*          aDocURI,
                                   nsIURI*          aBaseURI,
                                   nsIPrincipal*    aNodePrincipal,
                                   css::StyleRule** aResult)
{
  NS_PRECONDITION(aNodePrincipal, "Must have principal here!");
  NS_PRECONDITION(aBaseURI, "need base URI");

  // XXX line number?
  nsCSSScanner scanner(aAttributeValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURI);
  InitScanner(scanner, reporter, aDocURI, aBaseURI, aNodePrincipal);

  mSection = eCSSSection_General;

  // In quirks mode, allow style declarations to have braces or not
  // (bug 99554).
  bool haveBraces;
  if (mNavQuirkMode && GetToken(true)) {
    haveBraces = eCSSToken_Symbol == mToken.mType &&
                 '{' == mToken.mSymbol;
    UngetToken();
  }
  else {
    haveBraces = false;
  }

  uint32_t parseFlags = eParseDeclaration_AllowImportant;
  if (haveBraces) {
    parseFlags |= eParseDeclaration_InBraces;
  }

  css::Declaration* declaration = ParseDeclarationBlock(parseFlags);
  if (declaration) {
    // Create a style rule for the declaration
    NS_ADDREF(*aResult = new css::StyleRule(nullptr, declaration));
  } else {
    *aResult = nullptr;
  }

  ReleaseScanner();

  // XXX check for low level errors
  return NS_OK;
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

// See Bug 723197
#ifdef _MSC_VER
#pragma optimize( "", off )
#pragma warning( push )
#pragma warning( disable : 4748 )
#endif
nsresult
CSSParserImpl::ParseProperty(const nsCSSProperty aPropID,
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

  mData.AssertInitialState();
  mTempData.AssertInitialState();
  aDeclaration->AssertMutable();

  nsCSSScanner scanner(aPropValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aSheetURI);
  InitScanner(scanner, reporter, aSheetURI, aBaseURI, aSheetPrincipal);
  mSection = eCSSSection_General;
  scanner.SetSVGMode(aIsSVGMode);

  *aChanged = false;

  // Check for unknown or preffed off properties
  if (eCSSProperty_UNKNOWN == aPropID || !nsCSSProps::IsEnabled(aPropID)) {
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    REPORT_UNEXPECTED_P(PEUnknownProperty, propName);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ReleaseScanner();
    return NS_OK;
  }

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
      *aChanged = mData.TransferFromBlock(mTempData, aPropID, aIsImportant,
                                          true, false, aDeclaration);
      aDeclaration->CompressFrom(&mData);
    }
    CLEAR_ERROR();
  }

  mTempData.AssertInitialState();

  ReleaseScanner();
  return NS_OK;
}
#ifdef _MSC_VER
#pragma warning( pop )
#pragma optimize( "", on )
#endif

nsresult
CSSParserImpl::ParseMediaList(const nsSubstring& aBuffer,
                              nsIURI* aURI, // for error reporting
                              uint32_t aLineNumber, // for error reporting
                              nsMediaList* aMediaList,
                              bool aHTMLMode)
{
  // XXX Are there cases where the caller wants to keep what it already
  // has in case of parser error?  If GatherMedia ever changes to return
  // a value other than true, we probably should avoid modifying aMediaList.
  aMediaList->Clear();

  // fake base URI since media lists don't have URIs in them
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  mHTMLMediaMode = aHTMLMode;

    // XXXldb We need to make the scanner not skip CSS comments!  (Or
    // should we?)

  // For aHTMLMode, we used to follow the parsing rules in
  // http://www.w3.org/TR/1999/REC-html401-19991224/types.html#type-media-descriptors
  // which wouldn't work for media queries since they remove all but the
  // first word.  However, they're changed in
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/section-document.html#media2
  // (as of 2008-05-29) which says that the media attribute just points
  // to a media query.  (The main substative difference is the relative
  // precedence of commas and paretheses.)

  DebugOnly<bool> parsedOK = GatherMedia(aMediaList, false);
  NS_ASSERTION(parsedOK, "GatherMedia returned false; we probably want to avoid "
                         "trashing aMediaList");

  CLEAR_ERROR();
  ReleaseScanner();
  mHTMLMediaMode = false;

  return NS_OK;
}

bool
CSSParserImpl::ParseColorString(const nsSubstring& aBuffer,
                                nsIURI* aURI, // for error reporting
                                uint32_t aLineNumber, // for error reporting
                                nsCSSValue& aValue)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  // Parse a color, and check that there's nothing else after it.
  bool colorParsed = ParseColor(aValue) && !GetToken(true);
  OUTPUT_ERROR();
  ReleaseScanner();
  return colorParsed;
}

nsresult
CSSParserImpl::ParseSelectorString(const nsSubstring& aSelectorString,
                                   nsIURI* aURI, // for error reporting
                                   uint32_t aLineNumber, // for error reporting
                                   nsCSSSelectorList **aSelectorList)
{
  nsCSSScanner scanner(aSelectorString, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  bool success = ParseSelectorList(*aSelectorList, PRUnichar(0));

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
CSSParserImpl::ParseKeyframeRule(const nsSubstring&  aBuffer,
                                 nsIURI*             aURI,
                                 uint32_t            aLineNumber)
{
  nsCSSScanner scanner(aBuffer, aLineNumber);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aURI);
  InitScanner(scanner, reporter, aURI, aURI, nullptr);

  nsRefPtr<nsCSSKeyframeRule> result = ParseKeyframeRule();
  if (GetToken(true)) {
    // extra garbage at the end
    result = nullptr;
  }

  OUTPUT_ERROR();
  ReleaseScanner();

  return result.forget();
}

bool
CSSParserImpl::ParseKeyframeSelectorString(const nsSubstring& aSelectorString,
                                           nsIURI* aURI, // for error reporting
                                           uint32_t aLineNumber, // for error reporting
                                           InfallibleTArray<float>& aSelectorList)
{
  NS_ABORT_IF_FALSE(aSelectorList.IsEmpty(), "given list should start empty");

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
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty,
                                                    nsCSSProps::eEnabled);
  if (propID == eCSSProperty_UNKNOWN) {
    return false;
  }

  nsCSSScanner scanner(aValue, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURL);
  InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);
  nsAutoSuppressErrors suppressErrors(this);

  bool parsedOK = ParseProperty(propID) && !GetToken(true);

  CLEAR_ERROR();
  ReleaseScanner();

  mTempData.ClearProperty(propID);
  mTempData.AssertInitialState();

  return parsedOK;
}

bool
CSSParserImpl::EvaluateSupportsCondition(const nsAString& aDeclaration,
                                         nsIURI* aDocURL,
                                         nsIURI* aBaseURL,
                                         nsIPrincipal* aDocPrincipal)
{
  nsCSSScanner scanner(aDeclaration, 0);
  css::ErrorReporter reporter(scanner, mSheet, mChildLoader, aDocURL);
  InitScanner(scanner, reporter, aDocURL, aBaseURL, aDocPrincipal);
  nsAutoSuppressErrors suppressErrors(this);

  bool conditionMet;
  bool parsedOK = ParseSupportsCondition(conditionMet) && !GetToken(true);

  CLEAR_ERROR();
  ReleaseScanner();

  return parsedOK && conditionMet;
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
  return mScanner->Next(mToken, aSkipWS);
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
CSSParserImpl::ExpectSymbol(PRUnichar aSymbol,
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

nsSubstring*
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
      PRUnichar symbol = mToken.mSymbol;
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

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("font-feature-values") &&
             nsCSSFontFeatureValuesRule::PrefEnabled()) {
    parseFunc = &CSSParserImpl::ParseFontFeatureValuesRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("page")) {
    parseFunc = &CSSParserImpl::ParsePageRule;
    newSection = eCSSSection_General;

  } else if ((nsCSSProps::IsEnabled(eCSSPropertyAlias_MozAnimation) &&
              mToken.mIdent.LowerCaseEqualsLiteral("-moz-keyframes")) ||
             mToken.mIdent.LowerCaseEqualsLiteral("keyframes")) {
    parseFunc = &CSSParserImpl::ParseKeyframesRule;
    newSection = eCSSSection_General;

  } else if (mToken.mIdent.LowerCaseEqualsLiteral("supports") &&
             CSSSupportsRule::PrefEnabled()) {
    parseFunc = &CSSParserImpl::ParseSupportsRule;
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
  if (!GetToken(true)) {
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

  nsRefPtr<css::CharsetRule> rule = new css::CharsetRule(charset);
  (*aAppendFunc)(rule, aData);

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
CSSParserImpl::ParseMediaQuery(bool aInAtRule,
                               nsMediaQuery **aQuery,
                               bool *aHitStop)
{
  *aQuery = nullptr;
  *aHitStop = false;

  // "If the comma-separated list is the empty list it is assumed to
  // specify the media query 'all'."  (css3-mediaqueries, section
  // "Media Queries")
  if (!GetToken(true)) {
    *aHitStop = true;
    // expected termination by EOF
    if (!aInAtRule)
      return true;

    // unexpected termination by EOF
    REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
    return true;
  }

  if (eCSSToken_Symbol == mToken.mType && aInAtRule &&
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
  } else {
    nsCOMPtr<nsIAtom> mediaType;
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
      mediaType = do_GetAtom(mToken.mIdent);
      if (!mediaType) {
        NS_RUNTIMEABORT("do_GetAtom failed - out of memory?");
      }
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
      if (!aInAtRule)
        break;

      // unexpected termination by EOF
      REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
      break;
    }

    if (eCSSToken_Symbol == mToken.mType && aInAtRule &&
        (mToken.mSymbol == ';' || mToken.mSymbol == '{' || mToken.mSymbol == '}')) {
      *aHitStop = true;
      UngetToken();
      break;
    }
    if (eCSSToken_Symbol == mToken.mType && mToken.mSymbol == ',') {
      // Done with the expressions for this query
      break;
    }
    if (eCSSToken_Ident != mToken.mType ||
        !mToken.mIdent.LowerCaseEqualsLiteral("and")) {
      REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotComma);
      UngetToken();
      return false;
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
  for (;;) {
    nsAutoPtr<nsMediaQuery> query;
    bool hitStop;
    if (!ParseMediaQuery(aInAtRule, getter_Transfers(query),
                         &hitStop)) {
      NS_ASSERTION(!hitStop, "should return true when hit stop");
      OUTPUT_ERROR();
      if (query) {
        query->SetHadUnknownExpression();
      }
      if (aInAtRule) {
        const PRUnichar stopChars[] =
          { PRUnichar(','), PRUnichar('{'), PRUnichar(';'), PRUnichar('}'), PRUnichar(0) };
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
  const PRUnichar *featureString;
  if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("min-"))) {
    expr->mRange = nsMediaExpression::eMin;
    featureString = mToken.mIdent.get() + 4;
  } else if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("max-"))) {
    expr->mRange = nsMediaExpression::eMax;
    featureString = mToken.mIdent.get() + 4;
  } else {
    expr->mRange = nsMediaExpression::eEqual;
    featureString = mToken.mIdent.get();
  }

  nsCOMPtr<nsIAtom> mediaFeatureAtom = do_GetAtom(featureString);
  if (!mediaFeatureAtom) {
    NS_RUNTIMEABORT("do_GetAtom failed - out of memory?");
  }
  const nsMediaFeature *feature = nsMediaFeatures::features;
  for (; feature->mName; ++feature) {
    if (*(feature->mName) == mediaFeatureAtom) {
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

  bool rv;
  switch (feature->mValueType) {
    case nsMediaFeature::eLength:
      rv = ParseNonNegativeVariant(expr->mValue, VARIANT_LENGTH, nullptr);
      break;
    case nsMediaFeature::eInteger:
    case nsMediaFeature::eBoolInteger:
      rv = ParseNonNegativeVariant(expr->mValue, VARIANT_INTEGER, nullptr);
      // Enforce extra restrictions for eBoolInteger
      if (rv &&
          feature->mValueType == nsMediaFeature::eBoolInteger &&
          expr->mValue.GetIntValue() > 1)
        rv = false;
      break;
    case nsMediaFeature::eFloat:
      rv = ParseNonNegativeVariant(expr->mValue, VARIANT_NUMBER, nullptr);
      break;
    case nsMediaFeature::eIntRatio:
      {
        // Two integers separated by '/', with optional whitespace on
        // either side of the '/'.
        nsRefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);
        expr->mValue.SetArrayValue(a, eCSSUnit_Array);
        // We don't bother with ParseNonNegativeVariant since we have to
        // check for != 0 as well; no need to worry about the UngetToken
        // since we're throwing out up to the next ')' anyway.
        rv = ParseVariant(a->Item(0), VARIANT_INTEGER, nullptr) &&
             a->Item(0).GetIntValue() > 0 &&
             ExpectSymbol('/', true) &&
             ParseVariant(a->Item(1), VARIANT_INTEGER, nullptr) &&
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
      rv = ParseVariant(expr->mValue, VARIANT_KEYWORD,
                        feature->mData.mKeywordTable);
      break;
    case nsMediaFeature::eIdent:
      rv = ParseVariant(expr->mValue, VARIANT_IDENTIFIER, nullptr);
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
  nsRefPtr<nsMediaList> media = new nsMediaList();

  nsAutoString url;
  if (!ParseURLOrString(url)) {
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
    NS_ASSERTION(media->Count() != 0, "media list must be nonempty");
  }

  ProcessImport(url, media, aAppendFunc, aData);
  return true;
}


void
CSSParserImpl::ProcessImport(const nsString& aURLSpec,
                             nsMediaList* aMedia,
                             RuleAppendFunc aAppendFunc,
                             void* aData)
{
  nsRefPtr<css::ImportRule> rule = new css::ImportRule(aMedia, aURLSpec);
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
    mChildLoader->LoadChildSheet(mSheet, url, aMedia, rule);
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
  nsRefPtr<nsMediaList> media = new nsMediaList();

  if (GatherMedia(media, true)) {
    // XXXbz this could use better error reporting throughout the method
    nsRefPtr<css::MediaRule> rule = new css::MediaRule();
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
  css::DocumentRule::URL *urls = nullptr;
  css::DocumentRule::URL **next = &urls;
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
      REPORT_UNEXPECTED_TOKEN(PEMozDocRuleBadFunc);
      UngetToken();
      delete urls;
      return false;
    }
    css::DocumentRule::URL *cur = *next = new css::DocumentRule::URL;
    next = &cur->next;
    if (mToken.mType == eCSSToken_URL) {
      cur->func = css::DocumentRule::eURL;
      CopyUTF16toUTF8(mToken.mIdent, cur->url);
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("regexp")) {
      // regexp() is different from url-prefix() and domain() (but
      // probably the way they *should* have been* in that it requires a
      // string argument, and doesn't try to behave like url().
      cur->func = css::DocumentRule::eRegExp;
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
        cur->func = css::DocumentRule::eURLPrefix;
      } else if (mToken.mIdent.LowerCaseEqualsLiteral("domain")) {
        cur->func = css::DocumentRule::eDomain;
      }

      NS_ASSERTION(!mHavePushBack, "mustn't have pushback at this point");
      if (!mScanner->NextURL(mToken) || mToken.mType != eCSSToken_URL) {
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

  nsRefPtr<css::DocumentRule> rule = new css::DocumentRule();
  rule->SetURLs(urls);

  return ParseGroupRule(rule, aAppendFunc, aData);
}

// Parse a CSS3 namespace rule: "@namespace [prefix] STRING | URL;"
bool
CSSParserImpl::ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aData)
{
  if (!GetToken(true)) {
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

  ProcessNameSpace(prefix, url, aAppendFunc, aData);
  return true;
}

void
CSSParserImpl::ProcessNameSpace(const nsString& aPrefix,
                                const nsString& aURLSpec,
                                RuleAppendFunc aAppendFunc,
                                void* aData)
{
  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = do_GetAtom(aPrefix);
    if (!prefix) {
      NS_RUNTIMEABORT("do_GetAtom failed - out of memory?");
    }
  }

  nsRefPtr<css::NameSpaceRule> rule = new css::NameSpaceRule(prefix, aURLSpec);
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
  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEBadFontBlockStart);
    return false;
  }

  nsRefPtr<nsCSSFontFaceRule> rule(new nsCSSFontFaceRule());

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

  if (descID == eCSSFontDesc_UNKNOWN) {
    if (NonMozillaVendorIdentifier(descName)) {
      // silently skip other vendors' extensions
      SkipDeclaration(true);
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

  if (!ExpectEndProperty())
    return false;

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
  nsRefPtr<nsCSSFontFeatureValuesRule>
               valuesRule(new nsCSSFontFeatureValuesRule());

  // parse family list
  nsCSSValue familyValue;

  if (!ParseFamily(familyValue) ||
      familyValue.GetUnit() != eCSSUnit_Families)
  {
    REPORT_UNEXPECTED_TOKEN(PEFFVNoFamily);
    return false;
  }

  // add family to rule
  nsAutoString familyList;
  bool hasGeneric;
  familyValue.GetStringValue(familyList);
  valuesRule->SetFamilyList(familyList, hasGeneric);

  // family list has generic ==> parse error
  if (hasGeneric) {
    REPORT_UNEXPECTED_TOKEN(PEFFVGenericInFamilyList);
    return false;
  }

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
  if (keyword == eCSSKeyword_UNKNOWN ||
      !nsCSSProps::FindKeyword(keyword,
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
  nsAutoTArray<gfxFontFeatureValueSet::ValueList, 5> values;

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
    nsAutoTArray<uint32_t,4>   featureSelectors;

    nsCSSValue intValue;
    while (ParseNonNegativeVariant(intValue, VARIANT_INTEGER, nullptr)) {
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
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEKeyframeNameEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Ident) {
    REPORT_UNEXPECTED_TOKEN(PEKeyframeBadName);
    UngetToken();
    return false;
  }
  nsString name(mToken.mIdent);

  if (!ExpectSymbol('{', true)) {
    REPORT_UNEXPECTED_TOKEN(PEKeyframeBrace);
    return false;
  }

  nsRefPtr<nsCSSKeyframesRule> rule = new nsCSSKeyframesRule(name);

  while (!ExpectSymbol('}', true)) {
    nsRefPtr<nsCSSKeyframeRule> kid = ParseKeyframeRule();
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
  // TODO: There can be page selectors after @page such as ":first", ":left".
  uint32_t parseFlags = eParseDeclaration_InBraces |
                        eParseDeclaration_AllowImportant;

  // Forbid viewport units in @page rules. See bug 811391.
  NS_ABORT_IF_FALSE(mViewportUnitsEnabled,
                    "Viewport units should be enabled outside of @page rules.");
  mViewportUnitsEnabled = false;
  nsAutoPtr<css::Declaration> declaration(
                                ParseDeclarationBlock(parseFlags,
                                                      eCSSContext_Page));
  mViewportUnitsEnabled = true;

  if (!declaration) {
    return false;
  }

  // Takes ownership of declaration.
  nsRefPtr<nsCSSPageRule> rule = new nsCSSPageRule(declaration);

  (*aAppendFunc)(rule, aData);
  return true;
}

already_AddRefed<nsCSSKeyframeRule>
CSSParserImpl::ParseKeyframeRule()
{
  InfallibleTArray<float> selectorList;
  if (!ParseKeyframeSelectorList(selectorList)) {
    REPORT_UNEXPECTED(PEBadSelectorKeyframeRuleIgnored);
    return nullptr;
  }

  // Ignore !important in keyframe rules
  uint32_t parseFlags = eParseDeclaration_InBraces;
  nsAutoPtr<css::Declaration> declaration(ParseDeclarationBlock(parseFlags));
  if (!declaration) {
    return nullptr;
  }

  // Takes ownership of declaration, and steals contents of selectorList.
  nsRefPtr<nsCSSKeyframeRule> rule =
    new nsCSSKeyframeRule(selectorList, declaration);

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
        // fall through
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

  nsRefPtr<css::GroupRule> rule = new CSSSupportsRule(conditionMet, condition);
  return ParseGroupRule(rule, aAppendFunc, aProcessData);
}

// supports_condition
//   : supports_condition_in_parens supports_condition_terms
//   | supports_condition_negation
//   ;
bool
CSSParserImpl::ParseSupportsCondition(bool& aConditionMet)
{
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
    return ParseSupportsConditionInParens(aConditionMet) &&
           ParseSupportsConditionTerms(aConditionMet) &&
           !mScanner->SeenBadToken();
  }

  if (mToken.mType == eCSSToken_Ident &&
      mToken.mIdent.LowerCaseEqualsLiteral("not")) {
    return ParseSupportsConditionNegation(aConditionMet) &&
           !mScanner->SeenBadToken();
  }

  REPORT_UNEXPECTED_TOKEN(PESupportsConditionExpectedStart);
  return false;
}

// supports_condition_negation
//   : 'not' S+ supports_condition_in_parens
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

  if (!RequireWhitespace()) {
    REPORT_UNEXPECTED(PESupportsWhitespaceRequired);
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
    REPORT_UNEXPECTED_TOKEN(PESupportsConditionExpectedCloseParen);
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

      if (ExpectSymbol(')', true)) {
        UngetToken();
        return false;
      }

      nsCSSProperty propID = nsCSSProps::LookupProperty(propertyName,
                                                        nsCSSProps::eEnabled);
      if (propID == eCSSProperty_UNKNOWN) {
        aConditionMet = false;
        SkipUntil(')');
        UngetToken();
      } else {
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
//   : S+ 'and' supports_condition_terms_after_operator('and')
//   | S+ 'or' supports_condition_terms_after_operator('or')
//   |
//   ;
bool
CSSParserImpl::ParseSupportsConditionTerms(bool& aConditionMet)
{
  if (!RequireWhitespace() || !GetToken(false)) {
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
//   : S+ supports_condition_in_parens ( <operator> supports_condition_in_parens )*
//   ;
bool
CSSParserImpl::ParseSupportsConditionTermsAfterOperator(
                         bool& aConditionMet,
                         CSSParserImpl::SupportsConditionTermOperator aOperator)
{
  if (!RequireWhitespace()) {
    REPORT_UNEXPECTED(PESupportsWhitespaceRequired);
    return false;
  }

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
CSSParserImpl::SkipUntil(PRUnichar aStopSymbol)
{
  nsCSSToken* tk = &mToken;
  nsAutoTArray<PRUnichar, 16> stack;
  stack.AppendElement(aStopSymbol);
  for (;;) {
    if (!GetToken(true)) {
      return false;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
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

void
CSSParserImpl::SkipUntilOneOf(const PRUnichar* aStopSymbolChars)
{
  nsCSSToken* tk = &mToken;
  nsDependentString stopSymbolChars(aStopSymbolChars);
  for (;;) {
    if (!GetToken(true)) {
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
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
      PRUnichar symbol = tk->mSymbol;
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
      PRUnichar symbol = tk->mSymbol;
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
      !ParseSelectorList(slist, PRUnichar('{'))) {
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
  css::Declaration* declaration = ParseDeclarationBlock(parseFlags);
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

  nsRefPtr<css::StyleRule> rule = new css::StyleRule(slist, declaration);
  rule->SetLineNumberAndColumnNumber(linenum, colnum);
  (*aAppendFunc)(rule, aData);

  return true;
}

bool
CSSParserImpl::ParseSelectorList(nsCSSSelectorList*& aListHead,
                                 PRUnichar aStopChar)
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
      if (aStopChar == PRUnichar(0)) {
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
      } else if (aStopChar == tk->mSymbol && aStopChar != PRUnichar(0)) {
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
  PRUnichar combinator = 0;
  nsAutoPtr<nsCSSSelectorList> list(new nsCSSSelectorList());

  for (;;) {
    if (!ParseSelector(list, combinator)) {
      return false;
    }

    // Look for a combinator.
    if (!GetToken(false)) {
      break; // EOF ok here
    }

    combinator = PRUnichar(0);
    if (mToken.mType == eCSSToken_Whitespace) {
      if (!GetToken(true)) {
        break; // EOF ok here
      }
      combinator = PRUnichar(' ');
    }

    if (mToken.mType != eCSSToken_Symbol) {
      UngetToken(); // not a combinator
    } else {
      PRUnichar symbol = mToken.mSymbol;
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

  if (! GetToken(true)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttSelInnerEOF);
    return eSelectorParsingStatus_Error;
  }
  if ((eCSSToken_Symbol == mToken.mType) ||
      (eCSSToken_Includes == mToken.mType) ||
      (eCSSToken_Dashmatch == mToken.mType) ||
      (eCSSToken_Beginsmatch == mToken.mType) ||
      (eCSSToken_Endsmatch == mToken.mType) ||
      (eCSSToken_Containsmatch == mToken.mType)) {
    uint8_t func;
    if (eCSSToken_Includes == mToken.mType) {
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
    else if (']' == mToken.mSymbol) {
      aDataMask |= SEL_MASK_ATTRIB;
      aSelector.AddAttribute(nameSpaceID, attr);
      func = NS_ATTR_FUNC_SET;
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
        if (! GetToken(true)) { // premature EOF
          REPORT_UNEXPECTED_EOF(PEAttSelCloseEOF);
          return eSelectorParsingStatus_Error;
        }
        if (mToken.IsSymbol(']')) {
          bool isCaseSensitive = true;

          // For cases when this style sheet is applied to an HTML
          // element in an HTML document, and the attribute selector is
          // for a non-namespaced attribute, then check to see if it's
          // one of the known attributes whose VALUE is
          // case-insensitive.
          if (nameSpaceID == kNameSpaceID_None) {
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
                isCaseSensitive = false;
                break;
              }
            }
          }
          aDataMask |= SEL_MASK_ATTRIB;
          aSelector.AddAttribute(nameSpaceID, attr, func, value, isCaseSensitive);
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
                                   nsIAtom**      aPseudoElement,
                                   nsAtomList**   aPseudoElementArgs,
                                   nsCSSPseudoElements::Type* aPseudoElementType)
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
  buffer.Append(PRUnichar(':'));
  buffer.Append(mToken.mIdent);
  nsContentUtils::ASCIIToLower(buffer);
  nsCOMPtr<nsIAtom> pseudo = do_GetAtom(buffer);
  if (!pseudo) {
    NS_RUNTIMEABORT("do_GetAtom failed - out of memory?");
  }

  // stash away some info about this pseudo so we only have to get it once.
  bool isTreePseudo = false;
  nsCSSPseudoElements::Type pseudoElementType =
    nsCSSPseudoElements::GetPseudoType(pseudo);
  nsCSSPseudoClasses::Type pseudoClassType =
    nsCSSPseudoClasses::GetPseudoType(pseudo);

  // We currently allow :-moz-placeholder and ::-moz-placeholder. We have to
  // be a bit stricter regarding the pseudo-element parsing rules.
  if (pseudoElementType == nsCSSPseudoElements::ePseudo_mozPlaceholder &&
      pseudoClassType == nsCSSPseudoClasses::ePseudoClass_mozPlaceholder) {
    if (parsingPseudoElement) {
      pseudoClassType = nsCSSPseudoClasses::ePseudoClass_NotPseudoClass;
    } else {
      pseudoElementType = nsCSSPseudoElements::ePseudo_NotPseudoElement;
    }
  }

#ifdef MOZ_XUL
  isTreePseudo = (pseudoElementType == nsCSSPseudoElements::ePseudo_XULTree);
  // If a tree pseudo-element is using the function syntax, it will
  // get isTree set here and will pass the check below that only
  // allows functions if they are in our list of things allowed to be
  // functions.  If it is _not_ using the function syntax, isTree will
  // be false, and it will still pass that check.  So the tree
  // pseudo-elements are allowed to be either functions or not, as
  // desired.
  bool isTree = (eCSSToken_Function == mToken.mType) && isTreePseudo;
#endif
  bool isPseudoElement =
    (pseudoElementType < nsCSSPseudoElements::ePseudo_PseudoElementCount);
  // anonymous boxes are only allowed if they're the tree boxes or we have
  // enabled unsafe rules
  bool isAnonBox = isTreePseudo ||
    (pseudoElementType == nsCSSPseudoElements::ePseudo_AnonBox &&
     mUnsafeRulesEnabled);
  bool isPseudoClass =
    (pseudoClassType != nsCSSPseudoClasses::ePseudoClass_NotPseudoClass);

  NS_ASSERTION(!isPseudoClass ||
               pseudoElementType == nsCSSPseudoElements::ePseudo_NotPseudoElement,
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
       nsCSSPseudoClasses::ePseudoClass_notPseudo == pseudoClassType ||
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

  if (!parsingPseudoElement &&
      nsCSSPseudoClasses::ePseudoClass_notPseudo == pseudoClassType) {
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
        NS_ABORT_IF_FALSE(nsCSSPseudoClasses::HasSelectorListArg(pseudoClassType),
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

      // the next *non*whitespace token must be '{' or ',' or EOF
      if (!GetToken(true)) { // premature eof is ok (here!)
        return eSelectorParsingStatus_Done;
      }
      if ((mToken.IsSymbol('{') || mToken.IsSymbol(','))) {
        UngetToken();
        return eSelectorParsingStatus_Done;
      }
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelTrailing);
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
                                            nsCSSPseudoClasses::Type aType)
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

  // -moz-locale-dir and -moz-dir can only have values of 'ltr' or 'rtl'.
  if (aType == nsCSSPseudoClasses::ePseudoClass_mozLocaleDir ||
      aType == nsCSSPseudoClasses::ePseudoClass_dir) {
    nsContentUtils::ASCIIToLower(mToken.mIdent); // case insensitive
    if (!mToken.mIdent.EqualsLiteral("ltr") &&
        !mToken.mIdent.EqualsLiteral("rtl")) {
      REPORT_UNEXPECTED_TOKEN(PEBadDirValue);
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
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
                                              nsCSSPseudoClasses::Type aType)
{
  int32_t numbers[2] = { 0, 0 };
  int32_t sign[2] = { 1, 1 };
  bool hasSign[2] = { false, false };
  bool lookForB = true;

  // Follow the whitespace rules as proposed in
  // http://lists.w3.org/Archives/Public/www-style/2008Mar/0121.html

  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }

  if (mToken.IsSymbol('+') || mToken.IsSymbol('-')) {
    hasSign[0] = true;
    if (mToken.IsSymbol('-')) {
      sign[0] = -1;
    }
    if (! GetToken(false)) {
      REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
      return eSelectorParsingStatus_Error;
    }
  }

  if (eCSSToken_Ident == mToken.mType || eCSSToken_Dimension == mToken.mType) {
    // The CSS tokenization doesn't handle :nth-child() containing - well:
    //   2n-1 is a dimension
    //   n-1 is an identifier
    // The easiest way to deal with that is to push everything from the
    // minus on back onto the scanner's pushback buffer.
    uint32_t truncAt = 0;
    if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("n-"))) {
      truncAt = 1;
    } else if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("-n-")) && !hasSign[0]) {
      truncAt = 2;
    }
    if (truncAt != 0) {
      mScanner->Backup(mToken.mIdent.Length() - truncAt);
      mToken.mIdent.Truncate(truncAt);
    }
  }

  if (eCSSToken_Ident == mToken.mType) {
    if (mToken.mIdent.LowerCaseEqualsLiteral("odd") && !hasSign[0]) {
      numbers[0] = 2;
      numbers[1] = 1;
      lookForB = false;
    }
    else if (mToken.mIdent.LowerCaseEqualsLiteral("even") && !hasSign[0]) {
      numbers[0] = 2;
      numbers[1] = 0;
      lookForB = false;
    }
    else if (mToken.mIdent.LowerCaseEqualsLiteral("n")) {
      numbers[0] = sign[0];
    }
    else if (mToken.mIdent.LowerCaseEqualsLiteral("-n") && !hasSign[0]) {
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
    // for +-an case
    if (mToken.mHasSign && hasSign[0]) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
    int32_t intValue = mToken.mInteger * sign[0];
    // for -a/**/n case
    if (! GetToken(false)) {
      numbers[1] = intValue;
      lookForB = false;
    }
    else {
      if (eCSSToken_Ident == mToken.mType && mToken.mIdent.LowerCaseEqualsLiteral("n")) {
        numbers[0] = intValue;
      }
      else if (eCSSToken_Ident == mToken.mType && StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("n-"))) {
        numbers[0] = intValue;
        mScanner->Backup(mToken.mIdent.Length() - 1);
      }
      else {
        UngetToken();
        numbers[1] = intValue;
        lookForB = false;
      }
    }
  }
  else if (eCSSToken_Dimension == mToken.mType) {
    if (!mToken.mIntegerValid || !mToken.mIdent.LowerCaseEqualsLiteral("n")) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
    // for +-an case
    if ( mToken.mHasSign && hasSign[0] ) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
    numbers[0] = mToken.mInteger * sign[0];
  }
  // XXX If it's a ')', is that valid?  (as 0n+0)
  else {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
    UngetToken();
    return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
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
      hasSign[1] = true;
      if (mToken.IsSymbol('-')) {
        sign[1] = -1;
      }
      if (! GetToken(true)) {
        REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
        return eSelectorParsingStatus_Error;
      }
    }
    if (eCSSToken_Number != mToken.mType ||
        !mToken.mIntegerValid || mToken.mHasSign == hasSign[1]) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      UngetToken();
      return eSelectorParsingStatus_Error; // our caller calls SkipUntil(')')
    }
    numbers[1] = mToken.mInteger * sign[1];
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
                                                   nsCSSPseudoClasses::Type aType)
{
  nsAutoPtr<nsCSSSelectorList> slist;
  if (! ParseSelectorList(*getter_Transfers(slist), PRUnichar(')'))) {
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
                             PRUnichar aPrevCombinator)
{
  if (! GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PESelectorEOF);
    return false;
  }

  nsCSSSelector* selector = aList->AddSelector(aPrevCombinator);
  nsCOMPtr<nsIAtom> pseudoElement;
  nsAutoPtr<nsAtomList> pseudoElementArgs;
  nsCSSPseudoElements::Type pseudoElementType =
    nsCSSPseudoElements::ePseudo_NotPseudoElement;

  int32_t dataMask = 0;
  nsSelectorParsingStatus parsingStatus =
    ParseTypeOrUniversalSelector(dataMask, *selector, false);

  while (parsingStatus == eSelectorParsingStatus_Continue) {
    if (eCSSToken_ID == mToken.mType) { // #id
      parsingStatus = ParseIDSelector(dataMask, *selector);
    }
    else if (mToken.IsSymbol('.')) {    // .class
      parsingStatus = ParseClassSelector(dataMask, *selector);
    }
    else if (mToken.IsSymbol(':')) {    // :pseudo
      parsingStatus = ParsePseudoSelector(dataMask, *selector, false,
                                          getter_AddRefs(pseudoElement),
                                          getter_Transfers(pseudoElementArgs),
                                          &pseudoElementType);
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

  if (pseudoElementType == nsCSSPseudoElements::ePseudo_AnonBox) {
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

  // Pseudo-elements other than anonymous boxes are represented as
  // direct children ('>' combinator) of the rest of the selector.
  if (pseudoElement) {
    selector = aList->AddSelector('>');

    selector->mLowercaseTag.swap(pseudoElement);
    selector->mClassList = pseudoElementArgs.forget();
    selector->SetPseudoType(pseudoElementType);
  }

  return true;
}

css::Declaration*
CSSParserImpl::ParseDeclarationBlock(uint32_t aFlags, nsCSSContextType aContext)
{
  bool checkForBraces = (aFlags & eParseDeclaration_InBraces) != 0;

  if (checkForBraces) {
    if (!ExpectSymbol('{', true)) {
      REPORT_UNEXPECTED_TOKEN(PEBadDeclBlockStart);
      OUTPUT_ERROR();
      return nullptr;
    }
  }
  css::Declaration* declaration = new css::Declaration();
  mData.AssertInitialState();
  if (declaration) {
    for (;;) {
      bool changed;
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
  }
  return declaration;
}

// The types to pass to ParseColorComponent.  These correspond to the
// various datatypes that can go within rgb().
#define COLOR_TYPE_UNKNOWN 0
#define COLOR_TYPE_INTEGERS 1
#define COLOR_TYPE_PERCENTAGES 2

bool
CSSParserImpl::ParseColor(nsCSSValue& aValue)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorEOF);
    return false;
  }

  nsCSSToken* tk = &mToken;
  nscolor rgba;
  switch (tk->mType) {
    case eCSSToken_ID:
    case eCSSToken_Hash:
      // #xxyyzz
      if (NS_HexToRGB(tk->mIdent, &rgba)) {
        aValue.SetColorValue(rgba);
        return true;
      }
      break;

    case eCSSToken_Ident:
      if (NS_ColorNameToRGB(tk->mIdent, &rgba)) {
        aValue.SetStringValue(tk->mIdent, eCSSUnit_Ident);
        return true;
      }
      else {
        nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
        if (eCSSKeyword_UNKNOWN < keyword) { // known keyword
          int32_t value;
          if (nsCSSProps::FindKeyword(keyword, nsCSSProps::kColorKTable, value)) {
            aValue.SetIntValue(value, eCSSUnit_EnumColor);
            return true;
          }
        }
      }
      break;
    case eCSSToken_Function:
      if (mToken.mIdent.LowerCaseEqualsLiteral("rgb")) {
        // rgb ( component , component , component )
        uint8_t r, g, b;
        int32_t type = COLOR_TYPE_UNKNOWN;
        if (ParseColorComponent(r, type, ',') &&
            ParseColorComponent(g, type, ',') &&
            ParseColorComponent(b, type, ')')) {
          aValue.SetColorValue(NS_RGB(r,g,b));
          return true;
        }
        SkipUntil(')');
        return false;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-rgba") ||
               mToken.mIdent.LowerCaseEqualsLiteral("rgba")) {
        // rgba ( component , component , component , opacity )
        uint8_t r, g, b, a;
        int32_t type = COLOR_TYPE_UNKNOWN;
        if (ParseColorComponent(r, type, ',') &&
            ParseColorComponent(g, type, ',') &&
            ParseColorComponent(b, type, ',') &&
            ParseColorOpacity(a)) {
          aValue.SetColorValue(NS_RGBA(r, g, b, a));
          return true;
        }
        SkipUntil(')');
        return false;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("hsl")) {
        // hsl ( hue , saturation , lightness )
        // "hue" is a number, "saturation" and "lightness" are percentages.
        if (ParseHSLColor(rgba, ')')) {
          aValue.SetColorValue(rgba);
          return true;
        }
        SkipUntil(')');
        return false;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-hsla") ||
               mToken.mIdent.LowerCaseEqualsLiteral("hsla")) {
        // hsla ( hue , saturation , lightness , opacity )
        // "hue" is a number, "saturation" and "lightness" are percentages,
        // "opacity" is a number.
        uint8_t a;
        if (ParseHSLColor(rgba, ',') &&
            ParseColorOpacity(a)) {
          aValue.SetColorValue(NS_RGBA(NS_GET_R(rgba), NS_GET_G(rgba),
                                       NS_GET_B(rgba), a));
          return true;
        }
        SkipUntil(')');
        return false;
      }
      break;
    default:
      break;
  }

  // try 'xxyyzz' without '#' prefix for compatibility with IE and Nav4x (bug 23236 and 45804)
  if (mHashlessColorQuirk) {
    // - If the string starts with 'a-f', the nsCSSScanner builds the
    //   token as a eCSSToken_Ident and we can parse the string as a
    //   'xxyyzz' RGB color.
    // - If it only contains '0-9' digits, the token is a
    //   eCSSToken_Number and it must be converted back to a 6
    //   characters string to be parsed as a RGB color.
    // - If it starts with '0-9' and contains any 'a-f', the token is a
    //   eCSSToken_Dimension, the mNumber part must be converted back to
    //   a string and the mIdent part must be appended to that string so
    //   that the resulting string has 6 characters.
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
        if (tk->mIntegerValid) {
          PR_snprintf(buffer, sizeof(buffer), "%06d", tk->mInteger);
          str.AssignWithConversion(buffer);
        }
        break;

      case eCSSToken_Dimension:
        if (tk->mIdent.Length() <= 6) {
          PR_snprintf(buffer, sizeof(buffer), "%06.0f", tk->mNumber);
          nsAutoString temp;
          temp.AssignWithConversion(buffer);
          temp.Right(str, 6 - tk->mIdent.Length());
          str.Append(tk->mIdent);
        }
        break;
      default:
        // There is a whole bunch of cases that are
        // not handled by this switch.  Ignore them.
        break;
    }
    if (NS_HexToRGB(str, &rgba)) {
      aValue.SetColorValue(rgba);
      return true;
    }
  }

  // It's not a color
  REPORT_UNEXPECTED_TOKEN(PEColorNotColor);
  UngetToken();
  return false;
}

// aType will be set if we have already parsed other color components
// in this color spec
bool
CSSParserImpl::ParseColorComponent(uint8_t& aComponent,
                                   int32_t& aType,
                                   char aStop)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorComponentEOF);
    return false;
  }
  float value;
  nsCSSToken* tk = &mToken;
  switch (tk->mType) {
  case eCSSToken_Number:
    switch (aType) {
      case COLOR_TYPE_UNKNOWN:
        aType = COLOR_TYPE_INTEGERS;
        break;
      case COLOR_TYPE_INTEGERS:
        break;
      case COLOR_TYPE_PERCENTAGES:
        REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
        UngetToken();
        return false;
      default:
        NS_NOTREACHED("Someone forgot to add the new color component type in here");
    }

    if (!mToken.mIntegerValid) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedInt);
      UngetToken();
      return false;
    }
    value = tk->mNumber;
    break;
  case eCSSToken_Percentage:
    switch (aType) {
      case COLOR_TYPE_UNKNOWN:
        aType = COLOR_TYPE_PERCENTAGES;
        break;
      case COLOR_TYPE_INTEGERS:
        REPORT_UNEXPECTED_TOKEN(PEExpectedInt);
        UngetToken();
        return false;
      case COLOR_TYPE_PERCENTAGES:
        break;
      default:
        NS_NOTREACHED("Someone forgot to add the new color component type in here");
    }
    value = tk->mNumber * 255.0f;
    break;
  default:
    REPORT_UNEXPECTED_TOKEN(PEColorBadRGBContents);
    UngetToken();
    return false;
  }
  if (ExpectSymbol(aStop, true)) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    aComponent = NSToIntRound(value);
    return true;
  }
  REPORT_UNEXPECTED_TOKEN_CHAR(PEColorComponentBadTerm, aStop);
  return false;
}


bool
CSSParserImpl::ParseHSLColor(nscolor& aColor,
                             char aStop)
{
  float h, s, l;

  // Get the hue
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorHueEOF);
    return false;
  }
  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return false;
  }
  h = mToken.mNumber;
  h /= 360.0f;
  // hue values are wraparound
  h = h - floor(h);

  if (!ExpectSymbol(',', true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return false;
  }

  // Get the saturation
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorSaturationEOF);
    return false;
  }
  if (mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
    UngetToken();
    return false;
  }
  s = mToken.mNumber;
  if (s < 0.0f) s = 0.0f;
  if (s > 1.0f) s = 1.0f;

  if (!ExpectSymbol(',', true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return false;
  }

  // Get the lightness
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorLightnessEOF);
    return false;
  }
  if (mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
    UngetToken();
    return false;
  }
  l = mToken.mNumber;
  if (l < 0.0f) l = 0.0f;
  if (l > 1.0f) l = 1.0f;

  if (ExpectSymbol(aStop, true)) {
    aColor = NS_HSL2RGB(h, s, l);
    return true;
  }

  REPORT_UNEXPECTED_TOKEN_CHAR(PEColorComponentBadTerm, aStop);
  return false;
}


bool
CSSParserImpl::ParseColorOpacity(uint8_t& aOpacity)
{
  if (!GetToken(true)) {
    REPORT_UNEXPECTED_EOF(PEColorOpacityEOF);
    return false;
  }

  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return false;
  }

  if (mToken.mNumber < 0.0f) {
    mToken.mNumber = 0.0f;
  } else if (mToken.mNumber > 1.0f) {
    mToken.mNumber = 1.0f;
  }

  uint8_t value = nsStyleUtil::FloatToColorComponent(mToken.mNumber);
  // Need to compare to something slightly larger
  // than 0.5 due to floating point inaccuracies.
  NS_ASSERTION(fabs(255.0f*mToken.mNumber - value) <= 0.51f,
               "FloatToColorComponent did something weird");

  if (!ExpectSymbol(')', true)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
    return false;
  }

  aOpacity = value;

  return true;
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
        return true;  // Not a declaration, but don’t skip until ';'
      }
    }
    // Not a declaration...
    UngetToken();
    return false;
  }

  // Don't report property parse errors if we're inside a failing @supports
  // rule.
  nsAutoSuppressErrors suppressErrors(this, mInFailingSupportsRule);

  // Map property name to its ID and then parse the property
  nsCSSProperty propID = nsCSSProps::LookupProperty(propertyName,
                                                    nsCSSProps::eEnabled);
  if (eCSSProperty_UNKNOWN == propID ||
     (aContext == eCSSContext_Page &&
      !nsCSSProps::PropHasFlags(propID, CSS_PROPERTY_APPLIES_TO_PAGE_RULE))) { // unknown property
    if (!NonMozillaVendorIdentifier(propertyName)) {
      REPORT_UNEXPECTED_P(PEUnknownProperty, propertyName);
      REPORT_UNEXPECTED(PEDeclDropped);
      OUTPUT_ERROR();
    }

    return false;
  }
  if (! ParseProperty(propID)) {
    // XXX Much better to put stuff in the value parsers instead...
    REPORT_UNEXPECTED_P(PEValueParsingError, propertyName);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    mTempData.ClearProperty(propID);
    mTempData.AssertInitialState();
    return false;
  }
  CLEAR_ERROR();

  // Look for "!important".
  PriorityParsingStatus status;
  if ((aFlags & eParseDeclaration_AllowImportant) != 0) {
    status = ParsePriority();
  }
  else {
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
    mTempData.ClearProperty(propID);
    mTempData.AssertInitialState();
    return false;
  }

  *aChanged |= mData.TransferFromBlock(mTempData, propID,
                                       status == ePriority_Important,
                                       false, aMustCallValueAppended,
                                       aDeclaration);
  return true;
}

static const nsCSSProperty kBorderTopIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color
};
static const nsCSSProperty kBorderRightIDs[] = {
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_right_color_value,
  eCSSProperty_border_right_width,
  eCSSProperty_border_right_style,
  eCSSProperty_border_right_color
};
static const nsCSSProperty kBorderBottomIDs[] = {
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_color
};
static const nsCSSProperty kBorderLeftIDs[] = {
  eCSSProperty_border_left_width_value,
  eCSSProperty_border_left_style_value,
  eCSSProperty_border_left_color_value,
  eCSSProperty_border_left_width,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_color
};
static const nsCSSProperty kBorderStartIDs[] = {
  eCSSProperty_border_start_width_value,
  eCSSProperty_border_start_style_value,
  eCSSProperty_border_start_color_value,
  eCSSProperty_border_start_width,
  eCSSProperty_border_start_style,
  eCSSProperty_border_start_color
};
static const nsCSSProperty kBorderEndIDs[] = {
  eCSSProperty_border_end_width_value,
  eCSSProperty_border_end_style_value,
  eCSSProperty_border_end_color_value,
  eCSSProperty_border_end_width,
  eCSSProperty_border_end_style,
  eCSSProperty_border_end_color
};
static const nsCSSProperty kColumnRuleIDs[] = {
  eCSSProperty__moz_column_rule_width,
  eCSSProperty__moz_column_rule_style,
  eCSSProperty__moz_column_rule_color
};

bool
CSSParserImpl::ParseEnum(nsCSSValue& aValue,
                         const int32_t aKeywordTable[])
{
  nsSubstring* ident = NextIdent();
  if (nullptr == ident) {
    return false;
  }
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(*ident);
  if (eCSSKeyword_UNKNOWN < keyword) {
    int32_t value;
    if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
      aValue.SetIntValue(value, eCSSUnit_Enumerated);
      return true;
    }
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
  { STR_WITH_LEN("mozmm"), eCSSUnit_PhysicalMillimeter, VARIANT_LENGTH },
  { STR_WITH_LEN("vw"), eCSSUnit_ViewportWidth, VARIANT_LENGTH },
  { STR_WITH_LEN("vh"), eCSSUnit_ViewportHeight, VARIANT_LENGTH },
  { STR_WITH_LEN("vmin"), eCSSUnit_ViewportMin, VARIANT_LENGTH },
  { STR_WITH_LEN("vmax"), eCSSUnit_ViewportMax, VARIANT_LENGTH },
  { STR_WITH_LEN("pc"), eCSSUnit_Pica, VARIANT_LENGTH },
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
                                  int32_t aVariantMask,
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

    if (!mViewportUnitsEnabled &&
        (eCSSUnit_ViewportWidth == units  ||
         eCSSUnit_ViewportHeight == units ||
         eCSSUnit_ViewportMin == units    ||
         eCSSUnit_ViewportMax == units)) {
      // Viewport units aren't allowed right now, probably because we're
      // inside an @page declaration. Fail.
      return false;
    }

    if (i == ArrayLength(UnitData)) {
      // Unknown unit
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
  VARIANT_CALC

// Note that callers passing VARIANT_CALC in aVariantMask will get
// full-range parsing inside the calc() expression, and the code that
// computes the calc will be required to clamp the resulting value to an
// appropriate range.
bool
CSSParserImpl::ParseNonNegativeVariant(nsCSSValue& aValue,
                                       int32_t aVariantMask,
                                       const int32_t aKeywordTable[])
{
  // The variant mask must only contain non-numeric variants or the ones
  // that we specifically handle.
  NS_ABORT_IF_FALSE((aVariantMask & ~(VARIANT_ALL_NONNUMERIC |
                                      VARIANT_NUMBER |
                                      VARIANT_LENGTH |
                                      VARIANT_PERCENT |
                                      VARIANT_INTEGER)) == 0,
                    "need to update code below to handle additional variants");

  if (ParseVariant(aValue, aVariantMask, aKeywordTable)) {
    if (eCSSUnit_Number == aValue.GetUnit() ||
        aValue.IsLengthUnit()){
      if (aValue.GetFloatValue() < 0) {
        UngetToken();
        return false;
      }
    }
    else if (aValue.GetUnit() == eCSSUnit_Percent) {
      if (aValue.GetPercentValue() < 0) {
        UngetToken();
        return false;
      }
    } else if (aValue.GetUnit() == eCSSUnit_Integer) {
      if (aValue.GetIntValue() < 0) {
        UngetToken();
        return false;
      }
    }
    return true;
  }
  return false;
}

// Note that callers passing VARIANT_CALC in aVariantMask will get
// full-range parsing inside the calc() expression, and the code that
// computes the calc will be required to clamp the resulting value to an
// appropriate range.
bool
CSSParserImpl::ParseOneOrLargerVariant(nsCSSValue& aValue,
                                       int32_t aVariantMask,
                                       const int32_t aKeywordTable[])
{
  // The variant mask must only contain non-numeric variants or the ones
  // that we specifically handle.
  NS_ABORT_IF_FALSE((aVariantMask & ~(VARIANT_ALL_NONNUMERIC |
                                      VARIANT_NUMBER |
                                      VARIANT_INTEGER)) == 0,
                    "need to update code below to handle additional variants");

  if (ParseVariant(aValue, aVariantMask, aKeywordTable)) {
    if (aValue.GetUnit() == eCSSUnit_Integer) {
      if (aValue.GetIntValue() < 1) {
        UngetToken();
        return false;
      }
    } else if (eCSSUnit_Number == aValue.GetUnit()) {
      if (aValue.GetFloatValue() < 1.0f) {
        UngetToken();
        return false;
      }
    }
    return true;
  }
  return false;
}

// Assigns to aValue iff it returns true.
bool
CSSParserImpl::ParseVariant(nsCSSValue& aValue,
                            int32_t aVariantMask,
                            const int32_t aKeywordTable[])
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
  NS_ABORT_IF_FALSE(!(aVariantMask & VARIANT_IDENTIFIER) ||
                    !(aVariantMask & VARIANT_IDENTIFIER_NO_INHERIT),
                    "must not set both VARIANT_IDENTIFIER and "
                    "VARIANT_IDENTIFIER_NO_INHERIT");

  if (!GetToken(true)) {
    return false;
  }
  nsCSSToken* tk = &mToken;
  if (((aVariantMask & (VARIANT_AHK | VARIANT_NORMAL | VARIANT_NONE | VARIANT_ALL)) != 0) &&
      (eCSSToken_Ident == tk->mType)) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
    if (eCSSKeyword_UNKNOWN < keyword) { // known keyword
      if ((aVariantMask & VARIANT_AUTO) != 0) {
        if (eCSSKeyword_auto == keyword) {
          aValue.SetAutoValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_INHERIT) != 0) {
        // XXX Should we check IsParsingCompoundProperty, or do all
        // callers handle it?  (Not all callers set it, though, since
        // they want the quirks that are disabled by setting it.)
        if (eCSSKeyword_inherit == keyword) {
          aValue.SetInheritValue();
          return true;
        }
        else if (eCSSKeyword_initial == keyword) { // anything that can inherit can also take an initial val.
          aValue.SetInitialValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_NONE) != 0) {
        if (eCSSKeyword_none == keyword) {
          aValue.SetNoneValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_ALL) != 0) {
        if (eCSSKeyword_all == keyword) {
          aValue.SetAllValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_NORMAL) != 0) {
        if (eCSSKeyword_normal == keyword) {
          aValue.SetNormalValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_SYSFONT) != 0) {
        if (eCSSKeyword__moz_use_system_font == keyword &&
            !IsParsingCompoundProperty()) {
          aValue.SetSystemFontValue();
          return true;
        }
      }
      if ((aVariantMask & VARIANT_KEYWORD) != 0) {
        int32_t value;
        if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
          aValue.SetIntValue(value, eCSSUnit_Enumerated);
          return true;
        }
      }
    }
  }
  // Check VARIANT_NUMBER and VARIANT_INTEGER before VARIANT_LENGTH or
  // VARIANT_ZERO_ANGLE.
  if (((aVariantMask & VARIANT_NUMBER) != 0) &&
      (eCSSToken_Number == tk->mType)) {
    aValue.SetFloatValue(tk->mNumber, eCSSUnit_Number);
    return true;
  }
  if (((aVariantMask & VARIANT_INTEGER) != 0) &&
      (eCSSToken_Number == tk->mType) && tk->mIntegerValid) {
    aValue.SetIntValue(tk->mInteger, eCSSUnit_Integer);
    return true;
  }
  if (((aVariantMask & (VARIANT_LENGTH | VARIANT_ANGLE |
                        VARIANT_FREQUENCY | VARIANT_TIME)) != 0 &&
       eCSSToken_Dimension == tk->mType) ||
      ((aVariantMask & (VARIANT_LENGTH | VARIANT_ZERO_ANGLE)) != 0 &&
       eCSSToken_Number == tk->mType &&
       tk->mNumber == 0.0f)) {
    if (((aVariantMask & VARIANT_POSITIVE_DIMENSION) != 0 && 
         tk->mNumber <= 0.0) ||
        ((aVariantMask & VARIANT_NONNEGATIVE_DIMENSION) != 0 && 
         tk->mNumber < 0.0)) {
        UngetToken();
        return false;
    }
    if (TranslateDimension(aValue, aVariantMask, tk->mNumber, tk->mIdent)) {
      return true;
    }
    // Put the token back; we didn't parse it, so we shouldn't consume it
    UngetToken();
    return false;
  }
  if (((aVariantMask & VARIANT_PERCENT) != 0) &&
      (eCSSToken_Percentage == tk->mType)) {
    aValue.SetPercentValue(tk->mNumber);
    return true;
  }
  if (mUnitlessLengthQuirk) { // NONSTANDARD: Nav interprets unitless numbers as px
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return true;
    }
  }

  if (IsSVGMode() && !IsParsingCompoundProperty()) {
    // STANDARD: SVG Spec states that lengths and coordinates can be unitless
    // in which case they default to user-units (1 px = 1 user unit)
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return true;
    }
  }

  if (((aVariantMask & VARIANT_URL) != 0) &&
      eCSSToken_URL == tk->mType) {
    SetValueToURL(aValue, tk->mIdent);
    return true;
  }
  if ((aVariantMask & VARIANT_GRADIENT) != 0 &&
      eCSSToken_Function == tk->mType) {
    // a generated gradient
    nsDependentString tmp(tk->mIdent, 0);
    bool isLegacy = false;
    if (StringBeginsWith(tmp, NS_LITERAL_STRING("-moz-"))) {
      tmp.Rebind(tmp, 5);
      isLegacy = true;
    }
    bool isRepeating = false;
    if (StringBeginsWith(tmp, NS_LITERAL_STRING("repeating-"))) {
      tmp.Rebind(tmp, 10);
      isRepeating = true;
    }

    if (tmp.LowerCaseEqualsLiteral("linear-gradient")) {
      return ParseLinearGradient(aValue, isRepeating, isLegacy);
    }
    if (tmp.LowerCaseEqualsLiteral("radial-gradient")) {
      return ParseRadialGradient(aValue, isRepeating, isLegacy);
    }
  }
  if ((aVariantMask & VARIANT_IMAGE_RECT) != 0 &&
      eCSSToken_Function == tk->mType &&
      tk->mIdent.LowerCaseEqualsLiteral("-moz-image-rect")) {
    return ParseImageRect(aValue);
  }
  if ((aVariantMask & VARIANT_ELEMENT) != 0 &&
      eCSSToken_Function == tk->mType &&
      tk->mIdent.LowerCaseEqualsLiteral("-moz-element")) {
    return ParseElement(aValue);
  }
  if ((aVariantMask & VARIANT_COLOR) != 0) {
    if (mHashlessColorQuirk || // NONSTANDARD: Nav interprets 'xxyyzz' values even without '#' prefix
        (eCSSToken_ID == tk->mType) ||
        (eCSSToken_Hash == tk->mType) ||
        (eCSSToken_Ident == tk->mType) ||
        ((eCSSToken_Function == tk->mType) &&
         (tk->mIdent.LowerCaseEqualsLiteral("rgb") ||
          tk->mIdent.LowerCaseEqualsLiteral("hsl") ||
          tk->mIdent.LowerCaseEqualsLiteral("-moz-rgba") ||
          tk->mIdent.LowerCaseEqualsLiteral("-moz-hsla") ||
          tk->mIdent.LowerCaseEqualsLiteral("rgba") ||
          tk->mIdent.LowerCaseEqualsLiteral("hsla"))))
    {
      // Put token back so that parse color can get it
      UngetToken();
      if (ParseColor(aValue)) {
        return true;
      }
      return false;
    }
  }
  if (((aVariantMask & VARIANT_STRING) != 0) &&
      (eCSSToken_String == tk->mType)) {
    nsAutoString  buffer;
    buffer.Append(tk->mIdent);
    aValue.SetStringValue(buffer, eCSSUnit_String);
    return true;
  }
  if (((aVariantMask &
        (VARIANT_IDENTIFIER | VARIANT_IDENTIFIER_NO_INHERIT)) != 0) &&
      (eCSSToken_Ident == tk->mType) &&
      ((aVariantMask & VARIANT_IDENTIFIER) != 0 ||
       !(tk->mIdent.LowerCaseEqualsLiteral("inherit") ||
         tk->mIdent.LowerCaseEqualsLiteral("initial")))) {
    aValue.SetStringValue(tk->mIdent, eCSSUnit_Ident);
    return true;
  }
  if (((aVariantMask & VARIANT_COUNTER) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      (tk->mIdent.LowerCaseEqualsLiteral("counter") ||
       tk->mIdent.LowerCaseEqualsLiteral("counters"))) {
    return ParseCounter(aValue);
  }
  if (((aVariantMask & VARIANT_ATTR) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      tk->mIdent.LowerCaseEqualsLiteral("attr")) {
    if (!ParseAttr(aValue)) {
      SkipUntil(')');
      return false;
    }
    return true;
  }
  if (((aVariantMask & VARIANT_TIMING_FUNCTION) != 0) &&
      (eCSSToken_Function == tk->mType)) {
    if (tk->mIdent.LowerCaseEqualsLiteral("cubic-bezier")) {
      if (!ParseTransitionTimingFunctionValues(aValue)) {
        SkipUntil(')');
        return false;
      }
      return true;
    }
    if (tk->mIdent.LowerCaseEqualsLiteral("steps")) {
      if (!ParseTransitionStepTimingFunctionValues(aValue)) {
        SkipUntil(')');
        return false;
      }
      return true;
    }
  }
  if ((aVariantMask & VARIANT_CALC) &&
      (eCSSToken_Function == tk->mType) &&
      (tk->mIdent.LowerCaseEqualsLiteral("calc") ||
       tk->mIdent.LowerCaseEqualsLiteral("-moz-calc"))) {
    // calc() currently allows only lengths and percents inside it.
    return ParseCalc(aValue, aVariantMask & VARIANT_LP);
  }

  UngetToken();
  return false;
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

    nsRefPtr<nsCSSValue::Array> val =
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
    int32_t type = NS_STYLE_LIST_STYLE_DECIMAL;
    if (ExpectSymbol(',', true)) {
      if (!GetToken(true)) {
        break;
      }
      nsCSSKeyword keyword;
      if (eCSSToken_Ident != mToken.mType ||
          (keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent)) ==
            eCSSKeyword_UNKNOWN ||
          !nsCSSProps::FindKeyword(keyword, nsCSSProps::kListStyleKTable,
                                   type)) {
        UngetToken();
        break;
      }
    }

    int32_t typeItem = eCSSUnit_Counters == unit ? 2 : 1;
    val->Item(typeItem).SetIntValue(type, eCSSUnit_Enumerated);

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
      attr.Append(PRUnichar('|'));
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
CSSParserImpl::SetValueToURL(nsCSSValue& aValue, const nsString& aURL)
{
  if (!mSheetPrincipal) {
    NS_NOTREACHED("Codepaths that expect to parse URLs MUST pass in an "
                  "origin principal");
    return false;
  }

  nsRefPtr<nsStringBuffer> buffer(nsCSSValue::BufferFromString(aURL));

  // Note: urlVal retains its own reference to |buffer|.
  mozilla::css::URLValue *urlVal =
    new mozilla::css::URLValue(buffer, mBaseURI, mSheetURI, mSheetPrincipal);
  aValue.SetURLValue(urlVal);
  return true;
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
    if (!ParseNonNegativeVariant(top, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseNonNegativeVariant(right, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseNonNegativeVariant(bottom, VARIANT_SIDE, nullptr) ||
        !ExpectSymbol(',', true) ||
        !ParseNonNegativeVariant(left, VARIANT_SIDE, nullptr) ||
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
  // First check for inherit / initial
  nsCSSValue tmpVal;
  if (ParseVariant(tmpVal, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_flex_grow, tmpVal);
    AppendValue(eCSSProperty_flex_shrink, tmpVal);
    AppendValue(eCSSProperty_flex_basis, tmpVal);
    return true;
  }

  // Next, check for 'none' == '0 0 auto'
  if (ParseVariant(tmpVal, VARIANT_NONE, nullptr)) {
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
  if (!ParseNonNegativeVariant(tmpVal, flexBasisVariantMask | VARIANT_NUMBER,
                               nsCSSProps::kWidthKTable)) {
    // First component was not a valid flex-basis or flex-grow value. Fail.
    return false;
  }

  // Record what we just parsed as either flex-basis or flex-grow:
  bool wasFirstComponentFlexBasis = (tmpVal.GetUnit() != eCSSUnit_Number);
  (wasFirstComponentFlexBasis ? flexBasis : flexGrow) = tmpVal;

  // (b) If we didn't get flex-grow yet, parse _next_ component as flex-grow.
  bool doneParsing = false;
  if (wasFirstComponentFlexBasis) {
    if (ParseNonNegativeVariant(tmpVal, VARIANT_NUMBER, nullptr)) {
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
    if (ParseNonNegativeVariant(tmpVal, VARIANT_NUMBER, nullptr)) {
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
    if (!wasFirstComponentFlexBasis &&
        ParseNonNegativeVariant(tmpVal, flexBasisVariantMask,
                                nsCSSProps::kWidthKTable)) {
      flexBasis = tmpVal;
    }
  }

  AppendValue(eCSSProperty_flex_grow,   flexGrow);
  AppendValue(eCSSProperty_flex_shrink, flexShrink);
  AppendValue(eCSSProperty_flex_basis,  flexBasis);

  return true;
}

// <color-stop> : <color> [ <percentage> | <length> ]?
bool
CSSParserImpl::ParseColorStop(nsCSSValueGradient* aGradient)
{
  nsCSSValueGradientStop* stop = aGradient->mStops.AppendElement();
  if (!ParseVariant(stop->mColor, VARIANT_COLOR, nullptr)) {
    return false;
  }

  // Stop positions do not have to fall between the starting-point and
  // ending-point, so we don't use ParseNonNegativeVariant.
  if (!ParseVariant(stop->mLocation, VARIANT_LP | VARIANT_CALC, nullptr)) {
    stop->mLocation.SetNoneValue();
  }
  return true;
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
CSSParserImpl::ParseLinearGradient(nsCSSValue& aValue, bool aIsRepeating,
                                   bool aIsLegacy)
{
  nsRefPtr<nsCSSValueGradient> cssGradient
    = new nsCSSValueGradient(false, aIsRepeating);

  if (!GetToken(true)) {
    return false;
  }

  if (mToken.mType == eCSSToken_Ident &&
      mToken.mIdent.LowerCaseEqualsLiteral("to")) {

    // "to" syntax doesn't allow explicit "center"
    if (!ParseBoxPositionValues(cssGradient->mBgPos, false, false)) {
      SkipUntil(')');
      return false;
    }

    // [ to [left | right] || [top | bottom] ] ,
    const nsCSSValue& xValue = cssGradient->mBgPos.mXValue;
    const nsCSSValue& yValue = cssGradient->mBgPos.mYValue;
    if (xValue.GetUnit() != eCSSUnit_Enumerated ||
        !(xValue.GetIntValue() & (NS_STYLE_BG_POSITION_LEFT |
                                  NS_STYLE_BG_POSITION_CENTER |
                                  NS_STYLE_BG_POSITION_RIGHT)) ||
        yValue.GetUnit() != eCSSUnit_Enumerated ||
        !(yValue.GetIntValue() & (NS_STYLE_BG_POSITION_TOP |
                                  NS_STYLE_BG_POSITION_CENTER |
                                  NS_STYLE_BG_POSITION_BOTTOM))) {
      SkipUntil(')');
      return false;
    }

    if (!ExpectSymbol(',', true)) {
      SkipUntil(')');
      return false;
    }

    return ParseGradientColorStops(cssGradient, aValue);
  }

  if (!aIsLegacy) {
    UngetToken();

    // <angle> ,
    if (ParseVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr) &&
        !ExpectSymbol(',', true)) {
      SkipUntil(')');
      return false;
    }

    return ParseGradientColorStops(cssGradient, aValue);
  }

  nsCSSTokenType ty = mToken.mType;
  nsString id = mToken.mIdent;
  UngetToken();

  // <legacy-gradient-line>
  bool haveGradientLine = IsLegacyGradientLine(ty, id);
  if (haveGradientLine) {
    cssGradient->mIsLegacySyntax = true;
    bool haveAngle =
      ParseVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr);

    // if we got an angle, we might now have a comma, ending the gradient-line
    if (!haveAngle || !ExpectSymbol(',', true)) {
      if (!ParseBoxPositionValues(cssGradient->mBgPos, false)) {
        SkipUntil(')');
        return false;
      }

      if (!ExpectSymbol(',', true) &&
          // if we didn't already get an angle, we might have one now,
          // otherwise it's an error
          (haveAngle ||
           !ParseVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr) ||
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
CSSParserImpl::ParseRadialGradient(nsCSSValue& aValue, bool aIsRepeating,
                                   bool aIsLegacy)
{
  nsRefPtr<nsCSSValueGradient> cssGradient
    = new nsCSSValueGradient(true, aIsRepeating);

  // [ <shape> || <size> ]
  bool haveShape =
    ParseVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                 nsCSSProps::kRadialGradientShapeKTable);

  bool haveSize = ParseVariant(cssGradient->GetRadialSize(), VARIANT_KEYWORD,
                               aIsLegacy ?
                               nsCSSProps::kRadialGradientLegacySizeKTable :
                               nsCSSProps::kRadialGradientSizeKTable);
  if (haveSize) {
    if (!haveShape) {
      // <size> <shape>
      haveShape = ParseVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                               nsCSSProps::kRadialGradientShapeKTable);
    }
  } else if (!aIsLegacy) {
    // <length> | [<length> | <percentage>]{2}
    haveSize =
      ParseNonNegativeVariant(cssGradient->GetRadiusX(), VARIANT_LP, nullptr);
    if (haveSize) {
      // vertical extent is optional
      bool haveYSize =
        ParseNonNegativeVariant(cssGradient->GetRadiusY(), VARIANT_LP, nullptr);
      if (!haveShape) {
        nsCSSValue shapeValue;
        haveShape = ParseVariant(shapeValue, VARIANT_KEYWORD,
                                 nsCSSProps::kRadialGradientShapeKTable);
      }
      int32_t shape =
        cssGradient->GetRadialShape().GetUnit() == eCSSUnit_Enumerated ?
        cssGradient->GetRadialShape().GetIntValue() : -1;
      if (haveYSize
            ? shape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR
            : cssGradient->GetRadiusX().GetUnit() == eCSSUnit_Percent ||
              shape == NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL) {
        SkipUntil(')');
        return false;
      }
      cssGradient->mIsExplicitSize = true;
    }
  }

  if ((haveShape || haveSize) && ExpectSymbol(',', true)) {
    // [ <shape> || <size> ] ,
    return ParseGradientColorStops(cssGradient, aValue);
  }

  if (!GetToken(true)) {
    return false;
  }

  if (!aIsLegacy) {
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
    bool haveAngle =
      ParseVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr);

    // if we got an angle, we might now have a comma, ending the gradient-line
    if (!haveAngle || !ExpectSymbol(',', true)) {
      if (!ParseBoxPositionValues(cssGradient->mBgPos, false)) {
        SkipUntil(')');
        return false;
      }

      if (!ExpectSymbol(',', true) &&
          // if we didn't already get an angle, we might have one now,
          // otherwise it's an error
          (haveAngle ||
           !ParseVariant(cssGradient->mAngle, VARIANT_ANGLE, nullptr) ||
           // now we better have a comma
           !ExpectSymbol(',', true))) {
        SkipUntil(')');
        return false;
      }
    }

    if (cssGradient->mAngle.GetUnit() != eCSSUnit_None) {
      cssGradient->mIsLegacySyntax = true;
    }
  }

  // radial gradients might have a shape and size here for legacy syntax
  if (!haveShape && !haveSize) {
    haveShape =
      ParseVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
                   nsCSSProps::kRadialGradientShapeKTable);
    haveSize =
      ParseVariant(cssGradient->GetRadialSize(), VARIANT_KEYWORD,
                   nsCSSProps::kRadialGradientLegacySizeKTable);

    // could be in either order
    if (!haveShape) {
      haveShape =
        ParseVariant(cssGradient->GetRadialShape(), VARIANT_KEYWORD,
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
    if (aId.LowerCaseEqualsLiteral("calc") ||
        aId.LowerCaseEqualsLiteral("-moz-calc")) {
      haveGradientLine = true;
      break;
    }
    // fall through
  case eCSSToken_ID:
  case eCSSToken_Hash:
    // this is a color
    break;

  case eCSSToken_Ident: {
    // This is only a gradient line if it's a box position keyword.
    nsCSSKeyword kw = nsCSSKeywords::LookupKeyword(aId);
    int32_t junk;
    if (kw != eCSSKeyword_UNKNOWN &&
        nsCSSProps::FindKeyword(kw, nsCSSProps::kBackgroundPositionKTable,
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

  aValue.SetGradientValue(aGradient);
  return true;
}

int32_t
CSSParserImpl::ParseChoice(nsCSSValue aValues[],
                           const nsCSSProperty aPropIDs[], int32_t aNumIDs)
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
        if (ParseSingleValueProperty(aValues[index], aPropIDs[index])) {
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
    }
    else {  // more than one value, verify no inherits or initials
      for (loop = 0; loop < aNumIDs; loop++) {
        if (eCSSUnit_Inherit == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
        else if (eCSSUnit_Initial == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
      }
    }
  }
  return found;
}

void
CSSParserImpl::AppendValue(nsCSSProperty aPropID, const nsCSSValue& aValue)
{
  mTempData.AddLonghandProperty(aPropID, aValue);
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values.
 */
bool
CSSParserImpl::ParseBoxProperties(const nsCSSProperty aPropIDs[])
{
  // Get up to four values for the property
  int32_t count = 0;
  nsCSSRect result;
  NS_FOR_CSS_SIDES (index) {
    if (! ParseSingleValueProperty(result.*(nsCSSRect::sides[index]),
                                   aPropIDs[index])) {
      break;
    }
    count++;
  }
  if ((count == 0) || (false == ExpectEndProperty())) {
    return false;
  }

  if (1 < count) { // verify no more than single inherit or initial
    NS_FOR_CSS_SIDES (index) {
      nsCSSUnit unit = (result.*(nsCSSRect::sides[index])).GetUnit();
      if (eCSSUnit_Inherit == unit || eCSSUnit_Initial == unit) {
        return false;
      }
    }
  }

  // Provide missing values by replicating some of the values found
  switch (count) {
    case 1: // Make right == top
      result.mRight = result.mTop;
    case 2: // Make bottom == top
      result.mBottom = result.mTop;
    case 3: // Make left == right
      result.mLeft = result.mRight;
  }

  NS_FOR_CSS_SIDES (index) {
    AppendValue(aPropIDs[index], result.*(nsCSSRect::sides[index]));
  }
  return true;
}

// Similar to ParseBoxProperties, except there is only one property
// with the result as its value, not four. Requires values be nonnegative.
bool
CSSParserImpl::ParseGroupedBoxProperty(int32_t aVariantMask,
                                       /** outparam */ nsCSSValue& aValue)
{
  nsCSSRect& result = aValue.SetRectValue();

  int32_t count = 0;
  NS_FOR_CSS_SIDES (index) {
    if (!ParseNonNegativeVariant(result.*(nsCSSRect::sides[index]),
                                 aVariantMask, nullptr)) {
      break;
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
    case 2: // Make bottom == top
      result.mBottom = result.mTop;
    case 3: // Make left == right
      result.mLeft = result.mRight;
  }

  return true;
}

bool
CSSParserImpl::ParseDirectionalBoxProperty(nsCSSProperty aProperty,
                                           int32_t aSourceType)
{
  const nsCSSProperty* subprops = nsCSSProps::SubpropertyEntryFor(aProperty);
  NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
               "not box property with physical vs. logical cascading");
  nsCSSValue value;
  if (!ParseSingleValueProperty(value, subprops[0]) ||
      !ExpectEndProperty())
    return false;

  AppendValue(subprops[0], value);
  nsCSSValue typeVal(aSourceType, eCSSUnit_Enumerated);
  AppendValue(subprops[1], typeVal);
  AppendValue(subprops[2], typeVal);
  return true;
}

bool
CSSParserImpl::ParseBoxCornerRadius(nsCSSProperty aPropID)
{
  nsCSSValue dimenX, dimenY;
  // required first value
  if (! ParseNonNegativeVariant(dimenX, VARIANT_HLP | VARIANT_CALC, nullptr))
    return false;

  // optional second value (forbidden if first value is inherit/initial)
  if (dimenX.GetUnit() != eCSSUnit_Inherit &&
      dimenX.GetUnit() != eCSSUnit_Initial) {
    ParseNonNegativeVariant(dimenY, VARIANT_LP | VARIANT_CALC, nullptr);
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
CSSParserImpl::ParseBoxCornerRadii(const nsCSSProperty aPropIDs[])
{
  // Rectangles are used as scratch storage.
  // top => top-left, right => top-right,
  // bottom => bottom-right, left => bottom-left.
  nsCSSRect dimenX, dimenY;
  int32_t countX = 0, countY = 0;

  NS_FOR_CSS_SIDES (side) {
    if (! ParseNonNegativeVariant(dimenX.*nsCSSRect::sides[side],
                                  (side > 0 ? 0 : VARIANT_INHERIT) |
                                    VARIANT_LP | VARIANT_CALC,
                                  nullptr))
      break;
    countX++;
  }
  if (countX == 0)
    return false;

  if (ExpectSymbol('/', true)) {
    NS_FOR_CSS_SIDES (side) {
      if (! ParseNonNegativeVariant(dimenY.*nsCSSRect::sides[side],
                                    VARIANT_LP | VARIANT_CALC, nullptr))
        break;
      countY++;
    }
    if (countY == 0)
      return false;
  }
  if (!ExpectEndProperty())
    return false;

  // if 'initial' or 'inherit' was used, it must be the only value
  if (countX > 1 || countY > 0) {
    nsCSSUnit unit = dimenX.mTop.GetUnit();
    if (eCSSUnit_Inherit == unit || eCSSUnit_Initial == unit)
      return false;
  }

  // if we have no Y-values, use the X-values
  if (countY == 0) {
    dimenY = dimenX;
    countY = countX;
  }

  // Provide missing values by replicating some of the values found
  switch (countX) {
    case 1: dimenX.mRight = dimenX.mTop;  // top-right same as top-left, and
    case 2: dimenX.mBottom = dimenX.mTop; // bottom-right same as top-left, and 
    case 3: dimenX.mLeft = dimenX.mRight; // bottom-left same as top-right
  }

  switch (countY) {
    case 1: dimenY.mRight = dimenY.mTop;  // top-right same as top-left, and
    case 2: dimenY.mBottom = dimenY.mTop; // bottom-right same as top-left, and 
    case 3: dimenY.mLeft = dimenY.mRight; // bottom-left same as top-right
  }

  NS_FOR_CSS_SIDES(side) {
    nsCSSValue& x = dimenX.*nsCSSRect::sides[side];
    nsCSSValue& y = dimenY.*nsCSSRect::sides[side];

    if (x == y) {
      AppendValue(aPropIDs[side], x);
    } else {
      nsCSSValue pair;
      pair.SetPairValue(x, y);
      AppendValue(aPropIDs[side], pair);
    }
  }
  return true;
}

// These must be in CSS order (top,right,bottom,left) for indexing to work
static const nsCSSProperty kBorderStyleIDs[] = {
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style_value
};
static const nsCSSProperty kBorderWidthIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width_value
};
static const nsCSSProperty kBorderColorIDs[] = {
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color_value,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color_value
};
static const nsCSSProperty kBorderRadiusIDs[] = {
  eCSSProperty_border_top_left_radius,
  eCSSProperty_border_top_right_radius,
  eCSSProperty_border_bottom_right_radius,
  eCSSProperty_border_bottom_left_radius
};
static const nsCSSProperty kOutlineRadiusIDs[] = {
  eCSSProperty__moz_outline_radius_topLeft,
  eCSSProperty__moz_outline_radius_topRight,
  eCSSProperty__moz_outline_radius_bottomRight,
  eCSSProperty__moz_outline_radius_bottomLeft
};

bool
CSSParserImpl::ParseProperty(nsCSSProperty aPropID)
{
  // Can't use AutoRestore<bool> because it's a bitfield.
  NS_ABORT_IF_FALSE(!mHashlessColorQuirk,
                    "hashless color quirk should not be set");
  NS_ABORT_IF_FALSE(!mUnitlessLengthQuirk,
                    "unitless length quirk should not be set");
  if (mNavQuirkMode) {
    mHashlessColorQuirk =
      nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_HASHLESS_COLOR_QUIRK);
    mUnitlessLengthQuirk =
      nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_UNITLESS_LENGTH_QUIRK);
  }

  NS_ASSERTION(aPropID < eCSSProperty_COUNT, "index out of range");
  bool result;
  switch (nsCSSProps::PropertyParseType(aPropID)) {
    case CSS_PROPERTY_PARSE_INACCESSIBLE: {
      // The user can't use these
      REPORT_UNEXPECTED(PEInaccessibleProperty2);
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
      if (ParseSingleValueProperty(value, aPropID)) {
        if (ExpectEndProperty()) {
          AppendValue(aPropID, value);
          result = true;
        }
        // XXX Report errors?
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
      NS_ABORT_IF_FALSE(false,
                        "Property's flags field in nsCSSPropList.h is missing "
                        "one of the CSS_PROPERTY_PARSE_* constants");
      break;
    }
  }

  if (mNavQuirkMode) {
    mHashlessColorQuirk = false;
    mUnitlessLengthQuirk = false;
  }

  return result;
}

bool
CSSParserImpl::ParsePropertyByFunction(nsCSSProperty aPropID)
{
  switch (aPropID) {  // handle shorthand or multiple properties
  case eCSSProperty_background:
    return ParseBackground();
  case eCSSProperty_background_repeat:
    return ParseBackgroundRepeat();
  case eCSSProperty_background_position:
    return ParseBackgroundPosition();
  case eCSSProperty_background_size:
    return ParseBackgroundSize();
  case eCSSProperty_border:
    return ParseBorderSide(kBorderTopIDs, true);
  case eCSSProperty_border_color:
    return ParseBorderColor();
  case eCSSProperty_border_spacing:
    return ParseBorderSpacing();
  case eCSSProperty_border_style:
    return ParseBorderStyle();
  case eCSSProperty_border_bottom:
    return ParseBorderSide(kBorderBottomIDs, false);
  case eCSSProperty_border_end:
    return ParseDirectionalBorderSide(kBorderEndIDs,
                                      NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_left:
    return ParseDirectionalBorderSide(kBorderLeftIDs,
                                      NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_right:
    return ParseDirectionalBorderSide(kBorderRightIDs,
                                      NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_start:
    return ParseDirectionalBorderSide(kBorderStartIDs,
                                      NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_top:
    return ParseBorderSide(kBorderTopIDs, false);
  case eCSSProperty_border_bottom_colors:
  case eCSSProperty_border_left_colors:
  case eCSSProperty_border_right_colors:
  case eCSSProperty_border_top_colors:
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
  case eCSSProperty_border_end_color:
    return ParseDirectionalBoxProperty(eCSSProperty_border_end_color,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_left_color:
    return ParseDirectionalBoxProperty(eCSSProperty_border_left_color,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_right_color:
    return ParseDirectionalBoxProperty(eCSSProperty_border_right_color,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_start_color:
    return ParseDirectionalBoxProperty(eCSSProperty_border_start_color,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_end_width:
    return ParseDirectionalBoxProperty(eCSSProperty_border_end_width,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_left_width:
    return ParseDirectionalBoxProperty(eCSSProperty_border_left_width,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_right_width:
    return ParseDirectionalBoxProperty(eCSSProperty_border_right_width,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_start_width:
    return ParseDirectionalBoxProperty(eCSSProperty_border_start_width,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_end_style:
    return ParseDirectionalBoxProperty(eCSSProperty_border_end_style,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_left_style:
    return ParseDirectionalBoxProperty(eCSSProperty_border_left_style,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_right_style:
    return ParseDirectionalBoxProperty(eCSSProperty_border_right_style,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_border_start_style:
    return ParseDirectionalBoxProperty(eCSSProperty_border_start_style,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_border_radius:
    return ParseBoxCornerRadii(kBorderRadiusIDs);
  case eCSSProperty__moz_outline_radius:
    return ParseBoxCornerRadii(kOutlineRadiusIDs);

  case eCSSProperty_border_top_left_radius:
  case eCSSProperty_border_top_right_radius:
  case eCSSProperty_border_bottom_right_radius:
  case eCSSProperty_border_bottom_left_radius:
  case eCSSProperty__moz_outline_radius_topLeft:
  case eCSSProperty__moz_outline_radius_topRight:
  case eCSSProperty__moz_outline_radius_bottomRight:
  case eCSSProperty__moz_outline_radius_bottomLeft:
    return ParseBoxCornerRadius(aPropID);

  case eCSSProperty_box_shadow:
  case eCSSProperty_text_shadow:
    return ParseShadowList(aPropID);

  case eCSSProperty_clip:
    return ParseRect(eCSSProperty_clip);
  case eCSSProperty__moz_columns:
    return ParseColumns();
  case eCSSProperty__moz_column_rule:
    return ParseBorderSide(kColumnRuleIDs, false);
  case eCSSProperty_content:
    return ParseContent();
  case eCSSProperty_counter_increment:
  case eCSSProperty_counter_reset:
    return ParseCounterData(aPropID);
  case eCSSProperty_cursor:
    return ParseCursor();
  case eCSSProperty_filter:
    return ParseFilter();
  case eCSSProperty_flex:
    return ParseFlex();
  case eCSSProperty_font:
    return ParseFont();
  case eCSSProperty_image_region:
    return ParseRect(eCSSProperty_image_region);
  case eCSSProperty_list_style:
    return ParseListStyle();
  case eCSSProperty_margin:
    return ParseMargin();
  case eCSSProperty_margin_end:
    return ParseDirectionalBoxProperty(eCSSProperty_margin_end,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_margin_left:
    return ParseDirectionalBoxProperty(eCSSProperty_margin_left,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_margin_right:
    return ParseDirectionalBoxProperty(eCSSProperty_margin_right,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_margin_start:
    return ParseDirectionalBoxProperty(eCSSProperty_margin_start,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_outline:
    return ParseOutline();
  case eCSSProperty_overflow:
    return ParseOverflow();
  case eCSSProperty_padding:
    return ParsePadding();
  case eCSSProperty_padding_end:
    return ParseDirectionalBoxProperty(eCSSProperty_padding_end,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_padding_left:
    return ParseDirectionalBoxProperty(eCSSProperty_padding_left,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_padding_right:
    return ParseDirectionalBoxProperty(eCSSProperty_padding_right,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_padding_start:
    return ParseDirectionalBoxProperty(eCSSProperty_padding_start,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_quotes:
    return ParseQuotes();
  case eCSSProperty_size:
    return ParseSize();
  case eCSSProperty_text_decoration:
    return ParseTextDecoration();
  case eCSSProperty_transform:
    return ParseTransform(false);
  case eCSSProperty__moz_transform:
    return ParseTransform(true);
  case eCSSProperty_transform_origin:
    return ParseTransformOrigin(false);
  case eCSSProperty_perspective_origin:
    return ParseTransformOrigin(true);
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
  default:
    NS_ABORT_IF_FALSE(false, "should not be called");
    return false;
  }
}

// Bits used in determining which background position info we have
#define BG_CENTER  NS_STYLE_BG_POSITION_CENTER
#define BG_TOP     NS_STYLE_BG_POSITION_TOP
#define BG_BOTTOM  NS_STYLE_BG_POSITION_BOTTOM
#define BG_LEFT    NS_STYLE_BG_POSITION_LEFT
#define BG_RIGHT   NS_STYLE_BG_POSITION_RIGHT
#define BG_CTB    (BG_CENTER | BG_TOP | BG_BOTTOM)
#define BG_TB     (BG_TOP | BG_BOTTOM)
#define BG_CLR    (BG_CENTER | BG_LEFT | BG_RIGHT)
#define BG_LR     (BG_LEFT | BG_RIGHT)

bool
CSSParserImpl::ParseSingleValueProperty(nsCSSValue& aValue,
                                        nsCSSProperty aPropID)
{
  if (aPropID == eCSSPropertyExtra_x_none_value) {
    return ParseVariant(aValue, VARIANT_NONE | VARIANT_INHERIT, nullptr);
  }

  if (aPropID == eCSSPropertyExtra_x_auto_value) {
    return ParseVariant(aValue, VARIANT_AUTO | VARIANT_INHERIT, nullptr);
  }

  if (aPropID < 0 || aPropID >= eCSSProperty_COUNT_no_shorthands) {
    NS_ABORT_IF_FALSE(false, "not a single value property");
    return false;
  }

  if (nsCSSProps::PropHasFlags(aPropID, CSS_PROPERTY_VALUE_PARSER_FUNCTION)) {
    switch (aPropID) {
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
      case eCSSProperty_font_weight:
        return ParseFontWeight(aValue);
      case eCSSProperty_marks:
        return ParseMarks(aValue);
      case eCSSProperty_text_decoration_line:
        return ParseTextDecorationLine(aValue);
      case eCSSProperty_text_overflow:
        return ParseTextOverflow(aValue);
      default:
        NS_ABORT_IF_FALSE(false, "should not reach here");
        return false;
    }
  }

  uint32_t variant = nsCSSProps::ParserVariant(aPropID);
  if (variant == 0) {
    NS_ABORT_IF_FALSE(false, "not a single value property");
    return false;
  }

  // We only allow 'script-level' when unsafe rules are enabled, because
  // otherwise it could interfere with rulenode optimizations if used in
  // a non-MathML-enabled document.
  if (aPropID == eCSSProperty_script_level && !mUnsafeRulesEnabled)
    return false;

  const int32_t *kwtable = nsCSSProps::kKeywordTableTable[aPropID];
  switch (nsCSSProps::ValueRestrictions(aPropID)) {
    default:
      NS_ABORT_IF_FALSE(false, "should not be reached");
    case 0:
      return ParseVariant(aValue, variant, kwtable);
    case CSS_PROPERTY_VALUE_NONNEGATIVE:
      return ParseNonNegativeVariant(aValue, variant, kwtable);
    case CSS_PROPERTY_VALUE_AT_LEAST_ONE:
      return ParseOneOrLargerVariant(aValue, variant, kwtable);
  }
}

// nsFont::EnumerateFamilies callback for ParseFontDescriptorValue
struct MOZ_STACK_CLASS ExtractFirstFamilyData {
  nsAutoString mFamilyName;
  bool mGood;
  ExtractFirstFamilyData() : mFamilyName(), mGood(false) {}
};

static bool
ExtractFirstFamily(const nsString& aFamily,
                   bool aGeneric,
                   void* aData)
{
  ExtractFirstFamilyData* realData = (ExtractFirstFamilyData*) aData;
  if (aGeneric || realData->mFamilyName.Length() > 0) {
    realData->mGood = false;
    return false;
  }
  realData->mFamilyName.Assign(aFamily);
  realData->mGood = true;
  return true;
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
    if (!ParseFamily(aValue) ||
        aValue.GetUnit() != eCSSUnit_Families)
      return false;

    // the style parameters to the nsFont constructor are ignored,
    // because it's only being used to call EnumerateFamilies
    nsAutoString valueStr;
    aValue.GetStringValue(valueStr);
    nsFont font(valueStr, 0, 0, 0, 0, 0, 0);
    ExtractFirstFamilyData dat;

    font.EnumerateFamilies(ExtractFirstFamily, (void*) &dat);
    if (!dat.mGood)
      return false;

    aValue.SetStringValue(dat.mFamilyName, eCSSUnit_String);
    return true;
  }

  case eCSSFontDesc_Style:
    // property is VARIANT_HMK|VARIANT_SYSFONT
    return ParseVariant(aValue, VARIANT_KEYWORD | VARIANT_NORMAL,
                        nsCSSProps::kFontStyleKTable);

  case eCSSFontDesc_Weight:
    return (ParseFontWeight(aValue) &&
            aValue.GetUnit() != eCSSUnit_Inherit &&
            aValue.GetUnit() != eCSSUnit_Initial &&
            (aValue.GetUnit() != eCSSUnit_Enumerated ||
             (aValue.GetIntValue() != NS_STYLE_FONT_WEIGHT_BOLDER &&
              aValue.GetIntValue() != NS_STYLE_FONT_WEIGHT_LIGHTER)));

  case eCSSFontDesc_Stretch:
    // property is VARIANT_HK|VARIANT_SYSFONT
    return ParseVariant(aValue, VARIANT_KEYWORD,
                        nsCSSProps::kFontStretchKTable);

    // These two are unique to @font-face and have their own special grammar.
  case eCSSFontDesc_Src:
    return ParseFontSrc(aValue);

  case eCSSFontDesc_UnicodeRange:
    return ParseFontRanges(aValue);

  case eCSSFontDesc_FontFeatureSettings:
    return ParseFontFeatureSettings(aValue);

  case eCSSFontDesc_FontLanguageOverride:
    return ParseVariant(aValue, VARIANT_NORMAL | VARIANT_STRING, nullptr);

  case eCSSFontDesc_UNKNOWN:
  case eCSSFontDesc_COUNT:
    NS_NOTREACHED("bad nsCSSFontDesc code");
  }
  // explicitly do NOT have a default case to let the compiler
  // help find missing descriptors
  return false;
}

void
CSSParserImpl::InitBoxPropsAsPhysical(const nsCSSProperty *aSourceProperties)
{
  nsCSSValue physical(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  for (const nsCSSProperty *prop = aSourceProperties;
       *prop != eCSSProperty_UNKNOWN; ++prop) {
    AppendValue(*prop, physical);
  }
}

static nsCSSValue
BoxPositionMaskToCSSValue(int32_t aMask, bool isX)
{
  int32_t val = NS_STYLE_BG_POSITION_CENTER;
  if (isX) {
    if (aMask & BG_LEFT) {
      val = NS_STYLE_BG_POSITION_LEFT;
    }
    else if (aMask & BG_RIGHT) {
      val = NS_STYLE_BG_POSITION_RIGHT;
    }
  }
  else {
    if (aMask & BG_TOP) {
      val = NS_STYLE_BG_POSITION_TOP;
    }
    else if (aMask & BG_BOTTOM) {
      val = NS_STYLE_BG_POSITION_BOTTOM;
    }
  }

  return nsCSSValue(val, eCSSUnit_Enumerated);
}

bool
CSSParserImpl::ParseBackground()
{
  nsAutoParseCompoundProperty compound(this);

  // background-color can only be set once, so it's not a list.
  nsCSSValue color;

  // Check first for inherit/initial.
  if (ParseVariant(color, VARIANT_INHERIT, nullptr)) {
    // must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
    for (const nsCSSProperty* subprops =
           nsCSSProps::SubpropertyEntryFor(eCSSProperty_background);
         *subprops != eCSSProperty_UNKNOWN; ++subprops) {
      AppendValue(*subprops, color);
    }
    return true;
  }

  nsCSSValue image, repeat, attachment, clip, origin, position, size;
  BackgroundParseState state(color, image.SetListValue(), 
                             repeat.SetPairListValue(),
                             attachment.SetListValue(), clip.SetListValue(),
                             origin.SetListValue(), position.SetListValue(),
                             size.SetPairListValue());

  for (;;) {
    if (!ParseBackgroundItem(state)) {
      return false;
    }
    if (CheckEndProperty()) {
      break;
    }
    // If we saw a color, this must be the last item.
    if (color.GetUnit() != eCSSUnit_Null) {
      REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
      return false;
    }
    // Otherwise, a comma is mandatory.
    if (!ExpectSymbol(',', true)) {
      return false;
    }
    // Chain another entry on all the lists.
    state.mImage->mNext = new nsCSSValueList;
    state.mImage = state.mImage->mNext;
    state.mRepeat->mNext = new nsCSSValuePairList;
    state.mRepeat = state.mRepeat->mNext;
    state.mAttachment->mNext = new nsCSSValueList;
    state.mAttachment = state.mAttachment->mNext;
    state.mClip->mNext = new nsCSSValueList;
    state.mClip = state.mClip->mNext;
    state.mOrigin->mNext = new nsCSSValueList;
    state.mOrigin = state.mOrigin->mNext;
    state.mPosition->mNext = new nsCSSValueList;
    state.mPosition = state.mPosition->mNext;
    state.mSize->mNext = new nsCSSValuePairList;
    state.mSize = state.mSize->mNext;
  }

  // If we get to this point without seeing a color, provide a default.
  if (color.GetUnit() == eCSSUnit_Null) {
    color.SetColorValue(NS_RGBA(0,0,0,0));
  }

  AppendValue(eCSSProperty_background_image,      image);
  AppendValue(eCSSProperty_background_repeat,     repeat);
  AppendValue(eCSSProperty_background_attachment, attachment);
  AppendValue(eCSSProperty_background_clip,       clip);
  AppendValue(eCSSProperty_background_origin,     origin);
  AppendValue(eCSSProperty_background_position,   position);
  AppendValue(eCSSProperty_background_size,       size);
  AppendValue(eCSSProperty_background_color,      color);
  return true;
}

// Parse one item of the background shorthand property.
bool
CSSParserImpl::ParseBackgroundItem(CSSParserImpl::BackgroundParseState& aState)

{
  // Fill in the values that the shorthand will set if we don't find
  // other values.
  aState.mImage->mValue.SetNoneValue();
  aState.mRepeat->mXValue.SetIntValue(NS_STYLE_BG_REPEAT_REPEAT,
                                      eCSSUnit_Enumerated);
  aState.mRepeat->mYValue.Reset();
  aState.mAttachment->mValue.SetIntValue(NS_STYLE_BG_ATTACHMENT_SCROLL,
                                         eCSSUnit_Enumerated);
  aState.mClip->mValue.SetIntValue(NS_STYLE_BG_CLIP_BORDER,
                                   eCSSUnit_Enumerated);
  aState.mOrigin->mValue.SetIntValue(NS_STYLE_BG_ORIGIN_PADDING,
                                     eCSSUnit_Enumerated);
  nsRefPtr<nsCSSValue::Array> positionArr = nsCSSValue::Array::Create(4);
  aState.mPosition->mValue.SetArrayValue(positionArr, eCSSUnit_Array);
  positionArr->Item(1).SetPercentValue(0.0f);
  positionArr->Item(3).SetPercentValue(0.0f);
  aState.mSize->mXValue.SetAutoValue();
  aState.mSize->mYValue.SetAutoValue();

  bool haveColor = false,
       haveImage = false,
       haveRepeat = false,
       haveAttach = false,
       havePositionAndSize = false,
       haveOrigin = false,
       haveSomething = false;

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
          keyword == eCSSKeyword_initial) {
        return false;
      } else if (keyword == eCSSKeyword_none) {
        if (haveImage)
          return false;
        haveImage = true;
        if (!ParseSingleValueProperty(aState.mImage->mValue,
                                      eCSSProperty_background_image)) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundAttachmentKTable, dummy)) {
        if (haveAttach)
          return false;
        haveAttach = true;
        if (!ParseSingleValueProperty(aState.mAttachment->mValue,
                                      eCSSProperty_background_attachment)) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundRepeatKTable, dummy)) {
        if (haveRepeat)
          return false;
        haveRepeat = true;
        nsCSSValuePair scratch;
        if (!ParseBackgroundRepeatValues(scratch)) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }
        aState.mRepeat->mXValue = scratch.mXValue;
        aState.mRepeat->mYValue = scratch.mYValue;
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundPositionKTable, dummy)) {
        if (havePositionAndSize)
          return false;
        havePositionAndSize = true;
        if (!ParseBackgroundPositionValues(aState.mPosition->mValue, false)) {
          return false;
        }
        if (ExpectSymbol('/', true)) {
          nsCSSValuePair scratch;
          if (!ParseBackgroundSizeValues(scratch)) {
            return false;
          }
          aState.mSize->mXValue = scratch.mXValue;
          aState.mSize->mYValue = scratch.mYValue;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundOriginKTable, dummy)) {
        if (haveOrigin)
          return false;
        haveOrigin = true;
        if (!ParseSingleValueProperty(aState.mOrigin->mValue,
                                      eCSSProperty_background_origin)) {
          NS_NOTREACHED("should be able to parse");
          return false;
        }

        // The spec allows a second box value (for background-clip),
        // immediately following the first one (for background-origin).

        // 'background-clip' and 'background-origin' use the same keyword table
        MOZ_ASSERT(nsCSSProps::kKeywordTableTable[
                     eCSSProperty_background_origin] ==
                   nsCSSProps::kBackgroundOriginKTable);
        MOZ_ASSERT(nsCSSProps::kKeywordTableTable[
                     eCSSProperty_background_clip] ==
                   nsCSSProps::kBackgroundOriginKTable);
        static_assert(NS_STYLE_BG_CLIP_BORDER ==
                      NS_STYLE_BG_ORIGIN_BORDER &&
                      NS_STYLE_BG_CLIP_PADDING ==
                      NS_STYLE_BG_ORIGIN_PADDING &&
                      NS_STYLE_BG_CLIP_CONTENT ==
                      NS_STYLE_BG_ORIGIN_CONTENT,
                      "bg-clip and bg-origin style constants must agree");

        if (!ParseSingleValueProperty(aState.mClip->mValue,
                                      eCSSProperty_background_clip)) {
          // When exactly one <box> value is set, it is used for both
          // 'background-origin' and 'background-clip'.
          // See assertions above showing these values are compatible.
          aState.mClip->mValue = aState.mOrigin->mValue;
        }
      } else {
        if (haveColor)
          return false;
        haveColor = true;
        if (!ParseSingleValueProperty(aState.mColor,
                                      eCSSProperty_background_color)) {
          return false;
        }
      }
    } else if (tt == eCSSToken_URL ||
               (tt == eCSSToken_Function &&
                (mToken.mIdent.LowerCaseEqualsLiteral("linear-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("radial-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("repeating-linear-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("repeating-radial-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-linear-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-radial-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-repeating-linear-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-repeating-radial-gradient") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-image-rect") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-element")))) {
      if (haveImage)
        return false;
      haveImage = true;
      if (!ParseSingleValueProperty(aState.mImage->mValue,
                                    eCSSProperty_background_image)) {
        return false;
      }
    } else if (tt == eCSSToken_Dimension ||
               tt == eCSSToken_Number ||
               tt == eCSSToken_Percentage ||
               (tt == eCSSToken_Function &&
                (mToken.mIdent.LowerCaseEqualsLiteral("calc") ||
                 mToken.mIdent.LowerCaseEqualsLiteral("-moz-calc")))) {
      if (havePositionAndSize)
        return false;
      havePositionAndSize = true;
      if (!ParseBackgroundPositionValues(aState.mPosition->mValue, false)) {
        return false;
      }
      if (ExpectSymbol('/', true)) {
        nsCSSValuePair scratch;
        if (!ParseBackgroundSizeValues(scratch)) {
          return false;
        }
        aState.mSize->mXValue = scratch.mXValue;
        aState.mSize->mYValue = scratch.mYValue;
      }
    } else {
      if (haveColor)
        return false;
      haveColor = true;
      // Note: This parses 'inherit' and 'initial', but
      // we've already checked for them, so it's ok.
      if (!ParseSingleValueProperty(aState.mColor,
                                    eCSSProperty_background_color)) {
        return false;
      }
    }
    haveSomething = true;
  }

  return haveSomething;
}

// This function is very similar to ParseBackgroundPosition and
// ParseBackgroundSize.
bool
CSSParserImpl::ParseValueList(nsCSSProperty aPropID)
{
  // aPropID is a single value prop-id
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // 'initial' and 'inherit' stand alone, no list permitted.
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList* item = value.SetListValue();
    for (;;) {
      if (!ParseSingleValueProperty(item->mValue, aPropID)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        return false;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
  }
  AppendValue(aPropID, value);
  return true;
}

bool
CSSParserImpl::ParseBackgroundRepeat()
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // 'initial' and 'inherit' stand alone, no list permitted.
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValuePair valuePair;
    if (!ParseBackgroundRepeatValues(valuePair)) {
      return false;
    }
    nsCSSValuePairList* item = value.SetPairListValue();
    for (;;) {
      item->mXValue = valuePair.mXValue;
      item->mYValue = valuePair.mYValue;
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        return false;
      }
      if (!ParseBackgroundRepeatValues(valuePair)) {
        return false;
      }
      item->mNext = new nsCSSValuePairList;
      item = item->mNext;
    }
  }

  AppendValue(eCSSProperty_background_repeat, value);
  return true;
}

bool
CSSParserImpl::ParseBackgroundRepeatValues(nsCSSValuePair& aValue) 
{
  nsCSSValue& xValue = aValue.mXValue;
  nsCSSValue& yValue = aValue.mYValue;
  
  if (ParseEnum(xValue, nsCSSProps::kBackgroundRepeatKTable)) {
    int32_t value = xValue.GetIntValue();
    // For single values set yValue as eCSSUnit_Null.
    if (value == NS_STYLE_BG_REPEAT_REPEAT_X ||
        value == NS_STYLE_BG_REPEAT_REPEAT_Y ||
        !ParseEnum(yValue, nsCSSProps::kBackgroundRepeatPartKTable)) {
      // the caller will fail cases like "repeat-x no-repeat"
      // by expecting a list separator or an end property.
      yValue.Reset();
    }
    return true;
  }
  
  return false;
}

// This function is very similar to ParseBackgroundList and ParseBackgroundSize.
bool
CSSParserImpl::ParseBackgroundPosition()
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // 'initial' and 'inherit' stand alone, no list permitted.
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValue itemValue;
    if (!ParseBackgroundPositionValues(itemValue, false)) {
      return false;
    }
    nsCSSValueList* item = value.SetListValue();
    for (;;) {
      item->mValue = itemValue;
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        return false;
      }
      if (!ParseBackgroundPositionValues(itemValue, false)) {
        return false;
      }
      item->mNext = new nsCSSValueList;
      item = item->mNext;
    }
  }
  AppendValue(eCSSProperty_background_position, value);
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
 * @param aAcceptsInherit If true, 'inherit' and 'initial' are legal values
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
  if (ParseVariant(xValue, variantMask, nullptr)) {
    if (eCSSUnit_Inherit == xValue.GetUnit() ||
        eCSSUnit_Initial == xValue.GetUnit()) {  // both are inherited or both are set to initial
      yValue = xValue;
      return true;
    }
    // We have one percentage/length/calc. Get the optional second
    // percentage/length/calc/keyword.
    if (ParseVariant(yValue, VARIANT_LP | VARIANT_CALC, nullptr)) {
      // We have two numbers
      return true;
    }

    if (ParseEnum(yValue, nsCSSProps::kBackgroundPositionKTable)) {
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
  if (ParseEnum(xValue, nsCSSProps::kBackgroundPositionKTable)) {
    int32_t bit = xValue.GetIntValue();
    mask |= bit;
    if (ParseEnum(xValue, nsCSSProps::kBackgroundPositionKTable)) {
      bit = xValue.GetIntValue();
      if (mask & (bit & ~BG_CENTER)) {
        // Only the 'center' keyword can be duplicated.
        return false;
      }
      mask |= bit;
    }
    else {
      // Only one keyword.  See if we have a length, percentage, or calc.
      if (ParseVariant(yValue, VARIANT_LP | VARIANT_CALC, nullptr)) {
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

bool CSSParserImpl::ParseBackgroundPositionValues(nsCSSValue& aOut,
                                                  bool aAcceptsInherit)
{
  // css3-background allows positions to be defined as offsets
  // from an edge. There can be 2 keywords and 2 offsets given. These
  // four 'values' are stored in an array in the following order:
  // [keyword offset keyword offset]. If a keyword or offset isn't
  // parsed the value of the corresponding array element is set
  // to eCSSUnit_Null by a call to nsCSSValue::Reset().
  if (aAcceptsInherit && ParseVariant(aOut, VARIANT_INHERIT, nullptr)) {
    return true;
  }

  nsRefPtr<nsCSSValue::Array> value = nsCSSValue::Array::Create(4);
  aOut.SetArrayValue(value, eCSSUnit_Array);

  // The following clarifies organisation of the array.
  nsCSSValue &xEdge   = value->Item(0),
             &xOffset = value->Item(1),
             &yEdge   = value->Item(2),
             &yOffset = value->Item(3);

  // Parse all the values into the array.
  uint32_t valueCount = 0;
  for (int32_t i = 0; i < 4; i++) {
    if (!ParseVariant(value->Item(i), VARIANT_LPCALC | VARIANT_KEYWORD,
                      nsCSSProps::kBackgroundPositionKTable)) {
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
      yEdge.SetIntValue(NS_STYLE_BG_POSITION_CENTER, eCSSUnit_Enumerated);
      yOffset.Reset();
      break;
    default:
      return false;
  }

  // For compatibility with CSS2.1 code the edges can be unspecified.
  // Unspecified edges are recorded as NULL.
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

// This function is very similar to ParseBackgroundList and
// ParseBackgroundPosition.
bool
CSSParserImpl::ParseBackgroundSize()
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // 'initial' and 'inherit' stand alone, no list permitted.
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValuePair valuePair;
    if (!ParseBackgroundSizeValues(valuePair)) {
      return false;
    }
    nsCSSValuePairList* item = value.SetPairListValue();
    for (;;) {
      item->mXValue = valuePair.mXValue;
      item->mYValue = valuePair.mYValue;
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        return false;
      }
      if (!ParseBackgroundSizeValues(valuePair)) {
        return false;
      }
      item->mNext = new nsCSSValuePairList;
      item = item->mNext;
    }
  }
  AppendValue(eCSSProperty_background_size, value);
  return true;
}

/**
 * Parses two values that correspond to lengths for the background-size
 * property.  These can be one or two lengths (or the 'auto' keyword) or
 * percentages corresponding to the element's dimensions or the single keywords
 * 'contain' or 'cover'.  'initial' and 'inherit' must be handled by the caller
 * if desired.
 *
 * @param aOut The nsCSSValuePair in which to place the result.
 * @return Whether or not the operation succeeded.
 */
#define BG_SIZE_VARIANT (VARIANT_LP | VARIANT_AUTO | VARIANT_CALC)
bool CSSParserImpl::ParseBackgroundSizeValues(nsCSSValuePair &aOut)
{
  // First try a percentage or a length value
  nsCSSValue &xValue = aOut.mXValue,
             &yValue = aOut.mYValue;
  if (ParseNonNegativeVariant(xValue, BG_SIZE_VARIANT, nullptr)) {
    // We have one percentage/length/calc/auto. Get the optional second
    // percentage/length/calc/keyword.
    if (ParseNonNegativeVariant(yValue, BG_SIZE_VARIANT, nullptr)) {
      // We have a second percentage/length/calc/auto.
      return true;
    }

    // If only one percentage or length value is given, it sets the
    // horizontal size only, and the vertical size will be as if by 'auto'.
    yValue.SetAutoValue();
    return true;
  }

  // Now address 'contain' and 'cover'.
  if (!ParseEnum(xValue, nsCSSProps::kBackgroundSizeKTable))
    return false;
  yValue.Reset();
  return true;
}
#undef BG_SIZE_VARIANT

bool
CSSParserImpl::ParseBorderColor()
{
  static const nsCSSProperty kBorderColorSources[] = {
    eCSSProperty_border_left_color_ltr_source,
    eCSSProperty_border_left_color_rtl_source,
    eCSSProperty_border_right_color_ltr_source,
    eCSSProperty_border_right_color_rtl_source,
    eCSSProperty_UNKNOWN
  };

  // do this now, in case 4 values weren't specified
  InitBoxPropsAsPhysical(kBorderColorSources);
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

  if (aAcceptsInherit && ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keyword "inherit" can not be mixed, so we are done.
    AppendValue(eCSSProperty_border_image_slice, value);
    return true;
  }

  // Try parsing "fill" value.
  nsCSSValue imageSliceFillValue;
  bool hasFill = ParseEnum(imageSliceFillValue,
                           nsCSSProps::kBorderImageSliceKTable);

  // Parse the box dimensions.
  nsCSSValue imageSliceBoxValue;
  if (!ParseGroupedBoxProperty(VARIANT_PN, imageSliceBoxValue)) {
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

  if (aAcceptsInherit && ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keyword "inherit" can no be mixed, so we are done.
    AppendValue(eCSSProperty_border_image_width, value);
    return true;
  }

  // Parse the box dimensions.
  if (!ParseGroupedBoxProperty(VARIANT_ALPN, value)) {
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

  if (aAcceptsInherit && ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keyword "inherit" can not be mixed, so we are done.
    AppendValue(eCSSProperty_border_image_outset, value);
    return true;
  }

  // Parse the box dimensions.
  if (!ParseGroupedBoxProperty(VARIANT_LN, value)) {
    return false;
  }

  AppendValue(eCSSProperty_border_image_outset, value);
  return true;
}

bool
CSSParserImpl::ParseBorderImageRepeat(bool aAcceptsInherit)
{
  nsCSSValue value;
  if (aAcceptsInherit && ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // Keyword "inherit" can not be mixed, so we are done.
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
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    AppendValue(eCSSProperty_border_image_source, value);
    AppendValue(eCSSProperty_border_image_slice, value);
    AppendValue(eCSSProperty_border_image_width, value);
    AppendValue(eCSSProperty_border_image_outset, value);
    AppendValue(eCSSProperty_border_image_repeat, value);
    // Keyword "inherit" (and "initial") can't be mixed, so we are done.
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
    if (!foundSource && ParseVariant(imageSourceValue, VARIANT_UO, nullptr)) {
      AppendValue(eCSSProperty_border_image_source, imageSourceValue);
      foundSource = true;
      continue;
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
  if (!ParseNonNegativeVariant(xValue, VARIANT_HL | VARIANT_CALC, nullptr)) {
    return false;
  }

  // If we have one length, get the optional second length.
  // set the second value equal to the first.
  if (xValue.IsLengthUnit() || xValue.IsCalcUnit()) {
    ParseNonNegativeVariant(yValue, VARIANT_LENGTH | VARIANT_CALC, nullptr);
  }

  if (!ExpectEndProperty()) {
    return false;
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
CSSParserImpl::ParseBorderSide(const nsCSSProperty aPropIDs[],
                               bool aSetAllSides)
{
  const int32_t numProps = 3;
  nsCSSValue  values[numProps];

  int32_t found = ParseChoice(values, aPropIDs, numProps);
  if ((found < 1) || (false == ExpectEndProperty())) {
    return false;
  }

  if ((found & 1) == 0) { // Provide default border-width
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default border-style
    values[1].SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) { // text color will be used
    values[2].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  }

  if (aSetAllSides) {
    static const nsCSSProperty kBorderSources[] = {
      eCSSProperty_border_left_color_ltr_source,
      eCSSProperty_border_left_color_rtl_source,
      eCSSProperty_border_right_color_ltr_source,
      eCSSProperty_border_right_color_rtl_source,
      eCSSProperty_border_left_style_ltr_source,
      eCSSProperty_border_left_style_rtl_source,
      eCSSProperty_border_right_style_ltr_source,
      eCSSProperty_border_right_style_rtl_source,
      eCSSProperty_border_left_width_ltr_source,
      eCSSProperty_border_left_width_rtl_source,
      eCSSProperty_border_right_width_ltr_source,
      eCSSProperty_border_right_width_rtl_source,
      eCSSProperty_UNKNOWN
    };

    InitBoxPropsAsPhysical(kBorderSources);

    // Parsing "border" shorthand; set all four sides to the same thing
    for (int32_t index = 0; index < 4; index++) {
      NS_ASSERTION(numProps == 3, "This code needs updating");
      AppendValue(kBorderWidthIDs[index], values[0]);
      AppendValue(kBorderStyleIDs[index], values[1]);
      AppendValue(kBorderColorIDs[index], values[2]);
    }

    static const nsCSSProperty kBorderColorsProps[] = {
      eCSSProperty_border_top_colors,
      eCSSProperty_border_right_colors,
      eCSSProperty_border_bottom_colors,
      eCSSProperty_border_left_colors
    };

    // Set the other properties that the border shorthand sets to their
    // initial values.
    nsCSSValue extraValue;
    switch (values[0].GetUnit()) {
    case eCSSUnit_Inherit:
    case eCSSUnit_Initial:
      extraValue = values[0];
      // Set value of border-image properties to initial/inherit
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
CSSParserImpl::ParseDirectionalBorderSide(const nsCSSProperty aPropIDs[],
                                          int32_t aSourceType)
{
  const int32_t numProps = 3;
  nsCSSValue  values[numProps];

  int32_t found = ParseChoice(values, aPropIDs, numProps);
  if ((found < 1) || (false == ExpectEndProperty())) {
    return false;
  }

  if ((found & 1) == 0) { // Provide default border-width
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default border-style
    values[1].SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) { // text color will be used
    values[2].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  }
  for (int32_t index = 0; index < numProps; index++) {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(aPropIDs[index + numProps]);
    NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
                 "not box property with physical vs. logical cascading");
    AppendValue(subprops[0], values[index]);
    nsCSSValue typeVal(aSourceType, eCSSUnit_Enumerated);
    AppendValue(subprops[1], typeVal);
    AppendValue(subprops[2], typeVal);
  }
  return true;
}

bool
CSSParserImpl::ParseBorderStyle()
{
  static const nsCSSProperty kBorderStyleSources[] = {
    eCSSProperty_border_left_style_ltr_source,
    eCSSProperty_border_left_style_rtl_source,
    eCSSProperty_border_right_style_ltr_source,
    eCSSProperty_border_right_style_rtl_source,
    eCSSProperty_UNKNOWN
  };

  // do this now, in case 4 values weren't specified
  InitBoxPropsAsPhysical(kBorderStyleSources);
  return ParseBoxProperties(kBorderStyleIDs);
}

bool
CSSParserImpl::ParseBorderWidth()
{
  static const nsCSSProperty kBorderWidthSources[] = {
    eCSSProperty_border_left_width_ltr_source,
    eCSSProperty_border_left_width_rtl_source,
    eCSSProperty_border_right_width_ltr_source,
    eCSSProperty_border_right_width_rtl_source,
    eCSSProperty_UNKNOWN
  };

  // do this now, in case 4 values weren't specified
  InitBoxPropsAsPhysical(kBorderWidthSources);
  return ParseBoxProperties(kBorderWidthIDs);
}

bool
CSSParserImpl::ParseBorderColors(nsCSSProperty aProperty)
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    // 'inherit', 'initial', and 'none' are only allowed on their own
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList *cur = value.SetListValue();
    for (;;) {
      if (!ParseVariant(cur->mValue, VARIANT_COLOR | VARIANT_KEYWORD,
                        nsCSSProps::kBorderColorKTable)) {
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
CSSParserImpl::ParseCalc(nsCSSValue &aValue, int32_t aVariantMask)
{
  // Parsing calc expressions requires, in a number of cases, looking
  // for a token that is *either* a value of the property or a number.
  // This can be done without lookahead when we assume that the property
  // values cannot themselves be numbers.
  NS_ASSERTION(!(aVariantMask & VARIANT_NUMBER), "unexpected variant mask");
  NS_ABORT_IF_FALSE(aVariantMask != 0, "unexpected variant mask");

  bool oldUnitlessLengthQuirk = mUnitlessLengthQuirk;
  mUnitlessLengthQuirk = false;

  // One-iteration loop so we can break to the error-handling case.
  do {
    // The toplevel of a calc() is always an nsCSSValue::Array of length 1.
    nsRefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(1);

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
                                           int32_t& aVariantMask)
{
  NS_ABORT_IF_FALSE(aVariantMask != 0, "unexpected variant mask");
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

    nsRefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(2);
    arr->Item(0) = aValue;
    storage = &arr->Item(1);
    aValue.SetArrayValue(arr, unit);
  }
}

struct ReduceNumberCalcOps : public mozilla::css::BasicFloatCalcOps,
                             public mozilla::css::CSSValueInputCalcOps
{
  result_type ComputeLeafValue(const nsCSSValue& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    return aValue.GetFloatValue();
  }

  float ComputeNumber(const nsCSSValue& aValue)
  {
    return mozilla::css::ComputeCalc(aValue, *this);
  }
};

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
                                                 int32_t& aVariantMask,
                                                 bool *aHadFinalWS)
{
  NS_ABORT_IF_FALSE(aVariantMask != 0, "unexpected variant mask");
  bool gotValue = false; // already got the part with the unit
  bool afterDivision = false;

  nsCSSValue *storage = &aValue;
  for (;;) {
    int32_t variantMask;
    if (afterDivision || gotValue) {
      variantMask = VARIANT_NUMBER;
    } else {
      variantMask = aVariantMask | VARIANT_NUMBER;
    }
    if (!ParseCalcTerm(*storage, variantMask))
      return false;
    NS_ABORT_IF_FALSE(variantMask != 0,
                      "ParseCalcTerm did not set variantMask appropriately");
    NS_ABORT_IF_FALSE(!(variantMask & VARIANT_NUMBER) ||
                      !(variantMask & ~int32_t(VARIANT_NUMBER)),
                      "ParseCalcTerm did not set variantMask appropriately");

    if (variantMask & VARIANT_NUMBER) {
      // Simplify the value immediately so we can check for division by
      // zero.
      ReduceNumberCalcOps ops;
      float number = mozilla::css::ComputeCalc(*storage, ops);
      if (number == 0.0 && afterDivision)
        return false;
      storage->SetFloatValue(number, eCSSUnit_Number);
    } else {
      gotValue = true;

      if (storage != &aValue) {
        // Simplify any numbers in the Times_L position (which are
        // not simplified by the check above).
        NS_ABORT_IF_FALSE(storage == &aValue.GetArrayValue()->Item(1),
                          "unexpected relationship to current storage");
        nsCSSValue &leftValue = aValue.GetArrayValue()->Item(0);
        ReduceNumberCalcOps ops;
        float number = mozilla::css::ComputeCalc(leftValue, ops);
        leftValue.SetFloatValue(number, eCSSUnit_Number);
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
      unit = eCSSUnit_Calc_Divided;
      afterDivision = true;
    } else {
      UngetToken();
      *aHadFinalWS = hadWS;
      break;
    }

    nsRefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(2);
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
CSSParserImpl::ParseCalcTerm(nsCSSValue& aValue, int32_t& aVariantMask)
{
  NS_ABORT_IF_FALSE(aVariantMask != 0, "unexpected variant mask");
  if (!GetToken(true))
    return false;
  // Either an additive expression in parentheses...
  if (mToken.IsSymbol('(')) {
    if (!ParseCalcAdditiveExpression(aValue, aVariantMask) ||
        !ExpectSymbol(')', true)) {
      SkipUntil(')');
      return false;
    }
    return true;
  }
  // ... or just a value
  UngetToken();
  // Always pass VARIANT_NUMBER to ParseVariant so that unitless zero
  // always gets picked up
  if (!ParseVariant(aValue, aVariantMask | VARIANT_NUMBER, nullptr)) {
    return false;
  }
  // ...and do the VARIANT_NUMBER check ourselves.
  if (!(aVariantMask & VARIANT_NUMBER) && aValue.GetUnit() == eCSSUnit_Number) {
    return false;
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
CSSParserImpl::ParseRect(nsCSSProperty aPropID)
{
  if (! GetToken(true)) {
    return false;
  }

  nsCSSValue val;

  if (mToken.mType == eCSSToken_Ident) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    switch (keyword) {
      case eCSSKeyword_auto:
        if (!ExpectEndProperty()) {
          return false;
        }
        val.SetAutoValue();
        break;
      case eCSSKeyword_inherit:
        if (!ExpectEndProperty()) {
          return false;
        }
        val.SetInheritValue();
        break;
      case eCSSKeyword_initial:
        if (!ExpectEndProperty()) {
          return false;
        }
        val.SetInitialValue();
        break;
      default:
        UngetToken();
        return false;
    }
  } else if (mToken.mType == eCSSToken_Function &&
             mToken.mIdent.LowerCaseEqualsLiteral("rect")) {
    nsCSSRect& rect = val.SetRectValue();
    bool useCommas;
    NS_FOR_CSS_SIDES(side) {
      if (! ParseVariant(rect.*(nsCSSRect::sides[side]),
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
    if (!ExpectEndProperty()) {
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
  static const nsCSSProperty columnIDs[] = {
    eCSSPropertyExtra_x_auto_value,
    eCSSProperty__moz_column_count,
    eCSSProperty__moz_column_width
  };
  const int32_t numProps = NS_ARRAY_LENGTH(columnIDs);

  nsCSSValue values[numProps];
  int32_t found = ParseChoice(values, columnIDs, numProps);
  if (found < 1 || !ExpectEndProperty()) {
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
  static const int32_t kContentListKWs[] = {
    eCSSKeyword_open_quote, NS_STYLE_CONTENT_OPEN_QUOTE,
    eCSSKeyword_close_quote, NS_STYLE_CONTENT_CLOSE_QUOTE,
    eCSSKeyword_no_open_quote, NS_STYLE_CONTENT_NO_OPEN_QUOTE,
    eCSSKeyword_no_close_quote, NS_STYLE_CONTENT_NO_CLOSE_QUOTE,
    eCSSKeyword_UNKNOWN,-1
  };

  static const int32_t kContentSolitaryKWs[] = {
    eCSSKeyword__moz_alt_content, NS_STYLE_CONTENT_ALT_CONTENT,
    eCSSKeyword_UNKNOWN,-1
  };

  // Verify that these two lists add up to the size of
  // nsCSSProps::kContentKTable.
  NS_ABORT_IF_FALSE(nsCSSProps::kContentKTable[
                      ArrayLength(kContentListKWs) +
                      ArrayLength(kContentSolitaryKWs) - 4] ==
                    eCSSKeyword_UNKNOWN &&
                    nsCSSProps::kContentKTable[
                      ArrayLength(kContentListKWs) +
                      ArrayLength(kContentSolitaryKWs) - 3] == -1,
                    "content keyword tables out of sync");

  nsCSSValue value;
  if (ParseVariant(value, VARIANT_HMK | VARIANT_NONE,
                   kContentSolitaryKWs)) {
    // 'inherit', 'initial', 'normal', 'none', and 'alt-content' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseVariant(cur->mValue, VARIANT_CONTENT, kContentListKWs)) {
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
CSSParserImpl::ParseCounterData(nsCSSProperty aPropID)
{
  nsCSSValue value;
  if (!ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    if (!GetToken(true) || mToken.mType != eCSSToken_Ident) {
      return false;
    }

    nsCSSValuePairList *cur = value.SetPairListValue();
    for (;;) {
      cur->mXValue.SetStringValue(mToken.mIdent, eCSSUnit_Ident);
      if (!GetToken(true)) {
        break;
      }
      if (mToken.mType == eCSSToken_Number && mToken.mIntegerValid) {
        cur->mYValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
      } else {
        UngetToken();
      }
      if (CheckEndProperty()) {
        break;
      }
      if (!GetToken(true) || mToken.mType != eCSSToken_Ident) {
        return false;
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
  if (ParseVariant(value, VARIANT_INHERIT, nullptr)) {
    // 'inherit' and 'initial' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseVariant(cur->mValue, VARIANT_UK, nsCSSProps::kCursorKTable)) {
        return false;
      }
      if (cur->mValue.GetUnit() != eCSSUnit_URL) { // keyword must be last
        if (ExpectEndProperty()) {
          break;
        }
        return false;
      }

      // We have a URL, so make a value array with three values.
      nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(3);
      val->Item(0) = cur->mValue;

      // Parse optional x and y position of cursor hotspot (css3-ui).
      if (ParseVariant(val->Item(1), VARIANT_NUMBER, nullptr)) {
        // If we have one number, we must have two.
        if (!ParseVariant(val->Item(2), VARIANT_NUMBER, nullptr)) {
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
  static const nsCSSProperty fontIDs[] = {
    eCSSProperty_font_style,
    eCSSProperty_font_variant,
    eCSSProperty_font_weight
  };

  nsCSSValue  family;
  if (ParseVariant(family, VARIANT_HK, nsCSSProps::kFontKTable)) {
    if (ExpectEndProperty()) {
      if (eCSSUnit_Inherit == family.GetUnit() ||
          eCSSUnit_Initial == family.GetUnit()) {
        AppendValue(eCSSProperty__x_system_font, nsCSSValue(eCSSUnit_None));
        AppendValue(eCSSProperty_font_family, family);
        AppendValue(eCSSProperty_font_style, family);
        AppendValue(eCSSProperty_font_variant, family);
        AppendValue(eCSSProperty_font_weight, family);
        AppendValue(eCSSProperty_font_size, family);
        AppendValue(eCSSProperty_line_height, family);
        AppendValue(eCSSProperty_font_stretch, family);
        AppendValue(eCSSProperty_font_size_adjust, family);
        AppendValue(eCSSProperty_font_feature_settings, family);
        AppendValue(eCSSProperty_font_language_override, family);
        AppendValue(eCSSProperty_font_kerning, family);
        AppendValue(eCSSProperty_font_synthesis, family);
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
        AppendValue(eCSSProperty_font_variant, systemFont);
        AppendValue(eCSSProperty_font_weight, systemFont);
        AppendValue(eCSSProperty_font_size, systemFont);
        AppendValue(eCSSProperty_line_height, systemFont);
        AppendValue(eCSSProperty_font_stretch, systemFont);
        AppendValue(eCSSProperty_font_size_adjust, systemFont);
        AppendValue(eCSSProperty_font_feature_settings, systemFont);
        AppendValue(eCSSProperty_font_language_override, systemFont);
        AppendValue(eCSSProperty_font_kerning, systemFont);
        AppendValue(eCSSProperty_font_synthesis, systemFont);
        AppendValue(eCSSProperty_font_variant_alternates, systemFont);
        AppendValue(eCSSProperty_font_variant_caps, systemFont);
        AppendValue(eCSSProperty_font_variant_east_asian, systemFont);
        AppendValue(eCSSProperty_font_variant_ligatures, systemFont);
        AppendValue(eCSSProperty_font_variant_numeric, systemFont);
        AppendValue(eCSSProperty_font_variant_position, systemFont);
      }
      return true;
    }
    return false;
  }

  // Get optional font-style, font-variant and font-weight (in any order)
  const int32_t numProps = 3;
  nsCSSValue  values[numProps];
  int32_t found = ParseChoice(values, fontIDs, numProps);
  if ((found < 0) || (eCSSUnit_Inherit == values[0].GetUnit()) ||
      (eCSSUnit_Initial == values[0].GetUnit())) { // illegal data
    return false;
  }
  if ((found & 1) == 0) {
    // Provide default font-style
    values[0].SetIntValue(NS_FONT_STYLE_NORMAL, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) {
    // Provide default font-variant
    values[1].SetIntValue(NS_FONT_VARIANT_NORMAL, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) {
    // Provide default font-weight
    values[2].SetIntValue(NS_FONT_WEIGHT_NORMAL, eCSSUnit_Enumerated);
  }

  // Get mandatory font-size
  nsCSSValue  size;
  if (! ParseNonNegativeVariant(size, VARIANT_KEYWORD | VARIANT_LP,
                                nsCSSProps::kFontSizeKTable)) {
    return false;
  }

  // Get optional "/" line-height
  nsCSSValue  lineHeight;
  if (ExpectSymbol('/', true)) {
    if (! ParseNonNegativeVariant(lineHeight,
                                  VARIANT_NUMBER | VARIANT_LP | VARIANT_NORMAL,
                                  nullptr)) {
      return false;
    }
  }
  else {
    lineHeight.SetNormalValue();
  }

  // Get final mandatory font-family
  nsAutoParseCompoundProperty compound(this);
  if (ParseFamily(family)) {
    if ((eCSSUnit_Inherit != family.GetUnit()) && (eCSSUnit_Initial != family.GetUnit()) &&
        ExpectEndProperty()) {
      AppendValue(eCSSProperty__x_system_font, nsCSSValue(eCSSUnit_None));
      AppendValue(eCSSProperty_font_family, family);
      AppendValue(eCSSProperty_font_style, values[0]);
      AppendValue(eCSSProperty_font_variant, values[1]);
      AppendValue(eCSSProperty_font_weight, values[2]);
      AppendValue(eCSSProperty_font_size, size);
      AppendValue(eCSSProperty_line_height, lineHeight);
      AppendValue(eCSSProperty_font_stretch,
                  nsCSSValue(NS_FONT_STRETCH_NORMAL, eCSSUnit_Enumerated));
      AppendValue(eCSSProperty_font_size_adjust, nsCSSValue(eCSSUnit_None));
      AppendValue(eCSSProperty_font_feature_settings, nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_language_override, nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_kerning,
                  nsCSSValue(NS_FONT_KERNING_AUTO, eCSSUnit_Enumerated));
      AppendValue(eCSSProperty_font_synthesis,
                  nsCSSValue(NS_FONT_SYNTHESIS_WEIGHT | NS_FONT_SYNTHESIS_STYLE,
                             eCSSUnit_Enumerated));
      AppendValue(eCSSProperty_font_variant_alternates,
                  nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_variant_caps, nsCSSValue(eCSSUnit_Normal));
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
  if (!ParseVariant(aValue, VARIANT_HK | VARIANT_NONE,
                    nsCSSProps::kFontSynthesisKTable)) {
    return false;
  }

  // first value 'none' ==> done
  if (eCSSUnit_None == aValue.GetUnit() ||
      eCSSUnit_Initial == aValue.GetUnit() ||
      eCSSUnit_Inherit == aValue.GetUnit())
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
// font-variant-alternates: swash(flowing), historical-forms, styleset(alt-g, alt-m);
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
  if (!(eCSSKeyword_UNKNOWN < keyword &&
        nsCSSProps::FindKeyword(keyword,
                                (isIdent ?
                                 nsCSSProps::kFontVariantAlternatesKTable :
                                 nsCSSProps::kFontVariantAlternatesFuncsKTable),
                                aWhichFeature)))
  {
    // failed, pop token
    UngetToken();
    return false;
  }

  if (isIdent) {
    aValue.SetIntValue(aWhichFeature, eCSSUnit_Enumerated);
    return true;
  }

  uint16_t maxElems = 1;
  if (keyword == eCSSKeyword_styleset ||
      keyword == eCSSKeyword_character_variant) {
    maxElems = MAX_ALLOWED_FEATURES;
  }
  return ParseFunction(keyword, nullptr, VARIANT_IDENTIFIER,
                       1, maxElems, aValue);
}

bool
CSSParserImpl::ParseFontVariantAlternates(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL, nullptr)) {
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

  nsCSSValue featureValue;
  featureValue.SetIntValue(featureFlags, eCSSUnit_Enumerated);
  aValue.SetPairValue(featureValue, listValue);

  return true;
}

#define MASK_END_VALUE  -1

// aMasks - array of masks for mutually-exclusive property values,
//          e.g. proportial-nums, tabular-nums

bool
CSSParserImpl::ParseBitmaskValues(nsCSSValue& aValue,
                                  const int32_t aKeywordTable[],
                                  const int32_t aMasks[])
{
  if (!ParseVariant(aValue, VARIANT_HMK, aKeywordTable)) {
    return false;
  }

  // first value 'normal', 'inherit', 'initial' ==> done
  if (eCSSUnit_Normal == aValue.GetUnit() ||
      eCSSUnit_Initial == aValue.GetUnit() ||
      eCSSUnit_Inherit == aValue.GetUnit())
  {
    return true;
  }

  // look for more values
  nsCSSValue nextValue;
  int32_t mergedValue = aValue.GetIntValue();

  while (ParseEnum(nextValue, aKeywordTable))
  {
    int32_t nextIntValue = nextValue.GetIntValue();

    // check to make sure value not already set
    if (nextIntValue & mergedValue) {
      return false;
    }

    const int32_t *m = aMasks;
    int32_t c = 0;

    while (*m != MASK_END_VALUE) {
      if (*m & nextIntValue) {
        c = mergedValue & *m;
        break;
      }
      m++;
    }

    if (c) {
      return false;
    }

    mergedValue |= nextIntValue;
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
  NS_ASSERTION(maskEastAsian[NS_ARRAY_LENGTH(maskEastAsian) - 1] ==
                 MASK_END_VALUE,
               "incorrectly terminated array");

  return ParseBitmaskValues(aValue, nsCSSProps::kFontVariantEastAsianKTable,
                            maskEastAsian);
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
  NS_ASSERTION(maskLigatures[NS_ARRAY_LENGTH(maskLigatures) - 1] ==
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
  NS_ASSERTION(maskNumeric[NS_ARRAY_LENGTH(maskNumeric) - 1] ==
                 MASK_END_VALUE,
               "incorrectly terminated array");

  return ParseBitmaskValues(aValue, nsCSSProps::kFontVariantNumericKTable,
                            maskNumeric);
}

bool
CSSParserImpl::ParseFontWeight(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HKI | VARIANT_SYSFONT,
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
CSSParserImpl::ParseOneFamily(nsAString& aFamily, bool& aOneKeyword)
{
  if (!GetToken(true))
    return false;

  nsCSSToken* tk = &mToken;

  aOneKeyword = false;
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
        aFamily.Append(PRUnichar(' '));
        aFamily.Append(tk->mIdent);
      } else if (eCSSToken_Whitespace != tk->mType) {
        UngetToken();
        break;
      }
    }
    return true;

  } else if (eCSSToken_String == tk->mType) {
    aFamily.Append(tk->mSymbol); // replace the quotes
    aFamily.Append(tk->mIdent); // XXX What if it had escaped quotes?
    aFamily.Append(tk->mSymbol);
    return true;

  } else {
    UngetToken();
    return false;
  }
}

bool
CSSParserImpl::ParseFamily(nsCSSValue& aValue)
{
  nsAutoString family;
  bool single;

  // keywords only have meaning in the first position
  if (!ParseOneFamily(family, single))
    return false;

  // check for keywords, but only when keywords appear by themselves
  // i.e. not in compounds such as font-family: default blah;
  if (single) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(family);
    if (keyword == eCSSKeyword_inherit) {
      aValue.SetInheritValue();
      return true;
    }
    // 605231 - don't parse unquoted 'default' reserved keyword
    if (keyword == eCSSKeyword_default) {
      return false;
    }
    if (keyword == eCSSKeyword_initial) {
      aValue.SetInitialValue();
      return true;
    }
    if (keyword == eCSSKeyword__moz_use_system_font &&
        !IsParsingCompoundProperty()) {
      aValue.SetSystemFontValue();
      return true;
    }
  }

  for (;;) {
    if (!ExpectSymbol(',', true))
      break;

    family.Append(PRUnichar(','));

    nsAutoString nextFamily;
    if (!ParseOneFamily(nextFamily, single))
      return false;

    // at this point unquoted keywords are not allowed
    // as font family names but can appear within names
    if (single) {
      nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(nextFamily);
      switch (keyword) {
        case eCSSKeyword_inherit:
        case eCSSKeyword_initial:
        case eCSSKeyword_default:
        case eCSSKeyword__moz_use_system_font:
          return false;
        default:
          break;
      }
    }

    family.Append(nextFamily);
  }

  if (family.IsEmpty()) {
    return false;
  }
  aValue.SetStringValue(family, eCSSUnit_Families);
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
      bool single;
      if (!ParseOneFamily(family, single)) {
        SkipUntil(')');
        return false;
      }
      if (!ExpectSymbol(')', true)) {
        SkipUntil(')');
        return false;
      }

      // the style parameters to the nsFont constructor are ignored,
      // because it's only being used to call EnumerateFamilies
      nsFont font(family, 0, 0, 0, 0, 0, 0);
      ExtractFirstFamilyData dat;

      font.EnumerateFamilies(ExtractFirstFamily, (void*) &dat);
      if (!dat.mGood)
        return false;

      cur.SetStringValue(dat.mFamilyName, eCSSUnit_Local_Font);
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

  nsRefPtr<nsCSSValue::Array> srcVals
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

    // A range that descends, or a range that is entirely outside the
    // current range of Unicode (U+0-10FFFF) is ignored, but does not
    // invalidate the descriptor.  A range that straddles the high end
    // is clipped.
    if (low <= 0x10FFFF && low <= high) {
      if (high > 0x10FFFF)
        high = 0x10FFFF;

      ranges.AppendElement(low);
      ranges.AppendElement(high);
    }
    if (!ExpectSymbol(',', true))
      break;
  }

  if (ranges.Length() == 0)
    return false;

  nsRefPtr<nsCSSValue::Array> srcVals
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
  if (ParseVariant(aValue, VARIANT_INHERIT | VARIANT_NORMAL, nullptr)) {
    return true;
  }

  nsCSSValuePairList *cur = aValue.SetPairListValue();
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

  return true;
}

bool
CSSParserImpl::ParseListStyle()
{
  // 'list-style' can accept 'none' for two different subproperties,
  // 'list-style-type' and 'list-style-position'.  In order to accept
  // 'none' as the value of either but still allow another value for
  // either, we need to ensure that the first 'none' we find gets
  // allocated to a dummy property instead.
  static const nsCSSProperty listStyleIDs[] = {
    eCSSPropertyExtra_x_none_value,
    eCSSProperty_list_style_type,
    eCSSProperty_list_style_position,
    eCSSProperty_list_style_image
  };

  nsCSSValue values[NS_ARRAY_LENGTH(listStyleIDs)];
  int32_t found =
    ParseChoice(values, listStyleIDs, ArrayLength(listStyleIDs));
  if (found < 1 || !ExpectEndProperty()) {
    return false;
  }

  if ((found & (1|2|8)) == (1|2|8)) {
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

  // Provide default values
  if ((found & 2) == 0) {
    if (found & 1) {
      values[1].SetIntValue(NS_STYLE_LIST_STYLE_NONE, eCSSUnit_Enumerated);
    } else {
      values[1].SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
    }
  }
  if ((found & 4) == 0) {
    values[2].SetIntValue(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
                          eCSSUnit_Enumerated);
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
CSSParserImpl::ParseMargin()
{
  static const nsCSSProperty kMarginSideIDs[] = {
    eCSSProperty_margin_top,
    eCSSProperty_margin_right_value,
    eCSSProperty_margin_bottom,
    eCSSProperty_margin_left_value
  };
  static const nsCSSProperty kMarginSources[] = {
    eCSSProperty_margin_left_ltr_source,
    eCSSProperty_margin_left_rtl_source,
    eCSSProperty_margin_right_ltr_source,
    eCSSProperty_margin_right_rtl_source,
    eCSSProperty_UNKNOWN
  };

  // do this now, in case 4 values weren't specified
  InitBoxPropsAsPhysical(kMarginSources);
  return ParseBoxProperties(kMarginSideIDs);
}

bool
CSSParserImpl::ParseMarks(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HK, nsCSSProps::kPageMarksKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      if (NS_STYLE_PAGE_MARKS_NONE != aValue.GetIntValue() &&
          false == CheckEndProperty()) {
        nsCSSValue second;
        if (ParseEnum(second, nsCSSProps::kPageMarksKTable)) {
          // 'none' keyword in conjuction with others is not allowed
          if (NS_STYLE_PAGE_MARKS_NONE != second.GetIntValue()) {
            aValue.SetIntValue(aValue.GetIntValue() | second.GetIntValue(),
                               eCSSUnit_Enumerated);
            return true;
          }
        }
        return false;
      }
    }
    return true;
  }
  return false;
}

bool
CSSParserImpl::ParseOutline()
{
  const int32_t numProps = 3;
  static const nsCSSProperty kOutlineIDs[] = {
    eCSSProperty_outline_color,
    eCSSProperty_outline_style,
    eCSSProperty_outline_width
  };

  nsCSSValue  values[numProps];
  int32_t found = ParseChoice(values, kOutlineIDs, numProps);
  if ((found < 1) || (false == ExpectEndProperty())) {
    return false;
  }

  // Provide default values
  if ((found & 1) == 0) { // Provide default outline-color
    values[0].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
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
  if (!ParseVariant(overflow, VARIANT_HK,
                    nsCSSProps::kOverflowKTable) ||
      !ExpectEndProperty())
    return false;

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
  static const nsCSSProperty kPaddingSideIDs[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right_value,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left_value
  };
  static const nsCSSProperty kPaddingSources[] = {
    eCSSProperty_padding_left_ltr_source,
    eCSSProperty_padding_left_rtl_source,
    eCSSProperty_padding_right_ltr_source,
    eCSSProperty_padding_right_rtl_source,
    eCSSProperty_UNKNOWN
  };

  // do this now, in case 4 values weren't specified
  InitBoxPropsAsPhysical(kPaddingSources);
  return ParseBoxProperties(kPaddingSideIDs);
}

bool
CSSParserImpl::ParseQuotes()
{
  nsCSSValue value;
  if (!ParseVariant(value, VARIANT_HOS, nullptr)) {
    return false;
  }
  if (value.GetUnit() != eCSSUnit_String) {
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValue open = value;
    nsCSSValuePairList* quotes = value.SetPairListValue();
    for (;;) {
      quotes->mXValue = open;
      // get mandatory close
      if (!ParseVariant(quotes->mYValue, VARIANT_STRING, nullptr)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      // look for another open
      if (!ParseVariant(open, VARIANT_STRING, nullptr)) {
        return false;
      }
      quotes->mNext = new nsCSSValuePairList;
      quotes = quotes->mNext;
    }
  }
  AppendValue(eCSSProperty_quotes, value);
  return true;
}

bool
CSSParserImpl::ParseSize()
{
  nsCSSValue width, height;
  if (!ParseVariant(width, VARIANT_AHKL, nsCSSProps::kPageSizeKTable)) {
    return false;
  }
  if (width.IsLengthUnit()) {
    ParseVariant(height, VARIANT_LENGTH, nullptr);
  }
  if (!ExpectEndProperty()) {
    return false;
  }

  if (width == height || height.GetUnit() == eCSSUnit_Null) {
    AppendValue(eCSSProperty_size, width);
  } else {
    nsCSSValue pair;
    pair.SetPairValue(width, height);
    AppendValue(eCSSProperty_size, pair);
  }
  return true;
}

bool
CSSParserImpl::ParseTextDecoration()
{
  enum {
    eDecorationNone         = NS_STYLE_TEXT_DECORATION_LINE_NONE,
    eDecorationUnderline    = NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
    eDecorationOverline     = NS_STYLE_TEXT_DECORATION_LINE_OVERLINE,
    eDecorationLineThrough  = NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH,
    eDecorationBlink        = NS_STYLE_TEXT_DECORATION_LINE_BLINK,
    eDecorationPrefAnchors  = NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS
  };
  static_assert((eDecorationNone ^ eDecorationUnderline ^
                 eDecorationOverline ^ eDecorationLineThrough ^
                 eDecorationBlink ^ eDecorationPrefAnchors) ==
                (eDecorationNone | eDecorationUnderline |
                 eDecorationOverline | eDecorationLineThrough |
                 eDecorationBlink | eDecorationPrefAnchors),
                "text decoration constants need to be bitmasks");

  static const int32_t kTextDecorationKTable[] = {
    eCSSKeyword_none,                   eDecorationNone,
    eCSSKeyword_underline,              eDecorationUnderline,
    eCSSKeyword_overline,               eDecorationOverline,
    eCSSKeyword_line_through,           eDecorationLineThrough,
    eCSSKeyword_blink,                  eDecorationBlink,
    eCSSKeyword__moz_anchor_decoration, eDecorationPrefAnchors,
    eCSSKeyword_UNKNOWN,-1
  };

  nsCSSValue value;
  if (!ParseVariant(value, VARIANT_HK, kTextDecorationKTable)) {
    return false;
  }

  nsCSSValue line, style, color;
  switch (value.GetUnit()) {
    case eCSSUnit_Enumerated: {
      // We shouldn't accept decoration line style and color via
      // text-decoration.
      color.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR,
                        eCSSUnit_Enumerated);
      style.SetIntValue(NS_STYLE_TEXT_DECORATION_STYLE_SOLID,
                        eCSSUnit_Enumerated);

      int32_t intValue = value.GetIntValue();
      if (intValue == eDecorationNone) {
        line.SetIntValue(NS_STYLE_TEXT_DECORATION_LINE_NONE,
                         eCSSUnit_Enumerated);
        break;
      }

      // look for more keywords
      nsCSSValue keyword;
      int32_t index;
      for (index = 0; index < 3; index++) {
        if (!ParseEnum(keyword, kTextDecorationKTable)) {
          break;
        }
        int32_t newValue = keyword.GetIntValue();
        if (newValue == eDecorationNone || newValue & intValue) {
          // 'none' keyword in conjuction with others is not allowed, and
          // duplicate keyword is not allowed.
          return false;
        }
        intValue |= newValue;
      }

      line.SetIntValue(intValue, eCSSUnit_Enumerated);
      break;
    }
    default:
      line = color = style = value;
      break;
  }

  AppendValue(eCSSProperty_text_decoration_line, line);
  AppendValue(eCSSProperty_text_decoration_color, color);
  AppendValue(eCSSProperty_text_decoration_style, style);

  return true;
}

bool
CSSParserImpl::ParseTextDecorationLine(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HK, nsCSSProps::kTextDecorationLineKTable)) {
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
  if (ParseVariant(aValue, VARIANT_INHERIT, nullptr)) {
    // 'inherit' and 'initial' must be alone
    return true;
  }

  nsCSSValue left;
  if (!ParseVariant(left, VARIANT_KEYWORD | VARIANT_STRING,
                    nsCSSProps::kTextOverflowKTable))
    return false;

  nsCSSValue right;
  if (ParseVariant(right, VARIANT_KEYWORD | VARIANT_STRING,
                    nsCSSProps::kTextOverflowKTable))
    aValue.SetPairValue(left, right);
  else {
    aValue = left;
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
CSSParserImpl::ParseFunctionInternals(const int32_t aVariantMask[],
                                      int32_t aVariantMaskAll,
                                      uint16_t aMinElems,
                                      uint16_t aMaxElems,
                                      InfallibleTArray<nsCSSValue> &aOutput)
{
  NS_ASSERTION((aVariantMask && !aVariantMaskAll) ||
               (!aVariantMask && aVariantMaskAll),
               "only one of the two variant mask parameters can be set");

  for (uint16_t index = 0; index < aMaxElems; ++index) {
    nsCSSValue newValue;
    int32_t m = aVariantMaskAll ? aVariantMaskAll : aVariantMask[index];
    if (!ParseVariant(newValue, m, nullptr)) {
      break;
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
                             const int32_t aAllowedTypes[],
                             int32_t aAllowedTypesAll,
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
   */
  InfallibleTArray<nsCSSValue> foundValues;
  if (!ParseFunctionInternals(aAllowedTypes, aAllowedTypesAll, aMinElems,
                              aMaxElems, foundValues)) {
    return false;
  }

  /*
   * In case the user has given us more than 2^16 - 2 arguments,
   * we'll truncate them at 2^16 - 2 arguments.
   */
  uint16_t numArgs = std::min(foundValues.Length(), MAX_ALLOWED_ELEMS);
  nsRefPtr<nsCSSValue::Array> convertedArray =
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
 * @param aMinElems [out] The minimum number of elements to read.
 * @param aMaxElems [out] The maximum number of elements to read
 * @param aVariantMask [out] The variant mask to use during parsing
 * @return Whether the information was loaded successfully.
 */
static bool GetFunctionParseInformation(nsCSSKeyword aToken,
                                        bool aIsPrefixed,
                                        uint16_t &aMinElems,
                                        uint16_t &aMaxElems,
                                        const int32_t *& aVariantMask)
{
/* These types represent the common variant masks that will be used to
   * parse out the individual functions.  The order in the enumeration
   * must match the order in which the masks are declared.
   */
  enum { eLengthPercentCalc,
         eLengthCalc,
         eTwoLengthPercentCalcs,
         eTwoLengthPercentCalcsOneLengthCalc,
         eAngle,
         eTwoAngles,
         eNumber,
         ePositiveLength,
         eTwoNumbers,
         eThreeNumbers,
         eThreeNumbersOneAngle,
         eMatrix,
         eMatrixPrefixed,
         eMatrix3d,
         eMatrix3dPrefixed,
         eNumVariantMasks };
  static const int32_t kMaxElemsPerFunction = 16;
  static const int32_t kVariantMasks[eNumVariantMasks][kMaxElemsPerFunction] = {
    {VARIANT_LPCALC},
    {VARIANT_LENGTH|VARIANT_CALC},
    {VARIANT_LPCALC, VARIANT_LPCALC},
    {VARIANT_LPCALC, VARIANT_LPCALC, VARIANT_LENGTH|VARIANT_CALC},
    {VARIANT_ANGLE_OR_ZERO},
    {VARIANT_ANGLE_OR_ZERO, VARIANT_ANGLE_OR_ZERO},
    {VARIANT_NUMBER},
    {VARIANT_LENGTH|VARIANT_POSITIVE_DIMENSION},
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

#ifdef DEBUG
  static const uint8_t kVariantMaskLengths[eNumVariantMasks] =
    {1, 1, 2, 3, 1, 2, 1, 1, 2, 3, 4, 6, 6, 16, 16};
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
    variantIndex = ePositiveLength;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  default:
    /* Oh dear, we didn't match.  Report an error. */
    return false;
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

/* Reads a single transform function from the tokenizer stream, reporting an
 * error if something goes wrong.
 */
bool
CSSParserImpl::ParseSingleTransform(bool aIsPrefixed, nsCSSValue& aValue)
{
  if (!GetToken(true))
    return false;

  if (mToken.mType != eCSSToken_Function) {
    UngetToken();
    return false;
  }

  const int32_t* variantMask;
  uint16_t minElems, maxElems;
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);

  if (!GetFunctionParseInformation(keyword, aIsPrefixed,
                                   minElems, maxElems, variantMask))
    return false;

  return ParseFunction(keyword, variantMask, 0, minElems, maxElems, aValue);
}

/* Parses a transform property list by continuously reading in properties
 * and constructing a matrix from it.
 */
bool CSSParserImpl::ParseTransform(bool aIsPrefixed)
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    // 'inherit', 'initial', and 'none' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseSingleTransform(aIsPrefixed, cur->mValue)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      cur->mNext = new nsCSSValueList;
      cur = cur->mNext;
    }
  }
  AppendValue(eCSSProperty_transform, value);
  return true;
}

bool CSSParserImpl::ParseTransformOrigin(bool aPerspective)
{
  nsCSSValuePair position;
  if (!ParseBoxPositionValues(position, true))
    return false;

  nsCSSProperty prop = eCSSProperty_transform_origin;
  if (aPerspective) {
    if (!ExpectEndProperty()) {
      return false;
    }
    prop = eCSSProperty_perspective_origin;
  }

  // Unlike many other uses of pairs, this position should always be stored
  // as a pair, even if the values are the same, so it always serializes as
  // a pair, and to keep the computation code simple.
  if (position.mXValue.GetUnit() == eCSSUnit_Inherit ||
      position.mXValue.GetUnit() == eCSSUnit_Initial) {
    NS_ABORT_IF_FALSE(position.mXValue == position.mYValue,
                      "inherit/initial only half?");
    AppendValue(prop, position.mXValue);
  } else {
    nsCSSValue value;
    if (aPerspective) {
      value.SetPairValue(position.mXValue, position.mYValue);
    } else {
      nsCSSValue depth;
      if (!ParseVariant(depth, VARIANT_LENGTH | VARIANT_CALC, nullptr)) {
        depth.SetFloatValue(0.0f, eCSSUnit_Pixel);
      }
      value.SetTripletValue(position.mXValue, position.mYValue, depth);
    }

    AppendValue(prop, value);
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
  if (ParseVariant(*aValue, VARIANT_URL, nullptr)) {
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
  int32_t variantMask = VARIANT_PN;
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
  NS_ABORT_IF_FALSE(aValue->GetUnit() == eCSSUnit_Function,
                    "expected a filter function");
  NS_ABORT_IF_FALSE(aValue->UnitHasArrayValue(),
                    "filter function should be an array");
  NS_ABORT_IF_FALSE(aValue->GetArrayValue()->Count() == 2,
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
  if (ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    // 'inherit', 'initial', and 'none' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
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
  if (ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    // 'inherit', 'initial', and 'none' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    // Accept a list of arbitrary identifiers.  They should be
    // CSS properties, but we want to accept any so that we
    // accept properties that we don't know about yet, e.g.
    // transition-property: invalid-property, left, opacity;
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseVariant(cur->mValue, VARIANT_IDENTIFIER | VARIANT_ALL, nullptr)) {
        return false;
      }
      if (cur->mValue.GetUnit() == eCSSUnit_Ident) {
        nsDependentString str(cur->mValue.GetStringBufferValue());
        // Exclude 'none' and 'inherit' and 'initial' according to the
        // same rules as for 'counter-reset' in CSS 2.1.
        if (str.LowerCaseEqualsLiteral("none") ||
            str.LowerCaseEqualsLiteral("inherit") ||
            str.LowerCaseEqualsLiteral("initial")) {
          return false;
        }
      }
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
        return false;
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

  nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(4);

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
                                                           bool aCheckRange)
{
  if (!GetToken(true)) {
    return false;
  }
  nsCSSToken* tk = &mToken;
  if (tk->mType == eCSSToken_Number) {
    float num = tk->mNumber;
    if (aCheckRange && (num < 0.0 || num > 1.0)) {
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

  nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(2);

  if (!ParseOneOrLargerVariant(val->Item(0), VARIANT_INTEGER, nullptr)) {
    return false;
  }

  int32_t type = NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END;
  if (ExpectSymbol(',', true)) {
    if (!GetToken(true)) {
      return false;
    }
    type = -1;
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

static nsCSSValueList*
AppendValueToList(nsCSSValue& aContainer,
                  nsCSSValueList* aTail,
                  const nsCSSValue& aValue)
{
  nsCSSValueList* entry;
  if (aContainer.GetUnit() == eCSSUnit_Null) {
    NS_ABORT_IF_FALSE(!aTail, "should not have an entry");
    entry = aContainer.SetListValue();
  } else {
    NS_ABORT_IF_FALSE(!aTail->mNext, "should not have a next entry");
    NS_ABORT_IF_FALSE(aContainer.GetUnit() == eCSSUnit_List, "not a list");
    entry = new nsCSSValueList;
    aTail->mNext = entry;
  }
  entry->mValue = aValue;
  return entry;
}

CSSParserImpl::ParseAnimationOrTransitionShorthandResult
CSSParserImpl::ParseAnimationOrTransitionShorthand(
                 const nsCSSProperty* aProperties,
                 const nsCSSValue* aInitialValues,
                 nsCSSValue* aValues,
                 size_t aNumProperties)
{
  nsCSSValue tempValue;
  // first see if 'inherit' or 'initial' is specified.  If one is,
  // it can be the only thing specified, so don't attempt to parse any
  // additional properties
  if (ParseVariant(tempValue, VARIANT_INHERIT, nullptr)) {
    for (uint32_t i = 0; i < aNumProperties; ++i) {
      AppendValue(aProperties[i], tempValue);
    }
    return eParseAnimationOrTransitionShorthand_Inherit;
  }

  static const size_t maxNumProperties = 7;
  NS_ABORT_IF_FALSE(aNumProperties <= maxNumProperties,
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
          if (ParseSingleValueProperty(tempValue, aProperties[i])) {
            parsedProperty[i] = true;
            cur[i] = AppendValueToList(aValues[i], cur[i], tempValue);
            foundProperty = true;
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
  static const nsCSSProperty kTransitionProperties[] = {
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
  static const uint32_t numProps = NS_ARRAY_LENGTH(kTransitionProperties);
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
  //     'none' or 'all'.
  //   + None of the items can be 'inherit' or 'initial' (this is the case,
  //     like with counter-reset &c., where CSS 2.1 specifies 'initial', so
  //     we should check it without the -moz- prefix).
  {
    NS_ABORT_IF_FALSE(kTransitionProperties[3] ==
                        eCSSProperty_transition_property,
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
        if (str.EqualsLiteral("inherit") || str.EqualsLiteral("initial")) {
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
  static const nsCSSProperty kAnimationProperties[] = {
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
    // Must check 'animation-name' after 'animation-timing-function',
    // 'animation-direction', 'animation-fill-mode',
    // 'animation-iteration-count', and 'animation-play-state' since
    // 'animation-name' accepts any keyword.
    eCSSProperty_animation_name
  };
  static const uint32_t numProps = NS_ARRAY_LENGTH(kAnimationProperties);
  // this is a shorthand property that accepts -property, -delay,
  // -duration, and -timing-function with some components missing.
  // there can be multiple animations, separated with commas

  nsCSSValue initialValues[numProps];
  initialValues[0].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[1].SetIntValue(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE,
                               eCSSUnit_Enumerated);
  initialValues[2].SetFloatValue(0.0, eCSSUnit_Seconds);
  initialValues[3].SetIntValue(NS_STYLE_ANIMATION_DIRECTION_NORMAL, eCSSUnit_Enumerated);
  initialValues[4].SetIntValue(NS_STYLE_ANIMATION_FILL_MODE_NONE, eCSSUnit_Enumerated);
  initialValues[5].SetFloatValue(1.0f, eCSSUnit_Number);
  initialValues[6].SetNoneValue();

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

  nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(6);

  if (aIsBoxShadow) {
    // Optional inset keyword (ignore errors)
    ParseVariant(val->Item(IndexInset), VARIANT_KEYWORD,
                 nsCSSProps::kBoxShadowTypeKTable);
  }

  nsCSSValue xOrColor;
  bool haveColor = false;
  if (!ParseVariant(xOrColor, VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC,
                    nullptr)) {
    return false;
  }
  if (xOrColor.IsLengthUnit() || xOrColor.IsCalcUnit()) {
    val->Item(IndexX) = xOrColor;
  } else {
    // Must be a color (as string or color value)
    NS_ASSERTION(xOrColor.GetUnit() == eCSSUnit_Ident ||
                 xOrColor.GetUnit() == eCSSUnit_Color ||
                 xOrColor.GetUnit() == eCSSUnit_EnumColor,
                 "Must be a color value");
    val->Item(IndexColor) = xOrColor;
    haveColor = true;

    // X coordinate mandatory after color
    if (!ParseVariant(val->Item(IndexX), VARIANT_LENGTH | VARIANT_CALC,
                      nullptr)) {
      return false;
    }
  }

  // Y coordinate; mandatory
  if (!ParseVariant(val->Item(IndexY), VARIANT_LENGTH | VARIANT_CALC,
                    nullptr)) {
    return false;
  }

  // Optional radius. Ignore errors except if they pass a negative
  // value which we must reject. If we use ParseNonNegativeVariant
  // we can't tell the difference between an unspecified radius
  // and a negative radius.
  if (ParseVariant(val->Item(IndexRadius), VARIANT_LENGTH | VARIANT_CALC,
                   nullptr) &&
      val->Item(IndexRadius).IsLengthUnit() &&
      val->Item(IndexRadius).GetFloatValue() < 0) {
    return false;
  }

  if (aIsBoxShadow) {
    // Optional spread
    ParseVariant(val->Item(IndexSpread), VARIANT_LENGTH | VARIANT_CALC, nullptr);
  }

  if (!haveColor) {
    // Optional color
    ParseVariant(val->Item(IndexColor), VARIANT_COLOR, nullptr);
  }

  if (aIsBoxShadow && val->Item(IndexInset).GetUnit() == eCSSUnit_Null) {
    // Optional inset keyword
    ParseVariant(val->Item(IndexInset), VARIANT_KEYWORD,
                 nsCSSProps::kBoxShadowTypeKTable);
  }

  aValue.SetArrayValue(val, eCSSUnit_Array);
  return true;
}

bool
CSSParserImpl::ParseShadowList(nsCSSProperty aProperty)
{
  nsAutoParseCompoundProperty compound(this);
  bool isBoxShadow = aProperty == eCSSProperty_box_shadow;

  nsCSSValue value;
  if (ParseVariant(value, VARIANT_INHERIT | VARIANT_NONE, nullptr)) {
    // 'inherit', 'initial', and 'none' must be alone
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList* cur = value.SetListValue();
    for (;;) {
      if (!ParseShadowItem(cur->mValue, isBoxShadow)) {
        return false;
      }
      if (CheckEndProperty()) {
        break;
      }
      if (!ExpectSymbol(',', true)) {
        return false;
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
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(aPrefix);
    if (!prefix) {
      NS_RUNTIMEABORT("do_GetAtom failed - out of memory?");
    }
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
CSSParserImpl::ParsePaint(nsCSSProperty aPropID)
{
  nsCSSValue x, y;
  if (!ParseVariant(x, VARIANT_HCK | VARIANT_NONE | VARIANT_URL,
                    nsCSSProps::kObjectPatternKTable)) {
    return false;
  }

  bool canHaveFallback = x.GetUnit() == eCSSUnit_URL ||
                         x.GetUnit() == eCSSUnit_Enumerated;
  if (canHaveFallback) {
    if (!ParseVariant(y, VARIANT_COLOR | VARIANT_NONE, nullptr))
      y.SetNoneValue();
  }
  if (!ExpectEndProperty())
    return false;

  if (!canHaveFallback) {
    AppendValue(aPropID, x);
  } else {
    nsCSSValue val;
    val.SetPairValue(x, y);
    AppendValue(aPropID, val);
  }
  return true;
}

bool
CSSParserImpl::ParseDasharray()
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_HK | VARIANT_NONE,
                   nsCSSProps::kStrokeObjectValueKTable)) {
    // 'inherit', 'initial', and 'none' are only allowed on their own
    if (!ExpectEndProperty()) {
      return false;
    }
  } else {
    nsCSSValueList *cur = value.SetListValue();
    for (;;) {
      if (!ParseNonNegativeVariant(cur->mValue, VARIANT_LPN, nullptr)) {
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
  if (ParseSingleValueProperty(marker, eCSSProperty_marker_end)) {
    if (ExpectEndProperty()) {
      AppendValue(eCSSProperty_marker_end, marker);
      AppendValue(eCSSProperty_marker_mid, marker);
      AppendValue(eCSSProperty_marker_start, marker);
      return true;
    }
  }
  return false;
}

bool
CSSParserImpl::ParsePaintOrder()
{
  static_assert
    ((1 << NS_STYLE_PAINT_ORDER_BITWIDTH) > NS_STYLE_PAINT_ORDER_LAST_VALUE,
     "bitfield width insufficient for paint-order constants");

  static const int32_t kPaintOrderKTable[] = {
    eCSSKeyword_normal,  NS_STYLE_PAINT_ORDER_NORMAL,
    eCSSKeyword_fill,    NS_STYLE_PAINT_ORDER_FILL,
    eCSSKeyword_stroke,  NS_STYLE_PAINT_ORDER_STROKE,
    eCSSKeyword_markers, NS_STYLE_PAINT_ORDER_MARKERS,
    eCSSKeyword_UNKNOWN,-1
  };

  static_assert(NS_ARRAY_LENGTH(kPaintOrderKTable) ==
                  2 * (NS_STYLE_PAINT_ORDER_LAST_VALUE + 2),
                "missing paint-order values in kPaintOrderKTable");

  nsCSSValue value;
  if (!ParseVariant(value, VARIANT_HK, kPaintOrderKTable)) {
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

  if (!ExpectEndProperty()) {
    return false;
  }

  AppendValue(eCSSProperty_paint_order, value);
  return true;
}

} // anonymous namespace

// Recycling of parser implementation objects

static CSSParserImpl* gFreeList = nullptr;

nsCSSParser::nsCSSParser(mozilla::css::Loader* aLoader,
                         nsCSSStyleSheet* aSheet)
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
nsCSSParser::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    SetStyleSheet(aSheet);
}

nsresult
nsCSSParser::SetQuirkMode(bool aQuirkMode)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    SetQuirkMode(aQuirkMode);
}

nsresult
nsCSSParser::SetChildLoader(mozilla::css::Loader* aChildLoader)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    SetChildLoader(aChildLoader);
}

nsresult
nsCSSParser::ParseSheet(const nsAString& aInput,
                        nsIURI*          aSheetURI,
                        nsIURI*          aBaseURI,
                        nsIPrincipal*    aSheetPrincipal,
                        uint32_t         aLineNumber,
                        bool             aAllowUnsafeRules)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseSheet(aInput, aSheetURI, aBaseURI, aSheetPrincipal, aLineNumber,
               aAllowUnsafeRules);
}

nsresult
nsCSSParser::ParseStyleAttribute(const nsAString&  aAttributeValue,
                                 nsIURI*           aDocURI,
                                 nsIURI*           aBaseURI,
                                 nsIPrincipal*     aNodePrincipal,
                                 css::StyleRule**  aResult)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseStyleAttribute(aAttributeValue, aDocURI, aBaseURI,
                        aNodePrincipal, aResult);
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

nsresult
nsCSSParser::ParseProperty(const nsCSSProperty aPropID,
                           const nsAString&    aPropValue,
                           nsIURI*             aSheetURI,
                           nsIURI*             aBaseURI,
                           nsIPrincipal*       aSheetPrincipal,
                           css::Declaration*   aDeclaration,
                           bool*               aChanged,
                           bool                aIsImportant,
                           bool                aIsSVGMode)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseProperty(aPropID, aPropValue, aSheetURI, aBaseURI,
                  aSheetPrincipal, aDeclaration, aChanged,
                  aIsImportant, aIsSVGMode);
}

nsresult
nsCSSParser::ParseMediaList(const nsSubstring& aBuffer,
                            nsIURI*            aURI,
                            uint32_t           aLineNumber,
                            nsMediaList*       aMediaList,
                            bool               aHTMLMode)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseMediaList(aBuffer, aURI, aLineNumber, aMediaList, aHTMLMode);
}

bool
nsCSSParser::ParseColorString(const nsSubstring& aBuffer,
                              nsIURI*            aURI,
                              uint32_t           aLineNumber,
                              nsCSSValue&        aValue)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseColorString(aBuffer, aURI, aLineNumber, aValue);
}

nsresult
nsCSSParser::ParseSelectorString(const nsSubstring&  aSelectorString,
                                 nsIURI*             aURI,
                                 uint32_t            aLineNumber,
                                 nsCSSSelectorList** aSelectorList)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseSelectorString(aSelectorString, aURI, aLineNumber, aSelectorList);
}

already_AddRefed<nsCSSKeyframeRule>
nsCSSParser::ParseKeyframeRule(const nsSubstring& aBuffer,
                               nsIURI*            aURI,
                               uint32_t           aLineNumber)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    ParseKeyframeRule(aBuffer, aURI, aLineNumber);
}

bool
nsCSSParser::ParseKeyframeSelectorString(const nsSubstring& aSelectorString,
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
                                       nsIPrincipal* aDocPrincipal)
{
  return static_cast<CSSParserImpl*>(mImpl)->
    EvaluateSupportsCondition(aCondition, aDocURL, aBaseURL, aDocPrincipal);
}
