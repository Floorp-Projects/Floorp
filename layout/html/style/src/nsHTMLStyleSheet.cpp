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
#include "nsINameSpaceManager.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
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
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsHTMLIIDs.h"
#include "nsCSSFrameConstructor.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"

static NS_DEFINE_IID(kIHTMLStyleSheetIID, NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);

class HTMLColorRule : public nsIStyleRule {
public:
  HTMLColorRule(nsIHTMLStyleSheet* aSheet);
  virtual ~HTMLColorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nscolor             mColor;
  nsIHTMLStyleSheet*  mSheet;
};

class HTMLDocumentColorRule : public HTMLColorRule {
public:
  HTMLDocumentColorRule(nsIHTMLStyleSheet* aSheet);
  virtual ~HTMLDocumentColorRule();

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  nscolor mBackgroundColor;
  PRBool mForegroundSet;
  PRBool mBackgroundSet;
};

HTMLColorRule::HTMLColorRule(nsIHTMLStyleSheet* aSheet)
  : mSheet(aSheet)
{
  NS_INIT_REFCNT();
}

HTMLColorRule::~HTMLColorRule()
{
}

NS_IMPL_ISUPPORTS(HTMLColorRule, kIStyleRuleIID);

NS_IMETHODIMP
HTMLColorRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)(mColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
HTMLColorRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  nsStyleColor* styleColor = (nsStyleColor*)(aContext->GetMutableStyleData(eStyleStruct_Color));

  if (nsnull != styleColor) {
    styleColor->mColor = mColor;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

HTMLDocumentColorRule::HTMLDocumentColorRule(nsIHTMLStyleSheet* aSheet) 
  : HTMLColorRule(aSheet)
{
  mForegroundSet = PR_FALSE;
  mBackgroundSet = PR_FALSE;
}
  
HTMLDocumentColorRule::~HTMLDocumentColorRule()
{
}

NS_IMETHODIMP
HTMLDocumentColorRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  nsStyleColor* styleColor = (nsStyleColor*)(aContext->GetMutableStyleData(eStyleStruct_Color));

  if (nsnull != styleColor) {
    if (mForegroundSet) {
      styleColor->mColor = mColor;
    }
    if (mBackgroundSet) {
      styleColor->mBackgroundColor = mBackgroundColor;
      styleColor->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
  }
  return NS_OK;
}

// -----------------------------------------------------------
// this rule only applies in NavQuirks mode
// -----------------------------------------------------------
class TableBackgroundRule: public nsIStyleRule {
public:
  TableBackgroundRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableBackgroundRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext,
                              nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext,
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

TableBackgroundRule::TableBackgroundRule(nsIHTMLStyleSheet* aSheet)
{
  NS_INIT_REFCNT();
  mSheet = aSheet;
}

TableBackgroundRule::~TableBackgroundRule()
{
}

NS_IMPL_ISUPPORTS(TableBackgroundRule, kIStyleRuleIID);

NS_IMETHODIMP
TableBackgroundRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
TableBackgroundRule::HashValue(PRUint32& aValue) const
{
  aValue = 0;
  return NS_OK;
}

NS_IMETHODIMP
TableBackgroundRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, useful for mapping CSS ! important
// always 0 here
NS_IMETHODIMP
TableBackgroundRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
TableBackgroundRule::MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
TableBackgroundRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  nsStyleColor* styleColor;
  styleColor = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);

  nsIStyleContext* parentContext = aContext->GetParent();
  const nsStyleColor* parentStyleColor;
  parentStyleColor = (const nsStyleColor*)parentContext->GetStyleData(eStyleStruct_Color);

  if (!(parentStyleColor->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
    styleColor->mBackgroundColor = parentStyleColor->mBackgroundColor;
    styleColor->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
  }

  if (!(parentStyleColor->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE)) {
    styleColor->mBackgroundImage = parentStyleColor->mBackgroundImage;
    styleColor->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
  }

  return NS_OK;
}

NS_IMETHODIMP
TableBackgroundRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

// -----------------------------------------------------------

class AttributeKey: public nsHashKey
{
public:
  AttributeKey(nsIHTMLMappedAttributes* aAttributes);
  virtual ~AttributeKey(void);

  PRBool      Equals(const nsHashKey* aOther) const;
  PRUint32    HashValue(void) const;
  nsHashKey*  Clone(void) const;

private:
  AttributeKey(void);
  AttributeKey(const AttributeKey& aCopy);
  AttributeKey& operator=(const AttributeKey& aCopy);

public:
  nsIHTMLMappedAttributes*  mAttributes;
  union {
    struct {
      PRUint32  mHashSet: 1;
      PRUint32  mHashCode: 31;
    } mBits;
    PRUint32    mInitializer;
  } mHash;
};

AttributeKey::AttributeKey(nsIHTMLMappedAttributes* aAttributes)
  : mAttributes(aAttributes)
{
  NS_ADDREF(mAttributes);
  mHash.mInitializer = 0;
}

AttributeKey::~AttributeKey(void)
{
  NS_RELEASE(mAttributes);
}

PRBool AttributeKey::Equals(const nsHashKey* aOther) const
{
  const AttributeKey* other = (const AttributeKey*)aOther;
  PRBool  equals;
  mAttributes->Equals(other->mAttributes, equals);
  return equals;
}

PRUint32 AttributeKey::HashValue(void) const
{
  if (0 == mHash.mBits.mHashSet) {
    AttributeKey* self = (AttributeKey*)this; // break const
    PRUint32  hash;
    mAttributes->HashValue(hash);
    self->mHash.mBits.mHashCode = (0x7FFFFFFF & hash);
    self->mHash.mBits.mHashSet = 1;
  }
  return mHash.mBits.mHashCode;
}

nsHashKey* AttributeKey::Clone(void) const
{
  AttributeKey* clown = new AttributeKey(mAttributes);
  if (nsnull != clown) {
    clown->mHash.mInitializer = mHash.mInitializer;
  }
  return clown;
}

// -----------------------------------------------------------

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLStyleSheetImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIStyleSheet api
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

  NS_IMETHOD SetOwningDocument(nsIDocument* aDocumemt);

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

  // nsIHTMLStyleSheet api
  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURI* aURL);
  NS_IMETHOD GetLinkColor(nscolor& aColor);
  NS_IMETHOD GetActiveLinkColor(nscolor& aColor);
  NS_IMETHOD GetVisitedLinkColor(nscolor& aColor);
  NS_IMETHOD GetDocumentForegroundColor(nscolor& aColor);
  NS_IMETHOD GetDocumentBackgroundColor(nscolor& aColor);
  NS_IMETHOD SetLinkColor(nscolor aColor);
  NS_IMETHOD SetActiveLinkColor(nscolor aColor);
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor);
  NS_IMETHOD SetDocumentForegroundColor(nscolor aColor);
  NS_IMETHOD SetDocumentBackgroundColor(nscolor aColor);

  // Attribute management methods, aAttributes is an in/out param
  NS_IMETHOD SetAttributesFor(nsIHTMLContent* aContent, 
                              nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsString& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, nsIHTMLContent* aContent, 
                               nsIHTMLAttributes*& aAttributes);

  // Mapped Attribute management methods
  NS_IMETHOD UniqueMappedAttributes(nsIHTMLMappedAttributes* aMapped,
                                    nsIHTMLMappedAttributes*& aUniqueMapped);
  NS_IMETHOD DropMappedAttributes(nsIHTMLMappedAttributes* aMapped);

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

  nsIURI*              mURL;
  nsIDocument*         mDocument;
  HTMLColorRule*       mLinkRule;
  HTMLColorRule*       mVisitedRule;
  HTMLColorRule*       mActiveRule;
  HTMLDocumentColorRule* mDocumentColorRule;
  TableBackgroundRule* mTableBackgroundRule;
  nsHashtable          mMappedAttrTable;
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
      ::operator delete(ptr);
    }
  }
}



HTMLStyleSheetImpl::HTMLStyleSheetImpl(void)
  : nsIHTMLStyleSheet(),
    mURL(nsnull),
    mDocument(nsnull),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull),
    mDocumentColorRule(nsnull)
{
  NS_INIT_REFCNT();
  mTableBackgroundRule = new TableBackgroundRule(this);
  NS_ADDREF(mTableBackgroundRule);
}

PRBool MappedDropSheet(nsHashKey *aKey, void *aData, void* closure)
{
  nsIHTMLMappedAttributes* mapped = (nsIHTMLMappedAttributes*)aData;
  mapped->DropStyleSheetReference();
  return PR_TRUE;
}

HTMLStyleSheetImpl::~HTMLStyleSheetImpl()
{
  NS_RELEASE(mURL);
  if (nsnull != mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (nsnull != mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (nsnull != mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  if (nsnull != mDocumentColorRule) {
    mDocumentColorRule->mSheet = nsnull;
    NS_RELEASE(mDocumentColorRule);
  }
  if (nsnull != mTableBackgroundRule) {
    mTableBackgroundRule->mSheet = nsnull;
    NS_RELEASE(mTableBackgroundRule);
  }
  mMappedAttrTable.Enumerate(MappedDropSheet);
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
    // XXX this breaks XPCOM rules since it isn't a proper delegate
    // This is a temporary method of connecting the constructor for now
    nsCSSFrameConstructor* constructor = new nsCSSFrameConstructor();
    if (nsnull != constructor) {
      constructor->Init(mDocument);
      nsresult result = constructor->QueryInterface(aIID, aInstancePtrResult);
      if (NS_FAILED(result)) {
        delete constructor;
      }
      return result;
    }
    return NS_ERROR_OUT_OF_MEMORY;
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

  nsIStyledContent* styledContent;
  if (NS_SUCCEEDED(aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent))) {
    PRInt32 nameSpace;
    styledContent->GetNameSpaceID(nameSpace);
    if (kNameSpaceID_HTML == nameSpace) {
      nsIAtom*  tag;
      styledContent->GetTag(tag);
      // if we have anchor colors, check if this is an anchor with an href
      if (tag == nsHTMLAtoms::a) {
        if ((nsnull != mLinkRule) || (nsnull != mVisitedRule)) {
          // test link state
          nsILinkHandler* linkHandler;

          if (NS_OK == aPresContext->GetLinkHandler(&linkHandler)) {
            nsAutoString base, href;
            nsresult attrState = styledContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, href);

            if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {

              if (! linkHandler) {
                if (nsnull != mLinkRule) {
                  aResults->AppendElement(mLinkRule);
                  matchCount++;
                }
              }
              else {
                nsIURI* docURL = nsnull;

                nsIHTMLContent* htmlContent;
                if (NS_SUCCEEDED(styledContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
                  htmlContent->GetBaseURL(docURL);
               
                  nsAutoString absURLSpec;
                  nsresult rv;
#ifndef NECKO
                  rv = NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
#else
                  nsIURI *baseUri = nsnull;
                  rv = docURL->QueryInterface(nsIURI::GetIID(), (void**)&baseUri);
                  if (NS_FAILED(rv)) return 0;

                  rv = NS_MakeAbsoluteURI(href, baseUri, absURLSpec);

                  NS_RELEASE(baseUri);
#endif // NECKO
                  NS_IF_RELEASE(docURL);

                  nsLinkState  state;
                  if (NS_OK == linkHandler->GetLinkState(absURLSpec.GetUnicode(), state)) {
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
                      default:
                        break;
                    }
                  }
                  NS_RELEASE(htmlContent);
                }
              }
            }
          }
          NS_IF_RELEASE(linkHandler);
        } // end link/visited rules
        if (nsnull != mActiveRule) {  // test active state of link
          nsIEventStateManager* eventStateManager;

          if ((NS_OK == aPresContext->GetEventStateManager(&eventStateManager)) &&
              (nsnull != eventStateManager)) {
            PRInt32 state;
            if (NS_OK == eventStateManager->GetContentState(aContent, state)) {
              if (0 != (state & NS_EVENT_STATE_ACTIVE)) {
                aResults->AppendElement(mActiveRule);
                matchCount++;
              }
            }
            NS_RELEASE(eventStateManager);
          }
        } // end active rule
      } // end A tag
      // add the quirks background compatibility rule if in quirks mode
      else if ((tag == nsHTMLAtoms::td) ||
               (tag == nsHTMLAtoms::th) ||
               (tag == nsHTMLAtoms::tr) ||
               (tag == nsHTMLAtoms::thead) || // Nav4.X doesn't support row groups, but it is a lot
               (tag == nsHTMLAtoms::tbody) || // easier passing from the table to rows this way
               (tag == nsHTMLAtoms::tfoot)) {
        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode) {
          if (mDocumentColorRule) {
            aResults->AppendElement(mDocumentColorRule);
            matchCount++;
          }
          aResults->AppendElement(mTableBackgroundRule);
          matchCount++;
        }
      }
      else if (tag == nsHTMLAtoms::html) {
        if (mDocumentColorRule) {
          aResults->AppendElement(mDocumentColorRule);
          matchCount++;
        }
      }
      NS_IF_RELEASE(tag);
    } // end html namespace

    // just get the one and only style rule from the content
    PRUint32 preCount = 0;
    PRUint32 postCount = 0;
    aResults->Count(&preCount);
    styledContent->GetContentStyleRules(aResults);
    aResults->Count(&postCount);
    matchCount += (postCount - preCount);

    NS_RELEASE(styledContent);
  }

  return matchCount;
}

// Test if style is dependent on content state
NS_IMETHODIMP
HTMLStyleSheetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                           nsIContent*     aContent)
{
  nsresult result = NS_COMFALSE;

  if (mActiveRule || mLinkRule || mVisitedRule) { // do we have any rules dependent on state?
    nsIStyledContent* styledContent;
    if (NS_SUCCEEDED(aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent))) {
      PRInt32 nameSpace;
      styledContent->GetNameSpaceID(nameSpace);
      if (kNameSpaceID_HTML == nameSpace) {
        nsIAtom*  tag;
        styledContent->GetTag(tag);
        // if we have anchor colors, check if this is an anchor with an href
        if (tag == nsHTMLAtoms::a) {
          nsAutoString href;
          nsresult attrState = styledContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, href);
          if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
            result = NS_OK; // yes, style will depend on link state
          }
        }
        NS_IF_RELEASE(tag);
      }
      NS_RELEASE(styledContent);
    }
  }
  return result;
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


  // nsIStyleSheet api
NS_IMETHODIMP
HTMLStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetType(nsString& aType) const
{
  aType.Truncate();
  aType.Append("text/html");
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  return NS_OK; // works for all media
}


NS_IMETHODIMP
HTMLStyleSheetImpl::GetEnabled(PRBool& aEnabled) const
{
  aEnabled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  NS_IF_ADDREF(mDocument);
  aDocument = mDocument;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::Init(nsIURI* aURL, nsIDocument* aDocument)
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

NS_IMETHODIMP HTMLStyleSheetImpl::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);
  
  if (nsnull != mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (nsnull != mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (nsnull != mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  if (mDocumentColorRule) {
    mDocumentColorRule->mSheet = nsnull;
    NS_RELEASE(mDocumentColorRule);
  }
  if (mTableBackgroundRule) {
    mTableBackgroundRule->mSheet = nsnull;
    NS_RELEASE(mTableBackgroundRule);
  }

  mMappedAttrTable.Enumerate(MappedDropSheet);
  mMappedAttrTable.Reset();

  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetLinkColor(nscolor& aColor)
{
  if (nsnull == mLinkRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mLinkRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetActiveLinkColor(nscolor& aColor)
{
  if (nsnull == mActiveRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mActiveRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetVisitedLinkColor(nscolor& aColor)
{
  if (nsnull == mVisitedRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mVisitedRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetDocumentForegroundColor(nscolor& aColor)
{
  if ((nsnull == mDocumentColorRule) ||
      !mDocumentColorRule->mForegroundSet) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mDocumentColorRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetDocumentBackgroundColor(nscolor& aColor)
{
  if ((nsnull == mDocumentColorRule) ||
      !mDocumentColorRule->mBackgroundSet) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mDocumentColorRule->mBackgroundColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetLinkColor(nscolor aColor)
{
  if (nsnull == mLinkRule) {
    mLinkRule = new HTMLColorRule(this);
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
    mActiveRule = new HTMLColorRule(this);
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
    mVisitedRule = new HTMLColorRule(this);
    if (nsnull == mVisitedRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mVisitedRule);
  }
  mVisitedRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetDocumentForegroundColor(nscolor aColor)
{
  if (nsnull == mDocumentColorRule) {
    mDocumentColorRule = new HTMLDocumentColorRule(this);
    if (nsnull == mDocumentColorRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mDocumentColorRule);
  }
  mDocumentColorRule->mColor = aColor;
  mDocumentColorRule->mForegroundSet = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetDocumentBackgroundColor(nscolor aColor)
{
  if (nsnull == mDocumentColorRule) {
    mDocumentColorRule = new HTMLDocumentColorRule(this);
    if (nsnull == mDocumentColorRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mDocumentColorRule);
  }
  mDocumentColorRule->mBackgroundColor = aColor;
  mDocumentColorRule->mBackgroundSet = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributesFor(nsIHTMLContent* aContent, nsIHTMLAttributes*& aAttributes)
{
  if (aAttributes) {
    aAttributes->SetStyleSheet(this);
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsString& aValue,
                                                  PRBool aMappedToStyle,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if (! aAttributes) {
    result = NS_NewHTMLAttributes(&aAttributes);
  }
  if (aAttributes) {
    result = aAttributes->SetAttributeFor(aAttribute, aValue, aMappedToStyle, 
                                          aContent, this);
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsHTMLValue& aValue,
                                                  PRBool aMappedToStyle,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if ((! aAttributes) && (eHTMLUnit_Null != aValue.GetUnit())) {
    result = NS_NewHTMLAttributes(&aAttributes);
  }
  if (aAttributes) {
    PRInt32 count;
    result = aAttributes->SetAttributeFor(aAttribute, aValue, aMappedToStyle,
                                          aContent, this, count);
    if (0 == count) {
      NS_RELEASE(aAttributes);
    }
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::UnsetAttributeFor(nsIAtom* aAttribute,
                                                    nsIHTMLContent* aContent, 
                                                    nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if (aAttributes) {
    PRInt32 count;
    result = aAttributes->UnsetAttributeFor(aAttribute, aContent, this, count);
    if (0 == count) {
      NS_RELEASE(aAttributes);
    }
  }

  return result;

}

NS_IMETHODIMP
HTMLStyleSheetImpl::UniqueMappedAttributes(nsIHTMLMappedAttributes* aMapped,
                                           nsIHTMLMappedAttributes*& aUniqueMapped)
{
  nsresult result = NS_OK;

  AttributeKey  key(aMapped);
  nsIHTMLMappedAttributes* sharedAttrs = (nsIHTMLMappedAttributes*)mMappedAttrTable.Get(&key);
  if (nsnull == sharedAttrs) {  // we have a new unique set
    mMappedAttrTable.Put(&key, aMapped);
    aMapped->SetUniqued(PR_TRUE);
    NS_ADDREF(aMapped);
    aUniqueMapped = aMapped;
  }
  else {  // found existing set
    aUniqueMapped = sharedAttrs;
    NS_ADDREF(aUniqueMapped);
  }
  return result;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::DropMappedAttributes(nsIHTMLMappedAttributes* aMapped)
{
  if (aMapped) {
    PRBool inTable = PR_FALSE;
    aMapped->GetUniqued(inTable);
    if (inTable) {
      AttributeKey  key(aMapped);
      nsIHTMLMappedAttributes* old = (nsIHTMLMappedAttributes*)mMappedAttrTable.Remove(&key);
      NS_ASSERTION(old == aMapped, "not in table");
      aMapped->SetUniqued(PR_FALSE);
    }
  }
  return NS_OK;
}

void HTMLStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
#ifdef NECKO
  char* urlSpec = nsnull;
  mURL->GetSpec(&urlSpec);
  if (urlSpec) {
    fputs(urlSpec, out);
    nsCRT::free(urlSpec);
  }
#else
  PRUnichar* urlSpec = nsnull;
  mURL->ToString(&urlSpec);
  if (urlSpec) {
    nsAutoString buffer(urlSpec);
    fputs(buffer, out);
    delete [] urlSpec;
  }
#endif
  fputs("\n", out);
}


// XXX For convenience and backwards compatibility
NS_HTML nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument)
{
  nsresult rv;
  nsIHTMLStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}


NS_HTML nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLStyleSheetImpl  *it = new HTMLStyleSheetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
}
