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

static NS_DEFINE_IID(kIHTMLCSSStyleSheetIID, NS_IHTML_CSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);


class HTMLCSSStyleSheetImpl : public nsIHTMLCSSStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLCSSStyleSheetImpl(nsIURL* aURL);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual nsIURL* GetURL(void);

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

  nsIURL* mURL;
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
      ::delete ptr;
    }
  }
}



HTMLCSSStyleSheetImpl::HTMLCSSStyleSheetImpl(nsIURL* aURL)
  : nsIHTMLCSSStyleSheet(),
    mURL(aURL)
{
  NS_INIT_REFCNT();
  NS_ADDREF(mURL);
}

HTMLCSSStyleSheetImpl::~HTMLCSSStyleSheetImpl()
{
  NS_RELEASE(mURL);
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
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PRInt32 HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                             nsIContent* aContent,
                                             nsIFrame* aParentFrame,
                                             nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
//  NS_PRECONDITION(nsnull != aParentFrame, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIHTMLContent* htmlContent;

  // for now, just get the one and only style rule from the content
  // this may need some special handling for pseudo-frames
  if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
    nsHTMLValue value;
    if (eContentAttr_HasValue == htmlContent->GetAttribute(nsHTMLAtoms::style, value)) {
      if (eHTMLUnit_ISupports == value.GetUnit()) {
        nsISupports*  rule = value.GetISupportsValue();
        if (nsnull != rule) {
          aResults->AppendElement(rule);
          NS_RELEASE(rule);
          matchCount++;
        }
      }
    }
    NS_RELEASE(htmlContent);
  }

  return matchCount;
}

nsIURL* HTMLCSSStyleSheetImpl::GetURL(void)
{
  NS_ADDREF(mURL);
  return mURL;
}

void HTMLCSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;

  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML CSS Style Sheet: ", out);
  mURL->ToString(buffer);
  fputs(buffer, out);
  fputs("\n", out);

}

NS_HTML nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult, nsIURL* aURL)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLCSSStyleSheetImpl*  it = new HTMLCSSStyleSheetImpl(aURL);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIHTMLCSSStyleSheetIID, (void **) aInstancePtrResult);
}

