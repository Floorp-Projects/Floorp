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
#include "nsIHTMLTableCellElement.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleFrameConstruction.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsStyleConsts.h"
#include "nsTableOuterFrame.h"
#include "nsIXMLDocument.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIHTMLStyleSheetIID, NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

class HTMLAnchorRule : public nsIStyleRule {
public:
  HTMLAnchorRule();
  ~HTMLAnchorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength);

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

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
HTMLAnchorRule::GetStrength(PRInt32& aStrength)
{
  aStrength = 0;
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
  mHashCode = 0;
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

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet,
                           public nsIStyleFrameConstruction {
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
                                nsIStyleContext* aParentContext,
                                nsISupportsArray* aResults);

  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aParentContent,
                                nsIAtom* aPseudoTag,
                                nsIStyleContext* aParentContext,
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

  NS_IMETHOD ConstructFrame(nsIPresContext* aPresContext,
                            nsIContent*     aContent,
                            nsIFrame*       aParentFrame,
                            nsIFrame*&      aFrameSubTree);

  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer);

  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer);

  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer);

  NS_IMETHOD ContentRemoved(nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInContainer);

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

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

  nsresult ConstructRootFrame(nsIPresContext*  aPresContext,
                              nsIContent*      aContent,
                              nsIStyleContext* aStyleContext,
                              nsIFrame*&       aNewFrame);

  nsresult ConstructXMLRootFrame(nsIPresContext*  aPresContext,
                                 nsIContent*      aContent,
                                 nsIStyleContext* aStyleContext,
                                 nsIFrame*&       aNewFrame);

  nsresult ConstructTableFrame(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aNewFrame);

  nsresult ConstructFrameByTag(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParentFrame,
                               nsIAtom*         aTag,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aNewFrame);

  nsresult ConstructFrameByDisplayType(nsIPresContext*  aPresContext,
                                       const nsStyleDisplay*  aDisplay,
                                       nsIContent*      aContent,
                                       nsIFrame*        aParentFrame,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aNewFrame);

  nsresult GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                  PRUint8    aChildDisplayType,
                                  nsIFrame*& aNewParentFrame);


  nsresult ProcessChildren(nsIPresContext* aPresContext,
                           nsIFrame*       aFrame,
                           nsIContent*     aContent,
                           nsIFrame*&      aChildList);

  nsresult CreateInputFrame(nsIContent* aContent,
                            nsIFrame*   aParentFrame,
                            nsIFrame*&  aFrame);

  PRBool IsScrollable(nsIPresContext* aPresContext, const nsStyleDisplay* aDisplay);

  nsIFrame* GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                        nsIContent* aContent);

  PRBool AttributeRequiresRepaint(nsIAtom* aAttribute);
  PRBool AttributeRequiresReflow (nsIAtom* aAttribute);

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
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleFrameConstructionIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleFrameConstruction*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PRInt32 HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                          nsIContent* aContent,
                                          nsIStyleContext* aParentContext,
                                          nsISupportsArray* aResults)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aResults, "null arg");

  PRInt32 matchCount = 0;

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

  return matchCount;
}

PRInt32 HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                          nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
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

nsresult
HTMLStyleSheetImpl::ProcessChildren(nsIPresContext* aPresContext,
                                    nsIFrame*       aFrame,
                                    nsIContent*     aContent,
                                    nsIFrame*&      aChildList)
{
  // Initialize OUT parameter
  aChildList = nsnull;

  // Iterate the child content objects and construct a frame
  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsIContent* childContent;
    aContent->ChildAt(i, childContent);

    if (nsnull != childContent) {
      nsIFrame* childFrame;

      // Construct a child frame
      ConstructFrame(aPresContext, childContent, aFrame, childFrame);

      if (nsnull != childFrame) {
        // Link the frame into the child list
        if (nsnull == lastChildFrame) {
          aChildList = childFrame;
        } else {
          lastChildFrame->SetNextSibling(childFrame);
        }
        lastChildFrame = childFrame;
      }

      NS_RELEASE(childContent);
    }
  }

  return NS_OK;
}

nsresult
HTMLStyleSheetImpl::CreateInputFrame(nsIContent* aContent,
                                     nsIFrame*   aParentFrame,
                                     nsIFrame*&  aFrame)
{
  nsresult  rv;

  // Figure out which type of input frame to create
  nsAutoString  val;
  if (NS_OK == aContent->GetAttribute(nsAutoString("type"), val)) {
    if (val.EqualsIgnoreCase("submit")) {
      rv = NS_NewButtonControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("reset")) {
      rv = NS_NewButtonControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("button")) {
      rv = NS_NewButtonControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("checkbox")) {
      rv = NS_NewCheckboxControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("file")) {
      rv = NS_NewFileControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("hidden")) {
      rv = NS_NewButtonControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("image")) {
      rv = NS_NewImageControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("password")) {
      rv = NS_NewTextControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("radio")) {
      rv = NS_NewRadioControlFrame(aContent, aParentFrame, aFrame);
    }
    else if (val.EqualsIgnoreCase("text")) {
      rv = NS_NewTextControlFrame(aContent, aParentFrame, aFrame);
    }
    else {
      rv = NS_NewTextControlFrame(aContent, aParentFrame, aFrame);
    }
  } else {
    rv = NS_NewTextControlFrame(aContent, aParentFrame, aFrame);
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructTableFrame(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParentFrame,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*&       aNewFrame)
{
  nsIFrame* childList;
  nsIFrame* innerFrame;
  nsIFrame* innerChildList = nsnull;
  nsIFrame* captionFrame = nsnull;

  // Create an anonymous table outer frame which holds the caption and the
  // table frame
  NS_NewTableOuterFrame(aContent, aParentFrame, aNewFrame);
  aNewFrame->SetStyleContext(aPresContext, aStyleContext);

  // Create the inner table frame
  NS_NewTableFrame(aContent, aNewFrame, innerFrame);
  childList = innerFrame;

  // Have the inner table frame use the same style context as the outer table frame
  innerFrame->SetStyleContext(aPresContext, aStyleContext);

  // Iterate the child content
  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsIFrame* grandChildList=nsnull;  // to be used only when pseudoframes need to be created
    nsIContent* childContent;
    aContent->ChildAt(i, childContent);

    if (nsnull != childContent) {
      nsIFrame*         frame = nsnull;
      nsIStyleContext*  childStyleContext;

      // Resolve the style context
      childStyleContext = aPresContext->ResolveStyleContextFor(childContent, aStyleContext);

      // See how it should be displayed
      const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
        childStyleContext->GetStyleData(eStyleStruct_Display);

      switch (styleDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_TABLE_CAPTION:
        // Have we already created a caption? If so, ignore this caption
        if (nsnull == captionFrame) {
          NS_NewBodyFrame(childContent, aNewFrame, captionFrame,
                          NS_BODY_NO_AUTO_MARGINS);
          captionFrame->SetStyleContext(aPresContext, childStyleContext);
          // Process the caption's child content and set the initial child list
          nsIFrame* captionChildList;
          ProcessChildren(aPresContext, captionFrame, childContent, captionChildList);
          captionFrame->SetInitialChildList(*aPresContext, nsnull, captionChildList);

          // Prepend the caption frame to the outer frame's child list
          innerFrame->SetNextSibling(captionFrame);
        }
        break;

      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
        NS_NewTableRowGroupFrame(childContent, innerFrame, frame);
        break;

      case NS_STYLE_DISPLAY_TABLE_ROW:
      {
        // When we're done here, "frame" will be the pseudo rowgroup frame and will be dealt with normally after the swtich.
        // The row itself will have already been dealt with
        nsIStyleContext*  rowStyleContext;
        nsIStyleContext*  rowGroupStyleContext;
        rowGroupStyleContext = aPresContext->ResolvePseudoStyleContextFor (childContent, 
                                                                           nsHTMLAtoms::columnPseudo,
                                                                           aStyleContext);
        nsStyleDisplay *rowGroupDisplay = (nsStyleDisplay *)rowGroupStyleContext->GetMutableStyleData(eStyleStruct_Display);
        rowGroupDisplay->mDisplay = NS_STYLE_DISPLAY_TABLE_ROW_GROUP;
        NS_NewTableRowGroupFrame(childContent, innerFrame, frame);
        NS_RELEASE(childStyleContext); // we can't use this resolved style, so get rid of it
        childStyleContext = rowGroupStyleContext;  // the row group style context will get set to childStyleContext after the switch ends.
        // need to resolve the style context for the column again to be sure it's a child of the colgroup style context
        rowStyleContext = aPresContext->ResolveStyleContextFor(childContent, rowGroupStyleContext);
        nsIFrame *rowFrame;
        NS_NewTableRowFrame(childContent, frame, rowFrame);
        rowFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
        rowFrame->SetStyleContext(aPresContext, rowStyleContext); 
        grandChildList = rowFrame;
        break;
      }
/*
      case NS_STYLE_DISPLAY_TABLE_CELL:
      {
        // When we're done here, "frame" will be the pseudo rowgroup frame and will be dealt with normally after the swtich.
        // The row itself will have already been dealt with
        nsIStyleContext*  cellStyleContext;
        nsIStyleContext*  rowStyleContext;
        nsIStyleContext*  rowGroupStyleContext;
        rowGroupStyleContext = aPresContext->ResolvePseudoStyleContextFor (childContent, 
                                                                           nsHTMLAtoms::columnPseudo,
                                                                           aStyleContext);
        nsStyleDisplay *rowGroupDisplay = (nsStyleDisplay *)rowGroupStyleContext->GetMutableStyleData(eStyleStruct_Display);
        rowGroupDisplay->mDisplay = NS_STYLE_DISPLAY_TABLE_ROW_GROUP;
        NS_NewTableRowGroupFrame(childContent, innerFrame, frame);
        NS_RELEASE(childStyleContext); // we can't use this resolved style, so get rid of it
        childStyleContext = rowGroupStyleContext;  // the row group style context will get set to childStyleContext after the switch ends.
        // need to resolve the style context for the column again to be sure it's a child of the colgroup style context
        rowStyleContext = aPresContext->ResolveStyleContextFor(childContent, rowGroupStyleContext);
        nsIFrame *rowFrame;
        NS_NewTableRowFrame(childContent, frame, rowFrame);
        rowFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
        rowFrame->SetStyleContext(aPresContext, rowStyleContext); 
        grandChildList = rowFrame;
        break;
      }
*/
      case NS_STYLE_DISPLAY_TABLE_COLUMN:
      {
        //XXX: Peter -  please code review and remove this comment when all is well.
        // When we're done here, "frame" will be the pseudo colgroup frame and will be dealt with normally after the swtich.
        // The column itself will have already been dealt with
        nsIStyleContext*  colStyleContext;
        nsIStyleContext*  colGroupStyleContext;
        colGroupStyleContext = aPresContext->ResolvePseudoStyleContextFor (childContent, 
                                                                           nsHTMLAtoms::columnPseudo,
                                                                           aStyleContext);
        nsStyleDisplay *colGroupDisplay = (nsStyleDisplay *)colGroupStyleContext->GetMutableStyleData(eStyleStruct_Display);
        colGroupDisplay->mDisplay = NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP;
        NS_NewTableColGroupFrame(childContent, innerFrame, frame);
        NS_RELEASE(childStyleContext);  // we can't use this resolved style, so get rid of it
        childStyleContext = colGroupStyleContext;  // the col group style context will get set to childStyleContext after the switch ends.
        // need to resolve the style context for the column again to be sure it's a child of the colgroup style context
        colStyleContext = aPresContext->ResolveStyleContextFor(childContent, colGroupStyleContext);
        nsIFrame *colFrame;
        NS_NewTableColFrame(childContent, frame, colFrame);
        colFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
        colFrame->SetStyleContext(aPresContext, colStyleContext); 
        grandChildList = colFrame;
        break;
      }

      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
        NS_NewTableColGroupFrame(childContent, innerFrame, frame);
        break;

      default:
        // XXX For the time being ignore everything else. We should deal with
        // things like table cells and create anonymous frames...
        break;
      }

      // If it's not a caption frame, then set the style context and link the
      // frame into the inner frame's child list
      if (nsnull != frame) {
        frame->SetStyleContext(aPresContext, childStyleContext);

        // Process the children, and set the frame's initial child list
        nsIFrame* childChildList;
        if (nsnull==grandChildList)
        {
          ProcessChildren(aPresContext, frame, childContent, childChildList);
          grandChildList = childChildList;
        }
        else
        {
          ProcessChildren(aPresContext, grandChildList, childContent, childChildList);
          grandChildList->SetInitialChildList(*aPresContext, nsnull, childChildList);
        }
        frame->SetInitialChildList(*aPresContext, nsnull, grandChildList);
  
        // Link the frame into the child list
        if (nsnull == lastChildFrame) {
          innerChildList = frame;
        } else {
          lastChildFrame->SetNextSibling(frame);
        }
        lastChildFrame = frame;
      }

      NS_RELEASE(childStyleContext);
      NS_RELEASE(childContent);
    }
  }

  // Set the inner table frame's list of initial child frames
  innerFrame->SetInitialChildList(*aPresContext, nsnull, innerChildList);

  // Set the anonymous table outer frame's initial child list
  aNewFrame->SetInitialChildList(*aPresContext, nsnull, childList);
  return NS_OK;
}

nsresult
HTMLStyleSheetImpl::ConstructRootFrame(nsIPresContext*  aPresContext,
                                       nsIContent*      aContent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aNewFrame)
{
#ifdef NS_DEBUG
    nsIDocument*  doc;
    nsIContent*   rootContent;

    // Verify it's the root content object
    aContent->GetDocument(doc);
    rootContent = doc->GetRootContent();
    NS_RELEASE(doc);
    NS_ASSERTION(rootContent == aContent, "unexpected content");
    NS_RELEASE(rootContent);
#endif

  // Create the root frame
  nsresult  rv = NS_NewHTMLFrame(aContent, nsnull, aNewFrame);

  if (NS_SUCCEEDED(rv)) {
    // Bind root frame to root view (and root window)
    nsIPresShell*   presShell = aPresContext->GetShell();
    nsIViewManager* viewManager = presShell->GetViewManager();
    nsIView*        rootView;
  
    NS_RELEASE(presShell);
    viewManager->GetRootView(rootView);
    aNewFrame->SetView(rootView);
    NS_RELEASE(viewManager);
  
    // Set the style context
    aNewFrame->SetStyleContext(aPresContext, aStyleContext);
  
    // See if we're paginated
    if (aPresContext->IsPaginated()) {
      nsIFrame*  scrollFrame;
      nsIFrame*  pageSequenceFrame;

      // XXX This isn't the correct style context pseudo element to use...
      nsIStyleContext*  pseudoStyle;
      pseudoStyle = aPresContext->ResolvePseudoStyleContextFor(aContent,
                      nsHTMLAtoms::columnPseudo, aStyleContext);

      // Wrap the simple page sequence frame in a scroll frame
      // XXX Only do this if it's print preview
      if NS_SUCCEEDED(NS_NewScrollFrame(aContent, aNewFrame, scrollFrame)) {
        // Set the scroll frame's style context
        scrollFrame->SetStyleContext(aPresContext, pseudoStyle);

        // Create a simple page sequence frame
        rv = NS_NewSimplePageSequenceFrame(aContent, scrollFrame, pageSequenceFrame);
        if (NS_SUCCEEDED(rv)) {
          nsIFrame* childList;
  
          // Set the page sequence frame's style context
          pageSequenceFrame->SetStyleContext(aPresContext, pseudoStyle);
          NS_RELEASE(pseudoStyle);
  
          // Process the child content, and set the page sequence frame's initial
          // child list
          rv = ProcessChildren(aPresContext, pageSequenceFrame, aContent, childList);
          if (NS_SUCCEEDED(rv)) {
            pageSequenceFrame->SetInitialChildList(*aPresContext, nsnull, childList);
    
            // Set the scroll frame's initial child list
            scrollFrame->SetInitialChildList(*aPresContext, nsnull, pageSequenceFrame);
          }

          // Set the root frame's initial child list
          aNewFrame->SetInitialChildList(*aPresContext, nsnull, scrollFrame);
        }
      }
    } else {
      nsIFrame* childList;

      // Process the child content, and set the frame's initial child list
      rv = ProcessChildren(aPresContext, aNewFrame, aContent, childList);
      if (NS_SUCCEEDED(rv)) {
        aNewFrame->SetInitialChildList(*aPresContext, nsnull, childList);
      }
    }
  }

  return rv;  
}

nsresult
HTMLStyleSheetImpl::ConstructXMLRootFrame(nsIPresContext*  aPresContext,
                                          nsIContent*      aContent,
                                          nsIStyleContext* aStyleContext,
                                          nsIFrame*&       aNewFrame)
{
  // Create the root frame. It gets a special pseudo element style.
  // XXX It's wrong that the document element's style context (which is
  // passed in) isn't based on the xml-root pseudo element style context
  // we create below. That means that things like font information defined
  // in the ua.css don't get properly inherited. We could re-resolve the
  // style context, or change the flow of control so we create the style
  // context rather than pass it in...
  nsIStyleContext*  rootPseudoStyle;
  rootPseudoStyle = aPresContext->ResolvePseudoStyleContextFor(nsnull, 
                      nsHTMLAtoms::xmlRootPseudo, nsnull);

  // XXX It would be nice if we didn't need this and we made the scroll
  // frame (or the body wrapper frame) the root of the frame hierarchy
  nsresult  rv = NS_NewHTMLFrame(nsnull, nsnull, aNewFrame);

  if (NS_SUCCEEDED(rv)) {
    // Bind root frame to root view (and root window)
    nsIPresShell*   presShell = aPresContext->GetShell();
    nsIViewManager* viewManager = presShell->GetViewManager();
    nsIView*        rootView;
  
    NS_RELEASE(presShell);
    viewManager->GetRootView(rootView);
    aNewFrame->SetView(rootView);
    NS_RELEASE(viewManager);
  
    // Set the style context
    aNewFrame->SetStyleContext(aPresContext, rootPseudoStyle);

    // Create a scroll frame.
    // XXX Use the rootPseudoStyle overflow style information to decide whether
    // we create a scroll frame or just a body wrapper frame...
    nsIFrame* scrollFrame;
    
    if (NS_SUCCEEDED(NS_NewScrollFrame(nsnull, aNewFrame, scrollFrame))) {
      // The scroll frame gets the root pseudo style context, and the scrolled
      // frame gets a SCROLLED-CONTENT pseudo element style context.
      // XXX We should probably use a different pseudo style context...
      scrollFrame->SetStyleContext(aPresContext, rootPseudoStyle);

      nsIStyleContext*  scrolledPseudoStyle;
      nsIFrame* wrapperFrame;
                    
      scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                              (nsnull, nsHTMLAtoms::scrolledContentPseudo,
                               rootPseudoStyle);

      // Create a body frame to wrap the document element
      NS_NewBodyFrame(nsnull, scrollFrame, wrapperFrame, NS_BODY_SHRINK_WRAP);
      wrapperFrame->SetStyleContext(aPresContext, scrolledPseudoStyle);

      // Construct a frame for the document element and process its children
      nsIFrame* docElementFrame;
      ConstructFrame(aPresContext, aContent, wrapperFrame, docElementFrame);
      wrapperFrame->SetInitialChildList(*aPresContext, nsnull, docElementFrame);

      // Set the scroll frame's initial child list
      scrollFrame->SetInitialChildList(*aPresContext, nsnull, wrapperFrame);
    }

    // initialize the root frame
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, scrollFrame);
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructFrameByTag(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParentFrame,
                                        nsIAtom*         aTag,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*&       aNewFrame)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  nsresult  rv = NS_OK;

  // Initialize OUT parameter
  aNewFrame = nsnull;

  if (nsnull == aTag) {
    rv = NS_NewTextFrame(aContent, aParentFrame, aNewFrame);
  }
  else {
    nsIHTMLContent *htmlContent;
    rv = aContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
    if (NS_SUCCEEDED(rv)) {
      if (nsHTMLAtoms::img == aTag) {
        rv = NS_NewImageFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::hr == aTag) {
        rv = NS_NewHRFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::br == aTag) {
        rv = NS_NewBRFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::wbr == aTag) {
        rv = NS_NewWBRFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::input == aTag) {
        rv = CreateInputFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::textarea == aTag) {
        rv = NS_NewTextControlFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::select == aTag) {
        rv = NS_NewSelectControlFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::applet == aTag) {
        rv = NS_NewObjectFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::embed == aTag) {
        rv = NS_NewObjectFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::fieldset == aTag) {
        rv = NS_NewFieldSetFrame(aContent, aParentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::legend == aTag) {
        rv = NS_NewLegendFrame(aContent, aParentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::object == aTag) {
        rv = NS_NewObjectFrame(aContent, aParentFrame, aNewFrame);
        //    processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::body == aTag) {
        rv = NS_NewBodyFrame(aContent, aParentFrame, aNewFrame,
                             NS_BODY_THE_BODY|NS_BODY_NO_AUTO_MARGINS);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::form == aTag) {
        rv = NS_NewFormFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::frameset == aTag) {
        rv = NS_NewHTMLFramesetFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        rv = NS_NewHTMLFrameOuterFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        rv = NS_NewSpacerFrame(aContent, aParentFrame, aNewFrame);
      }
      else if (nsHTMLAtoms::button == aTag) {
        rv = NS_NewHTMLButtonControlFrame(aContent, aParentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::label == aTag) {
        rv = NS_NewLabelFrame(aContent, aParentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
      NS_RELEASE(htmlContent);
    }
    else {
      aNewFrame = nsnull;
      rv = NS_OK;
    }
  }

  // If we succeeded in creating a frame then set its style context,
  // process its children (if requested), and initialize the frame
  if (NS_SUCCEEDED(rv) && (nsnull != aNewFrame)) {
    aNewFrame->SetStyleContext(aPresContext, aStyleContext);

    // Process the child content if requested
    nsIFrame* childList = nsnull;
    if (processChildren) {
      rv = ProcessChildren(aPresContext, aNewFrame, aContent, childList);
    }

    // Set the frame's initial child list
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, childList);
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructFrameByDisplayType(nsIPresContext*  aPresContext,
                                                const nsStyleDisplay* aDisplay,
                                                nsIContent*      aContent,
                                                nsIFrame*        aParentFrame,
                                                nsIStyleContext* aStyleContext,
                                                nsIFrame*&       aNewFrame)
{
  PRBool                processChildren = PR_FALSE;  // whether we should process child content
  nsresult              rv = NS_OK;

  // Initialize OUT parameter
  aNewFrame = nsnull;

  switch (aDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_RUN_IN:
  case NS_STYLE_DISPLAY_COMPACT:
    rv = NS_NewBlockFrame(aContent, aParentFrame, aNewFrame, PR_FALSE);
    processChildren = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_INLINE:
    rv = NS_NewInlineFrame(aContent, aParentFrame, aNewFrame);
    processChildren = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE:
    rv = ConstructTableFrame(aPresContext, aContent, aParentFrame,
                             aStyleContext, aNewFrame);
    // Note: table construction function takes care of setting the style context,
    // processing children, and setting the initial child list
    return rv;

  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    // XXX We should check for being inside of a table. If there's a missing
    // table then create an anonynmous table frame
    // XXX: see ConstructTableFrame for a prototype of how this should be done,
    //      and propagate similar logic to other table elements
    {
      nsIFrame *parentFrame;
      rv = GetAdjustedParentFrame(aParentFrame, aDisplay->mDisplay, parentFrame);
      if (NS_SUCCEEDED(rv))
      {
        rv = NS_NewTableRowGroupFrame(aContent, parentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
    }
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN:
    // XXX We should check for being inside of a table column group...
    rv = NS_NewTableColFrame(aContent, aParentFrame, aNewFrame);
    processChildren = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    // XXX We should check for being inside of a table...
    {
      nsIFrame *parentFrame;
      rv = GetAdjustedParentFrame(aParentFrame, aDisplay->mDisplay, parentFrame);
      if (NS_SUCCEEDED(rv))
      {
        rv = NS_NewTableColGroupFrame(aContent, parentFrame, aNewFrame);
        processChildren = PR_TRUE;
      }
    }
    break;

  case NS_STYLE_DISPLAY_TABLE_ROW:
    // XXX We should check for being inside of a table row group...
    rv = NS_NewTableRowFrame(aContent, aParentFrame, aNewFrame);
    processChildren = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE_CELL:
    // XXX We should check for being inside of a table row frame...
    rv = NS_NewTableCellFrame(aContent, aParentFrame, aNewFrame);
    processChildren = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE_CAPTION:
    // XXX We should check for being inside of a table row frame...
    rv = NS_NewBodyFrame(aContent, aParentFrame, aNewFrame, NS_BODY_NO_AUTO_MARGINS);
    processChildren = PR_TRUE;
    break;

  default:
    // Don't create any frame for content that's not displayed...
    break;
  }

  // If we succeeded in creating a frame then set its style context,
  // process children (if requested), and initialize the frame
  if (NS_SUCCEEDED(rv) && (nsnull != aNewFrame)) {
    aNewFrame->SetStyleContext(aPresContext, aStyleContext);

    // Process the child content if requested
    nsIFrame* childList = nsnull;
    if (processChildren) {
      rv = ProcessChildren(aPresContext, aNewFrame, aContent, childList);
    }

    // Set the frame's initial child list
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, childList);
  }

  return rv;
}

nsresult 
HTMLStyleSheetImpl::GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                           PRUint8    aChildDisplayType, 
                                           nsIFrame*& aNewParentFrame)
{
  NS_PRECONDITION(nsnull!=aCurrentParentFrame, "bad arg aCurrentParentFrame");

  nsresult rv=NS_OK;
  // by default, the new parent frame is the given current parent frame
  aNewParentFrame=aCurrentParentFrame;
  if (nsnull!=aCurrentParentFrame)
  {
    const nsStyleDisplay* currentParentDisplay;
    aCurrentParentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)currentParentDisplay);
    if (NS_STYLE_DISPLAY_TABLE==currentParentDisplay->mDisplay)
    {
      if (NS_STYLE_DISPLAY_TABLE_CAPTION!=aChildDisplayType)
      {
        nsIFrame *innerTableFrame=nsnull;
        aCurrentParentFrame->FirstChild(nsnull, innerTableFrame);
        if (nsnull!=innerTableFrame)
        {
          const nsStyleDisplay* innerTableDisplay;
          innerTableFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)innerTableDisplay);
          if (NS_STYLE_DISPLAY_TABLE==innerTableDisplay->mDisplay)
          { // we were given the outer table frame, use the inner table frame
            aNewParentFrame=innerTableFrame;
          }
          // else we were already given the inner table frame
        }
        // else the current parent has no children and cannot be an outer table frame
      }
      // else the child is a caption and really belongs to the outer table frame
    }
    else
    { 
      if (NS_STYLE_DISPLAY_TABLE_CAPTION     ==aChildDisplayType ||
          NS_STYLE_DISPLAY_TABLE_ROW_GROUP   ==aChildDisplayType ||
          NS_STYLE_DISPLAY_TABLE_HEADER_GROUP==aChildDisplayType ||
          NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP==aChildDisplayType ||
          NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP==aChildDisplayType)
      { // we need to create an anonymous table frame (outer and inner) around the new frame
        NS_NOTYETIMPLEMENTED("anonymous table frame as parent of table content not yet implemented.");
        rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    }
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  NS_POSTCONDITION(nsnull!=aNewParentFrame, "bad result null aNewParentFrame");
  return rv;
}

PRBool
HTMLStyleSheetImpl::IsScrollable(nsIPresContext* aPresContext, const nsStyleDisplay* aDisplay)
{
  // For the time being it's scrollable if the overflow property is auto or
  // scroll, regardless of whether the width or height is fixed in size
  PRInt32 scrolling = -1;
  nsISupports* container;
  if (nsnull != aPresContext) {
    aPresContext->GetContainer(&container);
    if (nsnull != container) {
      nsIWebShell* webShell = nsnull;
      container->QueryInterface(kIWebShellIID, (void**) &webShell);
      if (nsnull != webShell) {
        webShell->GetScrolling(scrolling);
        NS_RELEASE(webShell);
      }
      NS_RELEASE(container);
    }
  }

  if (-1 == scrolling) {
    scrolling = aDisplay->mOverflow;
  }
  if ((NS_STYLE_OVERFLOW_SCROLL == scrolling) ||
      (NS_STYLE_OVERFLOW_AUTO == scrolling)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ConstructFrame(nsIPresContext*  aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame*        aParentFrame,
                                   nsIFrame*&       aFrameSubTree)
{
  nsresult  rv;

  // Get the tag
  nsIAtom*  tag;
  aContent->GetTag(tag);

  // Resolve the style context.
  // XXX Cheesy hack for text
  nsIStyleContext* parentStyleContext = nsnull;
  if (nsnull != aParentFrame) {
    aParentFrame->GetStyleContext(parentStyleContext);
  }
  nsIStyleContext* styleContext;
  if (nsnull == tag) {
    nsIContent* parentContent = nsnull;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(parentContent);
    }
    styleContext = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                              nsHTMLAtoms::text, 
                                                              parentStyleContext);
    NS_IF_RELEASE(parentContent);
  } else {
    styleContext = aPresContext->ResolveStyleContextFor(aContent, parentStyleContext);
  }
  NS_IF_RELEASE(parentStyleContext);
  // XXX bad api - no nsresult returned!
  if (nsnull == styleContext) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    // Pre-check for display "none" - if we find that, don't create
    // any frame at all.
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);
    if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
      aFrameSubTree = nsnull;
      rv = NS_OK;
    }
    else {
      // Create a frame.
      if (nsnull == aParentFrame) {
        nsIDocument* document;
        rv = aContent->GetDocument(document);
        if (NS_FAILED(rv)) {
          NS_RELEASE(styleContext);
          return rv;
        }
        if (nsnull != document) {
          nsIXMLDocument* xmlDocument;
          rv = document->QueryInterface(kIXMLDocumentIID, (void **)&xmlDocument);
          if (NS_FAILED(rv)) {
            // Construct the root frame object
            rv = ConstructRootFrame(aPresContext, aContent, styleContext,
                                    aFrameSubTree);
          }
          else {
            // Construct the root frame object for XML
            rv = ConstructXMLRootFrame(aPresContext, aContent, styleContext,
                                       aFrameSubTree);
            NS_RELEASE(xmlDocument);
          }
          NS_RELEASE(document);
        }
      } else {
        // If the frame is a block-level frame and is scrollable then wrap it
        // in a scroll frame.
        // XXX Applies to replaced elements, too, but how to tell if the element
        // is replaced?
        nsIFrame* scrollFrame = nsnull;
        nsIFrame* wrapperFrame = nsnull;

        // If we're paginated then don't ever make the BODY scrollable
        // XXX Use a special BODY rule for paged media...
        if (!(aPresContext->IsPaginated() && (nsHTMLAtoms::body == tag))) {
          if (display->IsBlockLevel() && IsScrollable(aPresContext, display)) {
            // Create a scroll frame which will wrap the frame that needs to
            // be scrolled
            if NS_SUCCEEDED(NS_NewScrollFrame(aContent, aParentFrame, scrollFrame)) {
              nsIStyleContext*  scrolledPseudoStyle;
              
              // The scroll frame gets the original style context, and the scrolled
              // frame gets a SCROLLED-CONTENT pseudo element style context that
              // inherits the background properties
              scrollFrame->SetStyleContext(aPresContext, styleContext);
              scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                                      (aContent, nsHTMLAtoms::scrolledContentPseudo,
                                       styleContext);
              NS_RELEASE(styleContext);
              
              // If the content element can contain children then wrap it in a
              // BODY frame. Don't do this for the BODY element, though...
              PRBool  isContainer;
              aContent->CanContainChildren(isContainer);
              if (isContainer && (tag != nsHTMLAtoms::body)) {
                NS_NewBodyFrame(aContent, scrollFrame, wrapperFrame, NS_BODY_SHRINK_WRAP);
                wrapperFrame->SetStyleContext(aPresContext, scrolledPseudoStyle);

                // The wrapped frame also gets a pseudo style context, but it doesn't
                // inherit any background properties
                // XXX We should define something like :WRAPPED-FRAME in ua.css
                nsIStyleContext*  wrappedPseudoStyle;
                wrappedPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                                       (aContent, nsHTMLAtoms::columnPseudo,
                                        scrolledPseudoStyle);
                NS_RELEASE(scrolledPseudoStyle);
                aParentFrame = wrapperFrame;
                styleContext = wrappedPseudoStyle;

              } else {
                aParentFrame = scrollFrame;
                styleContext = scrolledPseudoStyle;
              }
            }
          }
        }

        // Handle specific frame types
        rv = ConstructFrameByTag(aPresContext, aContent, aParentFrame,
                                 tag, styleContext, aFrameSubTree);

        if (NS_SUCCEEDED(rv) && (nsnull == aFrameSubTree)) {
          // When there is no explicit frame to create, assume it's a
          // container and let display style dictate the rest
          rv = ConstructFrameByDisplayType(aPresContext, display,
                                           aContent, aParentFrame,
                                           styleContext, aFrameSubTree);
        }

        // Set the scroll frame's initial child list and return the scroll frame
        // as the frame sub-tree
        if (nsnull != scrollFrame) {
          if (nsnull != wrapperFrame) {
            wrapperFrame->SetInitialChildList(*aPresContext, nsnull, aFrameSubTree);
            scrollFrame->SetInitialChildList(*aPresContext, nsnull, wrapperFrame);
          } else {
            scrollFrame->SetInitialChildList(*aPresContext, nsnull, aFrameSubTree);
          }
          aFrameSubTree = scrollFrame;
        }
      }
    }
    NS_RELEASE(styleContext);
  }
  
  NS_IF_RELEASE(tag);
  return rv;
}

nsIFrame*
HTMLStyleSheetImpl::GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                                nsIContent* aContent)
{
  nsIFrame* frame = aPresShell->FindFrameWithContent(aContent);

  if (nsnull != frame) {
    // If the primary frame is a scroll frame, then get the scrolled frame.
    // That's the frame that gets the reflow command
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    if (display->IsBlockLevel() && IsScrollable(aPresContext, display)) {
      frame->FirstChild(nsnull, frame);
    }
  }

  return frame;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ContentAppended(nsIPresContext* aPresContext,
                                    nsIContent*     aContainer,
                                    PRInt32         aNewIndexInContainer)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsIFrame*     parentFrame = GetFrameFor(shell, aPresContext, aContainer);

#ifdef NS_DEBUG
  if (nsnull == parentFrame) {
    NS_WARNING("Ignoring content-appended notification with no matching frame...");
  }
#endif
  if (nsnull != parentFrame) {
    // Get the parent frame's last-in-flow
    nsIFrame* nextInFlow = parentFrame;
    while (nsnull != nextInFlow) {
      parentFrame->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        parentFrame = nextInFlow;
      }
    }

    // Create some new frames
    PRInt32   count;
    nsIFrame* lastChildFrame = nsnull;
    nsIFrame* firstAppendedFrame = nsnull;

    aContainer->ChildCount(count);

    for (PRInt32 i = aNewIndexInContainer; i < count; i++) {
      nsIContent* child;
      nsIFrame*   frame;

      aContainer->ChildAt(i, child);
      ConstructFrame(aPresContext, child, parentFrame, frame);

      if (nsnull != frame) {
        // Link the frame into the child frame list
        if (nsnull == lastChildFrame) {
          firstAppendedFrame = frame;
        } else {
          lastChildFrame->SetNextSibling(frame);
        }

        lastChildFrame = frame;
      }
      NS_RELEASE(child);
    }

    // Adjust parent frame for table inner/outer frame
    // we need to do this here because we need both the parent frame and the constructed frame
    nsresult result = NS_OK;
    nsIFrame *adjustedParentFrame=parentFrame;
    if (nsnull!=firstAppendedFrame)
    {
      const nsStyleDisplay* firstAppendedFrameDisplay;
      firstAppendedFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)firstAppendedFrameDisplay);
      result = GetAdjustedParentFrame(parentFrame, firstAppendedFrameDisplay->mDisplay, adjustedParentFrame);
    }

    // Notify the parent frame with a reflow command, passing it the list of
    // new frames.
    if (NS_SUCCEEDED(result)) {
      nsIReflowCommand* reflowCmd;
      result = NS_NewHTMLReflowCommand(&reflowCmd, adjustedParentFrame,
                                       nsIReflowCommand::FrameAppended,
                                       firstAppendedFrame);
      if (NS_SUCCEEDED(result)) {
        shell->AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
  }

  NS_RELEASE(shell);
  return NS_OK;
}

static nsIFrame*
FindPreviousSibling(nsIPresShell* aPresShell,
                    nsIContent*   aContainer,
                    PRInt32       aIndexInContainer)
{
  nsIFrame* prevSibling = nsnull;

  // Note: not all content objects are associated with a frame so
  // keep looking until we find a previous frame
  for (PRInt32 index = aIndexInContainer - 1; index >= 0; index--) {
    nsIContent* precedingContent;
    aContainer->ChildAt(index, precedingContent);
    prevSibling = aPresShell->FindFrameWithContent(precedingContent);
    NS_RELEASE(precedingContent);

    if (nsnull != prevSibling) {
      // The frame may have a next-in-flow. Get the last-in-flow
      nsIFrame* nextInFlow;
      do {
        prevSibling->GetNextInFlow(nextInFlow);
        if (nsnull != nextInFlow) {
          prevSibling = nextInFlow;
        }
      } while (nsnull != nextInFlow);

      break;
    }
  }

  return prevSibling;
}

static nsIFrame*
FindNextSibling(nsIPresShell* aPresShell,
                nsIContent*   aContainer,
                PRInt32       aIndexInContainer)
{
  nsIFrame* nextSibling = nsnull;

  // Note: not all content objects are associated with a frame so
  // keep looking until we find a next frame
  PRInt32 count;
  aContainer->ChildCount(count);
  for (PRInt32 index = aIndexInContainer + 1; index < count; index++) {
    nsIContent* nextContent;
    aContainer->ChildAt(index, nextContent);
    nextSibling = aPresShell->FindFrameWithContent(nextContent);
    NS_RELEASE(nextContent);

    if (nsnull != nextSibling) {
      // The frame may have a next-in-flow. Get the last-in-flow
      nsIFrame* nextInFlow;
      do {
        nextSibling->GetNextInFlow(nextInFlow);
        if (nsnull != nextInFlow) {
          nextSibling = nextInFlow;
        }
      } while (nsnull != nextInFlow);

      break;
    }
  }

  return nextSibling;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ContentInserted(nsIPresContext* aPresContext,
                                    nsIContent*     aContainer,
                                    nsIContent*     aChild,
                                    PRInt32         aIndexInContainer)
{
  nsIPresShell* shell = aPresContext->GetShell();

  // Find the frame that precedes the insertion point.
  nsIFrame* prevSibling = FindPreviousSibling(shell, aContainer, aIndexInContainer);
  nsIFrame* nextSibling = nsnull;
  PRBool    isAppend = PR_FALSE;

  // If there is no previous sibling, then find the frame that follows
  if (nsnull == prevSibling) {
    nextSibling = FindNextSibling(shell, aContainer, aIndexInContainer);
  }

  // Get the geometric parent.
  nsIFrame* parentFrame;
  if ((nsnull == prevSibling) && (nsnull == nextSibling)) {
    // No previous or next sibling so treat this like an appended frame.
    // XXX This won't always be true if there's auto-generated before/after
    // content
    isAppend = PR_TRUE;
    parentFrame = shell->FindFrameWithContent(aContainer);

  } else {
    // Use the prev sibling if we have it; otherwise use the next sibling.
    // Note that we use the content parent, and not the geometric parent,
    // in case the frame has been moved out of the flow...
    if (nsnull != prevSibling) {
      prevSibling->GetContentParent(parentFrame);
    } else {
      nextSibling->GetContentParent(parentFrame);
    }
  }

  // Construct a new frame
  nsresult  rv = NS_OK;
  if (nsnull != parentFrame) {
    nsIFrame* newFrame;
    rv = ConstructFrame(aPresContext, aChild, parentFrame, newFrame);

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      nsIReflowCommand* reflowCmd = nsnull;

      // Notify the parent frame with a reflow command.
      if (isAppend) {
        // Generate a FrameAppended reflow command
        rv = NS_NewHTMLReflowCommand(&reflowCmd, parentFrame,
                                     nsIReflowCommand::FrameAppended, newFrame);
      } else {
        // Generate a FrameInserted reflow command
        rv = NS_NewHTMLReflowCommand(&reflowCmd, parentFrame, newFrame, prevSibling);
      }

      if (NS_SUCCEEDED(rv)) {
        shell->AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
  }

  NS_RELEASE(shell);
  return rv;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ContentReplaced(nsIPresContext* aPresContext,
                                    nsIContent*     aContainer,
                                    nsIContent*     aOldChild,
                                    nsIContent*     aNewChild,
                                    PRInt32         aIndexInContainer)
{
  NS_NOTYETIMPLEMENTED("content replaced");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ContentRemoved(nsIPresContext* aPresContext,
                                   nsIContent*     aContainer,
                                   nsIContent*     aChild,
                                   PRInt32         aIndexInContainer)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* childFrame = shell->FindFrameWithContent(aChild);

  if (nsnull != childFrame) {
    // Get the parent frame.
    // Note that we use the content parent, and not the geometric parent,
    // in case the frame has been moved out of the flow...
    nsIFrame* parentFrame;
    childFrame->GetContentParent(parentFrame);
    NS_ASSERTION(nsnull != parentFrame, "null content parent frame");

    // Notify the parent frame with a reflow command.
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, parentFrame,
                                 nsIReflowCommand::FrameRemoved, childFrame);

    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  NS_RELEASE(shell);
  return rv;
}


static void
ApplyRenderingChangeToTree(nsIPresContext* aPresContext,
                           nsIFrame* aFrame)
{
  nsIViewManager* viewManager = nsnull;

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.
  while (nsnull != aFrame) {
    // Get the frame's bounding rect
    nsRect r;
    aFrame->GetRect(r);
    r.x = 0;
    r.y = 0;

    // Get view if this frame has one and trigger an update. If the
    // frame doesn't have a view, find the nearest containing view
    // (adjusting r's coordinate system to reflect the nesting) and
    // update there.
    nsIView* view;
    aFrame->GetView(view);
    if (nsnull != view) {
    } else {
      nsPoint offset;
      aFrame->GetOffsetFromView(offset, view);
      NS_ASSERTION(nsnull != view, "no view");
      r += offset;
    }
    if (nsnull == viewManager) {
      view->GetViewManager(viewManager);
    }
    const nsStyleColor* color;
    aFrame->GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&) color);
    viewManager->SetViewOpacity(view, color->mOpacity);
    viewManager->UpdateView(view, r, NS_VMREFRESH_NO_SYNC);

    aFrame->GetNextInFlow(aFrame);
  }

  if (nsnull != viewManager) {
    viewManager->Composite();
    NS_RELEASE(viewManager);
  }
}

static void
StyleChangeReflow(nsIPresContext* aPresContext,
                  nsIFrame* aFrame,
                  nsIAtom * aAttribute)
{
  nsIPresShell* shell;
  shell = aPresContext->GetShell();
    
  nsIReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        nsIReflowCommand::StyleChanged,
                                        nsnull,
                                        aAttribute);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  NS_RELEASE(shell);
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ContentChanged(nsIPresContext* aPresContext,
                                   nsIContent*  aContent,
                                   nsISupports* aSubContent)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* frame = shell->FindFrameWithContent(aContent);

  // Notify the first frame that maps the content. It will generate a reflow
  // command

  // It's possible the frame whose content changed isn't inserted into the
  // frame hierarchy yet, or that there is no frame that maps the content
  if (nsnull != frame) {
#if 0
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsHTMLStyleSheet::ContentChanged: content=%p[%s] subcontent=%p frame=%p",
        aContent, ContentTag(aContent, 0),
        aSubContent, frame));
#endif
    frame->ContentChanged(aPresContext, aContent, aSubContent);
  }

  NS_RELEASE(shell);
  return rv;
}

PRBool HTMLStyleSheetImpl::AttributeRequiresReflow(nsIAtom* aAttribute)
{
    // these are attributes that always require a restyle and reflow
    // regardless of the tag they are associated with
  return (PRBool)
    (aAttribute==nsHTMLAtoms::align        ||
     aAttribute==nsHTMLAtoms::border       ||
     aAttribute==nsHTMLAtoms::cellpadding  ||
     aAttribute==nsHTMLAtoms::cellspacing  ||
     aAttribute==nsHTMLAtoms::ch           ||
     aAttribute==nsHTMLAtoms::choff        ||
     aAttribute==nsHTMLAtoms::colspan      ||
     aAttribute==nsHTMLAtoms::face         ||
     aAttribute==nsHTMLAtoms::frame        ||
     aAttribute==nsHTMLAtoms::height       ||
     aAttribute==nsHTMLAtoms::nowrap       ||
     aAttribute==nsHTMLAtoms::rowspan      ||
     aAttribute==nsHTMLAtoms::rules        ||
     aAttribute==nsHTMLAtoms::span         ||
     aAttribute==nsHTMLAtoms::valign       ||
     aAttribute==nsHTMLAtoms::width);
}

PRBool HTMLStyleSheetImpl::AttributeRequiresRepaint(nsIAtom* aAttribute)
{
  // these are attributes that always require a restyle and a repaint, but not a reflow
  // regardless of the tag they are associated with
  return (PRBool)
    (aAttribute==nsHTMLAtoms::bgcolor ||
     aAttribute==nsHTMLAtoms::color);
}

    
NS_IMETHODIMP
HTMLStyleSheetImpl::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent* aContent,
                                     nsIAtom* aAttribute,
                                     PRInt32 aHint)
{
  nsresult  result = NS_OK;

  nsIPresShell* shell = aPresContext->GetShell();
  nsIFrame* frame = shell->FindFrameWithContent(aContent);

  if (nsnull != frame) {
    PRBool  restyle = PR_FALSE;
    PRBool  reflow  = PR_FALSE;
    PRBool  reframe = PR_FALSE;
    PRBool  render  = PR_FALSE;

#if 0
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("HTMLStyleSheet::AttributeChanged: content=%p[%s] frame=%p",
        aContent, ContentTag(aContent, 0), frame));
#endif

    if (PR_TRUE==AttributeRequiresReflow(aAttribute)) {
      restyle = PR_TRUE;
      reflow = PR_TRUE;
    }
    else if (PR_TRUE==AttributeRequiresRepaint(aAttribute)) {
      restyle = PR_TRUE;
      render = PR_TRUE;
    }
    // the style tag has its own interpretation based on aHint
    else if (nsHTMLAtoms::style==aAttribute) {
      switch (aHint) {
        default:
        case NS_STYLE_HINT_UNKNOWN:
        case NS_STYLE_HINT_FRAMECHANGE:
          reframe = PR_TRUE;
        case NS_STYLE_HINT_REFLOW:
          reflow = PR_TRUE;
        case NS_STYLE_HINT_VISUAL:
          render = PR_TRUE;
        case NS_STYLE_HINT_CONTENT:
          restyle = PR_TRUE;
          break;
        case NS_STYLE_HINT_AURAL:
          break;
      }
    }

    // this is the hook for specifying behavior of an attribute based on the tag
    else
    {
      nsIHTMLContent* htmlContent;
      result = aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);

      if (NS_OK == result) {
        nsIAtom*  tag;
        htmlContent->GetTag(tag);

        // handle specific attributes by tag
        if (nsHTMLAtoms::table==tag)
        {
          if (nsHTMLAtoms::summary!=aAttribute)
          {
            restyle = PR_TRUE;
            reflow = PR_TRUE;
          }
        }
        else if (nsHTMLAtoms::col==tag      ||
                 nsHTMLAtoms::colgroup==tag ||
                 nsHTMLAtoms::tr==tag       ||
                 nsHTMLAtoms::tbody==tag    ||
                 nsHTMLAtoms::thead==tag    ||
                 nsHTMLAtoms::tfoot==tag)
        {
          restyle = PR_TRUE;
          reflow = PR_TRUE;
        }
        else if (nsHTMLAtoms::td==tag ||
                 nsHTMLAtoms::th==tag)
        {
          if (nsHTMLAtoms::abbr!=aAttribute &&
              nsHTMLAtoms::axis!=aAttribute &&
              nsHTMLAtoms::headers!=aAttribute &&
              nsHTMLAtoms::scope!=aAttribute)
          {
            restyle = PR_TRUE;
            reflow = PR_TRUE;
          }
        }

        NS_IF_RELEASE(tag);
        NS_RELEASE(htmlContent);
      }
    }

    // apply changes
    if (PR_TRUE == restyle) {
      nsIStyleContext* frameContext;
      frame->GetStyleContext(frameContext);
      NS_ASSERTION(nsnull != frameContext, "frame must have style context");
      if (nsnull != frameContext) {
        nsIStyleContext*  parentContext = frameContext->GetParent();
        frame->ReResolveStyleContext(aPresContext, parentContext);
        NS_IF_RELEASE(parentContext);
        NS_RELEASE(frameContext);
      }
    }
    if (PR_TRUE == reframe) {
      NS_NOTYETIMPLEMENTED("frame change reflow");
    }
    else if (PR_TRUE == reflow) {
      StyleChangeReflow(aPresContext, frame, aAttribute);
    }
    else if (PR_TRUE == render) {
      ApplyRenderingChangeToTree(aPresContext, frame);
    }
    else {  // let the frame deal with it, since we don't know how to
      frame->AttributeChanged(aPresContext, aContent, aAttribute, aHint);
    }
  }

  NS_RELEASE(shell);
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
