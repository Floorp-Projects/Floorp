/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsICSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsCSSScanner.h"
#include "nsICSSStyleRule.h"
#include "nsIUnicharInputStream.h"
#include "nsIStyleSet.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSDeclaration.h"
#include "nsStyleConsts.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsColor.h"
#include "nsStyleConsts.h"
#include "nsCSSAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"

// XXX TODO:
// - rework aErrorCode stuff: switch over to nsresult
// verify ! is followed by important and nothing else

static NS_DEFINE_IID(kICSSParserIID, NS_ICSS_PARSER_IID);
static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);

#define KEYWORD_BUFFER_SIZE 100    // big enough for any keyword

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
  nsAutoString    mSourceString;
  PRInt32         mWeight;
  SelectorList*   mNext;
};

SelectorList::SelectorList(void)
  : mSelectors(nsnull),
    mNext(nsnull)
{
}

SelectorList::~SelectorList()
{
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
  CSSParserImpl(nsICSSStyleSheet* aSheet);
  virtual ~CSSParserImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetInfoMask(PRUint32& aResult);

  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive);

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURL*                aInputURL,
                   nsICSSStyleSheet*&     aResult);

  NS_IMETHOD ParseDeclarations(const nsString& aDeclaration,
                               nsIURL*         aBaseURL,
                               nsIStyleRule*&  aResult);
  
  NS_IMETHOD ParseAndAppendDeclaration(const nsString&    aBuffer,
                                       nsIURL*            aBaseURL,
                                       nsICSSDeclaration* aDeclaration,
                                       PRInt32*           aHint);

protected:
  PRBool GetToken(PRInt32& aErrorCode, PRBool aSkipWS);
  PRBool GetURLToken(PRInt32& aErrorCode, PRBool aSkipWS);
  void UngetToken();

  PRBool ExpectSymbol(PRInt32& aErrorCode, char aSymbol, PRBool aSkipWS);
  PRBool ExpectEndProperty(PRInt32& aErrorCode, PRBool aSkipWS);
  nsString* NextIdent(PRInt32& aErrorCode);
  void SkipUntil(PRInt32& aErrorCode, PRUnichar aStopSymbol);
  void SkipRuleSet(PRInt32& aErrorCode);
  PRBool SkipDeclaration(PRInt32& aErrorCode, PRBool aCheckForBraces);

  PRBool ParseRuleSet(PRInt32& aErrorCode);
  PRBool ParseAtRule(PRInt32& aErrorCode);
  PRBool ParseImportRule(PRInt32& aErrorCode);
  PRBool GatherMedia(PRInt32& aErrorCode, nsString& aMedia);
  PRBool ProcessImport(PRInt32& aErrorCode, const nsString& aURLSpec, const nsString& aMedia);

  PRBool ParseSelectorList(PRInt32& aErrorCode, SelectorList*& aListHead);
  PRBool ParseSelectorGroup(PRInt32& aErrorCode, SelectorList*& aListHead);
  PRBool ParseSelector(PRInt32& aErrorCode, nsCSSSelector& aSelectorResult, nsString& aSource);
  nsICSSDeclaration* ParseDeclarationBlock(PRInt32& aErrorCode,
                                           PRBool aCheckForBraces);
  PRBool ParseDeclaration(PRInt32& aErrorCode,
                          nsICSSDeclaration* aDeclaration,
                          PRBool aCheckForBraces,
                          PRInt32& aChangeHint);
  PRBool ParseProperty(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                       PRInt32 aPropID, PRInt32& aChangeHint);
  PRBool ParseProperty(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration, 
                       PRInt32 aPropID);
  PRBool ParseSingleValueProperty(PRInt32& aErrorCode, nsCSSValue& aValue, 
                                  PRInt32 aPropID);

  // Property specific parsing routines
  PRBool ParseAzimuth(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseBackground(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBackgroundFilter(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseForegroundFilter(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseBackgroundPosition(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorder(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderColor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderSpacing(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderSide(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                         const PRInt32 aPropIDs[]);
  PRBool ParseBorderStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderWidth(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseClip(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseContent(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseCounterData(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                          PRInt32 aPropID);
  PRBool ParseCue(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseCursor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseFont(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseFamily(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseListStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseMargin(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseMarks(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseOutline(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParsePadding(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParsePause(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParsePlayDuring(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseQuotes(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseSize(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseTextDecoration(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseTextShadow(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration);

  // Reused utility parsing routines
  PRBool ParseBoxProperties(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                            const PRInt32 aPropIDs[]);
  PRInt32 ParseChoice(PRInt32& aErrorCode, nsCSSValue aValues[],
                      const PRInt32 aPropIDs[], PRInt32 aNumIDs);
  PRBool ParseColor(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseColorComponent(PRInt32& aErrorCode, PRUint8& aComponent,
                             char aStop);
  PRBool ParseEnum(PRInt32& aErrorCode, nsCSSValue& aValue, const PRInt32 aKeywordTable[]);
  PRInt32 SearchKeywordTable(PRInt32 aID, const PRInt32 aTable[]);
  PRBool ParseVariant(PRInt32& aErrorCode, nsCSSValue& aValue,
                      PRInt32 aVariantMask,
                      const PRInt32 aKeywordTable[]);
  PRBool ParsePositiveVariant(PRInt32& aErrorCode, nsCSSValue& aValue, 
                              PRInt32 aVariantMask, 
                              const PRInt32 aKeywordTable[]); 
  PRBool ParseCounter(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseAttr(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool ParseURL(PRInt32& aErrorCode, nsCSSValue& aValue);
  PRBool TranslateDimension(nsCSSValue& aValue, PRInt32 aVariantMask,
                            float aNumber, const nsString& aUnit);

  // Current token. The value is valid after calling GetToken
  nsCSSToken mToken;

  // After an UngetToken is done this flag is true. The next call to
  // GetToken clears the flag.
  PRBool mHavePushBack;

  nsCSSScanner* mScanner;
  nsIURL* mURL;
  nsICSSStyleSheet* mSheet;

  PRBool mInHead;

  PRBool  mNavQuirkMode;
  PRBool  mCaseSensitive;
};

NS_HTML nsresult
NS_NewCSSParser(nsICSSParser** aInstancePtrResult)
{
  CSSParserImpl *it = new CSSParserImpl();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSParserIID, (void **) aInstancePtrResult);
}

CSSParserImpl::CSSParserImpl()
{
  NS_INIT_REFCNT();
  nsCSSAtoms::AddrefAtoms();
  mScanner = nsnull;
  mSheet = nsnull;
  mHavePushBack = PR_FALSE;
  mNavQuirkMode = PR_TRUE;
  mCaseSensitive = PR_FALSE;
}

CSSParserImpl::CSSParserImpl(nsICSSStyleSheet* aSheet)
{
  NS_INIT_REFCNT();
  nsCSSAtoms::AddrefAtoms();
  mScanner = nsnull;
  mSheet = aSheet; NS_ADDREF(aSheet);
  mHavePushBack = PR_FALSE;
  mNavQuirkMode = PR_TRUE;
  mCaseSensitive = PR_FALSE;
}

NS_IMPL_ISUPPORTS(CSSParserImpl,kICSSParserIID)

CSSParserImpl::~CSSParserImpl()
{
  NS_IF_RELEASE(mSheet);
  nsCSSAtoms::ReleaseAtoms();
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
    NS_IF_RELEASE(mSheet);
    mSheet = aSheet;
    NS_ADDREF(mSheet);
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
CSSParserImpl::Parse(nsIUnicharInputStream* aInput,
                     nsIURL*                aInputURL,
                     nsICSSStyleSheet*&     aResult)
{
  NS_ASSERTION(nsnull != aInputURL, "need base URL");

  if (nsnull == mSheet) {
    NS_NewCSSStyleSheet(&mSheet, aInputURL);
  }

  PRInt32 errorCode = NS_OK;
  mScanner = new nsCSSScanner();
  if (nsnull == mScanner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->Init(aInput);
  mURL = aInputURL;
  NS_IF_ADDREF(aInputURL);
  mInHead = PR_TRUE;
  nsCSSToken* tk = &mToken;
  for (;;) {
    // Get next non-whitespace token
    if (!GetToken(errorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_HTMLComment == tk->mType) {
      continue; // legal here only
    }
    if (eCSSToken_AtKeyword == tk->mType) {
      ParseAtRule(errorCode);
      continue;
    }
    mInHead = PR_FALSE;
    UngetToken();
    ParseRuleSet(errorCode);
  }
  delete mScanner;
  mScanner = nsnull;
  NS_IF_RELEASE(mURL);

  aResult = mSheet;
  NS_ADDREF(aResult);

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseDeclarations(const nsString& aDeclaration,
                                 nsIURL*         aBaseURL,
                                 nsIStyleRule*&  aResult)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

  nsString* str = new nsString(aDeclaration);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIUnicharInputStream* input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(&input, str);
  if (NS_OK != rv) {
    delete str;
    return rv;
  }

  mScanner = new nsCSSScanner();
  if (nsnull == mScanner) {
    NS_RELEASE(input);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->Init(input);
  NS_RELEASE(input);

  mURL = aBaseURL;
  NS_IF_ADDREF(mURL);

  mInHead = PR_FALSE;
  PRInt32 errorCode = NS_OK;

  nsICSSDeclaration* declaration = ParseDeclarationBlock(errorCode, PR_FALSE);
  if (nsnull != declaration) {
    // Create a style rule for the delcaration
    nsICSSStyleRule* rule = nsnull;
    NS_NewCSSStyleRule(&rule, nsCSSSelector());
    rule->SetDeclaration(declaration);
    aResult = rule;
    NS_RELEASE(declaration);
  }
  else {
    aResult = nsnull;
  }

  delete mScanner;
  mScanner = nsnull;
  NS_IF_RELEASE(mURL);

  return NS_OK;
}

NS_IMETHODIMP
CSSParserImpl::ParseAndAppendDeclaration(const nsString&    aBuffer,
                                         nsIURL*            aBaseURL,
                                         nsICSSDeclaration* aDeclaration,
                                         PRInt32*           aHint)
{
  NS_ASSERTION(nsnull != aBaseURL, "need base URL");

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

  mScanner = new nsCSSScanner();
  if (nsnull == mScanner) {
    NS_RELEASE(input);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->Init(input);
  NS_RELEASE(input);

  mURL = aBaseURL;
  NS_IF_ADDREF(mURL);

  mInHead = PR_FALSE;
  PRInt32 errorCode = NS_OK;

  PRInt32 hint = NS_STYLE_HINT_UNKNOWN;

  ParseDeclaration(errorCode, aDeclaration, PR_FALSE, hint);  
  if (nsnull != aHint) {
    *aHint = hint;
  }

  delete mScanner;
  mScanner = nsnull;
  NS_IF_RELEASE(mURL);

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
                                   char aSymbol,
                                   PRBool aSkipWS)
{
  if (!GetToken(aErrorCode, aSkipWS)) {
    return PR_FALSE;
  }
  nsCSSToken* tk = &mToken;
  if ((eCSSToken_Symbol == tk->mType) && (aSymbol == tk->mSymbol)) {
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
  UngetToken();
  return PR_FALSE;
}


nsString* CSSParserImpl::NextIdent(PRInt32& aErrorCode)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return nsnull;
  }
  if (eCSSToken_Ident != mToken.mType) {
    UngetToken();
    return nsnull;
  }
  return &mToken.mIdent;
}

// XXX @media type {, types } '{' rules '}'
// XXX @font-face
// XXX @page { :left | :right } '{' declarations '}'
PRBool CSSParserImpl::ParseAtRule(PRInt32& aErrorCode)
{
  nsCSSToken* tk = &mToken;
  if (mInHead && tk->mIdent.EqualsIgnoreCase("import")) {
    ParseImportRule(aErrorCode);
    return PR_TRUE;
  }

  // Skip over unsupported at rule
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    if (eCSSToken_Symbol == tk->mType) {
      PRUnichar symbol = tk->mSymbol;
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
  mInHead = PR_FALSE;
  return PR_TRUE;
}

PRBool CSSParserImpl::GatherMedia(PRInt32& aErrorCode, nsString& aMedia)
{
  PRBool first = PR_TRUE;
  PRBool expectIdent = PR_TRUE;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_Symbol == mToken.mType) {
      PRUnichar symbol = mToken.mSymbol;
      if (';' == symbol) {
        UngetToken();
        aMedia.ToLowerCase(); // case insensitive from CSS - must be lower cased
        return PR_TRUE;
      } else if (',' != symbol) {
        UngetToken();
        return PR_FALSE;
      } else if (expectIdent) {
        UngetToken();
        return PR_FALSE;
      }
      else {
        expectIdent = PR_TRUE;
      }
    }
    else if (eCSSToken_Ident == mToken.mType) {
      if (expectIdent) {
        if (! first) {
          aMedia.Append(',');
        }
        aMedia.Append(mToken.mIdent);
        first = PR_FALSE;
        expectIdent = PR_FALSE;
      }
      else {
        UngetToken();
        return PR_FALSE;
      }
    }
    else {
      UngetToken();
      break;
    }
  }
  aMedia.Truncate();
  return PR_FALSE;
}

// Parse a CSS1 import rule: "@import STRING | URL [medium [, mdeium]]"
PRBool CSSParserImpl::ParseImportRule(PRInt32& aErrorCode)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  nsAutoString url;
  nsAutoString media;

  if (eCSSToken_String == mToken.mType) {
    url = mToken.mIdent;
    if (GatherMedia(aErrorCode, media)) {
      if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
        ProcessImport(aErrorCode, url, media);
        return PR_TRUE;
      }
    }
  }
  else if (eCSSToken_Function == mToken.mType) {
    if (ExpectSymbol(aErrorCode, '(', PR_FALSE)) {
      if (GetURLToken(aErrorCode, PR_TRUE)) {
        if ((eCSSToken_String == mToken.mType) || (eCSSToken_URL == mToken.mType)) {
          url = mToken.mIdent;
          if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
            if (GatherMedia(aErrorCode, media)) {
              if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
                ProcessImport(aErrorCode, url, media);
                return PR_TRUE;
              }
            }
          }
        }
      }
    }
  }
  // invalid @import
  if ((eCSSToken_Symbol != mToken.mType) || (';' != mToken.mSymbol)) {
    SkipUntil(aErrorCode, ';');
  }

//  mInHead = PR_FALSE; // an invalid @import doesn't block other @imports (I think) awaiting clarification
  return PR_TRUE;
}


typedef PRBool (*nsStringEnumFunc)(const nsString& aSubString, void *aData);

const PRUnichar kNullCh       = PRUnichar('\0');
const PRUnichar kSingleQuote  = PRUnichar('\'');
const PRUnichar kDoubleQuote  = PRUnichar('\"');
const PRUnichar kComma        = PRUnichar(',');
const PRUnichar kHyphenCh     = PRUnichar('-');

static PRBool EnumerateString(const nsString& aStringList, nsStringEnumFunc aFunc, void* aData)
{
  PRBool    running = PR_TRUE;

  nsAutoString  stringList(aStringList); // copy to work buffer
  nsAutoString  subStr;

  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList;
  PRUnichar* end   = start;

  while (running && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;

    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }

    if ((kSingleQuote == *start) || (kDoubleQuote == *start)) { // quoted string
      PRUnichar quote = *start++;
      quoted = PR_TRUE;
      end = start;
      while (kNullCh != *end) {
        if (quote == *end) {  // found closing quote
          *end++ = kNullCh;     // end string here
          while ((kNullCh != *end) && (kComma != *end)) { // keep going until comma
            end++;
          }
          break;
        }
        end++;
      }
    }
    else {  // non-quoted string or ended
      end = start;

      while ((kNullCh != *end) && (kComma != *end)) { // look for comma
        end++;
      }
      *end = kNullCh; // end string here
    }

    // truncate at first non letter, digit or hyphen
    PRUnichar* test = start;
    while (test <= end) {
      if ((PR_FALSE == nsString::IsAlpha(*test)) && 
          (PR_FALSE == nsString::IsDigit(*test)) && (kHyphenCh != *test)) {
        *test = kNullCh;
        break;
      }
      test++;
    }
    subStr = start;

    if (PR_FALSE == quoted) {
      subStr.CompressWhitespace(PR_FALSE, PR_TRUE);
    }

    if (0 < subStr.Length()) {
      running = (*aFunc)(subStr, aData);
    }

    start = ++end;
  }

  return running;
}

static PRBool MediumEnumFunc(const nsString& aSubString, void *aData)
{
  nsIAtom*  medium = NS_NewAtom(aSubString);
  ((nsICSSStyleSheet*)aData)->AppendMedium(medium);
  return PR_TRUE;
}

PRBool CSSParserImpl::ProcessImport(PRInt32& aErrorCode, const nsString& aURLSpec, const nsString& aMedia)
{
  PRBool result = PR_FALSE;

  // XXX probably need a way to encode unicode junk for the part of
  // the url that follows a "?"
  char* cp = aURLSpec.ToNewCString();
  nsIURL* url;
  aErrorCode = NS_NewURL(&url, cp, mURL);
  delete [] cp;
  if (NS_FAILED(aErrorCode)) {
    // import url is bad
    // XXX log this somewhere for easier web page debugging
    return PR_FALSE;
  }

  if (PR_FALSE == mSheet->ContainsStyleSheet(url)) { // don't allow circular references

    nsIInputStream* in;
    nsresult rv = NS_OpenURL(url, &in);
    if (rv != NS_OK) {
      // failure to make connection
      // XXX log this somewhere for easier web page debugging
    }
    else {

      nsIUnicharInputStream* uin;
      aErrorCode = NS_NewConverterStream(&uin, nsnull, in);
      if (NS_FAILED(aErrorCode)) {
        // XXX no iso-latin-1 converter? out of memory?
        NS_RELEASE(in);
      }
      else {

        NS_RELEASE(in);

        // Create a new parse to parse the import. 

        nsICSSParser* parser;
        aErrorCode = NS_NewCSSParser(&parser);
        if (NS_SUCCEEDED(aErrorCode)) {
          nsICSSStyleSheet* childSheet = nsnull;
          aErrorCode = parser->Parse(uin, url, childSheet);
          NS_RELEASE(parser);
          if (NS_SUCCEEDED(aErrorCode) && (nsnull != childSheet)) {
            if (0 < aMedia.Length()) {
              EnumerateString(aMedia, MediumEnumFunc, childSheet);
            }
            mSheet->AppendStyleSheet(childSheet);
            result = PR_TRUE;
          }
          NS_IF_RELEASE(childSheet);
        }
        NS_RELEASE(uin);
      }
    }
  }
  NS_RELEASE(url);
  
  return result;
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

PRBool CSSParserImpl::ParseRuleSet(PRInt32& aErrorCode)
{
  // First get the list of selectors for the rule
  SelectorList* slist = nsnull;
  if (! ParseSelectorList(aErrorCode, slist)) {
    SkipRuleSet(aErrorCode);
    return PR_FALSE;
  }
  NS_ASSERTION(nsnull != slist, "null selector list");

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
      rule->SetDeclaration(declaration);
      rule->SetWeight(list->mWeight);
      rule->SetSourceSelectorText(list->mSourceString); // capture the original input (need this for namespace prefixes)
//      rule->List();
      mSheet->AppendStyleRule(rule);
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
      break;
    }
    if (eCSSToken_Symbol != tk->mType) {
      UngetToken();
      break;
    }
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
    } else {
      UngetToken();
      break;
    }
  }

  delete aListHead;
  aListHead = nsnull;
  return PR_FALSE;
}

static PRBool IsPseudoClass(const nsIAtom* aAtom)
{
  return PRBool((nsCSSAtoms::activePseudo == aAtom) || 
                (nsCSSAtoms::disabledPseudo == aAtom) ||
                (nsCSSAtoms::enabledPseudo == aAtom) ||
                (nsCSSAtoms::firstChildPseudo == aAtom) ||
                (nsCSSAtoms::focusPseudo == aAtom) ||
                (nsCSSAtoms::hoverPseudo == aAtom) ||
                (nsCSSAtoms::langPseudo == aAtom) ||
                (nsCSSAtoms::linkPseudo == aAtom) ||
                (nsCSSAtoms::outOfDatePseudo == aAtom) ||
                (nsCSSAtoms::selectedPseudo == aAtom) ||
                (nsCSSAtoms::visitedPseudo == aAtom));
}

static PRBool IsSinglePseudoClass(const nsCSSSelector& aSelector)
{
  return PRBool((aSelector.mNameSpace == kNameSpaceID_Unknown) && 
                (aSelector.mTag == nsnull) && 
                (aSelector.mID == nsnull) &&
                (aSelector.mClassList == nsnull) &&
                (aSelector.mAttrList == nsnull) &&
                (aSelector.mPseudoClassList != nsnull) &&
                (aSelector.mPseudoClassList->mNext == nsnull));
}

PRBool CSSParserImpl::ParseSelectorGroup(PRInt32& aErrorCode,
                                         SelectorList*& aList)
{
  nsAutoString  sourceBuffer;
  SelectorList* list = nsnull;
  PRUnichar     combinator = PRUnichar(0);
  PRInt32       weight = 0;
  PRBool        havePseudoElement = PR_FALSE;
  for (;;) {
    nsCSSSelector selector;
    if (! ParseSelector(aErrorCode, selector, sourceBuffer)) {
      break;
    }
    if (nsnull == list) {
      list = new SelectorList();
      if (nsnull == list) {
        aErrorCode = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
    }
    sourceBuffer.Append(PRUnichar(' '));
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
          listSel->mTag = pseudoElement;
        }
        else {  // append new pseudo element selector
          selector.Reset();
          selector.mTag = pseudoClassList->mAtom; // steal ref count
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
        sourceBuffer.Append(combinator);
        sourceBuffer.Append(PRUnichar(' '));
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
  if (PRUnichar(0) != combinator) { // no dangling combinators
    if (list) {
      delete list;
    }
    list = nsnull;
  }
  aList = list;
  if (nsnull != list) {
    sourceBuffer.Truncate(sourceBuffer.Length() - 1); // kill trailing space
    list->mSourceString = sourceBuffer;
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

/**
 * This is the format for selectors:
 * operator? [namespace \:]? element_name? [ ID | class | attrib | pseudo ]*
 */
PRBool CSSParserImpl::ParseSelector(PRInt32& aErrorCode,
                                    nsCSSSelector& aSelector,
                                    nsString& aSource)
{
  PRInt32       dataMask = 0;
  nsAutoString  buffer;

  if (! GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if ((eCSSToken_Symbol == mToken.mType) && ('*' == mToken.mSymbol)) {  // universal element selector
    // don't set any tag in the selector
    mToken.AppendToString(aSource);
    dataMask |= SEL_MASK_ELEM;
    if (! GetToken(aErrorCode, PR_FALSE)) {   // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  else if (eCSSToken_Ident == mToken.mType) {    // element name
    mToken.AppendToString(aSource);
    PRInt32 colon = mToken.mIdent.Find(':');
    if (-1 == colon) {  // no namespace
      if (mCaseSensitive) {
        aSelector.SetTag(mToken.mIdent);
      }
      else {
        mToken.mIdent.ToLowerCase(buffer);
        aSelector.SetTag(buffer);
      }
    }
    else {  // pull out the namespace
      nsAutoString  nameSpace;
      mToken.mIdent.Left(nameSpace, colon);
      mToken.mIdent.Right(buffer, (mToken.mIdent.Length() - (colon + 1)));
      if (! mCaseSensitive) {
        buffer.ToLowerCase();
      }
      // XXX lookup namespace, set it
      // deal with * namespace (== unknown)
      aSelector.SetTag(buffer);
    }
    dataMask |= SEL_MASK_ELEM;
    if (! GetToken(aErrorCode, PR_FALSE)) {   // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  for (;;) {
    if (eCSSToken_ID == mToken.mType) {   // #id
      if ((0 == (dataMask & SEL_MASK_ID)) &&  // only one ID
          (0 < mToken.mIdent.Length()) && 
          (nsString::IsAlpha(mToken.mIdent.CharAt(0)))) { // verify is legal ID
        mToken.AppendToString(aSource);
        dataMask |= SEL_MASK_ID;
        aSelector.SetID(mToken.mIdent);
      }
      else {
        UngetToken();
        return PR_FALSE;
      }
    }
    else if ((eCSSToken_Symbol == mToken.mType) && ('.' == mToken.mSymbol)) {  // .class
      mToken.AppendToString(aSource);
      if (! GetToken(aErrorCode, PR_FALSE)) { // get ident
        return PR_FALSE;
      }
      if (eCSSToken_Ident != mToken.mType) {  // malformed selector (XXX what about leading digits?)
        UngetToken();
        return PR_FALSE;
      }
      mToken.AppendToString(aSource);
      dataMask |= SEL_MASK_CLASS;
      aSelector.AddClass(mToken.mIdent);  // class always case sensitive
    }
    else if ((eCSSToken_Symbol == mToken.mType) && (':' == mToken.mSymbol)) { // :pseudo
      mToken.AppendToString(aSource);
      if (! GetToken(aErrorCode, PR_FALSE)) { // premature eof
        return PR_FALSE;
      }
      if (eCSSToken_Ident != mToken.mType) {  // malformed selector
        UngetToken();
        return PR_FALSE;
      }
      mToken.AppendToString(aSource);
      buffer.Truncate();
      buffer.Append(':');
      buffer.Append(mToken.mIdent);
      buffer.ToLowerCase();
      nsIAtom* pseudo = NS_NewAtom(buffer);
      if (IsPseudoClass(pseudo)) {
        // XXX parse lang pseudo class
        dataMask |= SEL_MASK_PCLASS;
        aSelector.AddPseudoClass(pseudo);
      }
      else {
        if (0 == (dataMask & SEL_MASK_PELEM)) {
          dataMask |= SEL_MASK_PELEM;
          aSelector.AddPseudoClass(pseudo); // store it here, it gets pulled later

          // ensure selector ends here, must be followed by EOF, space, '{' or ','
          if (GetToken(aErrorCode, PR_FALSE)) { // premature eof is ok (here!)
            if ((eCSSToken_WhiteSpace == mToken.mType) || 
                ((eCSSToken_Symbol == mToken.mType) &&
                 (('{' == mToken.mSymbol) || (',' == mToken.mSymbol)))) {
              UngetToken();
              return PR_TRUE;
            }
            UngetToken();
            return PR_FALSE;
          }
        }
        else {  // multiple pseudo elements, not legal
          UngetToken();
          NS_RELEASE(pseudo);
          return PR_FALSE;
        }
      }
      NS_RELEASE(pseudo);
    }
    else if ((eCSSToken_Symbol == mToken.mType) && ('[' == mToken.mSymbol)) {  // attribute
      mToken.AppendToString(aSource);
      if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
        return PR_FALSE;
      }
      if (eCSSToken_Ident != mToken.mType) {  // malformed selector
        UngetToken();
        return PR_FALSE;
      }
      mToken.AppendToString(aSource);
      nsAutoString  attr(mToken.mIdent);
      if (! mCaseSensitive) {
        attr.ToLowerCase();
      }
      if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
        return PR_FALSE;
      }
      if (eCSSToken_Symbol == mToken.mType) {
        mToken.AppendToString(aSource);
        PRUint8 func;
        if (']' == mToken.mSymbol) {
          dataMask |= SEL_MASK_ATTRIB;
          aSelector.AddAttribute(attr);
          func = NS_ATTR_FUNC_SET;
        }
        else if ('=' == mToken.mSymbol) {
          func = NS_ATTR_FUNC_EQUALS;
        }
        else if ('~' == mToken.mSymbol) {
          if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
            return PR_FALSE;
          }
          mToken.AppendToString(aSource);
          if ((eCSSToken_Symbol == mToken.mType) && ('=' == mToken.mSymbol)) {
            func = NS_ATTR_FUNC_INCLUDES;
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }
        else if ('|' == mToken.mSymbol) {
          if (! GetToken(aErrorCode, PR_FALSE)) { // premature EOF
            return PR_FALSE;
          }
          mToken.AppendToString(aSource);
          if ((eCSSToken_Symbol == mToken.mType) && ('=' == mToken.mSymbol)) {
            func = NS_ATTR_FUNC_DASHMATCH;
          }
          else {
            UngetToken();
            return PR_FALSE;
          }
        }
        else {
          UngetToken(); // bad function
          return PR_FALSE;
        }
        if (NS_ATTR_FUNC_SET != func) { // get value
          if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
            return PR_FALSE;
          }
          if ((eCSSToken_Ident == mToken.mType) || (eCSSToken_String == mToken.mType)) {
            mToken.AppendToString(aSource);
            nsAutoString  value(mToken.mIdent);
            if (! GetToken(aErrorCode, PR_TRUE)) { // premature EOF
              return PR_FALSE;
            }
            if ((eCSSToken_Symbol == mToken.mType) && (']' == mToken.mSymbol)) {
              mToken.AppendToString(aSource);
              dataMask |= SEL_MASK_ATTRIB;
              aSelector.AddAttribute(attr, func, value, mCaseSensitive);
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
        }
      }
      else {
        UngetToken(); // bad dog, no biscut!
        return PR_FALSE;
      }
    }
    else {  // not a selector token, we're done
      break;
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
      return nsnull;
    }
  }
  nsICSSDeclaration* declaration = nsnull;
  if (NS_OK == NS_NewCSSDeclaration(&declaration)) {
    PRInt32 count = 0;
    for (;;) {
      PRInt32 hint;
      if (ParseDeclaration(aErrorCode, declaration, aCheckForBraces, hint)) {
        count++;  // count declarations
      }
      else {
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
    if (0 == count) { // didn't get any XXX is this ok with the DOM?
      NS_RELEASE(declaration);
    }
  }
  return declaration;
}

PRBool CSSParserImpl::ParseColor(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  char cbuf[KEYWORD_BUFFER_SIZE];

  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }

  nsCSSToken* tk = &mToken;
  nscolor rgba;
  switch (tk->mType) {
    case eCSSToken_ID:
      // #xxyyzz
      tk->mIdent.ToCString(cbuf, sizeof(cbuf));
      if (NS_HexToRGB(cbuf, &rgba)) {
        aValue.SetColorValue(rgba);
        return PR_TRUE;
      }
      break;

    case eCSSToken_Ident:
      tk->mIdent.ToCString(cbuf, sizeof(cbuf));
      if (NS_ColorNameToRGB(cbuf, &rgba)) {
        aValue.SetStringValue(tk->mIdent, eCSSUnit_String);
        return PR_TRUE;
      }
      break;
    case eCSSToken_Function:
      // rgb ( component , component , component )
      PRUint8 r, g, b;
      if (ExpectSymbol(aErrorCode, '(', PR_FALSE) &&
          ParseColorComponent(aErrorCode, r, ',') &&
          ParseColorComponent(aErrorCode, g, ',') &&
          ParseColorComponent(aErrorCode, b, ')')) {
        nscolor rgba = NS_RGB(r,g,b);
        aValue.SetColorValue(rgba);
        return PR_TRUE;
      }
      return PR_FALSE;  // already pushed back

    default:
      break;
  }

  // It's not a color
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseColorComponent(PRInt32& aErrorCode,
                                          PRUint8& aComponent,
                                          char aStop)
{
  if (!GetToken(aErrorCode, PR_TRUE)) {
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
    UngetToken();
    return PR_FALSE;
  }
  if (ExpectSymbol(aErrorCode, aStop, PR_TRUE)) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    aComponent = (PRUint8) value;
    return PR_TRUE;
  }
  return PR_FALSE;
}

//----------------------------------------------------------------------

PRBool
CSSParserImpl::ParseDeclaration(PRInt32& aErrorCode,
                                nsICSSDeclaration* aDeclaration,
                                PRBool aCheckForBraces,
                                PRInt32& aChangeHint)
{
  aChangeHint = NS_STYLE_HINT_UNKNOWN;

  // Get property name
  nsCSSToken* tk = &mToken;
  char propertyName[100];
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    if (eCSSToken_Ident == tk->mType) {
      tk->mIdent.ToCString(propertyName, sizeof(propertyName));
      // grab the ident before the ExpectSymbol trashes the token
      if (!ExpectSymbol(aErrorCode, ':', PR_TRUE)) {
        return PR_FALSE;
      }
      break;
    }
    if ((eCSSToken_Symbol == tk->mType) && (';' == tk->mSymbol)) {
      // dangling semicolons are skipped
      continue;
    }

    // Not a declaration...
    UngetToken();
    return PR_FALSE;
  }

  // Map property name to it's ID and then parse the property
  PRInt32 propID = nsCSSProps::LookupName(propertyName);
  if (0 > propID) { // unknown property
    return PR_FALSE;
  }
  if (! ParseProperty(aErrorCode, aDeclaration, propID, aChangeHint)) {
    return PR_FALSE;
  }

  // See if the declaration is followed by a "!important" declaration
  PRBool isImportant = PR_FALSE;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    if (aCheckForBraces) {
      // Premature eof is not ok when proper termination is mandated
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
          return PR_FALSE;
        }
        if ((eCSSToken_Ident != tk->mType) ||
            !tk->mIdent.EqualsIgnoreCase("important")) {
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
      return PR_FALSE;
    }
    if ('}' == tk->mSymbol) {
      UngetToken();
      return PR_TRUE;
    }
  }
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

static const PRInt32 kBorderTopIDs[] = {
  PROP_BORDER_TOP_WIDTH,
  PROP_BORDER_TOP_STYLE,
  PROP_BORDER_TOP_COLOR
};
static const PRInt32 kBorderRightIDs[] = {
  PROP_BORDER_RIGHT_WIDTH,
  PROP_BORDER_RIGHT_STYLE,
  PROP_BORDER_RIGHT_COLOR
};
static const PRInt32 kBorderBottomIDs[] = {
  PROP_BORDER_BOTTOM_WIDTH,
  PROP_BORDER_BOTTOM_STYLE,
  PROP_BORDER_BOTTOM_COLOR
};
static const PRInt32 kBorderLeftIDs[] = {
  PROP_BORDER_LEFT_WIDTH,
  PROP_BORDER_LEFT_STYLE,
  PROP_BORDER_LEFT_COLOR
};

PRInt32 CSSParserImpl::SearchKeywordTable(PRInt32 aKeywordID, const PRInt32 aKeywordTable[])
{
  PRInt32 index = 0;
  while (0 <= aKeywordTable[index]) {
    if (aKeywordID == aKeywordTable[index++]) {
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
  char cbuf[KEYWORD_BUFFER_SIZE];
  ident->ToCString(cbuf, sizeof(cbuf));
  PRInt32 keyword = nsCSSKeywords::LookupName(cbuf);
  if (0 <= keyword) {
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

PRBool CSSParserImpl::TranslateDimension(nsCSSValue& aValue,
                                         PRInt32 aVariantMask,
                                         float aNumber,
                                         const nsString& aUnit)
{
  nsCSSUnit units;
  PRInt32   type = 0;
  if (0 != aUnit.Length()) {
    char cbuf[KEYWORD_BUFFER_SIZE];
    aUnit.ToCString(cbuf, sizeof(cbuf));
    PRInt32 id = nsCSSKeywords::LookupName(cbuf);
    switch (id) {
      case KEYWORD_EM:    units = eCSSUnit_EM;          type = VARIANT_LENGTH;  break;
      case KEYWORD_EX:    units = eCSSUnit_XHeight;     type = VARIANT_LENGTH;  break;
      case KEYWORD_PX:    units = eCSSUnit_Pixel;       type = VARIANT_LENGTH;  break;
      case KEYWORD_IN:    units = eCSSUnit_Inch;        type = VARIANT_LENGTH;  break;
      case KEYWORD_CM:    units = eCSSUnit_Centimeter;  type = VARIANT_LENGTH;  break;
      case KEYWORD_MM:    units = eCSSUnit_Millimeter;  type = VARIANT_LENGTH;  break;
      case KEYWORD_PT:    units = eCSSUnit_Point;       type = VARIANT_LENGTH;  break;
      case KEYWORD_PC:    units = eCSSUnit_Pica;        type = VARIANT_LENGTH;  break;

      case KEYWORD_DEG:   units = eCSSUnit_Degree;      type = VARIANT_ANGLE;   break;
      case KEYWORD_GRAD:  units = eCSSUnit_Grad;        type = VARIANT_ANGLE;   break;
      case KEYWORD_RAD:   units = eCSSUnit_Radian;      type = VARIANT_ANGLE;   break;

      case KEYWORD_HZ:    units = eCSSUnit_Hertz;       type = VARIANT_FREQUENCY; break;
      case KEYWORD_KHZ:   units = eCSSUnit_Kilohertz;   type = VARIANT_FREQUENCY; break;

      case KEYWORD_S:     units = eCSSUnit_Seconds;       type = VARIANT_TIME;  break;
      case KEYWORD_MS:    units = eCSSUnit_Milliseconds;  type = VARIANT_TIME;  break;
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
    char cbuf[KEYWORD_BUFFER_SIZE];
    tk->mIdent.ToCString(cbuf, sizeof(cbuf));
    PRInt32 keyword = nsCSSKeywords::LookupName(cbuf);
    if (0 <= keyword) { // known keyword
      if ((aVariantMask & VARIANT_AUTO) != 0) {
        if (KEYWORD_AUTO == keyword) {
          aValue.SetAutoValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_INHERIT) != 0) {
        if (KEYWORD_INHERIT == keyword) {
          aValue.SetInheritValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_NONE) != 0) {
        if (KEYWORD_NONE == keyword) {
          aValue.SetNoneValue();
          return PR_TRUE;
        }
      }
      if ((aVariantMask & VARIANT_NORMAL) != 0) {
        if (KEYWORD_NORMAL == keyword) {
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
      tk->isDimension()) {
    return TranslateDimension(aValue, aVariantMask, tk->mNumber, tk->mIdent);
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
  if (PR_TRUE == mNavQuirkMode) { // NONSTANDARD: Nav interprets unitless numbers as px
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
    if ((eCSSToken_ID == tk->mType) || 
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
    if (ParseCounter(aErrorCode, aValue)) {
      return PR_TRUE;
    }
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
            counter.Append(',');
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
            char cbuf[KEYWORD_BUFFER_SIZE];
            mToken.mIdent.ToCString(cbuf, sizeof(cbuf));
            PRInt32 keyword = nsCSSKeywords::LookupName(cbuf);
            if ((0 <= keyword) && 
                (0 < SearchKeywordTable(keyword, nsCSSProps::kListStyleKTable))) {
              counter.Append(',');
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
      if ((eCSSToken_String == mToken.mType) || 
          (eCSSToken_Ident == mToken.mType)) {
        nsAutoString  attr;
        if (eCSSToken_String == mToken.mType) {
          attr.Append(mToken.mSymbol);
          attr.Append(mToken.mIdent);
          attr.Append(mToken.mSymbol);
        }
        else {
          attr.Append(mToken.mIdent);
        }
        if (ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
          aValue.SetStringValue(attr, eCSSUnit_Attr);
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
      if (nsnull != mURL) {
        nsString baseURL;
        nsresult rv = NS_MakeAbsoluteURL(mURL, baseURL, tk->mIdent, absURL);
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
                                   const PRInt32 aPropIDs[], PRInt32 aNumIDs)
{
  PRInt32 found = 0;
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
    }
    else {  // more than one value, verify no inherits
      for (loop = 0; loop < aNumIDs; loop++) {
        if (eCSSUnit_Inherit == aValues[loop].GetUnit()) {
          found = -1;
          break;
        }
      }
    }
  }
  return found;
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values. 
 */
PRBool CSSParserImpl::ParseBoxProperties(PRInt32& aErrorCode,
                                         nsICSSDeclaration* aDeclaration,
                                         const PRInt32 aPropIDs[])
{
  // Get up to four values for the property
  nsCSSValue  values[4];
  PRInt32 count = 0;
  PRInt32 index;
  for (index = 0; index < 4; index++) {
    if (! ParseSingleValueProperty(aErrorCode, values[index], aPropIDs[index])) {
      break;
    }
    count++;
  }
  if ((count == 0) || (PR_FALSE == ExpectEndProperty(aErrorCode, PR_TRUE))) {
    return PR_FALSE;
  }

  if (1 < count) { // verify no more than single inherit
    for (index = 0; index < 4; index++) {
      if (eCSSUnit_Inherit == values[index].GetUnit()) {
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
    aDeclaration->AppendValue(aPropIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseProperty(PRInt32& aErrorCode,
                                    nsICSSDeclaration* aDeclaration,
                                    PRInt32 aPropID,
                                    PRInt32& aChangeHint)
{
  // Strip out properties we use internally. These properties are used
  // by compound property parsing routines (e.g. "background-position").
  switch (aPropID) {
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BORDER_X_SPACING:
    case PROP_BORDER_Y_SPACING:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_TOP:
      // The user can't use these
      return PR_FALSE;
    default:
      break;
  }

  if (aChangeHint < nsCSSProps::kHintTable[aPropID]) {
    aChangeHint = nsCSSProps::kHintTable[aPropID];
  }

  return ParseProperty(aErrorCode, aDeclaration, aPropID);
}

PRBool CSSParserImpl::ParseProperty(PRInt32& aErrorCode,
                                    nsICSSDeclaration* aDeclaration,
                                    PRInt32 aPropID)
{
  switch (aPropID) {  // handle shorthand or multiple properties
  case PROP_BACKGROUND:
    return ParseBackground(aErrorCode, aDeclaration);
  case PROP_BACKGROUND_POSITION:
    return ParseBackgroundPosition(aErrorCode, aDeclaration);
  case PROP_BORDER:
    return ParseBorder(aErrorCode, aDeclaration);
  case PROP_BORDER_COLOR:
    return ParseBorderColor(aErrorCode, aDeclaration);
  case PROP_BORDER_SPACING:
    return ParseBorderSpacing(aErrorCode, aDeclaration);
  case PROP_BORDER_STYLE:
    return ParseBorderStyle(aErrorCode, aDeclaration);
  case PROP_BORDER_BOTTOM:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderBottomIDs);
  case PROP_BORDER_LEFT:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderLeftIDs);
  case PROP_BORDER_RIGHT:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderRightIDs);
  case PROP_BORDER_TOP:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderTopIDs);
  case PROP_BORDER_WIDTH:
    return ParseBorderWidth(aErrorCode, aDeclaration);
  case PROP_CLIP:
    return ParseClip(aErrorCode, aDeclaration);
  case PROP_CONTENT:
    return ParseContent(aErrorCode, aDeclaration);
  case PROP_COUNTER_INCREMENT:
  case PROP_COUNTER_RESET:
    return ParseCounterData(aErrorCode, aDeclaration, aPropID);
  case PROP_CUE:
    return ParseCue(aErrorCode, aDeclaration);
  case PROP_CURSOR:
    return ParseCursor(aErrorCode, aDeclaration);
  case PROP_FONT:
    return ParseFont(aErrorCode, aDeclaration);
  case PROP_LIST_STYLE:
    return ParseListStyle(aErrorCode, aDeclaration);
  case PROP_MARGIN:
    return ParseMargin(aErrorCode, aDeclaration);
  case PROP_OUTLINE:
    return ParseOutline(aErrorCode, aDeclaration);
  case PROP_PADDING:
    return ParsePadding(aErrorCode, aDeclaration);
  case PROP_PAUSE:
    return ParsePause(aErrorCode, aDeclaration);
  case PROP_PLAY_DURING:
    return ParsePlayDuring(aErrorCode, aDeclaration);
  case PROP_QUOTES:
    return ParseQuotes(aErrorCode, aDeclaration);
  case PROP_SIZE:
    return ParseSize(aErrorCode, aDeclaration);
  case PROP_TEXT_SHADOW:
    return ParseTextShadow(aErrorCode, aDeclaration);

  default:  // must be single property
    {
      nsCSSValue  value;
      if (ParseSingleValueProperty(aErrorCode, value, aPropID)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aDeclaration->AppendValue(aPropID, value);
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

// Bits used in determining which background position info we have
#define BG_CENTER  0
#define BG_TOP     1
#define BG_BOTTOM  2
#define BG_LEFT    4
#define BG_RIGHT   8
#define BG_CENTER1 16
#define BG_CENTER2 32

// Note: Don't change this table unless you update
// parseBackgroundPosition!

static const PRInt32 kBackgroundXYPositionKTable[] = {
  KEYWORD_CENTER, BG_CENTER,
  KEYWORD_TOP, BG_TOP,
  KEYWORD_BOTTOM, BG_BOTTOM,
  KEYWORD_LEFT, BG_LEFT,
  KEYWORD_RIGHT, BG_RIGHT,
  -1,
};

PRBool CSSParserImpl::ParseSingleValueProperty(PRInt32& aErrorCode,
                                               nsCSSValue& aValue,
                                               PRInt32 aPropID)
{
  switch (aPropID) {
  case PROP_BACKGROUND:
  case PROP_BACKGROUND_POSITION:
  case PROP_BORDER:
  case PROP_BORDER_COLOR:
  case PROP_BORDER_SPACING:
  case PROP_BORDER_STYLE:
  case PROP_BORDER_BOTTOM:
  case PROP_BORDER_LEFT:
  case PROP_BORDER_RIGHT:
  case PROP_BORDER_TOP:
  case PROP_BORDER_WIDTH:
  case PROP_CLIP:
  case PROP_CONTENT:
  case PROP_COUNTER_INCREMENT:
  case PROP_COUNTER_RESET:
  case PROP_CUE:
  case PROP_CURSOR:
  case PROP_FONT:
  case PROP_LIST_STYLE:
  case PROP_MARGIN:
  case PROP_OUTLINE:
  case PROP_PADDING:
  case PROP_PAUSE:
  case PROP_QUOTES:
  case PROP_SIZE:
  case PROP_TEXT_SHADOW:
    NS_ERROR("not a single value property");
    return PR_FALSE;

  case PROP_AZIMUTH:
    return ParseAzimuth(aErrorCode, aValue);
  case PROP_BACKGROUND_ATTACHMENT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundAttachmentKTable);
  case PROP_BACKGROUND_COLOR:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK,
                        nsCSSProps::kBackgroundColorKTable);
  case PROP_BACKGROUND_FILTER:  // XXX
    return ParseBackgroundFilter(aErrorCode, aValue);
  case PROP_BACKGROUND_IMAGE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case PROP_BACKGROUND_REPEAT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBackgroundRepeatKTable);
  case PROP_BACKGROUND_X_POSITION:
  case PROP_BACKGROUND_Y_POSITION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKLP,
                        kBackgroundXYPositionKTable);
  case PROP_BORDER_COLLAPSE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kBorderCollapseKTable);
  case PROP_BORDER_BOTTOM_COLOR:
  case PROP_BORDER_LEFT_COLOR:
  case PROP_BORDER_RIGHT_COLOR:
  case PROP_BORDER_TOP_COLOR:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK, 
                        nsCSSProps::kBorderColorKTable);
  case PROP_BORDER_BOTTOM_STYLE:
  case PROP_BORDER_LEFT_STYLE:
  case PROP_BORDER_RIGHT_STYLE:
  case PROP_BORDER_TOP_STYLE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kBorderStyleKTable);
  case PROP_BORDER_BOTTOM_WIDTH:
  case PROP_BORDER_LEFT_WIDTH:
  case PROP_BORDER_RIGHT_WIDTH:
  case PROP_BORDER_TOP_WIDTH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKL,
                        nsCSSProps::kBorderWidthKTable);
  case PROP_BOTTOM:
  case PROP_TOP:
  case PROP_LEFT:
  case PROP_RIGHT:
	  return ParseVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case PROP_HEIGHT:
  case PROP_WIDTH:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case PROP_CAPTION_SIDE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, 
                        nsCSSProps::kCaptionSideKTable);
  case PROP_CLEAR:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kClearKTable);
  case PROP_COLOR:
    return ParseVariant(aErrorCode, aValue, VARIANT_HC, nsnull);
  case PROP_CUE_AFTER:
  case PROP_CUE_BEFORE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case PROP_DIRECTION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kDirectionKTable);
  case PROP_DISPLAY:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kDisplayKTable);
  case PROP_ELEVATION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK | VARIANT_ANGLE,
                        nsCSSProps::kElevationKTable);
  case PROP_EMPTY_CELLS:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kEmptyCellsKTable);
  case PROP_FILTER:
    return ParseForegroundFilter(aErrorCode, aValue);
  case PROP_FLOAT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kFloatKTable);
  case PROP_FONT_FAMILY:
    return ParseFamily(aErrorCode, aValue);
  case PROP_FONT_SIZE:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HKLP,
                        nsCSSProps::kFontSizeKTable);
  case PROP_FONT_SIZE_ADJUST:
    return ParseVariant(aErrorCode, aValue, VARIANT_HON,
                        nsnull);
  case PROP_FONT_STRETCH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontStretchKTable);
  case PROP_FONT_STYLE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontStyleKTable);
  case PROP_FONT_VARIANT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kFontVariantKTable);
  case PROP_FONT_WEIGHT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMKI,
                        nsCSSProps::kFontWeightKTable);
  case PROP_LETTER_SPACING:
  case PROP_WORD_SPACING:
    return ParseVariant(aErrorCode, aValue, VARIANT_HL | VARIANT_NORMAL, nsnull);
  case PROP_LINE_HEIGHT:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLPN | VARIANT_NORMAL, nsnull);
  case PROP_LIST_STYLE_IMAGE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HUO, nsnull);
  case PROP_LIST_STYLE_POSITION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, nsCSSProps::kListStylePositionKTable);
  case PROP_LIST_STYLE_TYPE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK, nsCSSProps::kListStyleKTable);
  case PROP_MARGIN_BOTTOM:
  case PROP_MARGIN_LEFT:
  case PROP_MARGIN_RIGHT:
  case PROP_MARGIN_TOP:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHLP, nsnull);
  case PROP_MARKER_OFFSET:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHL, nsnull);
  case PROP_MARKS:
    return ParseMarks(aErrorCode, aValue);
  case PROP_MAX_HEIGHT:
  case PROP_MAX_WIDTH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLPO, nsnull);
  case PROP_MIN_HEIGHT:
  case PROP_MIN_WIDTH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case PROP_OPACITY:
    return ParseVariant(aErrorCode, aValue, VARIANT_HPN, nsnull);
  case PROP_ORPHANS:
  case PROP_WIDOWS:
    return ParseVariant(aErrorCode, aValue, VARIANT_HI, nsnull);
  case PROP_OUTLINE_COLOR:
    return ParseVariant(aErrorCode, aValue, VARIANT_HCK, 
                        nsCSSProps::kOutlineColorKTable);
  case PROP_OUTLINE_STYLE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK, 
                        nsCSSProps::kBorderStyleKTable);
  case PROP_OUTLINE_WIDTH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKL,
                        nsCSSProps::kBorderWidthKTable);
  case PROP_OVERFLOW:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kOverflowKTable);
  case PROP_PADDING_BOTTOM:
  case PROP_PADDING_LEFT:
  case PROP_PADDING_RIGHT:
  case PROP_PADDING_TOP:
    return ParsePositiveVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case PROP_PAGE:
    return ParseVariant(aErrorCode, aValue, VARIANT_AUTO | VARIANT_IDENTIFIER, nsnull);
  case PROP_PAGE_BREAK_AFTER:
  case PROP_PAGE_BREAK_BEFORE:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK, 
                        nsCSSProps::kPageBreakKTable);
  case PROP_PAGE_BREAK_INSIDE:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK, 
                        nsCSSProps::kPageBreakInsideKTable);
  case PROP_PAUSE_AFTER:
  case PROP_PAUSE_BEFORE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HTP, nsnull);
  case PROP_PITCH:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKF, nsCSSProps::kPitchKTable);
  case PROP_PITCH_RANGE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case PROP_POSITION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, nsCSSProps::kPositionKTable);
  case PROP_RICHNESS:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case PROP_SPEAK:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK | VARIANT_NONE,
                        nsCSSProps::kSpeakKTable);
  case PROP_SPEAK_HEADER:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakHeaderKTable);
  case PROP_SPEAK_NUMERAL:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakNumeralKTable);
  case PROP_SPEAK_PUNCTUATION:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK,
                        nsCSSProps::kSpeakPunctuationKTable);
  case PROP_SPEECH_RATE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN | VARIANT_KEYWORD,
                        nsCSSProps::kSpeechRateKTable);
  case PROP_STRESS:
    return ParseVariant(aErrorCode, aValue, VARIANT_HN, nsnull);
  case PROP_TABLE_LAYOUT:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHK,
                        nsCSSProps::kTableLayoutKTable);
  case PROP_TEXT_ALIGN:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK | VARIANT_STRING,
                        nsCSSProps::kTextAlignKTable);
  case PROP_TEXT_DECORATION:
    return ParseTextDecoration(aErrorCode, aValue);
  case PROP_TEXT_INDENT:
    return ParseVariant(aErrorCode, aValue, VARIANT_HLP, nsnull);
  case PROP_TEXT_TRANSFORM:
    return ParseVariant(aErrorCode, aValue, VARIANT_HOK,
                        nsCSSProps::kTextTransformKTable);
  case PROP_UNICODE_BIDI:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kUnicodeBidiKTable);
  case PROP_VERTICAL_ALIGN:
    return ParseVariant(aErrorCode, aValue, VARIANT_HKLP,
                        nsCSSProps::kVerticalAlignKTable);
  case PROP_VISIBILITY:
    return ParseVariant(aErrorCode, aValue, VARIANT_HK, 
                        nsCSSProps::kVisibilityKTable);
  case PROP_VOICE_FAMILY:
    return ParseFamily(aErrorCode, aValue);
  case PROP_VOLUME:
    return ParseVariant(aErrorCode, aValue, VARIANT_HPN | VARIANT_KEYWORD,
                        nsCSSProps::kVolumeKTable);
  case PROP_WHITE_SPACE:
    return ParseVariant(aErrorCode, aValue, VARIANT_HMK,
                        nsCSSProps::kWhitespaceKTable);
  case PROP_Z_INDEX:
    return ParseVariant(aErrorCode, aValue, VARIANT_AHI, nsnull);
  }
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

PRBool CSSParserImpl::ParseBackground(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  const PRInt32 numProps = 6;
  static const PRInt32 kBackgroundIDs[numProps] = {
    PROP_BACKGROUND_COLOR,
    PROP_BACKGROUND_IMAGE,
    PROP_BACKGROUND_REPEAT,
    PROP_BACKGROUND_ATTACHMENT,
    PROP_BACKGROUND_X_POSITION,
    PROP_BACKGROUND_Y_POSITION
//    PROP_BACKGROUND_FILTER
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
      else {
        values[5].SetPercentValue(50.0f);
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

  // Note: no default for filter (yet)

  PRInt32 index;
  for (index = 0; index < numProps; index++) {
    aDeclaration->AppendValue(kBackgroundIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBackgroundPosition(PRInt32& aErrorCode,
                                              nsICSSDeclaration* aDeclaration)
{
  // First try a number or a length value
  nsCSSValue  xValue;
  if (ParseVariant(aErrorCode, xValue, VARIANT_HLP, nsnull)) {
    if (eCSSUnit_Inherit == xValue.GetUnit()) {  // both are inherited
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        aDeclaration->AppendValue(PROP_BACKGROUND_X_POSITION, xValue);
        aDeclaration->AppendValue(PROP_BACKGROUND_Y_POSITION, xValue);
        return PR_TRUE;
      }
      return PR_FALSE;
    }
    // We have one number/length. Get the optional second number/length.
    nsCSSValue yValue;
    if (ParseVariant(aErrorCode, yValue, VARIANT_LP, nsnull)) {
      // We have two numbers
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        aDeclaration->AppendValue(PROP_BACKGROUND_X_POSITION, xValue);
        aDeclaration->AppendValue(PROP_BACKGROUND_Y_POSITION, yValue);
        return PR_TRUE;
      }
      return PR_FALSE;
    }

    // We have one number which is the x position. Create an value for
    // the vertical position which is of value 50%
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_BACKGROUND_X_POSITION, xValue);
      aDeclaration->AppendValue(PROP_BACKGROUND_Y_POSITION, nsCSSValue(0.5f, eCSSUnit_Percent));
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
    if (0 == bit) {
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
    aDeclaration->AppendValue(PROP_BACKGROUND_X_POSITION, nsCSSValue(xEnumValue, eCSSUnit_Enumerated));
    aDeclaration->AppendValue(PROP_BACKGROUND_Y_POSITION, nsCSSValue(yEnumValue, eCSSUnit_Enumerated));
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseBackgroundFilter(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  // XXX not yet supported
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseForegroundFilter(PRInt32& aErrorCode, nsCSSValue& aValue)
{
  // XXX not yet supported
  return PR_FALSE;
}

// These must be in CSS order (top,right,bottom,left) for indexing to work
static const PRInt32 kBorderStyleIDs[] = {
  PROP_BORDER_TOP_STYLE,
  PROP_BORDER_RIGHT_STYLE,
  PROP_BORDER_BOTTOM_STYLE,
  PROP_BORDER_LEFT_STYLE
};
static const PRInt32 kBorderWidthIDs[] = {
  PROP_BORDER_TOP_WIDTH,
  PROP_BORDER_RIGHT_WIDTH,
  PROP_BORDER_BOTTOM_WIDTH,
  PROP_BORDER_LEFT_WIDTH
};
static const PRInt32 kBorderColorIDs[] = {
  PROP_BORDER_TOP_COLOR,
  PROP_BORDER_RIGHT_COLOR,
  PROP_BORDER_BOTTOM_COLOR,
  PROP_BORDER_LEFT_COLOR
};


PRBool CSSParserImpl::ParseBorder(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  const PRInt32 numProps = 3;
  static const PRInt32 kBorderIDs[] = {
    PROP_BORDER_TOP_WIDTH,  // only one value per property
    PROP_BORDER_TOP_STYLE,
    PROP_BORDER_TOP_COLOR
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

  if (0 == (found & 4)) { // clear missing color values so color property will be used
    values[2].Reset();
  }

  PRInt32 index;
  for (index = 0; index < 4; index++) {
    aDeclaration->AppendValue(kBorderWidthIDs[index], values[0]);
    aDeclaration->AppendValue(kBorderStyleIDs[index], values[1]);
    aDeclaration->AppendValue(kBorderColorIDs[index], values[2]);
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderColor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderColorIDs);
}

PRBool CSSParserImpl::ParseBorderSpacing(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  nsCSSValue  xValue;
  if (ParseVariant(aErrorCode, xValue, VARIANT_HL, nsnull)) {
    if (xValue.IsLengthUnit()) {
      // We have one length. Get the optional second length.
      nsCSSValue yValue;
      if (ParseVariant(aErrorCode, yValue, VARIANT_LENGTH, nsnull)) {
        // We have two numbers
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aDeclaration->AppendValue(PROP_BORDER_X_SPACING, xValue);
          aDeclaration->AppendValue(PROP_BORDER_Y_SPACING, yValue);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }

    // We have one length which is the horizontal spacing. Create a value for
    // the vertical spacing which is equal
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_BORDER_X_SPACING, xValue);
      aDeclaration->AppendValue(PROP_BORDER_Y_SPACING, xValue);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseBorderSide(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration,
                                      const PRInt32 aPropIDs[])
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
    aDeclaration->AppendValue(aPropIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderStyleIDs);
}

PRBool CSSParserImpl::ParseBorderWidth(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  return ParseBoxProperties(aErrorCode, aDeclaration, kBorderWidthIDs);
}

PRBool CSSParserImpl::ParseClip(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const PRInt32 kClipIDs[] = {
    PROP_CLIP_TOP,
    PROP_CLIP_RIGHT,
    PROP_CLIP_BOTTOM,
    PROP_CLIP_LEFT
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
        aDeclaration->AppendValue(kClipIDs[index], valueAuto);
      }
      return PR_TRUE;
    }
  } else if ((eCSSToken_Ident == mToken.mType) && 
             mToken.mIdent.EqualsIgnoreCase("inherit")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      nsCSSValue  inherit(eCSSUnit_Inherit);
      for (index = 0; index < 4; index++) {
        aDeclaration->AppendValue(kClipIDs[index], inherit);
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
      if (! ParsePositiveVariant(aErrorCode, values[index], VARIANT_AL, nsnull)) {
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
        aDeclaration->AppendValue(kClipIDs[index], values[index]);
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
PRBool CSSParserImpl::ParseContent(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  nsCSSValue  value;
  if (ParseVariant(aErrorCode, value, VARIANT_CONTENT | VARIANT_INHERIT, 
                   nsCSSProps::kContentKTable)) {
    if (eCSSUnit_Inherit == value.GetUnit()) {
      if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
        aDeclaration->AppendValue(PROP_CONTENT, value);
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
        aErrorCode = aDeclaration->AppendStructValue(PROP_CONTENT, listHead);
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
                                       PRInt32 aPropID)
{
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  if (ident->EqualsIgnoreCase("none")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(aPropID, nsCSSValue(eCSSUnit_None));
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  else if (ident->EqualsIgnoreCase("inherit")) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(aPropID, nsCSSValue(eCSSUnit_Inherit));
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
        aErrorCode = aDeclaration->AppendStructValue(aPropID, dataHead);
        return NS_SUCCEEDED(aErrorCode);
      }
      if (! GetToken(aErrorCode, PR_TRUE)) {
        break;
      }
      if ((eCSSToken_Number == mToken.mType) && (mToken.mIntegerValid)) {
        data->mValue.SetIntValue(mToken.mInteger, eCSSUnit_Integer);
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
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

PRBool CSSParserImpl::ParseCue(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  nsCSSValue before;
  if (ParseSingleValueProperty(aErrorCode, before, PROP_CUE_BEFORE)) {
    if (eCSSUnit_URL == before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, PROP_CUE_AFTER)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aDeclaration->AppendValue(PROP_CUE_BEFORE, before);
          aDeclaration->AppendValue(PROP_CUE_AFTER, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_CUE_BEFORE, before);
      aDeclaration->AppendValue(PROP_CUE_AFTER, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseCursor(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
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
          aErrorCode = aDeclaration->AppendStructValue(PROP_CURSOR, listHead);
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
      aDeclaration->AppendValue(PROP_CURSOR, value);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


PRBool CSSParserImpl::ParseFont(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const PRInt32 fontIDs[] = {
    PROP_FONT_STYLE,
    PROP_FONT_VARIANT,
    PROP_FONT_WEIGHT
  };

  nsCSSValue  family;
  if (ParseVariant(aErrorCode, family, VARIANT_HK, nsCSSProps::kFontKTable)) {
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      if (eCSSUnit_Inherit == family.GetUnit()) {
        aDeclaration->AppendValue(PROP_FONT_FAMILY, family);
        aDeclaration->AppendValue(PROP_FONT_STYLE, family);
        aDeclaration->AppendValue(PROP_FONT_VARIANT, family);
        aDeclaration->AppendValue(PROP_FONT_WEIGHT, family);
        aDeclaration->AppendValue(PROP_FONT_SIZE, family);
        aDeclaration->AppendValue(PROP_LINE_HEIGHT, family);
        aDeclaration->AppendValue(PROP_FONT_STRETCH, family);
        aDeclaration->AppendValue(PROP_FONT_SIZE_ADJUST, family);
      }
      else {
        aDeclaration->AppendValue(PROP_FONT_FAMILY, family);  // keyword value overrides everything else
        nsCSSValue empty;
        aDeclaration->AppendValue(PROP_FONT_STYLE, empty);
        aDeclaration->AppendValue(PROP_FONT_VARIANT, empty);
        aDeclaration->AppendValue(PROP_FONT_WEIGHT, empty);
        aDeclaration->AppendValue(PROP_FONT_SIZE, empty);
        aDeclaration->AppendValue(PROP_LINE_HEIGHT, empty);
        aDeclaration->AppendValue(PROP_FONT_STRETCH, empty);
        aDeclaration->AppendValue(PROP_FONT_SIZE_ADJUST, empty);
      }
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // Get optional font-style, font-variant and font-weight (in any order)
  const PRInt32 numProps = 3;
  nsCSSValue  values[numProps];
  PRInt32 found = ParseChoice(aErrorCode, values, fontIDs, numProps);
  if ((found < 0) || (eCSSUnit_Inherit == values[0].GetUnit())) { // illegal data
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
    if ((eCSSUnit_Inherit != family.GetUnit()) && ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_FONT_FAMILY, family);
      aDeclaration->AppendValue(PROP_FONT_STYLE, values[0]);
      aDeclaration->AppendValue(PROP_FONT_VARIANT, values[1]);
      aDeclaration->AppendValue(PROP_FONT_WEIGHT, values[2]);
      aDeclaration->AppendValue(PROP_FONT_SIZE, size);
      aDeclaration->AppendValue(PROP_LINE_HEIGHT, lineHeight);
      aDeclaration->AppendValue(PROP_FONT_STRETCH, nsCSSValue(eCSSUnit_Normal));
      aDeclaration->AppendValue(PROP_FONT_SIZE_ADJUST, nsCSSValue(eCSSUnit_None));
      return PR_TRUE;
    }
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

PRBool CSSParserImpl::ParseListStyle(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  const PRInt32 numProps = 3;
  static const PRInt32 listStyleIDs[] = {
    PROP_LIST_STYLE_TYPE,
    PROP_LIST_STYLE_POSITION,
    PROP_LIST_STYLE_IMAGE
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
    aDeclaration->AppendValue(listStyleIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseMargin(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const PRInt32 kMarginSideIDs[] = {
    PROP_MARGIN_TOP,
    PROP_MARGIN_RIGHT,
    PROP_MARGIN_BOTTOM,
    PROP_MARGIN_LEFT
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kMarginSideIDs);
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

PRBool CSSParserImpl::ParseOutline(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  const PRInt32 numProps = 3;
  static const PRInt32 kOutlineIDs[] = {
    PROP_OUTLINE_COLOR,
    PROP_OUTLINE_STYLE,
    PROP_OUTLINE_WIDTH
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
    aDeclaration->AppendValue(kOutlineIDs[index], values[index]);
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParsePadding(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const PRInt32 kPaddingSideIDs[] = {
    PROP_PADDING_TOP,
    PROP_PADDING_RIGHT,
    PROP_PADDING_BOTTOM,
    PROP_PADDING_LEFT
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kPaddingSideIDs);
}

PRBool CSSParserImpl::ParsePause(PRInt32& aErrorCode,
                                 nsICSSDeclaration* aDeclaration)
{
  nsCSSValue  before;
  if (ParseSingleValueProperty(aErrorCode, before, PROP_PAUSE_BEFORE)) {
    if (eCSSUnit_Inherit != before.GetUnit()) {
      nsCSSValue after;
      if (ParseSingleValueProperty(aErrorCode, after, PROP_PAUSE_AFTER)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aDeclaration->AppendValue(PROP_PAUSE_BEFORE, before);
          aDeclaration->AppendValue(PROP_PAUSE_AFTER, after);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_PAUSE_BEFORE, before);
      aDeclaration->AppendValue(PROP_PAUSE_AFTER, before);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParsePlayDuring(PRInt32& aErrorCode,
                                      nsICSSDeclaration* aDeclaration)
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
      aDeclaration->AppendValue(PROP_PLAY_DURING, playDuring);
      aDeclaration->AppendValue(PROP_PLAY_DURING_FLAGS, flags);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseQuotes(PRInt32& aErrorCode,
                                  nsICSSDeclaration* aDeclaration)
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
            aErrorCode = aDeclaration->AppendStructValue(PROP_QUOTES, quotesHead);
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
      aDeclaration->AppendValue(PROP_QUOTES_OPEN, open);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseSize(PRInt32& aErrorCode, nsICSSDeclaration* aDeclaration)
{
  nsCSSValue width;
  if (ParseVariant(aErrorCode, width, VARIANT_AHKL, nsCSSProps::kPageSizeKTable)) {
    if (width.IsLengthUnit()) {
      nsCSSValue  height;
      if (ParseVariant(aErrorCode, height, VARIANT_LENGTH, nsnull)) {
        if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
          aDeclaration->AppendValue(PROP_SIZE_WIDTH, width);
          aDeclaration->AppendValue(PROP_SIZE_HEIGHT, height);
          return PR_TRUE;
        }
        return PR_FALSE;
      }
    }
    if (ExpectEndProperty(aErrorCode, PR_TRUE)) {
      aDeclaration->AppendValue(PROP_SIZE_WIDTH, width);
      aDeclaration->AppendValue(PROP_SIZE_HEIGHT, width);
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
                                      nsICSSDeclaration* aDeclaration)
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
            aErrorCode = aDeclaration->AppendStructValue(PROP_TEXT_SHADOW, shadowHead);
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
      aDeclaration->AppendValue(PROP_TEXT_SHADOW_X, value);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

