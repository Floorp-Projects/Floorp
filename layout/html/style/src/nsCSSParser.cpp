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

// XXX TODO:
// - rework aErrorCode stuff: switch over to nsresult
// - bug: "color: red {fish};" emits a property for color instead of zapping it

static NS_DEFINE_IID(kICSSParserIID, NS_ICSS_PARSER_IID);
static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);

// XXX cell-padding, spacing, etc.???

struct Selector {
  nsString mTag;     // weight 1
  nsString mID;      // weight 100
  nsString mClass;   // weight 10
  nsString mPseudoClass;  // weight 10 (== class)
  nsString mPseudoElement;  // weight 10 (== class) ??
  PRUint32 mMask;    // which fields have values

  Selector();
  ~Selector();

  PRInt32 Weight() const;
#ifdef NS_DEBUG
  void Dump() const;
#endif
};

#define SELECTOR_TAG            0x01
#define SELECTOR_ID             0x02
#define SELECTOR_CLASS          0x04
#define SELECTOR_PSEUDO_CLASS   0x08
#define SELECTOR_PSEUDO_ELEMENT 0x10

#define SELECTOR_WEIGHT_BASE 10

Selector::Selector()
{
  mMask = 0;
}

Selector::~Selector()
{
}

PRInt32 Selector::Weight() const
{
  return (((0 != (SELECTOR_TAG & mMask)) ? 1 : 0) + 
          ((0 != (SELECTOR_ID & mMask)) ? (SELECTOR_WEIGHT_BASE * SELECTOR_WEIGHT_BASE) : 0) + 
          ((0 != ((SELECTOR_CLASS | SELECTOR_PSEUDO_CLASS | SELECTOR_PSEUDO_ELEMENT) & mMask)) ? SELECTOR_WEIGHT_BASE : 0));
}

#ifdef NS_DEBUG
void Selector::Dump() const
{
  PRBool needSpace = PR_FALSE;
  if (mTag.Length() > 0) {
    fputs(mTag, stdout);
    needSpace = PR_TRUE;
  }
  if (mID.Length() > 0) {
    if (needSpace) fputs(" ", stdout);
    fputs("#", stdout);
    fputs(mID, stdout);
    needSpace = PR_TRUE;
  }
  if (mClass.Length() > 0) {
    if (needSpace) fputs(" ", stdout);
    fputs(".", stdout);
    fputs(mClass, stdout);
    needSpace = PR_TRUE;
  }
  if (mPseudoClass.Length() > 0) {
    if (needSpace) fputs(" ", stdout);
    fputs(":", stdout);
    fputs(mPseudoClass, stdout);
    needSpace = PR_TRUE;
  }
  if (mPseudoElement.Length() > 0) {
    if (needSpace) fputs(" ", stdout);
    fputs(":", stdout);
    fputs(mPseudoElement, stdout);
    needSpace = PR_TRUE;
  }
}
#endif

// e.g. "P B, H1 B { ... }" has a selector list with two elements,
// each of which has two selectors.
struct SelectorList {
  SelectorList* mNext;
  nsVoidArray mSelectors;

  SelectorList();

  void Destroy();

  void AddSelector(Selector* aSelector) {
    mSelectors.AppendElement(aSelector);
  }

#ifdef NS_DEBUG
  void Dump();
#endif

private:
  ~SelectorList();
};

SelectorList::SelectorList()
{
  mNext = nsnull;
}

SelectorList::~SelectorList()
{
  PRInt32 n = mSelectors.Count();
  for (PRInt32 i = 0; i < n; i++) {
    Selector* sel = (Selector*) mSelectors.ElementAt(i);
    delete sel;
  }
}

void SelectorList::Destroy()
{
  SelectorList* list = this;
  while (nsnull != list) {
    SelectorList* next = list->mNext;
    delete list;
    list = next;
  }
}

#ifdef NS_DEBUG
void SelectorList::Dump()
{
  PRInt32 n = mSelectors.Count();
  for (PRInt32 i = 0; i < n; i++) {
    Selector* sel = (Selector*) mSelectors.ElementAt(i);
    sel->Dump();
    fputs(" ", stdout);
  }
  if (mNext) {
    fputs(", ", stdout);
    mNext->Dump();
  }
}
#endif

//----------------------------------------------------------------------

// Your basic top-down recursive descent style parser
class CSSParserImpl : public nsICSSParser {
public:
  CSSParserImpl();
  CSSParserImpl(nsICSSStyleSheet* aSheet);
  ~CSSParserImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetInfoMask(PRUint32& aResult);

  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aSheet);

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURL*                aInputURL,
                   nsIStyleSheet*&        aResult);

  NS_IMETHOD ParseDeclarations(const nsString& aDeclaration,
                               nsIURL*         aBaseURL,
                               nsIStyleRule*&  aResult);

protected:
  PRBool GetToken(PRInt32* aErrorCode, PRBool aSkipWS);
  void UngetToken();

  PRBool ExpectSymbol(PRInt32* aErrorCode, char aSymbol, PRBool aSkipWS);
  nsString* NextIdent(PRInt32* aErrorCode);
  void SkipUntil(PRInt32* aErrorCode, PRUnichar aStopSymbol);
  void SkipRuleSet(PRInt32* aErrorCode);
  PRBool SkipDeclaration(PRInt32* aErrorCode, PRBool aCheckForBraces);

  PRBool ParseRuleSet(PRInt32* aErrorCode);
  PRBool ParseAtRule(PRInt32* aErrorCode);
  PRBool ParseImportRule(PRInt32* aErrorCode);

  PRBool ParseSelectorGroup(PRInt32* aErrorCode, SelectorList* aListHead);
  PRBool ParseSelectorList(PRInt32* aErrorCode, SelectorList* aListHead);
  PRBool ParseSelector(PRInt32* aErrorCode, Selector* aSelectorResult);
  nsICSSDeclaration* ParseDeclarationBlock(PRInt32* aErrorCode,
                                           PRBool aCheckForBraces);
  PRBool ParseDeclaration(PRInt32* aErrorCode,
                          nsICSSDeclaration* aDeclaration,
                          PRBool aCheckForBraces);
  PRBool ParseProperty(PRInt32* aErrorCode, const char* aName,
                       nsICSSDeclaration* aDeclaration);
  PRBool ParseProperty(PRInt32* aErrorCode, const char* aName,
                       nsICSSDeclaration* aDeclaration, PRInt32 aID);

  // Property specific parsing routines
  PRBool ParseBackground(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBackgroundFilter(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                               const char* aName);
  PRBool ParseForegroundFilter(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                               const char* aName);
  PRBool ParseBackgroundPosition(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                 const char* aName);
  PRBool ParseBorder(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderColor(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderSide(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                         const char* aNames[], PRInt32 aWhichSide);
  PRBool ParseBorderStyle(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseBorderWidth(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseClip(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseFont(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                   const char* aName);
  PRBool ParseFontFamily(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                         const char* aName);
  PRBool ParseFontWeight(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                         const char* aName);
  PRBool ParseListStyle(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseMargin(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParsePadding(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration);
  PRBool ParseTextDecoration(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                             const char* aName);

  // Reused utility parsing routines
  PRBool ParseBoxProperties(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                            const char* aNames[]);
  PRInt32 ParseChoice(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                      const char* aNames[], PRInt32 aNumNames);
  PRBool ParseColor(PRInt32* aErrorCode, nscolor* aColorResult);
  PRBool ParseColorComponent(PRInt32* aErrorCode, PRUint8* aComponent,
                             char aStop);
  PRBool ParseEnum(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                   const char* aName, PRInt32* aTable);
  PRInt32 SearchKeywordTable(PRInt32 aID, PRInt32 aTable[]);
  PRBool ParseVariant(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                      const char* aName, PRInt32 aVariant,
                      PRInt32* aTable);
  PRBool TranslateLength(nsICSSDeclaration* aDeclaration, const char* aName,
                         float aNumber, const nsString& aDimension);

  void ProcessImport(const nsString& aURLSpec);

  // Current token. The value is valid after calling GetToken
  nsCSSToken mToken;

  // After an UngetToken is done this flag is true. The next call to
  // GetToken clears the flag.
  PRBool mHavePushBack;

  nsCSSScanner* mScanner;
  nsIURL* mURL;
  nsICSSStyleSheet* mSheet;

  PRBool mInHead;
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
  mScanner = nsnull;
  mSheet = nsnull;
  mHavePushBack = PR_FALSE;
}

CSSParserImpl::CSSParserImpl(nsICSSStyleSheet* aSheet)
{
  NS_INIT_REFCNT();
  mScanner = nsnull;
  mSheet = aSheet; aSheet->AddRef();
  mHavePushBack = PR_FALSE;
}

NS_IMPL_ISUPPORTS(CSSParserImpl,kICSSParserIID)

CSSParserImpl::~CSSParserImpl()
{
  NS_IF_RELEASE(mSheet);
}

NS_METHOD
CSSParserImpl::GetInfoMask(PRUint32& aResult)
{
  aResult = NS_CSS_GETINFO_CSS1 | NS_CSS_GETINFO_CSSP;
  return NS_OK;
}

NS_METHOD
CSSParserImpl::SetStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null ptr");
  if (nsnull == aSheet) {
    return NS_ERROR_NULL_POINTER;
  }

  // Make sure the sheet supports the correct interface!
  static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
  nsICSSStyleSheet* cssSheet;
  nsresult rv = aSheet->QueryInterface(kICSSStyleSheetIID, (void**)&cssSheet);
  if (NS_OK != rv) {
    return rv;
  }

  // Switch to using the new sheet
  NS_IF_RELEASE(mSheet);
  mSheet = cssSheet;

  return NS_OK;
}

NS_METHOD
CSSParserImpl::Parse(nsIUnicharInputStream* aInput,
                     nsIURL*                aInputURL,
                     nsIStyleSheet*&        aResult)
{
  if (nsnull == mSheet) {
    NS_NewCSSStyleSheet(&mSheet, aInputURL);
  }

  PRInt32 errorCode = NS_OK;
  mScanner = new nsCSSScanner();
  mScanner->Init(aInput);
  mURL = aInputURL;
  if (nsnull != aInputURL) {
    aInputURL->AddRef();
  }
  mInHead = PR_TRUE;
  nsCSSToken* tk = &mToken;
  for (;;) {
    // Get next non-whitespace token
    if (!GetToken(&errorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_AtKeyword == tk->mType) {
      ParseAtRule(&errorCode);
      continue;
    } else if (eCSSToken_Symbol == tk->mType) {
      // Discard dangling semicolons. This is not part of the CSS1
      // forward compatible parsing spec, but it makes alot of sense.
      if (';' == tk->mSymbol) {
        continue;
      }
    }
    mInHead = PR_FALSE;
    UngetToken();
    ParseRuleSet(&errorCode);
  }
  delete mScanner;
  mScanner = nsnull;
  NS_IF_RELEASE(mURL);

  nsIStyleSheet* sheet = nsnull;
  mSheet->QueryInterface(kIStyleSheetIID, (void**)&sheet);
  aResult = sheet;

  return NS_OK;
}

NS_METHOD
CSSParserImpl::ParseDeclarations(const nsString& aDeclaration,
                                 nsIURL*         aBaseURL,
                                 nsIStyleRule*&  aResult)
{
  nsString* str = new nsString(aDeclaration);
  if (nsnull == str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIUnicharInputStream* input = nsnull;
  nsresult rv = NS_NewStringUnicharInputStream(&input, str);
  if (NS_OK != rv) {
    return rv;
  }

  mScanner = new nsCSSScanner();
  mScanner->Init(input);
  NS_RELEASE(input);

  mURL = aBaseURL;
  NS_IF_ADDREF(mURL);

  mInHead = PR_FALSE;
  PRInt32 errorCode = NS_OK;

  nsICSSDeclaration* declaration = ParseDeclarationBlock(&errorCode, PR_FALSE);
  if (nsnull != declaration) {
    // Create a style rule for the delcaration
    nsICSSStyleRule* rule = nsnull;
    NS_NewCSSStyleRule(&rule, nsCSSSelector());
    rule->SetDeclaration(declaration);
    rule->SetWeight(0x7fffffff);
    aResult = rule;
  }
  else {
    aResult = nsnull;
  }

  delete mScanner;
  mScanner = nsnull;
  NS_IF_RELEASE(mURL);

  return NS_OK;
}

//----------------------------------------------------------------------

PRBool CSSParserImpl::GetToken(PRInt32* aErrorCode, PRBool aSkipWS)
{
  for (;;) {
    if (!mHavePushBack) {
      if (!mScanner->Next(aErrorCode, &mToken)) {
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

PRBool CSSParserImpl::ExpectSymbol(PRInt32* aErrorCode,
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

nsString* CSSParserImpl::NextIdent(PRInt32* aErrorCode)
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
PRBool CSSParserImpl::ParseAtRule(PRInt32* aErrorCode)
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

// Parse a CSS1 import rule: "@import STRING | URL"
PRBool CSSParserImpl::ParseImportRule(PRInt32* aErrorCode)
{
  nsCSSToken* tk = &mToken;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if ((eCSSToken_String == tk->mType) || (eCSSToken_URL == tk->mType)) {
    if (ExpectSymbol(aErrorCode, ';', PR_TRUE)) {
      ProcessImport(tk->mIdent);
      return PR_TRUE;
    }
    if ((eCSSToken_Symbol != tk->mType) || (';' != tk->mSymbol)) {
      SkipUntil(aErrorCode, ';');
    }
  }
  mInHead = PR_FALSE;
  return PR_TRUE;
}

void CSSParserImpl::ProcessImport(const nsString& aURLSpec)
{
  // XXX probably need a way to encode unicode junk for the part of
  // the url that follows a "?"
  char* cp = aURLSpec.ToNewCString();
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, mURL, cp);
  delete cp;
  if (NS_OK != rv) {
    // import url is bad
    // XXX log this somewhere for easier web page debugging
    return;
  }

  if (PR_FALSE == mSheet->ContainsStyleSheet(url)) { // don't allow circular references

    PRInt32 ec;
    nsIInputStream* in = url->Open(&ec);
    if (nsnull == in) {
      // failure to make connection
      // XXX log this somewhere for easier web page debugging
    }
    else {

      nsIUnicharInputStream* uin;
      rv = NS_NewConverterStream(&uin, nsnull, in);
      if (NS_OK != rv) {
        // XXX no iso-latin-1 converter? out of memory?
        NS_RELEASE(in);
      }
      else {

        NS_RELEASE(in);

        // Create a new parse to parse the import. 

        if (NS_OK == rv) {
          CSSParserImpl *parser = new CSSParserImpl();
          nsIStyleSheet* childSheet;
          rv = parser->Parse(uin, url, childSheet);
          NS_RELEASE(parser);
          if ((NS_OK == rv) && (nsnull != childSheet)) {
            nsICSSStyleSheet* cssChild = nsnull;
            if (NS_OK == childSheet->QueryInterface(kICSSStyleSheetIID, (void**)&cssChild)) {
              mSheet->AppendStyleSheet(cssChild);
              NS_RELEASE(cssChild);
            }
          }
          NS_RELEASE(childSheet);
        }
        NS_RELEASE(uin);
      }
    }
  }
  NS_RELEASE(url);
}

void CSSParserImpl::SkipUntil(PRInt32* aErrorCode, PRUnichar aStopSymbol)
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
CSSParserImpl::SkipDeclaration(PRInt32* aErrorCode, PRBool aCheckForBraces)
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

void CSSParserImpl::SkipRuleSet(PRInt32* aErrorCode)
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

PRBool CSSParserImpl::ParseRuleSet(PRInt32* aErrorCode)
{
  nsCSSToken* tk = &mToken;

  // First get the list of selectors for the rule
  SelectorList *slist = new SelectorList();
  if (!ParseSelectorList(aErrorCode, slist)) {
    SkipRuleSet(aErrorCode);
    slist->Destroy();
    return PR_FALSE;
  }

  // Next parse the declaration block
  nsICSSDeclaration* declaration = ParseDeclarationBlock(aErrorCode, PR_TRUE);
  if (nsnull == declaration) {
    // XXX skip something here
    slist->Destroy();
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
  nsCSSSelector selector;
  nsICSSStyleRule* rule = nsnull;

  while (nsnull != list) {
    PRInt32 selIndex = list->mSelectors.Count();

    Selector* sel = (Selector*)list->mSelectors[--selIndex];
    if (0 < sel->mPseudoElement.Length()) { // only pseudo elements at end selector
      nsString  nullStr;
      selector.Set(sel->mPseudoElement, nullStr, nullStr, nullStr);
      NS_NewCSSStyleRule(&rule, selector);
    }
    selector.Set(sel->mTag, sel->mID, sel->mClass, sel->mPseudoClass);
    PRInt32 weight = sel->Weight();

    if (nsnull == rule) {
      NS_NewCSSStyleRule(&rule, selector);
    }
    else {
      rule->AddSelector(selector);
    }
    if (nsnull != rule) {
      while (--selIndex >= 0) {
        Selector* sel = (Selector*)list->mSelectors[selIndex];
        selector.Set(sel->mTag, sel->mID, sel->mClass, sel->mPseudoClass);

        rule->AddSelector(selector);
        weight += sel->Weight();
      }
      rule->SetDeclaration(declaration);
      rule->SetWeight(weight);
//      rule->List();
      mSheet->AppendStyleRule(rule);
      NS_RELEASE(rule);
    }

    list = list->mNext;
  }

  // Release temporary storage
  slist->Destroy();
  NS_RELEASE(declaration);
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseSelectorList(PRInt32* aErrorCode,
                                        SelectorList* aListHead)
{
  if (!ParseSelectorGroup(aErrorCode, aListHead)) {
    // must have at least one selector group
    return PR_FALSE;
  }

  // After that there must either be a "," or a "{"
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    if (eCSSToken_Symbol != tk->mType) {
      UngetToken();
      return PR_FALSE;
    }
    if (',' == tk->mSymbol) {
      // Another selector group must follow
      SelectorList* newList = new SelectorList();
      if (!ParseSelectorGroup(aErrorCode, newList)) {
        newList->Destroy();
        return PR_FALSE;
      }
      // add new list to the end of the selector list
      aListHead->mNext = newList;
      aListHead = newList;
      continue;
    } else if ('{' == tk->mSymbol) {
      UngetToken();
      break;
    } else {
      UngetToken();
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseSelectorGroup(PRInt32* aErrorCode,
                                         SelectorList* aList)
{
  for (;;) {
    Selector* sel = new Selector();
    if (!ParseSelector(aErrorCode, sel)) {
      delete sel;
      break;
    }
    aList->AddSelector(sel);
  }
  return (PRBool) (aList->mSelectors.Count() > 0);
}

static PRBool IsPseudoClass(const nsString& aBuffer)
{
  return (aBuffer.EqualsIgnoreCase("link") ||
          aBuffer.EqualsIgnoreCase("visited") ||
          aBuffer.EqualsIgnoreCase("hover") ||
          aBuffer.EqualsIgnoreCase("out-of-date") ||  // our extension
          aBuffer.EqualsIgnoreCase("active"));
}

/**
 * These are the 15 possible kinds of CSS1 style selectors:
 * <UL>
 * <LI>Tag
 * <LI>Tag#Id
 * <LI>Tag#Id.Class
 * <LI>Tag#Id.Class:Pseudo
 * <LI>Tag#Id:Pseudo
 * <LI>Tag.Class
 * <LI>Tag.Class:Pseudo
 * <LI>Tag:Pseudo
 * <LI>#Id
 * <LI>#Id.Class
 * <LI>#Id.Class:Pseudo
 * <LI>#Id:Pseudo
 * <LI>.Class
 * <LI>.Class:Pseudo
 * <LI>:Pseudo
 * </UL>
 */
PRBool CSSParserImpl::ParseSelector(PRInt32* aErrorCode,
                                    Selector* aSelectorResult)
{
  PRUint32 mask = 0;
  aSelectorResult->mTag.Truncate(0);
  aSelectorResult->mClass.Truncate(0);
  aSelectorResult->mID.Truncate(0);
  aSelectorResult->mPseudoClass.Truncate(0);
  aSelectorResult->mPseudoElement.Truncate(0);

  nsCSSToken* tk = &mToken;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_Ident == tk->mType) {
    // tag
    mask |= SELECTOR_TAG;
    aSelectorResult->mTag.Append(tk->mIdent);
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  if (eCSSToken_ID == tk->mType) {
    // #id
    mask |= SELECTOR_ID;
    aSelectorResult->mID.Append(tk->mIdent);
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  if ((eCSSToken_Symbol == tk->mType) && ('.' == tk->mSymbol)) {
    // .class
    if (!GetToken(aErrorCode, PR_FALSE)) {
      return PR_FALSE;
    }
    if (eCSSToken_Ident != tk->mType) {
      // malformed selector
      UngetToken();
      return PR_FALSE;
    }
    mask |= SELECTOR_CLASS;
    aSelectorResult->mClass.Append(tk->mIdent);
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  if ((eCSSToken_Symbol == tk->mType) && (':' == tk->mSymbol)) {
    // :pseudo
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof
      return PR_FALSE;
    }
    if (eCSSToken_Ident != tk->mType) {
      // malformed selector
      UngetToken();
      return PR_FALSE;
    }
    if (IsPseudoClass(tk->mIdent)) {
      mask |= SELECTOR_PSEUDO_CLASS;
      aSelectorResult->mPseudoClass.Append(tk->mIdent);
    }
    else {
      mask |= SELECTOR_PSEUDO_ELEMENT;
      aSelectorResult->mPseudoElement.Append(':');  // keep the colon
      aSelectorResult->mPseudoElement.Append(tk->mIdent);
    }
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof is ok (here!)
      return PR_TRUE;
    }
  }
  if ((eCSSToken_Symbol == tk->mType) && (':' == tk->mSymbol)) {
    // :pseudo
    if (!GetToken(aErrorCode, PR_FALSE)) {
      // premature eof
      return PR_FALSE;
    }
    if (eCSSToken_Ident != tk->mType) {
      // malformed selector
      UngetToken();
      return PR_FALSE;
    }
    if (! IsPseudoClass(tk->mIdent)) {
      mask |= SELECTOR_PSEUDO_ELEMENT;
      aSelectorResult->mPseudoElement.Truncate(0);
      aSelectorResult->mPseudoElement.Append(':');  // keep the colon
      aSelectorResult->mPseudoElement.Append(tk->mIdent);
    }
    tk = nsnull;
  }
  if (nsnull != tk) {
    UngetToken();
  }
  if (mask == 0) {
    return PR_FALSE;
  }
  aSelectorResult->mMask = mask;
  return PR_TRUE;
}

nsICSSDeclaration*
CSSParserImpl::ParseDeclarationBlock(PRInt32* aErrorCode,
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
      if (ParseDeclaration(aErrorCode, declaration, aCheckForBraces)) {
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

PRBool CSSParserImpl::ParseColor(PRInt32* aErrorCode, nscolor* aColorResult)
{
  char cbuf[50];

  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }

  nsCSSToken* tk = &mToken;
  switch (tk->mType) {
  case eCSSToken_ID:
    // #xxyyzz
    tk->mIdent.ToCString(cbuf, sizeof(cbuf));
    if (NS_HexToRGB(cbuf, aColorResult)) {
      return PR_TRUE;
    }
    break;

  case eCSSToken_Ident:
    if (!tk->mIdent.EqualsIgnoreCase("rgb")) {
      // named color (maybe!)
      tk->mIdent.ToCString(cbuf, sizeof(cbuf));
      if (NS_ColorNameToRGB(cbuf, aColorResult)) {
        return PR_TRUE;
      }
    } else {
      // rgb ( component , component , component )
      PRUint8 r, g, b;
      if (ExpectSymbol(aErrorCode, '(', PR_TRUE) &&
          ParseColorComponent(aErrorCode, &r, ',') &&
          ParseColorComponent(aErrorCode, &g, ',') &&
          ParseColorComponent(aErrorCode, &b, ')')) {
        *aColorResult = NS_RGB(r,g,b);
        return PR_TRUE;
      }
    }
    break;

  default:
    break;
  }

  // It's not a color
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseColorComponent(PRInt32* aErrorCode,
                                          PRUint8* aComponent,
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
    *aComponent = (PRUint8) value;
    return PR_TRUE;
  }
  return PR_FALSE;
}

//----------------------------------------------------------------------

PRBool
CSSParserImpl::ParseDeclaration(PRInt32* aErrorCode,
                                nsICSSDeclaration* aDeclaration,
                                PRBool aCheckForBraces)
{
  // Get property name
  nsCSSToken* tk = &mToken;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      return PR_FALSE;
    }
    if (eCSSToken_Ident == tk->mType) {
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
  char propertyName[100];
  tk->mIdent.ToCString(propertyName, sizeof(propertyName));
  if (!ParseProperty(aErrorCode, propertyName, aDeclaration)) {
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
  // XXX do something with isImportant flag

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
#define VARIANT_KEYWORD         0x0001
#define VARIANT_LENGTH          0x0002
#define VARIANT_PERCENT         0x0004
#define VARIANT_COLOR           0x0008
#define VARIANT_URL             0x0010
#define VARIANT_NUMBER          0x0020
#define VARIANT_INTEGER         0x0040
#define VARIANT_AUTO            0x0100
#define VARIANT_INHERIT         0x0200
#define VARIANT_NONE            0x0400
#define VARIANT_NORMAL          0x0800

// Common combinations of variants
#define VARIANT_KL   (VARIANT_KEYWORD | VARIANT_LENGTH)
#define VARIANT_KLP  (VARIANT_KEYWORD | VARIANT_LENGTH | VARIANT_PERCENT)
#define VARIANT_KP   (VARIANT_KEYWORD | VARIANT_PERCENT)
#define VARIANT_AH   (VARIANT_AUTO | VARIANT_INHERIT)
#define VARIANT_AHLP (VARIANT_AH | VARIANT_LP)
#define VARIANT_AHI  (VARIANT_AH | VARIANT_INTEGER)
#define VARIANT_AHK  (VARIANT_AH | VARIANT_KEYWORD)
#define VARIANT_AHL  (VARIANT_AH | VARIANT_LENGTH)
#define VARIANT_HK   (VARIANT_INHERIT | VARIANT_KEYWORD)
#define VARIANT_HL   (VARIANT_INHERIT | VARIANT_LENGTH)
#define VARIANT_HLP  (VARIANT_HL | VARIANT_PERCENT)
#define VARIANT_HLPN (VARIANT_HLP | VARIANT_NUMBER)
#define VARIANT_HMK  (VARIANT_INHERIT | VARIANT_NORMAL | VARIANT_KEYWORD)
#define VARIANT_AL   (VARIANT_AUTO | VARIANT_LENGTH)
#define VARIANT_LP   (VARIANT_LENGTH | VARIANT_PERCENT)
#define VARIANT_CK   (VARIANT_COLOR | VARIANT_KEYWORD)
#define VARIANT_C    VARIANT_COLOR
#define VARIANT_UK   (VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_HPN  (VARIANT_INHERIT | VARIANT_PERCENT | VARIANT_NUMBER)

// Keyword id tables for variant/enum parsing
static PRInt32 kBackgroundAttachmentKTable[] = {
  KEYWORD_FIXED, NS_STYLE_BG_ATTACHMENT_FIXED,
  KEYWORD_SCROLL, NS_STYLE_BG_ATTACHMENT_SCROLL,
  -1
};

static PRInt32 kBackgroundColorKTable[] = {
  KEYWORD_TRANSPARENT, NS_STYLE_BG_COLOR_TRANSPARENT,
  -1
};

static PRInt32 kBackgroundRepeatKTable[] = {
  KEYWORD_NO_REPEAT, NS_STYLE_BG_REPEAT_OFF,
  KEYWORD_REPEAT, NS_STYLE_BG_REPEAT_XY,
  KEYWORD_REPEAT_X, NS_STYLE_BG_REPEAT_X,
  KEYWORD_REPEAT_Y, NS_STYLE_BG_REPEAT_Y,
  -1
};

static PRInt32 kBorderStyleKTable[] = {
  KEYWORD_NONE,   NS_STYLE_BORDER_STYLE_NONE,
  KEYWORD_DOTTED, NS_STYLE_BORDER_STYLE_DOTTED,
  KEYWORD_DASHED, NS_STYLE_BORDER_STYLE_DASHED,
  KEYWORD_SOLID,  NS_STYLE_BORDER_STYLE_SOLID,
  KEYWORD_DOUBLE, NS_STYLE_BORDER_STYLE_DOUBLE,
  KEYWORD_GROOVE, NS_STYLE_BORDER_STYLE_GROOVE,
  KEYWORD_RIDGE,  NS_STYLE_BORDER_STYLE_RIDGE,
  KEYWORD_INSET,  NS_STYLE_BORDER_STYLE_INSET,
  KEYWORD_OUTSET, NS_STYLE_BORDER_STYLE_OUTSET,
  -1
};

static PRInt32 kBorderWidthKTable[] = {
  KEYWORD_THIN, NS_STYLE_BORDER_WIDTH_THIN,
  KEYWORD_MEDIUM, NS_STYLE_BORDER_WIDTH_MEDIUM,
  KEYWORD_THICK, NS_STYLE_BORDER_WIDTH_THICK,
  -1
};

static PRInt32 kClearKTable[] = {
  KEYWORD_NONE, NS_STYLE_CLEAR_NONE,
  KEYWORD_LEFT, NS_STYLE_CLEAR_LEFT,
  KEYWORD_RIGHT, NS_STYLE_CLEAR_RIGHT,
  KEYWORD_BOTH, NS_STYLE_CLEAR_LEFT_AND_RIGHT,
  -1
};

static PRInt32 kCursorKTable[] = {
  KEYWORD_IBEAM, NS_STYLE_CURSOR_IBEAM,
  KEYWORD_ARROW, NS_STYLE_CURSOR_DEFAULT,
  KEYWORD_HAND, NS_STYLE_CURSOR_HAND,
  -1
};

static PRInt32 kDirectionKTable[] = {
  KEYWORD_LTR,      NS_STYLE_DIRECTION_LTR,
  KEYWORD_RTL,      NS_STYLE_DIRECTION_RTL,
  KEYWORD_INHERIT,  NS_STYLE_DIRECTION_INHERIT,
  -1
};

static PRInt32 kDisplayKTable[] = {
  KEYWORD_NONE,               NS_STYLE_DISPLAY_NONE,
  KEYWORD_BLOCK,              NS_STYLE_DISPLAY_BLOCK,
  KEYWORD_INLINE,             NS_STYLE_DISPLAY_INLINE,
  KEYWORD_LIST_ITEM,          NS_STYLE_DISPLAY_LIST_ITEM,
  KEYWORD_MARKER,             NS_STYLE_DISPLAY_MARKER,
  KEYWORD_RUN_IN,             NS_STYLE_DISPLAY_RUN_IN,
  KEYWORD_COMPACT,            NS_STYLE_DISPLAY_COMPACT,
  KEYWORD_TABLE,              NS_STYLE_DISPLAY_TABLE,
  KEYWORD_INLINE_TABLE,       NS_STYLE_DISPLAY_INLINE_TABLE,
  KEYWORD_TABLE_ROW_GROUP,    NS_STYLE_DISPLAY_TABLE_ROW_GROUP,
  KEYWORD_TABLE_COLUMN,       NS_STYLE_DISPLAY_TABLE_COLUMN,
  KEYWORD_TABLE_COLUMN_GROUP, NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP,
  KEYWORD_TABLE_HEADER_GROUP, NS_STYLE_DISPLAY_TABLE_HEADER_GROUP,
  KEYWORD_TABLE_FOOTER_GROUP, NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP,
  KEYWORD_TABLE_ROW,          NS_STYLE_DISPLAY_TABLE_ROW,
  KEYWORD_TABLE_CELL,         NS_STYLE_DISPLAY_TABLE_CELL,
  KEYWORD_TABLE_CAPTION,      NS_STYLE_DISPLAY_TABLE_CAPTION,
  -1
};

static PRInt32 kFloatKTable[] = {
  KEYWORD_NONE,  NS_STYLE_FLOAT_NONE,
  KEYWORD_LEFT,  NS_STYLE_FLOAT_LEFT,
  KEYWORD_RIGHT, NS_STYLE_FLOAT_RIGHT,
  -1
};

static PRInt32 kFontSizeKTable[] = {
  KEYWORD_XX_SMALL, NS_STYLE_FONT_SIZE_XXSMALL,
  KEYWORD_X_SMALL, NS_STYLE_FONT_SIZE_XSMALL,
  KEYWORD_SMALL, NS_STYLE_FONT_SIZE_SMALL,
  KEYWORD_MEDIUM, NS_STYLE_FONT_SIZE_MEDIUM,
  KEYWORD_LARGE, NS_STYLE_FONT_SIZE_LARGE,
  KEYWORD_X_LARGE, NS_STYLE_FONT_SIZE_XLARGE,
  KEYWORD_XX_LARGE, NS_STYLE_FONT_SIZE_XXLARGE,
  KEYWORD_LARGER, NS_STYLE_FONT_SIZE_LARGER,
  KEYWORD_SMALLER, NS_STYLE_FONT_SIZE_SMALLER,
  -1
};

static PRInt32 kFontStyleKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_FONT_STYLE_NORMAL,
  KEYWORD_ITALIC, NS_STYLE_FONT_STYLE_ITALIC,
  KEYWORD_OBLIQUE, NS_STYLE_FONT_STYLE_OBLIQUE,
  -1
};

static PRInt32 kFontVariantKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_FONT_VARIANT_NORMAL,
  KEYWORD_SMALL_CAPS, NS_STYLE_FONT_VARIANT_SMALL_CAPS,
  -1
};

static PRInt32 kListStyleImageKTable[] = {
  KEYWORD_NONE, NS_STYLE_LIST_STYLE_IMAGE_NONE,
  -1
};

static PRInt32 kListStylePositionKTable[] = {
  KEYWORD_INSIDE, NS_STYLE_LIST_STYLE_POSITION_INSIDE,
  KEYWORD_OUTSIDE, NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
  -1
};

static PRInt32 kListStyleKTable[] = {
  KEYWORD_NONE, NS_STYLE_LIST_STYLE_NONE,
  KEYWORD_DISC, NS_STYLE_LIST_STYLE_DISC,
  KEYWORD_CIRCLE, NS_STYLE_LIST_STYLE_CIRCLE,
  KEYWORD_SQUARE, NS_STYLE_LIST_STYLE_SQUARE,
  KEYWORD_DECIMAL, NS_STYLE_LIST_STYLE_DECIMAL,
  KEYWORD_LOWER_ROMAN, NS_STYLE_LIST_STYLE_LOWER_ROMAN,
  KEYWORD_UPPER_ROMAN, NS_STYLE_LIST_STYLE_UPPER_ROMAN,
  KEYWORD_LOWER_ALPHA, NS_STYLE_LIST_STYLE_LOWER_ALPHA,
  KEYWORD_UPPER_ALPHA, NS_STYLE_LIST_STYLE_UPPER_ALPHA,
  -1
};

static PRInt32 kMarginSizeKTable[] = {
  KEYWORD_AUTO, NS_STYLE_MARGIN_SIZE_AUTO,
  -1
};

static PRInt32 kOverflowKTable[] = {
  KEYWORD_VISIBLE, NS_STYLE_OVERFLOW_VISIBLE,
  KEYWORD_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN,
  KEYWORD_SCROLL, NS_STYLE_OVERFLOW_SCROLL,
  KEYWORD_AUTO, NS_STYLE_OVERFLOW_AUTO,
  -1
};

static PRInt32 kPositionKTable[] = {
  KEYWORD_RELATIVE, NS_STYLE_POSITION_RELATIVE,
  KEYWORD_ABSOLUTE, NS_STYLE_POSITION_ABSOLUTE,
  KEYWORD_FIXED, NS_STYLE_POSITION_FIXED,
  -1
};

static PRInt32 kTextAlignKTable[] = {
  KEYWORD_LEFT, NS_STYLE_TEXT_ALIGN_LEFT,
  KEYWORD_RIGHT, NS_STYLE_TEXT_ALIGN_RIGHT,
  KEYWORD_CENTER, NS_STYLE_TEXT_ALIGN_CENTER,
  KEYWORD_JUSTIFY, NS_STYLE_TEXT_ALIGN_JUSTIFY,
  -1
};

static PRInt32 kTextTransformKTable[] = {
  KEYWORD_NONE, NS_STYLE_TEXT_TRANSFORM_NONE,
  KEYWORD_CAPITALIZE, NS_STYLE_TEXT_TRANSFORM_CAPITALIZE,
  KEYWORD_LOWERCASE, NS_STYLE_TEXT_TRANSFORM_LOWERCASE,
  KEYWORD_UPPERCASE, NS_STYLE_TEXT_TRANSFORM_UPPERCASE,
  -1
};

static PRInt32 kVerticalAlignKTable[] = {
  KEYWORD_BASELINE, NS_STYLE_VERTICAL_ALIGN_BASELINE,
  KEYWORD_SUB, NS_STYLE_VERTICAL_ALIGN_SUB,
  KEYWORD_SUPER, NS_STYLE_VERTICAL_ALIGN_SUPER,
  KEYWORD_TOP, NS_STYLE_VERTICAL_ALIGN_TOP,
  KEYWORD_TEXT_TOP, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP,
  KEYWORD_MIDDLE, NS_STYLE_VERTICAL_ALIGN_MIDDLE,
  KEYWORD_BOTTOM, NS_STYLE_VERTICAL_ALIGN_BOTTOM,
  KEYWORD_TEXT_BOTTOM, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM,
  -1
};

static PRInt32 kVisibilityKTable[] = {
  KEYWORD_VISIBLE, NS_STYLE_VISIBILITY_VISIBLE,
  KEYWORD_HIDDEN, NS_STYLE_VISIBILITY_HIDDEN,
  -1
};

static PRInt32 kWhitespaceKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_WHITESPACE_NORMAL,
  KEYWORD_PRE, NS_STYLE_WHITESPACE_PRE,
  KEYWORD_NOWRAP, NS_STYLE_WHITESPACE_NOWRAP,
  -1
};

static const char* kBorderTopNames[] = {
  "border-top-width",
  "border-top-style",
  "border-top-color",
};
static const char* kBorderRightNames[] = {
  "border-right-width",
  "border-right-style",
  "border-right-color",
};
static const char* kBorderBottomNames[] = {
  "border-bottom-width",
  "border-bottom-style",
  "border-bottom-color",
};
static const char* kBorderLeftNames[] = {
  "border-left-width",
  "border-left-style",
  "border-left-color",
};

PRInt32 CSSParserImpl::SearchKeywordTable(PRInt32 aID, PRInt32 aTable[])
{
  PRInt32 i = 0;
  for (;;) {
    if (aTable[i] < 0) {
      break;
    }
    if (aID == aTable[i]) {
      return i;
    }
    i += 2;
  }
  return -1;
}

PRBool CSSParserImpl::ParseEnum(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                const char* aName, PRInt32* aTable)
{
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  char cbuf[50];
  ident->ToCString(cbuf, sizeof(cbuf));
  PRInt32 id = nsCSSKeywords::LookupName(cbuf);
  if (id >= 0) {
    PRInt32 ix = SearchKeywordTable(id, aTable);
    if (ix >= 0) {
      aDeclaration->AddValue(aName, nsCSSValue(aTable[ix+1], eCSSUnit_Enumerated));
      return PR_TRUE;
    }
  }

  // Put the unknown identifier back and return
  UngetToken();
  return PR_FALSE;
}

PRBool CSSParserImpl::TranslateLength(nsICSSDeclaration* aDeclaration,
                                      const char* aName,
                                      float aNumber,
                                      const nsString& aDimension)
{
  nsCSSUnit units;
  if (0 != aDimension.Length()) {
    char cbuf[50];
    aDimension.ToCString(cbuf, sizeof(cbuf));
    PRInt32 id = nsCSSKeywords::LookupName(cbuf);
    switch (id) {
    case KEYWORD_EM:  units = eCSSUnit_EM; break;
    case KEYWORD_EX:  units = eCSSUnit_XHeight; break;
    case KEYWORD_PX:  units = eCSSUnit_Pixel; break;
    case KEYWORD_IN:  units = eCSSUnit_Inch; break;
    case KEYWORD_CM:  units = eCSSUnit_Centimeter; break;
    case KEYWORD_MM:  units = eCSSUnit_Millimeter; break;
    case KEYWORD_PT:  units = eCSSUnit_Point; break;
    case KEYWORD_PC:  units = eCSSUnit_Pica; break;
    default:
      // unknown dimension
      return PR_FALSE;
    }
  } else {
    // Must be a zero number...
    units = eCSSUnit_Point;
  }
  aDeclaration->AddValue(aName, nsCSSValue(aNumber, units));
  return PR_TRUE;
}


PRBool CSSParserImpl::ParseVariant(PRInt32* aErrorCode,
                                   nsICSSDeclaration* aDeclaration,
                                   const char* aName,
                                   PRInt32 aVariants, PRInt32* aTable)
{
  nsCSSToken* tk = &mToken;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if (((aVariants & (VARIANT_AHK | VARIANT_NORMAL | VARIANT_NONE)) != 0) &&
      (eCSSToken_Ident == tk->mType)) {
    char cbuf[50];
    tk->mIdent.ToCString(cbuf, sizeof(cbuf));
    PRInt32 sid = nsCSSKeywords::LookupName(cbuf);
    if (sid >= 0) { // known keyword
      if ((aVariants & VARIANT_AUTO) != 0) {
        if (sid == KEYWORD_AUTO) {
          aDeclaration->AddValue(aName, nsCSSValue(eCSSUnit_Auto));
          return PR_TRUE;
        }
      }
      if ((aVariants & VARIANT_INHERIT) != 0) {
        if (sid == KEYWORD_INHERIT) {
          aDeclaration->AddValue(aName, nsCSSValue(eCSSUnit_Inherit));
          return PR_TRUE;
        }
      }
      if ((aVariants & VARIANT_NONE) != 0) {
        if (sid == KEYWORD_NONE) {
          aDeclaration->AddValue(aName, nsCSSValue(eCSSUnit_None));
          return PR_TRUE;
        }
      }
      if ((aVariants & VARIANT_NORMAL) != 0) {
        if (sid == KEYWORD_NORMAL) {
          aDeclaration->AddValue(aName, nsCSSValue(eCSSUnit_Normal));
          return PR_TRUE;
        }
      }
      if ((aVariants & VARIANT_KEYWORD) != 0) {
        PRInt32 ix = SearchKeywordTable(sid, aTable);
        if (ix >= 0) {
          aDeclaration->AddValue(aName, nsCSSValue(aTable[ix+1], eCSSUnit_Enumerated));
          return PR_TRUE;
        }
      }
    }
  }
  if (((aVariants & VARIANT_LENGTH) != 0) && tk->isDimension()) {
    return TranslateLength(aDeclaration, aName, tk->mNumber, tk->mIdent);
  }
  if (((aVariants & VARIANT_PERCENT) != 0) &&
      (eCSSToken_Percentage == tk->mType)) {
    aDeclaration->AddValue(aName, nsCSSValue(tk->mNumber, eCSSUnit_Percent));
    return PR_TRUE;
  }
  if (((aVariants & VARIANT_NUMBER) != 0) &&
      (eCSSToken_Number == tk->mType)) {
    aDeclaration->AddValue(aName, nsCSSValue(tk->mNumber, eCSSUnit_Number));
    return PR_TRUE;
  }
  if (((aVariants & VARIANT_INTEGER) != 0) &&
      (eCSSToken_Number == tk->mType) && tk->mIntegerValid) {
    aDeclaration->AddValue(aName, nsCSSValue(tk->mInteger, eCSSUnit_Integer));
    return PR_TRUE;
  }
  if (((aVariants & VARIANT_URL) != 0) &&
      (eCSSToken_URL == tk->mType)) {
    // Translate url into an absolute url if the url is relative to
    // the style sheet.
    // XXX editors won't like this - too bad for now
    nsAutoString absURL;
    nsString baseURL;
    nsresult rv = NS_MakeAbsoluteURL(mURL, baseURL, tk->mIdent, absURL);
    if (NS_OK == rv) {
      aDeclaration->AddValue(aName, nsCSSValue(absURL));
    }
    else {
      aDeclaration->AddValue(aName, nsCSSValue(tk->mIdent));
    }
    return PR_TRUE;
  }
  if ((aVariants & VARIANT_COLOR) != 0) {
    if ((eCSSToken_ID == tk->mType) || (eCSSToken_Ident == tk->mType)) {
      // Put token back so that parse color can get it
      UngetToken();
      nscolor rgba;
      // XXX This loses the original input format (e.g. a name which
      // should be preserved when editing)
      if (ParseColor(aErrorCode, &rgba)) {
        aDeclaration->AddValue(aName, nsCSSValue(rgba));
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  }
  UngetToken();
  return PR_FALSE;
}

PRInt32 CSSParserImpl::ParseChoice(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                   const char* aNames[], PRInt32 aNumNames)
{
  PRInt32 found = 0;
  for (int i = 0; i < aNumNames; i++) {
    // Try each property parser in order
    for (int j = 0; j < aNumNames; j++) {
      PRInt32 bit = 1 << j;
      if ((found & bit) == 0) {
        PRInt32 id = nsCSSProps::LookupName(aNames[j]);
        if (ParseProperty(aErrorCode, aNames[j], aDeclaration, id)) {
          found |= bit;
        }
      }
    }
  }
  return found;
}

/**
 * Parse a "box" property. Box properties have 1 to 4 values. When less
 * than 4 values are provided a standard mapping is used to replicate
 * existing values. Note that the replication requires renaming the
 * values.
 */
PRBool CSSParserImpl::ParseBoxProperties(PRInt32* aErrorCode,
                                         nsICSSDeclaration* aDeclaration,
                                         const char* aNames[])
{
  // Get up to four values for the property
  PRInt32 count = 0;
  for (int i = 0; i < 4; i++) {
    PRInt32 id = nsCSSProps::LookupName(aNames[i]);
    if (!ParseProperty(aErrorCode, aNames[i], aDeclaration, id)) {
      break;
    }
    count++;
  }
  if (count == 0) {
    return PR_FALSE;
  }

  // Provide missing values by replicating some of the values found
  nsCSSValue value;
  switch (count) {
    case 1: // Make right == top
      aDeclaration->GetValue(aNames[0], value);
      aDeclaration->AddValue(aNames[1], value);
    case 2: // Make bottom == top
      aDeclaration->GetValue(aNames[0], value);
      aDeclaration->AddValue(aNames[2], value);
    case 3: // Make left == right
      aDeclaration->GetValue(aNames[1], value);
      aDeclaration->AddValue(aNames[3], value);
  }

  return PR_TRUE;
}

PRBool CSSParserImpl::ParseProperty(PRInt32* aErrorCode,
                                    const char* aName,
                                    nsICSSDeclaration* aDeclaration)
{
  PRInt32 id = nsCSSProps::LookupName(aName);
  if (id < 0) {
    return PR_FALSE;
  }

  // Strip out properties we use internally. These properties are used
  // by compound property parsing routines (e.g. "background-position").
  switch (id) {
  case PROP_BACKGROUND_X_POSITION:
  case PROP_BACKGROUND_Y_POSITION:
  case PROP_BORDER_TOP_COLOR:
  case PROP_BORDER_RIGHT_COLOR:
  case PROP_BORDER_BOTTOM_COLOR:
  case PROP_BORDER_LEFT_COLOR:
  case PROP_BORDER_TOP_STYLE:
  case PROP_BORDER_RIGHT_STYLE:
  case PROP_BORDER_BOTTOM_STYLE:
  case PROP_BORDER_LEFT_STYLE:
  case PROP_CLIP_BOTTOM:
  case PROP_CLIP_LEFT:
  case PROP_CLIP_RIGHT:
  case PROP_CLIP_TOP:
    // The user can't use these
    return PR_FALSE;
  }

  return ParseProperty(aErrorCode, aName, aDeclaration, id);
}

PRBool CSSParserImpl::ParseProperty(PRInt32* aErrorCode,
                                    const char* aName,
                                    nsICSSDeclaration* aDeclaration,
                                    PRInt32 aID)
{
  switch (aID) {
  case PROP_BACKGROUND:
    return ParseBackground(aErrorCode, aDeclaration);
  case PROP_BACKGROUND_ATTACHMENT:
    return ParseEnum(aErrorCode, aDeclaration, aName, kBackgroundAttachmentKTable);
  case PROP_BACKGROUND_COLOR:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_CK,
                        kBackgroundColorKTable);
  case PROP_BACKGROUND_FILTER:
    return ParseBackgroundFilter(aErrorCode, aDeclaration, aName);
  case PROP_BACKGROUND_IMAGE:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_URL | VARIANT_NONE,
                        nsnull);
  case PROP_BACKGROUND_POSITION:
    return ParseBackgroundPosition(aErrorCode, aDeclaration, aName);
  case PROP_BACKGROUND_REPEAT:
    return ParseEnum(aErrorCode, aDeclaration, aName, kBackgroundRepeatKTable);
  case PROP_BORDER:
    return ParseBorder(aErrorCode, aDeclaration);
  case PROP_BORDER_COLOR:
    return ParseBorderColor(aErrorCode, aDeclaration);
  case PROP_BORDER_STYLE:
    return ParseBorderStyle(aErrorCode, aDeclaration);
  case PROP_BORDER_BOTTOM:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderBottomNames, 0);
  case PROP_BORDER_LEFT:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderLeftNames, 1);
  case PROP_BORDER_RIGHT:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderRightNames, 2);
  case PROP_BORDER_TOP:
    return ParseBorderSide(aErrorCode, aDeclaration, kBorderTopNames, 3);
  case PROP_BORDER_BOTTOM_COLOR:
  case PROP_BORDER_LEFT_COLOR:
  case PROP_BORDER_RIGHT_COLOR:
  case PROP_BORDER_TOP_COLOR:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_C, nsnull);
  case PROP_BORDER_BOTTOM_STYLE:
  case PROP_BORDER_LEFT_STYLE:
  case PROP_BORDER_RIGHT_STYLE:
  case PROP_BORDER_TOP_STYLE:
    return ParseEnum(aErrorCode, aDeclaration, aName, kBorderStyleKTable);
  case PROP_BORDER_BOTTOM_WIDTH:
  case PROP_BORDER_LEFT_WIDTH:
  case PROP_BORDER_RIGHT_WIDTH:
  case PROP_BORDER_TOP_WIDTH:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_KL,
                        kBorderWidthKTable);
  case PROP_BORDER_WIDTH:
    return ParseBorderWidth(aErrorCode, aDeclaration);
  case PROP_CLEAR:
    return ParseEnum(aErrorCode, aDeclaration, aName, kClearKTable);
  case PROP_CLIP:
    return ParseClip(aErrorCode, aDeclaration);
  case PROP_COLOR:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_COLOR, nsnull);
  case PROP_CURSOR:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHK, kCursorKTable);
  case PROP_DIRECTION:
    return ParseEnum(aErrorCode, aDeclaration, aName, kDirectionKTable);
  case PROP_DISPLAY:
    return ParseEnum(aErrorCode, aDeclaration, aName, kDisplayKTable);
  case PROP_FILTER:
    return ParseForegroundFilter(aErrorCode, aDeclaration, aName);
  case PROP_FLOAT:
    return ParseEnum(aErrorCode, aDeclaration, aName, kFloatKTable);
  case PROP_FONT:
    return ParseFont(aErrorCode, aDeclaration, aName);
  case PROP_FONT_FAMILY:
    return ParseFontFamily(aErrorCode, aDeclaration, aName);
  case PROP_FONT_SIZE:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_KLP,
                        kFontSizeKTable);
  case PROP_FONT_STYLE:
    return ParseEnum(aErrorCode, aDeclaration, aName, kFontStyleKTable);
  case PROP_FONT_VARIANT:
    return ParseEnum(aErrorCode, aDeclaration, aName, kFontVariantKTable);
  case PROP_FONT_WEIGHT:
    return ParseFontWeight(aErrorCode, aDeclaration, aName);
  case PROP_HEIGHT:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHLP, nsnull);
  case PROP_LEFT:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHLP, nsnull);
  case PROP_LINE_HEIGHT:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_HLPN | VARIANT_NORMAL,
                        nsnull);
  case PROP_LIST_STYLE:
    return ParseListStyle(aErrorCode, aDeclaration);
  case PROP_LIST_STYLE_IMAGE:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_UK,
                        kListStyleImageKTable);
  case PROP_LIST_STYLE_POSITION:
    return ParseEnum(aErrorCode, aDeclaration, aName, kListStylePositionKTable);
  case PROP_LIST_STYLE_TYPE:
    return ParseEnum(aErrorCode, aDeclaration, aName, kListStyleKTable);
  case PROP_MARGIN:
    return ParseMargin(aErrorCode, aDeclaration);
  case PROP_MARGIN_BOTTOM:
  case PROP_MARGIN_LEFT:
  case PROP_MARGIN_RIGHT:
  case PROP_MARGIN_TOP:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_KLP,
                        kMarginSizeKTable);
  case PROP_PADDING:
    return ParsePadding(aErrorCode, aDeclaration);
  case PROP_PADDING_BOTTOM:
  case PROP_PADDING_LEFT:
  case PROP_PADDING_RIGHT:
  case PROP_PADDING_TOP:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_LP, nsnull);
  case PROP_OPACITY:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_HPN, nsnull);
  case PROP_OVERFLOW:
    return ParseEnum(aErrorCode, aDeclaration, aName, kOverflowKTable);
  case PROP_POSITION:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_HMK, kPositionKTable);
  case PROP_TEXT_ALIGN:
    return ParseEnum(aErrorCode, aDeclaration, aName, kTextAlignKTable);
  case PROP_TEXT_DECORATION:
    return ParseTextDecoration(aErrorCode, aDeclaration, aName);
  case PROP_TEXT_INDENT:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_LP, nsnull);
  case PROP_TEXT_TRANSFORM:
    return ParseEnum(aErrorCode, aDeclaration, aName, kTextTransformKTable);
  case PROP_TOP:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHLP, nsnull);
  case PROP_VERTICAL_ALIGN:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_KP,
                        kVerticalAlignKTable);
  case PROP_VISIBILITY:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_HK, kVisibilityKTable);
  case PROP_WHITE_SPACE:
    return ParseEnum(aErrorCode, aDeclaration, aName, kWhitespaceKTable);
  case PROP_WIDTH:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHLP, nsnull);
  case PROP_LETTER_SPACING:
  case PROP_WORD_SPACING:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_HL | VARIANT_NORMAL, nsnull);
  case PROP_Z_INDEX:
    return ParseVariant(aErrorCode, aDeclaration, aName, VARIANT_AHI, nsnull);
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseBackground(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBackgroundNames[] = {
    "background-color",
    "background-image",
    "background-repeat",
    "background-attachment",
    "background-position",
    "background-filter",
  };

  PRInt32 found = ParseChoice(aErrorCode, aDeclaration, kBackgroundNames, 6);
  if (0 == found) {
    return PR_FALSE;
  }

  // Provide missing values
  if ((found & 1) == 0) {
    aDeclaration->AddValue(kBackgroundNames[0], 
                           nsCSSValue(NS_STYLE_BG_COLOR_TRANSPARENT,
                                      eCSSUnit_Enumerated));
  }
  if ((found & 2) == 0) {
    aDeclaration->AddValue(kBackgroundNames[1], 
                           nsCSSValue(NS_STYLE_BG_IMAGE_NONE,
                                      eCSSUnit_Enumerated));
  }
  if ((found & 4) == 0) {
    aDeclaration->AddValue(kBackgroundNames[2], 
                           nsCSSValue(NS_STYLE_BG_REPEAT_XY,
                                      eCSSUnit_Enumerated));
  }
  if ((found & 8) == 0) {
    aDeclaration->AddValue(kBackgroundNames[3], 
                           nsCSSValue(NS_STYLE_BG_ATTACHMENT_SCROLL,
                                      eCSSUnit_Enumerated));
  }
  if ((found & 16) == 0) {
    aDeclaration->AddValue("background-x-position", nsCSSValue(0.0f, eCSSUnit_Percent));
    aDeclaration->AddValue("background-y-position", nsCSSValue(0.0f, eCSSUnit_Percent));
  }

  // XXX Note: no default for filter (yet)
  return PR_TRUE;
}

// Bits used in determining which background position info we have
#define BG_CENTER  0
#define BG_TOP     1
#define BG_BOTTOM  2
#define BG_LEFT    4
#define BG_RIGHT   8
#define BG_CENTER1 16
#define BG_CENTER2 32

PRBool CSSParserImpl::ParseBackgroundPosition(PRInt32* aErrorCode,
                                              nsICSSDeclaration* aDeclaration,
                                              const char* aName)
{
  static PRInt32 kBackgroundPositionNames[] = {
    PROP_BACKGROUND_X_POSITION,
    PROP_BACKGROUND_Y_POSITION,
  };

  // Note: Don't change this table unless you update
  // parseBackgroundPosition!

  static PRInt32 kBackgroundXYPositionKTable[] = {
    KEYWORD_CENTER, BG_CENTER,
    KEYWORD_TOP, BG_TOP,
    KEYWORD_BOTTOM, BG_BOTTOM,
    KEYWORD_LEFT, BG_LEFT,
    KEYWORD_RIGHT, BG_RIGHT,
    -1,
  };

  const char* bxp = "background-x-position";
  const char* byp = "background-y-position";

  // First try a number or a length value
  if (ParseVariant(aErrorCode, aDeclaration, bxp, VARIANT_LP, nsnull)) {
    // We have one number/length. Get the optional second number/length.
    if (ParseVariant(aErrorCode, aDeclaration, byp, VARIANT_LP, nsnull)) {
      // We have two numbers
      return PR_TRUE;
    }

    // We have one number which is the x position. Create an value for
    // the vertical position which is of value 50%
    aDeclaration->AddValue(byp, nsCSSValue(0.5f, eCSSUnit_Percent));
    // XXX shouldn't this be CENTER enum instead?
    return PR_TRUE;
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
    nsString* ident = NextIdent(aErrorCode);
    if (nsnull == ident) {
      break;
    }
    char cbuf[50];
    ident->ToCString(cbuf, sizeof(cbuf));
    PRInt32 ix = SearchKeywordTable(nsCSSKeywords::LookupName(cbuf),
                                       kBackgroundXYPositionKTable);
    if (ix >= 0) {
      PRInt32 bit = kBackgroundXYPositionKTable[ix + 1];
      if (bit == 0) {
        // Special hack for center bits: We can have two of them
        mask |= centerBit;
        centerBit <<= 1;
        continue;
      } else if ((mask & bit) != 0) {
        // no duplicate values allowed (other than center)
        return PR_FALSE;
      }
      mask |= bit;
    } else {
      UngetToken();
      break;
    }
  }

  // Check for bad input. Bad input consists of no matching keywords,
  // or pairs of x keywords or pairs of y keywords.
  if ((mask == 0) || (mask == (BG_TOP | BG_BOTTOM)) ||
      (mask == (BG_LEFT | BG_RIGHT))) {
    return PR_FALSE;
  }

  // Map good input
  int xValue = 50;
  if ((mask & (BG_LEFT | BG_RIGHT)) != 0) {
    // We have an x value
    xValue = ((mask & BG_LEFT) != 0) ? 0 : 100;
  }
  int yValue = 50;
  if ((mask & (BG_TOP | BG_BOTTOM)) != 0) {
    // We have a y value
    yValue = ((mask & BG_TOP) != 0) ? 0 : 100;
  }

  // Create style values
  aDeclaration->AddValue(bxp, nsCSSValue(xValue, eCSSUnit_Enumerated));
  aDeclaration->AddValue(byp, nsCSSValue(yValue, eCSSUnit_Enumerated));
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBackgroundFilter(PRInt32* aErrorCode,
                                            nsICSSDeclaration* aDeclaration,
                                            const char* aName)
{
  // XXX not yet supported
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseForegroundFilter(PRInt32* aErrorCode,
                                            nsICSSDeclaration* aDeclaration,
                                            const char* aName)
{
  // XXX not yet supported
  return PR_FALSE;
}

// These must be in alphabetical order for aWhich based indexing to work
static const char* kBorderStyleNames[] = {
  "border-bottom-style",
  "border-left-style",
  "border-right-style",
  "border-top-style",
};
static const char* kBorderWidthNames[] = {
  "border-bottom-width",
  "border-left-width",
  "border-right-width",
  "border-top-width",
};


PRBool CSSParserImpl::ParseBorder(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBorderNames[] = {
    "border-width",
    "border-style",
    "border-color"
  };
  PRInt32 found = ParseChoice(aErrorCode, aDeclaration, kBorderNames, 3);
  if (0 == found) {
    return PR_FALSE;
  }
  if (0 == (found & 1)) {
    // provide missing border width's
    for (int i = 0; i < 4; i++) {
      aDeclaration->AddValue(kBorderWidthNames[i], 
                             nsCSSValue(NS_STYLE_BORDER_WIDTH_MEDIUM,
                                        eCSSUnit_Enumerated));
    }
  }
  if (0 == (found & 2)) {
    // provide missing border style's
    for (int i = 0; i < 4; i++) {
      aDeclaration->AddValue(kBorderStyleNames[i], 
                             nsCSSValue(NS_STYLE_BORDER_STYLE_NONE,
                                        eCSSUnit_Enumerated));
    }
  }

  // Do NOT provide a missing color value as the default is to be
  // the color of the element itself which must be determined later.
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderColor(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBoxBorderColorNames[] = {
    "border-top-color",
    "border-right-color",
    "border-bottom-color",
    "border-left-color",
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kBoxBorderColorNames);
}

PRBool CSSParserImpl::ParseBorderSide(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                      const char* aNames[], PRInt32 aWhich)
{
  PRInt32 found = ParseChoice(aErrorCode, aDeclaration, aNames, 3);
  if (found == 0) return PR_FALSE;
  if ((found & 1) == 0) {
    // Provide default border-width
    aDeclaration->AddValue(kBorderWidthNames[aWhich],
                           nsCSSValue(NS_STYLE_BORDER_WIDTH_MEDIUM, eCSSUnit_Enumerated));
  }
  if ((found & 2) == 0) {
    // Provide default border-style
    aDeclaration->AddValue(kBorderStyleNames[aWhich],
                           nsCSSValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated));
  }

  // Do NOT provide a missing color value as the default is to be
  // the color of the element itself which must be determined later.
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseBorderStyle(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  // in top, right, bottom, left order
  static const char* kBoxBorderStyleNames[] = {
    "border-top-style",
    "border-right-style",
    "border-bottom-style",
    "border-left-style",
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kBoxBorderStyleNames);
}

PRBool CSSParserImpl::ParseBorderWidth(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBoxBorderWidthNames[] = {
    "border-top-width",
    "border-right-width",
    "border-bottom-width",
    "border-left-width",
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kBoxBorderWidthNames);
}

PRBool CSSParserImpl::ParseClip(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kClipNames[] = {
    "clip-top",
    "clip-right",
    "clip-bottom",
    "clip-left",
  };
  nsString* ident = NextIdent(aErrorCode);
  if (nsnull == ident) {
    return PR_FALSE;
  }
  if (ident->EqualsIgnoreCase("auto")) {
    for (int i = 0; i < 4; i++) {
      aDeclaration->AddValue(kClipNames[i], nsCSSValue(eCSSUnit_Auto));
    }
    return PR_TRUE;
  } else if (ident->EqualsIgnoreCase("inherit")) {
    for (int i = 0; i < 4; i++) {
      aDeclaration->AddValue(kClipNames[i], nsCSSValue(eCSSUnit_Inherit));
    }
    return PR_TRUE;
  } else if (ident->EqualsIgnoreCase("rect")) {
    if (!ExpectSymbol(aErrorCode, '(', PR_TRUE)) {
      return PR_FALSE;
    }
    for (int i = 0; i < 4; i++) {
      if (!ParseVariant(aErrorCode, aDeclaration, kClipNames[i], VARIANT_AL, nsnull)) {
        return PR_FALSE;
      }
      if (3 != i) {
        // skip optional commas between elements
        ExpectSymbol(aErrorCode, ',', PR_TRUE);
      }
    }
    if (!ExpectSymbol(aErrorCode, ')', PR_TRUE)) {
      return PR_FALSE;
    }
    return PR_TRUE;
  } else {
    UngetToken();
  }
  return PR_FALSE;
}

PRBool CSSParserImpl::ParseFont(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                const char* aName)
{
  static const char* fontNames[] = {
    "font-style",
    "font-variant",
    "font-weight",
  };

  // Get optional font-style, font-variant and font-weight (in any order)
  PRInt32 found = ParseChoice(aErrorCode, aDeclaration, fontNames, 3);
  if ((found & 1) == 0) {
    // Provide default font-style
    aDeclaration->AddValue(fontNames[0],
                           nsCSSValue(NS_STYLE_FONT_STYLE_NORMAL, eCSSUnit_Enumerated));
  }
  if ((found & 2) == 0) {
    // Provide default font-variant
    aDeclaration->AddValue(fontNames[1],
                           nsCSSValue(NS_STYLE_FONT_VARIANT_NORMAL, eCSSUnit_Enumerated));
  }
  if ((found & 4) == 0) {
    // Provide default font-weight
    aDeclaration->AddValue(fontNames[2],
                           nsCSSValue(NS_STYLE_FONT_WEIGHT_NORMAL, eCSSUnit_Enumerated));
  }

  // Get mandatory font-size
  if (!ParseVariant(aErrorCode, aDeclaration, "font-size",
                    VARIANT_KLP, kFontSizeKTable)) {
    return PR_FALSE;
  }

  // Get optional "/" line-height
  if (ExpectSymbol(aErrorCode, '/', PR_TRUE)) {
    if (!ParseVariant(aErrorCode, aDeclaration, "line-height",
                      VARIANT_HLPN | VARIANT_NORMAL, nsnull)) {
      return PR_FALSE;
    }
  }

  // Get final mandatory font-family
  return ParseFontFamily(aErrorCode, aDeclaration, "font-family");
}

PRBool CSSParserImpl::ParseFontFamily(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                      const char* aName)
{
  nsCSSToken* tk = &mToken;
  nsAutoString family;
  PRBool firstOne = PR_TRUE;
  for (;;) {
    if (!GetToken(aErrorCode, PR_TRUE)) {
      break;
    }
    if (eCSSToken_Ident == tk->mType) {
      if (!firstOne) {
        family.Append(PRUnichar(','));
      }
      family.Append(PRUnichar('"'));
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
      family.Append(PRUnichar('"'));
      firstOne = PR_FALSE;
    } else if (eCSSToken_String == tk->mType) {
      if (!firstOne) {
        family.Append(PRUnichar(','));
      }
      family.Append(PRUnichar('"'));
      family.Append(tk->mIdent);
      family.Append(PRUnichar('"'));
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
  aDeclaration->AddValue(aName, nsCSSValue(family));
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseFontWeight(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration,
                                      const char* aName)
{
  static PRInt32 kFontWeightKTable[] = {
    KEYWORD_NORMAL, NS_STYLE_FONT_WEIGHT_NORMAL,
    KEYWORD_BOLD, NS_STYLE_FONT_WEIGHT_BOLD,
    KEYWORD_BOLDER, NS_STYLE_FONT_WEIGHT_BOLDER,
    KEYWORD_LIGHTER, NS_STYLE_FONT_WEIGHT_LIGHTER,
    -1,
  };
  nsCSSToken* tk = &mToken;
  if (!GetToken(aErrorCode, PR_TRUE)) {
    return PR_FALSE;
  }
  if (eCSSToken_Ident == tk->mType) {
    char cbuf[50];
    tk->mIdent.ToCString(cbuf, sizeof(cbuf));
    PRInt32 kid = nsCSSKeywords::LookupName(cbuf);
    PRInt32 ix = SearchKeywordTable(kid, kFontWeightKTable);
    if (ix < 0) {
      UngetToken();
      return PR_FALSE;
    }
    aDeclaration->AddValue(aName, nsCSSValue(kFontWeightKTable[ix+1], eCSSUnit_Enumerated));
  } else if (eCSSToken_Number == tk->mType) {
    PRInt32 v = (PRInt32) tk->mNumber;
    if (v < 100) v = 100;
    else if (v > 900) v = 900;
    v = v - (v % 100);
    aDeclaration->AddValue(aName, nsCSSValue(v, eCSSUnit_Integer));
  } else {
    UngetToken();
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseListStyle(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  const char* lst = "list-style-type";
  const char* lsp = "list-style-position";
  const char* lsi = "list-style-image";
  int found = 0;
  for (int i = 0; i < 3; i++) {
    if (((found & 1) == 0) &&
        ParseEnum(aErrorCode, aDeclaration, lst, kListStyleKTable)) {
      found |= 1;
      continue;
    }
    if (((found & 2) == 0) &&
        ParseEnum(aErrorCode, aDeclaration, lsp, kListStylePositionKTable)) {
      found |= 2;
      continue;
    }
    if ((found & 4) == 0) {
      if (!GetToken(aErrorCode, PR_TRUE)) {
        break;
      }
      if (eCSSToken_URL == mToken.mType) {
        aDeclaration->AddValue(lsi, nsCSSValue(mToken.mIdent));
        found |= 4;
        continue;
      } else {
        if (eCSSToken_Symbol == mToken.mType) {
          PRUnichar symbol = mToken.mSymbol;
          if ((';' == symbol) || ('}' == symbol)) {
            UngetToken();
            break;
          }
        }

        // We got something strange. That's an error.
        UngetToken();
        return PR_FALSE;
      }
    }
  }
  if (found == 0) {
    return PR_FALSE;
  }

  // Provide default values
  if ((found & 1) == 0) {
    aDeclaration->AddValue(lst, nsCSSValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated));
  }
  if ((found & 2) == 0) {
    aDeclaration->AddValue(lsp, nsCSSValue(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE, eCSSUnit_Enumerated));
  }
  if ((found & 4) == 0) {
    aDeclaration->AddValue(lsi, nsCSSValue(NS_STYLE_LIST_STYLE_IMAGE_NONE, eCSSUnit_Enumerated));
  }
  return PR_TRUE;
}

PRBool CSSParserImpl::ParseMargin(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBoxMarginSideNames[] = {
    "margin-top",
    "margin-right",
    "margin-bottom",
    "margin-left",
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kBoxMarginSideNames);
}

PRBool CSSParserImpl::ParsePadding(PRInt32* aErrorCode, nsICSSDeclaration* aDeclaration)
{
  static const char* kBoxPaddingSideNames[] = {
    "padding-top",
    "padding-right",
    "padding-bottom",
    "padding-left",
  };
  return ParseBoxProperties(aErrorCode, aDeclaration, kBoxPaddingSideNames);
}

PRBool CSSParserImpl::ParseTextDecoration(PRInt32* aErrorCode,
                                          nsICSSDeclaration* aDeclaration,
                                          const char* aName)
{
  static PRInt32 kTextDecorationNoneKTable[] = {
    KEYWORD_NONE, NS_STYLE_TEXT_DECORATION_NONE,
    -1
  };
  static PRInt32 kTextDecorationKTable[] = {
    KEYWORD_UNDERLINE, NS_STYLE_TEXT_DECORATION_UNDERLINE,
    KEYWORD_OVERLINE, NS_STYLE_TEXT_DECORATION_OVERLINE,
    KEYWORD_LINE_THROUGH, NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
    KEYWORD_BLINK, NS_STYLE_TEXT_DECORATION_BLINK,
    -1,
  };

  if (ParseEnum(aErrorCode, aDeclaration, aName, kTextDecorationNoneKTable)) {
    return PR_TRUE;
  }

  PRInt32 decoration = 0;
  for (int i = 0; i < 4; i++) {
    nsString* ident = NextIdent(aErrorCode);
    if (nsnull == ident) {
      break;
    }
    char cbuf[50];
    ident->ToCString(cbuf, sizeof(cbuf));
    PRInt32 id = nsCSSKeywords::LookupName(cbuf);
    PRInt32 ix = SearchKeywordTable(id, kTextDecorationKTable);
    if (ix < 0) {
      UngetToken();
      break;
    }
    decoration |= kTextDecorationKTable[ix+1];
  }
  if (0 == decoration) {
    return PR_FALSE;
  }
  aDeclaration->AddValue(aName, nsCSSValue(decoration, eCSSUnit_Integer));
  return PR_TRUE;
}
