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

#include "nsCSSFrameConstructor.h"
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
#include "nsDeque.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsITextContent.h"

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsTreeFrame.h"
#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h"
#include "nsTreeIndentationFrame.h"
#include "nsTreeCellFrame.h"
#include "nsProgressMeterFrame.h"
#endif

//#define FRAMEBASED_COMPONENTS 1 // This is temporary please leave in for now - rods

//static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID, NS_ICOMBOBOXCONTROLFRAME_IID);
static NS_DEFINE_IID(kIListControlFrameIID,     NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID, NS_IDOMCHARACTERDATA_IID);


// Structure used when constructing formatting object trees.
struct nsFrameItems {
  nsIFrame* childList;
  nsIFrame* lastChild;
  
  nsFrameItems();
  nsFrameItems(nsIFrame* aFrame);

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

nsFrameItems::nsFrameItems()
:childList(nsnull), lastChild(nsnull)
{}

nsFrameItems::nsFrameItems(nsIFrame* aFrame)
:childList(aFrame), lastChild(aFrame)
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
struct nsAbsoluteItems : nsFrameItems {
  nsIFrame* const containingBlock;  // containing block for absolutely positioned elements

  nsAbsoluteItems(nsIFrame* aContainingBlock);
};

nsAbsoluteItems::nsAbsoluteItems(nsIFrame* aContainingBlock)
  : containingBlock(aContainingBlock)
{
}


// -----------------------------------------------------------

// Structure used when creating table frames.
struct nsTableCreator {
  virtual nsresult CreateTableFrame(nsIFrame*& aNewFrame);
  virtual nsresult CreateTableRowGroupFrame(nsIFrame*& aNewFrame);
  virtual nsresult CreateTableColFrame(nsIFrame*& aNewFrame);
  virtual nsresult CreateTableColGroupFrame(nsIFrame*& aNewFrame);
  virtual nsresult CreateTableRowFrame(nsIFrame*& aNewFrame);
  virtual nsresult CreateTableCellFrame(nsIFrame*& aNewFrame);
};

nsresult
nsTableCreator::CreateTableFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableFrame(aNewFrame);
}

nsresult
nsTableCreator::CreateTableRowGroupFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableRowGroupFrame(aNewFrame);
}

nsresult
nsTableCreator::CreateTableColFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableColFrame(aNewFrame);
}

nsresult
nsTableCreator::CreateTableColGroupFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableColGroupFrame(aNewFrame);
}

nsresult
nsTableCreator::CreateTableRowFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableRowFrame(aNewFrame);
}

nsresult
nsTableCreator::CreateTableCellFrame(nsIFrame*& aNewFrame) {
  return NS_NewTableCellFrame(aNewFrame);
}

#ifdef INCLUDE_XUL

// Structure used when creating tree frames
struct nsTreeCreator: public nsTableCreator {
  nsresult CreateTableFrame(nsIFrame*& aNewFrame);
  nsresult CreateTableCellFrame(nsIFrame*& aNewFrame);
};

nsresult
nsTreeCreator::CreateTableFrame(nsIFrame*& aNewFrame)
{
  return NS_NewTreeFrame(aNewFrame);
}

nsresult
nsTreeCreator::CreateTableCellFrame(nsIFrame*& aNewFrame)
{
  return NS_NewTreeCellFrame(aNewFrame);
}

#endif // INCLUDE_XUL

// -----------------------------------------------------------

nsCSSFrameConstructor::nsCSSFrameConstructor(void)
  : nsIStyleFrameConstruction(),
    mDocument(nsnull),
    mInitialContainingBlock(nsnull),
    mFixedContainingBlock(nsnull),
    mDocElementContainingBlock(nsnull)
{
  NS_INIT_REFCNT();
#ifdef INCLUDE_XUL
  nsXULAtoms::AddrefAtoms();
#endif
}

nsCSSFrameConstructor::~nsCSSFrameConstructor(void)
{
#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
#endif
}

NS_IMPL_ISUPPORTS(nsCSSFrameConstructor, kIStyleFrameConstructionIID);


NS_IMETHODIMP 
nsCSSFrameConstructor::Init(nsIDocument* aDocument)
{
  NS_PRECONDITION(aDocument, "null ptr");
  if (! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  return NS_OK;
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
nsCSSFrameConstructor::ProcessChildren(nsIPresContext*  aPresContext,
                                       nsIContent*      aContent,
                                       nsIFrame*        aFrame,
                                       nsAbsoluteItems& aAbsoluteItems,
                                       nsFrameItems&    aFrameItems,
                                       nsAbsoluteItems& aFixedItems)
{
  // Iterate the child content objects and construct a frame
  PRInt32   count;

  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> childContent;
    
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent)))) {
      // Construct a child frame
      ConstructFrame(aPresContext, childContent, aFrame, aAbsoluteItems,
                     aFrameItems, aFixedItems);
    }
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::CreateInputFrame(nsIContent* aContent, nsIFrame*& aFrame)
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

/****************************************************
 **  BEGIN TABLE SECTION
 ****************************************************/

// too bad nsDeque requires such a class 
class nsBenignFunctor: public nsDequeFunctor{
  public:
    virtual void* operator()(void* aObject) {
      return aObject;
    }
};

static nsBenignFunctor* gBenignFunctor = new nsBenignFunctor;


// Construct the outer, inner table frames and the children frames for the table. 
// Tables can have any table-related child.
nsresult
nsCSSFrameConstructor::ConstructTableFrame(nsIPresContext*  aPresContext,
                                           nsIContent*      aContent,
                                           nsIFrame*        aParentFrame,
                                           nsIStyleContext* aStyleContext,
                                           nsAbsoluteItems& aAbsoluteItems,
                                           nsIFrame*&       aNewFrame,
                                           nsAbsoluteItems& aFixedItems,
                                           nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;
  nsIFrame* childList;
  nsIFrame* innerFrame;
  nsIFrame* innerChildList = nsnull;
  nsIFrame* captionFrame   = nsnull;

  // Create an anonymous table outer frame which holds the caption and table frame
  NS_NewTableOuterFrame(aNewFrame);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned
  aNewFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);
  nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                           aStyleContext, PR_FALSE);
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
#if 0
  nsCOMPtr<nsIStyleContext> outerStyleContext;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableOuterPseudo,
                                             parentStyleContext,
                                             getter_AddRefs(outerStyleContext));
  aNewFrame->Init(*aPresContext, aContent, aParentFrame, outerStyleContext);
  nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewFrame,
                                           outerStyleContext, PR_FALSE);
#endif
  // Create the inner table frame
  aTableCreator.CreateTableFrame(innerFrame);
  
  // This gets reset later, since there may also be a caption. 
  // It allows descendants to get at the inner frame before that
  aNewFrame->SetInitialChildList(*aPresContext, nsnull, innerFrame); 
  childList = innerFrame;

  innerFrame->Init(*aPresContext, aContent, aNewFrame, aStyleContext);

  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) { // iterate the child content
    nsCOMPtr<nsIContent> childContent;
    
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent)))) {
      nsIFrame*                 childFrame = nsnull;
      nsIFrame*                 ignore;
      nsCOMPtr<nsIStyleContext> childStyleContext;

      // Resolve the style context and get its display
      aPresContext->ResolveStyleContextFor(childContent, aStyleContext,
                                           PR_FALSE,
                                           getter_AddRefs(childStyleContext));
      const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
        childStyleContext->GetStyleData(eStyleStruct_Display);

      switch (styleDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_TABLE_CAPTION:
        if (nsnull == captionFrame) {  // only allow one caption
          // XXX should absolute items be passed along?
          rv = ConstructTableCaptionFrame(aPresContext, childContent, aNewFrame, childStyleContext,
                                          aAbsoluteItems, ignore, captionFrame, aFixedItems, aTableCreator);
        }
        break;

      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      {
        PRBool isRowGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != styleDisplay->mDisplay);
        rv = ConstructTableGroupFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                      aAbsoluteItems, isRowGroup, childFrame, ignore, aFixedItems, aTableCreator);
        break;
      }
      case NS_STYLE_DISPLAY_TABLE_ROW:
        rv = ConstructTableRowFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                    aAbsoluteItems, childFrame, ignore, aFixedItems, aTableCreator);
        break;

      case NS_STYLE_DISPLAY_TABLE_COLUMN:
        rv = ConstructTableColFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                    aAbsoluteItems, childFrame, ignore, aFixedItems, aTableCreator);
        break;


      case NS_STYLE_DISPLAY_TABLE_CELL:
        rv = ConstructTableCellFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                     aAbsoluteItems, childFrame, ignore, aFixedItems, aTableCreator);
        break;
      default:
        //nsIFrame* nonTableRelatedFrame;
        //nsIAtom*  tag;
        //childContent->GetTag(tag);
        //ConstructFrameByTag(aPresContext, childContent, aNewFrame, tag, childStyleContext,
        //                    aAbsoluteItems, nonTableRelatedFrame);
        //childList->SetNextSibling(nonTableRelatedFrame);
        //NS_IF_RELEASE(tag);
        nsFrameItems childItems;
        TableProcessChild(aPresContext, childContent, innerFrame, parentStyleContext,
                          aAbsoluteItems, childItems, aFixedItems, aTableCreator);
        childFrame = childItems.childList; // XXX: Need to change this whole function to return a list of frames
                                            // rather than a single frame. - DWH
        break;
      }

      // for every table related frame except captions, link into the child list
      if (nsnull != childFrame) { 
        if (nsnull == lastChildFrame) {
          innerChildList = childFrame;
        } else {
          lastChildFrame->SetNextSibling(childFrame);
        }
        lastChildFrame = childFrame;
      }
    }
  }

  // Set the inner table frame's list of initial child frames
  innerFrame->SetInitialChildList(*aPresContext, nsnull, innerChildList);

  // Set the anonymous table outer frame's initial child list
  aNewFrame->SetInitialChildList(*aPresContext, nsnull, childList);
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructAnonymousTableFrame (nsIPresContext*  aPresContext, 
                                                     nsIContent*      aContent, 
                                                     nsIFrame*        aParentFrame, 
                                                     nsIFrame*&       aOuterFrame, 
                                                     nsIFrame*&       aInnerFrame, 
                                                     nsAbsoluteItems& aFixedItems,
                                                     nsTableCreator&  aTableCreator)
{
  nsresult result = NS_OK;
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIStyleContext> parentStyleContext;
    result = aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
    if (NS_SUCCEEDED(result)) {
      // create the outer table frames
      nsCOMPtr<nsIStyleContext> outerStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableOuterPseudo, 
                                                 parentStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(outerStyleContext));
      result = NS_NewTableOuterFrame(aOuterFrame);
      if (NS_SUCCEEDED(result)) {
        aOuterFrame->Init(*aPresContext, aContent, aParentFrame, outerStyleContext);

        // create the inner table frames
        nsCOMPtr<nsIStyleContext> innerStyleContext;
        aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tablePseudo, 
                                                   outerStyleContext,
                                                   PR_FALSE,
                                                   getter_AddRefs(innerStyleContext));
        result = aTableCreator.CreateTableFrame(aInnerFrame);
        if (NS_SUCCEEDED(result)) {
          aInnerFrame->Init(*aPresContext, aContent, aOuterFrame, innerStyleContext);
        }
      }
    }
  }

  return result;
}

// if aParentFrame is a table, it is assummed that it is an outer table and the inner
// table is already a child
nsresult
nsCSSFrameConstructor::ConstructTableCaptionFrame(nsIPresContext*  aPresContext,
                                                  nsIContent*      aContent,
                                                  nsIFrame*        aParentFrame,
                                                  nsIStyleContext* aStyleContext,
                                                  nsAbsoluteItems& aAbsoluteItems,
                                                  nsIFrame*&       aNewTopMostFrame,
                                                  nsIFrame*&       aNewCaptionFrame,
                                                  nsAbsoluteItems& aFixedItems,
                                                  nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_NewAreaFrame(aNewCaptionFrame, 0);
  if (NS_SUCCEEDED(rv)) {
    const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
    nsIFrame* innerFrame;

    if (NS_STYLE_DISPLAY_TABLE == parentDisplay->mDisplay) { // parent is an outer table
      aParentFrame->FirstChild(nsnull, &innerFrame);
      aNewCaptionFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);
      innerFrame->SetNextSibling(aNewCaptionFrame);
      // the caller is responsible for calling SetInitialChildList on the outer, inner frames
      aNewTopMostFrame = aNewCaptionFrame;
    } else { // parent is not a table, need to create a new table
      nsIFrame* outerFrame;
      ConstructAnonymousTableFrame(aPresContext, aContent, aParentFrame, outerFrame, innerFrame, aFixedItems, aTableCreator);
      nsCOMPtr<nsIStyleContext> outerStyleContext;
      outerFrame->GetStyleContext(getter_AddRefs(outerStyleContext));
      nsCOMPtr<nsIStyleContext> adjStyleContext;
      aPresContext->ResolveStyleContextFor(aContent, outerStyleContext,
                                           PR_FALSE,
                                           getter_AddRefs(adjStyleContext));
      aNewCaptionFrame->Init(*aPresContext, aContent, outerFrame, adjStyleContext);
      innerFrame->SetNextSibling(aNewCaptionFrame);
      outerFrame->SetInitialChildList(*aPresContext, nsnull, innerFrame);
      innerFrame->SetInitialChildList(*aPresContext, nsnull, nsnull);
      aNewTopMostFrame = outerFrame;
    }
    nsFrameItems childItems;
    ProcessChildren(aPresContext, aContent, aNewCaptionFrame, aAbsoluteItems, 
                    childItems, aFixedItems);
    aNewCaptionFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  }
  return rv;
}

// if aParentFrame is a table, it is assummed that it is an inner table 
nsresult
nsCSSFrameConstructor::ConstructTableGroupFrame(nsIPresContext*  aPresContext,
                                                nsIContent*      aContent,
                                                nsIFrame*        aParentFrame,
                                                nsIStyleContext* aStyleContext,
                                                nsAbsoluteItems& aAbsoluteItems,
                                                PRBool           aIsRowGroup,
                                                nsIFrame*&       aNewTopMostFrame, 
                                                nsIFrame*&       aNewGroupFrame,
                                                nsAbsoluteItems& aFixedItems,
                                                nsTableCreator&  aTableCreator,
                                                nsDeque*         aToDo)   
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool contentDisplayIsGroup = 
    (aIsRowGroup) ? (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == styleDisplay->mDisplay) ||
                    (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == styleDisplay->mDisplay) ||
                    (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == styleDisplay->mDisplay)
                  : (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == styleDisplay->mDisplay);

  nsCOMPtr<nsIStyleContext> styleContext(aStyleContext);
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  const nsStyleDisplay* parentDisplay = 
   (const nsStyleDisplay*) parentStyleContext->GetStyleData(eStyleStruct_Display);

  if (NS_STYLE_DISPLAY_TABLE == parentDisplay->mDisplay) { // parent is an inner table
    if (!contentDisplayIsGroup) { // content is from some (soon to be) child of ours
      nsIAtom* pseudoGroup = (aIsRowGroup) ? nsHTMLAtoms::tableRowGroupPseudo : nsHTMLAtoms::tableColGroupPseudo;
      aPresContext->ResolvePseudoStyleContextFor(aContent, pseudoGroup, parentStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    rv = ConstructTableGroupFrameOnly(aPresContext, aContent, aParentFrame, styleContext, 
                                      aAbsoluteItems, aIsRowGroup, contentDisplayIsGroup,
                                      aNewTopMostFrame, aNewGroupFrame, aFixedItems, aTableCreator);
  } else { // construct anonymous table frames
    nsIFrame* innerFrame;
    nsIFrame* outerFrame;
    ConstructAnonymousTableFrame(aPresContext, aContent, aParentFrame, outerFrame, innerFrame, aFixedItems, aTableCreator);
    nsCOMPtr<nsIStyleContext> innerStyleContext;
    innerFrame->GetStyleContext(getter_AddRefs(innerStyleContext));
    if (contentDisplayIsGroup) {
      aPresContext->ResolveStyleContextFor(aContent, innerStyleContext, PR_FALSE,
                                           getter_AddRefs(styleContext));
    } else {
      nsIAtom* pseudoGroup = (aIsRowGroup) ? nsHTMLAtoms::tableRowGroupPseudo : nsHTMLAtoms::tableColGroupPseudo;
      aPresContext->ResolvePseudoStyleContextFor(aContent, pseudoGroup, innerStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    rv = ConstructTableGroupFrameOnly(aPresContext, aContent, innerFrame, styleContext,
                                      aAbsoluteItems, aIsRowGroup, contentDisplayIsGroup,
                                      aNewTopMostFrame, aNewGroupFrame, aFixedItems, aTableCreator);
    if (NS_SUCCEEDED(rv)) {
      if (aToDo) { // some descendant will set the table's child lists later
        aToDo->Push(innerFrame);
        aToDo->Push(aNewTopMostFrame);
        aToDo->Push(outerFrame);
        aToDo->Push(innerFrame);
      } else { // set the table's child lists now
        innerFrame->SetInitialChildList(*aPresContext, nsnull, aNewTopMostFrame);
        outerFrame->SetInitialChildList(*aPresContext, nsnull, innerFrame);
      }
    }
    aNewTopMostFrame = outerFrame;
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableGroupFrameOnly(nsIPresContext*  aPresContext,
                                                    nsIContent*      aContent,
                                                    nsIFrame*        aParentFrame,
                                                    nsIStyleContext* aStyleContext,
                                                    nsAbsoluteItems& aAbsoluteItems,
                                                    PRBool           aIsRowGroup,
                                                    PRBool           aProcessChildren,
                                                    nsIFrame*&       aNewTopMostFrame, 
                                                    nsIFrame*&       aNewGroupFrame,
                                                    nsAbsoluteItems& aFixedItems,
                                                    nsTableCreator&  aTableCreator) 
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* styleDisplay = 
    (const nsStyleDisplay*) aStyleContext->GetStyleData(eStyleStruct_Display);

  if (IsScrollable(aPresContext, styleDisplay)) {
    // Create a scroll frame and initialize it
    rv = NS_NewScrollFrame(aNewTopMostFrame);
    if (NS_SUCCEEDED(rv)) {
      aNewTopMostFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);

      // The scroll frame gets the original style context, the scrolled frame gets  
      // a pseudo element style context that inherits the background properties
      nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::scrolledContentPseudo,
                                                 aStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(scrolledPseudoStyle));

      // Create an area container for the frame
      rv = (aIsRowGroup) ? aTableCreator.CreateTableRowGroupFrame(aNewGroupFrame)
                         : aTableCreator.CreateTableColGroupFrame(aNewGroupFrame);
      if (NS_SUCCEEDED(rv)) {

        // Initialize the frame and force it to have a view
        aNewGroupFrame->Init(*aPresContext, aContent, aNewTopMostFrame, scrolledPseudoStyle);
        nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewGroupFrame,
                                                 scrolledPseudoStyle, PR_TRUE);
        aNewTopMostFrame->SetInitialChildList(*aPresContext, nsnull, aNewGroupFrame);
      }
    }
  } else {
    rv = (aIsRowGroup) ? aTableCreator.CreateTableRowGroupFrame(aNewTopMostFrame)
                       : aTableCreator.CreateTableColGroupFrame(aNewTopMostFrame);
    if (NS_SUCCEEDED(rv)) {
      aNewTopMostFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);
      aNewGroupFrame = aNewTopMostFrame;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    if (aProcessChildren) {
      nsFrameItems childItems;
      if (aIsRowGroup) {
        TableProcessChildren(aPresContext, aContent, aNewGroupFrame, aAbsoluteItems, 
                             childItems, aFixedItems, aTableCreator);
      } else {
        ProcessChildren(aPresContext, aContent, aNewGroupFrame, aAbsoluteItems, 
                        childItems, aFixedItems);
      }
      aNewGroupFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableRowFrame(nsIPresContext*  aPresContext,
                                              nsIContent*      aContent,
                                              nsIFrame*        aParentFrame,
                                              nsIStyleContext* aStyleContext,
                                              nsAbsoluteItems& aAbsoluteItems,
                                              nsIFrame*&       aNewTopMostFrame,
                                              nsIFrame*&       aNewRowFrame,
                                              nsAbsoluteItems& aFixedItems,
                                              nsTableCreator&  aTableCreator,
                                              nsDeque*         aToDo)
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool contentDisplayIsRow = (NS_STYLE_DISPLAY_TABLE_ROW == display->mDisplay);

  // if groupStyleContext gets set, both it and styleContext need to be released
  nsCOMPtr<nsIStyleContext> groupStyleContext;
  nsCOMPtr<nsIStyleContext> styleContext(aStyleContext);

  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  if ((NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == parentDisplay->mDisplay)) {
    if (!contentDisplayIsRow) { // content is from some (soon to be) child of ours
      aParentFrame = TableGetAsNonScrollFrame(aPresContext, aParentFrame, parentDisplay); 
      aParentFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo,
                                                 groupStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    // only process the row's children if the content display is row
    rv = ConstructTableRowFrameOnly(aPresContext, aContent, aParentFrame, styleContext, 
                                    aAbsoluteItems, contentDisplayIsRow, aNewRowFrame, aFixedItems, aTableCreator);
    aNewTopMostFrame = aNewRowFrame;
  } else { // construct an anonymous row group frame
    nsIFrame* groupFrame;
    nsDeque* toDo = (aToDo) ? aToDo : new nsDeque(*gBenignFunctor);
    rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, styleContext,
                                  aAbsoluteItems, PR_TRUE, aNewTopMostFrame, groupFrame,
                                  aFixedItems, aTableCreator, toDo);
    if (NS_SUCCEEDED(rv)) {
      groupFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
      if (contentDisplayIsRow) {
        aPresContext->ResolveStyleContextFor(aContent, groupStyleContext,
                                             PR_FALSE,
                                             getter_AddRefs(styleContext));
      } else {
        aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo,
                                                   groupStyleContext,
                                                   PR_FALSE,
                                                   getter_AddRefs(styleContext));
      }
      rv = ConstructTableRowFrameOnly(aPresContext, aContent, groupFrame, styleContext, 
                                      aAbsoluteItems, contentDisplayIsRow, aNewRowFrame, aFixedItems, aTableCreator);
      if (NS_SUCCEEDED(rv)) {
        aNewRowFrame->Init(*aPresContext, aContent, groupFrame, styleContext);
        if (aToDo) {
          aToDo->Push(groupFrame);
          aToDo->Push(aNewRowFrame);
        } else {
          groupFrame->SetInitialChildList(*aPresContext, nsnull, aNewRowFrame);
          TableProcessChildLists(aPresContext, toDo);
          delete toDo;
        }
      }
    }
  }

  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableRowFrameOnly(nsIPresContext*  aPresContext,
                                                  nsIContent*      aContent,
                                                  nsIFrame*        aParentFrame,
                                                  nsIStyleContext* aStyleContext,
                                                  nsAbsoluteItems& aAbsoluteItems,
                                                  PRBool           aProcessChildren,
                                                  nsIFrame*&       aNewRowFrame,
                                                  nsAbsoluteItems& aFixedItems,
                                                  nsTableCreator&  aTableCreator)
{
  nsresult rv = aTableCreator.CreateTableRowFrame(aNewRowFrame);
  if (NS_SUCCEEDED(rv)) {
    aNewRowFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);
    if (aProcessChildren) {
      nsFrameItems childItems;
      rv = TableProcessChildren(aPresContext, aContent, aNewRowFrame, aAbsoluteItems, 
                                childItems, aFixedItems, aTableCreator);
      if (NS_SUCCEEDED(rv)) {
        aNewRowFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
      }
    }
  }
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsIPresContext*  aPresContext,
                                              nsIContent*      aContent,
                                              nsIFrame*        aParentFrame,
                                              nsIStyleContext* aStyleContext,
                                              nsAbsoluteItems& aAbsoluteItems,
                                              nsIFrame*&       aNewTopMostFrame,
                                              nsIFrame*&       aNewColFrame,
                                              nsAbsoluteItems& aFixedItems,
                                              nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;

  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == parentDisplay->mDisplay) {
    rv = aTableCreator.CreateTableColFrame(aNewColFrame);
    aNewColFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext);
    aNewTopMostFrame = aNewColFrame;
  } else { // construct anonymous col group frame
    nsIFrame* groupFrame;
    rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, aStyleContext,
                                  aAbsoluteItems, PR_FALSE, aNewTopMostFrame, 
                                  groupFrame, aFixedItems, aTableCreator);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStyleContext> groupStyleContext; 
      groupFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
      nsCOMPtr<nsIStyleContext> styleContext;
      aPresContext->ResolveStyleContextFor(aContent, groupStyleContext,
                                           PR_FALSE,
                                           getter_AddRefs(styleContext));
      rv = aTableCreator.CreateTableColFrame(aNewColFrame);
      aNewColFrame->Init(*aPresContext, aContent, groupFrame, styleContext);
      if (NS_SUCCEEDED(rv)) {
        groupFrame->SetInitialChildList(*aPresContext, nsnull, aNewColFrame);
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    nsFrameItems colChildItems;
    rv = ProcessChildren(aPresContext, aContent, aNewColFrame, aAbsoluteItems, 
                         colChildItems, aFixedItems);
    if (NS_SUCCEEDED(rv)) {
      aNewColFrame->SetInitialChildList(*aPresContext, nsnull, colChildItems.childList);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsIPresContext*  aPresContext,
                                               nsIContent*      aContent,
                                               nsIFrame*        aParentFrame,
                                               nsIStyleContext* aStyleContext,
                                               nsAbsoluteItems& aAbsoluteItems,
                                               nsIFrame*&       aNewTopMostFrame,
                                               nsIFrame*&       aNewCellFrame,
                                               nsAbsoluteItems& aFixedItems,
                                               nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;

  const nsStyleDisplay* display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool contentDisplayIsCell = (NS_STYLE_DISPLAY_TABLE_CELL == display->mDisplay);

  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  const nsStyleDisplay* parentDisplay = (const nsStyleDisplay*)
    parentStyleContext->GetStyleData(eStyleStruct_Display);

  nsCOMPtr<nsIStyleContext> styleContext(aStyleContext);
  PRBool wrapContent = PR_FALSE;

  if (NS_STYLE_DISPLAY_TABLE_ROW == parentDisplay->mDisplay) {
    nsCOMPtr<nsIStyleContext> styleContextRelease;
    if (!contentDisplayIsCell) {
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo,
                                                 parentStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(styleContext));
      wrapContent = PR_TRUE;
    }
    rv = ConstructTableCellFrameOnly(aPresContext, aContent, aParentFrame, styleContext,
                                     wrapContent, aAbsoluteItems, aNewCellFrame, aFixedItems, aTableCreator);
    aNewTopMostFrame = aNewCellFrame;
  } else { // construct anonymous row frame
    nsIFrame* rowFrame;
    nsDeque toDo(*gBenignFunctor);
    rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, aStyleContext,
                                aAbsoluteItems, aNewTopMostFrame, rowFrame, aFixedItems, aTableCreator, &toDo);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStyleContext> rowStyleContext; 
      rowFrame->GetStyleContext(getter_AddRefs(rowStyleContext));
      if (contentDisplayIsCell) {
        aPresContext->ResolveStyleContextFor(aContent, rowStyleContext,
                                             PR_FALSE,
                                             getter_AddRefs(styleContext));
      } else {
        wrapContent = PR_TRUE;
        aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo,
                                                   rowStyleContext, PR_FALSE,
                                                   getter_AddRefs(styleContext));
      }
      rv = ConstructTableCellFrameOnly(aPresContext, aContent, rowFrame, styleContext,
                                       wrapContent, aAbsoluteItems, aNewCellFrame, aFixedItems, aTableCreator);
      if (NS_SUCCEEDED(rv)) {
        rowFrame->SetInitialChildList(*aPresContext, nsnull, aNewCellFrame);
        TableProcessChildLists(aPresContext, &toDo);
      }
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrameOnly(nsIPresContext*  aPresContext,
                                                   nsIContent*      aContent,
                                                   nsIFrame*        aParentFrame,
                                                   nsIStyleContext* aStyleContext,
                                                   PRBool           aWrapContent,
                                                   nsAbsoluteItems& aAbsoluteItems,
                                                   nsIFrame*&       aNewFrame,
                                                   nsAbsoluteItems& aFixedItems,
                                                   nsTableCreator&  aTableCreator)
{
  nsresult rv;

  // Create a table cell frame
  rv = aTableCreator.CreateTableCellFrame(aNewFrame);
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
    nsCOMPtr<nsIStyleContext>  bodyPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::cellContentPseudo,
                                               aStyleContext, PR_FALSE,
                                               getter_AddRefs(bodyPseudoStyle));
    cellBodyFrame->Init(*aPresContext, aContent, aNewFrame, bodyPseudoStyle);

    nsFrameItems childItems;
    if (aWrapContent) {
      // construct a new frame for the content, which is not <TD> content
      rv = ConstructFrame(aPresContext, aContent, cellBodyFrame, aAbsoluteItems, childItems, aFixedItems);
    } else {
      // Process children and set the body cell frame's initial child list
      rv = ProcessChildren(aPresContext, aContent, cellBodyFrame, aAbsoluteItems,
                           childItems, aFixedItems);
    }
    if (NS_SUCCEEDED(rv)) {
      cellBodyFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
      // Set the table cell frame's initial child list
      aNewFrame->SetInitialChildList(*aPresContext, nsnull, cellBodyFrame);
    }
  }

  return rv;
}

// This is only called by table row groups and rows. It allows children that are not
// table related to have a cell wrapped around them.
nsresult
nsCSSFrameConstructor::TableProcessChildren(nsIPresContext*  aPresContext,
                                            nsIContent*      aContent,
                                            nsIFrame*        aParentFrame,
                                            nsAbsoluteItems& aAbsoluteItems,
                                            nsFrameItems&    aChildItems,
                                            nsAbsoluteItems& aFixedItems,
                                            nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;
  
  // Iterate the child content objects and construct a frame
  PRInt32   count;

  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));

  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> childContent;
    
    aContent->ChildAt(i, *getter_AddRefs(childContent));
    rv = TableProcessChild(aPresContext, childContent, aParentFrame, parentStyleContext,
                           aAbsoluteItems, aChildItems, aFixedItems, aTableCreator);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::TableProcessChild(nsIPresContext*  aPresContext,
                                         nsIContent*      aChildContent,
                                         nsIFrame*        aParentFrame,
                                         nsIStyleContext* aParentStyleContext,
                                         nsAbsoluteItems& aAbsoluteItems,
                                         nsFrameItems&    aChildItems,
                                         nsAbsoluteItems& aFixedItems,
                                         nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;
  
  if (nsnull != aChildContent) {
    nsCOMPtr<nsIStyleContext> childStyleContext;
    aPresContext->ResolveStyleContextFor(aChildContent, aParentStyleContext,
                                         PR_FALSE,
                                         getter_AddRefs(childStyleContext));
    const nsStyleDisplay* childDisplay = (const nsStyleDisplay*)
      childStyleContext->GetStyleData(eStyleStruct_Display);
    if ( (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN) ||
         (childDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) ) {
      rv = ConstructFrame(aPresContext, aChildContent, aParentFrame, aAbsoluteItems, aChildItems, aFixedItems);
    } else {
      nsCOMPtr<nsIAtom> tag;
      aChildContent->GetTag(*getter_AddRefs(tag));
      // forms need a frame but it can't be a child of an inner table
      if (nsHTMLAtoms::form == tag) {  
        // if the parent is a table, put the form in the outer table frame
        const nsStyleDisplay* parentDisplay = (const nsStyleDisplay*)
          aParentStyleContext->GetStyleData(eStyleStruct_Display);
        if (parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE) {
          nsIFrame* outerFrame; 
          aParentFrame->GetParent(&outerFrame);
          rv = ConstructFrame(aPresContext, aChildContent, outerFrame, aAbsoluteItems, 
                              aChildItems, aFixedItems);
          // XXX: Seems like this is going into the inner frame's child list instead of the outer frame. - DWH
        } else {
          rv = ConstructFrame(aPresContext, aChildContent, aParentFrame, 
                              aAbsoluteItems, aChildItems, aFixedItems);
        }
      // wrap it in a table cell, row, row group, table if it is a valid tag or display
      // and not whitespace. For example we don't allow map, head, body, etc.
      } else { 
        if (TableIsValidCellContent(aPresContext, aParentFrame, aChildContent)) {
          PRBool needCell = PR_TRUE;
          nsIDOMCharacterData* domData = nsnull;
          nsresult rv2 = aChildContent->QueryInterface(kIDOMCharacterDataIID, (void**)&domData);
          if ((NS_OK == rv2) && (nsnull != domData)) {
            nsString charData;
            domData->GetData(charData);
            charData = charData.StripWhitespace();
            if ((charData.Length() <= 0) && (charData != " ")) { // XXX check this
              needCell = PR_FALSE;  // only contains whitespace, don't create cell
            }
            NS_RELEASE(domData);
          }
          if (needCell) {
            nsIFrame* cellFrame;
            nsIFrame* childFrame; // XXX: Need to change the ConstructTableCell function to return lists of frames. - DWH
            rv = ConstructTableCellFrame(aPresContext, aChildContent, aParentFrame, childStyleContext, 
                                         aAbsoluteItems, childFrame, cellFrame, aFixedItems, aTableCreator);
            aChildItems.AddChild(childFrame);
          }
        }
      }
    }
  }
  return rv;
}

PRBool 
nsCSSFrameConstructor::TableIsValidCellContent(nsIPresContext* aPresContext,
                                               nsIFrame*       aParentFrame,
                                               nsIContent*     aContent)
{
  nsCOMPtr<nsIAtom>  tag;
  aContent->GetTag(*getter_AddRefs(tag));

  nsCOMPtr<nsIStyleContext> styleContext;

  nsresult rv = ResolveStyleContext(aPresContext, aParentFrame, aContent, tag, getter_AddRefs(styleContext));
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  const nsStyleDisplay* display = (const nsStyleDisplay*)
    styleContext->GetStyleData(eStyleStruct_Display);

  if (NS_STYLE_DISPLAY_NONE != display->mDisplay) {
    return PR_FALSE;
  } 
   
  // check tags first
  if (  (nsHTMLAtoms::img      == tag) ||
        (nsHTMLAtoms::hr       == tag) ||
        (nsHTMLAtoms::br       == tag) ||
        (nsHTMLAtoms::wbr      == tag) ||
        (nsHTMLAtoms::input    == tag) ||
        (nsHTMLAtoms::textarea == tag) ||
        (nsHTMLAtoms::select   == tag) ||
        (nsHTMLAtoms::applet   == tag) ||
        (nsHTMLAtoms::embed    == tag) ||
        (nsHTMLAtoms::fieldset == tag) ||
        (nsHTMLAtoms::legend   == tag) ||
        (nsHTMLAtoms::object   == tag) ||
        (nsHTMLAtoms::form     == tag) ||
        (nsHTMLAtoms::iframe   == tag) ||
        (nsHTMLAtoms::spacer   == tag) ||
        (nsHTMLAtoms::button   == tag) ||
        (nsHTMLAtoms::label    == tag    )) {
    return PR_TRUE;
  }

#ifdef INCLUDE_XUL
  if (  (nsXULAtoms::button          == tag)  ||
        (nsXULAtoms::checkbox        == tag)  ||
        (nsXULAtoms::radio           == tag)  ||
        (nsXULAtoms::text            == tag)  ||
        (nsXULAtoms::widget          == tag)  ||
        (nsXULAtoms::tree            == tag)  ||
        (nsXULAtoms::treechildren    == tag)  ||
        (nsXULAtoms::treeitem        == tag)  ||
        (nsXULAtoms::treecell        == tag)  ||
        (nsXULAtoms::treeindentation == tag)  ||
        (nsXULAtoms::toolbox         == tag)  ||
        (nsXULAtoms::toolbar         == tag)  ||
        (nsXULAtoms::progressmeter   == tag    )) {
    return PR_TRUE;
  }
#endif
  
  // we should check for display type as well - later
  return PR_FALSE;
}

nsresult
nsCSSFrameConstructor::TableProcessChildLists(nsIPresContext* aPresContext,
                                              nsDeque* aParentChildPairs)
{
  if (aParentChildPairs) {
    nsIFrame* child;
    nsIFrame* parent = (nsIFrame*)aParentChildPairs->PopFront();
    while (nsnull != parent) {
      child = (nsIFrame*)aParentChildPairs->PopFront();
      parent->SetInitialChildList(*aPresContext, nsnull, child);
      parent = (nsIFrame*)aParentChildPairs->PopFront();
    }
  }
  return NS_OK;
}


nsIFrame*
nsCSSFrameConstructor::TableGetAsNonScrollFrame(nsIPresContext*       aPresContext, 
                                                nsIFrame*             aFrame, 
                                                const nsStyleDisplay* aDisplay) 
{
  if (nsnull == aFrame) {
    return nsnull;
  }
  nsIFrame* result = aFrame;
  if (IsScrollable(aPresContext, aDisplay)) {
    aFrame->FirstChild(nsnull, &result);
  }
  return result;
}

//  nsIAtom* pseudoTag;
//  styleContext->GetPseudoType(pseudoTag);
//  if (pseudoTag != nsHTMLAtoms::scrolledContentPseudo) {
//  NS_IF_RELEASE(pseudoTag);

const nsStyleDisplay* 
nsCSSFrameConstructor:: GetDisplay(nsIFrame* aFrame)
{
  if (nsnull == aFrame) {
    return nsnull;
  }
  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));
  const nsStyleDisplay* display = 
    (const nsStyleDisplay*)styleContext->GetStyleData(eStyleStruct_Display);
  return display;
}

/***********************************************
 * END TABLE SECTION
 ***********************************************/

nsresult
nsCSSFrameConstructor::ConstructDocElementFrame(nsIPresContext*  aPresContext,
                                                nsIContent*      aDocElement,
                                                nsIFrame*        aParentFrame,
                                                nsIStyleContext* aParentStyleContext,
                                                nsIFrame*&       aNewFrame,
                                                nsAbsoluteItems& aFixedItems)
{
  // Resolve the style context for the document element
  nsCOMPtr<nsIStyleContext>  styleContext;
  aPresContext->ResolveStyleContextFor(aDocElement, aParentStyleContext,
                                       PR_FALSE,
                                       getter_AddRefs(styleContext));
  
  // See if we're paginated
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    // Create an area frame for the document element
    nsIFrame* areaFrame;

    NS_NewAreaFrame(areaFrame, 0);
    areaFrame->Init(*aPresContext, aDocElement, aParentFrame, styleContext);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;

    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems, childItems,
                    aFixedItems);
    
    // Set the initial child lists
    areaFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }

    // Return the area frame
    aNewFrame = areaFrame;
  
  } else {
    // Unless the 'overflow' policy forbids scrolling, wrap the frame in a
    // scroll frame.
    nsIFrame* scrollFrame = nsnull;

    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    if (IsScrollable(aPresContext, display)) {
      NS_NewScrollFrame(scrollFrame);
      scrollFrame->Init(*aPresContext, aDocElement, aParentFrame, styleContext);
    
      // The scrolled frame gets a pseudo element style context
      nsCOMPtr<nsIStyleContext>  scrolledPseudoStyle;
      aPresContext->ResolvePseudoStyleContextFor(nsnull,
                                                 nsHTMLAtoms::scrolledContentPseudo,
                                                 styleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(scrolledPseudoStyle));
      styleContext = scrolledPseudoStyle;
    }

    // Create an area frame for the document element. This serves as the
    // "initial containing block"
    nsIFrame* areaFrame;

    // XXX Until we clean up how painting damage is handled, we need to use the
    // flag that says that this is the body...
    NS_NewAreaFrame(areaFrame, NS_BLOCK_DOCUMENT_ROOT|NS_BLOCK_MARGIN_ROOT);
    areaFrame->Init(*aPresContext, aDocElement, scrollFrame ? scrollFrame :
                    aParentFrame, styleContext);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, areaFrame,
                                             styleContext, PR_FALSE);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;
    
    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems, childItems,
                    aFixedItems);
    
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
nsCSSFrameConstructor::ConstructRootFrame(nsIPresContext* aPresContext,
                                          nsIContent*     aDocElement,
                                          nsIFrame*&      aNewFrame)
{
#ifdef NS_DEBUG
  nsCOMPtr<nsIDocument>  doc;
  nsIContent*            rootContent;

  // Verify that the content object is really the root content object
  aDocElement->GetDocument(*getter_AddRefs(doc));
  rootContent = doc->GetRootContent();
  NS_ASSERTION(rootContent == aDocElement, "unexpected content");
  NS_RELEASE(rootContent);
#endif

  nsIFrame*                 viewportFrame;
  nsCOMPtr<nsIStyleContext> viewportPseudoStyle;

  // Create the viewport frame
  NS_NewViewportFrame(viewportFrame);

  // Create a pseudo element style context
  aPresContext->ResolvePseudoStyleContextFor(nsnull, nsLayoutAtoms::viewportPseudo,
                                             nsnull, PR_FALSE,
                                             getter_AddRefs(viewportPseudoStyle));

  // Initialize the viewport frame. It has a NULL content object
  viewportFrame->Init(*aPresContext, nsnull, nsnull, viewportPseudoStyle);

  // Bind the viewport frame to the root view
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIViewManager> viewManager;
  presShell->GetViewManager(getter_AddRefs(viewManager));
  nsIView*        rootView;

  viewManager->GetRootView(rootView);
  viewportFrame->SetView(rootView);

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
    // XXX should probably be a scrolled content pseudo style context
    scrollFrame->Init(*aPresContext, nsnull, viewportFrame, viewportPseudoStyle);
  }

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    nsIFrame* pageSequenceFrame;

    // Create a page sequence frame
    NS_NewSimplePageSequenceFrame(pageSequenceFrame);
    // XXX should probably be a page sequence pseudo style context
    pageSequenceFrame->Init(*aPresContext, nsnull, isScrollable ? scrollFrame :
                            viewportFrame, viewportPseudoStyle);
    if (isScrollable) {
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, pageSequenceFrame,
                                               viewportPseudoStyle, PR_TRUE);
    }

    // Create the first page
    nsIFrame* pageFrame;
    NS_NewPageFrame(pageFrame);

    // The page is the containing block for 'fixed' elements. which are repeated
    // on every page
    mFixedContainingBlock = pageFrame;

    // Initialize the page and force it to have a view. This makes printing of
    // the pages easier and faster.
    // XXX Use a PAGE style context...
    pageFrame->Init(*aPresContext, nsnull, pageSequenceFrame, viewportPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, pageFrame,
                                             viewportPseudoStyle, PR_TRUE);

    // The eventual parent of the document element frame
    mDocElementContainingBlock = pageFrame;


    // Set the initial child lists
    pageSequenceFrame->SetInitialChildList(*aPresContext, nsnull, pageFrame);
    if (isScrollable) {
      scrollFrame->SetInitialChildList(*aPresContext, nsnull, pageSequenceFrame);
      viewportFrame->SetInitialChildList(*aPresContext, nsnull, scrollFrame);
    } else {
      viewportFrame->SetInitialChildList(*aPresContext, nsnull, pageSequenceFrame);
    }

  } else {
    // The viewport is the containing block for 'fixed' elements
    mFixedContainingBlock = viewportFrame;

    // Create the root frame. The document element's frame is a child of the
    // root frame.
    //
    // Note: the major reason we need the root frame is to implement margins for
    // the document element's frame. If we didn't need to support margins on the
    // document element's frame, then we could eliminate the root frame and make
    // the document element frame a child of the viewport (or its scroll frame)
    nsIFrame* rootFrame;
    NS_NewRootFrame(rootFrame);

    // XXX this should be a root pseudo style context
    rootFrame->Init(*aPresContext, nsnull, isScrollable ? scrollFrame :
                    viewportFrame, viewportPseudoStyle);
    if (isScrollable) {
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, rootFrame,
                                               viewportPseudoStyle, PR_TRUE);
    }

    // The eventual parent of the document element frame
    mDocElementContainingBlock = rootFrame;

    // Set the initial child lists
    if (isScrollable) {
      scrollFrame->SetInitialChildList(*aPresContext, nsnull, rootFrame);
      viewportFrame->SetInitialChildList(*aPresContext, nsnull, scrollFrame);
    } else {
      viewportFrame->SetInitialChildList(*aPresContext, nsnull, rootFrame);
    }
  }

  aNewFrame = viewportFrame;
  return NS_OK;  
}

nsresult
nsCSSFrameConstructor::CreatePlaceholderFrameFor(nsIPresContext*  aPresContext,
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
    nsCOMPtr<nsIStyleContext>  placeholderPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::placeholderPseudo,
                                               aStyleContext,
                                               PR_FALSE,
                                               getter_AddRefs(placeholderPseudoStyle));
    placeholderFrame->Init(*aPresContext, aContent, aParentFrame,
                           placeholderPseudoStyle);
  
    // Add mapping from absolutely positioned frame to its placeholder frame
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->SetPlaceholderFrameFor(aFrame, placeholderFrame);
  
    aPlaceholderFrame = placeholderFrame;
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructSelectFrame(nsIPresContext*  aPresContext,
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
        aAbsoluteItems.AddChild(aNewFrame);

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
nsCSSFrameConstructor::ConstructFrameByTag(nsIPresContext*  aPresContext,
                                           nsIContent*      aContent,
                                           nsIFrame*        aParentFrame,
                                           nsIAtom*         aTag,
                                           nsIStyleContext* aStyleContext,
                                           nsAbsoluteItems& aAbsoluteItems,
                                           nsFrameItems&    aFrameItems,
                                           nsAbsoluteItems& aFixedItems)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    canBePositioned = PR_TRUE;
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
      
      // See if the element is absolute or fixed positioned
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
      }
      if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
      }

      // Create a frame based on the tag
      if (nsHTMLAtoms::img == aTag) {
        // XXX If image display is turned off, then use ConstructAlternateImageFrame()
        // instead...
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
        canBePositioned = PR_FALSE;
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
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        rv = NS_NewHTMLFrameOuterFrame(newFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        rv = NS_NewSpacerFrame(newFrame);
        canBePositioned = PR_FALSE;
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
      nsIFrame* geometricParent = aParentFrame;
       
      if (canBePositioned) {
        if (isAbsolutelyPositioned) {
          geometricParent = aAbsoluteItems.containingBlock;
        } else if (isFixedPositioned) {
          geometricParent = aFixedItems.containingBlock;
        }
      }
      
      newFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

      // See if we need to create a view, e.g. the frame is absolutely positioned
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                             childItems, aFixedItems);
      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }

    // If the frame is positioned then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                aParentFrame, placeholderFrame);

      // Add the positioned frame to its containing block's list of child frames
      if (isAbsolutelyPositioned) {
        aAbsoluteItems.AddChild(newFrame);
      } else {
        aFixedItems.AddChild(newFrame);
      }

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
nsCSSFrameConstructor::ConstructXULFrame(nsIPresContext*  aPresContext,
                                         nsIContent*      aContent,
                                         nsIFrame*        aParentFrame,
                                         nsIAtom*         aTag,
                                         nsIStyleContext* aStyleContext,
                                         nsAbsoluteItems& aAbsoluteItems,
                                         nsFrameItems&    aFrameItems,
                                         nsAbsoluteItems& aFixedItems,
                                         PRBool&          haltProcessing)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
  nsIFrame* ignore = nsnull; // I have no idea what this is used for.
  nsTreeCreator treeCreator; // Used to make tree views.

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
      rv = NS_NewButtonControlFrame(newFrame);
    else if (aTag == nsXULAtoms::checkbox)
      rv = NS_NewCheckboxControlFrame(newFrame);
    else if (aTag == nsXULAtoms::radio)
      rv = NS_NewRadioControlFrame(newFrame);
    else if (aTag == nsXULAtoms::text)
      rv = NS_NewTextControlFrame(newFrame);
  else if (aTag == nsXULAtoms::widget)
    rv = NS_NewObjectFrame(newFrame);
  
  // TREE CONSTRUCTION
  // The following code is used to construct a tree view from the XUL content
  // model.  It has to take the hierarchical tree content structure and build a flattened
  // table row frame structure.
  else if (aTag == nsXULAtoms::tree)
  {
      nsIFrame* geometricParent = aParentFrame;
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
        aParentFrame = aAbsoluteItems.containingBlock;
      }
      if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
        aParentFrame = aFixedItems.containingBlock;
      }
      rv = ConstructTableFrame(aPresContext, aContent, geometricParent, aStyleContext,
                               aAbsoluteItems, newFrame, aFixedItems, treeCreator);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned || isFixedPositioned) {
        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                  aParentFrame, placeholderFrame);

        // Add the positioned frame to its containing block's list of child frames
        if (isAbsolutelyPositioned) {
          aAbsoluteItems.AddChild(newFrame);
        } else {
          aFixedItems.AddChild(newFrame);
        }

        // Add the placeholder frame to the flow
        aFrameItems.AddChild(placeholderFrame);
      
      } else {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  }
  else if (aTag == nsXULAtoms::treechildren)
  {
	  haltProcessing = PR_TRUE;
	  return rv; // This is actually handled by the treeitem node.
  }
  else if (aTag == nsXULAtoms::treeitem)
  {
    // A tree item causes a table row to be constructed that is always
    // slaved to the nearest enclosing table row group (regardless of how
    // deeply nested it is within other tree items).
	rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                aAbsoluteItems, newFrame, ignore, aFixedItems, treeCreator);
    aFrameItems.AddChild(newFrame);

	// We need to find the treechildren node that is a child of this node
	// and we need to construct new rows.
	PRInt32 aChildCount;
	aContent->ChildCount(aChildCount);
	for (PRInt32 i = 0; i < aChildCount; i++) 
	{
		nsCOMPtr<nsIContent> childContent;
		
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent))))
    {
		  // Construct a child frame
		  nsCOMPtr<nsIAtom> pTag;
		  childContent->GetTag(*getter_AddRefs(pTag));
		  if (pTag == nsXULAtoms::treechildren)
		  {
			  // We want to call ConstructFrame to keep building rows, but only if we're
			  // open.
			  nsString attrValue;
			  nsIAtom* kOpenAtom = NS_NewAtom("open");
			  nsresult result = aContent->GetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, attrValue);
			  attrValue.ToLowerCase();
			  processChildren =  (result == NS_CONTENT_ATTR_NO_VALUE ||
				(result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
			  NS_RELEASE(kOpenAtom);
			  
			  // If we do need to process, we have to "skip" this content node, since it
			  // doesn't really have any associated display.
  
			  if (processChildren)
			  {
				rv = ProcessChildren(aPresContext, childContent, aParentFrame, aAbsoluteItems,
						   aFrameItems, aFixedItems);
			  }
		  }
		}
	}
    return rv;
  
	// Note: See later in this method.  More processing has to be done after the
	// tree item has constructed its children and after this frame has been added
	// to our list.
  }
  else if (aTag == nsXULAtoms::treecell)
  {
    // We make a tree cell frame and process the children.
	// Find out what the attribute value for event allowance is.
	nsString attrValue;
    nsresult result = aContent->GetAttribute(nsXULAtoms::nameSpaceID, nsXULAtoms::treeallowevents, attrValue);
    attrValue.ToLowerCase();
    PRBool allowEvents =  (result == NS_CONTENT_ATTR_NO_VALUE ||
					      (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
    rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                 aAbsoluteItems, newFrame, ignore, aFixedItems, treeCreator);
    aFrameItems.AddChild(newFrame);
	((nsTreeCellFrame*)newFrame)->SetAllowEvents(allowEvents);
    return rv;
  }
  else if (aTag == nsXULAtoms::treeindentation)
  {
    rv = NS_NewTreeIndentationFrame(newFrame);
  }
  // End of TREE CONSTRUCTION code here (there's more later on in the function)

  // TOOLBAR CONSTRUCTION
  else if (aTag == nsXULAtoms::toolbox) {
    processChildren = PR_TRUE;
    rv = NS_NewToolboxFrame(newFrame);
  }
  else if (aTag == nsXULAtoms::toolbar) {
    processChildren = PR_TRUE;
    rv = NS_NewToolbarFrame(newFrame);
  }
  // End of TOOLBAR CONSTRUCTION logic

  // PROGRESS METER CONSTRUCTION
  else if (aTag == nsXULAtoms::progressmeter) {
    processChildren = PR_TRUE;
    rv = NS_NewProgressMeterFrame(newFrame);
  }
  // End of PROGRESS METER CONSTRUCTION logic

  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {
    nsIFrame* geometricParent = isAbsolutelyPositioned ? aAbsoluteItems.containingBlock :
                                                         aParentFrame;
    newFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(newFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren)
      rv = ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                           childItems, aFixedItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  
    // If the frame is absolutely positioned then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                aParentFrame, placeholderFrame);

      // Add the positioned frame to its containing block's list of child frames
      if (isAbsolutelyPositioned) {
        aAbsoluteItems.AddChild(newFrame);
	  } else {
        aFixedItems.AddChild(newFrame);
	  }

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
	}
  }

  return rv;
}

#endif

nsresult
nsCSSFrameConstructor::InitializeScrollFrame(nsIFrame*        scrollFrame,
                                             nsIPresContext*  aPresContext,
                                             nsIContent*      aContent,
                                             nsIFrame*        aParentFrame,
                                             nsIStyleContext* aStyleContext,
                                             nsAbsoluteItems& aAbsoluteItems,
                                             nsIFrame*&       aNewFrame,
                                             nsAbsoluteItems& aFixedItems,
                                             PRBool           aIsAbsolutelyPositioned,
                                             PRBool           aIsFixedPositioned,
                                             PRBool           aCreateBlock)
{
    // Initialize it
    nsIFrame* geometricParent = aParentFrame;
    
    if (aIsAbsolutelyPositioned) {
      geometricParent = aAbsoluteItems.containingBlock;
    } else if (aIsFixedPositioned) {
      geometricParent = aFixedItems.containingBlock;
    }
    scrollFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext);

    // The scroll frame gets the original style context, and the scrolled
    // frame gets a SCROLLED-CONTENT pseudo element style context that
    // inherits the background properties
    nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::scrolledContentPseudo,
                                               aStyleContext, PR_FALSE,
                                               getter_AddRefs(scrolledPseudoStyle));

    // Create an area container for the frame
    nsIFrame* scrolledFrame;
    NS_NewAreaFrame(scrolledFrame, NS_BLOCK_SHRINK_WRAP);

    // Initialize the frame and force it to have a view
    scrolledFrame->Init(*aPresContext, aContent, scrollFrame, scrolledPseudoStyle);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, scrolledFrame,
                                             scrolledPseudoStyle, PR_TRUE);

    // Process children
    if (aIsAbsolutelyPositioned | aIsFixedPositioned) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      nsAbsoluteItems  absoluteItems(scrolledFrame);
      nsFrameItems     childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, absoluteItems,
                      childItems, aFixedItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
      if (nsnull != absoluteItems.childList) {
        scrolledFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                           absoluteItems.childList);
      }

    } else {
      nsFrameItems childItems;
      ProcessChildren(aPresContext, aContent, scrolledFrame, aAbsoluteItems,
                      childItems, aFixedItems);
  
      // Set the initial child lists
      scrolledFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
    scrollFrame->SetInitialChildList(*aPresContext, nsnull, scrolledFrame);
    aNewFrame = scrollFrame;


    return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructFrameByDisplayType(nsIPresContext*       aPresContext,
                                                   const nsStyleDisplay* aDisplay,
                                                   nsIContent*           aContent,
                                                   nsIFrame*             aParentFrame,
                                                   nsIStyleContext*      aStyleContext,
                                                   nsAbsoluteItems&      aAbsoluteItems,
                                                   nsFrameItems&         aFrameItems,
                                                   nsAbsoluteItems&      aFixedItems)
{
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isBlock = aDisplay->IsBlockLevel();
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsTableCreator tableCreator; // Used to make table frames.
  nsresult  rv = NS_OK;

  // Get the position syle info
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);

  // The frame is also a block if it's an inline frame that's floated or
  // absolutely positioned
  if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) &&
      ((NS_STYLE_FLOAT_NONE != aDisplay->mFloats) || position->IsAbsolutelyPositioned())) {
    isBlock = PR_TRUE;
  }

  // If the frame is a block-level frame and is scrollable, then wrap it
  // in a scroll frame.
  // XXX Applies to replaced elements, too, but how to tell if the element
  // is replaced?
  // XXX Ignore tables for the time being
  if ((isBlock && (aDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE)) &&
      IsScrollable(aPresContext, aDisplay)) {

    // See if it's absolute positioned or fixed positioned
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
      isAbsolutelyPositioned = PR_TRUE;
    } else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
      isFixedPositioned = PR_TRUE;
    }

    // Create a scroll frame
    nsIFrame* scrollFrame;
    NS_NewScrollFrame(scrollFrame);

    // Initialize it
    InitializeScrollFrame(scrollFrame, aPresContext, aContent, aParentFrame,  aStyleContext,
                          aAbsoluteItems,  newFrame, aFixedItems,
                          isAbsolutelyPositioned, isFixedPositioned, PR_FALSE);

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

  // See if the frame is absolute or fixed positioned
  } else if (position->IsAbsolutelyPositioned() &&
             ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
              (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {

    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
      isAbsolutelyPositioned = PR_TRUE;
    } else {
      isFixedPositioned = PR_TRUE;
    }

    // Create an area frame
    NS_NewAreaFrame(newFrame, 0);
    newFrame->Init(*aPresContext, aContent, isAbsolutelyPositioned ?
                   aAbsoluteItems.containingBlock : aFixedItems.containingBlock,
                   aStyleContext);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. The area frame becomes a container for child
    // frames that are absolutely positioned
    nsAbsoluteItems  absoluteItems(newFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems, childItems,
                    aFixedItems);

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
    ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems, childItems,
                    aFixedItems);

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
    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems, childItems,
                    aFixedItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      newFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                    absoluteItems.childList);
    }

  } else {
    PRBool  processChildren = PR_FALSE;  // whether we should process child content
    nsIFrame* ignore;

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
      nsIFrame* geometricParent = aParentFrame;
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
        aParentFrame = aAbsoluteItems.containingBlock;
      }
      if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
        aParentFrame = aFixedItems.containingBlock;
      }
      rv = ConstructTableFrame(aPresContext, aContent, geometricParent, aStyleContext,
                               aAbsoluteItems, newFrame, aFixedItems, tableCreator);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned || isFixedPositioned) {
        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                                  aParentFrame, placeholderFrame);

        // Add the positioned frame to its containing block's list of child frames
        if (isAbsolutelyPositioned) {
          aAbsoluteItems.AddChild(newFrame);
        } else {
          aFixedItems.AddChild(newFrame);
        }

        // Add the placeholder frame to the flow
        aFrameItems.AddChild(placeholderFrame);
      
      } else {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      return rv;
    }
  
    // the next 5 cases are only relevant if the parent is not a table, ConstructTableFrame handles children
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
    {
      rv = ConstructTableCaptionFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                      aAbsoluteItems, ignore, newFrame, aFixedItems, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    {
      PRBool isRowGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != aDisplay->mDisplay);
      rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                    aAbsoluteItems, isRowGroup, newFrame, ignore, aFixedItems, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
   
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      rv = ConstructTableColFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                  aAbsoluteItems, newFrame, ignore, aFixedItems, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
  
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                  aAbsoluteItems, newFrame, ignore, aFixedItems, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                   aAbsoluteItems, newFrame, ignore, aFixedItems, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
  
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
                             childItems, aFixedItems);
      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    }
  }

  // If the frame is absolutely positioned then create a placeholder frame
  if (isAbsolutelyPositioned || isFixedPositioned) {
    nsIFrame* placeholderFrame;

    CreatePlaceholderFrameFor(aPresContext, aContent, newFrame, aStyleContext,
                              aParentFrame, placeholderFrame);

    // Add the positioned frame to its containing block's list of child frames
    if (isAbsolutelyPositioned) {
      aAbsoluteItems.AddChild(newFrame);
    } else {
      aFixedItems.AddChild(newFrame);
    }

    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);

  } else if (nsnull != newFrame) {
    // Add the frame we just created to the flowed list
    aFrameItems.AddChild(newFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::GetAdjustedParentFrame(nsIFrame*  aCurrentParentFrame, 
                                              PRUint8    aChildDisplayType, 
                                              nsIFrame*& aNewParentFrame)
{
  NS_PRECONDITION(nsnull!=aCurrentParentFrame, "bad arg aCurrentParentFrame");

  nsresult rv = NS_OK;
  // by default, the new parent frame is the given current parent frame
  aNewParentFrame = aCurrentParentFrame;
  if (nsnull != aCurrentParentFrame) {
    const nsStyleDisplay* currentParentDisplay;
    aCurrentParentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)currentParentDisplay);
    if (NS_STYLE_DISPLAY_TABLE == currentParentDisplay->mDisplay) {
      if (NS_STYLE_DISPLAY_TABLE_CAPTION != aChildDisplayType) {
        nsIFrame *innerTableFrame = nsnull;
        aCurrentParentFrame->FirstChild(nsnull, &innerTableFrame);
        if (nsnull != innerTableFrame) {
          const nsStyleDisplay* innerTableDisplay;
          innerTableFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)innerTableDisplay);
          if (NS_STYLE_DISPLAY_TABLE == innerTableDisplay->mDisplay) { 
            // we were given the outer table frame, use the inner table frame
            aNewParentFrame=innerTableFrame;
          } // else we were already given the inner table frame
        } // else the current parent has no children and cannot be an outer table frame       
      } // else the child is a caption and really belongs to the outer table frame     
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  NS_POSTCONDITION(nsnull!=aNewParentFrame, "bad result null aNewParentFrame");
  return rv;
}

PRBool
nsCSSFrameConstructor::IsScrollable(nsIPresContext*       aPresContext,
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
nsCSSFrameConstructor::ResolveStyleContext(nsIPresContext*   aPresContext,
                                           nsIFrame*         aParentFrame,
                                           nsIContent*       aContent,
                                           nsIAtom*          aTag,
                                           nsIStyleContext** aStyleContext)
{
  nsresult rv = NS_OK;
  // Resolve the style context based on the content object and the parent
  // style context
  nsCOMPtr<nsIStyleContext> parentStyleContext;

  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  if (nsLayoutAtoms::textTagName == aTag) {
    // Use a special pseudo element style context for text
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::textPseudo, 
                                                    parentStyleContext,
                                                    PR_FALSE,
                                                    aStyleContext);
  } else if (nsLayoutAtoms::commentTagName == aTag) {
    // Use a special pseudo element style context for comments
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::commentPseudo, 
                                                    parentStyleContext,
                                                    PR_FALSE,
                                                    aStyleContext);
  } else {
    rv = aPresContext->ResolveStyleContextFor(aContent, parentStyleContext,
                                              PR_FALSE,
                                              aStyleContext);
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructFrame(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsIFrame*        aParentFrame,
                                      nsAbsoluteItems& aAbsoluteItems,
                                      nsFrameItems&    aFrameItems,
                                      nsAbsoluteItems& aFixedItems)
{
  NS_PRECONDITION(nsnull != aParentFrame, "no parent frame");

  nsresult  rv;

  // Get the element's tag
  nsCOMPtr<nsIAtom>  tag;
  aContent->GetTag(*getter_AddRefs(tag));

  nsCOMPtr<nsIStyleContext> styleContext;
  rv = ResolveStyleContext(aPresContext, aParentFrame, aContent, tag, getter_AddRefs(styleContext));

#ifdef chris_needs_to_remove_this
  // Resolve the style context based on the content object and the parent
  // style context
  nsCOMPtr<nsIStyleContext> styleContext;
  nsCOMPtr<nsIStyleContext> parentStyleContext;

  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  if (nsLayoutAtoms::textTagName == tag) {
    // Use a special pseudo element style context for text
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::textPseudo, 
                                                    parentStyleContext,
                                                    PR_FALSE,
                                                    getter_AddRefs(styleContext));
  } else if (nsLayoutAtoms::commentTagName == tag) {
    // Use a special pseudo element style context for comments
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::commentPseudo, 
                                                    parentStyleContext,
                                                    PR_FALSE,
                                                    getter_AddRefs(styleContext));
  } else {
    rv = aPresContext->ResolveStyleContextFor(aContent, parentStyleContext,
                                              PR_FALSE,
                                              getter_AddRefs(styleContext));
  }
#endif

  if (NS_SUCCEEDED(rv)) {
    // Pre-check for display "none" - if we find that, don't create
    // any frame at all
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_DISPLAY_NONE != display->mDisplay) {

      nsIFrame* lastChild = aFrameItems.lastChild;

      // Handle specific frame types
      rv = ConstructFrameByTag(aPresContext, aContent, aParentFrame, tag,
                               styleContext, aAbsoluteItems, aFrameItems,
                               aFixedItems);

#ifdef INCLUDE_XUL
      // Failing to find a matching HTML frame, try creating a specialized
      // XUL frame. This is temporary, pending planned factoring of this
      // whole process into separate, pluggable steps.
      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                             (lastChild == aFrameItems.lastChild))) {
        PRBool haltProcessing = PR_FALSE;
        rv = ConstructXULFrame(aPresContext, aContent, aParentFrame, tag,
                               styleContext, aAbsoluteItems, aFrameItems,
                               aFixedItems, haltProcessing);
        if (haltProcessing) {
          return rv;
        }
      } 
#endif

      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                               (lastChild == aFrameItems.lastChild))) {
        // When there is no explicit frame to create, assume it's a
        // container and let display style dictate the rest
        rv = ConstructFrameByDisplayType(aPresContext, display, aContent, aParentFrame,
                                         styleContext, aAbsoluteItems, aFrameItems,
                                         aFixedItems);
      }
    }
  }
  
  return rv;
}

// XXX we need aFrameSubTree's prev-sibling in order to properly
// place its replacement!
NS_IMETHODIMP  
nsCSSFrameConstructor::ReconstructFrames(nsIPresContext* aPresContext,
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
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv)) {
      // XXX This API needs changing, because it appears to be designed for
      // an arbitrary content element, but yet it always constructs the document
      // element's frame. Plus it has the problem noted above in the previous XXX
      rv = aParentFrame->RemoveFrame(*aPresContext, *shell,
                                     nsnull, aFrameSubTree);

      // XXX Remove any existing fixed items...

      if (NS_SUCCEEDED(rv)) {
        nsIFrame*                 newChild;
        nsCOMPtr<nsIStyleContext> rootPseudoStyle;
        nsAbsoluteItems           fixedItems(nsnull);  // XXX FIX ME...

        aParentFrame->GetStyleContext(getter_AddRefs(rootPseudoStyle));
        rv = ConstructDocElementFrame(aPresContext, aContent,
                                      aParentFrame, rootPseudoStyle, newChild,
                                      fixedItems);
        // XXX Do something with the fixed items...
        if (NS_SUCCEEDED(rv)) {
          rv = aParentFrame->InsertFrames(*aPresContext, *shell,
                                          nsnull, nsnull, newChild);
        }
      }
    }  
    NS_RELEASE(document);
  }

  return rv;
}

nsIFrame*
nsCSSFrameConstructor::GetFrameFor(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                                   nsIContent* aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame;
  aPresShell->GetPrimaryFrameFor(aContent, &frame);

  if (nsnull != frame) {
    // If the primary frame is a scroll frame, then get the scrolled frame.
    // That's the frame that gets the reflow command
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    if (display->IsBlockLevel() && IsScrollable(aPresContext, display)) {
      frame->FirstChild(nsnull, &frame);
    } else if (NS_STYLE_DISPLAY_TABLE == display->mDisplay) { // we got an outer table frame, need an inner
      frame->FirstChild(nsnull, &frame);                      // the inner frame is always the 1st child
    }    
  }

  return frame;
}

nsIFrame*
nsCSSFrameConstructor::GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
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
    containingBlock->GetParent(&containingBlock);
  }

  // If we didn't find an absolutely positioned containing block, then use the
  // initial containing block
  if (nsnull == containingBlock) {
    containingBlock = mInitialContainingBlock;
  }
  
  return containingBlock;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentAppended(nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       PRInt32         aNewIndexInContainer)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
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
    nsIFrame*       firstAppendedFrame = nsnull;
    nsAbsoluteItems absoluteItems(absoluteContainingBlock);
    nsAbsoluteItems fixedItems(mFixedContainingBlock);
    nsFrameItems    frameItems;

    aContainer->ChildCount(count);

    for (PRInt32 i = aNewIndexInContainer; i < count; i++) {
      nsCOMPtr<nsIContent> child;
      
      aContainer->ChildAt(i, *getter_AddRefs(child));
      ConstructFrame(aPresContext, child, parentFrame, absoluteItems, frameItems,
                     fixedItems);
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

    // Notify the parent frame passing it the list of new frames
    if (NS_SUCCEEDED(result)) {
      result = adjustedParentFrame->AppendFrames(*aPresContext, *shell,
                                                 nsnull, firstAppendedFrame);

      // If there are new absolutely positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (nsnull != absoluteItems.childList) {
        absoluteItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                    nsLayoutAtoms::absoluteList,
                                                    absoluteItems.childList);
      }

      // If there are new fixed positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (nsnull != fixedItems.childList) {
        fixedItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                 nsLayoutAtoms::fixedList,
                                                 fixedItems.childList);
      }
    }
  }

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
    nsCOMPtr<nsIContent> precedingContent;

    aContainer->ChildAt(index, *getter_AddRefs(precedingContent));
    aPresShell->GetPrimaryFrameFor(precedingContent, &prevSibling);

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
    nsCOMPtr<nsIContent> nextContent;

    aContainer->ChildAt(index, *getter_AddRefs(nextContent));
    aPresShell->GetPrimaryFrameFor(nextContent, &nextSibling);

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
nsCSSFrameConstructor::ContentInserted(nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       nsIContent*     aChild,
                                       PRInt32         aIndexInContainer)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult rv = NS_OK;

  // If we have a null parent, then this must be the document element
  // being inserted
  if (nsnull == aContainer) {

    NS_PRECONDITION(nsnull == mInitialContainingBlock, "initial containing block already created");
    nsCOMPtr<nsIStyleContext> rootPseudoStyle;
    nsIContent* docElement = nsnull;

    docElement = mDocument->GetRootContent();

    // Create a pseudo element style context
    nsCOMPtr<nsIStyleContext> containerStyle;
    mDocElementContainingBlock->GetStyleContext(getter_AddRefs(containerStyle));
    // XXX Use a different pseudo style context...
    aPresContext->ResolvePseudoStyleContextFor(nsnull, nsHTMLAtoms::rootPseudo,
                                               containerStyle, PR_FALSE,
                                               getter_AddRefs(rootPseudoStyle));
    
    // Create frames for the document element and its child elements
    nsIFrame*       docElementFrame;
    nsAbsoluteItems fixedItems(mFixedContainingBlock);
    ConstructDocElementFrame(aPresContext, 
                             docElement, 
                             mDocElementContainingBlock,
                             rootPseudoStyle, 
                             docElementFrame, 
                             fixedItems);
    NS_IF_RELEASE(docElement);
    
    // Set the initial child list for the parent
    mDocElementContainingBlock->SetInitialChildList(*aPresContext, 
                                                    nsnull, 
                                                    docElementFrame);
    
    // Tell the fixed containing block about its 'fixed' frames
    if (nsnull != fixedItems.childList) {
      mFixedContainingBlock->SetInitialChildList(*aPresContext, 
                                                 nsLayoutAtoms::fixedList,
                                                 fixedItems.childList);
    }
    
  }
  else {
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
      shell->GetPrimaryFrameFor(aContainer, &parentFrame);
      
    } else {
      // Use the prev sibling if we have it; otherwise use the next sibling
      if (nsnull != prevSibling) {
        prevSibling->GetParent(&parentFrame);
      } else {
        nextSibling->GetParent(&parentFrame);
      }
    }

    // Construct a new frame
    if (nsnull != parentFrame) {
      // Get the containing block for absolutely positioned elements
      nsIFrame* absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext,
                                                                     parentFrame);
      
      nsAbsoluteItems absoluteItems(absoluteContainingBlock);
      nsFrameItems    frameItems;
      nsAbsoluteItems fixedItems(mFixedContainingBlock);
      rv = ConstructFrame(aPresContext, aChild, parentFrame, absoluteItems, frameItems,
                          fixedItems);
      
      nsIFrame* newFrame = frameItems.childList;
      
      if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
        // Notify the parent frame
        if (isAppend) {
          rv = parentFrame->AppendFrames(*aPresContext, *shell,
                                         nsnull, newFrame);
        } else {
          rv = parentFrame->InsertFrames(*aPresContext, *shell, nsnull,
                                         prevSibling, newFrame);
        }
        
        // If there are new absolutely positioned child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (nsnull != absoluteItems.childList) {
          rv = absoluteItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                           nsLayoutAtoms::absoluteList,
                                                           absoluteItems.childList);
        }
        
        // If there are new fixed positioned child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (nsnull != fixedItems.childList) {
          rv = fixedItems.containingBlock->AppendFrames(*aPresContext, *shell,
                                                        nsLayoutAtoms::fixedList,
                                                        fixedItems.childList);
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentReplaced(nsIPresContext* aPresContext,
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
nsCSSFrameConstructor::ContentRemoved(nsIPresContext* aPresContext,
                                      nsIContent*     aContainer,
                                      nsIContent*     aChild,
                                      PRInt32         aIndexInContainer)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* childFrame;
  shell->GetPrimaryFrameFor(aChild, &childFrame);

  if (nsnull != childFrame) {
    // See if it's absolutely positioned
    const nsStylePosition* position;
    childFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if (position->IsAbsolutelyPositioned()) {
      // Get the placeholder frame
      nsIFrame* placeholderFrame;
      shell->GetPlaceholderFrameFor(childFrame, &placeholderFrame);

      // Remove the mapping from the frame to its placeholder
      shell->SetPlaceholderFrameFor(childFrame, nsnull);

      // Generate two notifications. First for the absolutely positioned
      // frame
      nsIFrame* parentFrame;
      childFrame->GetParent(&parentFrame);
      rv = parentFrame->RemoveFrame(*aPresContext, *shell,
                                    nsLayoutAtoms::absoluteList, childFrame);

      // Now the placeholder frame
      if (nsnull != placeholderFrame) {
        placeholderFrame->GetParent(&parentFrame);
        rv = parentFrame->RemoveFrame(*aPresContext, *shell, nsnull,
                                      placeholderFrame);
      }

    } else {
      // Notify the parent frame that it should delete the frame
      nsIFrame* parentFrame;
      childFrame->GetParent(&parentFrame);
      rv = parentFrame->RemoveFrame(*aPresContext, *shell, nsnull, childFrame);
    }

    if (mInitialContainingBlock == childFrame) {
      mInitialContainingBlock = nsnull;
    }
  }

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
    aFrame->GetView(&view);
    if (nsnull != view) {
    } else {
      nsPoint offset;
      aFrame->GetOffsetFromView(offset, &view);
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
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
    
  nsIReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        nsIReflowCommand::StyleChanged,
                                        nsnull,
                                        aAttribute);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentChanged(nsIPresContext* aPresContext,
                                      nsIContent*  aContent,
                                      nsISupports* aSubContent)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* frame;
  shell->GetPrimaryFrameFor(aContent, &frame);

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

  return rv;
}
    
NS_IMETHODIMP
nsCSSFrameConstructor::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent* aContent,
                                        nsIAtom* aAttribute,
                                        PRInt32 aHint)
{
  nsresult  result = NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame*     frame;
   
  shell->GetPrimaryFrameFor(aContent, &frame);
  
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
    else aHint = NS_STYLE_HINT_REFLOW;
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
  if (PR_TRUE == reframe) {
      RecreateFramesOnAttributeChange(aPresContext, aContent, aAttribute);
  }
  else if (PR_TRUE == restyle) {
    // If there is no frame then there is no point in re-styling it,
    // is there?
    if (nsnull != frame) {
      nsIStyleContext* frameContext;
      frame->GetStyleContext(&frameContext);
      NS_ASSERTION(nsnull != frameContext, "frame must have style context");
      if (nsnull != frameContext) {
        nsIStyleContext*  parentContext = frameContext->GetParent();
        frame->ReResolveStyleContext(aPresContext, parentContext);
        NS_IF_RELEASE(parentContext);
        NS_RELEASE(frameContext);
      }    
      if (PR_TRUE == reflow) {
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

  return result;
}

  // Style change notifications
NS_IMETHODIMP
nsCSSFrameConstructor::StyleRuleChanged(nsIPresContext* aPresContext,
                                        nsIStyleSheet* aStyleSheet,
                                        nsIStyleRule* aStyleRule,
                                        PRInt32 aHint)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame* frame;
  shell->GetRootFrame(&frame);

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
    frame->GetStyleContext(&sc);
    sc->RemapStyle(aPresContext);
    NS_RELEASE(sc);

    // XXX hack, skip the root and scrolling frames
    frame->FirstChild(nsnull, &frame);
    frame->FirstChild(nsnull, &frame);
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

  return NS_OK;
}

NS_IMETHODIMP
nsCSSFrameConstructor::StyleRuleAdded(nsIPresContext* aPresContext,
                                      nsIStyleSheet* aStyleSheet,
                                      nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFrameConstructor::StyleRuleRemoved(nsIPresContext* aPresContext,
                                        nsIStyleSheet* aStyleSheet,
                                        nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

// Construct an alternate frame to use when the image can't be rendered
nsresult
nsCSSFrameConstructor::ConstructAlternateImageFrame(nsIPresContext* aPresContext,
                                                    nsIContent*     aContent,
                                                    nsIFrame*       aParentFrame,
                                                    nsIFrame*&      aFrame)
{
  nsIDOMHTMLImageElement*  imageElement;
  nsresult                 rv;

  // Initialize OUT parameter
  aFrame = nsnull;

  rv = aContent->QueryInterface(kIDOMHTMLImageElementIID, (void**)&imageElement);
  if (NS_SUCCEEDED(rv)) {
    nsAutoString  altText;
  
    // The "alt" attribute specifies alternate text that is rendered
    // when the image can not be displayed
    imageElement->GetAlt(altText);
    if (0 == altText.Length()) {
      // If there's no "alt" attribute, then use the value of the "title"
      // attribute
      imageElement->GetTitle(altText);
    }
    if (0 == altText.Length()) {
      // If there's no "title" attribute, then use the filename minus the
      // extension
      nsAutoString  srcURI;

      imageElement->GetSrc(srcURI);
      if (srcURI.Length() > 0) {
        nsIURL* url;
        if (NS_SUCCEEDED(NS_NewURL(&url, srcURI))) {
          const char* file;
          url->GetFile(&file);
          altText = file;
          NS_RELEASE(url);

          // Trim off the path part of the filename
          PRInt32 offset = altText.RFind('/');
          if (offset >= 0) {
            altText.Cut(0, offset + 1);
          }

          // Trim off the extension
          offset = altText.RFind('.');
          if (offset >= 0) {
            altText.Truncate(offset);
          }
        }
      }
    }
    NS_RELEASE(imageElement);
  
    if (altText.Length() > 0) {
      // Create a text content element for the alternate text
      nsCOMPtr<nsIContent> altTextContent;
      nsIDOMCharacterData* domData;
  
      NS_NewTextNode(getter_AddRefs(altTextContent));
      altTextContent->QueryInterface(kIDOMCharacterDataIID, (void**)&domData);
      domData->SetData(altText);
      NS_RELEASE(domData);
  
      // Create an inline frame and a text frame to display the alt-text.
      // XXX The only reason for the inline frame is so there's a frame that
      // points to the IMG content element. Some day when we have a hash table
      // for GetPrimaryFrameFor() we don't need the inline frame...
      nsIFrame* inlineFrame;
      nsIFrame* textFrame;
      
      NS_NewInlineFrame(inlineFrame);
      NS_NewTextFrame(textFrame);
  
      // Use a special text pseudo element style context
      nsCOMPtr<nsIStyleContext> textStyleContext;
      nsCOMPtr<nsIStyleContext> parentStyleContext;
      nsCOMPtr<nsIContent>      parentContent;
  
      aParentFrame->GetContent(getter_AddRefs(parentContent));
      aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
      aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::textPseudo,
                                                 parentStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(textStyleContext));
  
      // Initialize the frames
      inlineFrame->Init(*aPresContext, aContent, aParentFrame, textStyleContext);
      textFrame->Init(*aPresContext, altTextContent, inlineFrame, textStyleContext);
      inlineFrame->SetInitialChildList(*aPresContext, nsnull, textFrame);

      // Return the inline frame
      aFrame = inlineFrame;
    }
  }

  return rv;
}


NS_IMETHODIMP
nsCSSFrameConstructor::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIAtom>    tag;
  nsIFrame*            parentFrame;
  nsresult             rv = NS_OK;

  aFrame->GetParent(&parentFrame);

  // Get the content object associated with aFrame
  aFrame->GetContent(getter_AddRefs(content));
  NS_ASSERTION(content, "null content object");
  content->GetTag(*getter_AddRefs(tag));

  // See whether it's an IMG or an OBJECT element
  if (nsHTMLAtoms::img == tag) {
    // It's an IMG element. Try and construct an alternate frame to use when the
    // image can't be rendered
    nsIFrame* newFrame;
    rv = ConstructAlternateImageFrame(aPresContext, content, parentFrame, newFrame);

    if (NS_SUCCEEDED(rv)) {
      // Get the previous sibling frame
      nsIFrame*     firstChild;
      parentFrame->FirstChild(nsnull, &firstChild);
      nsFrameList   frameList(firstChild);
      nsIFrame*     prevSibling = frameList.GetPrevSiblingFor(aFrame);

      // Delete the current frame and insert the new frame
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      parentFrame->RemoveFrame(*aPresContext, *presShell, nsnull, aFrame);
      parentFrame->InsertFrames(*aPresContext, *presShell, nsnull, prevSibling, newFrame);
    }

  } else if (nsHTMLAtoms::object == tag) {
    ;
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected tag");
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::RecreateFramesOnAttributeChange(nsIPresContext* aPresContext,
                                                       nsIContent* aContent,
                                                       nsIAtom* aAttribute)                                   
{
  nsresult rv = NS_OK;

  // First, remove the frames associated with the content object on which the
  // attribute change occurred.

  // XXX Right now, ContentRemoved() does not do anything with its aContainer
  // and aIndexInContainer in parameters, so I am passing in null and 0, respectively
  rv = ContentRemoved(aPresContext, nsnull, aContent, 0);

  if (NS_OK == rv) {
    // Now, recreate the frames associated with this content object.
    nsIContent *container;
    rv = aContent->GetParent(container);
  
    if (NS_OK == rv) {
      PRInt32 indexInContainer;
      rv = container->IndexOf(aContent, indexInContainer);

      if (NS_OK == rv) {
        rv = ContentInserted(aPresContext, container, aContent, indexInContainer);
      }

      NS_RELEASE(container);    
    }    
  }

  return rv;
}

