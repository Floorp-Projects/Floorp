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
#include "nsIHTMLStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsIHTMLContent.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsTableCell.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"

static NS_DEFINE_IID(kIHTMLStyleSheetIID, NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);


class HTMLAnchorRule : public nsIStyleRule {
public:
  HTMLAnchorRule();
  ~HTMLAnchorRule();

  NS_DECL_ISUPPORTS

  virtual PRBool Equals(const nsIStyleRule* aRule) const;
  virtual PRUint32 HashValue(void) const;

  virtual void MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nscolor mColor;
};

HTMLAnchorRule::HTMLAnchorRule()
{
  NS_INIT_REFCNT();
}

HTMLAnchorRule::~HTMLAnchorRule()
{
}

NS_IMPL_ISUPPORTS(HTMLAnchorRule, kIStyleRuleIID);

PRBool HTMLAnchorRule::Equals(const nsIStyleRule* aRule) const
{
  return PRBool(this == aRule);
}

PRUint32 HTMLAnchorRule::HashValue(void) const
{
  return (PRUint32)(mColor);
}

void HTMLAnchorRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  nsStyleColor* styleColor = (nsStyleColor*)(aContext->GetMutableStyleData(eStyleStruct_Color));

  if (nsnull != styleColor) {
    styleColor->mColor = mColor;
  }
}

NS_IMETHODIMP
HTMLAnchorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

// -----------------------------------------------------------

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLStyleSheetImpl(nsIURL* aURL);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIAtom* aPseudoTag,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults);

  virtual nsIURL* GetURL(void);

  NS_IMETHOD SetLinkColor(nscolor aColor);
  NS_IMETHOD SetActiveLinkColor(nscolor aColor);
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor);

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  HTMLStyleSheetImpl(const HTMLStyleSheetImpl& aCopy); 
  HTMLStyleSheetImpl& operator=(const HTMLStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURL*         mURL;
  HTMLAnchorRule* mLinkRule;
  HTMLAnchorRule* mVisitedRule;
  HTMLAnchorRule* mActiveRule;
};


void* HTMLStyleSheetImpl::operator new(size_t size)
{
  HTMLStyleSheetImpl* rv = (HTMLStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* HTMLStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  HTMLStyleSheetImpl* rv = (HTMLStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void HTMLStyleSheetImpl::operator delete(void* ptr)
{
  HTMLStyleSheetImpl* sheet = (HTMLStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::delete ptr;
    }
  }
}



HTMLStyleSheetImpl::HTMLStyleSheetImpl(nsIURL* aURL)
  : nsIHTMLStyleSheet(),
    mURL(aURL),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull)
{
  NS_INIT_REFCNT();
  NS_ADDREF(mURL);
}

HTMLStyleSheetImpl::~HTMLStyleSheetImpl()
{
  NS_RELEASE(mURL);
  NS_IF_RELEASE(mLinkRule);
  NS_IF_RELEASE(mVisitedRule);
  NS_IF_RELEASE(mActiveRule);
}

NS_IMPL_ADDREF(HTMLStyleSheetImpl)
NS_IMPL_RELEASE(HTMLStyleSheetImpl)

nsresult HTMLStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIHTMLStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIHTMLStyleSheet*)this);
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

struct AppendData 
{
  AppendData(PRInt32 aBackstopCount, PRInt32 aInsertPoint, nsISupportsArray* aResults)
    : mCount(0),
      mBackstop(aBackstopCount),
      mInsert(aInsertPoint),
      mResults(aResults)
  {}

  PRInt32 mCount;
  PRInt32 mBackstop;
  PRInt32 mInsert;
  nsISupportsArray* mResults;
};

PRBool AppendFunc(nsISupports* aElement, void *aData)
{
  AppendData* data = (AppendData*)aData;
  if (data->mCount < data->mBackstop) {
    data->mResults->InsertElementAt(aElement, data->mInsert++);
  }
  else {
    data->mResults->AppendElement(aElement);
  }
  data->mCount++;
  return PR_TRUE;
}

PRInt32 AppendRulesFrom(nsIFrame* aFrame, nsIPresContext* aPresContext, PRInt32& aInsertPoint, nsISupportsArray* aResults)
{
  PRInt32 count = 0;
  nsIStyleContext*  context;
  aFrame->GetStyleContext(aPresContext, context);
  if (nsnull != context) {
    count = context->GetStyleRuleCount();
    if (0 < count) {
      PRInt32 backstopCount = context->GetBackstopStyleRuleCount();
      nsISupportsArray* rules = context->GetStyleRules();

      AppendData  data(backstopCount, aInsertPoint, aResults);
      rules->EnumerateForwards(AppendFunc, &data);
      aInsertPoint = data.mInsert;
    }
    NS_RELEASE(context);
  }
  return count;
}

PRInt32 HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                          nsIContent* aContent,
                                          nsIFrame* aParentFrame,
                                          nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
//  NS_PRECONDITION(nsnull != aParentFrame, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

  nsIContent* parentContent = nsnull;
  if (nsnull != aParentFrame) {
    aParentFrame->GetContent(parentContent);
  }

  if (aContent != parentContent) {  // if not a pseudo frame...
    nsIHTMLContent* htmlContent;
    if (NS_OK == aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent)) {
      nsIAtom*  tag;
      htmlContent->GetTag(tag);
      // if we have anchor colors, check if this is an anchor with an href
      if (tag == nsHTMLAtoms::a) {
        if ((nsnull != mLinkRule) || (nsnull != mVisitedRule) || (nsnull != mActiveRule)) {
          // test link state
          nsILinkHandler* linkHandler;

          if ((NS_OK == aPresContext->GetLinkHandler(&linkHandler)) &&
              (nsnull != linkHandler)) {
            nsAutoString base, href;  // XXX base??
            nsresult attrState = htmlContent->GetAttribute("href", href);

            if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
              nsIURL* docURL = nsnull;
              nsIDocument* doc = nsnull;
              aContent->GetDocument(doc);
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
                    if (nsnull != mLinkRule) {
                      aResults->AppendElement(mLinkRule);
                      matchCount++;
                    }
                    break;
                  case eLinkState_Visited:
                    if (nsnull != mVisitedRule) {
                      aResults->AppendElement(mVisitedRule);
                      matchCount++;
                    }
                    break;
                  case eLinkState_Active:
                    if (nsnull != mActiveRule) {
                      aResults->AppendElement(mActiveRule);
                      matchCount++;
                    }
                    break;
                }
              }
            }
            NS_RELEASE(linkHandler);
          }
        }
      } // end A tag
      else if ((tag == nsHTMLAtoms::td) || (tag == nsHTMLAtoms::th)) {
        // propogate row/col style rules
        PRInt32 backstopInsertPoint = 0;
        nsTableCell*  cell = (nsTableCell*)aContent;
        PRInt32 colIndex = cell->GetColIndex();

        nsIFrame* rowFrame = aParentFrame;
        nsIFrame* rowGroupFrame;
        nsIFrame* tableFrame;

        rowFrame->GetContentParent(rowGroupFrame);
        rowGroupFrame->GetContentParent(tableFrame);

        nsTableColFrame* colFrame;
        nsIFrame* colGroupFrame;

        ((nsTableFrame*)tableFrame)->GetColumnFrame(colIndex, colFrame);
        colFrame->GetContentParent(colGroupFrame);

        matchCount += AppendRulesFrom(colGroupFrame, aPresContext, backstopInsertPoint, aResults);
        matchCount += AppendRulesFrom(colFrame, aPresContext, backstopInsertPoint, aResults);
        matchCount += AppendRulesFrom(rowGroupFrame, aPresContext, backstopInsertPoint, aResults);
        matchCount += AppendRulesFrom(rowFrame, aPresContext, backstopInsertPoint, aResults);

      } // end TD/TH tag

      // just get the one and only style rule from the content
      nsIStyleRule* rule;
      htmlContent->GetStyleRule(rule);
      if (nsnull != rule) {
        aResults->AppendElement(rule);
        NS_RELEASE(rule);
        matchCount++;
      }

      NS_IF_RELEASE(tag);
      NS_RELEASE(htmlContent);
    }
  }
  NS_IF_RELEASE(parentContent);

  return matchCount;
}

PRInt32 HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                          nsIAtom* aPseudoTag,
                                          nsIFrame* aParentFrame,
                                          nsISupportsArray* aResults)
{
  // no pseudo frame style
  return 0;
}


nsIURL* HTMLStyleSheetImpl::GetURL(void)
{
  NS_ADDREF(mURL);
  return mURL;
}


NS_IMETHODIMP HTMLStyleSheetImpl::SetLinkColor(nscolor aColor)
{
  if (nsnull == mLinkRule) {
    mLinkRule = new HTMLAnchorRule();
    if (nsnull == mLinkRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mLinkRule);
  }
  mLinkRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetActiveLinkColor(nscolor aColor)
{
  if (nsnull == mActiveRule) {
    mActiveRule = new HTMLAnchorRule();
    if (nsnull == mActiveRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mActiveRule);
  }
  mActiveRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetVisitedLinkColor(nscolor aColor)
{
  if (nsnull == mVisitedRule) {
    mVisitedRule = new HTMLAnchorRule();
    if (nsnull == mVisitedRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mVisitedRule);
  }
  mVisitedRule->mColor = aColor;
  return NS_OK;
}

void HTMLStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  nsAutoString buffer;

  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
  mURL->ToString(buffer);
  fputs(buffer, out);
  fputs("\n", out);

}

NS_HTML nsresult
  NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURL* aURL)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLStyleSheetImpl  *it = new HTMLStyleSheetImpl(aURL);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIHTMLStyleSheetIID, (void **) aInstancePtrResult);
}
