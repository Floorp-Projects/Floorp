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
#include "nsICSSStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsICSSStyleRule.h"
#include "nsIHTMLContent.h"
#include "nsIFrame.h"
#include "nsString.h"

static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);


class CSSStyleSheetImpl : public nsICSSStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleSheetImpl(nsIURL* aURL);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRBool SelectorMatches(nsCSSSelector* aSelector, 
                                 nsIContent* aContent);
  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual nsIURL* GetURL(void);

  virtual PRBool ContainsStyleSheet(nsIURL* aURL);

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet);

  // XXX do these belong here or are they generic?
  virtual void PrependStyleRule(nsICSSStyleRule* aRule);
  virtual void AppendStyleRule(nsICSSStyleRule* aRule);

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  CSSStyleSheetImpl(const CSSStyleSheetImpl& aCopy); 
  CSSStyleSheetImpl& operator=(const CSSStyleSheetImpl& aCopy); 

protected:
  virtual ~CSSStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURL*             mURL;
  CSSStyleSheetImpl*  mFirstChild;
  nsISupportsArray*   mRules;
  CSSStyleSheetImpl*  mNext;
};


void* CSSStyleSheetImpl::operator new(size_t size)
{
  CSSStyleSheetImpl* rv = (CSSStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* CSSStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  CSSStyleSheetImpl* rv = (CSSStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void CSSStyleSheetImpl::operator delete(void* ptr)
{
  CSSStyleSheetImpl* sheet = (CSSStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::delete ptr;
    }
  }
}



CSSStyleSheetImpl::CSSStyleSheetImpl(nsIURL* aURL)
  : nsICSSStyleSheet(),
    mURL(aURL), mFirstChild(nsnull), mRules(nsnull), mNext(nsnull)
{
  NS_INIT_REFCNT();
  NS_ADDREF(mURL);
}

CSSStyleSheetImpl::~CSSStyleSheetImpl()
{
  NS_RELEASE(mURL);
  NS_IF_RELEASE(mFirstChild);
  NS_IF_RELEASE(mRules);
  NS_IF_RELEASE(mNext);
}

NS_IMPL_ADDREF(CSSStyleSheetImpl)
NS_IMPL_RELEASE(CSSStyleSheetImpl)

nsresult CSSStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                           void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kICSSStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsICSSStyleSheet*)this);
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

PRBool CSSStyleSheetImpl::SelectorMatches(nsCSSSelector* aSelector, nsIContent* aContent)
{
  PRBool  result = PR_FALSE;

  if ((nsnull == aSelector->mTag) || (aSelector->mTag == aContent->GetTag())) {
    if ((nsnull != aSelector->mClass) || (nsnull != aSelector->mTag)) {
      nsIHTMLContent* htmlContent;
      if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
        if ((nsnull == aSelector->mClass) || (aSelector->mClass == htmlContent->GetClass())) {
          if ((nsnull == aSelector->mID) || (aSelector->mID == htmlContent->GetID())) {
            result = PR_TRUE;
          }
        }
        NS_RELEASE(htmlContent);
      }
    }
    else {
      result = PR_TRUE;
    }
  }
  return result;
}

PRInt32 CSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsIFrame* aParentFrame,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
//  NS_PRECONDITION(nsnull != aParentFrame, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;
  CSSStyleSheetImpl*  child = mFirstChild;
  while (nsnull != child) {
    matchCount += child->RulesMatching(aPresContext, aContent, aParentFrame, aResults);
    child = child->mNext;
  }

  PRInt32 count = ((nsnull != mRules) ? mRules->Count() : 0);

  for (PRInt32 index = 0; index < count; index++) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mRules->ElementAt(index);

    nsCSSSelector* selector = rule->FirstSelector();
    if (SelectorMatches(selector, aContent)) {
      selector = selector->mNext;
      nsIFrame* frame = aParentFrame;
      while ((nsnull != selector) && (nsnull != frame)) { // check compound selectors
        nsIContent* content = frame->GetContent();
        if (SelectorMatches(selector, content)) {
          selector = selector->mNext;
        }
        frame = frame->GetGeometricParent();
        NS_RELEASE(content);
      }
      if (nsnull == selector) { // ran out, it matched
        nsIStyleRule* iRule;
        if (NS_OK == rule->QueryInterface(kIStyleRuleIID, (void**)&iRule)) {
          aResults->AppendElement(iRule);
          NS_RELEASE(iRule);
          matchCount++;
        }
      }
    }
    NS_RELEASE(rule);
  }
  return matchCount;
}

nsIURL* CSSStyleSheetImpl::GetURL(void)
{
  NS_ADDREF(mURL);
  return mURL;
}

PRBool CSSStyleSheetImpl::ContainsStyleSheet(nsIURL* aURL)
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

  PRBool result = (*mURL == *aURL);

  CSSStyleSheetImpl*  child = mFirstChild;
  while ((PR_FALSE == result) && (nsnull != child)) {
    result = child->ContainsStyleSheet(aURL);
    child = child->mNext;
  }
  return result;
}

void CSSStyleSheetImpl::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull == mFirstChild) {
    mFirstChild = (CSSStyleSheetImpl*)aSheet;
  }
  else {
    CSSStyleSheetImpl* child = mFirstChild;
    while (nsnull != child->mNext) {
      child = child->mNext;
    }
    child->mNext = (CSSStyleSheetImpl*)aSheet;
  }
  NS_ADDREF(aSheet);
}

void CSSStyleSheetImpl::PrependStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");
  NS_ADDREF(aRule);
  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (nsnull == mRules) {
    if (NS_OK != NS_NewISupportsArray(&mRules))
      return;
  }
  PRInt32 index = mRules->Count();
  while (0 <= --index) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    if (rule->GetWeight() > weight) { // insert before rules with equal or lesser weight
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mRules->InsertElementAt(aRule, index + 1);
}

void CSSStyleSheetImpl::AppendStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");
  NS_ADDREF(aRule);

  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (nsnull == mRules) {
    if (NS_OK != NS_NewISupportsArray(&mRules))
      return;
  }
  PRInt32 count = mRules->Count();
  PRInt32 index = -1;
  while (++index < count) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    if (rule->GetWeight() < weight) { // insert after rules with equal or greater weight (before lower weight)
      NS_RELEASE(rule);
      break;
    }
    NS_RELEASE(rule);
  }
  mRules->InsertElementAt(aRule, index);
}

void CSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;
  PRInt32 index;

  // Indent
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("CSS Style Sheet: ", out);
  mURL->ToString(buffer);
  fputs(buffer, out);
  fputs("\n", out);

  CSSStyleSheetImpl*  child = mFirstChild;
  while (nsnull != child) {
    child->List(out, aIndent + 1);
    child = child->mNext;
  }

  PRInt32 count = ((nsnull != mRules) ? mRules->Count() : 0);

  for (index = 0; index < count; index++) {
    nsICSSStyleRule* rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    rule->List(out, aIndent);
    NS_RELEASE(rule);
  }
}

NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURL* aURL)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSStyleSheetImpl  *it = new CSSStyleSheetImpl(aURL);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSStyleSheetIID, (void **) aInstancePtrResult);
}
