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
#include "nsISupportsArray.h"
#include "nsCOMArray.h"
#include "nsColor.h"
#include "nsStyleConsts.h"
#include "nsLayoutAtoms.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "nsThemeConstants.h"
#include "nsContentErrors.h"
#include "nsUnitConversion.h"
#include "nsPrintfCString.h"
#include "nsIStringBundle.h"

#include "prprf.h"
#include "math.h"

#define ENABLE_OUTLINE   // un-comment this to enable the outline properties (bug 9816)
                         // XXX un-commenting for temporary fix for nsbeta3+ Bug 48973
                         // so we can use "mozoutline

//#define ENABLE_COUNTERS  // un-comment this to enable counters (bug 15174)

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
                   PRUint32               aLineNumber,
                   nsICSSStyleSheet*&     aResult);

  NS_IMETHOD ParseStyleAttribute(const nsAString&  aAttributeValue,
                                 nsIURI*           aDocURL,
                                 nsIURI*           aBaseURL,
                                 nsICSSStyleRule** aResult);
  
  NS_IMETHOD ParseAndAppendDeclaration(const nsAString&  aBuffer,
                                       nsIURI*           aSheetURL,
                                       nsIURI*           aBaseURL,
                                       nsCSSDeclaration* aDeclaration,
                                       PRBool            aParseOnlyOneDecl,
                                       PRBool*           aChanged,
                                       PRBool            aClearOldDecl);

  NS_IMETHOD ParseRule(const nsAString&   aRule,
                       nsIURI*            aSheetURL,
                       nsIURI*            aBaseURL,
                       nsISupportsArray** aResult);

  NS_IMETHOD ParseProperty(const nsCSSProperty aPropID,
                           const nsAString& aPropValue,
                           nsIURI* aSheetURL,
                           nsIURI* aBaseURL,
                           nsCSSDeclaration* aDeclaration,
                           PRBool* aChanged);

  void AppendRule(nsICSSRule* aRule);

protected:
  nsresult InitScanner(nsIUnicharInputStream* aInput, nsIURI* aSheetURI,
                       PRUint32 aLineNumber, nsIURI* aBaseURI);
  nsresult ReleaseScanner(void);

  PRBool GetToken(nsresult& aErrorCode, PRBool aSkipWS);
  PRBool GetURLToken(nsresult& aErrorCode, PRBool aSkipWS);
  void UngetToken();

  PRBool ExpectSymbol(nsresult& aErrorCode, PRUnichar aSymbol, PRBool aSkipWS);
  PRBool ExpectEndProperty(nsresult& aErrorCode, PRBool aSkipWS);
  nsString* NextIdent(nsresult& aErrorCode);
  void SkipUntil(nsresult& aErrorCode, PRUnichar aStopSymbol);
  void SkipRuleSet(nsresult& aErrorCode);
  PRBool SkipAtRule(nsresult& aErrorCode);
  PRBool SkipDeclaration(nsresult& aErrorCode, PRBool aCheckForBraces);

  PRBool PushGroup(nsICSSGroupRule* aRule);
  void PopGroup(void);

  PRBool ParseRuleSet(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseAtRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseCharsetRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseImportRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool GatherURL(nsresult& aErrorCode, nsString& aURL);
  PRBool GatherMedia(nsresult& aErrorCode, nsISupportsArray* aMediaAtoms);
  PRBool ProcessImport(nsresult& aErrorCode,
                       const nsString& aURLSpec,
                       nsISupportsArray* aMedia,
                       RuleAppendFunc aAppendFunc,
                       void* aProcessData);
  PRBool ParseGroupRule(nsresult& aErrorCode, nsICSSGroupRule* aRule, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseMediaRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseMozDocumentRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseNameSpaceRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ProcessNameSpace(nsresult& aErrorCode, const nsString& aPrefix, 
                          const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                          void* aProcessData);
  PRBool ParseFontFaceRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParsePageRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);

  void ParseIDSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode);
  void ParseClassSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode);
  void ParsePseudoSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode,
                         PRBool aIsNegated);
  void ParseAttributeSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode);

  void ParseTypeOrUniversalSelector(PRInt32&  aDataMask,
                         nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode,
                         PRBool aIsNegated);
  void ParseNegatedSimpleSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, nsresult& aErrorCode);
  void ParseLangSelector(nsCSSSelector& aSelector, PRInt32& aParsingStatus,
                         nsresult& aErrorCode);

  PRBool ParseSelectorList(nsresult& aErrorCode, nsCSSSelectorList*& aListHead);
  PRBool ParseSelectorGroup(nsresult& aErrorCode, nsCSSSelectorList*& aListHead);
  PRBool ParseSelector(nsresult& aErrorCode, nsCSSSelector& aSelectorResult);
  nsCSSDeclaration* ParseDeclarationBlock(nsresult& aErrorCode,
                                           PRBool aCheckForBraces);
  PRBool ParseDeclaration(nsresult& aErrorCode,
                          nsCSSDeclaration* aDeclaration,
                          PRBool aCheckForBraces,
                          PRBool* aChanged);
  // After a parse error parsing |aPropID|, clear the data in
  // |mTempData|.
  void ClearTempData(nsCSSProperty aPropID);
  // After a successful parse of |aPropID|, transfer data from
  // |mTempData| to |mData|.  Set |*aChanged| to true if something
  // changed, but leave it unmodified otherwise.
  void TransferTempData(nsCSSDeclaration* aDeclaration,
                        nsCSSProperty aPropID, PRBool aIsImportant,
                        PRBool* aChanged);
  void DoTransferTempData(nsCSSDeclaration* aDeclaration,
                          nsCSSProperty aPropID, PRBool aIsImportant,
                          PRBool* aChanged);
  PRBool ParseProperty(nsresult& aErrorCode, nsCSSProperty aPropID);
  PRBool ParseSingleValueProperty(nsresult& aErrorCode, nsCSSValue& aValue, 
                                  nsCSSProperty aPropID);

#ifdef MOZ_XUL
  PRBool ParseTreePseudoElement(nsresult& aErrorCode, nsCSSSelector& aSelector);
#endif

  // Property specific parsing routines
  PRBool ParseAzimuth(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseBackground(nsresult& aErrorCode);
  PRBool ParseBackgroundPosition(nsresult& aErrorCode);
  PRBool ParseBorderColor(nsresult& aErrorCode);
  PRBool ParseBorderColors(nsresult& aErrorCode,
                           nsCSSValueList** aResult,
                           nsCSSProperty aProperty);
  PRBool ParseBorderSpacing(nsresult& aErrorCode);
  PRBool ParseBorderSide(nsresult& aErrorCode,
                         const nsCSSProperty aPropIDs[],
                         PRBool aSetAllSides);
  PRBool ParseBorderStyle(nsresult& aErrorCode);
  PRBool ParseBorderWidth(nsresult& aErrorCode);
  PRBool ParseBorderRadius(nsresult& aErrorCode);
#ifdef ENABLE_OUTLINE
  PRBool ParseOutlineRadius(nsresult& aErrorCode);
#endif
  // for 'clip' and '-moz-image-region'
  PRBool ParseRect(nsCSSRect& aRect, nsresult& aErrorCode,
                   nsCSSProperty aPropID);
  PRBool DoParseRect(nsCSSRect& aRect, nsresult& aErrorCode);
  PRBool ParseContent(nsresult& aErrorCode);
  PRBool SetSingleCounterValue(nsCSSCounterData** aResult,
                               nsresult& aErrorCode,
                               nsCSSProperty aPropID,
                               const nsCSSValue& aValue);
  PRBool ParseCounterData(nsresult& aErrorCode,
                          nsCSSCounterData** aResult,
                          nsCSSProperty aPropID);
  PRBool ParseCue(nsresult& aErrorCode);
  PRBool ParseCursor(nsresult& aErrorCode);
  PRBool ParseFont(nsresult& aErrorCode);
  PRBool ParseFontWeight(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseFamily(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseListStyle(nsresult& aErrorCode);
  PRBool ParseMargin(nsresult& aErrorCode);
  PRBool ParseMarks(nsresult& aErrorCode, nsCSSValue& aValue);
#ifdef ENABLE_OUTLINE
  PRBool ParseOutline(nsresult& aErrorCode);
#endif
  PRBool ParseOverflow(nsresult& aErrorCode);
  PRBool ParsePadding(nsresult& aErrorCode);
  PRBool ParsePause(nsresult& aErrorCode);
  PRBool ParseQuotes(nsresult& aErrorCode);
  PRBool ParseSize(nsresult& aErrorCode);
  PRBool ParseTextDecoration(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseTextShadow(nsresult& aErrorCode);

  // Reused utility parsing routines
  void AppendValue(nsCSSProperty aPropID, const nsCSSValue& aValue);
  PRBool ParseBoxProperties(nsresult& aErrorCode, nsCSSRect& aResult,
                            const nsCSSProperty aPropIDs[]);
  PRBool ParseDirectionalBoxProperty(nsresult& aErrorCode,
                                     nsCSSProperty aProperty,
                                     PRInt32 aSourceType);
  PRInt32 ParseChoice(nsresult& aErrorCode, nsCSSValue aValues[],
                      const nsCSSProperty aPropIDs[], PRInt32 aNumIDs);
  PRBool ParseColor(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseColorComponent(nsresult& aErrorCode, PRUint8& aComponent,
                             PRInt32& aType, char aStop);
  // ParseHSLColor parses everything starting with the opening '(' up through
  // and including the aStop char.
  PRBool ParseHSLColor(nsresult& aErrorCode, nscolor& aColor, char aStop);
  // ParseColorOpacity will enforce that the color ends with a ')' after the opacity
  PRBool ParseColorOpacity(nsresult& aErrorCode, PRUint8& aOpacity);
  PRBool ParseEnum(nsresult& aErrorCode, nsCSSValue& aValue, const PRInt32 aKeywordTable[]);
  PRBool ParseVariant(nsresult& aErrorCode, nsCSSValue& aValue,
                      PRInt32 aVariantMask,
                      const PRInt32 aKeywordTable[]);
  PRBool ParsePositiveVariant(nsresult& aErrorCode, nsCSSValue& aValue, 
                              PRInt32 aVariantMask, 
                              const PRInt32 aKeywordTable[]); 
  PRBool ParseCounter(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseAttr(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool ParseURL(nsresult& aErrorCode, nsCSSValue& aValue);
  PRBool TranslateDimension(nsresult& aErrorCode, nsCSSValue& aValue, PRInt32 aVariantMask,
                            float aNumber, const nsString& aUnit);

  void SetParsingCompoundProperty(PRBool aBool) {
    mParsingCompoundProperty = aBool;
  }
  PRBool IsParsingCompoundProperty(void) {
    return mParsingCompoundProperty;
  }

  // Current token. The value is valid after calling GetToken
  nsCSSToken mToken;

  // Our scanner.  We own this and are responsible for deallocating it.
  nsCSSScanner* mScanner;

  // The URI to be used as a base for relative URIs.
  nsCOMPtr<nsIURI> mBaseURL;

  // The sheet we're parsing into
  nsCOMPtr<nsICSSStyleSheet> mSheet;

  // Used for @import rules
  nsICSSLoader* mChildLoader; // not ref counted, it owns us

  // Sheet section we're in.  This is used to enforce correct ordering of the
  // various rule types (eg the fact that a @charset rule must come before
  // anything else).
  enum nsCSSSection { 
    eCSSSection_Charset, 
    eCSSSection_Import, 
    eCSSSection_NameSpace,
    eCSSSection_General 
  };
  nsCSSSection  mSection;

  nsCOMPtr<nsINameSpace> mNameSpace;

  // After an UngetToken is done this flag is true. The next call to
  // GetToken clears the flag.
  PRPackedBool mHavePushBack;

  // True if we are in quirks mode; false in standards or almost standards mode
  PRPackedBool  mNavQuirkMode;

#ifdef MOZ_SVG
  // True if we are in SVG mode; false in "normal" CSS
  PRPackedBool  mSVGMode;
#endif

  // True if tagnames and attributes are case-sensitive
  PRPackedBool  mCaseSensitive;

  // This flag is set when parsing a non-box shorthand; it's used to not apply
  // some quirks during shorthand parsing
  PRPackedBool  mParsingCompoundProperty;

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
};

PR_STATIC_CALLBACK(void) AppendRuleToArray(nsICSSRule* aRule, void* aArray)
{
  nsISupportsArray* arr = (nsISupportsArray*) aArray;
  arr->AppendElement(aRule);
}

PR_STATIC_CALLBACK(void) AppendRuleToSheet(nsICSSRule* aRule, void* aParser)
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
  ReportUnexpected(mScanner, #msg_)

#define REPORT_UNEXPECTED_P(msg_, params_) \
  ReportUnexpectedParams(mScanner, #msg_, params_, NS_ARRAY_LENGTH(params_))

#define REPORT_UNEXPECTED_EOF(lf_) \
  ReportUnexpectedEOF(mScanner, #lf_)

#define REPORT_UNEXPECTED_TOKEN(msg_) \
  ReportUnexpectedToken(mScanner, mToken, #msg_)

#define REPORT_UNEXPECTED_TOKEN_P(msg_, params_) \
  ReportUnexpectedTokenParams(mScanner,  mToken, #msg_, \
                              params_, NS_ARRAY_LENGTH(params_))


#define OUTPUT_ERROR() \
  mScanner->OutputError()

#define CLEAR_ERROR() \
  mScanner->ClearError()

static nsIStringBundle *gStringBundle;

static PRBool InitStringBundle()
{
  if (gStringBundle)
    return PR_TRUE;

  nsCOMPtr<nsIStringBundleService> sbs =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!sbs)
    return PR_FALSE;

  nsresult rv = 
    sbs->CreateBundle("chrome://global/locale/css.properties", &gStringBundle);
  if (NS_FAILED(rv)) {
    gStringBundle = nsnull;
    return PR_FALSE;
  }

  return PR_TRUE;
}

#define ENSURE_STRINGBUNDLE \
  PR_BEGIN_MACRO if (!InitStringBundle()) return; PR_END_MACRO

// aMessage must take no parameters
static void ReportUnexpected(nsCSSScanner *sc, const char* aMessage)
{
  ENSURE_STRINGBUNDLE;

  nsXPIDLString str;
  gStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                   getter_Copies(str));
  sc->AddToError(str);
}
  
static void ReportUnexpectedParams(nsCSSScanner *sc, const char* aMessage,
                                   const PRUnichar **aParams,
                                   PRUint32 aParamsLength)
{
  NS_PRECONDITION(aParamsLength > 0, "use the non-params version");
  ENSURE_STRINGBUNDLE;

  nsXPIDLString str;
  gStringBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(aMessage).get(),
                                      aParams, aParamsLength,
                                      getter_Copies(str));
  sc->AddToError(str);
}

// aMessage must take no parameters
static void ReportUnexpectedEOF(nsCSSScanner *sc, const char* aLookingFor)
{
  ENSURE_STRINGBUNDLE;

  nsXPIDLString innerStr;
  gStringBundle->GetStringFromName(NS_ConvertASCIItoUTF16(aLookingFor).get(),
                                   getter_Copies(innerStr));

  const PRUnichar *params[] = {
    innerStr.get()
  };
  nsXPIDLString str;
  gStringBundle->FormatStringFromName(NS_LITERAL_STRING("PEUnexpEOF").get(),
                                      params, NS_ARRAY_LENGTH(params),
                                      getter_Copies(str));
  sc->AddToError(str);
}

// aMessage must take 1 parameter (for the string representation of the
// unexpected token)
static void ReportUnexpectedToken(nsCSSScanner *sc, nsCSSToken& tok,
                                  const char *aMessage)
{
  ENSURE_STRINGBUNDLE;
  
  nsAutoString tokenString;
  tok.AppendToString(tokenString);

  const PRUnichar *params[] = {
    tokenString.get()
  };

  ReportUnexpectedParams(sc, aMessage, params, NS_ARRAY_LENGTH(params));
}

// aParams's first entry must be null, and we'll fill in the token
static void ReportUnexpectedTokenParams(nsCSSScanner *sc, nsCSSToken& tok,
                                        const char* aMessage,
                                        const PRUnichar **aParams,
                                        PRUint32 aParamsLength)
{
  NS_PRECONDITION(aParamsLength > 1, "use the non-params version");
  NS_PRECONDITION(aParams[0] == nsnull, "first param should be empty");

  ENSURE_STRINGBUNDLE;
  
  nsAutoString tokenString;
  tok.AppendToString(tokenString);
  aParams[0] = tokenString.get();

  ReportUnexpectedParams(sc, aMessage, aParams, aParamsLength);
}

#else

#define REPORT_UNEXPECTED(msg_)
#define REPORT_UNEXPECTED_P(msg_, params_)
#define REPORT_UNEXPECTED_EOF(lf_)
#define REPORT_UNEXPECTED_TOKEN(msg_)
#define REPORT_UNEXPECTED_TOKEN_P(msg_, params_)
#define OUTPUT_ERROR()
#define CLEAR_ERROR()

#endif

NS_HIDDEN_(void) NS_ShutdownCSSParser()
{
#ifdef CSS_REPORT_PARSE_ERRORS
  NS_IF_RELEASE(gStringBundle);
#endif
}


CSSParserImpl::CSSParserImpl()
  : mToken(),
    mScanner(nsnull),
    mChildLoader(nsnull),
    mSection(eCSSSection_Charset),
    mHavePushBack(PR_FALSE),
    mNavQuirkMode(PR_FALSE),
#ifdef MOZ_SVG
    mSVGMode(PR_FALSE),
#endif
    mCaseSensitive(PR_FALSE),
    mParsingCompoundProperty(PR_FALSE)
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
  NS_PRECONDITION(nsnull != aSheet, "null ptr");
  if (nsnull == aSheet) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aSheet != mSheet) {
    // Switch to using the new sheet
    mGroupStack.Clear();
    mSheet = aSheet;
    mSheet->GetNameSpace(*getter_AddRefs(mNameSpace));
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::SetCaseSensitive(PRBool aCaseSensitive)
{
  mCaseSensitive = aCaseSensitive;
  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::SetQuirkMode(PRBool aQuirkMode)
{
  mNavQuirkMode = aQuirkMode;
  return NS_OK;
}

#ifdef MOZ_SVG
NS_IMETHODIMP
CSSParserImpl::SetSVGMode(PRBool aSVGMode)
{
  mSVGMode = aSVGMode;
  return NS_OK;
}
#endif

NS_IMETHODIMP
CSSParserImpl::SetChildLoader(nsICSSLoader* aChildLoader)
{
  mChildLoader = aChildLoader;  // not ref counted, it owns us
  return NS_OK;
}

nsresult
CSSParserImpl::InitScanner(nsIUnicharInputStream* aInput, nsIURI* aSheetURI,
                           PRUint32 aLineNumber, nsIURI* aBaseURI)
{
  NS_ASSERTION(! mScanner, "already have scanner");

  mScanner = new nsCSSScanner();
  if (! mScanner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->Init(aInput, aSheetURI, aLineNumber);
  mBaseURL = aBaseURI;

  mHavePushBack = PR_FALSE;

  return NS_OK;
}

nsresult
CSSParserImpl::ReleaseScanner(void)
{
  if (mScanner) {
    delete mScanner;
    mScanner = nsnull;
  }
  mBaseURL = nsnull;
  return NS_OK;
}


NS_IMETHODIMP
CSSParserImpl::Parse(nsIUnicharInputStream* aInput,
                     nsIURI*                aSheetURI,
                     nsIURI*                aBaseURI,
                     PRUint32               aLineNumber,
                     nsICSSStyleSheet*&     aResult)
{
  NS_ASSERTION(nsnull != aBaseURI, "need base URL");
  NS_ASSERTION(nsnull != aSheetURI, "need sheet URL");

  if (! mSheet) {
    NS_NewCSSStyleSheet(getter_AddRefs(mSheet));
    mSheet->SetURIs(aSheetURI, aBaseURI);
  }
#ifdef DEBUG
  else {
    nsCOMPtr<nsIURI> uri;
    mSheet->GetSheetURI(getter_AddRefs(uri));
    PRBool equal;
    aSheetURI->Equals(uri, &equal);
    NS_ASSERTION(equal, "Sheet URI does not match passed URI");
  }
#endif
  
  if (! mSheet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult errorCode = NS_OK;

  nsresult result = InitScanner(aInput, aSheetURI, aLineNumber, aBaseURI);
  if (! NS_SUCCEEDED(result)) {
    return result;
  }

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

  nsCSSToken* tk = &mToken;
  for (;;) {
    // Get next non-whitespace token
    if (!GetToken(errorCode, PR_TRUE)) {
      OUTPUT_ERROR();
      break;
    }
    if (eCSSToken_HTMLComment == tk->mType) {
      continue; // legal here only
    }
    if (eCSSToken_AtKeyword == tk->mType) {
      ParseAtRule(errorCode, AppendRuleToSheet, this);
      continue;
    }
    UngetToken();
    if (ParseRuleSet(errorCode, AppendRuleToSheet, this)) {
      mSection = eCSSSection_General;
    }
  }
  ReleaseScanner();

  aResult = mSheet;
  NS_ADDREF(aResult);

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseStyleAttribute(const nsAString& aAttributeValue,
                                   nsIURI*                  aDocURL,
                                   nsIURI*                  aBaseURL,
                                   nsICSSStyleRule**        aResult)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

  // XXXldb XXXperf nsIUnicharInputStream is horrible!  It makes us make
  // a copy.
  nsString* str = new nsAutoString(aAttributeValue);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIUnicharInputStream* input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(&input, str);
  if (NS_OK != rv) {
    delete str;
    return rv;
  }

  rv = InitScanner(input, aDocURL, 0, aBaseURL); // XXX line number
  NS_RELEASE(input);
  if (! NS_SUCCEEDED(rv)) {
    return rv;
  }

  mSection = eCSSSection_General;
  nsresult errorCode = NS_OK;

  // In quirks mode, allow style declarations to have braces or not
  // (bug 99554).
  PRBool haveBraces;
  if (mNavQuirkMode) {
    GetToken(errorCode, PR_TRUE);
    haveBraces = eCSSToken_Symbol == mToken.mType &&
                 '{' == mToken.mSymbol;
    UngetToken();
  }
  else {
    haveBraces = PR_FALSE;
  }

  nsCSSDeclaration* declaration = ParseDeclarationBlock(errorCode, haveBraces);
  if (declaration) {
    // Create a style rule for the delcaration
    nsICSSStyleRule* rule = nsnull;
    rv = NS_NewCSSStyleRule(&rule, nsnull, declaration);
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

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseAndAppendDeclaration(const nsAString&  aBuffer,
                                         nsIURI*           aSheetURL,
                                         nsIURI*           aBaseURL,
                                         nsCSSDeclaration* aDeclaration,
                                         PRBool            aParseOnlyOneDecl,
                                         PRBool*           aChanged,
                                         PRBool            aClearOldDecl)
{
//  NS_ASSERTION(nsnull != aBaseURL, "need base URL");
  *aChanged = PR_FALSE;

  nsString* str = new nsString(aBuffer);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIUnicharInputStream* input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(&input, str);
  if (NS_OK != rv) {
    delete str;
    return rv;
  }

  rv = InitScanner(input, aSheetURL, 0, aBaseURL);
  NS_RELEASE(input);
  if (! NS_SUCCEEDED(rv)) {
    return rv;
  }

  mSection = eCSSSection_General;
  nsresult errorCode = NS_OK;

  if (aClearOldDecl) {
    mData.AssertInitialState();
    aDeclaration->ClearData();
    // We could check if it was already empty, but...
    *aChanged = PR_TRUE;
  } else {
    aDeclaration->ExpandTo(&mData);
  }

  do {
    if (!ParseDeclaration(errorCode, aDeclaration, PR_FALSE, aChanged)) {
      NS_ASSERTION(errorCode != nsresult(-1), "-1 is no longer used for EOF");
      rv = errorCode;

      if (NS_FAILED(errorCode))
        break;

      if (!SkipDeclaration(errorCode, PR_FALSE)) {
        NS_ASSERTION(errorCode != nsresult(-1), "-1 is no longer used for EOF");
        rv = errorCode;
        break;
      }
    }
  } while (!aParseOnlyOneDecl);
  aDeclaration->CompressFrom(&mData);

  ReleaseScanner();
  return rv;
}

NS_IMETHODIMP
CSSParserImpl::ParseRule(const nsAString& aRule,
                         nsIURI*            aSheetURL,
                         nsIURI*            aBaseURL,
                         nsISupportsArray** aResult)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");
  NS_ENSURE_ARG_POINTER(aResult);

  nsString* str = new nsString(aRule);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIUnicharInputStream> input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(getter_AddRefs(input), str);
  if (NS_FAILED(rv)) {
    delete str;
    return rv;
  }

  rv = InitScanner(input, aSheetURL, 0, aBaseURL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_NewISupportsArray(aResult);
  if (NS_FAILED(rv)) {
    ReleaseScanner();
    return rv;
  }
  
  mSection = eCSSSection_Charset; // callers are responsible for rejecting invalid rules.
  nsresult errorCode = NS_OK;

  nsCSSToken* tk = &mToken;
  // Get first non-whitespace token
  if (!GetToken(errorCode, PR_TRUE)) {
    REPORT_UNEXPECTED(PEParseRuleWSOnly);
    OUTPUT_ERROR();
  } else if (eCSSToken_AtKeyword == tk->mType) {
    ParseAtRule(errorCode, AppendRuleToArray, *aResult);    
  }
  else {
    UngetToken();
    ParseRuleSet(errorCode, AppendRuleToArray, *aResult);
  }
  OUTPUT_ERROR();
  ReleaseScanner();
  return NS_OK;
}

//XXXbz this function does not deal well with something like "foo
//!important" as the aPropValue.  It will parse the "foo" and set it
//in the decl, then ignore the !important.  It should either fail to
//parse this or do !important correctly....
NS_IMETHODIMP
CSSParserImpl::ParseProperty(const nsCSSProperty aPropID,
                             const nsAString& aPropValue,
                             nsIURI* aSheetURL,
                             nsIURI* aBaseURL,
                             nsCSSDeclaration* aDeclaration,
                             PRBool* aChanged)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");
  NS_ASSERTION(nsnull != aDeclaration, "Need declaration to parse into!");
  *aChanged = PR_FALSE;

  nsString* str = new nsAutoString(aPropValue);
  if (!str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIUnicharInputStream> input;
  nsresult rv = NS_NewStringUnicharInputStream(getter_AddRefs(input), str);
  if (NS_FAILED(rv)) {
    delete str;
    return rv;
  }

  rv = InitScanner(input, aSheetURL, 0, aBaseURL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSection = eCSSSection_General;
  nsresult errorCode = NS_OK;

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
  if (ParseProperty(errorCode, aPropID)) {
    TransferTempData(aDeclaration, aPropID, PR_FALSE, aChanged);
  } else {
    NS_ConvertASCIItoUTF16 propName(nsCSSProps::GetStringValue(aPropID));
    const PRUnichar *params[] = {
      propName.get()
    };
    REPORT_UNEXPECTED_P(PEPropertyParsingError, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ClearTempData(aPropID);
    NS_ASSERTION(errorCode != nsresult(-1), "-1 is no longer used for EOF");
    result = errorCode;
  }
  CLEAR_ERROR();
  
  aDeclaration->CompressFrom(&mData);
  
  ReleaseScanner();
  return result;
}
//----------------------------------------------------------------------

PRBool CSSParserImpl::GetToken(nsresult& aErrorCode, PRBool aSkipWS)
{
  for (;;) {
    if (!mHavePushBack) {
      if (!mScanner->Next(aErrorCode, mToken)) {
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

PRBool CSSParserImpl::GetURLToken(nsresult& aErrorCode, PRBool aSkipWS)
{
  for (;;) {
    if (! mHavePushBack) {
      if (! mScanner->NextURL(aErrorCode, mToken)) {
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

void CSSParserImpl::UngetToken()
{
  NS_PRECONDITION(mHavePushBack == PR_FALSE, "double pushback");
  mHavePushBack = PR_TRUE;
}

PRBool CSSParserImpl::ExpectSymbol(nsresult& aErrorCode,
                                   PRUnichar aSymbol,
                                   PRBool aSkipWS)
{
  if (!GetToken(aErrorCode, aSkipWS)) {
    return PR_FALSE;
  }
  if (mToken.IsSymbol(aSymbol)) {
    return PR_TRUE;
  }
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::ExpectEndProperty(nsresult& aErrorCode, PRBool aSkipWS)
{
  if (!GetToken(aErrorCode, aSkipWS)) {
    return PR_TRUE; // properties may end with eof
  }
  if ((eCSSToken_Symbol == mToken.mType) &&
      ((';' == mToken.mSymbol) || ('!' == mToken.mSymbol) || ('}' == mToken.mSymbol))) {
    // XXX need to verify that ! is only followed by "important [;|}]
    // XXX this requires a multi-token pushback buffer
    UngetToken();
    return PR_TRUE;
  }
  REPORT_UNEXPECTED_TOKEN(PEExpectEndProperty);
  UngetToken();
  return PR_FALSE;
}


nsString* CSSParserImpl::NextIdent(nsresult& aErrorCode)
{
  // XXX Error reporting?
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return nsnull;
  }
  if (eCSSToken_Ident != mToken.mType) {
    UngetToken();
    return nsnull;
  }
  return &mToken.mIdent;
}

PRBool CSSParserImpl::SkipAtRule(nsresult& aErrorCode)
{
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PESkipAtRuleEOF);
      return PR_FALSE;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      PRUnichar symbol = mToken.mSymbol;
      if (symbol == ';') {
        break;
      }
      if (symbol == '{') {
        SkipUntil(aErrorCode, '}');
        break;
      } else if (symbol == '(') {
        SkipUntil(aErrorCode, ')');
      } else if (symbol == '[') {
        SkipUntil(aErrorCode, ']');
      }
    }
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseAtRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc,
                                  void* aData)
{
  if ((mSection <= eCSSSection_Charset) && 
      (mToken.mIdent.LowerCaseEqualsLiteral("charset"))) {
    if (ParseCharsetRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_Import;  // only one charset allowed
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_Import) && 
      mToken.mIdent.LowerCaseEqualsLiteral("import")) {
    if (ParseImportRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_Import;
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_NameSpace) && 
      mToken.mIdent.LowerCaseEqualsLiteral("namespace")) {
    if (ParseNameSpaceRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_NameSpace;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("media")) {
    if (ParseMediaRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-document")) {
    if (ParseMozDocumentRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("font-face")) {
    if (ParseFontFaceRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.LowerCaseEqualsLiteral("page")) {
    if (ParsePageRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  REPORT_UNEXPECTED_TOKEN(PEUnknownAtRule);
  OUTPUT_ERROR();

  // Skip over unsupported at rule, don't advance section
  return SkipAtRule(aErrorCode);
}

PRBool CSSParserImpl::ParseCharsetRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc,
                                       void* aData)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PECharsetRuleEOF);
    return PR_FALSE;
  }

  if (eCSSToken_String != mToken.mType) {
    REPORT_UNEXPECTED_TOKEN(PECharsetRuleNotString);
    return PR_FALSE;
  }

  nsAutoString charset = mToken.mIdent;
  
  if (!ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsICSSRule> rule;
  NS_NewCSSCharsetRule(getter_AddRefs(rule), charset);

  if (rule) {
    (*aAppendFunc)(rule, aData);
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::GatherURL(nsresult& aErrorCode, nsString& aURL)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_String == mToken.mType) {
    aURL = mToken.mIdent;
    return PR_TRUE;
  }
  else if (eCSSToken_Function == mToken.mType && 
           mToken.mIdent.LowerCaseEqualsLiteral("url") &&
           ExpectSymbol(aErrorCode, '(', PR_FALSE) &&
           GetURLToken(aErrorCode, PR_TRUE) &&
           (eCSSToken_String == mToken.mType ||
            eCSSToken_URL == mToken.mType)) {
    aURL = mToken.mIdent;
    if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::GatherMedia(nsresult& aErrorCode,
                                  nsISupportsArray* aMediaAtoms)
{
  PRBool expectIdent = PR_TRUE;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEGatherMediaEOF);
      break;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      PRUnichar symbol = mToken.mSymbol;
      if ((';' == symbol) || ('{' == symbol)) {
        UngetToken();
        return PR_TRUE;
      } else if (',' != symbol) {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotComma);
        UngetToken();
        break;
      } else if (expectIdent) {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotIdent);
        UngetToken();
        break;
      }
      else {
        expectIdent = PR_TRUE;
      }
    }
    else if (eCSSToken_Ident == mToken.mType) {
      if (expectIdent) {
        ToLowerCase(mToken.mIdent);  // case insensitive from CSS - must be lower cased
        nsCOMPtr<nsIAtom> medium = do_GetAtom(mToken.mIdent);
        aMediaAtoms->AppendElement(medium);
        expectIdent = PR_FALSE;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotComma);
        UngetToken();
        break;
      }
    }
    else {
      if (expectIdent)
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotIdent);
      else
        REPORT_UNEXPECTED_TOKEN(PEGatherMediaNotComma);
      UngetToken();
      break;
    }
  }
  aMediaAtoms->Clear();
  return PR_FALSE;
}

// Parse a CSS2 import rule: "@import STRING | URL [medium [, mdeium]]"
PRBool CSSParserImpl::ParseImportRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  nsAutoString url;
  nsCOMPtr<nsISupportsArray> media;
  aErrorCode = NS_NewISupportsArray(getter_AddRefs(media));
  if (!media) {
    // Out of memory
    return PR_FALSE;
  }

  if (!GatherURL(aErrorCode, url)) {
    REPORT_UNEXPECTED_TOKEN(PEImportNotURI);
    return PR_FALSE;
  }
  
  if (!GatherMedia(aErrorCode, media) ||
      !ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEImportUnexpected);
    // don't advance section, simply ignore invalid @import
    return PR_FALSE;
  }

  ProcessImport(aErrorCode, url, media, aAppendFunc, aData);
  return PR_TRUE;
}


PRBool CSSParserImpl::ProcessImport(nsresult& aErrorCode,
                                    const nsString& aURLSpec,
                                    nsISupportsArray* aMedia,
                                    RuleAppendFunc aAppendFunc,
                                    void* aData)
{
  nsCOMPtr<nsICSSImportRule> rule;
  aErrorCode = NS_NewCSSImportRule(getter_AddRefs(rule), aURLSpec, aMedia);
  if (NS_FAILED(aErrorCode)) {
    return PR_FALSE;
  }
  (*aAppendFunc)(rule, aData);

  if (mChildLoader) {
    nsCOMPtr<nsIURI> url;
    // XXX should pass a charset!
    aErrorCode = NS_NewURI(getter_AddRefs(url), aURLSpec, nsnull, mBaseURL);

    if (NS_FAILED(aErrorCode)) {
      // import url is bad
      // XXX log this somewhere for easier web page debugging
      return PR_FALSE;
    }

    mChildLoader->LoadChildSheet(mSheet, url, aMedia, rule);
  }
  
  return PR_TRUE;
}

// Parse the {} part of an @media or @-moz-document rule.
PRBool CSSParserImpl::ParseGroupRule(nsresult& aErrorCode,
                                     nsICSSGroupRule* aRule,
                                     RuleAppendFunc aAppendFunc,
                                     void* aData)
{
  // XXXbz this could use better error reporting throughout the method
  if (!ExpectSymbol(aErrorCode, '{', PR_TRUE)) {
    return PR_FALSE;
  }

  // push rule on stack, loop over children
  if (!PushGroup(aRule)) {
    aErrorCode = NS_ERROR_OUT_OF_MEMORY;
    return PR_FALSE;
  }
  nsCSSSection holdSection = mSection;
  mSection = eCSSSection_General;

  for (;;) {
    // Get next non-whitespace token
    if (! GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEGroupRuleEOF);
      break;
    }
    if (mToken.IsSymbol('}')) { // done!
      UngetToken();
      break;
    }
    if (eCSSToken_AtKeyword == mToken.mType) {
      SkipAtRule(aErrorCode); // group rules cannot contain @rules
      continue;
    }
    UngetToken();
    ParseRuleSet(aErrorCode, AppendRuleToSheet, this);
  }
  PopGroup();

  if (!ExpectSymbol(aErrorCode, '}', PR_TRUE)) {
    mSection = holdSection;
    return PR_FALSE;
  }
  (*aAppendFunc)(aRule, aData);
  return PR_TRUE;
}

// Parse a CSS2 media rule: "@media medium [, medium] { ... }"
PRBool CSSParserImpl::ParseMediaRule(nsresult& aErrorCode,
                                     RuleAppendFunc aAppendFunc,
                                     void* aData)
{
  nsCOMPtr<nsISupportsArray> media;
  aErrorCode = NS_NewISupportsArray(getter_AddRefs(media));
  if (media) {
    if (GatherMedia(aErrorCode, media)) {
      // XXXbz this could use better error reporting throughout the method
      PRUint32 count;
      media->Count(&count);
      if (count > 0) {
        nsRefPtr<nsCSSMediaRule> rule(new nsCSSMediaRule());
        // Append first, so when we do SetMedia() the rule
        // knows what its stylesheet is.
        if (rule && ParseGroupRule(aErrorCode, rule, aAppendFunc, aData)) {
          rule->SetMedia(media);
          return PR_TRUE;
        }
      }
    }
  }

  return PR_FALSE;
}

// Parse a @-moz-document rule.  This is like an @media rule, but instead
// of a medium it has a nonempty list of items where each item is either
// url(), url-prefix(), or domain().
PRBool CSSParserImpl::ParseMozDocumentRule(nsresult& aErrorCode,
                                           RuleAppendFunc aAppendFunc,
                                           void* aData)
{
  nsCSSDocumentRule::URL *urls = nsnull;
  nsCSSDocumentRule::URL **next = &urls;
  do {
    if (!GetToken(aErrorCode, PR_TRUE) ||
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
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
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

    if (!ExpectSymbol(aErrorCode, '(', PR_FALSE) ||
        !GetURLToken(aErrorCode, PR_TRUE) ||
        (eCSSToken_String != mToken.mType &&
         eCSSToken_URL != mToken.mType)) {
      REPORT_UNEXPECTED_TOKEN(PEMozDocRuleNotURI);
      delete urls;
      return PR_FALSE;
    }
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      delete urls;
      return PR_FALSE;
    }

    // We could try to make the URL (as long as it's not domain())
    // canonical and absolute with NS_NewURI and GetSpec, but I'm
    // inclined to think we shouldn't.
    CopyUTF16toUTF8(mToken.mIdent, cur->url);
  } while (ExpectSymbol(aErrorCode, ',', PR_TRUE));

  nsRefPtr<nsCSSDocumentRule> rule(new nsCSSDocumentRule());
  if (!rule) {
    aErrorCode = NS_ERROR_OUT_OF_MEMORY;
    delete urls;
    return PR_FALSE;
  }
  rule->SetURLs(urls);

  return ParseGroupRule(aErrorCode, rule, aAppendFunc, aData);
}

// Parse a CSS3 namespace rule: "@namespace [prefix] STRING | URL;"
PRBool CSSParserImpl::ParseNameSpaceRule(nsresult& aErrorCode,
                                         RuleAppendFunc aAppendFunc,
                                         void* aData)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEAtNSPrefixEOF);
    return PR_FALSE;
  }

  nsAutoString  prefix;
  nsAutoString  url;

  if (eCSSToken_Ident == mToken.mType) {
    prefix = mToken.mIdent;
    ToLowerCase(prefix); // always case insensitive, since stays within CSS
    if (! GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PEAtNSURIEOF);
      return PR_FALSE;
    }
  }

  if (eCSSToken_String == mToken.mType) {
    url = mToken.mIdent;
    if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
      ProcessNameSpace(aErrorCode, prefix, url, aAppendFunc, aData);
      return PR_TRUE;
    }
  }
  else if ((eCSSToken_Function == mToken.mType) && 
           (mToken.mIdent.LowerCaseEqualsLiteral("url"))) {
    if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
      if (GetURLToken(aErrorCode, PR_TRUE)) {
        if ((eCSSToken_String == mToken.mType) || (eCSSToken_URL == mToken.mType)) {
          url = mToken.mIdent;
          if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
            if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
              ProcessNameSpace(aErrorCode, prefix, url, aAppendFunc, aData);
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

PRBool CSSParserImpl::ProcessNameSpace(nsresult& aErrorCode, const nsString& aPrefix, 
                                       const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
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
    mSheet->GetNameSpace(*getter_AddRefs(mNameSpace));
  }

  return result;
}

PRBool CSSParserImpl::ParseFontFaceRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

PRBool CSSParserImpl::ParsePageRule(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

void CSSParserImpl::SkipUntil(nsresult& aErrorCode, PRUnichar aStopSymbol)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
      if (symbol == aStopSymbol) {
        break;
      } else if ('{' == symbol) {
        SkipUntil(aErrorCode, '}');
      } else if ('[' == symbol) {
        SkipUntil(aErrorCode, ']');
      } else if ('(' == symbol) {
        SkipUntil(aErrorCode, ')');
      }
    }
  }
}

PRBool
CSSParserImpl::SkipDeclaration(nsresult& aErrorCode, PRBool aCheckForBraces)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
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
        SkipUntil(aErrorCode, '}');
      } else if ('(' == symbol) {
        SkipUntil(aErrorCode, ')');
      } else if ('[' == symbol) {
        SkipUntil(aErrorCode, ']');
      }
    }
  }
  return PR_TRUE;
}

void CSSParserImpl::SkipRuleSet(nsresult& aErrorCode)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PESkipRSBraceEOF);
      break;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
      if ('{' == symbol) {
        SkipUntil(aErrorCode, '}');
        break;
      }
      if ('(' == symbol) {
        SkipUntil(aErrorCode, ')');
      } else if ('[' == symbol) {
        SkipUntil(aErrorCode, ']');
      }
    }
  }
}

PRBool CSSParserImpl::PushGroup(nsICSSGroupRule* aRule)
{
  if (mGroupStack.AppendObject(aRule))
    return PR_TRUE;

  return PR_FALSE;
}

void CSSParserImpl::PopGroup(void)
{
  PRInt32 count = mGroupStack.Count();
  if (0 < count) {
    mGroupStack.RemoveObjectAt(count - 1);
  }
}

void CSSParserImpl::AppendRule(nsICSSRule* aRule)
{
  PRInt32 count = mGroupStack.Count();
  if (0 < count) {
    mGroupStack[count - 1]->AppendStyleRule(aRule);
  }
  else {
    mSheet->AppendStyleRule(aRule);
  }
}

PRBool CSSParserImpl::ParseRuleSet(nsresult& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // First get the list of selectors for the rule
  nsCSSSelectorList* slist = nsnull;
  PRUint32 linenum = mScanner->GetLineNumber();
  if (! ParseSelectorList(aErrorCode, slist)) {
    REPORT_UNEXPECTED(PEBadSelectorRSIgnored);
    OUTPUT_ERROR();
    SkipRuleSet(aErrorCode);
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != slist, "null selector list");
  CLEAR_ERROR();

  // Next parse the declaration block
  nsCSSDeclaration* declaration = ParseDeclarationBlock(aErrorCode, PR_TRUE);
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
    aErrorCode = NS_ERROR_OUT_OF_MEMORY;
    delete slist;
    return PR_FALSE;
  }
  rule->SetLineNumber(linenum);
  (*aAppendFunc)(rule, aData);

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseSelectorList(nsresult& aErrorCode,
                                        nsCSSSelectorList*& aListHead)
{
  nsCSSSelectorList* list = nsnull;
  if (! ParseSelectorGroup(aErrorCode, list)) {
    // must have at least one selector group
    aListHead = nsnull;
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != list, "no selector list");
  aListHead = list;

  // After that there must either be a "," or a "{"
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (! GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF(PESelectorListExtraEOF);
      break;
    }

    if (eCSSToken_Symbol == tk->mType) {
      if (',' == tk->mSymbol) {
        nsCSSSelectorList* newList = nsnull;
        // Another selector group must follow
        if (! ParseSelectorGroup(aErrorCode, newList)) {
          break;
        }
        // add new list to the end of the selector list
        list->mNext = newList;
        list = newList;
        continue;
      } else if ('{' == tk->mSymbol) {
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

PRBool CSSParserImpl::ParseSelectorGroup(nsresult& aErrorCode,
                                         nsCSSSelectorList*& aList)
{
  nsCSSSelectorList* list = nsnull;
  PRUnichar     combinator = PRUnichar(0);
  PRInt32       weight = 0;
  PRBool        havePseudoElement = PR_FALSE;
  PRBool        done = PR_FALSE;
  while (!done) {
    nsCSSSelector selector;
    if (! ParseSelector(aErrorCode, selector)) {
      break;
    }
    if (nsnull == list) {
      list = new nsCSSSelectorList();
      if (nsnull == list) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
    }
    list->AddSelector(selector);
    nsCSSSelector* listSel = list->mSelectors;

    // pull out pseudo elements here
    nsAtomStringList* prevList = nsnull;
    nsAtomStringList* pseudoClassList = listSel->mPseudoClassList;
    while (nsnull != pseudoClassList) {
      if (! nsCSSPseudoClasses::IsPseudoClass(pseudoClassList->mAtom)) {
        havePseudoElement = PR_TRUE;
        if (IsSinglePseudoClass(*listSel)) {  // convert to pseudo element selector
          nsIAtom* pseudoElement = pseudoClassList->mAtom;  // steal ref count
          pseudoClassList->mAtom = nsnull;
          listSel->Reset();
          if (listSel->mNext) {// more to the selector
            listSel->mOperator = PRUnichar('>');
            nsCSSSelector empty;
            list->AddSelector(empty); // leave a blank (universal) selector in the middle
            listSel = list->mSelectors; // use the new one for the pseudo
          }
          listSel->mTag = pseudoElement;
        }
        else {  // append new pseudo element selector
          selector.Reset();
          selector.mTag = pseudoClassList->mAtom; // steal ref count
#ifdef MOZ_XUL
          if (IsTreePseudoElement(selector.mTag)) {
            // Take the remaining "pseudoclasses" that we parsed
            // inside the tree pseudoelement's ()-list, and
            // make our new selector have these pseudoclasses
            // in its pseudoclass list.
            selector.mPseudoClassList = pseudoClassList->mNext;
            pseudoClassList->mNext = nsnull;
          }
#endif
          list->AddSelector(selector);
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
    if (!GetToken(aErrorCode, PR_FALSE)) {
      break;
    }

    // Assume we are done unless we find a combinator here.
    done = PR_TRUE;
    if (eCSSToken_WhiteSpace == mToken.mType) {
      if (!GetToken(aErrorCode, PR_TRUE)) {
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
      UngetToken(); // give it back to selector
    }

    if (havePseudoElement) {
      break;
    }
    else {
      weight += selector.CalcWeight();
    }
  }
  if (!list) {
    REPORT_UNEXPECTED(PESelectorGroupNoSelector);
  }
  if (PRUnichar(0) != combinator) { // no dangling combinators
    if (list) {
      delete list;
    }
    list = nsnull;
    // This should report the problematic combinator
    REPORT_UNEXPECTED(PESelectorGroupExtraCombinator);
  }
  aList = list;
  if (nsnull != list) {
    list->mWeight = weight;
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

#define SELECTOR_PARSING_ENDED_OK       1
#define SELECTOR_PARSING_STOPPED_OK     2
#define SELECTOR_PARSING_STOPPED_ERROR  3

//
// Parses an ID selector #name
//
void CSSParserImpl::ParseIDSelector(PRInt32&  aDataMask,
                                    nsCSSSelector& aSelector,
                                    PRInt32& aParsingStatus,
                                    nsresult& aErrorCode)
{
  if (!mToken.mIdent.IsEmpty()) { // verify is legal ID
    PRUnichar first = mToken.mIdent.First();
    PRUnichar second = 0;
    if (mToken.mIdent.Length() >= 2) {
      second = mToken.mIdent.CharAt(1);
    }
    if (!nsCSSScanner::StartsIdent(first, second,
                                   nsCSSScanner::GetLexTable())) {
      REPORT_UNEXPECTED_TOKEN(PEIDSelNotIdent);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    aDataMask |= SEL_MASK_ID;
    aSelector.AddID(mToken.mIdent);
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PEIDSelEmpty);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  aParsingStatus = SELECTOR_PARSING_ENDED_OK;
}

//
// Parses a class selector .name
//
void CSSParserImpl::ParseClassSelector(PRInt32&  aDataMask,
                                        nsCSSSelector& aSelector,
                                        PRInt32& aParsingStatus,
                                        nsresult& aErrorCode)
{
  if (! GetToken(aErrorCode, PR_FALSE)) { // get ident
    REPORT_UNEXPECTED_EOF(PEClassSelEOF);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  if (eCSSToken_Ident != mToken.mType) {  // malformed selector
    REPORT_UNEXPECTED_TOKEN(PEClassSelNotIdent);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  aDataMask |= SEL_MASK_CLASS;

  aSelector.AddClass(mToken.mIdent);

  aParsingStatus = SELECTOR_PARSING_ENDED_OK;
}

//
// Parse a type element selector or a universal selector
// namespace|type or namespace|* or *|* or *
//
void CSSParserImpl::ParseTypeOrUniversalSelector(PRInt32&  aDataMask,
                                      nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      nsresult& aErrorCode,
                                      PRBool aIsNegated)
{
  nsAutoString buffer;
  if (mToken.IsSymbol('*')) {  // universal element selector, or universal namespace
    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // namespace wildcard

      if (! GetToken(aErrorCode, PR_FALSE)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
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
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // was universal element selector
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
      if (mNameSpace) { // look for default namespace
        nsCOMPtr<nsINameSpace> defaultNameSpace;
        mNameSpace->FindNameSpace(nsnull, getter_AddRefs(defaultNameSpace));
        if (defaultNameSpace) {
          PRInt32 defaultID;
          defaultNameSpace->GetNameSpaceID(&defaultID);
          aSelector.SetNameSpace(defaultID);
        }
      }
      aDataMask |= SEL_MASK_ELEM;
      // don't set any tag in the selector
    }
    if (! GetToken(aErrorCode, PR_FALSE)) {   // premature eof is ok (here!)
      aParsingStatus = SELECTOR_PARSING_STOPPED_OK;
      return;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) {    // element name or namespace name
    buffer = mToken.mIdent; // hang on to ident

    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      PRInt32 nameSpaceID = kNameSpaceID_Unknown;
      if (mNameSpace) {
        ToLowerCase(buffer); // always case insensitive, since stays within CSS
        nsCOMPtr<nsIAtom> prefix = do_GetAtom(buffer);
        mNameSpace->FindNameSpaceID(prefix, &nameSpaceID);
      } // else, no declared namespaces
      if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
        const PRUnichar *params[] = {
          buffer.get()
        };
        REPORT_UNEXPECTED_P(PEUnknownNamespacePrefix, params);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      aSelector.SetNameSpace(nameSpaceID);

      if (! GetToken(aErrorCode, PR_FALSE)) {
        REPORT_UNEXPECTED_EOF(PETypeSelEOF);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
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
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // was element name
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
      if (mNameSpace) { // look for default namespace
        nsCOMPtr<nsINameSpace> defaultNameSpace;
        mNameSpace->FindNameSpace(nsnull, getter_AddRefs(defaultNameSpace));
        if (defaultNameSpace) {
          PRInt32 defaultID;
          defaultNameSpace->GetNameSpaceID(&defaultID);
          aSelector.SetNameSpace(defaultID);
        }
      }
      if (mCaseSensitive) {
        aSelector.SetTag(buffer);
      }
      else {
        ToLowerCase(buffer);
        aSelector.SetTag(buffer);
      }
      aDataMask |= SEL_MASK_ELEM;
    }
    if (! GetToken(aErrorCode, PR_FALSE)) {   // premature eof is ok (here!)
      aParsingStatus = SELECTOR_PARSING_STOPPED_OK;
      return;
    }
  }
  else if (mToken.IsSymbol('|')) {  // No namespace
    aDataMask |= SEL_MASK_NSPACE;
    aSelector.SetNameSpace(kNameSpaceID_None);  // explicit NO namespace

    // get mandatory tag
    if (! GetToken(aErrorCode, PR_FALSE)) {
      REPORT_UNEXPECTED_EOF(PETypeSelEOF);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
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
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (! GetToken(aErrorCode, PR_FALSE)) {   // premature eof is ok (here!)
      aParsingStatus = SELECTOR_PARSING_STOPPED_OK;
      return;
    }
  }
  else {
    // no tag or namespace: implied universal selector
    // set namespace to unknown since it is not specified
    aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
    if (mNameSpace) { // look for default namespace
      nsCOMPtr<nsINameSpace> defaultNameSpace;
      mNameSpace->FindNameSpace(nsnull, getter_AddRefs(defaultNameSpace));
      if (defaultNameSpace) {
        PRInt32 defaultID;
        defaultNameSpace->GetNameSpaceID(&defaultID);
        aSelector.SetNameSpace(defaultID);
      }
    }
  }

  aParsingStatus = SELECTOR_PARSING_ENDED_OK;
  if (aIsNegated) {
    // restore last token read in case of a negated type selector
    UngetToken();
  }
}

//
// Parse attribute selectors [attr], [attr=value], [attr|=value],
// [attr~=value], [attr^=value], [attr$=value] and [attr*=value]
//
void CSSParserImpl::ParseAttributeSelector(PRInt32&  aDataMask,
                                      nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      nsresult& aErrorCode)
{
  if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  PRInt32 nameSpaceID = kNameSpaceID_None;
  nsAutoString  attr;
  if (mToken.IsSymbol('*')) { // wildcard namespace
    nameSpaceID = kNameSpaceID_Unknown;
    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {
      if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
       }
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttSelNoBar);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  }
  else if (mToken.IsSymbol('|')) { // NO namespace
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
      REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (eCSSToken_Ident == mToken.mType) { // attr name
      attr = mToken.mIdent;
    }
    else {
      REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) { // attr name or namespace
    attr = mToken.mIdent; // hang on to it
    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // was a namespace
      nameSpaceID = kNameSpaceID_Unknown;
      if (mNameSpace) {
        ToLowerCase(attr); // always case insensitive, since stays within CSS
        nsCOMPtr<nsIAtom> prefix = do_GetAtom(attr);
        mNameSpace->FindNameSpaceID(prefix, &nameSpaceID);
      } // else, no declared namespaces
      if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
        const PRUnichar *params[] = {
          attr.get()
        };
        REPORT_UNEXPECTED_P(PEUnknownNamespacePrefix, params);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttributeNameEOF);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttributeNameExpected);
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
  }
  else {  // malformed
    REPORT_UNEXPECTED_TOKEN(PEAttributeNameOrNamespaceExpected);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  if (! mCaseSensitive) {
    ToLowerCase(attr);
  }
  if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF(PEAttSelInnerEOF);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
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
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (NS_ATTR_FUNC_SET != func) { // get value
      if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
        REPORT_UNEXPECTED_EOF(PEAttSelValueEOF);
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if ((eCSSToken_Ident == mToken.mType) || (eCSSToken_String == mToken.mType)) {
        nsAutoString  value(mToken.mIdent);
        if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
          REPORT_UNEXPECTED_EOF(PEAttSelCloseEOF);
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
        }
        if (mToken.IsSymbol(']')) {
          PRBool isCaseSensitive = mCaseSensitive;
          if (nameSpaceID == kNameSpaceID_None ||
              nameSpaceID == kNameSpaceID_XHTML) {
            static const char* caseSensitiveHTMLAttribute[] = {
              // list based on http://www.w3.org/TR/REC-html40/index/attributes.html
              "abbr",          "alt",        "label",
              "prompt",        "standby",    "summary",
              "title",         "class",      "archive",
              "cite",          "datetime",   "href",
              "name",          nsnull
            };
            short i = 0;
            const char* htmlAttr;
            while ((htmlAttr = caseSensitiveHTMLAttribute[i++])) {
              if (attr.EqualsIgnoreCase(htmlAttr)) {
                isCaseSensitive = PR_TRUE;
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
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
        }
      }
      else {
        REPORT_UNEXPECTED_TOKEN(PEAttSelBadValue);
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PEAttSelUnexpected);
    UngetToken(); // bad dog, no biscut!
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
   }
   aParsingStatus = SELECTOR_PARSING_ENDED_OK;
}

//
// Parse pseudo-classes and pseudo-elements
//
void CSSParserImpl::ParsePseudoSelector(PRInt32&  aDataMask,
                                      nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      nsresult& aErrorCode,
                                      PRBool aIsNegated)
{
  if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
    REPORT_UNEXPECTED_EOF(PEPseudoSelEOF);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  // First, find out whether we are parsing a CSS3 pseudo-element
  PRBool parsingPseudoElement = PR_FALSE;
  if (mToken.IsSymbol(':')) {
    parsingPseudoElement = PR_TRUE;
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
      REPORT_UNEXPECTED_EOF(PEPseudoSelEOF);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  }

  // Do some sanity-checking on the token
  if (eCSSToken_Ident != mToken.mType && eCSSToken_Function != mToken.mType) {
    // malformed selector
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelBadName);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  // OK, now we know we have an mIdent.  Atomize it.  All the atoms, for
  // pseudo-classes as well as pseudo-elements, start with a single ':'.
  nsAutoString buffer;
  buffer.Append(PRUnichar(':'));
  buffer.Append(mToken.mIdent);
  ToLowerCase(buffer);
  nsCOMPtr<nsIAtom> pseudo = do_GetAtom(buffer);

  // stash away some info about this pseudo so we only have to get it once.
#ifdef MOZ_XUL
  // If a tree pseudo-element is using the function syntax, it will
  // get isTree set here and will pass the check below that only
  // allows functions if they are in our list of things allowed to be
  // functions.  If it is _not_ using the function syntax, isTree will
  // be false, and it will still pass that check.  So the tree
  // pseudo-elements are allowed to be either functions or not, as
  // desired.
  PRBool isTree = (eCSSToken_Function == mToken.mType) &&
                  IsTreePseudoElement(pseudo);
#endif
  PRBool isPseudoElement = nsCSSPseudoElements::IsPseudoElement(pseudo);
  PRBool isAnonBox = nsCSSAnonBoxes::IsAnonBox(pseudo);

  // If it's a function token, it better be on our "ok" list, and if the name
  // is that of a function pseudo it better be a function token
  if ((eCSSToken_Function == mToken.mType) !=
      (
#ifdef MOZ_XUL
       isTree ||
#endif
       nsCSSPseudoClasses::notPseudo == pseudo ||
       nsCSSPseudoClasses::lang == pseudo)) { // There are no other function pseudos
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelNonFunc);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  
  // If it starts with "::", it better be a pseudo-element
  if (parsingPseudoElement &&
      !isPseudoElement &&
      !isAnonBox) {
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelNotPE);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  if (nsCSSPseudoClasses::notPseudo == pseudo) {
    if (aIsNegated) { // :not() can't be itself negated
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelDoubleNot);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    // CSS 3 Negation pseudo-class takes one simple selector as argument
    ParseNegatedSimpleSelector(aDataMask, aSelector, aParsingStatus, aErrorCode);
    if (SELECTOR_PARSING_ENDED_OK != aParsingStatus) {
      return;
    }
  }    
  else if (!parsingPseudoElement &&
           nsCSSPseudoClasses::IsPseudoClass(pseudo)) {
    aDataMask |= SEL_MASK_PCLASS;
    if (nsCSSPseudoClasses::lang == pseudo) {
      ParseLangSelector(aSelector, aParsingStatus, aErrorCode);
    }
    // XXX are there more pseudo classes which accept arguments ?
    else {
      aSelector.AddPseudoClass(pseudo);
    }
    if (SELECTOR_PARSING_ENDED_OK != aParsingStatus) {
      return;
    }
  }
  else if (isPseudoElement || isAnonBox) {
    // Pseudo-element.  Make some more sanity checks.
    
    if (aIsNegated) { // pseudo-elements can't be negated
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelPEInNot);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    // CSS2 pseudo-elements and -moz-tree-* pseudo-elements are allowed
    // to have a single ':' on them.  Others (CSS3+ pseudo-elements and
    // various -moz-* pseudo-elements) must have |parsingPseudoElement|
    // set.
    if (!parsingPseudoElement &&
        !nsCSSPseudoElements::IsCSS2PseudoElement(pseudo)
#ifdef MOZ_XUL
        && !IsTreePseudoElement(pseudo)
#endif
        ) {
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelNewStyleOnly);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;      
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
        if (!ParseTreePseudoElement(aErrorCode, aSelector)) {
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
        }
      }
#endif

      // ensure selector ends here, must be followed by EOF, space, '{' or ','
      if (GetToken(aErrorCode, PR_FALSE)) { // premature eof is ok (here!)
        if ((eCSSToken_WhiteSpace == mToken.mType) || 
            (mToken.IsSymbol('{') || mToken.IsSymbol(','))) {
          UngetToken();
          aParsingStatus = SELECTOR_PARSING_STOPPED_OK;
          return;
        }
        REPORT_UNEXPECTED_TOKEN(PEPseudoSelTrailing);
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // multiple pseudo elements, not legal
      REPORT_UNEXPECTED_TOKEN(PEPseudoSelMultiplePE);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  } else {
    // Not a pseudo-class, not a pseudo-element.... forget it
    REPORT_UNEXPECTED_TOKEN(PEPseudoSelUnknown);
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  aParsingStatus = SELECTOR_PARSING_ENDED_OK;
}

//
// Parse the argument of a negation pseudo-class :not()
//
void CSSParserImpl::ParseNegatedSimpleSelector(PRInt32&  aDataMask,
                                      nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      nsresult& aErrorCode)
{
  // Check if we have the first parenthesis
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
      REPORT_UNEXPECTED_EOF(PENegationEOF);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    aParsingStatus = SELECTOR_PARSING_ENDED_OK;
    if (!aSelector.mNegations) {
      aSelector.mNegations = new nsCSSSelector();
      if (!aSelector.mNegations) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    // ID, class and attribute selectors and pseudo-classes are stored in
    // the first mNegations attached to a selector
    if (eCSSToken_ID == mToken.mType) {   // #id
      ParseIDSelector(aDataMask, *aSelector.mNegations, aParsingStatus, aErrorCode);
    }
    else if (mToken.IsSymbol('.')) {  // .class
      ParseClassSelector(aDataMask, *aSelector.mNegations, aParsingStatus, aErrorCode);
    }
    else if (mToken.IsSymbol(':')) { // :pseudo
      ParsePseudoSelector(aDataMask, *aSelector.mNegations, aParsingStatus, aErrorCode, PR_TRUE);
    }
    else if (mToken.IsSymbol('[')) {  // attribute
      ParseAttributeSelector(aDataMask, *aSelector.mNegations, aParsingStatus, aErrorCode);
    }
    else {
      // then it should be a type element or universal selector
      nsCSSSelector *newSel = new nsCSSSelector();
      if (!newSel) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      nsCSSSelector* negations = aSelector.mNegations;
      while (nsnull != negations->mNegations) {
        negations = negations->mNegations;
      }
      // negated type element selectors and universal selectors are stored after the first
      // mNegations containing only negated IDs, classes, attributes and pseudo-classes
      negations->mNegations = newSel;
      ParseTypeOrUniversalSelector(aDataMask, *newSel, aParsingStatus, aErrorCode, PR_TRUE);
    }
    if (SELECTOR_PARSING_STOPPED_ERROR == aParsingStatus) {
      REPORT_UNEXPECTED_TOKEN(PENegationBadInner);
      return;
    }
    // close the parenthesis
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(PENegationNoClose);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PENegationBadArg);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
  }
}

//
// Parse the argument of a pseudo-class :lang()
//
void CSSParserImpl::ParseLangSelector(nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      nsresult& aErrorCode)
{
  // Check if we have the first parenthesis
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {

    if (! GetToken(aErrorCode, PR_TRUE)) { // premature eof
      REPORT_UNEXPECTED_EOF(PELangArgEOF);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    // We expect an identifier with a language abbreviation
    if (eCSSToken_Ident != mToken.mType) {
      REPORT_UNEXPECTED_TOKEN(PELangArgNotIdent);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }

    // Add the pseudo with the language parameter
    aSelector.AddPseudoClass(nsCSSPseudoClasses::lang, mToken.mIdent.get());

    // close the parenthesis
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(PELangNoClose);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(PELangNoArg);
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
  }
}

/**
 * This is the format for selectors:
 * operator? [[namespace |]? element_name]? [ ID | class | attrib | pseudo ]*
 */
PRBool CSSParserImpl::ParseSelector(nsresult& aErrorCode,
                                nsCSSSelector& aSelector)
{
  PRInt32  dataMask = 0;
  PRInt32  parsingStatus = SELECTOR_PARSING_ENDED_OK;

  if (! GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PESelectorEOF);
    return PR_FALSE;
  }

  ParseTypeOrUniversalSelector(dataMask, aSelector, parsingStatus, aErrorCode, PR_FALSE);
  if (SELECTOR_PARSING_STOPPED_OK == parsingStatus) {
    return PR_TRUE;
  }
  else if (SELECTOR_PARSING_STOPPED_ERROR == parsingStatus) {
    return PR_FALSE;
  }

  for (;;) {
    parsingStatus = SELECTOR_PARSING_ENDED_OK;
    if (eCSSToken_ID == mToken.mType) {   // #id
      ParseIDSelector(dataMask, aSelector, parsingStatus, aErrorCode);
    }
    else if (mToken.IsSymbol('.')) {  // .class
      ParseClassSelector(dataMask, aSelector, parsingStatus, aErrorCode);
    }
    else if (mToken.IsSymbol(':')) { // :pseudo
      ParsePseudoSelector(dataMask, aSelector, parsingStatus, aErrorCode, PR_FALSE);
    }
    else if (mToken.IsSymbol('[')) {  // attribute
      ParseAttributeSelector(dataMask, aSelector, parsingStatus, aErrorCode);
    }
    else {  // not a selector token, we're done
      break;
    }
    
    if (SELECTOR_PARSING_STOPPED_OK == parsingStatus) {
      return PR_TRUE;
    }
    else if (SELECTOR_PARSING_STOPPED_ERROR == parsingStatus) {
      return PR_FALSE;
    }
    
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  UngetToken();
  return PRBool(0 != dataMask);
}

nsCSSDeclaration*
CSSParserImpl::ParseDeclarationBlock(nsresult& aErrorCode,
                                     PRBool aCheckForBraces)
{
  if (aCheckForBraces) {
    if (!ExpectSymbol(aErrorCode, '{', PR_TRUE)) {
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
      if (!ParseDeclaration(aErrorCode, declaration, aCheckForBraces,
                            &changed)) {
        if (!SkipDeclaration(aErrorCode, aCheckForBraces)) {
          break;
        }
        if (aCheckForBraces) {
          if (ExpectSymbol(aErrorCode, '}', PR_TRUE)) {
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

PRBool CSSParserImpl::ParseColor(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorEOF);
    return PR_FALSE;
  }

  nsCSSToken* tk = &mToken;
  nscolor rgba;
  switch (tk->mType) {
    case eCSSToken_ID:
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
            aValue.SetIntValue(value, eCSSUnit_Integer);
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
        if (ExpectSymbol(aErrorCode, '(', PR_FALSE) && // this won't fail
            ParseColorComponent(aErrorCode, r, type, ',') &&
            ParseColorComponent(aErrorCode, g, type, ',') &&
            ParseColorComponent(aErrorCode, b, type, ')')) {
          aValue.SetColorValue(NS_RGB(r,g,b));
          return PR_TRUE;
        }
        return PR_FALSE;  // already pushed back
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-rgba")) {
        // rgba ( component , component , component , opacity )
        PRUint8 r, g, b, a;
        PRInt32 type = COLOR_TYPE_UNKNOWN;
        if (ExpectSymbol(aErrorCode, '(', PR_FALSE) && // this won't fail
            ParseColorComponent(aErrorCode, r, type, ',') &&
            ParseColorComponent(aErrorCode, g, type, ',') &&
            ParseColorComponent(aErrorCode, b, type, ',') &&
            ParseColorOpacity(aErrorCode, a)) {
          aValue.SetColorValue(NS_RGBA(r, g, b, a));
          return PR_TRUE;
        }
        return PR_FALSE;  // already pushed back
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("hsl")) {
        // hsl ( hue , saturation , lightness )
        // "hue" is a number, "saturation" and "lightness" are percentages.
        if (ParseHSLColor(aErrorCode, rgba, ')')) {
          aValue.SetColorValue(rgba);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
      else if (mToken.mIdent.LowerCaseEqualsLiteral("-moz-hsla")) {
        // hsla ( hue , saturation , lightness , opacity )
        // "hue" is a number, "saturation" and "lightness" are percentages,
        // "opacity" is a number.
        PRUint8 a;
        if (ParseHSLColor(aErrorCode, rgba, ',') &&
            ParseColorOpacity(aErrorCode, a)) {
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
PRBool CSSParserImpl::ParseColorComponent(nsresult& aErrorCode,
                                          PRUint8& aComponent,
                                          PRInt32& aType,
                                          char aStop)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
  if (ExpectSymbol(aErrorCode, aStop, PR_TRUE)) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    aComponent = (PRUint8) value;
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


PRBool CSSParserImpl::ParseHSLColor(nsresult& aErrorCode, nscolor& aColor,
                                    char aStop)
{
  float h, s, l;
  if (!ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    NS_ERROR("How did this get to be a function token?");
    return PR_FALSE;
  }

  // Get the hue
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
  
  if (!ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return PR_FALSE;
  }
  
  // Get the saturation
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
  
  if (!ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedComma);
    return PR_FALSE;
  }

  // Get the lightness
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
        
  if (ExpectSymbol(aErrorCode, aStop, PR_TRUE)) {
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
 
 
PRBool CSSParserImpl::ParseColorOpacity(nsresult& aErrorCode, PRUint8& aOpacity)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF(PEColorOpacityEOF);
    return PR_FALSE;
  }

  if (mToken.mType != eCSSToken_Number) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedNumber);
    UngetToken();
    return PR_FALSE;
  }

  PRInt32 value = NSToIntRound(mToken.mNumber*255);

  if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
    REPORT_UNEXPECTED_TOKEN(PEExpectedCloseParen);
    return PR_FALSE;
  }
  
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  aOpacity = (PRUint8)value;

  return PR_TRUE;
}

#ifdef MOZ_XUL
PRBool CSSParserImpl::ParseTreePseudoElement(nsresult& aErrorCode,
                                                 nsCSSSelector& aSelector)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    while (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      if (!GetToken(aErrorCode, PR_TRUE)) {
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
CSSParserImpl::ParseDeclaration(nsresult& aErrorCode,
                                nsCSSDeclaration* aDeclaration,
                                PRBool aCheckForBraces,
                                PRBool* aChanged)
{
  mTempData.AssertInitialState();

  // Get property name
  nsCSSToken* tk = &mToken;
  nsAutoString propertyName;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF(PEDeclEndEOF);
      }
      return PR_FALSE;
    }
    if (eCSSToken_Ident == tk->mType) {
      propertyName = tk->mIdent;
      // grab the ident before the ExpectSymbol trashes the token
      if (!ExpectSymbol(aErrorCode, ':', PR_TRUE)) {
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

  // Map property name to it's ID and then parse the property
  nsCSSProperty propID = nsCSSProps::LookupProperty(propertyName);
  if (eCSSProperty_UNKNOWN == propID) { // unknown property
    const PRUnichar *params[] = {
      propertyName.get()
    };
    REPORT_UNEXPECTED_P(PEUnknownProperty, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    return PR_FALSE;
  }
  if (! ParseProperty(aErrorCode, propID)) {
    // XXX Much better to put stuff in the value parsers instead...
    const PRUnichar *params[] = {
      propertyName.get()
    };
    REPORT_UNEXPECTED_P(PEPropertyParsingError, params);
    REPORT_UNEXPECTED(PEDeclDropped);
    OUTPUT_ERROR();
    ClearTempData(propID);
    return PR_FALSE;
  }
  CLEAR_ERROR();

  // See if the declaration is followed by a "!important" declaration
  PRBool isImportant = PR_FALSE;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    if (aCheckForBraces) {
      // Premature eof is not ok when proper termination is mandated
      REPORT_UNEXPECTED_EOF(PEEndOfDeclEOF);
      ClearTempData(propID);
      return PR_FALSE;
    }
    TransferTempData(aDeclaration, propID, isImportant, aChanged);
    return PR_TRUE;
  }
  else {
    if (eCSSToken_Symbol == tk->mType) {
      if ('!' == tk->mSymbol) {
        // Look for important ident
        if (!GetToken(aErrorCode, PR_TRUE)) {
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
    }
    else {
      // Not a !important declaration
      UngetToken();
    }
  }

  // Make sure valid property declaration is terminated with either a
  // semicolon or a right-curly-brace (when aCheckForBraces is true).
  // When aCheckForBraces is false, proper termination is either
  // semicolon or EOF.
  if (!GetToken(aErrorCode, PR_TRUE)) {
    if (aCheckForBraces) {
      // Premature eof is not ok
      REPORT_UNEXPECTED_EOF(PEDeclEndEOF);
      ClearTempData(propID);
      return PR_FALSE;
    }
    TransferTempData(aDeclaration, propID, isImportant, aChanged);
    return PR_TRUE;
  } 
  if (eCSSToken_Symbol == tk->mType) {
    if (';' == tk->mSymbol) {
      TransferTempData(aDeclaration, propID, isImportant, aChanged);
      return PR_TRUE;
    }
    if (!aCheckForBraces) {
      // If we didn't hit eof and we didn't see a semicolon then the
      // declaration is not properly terminated.
      REPORT_UNEXPECTED_TOKEN(PEBadDeclEnd);
      REPORT_UNEXPECTED(PEDeclDropped);
      OUTPUT_ERROR();
      ClearTempData(propID);
      return PR_FALSE;
    }
    if ('}' == tk->mSymbol) {
      UngetToken();
      TransferTempData(aDeclaration, propID, isImportant, aChanged);
      return PR_TRUE;
    }
  }
  if (aCheckForBraces)
    REPORT_UNEXPECTED_TOKEN(PEBadDeclOrRuleEnd);
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
                                PRBool* aChanged)
{
  if (nsCSSProps::IsShorthand(aPropID)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aPropID) {
      DoTransferTempData(aDeclaration, *p, aIsImportant, aChanged);
    }
  } else {
    DoTransferTempData(aDeclaration, aPropID, aIsImportant, aChanged);
  }
  mTempData.AssertInitialState();
}

// Perhaps the transferring code should be in nsCSSExpandedDataBlock, in
// case some other caller wants to use it in the future (although I
// can't think of why).
void
CSSParserImpl::DoTransferTempData(nsCSSDeclaration* aDeclaration,
                                  nsCSSProperty aPropID, PRBool aIsImportant,
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
  mData.SetPropertyBit(aPropID);
  mTempData.ClearPropertyBit(aPropID);

  aDeclaration->ValueAppended(aPropID);

  /*
   * Save needless copying and allocation by calling the destructor in
   * the destination, copying memory directly, and then using placement
   * new.
   */
  void *v_source = mTempData.PropertyAt(aPropID);
  void *v_dest = mData.PropertyAt(aPropID);
  switch (nsCSSProps::kTypeTable[aPropID]) {
    case eCSSType_Value: {
      nsCSSValue *source = NS_STATIC_CAST(nsCSSValue*, v_source);
      nsCSSValue *dest = NS_STATIC_CAST(nsCSSValue*, v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSValue();
      memcpy(dest, source, sizeof(nsCSSValue));
      new (source) nsCSSValue();
    } break;

    case eCSSType_Rect: {
      nsCSSRect *source = NS_STATIC_CAST(nsCSSRect*, v_source);
      nsCSSRect *dest = NS_STATIC_CAST(nsCSSRect*, v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSRect();
      memcpy(dest, source, sizeof(nsCSSRect));
      new (source) nsCSSRect();
    } break;

    case eCSSType_ValuePair: {
      nsCSSValuePair *source = NS_STATIC_CAST(nsCSSValuePair*, v_source);
      nsCSSValuePair *dest = NS_STATIC_CAST(nsCSSValuePair*, v_dest);
      if (*source != *dest)
        *aChanged = PR_TRUE;
      dest->~nsCSSValuePair();
      memcpy(dest, source, sizeof(nsCSSValuePair));
      new (source) nsCSSValuePair();
    } break;

    case eCSSType_ValueList: {
      nsCSSValueList **source = NS_STATIC_CAST(nsCSSValueList**, v_source);
      nsCSSValueList **dest = NS_STATIC_CAST(nsCSSValueList**, v_dest);
      if (!nsCSSValueList::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;

    case eCSSType_CounterData: {
      nsCSSCounterData **source = NS_STATIC_CAST(nsCSSCounterData**, v_source);
      nsCSSCounterData **dest = NS_STATIC_CAST(nsCSSCounterData**, v_dest);
      if (!nsCSSCounterData::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;

    case eCSSType_Quotes: {
      nsCSSQuotes **source = NS_STATIC_CAST(nsCSSQuotes**, v_source);
      nsCSSQuotes **dest = NS_STATIC_CAST(nsCSSQuotes**, v_dest);
      if (!nsCSSQuotes::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;

    case eCSSType_Shadow: {
      nsCSSShadow **source = NS_STATIC_CAST(nsCSSShadow**, v_source);
      nsCSSShadow **dest = NS_STATIC_CAST(nsCSSShadow**, v_dest);
      if (!nsCSSShadow::Equal(*source, *dest))
        *aChanged = PR_TRUE;
      delete *dest;
      *dest = *source;
      *source = nsnull;
    } break;
  }
}

// Flags for ParseVariant method
#define VARIANT_KEYWORD         0x000001  // K
#define VARIANT_LENGTH          0x000002  // L
#define VARIANT_PERCENT         0x000004  // P
#define VARIANT_COLOR           0x000008  // C
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
#define VARIANT_INHERIT         0x020000  // H
#define VARIANT_NONE            0x040000  // O
#define VARIANT_NORMAL          0x080000  // M

// Common combinations of variants
#define VARIANT_AL   (VARIANT_AUTO | VARIANT_LENGTH)
#define VARIANT_LP   (VARIANT_LENGTH | VARIANT_PERCENT)
#define VARIANT_AH   (VARIANT_AUTO | VARIANT_INHERIT)
#define VARIANT_AHLP (VARIANT_AH | VARIANT_LP)
#define VARIANT_AHI  (VARIANT_AH | VARIANT_INTEGER)
#define VARIANT_AHK  (VARIANT_AH | VARIANT_KEYWORD)
#define VARIANT_AUK  (VARIANT_AUTO | VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_AHUK (VARIANT_AH | VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_AHL  (VARIANT_AH | VARIANT_LENGTH)
#define VARIANT_AHKL (VARIANT_AHK | VARIANT_LENGTH)
#define VARIANT_HK   (VARIANT_INHERIT | VARIANT_KEYWORD)
#define VARIANT_HKF  (VARIANT_HK | VARIANT_FREQUENCY)
#define VARIANT_HKL  (VARIANT_HK | VARIANT_LENGTH)
#define VARIANT_HKLP (VARIANT_HK | VARIANT_LP)
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

static const nsCSSProperty kBorderTopIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color
};
static const nsCSSProperty kBorderRightIDs[] = {
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
  eCSSProperty_border_left_width,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_color
};

PRBool CSSParserImpl::ParseEnum(nsresult& aErrorCode, nsCSSValue& aValue,
                                const PRInt32 aKeywordTable[])
{
  nsString* ident = NextIdent(aErrorCode);
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

PRBool CSSParserImpl::TranslateDimension(nsresult& aErrorCode,
                                         nsCSSValue& aValue,
                                         PRInt32 aVariantMask,
                                         float aNumber,
                                         const nsString& aUnit)
{
  nsCSSUnit units;
  PRInt32   type = 0;
  if (!aUnit.IsEmpty()) {
    nsCSSKeyword id = nsCSSKeywords::LookupKeyword(aUnit);
    switch (id) {
      case eCSSKeyword_em:    units = eCSSUnit_EM;          type = VARIANT_LENGTH;  break;
      case eCSSKeyword_ex:    units = eCSSUnit_XHeight;     type = VARIANT_LENGTH;  break;
      case eCSSKeyword_ch:    units = eCSSUnit_Char;        type = VARIANT_LENGTH;  break;
      case eCSSKeyword_px:    units = eCSSUnit_Pixel;       type = VARIANT_LENGTH;  break;
      case eCSSKeyword_in:    units = eCSSUnit_Inch;        type = VARIANT_LENGTH;  break;
      case eCSSKeyword_cm:    units = eCSSUnit_Centimeter;  type = VARIANT_LENGTH;  break;
      case eCSSKeyword_mm:    units = eCSSUnit_Millimeter;  type = VARIANT_LENGTH;  break;
      case eCSSKeyword_pt:    units = eCSSUnit_Point;       type = VARIANT_LENGTH;  break;
      case eCSSKeyword_pc:    units = eCSSUnit_Pica;        type = VARIANT_LENGTH;  break;

      case eCSSKeyword_deg:   units = eCSSUnit_Degree;      type = VARIANT_ANGLE;   break;
      case eCSSKeyword_grad:  units = eCSSUnit_Grad;        type = VARIANT_ANGLE;   break;
      case eCSSKeyword_rad:   units = eCSSUnit_Radian;      type = VARIANT_ANGLE;   break;

      case eCSSKeyword_hz:    units = eCSSUnit_Hertz;       type = VARIANT_FREQUENCY; break;
      case eCSSKeyword_khz:   units = eCSSUnit_Kilohertz;   type = VARIANT_FREQUENCY; break;

      case eCSSKeyword_s:     units = eCSSUnit_Seconds;       type = VARIANT_TIME;  break;
      case eCSSKeyword_ms:    units = eCSSUnit_Milliseconds;  type = VARIANT_TIME;  break;
      default:
        // unknown unit
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

PRBool CSSParserImpl::ParsePositiveVariant(nsresult& aErrorCode, 
                                           nsCSSValue& aValue, 
                                           PRInt32 aVariantMask, 
                                           const PRInt32 aKeywordTable[]) 
{ 
  if (ParseVariant(aErrorCode, aValue, aVariantMask, aKeywordTable)) { 
    if (eCSSUnit_Number == aValue.GetUnit() || 
        aValue.IsLengthUnit()){ 
      if (aValue.GetFloatValue() < 0) { 
        UngetToken();
        return PR_FALSE; 
      } 
    } 
    else if(aValue.GetUnit() == eCSSUnit_Percent) { 
      if (aValue.GetPercentValue() < 0) { 
        UngetToken();
        return PR_FALSE; 
      } 
    } 
    return PR_TRUE; 
  } 
  return PR_FALSE; 
} 

PRBool CSSParserImpl::ParseVariant(nsresult& aErrorCode, nsCSSValue& aValue,
                                   PRInt32 aVariantMask,
                                   const PRInt32 aKeywordTable[])
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
    if (TranslateDimension(aErrorCode, aValue, aVariantMask, tk->mNumber, tk->mIdent)) {
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
  if (mSVGMode && !IsParsingCompoundProperty()) {
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
    if (ParseURL(aErrorCode, aValue)) {
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  if ((aVariantMask & VARIANT_COLOR) != 0) {
    if ((mNavQuirkMode && !IsParsingCompoundProperty()) || // NONSTANDARD: Nav interprets 'xxyyzz' values even without '#' prefix
    		(eCSSToken_ID == tk->mType) || 
        (eCSSToken_Ident == tk->mType) ||
        ((eCSSToken_Function == tk->mType) && 
         (tk->mIdent.LowerCaseEqualsLiteral("rgb") ||
          tk->mIdent.LowerCaseEqualsLiteral("hsl") ||
          tk->mIdent.LowerCaseEqualsLiteral("-moz-rgba") ||
          tk->mIdent.LowerCaseEqualsLiteral("-moz-hsla")))) {
      // Put token back so that parse color can get it
      UngetToken();
      if (ParseColor(aErrorCode, aValue)) {
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
#ifdef ENABLE_COUNTERS
    if (ParseCounter(aErrorCode, aValue)) {
      return PR_TRUE;
    }
#endif
    return PR_FALSE;
  }
  if (((aVariantMask & VARIANT_ATTR) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      tk->mIdent.LowerCaseEqualsLiteral("attr")) {
    
    if (ParseAttr(aErrorCode, aValue)) {
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  UngetToken();
  return PR_FALSE;
}


PRBool CSSParserImpl::ParseCounter(nsresult& aErrorCode, nsCSSValue& aValue)
{
  nsCSSUnit unit = (mToken.mIdent.LowerCaseEqualsLiteral("counter") ? 
                    eCSSUnit_Counter : eCSSUnit_Counters);

  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    if (GetToken(aErrorCode, PR_TRUE)) {
      if (eCSSToken_Ident == mToken.mType) {
        nsAutoString  counter;
        counter.Append(mToken.mIdent);

        if (eCSSUnit_Counters == unit) {
          // get mandatory string
          if (! ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
            return PR_FALSE;
          }
          if (GetToken(aErrorCode, PR_TRUE) && (eCSSToken_String == mToken.mType)) {
            counter.Append(PRUnichar(','));
            counter.Append(mToken.mSymbol); // quote too
            counter.Append(mToken.mIdent);
            counter.Append(mToken.mSymbol); // quote too
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }
        // get optional type
        if (ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
          if (GetToken(aErrorCode, PR_TRUE) && (eCSSToken_Ident == mToken.mType)) {
            nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
            PRInt32 dummy;
            if ((eCSSKeyword_UNKNOWN < keyword) && 
                nsCSSProps::FindKeyword(keyword, nsCSSProps::kListStyleKTable, dummy)) {
              counter.Append(PRUnichar(','));
              counter.Append(mToken.mIdent);
            }
            else {
              return PR_FALSE;
            }
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }

        if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
          aValue.SetStringValue(counter, unit);
          return PR_TRUE;
        }
      }
      else {
        UngetToken();
      }
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseAttr(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    if (GetToken(aErrorCode, PR_TRUE)) {
      nsAutoString attr;
      if (eCSSToken_Ident == mToken.mType) {  // attr name or namespace
        nsAutoString  holdIdent(mToken.mIdent);
        if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // namespace
          PRInt32 nameSpaceID = kNameSpaceID_Unknown;
          if (mNameSpace) {
            ToLowerCase(holdIdent); // always case insensitive, since stays within CSS
            nsCOMPtr<nsIAtom> prefix = do_GetAtom(holdIdent);
            mNameSpace->FindNameSpaceID(prefix, &nameSpaceID);
          } // else, no declared namespaces
          if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
            const PRUnichar *params[] = {
              holdIdent.get()
            };
            REPORT_UNEXPECTED_P(PEUnknownNamespacePrefix, params);
            return PR_FALSE;
          }
          attr.AppendInt(nameSpaceID, 10);
          attr.Append(PRUnichar('|'));
          if (! GetToken(aErrorCode, PR_FALSE)) {
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
        if (! GetToken(aErrorCode, PR_FALSE)) {
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
      if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
        aValue.SetStringValue(attr, eCSSUnit_Attr);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseURL(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    if (! GetURLToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    nsCSSToken* tk = &mToken;
    if ((eCSSToken_String == tk->mType) || (eCSSToken_URL == tk->mType)) {
      // Translate url into an absolute url if the url is relative to
      // the style sheet.
      nsCOMPtr<nsIURI> uri;
      NS_NewURI(getter_AddRefs(uri), tk->mIdent, nsnull, mBaseURL);
      if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
        // Set a null value on failure.  Most failure cases should be
        // NS_ERROR_MALFORMED_URI.
        nsCSSValue::URL *url =
          new nsCSSValue::URL(uri, tk->mIdent.get(), mBaseURL);
        if (!url || !url->mString) {
          aErrorCode = NS_ERROR_OUT_OF_MEMORY;
          delete url;
          return PR_FALSE;
        }
        aValue.SetURLValue(url);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

PRInt32 CSSParserImpl::ParseChoice(nsresult& aErrorCode, nsCSSValue aValues[],
                                   const nsCSSProperty aPropIDs[], PRInt32 aNumIDs)
{
  PRInt32 found = 0;
  SetParsingCompoundProperty(PR_TRUE);

  PRInt32 loop;
  for (loop = 0; loop < aNumIDs; loop++) {
    // Try each property parser in order
    PRInt32 hadFound = found;
    PRInt32 index;
    for (index = 0; index < aNumIDs; index++) {
      PRInt32 bit = 1 << index;
      if ((found & bit) == 0) {
        if (ParseSingleValueProperty(aErrorCode, aValues[index], aPropIDs[index])) {
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
  SetParsingCompoundProperty(PR_FALSE);
  return found;
}

void
CSSParserImpl::AppendValue(nsCSSProperty aPropID,
                           const nsCSSValue& aValue)
{
  NS_ASSERTION(0 <= aPropID && aPropID < eCSSProperty_COUNT_no_shorthands,
               "property out of range");
  NS_ASSERTION(nsCSSProps::kTypeTable[aPropID] == eCSSType_Value,
               nsPrintfCString(64, "type error (property=\'%s\')",
                             nsCSSProps::GetStringValue(aPropID).get()).get());
  nsCSSValue& storage =
      *NS_STATIC_CAST(nsCSSValue*, mTempData.PropertyAt(aPropID));
  storage = aValue;
  mTempData.SetPropertyBit(aPropID);
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values. 
 */
PRBool CSSParserImpl::ParseBoxProperties(nsresult& aErrorCode,
                                         nsCSSRect& aResult,
                                         const nsCSSProperty aPropIDs[])
{
  // Get up to four values for the property
  PRInt32 count = 0;
  PRInt32 index;
  nsCSSRect result;
  for (index = 0; index < 4; index++) {
    if (! ParseSingleValueProperty(aErrorCode,
                                   result.*(nsCSSRect::sides[index]),
                                   aPropIDs[index])) {
      break;
    }
    count++;
  }
  if ((count == 0) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  if (1 < count) { // verify no more than single inherit or initial
    for (index = 0; index < 4; index++) {
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

  for (index = 0; index < 4; index++) {
    mTempData.SetPropertyBit(aPropIDs[index]);
  }
  aResult = result;
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseDirectionalBoxProperty(nsresult& aErrorCode,
                                                  nsCSSProperty aProperty,
                                                  PRInt32 aSourceType)
{
  const nsCSSProperty* subprops = nsCSSProps::SubpropertyEntryFor(aProperty);
  NS_ASSERTION(subprops[3] == eCSSProperty_UNKNOWN,
               "not box property with physical vs. logical cascading");
  nsCSSValue value;
  if (!ParseSingleValueProperty(aErrorCode, value, subprops[0]) ||
      !ExpectEndProperty(aErrorCode, PR_TRUE))
    return PR_FALSE;

  AppendValue(subprops[0], value);
  nsCSSValue typeVal(aSourceType, eCSSUnit_Enumerated);
  AppendValue(subprops[1], typeVal);
  AppendValue(subprops[2], typeVal);
  aErrorCode = NS_OK;
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseProperty(nsresult& aErrorCode,
                                    nsCSSProperty aPropID)
{
  switch (aPropID) {  // handle shorthand or multiple properties
  case eCSSProperty_background:
    return ParseBackground(aErrorCode);
  case eCSSProperty_background_position:
    return ParseBackgroundPosition(aErrorCode);
  case eCSSProperty_border:
    return ParseBorderSide(aErrorCode, kBorderTopIDs, PR_TRUE);
  case eCSSProperty_border_color:
    return ParseBorderColor(aErrorCode);
  case eCSSProperty_border_spacing:
    return ParseBorderSpacing(aErrorCode);
  case eCSSProperty_border_style:
    return ParseBorderStyle(aErrorCode);
  case eCSSProperty_border_bottom:
    return ParseBorderSide(aErrorCode, kBorderBottomIDs, PR_FALSE);
  case eCSSProperty_border_left:
    return ParseBorderSide(aErrorCode, kBorderLeftIDs, PR_FALSE);
  case eCSSProperty_border_right:
    return ParseBorderSide(aErrorCode, kBorderRightIDs, PR_FALSE);
  case eCSSProperty_border_top:
    return ParseBorderSide(aErrorCode, kBorderTopIDs, PR_FALSE);
  case eCSSProperty_border_bottom_colors:
    return ParseBorderColors(aErrorCode,
                             &mTempData.mMargin.mBorderColors.mBottom,
                             aPropID);
  case eCSSProperty_border_left_colors:
    return ParseBorderColors(aErrorCode,
                             &mTempData.mMargin.mBorderColors.mLeft,
                             aPropID);
  case eCSSProperty_border_right_colors:
    return ParseBorderColors(aErrorCode,
                             &mTempData.mMargin.mBorderColors.mRight,
                             aPropID);
  case eCSSProperty_border_top_colors:
    return ParseBorderColors(aErrorCode,
                             &mTempData.mMargin.mBorderColors.mTop,
                             aPropID);
  case eCSSProperty_border_width:
    return ParseBorderWidth(aErrorCode);
  case eCSSProperty__moz_border_radius:
    return ParseBorderRadius(aErrorCode);
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline_radius:
    return ParseOutlineRadius(aErrorCode);
#endif
  case eCSSProperty_clip:
    return ParseRect(mTempData.mDisplay.mClip, aErrorCode,
                     eCSSProperty_clip);
  case eCSSProperty_content:
    return ParseContent(aErrorCode);
  case eCSSProperty__moz_counter_increment:
    return ParseCounterData(aErrorCode, &mTempData.mContent.mCounterIncrement,
                            aPropID);
  case eCSSProperty__moz_counter_reset:
    return ParseCounterData(aErrorCode, &mTempData.mContent.mCounterReset,
                            aPropID);
  case eCSSProperty_cue:
    return ParseCue(aErrorCode);
  case eCSSProperty_cursor:
    return ParseCursor(aErrorCode);
  case eCSSProperty_font:
    return ParseFont(aErrorCode);
  case eCSSProperty_image_region:
    return ParseRect(mTempData.mList.mImageRegion, aErrorCode,
                     eCSSProperty_image_region);
  case eCSSProperty_list_style:
    return ParseListStyle(aErrorCode);
  case eCSSProperty_margin:
    return ParseMargin(aErrorCode);
  case eCSSProperty_margin_end:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_margin_end,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_margin_left:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_margin_left,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_margin_right:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_margin_right,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_margin_start:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_margin_start,
                                       NS_BOXPROP_SOURCE_LOGICAL);
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline:
    return ParseOutline(aErrorCode);
#endif
  case eCSSProperty_overflow:
    return ParseOverflow(aErrorCode);
  case eCSSProperty_padding:
    return ParsePadding(aErrorCode);
  case eCSSProperty_padding_end:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_padding_end,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_padding_left:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_padding_left,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_padding_right:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_padding_right,
                                       NS_BOXPROP_SOURCE_PHYSICAL);
  case eCSSProperty_padding_start:
    return ParseDirectionalBoxProperty(aErrorCode, eCSSProperty_padding_start,
                                       NS_BOXPROP_SOURCE_LOGICAL);
  case eCSSProperty_pause:
    return ParsePause(aErrorCode);
  case eCSSProperty_quotes:
    return ParseQuotes(aErrorCode);
  case eCSSProperty_size:
    return ParseSize(aErrorCode);
  case eCSSProperty_text_shadow:
    return ParseTextShadow(aErrorCode);

  // Strip out properties we use internally. These properties are used
  // by compound property parsing routines (e.g. "background-position").
  case eCSSProperty_background_x_position:
  case eCSSProperty_background_y_position:
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
    // The user can't use these
    REPORT_UNEXPECTED(PEInaccessibleProperty);
    return PR_FALSE;

  default:  // must be single property
    {
      nsCSSValue value;
      if (ParseSingleValueProperty(aErrorCode, value, aPropID)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(aPropID, value);
          aErrorCode = NS_OK;
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
#define BG_CENTER  0x01
#define BG_TOP     0x02
#define BG_BOTTOM  0x04
#define BG_LEFT    0x08
#define BG_RIGHT   0x10
#define BG_CTB    (BG_CENTER | BG_TOP | BG_BOTTOM)
#define BG_CLR    (BG_CENTER | BG_LEFT | BG_RIGHT)

// Note: Don't change this table unless you update
// parseBackgroundPosition!

static const PRInt32 kBackgroundXYPositionKTable[] = {
  eCSSKeyword_center, BG_CENTER,
  eCSSKeyword_top, BG_TOP,
  eCSSKeyword_bottom, BG_BOTTOM,
  eCSSKeyword_left, BG_LEFT,
  eCSSKeyword_right, BG_RIGHT,
  -1,
};

PRBool CSSParserImpl::ParseSingleValueProperty(nsresult& aErrorCode,
                                               nsCSSValue& aValue,
                                               nsCSSProperty aPropID)
{
  switch (aPropID) {
  case eCSSProperty_UNKNOWN:
  case eCSSProperty_background:
  case eCSSProperty_background_position:
  case eCSSProperty_border:
  case eCSSProperty_border_color:
  case eCSSProperty_border_bottom_colors:
  case eCSSProperty_border_left_colors:
  case eCSSProperty_border_right_colors:
  case eCSSProperty_border_top_colors:
  case eCSSProperty_border_spacing:
  case eCSSProperty_border_style:
  case eCSSProperty_border_bottom:
  case eCSSProperty_border_left:
  case eCSSProperty_border_right:
  case eCSSProperty_border_top:
  case eCSSProperty_border_width:
  case eCSSProperty__moz_border_radius:
  case eCSSProperty_clip:
  case eCSSProperty_content:
  case eCSSProperty__moz_counter_increment:
  case eCSSProperty__moz_counter_reset:
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
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline:
  case eCSSProperty__moz_outline_radius:
#endif
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
  case eCSSProperty_COUNT:
    NS_ERROR("not a single value property");
    return PR_FALSE;

  case eCSSProperty_margin_left_ltr_source:
  case eCSSProperty_margin_left_rtl_source:
  case eCSSProperty_margin_right_ltr_source:
  case eCSSProperty_margin_right_rtl_source:
  case eCSSProperty_padding_left_ltr_source:
  case eCSSProperty_padding_left_rtl_source:
  case eCSSProperty_padding_right_ltr_source:
  case eCSSProperty_padding_right_rtl_source:
    NS_ERROR("not currently parsed here");
    return PR_FALSE;

  case eCSSProperty_appearance:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kAppearanceKTable);
  case eCSSProperty_azimuth:
    return ParseAzimuth(aErrorCode, aValue);
  case eCSSProperty_background_attachment:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundAttachmentKTable);
  case eCSSProperty__moz_background_clip:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundClipKTable);
  case eCSSProperty_background_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK,
                        nsCSSProps::kBackgroundColorKTable);
  case eCSSProperty_background_image:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case eCSSProperty__moz_background_inline_policy:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundInlinePolicyKTable);
  case eCSSProperty__moz_background_origin:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundOriginKTable);
  case eCSSProperty_background_repeat:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundRepeatKTable);
  case eCSSProperty_background_x_position: // for internal use
  case eCSSProperty_background_y_position: // for internal use
    return ParseVariant(aErrorCode, aValue, VARIANT_HKLP,
                        kBackgroundXYPositionKTable);
  case eCSSProperty_binding:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_border_collapse:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBorderCollapseKTable);
  case eCSSProperty_border_bottom_color:
  case eCSSProperty_border_left_color:
  case eCSSProperty_border_right_color:
  case eCSSProperty_border_top_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK, 
                        nsCSSProps::kBorderColorKTable);
  case eCSSProperty_border_bottom_style:
  case eCSSProperty_border_left_style:
  case eCSSProperty_border_right_style:
  case eCSSProperty_border_top_style:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kBorderStyleKTable);
  case eCSSProperty_border_bottom_width:
  case eCSSProperty_border_left_width:
  case eCSSProperty_border_right_width:
  case eCSSProperty_border_top_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HKL,
                                nsCSSProps::kBorderWidthKTable);
  case eCSSProperty__moz_border_radius_topLeft:
  case eCSSProperty__moz_border_radius_topRight:
  case eCSSProperty__moz_border_radius_bottomRight:
  case eCSSProperty__moz_border_radius_bottomLeft:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case eCSSProperty__moz_column_count:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_AHI, nsnull);
  case eCSSProperty__moz_column_width:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHL, nsnull);
  case eCSSProperty__moz_column_gap:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline_radius_topLeft:
  case eCSSProperty__moz_outline_radius_topRight:
  case eCSSProperty__moz_outline_radius_bottomRight:
  case eCSSProperty__moz_outline_radius_bottomLeft:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
#endif
  case eCSSProperty_bottom:
  case eCSSProperty_top:
  case eCSSProperty_left:
  case eCSSProperty_right:
	  return ParseVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_box_align:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxAlignKTable);
  case eCSSProperty_box_direction:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxDirectionKTable);
  case eCSSProperty_box_flex:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case eCSSProperty_box_orient:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxOrientKTable);
  case eCSSProperty_box_pack:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxPackKTable);
  case eCSSProperty_box_ordinal_group:
    return ParseVariant(aErrorCode, aValue, VARIANT_INTEGER, nsnull);
#ifdef MOZ_SVG
  case eCSSProperty_dominant_baseline:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kDominantBaselineKTable);
  case eCSSProperty_fill:
    return ParseVariant(aErrorCode, aValue, VARIANT_HC | VARIANT_NONE,
                        nsnull);
  case eCSSProperty_fill_opacity:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN,
                        nsnull);
  case eCSSProperty_fill_rule:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kFillRuleKTable);
  case eCSSProperty_pointer_events:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kPointerEventsKTable);
  case eCSSProperty_shape_rendering:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kShapeRenderingKTable);
  case eCSSProperty_stroke:
    return ParseVariant(aErrorCode, aValue, VARIANT_HC | VARIANT_NONE,
                        nsnull);
  case eCSSProperty_stroke_dasharray:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOS,
                        nsnull); // XXX parse into new CSS value type, not string
  case eCSSProperty_stroke_dashoffset:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLPN,
                        nsnull);
  case eCSSProperty_stroke_linecap:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kStrokeLinecapKTable);
  case eCSSProperty_stroke_linejoin:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kStrokeLinejoinKTable);
  case eCSSProperty_stroke_miterlimit:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HN,
                                nsnull); // XXX value > 1
  case eCSSProperty_stroke_opacity:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN,
                        nsnull);
  case eCSSProperty_stroke_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLPN,
                        nsnull);
  case eCSSProperty_text_anchor:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kTextAnchorKTable);
  case eCSSProperty_text_rendering:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kTextRenderingKTable);
#endif
  case eCSSProperty_box_sizing:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxSizingKTable);
  case eCSSProperty_height:
  case eCSSProperty_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_force_broken_image_icon:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_INTEGER, nsnull);
  case eCSSProperty_caption_side:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, 
                        nsCSSProps::kCaptionSideKTable);
  case eCSSProperty_clear:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kClearKTable);
  case eCSSProperty_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HC, nsnull);
  case eCSSProperty_cue_after:
  case eCSSProperty_cue_before:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_direction:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kDirectionKTable);
  case eCSSProperty_display:
    if (ParseVariant(aErrorCode, aValue, VARIANT_HOK, nsCSSProps::kDisplayKTable)) {
			if (aValue.GetUnit() == eCSSUnit_Enumerated) {
				switch (aValue.GetIntValue()) {
					case NS_STYLE_DISPLAY_MARKER:        // bug 2055
					case NS_STYLE_DISPLAY_RUN_IN:		 // bug 2056
					case NS_STYLE_DISPLAY_COMPACT:       // bug 14983
					case NS_STYLE_DISPLAY_INLINE_TABLE:  // bug 18218
						return PR_FALSE;
				}
			}
      return PR_TRUE;
		}
    return PR_FALSE;
  case eCSSProperty_elevation:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK | VARIANT_ANGLE,
                        nsCSSProps::kElevationKTable);
  case eCSSProperty_empty_cells:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kEmptyCellsKTable);
  case eCSSProperty_float:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kFloatKTable);
  case eCSSProperty_float_edge:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kFloatEdgeKTable);
  case eCSSProperty_font_family:
    return ParseFamily(aErrorCode, aValue);
  case eCSSProperty_font_size: 
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HKLP,
                                nsCSSProps::kFontSizeKTable);
  case eCSSProperty_font_size_adjust:
    return ParseVariant(aErrorCode, aValue, VARIANT_HON,
                        nsnull);
  case eCSSProperty_font_stretch:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontStretchKTable);
  case eCSSProperty_font_style:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontStyleKTable);
  case eCSSProperty_font_variant:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontVariantKTable);
  case eCSSProperty_font_weight:
    return ParseFontWeight(aErrorCode, aValue);
  case eCSSProperty_letter_spacing:
  case eCSSProperty_word_spacing:
    return ParseVariant(aErrorCode, aValue, VARIANT_HL | VARIANT_NORMAL, nsnull);
  case eCSSProperty_key_equivalent:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kKeyEquivalentKTable);
  case eCSSProperty_line_height:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLPN | VARIANT_NORMAL, nsnull);
  case eCSSProperty_list_style_image:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_list_style_position:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, nsCSSProps::kListStylePositionKTable);
  case eCSSProperty_list_style_type:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK, nsCSSProps::kListStyleKTable);
  case eCSSProperty_margin_bottom:
  case eCSSProperty_margin_end_value: // for internal use
  case eCSSProperty_margin_left_value: // for internal use
  case eCSSProperty_margin_right_value: // for internal use
  case eCSSProperty_margin_start_value: // for internal use
  case eCSSProperty_margin_top:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_marker_offset:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHL, nsnull);
  case eCSSProperty_marks:
    return ParseMarks(aErrorCode, aValue);
  case eCSSProperty_max_height:
  case eCSSProperty_max_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLPO, nsnull);
  case eCSSProperty_min_height:
  case eCSSProperty_min_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_opacity:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case eCSSProperty_orphans:
  case eCSSProperty_widows:
    return ParseVariant(aErrorCode, aValue, VARIANT_HI, nsnull);
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK, 
                        nsCSSProps::kOutlineColorKTable);
  case eCSSProperty__moz_outline_style:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK, 
                        nsCSSProps::kBorderStyleKTable);
  case eCSSProperty__moz_outline_width:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKL,
                        nsCSSProps::kBorderWidthKTable);
#endif
  case eCSSProperty_overflow_x:
  case eCSSProperty_overflow_y:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kOverflowSubKTable);
  case eCSSProperty_padding_bottom:
  case eCSSProperty_padding_end_value: // for internal use
  case eCSSProperty_padding_left_value: // for internal use
  case eCSSProperty_padding_right_value: // for internal use
  case eCSSProperty_padding_start_value: // for internal use
  case eCSSProperty_padding_top:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_page:
    return ParseVariant(aErrorCode, aValue, VARIANT_AUTO | VARIANT_IDENTIFIER, nsnull);
  case eCSSProperty_page_break_after:
  case eCSSProperty_page_break_before:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK, 
                        nsCSSProps::kPageBreakKTable);
  case eCSSProperty_page_break_inside:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK, 
                        nsCSSProps::kPageBreakInsideKTable);
  case eCSSProperty_pause_after:
  case eCSSProperty_pause_before:
    return ParseVariant(aErrorCode, aValue, VARIANT_HTP, nsnull);
  case eCSSProperty_pitch:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKF, nsCSSProps::kPitchKTable);
  case eCSSProperty_pitch_range:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case eCSSProperty_position:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, nsCSSProps::kPositionKTable);
  case eCSSProperty_richness:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case eCSSProperty_speak:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK | VARIANT_NONE,
                        nsCSSProps::kSpeakKTable);
  case eCSSProperty_speak_header:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakHeaderKTable);
  case eCSSProperty_speak_numeral:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakNumeralKTable);
  case eCSSProperty_speak_punctuation:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakPunctuationKTable);
  case eCSSProperty_speech_rate:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN | VARIANT_KEYWORD,
                        nsCSSProps::kSpeechRateKTable);
  case eCSSProperty_stress:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case eCSSProperty_table_layout:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kTableLayoutKTable);
  case eCSSProperty_text_align:
    // When we support aligning on a string, we can parse text-align
    // as a string....
    return ParseVariant(aErrorCode, aValue, VARIANT_HK /* | VARIANT_STRING */,
                        nsCSSProps::kTextAlignKTable);
  case eCSSProperty_text_decoration:
    return ParseTextDecoration(aErrorCode, aValue);
  case eCSSProperty_text_indent:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_text_transform:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kTextTransformKTable);
  case eCSSProperty_unicode_bidi:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kUnicodeBidiKTable);
  case eCSSProperty_user_focus:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK | VARIANT_NONE,
                        nsCSSProps::kUserFocusKTable);
  case eCSSProperty_user_input:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK | VARIANT_NONE,
                        nsCSSProps::kUserInputKTable);
  case eCSSProperty_user_modify:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kUserModifyKTable);
  case eCSSProperty_user_select:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kUserSelectKTable);
  case eCSSProperty_vertical_align:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKLP,
                        nsCSSProps::kVerticalAlignKTable);
  case eCSSProperty_visibility:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, 
                        nsCSSProps::kVisibilityKTable);
  case eCSSProperty_voice_family:
    return ParseFamily(aErrorCode, aValue);
  case eCSSProperty_volume:
    return ParseVariant(aErrorCode, aValue, VARIANT_HPN | VARIANT_KEYWORD,
                        nsCSSProps::kVolumeKTable);
  case eCSSProperty_white_space:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kWhitespaceKTable);
  case eCSSProperty_z_index:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHI, nsnull);
  }
  // explicitly do NOT have a default case to let the compiler
  // help find missing properties
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseAzimuth(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ParseVariant(aErrorCode, aValue, VARIANT_HK | VARIANT_ANGLE, 
                   nsCSSProps::kAzimuthKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_AZIMUTH_LEFT_SIDE <= intValue) && 
          (intValue <= NS_STYLE_AZIMUTH_BEHIND)) {  // look for optional modifier
        nsCSSValue  modifier;
        if (ParseEnum(aErrorCode, modifier, nsCSSProps::kAzimuthKTable)) {
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
BackgroundPositionMaskToCSSValue(PRInt32 aMask, PRBool isX)
{
  PRInt32 pct = 50;
  if (isX) {
    if (aMask & BG_LEFT) {
      pct = 0;
    }
    else if (aMask & BG_RIGHT) {
      pct = 100;
    }
  }
  else {
    if (aMask & BG_TOP) {
      pct = 0;
    }
    else if (aMask & BG_BOTTOM) {
      pct = 100;
    }
  }

  return nsCSSValue(pct, eCSSUnit_Enumerated);
}

PRBool CSSParserImpl::ParseBackground(nsresult& aErrorCode)
{
  const PRInt32 numProps = 6;
  static const nsCSSProperty kBackgroundIDs[numProps] = {
    eCSSProperty_background_color,
    eCSSProperty_background_image,
    eCSSProperty_background_repeat,
    eCSSProperty_background_attachment,
    eCSSProperty_background_x_position,
    eCSSProperty_background_y_position
  };

  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(aErrorCode, values, kBackgroundIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  if (0 != (found & 0x30)) {  // found one or more position values, validate them
    if (0 == (found & 0x20)) {
      if (eCSSUnit_Enumerated == values[4].GetUnit()) {
        PRInt32 mask = values[4].GetIntValue();
        values[4] = BackgroundPositionMaskToCSSValue(mask, PR_TRUE);
        values[5] = BackgroundPositionMaskToCSSValue(mask, PR_FALSE);
      }
      else {
        values[5].SetPercentValue(0.5f);
      }
    }
    else { // both x & y values
      nsCSSUnit xUnit = values[4].GetUnit();
      nsCSSUnit yUnit = values[5].GetUnit();
      if (eCSSUnit_Enumerated == xUnit) {
        PRInt32 xValue = values[4].GetIntValue();
        if (eCSSUnit_Enumerated == yUnit) {
          PRInt32 yValue = values[5].GetIntValue();
          if (0 != (xValue & (BG_LEFT | BG_RIGHT)) &&  // x is really an x value
              0 != (yValue & (BG_LEFT | BG_RIGHT))) {  // y is also an x value
            return PR_FALSE;
          }
          if (0 != (xValue & (BG_TOP | BG_BOTTOM)) &&  // x is really an y value
              0 != (yValue & (BG_TOP | BG_BOTTOM))) {  // y is also an y value
            return PR_FALSE;
          }
          if (0 != (xValue & (BG_TOP | BG_BOTTOM)) ||  // x is really a y value
              0 != (yValue & (BG_LEFT | BG_RIGHT))) {  // or y is really an x value
            PRInt32 holdXValue = xValue;
            xValue = yValue;
            yValue = holdXValue;
          }
          NS_ASSERTION(xValue & BG_CLR, "bad x value");
          NS_ASSERTION(yValue & BG_CTB, "bad y value");
          values[4] = BackgroundPositionMaskToCSSValue(xValue, PR_TRUE);
          values[5] = BackgroundPositionMaskToCSSValue(yValue, PR_FALSE);
        }
        else {
          if (!(xValue & BG_CLR)) {
            // The first keyword can only be 'center', 'left', or 'right'
            return PR_FALSE;
          }
          values[4] = BackgroundPositionMaskToCSSValue(xValue, PR_TRUE);
        }
      }
      else {
        if (eCSSUnit_Enumerated == yUnit) {
          PRInt32 yValue = values[5].GetIntValue();
          if (!(yValue & BG_CTB)) {
            // The second keyword can only be 'center', 'top', or 'bottom'
            return PR_FALSE;
          }
          values[5] = BackgroundPositionMaskToCSSValue(yValue, PR_FALSE);
        }
      }
    }
  }

  // Provide missing values
  if ((found & 0x01) == 0) {
    values[0].SetIntValue(NS_STYLE_BG_COLOR_TRANSPARENT, eCSSUnit_Enumerated);
  }
  if ((found & 0x02) == 0) {
    values[1].SetNoneValue();
  }
  if ((found & 0x04) == 0) {
    values[2].SetIntValue(NS_STYLE_BG_REPEAT_XY, eCSSUnit_Enumerated);
  }
  if ((found & 0x08) == 0) {
    values[3].SetIntValue(NS_STYLE_BG_ATTACHMENT_SCROLL, eCSSUnit_Enumerated);
  }
  if ((found & 0x30) == 0) {
    values[4].SetPercentValue(0.0f);
    values[5].SetPercentValue(0.0f);
  }

  PRInt32 index;
  for (index = 0; index < numProps; ++index) {
    AppendValue(kBackgroundIDs[index], values[index]);
  }

  // Background properties not settable from the shorthand get reset to their initial value
  static const PRInt32 numResetProps = 3;
  static const nsCSSProperty kBackgroundResetIDs[numResetProps] = {
    eCSSProperty__moz_background_clip,
    eCSSProperty__moz_background_inline_policy,
    eCSSProperty__moz_background_origin
  };

  nsCSSValue initial;
  initial.SetInitialValue();
  for (index = 0; index < numResetProps; ++index) {
    AppendValue(kBackgroundResetIDs[index], initial);
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBackgroundPosition(nsresult& aErrorCode)
{
  // First try a percentage or a length value
  nsCSSValue xValue, yValue;
  if (ParseVariant(aErrorCode, xValue, VARIANT_HLP, nsnull)) {
    if (eCSSUnit_Inherit == xValue.GetUnit() ||
        eCSSUnit_Initial == xValue.GetUnit()) {  // both are inherited or both are set to initial
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        AppendValue(eCSSProperty_background_x_position, xValue);
        AppendValue(eCSSProperty_background_y_position, xValue);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
    // We have one percentage/length. Get the optional second
    // percentage/length/keyword.
    if (ParseVariant(aErrorCode, yValue, VARIANT_LP, nsnull)) {
      // We have two numbers
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        AppendValue(eCSSProperty_background_x_position, xValue);
        AppendValue(eCSSProperty_background_y_position, yValue);
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    if (ParseEnum(aErrorCode, yValue, kBackgroundXYPositionKTable)) {
      PRInt32 yVal = yValue.GetIntValue();
      if (!(yVal & BG_CTB)) {
        // The second keyword can only be 'center', 'top', or 'bottom'
        return PR_FALSE;
      }
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        yValue = BackgroundPositionMaskToCSSValue(yVal, PR_FALSE);
        AppendValue(eCSSProperty_background_x_position, xValue);
        AppendValue(eCSSProperty_background_y_position, yValue);
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    // If only one percentage or length value is given, it sets the
    // horizontal position only, and the vertical position will be 50%.
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(eCSSProperty_background_x_position, xValue);
      AppendValue(eCSSProperty_background_y_position, nsCSSValue(0.5f, eCSSUnit_Percent));
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // Now try keywords. We do this manually to allow for the first
  // appearance of "center" to apply to the either the x or y
  // position (it's ambiguous so we have to disambiguate). Each
  // allowed keyword value is assigned it's own bit. We don't allow
  // any duplicate keywords other than center. We try to get two
  // keywords but it's okay if there is only one.
  PRInt32 mask = 0;
  if (ParseEnum(aErrorCode, xValue, kBackgroundXYPositionKTable)) {
    PRInt32 bit = xValue.GetIntValue();
    mask |= bit;
    if (ParseEnum(aErrorCode, xValue, kBackgroundXYPositionKTable)) {
      bit = xValue.GetIntValue();
      if (mask & (bit & ~BG_CENTER)) {
        // Only the 'center' keyword can be duplicated.
        return PR_FALSE;
      }
      mask |= bit;
    }
    else {
      // Only one keyword.  See if we have a length or percentage.
      if (ParseVariant(aErrorCode, yValue, VARIANT_LP, nsnull)) {
        if (!(mask & BG_CLR)) {
          // The first keyword can only be 'center', 'left', or 'right'
          return PR_FALSE;
        }

        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          xValue = BackgroundPositionMaskToCSSValue(mask, PR_TRUE);
          AppendValue(eCSSProperty_background_x_position, xValue);
          AppendValue(eCSSProperty_background_y_position, yValue);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
  }

  // Check for bad input. Bad input consists of no matching keywords,
  // or pairs of x keywords or pairs of y keywords.
  if ((mask == 0) || (mask == (BG_TOP | BG_BOTTOM)) ||
      (mask == (BG_LEFT | BG_RIGHT))) {
    return PR_FALSE;
  }

  if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
    // Create style values
    xValue = BackgroundPositionMaskToCSSValue(mask, PR_TRUE);
    yValue = BackgroundPositionMaskToCSSValue(mask, PR_FALSE);
    AppendValue(eCSSProperty_background_x_position, xValue);
    AppendValue(eCSSProperty_background_y_position, yValue);
    return PR_TRUE;
  }
  return PR_FALSE;
}

// These must be in CSS order (top,right,bottom,left) for indexing to work
static const nsCSSProperty kBorderStyleIDs[] = {
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style
};
static const nsCSSProperty kBorderWidthIDs[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width
};
static const nsCSSProperty kBorderColorIDs[] = {
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color
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

PRBool CSSParserImpl::ParseBorderColor(nsresult& aErrorCode)
{
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mBorderColor,
                            kBorderColorIDs);
}

PRBool CSSParserImpl::ParseBorderSpacing(nsresult& aErrorCode)
{
  nsCSSValue  xValue;
  if (ParsePositiveVariant(aErrorCode, xValue, VARIANT_HL, nsnull)) {
    if (xValue.IsLengthUnit()) {
      // We have one length. Get the optional second length.
      nsCSSValue yValue;
      if (ParsePositiveVariant(aErrorCode, yValue, VARIANT_LENGTH, nsnull)) {
        // We have two numbers
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
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
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      mTempData.mTable.mBorderSpacing.SetBothValuesTo(xValue);
      mTempData.SetPropertyBit(eCSSProperty_border_spacing);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseBorderSide(nsresult& aErrorCode,
                                      const nsCSSProperty aPropIDs[],
                                      PRBool aSetAllSides)
{
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];

  PRInt32 found = ParseChoice(aErrorCode, values, aPropIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
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

PRBool CSSParserImpl::ParseBorderStyle(nsresult& aErrorCode)
{
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mBorderStyle,
                            kBorderStyleIDs);
}

PRBool CSSParserImpl::ParseBorderWidth(nsresult& aErrorCode)
{
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mBorderWidth,
                            kBorderWidthIDs);
}

PRBool CSSParserImpl::ParseBorderRadius(nsresult& aErrorCode)
{
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mBorderRadius,
                            kBorderRadiusIDs);
}

#ifdef ENABLE_OUTLINE
PRBool CSSParserImpl::ParseOutlineRadius(nsresult& aErrorCode)
{
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mOutlineRadius,
                            kOutlineRadiusIDs);
}
#endif

PRBool CSSParserImpl::ParseBorderColors(nsresult& aErrorCode,
                                        nsCSSValueList** aResult,
                                        nsCSSProperty aProperty)
{
  nsCSSValue value;
  if (ParseVariant(aErrorCode, value, VARIANT_HCK|VARIANT_NONE, nsCSSProps::kBorderColorKTable)) {
    nsCSSValueList* listHead = new nsCSSValueList();
    nsCSSValueList* list = listHead;
    if (!list) {
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      return PR_FALSE;
    }
    list->mValue = value;

    while (list) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        mTempData.SetPropertyBit(aProperty);
        *aResult = listHead;
        aErrorCode = NS_OK;
        return PR_TRUE;
      }
      if (ParseVariant(aErrorCode, value, VARIANT_HCK|VARIANT_NONE, nsCSSProps::kBorderColorKTable)) {
        list->mNext = new nsCSSValueList();
        list = list->mNext;
        if (list)
          list->mValue = value;
        else
          aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      }
      else
        break;
    }
    delete listHead;
  }
  return PR_FALSE;
}

PRBool
CSSParserImpl::ParseRect(nsCSSRect& aRect, nsresult& aErrorCode,
                         nsCSSProperty aPropID)
{
  nsCSSRect rect;
  PRBool result;
  if ((result = DoParseRect(rect, aErrorCode)) &&
      rect != aRect) {
    aRect = rect;
    mTempData.SetPropertyBit(aPropID);
  }
  return result;
}

PRBool
CSSParserImpl::DoParseRect(nsCSSRect& aRect, nsresult& aErrorCode)
{
  if (! GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_Ident == mToken.mType) {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(mToken.mIdent);
    switch (keyword) {
      case eCSSKeyword_auto:
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aRect.SetAllSidesTo(nsCSSValue(eCSSUnit_Auto));
          return PR_TRUE;
        }
        break;
      case eCSSKeyword_inherit:
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aRect.SetAllSidesTo(nsCSSValue(eCSSUnit_Inherit));
          return PR_TRUE;
        }
        break;
      case eCSSKeyword__moz_initial:
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
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
    if (!ExpectSymbol(aErrorCode, '(', PR_TRUE)) {
      return PR_FALSE;
    }
    NS_FOR_CSS_SIDES(side) {
      if (! ParseVariant(aErrorCode, aRect.*(nsCSSRect::sides[side]),
                         VARIANT_AL, nsnull)) {
        return PR_FALSE;
      }
      if (3 != side) {
        // skip optional commas between elements
        ExpectSymbol(aErrorCode, ',', PR_TRUE);
      }
    }
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      return PR_FALSE;
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      return PR_TRUE;
    }
  } else {
    UngetToken();
  }
  return PR_FALSE;
}

#define VARIANT_CONTENT (VARIANT_STRING | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR | \
                         VARIANT_KEYWORD)
PRBool CSSParserImpl::ParseContent(nsresult& aErrorCode)
{
  nsCSSValue  value;
  if (ParseVariant(aErrorCode, value,
                   VARIANT_CONTENT | VARIANT_INHERIT | VARIANT_NORMAL, 
                   nsCSSProps::kContentKTable)) {
    nsCSSValueList* listHead = new nsCSSValueList();
    nsCSSValueList* list = listHead;
    if (nsnull == list) {
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      return PR_FALSE;
    }
    list->mValue = value;

    while (nsnull != list) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        mTempData.SetPropertyBit(eCSSProperty_content);
        mTempData.mContent.mContent = listHead;
        aErrorCode = NS_OK;
        return PR_TRUE;
      }
      if (eCSSUnit_Inherit == value.GetUnit() ||
          eCSSUnit_Initial == value.GetUnit() ||
          eCSSUnit_Normal == value.GetUnit()) {
        // This only matters the first time through the loop.
        return PR_FALSE;
      }
      if (ParseVariant(aErrorCode, value, VARIANT_CONTENT, nsCSSProps::kContentKTable)) {
        list->mNext = new nsCSSValueList();
        list = list->mNext;
        if (nsnull != list) {
          list->mValue = value;
        }
        else {
          aErrorCode = NS_ERROR_OUT_OF_MEMORY;
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

PRBool
CSSParserImpl::SetSingleCounterValue(nsCSSCounterData** aResult,
                                     nsresult& aErrorCode,
                                     nsCSSProperty aPropID,
                                     const nsCSSValue& aValue)
{
  nsCSSCounterData* dataHead = new nsCSSCounterData();
  if (!dataHead) {
    aErrorCode = NS_ERROR_OUT_OF_MEMORY;
    return PR_FALSE;
  }
  dataHead->mCounter = aValue;
  *aResult = dataHead;
  mTempData.SetPropertyBit(aPropID);
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseCounterData(nsresult& aErrorCode,
                                       nsCSSCounterData** aResult,
                                       nsCSSProperty aPropID)
{
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  if (ident->LowerCaseEqualsLiteral("none")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      return SetSingleCounterValue(aResult, aErrorCode, aPropID,
                                   nsCSSValue(eCSSUnit_None));
    }
    return PR_FALSE;
  }
  else if (ident->LowerCaseEqualsLiteral("inherit")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      return SetSingleCounterValue(aResult, aErrorCode, aPropID,
                                   nsCSSValue(eCSSUnit_Inherit));
    }
    return PR_FALSE;
  }
  else if (ident->LowerCaseEqualsLiteral("-moz-initial")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      return SetSingleCounterValue(aResult, aErrorCode, aPropID,
                                   nsCSSValue(eCSSUnit_Initial));
    }
    return PR_FALSE;
  }
  else {
    nsCSSCounterData* dataHead = new nsCSSCounterData();
    nsCSSCounterData* data = dataHead;
    if (nsnull == data) {
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      return PR_FALSE;
    }
    data->mCounter.SetStringValue(*ident, eCSSUnit_String);

    while (nsnull != data) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        mTempData.SetPropertyBit(aPropID);
        *aResult = dataHead;
        aErrorCode = NS_OK;
        return PR_TRUE;
      }
      if (! GetToken(aErrorCode, PR_TRUE)) {
        break;
      }
      if ((eCSSToken_Number == mToken.mType) && (mToken.mIntegerValid)) {
        data->mValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          mTempData.SetPropertyBit(aPropID);
          *aResult = dataHead;
          aErrorCode = NS_OK;
          return PR_TRUE;
        }
        if (! GetToken(aErrorCode, PR_TRUE)) {
          break;
        }
      }
      if (eCSSToken_Ident == mToken.mType) {
        data->mNext = new nsCSSCounterData();
        data = data->mNext;
        if (nsnull != data) {
          data->mCounter.SetStringValue(mToken.mIdent, eCSSUnit_String);
        }
        else {
          aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        break;
      }
    }
    delete dataHead;
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseCue(nsresult& aErrorCode)
{
  nsCSSValue before;
  if (ParseSingleValueProperty(aErrorCode, before, eCSSProperty_cue_before)) {
    if (eCSSUnit_URL == before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, eCSSProperty_cue_after)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(eCSSProperty_cue_before, before);
          AppendValue(eCSSProperty_cue_after, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(eCSSProperty_cue_before, before);
      AppendValue(eCSSProperty_cue_after, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseCursor(nsresult& aErrorCode)
{
  nsCSSValueList *list = nsnull;
  for (nsCSSValueList **curp = &list, *cur; ; curp = &cur->mNext) {
    cur = *curp = new nsCSSValueList();
    if (!cur) {
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      delete list;
      return PR_FALSE;
    }
    if (!ParseVariant(aErrorCode, cur->mValue,
                      (cur == list) ? VARIANT_AHUK : VARIANT_AUK,
                      nsCSSProps::kCursorKTable)) {
      delete list;
      return PR_FALSE;
    }
    if (cur->mValue.GetUnit() != eCSSUnit_URL)
      break;
    if (!ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
      delete list;
      return PR_FALSE;
    }
  }
  if (!ExpectEndProperty(aErrorCode, PR_TRUE)) {
    delete list;
    return PR_FALSE;
  }
  mTempData.SetPropertyBit(eCSSProperty_cursor);
  mTempData.mUserInterface.mCursor = list;
  aErrorCode = NS_OK;
  return PR_TRUE;
}


PRBool CSSParserImpl::ParseFont(nsresult& aErrorCode)
{
  static const nsCSSProperty fontIDs[] = {
    eCSSProperty_font_style,
    eCSSProperty_font_variant,
    eCSSProperty_font_weight
  };

  nsCSSValue  family;
  if (ParseVariant(aErrorCode, family, VARIANT_HK, nsCSSProps::kFontKTable)) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      if (eCSSUnit_Inherit == family.GetUnit()) {
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
        AppendValue(eCSSProperty_font_family, family);  // keyword value overrides everything else
        nsCSSValue empty;
        AppendValue(eCSSProperty_font_style, empty);
        AppendValue(eCSSProperty_font_variant, empty);
        AppendValue(eCSSProperty_font_weight, empty);
        AppendValue(eCSSProperty_font_size, empty);
        AppendValue(eCSSProperty_line_height, empty);
        AppendValue(eCSSProperty_font_stretch, empty);
        AppendValue(eCSSProperty_font_size_adjust, empty);
      }
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // Get optional font-style, font-variant and font-weight (in any order)
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(aErrorCode, values, fontIDs, numProps);
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
  if (! ParseVariant(aErrorCode, size, VARIANT_KEYWORD | VARIANT_LP, nsCSSProps::kFontSizeKTable)) {
    return PR_FALSE;
  }

  // Get optional "/" line-height
  nsCSSValue  lineHeight;
  if (ExpectSymbol(aErrorCode, '/', PR_TRUE)) {
    if (! ParsePositiveVariant(aErrorCode, lineHeight,
                               VARIANT_NUMBER | VARIANT_LP | VARIANT_NORMAL,
                               nsnull)) {
      return PR_FALSE;
    }
  }
  else {
    lineHeight.SetNormalValue();
  }

  // Get final mandatory font-family
  if (ParseFamily(aErrorCode, family)) {
    if ((eCSSUnit_Inherit != family.GetUnit()) && (eCSSUnit_Initial != family.GetUnit()) &&
        ExpectEndProperty(aErrorCode, PR_TRUE)) {
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

PRBool CSSParserImpl::ParseFontWeight(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ParseVariant(aErrorCode, aValue, VARIANT_HMKI, nsCSSProps::kFontWeightKTable)) {
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

PRBool CSSParserImpl::ParseFamily(nsresult& aErrorCode, nsCSSValue& aValue)
{
  nsCSSToken* tk = &mToken;
  nsAutoString family;
  PRBool firstOne = PR_TRUE;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_Ident == tk->mType) {
      if (firstOne) {
        nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(tk->mIdent);
        if (keyword == eCSSKeyword_inherit) {
          aValue.SetInheritValue();
          return PR_TRUE;
        }
        else if (keyword == eCSSKeyword__moz_initial) {
          aValue.SetInitialValue();
          return PR_TRUE;
        }
      }
      else {
        family.Append(PRUnichar(','));
      }
      family.Append(tk->mIdent);
      for (;;) {
        if (!GetToken(aErrorCode, PR_FALSE)) {
          break;
        }
        if (eCSSToken_Ident == tk->mType) {
          family.Append(tk->mIdent);
        } else if (eCSSToken_WhiteSpace == tk->mType) {
          // Lookahead one token and drop whitespace if we ending the
          // font name.
          if (!GetToken(aErrorCode, PR_TRUE)) {
            break;
          }
          if (eCSSToken_Ident != tk->mType) {
            UngetToken();
            break;
          }
          UngetToken();
          family.Append(PRUnichar(' '));
        } else {
          UngetToken();
          break;
        }
      }
      firstOne = PR_FALSE;
    } else if (eCSSToken_String == tk->mType) {
      if (!firstOne) {
        family.Append(PRUnichar(','));
      }
      family.Append(tk->mSymbol); // replace the quotes
      family.Append(tk->mIdent);
      family.Append(tk->mSymbol);
      firstOne = PR_FALSE;
    } else if (eCSSToken_Symbol == tk->mType) {
      if (',' != tk->mSymbol) {
        UngetToken();
        break;
      }
    } else {
      UngetToken();
      break;
    }
  }
  if (family.IsEmpty()) {
    return PR_FALSE;
  }
  aValue.SetStringValue(family, eCSSUnit_String);
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseListStyle(nsresult& aErrorCode)
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty listStyleIDs[] = {
    eCSSProperty_list_style_type,
    eCSSProperty_list_style_position,
    eCSSProperty_list_style_image
  };

  nsCSSValue  values[numProps];
  PRInt32 index;
  PRInt32 found = ParseChoice(aErrorCode, values, listStyleIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
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

PRBool CSSParserImpl::ParseMargin(nsresult& aErrorCode)
{
  static const nsCSSProperty kMarginSideIDs[] = {
    eCSSProperty_margin_top,
    eCSSProperty_margin_right_value,
    eCSSProperty_margin_bottom,
    eCSSProperty_margin_left_value
  };
  // do this now, in case 4 values weren't specified
  mTempData.SetPropertyBit(eCSSProperty_margin_left_ltr_source);
  mTempData.SetPropertyBit(eCSSProperty_margin_left_rtl_source);
  mTempData.SetPropertyBit(eCSSProperty_margin_right_ltr_source);
  mTempData.SetPropertyBit(eCSSProperty_margin_right_rtl_source);
  mTempData.mMargin.mMarginLeftLTRSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mMarginLeftRTLSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mMarginRightLTRSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mMarginRightRTLSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mMargin,
                            kMarginSideIDs);
}

PRBool CSSParserImpl::ParseMarks(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ParseVariant(aErrorCode, aValue, VARIANT_HOK, nsCSSProps::kPageMarksKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {
      if (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE)) {
        nsCSSValue  second;
        if (ParseEnum(aErrorCode, second, nsCSSProps::kPageMarksKTable)) {
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

#ifdef ENABLE_OUTLINE
PRBool CSSParserImpl::ParseOutline(nsresult& aErrorCode)
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty kOutlineIDs[] = {
    eCSSProperty__moz_outline_color,
    eCSSProperty__moz_outline_style,
    eCSSProperty__moz_outline_width
  };

  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(aErrorCode, values, kOutlineIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  // Provide default values
  if ((found & 1) == 0) {
    values[0].SetIntValue(NS_STYLE_COLOR_INVERT, eCSSUnit_Enumerated);
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
#endif

PRBool CSSParserImpl::ParseOverflow(nsresult& aErrorCode)
{
  nsCSSValue overflow;
  if (!ParseVariant(aErrorCode, overflow, VARIANT_AHK,
                   nsCSSProps::kOverflowKTable) ||
      !ExpectEndProperty(aErrorCode, PR_TRUE))
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
  aErrorCode = NS_OK;
  return PR_TRUE;
}

PRBool CSSParserImpl::ParsePadding(nsresult& aErrorCode)
{
  static const nsCSSProperty kPaddingSideIDs[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right_value,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left_value
  };
  // do this now, in case 4 values weren't specified
  mTempData.SetPropertyBit(eCSSProperty_padding_left_ltr_source);
  mTempData.SetPropertyBit(eCSSProperty_padding_left_rtl_source);
  mTempData.SetPropertyBit(eCSSProperty_padding_right_ltr_source);
  mTempData.SetPropertyBit(eCSSProperty_padding_right_rtl_source);
  mTempData.mMargin.mPaddingLeftLTRSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mPaddingLeftRTLSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mPaddingRightLTRSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  mTempData.mMargin.mPaddingRightRTLSource.SetIntValue(NS_BOXPROP_SOURCE_PHYSICAL, eCSSUnit_Enumerated);
  return ParseBoxProperties(aErrorCode, mTempData.mMargin.mPadding,
                            kPaddingSideIDs);
}

PRBool CSSParserImpl::ParsePause(nsresult& aErrorCode)
{
  nsCSSValue  before;
  if (ParseSingleValueProperty(aErrorCode, before, eCSSProperty_pause_before)) {
    if (eCSSUnit_Inherit != before.GetUnit() && eCSSUnit_Initial != before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, eCSSProperty_pause_after)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(eCSSProperty_pause_before, before);
          AppendValue(eCSSProperty_pause_after, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(eCSSProperty_pause_before, before);
      AppendValue(eCSSProperty_pause_after, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseQuotes(nsresult& aErrorCode)
{
  nsCSSValue  open;
  if (ParseVariant(aErrorCode, open, VARIANT_HOS, nsnull)) {
    if (eCSSUnit_String == open.GetUnit()) {
      nsCSSQuotes* quotesHead = new nsCSSQuotes();
      nsCSSQuotes* quotes = quotesHead;
      if (nsnull == quotes) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
      quotes->mOpen = open;
      while (nsnull != quotes) {
        // get mandatory close
        if (ParseVariant(aErrorCode, quotes->mClose, VARIANT_STRING, nsnull)) {
          if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
            mTempData.SetPropertyBit(eCSSProperty_quotes);
            mTempData.mContent.mQuotes = quotesHead;
            aErrorCode = NS_OK;
            return PR_TRUE;
          }
          // look for another open
          if (ParseVariant(aErrorCode, open, VARIANT_STRING, nsnull)) {
            quotes->mNext = new nsCSSQuotes();
            quotes = quotes->mNext;
            if (nsnull != quotes) {
              quotes->mOpen = open;
              continue;
            }
            aErrorCode = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        break;
      }
      delete quotesHead;
      return PR_FALSE;
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSQuotes* quotesHead = new nsCSSQuotes();
      quotesHead->mOpen = open;
      mTempData.mContent.mQuotes = quotesHead;
      mTempData.SetPropertyBit(eCSSProperty_quotes);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseSize(nsresult& aErrorCode)
{
  nsCSSValue width;
  if (ParseVariant(aErrorCode, width, VARIANT_AHKL, nsCSSProps::kPageSizeKTable)) {
    if (width.IsLengthUnit()) {
      nsCSSValue  height;
      if (ParseVariant(aErrorCode, height, VARIANT_LENGTH, nsnull)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          mTempData.mPage.mSize.mXValue = width;
          mTempData.mPage.mSize.mYValue = height;
          mTempData.SetPropertyBit(eCSSProperty_size);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      mTempData.mPage.mSize.SetBothValuesTo(width);
      mTempData.SetPropertyBit(eCSSProperty_size);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseTextDecoration(nsresult& aErrorCode, nsCSSValue& aValue)
{
  if (ParseVariant(aErrorCode, aValue, VARIANT_HOK, nsCSSProps::kTextDecorationKTable)) {
    if (eCSSUnit_Enumerated == aValue.GetUnit()) {  // look for more keywords
      PRInt32 intValue = aValue.GetIntValue();
      nsCSSValue  keyword;
      PRInt32 index;
      for (index = 0; index < 3; index++) {
        if (ParseEnum(aErrorCode, keyword, nsCSSProps::kTextDecorationKTable)) {
          intValue |= keyword.GetIntValue();
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

PRBool CSSParserImpl::ParseTextShadow(nsresult& aErrorCode)
{
  nsCSSValue  value;
  if (ParseVariant(aErrorCode, value, VARIANT_HC | VARIANT_LENGTH | VARIANT_NONE, nsnull)) {
    nsCSSUnit unit = value.GetUnit();
    if ((eCSSUnit_Color == unit) || (eCSSUnit_String == unit) || value.IsLengthUnit()) {
      nsCSSShadow*  shadowHead = new nsCSSShadow();
      nsCSSShadow*  shadow = shadowHead;
      if (nsnull == shadow) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
      while (nsnull != shadow) {
        PRBool  haveColor = PR_FALSE;
        if (value.IsLengthUnit()) {
          shadow->mXOffset = value;
        }
        else {
          haveColor = PR_TRUE;
          shadow->mColor = value;
          if (ParseVariant(aErrorCode, value, VARIANT_LENGTH, nsnull)) {
            shadow->mXOffset = value;
          }
          else {
            break;
          }
        }
        if (ParseVariant(aErrorCode, value, VARIANT_LENGTH, nsnull)) {
          shadow->mYOffset = value;
        }
        else {
          break;
        }
        if (ParseVariant(aErrorCode, value, VARIANT_LENGTH, nsnull)) {
          shadow->mRadius = value;
        } // optional
        if (PR_FALSE == haveColor) {
          if (ParseVariant(aErrorCode, value, VARIANT_COLOR, nsnull)) {
            shadow->mColor = value;
          }
        }
        if (ExpectSymbol(aErrorCode, ',', PR_TRUE)) {
          shadow->mNext = new nsCSSShadow();
          shadow = shadow->mNext;
          if (nsnull == shadow) {
            aErrorCode = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
          if (PR_FALSE == ParseVariant(aErrorCode, value, VARIANT_COLOR | VARIANT_LENGTH, nsnull)) {
            break;
          }
        }
        else {
          if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
            mTempData.SetPropertyBit(eCSSProperty_text_shadow);
            mTempData.mText.mTextShadow = shadowHead;
            aErrorCode = NS_OK;
            return PR_TRUE;
          }
          break;
        }
      }
      delete shadowHead;
      return PR_FALSE;
    }
    // value is inherit or none
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSShadow* shadowHead = new nsCSSShadow();
      shadowHead->mXOffset = value;
      mTempData.SetPropertyBit(eCSSProperty_text_shadow);
      mTempData.mText.mTextShadow = shadowHead;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}
