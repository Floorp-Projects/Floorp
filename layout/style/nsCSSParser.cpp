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
 *   emk <VYV03354@nifty.ne.jp>
 *   Daniel Glazman <glazman@netscape.com>
 *   L. David Baron <dbaron@dbaron.org>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Christian Biesinger <cbiesinger@web.de>
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

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#include "nsICSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsCSSScanner.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleRule.h"
#include "nsICSSImportRule.h"
#include "nsCSSRules.h"
#include "nsICSSNameSpaceRule.h"
#include "nsIUnicharInputStream.h"
#include "nsICSSStyleSheet.h"
#include "nsCSSDeclaration.h"
#include "nsStyleConsts.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsCOMArray.h"
#include "nsColor.h"
#include "nsStyleConsts.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsINameSpaceManager.h"
#include "nsXMLNameSpaceMap.h"
#include "nsThemeConstants.h"
#include "nsContentErrors.h"
#include "nsPrintfCString.h"
#include "nsIMediaList.h"
#include "nsILookAndFeel.h"
#include "nsStyleUtil.h"
#include "nsIPrincipal.h"
#include "prprf.h"
#include "math.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

// Flags for ParseVariant method
#define VARIANT_KEYWORD         0x000001  // K
#define VARIANT_LENGTH          0x000002  // L
#define VARIANT_PERCENT         0x000004  // P
#define VARIANT_COLOR           0x000008  // C eCSSUnit_Color, eCSSUnit_String (e.g.  "red")
#define VARIANT_URL             0x000010  // U
#define VARIANT_NUMBER          0x000020  // N
#define VARIANT_INTEGER         0x000040  // I
#define VARIANT_ANGLE           0x000080  // G
#define VARIANT_FREQUENCY       0x000100  // F
#define VARIANT_TIME            0x000200  // T
#define VARIANT_STRING          0x000400  // S
#define VARIANT_COUNTER         0x000800  //
#define VARIANT_ATTR            0x001000  //
#define VARIANT_IDENTIFIER      0x002000  // D
#define VARIANT_AUTO            0x010000  // A
#define VARIANT_INHERIT         0x020000  // H eCSSUnit_Initial, eCSSUnit_Inherit
#define VARIANT_NONE            0x040000  // O
#define VARIANT_NORMAL          0x080000  // M
#define VARIANT_SYSFONT         0x100000  // eCSSUnit_System_Font

// Common combinations of variants
#define VARIANT_AL   (VARIANT_AUTO | VARIANT_LENGTH)
#define VARIANT_LP   (VARIANT_LENGTH | VARIANT_PERCENT)
#define VARIANT_AH   (VARIANT_AUTO | VARIANT_INHERIT)
#define VARIANT_AHLP (VARIANT_AH | VARIANT_LP)
#define VARIANT_AHI  (VARIANT_AH | VARIANT_INTEGER)
#define VARIANT_AHK  (VARIANT_AH | VARIANT_KEYWORD)
#define VARIANT_AHKLP (VARIANT_AHLP | VARIANT_KEYWORD)
#define VARIANT_AUK  (VARIANT_AUTO | VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_AHUK (VARIANT_AH | VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_AHL  (VARIANT_AH | VARIANT_LENGTH)
#define VARIANT_AHKL (VARIANT_AHK | VARIANT_LENGTH)
#define VARIANT_HK   (VARIANT_INHERIT | VARIANT_KEYWORD)
#define VARIANT_HKF  (VARIANT_HK | VARIANT_FREQUENCY)
#define VARIANT_HKL  (VARIANT_HK | VARIANT_LENGTH)
#define VARIANT_HKLP (VARIANT_HK | VARIANT_LP)
#define VARIANT_HKLPO (VARIANT_HKLP | VARIANT_NONE)
#define VARIANT_HL   (VARIANT_INHERIT | VARIANT_LENGTH)
#define VARIANT_HI   (VARIANT_INHERIT | VARIANT_INTEGER)
#define VARIANT_HLP  (VARIANT_HL | VARIANT_PERCENT)
#define VARIANT_HLPN (VARIANT_HLP | VARIANT_NUMBER)
#define VARIANT_HLPO (VARIANT_HLP | VARIANT_NONE)
#define VARIANT_HTP  (VARIANT_INHERIT | VARIANT_TIME | VARIANT_PERCENT)
#define VARIANT_HMK  (VARIANT_HK | VARIANT_NORMAL)
#define VARIANT_HMKI (VARIANT_HMK | VARIANT_INTEGER)
#define VARIANT_HC   (VARIANT_INHERIT | VARIANT_COLOR)
#define VARIANT_HCK  (VARIANT_HK | VARIANT_COLOR)
#define VARIANT_HUO  (VARIANT_INHERIT | VARIANT_URL | VARIANT_NONE)
#define VARIANT_AHUO (VARIANT_AUTO | VARIANT_HUO)
#define VARIANT_HPN  (VARIANT_INHERIT | VARIANT_PERCENT | VARIANT_NUMBER)
#define VARIANT_HOK  (VARIANT_HK | VARIANT_NONE)
#define VARIANT_HN   (VARIANT_INHERIT | VARIANT_NUMBER)
#define VARIANT_HON  (VARIANT_HN | VARIANT_NONE)
#define VARIANT_HOS  (VARIANT_INHERIT | VARIANT_NONE | VARIANT_STRING)

//----------------------------------------------------------------------

// Your basic top-down recursive descent style parser
class CSSParserImpl : public nsICSSParser {
public:
  CSSParserImpl();
  virtual ~CSSParserImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);

  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode);

#ifdef  MOZ_SVG
  NS_IMETHOD SetSVGMode(PRBool aSVGMode);
#endif

  NS_IMETHOD SetChildLoader(nsICSSLoader* aChildLoader);

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aSheetURI,
                   nsIURI*                aBaseURI,
                   nsIPrincipal*          aSheetPrincipal,
                   PRUint32               aLineNumber,
                   PRBool                 aAllowUnsafeRules);

  NS_IMETHOD ParseStyleAttribute(const nsAString&  aAttributeValue,
                                 nsIURI*           aDocURL,
                                 nsIURI*           aBaseURL,
                                 nsIPrincipal*     aNodePrincipal,
                                 nsICSSStyleRule** aResult);

  NS_IMETHOD ParseAndAppendDeclaration(const nsAString&  aBuffer,
                                       nsIURI*           aSheetURL,
                                       nsIURI*           aBaseURL,
                                       nsIPrincipal*     aSheetPrincipal,
                                       nsCSSDeclaration* aDeclaration,
                                       PRBool            aParseOnlyOneDecl,
                                       PRBool*           aChanged,
                                       PRBool            aClearOldDecl);

  NS_IMETHOD ParseRule(const nsAString&        aRule,
                       nsIURI*                 aSheetURL,
                       nsIURI*                 aBaseURL,
                       nsIPrincipal*           aSheetPrincipal,
                       nsCOMArray<nsICSSRule>& aResult);

  NS_IMETHOD ParseProperty(const nsCSSProperty aPropID,
                           const nsAString& aPropValue,
                           nsIURI* aSheetURL,
                           nsIURI* aBaseURL,
                           nsIPrincipal* aSheetPrincipal,
                           nsCSSDeclaration* aDeclaration,
                           PRBool* aChanged);

  NS_IMETHOD ParseMediaList(const nsSubstring& aBuffer,
                            nsIURI* aURL, // for error reporting
                            PRUint32 aLineNumber, // for error reporting
                            nsMediaList* aMediaList,
                            PRBool aHTMLMode);

  NS_IMETHOD ParseColorString(const nsSubstring& aBuffer,
                              nsIURI* aURL, // for error reporting
                              PRUint32 aLineNumber, // for error reporting
                              nscolor* aColor);

  NS_IMETHOD ParseSelectorString(const nsSubstring& aSelectorString,
                                 nsIURI* aURL, // for error reporting
                                 PRUint32 aLineNumber, // for error reporting
                                 nsCSSSelectorList **aSelectorList);

  void AppendRule(nsICSSRule* aRule);

protected:
  class nsAutoParseCompoundProperty;
  friend class nsAutoParseCompoundProperty;

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
        aParser->SetParsingCompoundProperty(PR_TRUE);
      }

      ~nsAutoParseCompoundProperty()
      {
        mParser->SetParsingCompoundProperty(PR_FALSE);
      }
    private:
      CSSParserImpl* mParser;
  };

  void InitScanner(nsIUnicharInputStream* aInput, nsIURI* aSheetURI,
                   PRUint32 aLineNumber, nsIURI* aBaseURI,
                   nsIPrincipal* aSheetPrincipal);
  // the caller must hold on to aBuffer until parsing is done
  void InitScanner(const nsSubstring& aString, nsIURI* aSheetURI,
                   PRUint32 aLineNumber, nsIURI* aBaseURI,
                   nsIPrincipal* aSheetPrincipal);
  void ReleaseScanner(void);
#ifdef MOZ_SVG
  PRBool IsSVGMode() const {
    return mScanner.IsSVGMode();
  }
#endif

  PRBool GetToken(PRBool aSkipWS);
  PRBool GetURLToken();
  void UngetToken();

  void AssertInitialState() {
    NS_PRECONDITION(!mHTMLMediaMode, "Bad initial state");
    NS_PRECONDITION(!mUnresolvablePrefixException, "Bad initial state");
    NS_PRECONDITION(!mParsingCompoundProperty, "Bad initial state");
  }

  PRBool ExpectSymbol(PRUnichar aSymbol, PRBool aSkipWS);
  PRBool ExpectEndProperty();
  PRBool CheckEndProperty();
  nsSubstring* NextIdent();
  void SkipUntil(PRUnichar aStopSymbol);
  void SkipUntilOneOf(const PRUnichar* aStopSymbolChars);
  void SkipRuleSet();
  PRBool SkipAtRule();
  PRBool SkipDeclaration(PRBool aCheckForBraces);
  PRBool GetNonCloseParenToken(PRBool aSkipWS);

  PRBool PushGroup(nsICSSGroupRule* aRule);
  void PopGroup(void);

  PRBool ParseRuleSet(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseAtRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseCharsetRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseImportRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool GatherURL(nsString& aURL);
  PRBool GatherMedia(nsMediaList* aMedia,
                     PRUnichar aStopSymbol);
  PRBool ParseMediaQuery(PRUnichar aStopSymbol, nsMediaQuery **aQuery,
                         PRBool *aParsedSomething, PRBool *aHitStop);
  PRBool ParseMediaQueryExpression(nsMediaQuery* aQuery);
  PRBool ProcessImport(const nsString& aURLSpec,
                       nsMediaList* aMedia,
                       RuleAppendFunc aAppendFunc,
                       void* aProcessData);
  PRBool ParseGroupRule(nsICSSGroupRule* aRule, RuleAppendFunc aAppendFunc,
                        void* aProcessData);
  PRBool ParseMediaRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseMozDocumentRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ProcessNameSpace(const nsString& aPrefix,
                          const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                          void* aProcessData);

  PRBool ParseFontFaceRule(RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseFontDescriptor(nsCSSFontFaceRule* aRule);
  PRBool ParseFontDescriptorValue(nsCSSFontDesc aDescID,
                                  nsCSSValue& aValue);

  PRBool ParsePageRule(RuleAppendFunc aAppendFunc, void* aProcessData);

  enum nsSelectorParsingStatus {
    // we have parsed a selector and we saw a token that cannot be
    // part of a selector:
    eSelectorParsingStatus_Done,
    // we should continue parsing the selector:
    eSelectorParsingStatus_Continue,
    // same as "Done" but we did not find a selector:
    eSelectorParsingStatus_Empty,
    // we saw an unexpected token or token value,
    // or we saw end-of-file with an unfinished selector:
    eSelectorParsingStatus_Error
  };
  nsSelectorParsingStatus ParseIDSelector(PRInt32&       aDataMask,
                                          nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseClassSelector(PRInt32&       aDataMask,
                                             nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParsePseudoSelector(PRInt32&       aDataMask,
                                              nsCSSSelector& aSelector,
                                              PRBool         aIsNegated);

  nsSelectorParsingStatus ParseAttributeSelector(PRInt32&       aDataMask,
                                                 nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseTypeOrUniversalSelector(PRInt32&       aDataMask,
                                                       nsCSSSelector& aSelector,
                                                       PRBool         aIsNegated);

  nsSelectorParsingStatus ParsePseudoClassWithIdentArg(nsCSSSelector& aSelector,
                                                       nsIAtom*       aPseudo);

  nsSelectorParsingStatus ParsePseudoClassWithNthPairArg(nsCSSSelector& aSelector,
                                                         nsIAtom*       aPseudo);

  nsSelectorParsingStatus ParseNegatedSimpleSelector(PRInt32&       aDataMask,
                                                     nsCSSSelector& aSelector);

  nsSelectorParsingStatus ParseSelector(nsCSSSelector& aSelectorResult);

  // If aTerminateAtBrace is true, the selector list is done when we
  // hit a '{'.  Otherwise, it's done when we hit EOF.
  PRBool ParseSelectorList(nsCSSSelectorList*& aListHead,
                           PRBool aTerminateAtBrace);
  PRBool ParseSelectorGroup(nsCSSSelectorList*& aListHead);
  nsCSSDeclaration* ParseDeclarationBlock(PRBool aCheckForBraces);
  PRBool ParseDeclaration(nsCSSDeclaration* aDeclaration,
                          PRBool aCheckForBraces,
                          PRBool aMustCallValueAppended,
                          PRBool* aChanged);
  // After a parse error parsing |aPropID|, clear the data in
  // |mTempData|.
  void ClearTempData(nsCSSProperty aPropID);
  // After a successful parse of |aPropID|, transfer data from
  // |mTempData| to |mData|.  Set |*aChanged| to true if something
  // changed, but leave it unmodified otherwise.  If aMustCallValueAppended
  // is false, will not call ValueAppended on aDeclaration if the property
  // is already set in it.
  void TransferTempData(nsCSSDeclaration* aDeclaration,
                        nsCSSProperty aPropID, PRBool aIsImportant,
                        PRBool aMustCallValueAppended,
                        PRBool* aChanged);
  void DoTransferTempData(nsCSSDeclaration* aDeclaration,
                          nsCSSProperty aPropID, PRBool aIsImportant,
                          PRBool aMustCallValueAppended,
                          PRBool* aChanged);
  PRBool ParseProperty(nsCSSProperty aPropID);
  PRBool ParseSingleValueProperty(nsCSSValue& aValue,
                                  nsCSSProperty aPropID);

#ifdef MOZ_XUL
  PRBool ParseTreePseudoElement(nsCSSSelector& aSelector);
#endif

  void InitBoxPropsAsPhysical(const nsCSSProperty *aSourceProperties);

  // Property specific parsing routines
  PRBool ParseAzimuth(nsCSSValue& aValue);
  PRBool ParseBackground();
  PRBool ParseBackgroundPosition();
  PRBool ParseBackgroundPositionValues();
  PRBool ParseBoxPosition(nsCSSValuePair& aOut);
  PRBool ParseBoxPositionValues(nsCSSValuePair& aOut);
  PRBool ParseBorderColor();
  PRBool ParseBorderColors(nsCSSValueList** aResult,
                           nsCSSProperty aProperty);
  PRBool ParseBorderImage();
  PRBool ParseBorderSpacing();
  PRBool ParseBorderSide(const nsCSSProperty aPropIDs[],
                         PRBool aSetAllSides);
  PRBool ParseDirectionalBorderSide(const nsCSSProperty aPropIDs[],
                                    PRInt32 aSourceType);
  PRBool ParseBorderStyle();
  PRBool ParseBorderWidth();
  // for 'clip' and '-moz-image-region'
  PRBool ParseRect(nsCSSRect& aRect,
                   nsCSSProperty aPropID);
  PRBool DoParseRect(nsCSSRect& aRect);
  PRBool ParseContent();
  PRBool ParseCounterData(nsCSSValuePairList** aResult,
                          nsCSSProperty aPropID);
  PRBool ParseCue();
  PRBool ParseCursor();
  PRBool ParseFont();
  PRBool ParseFontWeight(nsCSSValue& aValue);
  PRBool ParseOneFamily(nsAString& aValue);
  PRBool ParseFamily(nsCSSValue& aValue);
  PRBool ParseFontSrc(nsCSSValue& aValue);
  PRBool ParseFontSrcFormat(nsTArray<nsCSSValue>& values);
  PRBool ParseFontRanges(nsCSSValue& aValue);
  PRBool ParseListStyle();
  PRBool ParseMargin();
  PRBool ParseMarks(nsCSSValue& aValue);
  PRBool ParseMozTransform();
  PRBool ParseOutline();
  PRBool ParseOverflow();
  PRBool ParsePadding();
  PRBool ParsePause();
  PRBool ParseQuotes();
  PRBool ParseSize();
  PRBool ParseTextDecoration(nsCSSValue& aValue);

  nsCSSValueList* ParseCSSShadowList(PRBool aUsesSpread);
  PRBool ParseTextShadow();
  PRBool ParseBoxShadow();

#ifdef MOZ_SVG
  PRBool ParsePaint(nsCSSValuePair* aResult,
                    nsCSSProperty aPropID);
  PRBool ParseDasharray();
  PRBool ParseMarker();
#endif

  // Reused utility parsing routines
  void AppendValue(nsCSSProperty aPropID, const nsCSSValue& aValue);
  PRBool ParseBoxProperties(nsCSSRect& aResult,
                            const nsCSSProperty aPropIDs[]);
  PRBool ParseDirectionalBoxProperty(nsCSSProperty aProperty,
                                     PRInt32 aSourceType);
  PRBool ParseBoxCornerRadius(const nsCSSProperty aPropID);
  PRBool ParseBoxCornerRadii(nsCSSCornerSizes& aRadii,
                             const nsCSSProperty aPropIDs[]);
  PRInt32 ParseChoice(nsCSSValue aValues[],
                      const nsCSSProperty aPropIDs[], PRInt32 aNumIDs);
  PRBool ParseColor(nsCSSValue& aValue);
  PRBool ParseColorComponent(PRUint8& aComponent,
                             PRInt32& aType, char aStop);
  // ParseHSLColor parses everything starting with the opening '('
  // up through and including the aStop char.
  PRBool ParseHSLColor(nscolor& aColor, char aStop);
  // ParseColorOpacity will enforce that the color ends with a ')'
  // after the opacity
  PRBool ParseColorOpacity(PRUint8& aOpacity);
  PRBool ParseEnum(nsCSSValue& aValue, const PRInt32 aKeywordTable[]);
  PRBool ParseVariant(nsCSSValue& aValue,
                      PRInt32 aVariantMask,
                      const PRInt32 aKeywordTable[]);
  PRBool ParsePositiveVariant(nsCSSValue& aValue,
                              PRInt32 aVariantMask,
                              const PRInt32 aKeywordTable[]);
  PRBool ParseCounter(nsCSSValue& aValue);
  PRBool ParseAttr(nsCSSValue& aValue);
  PRBool ParseURL(nsCSSValue& aValue);
  PRBool TranslateDimension(nsCSSValue& aValue, PRInt32 aVariantMask,
                            float aNumber, const nsString& aUnit);

  void SetParsingCompoundProperty(PRBool aBool) {
    NS_ASSERTION(aBool == PR_TRUE || aBool == PR_FALSE, "bad PRBool value");
    mParsingCompoundProperty = aBool;
  }
  PRBool IsParsingCompoundProperty(void) const {
    return mParsingCompoundProperty;
  }

  /* Functions for -moz-transform Parsing */
  PRBool ReadSingleTransform(nsCSSValueList**& aTail);
  PRBool ParseFunction(const nsString &aFunction, const PRInt32 aAllowedTypes[],
                       PRUint16 aMinElems, PRUint16 aMaxElems,
                       nsCSSValue &aValue);
  PRBool ParseFunctionInternals(const PRInt32 aVariantMask[],
                                PRUint16 aMinElems,
                                PRUint16 aMaxElems,
                                nsTArray<nsCSSValue>& aOutput);

  /* Functions for -moz-transform-origin Parsing */
  PRBool ParseMozTransformOrigin();


  /* Find and return the correct namespace ID for the prefix aPrefix.
     If the prefix cannot be resolved to a namespace, this method will
     return false.  Otherwise it will return true.  When returning
     false, it may set the low-level error code, depending on the
     value of mUnresolvablePrefixException.

     This method never returns kNameSpaceID_Unknown or
     kNameSpaceID_None for aNameSpaceID while returning true.
  */
  PRBool GetNamespaceIdForPrefix(const nsString& aPrefix,
                                 PRInt32* aNameSpaceID);

  /* Find the correct default namespace, and set it on aSelector. */
  void SetDefaultNamespaceOnSelector(nsCSSSelector& aSelector);

  // Current token. The value is valid after calling GetToken and invalidated
  // by UngetToken.
  nsCSSToken mToken;

  // Our scanner.
  nsCSSScanner mScanner;

  // The URI to be used as a base for relative URIs.
  nsCOMPtr<nsIURI> mBaseURL;

  // The URI to be used as an HTTP "Referer" and for error reporting.
  nsCOMPtr<nsIURI> mSheetURL;

  // The principal of the sheet involved
  nsCOMPtr<nsIPrincipal> mSheetPrincipal;

  // The sheet we're parsing into
  nsCOMPtr<nsICSSStyleSheet> mSheet;

  // Used for @import rules
  nsICSSLoader* mChildLoader; // not ref counted, it owns us

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
  PRPackedBool mHavePushBack : 1;

  // True if we are in quirks mode; false in standards or almost standards mode
  PRPackedBool  mNavQuirkMode : 1;

  // True if unsafe rules should be allowed
  PRPackedBool mUnsafeRulesEnabled : 1;

  // True for parsing media lists for HTML attributes, where we have to
  // ignore CSS comments.
  PRPackedBool mHTMLMediaMode : 1;

  // True if tagnames and attributes are case-sensitive
  PRPackedBool  mCaseSensitive : 1;

  // This flag is set when parsing a non-box shorthand; it's used to not apply
  // some quirks during shorthand parsing
  PRPackedBool  mParsingCompoundProperty : 1;

  // If this flag is true, failure to resolve a namespace prefix
  // should set the low-level error to NS_ERROR_DOM_NAMESPACE_ERR
  PRPackedBool  mUnresolvablePrefixException : 1;

  // Stack of rule groups; used for @media and such.
  nsCOMArray<nsICSSGroupRule> mGroupStack;

  // During the parsing of a property (which may be a shorthand), the data
  // are stored in |mTempData|.  (It is needed to ensure that parser
  // errors cause the data to be ignored, and to ensure that a
  // non-'!important' declaration does not override an '!important'
  // one.)
  nsCSSExpandedDataBlock mTempData;

  // All data from successfully parsed properties are placed into |mData|.
  nsCSSExpandedDataBlock mData;

#ifdef DEBUG
  PRPackedBool mScannerInited;
#endif
};

static void AppendRuleToArray(nsICSSRule* aRule, void* aArray)
{
  static_cast<nsCOMArray<nsICSSRule>*>(aArray)->AppendObject(aRule);
}

static void AppendRuleToSheet(nsICSSRule* aRule, void* aParser)
{
  CSSParserImpl* parser = (CSSParserImpl*) aParser;
  parser->AppendRule(aRule);
}

nsresult
NS_NewCSSParser(nsICSSParser** aInstancePtrResult)
{
  CSSParserImpl *it = new CSSParserImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsICSSParser), (void **) aInstancePtrResult);
}

#ifdef CSS_REPORT_PARSE_ERRORS

#define REPORT_UNEXPECTED(msg_) \
  mScanner.ReportUnexpected(#msg_)

#define REPORT_UNEXPECTED_P(msg_, params_) \
  mScanner.ReportUnexpectedParams(#msg_, params_, NS_ARRAY_LENGTH(params_))

#define REPORT_UNEXPECTED_EOF(lf_) \
  mScanner.ReportUnexpectedEOF(#lf_)

#define REPORT_UNEXPECTED_EOF_CHAR(ch_) \
  mScanner.ReportUnexpectedEOF(ch_)

#define REPORT_UNEXPECTED_TOKEN(msg_) \
  mScanner.ReportUnexpectedToken(mToken, #msg_)

#define REPORT_UNEXPECTED_TOKEN_P(msg_, params_) \
  mScanner.ReportUnexpectedTokenParams(mToken, #msg_, \
                                       params_, NS_ARRAY_LENGTH(params_))


#define OUTPUT_ERROR() \
  mScanner.OutputError()

#define CLEAR_ERROR() \
  mScanner.ClearError()

#else

#define REPORT_UNEXPECTED(msg_)
#define REPORT_UNEXPECTED_P(msg_, params_)
#define REPORT_UNEXPECTED_EOF(lf_)
#define REPORT_UNEXPECTED_EOF_CHAR(ch_)
#define REPORT_UNEXPECTED_TOKEN(msg_)
#define REPORT_UNEXPECTED_TOKEN_P(msg_, params_)
#define OUTPUT_ERROR()
#define CLEAR_ERROR()

#endif

CSSParserImpl::CSSParserImpl()
  : mToken(),
    mScanner(),
    mChildLoader(nsnull),
    mSection(eCSSSection_Charset),
    mNameSpaceMap(nsnull),
    mHavePushBack(PR_FALSE),
    mNavQuirkMode(PR_FALSE),
    mUnsafeRulesEnabled(PR_FALSE),
    mHTMLMediaMode(PR_FALSE),
    mCaseSensitive(PR_FALSE),
    mParsingCompoundProperty(PR_FALSE),
    mUnresolvablePrefixException(PR_FALSE)
#ifdef DEBUG
    , mScannerInited(PR_FALSE)
#endif
{
}

NS_IMPL_ISUPPORTS1(CSSParserImpl, nsICSSParser)

CSSParserImpl::~CSSParserImpl()
{
  mData.AssertInitialState();
  mTempData.AssertInitialState();
}

NS_IMETHODIMP
CSSParserImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  if (aSheet != mSheet) {
    // Switch to using the new sheet, if any
    mGroupStack.Clear();
    mSheet = aSheet;
    if (mSheet) {
      mNameSpaceMap = mSheet->GetNameSpaceMap();
    } else {
      mNameSpaceMap = nsnull;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::SetCaseSensitive(PRBool aCaseSensitive)
{
  NS_ASSERTION(aCaseSensitive == PR_TRUE || aCaseSensitive == PR_FALSE, "bad PRBool value");
  mCaseSensitive = aCaseSensitive;
  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::SetQuirkMode(PRBool aQuirkMode)
{
  NS_ASSERTION(aQuirkMode == PR_TRUE || aQuirkMode == PR_FALSE, "bad PRBool value");
  mNavQuirkMode = aQuirkMode;
  return NS_OK;
}

#ifdef MOZ_SVG
NS_IMETHODIMP
CSSParserImpl::SetSVGMode(PRBool aSVGMode)
{
  NS_ASSERTION(aSVGMode == PR_TRUE || aSVGMode == PR_FALSE,
               "bad PRBool value");
  mScanner.SetSVGMode(aSVGMode);
  return NS_OK;
}
#endif

NS_IMETHODIMP
CSSParserImpl::SetChildLoader(nsICSSLoader* aChildLoader)
{
  mChildLoader = aChildLoader;  // not ref counted, it owns us
  return NS_OK;
}

void
CSSParserImpl::InitScanner(nsIUnicharInputStream* aInput, nsIURI* aSheetURI,
                           PRUint32 aLineNumber, nsIURI* aBaseURI,
                           nsIPrincipal* aSheetPrincipal)
{
  NS_ASSERTION(! mScannerInited, "already have scanner");

  mScanner.Init(aInput, nsnull, 0, aSheetURI, aLineNumber);
#ifdef DEBUG
  mScannerInited = PR_TRUE;
#endif
  mBaseURL = aBaseURI;
  mSheetURL = aSheetURI;
  mSheetPrincipal = aSheetPrincipal;

  mHavePushBack = PR_FALSE;
}

void
CSSParserImpl::InitScanner(const nsSubstring& aString, nsIURI* aSheetURI,
                           PRUint32 aLineNumber, nsIURI* aBaseURI,
                           nsIPrincipal* aSheetPrincipal)
{
  // Having it not own the string is OK since the caller will hold on to
  // the stream until we're done parsing.
  NS_ASSERTION(! mScannerInited, "already have scanner");

  mScanner.Init(nsnull, aString.BeginReading(), aString.Length(), aSheetURI, aLineNumber);

#ifdef DEBUG
  mScannerInited = PR_TRUE;
#endif
  mBaseURL = aBaseURI;
  mSheetURL = aSheetURI;
  mSheetPrincipal = aSheetPrincipal;

  mHavePushBack = PR_FALSE;
}

void
CSSParserImpl::ReleaseScanner(void)
{
  mScanner.Close();
#ifdef DEBUG
  mScannerInited = PR_FALSE;
#endif
  mBaseURL = nsnull;
  mSheetURL = nsnull;
  mSheetPrincipal = nsnull;
}


NS_IMETHODIMP
CSSParserImpl::Parse(nsIUnicharInputStream* aInput,
                     nsIURI*                aSheetURI,
                     nsIURI*                aBaseURI,
                     nsIPrincipal*          aSheetPrincipal,
                     PRUint32               aLineNumber,
                     PRBool                 aAllowUnsafeRules)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");

  NS_ASSERTION(nsnull != aBaseURI, "need base URL");
  NS_ASSERTION(nsnull != aSheetURI, "need sheet URL");
  AssertInitialState();

  NS_PRECONDITION(mSheet, "Must have sheet to parse into");
  NS_ENSURE_STATE(mSheet);

#ifdef DEBUG
  nsCOMPtr<nsIURI> uri;
  mSheet->GetSheetURI(getter_AddRefs(uri));
  PRBool equal;
  NS_ASSERTION(NS_SUCCEEDED(aSheetURI->Equals(uri, &equal)) && equal,
               "Sheet URI does not match passed URI");
  NS_ASSERTION(NS_SUCCEEDED(mSheet->Principal()->Equals(aSheetPrincipal,
                                                        &equal)) &&
               equal,
               "Sheet principal does not match passed principal");
#endif

  InitScanner(aInput, aSheetURI, aLineNumber, aBaseURI, aSheetPrincipal);

  PRInt32 ruleCount = 0;
  mSheet->StyleRuleCount(ruleCount);
  if (0 < ruleCount) {
    nsICSSRule* lastRule = nsnull;
    mSheet->GetStyleRuleAt(ruleCount - 1, lastRule);
    if (lastRule) {
      PRInt32 type;
      lastRule->GetType(type);
      switch (type) {
        case nsICSSRule::CHARSET_RULE:
        case nsICSSRule::IMPORT_RULE:
          mSection = eCSSSection_Import;
          break;
        case nsICSSRule::NAMESPACE_RULE:
          mSection = eCSSSection_NameSpace;
          break;
        default:
          mSection = eCSSSection_General;
          break;
      }
      NS_RELEASE(lastRule);
    }
  }
  else {
    mSection = eCSSSection_Charset; // sheet is empty, any rules are fair
  }

  mUnsafeRulesEnabled = aAllowUnsafeRules;

  nsCSSToken* tk = &mToken;
  for (;;) {
    // Get next non-whitespace token
    if (!GetToken(PR_TRUE)) {
      OUTPUT_ERROR();
      break;
    }
    if (eCSSToken_HTMLComment == tk->mType) {
      continue; // legal here only
    }
    if (eCSSToken_AtKeyword == tk->mType) {
      ParseAtRule(AppendRuleToSheet, this);
      continue;
    }
    UngetToken();
    if (ParseRuleSet(AppendRuleToSheet, this)) {
      mSection = eCSSSection_General;
    }
  }
  ReleaseScanner();

  mUnsafeRulesEnabled = PR_FALSE;

  // XXX check for low level errors
  return NS_OK;
}

/**
 * Determines whether the identifier contained in the given string is a
 * vendor-specific identifier, as described in CSS 2.1 section 4.1.2.1.
 */
static PRBool
NonMozillaVendorIdentifier(const nsAString& ident)
{
  return (ident.First() == PRUnichar('-') &&
          !StringBeginsWith(ident, NS_LITERAL_STRING("-moz-"))) ||
         ident.First() == PRUnichar('_');

}

NS_IMETHODIMP
CSSParserImpl::ParseStyleAttribute(const nsAString& aAttributeValue,
                                   nsIURI*                  aDocURL,
                                   nsIURI*                  aBaseURL,
                                   nsIPrincipal*            aNodePrincipal,
                                   nsICSSStyleRule**        aResult)
{
  NS_PRECONDITION(aNodePrincipal, "Must have principal here!");
  AssertInitialState();

  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

  // XXX line number?
  InitScanner(aAttributeValue, aDocURL, 0, aBaseURL, aNodePrincipal);

  mSection = eCSSSection_General;

  // In quirks mode, allow style declarations to have braces or not
  // (bug 99554).
  PRBool haveBraces;
  if (mNavQuirkMode && GetToken(PR_TRUE)) {
    haveBraces = eCSSToken_Symbol == mToken.mType &&
                 '{' == mToken.mSymbol;
    UngetToken();
  }
  else {
    haveBraces = PR_FALSE;
  }

  nsCSSDeclaration* declaration = ParseDeclarationBlock(haveBraces);
  if (declaration) {
    // Create a style rule for the declaration
    nsICSSStyleRule* rule = nsnull;
    nsresult rv = NS_NewCSSStyleRule(&rule, nsnull, declaration);
    if (NS_FAILED(rv)) {
      declaration->RuleAbort();
      ReleaseScanner();
      return rv;
    }
    *aResult = rule;
  }
  else {
    *aResult = nsnull;
  }

  ReleaseScanner();

  // XXX check for low level errors
  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseAndAppendDeclaration(const nsAString&  aBuffer,
                                         nsIURI*           aSheetURL,
                                         nsIURI*           aBaseURL,
                                         nsIPrincipal*     aSheetPrincipal,
                                         nsCSSDeclaration* aDeclaration,
                                         PRBool            aParseOnlyOneDecl,
                                         PRBool*           aChanged,
                                         PRBool            aClearOldDecl)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  AssertInitialState();

  *aChanged = PR_FALSE;

  InitScanner(aBuffer, aSheetURL, 0, aBaseURL, aSheetPrincipal);

  mSection = eCSSSection_General;

  if (aClearOldDecl) {
    mData.AssertInitialState();
    aDeclaration->ClearData();
    // We could check if it was already empty, but...
    *aChanged = PR_TRUE;
  } else {
    aDeclaration->ExpandTo(&mData);
  }

  nsresult rv = NS_OK;
  do {
    // If we cleared the old decl, then we want to be calling
    // ValueAppended as we parse.
    if (!ParseDeclaration(aDeclaration, PR_FALSE, aClearOldDecl, aChanged)) {
      rv = mScanner.GetLowLevelError();
      if (NS_FAILED(rv))
        break;

      if (!SkipDeclaration(PR_FALSE)) {
        rv = mScanner.GetLowLevelError();
        break;
      }
    }
  } while (!aParseOnlyOneDecl);
  aDeclaration->CompressFrom(&mData);

  ReleaseScanner();
  return rv;
}

NS_IMETHODIMP
CSSParserImpl::ParseRule(const nsAString&        aRule,
                         nsIURI*                 aSheetURL,
                         nsIURI*                 aBaseURL,
                         nsIPrincipal*           aSheetPrincipal,
                         nsCOMArray<nsICSSRule>& aResult)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  AssertInitialState();

  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

  InitScanner(aRule, aSheetURL, 0, aBaseURL, aSheetPrincipal);

  mSection = eCSSSection_Charset; // callers are responsible for rejecting invalid rules.

  nsCSSToken* tk = &mToken;
  // Get first non-whitespace token
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED(PEParseRuleWSOnly);
    OUTPUT_ERROR();
  } else if (eCSSToken_AtKeyword == tk->mType) {
    ParseAtRule(AppendRuleToArray, &aResult);
  }
  else {
    UngetToken();
    ParseRuleSet(AppendRuleToArray, &aResult);
  }
  OUTPUT_ERROR();
  ReleaseScanner();
  // XXX check for low-level errors
  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseProperty(const nsCSSProperty aPropID,
                             const nsAString& aPropValue,
                             nsIURI* aSheetURL,
                             nsIURI* aBaseURL,
                             nsIPrincipal* aSheetPrincipal,
                             nsCSSDeclaration* aDeclaration,
                             PRBool* aChanged)
{
  NS_PRECONDITION(aSheetPrincipal, "Must have principal here!");
  AssertInitialState();

  NS_ASSERTION(nsnull != aBaseURL, "need base URL");
  NS_ASSERTION(nsnull != aDeclaration, "Need declaration to parse into!");
  *aChanged = PR_FALSE;

  InitScanner(aPropValue, aSheetURL, 0, aBaseURL, aSheetPrincipal);

  mSection = eCSSSection_General;

  if (eCSSProperty_UNKNOWN == aPropID) { // unknown property
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    const PRUnichar *params[] = {
      propName.get()
    };
    REPORT_UNEXPECTED_P(PEUnknownProperty, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ReleaseScanner();
    return NS_OK;
  }

  mData.AssertInitialState();
  mTempData.AssertInitialState();
  aDeclaration->ExpandTo(&mData);
  nsresult result = NS_OK;
  PRBool parsedOK = ParseProperty(aPropID);
  if (parsedOK && !GetToken(PR_TRUE)) {
    TransferTempData(aDeclaration, aPropID, PR_FALSE, PR_FALSE, aChanged);
  } else {
    if (parsedOK) {
      // Junk at end of property value.
      REPORT_UNEXPECTED_TOKEN(PEExpectEndValue);
    }
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    const PRUnichar *params[] = {
      propName.get()
    };
    REPORT_UNEXPECTED_P(PEValueParsingError, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ClearTempData(aPropID);
    result = mScanner.GetLowLevelError();
  }
  CLEAR_ERROR();

  aDeclaration->CompressFrom(&mData);

  ReleaseScanner();
  return result;
}

NS_IMETHODIMP
CSSParserImpl::ParseMediaList(const nsSubstring& aBuffer,
                              nsIURI* aURL, // for error reporting
                              PRUint32 aLineNumber, // for error reporting
                              nsMediaList* aMediaList,
                              PRBool aHTMLMode)
{
  // XXX Are there cases where the caller wants to keep what it already
  // has in case of parser error?
  aMediaList->Clear();

  // fake base URL since media lists don't have URLs in them
  InitScanner(aBuffer, aURL, aLineNumber, aURL, nsnull);

  AssertInitialState();
  NS_ASSERTION(aHTMLMode == PR_TRUE || aHTMLMode == PR_FALSE,
               "invalid PRBool");
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

  if (!GatherMedia(aMediaList, PRUnichar(0))) {
    aMediaList->Clear();
    aMediaList->SetNonEmpty(); // don't match anything
    if (!mHTMLMediaMode) {
      OUTPUT_ERROR();
    }
  }
  nsresult rv = mScanner.GetLowLevelError();
  CLEAR_ERROR();
  ReleaseScanner();
  mHTMLMediaMode = PR_FALSE;

  return rv;
}

NS_IMETHODIMP
CSSParserImpl::ParseColorString(const nsSubstring& aBuffer,
                                nsIURI* aURL, // for error reporting
                                PRUint32 aLineNumber, // for error reporting
                                nscolor* aColor)
{
  AssertInitialState();
  InitScanner(aBuffer, aURL, aLineNumber, aURL, nsnull);

  nsCSSValue value;
  PRBool colorParsed = ParseColor(value);
  nsresult rv = mScanner.GetLowLevelError();
  OUTPUT_ERROR();
  ReleaseScanner();

  if (!colorParsed) {
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  if (value.GetUnit() == eCSSUnit_String) {
    nscolor rgba;
    if (NS_ColorNameToRGB(nsDependentString(value.GetStringBufferValue()), &rgba)) {
      (*aColor) = rgba;
      rv = NS_OK;
    }
  } else if (value.GetUnit() == eCSSUnit_Color) {
    (*aColor) = value.GetColorValue();
    rv = NS_OK;
  } else if (value.GetUnit() == eCSSUnit_EnumColor) {
    PRInt32 intValue = value.GetIntValue();
    if (intValue >= 0) {
      nsCOMPtr<nsILookAndFeel> lfSvc = do_GetService("@mozilla.org/widget/lookandfeel;1");
      if (lfSvc) {
        nscolor rgba;
        rv = lfSvc->GetColor((nsILookAndFeel::nsColorID) value.GetIntValue(), rgba);
        if (NS_SUCCEEDED(rv))
          (*aColor) = rgba;
      }
    } else {
      // XXX - this is NS_COLOR_CURRENTCOLOR, NS_COLOR_MOZ_HYPERLINKTEXT, etc.
      // which we don't handle as per the ParseColorString definition.  Should
      // remove this limitation at some point.
      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

NS_IMETHODIMP
CSSParserImpl::ParseSelectorString(const nsSubstring& aSelectorString,
                                   nsIURI* aURL, // for error reporting
                                   PRUint32 aLineNumber, // for error reporting
                                   nsCSSSelectorList **aSelectorList)
{
  InitScanner(aSelectorString, aURL, aLineNumber, aURL, nsnull);

  AssertInitialState();

  mUnresolvablePrefixException = PR_TRUE;

  PRBool success = ParseSelectorList(*aSelectorList, PR_FALSE);
  nsresult rv = mScanner.GetLowLevelError();
  OUTPUT_ERROR();
  ReleaseScanner();

  mUnresolvablePrefixException = PR_FALSE;

  if (success) {
    NS_ASSERTION(*aSelectorList, "Should have list!");
    return NS_OK;
  }

  NS_ASSERTION(!*aSelectorList, "Shouldn't have list!");
  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_DOM_SYNTAX_ERR;
  }
  return rv;
}

//----------------------------------------------------------------------

PRBool
CSSParserImpl::GetToken(PRBool aSkipWS)
{
  for (;;) {
    if (!mHavePushBack) {
      if (!mScanner.Next(mToken)) {
        break;
      }
    }
    mHavePushBack = PR_FALSE;
    if (aSkipWS && (eCSSToken_WhiteSpace == mToken.mType)) {
      continue;
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::GetURLToken()
{
  for (;;) {
    // XXXldb This pushback code doesn't make sense.
    if (! mHavePushBack) {
      if (! mScanner.NextURL(mToken)) {
        break;
      }
    }
    mHavePushBack = PR_FALSE;
    if (eCSSToken_WhiteSpace != mToken.mType) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
CSSParserImpl::UngetToken()
{
  NS_PRECONDITION(mHavePushBack == PR_FALSE, "double pushback");
  mHavePushBack = PR_TRUE;
}

PRBool
CSSParserImpl::ExpectSymbol(PRUnichar aSymbol,
                            PRBool aSkipWS)
{
  if (!GetToken(aSkipWS)) {
    // CSS2.1 specifies that all "open constructs" are to be closed at
    // EOF.  It simplifies higher layers if we claim to have found an
    // ), ], }, or ; if we encounter EOF while looking for one of them.
    // Do still issue a diagnostic, to aid debugging.
    if (aSymbol == ')' || aSymbol == ']' ||
        aSymbol == '}' || aSymbol == ';') {
      REPORT_UNEXPECTED_EOF_CHAR(aSymbol);
      return PR_TRUE;
    }
    else
      return PR_FALSE;
  }
  if (mToken.IsSymbol(aSymbol)) {
    return PR_TRUE;
  }
  UngetToken();
  return PR_FALSE;
}

// Checks to see if we're at the end of a property.  If an error occurs during
// the check, does not signal a parse error.
PRBool
CSSParserImpl::CheckEndProperty()
{
  if (!GetToken(PR_TRUE)) {
    return PR_TRUE; // properties may end with eof
  }
  if ((eCSSToken_Symbol == mToken.mType) &&
      ((';' == mToken.mSymbol) ||
       ('!' == mToken.mSymbol) ||
       ('}' == mToken.mSymbol))) {
    // XXX need to verify that ! is only followed by "important [;|}]
    // XXX this requires a multi-token pushback buffer
    UngetToken();
    return PR_TRUE;
  }
  UngetToken();
  return PR_FALSE;
}

// Checks if we're at the end of a property, raising an error if we're not.
PRBool
CSSParserImpl::ExpectEndProperty()
{
  if (CheckEndProperty())
    return PR_TRUE;

  // If we're here, we read something incorrect, so we should report it.
  REPORT_UNEXPECTED_TOKEN(PRExpectEndValue);
  return PR_FALSE;
}


nsSubstring*
CSSParserImpl::NextIdent()
{
  // XXX Error reporting?
  if (!GetToken(PR_TRUE)) {
    return nsnull;
  }
  if (eCSSToken_Ident != mToken.mType) {
    UngetToken();
    return nsnull;
  }
  return &mToken.mIdent;
}

PRBool
CSSParserImpl::SkipAtRule()
{
  for (;;) {
    if (!GetToken(PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PESkipAtRuleEOF);
      return PR_FALSE;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      PRUnichar symbol = mToken.mSymbol;
      if (symbol == ';') {
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
    }
  }
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseAtRule(RuleAppendFunc aAppendFunc,
                           void* aData)
{
  if ((mSection <= eCSSSection_Charset) &&
      (mToken.mIdent.LowerCaseEqualsLiteral("charset"))) {
    if (ParseCharsetRule(aAppendFunc, aData)) {
      mSection = eCSSSection_Import;  // only one charset allowed
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_Import) &&
      mToken.mIdent.LowerCaseEqualsLiteral("import")) {
    if (ParseImportRule(aAppendFunc, aData)) {
      mSection = eCSSSection_Import;
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_NameSpace) &&
      mToken.mIdent.LowerCaseEqualsLiteral("namespace")) {
    if (ParseNameSpaceRule(aAppendFunc, aData)) {
      mSection = eCSSSection_NameSpace;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("media")) {
    if (ParseMediaRule(aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-document")) {
    if (ParseMozDocumentRule(aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("font-face")) {
    if (ParseFontFaceRule(aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("page")) {
    if (ParsePageRule(aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }

  if (!NonMozillaVendorIdentifier(mToken.mIdent)) {
    REPORT_UNEXPECTED_TOKEN(PEUnknownAtRule);
    OUTPUT_ERROR();
  }

  // Skip over unsupported at rule, don't advance section
  return SkipAtRule();
}

PRBool
CSSParserImpl::ParseCharsetRule(RuleAppendFunc aAppendFunc,
                                void* aData)
{
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PECharsetRuleEOF);
    return PR_FALSE;
  }

  if (eCSSToken_String != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PECharsetRuleNotString);
    return PR_FALSE;
  }

  nsAutoString charset = mToken.mIdent;

  if (!ExpectSymbol(';', PR_TRUE)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsICSSRule> rule;
  NS_NewCSSCharsetRule(getter_AddRefs(rule), charset);

  if (rule) {
    (*aAppendFunc)(rule, aData);
  }

  return PR_TRUE;
}

PRBool
CSSParserImpl::GatherURL(nsString& aURL)
{
  if (!GetToken(PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_String == mToken.mType) {
    aURL = mToken.mIdent;
    return PR_TRUE;
  }
  else if (eCSSToken_Function == mToken.mType &&
           mToken.mIdent.LowerCaseEqualsLiteral("url") &&
           ExpectSymbol('(', PR_FALSE) &&
           GetURLToken() &&
           (eCSSToken_String == mToken.mType ||
            eCSSToken_URL == mToken.mType)) {
    aURL = mToken.mIdent;
    if (ExpectSymbol(')', PR_TRUE)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseMediaQuery(PRUnichar aStopSymbol,
                               nsMediaQuery **aQuery,
                               PRBool *aParsedSomething,
                               PRBool *aHitStop)
{
  *aQuery = nsnull;
  *aParsedSomething = PR_FALSE;
  *aHitStop = PR_FALSE;

  // "If the comma-separated list is the empty list it is assumed to
  // specify the media query 'all'."  (css3-mediaqueries, section
  // "Media Queries")
  if (!GetToken(PR_TRUE)) {
    *aHitStop = PR_TRUE;
    // expected termination by EOF
    if (aStopSymbol == PRUnichar(0))
      return PR_TRUE;

    // unexpected termination by EOF
    REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
    return PR_TRUE;
  }

  if (eCSSToken_Symbol == mToken.mType &&
      mToken.mSymbol == aStopSymbol) {
    *aHitStop = PR_TRUE;
    UngetToken();
    return PR_TRUE;
  }
  UngetToken();

  *aParsedSomething = PR_TRUE;

  nsAutoPtr<nsMediaQuery> query(new nsMediaQuery);
  if (!query) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  if (ExpectSymbol('(', PR_TRUE)) {
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
    PRBool gotNotOrOnly = PR_FALSE;
    for (;;) {
      if (!GetToken(PR_TRUE)) {
        REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
        return PR_FALSE;
      }
      if (eCSSToken_Ident != mToken.mType) {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotIdent);
        UngetToken();
        return PR_FALSE;
      }
      // case insensitive from CSS - must be lower cased
      ToLowerCase(mToken.mIdent);
      mediaType = do_GetAtom(mToken.mIdent);
      if (gotNotOrOnly ||
          (mediaType != nsGkAtoms::_not && mediaType != nsGkAtoms::only))
        break;
      gotNotOrOnly = PR_TRUE;
      if (mediaType == nsGkAtoms::_not)
        query->SetNegated();
      else
        query->SetHasOnly();
    }
    query->SetType(mediaType);
  }

  for (;;) {
    if (!GetToken(PR_TRUE)) {
      *aHitStop = PR_TRUE;
      // expected termination by EOF
      if (aStopSymbol == PRUnichar(0))
        break;

      // unexpected termination by EOF
      REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
      break;
    }

    if (eCSSToken_Symbol == mToken.mType &&
        mToken.mSymbol == aStopSymbol) {
      *aHitStop = PR_TRUE;
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
      return PR_FALSE;
    }
    if (!ParseMediaQueryExpression(query)) {
      OUTPUT_ERROR();
      query->SetHadUnknownExpression();
    }
  }
  *aQuery = query.forget();
  return PR_TRUE;
}

// Returns false only when there is a low-level error in the scanner
// (out-of-memory).
PRBool
CSSParserImpl::GatherMedia(nsMediaList* aMedia,
                           PRUnichar aStopSymbol)
{
  for (;;) {
    nsAutoPtr<nsMediaQuery> query;
    PRBool parsedSomething, hitStop;
    if (!ParseMediaQuery(aStopSymbol, getter_Transfers(query),
                         &parsedSomething, &hitStop)) {
      NS_ASSERTION(!hitStop, "should return true when hit stop");
      if (NS_FAILED(mScanner.GetLowLevelError())) {
        return PR_FALSE;
      }
      const PRUnichar stopChars[] =
        { PRUnichar(','), aStopSymbol /* may be null */, PRUnichar(0) };
      SkipUntilOneOf(stopChars);
      // Rely on SkipUntilOneOf leaving mToken around as the last token read.
      if (mToken.mType == eCSSToken_Symbol && mToken.mSymbol == aStopSymbol) {
        UngetToken();
        hitStop = PR_TRUE;
      }
    }
    if (parsedSomething) {
      aMedia->SetNonEmpty();
    }
    if (query) {
      nsresult rv = aMedia->AppendQuery(query);
      if (NS_FAILED(rv)) {
        mScanner.SetLowLevelError(rv);
        return PR_FALSE;
      }
    }
    if (hitStop) {
      break;
    }
  }
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseMediaQueryExpression(nsMediaQuery* aQuery)
{
  if (!ExpectSymbol('(', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedExpressionStart);
    return PR_FALSE;
  }
  if (! GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEMQExpressionEOF);
    return PR_FALSE;
  }
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedFeatureName);
    SkipUntil(')');
    return PR_FALSE;
  }

  nsMediaExpression *expr = aQuery->NewExpression();
  if (!expr) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    SkipUntil(')');
    return PR_FALSE;
  }

  // case insensitive from CSS - must be lower cased
  ToLowerCase(mToken.mIdent);
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
    return PR_FALSE;
  }
  expr->mFeature = feature;

  if (!GetToken(PR_TRUE) || mToken.IsSymbol(')')) {
    // Query expressions for any feature can be given without a value.
    // However, min/max prefixes are not allowed.
    if (expr->mRange != nsMediaExpression::eEqual) {
      REPORT_UNEXPECTED(PEMQNoMinMaxWithoutValue);
      return PR_FALSE;
    }
    expr->mValue.Reset();
    return PR_TRUE;
  }

  if (!mToken.IsSymbol(':')) {
    REPORT_UNEXPECTED_TOKEN(PEMQExpectedFeatureNameEnd);
    SkipUntil(')');
    return PR_FALSE;
  }

  PRBool rv;
  switch (feature->mValueType) {
    case nsMediaFeature::eLength:
      rv = ParsePositiveVariant(expr->mValue, VARIANT_LENGTH, nsnull);
      break;
    case nsMediaFeature::eInteger:
    case nsMediaFeature::eBoolInteger:
      rv = ParsePositiveVariant(expr->mValue, VARIANT_INTEGER, nsnull);
      // Enforce extra restrictions for eBoolInteger
      if (rv &&
          feature->mValueType == nsMediaFeature::eBoolInteger &&
          expr->mValue.GetIntValue() > 1)
        rv = PR_FALSE;
      break;
    case nsMediaFeature::eIntRatio:
      {
        // Two integers separated by '/', with optional whitespace on
        // either side of the '/'.
        nsRefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);
        if (!a) {
          mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
          SkipUntil(')');
          return PR_FALSE;
        }
        expr->mValue.SetArrayValue(a, eCSSUnit_Array);
        // We don't bother with ParsePositiveVariant since we have to
        // check for != 0 as well; no need to worry about the UngetToken
        // since we're throwing out up to the next ')' anyway.
        rv = ParseVariant(a->Item(0), VARIANT_INTEGER, nsnull) &&
             a->Item(0).GetIntValue() > 0 &&
             ExpectSymbol('/', PR_TRUE) &&
             ParseVariant(a->Item(1), VARIANT_INTEGER, nsnull) &&
             a->Item(1).GetIntValue() > 0;
      }
      break;
    case nsMediaFeature::eResolution:
      rv = GetToken(PR_TRUE) && mToken.IsDimension() &&
           mToken.mIntegerValid && mToken.mNumber > 0.0f;
      if (rv) {
        // No worries about whether unitless zero is allowed, since the
        // value must be positive (and we checked that above).
        NS_ASSERTION(!mToken.mIdent.IsEmpty(), "IsDimension lied");
        if (mToken.mIdent.LowerCaseEqualsLiteral("dpi")) {
          expr->mValue.SetFloatValue(mToken.mNumber, eCSSUnit_Inch);
        } else if (mToken.mIdent.LowerCaseEqualsLiteral("dpcm")) {
          expr->mValue.SetFloatValue(mToken.mNumber, eCSSUnit_Centimeter);
        } else {
          rv = PR_FALSE;
        }
      }
      break;
    case nsMediaFeature::eEnumerated:
      rv = ParseVariant(expr->mValue, VARIANT_KEYWORD,
                        feature->mKeywordTable);
      break;
  }
  if (!rv || !ExpectSymbol(')', PR_TRUE)) {
    REPORT_UNEXPECTED(PEMQExpectedFeatureValue);
    SkipUntil(')');
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Parse a CSS2 import rule: "@import STRING | URL [medium [, medium]]"
PRBool
CSSParserImpl::ParseImportRule(RuleAppendFunc aAppendFunc, void* aData)
{
  nsCOMPtr<nsMediaList> media = new nsMediaList();
  if (!media) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  nsAutoString url;
  if (!GatherURL(url)) {
    REPORT_UNEXPECTED_TOKEN(PEImportNotURI);
    return PR_FALSE;
  }

  if (!ExpectSymbol(';', PR_TRUE)) {
    if (!GatherMedia(media, ';') ||
        !ExpectSymbol(';', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(PEImportUnexpected);
      // don't advance section, simply ignore invalid @import
      return PR_FALSE;
    }

    // Safe to assert this, since we ensured that there is something
    // other than the ';' coming after the @import's url() token.
    NS_ASSERTION(media->Count() != 0, "media list must be nonempty");
  }

  ProcessImport(url, media, aAppendFunc, aData);
  return PR_TRUE;
}


PRBool
CSSParserImpl::ProcessImport(const nsString& aURLSpec,
                             nsMediaList* aMedia,
                             RuleAppendFunc aAppendFunc,
                             void* aData)
{
  nsCOMPtr<nsICSSImportRule> rule;
  nsresult rv = NS_NewCSSImportRule(getter_AddRefs(rule), aURLSpec, aMedia);
  if (NS_FAILED(rv)) {
    mScanner.SetLowLevelError(rv);
    return PR_FALSE;
  }
  (*aAppendFunc)(rule, aData);

  if (mChildLoader) {
    nsCOMPtr<nsIURI> url;
    // XXX should pass a charset!
    rv = NS_NewURI(getter_AddRefs(url), aURLSpec, nsnull, mBaseURL);

    if (NS_FAILED(rv)) {
      // import url is bad
      // XXX log this somewhere for easier web page debugging
      mScanner.SetLowLevelError(rv);
      return PR_FALSE;
    }

    mChildLoader->LoadChildSheet(mSheet, url, aMedia, rule);
  }

  return PR_TRUE;
}

// Parse the {} part of an @media or @-moz-document rule.
PRBool
CSSParserImpl::ParseGroupRule(nsICSSGroupRule* aRule,
                              RuleAppendFunc aAppendFunc,
                              void* aData)
{
  // XXXbz this could use better error reporting throughout the method
  if (!ExpectSymbol('{', PR_TRUE)) {
    return PR_FALSE;
  }

  // push rule on stack, loop over children
  if (!PushGroup(aRule)) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }
  nsCSSSection holdSection = mSection;
  mSection = eCSSSection_General;

  for (;;) {
    // Get next non-whitespace token
    if (! GetToken(PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEGroupRuleEOF);
      break;
    }
    if (mToken.IsSymbol('}')) { // done!
      UngetToken();
      break;
    }
    if (eCSSToken_AtKeyword == mToken.mType) {
      SkipAtRule(); // group rules cannot contain @rules
      continue;
    }
    UngetToken();
    ParseRuleSet(AppendRuleToSheet, this);
  }
  PopGroup();

  if (!ExpectSymbol('}', PR_TRUE)) {
    mSection = holdSection;
    return PR_FALSE;
  }
  (*aAppendFunc)(aRule, aData);
  return PR_TRUE;
}

// Parse a CSS2 media rule: "@media medium [, medium] { ... }"
PRBool
CSSParserImpl::ParseMediaRule(RuleAppendFunc aAppendFunc, void* aData)
{
  nsCOMPtr<nsMediaList> media = new nsMediaList();
  if (!media) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  if (GatherMedia(media, '{')) {
    // XXXbz this could use better error reporting throughout the method
    nsRefPtr<nsCSSMediaRule> rule(new nsCSSMediaRule());
    // Append first, so when we do SetMedia() the rule
    // knows what its stylesheet is.
    if (rule && ParseGroupRule(rule, aAppendFunc, aData)) {
      rule->SetMedia(media);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// Parse a @-moz-document rule.  This is like an @media rule, but instead
// of a medium it has a nonempty list of items where each item is either
// url(), url-prefix(), or domain().
PRBool
CSSParserImpl::ParseMozDocumentRule(RuleAppendFunc aAppendFunc, void* aData)
{
  nsCSSDocumentRule::URL *urls = nsnull;
  nsCSSDocumentRule::URL **next = &urls;
  do {
    if (!GetToken(PR_TRUE) ||
        eCSSToken_Function != mToken.mType ||
        !(mToken.mIdent.LowerCaseEqualsLiteral("url") ||
          mToken.mIdent.LowerCaseEqualsLiteral("url-prefix") ||
          mToken.mIdent.LowerCaseEqualsLiteral("domain"))) {
      REPORT_UNEXPECTED_TOKEN(PEMozDocRuleBadFunc);
      delete urls;
      return PR_FALSE;
    }
    nsCSSDocumentRule::URL *cur = *next = new nsCSSDocumentRule::URL;
    if (!cur) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      delete urls;
      return PR_FALSE;
    }
    next = &cur->next;
    if (mToken.mIdent.LowerCaseEqualsLiteral("url")) {
      cur->func = nsCSSDocumentRule::eURL;
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("url-prefix")) {
      cur->func = nsCSSDocumentRule::eURLPrefix;
    } else if (mToken.mIdent.LowerCaseEqualsLiteral("domain")) {
      cur->func = nsCSSDocumentRule::eDomain;
    }

    if (!ExpectSymbol('(', PR_FALSE) ||
        !GetURLToken() ||
        (eCSSToken_String != mToken.mType &&
         eCSSToken_URL != mToken.mType)) {
      REPORT_UNEXPECTED_TOKEN(PEMozDocRuleNotURI);
      delete urls;
      return PR_FALSE;
    }
    if (!ExpectSymbol(')', PR_TRUE)) {
      delete urls;
      return PR_FALSE;
    }

    // We could try to make the URL (as long as it's not domain())
    // canonical and absolute with NS_NewURI and GetSpec, but I'm
    // inclined to think we shouldn't.
    CopyUTF16toUTF8(mToken.mIdent, cur->url);
  } while (ExpectSymbol(',', PR_TRUE));

  nsRefPtr<nsCSSDocumentRule> rule(new nsCSSDocumentRule());
  if (!rule) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    delete urls;
    return PR_FALSE;
  }
  rule->SetURLs(urls);

  return ParseGroupRule(rule, aAppendFunc, aData);
}

// Parse a CSS3 namespace rule: "@namespace [prefix] STRING | URL;"
PRBool
CSSParserImpl::ParseNameSpaceRule(RuleAppendFunc aAppendFunc, void* aData)
{
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEAtNSPrefixEOF);
    return PR_FALSE;
  }

  nsAutoString  prefix;
  nsAutoString  url;

  if (eCSSToken_Ident == mToken.mType) {
    prefix = mToken.mIdent;
    // user-specified identifiers are case-sensitive (bug 416106)
    if (! GetToken(PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEAtNSURIEOF);
      return PR_FALSE;
    }
  }

  if (eCSSToken_String == mToken.mType) {
    url = mToken.mIdent;
    if (ExpectSymbol(';', PR_TRUE)) {
      ProcessNameSpace(prefix, url, aAppendFunc, aData);
      return PR_TRUE;
    }
  }
  else if ((eCSSToken_Function == mToken.mType) &&
           (mToken.mIdent.LowerCaseEqualsLiteral("url"))) {
    if (ExpectSymbol('(', PR_FALSE)) {
      if (GetURLToken()) {
        if ((eCSSToken_String == mToken.mType) || (eCSSToken_URL == mToken.mType)) {
          url = mToken.mIdent;
          if (ExpectSymbol(')', PR_TRUE)) {
            if (ExpectSymbol(';', PR_TRUE)) {
              ProcessNameSpace(prefix, url, aAppendFunc, aData);
              return PR_TRUE;
            }
          }
        }
      }
    }
  }
  REPORT_UNEXPECTED_TOKEN(PEAtNSUnexpected);

  return PR_FALSE;
}

PRBool
CSSParserImpl::ProcessNameSpace(const nsString& aPrefix,
                                const nsString& aURLSpec,
                                RuleAppendFunc aAppendFunc,
                                void* aData)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsICSSNameSpaceRule> rule;
  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = do_GetAtom(aPrefix);
  }

  NS_NewCSSNameSpaceRule(getter_AddRefs(rule), prefix, aURLSpec);
  if (rule) {
    (*aAppendFunc)(rule, aData);

    // If this was the first namespace rule encountered, it will trigger
    // creation of a namespace map.
    if (!mNameSpaceMap) {
      mNameSpaceMap = mSheet->GetNameSpaceMap();
    }
  }

  return result;
}

// font-face-rule: '@font-face' '{' font-description '}'
// font-description: font-descriptor+
PRBool
CSSParserImpl::ParseFontFaceRule(RuleAppendFunc aAppendFunc, void* aData)
{
  if (!ExpectSymbol('{', PR_TRUE))
    return PR_FALSE;

  nsRefPtr<nsCSSFontFaceRule> rule(new nsCSSFontFaceRule());
  if (!rule) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  for (;;) {
    if (!GetToken(PR_TRUE)) {
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
      if (!SkipDeclaration(PR_TRUE))
        break;
    }
  }
  if (!ExpectSymbol('}', PR_TRUE))
    return PR_FALSE;
  (*aAppendFunc)(rule, aData);
  return PR_TRUE;
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

PRBool
CSSParserImpl::ParseFontDescriptor(nsCSSFontFaceRule* aRule)
{
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEFontDescExpected);
    return PR_FALSE;
  }

  nsString descName = mToken.mIdent;
  if (!ExpectSymbol(':', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
    OUTPUT_ERROR();
    return PR_FALSE;
  }

  nsCSSFontDesc descID = nsCSSProps::LookupFontDesc(descName);
  nsCSSValue value;

  if (descID == eCSSFontDesc_UNKNOWN) {
    if (NonMozillaVendorIdentifier(descName)) {
      // silently skip other vendors' extensions
      SkipDeclaration(PR_TRUE);
      return PR_TRUE;
    } else {
      const PRUnichar *params[] = {
        descName.get()
      };
      REPORT_UNEXPECTED_P(PEUnknownFontDesc, params);
      return PR_FALSE;
    }
  }

  if (!ParseFontDescriptorValue(descID, value)) {
    const PRUnichar *params[] = {
      descName.get()
    };
    REPORT_UNEXPECTED_P(PEValueParsingError, params);
    return PR_FALSE;
  }

  if (!ExpectEndProperty())
    return PR_FALSE;

  aRule->SetDesc(descID, value);
  return PR_TRUE;
}


PRBool
CSSParserImpl::ParsePageRule(RuleAppendFunc aAppendFunc, void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

void
CSSParserImpl::SkipUntil(PRUnichar aStopSymbol)
{
  nsCSSToken* tk = &mToken;
  nsAutoTArray<PRUnichar, 16> stack;
  stack.AppendElement(aStopSymbol);
  for (;;) {
    if (!GetToken(PR_TRUE)) {
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
      PRUint32 stackTopIndex = stack.Length() - 1;
      if (symbol == stack.ElementAt(stackTopIndex)) {
        stack.RemoveElementAt(stackTopIndex);
        if (stackTopIndex == 0) {
          break;
        }
      } else if ('{' == symbol) {
        // In this case and the two below, just handle out-of-memory by
        // parsing incorrectly.  It's highly unlikely we're dealing with
        // a legitimate style sheet anyway.
        stack.AppendElement('}');
      } else if ('[' == symbol) {
        stack.AppendElement(']');
      } else if ('(' == symbol) {
        stack.AppendElement(')');
      }
    }
  }
}

void
CSSParserImpl::SkipUntilOneOf(const PRUnichar* aStopSymbolChars)
{
  nsCSSToken* tk = &mToken;
  nsDependentString stopSymbolChars(aStopSymbolChars);
  for (;;) {
    if (!GetToken(PR_TRUE)) {
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
    }
  }
}

PRBool
CSSParserImpl::GetNonCloseParenToken(PRBool aSkipWS)
{
  if (!GetToken(aSkipWS))
    return PR_FALSE;
  if (mToken.mType == eCSSToken_Symbol && mToken.mSymbol == ')') {
    UngetToken();
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
CSSParserImpl::SkipDeclaration(PRBool aCheckForBraces)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(PR_TRUE)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF(PESkipDeclBraceEOF);
      }
      return PR_FALSE;
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
    }
  }
  return PR_TRUE;
}

void
CSSParserImpl::SkipRuleSet()
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PESkipRSBraceEOF);
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
      if ('{' == symbol) {
        SkipUntil('}');
        break;
      }
      if ('(' == symbol) {
        SkipUntil(')');
      } else if ('[' == symbol) {
        SkipUntil(']');
      }
    }
  }
}

PRBool
CSSParserImpl::PushGroup(nsICSSGroupRule* aRule)
{
  if (mGroupStack.AppendObject(aRule))
    return PR_TRUE;

  return PR_FALSE;
}

void
CSSParserImpl::PopGroup(void)
{
  PRInt32 count = mGroupStack.Count();
  if (0 < count) {
    mGroupStack.RemoveObjectAt(count - 1);
  }
}

void
CSSParserImpl::AppendRule(nsICSSRule* aRule)
{
  PRInt32 count = mGroupStack.Count();
  if (0 < count) {
    mGroupStack[count - 1]->AppendStyleRule(aRule);
  }
  else {
    mSheet->AppendStyleRule(aRule);
  }
}

PRBool
CSSParserImpl::ParseRuleSet(RuleAppendFunc aAppendFunc, void* aData)
{
  // First get the list of selectors for the rule
  nsCSSSelectorList* slist = nsnull;
  PRUint32 linenum = mScanner.GetLineNumber();
  if (! ParseSelectorList(slist, PR_TRUE)) {
    REPORT_UNEXPECTED(PEBadSelectorRSIgnored);
    OUTPUT_ERROR();
    SkipRuleSet();
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != slist, "null selector list");
  CLEAR_ERROR();

  // Next parse the declaration block
  nsCSSDeclaration* declaration = ParseDeclarationBlock(PR_TRUE);
  if (nsnull == declaration) {
    // XXX skip something here
    delete slist;
    return PR_FALSE;
  }

#if 0
  slist->Dump();
  fputs("{\n", stdout);
  declaration->List();
  fputs("}\n", stdout);
#endif

  // Translate the selector list and declaration block into style data

  nsCOMPtr<nsICSSStyleRule> rule;
  NS_NewCSSStyleRule(getter_AddRefs(rule), slist, declaration);
  if (!rule) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    delete slist;
    return PR_FALSE;
  }
  rule->SetLineNumber(linenum);
  (*aAppendFunc)(rule, aData);

  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseSelectorList(nsCSSSelectorList*& aListHead,
                                 PRBool aTerminateAtBrace)
{
  nsCSSSelectorList* list = nsnull;
  if (! ParseSelectorGroup(list)) {
    // must have at least one selector group
    aListHead = nsnull;
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != list, "no selector list");
  aListHead = list;

  // After that there must either be a "," or a "{" (the latter if
  // aTerminateAtBrace is true)
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (! GetToken(PR_TRUE)) {
      if (!aTerminateAtBrace) {
        return PR_TRUE;
      }

      REPORT_UNEXPECTED_EOF(PESelectorListExtraEOF);
      break;
    }

    if (eCSSToken_Symbol == tk->mType) {
      if (',' == tk->mSymbol) {
        nsCSSSelectorList* newList = nsnull;
        // Another selector group must follow
        if (! ParseSelectorGroup(newList)) {
          break;
        }
        // add new list to the end of the selector list
        list->mNext = newList;
        list = newList;
        continue;
      } else if ('{' == tk->mSymbol && aTerminateAtBrace) {
        UngetToken();
        return PR_TRUE;
      }
    }
    REPORT_UNEXPECTED_TOKEN(PESelectorListExtra);
    UngetToken();
    break;
  }

  delete aListHead;
  aListHead = nsnull;
  return PR_FALSE;
}

static PRBool IsSinglePseudoClass(const nsCSSSelector& aSelector)
{
  return PRBool((aSelector.mNameSpace == kNameSpaceID_Unknown) &&
                (aSelector.mTag == nsnull) &&
                (aSelector.mIDList == nsnull) &&
                (aSelector.mClassList == nsnull) &&
                (aSelector.mAttrList == nsnull) &&
                (aSelector.mNegations == nsnull) &&
                (aSelector.mPseudoClassList != nsnull) &&
                (aSelector.mPseudoClassList->mNext == nsnull));
}

#ifdef MOZ_XUL
static PRBool IsTreePseudoElement(nsIAtom* aPseudo)
{
  const char* str;
  aPseudo->GetUTF8String(&str);
  static const char moz_tree[] = ":-moz-tree-";
  return nsCRT::strncmp(str, moz_tree, PRInt32(sizeof(moz_tree)-1)) == 0;
}
#endif

PRBool
CSSParserImpl::ParseSelectorGroup(nsCSSSelectorList*& aList)
{
  nsAutoPtr<nsCSSSelectorList> list;
  PRUnichar     combinator = PRUnichar(0);
  PRInt32       weight = 0;
  PRBool        havePseudoElement = PR_FALSE;
  PRBool        done = PR_FALSE;
  while (!done) {
    nsAutoPtr<nsCSSSelector> newSelector(new nsCSSSelector());
    if (!newSelector) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      return PR_FALSE;
    }
    nsSelectorParsingStatus parsingStatus =
      ParseSelector(*newSelector);
    if (parsingStatus == eSelectorParsingStatus_Empty) {
      if (!list) {
        REPORT_UNEXPECTED(PESelectorGroupNoSelector);
      }
      break;
    }
    if (parsingStatus == eSelectorParsingStatus_Error) {
      list = nsnull;
      break;
    }
    if (nsnull == list) {
      list = new nsCSSSelectorList();
      if (nsnull == list) {
        mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
        return PR_FALSE;
      }
    }
    list->AddSelector(newSelector);
    nsCSSSelector* listSel = list->mSelectors;

    // pull out pseudo elements here
    nsPseudoClassList* prevList = nsnull;
    nsPseudoClassList* pseudoClassList = listSel->mPseudoClassList;
    while (nsnull != pseudoClassList) {
      if (! nsCSSPseudoClasses::IsPseudoClass(pseudoClassList->mAtom)) {
        havePseudoElement = PR_TRUE;
        if (IsSinglePseudoClass(*listSel)) {  // convert to pseudo element selector
          nsIAtom* pseudoElement = pseudoClassList->mAtom;  // steal ref count
          pseudoClassList->mAtom = nsnull;
          listSel->Reset();
          if (listSel->mNext) {// more to the selector
            listSel->mOperator = PRUnichar('>');
            nsAutoPtr<nsCSSSelector> empty(new nsCSSSelector());
            if (!empty) {
              mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
              return PR_FALSE;
            }
            list->AddSelector(empty); // leave a blank (universal) selector in the middle
            listSel = list->mSelectors; // use the new one for the pseudo
          }
          listSel->mTag = pseudoElement;
        }
        else {  // append new pseudo element selector
          nsAutoPtr<nsCSSSelector> pseudoTagSelector(new nsCSSSelector());
          if (!pseudoTagSelector) {
            mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
            return PR_FALSE;
          }
          pseudoTagSelector->mTag = pseudoClassList->mAtom; // steal ref count
#ifdef MOZ_XUL
          if (IsTreePseudoElement(pseudoTagSelector->mTag)) {
            // Take the remaining "pseudoclasses" that we parsed
            // inside the tree pseudoelement's ()-list, and
            // make our new selector have these pseudoclasses
            // in its pseudoclass list.
            pseudoTagSelector->mPseudoClassList = pseudoClassList->mNext;
            pseudoClassList->mNext = nsnull;
          }
#endif
          list->AddSelector(pseudoTagSelector);
          pseudoClassList->mAtom = nsnull;
          listSel->mOperator = PRUnichar('>');
          if (nsnull == prevList) { // delete list entry
            listSel->mPseudoClassList = pseudoClassList->mNext;
          }
          else {
            prevList->mNext = pseudoClassList->mNext;
          }
          pseudoClassList->mNext = nsnull;
          delete pseudoClassList;
          weight += listSel->CalcWeight(); // capture weight from remainder
        }
        break;  // only one pseudo element per selector
      }
      prevList = pseudoClassList;
      pseudoClassList = pseudoClassList->mNext;
    }

    combinator = PRUnichar(0);
    if (!GetToken(PR_FALSE)) {
      break;
    }

    // Assume we are done unless we find a combinator here.
    done = PR_TRUE;
    if (eCSSToken_WhiteSpace == mToken.mType) {
      if (!GetToken(PR_TRUE)) {
        break;
      }
      done = PR_FALSE;
    }

    if (eCSSToken_Symbol == mToken.mType &&
        ('+' == mToken.mSymbol ||
         '>' == mToken.mSymbol ||
         '~' == mToken.mSymbol)) {
      done = PR_FALSE;
      combinator = mToken.mSymbol;
      list->mSelectors->SetOperator(combinator);
    }
    else {
      if (eCSSToken_Symbol == mToken.mType &&
          ('{' == mToken.mSymbol ||
           ',' == mToken.mSymbol)) {
        // End of this selector group
        done = PR_TRUE;
      }
      UngetToken(); // give it back to selector if we're not done, or make sure
                    // we see it as the end of the selector if we are.
    }

    if (havePseudoElement) {
      break;
    }
    else {
      weight += listSel->CalcWeight();
    }
  }

  if (PRUnichar(0) != combinator) { // no dangling combinators
    list = nsnull;
    // This should report the problematic combinator
    REPORT_UNEXPECTED(PESelectorGroupExtraCombinator);
  }
  aList = list.forget();
  if (aList) {
    aList->mWeight = weight;
  }
  return PRBool(nsnull != aList);
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
CSSParserImpl::ParseIDSelector(PRInt32&       aDataMask,
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
CSSParserImpl::ParseClassSelector(PRInt32&       aDataMask,
                                  nsCSSSelector& aSelector)
{
  if (! GetToken(PR_FALSE)) { // get ident
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
CSSParserImpl::ParseTypeOrUniversalSelector(PRInt32&       aDataMask,
                                            nsCSSSelector& aSelector,
                                            PRBool         aIsNegated)
{
  nsAutoString buffer;
  if (mToken.IsSymbol('*')) {  // universal element selector, or universal namespace
    if (ExpectSymbol('|', PR_FALSE)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // namespace wildcard

      if (! GetToken(PR_FALSE)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;
        if (mCaseSensitive) {
          aSelector.SetTag(mToken.mIdent);
        }
        else {
          ToLowerCase(mToken.mIdent, buffer);
          aSelector.SetTag(buffer);
        }
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
    if (! GetToken(PR_FALSE)) {   // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) {    // element name or namespace name
    buffer = mToken.mIdent; // hang on to ident

    if (ExpectSymbol('|', PR_FALSE)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      PRInt32 nameSpaceID;
      if (!GetNamespaceIdForPrefix(buffer, &nameSpaceID)) {
        return eSelectorParsingStatus_Error;
      }
      aSelector.SetNameSpace(nameSpaceID);

      if (! GetToken(PR_FALSE)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        return eSelectorParsingStatus_Error;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;
        if (mCaseSensitive) {
          aSelector.SetTag(mToken.mIdent);
        }
        else {
          ToLowerCase(mToken.mIdent, buffer);
          aSelector.SetTag(buffer);
        }
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
      if (mCaseSensitive) {
        aSelector.SetTag(buffer);
      }
      else {
        ToLowerCase(buffer);
        aSelector.SetTag(buffer);
      }
      aDataMask |= SEL_MASK_ELEM;
    }
    if (! GetToken(PR_FALSE)) {   // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  else if (mToken.IsSymbol('|')) {  // No namespace
    aDataMask |= SEL_MASK_NSPACE;
    aSelector.SetNameSpace(kNameSpaceID_None);  // explicit NO namespace

    // get mandatory tag
    if (! GetToken(PR_FALSE)) {
      REPORT_UNEXPECTED_EOF(PETypeSelEOF);
      return eSelectorParsingStatus_Error;
    }
    if (eCSSToken_Ident == mToken.mType) {  // element name
      aDataMask |= SEL_MASK_ELEM;
      if (mCaseSensitive) {
        aSelector.SetTag(mToken.mIdent);
      }
      else {
        ToLowerCase(mToken.mIdent, buffer);
        aSelector.SetTag(buffer);
      }
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
    if (! GetToken(PR_FALSE)) {   // premature eof is ok (here!)
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
CSSParserImpl::ParseAttributeSelector(PRInt32&       aDataMask,
                                      nsCSSSelector& aSelector)
{
  if (! GetToken(PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
    return eSelectorParsingStatus_Error;
  }

  PRInt32 nameSpaceID = kNameSpaceID_None;
  nsAutoString  attr;
  if (mToken.IsSymbol('*')) { // wildcard namespace
    nameSpaceID = kNameSpaceID_Unknown;
    if (ExpectSymbol('|', PR_FALSE)) {
      if (! GetToken(PR_FALSE)) { // premature EOF
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
    if (! GetToken(PR_FALSE)) { // premature EOF
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
    if (ExpectSymbol('|', PR_FALSE)) {  // was a namespace
      if (!GetNamespaceIdForPrefix(attr, &nameSpaceID)) {
        return eSelectorParsingStatus_Error;
      }
      if (! GetToken(PR_FALSE)) { // premature EOF
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

  if (! mCaseSensitive) {
    ToLowerCase(attr);
  }
  if (! GetToken(PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttSelInnerEOF);
    return eSelectorParsingStatus_Error;
  }
  if ((eCSSToken_Symbol == mToken.mType) ||
      (eCSSToken_Includes == mToken.mType) ||
      (eCSSToken_Dashmatch == mToken.mType) ||
      (eCSSToken_Beginsmatch == mToken.mType) ||
      (eCSSToken_Endsmatch == mToken.mType) ||
      (eCSSToken_Containsmatch == mToken.mType)) {
    PRUint8 func;
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
      if (! GetToken(PR_TRUE)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttSelValueEOF);
        return eSelectorParsingStatus_Error;
      }
      if ((eCSSToken_Ident == mToken.mType) || (eCSSToken_String == mToken.mType)) {
        nsAutoString  value(mToken.mIdent);
        if (! GetToken(PR_TRUE)) { // premature EOF
          REPORT_UNEXPECTED_EOF(PEAttSelCloseEOF);
          return eSelectorParsingStatus_Error;
        }
        if (mToken.IsSymbol(']')) {
          PRBool isCaseSensitive = PR_TRUE;

          // If we're parsing a style sheet for an HTML document, and
          // the attribute selector is for a non-namespaced attribute,
          // then check to see if it's one of the known attributes whose
          // VALUE is case-insensitive.
          if (!mCaseSensitive && nameSpaceID == kNameSpaceID_None) {
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
              nsnull
            };
            short i = 0;
            const char* htmlAttr;
            while ((htmlAttr = caseInsensitiveHTMLAttribute[i++])) {
              if (attr.EqualsIgnoreCase(htmlAttr)) {
                isCaseSensitive = PR_FALSE;
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
CSSParserImpl::ParsePseudoSelector(PRInt32&       aDataMask,
                                   nsCSSSelector& aSelector,
                                   PRBool         aIsNegated)
{
  if (! GetToken(PR_FALSE)) { // premature eof
    REPORT_UNEXPECTED_EOF(PEPseudoSelEOF);
    return eSelectorParsingStatus_Error;
  }

  // First, find out whether we are parsing a CSS3 pseudo-element
  PRBool parsingPseudoElement = PR_FALSE;
  if (mToken.IsSymbol(':')) {
    parsingPseudoElement = PR_TRUE;
    if (! GetToken(PR_FALSE)) { // premature eof
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
  ToLowerCase(buffer);
  nsCOMPtr<nsIAtom> pseudo = do_GetAtom(buffer);

  // stash away some info about this pseudo so we only have to get it once.
  PRBool isTreePseudo = PR_FALSE;
#ifdef MOZ_XUL
  isTreePseudo = IsTreePseudoElement(pseudo);
  // If a tree pseudo-element is using the function syntax, it will
  // get isTree set here and will pass the check below that only
  // allows functions if they are in our list of things allowed to be
  // functions.  If it is _not_ using the function syntax, isTree will
  // be false, and it will still pass that check.  So the tree
  // pseudo-elements are allowed to be either functions or not, as
  // desired.
  PRBool isTree = (eCSSToken_Function == mToken.mType) && isTreePseudo;
#endif
  PRBool isPseudoElement = nsCSSPseudoElements::IsPseudoElement(pseudo);
  // anonymous boxes are only allowed if they're the tree boxes or we have
  // enabled unsafe rules
  PRBool isAnonBox = nsCSSAnonBoxes::IsAnonBox(pseudo) &&
    (mUnsafeRulesEnabled || isTreePseudo);
  PRBool isPseudoClass = nsCSSPseudoClasses::IsPseudoClass(pseudo);

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
       nsCSSPseudoClasses::notPseudo == pseudo ||
       nsCSSPseudoClasses::HasStringArg(pseudo) ||
       nsCSSPseudoClasses::HasNthPairArg(pseudo))) {
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

  if (!parsingPseudoElement && nsCSSPseudoClasses::notPseudo == pseudo) {
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
    if (nsCSSPseudoClasses::HasStringArg(pseudo)) {
      nsSelectorParsingStatus parsingStatus =
        ParsePseudoClassWithIdentArg(aSelector, pseudo);
      if (eSelectorParsingStatus_Continue != parsingStatus) {
        return parsingStatus;
      }
    }
    else if (nsCSSPseudoClasses::HasNthPairArg(pseudo)) {
      nsSelectorParsingStatus parsingStatus =
        ParsePseudoClassWithNthPairArg(aSelector, pseudo);
      if (eSelectorParsingStatus_Continue != parsingStatus) {
        return parsingStatus;
      }
    }
    else {
      aSelector.AddPseudoClass(pseudo);
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
      aSelector.AddPseudoClass(pseudo); // store it here, it gets pulled later

#ifdef MOZ_XUL
      if (isTree) {
        // We have encountered a pseudoelement of the form
        // -moz-tree-xxxx(a,b,c).  We parse (a,b,c) and add each
        // item in the list to the pseudoclass list.  They will be pulled
        // from the list later along with the pseudo-element.
        if (!ParseTreePseudoElement(aSelector)) {
          return eSelectorParsingStatus_Error;
        }
      }
#endif

      // ensure selector ends here, must be followed by EOF, space, '{' or ','
      if (GetToken(PR_FALSE)) { // premature eof is ok (here!)
        if ((eCSSToken_WhiteSpace == mToken.mType) ||
            (mToken.IsSymbol('{') || mToken.IsSymbol(','))) {
          UngetToken();
          return eSelectorParsingStatus_Done;
        }
        REPORT_UNEXPECTED_TOKEN(PEPseudoSelTrailing);
        UngetToken();
        return eSelectorParsingStatus_Error;
      }
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
CSSParserImpl::ParseNegatedSimpleSelector(PRInt32&       aDataMask,
                                          nsCSSSelector& aSelector)
{
  // Check if we have the first parenthesis
  if (!ExpectSymbol('(', PR_FALSE)) {
    REPORT_UNEXPECTED_TOKEN(PENegationBadArg);
    return eSelectorParsingStatus_Error;
  }

  if (! GetToken(PR_TRUE)) { // premature eof
    REPORT_UNEXPECTED_EOF(PENegationEOF);
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
  if (!newSel) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return eSelectorParsingStatus_Error;
  }
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
    parsingStatus = ParsePseudoSelector(aDataMask, *newSel, PR_TRUE);
  }
  else if (mToken.IsSymbol('[')) {    // [attribute
    parsingStatus = ParseAttributeSelector(aDataMask, *newSel);
  }
  else {
    // then it should be a type element or universal selector
    parsingStatus = ParseTypeOrUniversalSelector(aDataMask, *newSel, PR_TRUE);
  }
  if (eSelectorParsingStatus_Error == parsingStatus) {
    REPORT_UNEXPECTED_TOKEN(PENegationBadInner);
    return parsingStatus;
  }
  // close the parenthesis
  if (!ExpectSymbol(')', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PENegationNoClose);
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
                                            nsIAtom*       aPseudo)
{
  // Check if we have the first parenthesis
  if (!ExpectSymbol('(', PR_FALSE)) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoArg);
    return eSelectorParsingStatus_Error;
  }

  if (! GetToken(PR_TRUE)) { // premature eof
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }
  // We expect an identifier with a language abbreviation
  if (eCSSToken_Ident != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotIdent);
    UngetToken();
    // XXX Call SkipUntil to the next ")"?
    return eSelectorParsingStatus_Error;
  }

  // Add the pseudo with the language parameter
  aSelector.AddPseudoClass(aPseudo, mToken.mIdent.get());

  // close the parenthesis
  if (!ExpectSymbol(')', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoClose);
    // XXX Call SkipUntil to the next ")"?
    return eSelectorParsingStatus_Error;
  }

  return eSelectorParsingStatus_Continue;
}

CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParsePseudoClassWithNthPairArg(nsCSSSelector& aSelector,
                                              nsIAtom*       aPseudo)
{
  PRInt32 numbers[2] = { 0, 0 };
  PRBool lookForB = PR_TRUE;

  // Check whether we have the first parenthesis
  if (!ExpectSymbol('(', PR_FALSE)) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoArg);
    return eSelectorParsingStatus_Error;
  }

  // Follow the whitespace rules as proposed in
  // http://lists.w3.org/Archives/Public/www-style/2008Mar/0121.html

  if (! GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }

  if (eCSSToken_Ident == mToken.mType || eCSSToken_Dimension == mToken.mType) {
    // The CSS tokenization doesn't handle :nth-child() containing - well:
    //   2n-1 is a dimension
    //   n-1 is an identifier
    // The easiest way to deal with that is to push everything from the
    // minus on back onto the scanner's pushback buffer.
    PRUint32 truncAt = 0;
    if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("n-"))) {
      truncAt = 1;
    } else if (StringBeginsWith(mToken.mIdent, NS_LITERAL_STRING("-n-"))) {
      truncAt = 2;
    }
    if (truncAt != 0) {
      for (PRUint32 i = mToken.mIdent.Length() - 1; i >= truncAt; --i) {
        mScanner.Pushback(mToken.mIdent[i]);
      }
      mToken.mIdent.Truncate(truncAt);
    }
  }

  if (eCSSToken_Ident == mToken.mType) {
    if (mToken.mIdent.EqualsIgnoreCase("odd")) {
      numbers[0] = 2;
      numbers[1] = 1;
      lookForB = PR_FALSE;
    }
    else if (mToken.mIdent.EqualsIgnoreCase("even")) {
      numbers[0] = 2;
      numbers[1] = 0;
      lookForB = PR_FALSE;
    }
    else if (mToken.mIdent.EqualsIgnoreCase("n")) {
      numbers[0] = 1;
    }
    else if (mToken.mIdent.EqualsIgnoreCase("-n")) {
      numbers[0] = -1;
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      // XXX Call SkipUntil to the next ")"?
      return eSelectorParsingStatus_Error;
    }
  }
  else if (eCSSToken_Number == mToken.mType) {
    if (!mToken.mIntegerValid) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      // XXX Call SkipUntil to the next ")"?
      return eSelectorParsingStatus_Error;
    }
    numbers[1] = mToken.mInteger;
    lookForB = PR_FALSE;
  }
  else if (eCSSToken_Dimension == mToken.mType) {
    if (!mToken.mIntegerValid || !mToken.mIdent.EqualsIgnoreCase("n")) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      // XXX Call SkipUntil to the next ")"?
      return eSelectorParsingStatus_Error;
    }
    numbers[0] = mToken.mInteger;
  }
  // XXX If it's a ')', is that valid?  (as 0n+0)
  else {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
    // XXX Call SkipUntil to the next ")" (unless this is one already)?
    return eSelectorParsingStatus_Error;
  }

  if (! GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
    return eSelectorParsingStatus_Error;
  }
  if (lookForB && !mToken.IsSymbol(')')) {
    // The '+' or '-' sign can optionally be separated by whitespace.
    // If it is separated by whitespace from what follows it, it appears
    // as a separate token rather than part of the number token.
    PRBool haveSign = PR_FALSE;
    PRInt32 sign = 1;
    if (mToken.IsSymbol('+') || mToken.IsSymbol('-')) {
      haveSign = PR_TRUE;
      if (mToken.IsSymbol('-')) {
        sign = -1;
      }
      if (! GetToken(PR_TRUE)) {
        REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
        return eSelectorParsingStatus_Error;
      }
    }
    if (eCSSToken_Number != mToken.mType ||
        !mToken.mIntegerValid || mToken.mHasSign == haveSign) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoClassArgNotNth);
      // XXX Call SkipUntil to the next ")"?
      return eSelectorParsingStatus_Error;
    }
    numbers[1] = mToken.mInteger * sign;
    if (! GetToken(PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEPseudoClassArgEOF);
      return eSelectorParsingStatus_Error;
    }
  }
  if (!mToken.IsSymbol(')')) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoClassNoClose);
    // XXX Call SkipUntil to the next ")"?
    return eSelectorParsingStatus_Error;
  }
  aSelector.AddPseudoClass(aPseudo, numbers);
  return eSelectorParsingStatus_Continue;
}


/**
 * This is the format for selectors:
 * operator? [[namespace |]? element_name]? [ ID | class | attrib | pseudo ]*
 */
CSSParserImpl::nsSelectorParsingStatus
CSSParserImpl::ParseSelector(nsCSSSelector& aSelector)
{
  if (! GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PESelectorEOF);
    return eSelectorParsingStatus_Error;
  }

  PRInt32 dataMask = 0;
  nsSelectorParsingStatus parsingStatus =
    ParseTypeOrUniversalSelector(dataMask, aSelector, PR_FALSE);
  if (parsingStatus != eSelectorParsingStatus_Continue) {
    return parsingStatus;
  }

  for (;;) {
    if (eCSSToken_ID == mToken.mType) { // #id
      parsingStatus = ParseIDSelector(dataMask, aSelector);
    }
    else if (mToken.IsSymbol('.')) {    // .class
      parsingStatus = ParseClassSelector(dataMask, aSelector);
    }
    else if (mToken.IsSymbol(':')) {    // :pseudo
      parsingStatus = ParsePseudoSelector(dataMask, aSelector, PR_FALSE);
    }
    else if (mToken.IsSymbol('[')) {    // [attribute
      parsingStatus = ParseAttributeSelector(dataMask, aSelector);
    }
    else {  // not a selector token, we're done
      parsingStatus = eSelectorParsingStatus_Done;
      break;
    }

    if (parsingStatus != eSelectorParsingStatus_Continue) {
      return parsingStatus;
    }

    if (! GetToken(PR_FALSE)) { // premature eof is ok (here!)
      return eSelectorParsingStatus_Done;
    }
  }
  UngetToken();
  return dataMask ? parsingStatus : eSelectorParsingStatus_Empty;
}

nsCSSDeclaration*
CSSParserImpl::ParseDeclarationBlock(PRBool aCheckForBraces)
{
  if (aCheckForBraces) {
    if (!ExpectSymbol('{', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(PEBadDeclBlockStart);
      OUTPUT_ERROR();
      return nsnull;
    }
  }
  nsCSSDeclaration* declaration = new nsCSSDeclaration();
  mData.AssertInitialState();
  if (declaration) {
    for (;;) {
      PRBool changed;
      if (!ParseDeclaration(declaration, aCheckForBraces,
                            PR_TRUE, &changed)) {
        if (!SkipDeclaration(aCheckForBraces)) {
          break;
        }
        if (aCheckForBraces) {
          if (ExpectSymbol('}', PR_TRUE)) {
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

PRBool
CSSParserImpl::ParseColor(nsCSSValue& aValue)
{
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorEOF);
    return PR_FALSE;
  }

  nsCSSToken* tk = &mToken;
  nscolor rgba;
  switch (tk->mType) {
    case eCSSToken_ID:
    case eCSSToken_Ref:
      // #xxyyzz
      if (NS_HexToRGB(tk->mIdent, &rgba)) {
        aValue.SetColorValue(rgba);
        return PR_TRUE;
      }
      break;

    case eCSSToken_Ident:
      if (NS_ColorNameToRGB(tk->mIdent, &rgba)) {
        aValue.SetStringValue(tk->mIdent, eCSSUnit_String);
        return PR_TRUE;
      }
      else {
        nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
        if (eCSSKeyword_UNKNOWN < keyword) { // known keyword
          PRInt32 value;
          if (nsCSSProps::FindKeyword(keyword, nsCSSProps::kColorKTable, value)) {
            aValue.SetIntValue(value, eCSSUnit_EnumColor);
            return PR_TRUE;
          }
        }
      }
      break;
    case eCSSToken_Function:
      if (mToken.mIdent.LowerCaseEqualsLiteral("rgb")) {
        // rgb ( component , component , component )
        PRUint8 r, g, b;
        PRInt32 type = COLOR_TYPE_UNKNOWN;
        if (ExpectSymbol('(', PR_FALSE) && // this won't fail
            ParseColorComponent(r, type, ',') &&
            ParseColorComponent(g, type, ',') &&
            ParseColorComponent(b, type, ')')) {
          aValue.SetColorValue(NS_RGB(r,g,b));
          return PR_TRUE;
        }
        return PR_FALSE;  // already pushed back
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-rgba") ||
               mToken.mIdent.LowerCaseEqualsLiteral("rgba")) {
        // rgba ( component , component , component , opacity )
        PRUint8 r, g, b, a;
        PRInt32 type = COLOR_TYPE_UNKNOWN;
        if (ExpectSymbol('(', PR_FALSE) && // this won't fail
            ParseColorComponent(r, type, ',') &&
            ParseColorComponent(g, type, ',') &&
            ParseColorComponent(b, type, ',') &&
            ParseColorOpacity(a)) {
          aValue.SetColorValue(NS_RGBA(r, g, b, a));
          return PR_TRUE;
        }
        return PR_FALSE;  // already pushed back
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("hsl")) {
        // hsl ( hue , saturation , lightness )
        // "hue" is a number, "saturation" and "lightness" are percentages.
        if (ParseHSLColor(rgba, ')')) {
          aValue.SetColorValue(rgba);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-hsla") ||
               mToken.mIdent.LowerCaseEqualsLiteral("hsla")) {
        // hsla ( hue , saturation , lightness , opacity )
        // "hue" is a number, "saturation" and "lightness" are percentages,
        // "opacity" is a number.
        PRUint8 a;
        if (ParseHSLColor(rgba, ',') &&
            ParseColorOpacity(a)) {
          aValue.SetColorValue(NS_RGBA(NS_GET_R(rgba), NS_GET_G(rgba),
                                       NS_GET_B(rgba), a));
          return PR_TRUE;
        }
        return PR_FALSE;
      }
      break;
    default:
      break;
  }

  // try 'xxyyzz' without '#' prefix for compatibility with IE and Nav4x (bug 23236 and 45804)
  if (mNavQuirkMode && !IsParsingCompoundProperty()) {
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
      return PR_TRUE;
    }
  }

  // It's not a color
  REPORT_UNEXPECTED_TOKEN(PEColorNotColor);
  UngetToken();
  return PR_FALSE;
}

// aType will be set if we have already parsed other color components
// in this color spec
PRBool
CSSParserImpl::ParseColorComponent(PRUint8& aComponent,
                                   PRInt32& aType,
                                   char aStop)
{
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorComponentEOF);
    return PR_FALSE;
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
        return PR_FALSE;
      default:
        NS_NOTREACHED("Someone forgot to add the new color component type in here");
    }

    if (!mToken.mIntegerValid) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedInt);
      UngetToken();
      return PR_FALSE;
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
        return PR_FALSE;
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
    return PR_FALSE;
  }
  if (ExpectSymbol(aStop, PR_TRUE)) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    aComponent = NSToIntRound(value);
    return PR_TRUE;
  }
  const PRUnichar stopString[] = { PRUnichar(aStop), PRUnichar(0) };
  const PRUnichar *params[] = {
    nsnull,
    stopString
  };
  REPORT_UNEXPECTED_TOKEN_P(PEColorComponentBadTerm, params);
  return PR_FALSE;
}


PRBool
CSSParserImpl::ParseHSLColor(nscolor& aColor,
                             char aStop)
{
  float h, s, l;
  if (!ExpectSymbol('(', PR_FALSE)) {
    NS_ERROR("How did this get to be a function token?");
    return PR_FALSE;
  }

  // Get the hue
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorHueEOF);
    return PR_FALSE;
  }
  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return PR_FALSE;
  }
  h = mToken.mNumber;
  h /= 360.0f;
  // hue values are wraparound
  h = h - floor(h);

  if (!ExpectSymbol(',', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return PR_FALSE;
  }

  // Get the saturation
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorSaturationEOF);
    return PR_FALSE;
  }
  if (mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
    UngetToken();
    return PR_FALSE;
  }
  s = mToken.mNumber;
  if (s < 0.0f) s = 0.0f;
  if (s > 1.0f) s = 1.0f;

  if (!ExpectSymbol(',', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return PR_FALSE;
  }

  // Get the lightness
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorLightnessEOF);
    return PR_FALSE;
  }
  if (mToken.mType != eCSSToken_Percentage) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedPercent);
    UngetToken();
    return PR_FALSE;
  }
  l = mToken.mNumber;
  if (l < 0.0f) l = 0.0f;
  if (l > 1.0f) l = 1.0f;

  if (ExpectSymbol(aStop, PR_TRUE)) {
    aColor = NS_HSL2RGB(h, s, l);
    return PR_TRUE;
  }

  const PRUnichar stopString[] = { PRUnichar(aStop), PRUnichar(0) };
  const PRUnichar *params[] = {
    nsnull,
    stopString
  };
  REPORT_UNEXPECTED_TOKEN_P(PEColorComponentBadTerm, params);
  return PR_FALSE;
}


PRBool
CSSParserImpl::ParseColorOpacity(PRUint8& aOpacity)
{
  if (!GetToken(PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorOpacityEOF);
    return PR_FALSE;
  }

  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return PR_FALSE;
  }

  if (mToken.mNumber < 0.0f) {
    mToken.mNumber = 0.0f;
  } else if (mToken.mNumber > 1.0f) {
    mToken.mNumber = 1.0f;
  }

  PRUint8 value = nsStyleUtil::FloatToColorComponent(mToken.mNumber);
  NS_ASSERTION(fabs(mToken.mNumber - value/255.0f) <= 0.5f,
               "FloatToColorComponent did something weird");

  if (!ExpectSymbol(')', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
    return PR_FALSE;
  }

  aOpacity = value;

  return PR_TRUE;
}

#ifdef MOZ_XUL
PRBool
CSSParserImpl::ParseTreePseudoElement(nsCSSSelector& aSelector)
{
  if (ExpectSymbol('(', PR_FALSE)) {
    while (!ExpectSymbol(')', PR_TRUE)) {
      if (!GetToken(PR_TRUE)) {
        return PR_FALSE;
      }
      else if (eCSSToken_Ident == mToken.mType) {
        nsCOMPtr<nsIAtom> pseudo = do_GetAtom(mToken.mIdent);
        aSelector.AddPseudoClass(pseudo);
      }
      else if (eCSSToken_Symbol == mToken.mType) {
        if (!mToken.IsSymbol(','))
          return PR_FALSE;
      }
      else return PR_FALSE;
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

//----------------------------------------------------------------------

PRBool
CSSParserImpl::ParseDeclaration(nsCSSDeclaration* aDeclaration,
                                PRBool aCheckForBraces,
                                PRBool aMustCallValueAppended,
                                PRBool* aChanged)
{
  mTempData.AssertInitialState();

  // Get property name
  nsCSSToken* tk = &mToken;
  nsAutoString propertyName;
  for (;;) {
    if (!GetToken(PR_TRUE)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF(PEDeclEndEOF);
      }
      return PR_FALSE;
    }
    if (eCSSToken_Ident == tk->mType) {
      propertyName = tk->mIdent;
      // grab the ident before the ExpectSymbol trashes the token
      if (!ExpectSymbol(':', PR_TRUE)) {
        REPORT_UNEXPECTED_TOKEN(PEParseDeclarationNoColon);
        REPORT_UNEXPECTED(PEDeclDropped);
        OUTPUT_ERROR();
        return PR_FALSE;
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
    }
    // Not a declaration...
    UngetToken();
    return PR_FALSE;
  }

  // Map property name to its ID and then parse the property
  nsCSSProperty propID = nsCSSProps::LookupProperty(propertyName);
  if (eCSSProperty_UNKNOWN == propID) { // unknown property
    if (!NonMozillaVendorIdentifier(propertyName)) {
      const PRUnichar *params[] = {
        propertyName.get()
      };
      REPORT_UNEXPECTED_P(PEUnknownProperty, params);
      REPORT_UNEXPECTED(PEDeclDropped);
      OUTPUT_ERROR();
    }

    return PR_FALSE;
  }
  if (! ParseProperty(propID)) {
    // XXX Much better to put stuff in the value parsers instead...
    const PRUnichar *params[] = {
      propertyName.get()
    };
    REPORT_UNEXPECTED_P(PEValueParsingError, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ClearTempData(propID);
    return PR_FALSE;
  }
  CLEAR_ERROR();

  // See if the declaration is followed by a "!important" declaration
  PRBool isImportant = PR_FALSE;
  if (!GetToken(PR_TRUE)) {
    // EOF is a perfectly good way to end a declaration and declaration block
    TransferTempData(aDeclaration, propID, isImportant,
                     aMustCallValueAppended, aChanged);
    return PR_TRUE;
  }

  if (eCSSToken_Symbol == tk->mType && '!' == tk->mSymbol) {
    // Look for important ident
    if (!GetToken(PR_TRUE)) {
      // Premature eof is not ok
      REPORT_UNEXPECTED_EOF(PEImportantEOF);
      ClearTempData(propID);
      return PR_FALSE;
    }
    if ((eCSSToken_Ident != tk->mType) ||
        !tk->mIdent.LowerCaseEqualsLiteral("important")) {
      REPORT_UNEXPECTED_TOKEN(PEExpectedImportant);
      OUTPUT_ERROR();
      UngetToken();
      ClearTempData(propID);
      return PR_FALSE;
    }
    isImportant = PR_TRUE;
  }
  else {
    // Not a !important declaration
    UngetToken();
  }

  // Make sure valid property declaration is terminated with either a
  // semicolon, EOF or a right-curly-brace (this last only when
  // aCheckForBraces is true).
  if (!GetToken(PR_TRUE)) {
    // EOF is a perfectly good way to end a declaration and declaration block
    TransferTempData(aDeclaration, propID, isImportant,
                     aMustCallValueAppended, aChanged);
    return PR_TRUE;
  }
  if (eCSSToken_Symbol == tk->mType) {
    if (';' == tk->mSymbol) {
      TransferTempData(aDeclaration, propID, isImportant,
                       aMustCallValueAppended, aChanged);
      return PR_TRUE;
    }
    if (aCheckForBraces && '}' == tk->mSymbol) {
      // Unget the '}' so we'll be able to tell that this is the end
      // of the declaration block when we unwind from here.
      UngetToken();
      TransferTempData(aDeclaration, propID, isImportant,
                       aMustCallValueAppended, aChanged);
      return PR_TRUE;
    }
  }
  if (aCheckForBraces)
    REPORT_UNEXPECTED_TOKEN(PEBadDeclOrRuleEnd2);
  else
    REPORT_UNEXPECTED_TOKEN(PEBadDeclEnd);
  REPORT_UNEXPECTED(PEDeclDropped);
  OUTPUT_ERROR();
  ClearTempData(propID);
  return PR_FALSE;
}

void
CSSParserImpl::ClearTempData(nsCSSProperty aPropID)
{
  if (nsCSSProps::IsShorthand(aPropID)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID) {
      mTempData.ClearProperty(*p);
    }
  } else {
    mTempData.ClearProperty(aPropID);
  }
  mTempData.AssertInitialState();
}

void
CSSParserImpl::TransferTempData(nsCSSDeclaration* aDeclaration,
                                nsCSSProperty aPropID, PRBool aIsImportant,
                                PRBool aMustCallValueAppended,
                                PRBool* aChanged)
{
  if (nsCSSProps::IsShorthand(aPropID)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID) {
      DoTransferTempData(aDeclaration, *p, aIsImportant,
                         aMustCallValueAppended, aChanged);
    }
  } else {
    DoTransferTempData(aDeclaration, aPropID, aIsImportant,
                       aMustCallValueAppended, aChanged);
  }
  mTempData.AssertInitialState();
}

// Perhaps the transferring code should be in nsCSSExpandedDataBlock, in
// case some other caller wants to use it in the future (although I
// can't think of why).
void
CSSParserImpl::DoTransferTempData(nsCSSDeclaration* aDeclaration,
                                  nsCSSProperty aPropID, PRBool aIsImportant,
                                  PRBool aMustCallValueAppended,
                                  PRBool* aChanged)
{
  NS_ASSERTION(mTempData.HasPropertyBit(aPropID), "oops");
  if (aIsImportant) {
    if (!mData.HasImportantBit(aPropID))
      *aChanged = PR_TRUE;
    mData.SetImportantBit(aPropID);
  } else {
    if (mData.HasImportantBit(aPropID)) {
      mTempData.ClearProperty(aPropID);
      return;
    }
  }

  if (aMustCallValueAppended || !mData.HasPropertyBit(aPropID)) {
    aDeclaration->ValueAppended(aPropID);
  }

  mData.SetPropertyBit(aPropID);
  mTempData.ClearPropertyBit(aPropID);

  /*
   * Save needless copying and allocation by calling the destructor in
   * the destination, copying memory directly, and then using placement
   * new.
   */
  void *v_source = mTempData.PropertyAt(aPropID);
  void *v_dest = mData.PropertyAt(aPropID);
  switch (nsCSSProps::kTypeTable[aPropID]) {
    case eCSSType_Value: {
      nsCSSValue *source = static_cast<nsCSSValue*>(v_source);
      nsCSSValue *dest = static_cast<nsCSSValue*>(v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSValue();
      memcpy(dest, source, sizeof(nsCSSValue));
      new (source) nsCSSValue();
    } break;

    case eCSSType_Rect: {
      nsCSSRect *source = static_cast<nsCSSRect*>(v_source);
      nsCSSRect *dest = static_cast<nsCSSRect*>(v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSRect();
      memcpy(dest, source, sizeof(nsCSSRect));
      new (source) nsCSSRect();
    } break;

    case eCSSType_ValuePair: {
      nsCSSValuePair *source = static_cast<nsCSSValuePair*>(v_source);
      nsCSSValuePair *dest = static_cast<nsCSSValuePair*>(v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSValuePair();
      memcpy(dest, source, sizeof(nsCSSValuePair));
      new (source) nsCSSValuePair();
    } break;

    case eCSSType_ValueList: {
      nsCSSValueList **source = static_cast<nsCSSValueList**>(v_source);
      nsCSSValueList **dest = static_cast<nsCSSValueList**>(v_dest);
      if (!nsCSSValueList::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;

    case eCSSType_ValuePairList: {
      nsCSSValuePairList **source =
        static_cast<nsCSSValuePairList**>(v_source);
      nsCSSValuePairList **dest =
        static_cast<nsCSSValuePairList**>(v_dest);
      if (!nsCSSValuePairList::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;
  }
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

PRBool
CSSParserImpl::ParseEnum(nsCSSValue& aValue,
                         const PRInt32 aKeywordTable[])
{
  nsSubstring* ident = NextIdent();
  if (nsnull == ident) {
    return PR_FALSE;
  }
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(*ident);
  if (eCSSKeyword_UNKNOWN < keyword) {
    PRInt32 value;
    if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
      aValue.SetIntValue(value, eCSSUnit_Enumerated);
      return PR_TRUE;
    }
  }

  // Put the unknown identifier back and return
  UngetToken();
  return PR_FALSE;
}


struct UnitInfo {
  char name[5];  // needs to be long enough for the longest unit, with
                 // terminating null.
  PRUint32 length;
  nsCSSUnit unit;
  PRInt32 type;
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
  { STR_WITH_LEN("mm"), eCSSUnit_Millimeter, VARIANT_LENGTH },
  { STR_WITH_LEN("pc"), eCSSUnit_Pica, VARIANT_LENGTH },
  { STR_WITH_LEN("deg"), eCSSUnit_Degree, VARIANT_ANGLE },
  { STR_WITH_LEN("grad"), eCSSUnit_Grad, VARIANT_ANGLE },
  { STR_WITH_LEN("rad"), eCSSUnit_Radian, VARIANT_ANGLE },
  { STR_WITH_LEN("hz"), eCSSUnit_Hertz, VARIANT_FREQUENCY },
  { STR_WITH_LEN("khz"), eCSSUnit_Kilohertz, VARIANT_FREQUENCY },
  { STR_WITH_LEN("s"), eCSSUnit_Seconds, VARIANT_TIME },
  { STR_WITH_LEN("ms"), eCSSUnit_Milliseconds, VARIANT_TIME }
};

#undef STR_WITH_LEN

PRBool
CSSParserImpl::TranslateDimension(nsCSSValue& aValue,
                                  PRInt32 aVariantMask,
                                  float aNumber,
                                  const nsString& aUnit)
{
  nsCSSUnit units;
  PRInt32   type = 0;
  if (!aUnit.IsEmpty()) {
    PRUint32 i;
    for (i = 0; i < NS_ARRAY_LENGTH(UnitData); ++i) {
      if (aUnit.LowerCaseEqualsASCII(UnitData[i].name,
                                     UnitData[i].length)) {
        units = UnitData[i].unit;
        type = UnitData[i].type;
        break;
      }
    }

    if (i == NS_ARRAY_LENGTH(UnitData)) {
      // Unknown unit
      return PR_FALSE;
    }
  } else {
    // Must be a zero number...
    NS_ASSERTION(0 == aNumber, "numbers without units must be 0");
    if ((VARIANT_LENGTH & aVariantMask) != 0) {
      units = eCSSUnit_Point;
      type = VARIANT_LENGTH;
    }
    else if ((VARIANT_ANGLE & aVariantMask) != 0) {
      units = eCSSUnit_Degree;
      type = VARIANT_ANGLE;
    }
    else if ((VARIANT_FREQUENCY & aVariantMask) != 0) {
      units = eCSSUnit_Hertz;
      type = VARIANT_FREQUENCY;
    }
    else if ((VARIANT_TIME & aVariantMask) != 0) {
      units = eCSSUnit_Seconds;
      type = VARIANT_TIME;
    }
    else {
      NS_ERROR("Variant mask does not include dimension; why were we called?");
      return PR_FALSE;
    }
  }
  if ((type & aVariantMask) != 0) {
    aValue.SetFloatValue(aNumber, units);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParsePositiveVariant(nsCSSValue& aValue,
                                    PRInt32 aVariantMask,
                                    const PRInt32 aKeywordTable[])
{
  if (ParseVariant(aValue, aVariantMask, aKeywordTable)) {
    if (eCSSUnit_Number == aValue.GetUnit() ||
        aValue.IsLengthUnit()){
      if (aValue.GetFloatValue() < 0) {
        UngetToken();
        return PR_FALSE;
      }
    }
    else if (aValue.GetUnit() == eCSSUnit_Percent) {
      if (aValue.GetPercentValue() < 0) {
        UngetToken();
        return PR_FALSE;
      }
    } else if (aValue.GetUnit() == eCSSUnit_Integer) {
      if (aValue.GetIntValue() < 0) {
        UngetToken();
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

// Assigns to aValue iff it returns PR_TRUE.
PRBool
CSSParserImpl::ParseVariant(nsCSSValue& aValue,
                            PRInt32 aVariantMask,
                            const PRInt32 aKeywordTable[])
{
  NS_ASSERTION(IsParsingCompoundProperty() ||
               ((~aVariantMask) & (VARIANT_LENGTH|VARIANT_COLOR)),
               "cannot distinguish lengths and colors in quirks mode");

  if (!GetToken(PR_TRUE)) {
    return PR_FALSE;
  }
  nsCSSToken* tk = &mToken;
  if (((aVariantMask & (VARIANT_AHK | VARIANT_NORMAL | VARIANT_NONE)) != 0) &&
      (eCSSToken_Ident == tk->mType)) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
    if (eCSSKeyword_UNKNOWN < keyword) { // known keyword
      if ((aVariantMask & VARIANT_AUTO) != 0) {
        if (eCSSKeyword_auto == keyword) {
          aValue.SetAutoValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_INHERIT) != 0) {
        // XXX Should we check IsParsingCompoundProperty, or do all
        // callers handle it?  (Not all callers set it, though, since
        // they want the quirks that are disabled by setting it.)
        if (eCSSKeyword_inherit == keyword) {
          aValue.SetInheritValue();
          return PR_TRUE;
        }
        else if (eCSSKeyword__moz_initial == keyword) { // anything that can inherit can also take an initial val.
          aValue.SetInitialValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_NONE) != 0) {
        if (eCSSKeyword_none == keyword) {
          aValue.SetNoneValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_NORMAL) != 0) {
        if (eCSSKeyword_normal == keyword) {
          aValue.SetNormalValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_SYSFONT) != 0) {
        if (eCSSKeyword__moz_use_system_font == keyword &&
            !IsParsingCompoundProperty()) {
          aValue.SetSystemFontValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_KEYWORD) != 0) {
        PRInt32 value;
        if (nsCSSProps::FindKeyword(keyword, aKeywordTable, value)) {
          aValue.SetIntValue(value, eCSSUnit_Enumerated);
          return PR_TRUE;
        }
      }
    }
  }
  if (((aVariantMask & (VARIANT_LENGTH | VARIANT_ANGLE | VARIANT_FREQUENCY | VARIANT_TIME)) != 0) &&
      tk->IsDimension()) {
    if (TranslateDimension(aValue, aVariantMask, tk->mNumber, tk->mIdent)) {
      return PR_TRUE;
    }
    // Put the token back; we didn't parse it, so we shouldn't consume it
    UngetToken();
    return PR_FALSE;
  }
  if (((aVariantMask & VARIANT_PERCENT) != 0) &&
      (eCSSToken_Percentage == tk->mType)) {
    aValue.SetPercentValue(tk->mNumber);
    return PR_TRUE;
  }
  if (((aVariantMask & VARIANT_NUMBER) != 0) &&
      (eCSSToken_Number == tk->mType)) {
    aValue.SetFloatValue(tk->mNumber, eCSSUnit_Number);
    return PR_TRUE;
  }
  if (((aVariantMask & VARIANT_INTEGER) != 0) &&
      (eCSSToken_Number == tk->mType) && tk->mIntegerValid) {
    aValue.SetIntValue(tk->mInteger, eCSSUnit_Integer);
    return PR_TRUE;
  }
  if (mNavQuirkMode && !IsParsingCompoundProperty()) { // NONSTANDARD: Nav interprets unitless numbers as px
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return PR_TRUE;
    }
  }

#ifdef  MOZ_SVG
  if (IsSVGMode() && !IsParsingCompoundProperty()) {
    // STANDARD: SVG Spec states that lengths and coordinates can be unitless
    // in which case they default to user-units (1 px = 1 user unit)
    if (((aVariantMask & VARIANT_LENGTH) != 0) &&
        (eCSSToken_Number == tk->mType)) {
      aValue.SetFloatValue(tk->mNumber, eCSSUnit_Pixel);
      return PR_TRUE;
    }
  }
#endif

  if (((aVariantMask & VARIANT_URL) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      tk->mIdent.LowerCaseEqualsLiteral("url")) {
    if (ParseURL(aValue)) {
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  if ((aVariantMask & VARIANT_COLOR) != 0) {
    if ((mNavQuirkMode && !IsParsingCompoundProperty()) || // NONSTANDARD: Nav interprets 'xxyyzz' values even without '#' prefix
        (eCSSToken_ID == tk->mType) ||
        (eCSSToken_Ref == tk->mType) ||
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
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  }
  if (((aVariantMask & VARIANT_STRING) != 0) &&
      (eCSSToken_String == tk->mType)) {
    nsAutoString  buffer;
    buffer.Append(tk->mSymbol);
    buffer.Append(tk->mIdent);
    buffer.Append(tk->mSymbol);
    aValue.SetStringValue(buffer, eCSSUnit_String);
    return PR_TRUE;
  }
  if (((aVariantMask & VARIANT_IDENTIFIER) != 0) &&
      (eCSSToken_Ident == tk->mType)) {
    aValue.SetStringValue(tk->mIdent, eCSSUnit_String);
    return PR_TRUE;
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
    return ParseAttr(aValue);
  }

  UngetToken();
  return PR_FALSE;
}


PRBool
CSSParserImpl::ParseCounter(nsCSSValue& aValue)
{
  nsCSSUnit unit = (mToken.mIdent.LowerCaseEqualsLiteral("counter") ?
                    eCSSUnit_Counter : eCSSUnit_Counters);

  if (!ExpectSymbol('(', PR_FALSE))
    return PR_FALSE;

  if (!GetNonCloseParenToken(PR_TRUE) ||
      eCSSToken_Ident != mToken.mType) {
    SkipUntil(')');
    return PR_FALSE;
  }

  nsRefPtr<nsCSSValue::Array> val =
    nsCSSValue::Array::Create(unit == eCSSUnit_Counter ? 2 : 3);
  if (!val) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  val->Item(0).SetStringValue(mToken.mIdent, eCSSUnit_String);

  if (eCSSUnit_Counters == unit) {
    // get mandatory separator string
    if (!ExpectSymbol(',', PR_TRUE) ||
        !(GetNonCloseParenToken(PR_TRUE) &&
          eCSSToken_String == mToken.mType)) {
      SkipUntil(')');
      return PR_FALSE;
    }
    val->Item(1).SetStringValue(mToken.mIdent, eCSSUnit_String);
  }

  // get optional type
  PRInt32 type = NS_STYLE_LIST_STYLE_DECIMAL;
  if (ExpectSymbol(',', PR_TRUE)) {
    nsCSSKeyword keyword;
    PRBool success = GetNonCloseParenToken(PR_TRUE) &&
                     eCSSToken_Ident == mToken.mType &&
                     (keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent)) !=
                      eCSSKeyword_UNKNOWN;
    if (success) {
      if (keyword == eCSSKeyword_none) {
        type = NS_STYLE_LIST_STYLE_NONE;
      } else {
        success = nsCSSProps::FindKeyword(keyword,
                                          nsCSSProps::kListStyleKTable, type);
      }
    }
    if (!success) {
      SkipUntil(')');
      return PR_FALSE;
    }
  }
  PRInt32 typeItem = eCSSUnit_Counters == unit ? 2 : 1;
  val->Item(typeItem).SetIntValue(type, eCSSUnit_Enumerated);

  if (!ExpectSymbol(')', PR_TRUE)) {
    SkipUntil(')');
    return PR_FALSE;
  }

  aValue.SetArrayValue(val, unit);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseAttr(nsCSSValue& aValue)
{
  if (ExpectSymbol('(', PR_FALSE)) {
    if (GetToken(PR_TRUE)) {
      nsAutoString attr;
      if (eCSSToken_Ident == mToken.mType) {  // attr name or namespace
        nsAutoString  holdIdent(mToken.mIdent);
        if (ExpectSymbol('|', PR_FALSE)) {  // namespace
          PRInt32 nameSpaceID;
          if (!GetNamespaceIdForPrefix(holdIdent, &nameSpaceID)) {
            return PR_FALSE;
          }
          attr.AppendInt(nameSpaceID, 10);
          attr.Append(PRUnichar('|'));
          if (! GetToken(PR_FALSE)) {
            REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
            return PR_FALSE;
          }
          if (eCSSToken_Ident == mToken.mType) {
            if (mCaseSensitive) {
              attr.Append(mToken.mIdent);
            } else {
              nsAutoString buffer;
              ToLowerCase(mToken.mIdent, buffer);
              attr.Append(buffer);
            }
          }
          else {
            REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
            UngetToken();
            return PR_FALSE;
          }
        }
        else {  // no namespace
          if (mCaseSensitive) {
            attr = holdIdent;
          }
          else {
            ToLowerCase(holdIdent, attr);
          }
        }
      }
      else if (mToken.IsSymbol('*')) {  // namespace wildcard
        // Wildcard namespace makes no sense here and is not allowed
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        return PR_FALSE;
      }
      else if (mToken.IsSymbol('|')) {  // explicit NO namespace
        if (! GetToken(PR_FALSE)) {
          REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
          return PR_FALSE;
        }
        if (eCSSToken_Ident == mToken.mType) {
          if (mCaseSensitive) {
            attr.Append(mToken.mIdent);
          } else {
            nsAutoString buffer;
            ToLowerCase(mToken.mIdent, buffer);
            attr.Append(buffer);
          }
        }
        else {
          REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
          UngetToken();
          return PR_FALSE;
        }
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameOrNamespaceExpected);
        UngetToken();
        return PR_FALSE;
      }
      if (ExpectSymbol(')', PR_TRUE)) {
        aValue.SetStringValue(attr, eCSSUnit_Attr);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseURL(nsCSSValue& aValue)
{
  if (!mSheetPrincipal) {
    NS_NOTREACHED("Codepaths that expect to parse URLs MUST pass in an "
                  "origin principal");
    return PR_FALSE;
  }

  if (!ExpectSymbol('(', PR_FALSE))
    return PR_FALSE;
  if (!GetURLToken())
    return PR_FALSE;

  nsCSSToken* tk = &mToken;
  if (eCSSToken_String != tk->mType && eCSSToken_URL != tk->mType)
    return PR_FALSE;

  nsString url = tk->mIdent;
  if (!ExpectSymbol(')', PR_TRUE))
    return PR_FALSE;

  // Translate url into an absolute url if the url is relative to the
  // style sheet.
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), url, nsnull, mBaseURL);

  nsStringBuffer* buffer = nsCSSValue::BufferFromString(url);
  if (NS_UNLIKELY(!buffer)) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }
  nsCSSValue::URL *urlVal =
    new nsCSSValue::URL(uri, buffer, mSheetURL, mSheetPrincipal);

  buffer->Release();
  if (NS_UNLIKELY(!urlVal)) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }
  aValue.SetURLValue(urlVal);
  return PR_TRUE;
}

PRInt32
CSSParserImpl::ParseChoice(nsCSSValue aValues[],
                           const nsCSSProperty aPropIDs[], PRInt32 aNumIDs)
{
  PRInt32 found = 0;
  nsAutoParseCompoundProperty compound(this);

  PRInt32 loop;
  for (loop = 0; loop < aNumIDs; loop++) {
    // Try each property parser in order
    PRInt32 hadFound = found;
    PRInt32 index;
    for (index = 0; index < aNumIDs; index++) {
      PRInt32 bit = 1 << index;
      if ((found & bit) == 0) {
        if (ParseSingleValueProperty(aValues[index], aPropIDs[index])) {
          found |= bit;
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
  NS_ASSERTION(0 <= aPropID && aPropID < eCSSProperty_COUNT_no_shorthands,
               "property out of range");
  NS_ASSERTION(nsCSSProps::kTypeTable[aPropID] == eCSSType_Value,
               nsPrintfCString(64, "type error (property=\'%s\')",
                             nsCSSProps::GetStringValue(aPropID).get()).get());
  nsCSSValue& storage =
      *static_cast<nsCSSValue*>(mTempData.PropertyAt(aPropID));
  storage = aValue;
  mTempData.SetPropertyBit(aPropID);
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values.
 */
PRBool
CSSParserImpl::ParseBoxProperties(nsCSSRect& aResult,
                                  const nsCSSProperty aPropIDs[])
{
  // Get up to four values for the property
  PRInt32 count = 0;
  nsCSSRect result;
  NS_FOR_CSS_SIDES (index) {
    if (! ParseSingleValueProperty(result.*(nsCSSRect::sides[index]),
                                   aPropIDs[index])) {
      break;
    }
    count++;
  }
  if ((count == 0) || (PR_FALSE == ExpectEndProperty())) {
    return PR_FALSE;
  }

  if (1 < count) { // verify no more than single inherit or initial
    NS_FOR_CSS_SIDES (index) {
      nsCSSUnit unit = (result.*(nsCSSRect::sides[index])).GetUnit();
      if (eCSSUnit_Inherit == unit || eCSSUnit_Initial == unit) {
        return PR_FALSE;
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
    mTempData.SetPropertyBit(aPropIDs[index]);
  }
  aResult = result;
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseDirectionalBoxProperty(nsCSSProperty aProperty,
                                           PRInt32 aSourceType)
{
  const nsCSSProperty* subprops = nsCSSProps::SubpropertyEntryFor(aProperty);
  NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
               "not box property with physical vs. logical cascading");
  nsCSSValue value;
  if (!ParseSingleValueProperty(value, subprops[0]) ||
      !ExpectEndProperty())
    return PR_FALSE;

  AppendValue(subprops[0], value);
  nsCSSValue typeVal(aSourceType, eCSSUnit_Enumerated);
  AppendValue(subprops[1], typeVal);
  AppendValue(subprops[2], typeVal);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseBoxCornerRadius(nsCSSProperty aPropID)
{
  nsCSSValue dimenX, dimenY;
  // required first value
  if (! ParsePositiveVariant(dimenX, VARIANT_HLP, nsnull))
    return PR_FALSE;
  // optional second value (forbidden if first value is inherit/initial)
  if (dimenX.GetUnit() == eCSSUnit_Inherit ||
      dimenX.GetUnit() == eCSSUnit_Initial ||
      ! ParsePositiveVariant(dimenY, VARIANT_LP, nsnull))
    dimenY = dimenX;

  NS_ASSERTION(nsCSSProps::kTypeTable[aPropID] == eCSSType_ValuePair,
               nsPrintfCString(64, "type error (property='%s')",
                               nsCSSProps::GetStringValue(aPropID).get())
               .get());
  nsCSSValuePair& storage =
    *static_cast<nsCSSValuePair*>(mTempData.PropertyAt(aPropID));
  storage.mXValue = dimenX;
  storage.mYValue = dimenY;
  mTempData.SetPropertyBit(aPropID);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseBoxCornerRadii(nsCSSCornerSizes& aRadii,
                                   const nsCSSProperty aPropIDs[])
{
  // Rectangles are used as scratch storage.
  // top => top-left, right => top-right,
  // bottom => bottom-right, left => bottom-left.
  nsCSSRect dimenX, dimenY;
  PRInt32 countX = 0, countY = 0;

  NS_FOR_CSS_SIDES (side) {
    if (! ParsePositiveVariant(dimenX.*nsCSSRect::sides[side],
                               side > 0 ? VARIANT_LP : VARIANT_HLP, nsnull))
      break;
    countX++;
  }
  if (countX == 0)
    return PR_FALSE;

  if (ExpectSymbol('/', PR_TRUE)) {
    NS_FOR_CSS_SIDES (side) {
      if (! ParsePositiveVariant(dimenY.*nsCSSRect::sides[side],
                                 VARIANT_LP, nsnull))
        break;
      countY++;
    }
    if (countY == 0)
      return PR_FALSE;
  }
  if (!ExpectEndProperty())
    return PR_FALSE;

  // if 'initial' or 'inherit' was used, it must be the only value
  if (countX > 1 || countY > 0) {
    nsCSSUnit unit = dimenX.mTop.GetUnit();
    if (eCSSUnit_Inherit == unit || eCSSUnit_Initial == unit)
      return PR_FALSE;
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
    nsCSSValuePair& corner =
      aRadii.GetFullCorner(NS_SIDE_TO_FULL_CORNER(side, PR_FALSE));
    corner.mXValue = dimenX.*nsCSSRect::sides[side];
    corner.mYValue = dimenY.*nsCSSRect::sides[side];
    mTempData.SetPropertyBit(aPropIDs[side]);
  }
  return PR_TRUE;
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
  eCSSProperty__moz_border_radius_topLeft,
  eCSSProperty__moz_border_radius_topRight,
  eCSSProperty__moz_border_radius_bottomRight,
  eCSSProperty__moz_border_radius_bottomLeft
};
static const nsCSSProperty kOutlineRadiusIDs[] = {
  eCSSProperty__moz_outline_radius_topLeft,
  eCSSProperty__moz_outline_radius_topRight,
  eCSSProperty__moz_outline_radius_bottomRight,
  eCSSProperty__moz_outline_radius_bottomLeft
};

PRBool
CSSParserImpl::ParseProperty(nsCSSProperty aPropID)
{
  NS_ASSERTION(aPropID < eCSSProperty_COUNT, "index out of range");

  switch (aPropID) {  // handle shorthand or multiple properties
  case eCSSProperty_background:
    return ParseBackground();
  case eCSSProperty_background_position:
    return ParseBackgroundPosition();
  case eCSSProperty_border:
    return ParseBorderSide(kBorderTopIDs, PR_TRUE);
  case eCSSProperty_border_color:
    return ParseBorderColor();
  case eCSSProperty_border_spacing:
    return ParseBorderSpacing();
  case eCSSProperty_border_style:
    return ParseBorderStyle();
  case eCSSProperty_border_bottom:
    return ParseBorderSide(kBorderBottomIDs, PR_FALSE);
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
    return ParseBorderSide(kBorderTopIDs, PR_FALSE);
  case eCSSProperty_border_bottom_colors:
    return ParseBorderColors(&mTempData.mMargin.mBorderColors.mBottom,
                             aPropID);
  case eCSSProperty_border_left_colors:
    return ParseBorderColors(&mTempData.mMargin.mBorderColors.mLeft,
                             aPropID);
  case eCSSProperty_border_right_colors:
    return ParseBorderColors(&mTempData.mMargin.mBorderColors.mRight,
                             aPropID);
  case eCSSProperty_border_top_colors:
    return ParseBorderColors(&mTempData.mMargin.mBorderColors.mTop,
                             aPropID);
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
  case eCSSProperty__moz_border_radius:
    return ParseBoxCornerRadii(mTempData.mMargin.mBorderRadius,
                               kBorderRadiusIDs);
  case eCSSProperty__moz_outline_radius:
    return ParseBoxCornerRadii(mTempData.mMargin.mOutlineRadius,
                               kOutlineRadiusIDs);

  case eCSSProperty__moz_border_radius_topLeft:
  case eCSSProperty__moz_border_radius_topRight:
  case eCSSProperty__moz_border_radius_bottomRight:
  case eCSSProperty__moz_border_radius_bottomLeft:
  case eCSSProperty__moz_outline_radius_topLeft:
  case eCSSProperty__moz_outline_radius_topRight:
  case eCSSProperty__moz_outline_radius_bottomRight:
  case eCSSProperty__moz_outline_radius_bottomLeft:
    return ParseBoxCornerRadius(aPropID);

  case eCSSProperty_box_shadow:
    return ParseBoxShadow();
  case eCSSProperty_clip:
    return ParseRect(mTempData.mDisplay.mClip, eCSSProperty_clip);
  case eCSSProperty__moz_column_rule:
    return ParseBorderSide(kColumnRuleIDs, PR_FALSE);
  case eCSSProperty_content:
    return ParseContent();
  case eCSSProperty_counter_increment:
    return ParseCounterData(&mTempData.mContent.mCounterIncrement,
                            aPropID);
  case eCSSProperty_counter_reset:
    return ParseCounterData(&mTempData.mContent.mCounterReset,
                            aPropID);
  case eCSSProperty_cue:
    return ParseCue();
  case eCSSProperty_cursor:
    return ParseCursor();
  case eCSSProperty_font:
    return ParseFont();
  case eCSSProperty_image_region:
    return ParseRect(mTempData.mList.mImageRegion,
                     eCSSProperty_image_region);
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
  case eCSSProperty_pause:
    return ParsePause();
  case eCSSProperty_quotes:
    return ParseQuotes();
  case eCSSProperty_size:
    return ParseSize();
  case eCSSProperty_text_shadow:
    return ParseTextShadow();
  case eCSSProperty__moz_transform:
    return ParseMozTransform();
  case eCSSProperty__moz_transform_origin:
    return ParseMozTransformOrigin();

#ifdef MOZ_SVG
  case eCSSProperty_fill:
    return ParsePaint(&mTempData.mSVG.mFill, eCSSProperty_fill);
  case eCSSProperty_stroke:
    return ParsePaint(&mTempData.mSVG.mStroke, eCSSProperty_stroke);
  case eCSSProperty_stroke_dasharray:
    return ParseDasharray();
  case eCSSProperty_marker:
    return ParseMarker();
#endif

  // Strip out properties we use internally.
  case eCSSProperty__x_system_font:
  case eCSSProperty_margin_end_value:
  case eCSSProperty_margin_left_value:
  case eCSSProperty_margin_right_value:
  case eCSSProperty_margin_start_value:
  case eCSSProperty_margin_left_ltr_source:
  case eCSSProperty_margin_left_rtl_source:
  case eCSSProperty_margin_right_ltr_source:
  case eCSSProperty_margin_right_rtl_source:
  case eCSSProperty_padding_end_value:
  case eCSSProperty_padding_left_value:
  case eCSSProperty_padding_right_value:
  case eCSSProperty_padding_start_value:
  case eCSSProperty_padding_left_ltr_source:
  case eCSSProperty_padding_left_rtl_source:
  case eCSSProperty_padding_right_ltr_source:
  case eCSSProperty_padding_right_rtl_source:
  case eCSSProperty_border_end_color_value:
  case eCSSProperty_border_left_color_value:
  case eCSSProperty_border_right_color_value:
  case eCSSProperty_border_start_color_value:
  case eCSSProperty_border_left_color_ltr_source:
  case eCSSProperty_border_left_color_rtl_source:
  case eCSSProperty_border_right_color_ltr_source:
  case eCSSProperty_border_right_color_rtl_source:
  case eCSSProperty_border_end_style_value:
  case eCSSProperty_border_left_style_value:
  case eCSSProperty_border_right_style_value:
  case eCSSProperty_border_start_style_value:
  case eCSSProperty_border_left_style_ltr_source:
  case eCSSProperty_border_left_style_rtl_source:
  case eCSSProperty_border_right_style_ltr_source:
  case eCSSProperty_border_right_style_rtl_source:
  case eCSSProperty_border_end_width_value:
  case eCSSProperty_border_left_width_value:
  case eCSSProperty_border_right_width_value:
  case eCSSProperty_border_start_width_value:
  case eCSSProperty_border_left_width_ltr_source:
  case eCSSProperty_border_left_width_rtl_source:
  case eCSSProperty_border_right_width_ltr_source:
  case eCSSProperty_border_right_width_rtl_source:
    // The user can't use these
    REPORT_UNEXPECTED(PEInaccessibleProperty2);
    return PR_FALSE;
  default:  // must be single property
    {
      nsCSSValue value;
      if (ParseSingleValueProperty(value, aPropID)) {
        if (ExpectEndProperty()) {
          AppendValue(aPropID, value);
          return PR_TRUE;
        }
        // XXX Report errors?
      }
      // XXX Report errors?
    }
  }
  return PR_FALSE;
}

// Bits used in determining which background position info we have
#define BG_CENTER  NS_STYLE_BG_POSITION_CENTER
#define BG_TOP     NS_STYLE_BG_POSITION_TOP
#define BG_BOTTOM  NS_STYLE_BG_POSITION_BOTTOM
#define BG_LEFT    NS_STYLE_BG_POSITION_LEFT
#define BG_RIGHT   NS_STYLE_BG_POSITION_RIGHT
#define BG_CTB    (BG_CENTER | BG_TOP | BG_BOTTOM)
#define BG_CLR    (BG_CENTER | BG_LEFT | BG_RIGHT)

PRBool
CSSParserImpl::ParseSingleValueProperty(nsCSSValue& aValue,
                                        nsCSSProperty aPropID)
{
  switch (aPropID) {
  case eCSSProperty_UNKNOWN:
  case eCSSProperty_background:
  case eCSSProperty_background_position:
  case eCSSProperty_border:
  case eCSSProperty_border_color:
  case eCSSProperty_border_bottom_colors:
  case eCSSProperty_border_image:
  case eCSSProperty_border_left_colors:
  case eCSSProperty_border_right_colors:
  case eCSSProperty_border_end_color:
  case eCSSProperty_border_left_color:
  case eCSSProperty_border_right_color:
  case eCSSProperty_border_start_color:
  case eCSSProperty_border_end_style:
  case eCSSProperty_border_left_style:
  case eCSSProperty_border_right_style:
  case eCSSProperty_border_start_style:
  case eCSSProperty_border_end_width:
  case eCSSProperty_border_left_width:
  case eCSSProperty_border_right_width:
  case eCSSProperty_border_start_width:
  case eCSSProperty_border_top_colors:
  case eCSSProperty_border_spacing:
  case eCSSProperty_border_style:
  case eCSSProperty_border_bottom:
  case eCSSProperty_border_end:
  case eCSSProperty_border_left:
  case eCSSProperty_border_right:
  case eCSSProperty_border_start:
  case eCSSProperty_border_top:
  case eCSSProperty_border_width:
  case eCSSProperty__moz_border_radius:
  case eCSSProperty__moz_border_radius_topLeft:
  case eCSSProperty__moz_border_radius_topRight:
  case eCSSProperty__moz_border_radius_bottomRight:
  case eCSSProperty__moz_border_radius_bottomLeft:
  case eCSSProperty_box_shadow:
  case eCSSProperty_clip:
  case eCSSProperty__moz_column_rule:
  case eCSSProperty_content:
  case eCSSProperty_counter_increment:
  case eCSSProperty_counter_reset:
  case eCSSProperty_cue:
  case eCSSProperty_cursor:
  case eCSSProperty_font:
  case eCSSProperty_image_region:
  case eCSSProperty_list_style:
  case eCSSProperty_margin:
  case eCSSProperty_margin_end:
  case eCSSProperty_margin_left:
  case eCSSProperty_margin_right:
  case eCSSProperty_margin_start:
  case eCSSProperty_outline:
  case eCSSProperty__moz_outline_radius:
  case eCSSProperty__moz_outline_radius_topLeft:
  case eCSSProperty__moz_outline_radius_topRight:
  case eCSSProperty__moz_outline_radius_bottomRight:
  case eCSSProperty__moz_outline_radius_bottomLeft:
  case eCSSProperty_overflow:
  case eCSSProperty_padding:
  case eCSSProperty_padding_end:
  case eCSSProperty_padding_left:
  case eCSSProperty_padding_right:
  case eCSSProperty_padding_start:
  case eCSSProperty_pause:
  case eCSSProperty_quotes:
  case eCSSProperty_size:
  case eCSSProperty_text_shadow:
  case eCSSProperty__moz_transform:
  case eCSSProperty__moz_transform_origin:
  case eCSSProperty_COUNT:
#ifdef MOZ_SVG
  case eCSSProperty_fill:
  case eCSSProperty_stroke:
  case eCSSProperty_stroke_dasharray:
  case eCSSProperty_marker:
#endif
    NS_ERROR("not a single value property");
    return PR_FALSE;

  case eCSSProperty__x_system_font:
  case eCSSProperty_margin_left_ltr_source:
  case eCSSProperty_margin_left_rtl_source:
  case eCSSProperty_margin_right_ltr_source:
  case eCSSProperty_margin_right_rtl_source:
  case eCSSProperty_padding_left_ltr_source:
  case eCSSProperty_padding_left_rtl_source:
  case eCSSProperty_padding_right_ltr_source:
  case eCSSProperty_padding_right_rtl_source:
  case eCSSProperty_border_left_color_ltr_source:
  case eCSSProperty_border_left_color_rtl_source:
  case eCSSProperty_border_right_color_ltr_source:
  case eCSSProperty_border_right_color_rtl_source:
  case eCSSProperty_border_left_style_ltr_source:
  case eCSSProperty_border_left_style_rtl_source:
  case eCSSProperty_border_right_style_ltr_source:
  case eCSSProperty_border_right_style_rtl_source:
  case eCSSProperty_border_left_width_ltr_source:
  case eCSSProperty_border_left_width_rtl_source:
  case eCSSProperty_border_right_width_ltr_source:
  case eCSSProperty_border_right_width_rtl_source:
#ifdef MOZ_MATHML
  case eCSSProperty_script_size_multiplier:
  case eCSSProperty_script_min_size:
#endif
    NS_ERROR("not currently parsed here");
    return PR_FALSE;

  case eCSSProperty_appearance:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kAppearanceKTable);
  case eCSSProperty_azimuth:
    return ParseAzimuth(aValue);
  case eCSSProperty_background_attachment:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundAttachmentKTable);
  case eCSSProperty__moz_background_clip:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundClipKTable);
  case eCSSProperty_background_color:
    return ParseVariant(aValue, VARIANT_HC, nsnull);
  case eCSSProperty_background_image:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty__moz_background_inline_policy:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundInlinePolicyKTable);
  case eCSSProperty__moz_background_origin:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundOriginKTable);
  case eCSSProperty_background_repeat:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundRepeatKTable);
  case eCSSProperty_binding:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_border_collapse:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBorderCollapseKTable);
  case eCSSProperty_border_bottom_color:
  case eCSSProperty_border_end_color_value: // for internal use
  case eCSSProperty_border_left_color_value: // for internal use
  case eCSSProperty_border_right_color_value: // for internal use
  case eCSSProperty_border_start_color_value: // for internal use
  case eCSSProperty_border_top_color:
  case eCSSProperty__moz_column_rule_color:
    return ParseVariant(aValue, VARIANT_HCK,
                        nsCSSProps::kBorderColorKTable);
  case eCSSProperty_border_bottom_style:
  case eCSSProperty_border_end_style_value: // for internal use
  case eCSSProperty_border_left_style_value: // for internal use
  case eCSSProperty_border_right_style_value: // for internal use
  case eCSSProperty_border_start_style_value: // for internal use
  case eCSSProperty_border_top_style:
  case eCSSProperty__moz_column_rule_style:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kBorderStyleKTable);
  case eCSSProperty_border_bottom_width:
  case eCSSProperty_border_end_width_value: // for internal use
  case eCSSProperty_border_left_width_value: // for internal use
  case eCSSProperty_border_right_width_value: // for internal use
  case eCSSProperty_border_start_width_value: // for internal use
  case eCSSProperty_border_top_width:
  case eCSSProperty__moz_column_rule_width:
    return ParsePositiveVariant(aValue, VARIANT_HKL,
                                nsCSSProps::kBorderWidthKTable);
  case eCSSProperty__moz_column_count:
    return ParsePositiveVariant(aValue, VARIANT_AHI, nsnull);
  case eCSSProperty__moz_column_width:
    return ParsePositiveVariant(aValue, VARIANT_AHL, nsnull);
  case eCSSProperty__moz_column_gap:
    return ParsePositiveVariant(aValue, VARIANT_HL | VARIANT_NORMAL, nsnull);
  case eCSSProperty_bottom:
  case eCSSProperty_top:
  case eCSSProperty_left:
  case eCSSProperty_right:
    return ParseVariant(aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_box_align:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBoxAlignKTable);
  case eCSSProperty_box_direction:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBoxDirectionKTable);
  case eCSSProperty_box_flex:
    return ParsePositiveVariant(aValue, VARIANT_HN, nsnull);
  case eCSSProperty_box_orient:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBoxOrientKTable);
  case eCSSProperty_box_pack:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBoxPackKTable);
  case eCSSProperty_box_ordinal_group:
    return ParsePositiveVariant(aValue, VARIANT_HI, nsnull);
#ifdef MOZ_SVG
  case eCSSProperty_clip_path:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_clip_rule:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kFillRuleKTable);
  case eCSSProperty_color_interpolation:
  case eCSSProperty_color_interpolation_filters:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kColorInterpolationKTable);
  case eCSSProperty_dominant_baseline:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kDominantBaselineKTable);
  case eCSSProperty_fill_opacity:
    return ParseVariant(aValue, VARIANT_HN,
                        nsnull);
  case eCSSProperty_fill_rule:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kFillRuleKTable);
  case eCSSProperty_filter:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_flood_color:
    return ParseVariant(aValue, VARIANT_HC, nsnull);
  case eCSSProperty_flood_opacity:
    return ParseVariant(aValue, VARIANT_HN, nsnull);
  case eCSSProperty_lighting_color:
    return ParseVariant(aValue, VARIANT_HC, nsnull);
  case eCSSProperty_marker_end:
  case eCSSProperty_marker_mid:
  case eCSSProperty_marker_start:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_mask:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_pointer_events:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kPointerEventsKTable);
  case eCSSProperty_shape_rendering:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kShapeRenderingKTable);
  case eCSSProperty_stop_color:
    return ParseVariant(aValue, VARIANT_HC,
                        nsnull);
  case eCSSProperty_stop_opacity:
    return ParseVariant(aValue, VARIANT_HN,
                        nsnull);
  case eCSSProperty_stroke_dashoffset:
    return ParseVariant(aValue, VARIANT_HLPN,
                        nsnull);
  case eCSSProperty_stroke_linecap:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kStrokeLinecapKTable);
  case eCSSProperty_stroke_linejoin:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kStrokeLinejoinKTable);
  case eCSSProperty_stroke_miterlimit:
    return ParsePositiveVariant(aValue, VARIANT_HN,
                                nsnull);
  case eCSSProperty_stroke_opacity:
    return ParseVariant(aValue, VARIANT_HN,
                        nsnull);
  case eCSSProperty_stroke_width:
    return ParsePositiveVariant(aValue, VARIANT_HLPN,
                        nsnull);
  case eCSSProperty_text_anchor:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kTextAnchorKTable);
  case eCSSProperty_text_rendering:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kTextRenderingKTable);
#endif
  case eCSSProperty_box_sizing:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kBoxSizingKTable);
  case eCSSProperty_height:
    return ParsePositiveVariant(aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_width:
    return ParsePositiveVariant(aValue, VARIANT_AHKLP,
                                nsCSSProps::kWidthKTable);
  case eCSSProperty_force_broken_image_icon:
    return ParsePositiveVariant(aValue, VARIANT_HI, nsnull);
  case eCSSProperty_caption_side:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kCaptionSideKTable);
  case eCSSProperty_clear:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kClearKTable);
  case eCSSProperty_color:
    return ParseVariant(aValue, VARIANT_HC, nsnull);
  case eCSSProperty_cue_after:
  case eCSSProperty_cue_before:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_direction:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kDirectionKTable);
  case eCSSProperty_display:
    if (ParseVariant(aValue, VARIANT_HOK, nsCSSProps::kDisplayKTable)) {
      if (aValue.GetUnit() == eCSSUnit_Enumerated) {
        switch (aValue.GetIntValue()) {
          case NS_STYLE_DISPLAY_MARKER:        // bug 2055
          case NS_STYLE_DISPLAY_RUN_IN:        // bug 2056
          case NS_STYLE_DISPLAY_COMPACT:       // bug 14983
            return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
    return PR_FALSE;
  case eCSSProperty_elevation:
    return ParseVariant(aValue, VARIANT_HK | VARIANT_ANGLE,
                        nsCSSProps::kElevationKTable);
  case eCSSProperty_empty_cells:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kEmptyCellsKTable);
  case eCSSProperty_float:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kFloatKTable);
  case eCSSProperty_float_edge:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kFloatEdgeKTable);
  case eCSSProperty_font_family:
    return ParseFamily(aValue);
  case eCSSProperty_font_size:
    return ParsePositiveVariant(aValue,
                                VARIANT_HKLP | VARIANT_SYSFONT,
                                nsCSSProps::kFontSizeKTable);
  case eCSSProperty_font_size_adjust:
    return ParseVariant(aValue, VARIANT_HON | VARIANT_SYSFONT,
                        nsnull);
  case eCSSProperty_font_stretch:
    return ParseVariant(aValue, VARIANT_HMK | VARIANT_SYSFONT,
                        nsCSSProps::kFontStretchKTable);
  case eCSSProperty_font_style:
    return ParseVariant(aValue, VARIANT_HMK | VARIANT_SYSFONT,
                        nsCSSProps::kFontStyleKTable);
  case eCSSProperty_font_variant:
    return ParseVariant(aValue, VARIANT_HMK | VARIANT_SYSFONT,
                        nsCSSProps::kFontVariantKTable);
  case eCSSProperty_font_weight:
    return ParseFontWeight(aValue);
  case eCSSProperty_ime_mode:
    return ParseVariant(aValue, VARIANT_AHK | VARIANT_NORMAL,
                        nsCSSProps::kIMEModeKTable);
  case eCSSProperty_letter_spacing:
  case eCSSProperty_word_spacing:
    return ParseVariant(aValue, VARIANT_HL | VARIANT_NORMAL, nsnull);
  case eCSSProperty_line_height:
    return ParsePositiveVariant(aValue, VARIANT_HLPN | VARIANT_NORMAL | VARIANT_SYSFONT, nsnull);
  case eCSSProperty_list_style_image:
    return ParseVariant(aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_list_style_position:
    return ParseVariant(aValue, VARIANT_HK, nsCSSProps::kListStylePositionKTable);
  case eCSSProperty_list_style_type:
    return ParseVariant(aValue, VARIANT_HOK, nsCSSProps::kListStyleKTable);
  case eCSSProperty_margin_bottom:
  case eCSSProperty_margin_end_value: // for internal use
  case eCSSProperty_margin_left_value: // for internal use
  case eCSSProperty_margin_right_value: // for internal use
  case eCSSProperty_margin_start_value: // for internal use
  case eCSSProperty_margin_top:
    return ParseVariant(aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_marker_offset:
    return ParseVariant(aValue, VARIANT_AHL, nsnull);
  case eCSSProperty_marks:
    return ParseMarks(aValue);
  case eCSSProperty_max_height:
    return ParsePositiveVariant(aValue, VARIANT_HLPO, nsnull);
  case eCSSProperty_max_width:
    return ParsePositiveVariant(aValue, VARIANT_HKLPO,
                                nsCSSProps::kWidthKTable);
  case eCSSProperty_min_height:
    return ParsePositiveVariant(aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_min_width:
    return ParsePositiveVariant(aValue, VARIANT_HKLP,
                                nsCSSProps::kWidthKTable);
  case eCSSProperty_opacity:
    return ParseVariant(aValue, VARIANT_HN, nsnull);
  case eCSSProperty_orphans:
  case eCSSProperty_widows:
    return ParseVariant(aValue, VARIANT_HI, nsnull);
  case eCSSProperty_outline_color:
    return ParseVariant(aValue, VARIANT_HCK,
                        nsCSSProps::kOutlineColorKTable);
  case eCSSProperty_outline_style:
    return ParseVariant(aValue, VARIANT_HOK | VARIANT_AUTO,
                        nsCSSProps::kOutlineStyleKTable);
  case eCSSProperty_outline_width:
    return ParsePositiveVariant(aValue, VARIANT_HKL,
                                nsCSSProps::kBorderWidthKTable);
  case eCSSProperty_outline_offset:
    return ParseVariant(aValue, VARIANT_HL, nsnull);
  case eCSSProperty_overflow_x:
  case eCSSProperty_overflow_y:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kOverflowSubKTable);
  case eCSSProperty_padding_bottom:
  case eCSSProperty_padding_end_value: // for internal use
  case eCSSProperty_padding_left_value: // for internal use
  case eCSSProperty_padding_right_value: // for internal use
  case eCSSProperty_padding_start_value: // for internal use
  case eCSSProperty_padding_top:
    return ParsePositiveVariant(aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_page:
    return ParseVariant(aValue, VARIANT_AUTO | VARIANT_IDENTIFIER, nsnull);
  case eCSSProperty_page_break_after:
  case eCSSProperty_page_break_before:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kPageBreakKTable);
  case eCSSProperty_page_break_inside:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kPageBreakInsideKTable);
  case eCSSProperty_pause_after:
  case eCSSProperty_pause_before:
    return ParseVariant(aValue, VARIANT_HTP, nsnull);
  case eCSSProperty_pitch:
    return ParseVariant(aValue, VARIANT_HKF, nsCSSProps::kPitchKTable);
  case eCSSProperty_pitch_range:
    return ParseVariant(aValue, VARIANT_HN, nsnull);
  case eCSSProperty_position:
    return ParseVariant(aValue, VARIANT_HK, nsCSSProps::kPositionKTable);
  case eCSSProperty_richness:
    return ParseVariant(aValue, VARIANT_HN, nsnull);
#ifdef MOZ_MATHML
  // script-level can take Integer or Number values, but only Integer ("relative")
  // values can be specified in a style sheet. Also we only allow this property
  // when unsafe rules are enabled, because otherwise it could interfere
  // with rulenode optimizations if used in a non-MathML-enabled document.
  case eCSSProperty_script_level:
    if (!mUnsafeRulesEnabled)
      return PR_FALSE;
    return ParseVariant(aValue, VARIANT_HI, nsnull);
#endif
  case eCSSProperty_speak:
    return ParseVariant(aValue, VARIANT_HMK | VARIANT_NONE,
                        nsCSSProps::kSpeakKTable);
  case eCSSProperty_speak_header:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kSpeakHeaderKTable);
  case eCSSProperty_speak_numeral:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kSpeakNumeralKTable);
  case eCSSProperty_speak_punctuation:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kSpeakPunctuationKTable);
  case eCSSProperty_speech_rate:
    return ParseVariant(aValue, VARIANT_HN | VARIANT_KEYWORD,
                        nsCSSProps::kSpeechRateKTable);
  case eCSSProperty_stack_sizing:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kStackSizingKTable);
  case eCSSProperty_stress:
    return ParseVariant(aValue, VARIANT_HN, nsnull);
  case eCSSProperty_table_layout:
    return ParseVariant(aValue, VARIANT_AHK,
                        nsCSSProps::kTableLayoutKTable);
  case eCSSProperty_text_align:
    // When we support aligning on a string, we can parse text-align
    // as a string....
    return ParseVariant(aValue, VARIANT_HK /* | VARIANT_STRING */,
                        nsCSSProps::kTextAlignKTable);
  case eCSSProperty_text_decoration:
    return ParseTextDecoration(aValue);
  case eCSSProperty_text_indent:
    return ParseVariant(aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_text_transform:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kTextTransformKTable);
  case eCSSProperty_unicode_bidi:
    return ParseVariant(aValue, VARIANT_HMK,
                        nsCSSProps::kUnicodeBidiKTable);
  case eCSSProperty_user_focus:
    return ParseVariant(aValue, VARIANT_HMK | VARIANT_NONE,
                        nsCSSProps::kUserFocusKTable);
  case eCSSProperty_user_input:
    return ParseVariant(aValue, VARIANT_AHK | VARIANT_NONE,
                        nsCSSProps::kUserInputKTable);
  case eCSSProperty_user_modify:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kUserModifyKTable);
  case eCSSProperty_user_select:
    return ParseVariant(aValue, VARIANT_AHK | VARIANT_NONE,
                        nsCSSProps::kUserSelectKTable);
  case eCSSProperty_vertical_align:
    return ParseVariant(aValue, VARIANT_HKLP,
                        nsCSSProps::kVerticalAlignKTable);
  case eCSSProperty_visibility:
    return ParseVariant(aValue, VARIANT_HK,
                        nsCSSProps::kVisibilityKTable);
  case eCSSProperty_voice_family:
    return ParseFamily(aValue);
  case eCSSProperty_volume:
    return ParseVariant(aValue, VARIANT_HPN | VARIANT_KEYWORD,
                        nsCSSProps::kVolumeKTable);
  case eCSSProperty_white_space:
    return ParseVariant(aValue, VARIANT_HMK,
                        nsCSSProps::kWhitespaceKTable);
  case eCSSProperty__moz_window_shadow:
    return ParseVariant(aValue, VARIANT_HOK,
                        nsCSSProps::kWindowShadowKTable);
  case eCSSProperty_word_wrap:
    return ParseVariant(aValue, VARIANT_HMK,
                        nsCSSProps::kWordwrapKTable);
  case eCSSProperty_z_index:
    return ParseVariant(aValue, VARIANT_AHI, nsnull);
  }
  // explicitly do NOT have a default case to let the compiler
  // help find missing properties
  return PR_FALSE;
}

// nsFont::EnumerateFamilies callback for ParseFontDescriptorValue
struct NS_STACK_CLASS ExtractFirstFamilyData {
  nsAutoString mFamilyName;
  PRBool mGood;
  ExtractFirstFamilyData() : mFamilyName(), mGood(PR_FALSE) {}
};

static PRBool
ExtractFirstFamily(const nsString& aFamily,
                   PRBool aGeneric,
                   void* aData)
{
  ExtractFirstFamilyData* realData = (ExtractFirstFamilyData*) aData;
  if (aGeneric || realData->mFamilyName.Length() > 0) {
    realData->mGood = PR_FALSE;
    return PR_FALSE;
  }
  realData->mFamilyName.Assign(aFamily);
  realData->mGood = PR_TRUE;
  return PR_TRUE;
}

// font-descriptor: descriptor ':' value ';'
// caller has advanced mToken to point at the descriptor
PRBool
CSSParserImpl::ParseFontDescriptorValue(nsCSSFontDesc aDescID,
                                        nsCSSValue& aValue)
{
  switch (aDescID) {
    // These four are similar to the properties of the same name,
    // possibly with more restrictions on the values they can take.
  case eCSSFontDesc_Family: {
    if (!ParseFamily(aValue) ||
        aValue.GetUnit() != eCSSUnit_String)
      return PR_FALSE;

    // the style parameters to the nsFont constructor are ignored,
    // because it's only being used to call EnumerateFamilies
    nsAutoString valueStr;
    aValue.GetStringValue(valueStr);
    nsFont font(valueStr, 0, 0, 0, 0, 0);
    ExtractFirstFamilyData dat;

    font.EnumerateFamilies(ExtractFirstFamily, (void*) &dat);
    if (!dat.mGood)
      return PR_FALSE;

    aValue.SetStringValue(dat.mFamilyName, eCSSUnit_String);
    return PR_TRUE;
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
    // property is VARIANT_HMK|VARIANT_SYSFONT
    return (ParseVariant(aValue, VARIANT_KEYWORD | VARIANT_NORMAL,
                         nsCSSProps::kFontStretchKTable) &&
            (aValue.GetUnit() != eCSSUnit_Enumerated ||
             (aValue.GetIntValue() != NS_STYLE_FONT_STRETCH_WIDER &&
              aValue.GetIntValue() != NS_STYLE_FONT_STRETCH_NARROWER)));

    // These two are unique to @font-face and have their own special grammar.
  case eCSSFontDesc_Src:
    return ParseFontSrc(aValue);

  case eCSSFontDesc_UnicodeRange:
    return ParseFontRanges(aValue);

  case eCSSFontDesc_UNKNOWN:
  case eCSSFontDesc_COUNT:
    NS_NOTREACHED("bad nsCSSFontDesc code");
  }
  // explicitly do NOT have a default case to let the compiler
  // help find missing descriptors
  return PR_FALSE;
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

PRBool
CSSParserImpl::ParseAzimuth(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HK | VARIANT_ANGLE,
                   nsCSSProps::kAzimuthKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_AZIMUTH_LEFT_SIDE <= intValue) &&
          (intValue <= NS_STYLE_AZIMUTH_BEHIND)) {  // look for optional modifier
        nsCSSValue  modifier;
        if (ParseEnum(modifier, nsCSSProps::kAzimuthKTable)) {
          PRInt32 enumValue = modifier.GetIntValue();
          if (((intValue == NS_STYLE_AZIMUTH_BEHIND) &&
               (NS_STYLE_AZIMUTH_LEFT_SIDE <= enumValue) && (enumValue <= NS_STYLE_AZIMUTH_RIGHT_SIDE)) ||
              ((enumValue == NS_STYLE_AZIMUTH_BEHIND) &&
               (NS_STYLE_AZIMUTH_LEFT_SIDE <= intValue) && (intValue <= NS_STYLE_AZIMUTH_RIGHT_SIDE))) {
            aValue.SetIntValue(intValue | enumValue, eCSSUnit_Enumerated);
            return PR_TRUE;
          }
          // Put the unknown identifier back and return
          UngetToken();
          return PR_FALSE;
        }
      }
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

static nsCSSValue
BoxPositionMaskToCSSValue(PRInt32 aMask, PRBool isX)
{
  PRInt32 val = NS_STYLE_BG_POSITION_CENTER;
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

PRBool
CSSParserImpl::ParseBackground()
{
  nsAutoParseCompoundProperty compound(this);

  // Fill in the values that the shorthand will set if we don't find
  // other values.
  mTempData.mColor.mBackColor.SetColorValue(NS_RGBA(0, 0, 0, 0));
  mTempData.SetPropertyBit(eCSSProperty_background_color);
  mTempData.mColor.mBackImage.SetNoneValue();
  mTempData.SetPropertyBit(eCSSProperty_background_image);
  mTempData.mColor.mBackRepeat.SetIntValue(NS_STYLE_BG_REPEAT_XY,
                                           eCSSUnit_Enumerated);
  mTempData.SetPropertyBit(eCSSProperty_background_repeat);
  mTempData.mColor.mBackAttachment.SetIntValue(NS_STYLE_BG_ATTACHMENT_SCROLL,
                                               eCSSUnit_Enumerated);
  mTempData.SetPropertyBit(eCSSProperty_background_attachment);
  mTempData.mColor.mBackPosition.mXValue.SetPercentValue(0.0f);
  mTempData.mColor.mBackPosition.mYValue.SetPercentValue(0.0f);
  mTempData.SetPropertyBit(eCSSProperty_background_position);
  // including the ones that we can't set from the shorthand.
  mTempData.mColor.mBackClip.SetIntValue(NS_STYLE_BG_CLIP_BORDER,
                                         eCSSUnit_Enumerated);
  mTempData.SetPropertyBit(eCSSProperty__moz_background_clip);
  mTempData.mColor.mBackOrigin.SetIntValue(NS_STYLE_BG_ORIGIN_PADDING,
                                           eCSSUnit_Enumerated);
  mTempData.SetPropertyBit(eCSSProperty__moz_background_origin);
  mTempData.mColor.mBackInlinePolicy.SetIntValue(
    NS_STYLE_BG_INLINE_POLICY_CONTINUOUS, eCSSUnit_Enumerated);
  mTempData.SetPropertyBit(eCSSProperty__moz_background_inline_policy);

  // XXX If ParseSingleValueProperty were table-driven (bug 376079) and
  // automatically filled in the right field of mTempData, we could move
  // ParseBackgroundPosition to it (as a special case) and switch back
  // to using ParseChoice here.

  PRBool haveColor = PR_FALSE,
         haveImage = PR_FALSE,
         haveRepeat = PR_FALSE,
         haveAttach = PR_FALSE,
         havePosition = PR_FALSE;
  while (GetToken(PR_TRUE)) {
    nsCSSTokenType tt = mToken.mType;
    UngetToken(); // ...but we'll still cheat and use mToken
    if (tt == eCSSToken_Symbol) {
      // ExpectEndProperty only looks for symbols, and nothing else will
      // show up as one.
      break;
    }

    if (tt == eCSSToken_Ident) {
      nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
      PRInt32 dummy;
      if (keyword == eCSSKeyword_inherit ||
          keyword == eCSSKeyword__moz_initial) {
        if (haveColor || haveImage || haveRepeat || haveAttach || havePosition)
          return PR_FALSE;
        haveColor = haveImage = haveRepeat = haveAttach = havePosition =
          PR_TRUE;
        GetToken(PR_TRUE); // undo the UngetToken above
        nsCSSValue val;
        if (keyword == eCSSKeyword_inherit) {
          val.SetInheritValue();
        } else {
          val.SetInitialValue();
        }
        mTempData.mColor.mBackColor = val;
        mTempData.mColor.mBackImage = val;
        mTempData.mColor.mBackRepeat = val;
        mTempData.mColor.mBackAttachment = val;
        mTempData.mColor.mBackPosition.mXValue = val;
        mTempData.mColor.mBackPosition.mYValue = val;
        // Reset (for 'inherit') the 3 properties that can't be
        // specified, although it's not entirely clear in the spec:
        // http://lists.w3.org/Archives/Public/www-style/2007Mar/0110
        mTempData.mColor.mBackClip = val;
        mTempData.mColor.mBackOrigin = val;
        mTempData.mColor.mBackInlinePolicy = val;
        break;
      } else if (keyword == eCSSKeyword_none) {
        if (haveImage)
          return PR_FALSE;
        haveImage = PR_TRUE;
        if (!ParseSingleValueProperty(mTempData.mColor.mBackImage,
                                      eCSSProperty_background_image)) {
          NS_NOTREACHED("should be able to parse");
          return PR_FALSE;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundAttachmentKTable, dummy)) {
        if (haveAttach)
          return PR_FALSE;
        haveAttach = PR_TRUE;
        if (!ParseSingleValueProperty(mTempData.mColor.mBackAttachment,
                                      eCSSProperty_background_attachment)) {
          NS_NOTREACHED("should be able to parse");
          return PR_FALSE;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundRepeatKTable, dummy)) {
        if (haveRepeat)
          return PR_FALSE;
        haveRepeat = PR_TRUE;
        if (!ParseSingleValueProperty(mTempData.mColor.mBackRepeat,
                                      eCSSProperty_background_repeat)) {
          NS_NOTREACHED("should be able to parse");
          return PR_FALSE;
        }
      } else if (nsCSSProps::FindKeyword(keyword,
                   nsCSSProps::kBackgroundPositionKTable, dummy)) {
        if (havePosition)
          return PR_FALSE;
        havePosition = PR_TRUE;
        if (!ParseBackgroundPositionValues()) {
          return PR_FALSE;
        }
      } else {
        if (haveColor)
          return PR_FALSE;
        haveColor = PR_TRUE;
        if (!ParseSingleValueProperty(mTempData.mColor.mBackColor,
                                      eCSSProperty_background_color)) {
          return PR_FALSE;
        }
      }
    } else if (eCSSToken_Function == tt &&
               mToken.mIdent.LowerCaseEqualsLiteral("url")) {
      if (haveImage)
        return PR_FALSE;
      haveImage = PR_TRUE;
      if (!ParseSingleValueProperty(mTempData.mColor.mBackImage,
                                    eCSSProperty_background_image)) {
        return PR_FALSE;
      }
    } else if (mToken.IsDimension() || tt == eCSSToken_Percentage) {
      if (havePosition)
        return PR_FALSE;
      havePosition = PR_TRUE;
      if (!ParseBackgroundPositionValues()) {
        return PR_FALSE;
      }
    } else {
      if (haveColor)
        return PR_FALSE;
      haveColor = PR_TRUE;
      if (!ParseSingleValueProperty(mTempData.mColor.mBackColor,
                                    eCSSProperty_background_color)) {
        return PR_FALSE;
      }
    }
  }

  return ExpectEndProperty() &&
         (haveColor || haveImage || haveRepeat || haveAttach || havePosition);
}

PRBool
CSSParserImpl::ParseBackgroundPosition()
{
  if (!ParseBoxPosition(mTempData.mColor.mBackPosition))
    return PR_FALSE;
  mTempData.SetPropertyBit(eCSSProperty_background_position);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseBackgroundPositionValues()
{
  return ParseBoxPositionValues(mTempData.mColor.mBackPosition);
}

/**
 * Parses two values that correspond to positions in a box.  These can be
 * values corresponding to percentages of the box, raw offsets, or keywords
 * like "top," "left center," etc.
 *
 * @param aOut The nsCSSValuePair where to place the result.
 * @return Whether or not the operation succeeded.
 */
PRBool CSSParserImpl::ParseBoxPosition(nsCSSValuePair &aOut)
{
  // Need to read the box positions and the end of the property.
  return ParseBoxPositionValues(aOut) && ExpectEndProperty();
}

PRBool CSSParserImpl::ParseBoxPositionValues(nsCSSValuePair &aOut)
{
  // First try a percentage or a length value
  nsCSSValue &xValue = aOut.mXValue,
             &yValue = aOut.mYValue;
  if (ParseVariant(xValue, VARIANT_HLP, nsnull)) {
    if (eCSSUnit_Inherit == xValue.GetUnit() ||
        eCSSUnit_Initial == xValue.GetUnit()) {  // both are inherited or both are set to initial
      yValue = xValue;
      return PR_TRUE;
    }
    // We have one percentage/length. Get the optional second
    // percentage/length/keyword.
    if (ParseVariant(yValue, VARIANT_LP, nsnull)) {
      // We have two numbers
      return PR_TRUE;
    }

    if (ParseEnum(yValue, nsCSSProps::kBackgroundPositionKTable)) {
      PRInt32 yVal = yValue.GetIntValue();
      if (!(yVal & BG_CTB)) {
        // The second keyword can only be 'center', 'top', or 'bottom'
        return PR_FALSE;
      }
      yValue = BoxPositionMaskToCSSValue(yVal, PR_FALSE);
      return PR_TRUE;
    }

    // If only one percentage or length value is given, it sets the
    // horizontal position only, and the vertical position will be 50%.
    yValue.SetPercentValue(0.5f);
    return PR_TRUE;
  }

  // Now try keywords. We do this manually to allow for the first
  // appearance of "center" to apply to the either the x or y
  // position (it's ambiguous so we have to disambiguate). Each
  // allowed keyword value is assigned it's own bit. We don't allow
  // any duplicate keywords other than center. We try to get two
  // keywords but it's okay if there is only one.
  PRInt32 mask = 0;
  if (ParseEnum(xValue, nsCSSProps::kBackgroundPositionKTable)) {
    PRInt32 bit = xValue.GetIntValue();
    mask |= bit;
    if (ParseEnum(xValue, nsCSSProps::kBackgroundPositionKTable)) {
      bit = xValue.GetIntValue();
      if (mask & (bit & ~BG_CENTER)) {
        // Only the 'center' keyword can be duplicated.
        return PR_FALSE;
      }
      mask |= bit;
    }
    else {
      // Only one keyword.  See if we have a length or percentage.
      if (ParseVariant(yValue, VARIANT_LP, nsnull)) {
        if (!(mask & BG_CLR)) {
          // The first keyword can only be 'center', 'left', or 'right'
          return PR_FALSE;
        }

        xValue = BoxPositionMaskToCSSValue(mask, PR_TRUE);
        return PR_TRUE;
      }
    }
  }

  // Check for bad input. Bad input consists of no matching keywords,
  // or pairs of x keywords or pairs of y keywords.
  if ((mask == 0) || (mask == (BG_TOP | BG_BOTTOM)) ||
      (mask == (BG_LEFT | BG_RIGHT))) {
    return PR_FALSE;
  }

  // Create style values
  xValue = BoxPositionMaskToCSSValue(mask, PR_TRUE);
  yValue = BoxPositionMaskToCSSValue(mask, PR_FALSE);
  return PR_TRUE;
}

PRBool
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
  return ParseBoxProperties(mTempData.mMargin.mBorderColor,
                            kBorderColorIDs);
}

PRBool
CSSParserImpl::ParseBorderImage()
{
  if (ParseVariant(mTempData.mMargin.mBorderImage,
                   VARIANT_INHERIT | VARIANT_NONE, nsnull)) {
    mTempData.SetPropertyBit(eCSSProperty_border_image);
    return PR_TRUE;
  }

  // <uri> [<number> | <percentage>]{1,4} [ / <border-width>{1,4} ]? [stretch | repeat | round]{0,2}
  nsRefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(11);
  if (!arr) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  nsCSSValue& url = arr->Item(0);
  nsCSSValue& splitTop = arr->Item(1);
  nsCSSValue& splitRight = arr->Item(2);
  nsCSSValue& splitBottom = arr->Item(3);
  nsCSSValue& splitLeft = arr->Item(4);
  nsCSSValue& borderWidthTop = arr->Item(5);
  nsCSSValue& borderWidthRight = arr->Item(6);
  nsCSSValue& borderWidthBottom = arr->Item(7);
  nsCSSValue& borderWidthLeft = arr->Item(8);
  nsCSSValue& horizontalKeyword = arr->Item(9);
  nsCSSValue& verticalKeyword = arr->Item(10);

  // <uri>
  if (!ParseVariant(url, VARIANT_URL, nsnull)) {
    return PR_FALSE;
  }

  // [<number> | <percentage>]{1,4}
  if (!ParsePositiveVariant(splitTop,
                            VARIANT_NUMBER | VARIANT_PERCENT, nsnull)) {
    return PR_FALSE;
  }
  if (!ParsePositiveVariant(splitRight,
                            VARIANT_NUMBER | VARIANT_PERCENT, nsnull)) {
    splitRight = splitTop;
  }
  if (!ParsePositiveVariant(splitBottom,
                            VARIANT_NUMBER | VARIANT_PERCENT, nsnull)) {
    splitBottom = splitTop;
  }
  if (!ParsePositiveVariant(splitLeft,
                            VARIANT_NUMBER | VARIANT_PERCENT, nsnull)) {
    splitLeft = splitRight;
  }

  // [ / <border-width>{1,4} ]?
  if (ExpectSymbol('/', PR_TRUE)) {
    // if have '/', at least one value is required
    if (!ParsePositiveVariant(borderWidthTop, VARIANT_LENGTH, nsnull)) {
      return PR_FALSE;
    }
    if (!ParsePositiveVariant(borderWidthRight, VARIANT_LENGTH, nsnull)) {
      borderWidthRight = borderWidthTop;
    }
    if (!ParsePositiveVariant(borderWidthBottom, VARIANT_LENGTH, nsnull)) {
      borderWidthBottom = borderWidthTop;
    }
    if (!ParsePositiveVariant(borderWidthLeft, VARIANT_LENGTH, nsnull)) {
      borderWidthLeft = borderWidthRight;
    }
  }

  // [stretch | repeat | round]{0,2}
  // missing keywords are handled in nsRuleNode::ComputeBorderData()
  if (ParseEnum(horizontalKeyword, nsCSSProps::kBorderImageKTable)) {
    ParseEnum(verticalKeyword, nsCSSProps::kBorderImageKTable);
  }

  if (!ExpectEndProperty()) {
    return PR_FALSE;
  }

  mTempData.mMargin.mBorderImage.SetArrayValue(arr, eCSSUnit_Array);
  mTempData.SetPropertyBit(eCSSProperty_border_image);

  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseBorderSpacing()
{
  nsCSSValue  xValue;
  if (ParsePositiveVariant(xValue, VARIANT_HL, nsnull)) {
    if (xValue.IsLengthUnit()) {
      // We have one length. Get the optional second length.
      nsCSSValue yValue;
      if (ParsePositiveVariant(yValue, VARIANT_LENGTH, nsnull)) {
        // We have two numbers
        if (ExpectEndProperty()) {
          mTempData.mTable.mBorderSpacing.mXValue = xValue;
          mTempData.mTable.mBorderSpacing.mYValue = yValue;
          mTempData.SetPropertyBit(eCSSProperty_border_spacing);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }

    // We have one length which is the horizontal spacing. Create a value for
    // the vertical spacing which is equal
    if (ExpectEndProperty()) {
      mTempData.mTable.mBorderSpacing.SetBothValuesTo(xValue);
      mTempData.SetPropertyBit(eCSSProperty_border_spacing);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseBorderSide(const nsCSSProperty aPropIDs[],
                               PRBool aSetAllSides)
{
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];

  PRInt32 found = ParseChoice(values, aPropIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty())) {
    return PR_FALSE;
  }

  if ((found & 1) == 0) { // Provide default border-width
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default border-style
    values[1].SetNoneValue();
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
    for (PRInt32 index = 0; index < 4; index++) {
      NS_ASSERTION(numProps == 3, "This code needs updating");
      AppendValue(kBorderWidthIDs[index], values[0]);
      AppendValue(kBorderStyleIDs[index], values[1]);
      AppendValue(kBorderColorIDs[index], values[2]);
    }
  }
  else {
    // Just set our one side
    for (PRInt32 index = 0; index < numProps; index++) {
      AppendValue(aPropIDs[index], values[index]);
    }
  }
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseDirectionalBorderSide(const nsCSSProperty aPropIDs[],
                                          PRInt32 aSourceType)
{
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];

  PRInt32 found = ParseChoice(values, aPropIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty())) {
    return PR_FALSE;
  }

  if ((found & 1) == 0) { // Provide default border-width
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) { // Provide default border-style
    values[1].SetNoneValue();
  }
  if ((found & 4) == 0) { // text color will be used
    values[2].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  }
  for (PRInt32 index = 0; index < numProps; index++) {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(aPropIDs[index + numProps]);
    NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
                 "not box property with physical vs. logical cascading");
    AppendValue(subprops[0], values[index]);
    nsCSSValue typeVal(aSourceType, eCSSUnit_Enumerated);
    AppendValue(subprops[1], typeVal);
    AppendValue(subprops[2], typeVal);
  }
  return PR_TRUE;
}

PRBool
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
  return ParseBoxProperties(mTempData.mMargin.mBorderStyle,
                            kBorderStyleIDs);
}

PRBool
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
  return ParseBoxProperties(mTempData.mMargin.mBorderWidth,
                            kBorderWidthIDs);
}

PRBool
CSSParserImpl::ParseBorderColors(nsCSSValueList** aResult,
                                 nsCSSProperty aProperty)
{
  nsCSSValueList *list = nsnull;
  for (nsCSSValueList **curp = &list, *cur; ; curp = &cur->mNext) {
    cur = *curp = new nsCSSValueList();
    if (!cur) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    if (!ParseVariant(cur->mValue,
                      (cur == list)
                        ? (VARIANT_HCK | VARIANT_NONE)
                        : (VARIANT_COLOR | VARIANT_KEYWORD),
                      nsCSSProps::kBorderColorKTable)) {
      break;
    }
    if (ExpectEndProperty()) {
      // Only success case here, since having the failure case at the
      // end allows more sharing of code.
      mTempData.SetPropertyBit(aProperty);
      *aResult = list;
      return PR_TRUE;
    }
    if (cur->mValue.GetUnit() == eCSSUnit_Inherit ||
        cur->mValue.GetUnit() == eCSSUnit_Initial ||
        cur->mValue.GetUnit() == eCSSUnit_None) {
      // 'inherit', 'initial', and 'none' are only allowed on their own
      break;
    }
  }
  // Have failure case at the end so we can |break| to get to it.
  delete list;
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseRect(nsCSSRect& aRect, nsCSSProperty aPropID)
{
  nsCSSRect rect;
  PRBool result;
  if ((result = DoParseRect(rect)) &&
      rect != aRect) {
    aRect = rect;
    mTempData.SetPropertyBit(aPropID);
  }
  return result;
}

PRBool
CSSParserImpl::DoParseRect(nsCSSRect& aRect)
{
  if (! GetToken(PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_Ident == mToken.mType) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    switch (keyword) {
      case eCSSKeyword_auto:
        if (ExpectEndProperty()) {
          aRect.SetAllSidesTo(nsCSSValue(eCSSUnit_Auto));
          return PR_TRUE;
        }
        break;
      case eCSSKeyword_inherit:
        if (ExpectEndProperty()) {
          aRect.SetAllSidesTo(nsCSSValue(eCSSUnit_Inherit));
          return PR_TRUE;
        }
        break;
      case eCSSKeyword__moz_initial:
        if (ExpectEndProperty()) {
          aRect.SetAllSidesTo(nsCSSValue(eCSSUnit_Initial));
          return PR_TRUE;
        }
        break;
      default:
        UngetToken();
        break;
    }
  } else if ((eCSSToken_Function == mToken.mType) &&
             mToken.mIdent.LowerCaseEqualsLiteral("rect")) {
    if (!ExpectSymbol('(', PR_TRUE)) {
      return PR_FALSE;
    }
    NS_FOR_CSS_SIDES(side) {
      if (! ParseVariant(aRect.*(nsCSSRect::sides[side]),
                         VARIANT_AL, nsnull)) {
        return PR_FALSE;
      }
      if (3 != side) {
        // skip optional commas between elements
        ExpectSymbol(',', PR_TRUE);
      }
    }
    if (!ExpectSymbol(')', PR_TRUE)) {
      return PR_FALSE;
    }
    if (ExpectEndProperty()) {
      return PR_TRUE;
    }
  } else {
    UngetToken();
  }
  return PR_FALSE;
}

#define VARIANT_CONTENT (VARIANT_STRING | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR | \
                         VARIANT_KEYWORD)
PRBool
CSSParserImpl::ParseContent()
{
  // XXX Rewrite to make it look more like ParseCursor or ParseCounterData?
  nsCSSValue  value;
  if (ParseVariant(value,
                   VARIANT_CONTENT | VARIANT_INHERIT | VARIANT_NORMAL |
                     VARIANT_NONE,
                   nsCSSProps::kContentKTable)) {
    nsCSSValueList* listHead = new nsCSSValueList();
    nsCSSValueList* list = listHead;
    if (nsnull == list) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      return PR_FALSE;
    }
    list->mValue = value;

    while (nsnull != list) {
      if (ExpectEndProperty()) {
        mTempData.SetPropertyBit(eCSSProperty_content);
        mTempData.mContent.mContent = listHead;
        return PR_TRUE;
      }
      if (eCSSUnit_Inherit == value.GetUnit() ||
          eCSSUnit_Initial == value.GetUnit() ||
          eCSSUnit_Normal == value.GetUnit() ||
          eCSSUnit_None == value.GetUnit() ||
          (eCSSUnit_Enumerated == value.GetUnit() &&
           NS_STYLE_CONTENT_ALT_CONTENT == value.GetIntValue())) {
        // This only matters the first time through the loop.
        delete listHead;
        return PR_FALSE;
      }
      if (ParseVariant(value, VARIANT_CONTENT, nsCSSProps::kContentKTable) &&
          // Make sure we didn't end up with NS_STYLE_CONTENT_ALT_CONTENT here
          (value.GetUnit() != eCSSUnit_Enumerated ||
           value.GetIntValue() != NS_STYLE_CONTENT_ALT_CONTENT)) {
        list->mNext = new nsCSSValueList();
        list = list->mNext;
        if (nsnull != list) {
          list->mValue = value;
        }
        else {
          mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
        }
      }
      else {
        break;
      }
    }
    delete listHead;
  }
  return PR_FALSE;
}

struct SingleCounterPropValue {
  char str[13];
  nsCSSUnit unit;
};

PRBool
CSSParserImpl::ParseCounterData(nsCSSValuePairList** aResult,
                                nsCSSProperty aPropID)
{
  nsSubstring* ident = NextIdent();
  if (nsnull == ident) {
    return PR_FALSE;
  }
  static const SingleCounterPropValue singleValues[] = {
    { "none", eCSSUnit_None },
    { "inherit", eCSSUnit_Inherit },
    { "-moz-initial", eCSSUnit_Initial }
  };
  for (const SingleCounterPropValue *sv = singleValues,
           *sv_end = singleValues + NS_ARRAY_LENGTH(singleValues);
       sv != sv_end; ++sv) {
    if (ident->LowerCaseEqualsASCII(sv->str)) {
      if (CheckEndProperty()) {
        nsCSSValuePairList* dataHead = new nsCSSValuePairList();
        if (!dataHead) {
          mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
          return PR_FALSE;
        }
        dataHead->mXValue = nsCSSValue(sv->unit);
        *aResult = dataHead;
        mTempData.SetPropertyBit(aPropID);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  }
  UngetToken(); // undo NextIdent

  nsCSSValuePairList* dataHead = nsnull;
  nsCSSValuePairList **next = &dataHead;
  for (;;) {
    if (!GetToken(PR_TRUE) || mToken.mType != eCSSToken_Ident) {
      break;
    }
    nsCSSValuePairList *data = *next = new nsCSSValuePairList();
    if (!data) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    next = &data->mNext;
    data->mXValue.SetStringValue(mToken.mIdent, eCSSUnit_String);
    if (GetToken(PR_TRUE)) {
      if (eCSSToken_Number == mToken.mType && mToken.mIntegerValid) {
        data->mYValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
      } else {
        UngetToken();
      }
    }
    if (ExpectEndProperty()) {
      mTempData.SetPropertyBit(aPropID);
      *aResult = dataHead;
      return PR_TRUE;
    }
  }
  delete dataHead;
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseCue()
{
  nsCSSValue before;
  if (ParseSingleValueProperty(before, eCSSProperty_cue_before)) {
    if (eCSSUnit_Inherit != before.GetUnit() &&
        eCSSUnit_Initial != before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(after, eCSSProperty_cue_after)) {
        if (ExpectEndProperty()) {
          AppendValue(eCSSProperty_cue_before, before);
          AppendValue(eCSSProperty_cue_after, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty()) {
      AppendValue(eCSSProperty_cue_before, before);
      AppendValue(eCSSProperty_cue_after, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseCursor()
{
  nsCSSValueList *list = nsnull;
  for (nsCSSValueList **curp = &list, *cur; ; curp = &cur->mNext) {
    cur = *curp = new nsCSSValueList();
    if (!cur) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    if (!ParseVariant(cur->mValue,
                      (cur == list) ? VARIANT_AHUK : VARIANT_AUK,
                      nsCSSProps::kCursorKTable)) {
      break;
    }
    if (cur->mValue.GetUnit() != eCSSUnit_URL) {
      if (!ExpectEndProperty()) {
        break;
      }
      // Only success case here, since having the failure case at the
      // end allows more sharing of code.
      mTempData.SetPropertyBit(eCSSProperty_cursor);
      mTempData.mUserInterface.mCursor = list;
      return PR_TRUE;
    }
    // We have a URL, so make a value array with three values.
    nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(3);
    if (!val) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    val->Item(0) = cur->mValue;
    cur->mValue.SetArrayValue(val, eCSSUnit_Array);

    // Parse optional x and y position of cursor hotspot (css3-ui).
    if (ParseVariant(val->Item(1), VARIANT_NUMBER, nsnull)) {
      // If we have one number, we must have two.
      if (!ParseVariant(val->Item(2), VARIANT_NUMBER, nsnull)) {
        break;
      }
    }

    if (!ExpectSymbol(',', PR_TRUE)) {
      break;
    }
  }
  // Have failure case at the end so we can |break| to get to it.
  delete list;
  return PR_FALSE;
}


PRBool
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
      }
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // Get optional font-style, font-variant and font-weight (in any order)
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(values, fontIDs, numProps);
  if ((found < 0) || (eCSSUnit_Inherit == values[0].GetUnit()) ||
      (eCSSUnit_Initial == values[0].GetUnit())) { // illegal data
    return PR_FALSE;
  }
  if ((found & 1) == 0) {
    // Provide default font-style
    values[0].SetNormalValue();
  }
  if ((found & 2) == 0) {
    // Provide default font-variant
    values[1].SetNormalValue();
  }
  if ((found & 4) == 0) {
    // Provide default font-weight
    values[2].SetNormalValue();
  }

  // Get mandatory font-size
  nsCSSValue  size;
  if (! ParseVariant(size, VARIANT_KEYWORD | VARIANT_LP, nsCSSProps::kFontSizeKTable)) {
    return PR_FALSE;
  }

  // Get optional "/" line-height
  nsCSSValue  lineHeight;
  if (ExpectSymbol('/', PR_TRUE)) {
    if (! ParsePositiveVariant(lineHeight,
                               VARIANT_NUMBER | VARIANT_LP | VARIANT_NORMAL,
                               nsnull)) {
      return PR_FALSE;
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
      AppendValue(eCSSProperty_font_stretch, nsCSSValue(eCSSUnit_Normal));
      AppendValue(eCSSProperty_font_size_adjust, nsCSSValue(eCSSUnit_None));
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseFontWeight(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HMKI | VARIANT_SYSFONT, nsCSSProps::kFontWeightKTable)) {
    if (eCSSUnit_Integer == aValue.GetUnit()) { // ensure unit value
      PRInt32 intValue = aValue.GetIntValue();
      if ((100 <= intValue) &&
          (intValue <= 900) &&
          (0 == (intValue % 100))) {
        return PR_TRUE;
      } else {
        UngetToken();
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseOneFamily(nsAString& aFamily)
{
  if (!GetToken(PR_TRUE))
    return PR_FALSE;

  nsCSSToken* tk = &mToken;

  if (eCSSToken_Ident == tk->mType) {
    aFamily.Append(tk->mIdent);
    for (;;) {
      if (!GetToken(PR_FALSE))
        break;

      if (eCSSToken_Ident == tk->mType) {
        aFamily.Append(tk->mIdent);
      } else if (eCSSToken_WhiteSpace == tk->mType) {
        // Lookahead one token and drop whitespace if we are ending the
        // font name.
        if (!GetToken(PR_TRUE))
          break;

        UngetToken();
        if (eCSSToken_Ident == tk->mType)
          aFamily.Append(PRUnichar(' '));
        else
          break;
      } else {
        UngetToken();
        break;
      }
    }
    return PR_TRUE;

  } else if (eCSSToken_String == tk->mType) {
    aFamily.Append(tk->mSymbol); // replace the quotes
    aFamily.Append(tk->mIdent); // XXX What if it had escaped quotes?
    aFamily.Append(tk->mSymbol);
    return PR_TRUE;

  } else {
    UngetToken();
    return PR_FALSE;
  }
}

///////////////////////////////////////////////////////
// -moz-transform Parsing Implementation

/* Reads a function list of arguments.  Do not call this function
 * directly; it's mean to be caled from ParseFunction.
 */
PRBool
CSSParserImpl::ParseFunctionInternals(const PRInt32 aVariantMask[],
                                      PRUint16 aMinElems,
                                      PRUint16 aMaxElems,
                                      nsTArray<nsCSSValue> &aOutput)
{
  for (PRUint16 index = 0; index < aMaxElems; ++index) {
    nsCSSValue newValue;
    if (!ParseVariant(newValue, aVariantMask[index], nsnull))
      return PR_FALSE;

    if (!aOutput.AppendElement(newValue)) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      return PR_FALSE;
    }
    
    // See whether to continue or whether to look for end of function.
    if (!ExpectSymbol(',', PR_TRUE)) {
      // We need to read the closing parenthesis, and also must take care
      // that we haven't read too few symbols.
      return ExpectSymbol(')', PR_TRUE) && (index + 1) >= aMinElems;
    }
  }

  // If we're here, we finished looping without hitting the end, so we read too
  // many elements.
  return PR_FALSE;
}

/* Parses a function [ input of the form (a [, b]*) ] and stores it
 * as an nsCSSValue that holds a function of the form
 * function-name arg1 arg2 ... argN
 *
 * On error, the return value is PR_FALSE.
 *
 * @param aFunction The name of the function that we're reading.
 * @param aAllowedTypes An array of values corresponding to the legal
 *        types for each element in the function.  The zeroth element in the
 *        array corresponds to the first function parameter, etc.  The length
 *        of this array _must_ be greater than or equal to aMaxElems or the
 *        behavior is undefined.
 * @param aMinElems Minimum number of elements to read.  Reading fewer than
 *        this many elements will result in the function failing.
 * @param aMaxElems Maximum number of elements to read.  Reading more than
 *        this many elements will result in the function failing.
 * @param aValue (out) The value that was parsed.
 */
PRBool
CSSParserImpl::ParseFunction(const nsString &aFunction,
                             const PRInt32 aAllowedTypes[],
                             PRUint16 aMinElems, PRUint16 aMaxElems,
                             nsCSSValue &aValue)
{
  typedef nsTArray<nsCSSValue>::size_type arrlen_t;

  /* 2^16 - 2, so that if we have 2^16 - 2 transforms, we have 2^16 - 1
   * elements stored in the the nsCSSValue::Array.
   */
  static const arrlen_t MAX_ALLOWED_ELEMS = 0xFFFE;

  /* Make a copy of the function name, since the reference is _probably_ to
   * mToken.mIdent, which is going to get overwritten during the course of this
   * function.
   */
  nsString functionName(aFunction);

  /* First things first... read the parenthesis.  If it fails, abort. */
  if (!ExpectSymbol('(', PR_TRUE))
    return PR_FALSE;
  
  /* Read in a list of values as an nsTArray, failing if we can't or if
   * it's out of bounds.
   */
  nsTArray<nsCSSValue> foundValues;
  if (!ParseFunctionInternals(aAllowedTypes, aMinElems, aMaxElems,
                              foundValues))
    return PR_FALSE;
  
  /* Now, convert this nsTArray into an nsCSSValue::Array object.
   * We'll need N + 1 spots, one for the function name and the rest for the
   * arguments.  In case the user has given us more than 2^16 - 2 arguments,
   * we'll truncate them at 2^16 - 2 arguments.
   */
  PRUint16 numElements = (foundValues.Length() <= MAX_ALLOWED_ELEMS ?
                          foundValues.Length() + 1 : MAX_ALLOWED_ELEMS);
  nsRefPtr<nsCSSValue::Array> convertedArray =
    nsCSSValue::Array::Create(numElements);
  if (!convertedArray) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }
  
  /* Copy things over. */
  convertedArray->Item(0).SetStringValue(functionName, eCSSUnit_String);
  for (PRUint16 index = 0; index + 1 < numElements; ++index)
    convertedArray->Item(index + 1) = foundValues[static_cast<arrlen_t>(index)];
  
  /* Fill in the outparam value with the array. */
  aValue.SetArrayValue(convertedArray, eCSSUnit_Function);
  
  /* Return it! */
  return PR_TRUE;
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
static PRBool GetFunctionParseInformation(nsCSSKeyword aToken,
                                          PRUint16 &aMinElems,
                                          PRUint16 &aMaxElems,
                                          const PRInt32 *& aVariantMask)
{
/* These types represent the common variant masks that will be used to
   * parse out the individual functions.  The order in the enumeration
   * must match the order in which the masks are declared.
   */
  enum { eLengthPercent,
         eTwoLengthPercents,
         eAngle,
         eTwoAngles,
         eNumber,
         eTwoNumbers,
         eMatrix,
         eNumVariantMasks };
  static const PRInt32 kMaxElemsPerFunction = 6;
  static const PRInt32 kVariantMasks[eNumVariantMasks][kMaxElemsPerFunction] = {
    {VARIANT_LENGTH | VARIANT_PERCENT},
    {VARIANT_LENGTH | VARIANT_PERCENT, VARIANT_LENGTH | VARIANT_PERCENT},
    {VARIANT_ANGLE},
    {VARIANT_ANGLE, VARIANT_ANGLE},
    {VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER},
    {VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER, VARIANT_NUMBER,
     VARIANT_LENGTH | VARIANT_PERCENT, VARIANT_LENGTH | VARIANT_PERCENT}};

#ifdef DEBUG
  static const PRUint8 kVariantMaskLengths[eNumVariantMasks] =
    {1, 2, 1, 2, 1, 2, 6};
#endif

  PRInt32 variantIndex = eNumVariantMasks;

  switch (aToken) {
  case eCSSKeyword_translatex:
    /* Exactly one length or percent. */
    variantIndex = eLengthPercent;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_translatey:
    /* Exactly one length or percent. */
    variantIndex = eLengthPercent;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_scalex:
    /* Exactly one scale factor. */
    variantIndex = eNumber;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_scaley:
    /* Exactly one scale factor. */
    variantIndex = eNumber;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_rotate:
    /* Exactly one angle. */
    variantIndex = eAngle;
    aMinElems = 1U;
    aMaxElems = 1U;
    break;
  case eCSSKeyword_translate:
    /* One or two lengths or percents. */
    variantIndex = eTwoLengthPercents;
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
    /* Six values, which can be numbers, lengths, or percents. */
    variantIndex = eMatrix;
    aMinElems = 6U;
    aMaxElems = 6U;
    break;    
  default:
    /* Oh dear, we didn't match.  Report an error. */
    return PR_FALSE;
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

  return PR_TRUE;
}
                                          

/* Reads a single transform function from the tokenizer stream, reporting an
 * error if something goes wrong.
 */
PRBool CSSParserImpl::ReadSingleTransform(nsCSSValueList **& aTail)
{
  typedef nsTArray<nsCSSValue>::size_type arrlen_t;

  if (!GetToken(PR_TRUE))
    return PR_FALSE;
  
  /* Check to make sure that we've read a function. */
  if (mToken.mType != eCSSToken_Function) {
    UngetToken();
    return PR_FALSE;
  }

  /* Load up the variant mask information for ParseFunction.  If we can't,
   * abort.
   */
  const PRInt32* variantMask;
  PRUint16 minElems, maxElems;
  if (!GetFunctionParseInformation(nsCSSKeywords::LookupKeyword(mToken.mIdent),
                                   minElems, maxElems, variantMask))
    return PR_FALSE;

  /* Create a cell to populate, fail if we're out of memory. */
  nsAutoPtr<nsCSSValue> newCell(new nsCSSValue);
  if (!newCell) {
    mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
    return PR_FALSE;
  }

  /* Try reading things in, failing if we can't */
  if (!ParseFunction(mToken.mIdent, variantMask, minElems, maxElems, *newCell))
    return PR_FALSE;

  /* Wrap up our result in an nsCSSValueList cell. */
  nsAutoPtr<nsCSSValueList> toAppend(new nsCSSValueList);
  if (!toAppend)
    return PR_FALSE;

  toAppend->mValue = *newCell;
  
  /* Chain the element to the end of the transform list, then update the
   * list.
   */
  *aTail = toAppend.forget();
  aTail = &(*aTail)->mNext;
  
  /* It worked!  Return true. */
  return PR_TRUE;
}

/* Parses a -moz-transform property list by continuously reading in properties
 * and constructing a matrix from it.
 */
PRBool CSSParserImpl::ParseMozTransform()
{
  mTempData.mDisplay.mTransform = nsnull;
 
  /* First, check to see if this is some sort of keyword - none, inherit,
   * or initial.
   */
  nsCSSValue keywordValue;
  if (ParseVariant(keywordValue, VARIANT_INHERIT | VARIANT_NONE, nsnull)) {
    /* Looks like it is.  Make a new value list and fill it in, failing if
     * we can't.
     */
    mTempData.mDisplay.mTransform = new nsCSSValueList;
    if (!mTempData.mDisplay.mTransform) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      return PR_FALSE;
    }

    /* Inform the parser that everything worked. */
    mTempData.mDisplay.mTransform->mValue = keywordValue;
    mTempData.SetPropertyBit(eCSSProperty__moz_transform);
    return PR_TRUE;
  }
  
  /* We will read a nonempty list of transforms.  Thus we'll read in a
   * transform, then continuously read transforms until we're no longer
   * able to do so.
   */
  nsCSSValueList *transformList = nsnull;
  nsCSSValueList **tail = &transformList;
  do {
    /* Try reading a transform.  If we fail to do so, abort. */
    if (!ReadSingleTransform(tail)) {
      delete transformList;
      return PR_FALSE;
    }
  }
  while (!CheckEndProperty());

  /* Confirm that this is the end of the property and set error code
   * appropriately otherwise.
   */
  if (!ExpectEndProperty()) {
    delete transformList;
    return PR_FALSE;
  }
  
  /* Validate our data. */
  NS_ASSERTION(transformList, "Didn't read any transforms!");
  
  mTempData.SetPropertyBit(eCSSProperty__moz_transform);
  mTempData.mDisplay.mTransform = transformList;

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseMozTransformOrigin()
{
  /* Read in a box position, fail if we can't. */
  if (!ParseBoxPosition(mTempData.mDisplay.mTransformOrigin))
    return PR_FALSE;

  /* Set the property bit and return. */
  mTempData.SetPropertyBit(eCSSProperty__moz_transform_origin);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseFamily(nsCSSValue& aValue)
{
  if (!GetToken(PR_TRUE))
    return PR_FALSE;

  if (eCSSToken_Ident == mToken.mType) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    if (keyword == eCSSKeyword_inherit) {
      aValue.SetInheritValue();
      return PR_TRUE;
    }
    if (keyword == eCSSKeyword__moz_initial) {
      aValue.SetInitialValue();
      return PR_TRUE;
    }
    if (keyword == eCSSKeyword__moz_use_system_font &&
        !IsParsingCompoundProperty()) {
      aValue.SetSystemFontValue();
      return PR_TRUE;
    }
  }

  UngetToken();

  nsAutoString family;
  for (;;) {
    if (!ParseOneFamily(family))
      return PR_FALSE;

    if (!ExpectSymbol(',', PR_TRUE))
      break;

    family.Append(PRUnichar(','));
  }

  if (family.IsEmpty()) {
    return PR_FALSE;
  }
  aValue.SetStringValue(family, eCSSUnit_String);
  return PR_TRUE;
}

// src: ( uri-src | local-src ) (',' ( uri-src | local-src ) )*
// uri-src: uri [ 'format(' string ( ',' string )* ')' ]
// local-src: 'local(' ( string | ident ) ')'

PRBool
CSSParserImpl::ParseFontSrc(nsCSSValue& aValue)
{
  // could we maybe turn nsCSSValue::Array into nsTArray<nsCSSValue>?
  nsTArray<nsCSSValue> values;
  nsCSSValue cur;
  for (;;) {
    if (!GetToken(PR_TRUE))
      break;

    if (mToken.mType == eCSSToken_Function &&
        mToken.mIdent.LowerCaseEqualsLiteral("url")) {
      if (!ParseURL(cur))
        return PR_FALSE;
      values.AppendElement(cur);
      if (!ParseFontSrcFormat(values))
        return PR_FALSE;

    } else if (mToken.mType == eCSSToken_Function &&
               mToken.mIdent.LowerCaseEqualsLiteral("local")) {
      // css3-fonts does not specify a formal grammar for local().
      // The text permits both unquoted identifiers and quoted
      // strings.  We resolve this ambiguity in the spec by
      // assuming that the appropriate production is a single
      // <family-name>, possibly surrounded by whitespace.

      nsAutoString family;
      if (!ExpectSymbol('(', PR_FALSE))
        return PR_FALSE;
      if (!ParseOneFamily(family))
        return PR_FALSE;
      if (!ExpectSymbol(')', PR_TRUE))
        return PR_FALSE;

      // the style parameters to the nsFont constructor are ignored,
      // because it's only being used to call EnumerateFamilies
      nsFont font(family, 0, 0, 0, 0, 0);
      ExtractFirstFamilyData dat;

      font.EnumerateFamilies(ExtractFirstFamily, (void*) &dat);
      if (!dat.mGood)
        return PR_FALSE;

      cur.SetStringValue(dat.mFamilyName, eCSSUnit_Local_Font);
      values.AppendElement(cur);
    } else {
      return PR_FALSE;
    }

    if (!ExpectSymbol(',', PR_TRUE))
      break;
  }

  nsRefPtr<nsCSSValue::Array> srcVals
    = nsCSSValue::Array::Create(values.Length());
  if (!srcVals)
    return PR_FALSE;

  PRUint32 i;
  for (i = 0; i < values.Length(); i++)
    srcVals->Item(i) = values[i];
  aValue.SetArrayValue(srcVals, eCSSUnit_Array);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseFontSrcFormat(nsTArray<nsCSSValue> & values)
{
  if (!GetToken(PR_TRUE))
    return PR_TRUE; // EOF harmless here
  if (mToken.mType != eCSSToken_Function ||
      !mToken.mIdent.LowerCaseEqualsLiteral("format")) {
    UngetToken();
    return PR_TRUE;
  }
  if (!ExpectSymbol('(', PR_FALSE))
    return PR_FALSE;

  do {
    if (!GetToken(PR_TRUE))
      return PR_FALSE;

    if (mToken.mType != eCSSToken_String)
      return PR_FALSE;

    nsCSSValue cur(mToken.mIdent, eCSSUnit_Font_Format);
    values.AppendElement(cur);
  } while (ExpectSymbol(',', PR_TRUE));

  return ExpectSymbol(')', PR_TRUE);
}

// font-ranges: urange ( ',' urange )*
PRBool
CSSParserImpl::ParseFontRanges(nsCSSValue& aValue)
{
  // not currently implemented (bug 443976)
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseListStyle()
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty listStyleIDs[] = {
    eCSSProperty_list_style_type,
    eCSSProperty_list_style_position,
    eCSSProperty_list_style_image
  };

  nsCSSValue  values[numProps];
  PRInt32 index;
  PRInt32 found = ParseChoice(values, listStyleIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty())) {
    return PR_FALSE;
  }

  // Provide default values
  if ((found & 1) == 0) {
    values[0].SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
  }
  if ((found & 2) == 0) {
    values[1].SetIntValue(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE, eCSSUnit_Enumerated);
  }
  if ((found & 4) == 0) {
    values[2].SetNoneValue();
  }

  for (index = 0; index < numProps; index++) {
    AppendValue(listStyleIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool
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
  return ParseBoxProperties(mTempData.mMargin.mMargin,
                            kMarginSideIDs);
}

PRBool
CSSParserImpl::ParseMarks(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HOK, nsCSSProps::kPageMarksKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      if (PR_FALSE == CheckEndProperty()) {
        nsCSSValue  second;
        if (ParseEnum(second, nsCSSProps::kPageMarksKTable)) {
          aValue.SetIntValue(aValue.GetIntValue() | second.GetIntValue(), eCSSUnit_Enumerated);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseOutline()
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty kOutlineIDs[] = {
    eCSSProperty_outline_color,
    eCSSProperty_outline_style,
    eCSSProperty_outline_width
  };

  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(values, kOutlineIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty())) {
    return PR_FALSE;
  }

  // Provide default values
  if ((found & 1) == 0) {
#ifdef GFX_HAS_INVERT
    values[0].SetIntValue(NS_STYLE_COLOR_INVERT, eCSSUnit_Enumerated);
#else
    values[0].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
#endif
  }
  if ((found & 2) == 0) {
    values[1].SetNoneValue();
  }
  if ((found & 4) == 0) {
    values[2].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }

  PRInt32 index;
  for (index = 0; index < numProps; index++) {
    AppendValue(kOutlineIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseOverflow()
{
  nsCSSValue overflow;
  if (!ParseVariant(overflow, VARIANT_AHK,
                   nsCSSProps::kOverflowKTable) ||
      !ExpectEndProperty())
    return PR_FALSE;

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
  return PR_TRUE;
}

PRBool
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
  return ParseBoxProperties(mTempData.mMargin.mPadding,
                            kPaddingSideIDs);
}

PRBool
CSSParserImpl::ParsePause()
{
  nsCSSValue  before;
  if (ParseSingleValueProperty(before, eCSSProperty_pause_before)) {
    if (eCSSUnit_Inherit != before.GetUnit() && eCSSUnit_Initial != before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(after, eCSSProperty_pause_after)) {
        if (ExpectEndProperty()) {
          AppendValue(eCSSProperty_pause_before, before);
          AppendValue(eCSSProperty_pause_after, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty()) {
      AppendValue(eCSSProperty_pause_before, before);
      AppendValue(eCSSProperty_pause_after, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseQuotes()
{
  nsCSSValue  open;
  if (ParseVariant(open, VARIANT_HOS, nsnull)) {
    if (eCSSUnit_String == open.GetUnit()) {
      nsCSSValuePairList* quotesHead = new nsCSSValuePairList();
      nsCSSValuePairList* quotes = quotesHead;
      if (nsnull == quotes) {
        mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
        return PR_FALSE;
      }
      quotes->mXValue = open;
      while (nsnull != quotes) {
        // get mandatory close
        if (ParseVariant(quotes->mYValue, VARIANT_STRING,
                         nsnull)) {
          if (CheckEndProperty()) {
            mTempData.SetPropertyBit(eCSSProperty_quotes);
            mTempData.mContent.mQuotes = quotesHead;
            return PR_TRUE;
          }
          // look for another open
          if (ParseVariant(open, VARIANT_STRING, nsnull)) {
            quotes->mNext = new nsCSSValuePairList();
            quotes = quotes->mNext;
            if (nsnull != quotes) {
              quotes->mXValue = open;
              continue;
            }
            mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
          }
        }
        break;
      }
      delete quotesHead;
      return PR_FALSE;
    }
    if (ExpectEndProperty()) {
      nsCSSValuePairList* quotesHead = new nsCSSValuePairList();
      quotesHead->mXValue = open;
      mTempData.mContent.mQuotes = quotesHead;
      mTempData.SetPropertyBit(eCSSProperty_quotes);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseSize()
{
  nsCSSValue width;
  if (ParseVariant(width, VARIANT_AHKL, nsCSSProps::kPageSizeKTable)) {
    if (width.IsLengthUnit()) {
      nsCSSValue  height;
      if (ParseVariant(height, VARIANT_LENGTH, nsnull)) {
        if (ExpectEndProperty()) {
          mTempData.mPage.mSize.mXValue = width;
          mTempData.mPage.mSize.mYValue = height;
          mTempData.SetPropertyBit(eCSSProperty_size);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty()) {
      mTempData.mPage.mSize.SetBothValuesTo(width);
      mTempData.SetPropertyBit(eCSSProperty_size);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseTextDecoration(nsCSSValue& aValue)
{
  if (ParseVariant(aValue, VARIANT_HOK, nsCSSProps::kTextDecorationKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {  // look for more keywords
      PRInt32 intValue = aValue.GetIntValue();
      nsCSSValue  keyword;
      PRInt32 index;
      for (index = 0; index < 3; index++) {
        if (ParseEnum(keyword, nsCSSProps::kTextDecorationKTable)) {
          PRInt32 newValue = keyword.GetIntValue();
          if (newValue & intValue) {
            // duplicate keyword is not allowed
            return PR_FALSE;
          }
          intValue |= newValue;
        }
        else {
          break;
        }
      }
      aValue.SetIntValue(intValue, eCSSUnit_Enumerated);
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsCSSValueList*
CSSParserImpl::ParseCSSShadowList(PRBool aUsesSpread)
{
  nsAutoParseCompoundProperty compound(this);

  // Parses x, y, radius, color (in two possible orders)
  // This parses the input into a list. Either it contains just a "none" or
  // "inherit" value, or a list of arrays.
  // The resulting arrays will always contain the above order, with color and
  // radius as null values as needed
  enum {
    IndexX,
    IndexY,
    IndexRadius,
    IndexSpread,
    IndexColor
  };

  nsCSSValueList *list = nsnull;
  for (nsCSSValueList **curp = &list, *cur; ; curp = &cur->mNext) {
    cur = *curp = new nsCSSValueList();
    if (!cur) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    if (!ParseVariant(cur->mValue,
                      (cur == list) ? VARIANT_HC | VARIANT_LENGTH | VARIANT_NONE
                                    : VARIANT_COLOR | VARIANT_LENGTH,
                      nsnull)) {
      break;
    }

    nsCSSUnit unit = cur->mValue.GetUnit();
    if (unit != eCSSUnit_None && unit != eCSSUnit_Inherit &&
        unit != eCSSUnit_Initial) {
      nsRefPtr<nsCSSValue::Array> val = nsCSSValue::Array::Create(5);
      if (!val) {
        mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
        break;
      }
      PRBool haveColor = PR_FALSE;
      if (cur->mValue.IsLengthUnit()) {
        val->Item(IndexX) = cur->mValue;
      } else {
        // Must be a color (as string or color value)
        NS_ASSERTION(unit == eCSSUnit_String || unit == eCSSUnit_Color ||
                     unit == eCSSUnit_EnumColor,
                     "Must be a color value (named color, numeric color, "
                     "or system color)");
        haveColor = PR_TRUE;
        val->Item(IndexColor) = cur->mValue;

        // Parse the X coordinate
        if (!ParseVariant(val->Item(IndexX), VARIANT_LENGTH,
                          nsnull)) {
          break;
        }
      }
      cur->mValue.SetArrayValue(val, eCSSUnit_Array);

      // Y coordinate; this one is not optional
      if (!ParseVariant(val->Item(IndexY), VARIANT_LENGTH, nsnull)) {
        break;
      }

      // Optional radius. Ignore errors except if they pass a negative
      // value which we must reject. If we use ParsePositiveVariant we can't
      // tell the difference between an unspecified radius and a negative
      // radius, so that's why we don't use it.
      if (ParseVariant(val->Item(IndexRadius), VARIANT_LENGTH, nsnull) &&
          val->Item(IndexRadius).GetFloatValue() < 0) {
        break;
      }

      if (aUsesSpread) {
        // Optional spread (ignore errors)
        ParseVariant(val->Item(IndexSpread), VARIANT_LENGTH,
                     nsnull);
      }

      if (!haveColor) {
        // Optional color (ignore errors)
        ParseVariant(val->Item(IndexColor), VARIANT_COLOR,
                     nsnull);
      }

      // Might be at a comma now
      if (ExpectSymbol(',', PR_TRUE)) {
        // Go to next value
        continue;
      }
    }

    if (!ExpectEndProperty()) {
      // If we don't have a comma to delimit the next value, we
      // must be at the end of the property.  Otherwise we've hit
      // something else, which is an error.
      break;
    }

    // Only success case here, since having the failure case at the
    // end allows more sharing of code.
    return list;
  }
  // Have failure case at the end so we can |break| to get to it.
  delete list;
  return nsnull;
}

PRBool
CSSParserImpl::ParseTextShadow()
{
  nsCSSValueList* list = ParseCSSShadowList(PR_FALSE);
  if (!list)
    return PR_FALSE;

  mTempData.SetPropertyBit(eCSSProperty_text_shadow);
  mTempData.mText.mTextShadow = list;
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseBoxShadow()
{
  nsCSSValueList* list = ParseCSSShadowList(PR_TRUE);
  if (!list)
    return PR_FALSE;

  mTempData.SetPropertyBit(eCSSProperty_box_shadow);
  mTempData.mMargin.mBoxShadow = list;
  return PR_TRUE;
}

PRBool
CSSParserImpl::GetNamespaceIdForPrefix(const nsString& aPrefix,
                                       PRInt32* aNameSpaceID)
{
  NS_PRECONDITION(!aPrefix.IsEmpty(), "Must have a prefix here");

  PRInt32 nameSpaceID = kNameSpaceID_Unknown;
  if (mNameSpaceMap) {
    // user-specified identifiers are case-sensitive (bug 416106)
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(aPrefix);
    nameSpaceID = mNameSpaceMap->FindNameSpaceID(prefix);
  }
  // else no declared namespaces

  if (kNameSpaceID_Unknown == nameSpaceID) {   // unknown prefix, dump it
    const PRUnichar *params[] = {
      aPrefix.get()
    };
    REPORT_UNEXPECTED_P(PEUnknownNamespacePrefix, params);
    if (mUnresolvablePrefixException)
      mScanner.SetLowLevelError(NS_ERROR_DOM_NAMESPACE_ERR);
    return PR_FALSE;
  }

  *aNameSpaceID = nameSpaceID;
  return PR_TRUE;
}

void
CSSParserImpl::SetDefaultNamespaceOnSelector(nsCSSSelector& aSelector)
{
  if (mNameSpaceMap) {
    aSelector.SetNameSpace(mNameSpaceMap->FindNameSpaceID(nsnull));
  } else {
    aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
  }
}

#ifdef MOZ_SVG
PRBool
CSSParserImpl::ParsePaint(nsCSSValuePair* aResult,
                          nsCSSProperty aPropID)
{
  if (!ParseVariant(aResult->mXValue,
                    VARIANT_HC | VARIANT_NONE | VARIANT_URL,
                    nsnull))
    return PR_FALSE;

  if (aResult->mXValue.GetUnit() == eCSSUnit_URL) {
    if (!ParseVariant(aResult->mYValue, VARIANT_COLOR | VARIANT_NONE,
                     nsnull))
      aResult->mYValue.SetColorValue(NS_RGB(0, 0, 0));
  } else {
    aResult->mYValue = aResult->mXValue;
  }

  if (!ExpectEndProperty())
    return PR_FALSE;

  mTempData.SetPropertyBit(aPropID);
  return PR_TRUE;
}

PRBool
CSSParserImpl::ParseDasharray()
{
  nsCSSValue value;
  if (ParseVariant(value, VARIANT_HLPN | VARIANT_NONE, nsnull)) {
    nsCSSValueList *listHead = new nsCSSValueList;
    nsCSSValueList *list = listHead;
    if (!list) {
      mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
      return PR_FALSE;
    }

    list->mValue = value;

    for (;;) {
      if (CheckEndProperty()) {
        mTempData.SetPropertyBit(eCSSProperty_stroke_dasharray);
        mTempData.mSVG.mStrokeDasharray = listHead;
        return PR_TRUE;
      }

      if (eCSSUnit_Inherit == value.GetUnit() ||
          eCSSUnit_Initial == value.GetUnit() ||
          eCSSUnit_None    == value.GetUnit())
        break;

      if (!ExpectSymbol(',', PR_TRUE))
        break;

      if (!ParseVariant(value,
                        VARIANT_LENGTH | VARIANT_PERCENT | VARIANT_NUMBER,
                        nsnull))
        break;

      list->mNext = new nsCSSValueList;
      list = list->mNext;
      if (list)
        list->mValue = value;
      else {
        mScanner.SetLowLevelError(NS_ERROR_OUT_OF_MEMORY);
        break;
      }
    }
    delete listHead;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseMarker()
{
  nsCSSValue marker;
  if (ParseSingleValueProperty(marker, eCSSProperty_marker_end)) {
    if (ExpectEndProperty()) {
      AppendValue(eCSSProperty_marker_end, marker);
      AppendValue(eCSSProperty_marker_mid, marker);
      AppendValue(eCSSProperty_marker_start, marker);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}
#endif
