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

#include "nsIHTMLCSSStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsHTMLIIDs.h"
#include "nsICSSStyleRule.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"

static NS_DEFINE_IID(kIHTMLCSSStyleSheetIID, NS_IHTML_CSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);

class CSSFirstLineRule : public nsIStyleRule {
public:
  CSSFirstLineRule(nsIHTMLCSSStyleSheet* aSheet);
  virtual ~CSSFirstLineRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext,
                          nsIPresContext* aPresContext);
  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext,
                              nsIPresContext* aPresContext);
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsIHTMLCSSStyleSheet*  mSheet;
};

CSSFirstLineRule::CSSFirstLineRule(nsIHTMLCSSStyleSheet* aSheet)
  : mSheet(aSheet)
{
  NS_INIT_REFCNT();
}

CSSFirstLineRule::~CSSFirstLineRule()
{
}

NS_IMPL_ISUPPORTS(CSSFirstLineRule, kIStyleRuleIID);

NS_IMETHODIMP
CSSFirstLineRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)7;         // XXX got a better suggestion?
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
CSSFirstLineRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::MapFontStyleInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext)
{
  return NS_OK;
}

// Fixup the style context so that all of the properties that don't
// apply (CSS2 spec section 5.12.1) don't apply.
//
// The properties that apply: font-properties, color-properties,
// backgorund-properties, word-spacing, letter-spacing,
// text-decoration, vertical-align, text-transform, line-height,
// text-shadow, and clear.
//
// Everything else doesn't apply.
NS_IMETHODIMP
CSSFirstLineRule::MapStyleInto(nsIStyleContext* aContext,
                               nsIPresContext* aPresContext)
{
  nsIStyleContext* parentContext;
  parentContext = aContext->GetParent();

  // Disable border
  nsStyleSpacing* spacing = (nsStyleSpacing*)
    aContext->GetMutableStyleData(eStyleStruct_Spacing);
  if (spacing) {
    spacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
  }

  // Undo any change made to "direction"
  nsStyleDisplay* display = (nsStyleDisplay*)
    aContext->GetMutableStyleData(eStyleStruct_Display);
  if (parentContext) {
    const nsStyleDisplay* parentDisplay = (const nsStyleDisplay*)
      parentContext->GetStyleData(eStyleStruct_Display);
    if (parentDisplay) {
      display->mDirection = parentDisplay->mDirection;
    }
  }

  // Undo any change made to "cursor"
  nsStyleColor* color = (nsStyleColor*)
    aContext->GetMutableStyleData(eStyleStruct_Color);
  if (parentContext) {
    const nsStyleColor* parentColor = (const nsStyleColor*)
      parentContext->GetStyleData(eStyleStruct_Color);
    if (parentColor) {
      color->mCursor = parentColor->mCursor;
    }
  }

  // Undo any change to quotes
  nsStyleContent* content = (nsStyleContent*)
    aContext->GetMutableStyleData(eStyleStruct_Content);
  if (parentContext) {
    const nsStyleContent* parentContent = (const nsStyleContent*)
      parentContext->GetStyleData(eStyleStruct_Content);
    if (parentContent) {
      nsAutoString open, close;
      PRUint32 i, n = parentContent->QuotesCount();
      content->AllocateQuotes(n);
      for (i = 0; i < n; i++) {
        parentContent->GetQuotesAt(i, open, close);
        content->SetQuotesAt(i, open, close);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

// -----------------------------------------------------------

class CSSFirstLetterRule : public CSSFirstLineRule {
public:
  CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet);

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext,
                          nsIPresContext* aPresContext);
};

CSSFirstLetterRule::CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet)
  : CSSFirstLineRule(aSheet)
{
}

NS_IMETHODIMP
CSSFirstLetterRule::MapStyleInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext)
{
  // These properties apply: font-properties, color-properties,
  // backgorund-properties, word-spacing, letter-spacing,
  // text-decoration, vertical-align, text-transform, line-height,
  // text-shadow, and clear.
  //
  // Everything else doesn't apply.

  // Disable border
  nsStyleSpacing* spacing = (nsStyleSpacing*)
    aContext->GetMutableStyleData(eStyleStruct_Spacing);
  if (spacing) {
    spacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    spacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
  }
  return NS_OK;
}

// -----------------------------------------------------------

class HTMLCSSStyleSheetImpl : public nsIHTMLCSSStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLCSSStyleSheetImpl();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // basic style sheet data
  NS_IMETHOD Init(nsIURL* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURL* aURL);
  NS_IMETHOD GetURL(nsIURL*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;

  NS_IMETHOD GetEnabled(PRBool& aEnabled) const;
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // will be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIStyleContext* aParentContext,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aParentContent,
                                nsIAtom* aPseudoTag,
                                nsIStyleContext* aParentContext,
                                nsISupportsArray* aResults);

  NS_IMETHOD  HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIContent*     aContent);

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  HTMLCSSStyleSheetImpl(const HTMLCSSStyleSheetImpl& aCopy); 
  HTMLCSSStyleSheetImpl& operator=(const HTMLCSSStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLCSSStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURL*         mURL;
  nsIDocument*    mDocument;

  CSSFirstLineRule* mFirstLineRule;
};


void* HTMLCSSStyleSheetImpl::operator new(size_t size)
{
  HTMLCSSStyleSheetImpl* rv = (HTMLCSSStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* HTMLCSSStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  HTMLCSSStyleSheetImpl* rv = (HTMLCSSStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void HTMLCSSStyleSheetImpl::operator delete(void* ptr)
{
  HTMLCSSStyleSheetImpl* sheet = (HTMLCSSStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::operator delete(ptr);
    }
  }
}



HTMLCSSStyleSheetImpl::HTMLCSSStyleSheetImpl()
  : nsIHTMLCSSStyleSheet(),
    mURL(nsnull),
    mDocument(nsnull),
    mFirstLineRule(nsnull)
{
  NS_INIT_REFCNT();
}

HTMLCSSStyleSheetImpl::~HTMLCSSStyleSheetImpl()
{
  NS_RELEASE(mURL);
  if (nsnull != mFirstLineRule) {
    mFirstLineRule->mSheet = nsnull;
    NS_RELEASE(mFirstLineRule);
  }
}

NS_IMPL_ADDREF(HTMLCSSStyleSheetImpl)
NS_IMPL_RELEASE(HTMLCSSStyleSheetImpl)

nsresult HTMLCSSStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                               void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIHTMLCSSStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIHTMLCSSStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PRInt32 HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                             nsIContent* aContent,
                                             nsIStyleContext* aParentContext,
                                             nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIStyledContent* styledContent;

  // just get the one and only style rule from the content's STYLE attribute
  if (NS_SUCCEEDED(aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent))) {
    nsIStyleRule* rule = nsnull;
    if (NS_SUCCEEDED(styledContent->GetInlineStyleRule(rule))) {
      if (nsnull != rule) {
        aResults->AppendElement(rule);
        matchCount++;
        nsICSSStyleRule*  cssRule;
        if (NS_SUCCEEDED(rule->QueryInterface(kICSSStyleRuleIID, (void**)&cssRule))) {
          nsIStyleRule* important = cssRule->GetImportantRule();
          if (nsnull != important) {
            aResults->AppendElement(important);
            matchCount++;
            NS_RELEASE(important);
          }
          NS_RELEASE(cssRule);
        }
        NS_RELEASE(rule);
      }
    }

    NS_RELEASE(styledContent);
  }

  return matchCount;
}

PRInt32 HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                             nsIContent* aParentContent,
                                             nsIAtom* aPseudoTag,
                                             nsIStyleContext* aParentContext,
                                             nsISupportsArray* aResults)
{
  if (aPseudoTag == nsHTMLAtoms::firstLinePseudo) {
    if (aResults->Count()) { 
      if (nsnull == mFirstLineRule) {
        mFirstLineRule = new CSSFirstLineRule(this);
        if (mFirstLineRule) {
          NS_ADDREF(mFirstLineRule);
        }
      }
      if (mFirstLineRule) {
        aResults->AppendElement(mFirstLineRule); 
        return 1; 
      }
    } 
  }
  // else no pseudo frame style... 
  return 0;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::Init(nsIURL* aURL, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURL && aDocument, "null ptr");
  if (! aURL || ! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mURL || mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  mURL = aURL;
  NS_ADDREF(mURL);
  return NS_OK;
}

// Test if style is dependent on content state
NS_IMETHODIMP
HTMLCSSStyleSheetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                              nsIContent*     aContent)
{
  return NS_COMFALSE;
}



NS_IMETHODIMP 
HTMLCSSStyleSheetImpl::Reset(nsIURL* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);
  if (nsnull != mFirstLineRule) {
    mFirstLineRule->mSheet = nsnull;
    NS_RELEASE(mFirstLineRule);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetURL(nsIURL*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  aTitle.Append("Internal HTML/CSS Style Sheet");
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetType(nsString& aType) const
{
  aType.Truncate();
  aType.Append("text/html");
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetEnabled(PRBool& aEnabled) const
{
  aEnabled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

// style sheet owner info
NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  NS_IF_ADDREF(mDocument);
  aDocument = mDocument;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}

void HTMLCSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  PRUnichar* buffer;

  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML CSS Style Sheet: ", out);
  mURL->ToString(&buffer);
  nsAutoString as(buffer,0);
  fputs(as, out);
  fputs("\n", out);
  delete buffer;
}


// XXX For backwards compatibility and convenience
NS_HTML nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult, nsIURL* aURL,
                          nsIDocument* aDocument)
{
  nsresult rv;
  nsIHTMLCSSStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLCSSStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}

NS_HTML nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLCSSStyleSheetImpl*  it = new HTMLCSSStyleSheetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
}
