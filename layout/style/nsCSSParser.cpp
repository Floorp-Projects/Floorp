/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   L. David Baron <dbaron@fas.harvard.edu>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsICSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsCSSScanner.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleRule.h"
#include "nsICSSImportRule.h"
#include "nsICSSMediaRule.h"
#include "nsICSSCharsetRule.h"
#include "nsICSSNameSpaceRule.h"
#include "nsIUnicharInputStream.h"
#include "nsIStyleSet.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSDeclaration.h"
#include "nsStyleConsts.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "nsColor.h"
#include "nsStyleConsts.h"
#include "nsCSSAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"

// XXX TODO:
// - rework aErrorCode stuff: switch over to nsresult
// verify ! is followed by important and nothing else


#define ENABLE_OUTLINE   // un-comment this to enable the outline properties (bug 9816)
                         // XXX un-commenting for temporary fix for nsbeta3+ Bug 48973
                         // so we can use "mozoutline

//#define ENABLE_COUNTERS  // un-comment this to enable counters (bug 15174)

MOZ_DECL_CTOR_COUNTER(SelectorList)

// e.g. "P B, H1 B { ... }" has a selector list with two elements,
// each of which has two selectors.
struct SelectorList {
  SelectorList(void);
  ~SelectorList(void);

  void AddSelector(const nsCSSSelector& aSelector);

#ifdef NS_DEBUG
  void Dump(void);
#endif

  nsCSSSelector*  mSelectors;
  PRInt32         mWeight;
  SelectorList*   mNext;
};

SelectorList::SelectorList(void)
  : mSelectors(nsnull),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(SelectorList);
}

SelectorList::~SelectorList()
{
  MOZ_COUNT_DTOR(SelectorList);
  nsCSSSelector*  sel = mSelectors;
  while (nsnull != sel) {
    nsCSSSelector* dead = sel;
    sel = sel->mNext;
    delete dead;
  }
  if (nsnull != mNext) {
    delete mNext;
  }
}

void SelectorList::AddSelector(const nsCSSSelector& aSelector)
{ // prepend to list
  nsCSSSelector* newSel = new nsCSSSelector(aSelector);
  if (nsnull != newSel) {
    newSel->mNext = mSelectors;
    mSelectors = newSel;
  }
}


#ifdef NS_DEBUG
void SelectorList::Dump()
{
}
#endif

//----------------------------------------------------------------------

// Your basic top-down recursive descent style parser
class CSSParserImpl : public nsICSSParser {
public:
  CSSParserImpl();
  virtual ~CSSParserImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsICSSStyleSheet* aSheet);

  NS_IMETHOD GetInfoMask(PRUint32& aResult);

  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);

  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode);

  NS_IMETHOD SetChildLoader(nsICSSLoader* aChildLoader);

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aInputURL,
                   nsICSSStyleSheet*&     aResult);

  NS_IMETHOD ParseStyleAttribute(const nsAReadableString& aAttributeValue,
                                 nsIURI*                  aBaseURL,
                                 nsIStyleRule**           aResult);
  
  NS_IMETHOD ParseAndAppendDeclaration(const nsAReadableString& aBuffer,
                                       nsIURI*                  aBaseURL,
                                       nsICSSDeclaration*       aDeclaration,
                                       PRBool                   aParseOnlyOneDecl,
                                       PRInt32*                 aHint);

  NS_IMETHOD ParseRule(nsAReadableString& aRule,
                       nsIURI*            aBaseURL,
                       nsISupportsArray** aResult);

  NS_IMETHOD GetCharset(/*out*/nsAWritableString &aCharsetDest) const;
    // sets the out-param to the current charset, as set by SetCharset
  NS_IMETHOD SetCharset(/*in*/ const nsAReadableString &aCharsetSrc);
    // NOTE: SetCharset expects the charset to be the preferred charset
    //       and it just records the string exactly as passed in (no alias resolution)
  void AppendRule(nsICSSRule* aRule);

protected:
  nsresult InitScanner(nsIUnicharInputStream* aInput, nsIURI* aURI);
  nsresult ReleaseScanner(void);

  PRBool GetToken(PRInt32& aErrorCode, PRBool aSkipWS);
  PRBool GetURLToken(PRInt32& aErrorCode, PRBool aSkipWS);
  void UngetToken();

  PRBool ExpectSymbol(PRInt32& aErrorCode, PRUnichar aSymbol, PRBool aSkipWS);
  PRBool ExpectEndProperty(PRInt32& aErrorCode, PRBool aSkipWS);
  nsString* NextIdent(PRInt32& aErrorCode);
  void SkipUntil(PRInt32& aErrorCode, PRUnichar aStopSymbol);
  void SkipRuleSet(PRInt32& aErrorCode);
  PRBool SkipAtRule(PRInt32& aErrorCode);
  PRBool SkipDeclaration(PRInt32& aErrorCode, PRBool aCheckForBraces);

  PRBool PushGroup(nsICSSGroupRule* aRule);
  void PopGroup(void);

  PRBool ParseRuleSet(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseAtRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseCharsetRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseImportRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool GatherMedia(PRInt32& aErrorCode, nsString& aMedia, nsISupportsArray* aMediaAtoms);
  PRBool ProcessImport(PRInt32& aErrorCode, const nsString& aURLSpec, const nsString& aMedia, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseMediaRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParseNameSpaceRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ProcessNameSpace(PRInt32& aErrorCode, const nsString& aPrefix, 
                          const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                          void* aProcessData);
  PRBool ParseFontFaceRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);
  PRBool ParsePageRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aProcessData);

  void ParseIDSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode);
  void ParseClassSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode);
  void ParsePseudoSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode,
                         PRBool aIsNegated);
  void ParseAttributeSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode);

  void ParseTypeOrUniversalSelector(PRInt32&  aDataMask,
                         nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode,
                         PRBool aIsNegated);
  void ParseNegatedSimpleSelector(PRInt32&  aDataMask, nsCSSSelector& aSelector,
                         PRInt32& aParsingStatus, PRInt32& aErrorCode);

  PRBool ParseSelectorList(PRInt32& aErrorCode, SelectorList*& aListHead);
  PRBool ParseSelectorGroup(PRInt32& aErrorCode, SelectorList*& aListHead);
  PRBool ParseSelector(PRInt32& aErrorCode, nsCSSSelector& aSelectorResult);
  nsICSSDeclaration* ParseDeclarationBlock(PRInt32& aErrorCode,
                                           PRBool aCheckForBraces);
  PRBool ParseDeclaration(PRInt32& aErrorCode,
                          nsICSSDeclaration* aDeclaration,
                          PRBool aCheckForBraces,
                          PRInt32& aChangeHint);
  PRBool ParseProperty(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                       nsCSSProperty aPropID, PRInt32& aChangeHint);
  PRBool ParseSingleValueProperty(PRInt32& aErrorCode, nsCSSValue& aValue, 
                                  nsCSSProperty aPropID);

#ifdef INCLUDE_XUL
  PRBool ParseOutlinerPseudoElement(PRInt32& aErrorCode, nsCSSSelector& aSelector);
#endif

  // Property specific parsing routines
  PRBool ParseAzimuth(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseBackground(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBackgroundPosition(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorder(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorderColor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorderSpacing(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorderSide(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                         const nsCSSProperty aPropIDs[], PRInt32& aChangeHint);
  PRBool ParseBorderStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorderWidth(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseBorderRadius(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
#ifdef ENABLE_OUTLINE
  PRBool ParseOutlineRadius(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
#endif
  PRBool ParseClip(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseContent(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseCounterData(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                          nsCSSProperty aPropID, PRInt32& aChangeHint);
  PRBool ParseCue(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseCursor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseFont(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseFontWeight(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseFamily(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseListStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseMargin(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseMarks(PRInt32& aErrorCode, nsCSSValue& aValue);
#ifdef ENABLE_OUTLINE
  PRBool ParseOutline(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
#endif
  PRBool ParsePadding(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParsePause(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParsePlayDuring(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseQuotes(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseSize(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);
  PRBool ParseTextDecoration(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseTextShadow(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint);

  // Reused utility parsing routines
  nsresult AppendValue(nsICSSDeclaration* aDeclaration, nsCSSProperty aPropID,
                       const nsCSSValue& aValue, PRInt32& aChangeHint);
  PRBool ParseBoxProperties(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                            const nsCSSProperty aPropIDs[], PRInt32& aChangeHint);
  PRInt32 ParseChoice(PRInt32& aErrorCode, nsCSSValue aValues[],
                      const nsCSSProperty aPropIDs[], PRInt32 aNumIDs);
  PRBool ParseColor(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseColorComponent(PRInt32& aErrorCode, PRUint8& aComponent,
                             char aStop);
  PRBool ParseEnum(PRInt32& aErrorCode, nsCSSValue& aValue, const PRInt32 aKeywordTable[]);
  PRInt32 SearchKeywordTable(nsCSSKeyword aKeyword, const PRInt32 aTable[]);
  PRBool ParseVariant(PRInt32& aErrorCode, nsCSSValue& aValue,
                      PRInt32 aVariantMask,
                      const PRInt32 aKeywordTable[]);
  PRBool ParsePositiveVariant(PRInt32& aErrorCode, nsCSSValue& aValue, 
                              PRInt32 aVariantMask, 
                              const PRInt32 aKeywordTable[]); 
  PRBool ParseCounter(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseAttr(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseURL(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool TranslateDimension(PRInt32& aErrorCode, nsCSSValue& aValue, PRInt32 aVariantMask,
                            float aNumber, const nsString& aUnit);

  // Current token. The value is valid after calling GetToken
  nsCSSToken mToken;

  // After an UngetToken is done this flag is true. The next call to
  // GetToken clears the flag.
  PRBool mHavePushBack;

  nsCSSScanner* mScanner;
  nsIURI* mURL;
  nsICSSStyleSheet* mSheet;
  PRInt32 mChildSheetCount;
  nsICSSLoader* mChildLoader; // not ref counted, it owns us

  enum nsCSSSection { 
    eCSSSection_Charset, 
    eCSSSection_Import, 
    eCSSSection_NameSpace,
    eCSSSection_General 
  };

  nsCSSSection  mSection;

  PRBool  mNavQuirkMode;
  PRBool  mCaseSensitive;

  nsINameSpace* mNameSpace;

  nsISupportsArray* mGroupStack;

  nsString      mCharset;        // the charset we are using

  PRBool  mParsingCompoundProperty;
  void    SetParsingCompoundProperty(PRBool aBool) {mParsingCompoundProperty = aBool;};
  PRBool  IsParsingCompoundProperty(void) {return mParsingCompoundProperty;};
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

NS_HTML nsresult
NS_NewCSSParser(nsICSSParser** aInstancePtrResult)
{
  CSSParserImpl *it = new CSSParserImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsICSSParser), (void **) aInstancePtrResult);
}

#ifdef CSS_REPORT_PARSE_ERRORS

#define REPORT_UNEXPECTED(_err) \
  mScanner->AddToError(_err)

#define REPORT_UNEXPECTED_EOF() \
  mScanner->AddToError(NS_LITERAL_STRING("Unexpected end of file"))

#define REPORT_UNEXPECTED_TOKEN(_reas) \
  ReportUnexpectedToken(mScanner,mToken,_reas)

#define OUTPUT_ERROR() \
  mScanner->OutputError()

#define CLEAR_ERROR() \
  mScanner->ClearError()
  

static void ReportUnexpectedToken(nsCSSScanner *sc,
                                  nsCSSToken& tok,
                                  const nsAReadableString& err)
{
  nsAutoString error(err + NS_LITERAL_STRING(" '"));
  tok.AppendToString(error);
  error += NS_LITERAL_STRING("'.");
  sc->AddToError(error);
}

#else

#define REPORT_UNEXPECTED(_err)
#define REPORT_UNEXPECTED_EOF()
#define REPORT_UNEXPECTED_TOKEN(_reas)
#define OUTPUT_ERROR()
#define CLEAR_ERROR()

#endif

CSSParserImpl::CSSParserImpl()
  : mToken(),
    mHavePushBack(PR_FALSE),
    mScanner(nsnull),
    mURL(nsnull),
    mSheet(nsnull),
    mChildSheetCount(0),
    mChildLoader(nsnull),
    mSection(eCSSSection_Charset),
    mNavQuirkMode(PR_FALSE),
    mCaseSensitive(PR_FALSE),
    mNameSpace(nsnull),
    mGroupStack(nsnull),
    mParsingCompoundProperty(PR_FALSE)
{
  NS_INIT_REFCNT();

  // set the default charset
  mCharset.AssignWithConversion("ISO-8859-1");
}

NS_IMETHODIMP
CSSParserImpl::Init(nsICSSStyleSheet* aSheet)
{
  NS_IF_RELEASE(mGroupStack);
  NS_IF_RELEASE(mNameSpace);
  NS_IF_RELEASE(mSheet);
  mSheet = aSheet;
  if (mSheet) {
    NS_ADDREF(aSheet);
    mSheet->StyleSheetCount(mChildSheetCount);
    mSheet->GetNameSpace(mNameSpace);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(CSSParserImpl, nsICSSParser)

CSSParserImpl::~CSSParserImpl()
{
  NS_IF_RELEASE(mGroupStack);
  NS_IF_RELEASE(mNameSpace);
  NS_IF_RELEASE(mSheet);
}

NS_IMETHODIMP
CSSParserImpl::GetInfoMask(PRUint32& aResult)
{
  aResult = NS_CSS_GETINFO_CSS1 | NS_CSS_GETINFO_CSSP;
  return NS_OK;
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
    NS_IF_RELEASE(mGroupStack);
    NS_IF_RELEASE(mNameSpace);
    NS_IF_RELEASE(mSheet);
    mSheet = aSheet;
    NS_ADDREF(mSheet);
    mSheet->StyleSheetCount(mChildSheetCount);
    mSheet->GetNameSpace(mNameSpace);
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

NS_IMETHODIMP
CSSParserImpl::SetChildLoader(nsICSSLoader* aChildLoader)
{
  mChildLoader = aChildLoader;  // not ref counted, it owns us
  return NS_OK;
}

nsresult
CSSParserImpl::InitScanner(nsIUnicharInputStream* aInput, nsIURI* aURI)
{
  NS_ASSERTION(! mScanner, "already have scanner");

  mScanner = new nsCSSScanner();
  if (! mScanner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->Init(aInput, aURI);
  NS_IF_RELEASE(mURL);
  mURL = aURI;
  NS_IF_ADDREF(mURL);

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
  NS_IF_RELEASE(mURL);
  return NS_OK;
}


NS_IMETHODIMP
CSSParserImpl::Parse(nsIUnicharInputStream* aInput,
                     nsIURI*                aInputURL,
                     nsICSSStyleSheet*&     aResult)
{
  NS_ASSERTION(nsnull != aInputURL, "need base URL");

  if (! mSheet) {
    NS_NewCSSStyleSheet(&mSheet, aInputURL);
    mChildSheetCount = 0;
  }
  if (! mSheet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 errorCode = NS_OK;

  nsresult result = InitScanner(aInput, aInputURL);
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
CSSParserImpl::ParseStyleAttribute(const nsAReadableString& aAttributeValue,
                                   nsIURI*                  aBaseURL,
                                   nsIStyleRule**           aResult)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

  // XXXldb XXXperf nsIUnicharInputStream is horrible!  It makes us make
  // a copy.
  nsString* str = new nsString(aAttributeValue);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIUnicharInputStream* input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(&input, str);
  if (NS_OK != rv) {
    delete str;
    return rv;
  }

  rv = InitScanner(input, aBaseURL);
  NS_RELEASE(input);
  if (! NS_SUCCEEDED(rv)) {
    return rv;
  }

  mSection = eCSSSection_General;
  PRInt32 errorCode = NS_OK;

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

  nsICSSDeclaration* declaration =
      ParseDeclarationBlock(errorCode, haveBraces);
  if (nsnull != declaration) {
    // Create a style rule for the delcaration
    nsICSSStyleRule* rule = nsnull;
    NS_NewCSSStyleRule(&rule, nsCSSSelector());
    rule->SetDeclaration(declaration);
    *aResult = rule;
    NS_RELEASE(declaration);
  }
  else {
    *aResult = nsnull;
  }

  ReleaseScanner();

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseAndAppendDeclaration(const nsAReadableString& aBuffer,
                                         nsIURI*                  aBaseURL,
                                         nsICSSDeclaration*       aDeclaration,
                                         PRBool                   aParseOnlyOneDecl,
                                         PRInt32*                 aHint)
{
//  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

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

  rv = InitScanner(input, aBaseURL);
  NS_RELEASE(input);
  if (! NS_SUCCEEDED(rv)) {
    return rv;
  }

  mSection = eCSSSection_General;
  PRInt32 errorCode = NS_OK;

  PRInt32 hint = NS_STYLE_HINT_NONE;

  if (nsnull != aHint) {
    *aHint = hint;
  }

  do {
    if (ParseDeclaration(errorCode, aDeclaration, PR_FALSE, hint)) {
      if (aHint && hint > *aHint) {
        *aHint = hint;
      }
    } else {
      if (errorCode != -1) { // -1 means EOF so we ignore that
        rv = errorCode;
      }

      if (NS_FAILED(errorCode))
        break;
    }
  } while (!aParseOnlyOneDecl);

  ReleaseScanner();
  return rv;
}

NS_IMETHODIMP
CSSParserImpl::ParseRule(nsAReadableString& aRule,
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

  rv = InitScanner(input, aBaseURL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_NewISupportsArray(aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  mSection = eCSSSection_Charset; // callers are responsible for rejecting invalid rules.
  PRInt32 errorCode = NS_OK;

  nsCSSToken* tk = &mToken;
  // Get first non-whitespace token
  if (!GetToken(errorCode, PR_TRUE)) {
    REPORT_UNEXPECTED(
      NS_LITERAL_STRING("Whitespace-only string given to be parsed as rule."));
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

//----------------------------------------------------------------------

PRBool CSSParserImpl::GetToken(PRInt32& aErrorCode, PRBool aSkipWS)
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

PRBool CSSParserImpl::GetURLToken(PRInt32& aErrorCode, PRBool aSkipWS)
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

PRBool CSSParserImpl::ExpectSymbol(PRInt32& aErrorCode,
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

PRBool CSSParserImpl::ExpectEndProperty(PRInt32& aErrorCode, PRBool aSkipWS)
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
  REPORT_UNEXPECTED_TOKEN(
    NS_LITERAL_STRING("Expected end of value for property but found"));
  UngetToken();
  return PR_FALSE;
}


nsString* CSSParserImpl::NextIdent(PRInt32& aErrorCode)
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

PRBool CSSParserImpl::SkipAtRule(PRInt32& aErrorCode)
{
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF();
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

PRBool CSSParserImpl::ParseAtRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc,
                                  void* aData)
{
  if ((mSection <= eCSSSection_Charset) && 
      (mToken.mIdent.EqualsIgnoreCase("charset"))) {
    if (ParseCharsetRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_Import;  // only one charset allowed
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_Import) && 
      mToken.mIdent.EqualsIgnoreCase("import")) {
    if (ParseImportRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_Import;
      return PR_TRUE;
    }
  }
  if ((mSection <= eCSSSection_NameSpace) && 
      mToken.mIdent.EqualsIgnoreCase("namespace")) {
    if (ParseNameSpaceRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_NameSpace;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.EqualsIgnoreCase("media")) {
    if (ParseMediaRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.EqualsIgnoreCase("font-face")) {
    if (ParseFontFaceRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  if (mToken.mIdent.EqualsIgnoreCase("page")) {
    if (ParsePageRule(aErrorCode, aAppendFunc, aData)) {
      mSection = eCSSSection_General;
      return PR_TRUE;
    }
  }
  REPORT_UNEXPECTED_TOKEN(
    NS_LITERAL_STRING("Unrecognized at-rule or error parsing at-rule"));
  OUTPUT_ERROR();

  // Skip over unsupported at rule, don't advance section
  return SkipAtRule(aErrorCode);
}

PRBool CSSParserImpl::ParseCharsetRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc,
                                       void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

PRBool CSSParserImpl::GatherMedia(PRInt32& aErrorCode, nsString& aMedia, 
                                  nsISupportsArray* aMediaAtoms)
{
  PRBool first = PR_TRUE;
  PRBool expectIdent = PR_TRUE;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF();
      break;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      PRUnichar symbol = mToken.mSymbol;
      if ((';' == symbol) || ('{' == symbol)) {
        UngetToken();
        return PR_TRUE;
      } else if (',' != symbol) {
        REPORT_UNEXPECTED_TOKEN(
          NS_LITERAL_STRING("Expected ',' in media list but found"));
        UngetToken();
        break;
      } else if (expectIdent) {
        REPORT_UNEXPECTED_TOKEN(
          NS_LITERAL_STRING("Expected identifier in media list but found"));
        UngetToken();
        break;
      }
      else {
        expectIdent = PR_TRUE;
      }
    }
    else if (eCSSToken_Ident == mToken.mType) {
      if (expectIdent) {
        if (! first) {
          aMedia.AppendWithConversion(',');
        }
        mToken.mIdent.ToLowerCase();  // case insensitive from CSS - must be lower cased
        if (aMediaAtoms) {
          nsIAtom* medium = NS_NewAtom(mToken.mIdent);
          aMediaAtoms->AppendElement(medium);
          NS_RELEASE(medium);
        }
        aMedia.Append(mToken.mIdent);
        first = PR_FALSE;
        expectIdent = PR_FALSE;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(
          NS_LITERAL_STRING("Expected ',' in media list but found"));
        UngetToken();
        break;
      }
    }
    else {
      REPORT_UNEXPECTED_TOKEN(
        expectIdent
          ? NS_LITERAL_STRING("Expected identifier in media list but found")
          : NS_LITERAL_STRING("Expected ',' in media list but found"));
      UngetToken();
      break;
    }
  }
  aMedia.Truncate();
  if (aMediaAtoms) {
    aMediaAtoms->Clear();
  }
  return PR_FALSE;
}

// Parse a CSS2 import rule: "@import STRING | URL [medium [, mdeium]]"
PRBool CSSParserImpl::ParseImportRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF();
    return PR_FALSE;
  }
  nsAutoString url;
  nsAutoString media;

  if (eCSSToken_String == mToken.mType) {
    url = mToken.mIdent;
    if (GatherMedia(aErrorCode, media, nsnull)) {
      if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
        ProcessImport(aErrorCode, url, media, aAppendFunc, aData);
        return PR_TRUE;
      }
    }
  }
  else if ((eCSSToken_Function == mToken.mType) && 
           (mToken.mIdent.EqualsIgnoreCase("url"))) {
    if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
      if (GetURLToken(aErrorCode, PR_TRUE)) {
        if ((eCSSToken_String == mToken.mType) || (eCSSToken_URL == mToken.mType)) {
          url = mToken.mIdent;
          if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
            if (GatherMedia(aErrorCode, media, nsnull)) {
              if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
                ProcessImport(aErrorCode, url, media, aAppendFunc, aData);
                return PR_TRUE;
              }
            }
          }
        }
      }
    }
  }
  REPORT_UNEXPECTED_TOKEN(
    NS_LITERAL_STRING("Unexpected token within @import:"));
  // don't advance section, simply ignore invalid @import
  return PR_FALSE;
}


PRBool CSSParserImpl::ProcessImport(PRInt32& aErrorCode, const nsString& aURLSpec, const nsString& aMedia, RuleAppendFunc aAppendFunc, void* aData)
{
  nsCOMPtr<nsICSSImportRule> rule;
  aErrorCode = NS_NewCSSImportRule(getter_AddRefs(rule), aURLSpec, aMedia);
  if (NS_FAILED(aErrorCode)) {
    return PR_FALSE;
  }
  (*aAppendFunc)(rule, aData);

  if (mChildLoader) {
    // XXX probably need a way to encode unicode junk for the part of
    // the url that follows a "?"
    nsCOMPtr<nsIURI> url;
    // XXX need to have nsILoadGroup passed in here
    aErrorCode = NS_NewURI(getter_AddRefs(url), aURLSpec, mURL/*, group*/);

    if (NS_FAILED(aErrorCode)) {
      // import url is bad
      // XXX log this somewhere for easier web page debugging
      return PR_FALSE;
    }

    PRBool bContains = PR_FALSE;
    if (NS_SUCCEEDED(mSheet->ContainsStyleSheet(url,bContains)) && 
        bContains != PR_TRUE ) { // don't allow circular references
      mChildLoader->LoadChildSheet(mSheet, url, aMedia, kNameSpaceID_Unknown, mChildSheetCount++, rule);
    }
  }
  
  return PR_TRUE;
}

// Parse a CSS2 media rule: "@media medium [, medium] { ... }"
PRBool CSSParserImpl::ParseMediaRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc,
                                     void* aData)
{
  nsAutoString  mediaStr;
  nsCOMPtr<nsISupportsArray> media;
  NS_NewISupportsArray(getter_AddRefs(media));
  if (media) {
    if (GatherMedia(aErrorCode, mediaStr, media)) {
      if ((0 < mediaStr.Length()) &&
          ExpectSymbol(aErrorCode, '{', PR_TRUE)) {
        // push media rule on stack, loop over children
        nsCOMPtr<nsICSSMediaRule>  rule;
        NS_NewCSSMediaRule(getter_AddRefs(rule));
        if (rule) {
          if (PushGroup(rule)) {

            nsCSSSection holdSection = mSection;
            mSection = eCSSSection_General;

            for (;;) {
              // Get next non-whitespace token
              if (! GetToken(aErrorCode, PR_TRUE)) {
                REPORT_UNEXPECTED_EOF();
                break;
              }
              if (mToken.IsSymbol('}')) { // done!
                UngetToken();
                break;
              }
              if (eCSSToken_AtKeyword == mToken.mType) {
                SkipAtRule(aErrorCode); // @media cannot contain @rules
                continue;
              }
              UngetToken();
              ParseRuleSet(aErrorCode, AppendRuleToSheet, this);
            }
            PopGroup();

            if (ExpectSymbol(aErrorCode, '}', PR_TRUE)) {
              //  Append first, so when we do SetMedia() the rule
              //  knows what its stylesheet is.
              (*aAppendFunc)(rule, aData);
              rule->SetMedia(media);
              return PR_TRUE;
            }
            mSection = holdSection;
          }
        }
        else {  // failed to create rule, backup and skip block
          UngetToken();
        }
      }
    }
  }

  return PR_FALSE;
}

// Parse a CSS3 namespace rule: "@namespace [prefix] STRING | URL;"
PRBool CSSParserImpl::ParseNameSpaceRule(PRInt32& aErrorCode,
                                         RuleAppendFunc aAppendFunc,
                                         void* aData)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF();
    return PR_FALSE;
  }

  nsAutoString  prefix;
  nsAutoString  url;

  if (eCSSToken_Ident == mToken.mType) {
    prefix = mToken.mIdent;
    prefix.ToLowerCase(); // always case insensitive, since stays within CSS
    if (! GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF();
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
           (mToken.mIdent.EqualsIgnoreCase("url"))) {
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
  REPORT_UNEXPECTED_TOKEN(
    NS_LITERAL_STRING("Unexpected token within @namespace:"));

  return PR_FALSE;
}

PRBool CSSParserImpl::ProcessNameSpace(PRInt32& aErrorCode, const nsString& aPrefix, 
                                       const nsString& aURLSpec, RuleAppendFunc aAppendFunc,
                                       void* aData)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsICSSNameSpaceRule> rule;
  nsCOMPtr<nsIAtom> prefix;

  if (0 < aPrefix.Length()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
  }

  NS_NewCSSNameSpaceRule(getter_AddRefs(rule), prefix, aURLSpec);
  if (rule) {
    (*aAppendFunc)(rule, aData);
    NS_IF_RELEASE(mNameSpace);
    mSheet->GetNameSpace(mNameSpace);
  }

  return result;
}

PRBool CSSParserImpl::ParseFontFaceRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

PRBool CSSParserImpl::ParsePageRule(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // XXX not yet implemented
  return PR_FALSE;
}

void CSSParserImpl::SkipUntil(PRInt32& aErrorCode, PRUnichar aStopSymbol)
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
CSSParserImpl::SkipDeclaration(PRInt32& aErrorCode, PRBool aCheckForBraces)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF();
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

void CSSParserImpl::SkipRuleSet(PRInt32& aErrorCode)
{
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      REPORT_UNEXPECTED_EOF();
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
  if (! mGroupStack) {
    NS_NewISupportsArray(&mGroupStack);
  }
  if (mGroupStack) {
    mGroupStack->AppendElement(aRule);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void CSSParserImpl::PopGroup(void)
{
  if (mGroupStack) {
    PRUint32 count;
    mGroupStack->Count(&count);
    if (0 < count) {
      mGroupStack->RemoveElementAt(count - 1);
    }
  }
}

void CSSParserImpl::AppendRule(nsICSSRule* aRule)
{
  PRUint32 count = 0;
  if (mGroupStack) {
    mGroupStack->Count(&count);
  }
  if (0 < count) {
    nsICSSGroupRule* group = (nsICSSGroupRule*)mGroupStack->ElementAt(count - 1);
    group->AppendStyleRule(aRule);
    NS_RELEASE(group);
  }
  else {
    mSheet->AppendStyleRule(aRule);
  }
}

PRBool CSSParserImpl::ParseRuleSet(PRInt32& aErrorCode, RuleAppendFunc aAppendFunc, void* aData)
{
  // First get the list of selectors for the rule
  SelectorList* slist = nsnull;
  PRUint32 linenum = mScanner->GetLineNumber();
  if (! ParseSelectorList(aErrorCode, slist)) {
    REPORT_UNEXPECTED(
      NS_LITERAL_STRING("Ruleset ignored due to bad selector."));
    OUTPUT_ERROR();
    SkipRuleSet(aErrorCode);
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != slist, "null selector list");
  CLEAR_ERROR();

  // Next parse the declaration block
  nsICSSDeclaration* declaration = ParseDeclarationBlock(aErrorCode, PR_TRUE);
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

  SelectorList* list = slist;

  while (nsnull != list) {
    nsICSSStyleRule* rule = nsnull;

    NS_NewCSSStyleRule(&rule, *(list->mSelectors));
    if (nsnull != rule) {
      if (nsnull != list->mSelectors->mNext) { // hand off other selectors to new rule
        nsCSSSelector* ruleFirst = rule->FirstSelector();
        ruleFirst->mNext = list->mSelectors->mNext;
        list->mSelectors->mNext = nsnull;
      }
      rule->SetLineNumber(linenum);
      rule->SetDeclaration(declaration);
      rule->SetWeight(list->mWeight);
//      rule->List();
      (*aAppendFunc)(rule, aData);
      NS_RELEASE(rule);
    }

    list = list->mNext;
  }

  // Release temporary storage
  delete slist;
  NS_RELEASE(declaration);
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseSelectorList(PRInt32& aErrorCode,
                                        SelectorList*& aListHead)
{
  SelectorList* list = nsnull;
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
      REPORT_UNEXPECTED_EOF();
      break;
    }

    if (eCSSToken_Symbol == tk->mType) {
      if (',' == tk->mSymbol) {
        SelectorList* newList = nsnull;
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
    REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected ',' or '{' but found"));
    UngetToken();
    break;
  }

  delete aListHead;
  aListHead = nsnull;
  return PR_FALSE;
}

static PRBool IsPseudoClass(const nsIAtom* aAtom)
{
  return PRBool((nsCSSAtoms::activePseudo == aAtom) || 
                (nsCSSAtoms::anyLinkPseudo == aAtom) ||
                (nsCSSAtoms::checkedPseudo == aAtom) ||
                (nsCSSAtoms::disabledPseudo == aAtom) ||
                (nsCSSAtoms::dragOverPseudo == aAtom) ||
                (nsCSSAtoms::dragPseudo == aAtom) ||
                (nsCSSAtoms::enabledPseudo == aAtom) ||
                (nsCSSAtoms::emptyPseudo == aAtom) ||
                (nsCSSAtoms::firstChildPseudo == aAtom) ||
                (nsCSSAtoms::firstNodePseudo == aAtom) ||
                (nsCSSAtoms::focusPseudo == aAtom) ||
                (nsCSSAtoms::hoverPseudo == aAtom) ||
                (nsCSSAtoms::langPseudo == aAtom) ||
                (nsCSSAtoms::lastChildPseudo == aAtom) ||
                (nsCSSAtoms::lastNodePseudo == aAtom) ||
                (nsCSSAtoms::linkPseudo == aAtom) ||
                (nsCSSAtoms::rootPseudo == aAtom) ||
                (nsCSSAtoms::xblBoundElementPseudo == aAtom) ||
                (nsCSSAtoms::visitedPseudo == aAtom));
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

#ifdef INCLUDE_XUL
static PRBool IsOutlinerPseudoElement(const nsString& aPseudo)
{
  return !(aPseudo.CompareWithConversion("-moz-outliner-", PR_FALSE, 14));
}

static PRBool IsOutlinerPseudoElement(nsIAtom* aPseudo)
{
  nsAutoString str;
  aPseudo->ToString(str);
  return !(str.CompareWithConversion(":-moz-outliner-", PR_FALSE, 15));
}
#endif

PRBool CSSParserImpl::ParseSelectorGroup(PRInt32& aErrorCode,
                                         SelectorList*& aList)
{
  SelectorList* list = nsnull;
  PRUnichar     combinator = PRUnichar(0);
  PRInt32       weight = 0;
  PRBool        havePseudoElement = PR_FALSE;
  for (;;) {
    nsCSSSelector selector;
    if (! ParseSelector(aErrorCode, selector)) {
      break;
    }
    if (nsnull == list) {
      list = new SelectorList();
      if (nsnull == list) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
    }
    list->AddSelector(selector);
    nsCSSSelector* listSel = list->mSelectors;

    // pull out pseudo elements here
    nsAtomList* prevList = nsnull;
    nsAtomList* pseudoClassList = listSel->mPseudoClassList;
    while (nsnull != pseudoClassList) {
      if (! IsPseudoClass(pseudoClassList->mAtom)) {
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
          if (IsOutlinerPseudoElement(selector.mTag)) {
            // Take the remaining "pseudoclasses" that we parsed
            // inside the outliner pseudoelement's ()-list, and
            // make our new selector have these pseudoclasses
            // in its pseudoclass list.
            selector.mPseudoClassList = pseudoClassList->mNext;
            pseudoClassList->mNext = nsnull;
          }
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
    if (GetToken(aErrorCode, PR_TRUE)) {
      if ((eCSSToken_Symbol == mToken.mType) && 
          (('+' == mToken.mSymbol) || ('>' == mToken.mSymbol))) {
        combinator = mToken.mSymbol;
        list->mSelectors->SetOperator(combinator);
      }
      else {
        UngetToken(); // give it back to selector
      }
    }

    if (havePseudoElement) {
      break;
    }
    else {
      weight += selector.CalcWeight();
    }
  }
  if (!list) {
    REPORT_UNEXPECTED(NS_LITERAL_STRING("Selector expected"));
  }
  if (PRUnichar(0) != combinator) { // no dangling combinators
    if (list) {
      delete list;
    }
    list = nsnull;
    REPORT_UNEXPECTED(NS_LITERAL_STRING("Dangling combinator"));
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
                                    PRInt32& aErrorCode)
{
  if (0 < mToken.mIdent.Length()) { // verify is legal ID
    aDataMask |= SEL_MASK_ID;
    aSelector.AddID(mToken.mIdent);
  }
  else {
    REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING(
      "Expected non-empty name for ID selector, but found"));
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
                                        PRInt32& aErrorCode)
{
  if (! GetToken(aErrorCode, PR_FALSE)) { // get ident
    REPORT_UNEXPECTED_EOF();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }
  if (eCSSToken_Ident != mToken.mType) {  // malformed selector
    REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier for class selector but found"));
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
                                      PRInt32& aErrorCode,
                                      PRBool aIsNegated)
{
  nsAutoString buffer;
  if (mToken.IsSymbol('*')) {  // universal element selector, or universal namespace
    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // was namespace
      aDataMask |= SEL_MASK_NSPACE;
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // namespace wildcard

      if (! GetToken(aErrorCode, PR_FALSE)) {
        REPORT_UNEXPECTED_EOF();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;
        if (mCaseSensitive) {
          aSelector.SetTag(mToken.mIdent);
        }
        else {
          mToken.mIdent.ToLowerCase(buffer);
          aSelector.SetTag(buffer);
        }
      }
      else if (mToken.IsSymbol('*')) {  // universal selector
        aDataMask |= SEL_MASK_ELEM;
        // don't set tag
      }
      else {
        REPORT_UNEXPECTED_TOKEN(
          NS_LITERAL_STRING("Expected element name or '*' but found"));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // was universal element selector
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
      if (mNameSpace) { // look for default namespace
        nsINameSpace* defaultNameSpace = nsnull;
        mNameSpace->FindNameSpace(nsnull, defaultNameSpace);
        if (defaultNameSpace) {
          PRInt32 defaultID;
          defaultNameSpace->GetNameSpaceID(defaultID);
          aSelector.SetNameSpace(defaultID);
          NS_RELEASE(defaultNameSpace);
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
        nsIAtom* prefix;
        buffer.ToLowerCase(); // always case insensitive, since stays within CSS
        prefix = NS_NewAtom(buffer);
        mNameSpace->FindNameSpaceID(prefix, nameSpaceID);
        NS_IF_RELEASE(prefix);
      } // else, no delcared namespaces
      if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Unknown namespace prefix"));
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      aSelector.SetNameSpace(nameSpaceID);

      if (! GetToken(aErrorCode, PR_FALSE)) {
        REPORT_UNEXPECTED_EOF();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) {  // element name
        aDataMask |= SEL_MASK_ELEM;
        if (mCaseSensitive) {
          aSelector.SetTag(mToken.mIdent);
        }
        else {
          mToken.mIdent.ToLowerCase(buffer);
          aSelector.SetTag(buffer);
        }
      }
      else if (mToken.IsSymbol('*')) {  // universal selector
        aDataMask |= SEL_MASK_ELEM;
        // don't set tag
      }
      else {
        REPORT_UNEXPECTED_TOKEN(
            NS_LITERAL_STRING("Expected element name or '*' but found"));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // was element name
      aSelector.SetNameSpace(kNameSpaceID_Unknown); // wildcard
      if (mNameSpace) { // look for default namespace
        nsINameSpace* defaultNameSpace = nsnull;
        mNameSpace->FindNameSpace(nsnull, defaultNameSpace);
        if (defaultNameSpace) {
          PRInt32 defaultID;
          defaultNameSpace->GetNameSpaceID(defaultID);
          aSelector.SetNameSpace(defaultID);
          NS_RELEASE(defaultNameSpace);
        }
      }
      if (mCaseSensitive) {
        aSelector.SetTag(buffer);
      }
      else {
        buffer.ToLowerCase();
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
      REPORT_UNEXPECTED_EOF();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (eCSSToken_Ident == mToken.mType) {  // element name
      aDataMask |= SEL_MASK_ELEM;
      if (mCaseSensitive) {
        aSelector.SetTag(mToken.mIdent);
      }
      else {
        mToken.mIdent.ToLowerCase(buffer);
        aSelector.SetTag(buffer);
      }
    }
    else if (mToken.IsSymbol('*')) {  // universal selector
      aDataMask |= SEL_MASK_ELEM;
      // don't set tag
    }
    else {
      REPORT_UNEXPECTED_TOKEN(
        NS_LITERAL_STRING("Expected element name or '*' but found"));
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
      nsINameSpace* defaultNameSpace = nsnull;
      mNameSpace->FindNameSpace(nsnull, defaultNameSpace);
      if (defaultNameSpace) {
        PRInt32 defaultID;
        defaultNameSpace->GetNameSpaceID(defaultID);
        aSelector.SetNameSpace(defaultID);
        NS_RELEASE(defaultNameSpace);
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
                                      PRInt32& aErrorCode)
{
  if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  PRInt32 nameSpaceID = kNameSpaceID_None;
  nsAutoString  attr;
  if (mToken.IsSymbol('*')) { // wildcard namespace
    nameSpaceID = kNameSpaceID_Unknown;
    if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {
      if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
        REPORT_UNEXPECTED_EOF();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier for attribute name but found"));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
       }
    }
    else {
      REPORT_UNEXPECTED_TOKEN(
          NS_LITERAL_STRING("Expected '|' but found"));
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  }
  else if (mToken.IsSymbol('|')) { // NO namespace
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
      REPORT_UNEXPECTED_EOF();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (eCSSToken_Ident == mToken.mType) { // attr name
      attr = mToken.mIdent;
    }
    else {
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier for attribute name but found"));
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
        nsIAtom* prefix;
        attr.ToLowerCase(); // always case insensitive, since stays within CSS
        prefix = NS_NewAtom(attr);
        mNameSpace->FindNameSpaceID(prefix, nameSpaceID);
        NS_IF_RELEASE(prefix);
      } // else, no delcared namespaces
      if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
        REPORT_UNEXPECTED_TOKEN(
        NS_LITERAL_STRING("Unknown namespace prefix"));
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
        REPORT_UNEXPECTED_EOF();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if (eCSSToken_Ident == mToken.mType) { // attr name
        attr = mToken.mIdent;
      }
      else {
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier for attribute name but found"));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
  }
  else {  // malformed
    REPORT_UNEXPECTED_TOKEN(
      NS_LITERAL_STRING("Expected attribute name or namespace but found"));
    UngetToken();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  if (! mCaseSensitive) {
    attr.ToLowerCase();
  }
  if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
    REPORT_UNEXPECTED_EOF();
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
      REPORT_UNEXPECTED_TOKEN(
        NS_LITERAL_STRING("Unexpected token in attribute selector: "));
      UngetToken(); // bad function
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (NS_ATTR_FUNC_SET != func) { // get value
      if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
        REPORT_UNEXPECTED_EOF();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
      if ((eCSSToken_Ident == mToken.mType) || (eCSSToken_String == mToken.mType)) {
        nsAutoString  value(mToken.mIdent);
        if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
          REPORT_UNEXPECTED_EOF();
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
        }
        if (mToken.IsSymbol(']')) {
          PRBool isCaseSensitive = mCaseSensitive;
          if (nameSpaceID == kNameSpaceID_None || nameSpaceID == kNameSpaceID_HTML) {
            static const char* caseSensitiveHTMLAttribute[] = {
              // list based on http://www.w3.org/TR/REC-html40/index/attributes.html
              "abbr",          "alt",        "label",
              "prompt",        "standby",     "summary",
              "title",         nsnull
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
          REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected ']' to terminate attribute selector but found"));
          UngetToken();
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
        }
      }
      else {
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier or string for value in attribute selector but found"));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(
      NS_LITERAL_STRING("Unexpected token in attribute selector: "));
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
                                      PRInt32& aErrorCode,
                                      PRBool aIsNegated)
{
  nsAutoString buffer;
  if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
    REPORT_UNEXPECTED_EOF();
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    return;
  }

  buffer.Truncate();
  buffer.AppendWithConversion(':');
  buffer.Append(mToken.mIdent);
  buffer.ToLowerCase();
  nsIAtom* pseudo = NS_NewAtom(buffer);

  if (eCSSToken_Ident != mToken.mType) {  // malformed selector
    if (eCSSToken_Function != mToken.mType ||
        !(
#ifdef INCLUDE_XUL
          // -moz-outliner is a pseudo-element and therefore cannot be negated
          (!aIsNegated && IsOutlinerPseudoElement(mToken.mIdent)) ||
#endif
          // the negation pseudo-class is a function
          (nsCSSAtoms::notPseudo == pseudo))) {
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected identifier for pseudo-class selector not found"));
      NS_RELEASE(pseudo);
      UngetToken();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
#ifdef INCLUDE_XUL
    }
#endif
  }

  if (nsCSSAtoms::notPseudo == pseudo) {
    NS_RELEASE(pseudo);
    if (aIsNegated) { // :not() can't be itself negated
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Negation pseudo-class can't be negated"));
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    // CSS 3 Negation pseudo-class takes one simple selector as argument
    ParseNegatedSimpleSelector(aDataMask, aSelector, aParsingStatus, aErrorCode);
    if (SELECTOR_PARSING_ENDED_OK != aParsingStatus) {
      return;
    }
  }    
  else if (IsPseudoClass(pseudo)) {
    // XXX parse pseudo classes accepting arguments
    aDataMask |= SEL_MASK_PCLASS;
    aSelector.AddPseudoClass(pseudo);
    NS_RELEASE(pseudo);
  }
  else {
    if (aIsNegated) { // pseudo-elements can't be negated
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Pseudo-elements can't be negated"));
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    if (0 == (aDataMask & SEL_MASK_PELEM)) {
      aDataMask |= SEL_MASK_PELEM;
      aSelector.AddPseudoClass(pseudo); // store it here, it gets pulled later
      NS_RELEASE(pseudo);

#ifdef INCLUDE_XUL
      if (eCSSToken_Function == mToken.mType && 
          IsOutlinerPseudoElement(mToken.mIdent)) {
        // We have encountered a pseudoelement of the form
        // -moz-outliner-xxxx(a,b,c).  We parse (a,b,c) and add each
        // item in the list to the pseudoclass list.  They will be pulled
        // from the list later along with the pseudoelement.
        if (!ParseOutlinerPseudoElement(aErrorCode, aSelector))
          aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
          return;
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
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Found trailing token after pseudo-element, which must be the last part of a selector: "));
        UngetToken();
        aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
        return;
      }
    }
    else {  // multiple pseudo elements, not legal
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Extra pseudo-element"));
      UngetToken();
      NS_RELEASE(pseudo);
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
  }
  aParsingStatus = SELECTOR_PARSING_ENDED_OK;
}

//
// Parse the argument of a negation pseudo-class :not()
//
void CSSParserImpl::ParseNegatedSimpleSelector(PRInt32&  aDataMask,
                                      nsCSSSelector& aSelector,
                                      PRInt32& aParsingStatus,
                                      PRInt32& aErrorCode)
{
  // Check if we have the first parenthesis
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    
    if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
      REPORT_UNEXPECTED_EOF();
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
      return;
    }
    aParsingStatus = SELECTOR_PARSING_ENDED_OK;
    nsCSSSelector* newSel = new nsCSSSelector();
    if (nsnull == aSelector.mNegations &&
        ((eCSSToken_ID == mToken.mType) ||
          mToken.IsSymbol('.') ||
          mToken.IsSymbol(':') ||
          mToken.IsSymbol('['))) {
      // ID, class and attribute selectors and pseudo-classes are stored in
      // the first mNegations attached to a selector
      aSelector.mNegations = newSel;
    }
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
      if (nsnull == aSelector.mNegations) {
        aSelector.mNegations = newSel;
      }
      newSel = new nsCSSSelector();
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
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Malformed simple selector as negation pseudo-class argument"));
      return;
    }
    // close the parenthesis
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Missing closing ')' in negation pseudo-class"));
      aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
    }
  }
  else {
    REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Missing argument in negation pseudo-class"));
    aParsingStatus = SELECTOR_PARSING_STOPPED_ERROR;
  }
}

/**
 * This is the format for selectors:
 * operator? [[namespace |]? element_name]? [ ID | class | attrib | pseudo ]*
 */
PRBool CSSParserImpl::ParseSelector(PRInt32& aErrorCode,
                                nsCSSSelector& aSelector)
{
  PRInt32  dataMask = 0;
  PRInt32  parsingStatus = SELECTOR_PARSING_ENDED_OK;

  if (! GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF();
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

nsICSSDeclaration*
CSSParserImpl::ParseDeclarationBlock(PRInt32& aErrorCode,
                                     PRBool aCheckForBraces)
{
  if (aCheckForBraces) {
    if (!ExpectSymbol(aErrorCode, '{', PR_TRUE)) {
      REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected '{' to begin declaration block but found"));
      OUTPUT_ERROR();
      return nsnull;
    }
  }
  nsICSSDeclaration* declaration = nsnull;
  if (NS_OK == NS_NewCSSDeclaration(&declaration)) {
    PRInt32 count = 0;
    PRBool dropDeclaration = PR_FALSE;
    for (;;) {
      PRInt32 hint = NS_STYLE_HINT_NONE;
      if (ParseDeclaration(aErrorCode, declaration, aCheckForBraces, hint)) {
        count++;  // count declarations
      }
      else {
      	dropDeclaration = (aErrorCode == NS_CSS_PARSER_DROP_DECLARATION);
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
    if (dropDeclaration ||
        (0 == count)) { // didn't get any XXX is this ok with the DOM?
      NS_RELEASE(declaration);
    }
  }
  return declaration;
}

PRBool CSSParserImpl::ParseColor(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF();
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
          PRInt32 index = SearchKeywordTable(keyword, nsCSSProps::kColorKTable);
          if (0 < index) {
            aValue.SetIntValue(nsCSSProps::kColorKTable[index], eCSSUnit_Integer);
            return PR_TRUE;
          }
        }
      }
      break;
    case eCSSToken_Function:
      if (mToken.mIdent.EqualsIgnoreCase("rgb")) {
        // rgb ( component , component , component )
        PRUint8 r, g, b;
        if (ExpectSymbol(aErrorCode, '(', PR_FALSE) && // this won't fail
            ParseColorComponent(aErrorCode, r, ',') &&
            ParseColorComponent(aErrorCode, g, ',') &&
            ParseColorComponent(aErrorCode, b, ')')) {
          rgba = NS_RGB(r,g,b);
          aValue.SetColorValue(rgba);
          return PR_TRUE;
        }
        return PR_FALSE;  // already pushed back
      }
      break;
    default:
      break;
  }

  // try 'xxyyzz' without '#' prefix for compatibility with IE and Nav4x (bug 23236 and 45804)
  if (mNavQuirkMode && !IsParsingCompoundProperty()) {
  	// - If the string starts with 'a-f', the nsCSSScanner builds the token
  	//   as a eCSSToken_Ident and we can parse the string as a 'xxyyzz' RGB color.
  	// - If it only contains '0-9' digits, the token is a eCSSToken_Number and it
  	//   must be converted back to a 6 characters string to be parsed as a RGB color.
  	// - If it starts with '0-9' and contains any 'a-f', the token is a eCSSToken_Dimension,
  	//   the mNumber part must be converted back to a string and the mIdent part must be
  	//   appended to that string so that the resulting string has 6 characters.
		// Note: This is a hack for Nav compatibility. Do not attempt to simplify it
		// by hacking into the ncCSSScanner. This would be very bad.
		nsAutoString str;
		char  buffer[10];
		switch (tk->mType) {
			case eCSSToken_Ident:
				str.Assign(tk->mIdent);
				break;

			case eCSSToken_Number:
				if (tk->mIntegerValid) {
					sprintf(buffer, "%06d", tk->mInteger);
					str.AssignWithConversion(buffer);
				}
				break;

			case eCSSToken_Dimension:
				if (tk->mIdent.Length() <= 6) {
					sprintf(buffer, "%06.0f", tk->mNumber);
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
  REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected color but found"));
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseColorComponent(PRInt32& aErrorCode,
                                          PRUint8& aComponent,
                                          char aStop)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    REPORT_UNEXPECTED_EOF();
    return PR_FALSE;
  }
  float value;
  nsCSSToken* tk = &mToken;
  switch (tk->mType) {
  case eCSSToken_Number:
    value = tk->mNumber;
    break;
  case eCSSToken_Percentage:
    value = tk->mNumber * 255.0f;
    break;
  default:
    REPORT_UNEXPECTED_TOKEN(
      NS_LITERAL_STRING("Expected number or percentage in rgb() but found"));
    UngetToken();
    return PR_FALSE;
  }
  if (ExpectSymbol(aErrorCode, aStop, PR_TRUE)) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    aComponent = (PRUint8) value;
    return PR_TRUE;
  }
  PRUnichar stopString[2];
  stopString[0] = PRUnichar(aStop);
  stopString[1] = PRUnichar(0);
  REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected ") +
                          nsDependentString(stopString, 1) +
                          NS_LITERAL_STRING(" but found"));
  return PR_FALSE;
}

#ifdef INCLUDE_XUL
PRBool CSSParserImpl::ParseOutlinerPseudoElement(PRInt32& aErrorCode,
                                                 nsCSSSelector& aSelector)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    while (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      if (!GetToken(aErrorCode, PR_TRUE)) {
        return PR_FALSE;
      }
      else if (eCSSToken_Ident == mToken.mType) {
        nsCOMPtr<nsIAtom> pseudo = getter_AddRefs(NS_NewAtom(mToken.mIdent));
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
CSSParserImpl::ParseDeclaration(PRInt32& aErrorCode,
                                nsICSSDeclaration* aDeclaration,
                                PRBool aCheckForBraces,
                                PRInt32& aChangeHint)
{
  // Get property name
  nsCSSToken* tk = &mToken;
  nsAutoString propertyName;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      if (aCheckForBraces) {
        REPORT_UNEXPECTED_EOF();
      }
      return PR_FALSE;
    }
    if (eCSSToken_Ident == tk->mType) {
      propertyName = tk->mIdent;
      // grab the ident before the ExpectSymbol trashes the token
      if (!ExpectSymbol(aErrorCode, ':', PR_TRUE)) {
        REPORT_UNEXPECTED_TOKEN(NS_LITERAL_STRING("Expected ':' but found"));
        REPORT_UNEXPECTED(NS_LITERAL_STRING("Declaration dropped."));
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
      REPORT_UNEXPECTED_TOKEN(
        NS_LITERAL_STRING("Expected declaration but found"));
      REPORT_UNEXPECTED(NS_LITERAL_STRING("Skipped to next declaration."));
      OUTPUT_ERROR();
    }
    // Not a declaration...
    UngetToken();
    return PR_FALSE;
  }

  // Map property name to it's ID and then parse the property
  nsCSSProperty propID = nsCSSProps::LookupProperty(propertyName);
  if (eCSSProperty_UNKNOWN == propID) { // unknown property
    REPORT_UNEXPECTED(NS_LITERAL_STRING("Unknown property '") +
                       propertyName +
                       NS_LITERAL_STRING("'.  Declaration dropped."));
    OUTPUT_ERROR();
    return PR_FALSE;
  }
  if (! ParseProperty(aErrorCode, aDeclaration, propID, aChangeHint)) {
    // XXX Much better to put stuff in the value parsers instead...
    REPORT_UNEXPECTED(
      NS_LITERAL_STRING("Error in parsing value for property '") +
      propertyName +
      NS_LITERAL_STRING("'.  Declaration dropped."));
    OUTPUT_ERROR();
    return PR_FALSE;
  }
  CLEAR_ERROR();

  // See if the declaration is followed by a "!important" declaration
  PRBool isImportant = PR_FALSE;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    if (aCheckForBraces) {
      // Premature eof is not ok when proper termination is mandated
      REPORT_UNEXPECTED_EOF();
      return PR_FALSE;
    }
    return PR_TRUE;
  }
  else {
    if (eCSSToken_Symbol == tk->mType) {
      if ('!' == tk->mSymbol) {
        // Look for important ident
        if (!GetToken(aErrorCode, PR_TRUE)) {
          // Premature eof is not ok
          REPORT_UNEXPECTED_EOF();
          return PR_FALSE;
        }
        if ((eCSSToken_Ident != tk->mType) ||
            !tk->mIdent.EqualsIgnoreCase("important")) {
          REPORT_UNEXPECTED_TOKEN(
            NS_LITERAL_STRING("Expected 'important' but found"));
          OUTPUT_ERROR();
          UngetToken();
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

  if (PR_TRUE == isImportant) {
    aDeclaration->SetValueImportant(propID);
  }

  // Make sure valid property declaration is terminated with either a
  // semicolon or a right-curly-brace (when aCheckForBraces is true).
  // When aCheckForBraces is false, proper termination is either
  // semicolon or EOF.
  if (!GetToken(aErrorCode, PR_TRUE)) {
    if (aCheckForBraces) {
      // Premature eof is not ok
      REPORT_UNEXPECTED_EOF();
      return PR_FALSE;
    }
    return PR_TRUE;
  } 
  if (eCSSToken_Symbol == tk->mType) {
    if (';' == tk->mSymbol) {
      return PR_TRUE;
    }
    if (!aCheckForBraces) {
      // If we didn't hit eof and we didn't see a semicolon then the
      // declaration is not properly terminated.
      REPORT_UNEXPECTED_TOKEN(
        NS_LITERAL_STRING("Expected ';' to terminate declaration but found"));
      REPORT_UNEXPECTED(NS_LITERAL_STRING("Declaration dropped."));
      OUTPUT_ERROR();
      return PR_FALSE;
    }
    if ('}' == tk->mSymbol) {
      UngetToken();
      return PR_TRUE;
    }
  }
  REPORT_UNEXPECTED_TOKEN(
    aCheckForBraces
      ? NS_LITERAL_STRING("Expected ';' or '}' to terminate declaration but found")
      : NS_LITERAL_STRING("Expected ';' to terminate declaration but found"));
  REPORT_UNEXPECTED(NS_LITERAL_STRING("Declaration dropped."));
  OUTPUT_ERROR();
  return PR_FALSE;
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

PRInt32 CSSParserImpl::SearchKeywordTable(nsCSSKeyword aKeyword, const PRInt32 aKeywordTable[])
{
  PRInt32 index = 0;
  while (0 <= aKeywordTable[index]) {
    if (aKeyword == nsCSSKeyword(aKeywordTable[index++])) {
      return index;
    }
    index++;
  }
  return -1;
}

PRBool CSSParserImpl::ParseEnum(PRInt32& aErrorCode, nsCSSValue& aValue,
                                const PRInt32 aKeywordTable[])
{
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(*ident);
  if (eCSSKeyword_UNKNOWN < keyword) {
    PRInt32 index = SearchKeywordTable(keyword, aKeywordTable);
    if (0 < index) {
      aValue.SetIntValue(aKeywordTable[index], eCSSUnit_Enumerated);
      return PR_TRUE;
    }
  }

  // Put the unknown identifier back and return
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::TranslateDimension(PRInt32& aErrorCode,
                                         nsCSSValue& aValue,
                                         PRInt32 aVariantMask,
                                         float aNumber,
                                         const nsString& aUnit)
{
  nsCSSUnit units;
  PRInt32   type = 0;
  if (0 != aUnit.Length()) {
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
        aErrorCode = NS_ERROR_ILLEGAL_VALUE;
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
  }
  if ((type & aVariantMask) != 0) {
    aValue.SetFloatValue(aNumber, units);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParsePositiveVariant(PRInt32& aErrorCode, 
                                           nsCSSValue& aValue, 
                                           PRInt32 aVariantMask, 
                                           const PRInt32 aKeywordTable[]) 
{ 
  if (ParseVariant(aErrorCode, aValue, aVariantMask, aKeywordTable)) { 
    if (eCSSUnit_Number == aValue.GetUnit() || 
        aValue.IsLengthUnit()){ 
      if (aValue.GetFloatValue() < 0) { 
        return PR_FALSE; 
      } 
    } 
    else if(aValue.GetUnit() == eCSSUnit_Percent) { 
      if (aValue.GetPercentValue() < 0) { 
        return PR_FALSE; 
      } 
    } 
    return PR_TRUE; 
  } 
  return PR_FALSE; 
} 

PRBool CSSParserImpl::ParseVariant(PRInt32& aErrorCode, nsCSSValue& aValue,
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
        PRInt32 index = SearchKeywordTable(keyword, aKeywordTable);
        if (0 < index) {
          aValue.SetIntValue(aKeywordTable[index], eCSSUnit_Enumerated);
          return PR_TRUE;
        }
      }
    }
  }
  if (((aVariantMask & (VARIANT_LENGTH | VARIANT_ANGLE | VARIANT_FREQUENCY | VARIANT_TIME)) != 0) && 
      tk->IsDimension()) {
    return TranslateDimension(aErrorCode, aValue, aVariantMask, tk->mNumber, tk->mIdent);
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
  if (((aVariantMask & VARIANT_URL) != 0) &&
      (eCSSToken_Function == tk->mType) && 
      tk->mIdent.EqualsIgnoreCase("url")) {
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
         (tk->mIdent.EqualsIgnoreCase("rgb")))) {
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
      (tk->mIdent.EqualsIgnoreCase("counter") || 
       tk->mIdent.EqualsIgnoreCase("counters"))) {
#ifdef ENABLE_COUNTERS
    if (ParseCounter(aErrorCode, aValue)) {
      return PR_TRUE;
    }
#endif
    return PR_FALSE;
  }
  if (((aVariantMask & VARIANT_ATTR) != 0) &&
      (eCSSToken_Function == tk->mType) &&
      tk->mIdent.EqualsIgnoreCase("attr")) {
    
    if (ParseAttr(aErrorCode, aValue)) {
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  UngetToken();
  return PR_FALSE;
}


PRBool CSSParserImpl::ParseCounter(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  nsCSSUnit unit = (mToken.mIdent.EqualsIgnoreCase("counter") ? 
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
            counter.AppendWithConversion(',');
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
            if ((eCSSKeyword_UNKNOWN < keyword) && 
                (0 < SearchKeywordTable(keyword, nsCSSProps::kListStyleKTable))) {
              counter.AppendWithConversion(',');
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

PRBool CSSParserImpl::ParseAttr(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    if (GetToken(aErrorCode, PR_TRUE)) {
      nsAutoString attr;
      if (eCSSToken_Ident == mToken.mType) {  // attr name or namespace
        nsAutoString  holdIdent(mToken.mIdent);
        if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {  // namespace
          PRInt32 nameSpaceID = kNameSpaceID_Unknown;
          if (mNameSpace) {
            nsIAtom* prefix;
            holdIdent.ToLowerCase(); // always case insensitive, since stays within CSS
            prefix = NS_NewAtom(holdIdent);
            mNameSpace->FindNameSpaceID(prefix, nameSpaceID);
            NS_IF_RELEASE(prefix);
          } // else, no delcared namespaces
          if (kNameSpaceID_Unknown == nameSpaceID) {  // unknown prefix, dump it
            return PR_FALSE;
          }
          attr.AppendInt(nameSpaceID, 10);
          attr.AppendWithConversion('|');
          if (! GetToken(aErrorCode, PR_FALSE)) {
            return PR_FALSE;
          }
          if (eCSSToken_Ident == mToken.mType) {
            if (mCaseSensitive) {
              attr.Append(mToken.mIdent);
            } else {
              nsAutoString buffer;
              mToken.mIdent.ToLowerCase(buffer);
              attr.Append(buffer);
            }
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }
        else {  // no namespace
          if (mCaseSensitive) {
            attr = holdIdent;
          }
          else {
            holdIdent.ToLowerCase(attr);
          }
        }
      }
      else if (mToken.IsSymbol('*')) {  // namespace wildcard
        if (ExpectSymbol(aErrorCode, '|', PR_FALSE)) {
          attr.AppendInt(kNameSpaceID_Unknown, 10);
          attr.AppendWithConversion('|');
          if (! GetToken(aErrorCode, PR_FALSE)) {
            return PR_FALSE;
          }
          if (eCSSToken_Ident == mToken.mType) {
            attr.Append(mToken.mIdent);
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }
        else {
          return PR_FALSE;
        }
      }
      else if (mToken.IsSymbol('|')) {  // explicit NO namespace
        if (! GetToken(aErrorCode, PR_FALSE)) {
          return PR_FALSE;
        }
        if (eCSSToken_Ident == mToken.mType) {
          if (mCaseSensitive) {
            attr.Append(mToken.mIdent);
          } else {
            nsAutoString buffer;
            mToken.mIdent.ToLowerCase(buffer);
            attr.Append(buffer);
          }
        }
        else {
          UngetToken();
          return PR_FALSE;
        }
      }
      else {
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

inline static PRBool
css_RequiresAbsoluteURI(const nsString& uri)
{
  // cheap shot at figuring out if this requires an absolute url translation
  return Substring(uri, 0, 9).Equals(NS_LITERAL_STRING("chrome:")) == PR_FALSE;
}

PRBool CSSParserImpl::ParseURL(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
    if (! GetURLToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    nsCSSToken* tk = &mToken;
    if ((eCSSToken_String == tk->mType) || (eCSSToken_URL == tk->mType)) {
      // Translate url into an absolute url if the url is relative to
      // the style sheet.
      // XXX editors won't like this - too bad for now
      nsAutoString absURL;
      if (nsnull != mURL && css_RequiresAbsoluteURI(tk->mIdent)) {
        nsresult rv;
        rv = NS_MakeAbsoluteURI(absURL, tk->mIdent, mURL);
        if (NS_FAILED(rv)) {
          absURL = tk->mIdent;
        }
      }
      else {
        absURL = tk->mIdent;
      }
      if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
        aValue.SetStringValue(absURL, eCSSUnit_URL);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

PRInt32 CSSParserImpl::ParseChoice(PRInt32& aErrorCode, nsCSSValue aValues[],
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
        if (aErrorCode == NS_ERROR_ILLEGAL_VALUE) { // bug 47138
          aErrorCode = NS_OK;
          found = 0;
          goto done;
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
done:
  SetParsingCompoundProperty(PR_FALSE);
  return found;
}

nsresult CSSParserImpl::AppendValue(nsICSSDeclaration* aDeclaration, nsCSSProperty aPropID,
                                    const nsCSSValue& aValue, PRInt32& aChangeHint)
{
  nsresult  result;
  nsCSSValue  oldValue;
  result = aDeclaration->GetValue(aPropID, oldValue);

  if (aValue != oldValue) {
    result = aDeclaration->AppendValue(aPropID, aValue);
    if (aChangeHint < nsCSSProps::kHintTable[aPropID]) {
      aChangeHint = nsCSSProps::kHintTable[aPropID];
    }
  }
  return result;
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values. 
 */
PRBool CSSParserImpl::ParseBoxProperties(PRInt32& aErrorCode,
                                         nsICSSDeclaration* aDeclaration,
                                         const nsCSSProperty aPropIDs[],
                                         PRInt32& aChangeHint)
{
  // Get up to four values for the property
  nsCSSValue  values[4];
  PRInt32 count = 0;
  PRInt32 index;
  for (index = 0; index < 4; index++) {
    if (! ParseSingleValueProperty(aErrorCode, values[index], aPropIDs[index])) {
      if (aErrorCode == NS_ERROR_ILLEGAL_VALUE) { // bug 47138
        aErrorCode = NS_OK;
        count = 0;
      }
      break;
    }
    count++;
  }
  if ((count == 0) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  if (1 < count) { // verify no more than single inherit or initial
    for (index = 0; index < 4; index++) {
      if (eCSSUnit_Inherit == values[index].GetUnit() ||
          eCSSUnit_Initial == values[index].GetUnit()) {
        return PR_FALSE;
      }
    }
  }

  // Provide missing values by replicating some of the values found
  switch (count) {
    case 1: // Make right == top
      values[1] = values[0];
    case 2: // Make bottom == top
      values[2] = values[0];
    case 3: // Make left == right
      values[3] = values[1];
  }

  for (index = 0; index < 4; index++) {
    AppendValue(aDeclaration, aPropIDs[index], values[index], aChangeHint);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseProperty(PRInt32& aErrorCode,
                                    nsICSSDeclaration* aDeclaration,
                                    nsCSSProperty aPropID,
                                    PRInt32& aChangeHint)
{
  switch (aPropID) {  // handle shorthand or multiple properties
  case eCSSProperty_background:
    return ParseBackground(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_background_position:
    return ParseBackgroundPosition(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_border:
    return ParseBorder(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_border_color:
    return ParseBorderColor(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_border_spacing:
    return ParseBorderSpacing(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_border_style:
    return ParseBorderStyle(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_border_bottom:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderBottomIDs, aChangeHint);
  case eCSSProperty_border_left:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderLeftIDs, aChangeHint);
  case eCSSProperty_border_right:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderRightIDs, aChangeHint);
  case eCSSProperty_border_top:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderTopIDs, aChangeHint);
  case eCSSProperty_border_width:
    return ParseBorderWidth(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty__moz_border_radius:
    return ParseBorderRadius(aErrorCode, aDeclaration, aChangeHint);
#ifdef ENABLE_OUTLINE
  case eCSSProperty__moz_outline_radius:
    return ParseOutlineRadius(aErrorCode, aDeclaration, aChangeHint);
#endif
  case eCSSProperty_clip:
    return ParseClip(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_content:
    return ParseContent(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_counter_increment:
  case eCSSProperty_counter_reset:
    return ParseCounterData(aErrorCode, aDeclaration, aPropID, aChangeHint);
  case eCSSProperty_cue:
    return ParseCue(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_cursor:
    return ParseCursor(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_font:
    return ParseFont(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_list_style:
    return ParseListStyle(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_margin:
    return ParseMargin(aErrorCode, aDeclaration, aChangeHint);
#ifdef ENABLE_OUTLINE
  case eCSSProperty_outline:
    return ParseOutline(aErrorCode, aDeclaration, aChangeHint);
#endif
  case eCSSProperty_padding:
    return ParsePadding(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_pause:
    return ParsePause(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_play_during:
    return ParsePlayDuring(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_quotes:
    return ParseQuotes(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_size:
    return ParseSize(aErrorCode, aDeclaration, aChangeHint);
  case eCSSProperty_text_shadow:
    return ParseTextShadow(aErrorCode, aDeclaration, aChangeHint);

  // Strip out properties we use internally. These properties are used
  // by compound property parsing routines (e.g. "background-position").
  case eCSSProperty_background_x_position:
  case eCSSProperty_background_y_position:
  case eCSSProperty_border_x_spacing:
  case eCSSProperty_border_y_spacing:
  case eCSSProperty_clip_bottom:
  case eCSSProperty_clip_left:
  case eCSSProperty_clip_right:
  case eCSSProperty_clip_top:
  case eCSSProperty_play_during_flags:
  case eCSSProperty_quotes_close:
  case eCSSProperty_quotes_open:
  case eCSSProperty_size_height:
  case eCSSProperty_size_width:
  case eCSSProperty_text_shadow_color:
  case eCSSProperty_text_shadow_radius:
  case eCSSProperty_text_shadow_x:
  case eCSSProperty_text_shadow_y:
    // The user can't use these
    REPORT_UNEXPECTED(NS_LITERAL_STRING("Attempt to use inaccessible property"));
    return PR_FALSE;

  default:  // must be single property
    {
      nsCSSValue  value;
      if (ParseSingleValueProperty(aErrorCode, value, aPropID)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aErrorCode = AppendValue(aDeclaration, aPropID, value, aChangeHint);
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
#define BG_CENTER1 0x20
#define BG_CENTER2 0x40

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

PRBool CSSParserImpl::ParseSingleValueProperty(PRInt32& aErrorCode,
                                               nsCSSValue& aValue,
                                               nsCSSProperty aPropID)
{
  switch (aPropID) {
  case eCSSProperty_UNKNOWN:
  case eCSSProperty_background:
  case eCSSProperty_background_position:
  case eCSSProperty_border:
  case eCSSProperty_border_color:
  case eCSSProperty_border_spacing:
  case eCSSProperty_border_style:
  case eCSSProperty_border_bottom:
  case eCSSProperty_border_left:
  case eCSSProperty_border_right:
  case eCSSProperty_border_top:
  case eCSSProperty_border_width:
  case eCSSProperty_clip:
  case eCSSProperty_content:
  case eCSSProperty_counter_increment:
  case eCSSProperty_counter_reset:
  case eCSSProperty_cue:
  case eCSSProperty_cursor:
  case eCSSProperty_font:
  case eCSSProperty_list_style:
  case eCSSProperty_margin:
#ifdef ENABLE_OUTLINE
  case eCSSProperty_outline:
#endif
  case eCSSProperty_padding:
  case eCSSProperty_pause:
  case eCSSProperty_play_during:
  case eCSSProperty_quotes:
  case eCSSProperty_size:
  case eCSSProperty_text_shadow:
  case eCSSProperty_COUNT:
    NS_ERROR("not a single value property");
    return PR_FALSE;

  case eCSSProperty_border_x_spacing:
  case eCSSProperty_border_y_spacing:
  case eCSSProperty_clip_bottom:
  case eCSSProperty_clip_left:
  case eCSSProperty_clip_right:
  case eCSSProperty_clip_top:
  case eCSSProperty_play_during_flags:
  case eCSSProperty_quotes_close:
  case eCSSProperty_quotes_open:
  case eCSSProperty_size_height:
  case eCSSProperty_size_width:
  case eCSSProperty_text_shadow_color:
  case eCSSProperty_text_shadow_radius:
  case eCSSProperty_text_shadow_x:
  case eCSSProperty_text_shadow_y:
    NS_ERROR("not currently parsed here");
    return PR_FALSE;

  case eCSSProperty_azimuth:
    return ParseAzimuth(aErrorCode, aValue);
  case eCSSProperty_background_attachment:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundAttachmentKTable);
  case eCSSProperty_background_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK,
                        nsCSSProps::kBackgroundColorKTable);
  case eCSSProperty_background_image:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case eCSSProperty_background_repeat:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundRepeatKTable);
  case eCSSProperty_background_x_position:
  case eCSSProperty_background_y_position:
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
    return ParseVariant(aErrorCode, aValue, VARIANT_HKL,
                        nsCSSProps::kBorderWidthKTable);
  case eCSSProperty__moz_border_radius_topLeft:
  case eCSSProperty__moz_border_radius_topRight:
  case eCSSProperty__moz_border_radius_bottomRight:
  case eCSSProperty__moz_border_radius_bottomLeft:
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
#ifdef INCLUDE_XUL
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
#endif
  case eCSSProperty_box_sizing:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBoxSizingKTable);
  case eCSSProperty_height:
  case eCSSProperty_width:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
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
						//aErrorCode = NS_CSS_PARSER_DROP_DECLARATION;  // bug 15432 (commented out for bug 46562 and 38397)
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
  case eCSSProperty_margin_left:
  case eCSSProperty_margin_right:
  case eCSSProperty_margin_top:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case eCSSProperty_marker_offset:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHL, nsnull);
  case eCSSProperty_marks:
    return ParseMarks(aErrorCode, aValue);
  case eCSSProperty_max_height:
  case eCSSProperty_max_width:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLPO, nsnull);
  case eCSSProperty_min_height:
  case eCSSProperty_min_width:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case eCSSProperty_opacity:
    return ParseVariant(aErrorCode, aValue, VARIANT_HPN, nsnull);
  case eCSSProperty_orphans:
  case eCSSProperty_widows:
    return ParseVariant(aErrorCode, aValue, VARIANT_HI, nsnull);
#ifdef ENABLE_OUTLINE
  case eCSSProperty_outline_color:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK, 
                        nsCSSProps::kOutlineColorKTable);
  case eCSSProperty_outline_style:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK, 
                        nsCSSProps::kBorderStyleKTable);
  case eCSSProperty_outline_width:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKL,
                        nsCSSProps::kBorderWidthKTable);
#endif
  case eCSSProperty_overflow:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kOverflowKTable);
  case eCSSProperty_padding_bottom:
  case eCSSProperty_padding_left:
  case eCSSProperty_padding_right:
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
  case eCSSProperty_resizer:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK | VARIANT_NONE,
                        nsCSSProps::kResizerKTable);
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
    return ParseVariant(aErrorCode, aValue, VARIANT_HK | VARIANT_STRING,
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

PRBool CSSParserImpl::ParseAzimuth(PRInt32& aErrorCode, nsCSSValue& aValue)
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

PRBool CSSParserImpl::ParseBackground(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                      PRInt32& aChangeHint)
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
    if (0 == (found & 0x20)) {  // x value only
      if (eCSSUnit_Enumerated == values[4].GetUnit()) {
        switch (values[4].GetIntValue()) {
          case BG_CENTER:
            values[4].SetIntValue(50, eCSSUnit_Enumerated);
            values[5].SetIntValue(50, eCSSUnit_Enumerated);
            break;
          case BG_TOP:
            values[4].SetIntValue(50, eCSSUnit_Enumerated);
            values[5].SetIntValue(0, eCSSUnit_Enumerated);
            break;
          case BG_BOTTOM:
            values[4].SetIntValue(50, eCSSUnit_Enumerated);
            values[5].SetIntValue(100, eCSSUnit_Enumerated);
            break;
          case BG_LEFT:
            values[4].SetIntValue(0, eCSSUnit_Enumerated);
            values[5].SetIntValue(50, eCSSUnit_Enumerated);
            break;
          case BG_RIGHT:
            values[4].SetIntValue(100, eCSSUnit_Enumerated);
            values[5].SetIntValue(50, eCSSUnit_Enumerated);
            break;
        }
      }
      else if (eCSSUnit_Inherit == values[4].GetUnit()) {
        values[5].SetInheritValue();
      }
      else if (eCSSUnit_Initial == values[4].GetUnit()) {
        values[5].SetInitialValue();
      }
      else {
        values[5].SetPercentValue(0.5f);
      }
    }
    else { // both x & y values
      nsCSSUnit xUnit = values[4].GetUnit();
      nsCSSUnit yUnit = values[5].GetUnit();
      if (eCSSUnit_Enumerated == xUnit) { // if one is enumerated, both must be
        if (eCSSUnit_Enumerated == yUnit) {
          PRInt32 xValue = values[4].GetIntValue();
          PRInt32 yValue = values[5].GetIntValue();
          if (0 != (xValue & (BG_LEFT | BG_CENTER | BG_RIGHT))) {  // x is really an x value
            if (0 != (yValue & (BG_LEFT | BG_RIGHT))) { // y is also an x value
              return PR_FALSE;
            }
          }
          else {  // x is a y value
            if (0 != (yValue & (BG_TOP | BG_BOTTOM))) { // y is also a y value
              return PR_FALSE;
            }
            PRInt32 holdXValue = xValue;
            xValue = yValue;
            yValue = holdXValue;
          }
          switch (xValue) {
            case BG_LEFT:
              values[4].SetIntValue(0, eCSSUnit_Enumerated);
              break;
            case BG_CENTER:
              values[4].SetIntValue(50, eCSSUnit_Enumerated);
              break;
            case BG_RIGHT:
              values[4].SetIntValue(100, eCSSUnit_Enumerated);
              break;
            default:
              NS_ERROR("bad x value");
          }
          switch (yValue) {
            case BG_TOP:
              values[5].SetIntValue(0, eCSSUnit_Enumerated);
              break;
            case BG_CENTER:
              values[5].SetIntValue(50, eCSSUnit_Enumerated);
              break;
            case BG_BOTTOM:
              values[5].SetIntValue(100, eCSSUnit_Enumerated);
              break;
            default:
              NS_ERROR("bad y value");
          }
        }
        else {
          return PR_FALSE;
        }
      }
      else {
        if (eCSSUnit_Enumerated == yUnit) {
          return PR_FALSE;
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
  for (index = 0; index < numProps; index++) {
    AppendValue(aDeclaration, kBackgroundIDs[index], values[index], aChangeHint);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBackgroundPosition(PRInt32& aErrorCode,
                                              nsICSSDeclaration* aDeclaration,
                                              PRInt32& aChangeHint)
{
  // First try a number or a length value
  nsCSSValue  xValue;
  if (ParseVariant(aErrorCode, xValue, VARIANT_HLP, nsnull)) {
    if (eCSSUnit_Inherit == xValue.GetUnit() ||
        eCSSUnit_Initial == xValue.GetUnit()) {  // both are inherited or both are set to initial
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        AppendValue(aDeclaration, eCSSProperty_background_x_position, xValue, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_background_y_position, xValue, aChangeHint);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
    // We have one number/length. Get the optional second number/length.
    nsCSSValue yValue;
    if (ParseVariant(aErrorCode, yValue, VARIANT_LP, nsnull)) {
      // We have two numbers
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        AppendValue(aDeclaration, eCSSProperty_background_x_position, xValue, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_background_y_position, yValue, aChangeHint);
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    // We have one number which is the x position. Create an value for
    // the vertical position which is of value 50%
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_background_x_position, xValue, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_background_y_position, nsCSSValue(0.5f, eCSSUnit_Percent), aChangeHint);
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
  PRInt32 centerBit = BG_CENTER1;
  for (PRInt32 i = 0; i < 2; i++) {
    if (PR_FALSE == ParseEnum(aErrorCode, xValue, kBackgroundXYPositionKTable)) {
      break;
    }
    PRInt32 bit = xValue.GetIntValue();
    if (BG_CENTER == bit) {
      // Special hack for center bits: We can have two of them
      mask |= centerBit;
      centerBit <<= 1;
      continue;
    } else if ((mask & bit) != 0) {
      // no duplicate values allowed (other than center)
      return PR_FALSE;
    }
    mask |= bit;
  }

  // Check for bad input. Bad input consists of no matching keywords,
  // or pairs of x keywords or pairs of y keywords.
  if ((mask == 0) || (mask == (BG_TOP | BG_BOTTOM)) ||
      (mask == (BG_LEFT | BG_RIGHT))) {
    return PR_FALSE;
  }

  // Map good input
  PRInt32 xEnumValue = 50;
  if ((mask & (BG_LEFT | BG_RIGHT)) != 0) {
    // We have an x value
    xEnumValue = ((mask & BG_LEFT) != 0) ? 0 : 100;
  }
  PRInt32 yEnumValue = 50;
  if ((mask & (BG_TOP | BG_BOTTOM)) != 0) {
    // We have a y value
    yEnumValue = ((mask & BG_TOP) != 0) ? 0 : 100;
  }

  if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
    // Create style values
    AppendValue(aDeclaration, eCSSProperty_background_x_position, nsCSSValue(xEnumValue, eCSSUnit_Enumerated), aChangeHint);
    AppendValue(aDeclaration, eCSSProperty_background_y_position, nsCSSValue(yEnumValue, eCSSUnit_Enumerated), aChangeHint);
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

PRBool CSSParserImpl::ParseBorder(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                  PRInt32& aChangeHint)
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty kBorderIDs[] = {
    eCSSProperty_border_top_width,  // only one value per property
    eCSSProperty_border_top_style,
    eCSSProperty_border_top_color
  };

  nsCSSValue  values[numProps];

  PRInt32 found = ParseChoice(aErrorCode, values, kBorderIDs, numProps);
  if ((found < 1) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  if (0 == (found & 1)) { // provide missing border width's
    values[0].SetIntValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated);
  }
  
  if (0 == (found & 2)) { // provide missing border style's
    values[1].SetNoneValue();
  }

  if (0 == (found & 4)) { // text color will be used
    values[2].SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  }

  PRInt32 index;
  for (index = 0; index < 4; index++) {
    AppendValue(aDeclaration, kBorderWidthIDs[index], values[0], aChangeHint);
    AppendValue(aDeclaration, kBorderStyleIDs[index], values[1], aChangeHint);
    AppendValue(aDeclaration, kBorderColorIDs[index], values[2], aChangeHint);
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderColor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                       PRInt32& aChangeHint)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderColorIDs, aChangeHint);
}

PRBool CSSParserImpl::ParseBorderSpacing(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                         PRInt32& aChangeHint)
{
  nsCSSValue  xValue;
  if (ParseVariant(aErrorCode, xValue, VARIANT_HL, nsnull)) {
    if (xValue.IsLengthUnit()) {
      // We have one length. Get the optional second length.
      nsCSSValue yValue;
      if (ParseVariant(aErrorCode, yValue, VARIANT_LENGTH, nsnull)) {
        // We have two numbers
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(aDeclaration, eCSSProperty_border_x_spacing, xValue, aChangeHint);
          AppendValue(aDeclaration, eCSSProperty_border_y_spacing, yValue, aChangeHint);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }

    // We have one length which is the horizontal spacing. Create a value for
    // the vertical spacing which is equal
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_border_x_spacing, xValue, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_border_y_spacing, xValue, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseBorderSide(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                      const nsCSSProperty aPropIDs[], PRInt32& aChangeHint)
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
  if ((found & 4) == 0) { // reset border-color so color property gets used
    values[2].Reset();
  }

  PRInt32 index;
  for (index = 0; index < numProps; index++) {
    AppendValue(aDeclaration, aPropIDs[index], values[index], aChangeHint);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                       PRInt32& aChangeHint)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderStyleIDs, aChangeHint);
}

PRBool CSSParserImpl::ParseBorderWidth(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                       PRInt32& aChangeHint)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderWidthIDs, aChangeHint);
}

PRBool CSSParserImpl::ParseBorderRadius(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                       PRInt32& aChangeHint)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderRadiusIDs, aChangeHint);
}

#ifdef ENABLE_OUTLINE
PRBool CSSParserImpl::ParseOutlineRadius(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                       PRInt32& aChangeHint)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kOutlineRadiusIDs, aChangeHint);
}
#endif

PRBool CSSParserImpl::ParseClip(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                PRInt32& aChangeHint)
{
  static const nsCSSProperty kClipIDs[] = {
    eCSSProperty_clip_top,
    eCSSProperty_clip_right,
    eCSSProperty_clip_bottom,
    eCSSProperty_clip_left
  };
  if (! GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  PRInt32 index;
  if ((eCSSToken_Ident == mToken.mType) && 
      mToken.mIdent.EqualsIgnoreCase("auto")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSValue valueAuto(eCSSUnit_Auto);
      for (index = 0; index < 4; index++) {
        AppendValue(aDeclaration, kClipIDs[index], valueAuto, aChangeHint);
      }
      return PR_TRUE;
    }
  } else if ((eCSSToken_Ident == mToken.mType) && 
             mToken.mIdent.EqualsIgnoreCase("inherit")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSValue  inherit(eCSSUnit_Inherit);
      for (index = 0; index < 4; index++) {
        AppendValue(aDeclaration, kClipIDs[index], inherit, aChangeHint);
      }
      return PR_TRUE;
    }
  } 
  else if ((eCSSToken_Ident == mToken.mType) && 
             mToken.mIdent.EqualsIgnoreCase("-moz-initial")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSValue  initial(eCSSUnit_Initial);
      for (index = 0; index < 4; index++) {
        AppendValue(aDeclaration, kClipIDs[index], initial, aChangeHint);
      }
      return PR_TRUE;
    }
  } else if ((eCSSToken_Function == mToken.mType) && 
             mToken.mIdent.EqualsIgnoreCase("rect")) {
    if (!ExpectSymbol(aErrorCode, '(', PR_TRUE)) {
      return PR_FALSE;
    }
    nsCSSValue  values[4];
    for (index = 0; index < 4; index++) {
      if (! ParseVariant(aErrorCode, values[index], VARIANT_AL, nsnull)) {
        return PR_FALSE;
      }
      if (3 != index) {
        // skip optional commas between elements
        ExpectSymbol(aErrorCode, ',', PR_TRUE);
      }
    }
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      return PR_FALSE;
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      for (index = 0; index < 4; index++) {
        AppendValue(aDeclaration, kClipIDs[index], values[index], aChangeHint);
      }
      return PR_TRUE;
    }
  } else {
    UngetToken();
  }
  return PR_FALSE;
}

#define VARIANT_CONTENT (VARIANT_STRING | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR | \
                         VARIANT_KEYWORD)
PRBool CSSParserImpl::ParseContent(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                   PRInt32& aChangeHint)
{
  nsCSSValue  value;
  if (ParseVariant(aErrorCode, value, VARIANT_CONTENT | VARIANT_INHERIT, 
                   nsCSSProps::kContentKTable)) {
    if (eCSSUnit_Inherit == value.GetUnit() || eCSSUnit_Initial == value.GetUnit()) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        AppendValue(aDeclaration, eCSSProperty_content, value, aChangeHint);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
    nsCSSValueList* listHead = new nsCSSValueList();
    nsCSSValueList* list = listHead;
    if (nsnull == list) {
      aErrorCode = NS_ERROR_OUT_OF_MEMORY;
      return PR_FALSE;
    }
    list->mValue = value;

    while (nsnull != list) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        if (aChangeHint < nsCSSProps::kHintTable[eCSSProperty_content]) {
          aChangeHint = nsCSSProps::kHintTable[eCSSProperty_content];
        }
        aErrorCode = aDeclaration->AppendStructValue(eCSSProperty_content, listHead);
        return NS_SUCCEEDED(aErrorCode);
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

PRBool CSSParserImpl::ParseCounterData(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                       nsCSSProperty aPropID, PRInt32& aChangeHint)
{
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  if (ident->EqualsIgnoreCase("none")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, aPropID, nsCSSValue(eCSSUnit_None), aChangeHint);
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  else if (ident->EqualsIgnoreCase("inherit")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, aPropID, nsCSSValue(eCSSUnit_Inherit), aChangeHint);
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  else if (ident->EqualsIgnoreCase("-moz-initial")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, aPropID, nsCSSValue(eCSSUnit_Initial), aChangeHint);
      return PR_TRUE;
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
        if (aChangeHint < nsCSSProps::kHintTable[aPropID]) {
          aChangeHint = nsCSSProps::kHintTable[aPropID];
        }
        aErrorCode = aDeclaration->AppendStructValue(aPropID, dataHead);
        return NS_SUCCEEDED(aErrorCode);
      }
      if (! GetToken(aErrorCode, PR_TRUE)) {
        break;
      }
      if ((eCSSToken_Number == mToken.mType) && (mToken.mIntegerValid)) {
        data->mValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          if (aChangeHint < nsCSSProps::kHintTable[aPropID]) {
            aChangeHint = nsCSSProps::kHintTable[aPropID];
          }
          aErrorCode = aDeclaration->AppendStructValue(aPropID, dataHead);
          return NS_SUCCEEDED(aErrorCode);
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

PRBool CSSParserImpl::ParseCue(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                               PRInt32& aChangeHint)
{
  nsCSSValue before;
  if (ParseSingleValueProperty(aErrorCode, before, eCSSProperty_cue_before)) {
    if (eCSSUnit_URL == before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, eCSSProperty_cue_after)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(aDeclaration, eCSSProperty_cue_before, before, aChangeHint);
          AppendValue(aDeclaration, eCSSProperty_cue_after, after, aChangeHint);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_cue_before, before, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_cue_after, before, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseCursor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                  PRInt32& aChangeHint)
{
  nsCSSValue  value;
  if (ParseVariant(aErrorCode, value, VARIANT_AHUK, nsCSSProps::kCursorKTable)) {
    if (eCSSUnit_URL == value.GetUnit()) {
      nsCSSValueList* listHead = new nsCSSValueList();
      nsCSSValueList* list = listHead;
      if (nsnull == list) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
      list->mValue = value;
      while (nsnull != list) {
        if (eCSSUnit_URL != value.GetUnit()) {
          if (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE)) {
            return PR_FALSE;
          }
        }
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          if (aChangeHint < nsCSSProps::kHintTable[eCSSProperty_cursor]) {
            aChangeHint = nsCSSProps::kHintTable[eCSSProperty_cursor];
          }
          aErrorCode = aDeclaration->AppendStructValue(eCSSProperty_cursor, listHead);
          return NS_SUCCEEDED(aErrorCode);
        }
        if (ParseVariant(aErrorCode, value, VARIANT_AHUK, nsCSSProps::kCursorKTable)) {
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
      return PR_FALSE;
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_cursor, value, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


PRBool CSSParserImpl::ParseFont(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                PRInt32& aChangeHint)
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
        AppendValue(aDeclaration, eCSSProperty_font_family, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_style, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_variant, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_weight, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_size, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_line_height, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_stretch, family, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_size_adjust, family, aChangeHint);
      }
      else {
        AppendValue(aDeclaration, eCSSProperty_font_family, family, aChangeHint);  // keyword value overrides everything else
        nsCSSValue empty;
        AppendValue(aDeclaration, eCSSProperty_font_style, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_variant, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_weight, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_size, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_line_height, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_stretch, empty, aChangeHint);
        AppendValue(aDeclaration, eCSSProperty_font_size_adjust, empty, aChangeHint);
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
    if (! ParseVariant(aErrorCode, lineHeight, VARIANT_NUMBER | VARIANT_LP | VARIANT_NORMAL, nsnull)) {
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
      AppendValue(aDeclaration, eCSSProperty_font_family, family, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_style, values[0], aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_variant, values[1], aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_weight, values[2], aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_size, size, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_line_height, lineHeight, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_stretch, nsCSSValue(eCSSUnit_Normal), aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_font_size_adjust, nsCSSValue(eCSSUnit_None), aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseFontWeight(PRInt32& aErrorCode, nsCSSValue& aValue)
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

PRBool CSSParserImpl::ParseFamily(PRInt32& aErrorCode, nsCSSValue& aValue)
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
        if (tk->mIdent.EqualsIgnoreCase("inherit")) {
          aValue.SetInheritValue();
          return PR_TRUE;
        }
        else if (tk->mIdent.EqualsIgnoreCase("initial")) {
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
  if (family.Length() == 0) {
    return PR_FALSE;
  }
  aValue.SetStringValue(family, eCSSUnit_String);
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseListStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                     PRInt32& aChangeHint)
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
    AppendValue(aDeclaration, listStyleIDs[index], values[index], aChangeHint);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseMargin(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                  PRInt32& aChangeHint)
{
  static const nsCSSProperty kMarginSideIDs[] = {
    eCSSProperty_margin_top,
    eCSSProperty_margin_right,
    eCSSProperty_margin_bottom,
    eCSSProperty_margin_left
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kMarginSideIDs, aChangeHint);
}

PRBool CSSParserImpl::ParseMarks(PRInt32& aErrorCode, nsCSSValue& aValue)
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
PRBool CSSParserImpl::ParseOutline(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                   PRInt32& aChangeHint)
{
  const PRInt32 numProps = 3;
  static const nsCSSProperty kOutlineIDs[] = {
    eCSSProperty_outline_color,
    eCSSProperty_outline_style,
    eCSSProperty_outline_width
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
    AppendValue(aDeclaration, kOutlineIDs[index], values[index], aChangeHint);
  }
  return PR_TRUE;
}
#endif

PRBool CSSParserImpl::ParsePadding(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                   PRInt32& aChangeHint)
{
  static const nsCSSProperty kPaddingSideIDs[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kPaddingSideIDs, aChangeHint);
}

PRBool CSSParserImpl::ParsePause(PRInt32& aErrorCode,
                                 nsICSSDeclaration* aDeclaration, PRInt32& aChangeHint)
{
  nsCSSValue  before;
  if (ParseSingleValueProperty(aErrorCode, before, eCSSProperty_pause_before)) {
    if (eCSSUnit_Inherit != before.GetUnit() && eCSSUnit_Initial != before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, eCSSProperty_pause_after)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(aDeclaration, eCSSProperty_pause_before, before, aChangeHint);
          AppendValue(aDeclaration, eCSSProperty_pause_after, after, aChangeHint);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_pause_before, before, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_pause_after, before, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParsePlayDuring(PRInt32& aErrorCode,
                                      nsICSSDeclaration* aDeclaration, 
                                      PRInt32& aChangeHint)
{
  nsCSSValue  playDuring;
  nsCSSValue  flags;
  if (ParseVariant(aErrorCode, playDuring, VARIANT_AHUO, nsnull)) {
    if (eCSSUnit_URL == playDuring.GetUnit()) {
      if (ParseEnum(aErrorCode, flags, nsCSSProps::kPlayDuringKTable)) {
        PRInt32 intValue = flags.GetIntValue();
        if (ParseEnum(aErrorCode, flags, nsCSSProps::kPlayDuringKTable)) {
          flags.SetIntValue(intValue | flags.GetIntValue(), eCSSUnit_Enumerated);
        }
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_play_during, playDuring, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_play_during_flags, flags, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseQuotes(PRInt32& aErrorCode,
                                  nsICSSDeclaration* aDeclaration, 
                                  PRInt32 &aChangeHint)
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
            if (aChangeHint < nsCSSProps::kHintTable[eCSSProperty_quotes]) {
              aChangeHint = nsCSSProps::kHintTable[eCSSProperty_quotes];
            }
            aErrorCode = aDeclaration->AppendStructValue(eCSSProperty_quotes, quotesHead);
            return NS_SUCCEEDED(aErrorCode);
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
      AppendValue(aDeclaration, eCSSProperty_quotes_open, open, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseSize(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                                PRInt32& aChangeHint)
{
  nsCSSValue width;
  if (ParseVariant(aErrorCode, width, VARIANT_AHKL, nsCSSProps::kPageSizeKTable)) {
    if (width.IsLengthUnit()) {
      nsCSSValue  height;
      if (ParseVariant(aErrorCode, height, VARIANT_LENGTH, nsnull)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          AppendValue(aDeclaration, eCSSProperty_size_width, width, aChangeHint);
          AppendValue(aDeclaration, eCSSProperty_size_height, height, aChangeHint);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_size_width, width, aChangeHint);
      AppendValue(aDeclaration, eCSSProperty_size_height, width, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseTextDecoration(PRInt32& aErrorCode, nsCSSValue& aValue)
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

PRBool CSSParserImpl::ParseTextShadow(PRInt32& aErrorCode,
                                      nsICSSDeclaration* aDeclaration, 
                                      PRInt32& aChangeHint)
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
            if (aChangeHint < nsCSSProps::kHintTable[eCSSProperty_text_shadow]) {
              aChangeHint = nsCSSProps::kHintTable[eCSSProperty_text_shadow];
            }
            aErrorCode = aDeclaration->AppendStructValue(eCSSProperty_text_shadow, shadowHead);
            return NS_SUCCEEDED(aErrorCode);
          }
          break;
        }
      }
      delete shadowHead;
      return PR_FALSE;
    }
    // value is inherit or none
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      AppendValue(aDeclaration, eCSSProperty_text_shadow_x, value, aChangeHint);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP CSSParserImpl::GetCharset(/*out*/nsAWritableString &aCharsetDest) const
{ 
  aCharsetDest = mCharset; 
  return NS_OK;
}

NS_IMETHODIMP CSSParserImpl::SetCharset(/*in*/ const nsAReadableString &aCharsetSrc)
{
  mCharset = aCharsetSrc;
  return NS_OK;
}
