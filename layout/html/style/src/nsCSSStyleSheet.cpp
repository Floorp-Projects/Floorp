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
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsILinkHandler.h"
#include "nsHTMLAtoms.h"
#include "nsIFrame.h"
#include "nsString.h"
#include "nsIPtr.h"

//#define DEBUG_REFS

static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);

NS_DEF_PTR(nsIHTMLContent);
NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleRule);
NS_DEF_PTR(nsICSSStyleRule);
NS_DEF_PTR(nsIURL);
NS_DEF_PTR(nsISupportsArray);
NS_DEF_PTR(nsICSSStyleSheet);

class CSSStyleSheetImpl : public nsICSSStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleSheetImpl(nsIURL* aURL);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRBool SelectorMatches(nsIPresContext* aPresContext,
                                 nsCSSSelector* aSelector, 
                                 nsIContent* aContent);
  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIAtom* aPseudoTag,
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

  nsIURLPtr             mURL;
  nsICSSStyleSheetPtr   mFirstChild;
  nsISupportsArrayPtr   mRules;
  nsICSSStyleSheetPtr   mNext;
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

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

CSSStyleSheetImpl::CSSStyleSheetImpl(nsIURL* aURL)
  : nsICSSStyleSheet(),
    mURL(nsnull), mFirstChild(nsnull), mRules(nsnull), mNext(nsnull)
{
  NS_INIT_REFCNT();
  mURL.SetAddRef(aURL);
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSStyleSheet size: %d\n", gInstanceCount, sizeof(*this));
#endif
}

CSSStyleSheetImpl::~CSSStyleSheetImpl()
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSStyleSheet\n", gInstanceCount);
#endif
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

PRBool CSSStyleSheetImpl::SelectorMatches(nsIPresContext* aPresContext,
                                          nsCSSSelector* aSelector, nsIContent* aContent)
{
  PRBool  result = PR_FALSE;
  nsIAtom*  contentTag = aContent->GetTag();

  if ((nsnull == aSelector->mTag) || (aSelector->mTag == contentTag)) {
    if ((nsnull != aSelector->mClass) || (nsnull != aSelector->mID) || 
        (nsnull != aSelector->mPseudoClass)) {
      nsIHTMLContentPtr htmlContent;
      if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, htmlContent.Query())) {
        if ((nsnull == aSelector->mClass) || (aSelector->mClass == htmlContent->GetClass())) {
          if ((nsnull == aSelector->mID) || (aSelector->mID == htmlContent->GetID())) {
            if ((contentTag == nsHTMLAtoms::a) && (nsnull != aSelector->mPseudoClass)) {
              // test link state
              nsILinkHandler* linkHandler;

              if (NS_OK == aPresContext->GetLinkHandler(&linkHandler)) {
                nsAutoString base, href;  // XXX base??
                htmlContent->GetAttribute("href", href);

                nsIURL* docURL = nsnull;
                nsIDocument* doc = aContent->GetDocument();
                if (nsnull != doc) {
                  docURL = doc->GetDocumentURL();
                  NS_RELEASE(doc);
                }

                nsAutoString absURLSpec;
                nsresult rv = NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
                NS_IF_RELEASE(docURL);

                nsLinkState  state;
                if (NS_OK == linkHandler->GetLinkState(absURLSpec, state)) {
                  switch (state) {
                    case eLinkState_Unvisited:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::link);
                      break;
                    case eLinkState_Visited:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::visited);
                      break;
                    case eLinkState_OutOfDate:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::outOfDate);
                      break;
                    case eLinkState_Active:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::active);
                      break;
                    case eLinkState_Hover:
                      result = PRBool (aSelector->mPseudoClass == nsHTMLAtoms::hover);
                      break;
                  }
                }
                NS_RELEASE(linkHandler);
              }
            }
            else {
              result = PR_TRUE;
            }
          }
        }
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
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIContentPtr parentContent;
  if (nsnull != aParentFrame) {
    aParentFrame->GetContent(parentContent.AssignRef());
  }

  if (aContent != parentContent) {  // if not a pseudo frame...
    nsICSSStyleSheet*  child = mFirstChild;
    while (nsnull != child) {
      matchCount += child->RulesMatching(aPresContext, aContent, aParentFrame, aResults);
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }

    PRInt32 count = (mRules.IsNotNull() ? mRules->Count() : 0);

    for (PRInt32 index = 0; index < count; index++) {
      nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mRules->ElementAt(index);

      nsCSSSelector* selector = rule->FirstSelector();
      if (SelectorMatches(aPresContext, selector, aContent)) {
        selector = selector->mNext;
        nsIFrame* frame = aParentFrame;
        nsIContentPtr lastContent;
        while ((nsnull != selector) && (nsnull != frame)) { // check compound selectors
          nsIContentPtr content;
          frame->GetContent(content.AssignRef());
          if ((content != lastContent) && // skip pseudo frames (actually we're skipping pseudo's parent, but same result)
              SelectorMatches(aPresContext, selector, content)) {
            selector = selector->mNext;
          }
          frame->GetGeometricParent(frame);
          lastContent = content;
        }
        if (nsnull == selector) { // ran out, it matched
          nsIStyleRulePtr iRule;
          if (NS_OK == rule->QueryInterface(kIStyleRuleIID, iRule.Query())) {
            aResults->AppendElement(iRule);
            matchCount++;
          }
        }
      }
    }
  }
  return matchCount;
}

PRInt32 CSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                         nsIAtom* aPseudoTag,
                                         nsIFrame* aParentFrame,
                                         nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aPseudoTag, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    matchCount += child->RulesMatching(aPresContext, aPseudoTag, aParentFrame, aResults);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  PRInt32 count = (mRules.IsNotNull() ? mRules->Count() : 0);

  for (PRInt32 index = 0; index < count; index++) {
    nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mRules->ElementAt(index);

    nsCSSSelector* selector = rule->FirstSelector();
    if (selector->mTag == aPseudoTag) {
      selector = selector->mNext;
      nsIFrame* frame = aParentFrame;
      nsIContentPtr lastContent;
      while ((nsnull != selector) && (nsnull != frame)) { // check compound selectors
        nsIContentPtr content;
        frame->GetContent(content.AssignRef());
        if ((content != lastContent) && // skip pseudo frames (actually we're skipping pseudo's parent, but same result)
            SelectorMatches(aPresContext, selector, content)) {
          selector = selector->mNext;
        }
        frame->GetGeometricParent(frame);
        lastContent = content;
      }
      if (nsnull == selector) { // ran out, it matched
        nsIStyleRulePtr iRule;
        if (NS_OK == rule->QueryInterface(kIStyleRuleIID, iRule.Query())) {
          aResults->AppendElement(iRule);
          matchCount++;
        }
      }
    }
  }
  return matchCount;
}

nsIURL* CSSStyleSheetImpl::GetURL(void)
{
  return mURL.AddRef();
}

PRBool CSSStyleSheetImpl::ContainsStyleSheet(nsIURL* aURL)
{
  NS_PRECONDITION(nsnull != aURL, "null arg");

  PRBool result = (*mURL == *aURL);

  nsICSSStyleSheet*  child = mFirstChild;
  while ((PR_FALSE == result) && (nsnull != child)) {
    result = child->ContainsStyleSheet(aURL);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }
  return result;
}

void CSSStyleSheetImpl::AppendStyleSheet(nsICSSStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (mFirstChild.IsNull()) {
    mFirstChild.SetAddRef(aSheet);
  }
  else {
    nsICSSStyleSheet* child = mFirstChild;
    while (((CSSStyleSheetImpl*)child)->mNext.IsNotNull()) {
      child = ((CSSStyleSheetImpl*)child)->mNext;
    }
    ((CSSStyleSheetImpl*)child)->mNext.SetAddRef(aSheet);
  }
}

void CSSStyleSheetImpl::PrependStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");
  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (mRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mRules.AssignPtr()))
      return;
  }
  PRInt32 index = mRules->Count();
  while (0 <= --index) {
    nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    if (rule->GetWeight() > weight) { // insert before rules with equal or lesser weight
      break;
    }
  }
  mRules->InsertElementAt(aRule, index + 1);
}

void CSSStyleSheetImpl::AppendStyleRule(nsICSSStyleRule* aRule)
{
  NS_PRECONDITION(nsnull != aRule, "null arg");

  //XXX replace this with a binary search?
  PRInt32 weight = aRule->GetWeight();
  if (mRules.IsNull()) {
    if (NS_OK != NS_NewISupportsArray(mRules.AssignPtr()))
      return;
  }
  PRInt32 count = mRules->Count();
  PRInt32 index = -1;
  while (++index < count) {
    nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    if (rule->GetWeight() < weight) { // insert after rules with equal or greater weight (before lower weight)
      break;
    }
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

  const nsICSSStyleSheet*  child = mFirstChild;
  while (nsnull != child) {
    child->List(out, aIndent + 1);
    child = ((CSSStyleSheetImpl*)child)->mNext;
  }

  PRInt32 count = (mRules.IsNotNull() ? mRules->Count() : 0);

  for (index = 0; index < count; index++) {
    nsICSSStyleRulePtr rule = (nsICSSStyleRule*)mRules->ElementAt(index);
    rule->List(out, aIndent);
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
