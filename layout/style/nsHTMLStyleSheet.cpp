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
#include "nsHashtable.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
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
#include "nsHTMLIIDs.h"

static NS_DEFINE_IID(kIHTMLStyleSheetIID, NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);


class HTMLAnchorRule : public nsIStyleRule {
public:
  HTMLAnchorRule();
  ~HTMLAnchorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

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

NS_IMETHODIMP
HTMLAnchorRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
HTMLAnchorRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)(mColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLAnchorRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  nsStyleColor* styleColor = (nsStyleColor*)(aContext->GetMutableStyleData(eStyleStruct_Color));

  if (nsnull != styleColor) {
    styleColor->mColor = mColor;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAnchorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

// -----------------------------------------------------------

class AttributeKey: public nsHashKey
{
public:
  AttributeKey(nsMapAttributesFunc aMapFunc, nsIHTMLAttributes* aAttributes);
  virtual ~AttributeKey(void);

  PRBool      Equals(const nsHashKey* aOther) const;
  PRUint32    HashValue(void) const;
  nsHashKey*  Clone(void) const;

private:
  AttributeKey(void);
  AttributeKey(const AttributeKey& aCopy);
  AttributeKey& operator=(const AttributeKey& aCopy);

public:
  nsMapAttributesFunc mMapFunc;
  nsIHTMLAttributes*  mAttributes;
  PRUint32            mHashSet: 1;
  PRUint32            mHashCode: 31;
};

AttributeKey::AttributeKey(nsMapAttributesFunc aMapFunc, nsIHTMLAttributes* aAttributes)
  : mMapFunc(aMapFunc),
    mAttributes(aAttributes)
{
  NS_ADDREF(mAttributes);
  mHashSet = 0;
}

AttributeKey::~AttributeKey(void)
{
  NS_RELEASE(mAttributes);
}

PRBool AttributeKey::Equals(const nsHashKey* aOther) const
{
  const AttributeKey* other = (const AttributeKey*)aOther;
  if (mMapFunc == other->mMapFunc) {
    PRBool  equals;
    mAttributes->Equals(other->mAttributes, equals);
    return equals;
  }
  return PR_FALSE;
}

PRUint32 AttributeKey::HashValue(void) const
{
  if (0 == mHashSet) {
    AttributeKey* self = (AttributeKey*)this; // break const
    PRUint32  hash;
    mAttributes->HashValue(hash);
    self->mHashCode = (0x7FFFFFFF & hash);
    self->mHashCode |= (0x7FFFFFFF & PRUint32(mMapFunc));
    self->mHashSet = 1;
  }
  return mHashCode;
}

nsHashKey* AttributeKey::Clone(void) const
{
  AttributeKey* clown = new AttributeKey(mMapFunc, mAttributes);
  if (nsnull != clown) {
    clown->mHashSet = mHashSet;
    clown->mHashCode = mHashCode;
  }
  return clown;
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

  // Attribute management methods, aAttributes is an in/out param
  NS_IMETHOD SetAttributesFor(nsIHTMLContent* aContent, 
                              nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetIDFor(nsIAtom* aID, nsIHTMLContent* aContent, 
                      nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetClassFor(nsIAtom* aClass, nsIHTMLContent* aContent, 
                         nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsString& aValue, 
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue, 
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, nsIHTMLContent* aContent, 
                               nsIHTMLAttributes*& aAttributes);


  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  HTMLStyleSheetImpl(const HTMLStyleSheetImpl& aCopy); 
  HTMLStyleSheetImpl& operator=(const HTMLStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLStyleSheetImpl();

  NS_IMETHOD EnsureSingleAttributes(nsIHTMLAttributes*& aAttributes, 
                                    nsMapAttributesFunc aMapFunc,
                                    PRBool aCreate, 
                                    nsIHTMLAttributes*& aSingleAttrs);
  NS_IMETHOD UniqueAttributes(nsIHTMLAttributes*& aSingleAttrs,
                              nsMapAttributesFunc aMapFunc,
                              PRInt32 aAttrCount,
                              nsIHTMLAttributes*& aAttributes);

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURL*         mURL;
  HTMLAnchorRule* mLinkRule;
  HTMLAnchorRule* mVisitedRule;
  HTMLAnchorRule* mActiveRule;
  nsHashtable     mAttrTable;
  nsIHTMLAttributes*  mRecycledAttrs;
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
    mActiveRule(nsnull),
    mRecycledAttrs(nsnull)
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
  NS_IF_RELEASE(mRecycledAttrs);
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
      NS_RELEASE(rules);
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
        // XXX: this approach has a few caveats
        //      behavior is bound to HTML content
        //      makes table cells process attributes from other content
        //      inherit based style (ie: font-size: 110%) can double on row & cell
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

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributesFor(nsIHTMLContent* aContent, nsIHTMLAttributes*& aAttributes)
{
  nsIHTMLAttributes*  attrs = aAttributes;

  if (nsnull != attrs) {
    nsMapAttributesFunc mapFunc;
    aContent->GetAttributeMappingFunction(mapFunc);
    AttributeKey  key(mapFunc, attrs);
    nsIHTMLAttributes* sharedAttrs = (nsIHTMLAttributes*)mAttrTable.Get(&key);
    if (nsnull == sharedAttrs) {  // we have a new unique set
      mAttrTable.Put(&key, attrs);
    }
    else {  // found existing set
      if (sharedAttrs != aAttributes) {
        aAttributes->ReleaseContentRef();
        NS_RELEASE(aAttributes);  // release content's ref

        aAttributes = sharedAttrs;
        aAttributes->AddContentRef();
        NS_ADDREF(aAttributes); // add ref for content
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetIDFor(nsIAtom* aID, nsIHTMLContent* aContent, nsIHTMLAttributes*& aAttributes)
{
  nsresult            result = NS_OK;
  nsIHTMLAttributes*  attrs;
  PRBool              hasValue = PRBool(nsnull != aID);
  nsMapAttributesFunc mapFunc;

  aContent->GetAttributeMappingFunction(mapFunc);

  result = EnsureSingleAttributes(aAttributes, mapFunc, hasValue, attrs);

  if ((NS_OK == result) && (nsnull != attrs)) {
    PRInt32 count;
    attrs->SetID(aID, count);

    result = UniqueAttributes(attrs, mapFunc, count, aAttributes);
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetClassFor(nsIAtom* aClass, nsIHTMLContent* aContent, nsIHTMLAttributes*& aAttributes)
{
  nsresult            result = NS_OK;
  nsIHTMLAttributes*  attrs;
  PRBool              hasValue = PRBool(nsnull != aClass);
  nsMapAttributesFunc mapFunc;

  aContent->GetAttributeMappingFunction(mapFunc);

  result = EnsureSingleAttributes(aAttributes, mapFunc, hasValue, attrs);

  if ((NS_OK == result) && (nsnull != attrs)) {
    PRInt32 count;
    attrs->SetClass(aClass, count);

    result = UniqueAttributes(attrs, mapFunc, count, aAttributes);
  }

  return result;
}


NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsString& aValue,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult            result = NS_OK;
  nsIHTMLAttributes*  attrs;
  nsMapAttributesFunc mapFunc;

  aContent->GetAttributeMappingFunction(mapFunc);

  result = EnsureSingleAttributes(aAttributes, mapFunc, PR_TRUE, attrs);

  if ((NS_OK == result) && (nsnull != attrs)) {
    PRInt32 count;
    attrs->SetAttribute(aAttribute, aValue, count);

    result = UniqueAttributes(attrs, mapFunc, count, aAttributes);
  }

  return result;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::EnsureSingleAttributes(nsIHTMLAttributes*& aAttributes, 
                                           nsMapAttributesFunc aMapFunc,
                                           PRBool aCreate, 
                                           nsIHTMLAttributes*& aSingleAttrs)
{
  nsresult  result = NS_OK;
  PRInt32   contentRefCount;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      if (nsnull != mRecycledAttrs) {
        aSingleAttrs = mRecycledAttrs;
        mRecycledAttrs = nsnull;
        aSingleAttrs->SetMappingFunction(aMapFunc);
      }
      else {
        result = NS_NewHTMLAttributes(&aSingleAttrs, aMapFunc);
      }
    }
    else {
      aSingleAttrs = nsnull;
    }
    contentRefCount = 0;
  }
  else {
    aSingleAttrs = aAttributes;
    aSingleAttrs->GetContentRefCount(contentRefCount);
    NS_ASSERTION(0 < contentRefCount, "bad content ref count");
  }

  if (NS_OK == result) {
    if (1 < contentRefCount) {  // already shared, copy it
      result = aSingleAttrs->Clone(&aSingleAttrs);
      if (NS_OK != result) {
        aSingleAttrs = nsnull;
        return result;
      }
      contentRefCount = 0;
      aAttributes->ReleaseContentRef();
      NS_RELEASE(aAttributes);
    }
    else {  // one content ref, ok to use, remove from table because hash may change
      if (1 == contentRefCount) {
        AttributeKey  key(aMapFunc, aSingleAttrs);
        mAttrTable.Remove(&key);
        NS_ADDREF(aSingleAttrs); // add a local ref so we match up 
      }
    }
    // at this point, content ref count is 0 or 1, and we hold a local ref
    // attrs is also unique and not in the table
    NS_ASSERTION(((0 == contentRefCount) && (nsnull == aAttributes)) ||
                 ((1 == contentRefCount) && (aSingleAttrs == aAttributes)), "this is broken");
  }
  return result;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::UniqueAttributes(nsIHTMLAttributes*& aSingleAttrs,
                                     nsMapAttributesFunc aMapFunc,
                                     PRInt32 aAttrCount,
                                     nsIHTMLAttributes*& aAttributes)
{
  nsresult result = NS_OK;

  if (0 < aAttrCount) {
    AttributeKey  key(aMapFunc, aSingleAttrs);
    nsIHTMLAttributes* sharedAttrs = (nsIHTMLAttributes*)mAttrTable.Get(&key);
    if (nsnull == sharedAttrs) {  // we have a new unique set
      mAttrTable.Put(&key, aSingleAttrs);
      if (aSingleAttrs != aAttributes) {
        NS_ASSERTION(nsnull == aAttributes, "this is broken");
        aAttributes = aSingleAttrs;
        aAttributes->AddContentRef(); 
        NS_ADDREF(aAttributes); // add ref for content
      }
    }
    else {  // found existing set
      NS_ASSERTION (sharedAttrs != aAttributes, "should never happen");
      if (nsnull != aAttributes) {
        aAttributes->ReleaseContentRef();
        NS_RELEASE(aAttributes);  // release content's ref
      }
      aAttributes = sharedAttrs;
      aAttributes->AddContentRef();
      NS_ADDREF(aAttributes); // add ref for content

      if (nsnull == mRecycledAttrs) {
        mRecycledAttrs = aSingleAttrs;
        NS_ADDREF(mRecycledAttrs);
        mRecycledAttrs->Reset();
      }
    }
  }
  else {  // no attributes to store
    if (nsnull != aAttributes) {
      aAttributes->ReleaseContentRef();
      NS_RELEASE(aAttributes);
    }
    if ((nsnull != aSingleAttrs) && (nsnull == mRecycledAttrs)) {
      mRecycledAttrs = aSingleAttrs;
      NS_ADDREF(mRecycledAttrs);
      mRecycledAttrs->Reset();
    }
  }
  NS_IF_RELEASE(aSingleAttrs);
  return result;
}



NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsHTMLValue& aValue,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult            result = NS_OK;
  nsIHTMLAttributes*  attrs;
  PRBool              hasValue = PRBool(eHTMLUnit_Null != aValue.GetUnit());
  nsMapAttributesFunc mapFunc;

  aContent->GetAttributeMappingFunction(mapFunc);

  result = EnsureSingleAttributes(aAttributes, mapFunc, hasValue, attrs);

  if ((NS_OK == result) && (nsnull != attrs)) {
    PRInt32 count;
    attrs->SetAttribute(aAttribute, aValue, count);

    result = UniqueAttributes(attrs, mapFunc, count, aAttributes);
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::UnsetAttributeFor(nsIAtom* aAttribute,
                                                    nsIHTMLContent* aContent, 
                                                    nsIHTMLAttributes*& aAttributes)
{
  nsresult            result = NS_OK;
  nsIHTMLAttributes*  attrs;
  nsMapAttributesFunc mapFunc;

  aContent->GetAttributeMappingFunction(mapFunc);

  result = EnsureSingleAttributes(aAttributes, mapFunc, PR_FALSE, attrs);

  if ((NS_OK == result) && (nsnull != attrs)) {
    PRInt32 count;
    attrs->UnsetAttribute(aAttribute, count);

    result = UniqueAttributes(attrs, mapFunc, count, aAttributes);
  }

  return result;

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
