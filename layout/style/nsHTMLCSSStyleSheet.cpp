/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"

#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"


class CSSFirstLineRule : public nsIStyleRule {
public:
  CSSFirstLineRule(nsIHTMLCSSStyleSheet* aSheet);
  virtual ~CSSFirstLineRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;
  NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext,
                          nsIPresContext* aPresContext);
  NS_IMETHOD MapFontStyleInto(nsIMutableStyleContext* aContext,
                              nsIPresContext* aPresContext);
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);

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

NS_IMPL_ISUPPORTS(CSSFirstLineRule, NS_GET_IID(nsIStyleRule));

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
CSSFirstLineRule::MapFontStyleInto(nsIMutableStyleContext* aContext,
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
CSSFirstLineRule::MapStyleInto(nsIMutableStyleContext* aContext,
                               nsIPresContext* aPresContext)
{
  nsIStyleContext* parentContext;
  parentContext = aContext->GetParent();

  // Disable border
  nsStyleBorder* border = (nsStyleBorder*)
    aContext->GetMutableStyleData(eStyleStruct_Border);
  if (border) {
    border->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    border->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    border->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    border->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
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

  NS_RELEASE(parentContext);
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSFirstLineRule's size): 
*    1) sizeof(*this) 
*
*  Contained / Aggregated data (not reported as CSSFirstLineRule's size):
*    1) Delegate to mSheet if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSFirstLineRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    // object has already been accounted for
    return;
  }

  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSFirstLine-LetterRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);

  if(mSheet){
    PRUint32 localSize=0;
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}

// -----------------------------------------------------------

class CSSFirstLetterRule : public CSSFirstLineRule {
public:
  CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet);

  NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext,
                          nsIPresContext* aPresContext);
};

CSSFirstLetterRule::CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet)
  : CSSFirstLineRule(aSheet)
{
}

// Fixup the style context so that all of the properties that don't
// apply (CSS2 spec section 5.12.2) don't apply.
//
// These properties apply: font-properties, color-properties,
// backgorund-properties, text-decoration, vertical-align,
// text-transform, line-height, margin-properties,
// padding-properties, border-properties, float, text-shadow and
// clear.
//
// Everything else doesn't apply.
NS_IMETHODIMP
CSSFirstLetterRule::MapStyleInto(nsIMutableStyleContext* aContext,
                                 nsIPresContext* aPresContext)
{
  nsIStyleContext* parentContext;
  parentContext = aContext->GetParent();

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

  NS_RELEASE(parentContext);
  return NS_OK;
}

// -----------------------------------------------------------

class HTMLCSSStyleSheetImpl : public nsIHTMLCSSStyleSheet,
                              public nsIStyleRuleProcessor {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLCSSStyleSheetImpl();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // basic style sheet data
  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURI* aURL);
  NS_IMETHOD GetURL(nsIURI*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD UseForMedium(nsIAtom* aMedium) const;

  NS_IMETHOD GetEnabled(PRBool& aEnabled) const;
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // will be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument);

  NS_IMETHOD GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                   nsIStyleRuleProcessor* aPrevProcessor);

  // nsIStyleRuleProcessor api
  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aContent,
                           nsIStyleContext* aParentContext,
                           nsISupportsArray* aResults);

  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aParentContent,
                           nsIAtom* aPseudoTag,
                           nsIStyleContext* aParentContext,
                           nsISupportsArray* aResults);

  NS_IMETHOD HasStateDependentStyle(nsIPresContext* aPresContext,
                                    nsIAtom*        aMedium,
                                    nsIContent*     aContent);

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);

  // If changing the given attribute cannot affect style context, aAffects
  // will be PR_FALSE on return.
  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects);
private: 
  // These are not supported and are not implemented! 
  HTMLCSSStyleSheetImpl(const HTMLCSSStyleSheetImpl& aCopy); 
  HTMLCSSStyleSheetImpl& operator=(const HTMLCSSStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLCSSStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  NS_DECL_OWNINGTHREAD // for thread-safety checking

  nsIURI*         mURL;
  nsIDocument*    mDocument;

  CSSFirstLineRule* mFirstLineRule;
  CSSFirstLetterRule* mFirstLetterRule;
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
    mFirstLineRule(nsnull),
    mFirstLetterRule(nsnull)
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
  if (nsnull != mFirstLetterRule) {
    mFirstLetterRule->mSheet = nsnull;
    NS_RELEASE(mFirstLetterRule);
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
  if (aIID.Equals(NS_GET_IID(nsIHTMLCSSStyleSheet))) {
    *aInstancePtrResult = (void*) ((nsIHTMLCSSStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleSheet))) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleRuleProcessor))) {
    *aInstancePtrResult = (void*) ((nsIStyleRuleProcessor*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)(nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                             nsIStyleRuleProcessor* /*aPrevProcessor*/)
{
  aProcessor = this;
  NS_ADDREF(aProcessor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                     nsIAtom* aMedium,
                                     nsIContent* aContent,
                                     nsIStyleContext* aParentContext,
                                     nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  nsIStyledContent* styledContent;

  // just get the one and only style rule from the content's STYLE attribute
  if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIStyledContent), (void**)&styledContent))) {
    PRUint32 index = 0;
    aResults->Count(&index);
    if (NS_SUCCEEDED(styledContent->GetInlineStyleRules(aResults))) {
      PRUint32 postCount = 0;
      aResults->Count(&postCount);
      while (index < postCount) {
        nsIStyleRule* rule = (nsIStyleRule*)aResults->ElementAt(index++);
        nsICSSStyleRule*  cssRule;
        if (NS_SUCCEEDED(rule->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void**)&cssRule))) {
          nsIStyleRule* important = cssRule->GetImportantRule();
          if (nsnull != important) {
            aResults->AppendElement(important);
            NS_RELEASE(important);
          }
          NS_RELEASE(cssRule);
        }
        NS_RELEASE(rule);
      }
    }

    NS_RELEASE(styledContent);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                     nsIAtom* aMedium,
                                     nsIContent* aParentContent,
                                     nsIAtom* aPseudoTag,
                                     nsIStyleContext* aParentContext,
                                     nsISupportsArray* aResults)
{
  if (aPseudoTag == nsHTMLAtoms::firstLinePseudo) {
    PRUint32 cnt;
    nsresult rv = aResults->Count(&cnt);
    if (NS_SUCCEEDED(rv) && cnt) { 
      if (nsnull == mFirstLineRule) {
        mFirstLineRule = new CSSFirstLineRule(this);
        if (mFirstLineRule) {
          NS_ADDREF(mFirstLineRule);
        }
      }
      if (mFirstLineRule) {
        aResults->AppendElement(mFirstLineRule); 
        return NS_OK; 
      }
    } 
  }
  if (aPseudoTag == nsHTMLAtoms::firstLetterPseudo) {
    PRUint32 cnt;
    nsresult rv = aResults->Count(&cnt);
    if (NS_SUCCEEDED(rv) && cnt) { 
      if (nsnull == mFirstLetterRule) {
        mFirstLetterRule = new CSSFirstLetterRule(this);
        if (mFirstLetterRule) {
          NS_ADDREF(mFirstLetterRule);
        }
      }
      if (mFirstLetterRule) {
        aResults->AppendElement(mFirstLetterRule); 
        return NS_OK; 
      }
    } 
  }
  // else no pseudo frame style... 
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::Init(nsIURI* aURL, nsIDocument* aDocument)
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
                                              nsIAtom*        aMedium,
                                              nsIContent*     aContent)
{
  return NS_COMFALSE;
}



NS_IMETHODIMP 
HTMLCSSStyleSheetImpl::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);
  if (nsnull != mFirstLineRule) {
    mFirstLineRule->mSheet = nsnull;
    NS_RELEASE(mFirstLineRule);
  }
  if (nsnull != mFirstLetterRule) {
    mFirstLetterRule->mSheet = nsnull;
    NS_RELEASE(mFirstLetterRule);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.AssignWithConversion("Internal HTML/CSS Style Sheet");
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetType(nsString& aType) const
{
  aType.AssignWithConversion("text/html");
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
HTMLCSSStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  return NS_OK; // works for all media
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
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML CSS Style Sheet: ", out);
  char* urlSpec = nsnull;
  mURL->GetSpec(&urlSpec);
  if (urlSpec) {
    fputs(urlSpec, out);
    nsCRT::free(urlSpec);
  }
  fputs("\n", out);
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as HTMLCSSStyleSheetImpl's size): 
*    1) sizeof(*this) 
*
*  Contained / Aggregated data (not reported as HTMLCSSStyleSheetImpl's size):
*    1) We don't really delegate but count seperately the FirstLineRule and 
*       the FirstLetterRule if the exist and are unique instances
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void HTMLCSSStyleSheetImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    // this style sheet is lared accounted for
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("HTMLCSSStyleSheet"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(HTMLCSSStyleSheetImpl);
  aSizeOfHandler->AddSize(tag,aSize);

  // Now the associated rules (if they exist)
  // - mFirstLineRule
  // - mFirstLetterRule
  if(mFirstLineRule && uniqueItems->AddItem((void*)mFirstLineRule)){
    localSize = sizeof(*mFirstLineRule);
    aSize += localSize;
    tag = getter_AddRefs(NS_NewAtom("FirstLineRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(mFirstLetterRule && uniqueItems->AddItem((void*)mFirstLetterRule)){
    localSize = sizeof(*mFirstLetterRule);
    aSize += localSize;
    tag = getter_AddRefs(NS_NewAtom("FirstLetterRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::AttributeAffectsStyle(nsIAtom *aAttribute,
                                             nsIContent *aContent,
                                             PRBool &aAffects)
{
  // XXX can attributes affect rules in these?
  aAffects = PR_FALSE;
  return NS_OK;
}

// XXX For backwards compatibility and convenience
NS_HTML nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult, nsIURI* aURL,
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
