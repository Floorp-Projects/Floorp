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
#include "nsHTMLIIDs.h"
#include "nsIStyleFrameConstruction.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsStyleConsts.h"
#include "nsTableOuterFrame.h"
#include "nsIXMLDocument.h"
#include "nsIWebShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIComboboxControlFrame.h"
#include "nsIListControlFrame.h"

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsTreeFrame.h"
#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h"
#include "nsTreeIndentationFrame.h"
#endif

//#define FRAMEBASED_COMPONENTS 1 // This is temporary please leave in for now - rods

static NS_DEFINE_IID(kIHTMLStyleSheetIID, NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_ISTYLE_SHEET_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID, NS_ICOMBOBOXCONTROLFRAME_IID);
static NS_DEFINE_IID(kIListControlFrameIID,     NS_ILISTCONTROLFRAME_IID);

class HTMLAnchorRule : public nsIStyleRule {
public:
  HTMLAnchorRule(nsIHTMLStyleSheet* aSheet);
  ~HTMLAnchorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nscolor             mColor;
  nsIHTMLStyleSheet*  mSheet;
};

HTMLAnchorRule::HTMLAnchorRule(nsIHTMLStyleSheet* aSheet)
  : mSheet(aSheet)
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
HTMLAnchorRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
HTMLAnchorRule::GetStrength(PRInt32& aStrength) const
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

// Structure used when constructing formatting object trees.
struct nsFrameItems {
  nsIFrame* childList;
  nsIFrame* lastChild;
  
  nsFrameItems();

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

nsFrameItems::nsFrameItems()
:childList(nsnull), lastChild(nsnull)
{}

void 
nsFrameItems::AddChild(nsIFrame* aChild)
{
  if (childList == nsnull) {
    childList = lastChild = aChild;
  }
  else
  {
    lastChild->SetNextSibling(aChild);
    lastChild = aChild;
  }
}

// -----------------------------------------------------------

// Structure used when constructing formatting object trees. Contains
// state information needed for absolutely positioned elements
struct nsAbsoluteItems {
  nsIFrame* const containingBlock;  // containing block for absolutely positioned elements
  nsIFrame*       childList;        // list of absolutely positioned child frames

  nsAbsoluteItems(nsIFrame* aContainingBlock);

  // Adds a child frame to the list of absolutely positioned child frames
  void  AddAbsolutelyPositionedChild(nsIFrame* aChildFrame);
};

nsAbsoluteItems::nsAbsoluteItems(nsIFrame* aContainingBlock)
  : containingBlock(aContainingBlock)
{
  childList = nsnull;
}

void
nsAbsoluteItems::AddAbsolutelyPositionedChild(nsIFrame* aChildFrame)
{
#ifdef NS_DEBUG
  nsIFrame* parent;
  aChildFrame->GetParent(parent);
  NS_PRECONDITION(parent == containingBlock, "bad geometric parent");
#endif

  if (nsnull == childList) {
    childList = aChildFrame;
  } else {
    // Get the last frane in the list
    nsIFrame* lastChild = nsnull;

    for (nsIFrame* f = childList; nsnull != f; f->GetNextSibling(f)) {
      lastChild = f;
    }

    lastChild->SetNextSibling(aChildFrame);
  }
}

// -----------------------------------------------------------

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet,
                           public nsIStyleFrameConstruction {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLStyleSheetImpl(void);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIStyleSheet api
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

  NS_IMETHOD Init(nsIURL* aURL, nsIDocument* aDocument);
  NS_IMETHOD SetLinkColor(nscolor aColor);
  NS_IMETHOD SetActiveLinkColor(nscolor aColor);
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor);

  // Attribute management methods, aAttributes is an in/out param
  NS_IMETHOD SetAttributesFor(nsIHTMLContent* aContent, 
                              nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsString& aValue, 
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue, 
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, nsIHTMLContent* aContent, 
                               nsIHTMLAttributes*& aAttributes);

  NS_IMETHOD ConstructRootFrame(nsIPresContext* aPresContext,
                                nsIContent*     aDocElement,
                                nsIFrame*&      aNewFrame);

  NS_IMETHODIMP ReconstructFrames(nsIPresContext* aPresContext,
                                  nsIContent*     aContent,
                                  nsIFrame*       aParentFrame,
                                  nsIFrame*       aFrameSubTree);

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

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint); // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

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

  nsresult ConstructFrame(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParentFrame,
                          nsAbsoluteItems& aAbsoluteItems,
                          nsFrameItems&    aFrameItems);

  nsresult ConstructDocElementFrame(nsIPresContext*  aPresContext,
                                    nsIContent*      aDocElement,
                                    nsIFrame*        aRootFrame,
                                    nsIStyleContext* aRootStyleContext,
                                    nsIFrame*&       aNewFrame);

  nsresult ConstructTableFrame(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aStyleContext,
                               nsAbsoluteItems& aAboluteItems,
                               nsIFrame*&       aNewFrame);

  nsresult ConstructTableRowGroupFrame(nsIPresContext*  aPresContext,
                                       nsIContent*      aContent,
                                       nsIFrame*        aParent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aNewScrollFrame,
                                       nsIFrame*&       aNewFrame);

  nsresult ConstructTableCellFrame(nsIPresContext*  aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame*        aParentFrame,
                                   nsIStyleContext* aStyleContext,
                                   nsAbsoluteItems& aAbsoluteItems,
                                   nsIFrame*&       aNewFrame);

  nsresult CreatePlaceholderFrameFor(nsIPresContext*  aPresContext,
                                     nsIContent*      aContent,
                                     nsIFrame*        aFrame,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*        aParentFrame,
                                     nsIFrame*&       aPlaceholderFrame);

  nsresult ConstructSelectFrame(nsIPresContext*  aPresContext,
                                nsIContent*      aContent,
                                nsIFrame*        aParentFrame,
                                nsIAtom*         aTag,
                                nsIStyleContext* aStyleContext,
                                nsAbsoluteItems& aAbsoluteItems,
                                nsIFrame*&       aNewFrame,
                                PRBool &         aProcessChildren,
                                PRBool &         aIsAbsolutelyPositioned,
                                PRBool &         aFrameHasBeenInitialized);

  nsresult ConstructFrameByTag(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParentFrame,
                               nsIAtom*         aTag,
                               nsIStyleContext* aStyleContext,
                               nsAbsoluteItems& aAbsoluteItems,
                               nsFrameItems&  aFrameItems);

#ifdef INCLUDE_XUL
  nsresult ConstructXULFrame(nsIPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParentFrame,
                             nsIAtom*         aTag,
                             nsIStyleContext* aStyleContext,
                             nsAbsoluteItems& aAbsoluteItems,
                             nsFrameItems&    aFrameItems,
               PRBool&      haltProcessing);

  nsresult ConstructTreeFrame(nsIPresContext*   aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aStyleContext,
                               nsAbsoluteItems& aAboluteItems,
                               nsIFrame*&       aNewFrame);

  nsresult ConstructTreeBodyFrame(nsIPresContext*       aPresContext,
                                       nsIContent*      aContent,
                                       nsIFrame*        aParent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aNewScrollFrame,
                                       nsIFrame*&       aNewFrame);

  nsresult ConstructTreeCellFrame(nsIPresContext*   aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame*        aParentFrame,
                                   nsIStyleContext* aStyleContext,
                                   nsAbsoluteItems& aAbsoluteItems,
                                   nsIFrame*&       aNewFrame);
#endif

  nsresult ConstructFrameByDisplayType(nsIPresContext*       aPresContext,
                                       const nsStyleDisplay* aDisplay,
                                       nsIContent*           aContent,
                                       nsIFrame*             aParentFrame,
                                       nsIStyleContext*      aStyleContext,
                                       nsAbsoluteItems&      aAbsoluteItems,
                                       nsFrameItems&         aFrameItems);

  nsresult GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                  PRUint8    aChildDisplayType,
                                  nsIFrame*& aNewParentFrame);


  nsresult ProcessChildren(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aFrame,
                           nsAbsoluteItems& aAbsoluteItems,
                           nsFrameItems&    aFrameItems);

  nsresult CreateInputFrame(nsIContent* aContent, nsIFrame*& aFrame);

  PRBool IsScrollable(nsIPresContext* aPresContext, const nsStyleDisplay* aDisplay);

  nsIFrame* GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                        nsIContent* aContent);

  nsIFrame* GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);

  nsresult InitializeScrollFrame(nsIFrame *            aScrollFrame,
                                 nsIPresContext*       aPresContext,
                                 nsIContent*           aContent,
                                 nsIFrame*             aParentFrame,
                                 nsIStyleContext*      aStyleContext,
                                 nsAbsoluteItems&      aAbsoluteItems,
                                 nsIFrame*&            aNewFrame,
                                 PRBool                isAbsolutelyPositioned,
                                 PRBool                aCreateBlock);
protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsIURL*             mURL;
  nsIDocument*        mDocument;
  HTMLAnchorRule*     mLinkRule;
  HTMLAnchorRule*     mVisitedRule;
  HTMLAnchorRule*     mActiveRule;
  nsHashtable         mAttrTable;
  nsIHTMLAttributes*  mRecycledAttrs;
  nsIFrame*           mInitialContainingBlock;
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



HTMLStyleSheetImpl::HTMLStyleSheetImpl(void)
  : nsIHTMLStyleSheet(),
    mURL(nsnull),
    mDocument(nsnull),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull),
    mRecycledAttrs(nsnull),
    mInitialContainingBlock(nsnull)
{
  NS_INIT_REFCNT();
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
          nsAutoString base, href;
          nsresult attrState = htmlContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);

          if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
            nsIURL* docURL = nsnull;
            htmlContent->GetBaseURL(docURL);

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
    htmlContent->GetContentStyleRule(rule);
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


  // nsIStyleSheet api
NS_IMETHODIMP
HTMLStyleSheetImpl::GetURL(nsIURL*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  aTitle.Append("Internal HTML Style Sheet");
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

NS_IMETHODIMP HTMLStyleSheetImpl::Init(nsIURL* aURL, nsIDocument* aDocument)
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

NS_IMETHODIMP HTMLStyleSheetImpl::SetLinkColor(nscolor aColor)
{
  if (nsnull == mLinkRule) {
    mLinkRule = new HTMLAnchorRule(this);
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
    mActiveRule = new HTMLAnchorRule(this);
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
    mVisitedRule = new HTMLAnchorRule(this);
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

#ifdef NS_DEBUG
#define NS_ASSERT_REFCOUNT(ptr,cnt,msg) { \
  nsrefcnt  count = ptr->AddRef();        \
  ptr->Release();                         \
  NS_ASSERTION(--count == cnt, msg);      \
}
#else
#define NS_ASSERT_REFCOUNT(ptr,cnt,msg) {}
#endif

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
        NS_ASSERT_REFCOUNT(mRecycledAttrs, 1, "attributes used elsewhere");
        aSingleAttrs = mRecycledAttrs;
        mRecycledAttrs = nsnull;
        aSingleAttrs->SetMappingFunction(aMapFunc);
      }
      else {
        result = NS_NewHTMLAttributes(&aSingleAttrs, this, aMapFunc);
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

/**
 * Request to process the child content elements and create frames.
 *
 * @param   aContent the content object whose child elements to process
 * @param   aFrame the the associated with aContent. This will be the
 *            parent frame (both content and geometric) for the flowed
 *            child frames
 * @param   aState the state information associated with the current process
 * @param   aChildList an OUT parameter. This is the list of flowed child
 *            frames that should go in the principal child list
 * @param aLastChildInList contains a pointer to the last child in aChildList
 */
nsresult
HTMLStyleSheetImpl::ProcessChildren(nsIPresContext*  aPresContext,
                                    nsIContent*      aContent,
                                    nsIFrame*        aFrame,
                                    nsAbsoluteItems& aAbsoluteItems,
                                    nsFrameItems&    aFrameItems)
{
  // Iterate the child content objects and construct a frame
  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;

  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsIContent* childContent;
    aContent->ChildAt(i, childContent);

    if (nsnull != childContent) {
      // Construct a child frame
      ConstructFrame(aPresContext, childContent, aFrame, aAbsoluteItems, aFrameItems);
      NS_RELEASE(childContent);
    }
  }

  return NS_OK;
}

nsresult
HTMLStyleSheetImpl::CreateInputFrame(nsIContent* aContent, nsIFrame*& aFrame)
{
  nsresult  rv;

  // Figure out which type of input frame to create
  nsAutoString  val;
  if (NS_OK == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, val)) {
    if (val.EqualsIgnoreCase("submit")) {
      rv = NS_NewButtonControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("reset")) {
      rv = NS_NewButtonControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("button")) {
      rv = NS_NewButtonControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("checkbox")) {
      rv = NS_NewCheckboxControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("file")) {
      rv = NS_NewFileControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("hidden")) {
      rv = NS_NewButtonControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("image")) {
      rv = NS_NewImageControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("password")) {
      rv = NS_NewTextControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("radio")) {
      rv = NS_NewRadioControlFrame(aFrame);
    }
    else if (val.EqualsIgnoreCase("text")) {
      rv = NS_NewTextControlFrame(aFrame);
    }
    else {
      rv = NS_NewTextControlFrame(aFrame);
    }
  } else {
    rv = NS_NewTextControlFrame(aFrame);
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructTableRowGroupFrame(nsIPresContext*  aPresContext,
                                                nsIContent*      aContent,
                                                nsIFrame*        aParent,
                                                nsIStyleContext* aStyleContext,
                                                nsIFrame*&       aNewScrollFrame,
                                                nsIFrame*&       aNewFrame)
{
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);

  if (IsScrollable(aPresContext, styleDisplay)) {
    // Create a scroll frame
    NS_NewScrollFrame(aNewScrollFrame);
 

    // Initialize it
    aNewScrollFrame->Init(*aPresContext, aContent, aParent, aStyleContext);

    // The scroll frame gets the original style context, and the scrolled
    // frame gets a SCROLLED-CONTENT pseudo element style context that
    // inherits the background properties
    nsIStyleContext*  scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                        (aContent, nsHTMLAtoms::scrolledContentPseudo, aStyleContext);

    // Create an area container for the frame
    NS_NewTableRowGroupFrame(aNewFrame);

    // Initialize the frame and force it to have a view
    aNewFrame->Init(*aPresContext, aContent, aNewScrollFrame, scrolledPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                             scrolledPseudoStyle, PR_TRUE);
    NS_RELEASE(scrolledPseudoStyle);

    aNewScrollFrame->SetInitialChildList(*aPresContext, nsnull, aNewFrame);
  } else {
    NS_NewTableRowGroupFrame(aNewFrame);
    aNewFrame->Init(*aPresContext, aContent, aParent, aStyleContext);
    aNewScrollFrame = nsnull;
  }

  return NS_OK;
}

nsresult
HTMLStyleSheetImpl::ConstructTableFrame(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        nsIFrame*&       aNewFrame)
{
  nsIFrame* childList;
  nsIFrame* innerFrame;
  nsIFrame* innerChildList = nsnull;
  nsIFrame* captionFrame = nsnull;

  // Create an anonymous table outer frame which holds the caption and the
  // table frame
  NS_NewTableOuterFrame(aNewFrame);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned
  aNewFrame->Init(*aPresContext, aContent, aParent, aStyleContext);
  nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                           aStyleContext, PR_FALSE);

  // Create the inner table frame
  NS_NewTableFrame(innerFrame);
  childList = innerFrame;

  // Have the inner table frame use a pseudo style context based on the outer table frame's
  /* XXX: comment this back in and use this as the inner table's p-style asap
  nsIStyleContext *innerTableStyleContext = 
    aPresContext->ResolvePseudoStyleContextFor (aContent, 
                                                nsHTMLAtoms::tablePseudo,
                                                aStyleContext);
  */
  innerFrame->Init(*aPresContext, aContent, aNewFrame, aStyleContext);
  // this should be "innerTableStyleContext" but I haven't tested that thoroughly yet

  // Iterate the child content
  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsIFrame* grandChildList=nsnull;  // to be used only when pseudoframes need to be created
    nsIContent* childContent;
    aContent->ChildAt(i, childContent);

    if (nsnull != childContent) {
      nsIFrame*         frame       = nsnull;
      nsIFrame*         scrollFrame = nsnull;
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
          NS_NewAreaFrame(captionFrame, 0);
          captionFrame->Init(*aPresContext, childContent, aNewFrame, childStyleContext);
          // Process the caption's child content and set the initial child list
          nsFrameItems captionChildItems;
          ProcessChildren(aPresContext, childContent, captionFrame,
                          aAbsoluteItems, captionChildItems);
          captionFrame->SetInitialChildList(*aPresContext, nsnull, captionChildItems.childList);

          // Prepend the caption frame to the outer frame's child list
          innerFrame->SetNextSibling(captionFrame);
        }
        break;

      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
        ConstructTableRowGroupFrame(aPresContext, childContent, innerFrame, 
                                    childStyleContext, scrollFrame, frame);
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
        NS_NewTableRowGroupFrame(frame);
        NS_RELEASE(childStyleContext); // we can't use this resolved style, so get rid of it
        childStyleContext = rowGroupStyleContext;  // the row group style context will get set to childStyleContext after the switch ends.
        frame->Init(*aPresContext, childContent, innerFrame, childStyleContext);
        // need to resolve the style context for the column again to be sure it's a child of the colgroup style context
        rowStyleContext = aPresContext->ResolveStyleContextFor(childContent, rowGroupStyleContext);
        nsIFrame *rowFrame;
        NS_NewTableRowFrame(rowFrame);
        rowFrame->Init(*aPresContext, childContent, frame, rowStyleContext); 
        rowFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
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
        NS_NewTableColGroupFrame(frame);
        NS_RELEASE(childStyleContext);  // we can't use this resolved style, so get rid of it
        childStyleContext = colGroupStyleContext;  // the col group style context will get set to childStyleContext after the switch ends.
        frame->Init(*aPresContext, childContent, innerFrame, childStyleContext);
        // need to resolve the style context for the column again to be sure it's a child of the colgroup style context
        colStyleContext = aPresContext->ResolveStyleContextFor(childContent, colGroupStyleContext);
        nsIFrame *colFrame;
        NS_NewTableColFrame(colFrame);
        colFrame->Init(*aPresContext, childContent, frame, colStyleContext); 
        colFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
        grandChildList = colFrame;
        break;
      }

      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
        NS_NewTableColGroupFrame(frame);
        frame->Init(*aPresContext, childContent, innerFrame, childStyleContext);
        break;

      default:
        // For non table related frames (e.g. forms) make them children of the outer table frame
        // XXX also need to deal with things like table cells and create anonymous frames...
        nsFrameItems nonTableRelatedFrameItems;
        nsIAtom*  tag;
        childContent->GetTag(tag);
        ConstructFrameByTag(aPresContext, childContent, aNewFrame, tag, childStyleContext,
                            aAbsoluteItems, nonTableRelatedFrameItems);
        childList->SetNextSibling(nonTableRelatedFrameItems.childList);
        NS_IF_RELEASE(tag);
       break;
      }

      // If it's not a caption frame, then link the frame into the inner
      // frame's child list
      if (nsnull != frame) {
        // Process the children, and set the frame's initial child list
        nsFrameItems childChildItems;
        if (nsnull==grandChildList) {
          ProcessChildren(aPresContext, childContent, frame, aAbsoluteItems,
                          childChildItems);
          grandChildList = childChildItems.childList;
        } else {
          ProcessChildren(aPresContext, childContent, grandChildList,
                          aAbsoluteItems, childChildItems);
          grandChildList->SetInitialChildList(*aPresContext, nsnull, childChildItems.childList);
        }
        frame->SetInitialChildList(*aPresContext, nsnull, grandChildList);
  
        // Link the frame into the child list
        nsIFrame* outerMostFrame = (nsnull == scrollFrame) ? frame : scrollFrame;
        if (nsnull == lastChildFrame) {
          innerChildList = outerMostFrame;
        } else {
          lastChildFrame->SetNextSibling(outerMostFrame);
        }
        lastChildFrame = outerMostFrame;
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
HTMLStyleSheetImpl::ConstructTableCellFrame(nsIPresContext*  aPresContext,
                                            nsIContent*      aContent,
                                            nsIFrame*        aParentFrame,
                                            nsIStyleContext* aStyleContext,
                                            nsAbsoluteItems& aAbsoluteItems,
                                            nsIFrame*&       aNewFrame)
{
  nsresult  rv;

  // Create a table cell frame
  rv = NS_NewTableCellFrame(aNewFrame);
  if (NS_SUCCEEDED(rv)) {
    // Initialize the table cell frame
    aNewFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

    // Create an area frame that will format the cell's content
    nsIFrame*   cellBodyFrame;

    rv = NS_NewAreaFrame(cellBodyFrame, 0);
    if (NS_FAILED(rv)) {
      aNewFrame->DeleteFrame(*aPresContext);
      aNewFrame = nsnull;
      return rv;
    }
  
    // Resolve pseudo style and initialize the body cell frame
    nsIStyleContext*  bodyPseudoStyle = aPresContext->ResolvePseudoStyleContextFor(aContent,
                                          nsHTMLAtoms::cellContentPseudo, aStyleContext);
    cellBodyFrame->Init(*aPresContext, aContent, aNewFrame, bodyPseudoStyle);
    NS_RELEASE(bodyPseudoStyle);

    // Process children and set the body cell frame's initial child list
    nsFrameItems childItems;
    rv = ProcessChildren(aPresContext, aContent, cellBodyFrame, aAbsoluteItems,
                         childItems);
    if (NS_SUCCEEDED(rv)) {
      cellBodyFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }

    // Set the table cell frame's initial child list
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, cellBodyFrame);
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructDocElementFrame(nsIPresContext*  aPresContext,
                                             nsIContent*      aDocElement,
                                             nsIFrame*        aRootFrame,
                                             nsIStyleContext* aRootStyleContext,
                                             nsIFrame*&       aNewFrame)
{
  // See if we're paginated
  if (aPresContext->IsPaginated()) {
    nsIFrame* pageSequenceFrame;

    // Create a page sequence frame
    NS_NewSimplePageSequenceFrame(pageSequenceFrame);
    pageSequenceFrame->Init(*aPresContext, nsnull, aRootFrame, aRootStyleContext);

    // Create the first page
    nsIFrame* pageFrame;
    NS_NewPageFrame(pageFrame);

    // Initialize the page and force it to have a view. This makes printing of
    // the pages easier and faster.
    // XXX Use a PAGE style context...
    pageFrame->Init(*aPresContext, nsnull, pageSequenceFrame, aRootStyleContext);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, pageFrame,
                                             aRootStyleContext, PR_TRUE);

    // Resolve the style context for the document element
    nsIStyleContext*  styleContext;
    styleContext = aPresContext->ResolveStyleContextFor(aDocElement, aRootStyleContext);

    // Create an area frame for the document element
    nsIFrame* areaFrame;

    NS_NewAreaFrame(areaFrame, 0);
    areaFrame->Init(*aPresContext, aDocElement, pageFrame, styleContext);
    NS_RELEASE(styleContext);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;

    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems, childItems);
    
    // Set the initial child lists
    areaFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }
    pageFrame->SetInitialChildList(*aPresContext, nsnull, areaFrame);
    pageSequenceFrame->SetInitialChildList(*aPresContext, nsnull, pageFrame);

    // Return the page sequence frame as the frame sub-tree
    aNewFrame = pageSequenceFrame;
  
  } else {
    // Resolve the style context for the document element
    nsIStyleContext*  styleContext;
    styleContext = aPresContext->ResolveStyleContextFor(aDocElement, aRootStyleContext);
  
    // Unless the 'overflow' policy forbids scrolling, wrap the frame in a
    // scroll frame.
    nsIFrame* scrollFrame = nsnull;

    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    if (IsScrollable(aPresContext, display)) {
      NS_NewScrollFrame(scrollFrame);
      scrollFrame->Init(*aPresContext, aDocElement, aRootFrame, styleContext);
    
      // The scrolled frame gets a pseudo element style context
      nsIStyleContext*  scrolledPseudoStyle =
        aPresContext->ResolvePseudoStyleContextFor(nsnull,
                                                   nsHTMLAtoms::scrolledContentPseudo,
                                                   styleContext);
      NS_RELEASE(styleContext);
      styleContext = scrolledPseudoStyle;
    }

    // Create an area frame for the document element. This serves as the
    // "initial containing block"
    nsIFrame* areaFrame;

    // XXX Until we clean up how painting damage is handled, we need to use the
    // flag that says that this is the body...
    NS_NewAreaFrame(areaFrame, NS_BLOCK_DOCUMENT_ROOT|NS_BLOCK_MARGIN_ROOT);
    nsIFrame* parentFrame = scrollFrame ? scrollFrame : aRootFrame;
    areaFrame->Init(*aPresContext, aDocElement, parentFrame, styleContext);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, areaFrame,
                                             styleContext, PR_FALSE);
    NS_RELEASE(styleContext);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;
    
    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems, childItems);
    
    // Set the initial child lists
    areaFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }
    if (nsnull != scrollFrame) {
      scrollFrame->SetInitialChildList(*aPresContext, nsnull, areaFrame);
    }

    aNewFrame = scrollFrame ? scrollFrame : areaFrame;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::ConstructRootFrame(nsIPresContext* aPresContext,
                                       nsIContent*     aDocElement,
                                       nsIFrame*&      aNewFrame)
{
#ifdef NS_DEBUG
  nsIDocument*  doc;
  nsIContent*   rootContent;

  // Verify that the content object is really the root content object
  aDocElement->GetDocument(doc);
  rootContent = doc->GetRootContent();
  NS_RELEASE(doc);
  NS_ASSERTION(rootContent == aDocElement, "unexpected content");
  NS_RELEASE(rootContent);
#endif

  nsIFrame*         viewportFrame;
  nsIStyleContext*  rootPseudoStyle;

  // Create the viewport frame
  NS_NewViewportFrame(viewportFrame);

  // Create a pseudo element style context
  // XXX Use a different pseudo style context...
  rootPseudoStyle = aPresContext->ResolvePseudoStyleContextFor(nsnull, 
                      nsHTMLAtoms::rootPseudo, nsnull);

  // Initialize the viewport frame. It has a NULL content object
  viewportFrame->Init(*aPresContext, nsnull, nsnull, rootPseudoStyle);

  // Bind the viewport frame to the root view
  nsIPresShell*   presShell = aPresContext->GetShell();
  nsIViewManager* viewManager = presShell->GetViewManager();
  nsIView*        rootView;

  NS_RELEASE(presShell);
  viewManager->GetRootView(rootView);
  viewportFrame->SetView(rootView);
  NS_RELEASE(viewManager);

  // As long as the webshell doesn't prohibit it, create a scroll frame
  // that will act as the scolling mechanism for the viewport
  // XXX We should only do this when presenting to the screen, i.e., for galley
  // mode and print-preview, but not when printing
  PRBool       isScrollable = PR_TRUE;
  nsISupports* container;
  if (nsnull != aPresContext) {
    aPresContext->GetContainer(&container);
    if (nsnull != container) {
      nsIWebShell* webShell = nsnull;
      container->QueryInterface(kIWebShellIID, (void**) &webShell);
      if (nsnull != webShell) {
        PRInt32 scrolling = -1;
        webShell->GetScrolling(scrolling);
        if (NS_STYLE_OVERFLOW_HIDDEN == scrolling) {
          isScrollable = PR_FALSE;
        }
        NS_RELEASE(webShell);
      }
      NS_RELEASE(container);
    }
  }

  // If the viewport should offer a scrolling mechanism, then create a
  // scroll frame
  nsIFrame* scrollFrame;
  if (isScrollable) {
    NS_NewScrollFrame(scrollFrame);
    scrollFrame->Init(*aPresContext, nsnull, viewportFrame, rootPseudoStyle);
  }

  // Create the root frame. The document element's frame is a child of the
  // root frame.
  //
  // Note: the major reason we need the root frame is to implement margins for
  // the document element's frame. If we didn't need to support margins on the
  // document element's frame, then we could eliminate the root frame and make
  // the document element frame a child of the viewport (or its scroll frame)
  nsIFrame* rootFrame;
  NS_NewRootFrame(rootFrame);

  rootFrame->Init(*aPresContext, nsnull, isScrollable ? scrollFrame :
                  viewportFrame, rootPseudoStyle);
  if (isScrollable) {
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, rootFrame,
                                             rootPseudoStyle, PR_TRUE);
  }

  // Create frames for the document element and its child elements
  nsIFrame* docElementFrame;
  ConstructDocElementFrame(aPresContext, aDocElement, rootFrame,
                           rootPseudoStyle, docElementFrame);
  NS_RELEASE(rootPseudoStyle);

  // Set the initial child lists
  rootFrame->SetInitialChildList(*aPresContext, nsnull, docElementFrame);
  if (isScrollable) {
    scrollFrame->SetInitialChildList(*aPresContext, nsnull, rootFrame);
    viewportFrame->SetInitialChildList(*aPresContext, nsnull, scrollFrame);
  } else {
    viewportFrame->SetInitialChildList(*aPresContext, nsnull, rootFrame);
  }

  aNewFrame = viewportFrame;
  return NS_OK;  
}

nsresult
HTMLStyleSheetImpl::CreatePlaceholderFrameFor(nsIPresContext*  aPresContext,
                                              nsIContent*      aContent,
                                              nsIFrame*        aFrame,
                                              nsIStyleContext* aStyleContext,
                                              nsIFrame*        aParentFrame,
                                              nsIFrame*&       aPlaceholderFrame)
{
  nsIFrame* placeholderFrame;
  nsresult  rv;

  rv = NS_NewEmptyFrame(&placeholderFrame);

  if (NS_SUCCEEDED(rv)) {
    // The placeholder frame gets a pseudo style context
    nsIStyleContext*  placeholderPseudoStyle =
      aPresContext->ResolvePseudoStyleContextFor(aContent,
                      nsHTMLAtoms::placeholderPseudo, aStyleContext);
    placeholderFrame->Init(*aPresContext, aContent, aParentFrame,
                           placeholderPseudoStyle);
    NS_RELEASE(placeholderPseudoStyle);
  
    // Add mapping from absolutely positioned frame to its placeholder frame
    nsIPresShell* presShell = aPresContext->GetShell();
    presShell->SetPlaceholderFrameFor(aFrame, placeholderFrame);
    NS_RELEASE(presShell);
  
    aPlaceholderFrame = placeholderFrame;
  }

  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructSelectFrame(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParentFrame,
                                        nsIAtom*         aTag,
                                        nsIStyleContext* aStyleContext,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        nsIFrame*&       aNewFrame,
                                        PRBool &         aProcessChildren,
                                        PRBool &         aIsAbsolutelyPositioned,
                                        PRBool &         aFrameHasBeenInitialized)
{
#ifdef FRAMEBASED_COMPONENTS
  nsresult rv = NS_OK;
  nsIDOMHTMLSelectElement* select   = nsnull;
  PRBool                   multiple = PR_FALSE;
  nsresult result = aContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
  if (NS_OK == result) {
    result = select->GetMultiple(&multiple); // XXX This is wrong!
    if (!multiple) {
      nsIFrame * comboboxFrame;
      rv = NS_NewComboboxControlFrame(comboboxFrame);
      nsIComboboxControlFrame* comboBox;
      if (NS_OK == comboboxFrame->QueryInterface(kIComboboxControlFrameIID, (void**)&comboBox)) {

        nsIFrame * listFrame;
        rv = NS_NewListControlFrame(listFrame);

        // This is important to do before it is initialized
        // it tells it that it is in "DropDown Mode"
        nsIListControlFrame * listControlFrame;
        if (NS_OK == listFrame->QueryInterface(kIListControlFrameIID, (void**)&listControlFrame)) {
          listControlFrame->SetComboboxFrame(comboboxFrame);
        }

        InitializeScrollFrame(listFrame, aPresContext, aContent, comboboxFrame, aStyleContext,
                              aAbsoluteItems,  aNewFrame, PR_TRUE, PR_TRUE);

        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresContext, aContent, aNewFrame, aStyleContext,
                                  aParentFrame, placeholderFrame);

        // Add the absolutely positioned frame to its containing block's list
        // of child frames
        aAbsoluteItems.AddAbsolutelyPositionedChild(aNewFrame);

        listFrame = aNewFrame;

        // This needs to be done "after" the ListFrame has it's ChildList set
        // because the SetInitChildList intializes the ListBox selection state
        // and this method initializes the ComboBox's selection state
        comboBox->SetDropDown(placeholderFrame, listFrame);

        // Set up the Pseudo Style contents
        nsIStyleContext*  visiblePseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                            (aContent, nsHTMLAtoms::dropDownVisible, aStyleContext);
        nsIStyleContext*  hiddenPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                            (aContent, nsHTMLAtoms::dropDownHidden, aStyleContext);
        nsIStyleContext*  outPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                            (aContent, nsHTMLAtoms::dropDownBtnOut, aStyleContext);
        nsIStyleContext* pressPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                            (aContent, nsHTMLAtoms::dropDownBtnPressed, aStyleContext);

        comboBox->SetDropDownStyleContexts(visiblePseudoStyle, hiddenPseudoStyle);
        comboBox->SetButtonStyleContexts(outPseudoStyle, pressPseudoStyle);

        aProcessChildren = PR_FALSE;
        nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, listFrame,
                                                 aStyleContext, PR_TRUE);
        aNewFrame = comboboxFrame;
      }

    } else {
      nsIFrame * listFrame;
      rv = NS_NewListControlFrame(listFrame);
      aNewFrame = listFrame;
      InitializeScrollFrame(listFrame, aPresContext, aContent, aParentFrame, aStyleContext,
                            aAbsoluteItems,  aNewFrame, aIsAbsolutelyPositioned, PR_TRUE);
      aFrameHasBeenInitialized = PR_TRUE;
    }
    NS_RELEASE(select);
  } else {
    rv = NS_NewSelectControlFrame(aNewFrame);
  }

#else
  nsresult rv = NS_NewSelectControlFrame(aNewFrame);
#endif
  return rv;
}

nsresult
HTMLStyleSheetImpl::ConstructFrameByTag(nsIPresContext*  aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParentFrame,
                                        nsIAtom*         aTag,
                                        nsIStyleContext* aStyleContext,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        nsFrameItems&    aFrameItems)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsresult  rv = NS_OK;

  if (nsLayoutAtoms::textTagName == aTag) {
    rv = NS_NewTextFrame(newFrame);
  }
  else {
    nsIHTMLContent *htmlContent;

    // Ignore the tag if it's not HTML content
    if (NS_SUCCEEDED(aContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent))) {
      NS_RELEASE(htmlContent);
      
      // See if the element is absolutely positioned
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
      }

      // Create a frame based on the tag
      if (nsHTMLAtoms::img == aTag) {
        rv = NS_NewImageFrame(newFrame);
      }
      else if (nsHTMLAtoms::hr == aTag) {
        rv = NS_NewHRFrame(newFrame);
      }
      else if (nsHTMLAtoms::br == aTag) {
        rv = NS_NewBRFrame(newFrame);
      }
      else if (nsHTMLAtoms::wbr == aTag) {
        rv = NS_NewWBRFrame(newFrame);
      }
      else if (nsHTMLAtoms::input == aTag) {
        rv = CreateInputFrame(aContent, newFrame);
      }
      else if (nsHTMLAtoms::textarea == aTag) {
        rv = NS_NewTextControlFrame(newFrame);
      }
      else if (nsHTMLAtoms::select == aTag) {
        rv = ConstructSelectFrame(aPresContext, aContent, aParentFrame,
                                  aTag, aStyleContext, aAbsoluteItems, newFrame, 
                                  processChildren, isAbsolutelyPositioned, frameHasBeenInitialized);
      }
      else if (nsHTMLAtoms::applet == aTag) {
        rv = NS_NewObjectFrame(newFrame);
      }
      else if (nsHTMLAtoms::embed == aTag) {
        rv = NS_NewObjectFrame(newFrame);
      }
      else if (nsHTMLAtoms::fieldset == aTag) {
        rv = NS_NewFieldSetFrame(newFrame);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::legend == aTag) {
        rv = NS_NewLegendFrame(newFrame);
        processChildren = PR_TRUE;
        isAbsolutelyPositioned = PR_FALSE;  // don't absolutely position
      }
      else if (nsHTMLAtoms::object == aTag) {
        rv = NS_NewObjectFrame(newFrame);
        nsIFrame *blockFrame;
        NS_NewBlockFrame(blockFrame, 0);
        blockFrame->Init(*aPresContext, aContent, newFrame, aStyleContext);
        newFrame = blockFrame;
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::form == aTag) {
        rv = NS_NewFormFrame(newFrame);
      }
      else if (nsHTMLAtoms::frameset == aTag) {
        rv = NS_NewHTMLFramesetFrame(newFrame);
        isAbsolutelyPositioned = PR_FALSE;  // don't absolutely position
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        rv = NS_NewHTMLFrameOuterFrame(newFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        rv = NS_NewSpacerFrame(newFrame);
        isAbsolutelyPositioned = PR_FALSE;  // don't absolutely position
      }
      else if (nsHTMLAtoms::button == aTag) {
        rv = NS_NewHTMLButtonControlFrame(newFrame);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::label == aTag) {
        rv = NS_NewLabelFrame(newFrame);
        processChildren = PR_TRUE;
      }
    }
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
    if (!frameHasBeenInitialized) {
      nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                           aParentFrame;
      newFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

      // See if we need to create a view, e.g. the frame is absolutely positioned
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                             childItems);
      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }

    // If the frame is absolutely positioned then create a placeholder frame
    if (isAbsolutelyPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                aParentFrame, placeholderFrame);

      // Add the absolutely positioned frame to its containing block's list
      // of child frames
      aAbsoluteItems.AddAbsolutelyPositionedChild(newFrame);

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);

    } else {
      // Add the newly constructed frame to the flow
      aFrameItems.AddChild(newFrame);
    }
  }

  return rv;
}

#ifdef INCLUDE_XUL
nsresult
HTMLStyleSheetImpl::ConstructXULFrame(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aParentFrame,
                                      nsIAtom*         aTag,
                                      nsIStyleContext* aStyleContext,
                                      nsAbsoluteItems& aAbsoluteItems,
                                      nsFrameItems&    aFrameItems,
                                      PRBool&          haltProcessing)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;

  // Initialize the new frame
  nsIFrame* aNewFrame = nsnull;

  NS_ASSERTION(aTag != nsnull, "null XUL tag");
  if (aTag == nsnull)
    return NS_OK;

  PRInt32 nameSpaceID;
  if (NS_SUCCEEDED(aContent->GetNameSpaceID(nameSpaceID)) &&
      nameSpaceID == nsXULAtoms::nameSpaceID) {
      
    // See if the element is absolutely positioned
    const nsStylePosition* position = (const nsStylePosition*)
      aStyleContext->GetStyleData(eStyleStruct_Position);
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition)
      isAbsolutelyPositioned = PR_TRUE;

    // Create a frame based on the tag
    if (aTag == nsXULAtoms::button)
      rv = NS_NewButtonControlFrame(aNewFrame);
    else if (aTag == nsXULAtoms::checkbox)
      rv = NS_NewCheckboxControlFrame(aNewFrame);
    else if (aTag == nsXULAtoms::radio)
      rv = NS_NewRadioControlFrame(aNewFrame);
    else if (aTag == nsXULAtoms::text)
      rv = NS_NewTextControlFrame(aNewFrame);
  else if (aTag == nsXULAtoms::widget)
    rv = NS_NewObjectFrame(aNewFrame);
  
  // TREE CONSTRUCTION
  // The following code is used to construct a tree view from the XUL content
  // model.  It has to take the hierarchical tree content structure and build a flattened
  // table row frame structure.
  else if (aTag == nsXULAtoms::tree)
  {
    isAbsolutelyPositioned = NS_STYLE_POSITION_ABSOLUTE == position->mPosition;
      nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                           aParentFrame;
      rv = ConstructTreeFrame(aPresContext, aContent, geometricParent, aStyleContext,
                               aAbsoluteItems, aNewFrame);
      // Note: the tree construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned) {
        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresContext, aContent, aNewFrame, aStyleContext,
                                  aParentFrame, placeholderFrame);

        // Add the absolutely positioned frame to its containing block's list
        // of child frames
        aAbsoluteItems.AddAbsolutelyPositionedChild(aNewFrame);

        // Add the placeholder frame to the flow
        aNewFrame = placeholderFrame;
      }
      return rv;
  }
  else if (aTag == nsXULAtoms::treeitem)
  {
    // A tree item is essentially a NO-OP as far as the frame construction code
    // is concerned.  The row for this item will be created (when the TREEROW child
    // tag is encountered).  The children of this node still need to be processed,
    // but this content node needs to be "skipped". 

    rv = ProcessChildren(aPresContext, aContent, aParentFrame, aAbsoluteItems,
                 aFrameItems);
    
    // No more work to do.
    return rv;
  }
  else if (aTag == nsXULAtoms::treechildren)
  {
    // Determine whether or not we need to process the child content of
    // this node. We determine this by checking the OPEN attribute on the parent
    // content node.  The OPEN attribute indicates whether or not the children
    // of this node should be part of the tree view (whether or not the folder
    // is expanded or collapsed).
    nsIContent* pTreeItemNode = nsnull;
    aContent->GetParent(pTreeItemNode);
    if (pTreeItemNode != nsnull)
    {
      nsString attrValue;
      nsIAtom* kOpenAtom = NS_NewAtom("open");
      nsresult result = pTreeItemNode->GetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, attrValue);
      attrValue.ToLowerCase();
      processChildren =  (result == NS_CONTENT_ATTR_NO_VALUE ||
        (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
      NS_RELEASE(kOpenAtom);
      NS_RELEASE(pTreeItemNode);

      // If we do need to process, we have to "skip" this content node, since it
      // doesn't really have any associated display.
      
      if (processChildren)
      {
        rv = ProcessChildren(aPresContext, aContent, aParentFrame, aAbsoluteItems,
                   aFrameItems);
      }
      else haltProcessing = PR_TRUE;
    }

    // No more work to do.
    return rv;
  }
  else if (aTag == nsXULAtoms::treerow)
  {
    // A tree item causes a table row to be constructed that is always
    // slaved to the nearest enclosing table row group (regardless of how
    // deeply nested it is within other tree items).
    rv = NS_NewTableRowFrame(aNewFrame);
    processChildren = PR_TRUE;
  }
  else if (aTag == nsXULAtoms::treecell)
  {
    // We make a tree cell frame and process the children.
    rv = ConstructTreeCellFrame(aPresContext, aContent, aParentFrame,
                                    aStyleContext, aAbsoluteItems, aNewFrame);
    aFrameItems.AddChild(aNewFrame);
    return rv;
  }
  else if (aTag == nsXULAtoms::treeindentation)
  {
    rv = NS_NewTreeIndentationFrame(aNewFrame);
  }
  // End of TREE CONSTRUCTION logic

  // TOOLBAR CONSTRUCTION
  else if (aTag == nsXULAtoms::toolbox) {
    processChildren = PR_TRUE;
    rv = NS_NewToolboxFrame(aNewFrame);
  }
  else if (aTag == nsXULAtoms::toolbar) {
    processChildren = PR_TRUE;
    rv = NS_NewToolbarFrame(aNewFrame);
  }
  // End of TOOLBAR CONSTRUCTION logic
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && aNewFrame != nsnull) {
    nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                         aParentFrame;
    aNewFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                             aStyleContext, PR_FALSE);

  // Add the new frame to our list of frame items.
  aFrameItems.AddChild(aNewFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren)
      rv = ProcessChildren(aPresContext, aContent, aNewFrame, aAbsoluteItems,
                           childItems);

    // Set the frame's initial child list
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  
    // If the frame is absolutely positioned then create a placeholder frame
    if (isAbsolutelyPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresContext, aContent, aNewFrame, aStyleContext,
                                aParentFrame, placeholderFrame);

      // Add the absolutely positioned frame to its containing block's list
      // of child frames
      aAbsoluteItems.AddAbsolutelyPositionedChild(aNewFrame);

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
    }
  }

  return rv;
}


nsresult
HTMLStyleSheetImpl::ConstructTreeFrame(nsIPresContext*   aPresContext,
                                        nsIContent*      aContent,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsAbsoluteItems& aAbsoluteItems,
                                        nsIFrame*&       aNewFrame)
{
  nsIFrame* childList;
  nsIFrame* innerFrame;
  nsIFrame* innerChildList = nsnull;
  nsIFrame* captionFrame = nsnull;

  // Create an anonymous table outer frame which holds the caption and the
  // table frame
  NS_NewTableOuterFrame(aNewFrame);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned
  aNewFrame->Init(*aPresContext, aContent, aParent, aStyleContext);
  nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                           aStyleContext, PR_FALSE);

  // Create the inner table frame
  NS_NewTreeFrame(innerFrame);
  childList = innerFrame;

  // Have the inner table frame use a pseudo style context based on the outer table frame's
  /* XXX: comment this back in and use this as the inner table's p-style asap
  nsIStyleContext *innerTableStyleContext = 
    aPresContext->ResolvePseudoStyleContextFor (aContent, 
                                                nsHTMLAtoms::tablePseudo,
                                                aStyleContext);
  */
  innerFrame->Init(*aPresContext, aContent, aNewFrame, aStyleContext);
  // this should be "innerTableStyleContext" but I haven't tested that thoroughly yet

  // Iterate the child content
  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsIFrame* grandChildList=nsnull;  // to be used only when pseudoframes need to be created
    nsIContent* childContent;
    aContent->ChildAt(i, childContent);

    if (nsnull != childContent) {
      nsIFrame*         frame       = nsnull;
      nsIFrame*         scrollFrame = nsnull;
      nsIStyleContext*  childStyleContext;

      // Resolve the style context
      childStyleContext = aPresContext->ResolveStyleContextFor(childContent, aStyleContext);

    // Get the element's tag
    nsIAtom*  tag;
    childContent->GetTag(tag);
      
    if (tag != nsnull)
    {
      if (tag == nsXULAtoms::treecaption)
      {
      // Have we already created a caption? If so, ignore this caption
      if (nsnull == captionFrame) {
        NS_NewAreaFrame(captionFrame, 0);
        captionFrame->Init(*aPresContext, childContent, aNewFrame, childStyleContext);
        // Process the caption's child content and set the initial child list
        nsFrameItems captionChildItems;
        ProcessChildren(aPresContext, childContent, captionFrame,
                aAbsoluteItems, captionChildItems);
        captionFrame->SetInitialChildList(*aPresContext, nsnull, captionChildItems.childList);

        // Prepend the caption frame to the outer frame's child list
        innerFrame->SetNextSibling(captionFrame);
      }
      }
      else if (tag == nsXULAtoms::treebody ||
             tag == nsXULAtoms::treehead)
      {
      
        ConstructTreeBodyFrame(aPresContext, childContent, innerFrame, 
                                         childStyleContext, scrollFrame, frame);
      }

      // Note: The row and cell cases have been excised, since the tree view's rows must occur
      // inside row groups.  The column case has been excised for now until I decide what the
      // appropriate syntax will be for tree columns.

          else
      {
      // For non table related frames (e.g. forms) make them children of the outer table frame
      // XXX also need to deal with things like table cells and create anonymous frames...
      nsFrameItems nonTableRelatedFrameItems;
      ConstructFrameByTag(aPresContext, childContent, aNewFrame, tag, childStyleContext,
                aAbsoluteItems, nonTableRelatedFrameItems);
      childList->SetNextSibling(nonTableRelatedFrameItems.childList);
      }
    }
    
    NS_IF_RELEASE(tag);
      
      // If it's not a caption frame, then link the frame into the inner
      // frame's child list
      if (nsnull != frame) {
        // Process the children, and set the frame's initial child list
        nsFrameItems childChildItems;
        if (nsnull==grandChildList) {
          ProcessChildren(aPresContext, childContent, frame, aAbsoluteItems,
                          childChildItems);
          grandChildList = childChildItems.childList;
        } else {
          ProcessChildren(aPresContext, childContent, grandChildList,
                          aAbsoluteItems, childChildItems);
          grandChildList->SetInitialChildList(*aPresContext, nsnull, childChildItems.childList);
        }
        frame->SetInitialChildList(*aPresContext, nsnull, grandChildList);
  
        // Link the frame into the child list
        nsIFrame* outerMostFrame = (nsnull == scrollFrame) ? frame : scrollFrame;
        if (nsnull == lastChildFrame) {
          innerChildList = outerMostFrame;
        } else {
          lastChildFrame->SetNextSibling(outerMostFrame);
        }
        lastChildFrame = outerMostFrame;
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
HTMLStyleSheetImpl::ConstructTreeBodyFrame(nsIPresContext*  aPresContext,
                                                nsIContent*      aContent,
                                                nsIFrame*        aParent,
                                                nsIStyleContext* aStyleContext,
                                                nsIFrame*&       aNewScrollFrame,
                                                nsIFrame*&       aNewFrame)
{
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);

  if (IsScrollable(aPresContext, styleDisplay)) {
    // Create a scroll frame
    NS_NewScrollFrame(aNewScrollFrame);
 
    // Initialize it
    aNewScrollFrame->Init(*aPresContext, aContent, aParent, aStyleContext);

    // The scroll frame gets the original style context, and the scrolled
    // frame gets a SCROLLED-CONTENT pseudo element style context that
    // inherits the background properties
    nsIStyleContext*  scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                        (aContent, nsHTMLAtoms::scrolledContentPseudo, aStyleContext);

    // Create an area container for the frame
    NS_NewTableRowGroupFrame(aNewFrame);

    // Initialize the frame and force it to have a view
    aNewFrame->Init(*aPresContext, aContent, aNewScrollFrame, scrolledPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                             scrolledPseudoStyle, PR_TRUE);
    NS_RELEASE(scrolledPseudoStyle);

    aNewScrollFrame->SetInitialChildList(*aPresContext, nsnull, aNewFrame);
  } else {
    NS_NewTableRowGroupFrame(aNewFrame);
    aNewFrame->Init(*aPresContext, aContent, aParent, aStyleContext);
    aNewScrollFrame = nsnull;
  }

  return NS_OK;
}

nsresult
HTMLStyleSheetImpl::ConstructTreeCellFrame(nsIPresContext*  aPresContext,
                                            nsIContent*      aContent,
                                            nsIFrame*        aParentFrame,
                                            nsIStyleContext* aStyleContext,
                                            nsAbsoluteItems& aAbsoluteItems,
                                            nsIFrame*&       aNewFrame)
{
  nsresult  rv;

  // Create a table cell frame
  rv = NS_NewTableCellFrame(aNewFrame);
  if (NS_SUCCEEDED(rv)) {
    // Initialize the table cell frame
    aNewFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

    // Create an area frame that will format the cell's content
    nsIFrame*   cellBodyFrame;

    rv = NS_NewAreaFrame(cellBodyFrame, 0);
    if (NS_FAILED(rv)) {
      aNewFrame->DeleteFrame(*aPresContext);
      aNewFrame = nsnull;
      return rv;
    }
  
    // Resolve pseudo style and initialize the body cell frame
    nsIStyleContext*  bodyPseudoStyle = aPresContext->ResolvePseudoStyleContextFor(aContent,
                                          nsHTMLAtoms::cellContentPseudo, aStyleContext);
    cellBodyFrame->Init(*aPresContext, aContent, aNewFrame, bodyPseudoStyle);
    NS_RELEASE(bodyPseudoStyle);

    // Process children and set the body cell frame's initial child list
    nsFrameItems childItems;
    rv = ProcessChildren(aPresContext, aContent, cellBodyFrame, aAbsoluteItems,
                         childItems);
    if (NS_SUCCEEDED(rv)) {
      cellBodyFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }

    // Set the table cell frame's initial child list
    aNewFrame->SetInitialChildList(*aPresContext, nsnull, cellBodyFrame);
  }

  return rv;
}

#endif

nsresult
HTMLStyleSheetImpl::InitializeScrollFrame(nsIFrame *            scrollFrame,
                                          nsIPresContext*       aPresContext,
                                          nsIContent*           aContent,
                                          nsIFrame*             aParentFrame,
                                          nsIStyleContext*      aStyleContext,
                                          nsAbsoluteItems&      aAbsoluteItems,
                                          nsIFrame*&            aNewFrame,
                                          PRBool                isAbsolutelyPositioned,
                                          PRBool                aCreateBlock)
{
    // Initialize it
    nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                         aParentFrame;
    scrollFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

    // The scroll frame gets the original style context, and the scrolled
    // frame gets a SCROLLED-CONTENT pseudo element style context that
    // inherits the background properties
    nsIStyleContext*  scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                        (aContent, nsHTMLAtoms::scrolledContentPseudo, aStyleContext);

    // Create an area container for the frame
    nsIFrame* scrolledFrame;
    NS_NewAreaFrame(scrolledFrame, NS_BLOCK_SHRINK_WRAP);

    // Initialize the frame and force it to have a view
    scrolledFrame->Init(*aPresContext, aContent, scrollFrame, scrolledPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, scrolledFrame,
                                             scrolledPseudoStyle, PR_TRUE);
    NS_RELEASE(scrolledPseudoStyle);

    // Process children
    if (isAbsolutelyPositioned) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      nsAbsoluteItems  absoluteItems(scrolledFrame);
      nsFrameItems  childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, absoluteItems,
                      childItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
      if (nsnull != absoluteItems.childList) {
        scrolledFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                           absoluteItems.childList);
      }

    } else {
      nsFrameItems childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, aAbsoluteItems,
                      childItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
    scrollFrame->SetInitialChildList(*aPresContext, nsnull, scrolledFrame);
    aNewFrame = scrollFrame;


    return NS_OK;
}

nsresult
HTMLStyleSheetImpl::ConstructFrameByDisplayType(nsIPresContext*       aPresContext,
                                                const nsStyleDisplay* aDisplay,
                                                nsIContent*           aContent,
                                                nsIFrame*             aParentFrame,
                                                nsIStyleContext*      aStyleContext,
                                                nsAbsoluteItems&      aAbsoluteItems,
                                                nsFrameItems&         aFrameItems)
{
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isBlock = aDisplay->IsBlockLevel();
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsresult  rv = NS_OK;

  // Get the position syle info
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);

  // The frame is also a block if it's an inline frame that's floated or
  // absolutely positioned
  if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) &&
      ((NS_STYLE_FLOAT_NONE != aDisplay->mFloats) ||
       (NS_STYLE_POSITION_ABSOLUTE == position->mPosition))) {
    isBlock = PR_TRUE;
  }

  // If the frame is a block-level frame and is scrollable, then wrap it
  // in a scroll frame.
  // XXX Applies to replaced elements, too, but how to tell if the element
  // is replaced?
  // XXX Ignore tables for the time being
  if ((isBlock && (aDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE)) &&
      IsScrollable(aPresContext, aDisplay)) {

    // See if it's absolutely positioned
    isAbsolutelyPositioned = NS_STYLE_POSITION_ABSOLUTE == position->mPosition;

    // Create a scroll frame
    nsIFrame* scrollFrame;
    NS_NewScrollFrame(scrollFrame);

    // Initialize it
    InitializeScrollFrame(scrollFrame, aPresContext, aContent, aParentFrame,  aStyleContext,
                         aAbsoluteItems,  newFrame, isAbsolutelyPositioned, PR_FALSE);

#if 0 // XXX The following "ifdef" could has been moved to the method "InitializeScrollFrame"
    nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                         aParentFrame;
    scrollFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

    // The scroll frame gets the original style context, and the scrolled
    // frame gets a SCROLLED-CONTENT pseudo element style context that
    // inherits the background properties
    nsIStyleContext*  scrolledPseudoStyle = aPresContext->ResolvePseudoStyleContextFor
                        (aContent, nsHTMLAtoms::scrolledContentPseudo, aStyleContext);

    // Create an area container for the frame
    nsIFrame* scrolledFrame;
    NS_NewAreaFrame(scrolledFrame, NS_BLOCK_SHRINK_WRAP);

    // Initialize the frame and force it to have a view
    scrolledFrame->Init(*aPresContext, aContent, scrollFrame, scrolledPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, scrolledFrame,
                                             scrolledPseudoStyle, PR_TRUE);
    NS_RELEASE(scrolledPseudoStyle);

    // Process children
    if (isAbsolutelyPositioned) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      nsAbsoluteItems  absoluteItems(scrolledFrame);
      nsFrameItems     childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, absoluteItems,
                      childItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
      if (nsnull != absoluteItems.childList) {
        scrolledFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                           absoluteItems.childList);
      }

    } else {
      nsFrameItems childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, aAbsoluteItems,
                      childItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
    scrollFrame->SetInitialChildList(*aPresContext, nsnull, scrolledFrame);
    aNewFrame = scrollFrame;
#endif

  // See if the frame is absolutely positioned
  } else if ((NS_STYLE_POSITION_ABSOLUTE == position->mPosition) &&
             ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {

    isAbsolutelyPositioned = PR_TRUE;

    // Create an area frame
    NS_NewAreaFrame(newFrame, 0);
    newFrame->Init(*aPresContext, aContent, aAbsoluteItems.containingBlock,
                   aStyleContext);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. The area frame becomes a container for child
    // frames that are absolutely positioned
    nsAbsoluteItems  absoluteItems(newFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems, childItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      newFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }

  // See if the frame is floated, and it's a block or inline frame
  } else if ((NS_STYLE_FLOAT_NONE != aDisplay->mFloats) &&
             ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {

    // Create an area frame
    NS_NewAreaFrame(newFrame, NS_BLOCK_SHRINK_WRAP);

    // Initialize the frame
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

    // See if we need to create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content
    nsFrameItems childItems;
    ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems, childItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);

  // See if it's a relatively positioned block
  } else if ((NS_STYLE_POSITION_RELATIVE == position->mPosition) &&
             ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay))) {

    // Create an area frame. No space manager, though
    NS_NewAreaFrame(newFrame, NS_AREA_NO_SPACE_MGR);
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. Relatively positioned frames becomes a
    // container for child frames that are positioned
    nsAbsoluteItems  absoluteItems(newFrame);
    nsFrameItems     childItems;
    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems, childItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      newFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                    absoluteItems.childList);
    }

  } else {
    PRBool  processChildren = PR_FALSE;  // whether we should process child content

    // Use the 'display' property to chose a frame type
    switch (aDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
    case NS_STYLE_DISPLAY_RUN_IN:
    case NS_STYLE_DISPLAY_COMPACT:
      rv = NS_NewBlockFrame(newFrame, 0);
      processChildren = PR_TRUE;
      break;
  
    case NS_STYLE_DISPLAY_INLINE:
      rv = NS_NewInlineFrame(newFrame);
      processChildren = PR_TRUE;
      break;
  
    case NS_STYLE_DISPLAY_TABLE:
    {
      isAbsolutelyPositioned = NS_STYLE_POSITION_ABSOLUTE == position->mPosition;
      nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                           aParentFrame;
      rv = ConstructTableFrame(aPresContext, aContent, geometricParent, aStyleContext,
                               aAbsoluteItems, newFrame);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned) {
        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                  aParentFrame, placeholderFrame);

        // Add the absolutely positioned frame to its containing block's list
        // of child frames
        aAbsoluteItems.AddAbsolutelyPositionedChild(newFrame);

        // Add the placeholder frame to the flow
        aFrameItems.AddChild(placeholderFrame);
      
      } else {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      return rv;
    }
  
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
          rv = NS_NewTableRowGroupFrame(newFrame);
          processChildren = PR_TRUE;
        }
      }
      break;
  
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      // XXX We should check for being inside of a table column group...
      rv = NS_NewTableColFrame(newFrame);
      processChildren = PR_TRUE;
      break;
  
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      // XXX We should check for being inside of a table...
      {
        nsIFrame *parentFrame;
        rv = GetAdjustedParentFrame(aParentFrame, aDisplay->mDisplay, parentFrame);
        if (NS_SUCCEEDED(rv))
        {
          rv = NS_NewTableColGroupFrame(newFrame);
          processChildren = PR_TRUE;
        }
      }
      break;
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      // XXX We should check for being inside of a table row group...
      rv = NS_NewTableRowFrame(newFrame);
      processChildren = PR_TRUE;
      break;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      // XXX We should check for being inside of a table row frame...
      rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame,
                                   aStyleContext, aAbsoluteItems, newFrame);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      aFrameItems.AddChild(newFrame);
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
      // XXX We should check for being inside of a table row frame...
      rv = NS_NewAreaFrame(newFrame, 0);
      processChildren = PR_TRUE;
      break;
  
    default:
      // Don't create any frame for content that's not displayed...
      break;
    }

    // If we succeeded in creating a frame then initialize the frame and
    // process children if requested
    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

      // See if we need to create a view, e.g. the frame is absolutely positioned
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                             childItems);
      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
  }

  // If the frame is absolutely positioned then create a placeholder frame
  if (isAbsolutelyPositioned) {
    nsIFrame* placeholderFrame;

    CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                              aParentFrame, placeholderFrame);

    // Add the absolutely positioned frame to its containing block's list
    // of child frames
    aAbsoluteItems.AddAbsolutelyPositionedChild(newFrame);

    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);

  } else if (nsnull != newFrame) {
    // Add the frame we just created to the flowed list
    aFrameItems.AddChild(newFrame);
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
HTMLStyleSheetImpl::IsScrollable(nsIPresContext*       aPresContext,
                                 const nsStyleDisplay* aDisplay)
{
  // For the time being it's scrollable if the overflow property is auto or
  // scroll, regardless of whether the width or height is fixed in size
  if ((NS_STYLE_OVERFLOW_SCROLL == aDisplay->mOverflow) ||
      (NS_STYLE_OVERFLOW_AUTO == aDisplay->mOverflow)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
HTMLStyleSheetImpl::ConstructFrame(nsIPresContext*  aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame*        aParentFrame,
                                   nsAbsoluteItems& aAbsoluteItems,
                                   nsFrameItems&    aFrameItems)
{
  NS_PRECONDITION(nsnull != aParentFrame, "no parent frame");

  nsresult  rv;

  // Get the element's tag
  nsIAtom*  tag;
  aContent->GetTag(tag);

  // Resolve the style context based on the content object and the parent
  // style context
  nsIStyleContext* styleContext;
  nsIStyleContext* parentStyleContext;

  aParentFrame->GetStyleContext(parentStyleContext);
  if (nsLayoutAtoms::textTagName == tag) {
    // Use a special pseudo element style context for text
    nsIContent* parentContent = nsnull;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(parentContent);
    }
    styleContext = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                              nsHTMLAtoms::textPseudo, 
                                                              parentStyleContext);
    rv = (nsnull == styleContext) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    NS_IF_RELEASE(parentContent);
  } else if (nsLayoutAtoms::commentTagName == tag) {
    // Use a special pseudo element style context for comments
    nsIContent* parentContent = nsnull;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(parentContent);
    }
    styleContext = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                              nsHTMLAtoms::commentPseudo, 
                                                              parentStyleContext);
    rv = (nsnull == styleContext) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    NS_IF_RELEASE(parentContent);
    
  } else {
    styleContext = aPresContext->ResolveStyleContextFor(aContent, parentStyleContext);
    rv = (nsnull == styleContext) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }
  NS_IF_RELEASE(parentStyleContext);

  if (NS_SUCCEEDED(rv)) {
    // Pre-check for display "none" - if we find that, don't create
    // any frame at all
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_DISPLAY_NONE != display->mDisplay) {
      nsIFrame* lastChild = aFrameItems.lastChild;

      // Handle specific frame types
      rv = ConstructFrameByTag(aPresContext, aContent, aParentFrame, tag,
                               styleContext, aAbsoluteItems, aFrameItems);

#ifdef INCLUDE_XUL
      // Failing to find a matching HTML frame, try creating a specialized
      // XUL frame. This is temporary, pending planned factoring of this
      // whole process into separate, pluggable steps.
      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                             (lastChild == aFrameItems.lastChild))) {
        PRBool haltProcessing = PR_FALSE;
        rv = ConstructXULFrame(aPresContext, aContent, aParentFrame, tag,
                               styleContext, aAbsoluteItems, aFrameItems, haltProcessing);
        if (haltProcessing) {
          NS_RELEASE(styleContext);
          NS_IF_RELEASE(tag);
          return rv;
        }
      } 
#endif

      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                               (lastChild == aFrameItems.lastChild))) {
        // When there is no explicit frame to create, assume it's a
        // container and let display style dictate the rest
        rv = ConstructFrameByDisplayType(aPresContext, display, aContent, aParentFrame,
                                         styleContext, aAbsoluteItems, aFrameItems);
      }
    }

    NS_RELEASE(styleContext);
  }
  
  NS_IF_RELEASE(tag);
  return rv;
}

// XXX we need aFrameSubTree's prev-sibling in order to properly
// place its replacement!
NS_IMETHODIMP  
HTMLStyleSheetImpl::ReconstructFrames(nsIPresContext* aPresContext,
                                      nsIContent*     aContent,
                                      nsIFrame*       aParentFrame,
                                      nsIFrame*       aFrameSubTree)
{
  nsresult rv = NS_OK;

  nsIDocument* document;
  rv = aContent->GetDocument(document);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  if (nsnull != document) {
    nsIPresShell* shell = aPresContext->GetShell();
    if (NS_SUCCEEDED(rv)) {
      // XXX This API needs changing, because it appears to be designed for
      // an arbitrary content element, but yet it always constructs the document
      // element's frame. Plus it has the problem noted above in the previous XXX
      rv = aParentFrame->RemoveFrame(*aPresContext, *shell,
                                     nsnull, aFrameSubTree);

      if (NS_SUCCEEDED(rv)) {
        nsIFrame*         newChild;
        nsIStyleContext*  rootPseudoStyle;

        aParentFrame->GetStyleContext(rootPseudoStyle);
        rv = ConstructDocElementFrame(aPresContext, aContent,
                                      aParentFrame, rootPseudoStyle, newChild);
        NS_RELEASE(rootPseudoStyle);

        if (NS_SUCCEEDED(rv)) {
          rv = aParentFrame->InsertFrames(*aPresContext, *shell,
                                          nsnull, nsnull, newChild);
        }
      }
    }  
    NS_RELEASE(document);
    NS_RELEASE(shell);
  }

  return rv;
}

nsIFrame*
HTMLStyleSheetImpl::GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                                nsIContent* aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame;
  aPresShell->GetPrimaryFrameFor(aContent, frame);

  if (nsnull != frame) {
    // If the primary frame is a scroll frame, then get the scrolled frame.
    // That's the frame that gets the reflow command
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    if (display->IsBlockLevel() && IsScrollable(aPresContext, display)) {
      frame->FirstChild(nsnull, frame);
    } else if (NS_STYLE_DISPLAY_TABLE == display->mDisplay) { // we got an outer table frame, need an inner
      frame->FirstChild(nsnull, frame);                       // the inner frame is always the 1st child
    }    
  }

  return frame;
}

nsIFrame*
HTMLStyleSheetImpl::GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
                                               nsIFrame*       aFrame)
{
  NS_PRECONDITION(nsnull != mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is absolutely positioned
  nsIFrame* containingBlock = aFrame;
  while (nsnull != containingBlock) {
    const nsStylePosition* position;

    // Is it absolutely positioned?
    containingBlock->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if (position->mPosition == NS_STYLE_POSITION_ABSOLUTE) {
      const nsStyleDisplay* display;
      
      // If it's a table then ignore it, because for the time being tables
      // are not containers for absolutely positioned child frames
      containingBlock->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      if (display->mDisplay != NS_STYLE_DISPLAY_TABLE) {
        // XXX If the frame is scrolled, then don't return the scrolled frame
        // XXX Verify that the frame type is an area-frame...
        break;
      }
    }

    // Continue walking up the hierarchy
    containingBlock->GetParent(containingBlock);
  }

  // If we didn't find an absolutely positioned containing block, then use the
  // initial containing block
  if (nsnull == containingBlock) {
    containingBlock = mInitialContainingBlock;
  }
  
  return containingBlock;
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

    // Get the containing block for absolutely positioned elements
    nsIFrame* absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext,
                                                                   parentFrame);

    // Create some new frames
    PRInt32         count;
    nsIFrame*       lastChildFrame = nsnull;
    nsIFrame*       firstAppendedFrame = nsnull;
    nsAbsoluteItems absoluteItems(absoluteContainingBlock);
    nsFrameItems    frameItems;

    aContainer->ChildCount(count);

    for (PRInt32 i = aNewIndexInContainer; i < count; i++) {
      nsIContent* child;
      
      aContainer->ChildAt(i, child);
      ConstructFrame(aPresContext, child, parentFrame, absoluteItems, frameItems);

      NS_RELEASE(child);
    }

    // Adjust parent frame for table inner/outer frame
    // we need to do this here because we need both the parent frame and the constructed frame
    nsresult result = NS_OK;
    nsIFrame *adjustedParentFrame=parentFrame;
    firstAppendedFrame = frameItems.childList;
    if (nsnull != firstAppendedFrame) {
      const nsStyleDisplay* firstAppendedFrameDisplay;
      firstAppendedFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)firstAppendedFrameDisplay);
      result = GetAdjustedParentFrame(parentFrame, firstAppendedFrameDisplay->mDisplay, adjustedParentFrame);
    }

    // Notify the parent frame with a reflow command, passing it the list of
    // new frames.
    if (NS_SUCCEEDED(result)) {
      result = adjustedParentFrame->AppendFrames(*aPresContext, *shell,
                                                 nsnull, firstAppendedFrame);

      // XXX update comment and code!
      // If there are new absolutely positioned child frames then send a reflow
      // command for them, too.
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (nsnull != absoluteItems.childList) {
        absoluteItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                    nsLayoutAtoms::absoluteList,
                                                    absoluteItems.childList);
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

  // Note: not all content objects are associated with a frame (e.g., if their
  // 'display' type is 'hidden') so keep looking until we find a previous frame
  for (PRInt32 index = aIndexInContainer - 1; index >= 0; index--) {
    nsIContent* precedingContent;
    aContainer->ChildAt(index, precedingContent);
    aPresShell->GetPrimaryFrameFor(precedingContent, prevSibling);
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

  // Note: not all content objects are associated with a frame (e.g., if their
  // 'display' type is 'hidden') so keep looking until we find a previous frame
  PRInt32 count;
  aContainer->ChildCount(count);
  for (PRInt32 index = aIndexInContainer + 1; index < count; index++) {
    nsIContent* nextContent;
    aContainer->ChildAt(index, nextContent);
    aPresShell->GetPrimaryFrameFor(nextContent, nextSibling);
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
    shell->GetPrimaryFrameFor(aContainer, parentFrame);

  } else {
    // Use the prev sibling if we have it; otherwise use the next sibling
    if (nsnull != prevSibling) {
      prevSibling->GetParent(parentFrame);
    } else {
      nextSibling->GetParent(parentFrame);
    }
  }

  // Construct a new frame
  nsresult  rv = NS_OK;
  if (nsnull != parentFrame) {
    // Get the containing block for absolutely positioned elements
    nsIFrame* absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext,
                                                                   parentFrame);

    nsAbsoluteItems absoluteItems(absoluteContainingBlock);
    nsFrameItems    frameItems;
    rv = ConstructFrame(aPresContext, aChild, parentFrame, absoluteItems, frameItems);

    nsIFrame* newFrame = frameItems.childList;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      nsIReflowCommand* reflowCmd = nsnull;

      // Notify the parent frame with a reflow command.
      if (isAppend) {
        // Generate a FrameAppended reflow command
        rv = parentFrame->AppendFrames(*aPresContext, *shell,
                                       nsnull, newFrame);
      } else {
        // Generate a FrameInserted reflow command
        rv = parentFrame->InsertFrames(*aPresContext, *shell, nsnull,
                                       prevSibling, newFrame);
      }

      // XXX update the comment and code!
      // If there are new absolutely positioned child frames then send a reflow
      // command for them, too.
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (NS_SUCCEEDED(rv) && (nsnull != absoluteItems.childList)) {
        rv = absoluteItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                         nsLayoutAtoms::absoluteList,
                                                         absoluteItems.childList);
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
  // XXX For now, do a brute force remove and insert.
  nsresult res = ContentRemoved(aPresContext, aContainer, 
                                aOldChild, aIndexInContainer);
  if (NS_OK == res) {
    res = ContentInserted(aPresContext, aContainer, 
                          aNewChild, aIndexInContainer);
  }

  return res;
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
  nsIFrame* childFrame;
  shell->GetPrimaryFrameFor(aChild, childFrame);

  if (nsnull != childFrame) {
    // See if it's absolutely positioned
    const nsStylePosition* position;
    childFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
      // Generate two reflow commands. First for the absolutely positioned
      // frame and then for its placeholder frame
      nsIFrame* parentFrame;
      childFrame->GetParent(parentFrame);
  
      // Update the parent frame
      rv = parentFrame->RemoveFrame(*aPresContext, *shell,
                                    nsLayoutAtoms::absoluteList, childFrame);

      // Now the placeholder frame
      nsIFrame* placeholderFrame;
      shell->GetPlaceholderFrameFor(childFrame, placeholderFrame);
      if (nsnull != placeholderFrame) {
        placeholderFrame->GetParent(parentFrame);
        rv = parentFrame->RemoveFrame(*aPresContext, *shell,
                                      nsnull, placeholderFrame);
      }

    } else {
      // Get the parent frame.
      // Note that we use the content parent, and not the geometric parent,
      // in case the frame has been moved out of the flow...
      nsIFrame* parentFrame;
      childFrame->GetParent(parentFrame);
      NS_ASSERTION(nsnull != parentFrame, "null content parent frame");
  
      // Update the parent frame
      rv = parentFrame->RemoveFrame(*aPresContext, *shell,
                                    nsnull, childFrame);
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
    const nsStyleDisplay* disp; 
    aFrame->GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&) color);
    aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) disp);

    view->SetVisibility(NS_STYLE_VISIBILITY_HIDDEN == disp->mVisible ?nsViewVisibility_kHide:nsViewVisibility_kShow); 

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
  nsIFrame* frame;
  shell->GetPrimaryFrameFor(aContent, frame);

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
    
NS_IMETHODIMP
HTMLStyleSheetImpl::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent* aContent,
                                     nsIAtom* aAttribute,
                                     PRInt32 aHint)
{
  nsresult  result = NS_OK;

  nsIPresShell* shell = aPresContext->GetShell();
  nsIFrame*     frame;
   
  shell->GetPrimaryFrameFor(aContent, frame);

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

    // the style tag has its own interpretation based on aHint 
    if ((nsHTMLAtoms::style != aAttribute) && (NS_STYLE_HINT_UNKNOWN == aHint)) { 
      nsIHTMLContent* htmlContent;
      result = aContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
  
      if (NS_OK == result) { 
        // Get style hint from HTML content object. 
        htmlContent->GetStyleHintForAttributeChange(aAttribute, &aHint);
        NS_RELEASE(htmlContent); 
      } 
    } 

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
      case NS_STYLE_HINT_AURAL:
        restyle = PR_TRUE;
        break;
      case NS_STYLE_HINT_NONE:
        break;
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
  }

  NS_RELEASE(shell);
  return result;
}

  // Style change notifications
NS_IMETHODIMP
HTMLStyleSheetImpl::StyleRuleChanged(nsIPresContext* aPresContext,
                                     nsIStyleSheet* aStyleSheet,
                                     nsIStyleRule* aStyleRule,
                                     PRInt32 aHint)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsIFrame* frame;
  shell->GetRootFrame(frame);

  PRBool reframe  = PR_FALSE;
  PRBool reflow   = PR_FALSE;
  PRBool render   = PR_FALSE;
  PRBool restyle  = PR_FALSE;
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
    case NS_STYLE_HINT_AURAL:
      restyle = PR_TRUE;
      break;
    case NS_STYLE_HINT_NONE:
      break;
  }

  if (restyle) {
    nsIStyleContext* sc;
    frame->GetStyleContext(sc);
    sc->RemapStyle(aPresContext);
    NS_RELEASE(sc);

    // XXX hack, skip the root and scrolling frames
    frame->FirstChild(nsnull, frame);
    frame->FirstChild(nsnull, frame);
    if (reframe) {
      NS_NOTYETIMPLEMENTED("frame change reflow");
    }
    else if (reflow) {
      StyleChangeReflow(aPresContext, frame, nsnull);
    }
    else if (render) {
      ApplyRenderingChangeToTree(aPresContext, frame);
    }
  }

  NS_RELEASE(shell);
  
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::StyleRuleAdded(nsIPresContext* aPresContext,
                                   nsIStyleSheet* aStyleSheet,
                                   nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::StyleRuleRemoved(nsIPresContext* aPresContext,
                                     nsIStyleSheet* aStyleSheet,
                                     nsIStyleRule* aStyleRule)
{
  return NS_OK;
}


void HTMLStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  PRUnichar* buffer;

  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
  mURL->ToString(&buffer);
  fputs(buffer, out);
  fputs("\n", out);
  delete buffer;
}

// XXX For convenience and backwards compatibility
NS_HTML nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURL* aURL, 
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
