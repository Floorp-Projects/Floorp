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
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsITextContent.h"
#include "nsPlaceholderFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsStyleChangeList.h"
#include "nsIFormControl.h"
#include "nsCSSAtoms.h"

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsTreeFrame.h"
#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h"
#include "nsTreeIndentationFrame.h"
#include "nsTreeCellFrame.h"

nsresult
NS_NewProgressMeterFrame ( nsIFrame*& aNewFrame );

nsresult
NS_NewTitledButtonFrame ( nsIFrame*& aNewFrame );

nsresult
NS_NewBoxFrame ( nsIFrame*& aNewFrame );

#endif

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
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

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
  : childList(nsnull), lastChild(nsnull)
{
}

nsFrameItems::nsFrameItems(nsIFrame* aFrame)
  : childList(aFrame), lastChild(aFrame)
{
}

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
  // containing block for absolutely positioned elements
  nsIFrame* const containingBlock;

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
  nsHTMLAtoms::AddrefAtoms();
#endif
}

nsCSSFrameConstructor::~nsCSSFrameConstructor(void)
{
#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
  nsHTMLAtoms::ReleaseAtoms();
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

PRBool
nsCSSFrameConstructor::CreateGeneratedContentFrame(nsIPresContext*  aPresContext,
                                                   nsIFrame*        aFrame,
                                                   nsIContent*      aContent,
                                                   nsIStyleContext* aStyleContext,
                                                   nsIAtom*         aPseudoElement,
                                                   nsIFrame**       aResult)
{
  *aResult = nsnull; // initialize OUT parameter

  // Probe for the existence of the pseudo-element
  nsIStyleContext*  pseudoStyleContext;
  aPresContext->ProbePseudoStyleContextFor(aContent, aPseudoElement, aStyleContext,
                                           PR_FALSE, &pseudoStyleContext);

  if (pseudoStyleContext) {
    nsIContent*           textContent;
    nsIDOMCharacterData*  domData;
    const nsStyleDisplay* display;

    // See whether the generated content should be displayed.
    display = (const nsStyleDisplay*)pseudoStyleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_DISPLAY_NONE != display->mDisplay) {
      PRUint8 displayValue = display->mDisplay;

      // Make sure the 'display' property value is allowable
      const nsStyleDisplay* subjectDisplay = (const nsStyleDisplay*)
        aStyleContext->GetStyleData(eStyleStruct_Display);

      if (subjectDisplay->IsBlockLevel()) {
        // For block-level elements the only allowed 'display' values are:
        // 'none', 'inline', 'block', and 'marker'
        if ((NS_STYLE_DISPLAY_INLINE != displayValue) &&
            (NS_STYLE_DISPLAY_BLOCK != displayValue) &&
            (NS_STYLE_DISPLAY_MARKER != displayValue)) {
          // Pseudo-element behaves as if the value were 'block'
          displayValue = NS_STYLE_DISPLAY_BLOCK;
        }

      } else {
        // For inline-level elements the only allowed 'display' values are
        // 'none' and 'inline'
        displayValue = NS_STYLE_DISPLAY_INLINE;
      }

      if (display->mDisplay != displayValue) {
        // Reset the value
        nsStyleDisplay* mutableDisplay = (nsStyleDisplay*)
          pseudoStyleContext->GetMutableStyleData(eStyleStruct_Display);

        mutableDisplay->mDisplay = displayValue;
      }

      // Now create the content object and frame based on the 'content' property
      // XXX For the time being just pretend it's text
      NS_NewTextNode(&textContent);
      textContent->QueryInterface(kIDOMCharacterDataIID, (void**)&domData);
      domData->SetData("GENERATED");
      NS_RELEASE(domData);
  
      // Create a text frame and initialize it
      nsIFrame* textFrame;
      NS_NewTextFrame(textFrame);
      textFrame->Init(*aPresContext, textContent, aFrame, pseudoStyleContext, nsnull);
  
      NS_RELEASE(pseudoStyleContext);
      *aResult = textFrame;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

/**
 * Request to process the child content elements and create frames.
 *
 * @param   aContent the content object whose child elements to process
 * @param   aFrame the the associated with aContent. This will be the
 *            parent frame (both content and geometric) for the flowed
 *            child frames
 */
nsresult
nsCSSFrameConstructor::ProcessChildren(nsIPresContext*  aPresContext,
                                       nsIContent*      aContent,
                                       nsIFrame*        aFrame,
                                       nsAbsoluteItems& aAbsoluteItems,
                                       nsFrameItems&    aFrameItems,
                                       nsAbsoluteItems& aFixedItems,
                                       nsAbsoluteItems& aFloatingItems)
{
  // If the content can contain children, then check for generated content
  nsIStyleContext*  styleContext = nsnull;
  PRBool            canContainChildren;
  aContent->CanContainChildren(canContainChildren);

  if (canContainChildren) {
    nsIFrame* generatedFrame;

    // Probe for generated content before
    aFrame->GetStyleContext(&styleContext);
    if (CreateGeneratedContentFrame(aPresContext, aFrame, aContent, styleContext,
                                    nsCSSAtoms::beforePseudo, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  // Iterate the child content objects and construct frames
  PRInt32   count;

  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> childContent;
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent)))) {
      // Construct a child frame
      ConstructFrame(aPresContext, childContent, aFrame, aAbsoluteItems,
                     aFrameItems, aFixedItems, aFloatingItems);
    }
  }

  if (canContainChildren) {
    nsIFrame* generatedFrame;

    // Probe for generated content after
    if (CreateGeneratedContentFrame(aPresContext, aFrame, aContent, styleContext,
                                    nsCSSAtoms::afterPseudo, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }
  
  NS_IF_RELEASE(styleContext);
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

struct nsTableList {
  nsTableList() {
    mInner = mChild = nsnull;
  }
  void Set(nsIFrame* aInner, nsIFrame* aChild) {
    mInner = aInner;
    mChild = aChild;
  }
  nsIFrame* mInner;
  nsIFrame* mChild;
};

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
  aNewFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, nsnull);
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
  // XXX This is very wrong. You cannot call SetInitialChildList() more
  // than once (see the nsIFrame header file). Either call it once only,
  // _or_ move the caption out into a separate named child list...
  // XXX The other things that's wrong here is that the calls to
  // SetInitialChildList() are bottom-up, and the order is wrong...
  aNewFrame->SetInitialChildList(*aPresContext, nsnull, innerFrame); 
  childList = innerFrame;

  innerFrame->Init(*aPresContext, aContent, aNewFrame, aStyleContext, nsnull);

  nsIFrame* lastChildFrame = nsnull;
  PRInt32   count;
  aContent->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) { // iterate the child content
    nsCOMPtr<nsIContent> childContent;
    
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent)))) {
      nsIFrame* childFrame = nsnull;
      nsIFrame* ignore1;
      nsIFrame* ignore2;
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
                                          aAbsoluteItems, ignore1, captionFrame, aFixedItems, aTableCreator);
        }
        break;

      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      {
        PRBool isRowGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != styleDisplay->mDisplay);
        rv = ConstructTableGroupFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                      aAbsoluteItems, isRowGroup, childFrame, ignore1, aFixedItems, aTableCreator);
        break;
      }
      case NS_STYLE_DISPLAY_TABLE_ROW:
        rv = ConstructTableRowFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                    aAbsoluteItems, childFrame, ignore1, aFixedItems, aTableCreator);
        break;

      case NS_STYLE_DISPLAY_TABLE_COLUMN:
        rv = ConstructTableColFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                    aAbsoluteItems, childFrame, ignore1, aFixedItems, aTableCreator);
        break;


      case NS_STYLE_DISPLAY_TABLE_CELL:
        rv = ConstructTableCellFrame(aPresContext, childContent, innerFrame, childStyleContext, 
                                     aAbsoluteItems, childFrame, ignore1, ignore2, aFixedItems, aTableCreator);
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
                                                     nsIFrame*&       aNewTopFrame,
                                                     nsIFrame*&       aOuterFrame, 
                                                     nsIFrame*&       aInnerFrame,
                                                     nsAbsoluteItems& aAbsoluteItems,
                                                     nsAbsoluteItems& aFixedItems,
                                                     nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;
  NS_WARNING("an anonymous table frame was created. \n");
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  const nsStyleDisplay* parentDisplay = 
   (const nsStyleDisplay*) parentStyleContext->GetStyleData(eStyleStruct_Display);

  PRBool cellIsParent = PR_TRUE;
  nsIFrame* cellFrame;
  nsIFrame* cellBodyFrame;
  nsCOMPtr<nsIStyleContext> cellStyleContext;
  nsIFrame* ignore;

  // construct the ancestors of anonymous table frames
  switch(parentDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_TABLE_CELL:
    break;
  case NS_STYLE_DISPLAY_TABLE_ROW: // construct a cell
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo, parentStyleContext,
                                               PR_FALSE, getter_AddRefs(cellStyleContext));
    rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame, cellStyleContext, aAbsoluteItems,
                                 aNewTopFrame, cellFrame, cellBodyFrame, aFixedItems, aTableCreator, PR_FALSE); 
    break;
  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP: // construct a row & a cell
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    {
      nsIFrame* rowFrame;
      nsCOMPtr<nsIStyleContext> rowStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo, 
                                                 parentStyleContext, PR_FALSE, 
                                                 getter_AddRefs(rowStyleContext));
      rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, rowStyleContext, 
                                  aAbsoluteItems, aNewTopFrame, rowFrame, aFixedItems, 
                                  aTableCreator);
      if (NS_FAILED(rv)) return rv;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo, parentStyleContext,
                                                 PR_FALSE, getter_AddRefs(cellStyleContext));
      rv = ConstructTableCellFrame(aPresContext, aContent, rowFrame, cellStyleContext, 
                                   aAbsoluteItems, ignore, cellFrame, cellBodyFrame, 
                                   aFixedItems, aTableCreator, PR_FALSE);
      rowFrame->SetInitialChildList(*aPresContext, nsnull, cellFrame);
      break;
    }
  case NS_STYLE_DISPLAY_TABLE: // construct a row group, a row, & a cell
    {
      nsIFrame* groupFrame;
      nsCOMPtr<nsIStyleContext> groupStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowGroupPseudo, 
                                                 parentStyleContext, PR_FALSE, 
                                                 getter_AddRefs(groupStyleContext));
      rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, groupStyleContext, 
                                    aAbsoluteItems, PR_TRUE, aNewTopFrame, groupFrame, aFixedItems, 
                                    aTableCreator);
      if (NS_FAILED(rv)) return rv;
      nsIFrame* rowFrame;
      nsCOMPtr<nsIStyleContext> rowStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo, 
                                                 parentStyleContext, PR_FALSE, 
                                                 getter_AddRefs(rowStyleContext));
      rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, rowStyleContext, 
                                  aAbsoluteItems, ignore, rowFrame, aFixedItems, 
                                  aTableCreator);
      if (NS_FAILED(rv)) return rv;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo, parentStyleContext,
                                               PR_FALSE, getter_AddRefs(cellStyleContext));
      rv = ConstructTableCellFrame(aPresContext, aContent, rowFrame, cellStyleContext, 
                                   aAbsoluteItems, ignore, cellFrame, cellBodyFrame,
                                   aFixedItems, aTableCreator, PR_FALSE);
      rowFrame->SetInitialChildList(*aPresContext, nsnull, cellFrame);
      groupFrame->SetInitialChildList(*aPresContext, nsnull, rowFrame);
      break;
    }
  default:
    cellIsParent = PR_FALSE;
  }

  // create the outer table, inner table frames and make the cell frame the parent of the 
  // outer frame
  if (NS_SUCCEEDED(rv)) {
    // create the outer table frame
    nsIFrame* tableParent = nsnull;
    nsIStyleContext* tableParentSC;
    if (cellIsParent) {
      tableParent = cellFrame;
      tableParentSC = cellStyleContext;
    } else {
      tableParent = aParentFrame;
      tableParentSC = parentStyleContext;
    }
    nsCOMPtr<nsIStyleContext> outerStyleContext;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableOuterPseudo, 
                                               tableParentSC, PR_FALSE,
                                               getter_AddRefs(outerStyleContext));
    rv = NS_NewTableOuterFrame(aOuterFrame);
    if (NS_FAILED(rv)) return rv;
    aOuterFrame->Init(*aPresContext, aContent, tableParent, outerStyleContext,
                      nsnull);

    // create the inner table frame
    nsCOMPtr<nsIStyleContext> innerStyleContext;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tablePseudo, 
                                               outerStyleContext, PR_FALSE,
                                               getter_AddRefs(innerStyleContext));
    rv = aTableCreator.CreateTableFrame(aInnerFrame);
    if (NS_FAILED(rv)) return rv;
    aInnerFrame->Init(*aPresContext, aContent, aOuterFrame, innerStyleContext,
                      nsnull);
    aOuterFrame->SetInitialChildList(*aPresContext, nsnull, aInnerFrame);

    if (cellIsParent) {
      cellBodyFrame->SetInitialChildList(*aPresContext, nsnull, aOuterFrame);
    } else {
      aNewTopFrame = aOuterFrame;
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCaptionFrame(nsIPresContext*  aPresContext,
                                                  nsIContent*      aContent,
                                                  nsIFrame*        aParentFrame,
                                                  nsIStyleContext* aStyleContext,
                                                  nsAbsoluteItems& aAbsoluteItems,
                                                  nsIFrame*&       aNewTopFrame,
                                                  nsIFrame*&       aNewCaptionFrame,
                                                  nsAbsoluteItems& aFixedItems,
                                                  nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_NewAreaFrame(aNewCaptionFrame, 0);
  if (NS_FAILED(rv)) return rv;

  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  nsIFrame* innerFrame;

  if (NS_STYLE_DISPLAY_TABLE == parentDisplay->mDisplay) { // parent is an outer table
    // determine the inner table frame, it is either aParentFrame or its first child
    nsIFrame* parFrame = aParentFrame;
    aParentFrame->FirstChild(nsnull, &innerFrame);
    const nsStyleDisplay* innerDisplay;
    innerFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)innerDisplay);
    if (NS_STYLE_DISPLAY_TABLE != innerDisplay->mDisplay) {
      innerFrame = aParentFrame; 
      innerFrame->GetParent(&parFrame);
    }

    aNewCaptionFrame->Init(*aPresContext, aContent, parFrame, aStyleContext,
                           nsnull);
    innerFrame->SetNextSibling(aNewCaptionFrame);
    // the caller is responsible for calling SetInitialChildList on the outer, inner frames
    aNewTopFrame = aNewCaptionFrame;
  } else { // parent is not a table, need to create a new table
    NS_WARNING("a non table contains a table caption child. \n");
    nsIFrame* outerFrame;
    ConstructAnonymousTableFrame(aPresContext, aContent, aParentFrame, aNewTopFrame,
                                 outerFrame, innerFrame, aAbsoluteItems, aFixedItems, 
                                 aTableCreator);
    nsCOMPtr<nsIStyleContext> outerStyleContext;
    outerFrame->GetStyleContext(getter_AddRefs(outerStyleContext));
    nsCOMPtr<nsIStyleContext> adjStyleContext;
    aPresContext->ResolveStyleContextFor(aContent, outerStyleContext,
                                         PR_FALSE, getter_AddRefs(adjStyleContext));
    aNewCaptionFrame->Init(*aPresContext, aContent, outerFrame, 
                           adjStyleContext, nsnull);
    innerFrame->SetNextSibling(aNewCaptionFrame);
  }

  nsFrameItems childItems;
  nsAbsoluteItems floatingKids(aNewCaptionFrame);
  ProcessChildren(aPresContext, aContent, aNewCaptionFrame, aAbsoluteItems, 
                  childItems, aFixedItems, floatingKids);
  aNewCaptionFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  if (floatingKids.childList) {
    aNewCaptionFrame->SetInitialChildList(*aPresContext,
                                          nsLayoutAtoms::floaterList,
                                          floatingKids.childList);
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
                                                nsIFrame*&       aNewTopFrame, 
                                                nsIFrame*&       aNewGroupFrame,
                                                nsAbsoluteItems& aFixedItems,
                                                nsTableCreator&  aTableCreator,
                                                nsTableList*     aToDo)   
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  nsCOMPtr<nsIStyleContext> styleContext( dont_QueryInterface(aStyleContext) );

  // TRUE if we are being called from above, FALSE if from below (e.g. row)
  PRBool contentDisplayIsGroup = 
    (aIsRowGroup) ? (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == styleDisplay->mDisplay) ||
                    (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == styleDisplay->mDisplay) ||
                    (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == styleDisplay->mDisplay)
                  : (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == styleDisplay->mDisplay);
  if (!contentDisplayIsGroup) {
    NS_ASSERTION(aToDo, "null nsTableList when constructing from below");
  }

  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  const nsStyleDisplay* parentDisplay = 
   (const nsStyleDisplay*) parentStyleContext->GetStyleData(eStyleStruct_Display);

  if (NS_STYLE_DISPLAY_TABLE == parentDisplay->mDisplay) { // parent is an inner table
    if (!contentDisplayIsGroup) { // called from row below
      nsIAtom* pseudoGroup = (aIsRowGroup) ? nsHTMLAtoms::tableRowGroupPseudo : nsHTMLAtoms::tableColGroupPseudo;
      aPresContext->ResolvePseudoStyleContextFor(aContent, pseudoGroup, parentStyleContext,
                                                 PR_FALSE, getter_AddRefs(styleContext));
    }
    // only process the group's children if we're called from above
    rv = ConstructTableGroupFrameOnly(aPresContext, aContent, aParentFrame, styleContext, 
                                      aAbsoluteItems, aIsRowGroup, aNewTopFrame, aNewGroupFrame, 
                                      aFixedItems, aTableCreator, contentDisplayIsGroup);
  } else { // construct anonymous frames
    NS_WARNING("a non table contains a table row or col group child. \n");
    nsIFrame* innerFrame;
    nsIFrame* outerFrame;

    ConstructAnonymousTableFrame(aPresContext, aContent, aParentFrame, aNewTopFrame, outerFrame, 
                                 innerFrame, aAbsoluteItems, aFixedItems, aTableCreator);
    nsCOMPtr<nsIStyleContext> innerStyleContext;
    innerFrame->GetStyleContext(getter_AddRefs(innerStyleContext));
    if (contentDisplayIsGroup) { // called from above
      aPresContext->ResolveStyleContextFor(aContent, innerStyleContext, PR_FALSE,
                                           getter_AddRefs(styleContext));
    } else { // called from row below
      nsIAtom* pseudoGroup = (aIsRowGroup) ? nsHTMLAtoms::tableRowGroupPseudo : nsHTMLAtoms::tableColGroupPseudo;
      aPresContext->ResolvePseudoStyleContextFor(aContent, pseudoGroup, innerStyleContext,
                                                 PR_FALSE, getter_AddRefs(styleContext));
    }
    nsIFrame* topFrame;
    // only process the group's children if we're called from above
    rv = ConstructTableGroupFrameOnly(aPresContext, aContent, innerFrame, styleContext,
                                      aAbsoluteItems, aIsRowGroup, topFrame, aNewGroupFrame, 
                                      aFixedItems, aTableCreator, contentDisplayIsGroup);
    if (NS_FAILED(rv)) return rv;
    if (contentDisplayIsGroup) { // called from above
      innerFrame->SetInitialChildList(*aPresContext, nsnull, topFrame);
    } else { // called from row below
      aToDo->Set(innerFrame, topFrame);
    }
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
                                                    nsIFrame*&       aNewTopFrame, 
                                                    nsIFrame*&       aNewGroupFrame,
                                                    nsAbsoluteItems& aFixedItems,
                                                    nsTableCreator&  aTableCreator,
                                                    PRBool           aProcessChildren) 
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* styleDisplay = 
    (const nsStyleDisplay*) aStyleContext->GetStyleData(eStyleStruct_Display);

  if (IsScrollable(aPresContext, styleDisplay)) {
    // Create a scroll frame and initialize it
    rv = NS_NewScrollFrame(aNewTopFrame);
    if (NS_FAILED(rv)) return rv;
    aNewTopFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, nsnull);

    // The scroll frame gets the original style context, the scrolled frame gets  
    // a pseudo element style context that inherits the background properties
    nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::scrolledContentPseudo,
                                               aStyleContext, PR_FALSE,
                                               getter_AddRefs(scrolledPseudoStyle));

    // Create an area container for the frame
    rv = (aIsRowGroup) ? aTableCreator.CreateTableRowGroupFrame(aNewGroupFrame)
                       : aTableCreator.CreateTableColGroupFrame(aNewGroupFrame);
    if (NS_FAILED(rv)) return rv;
    // Initialize the frame and force it to have a view
    aNewGroupFrame->Init(*aPresContext, aContent, aNewTopFrame, scrolledPseudoStyle,
                         nsnull);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, aNewGroupFrame,
                                             scrolledPseudoStyle, PR_TRUE);
    aNewTopFrame->SetInitialChildList(*aPresContext, nsnull, aNewGroupFrame);
  } else {
    rv = (aIsRowGroup) ? aTableCreator.CreateTableRowGroupFrame(aNewTopFrame)
                       : aTableCreator.CreateTableColGroupFrame(aNewTopFrame);
    if (NS_FAILED(rv)) return rv;
    aNewTopFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext,
                       nsnull);
    aNewGroupFrame = aNewTopFrame;
  }

  if (aProcessChildren) {
    nsFrameItems childItems;
    if (aIsRowGroup) {
      TableProcessChildren(aPresContext, aContent, aNewGroupFrame, aAbsoluteItems, 
                           childItems, aFixedItems, aTableCreator);
    } else {
        nsAbsoluteItems floatingKids(nsnull);
      ProcessChildren(aPresContext, aContent, aNewGroupFrame, aAbsoluteItems, 
                      childItems, aFixedItems, floatingKids);
        NS_ASSERTION(nsnull == floatingKids.childList, "floater in odd spot");
    }
    aNewGroupFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableRowFrame(nsIPresContext*  aPresContext,
                                              nsIContent*      aContent,
                                              nsIFrame*        aParentFrame,
                                              nsIStyleContext* aStyleContext,
                                              nsAbsoluteItems& aAbsoluteItems,
                                              nsIFrame*&       aNewTopFrame,
                                              nsIFrame*&       aNewRowFrame,
                                              nsAbsoluteItems& aFixedItems,
                                              nsTableCreator&  aTableCreator,
                                              nsTableList*     aToDo)
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);

  // TRUE if we are being called from above, FALSE if from below (e.g. cell)
  PRBool contentDisplayIsRow = (NS_STYLE_DISPLAY_TABLE_ROW == display->mDisplay);
  if (!contentDisplayIsRow) {
    NS_ASSERTION(aToDo, "null nsTableList when constructing from below");
  }

  nsCOMPtr<nsIStyleContext> groupStyleContext;
  nsCOMPtr<nsIStyleContext> styleContext( dont_QueryInterface(aStyleContext) );

  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  if ((NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == parentDisplay->mDisplay)) {
    if (!contentDisplayIsRow) { // content is from some (soon to be) child of ours
      aParentFrame = TableGetAsNonScrollFrame(aPresContext, aParentFrame, parentDisplay); 
      aParentFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo,
                                                 groupStyleContext, PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    // only process the row's children if we're called from above
    rv = ConstructTableRowFrameOnly(aPresContext, aContent, aParentFrame, styleContext, 
                                    aAbsoluteItems, contentDisplayIsRow, aNewRowFrame, 
                                    aFixedItems, aTableCreator);
    aNewTopFrame = aNewRowFrame;
  } else { // construct an anonymous row group frame
    nsIFrame* groupFrame;
    nsTableList localToDo;
    nsTableList* toDo = (aToDo) ? aToDo : &localToDo;

    // it may also need to create a table frame
    rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, styleContext,
                                  aAbsoluteItems, PR_TRUE, aNewTopFrame, groupFrame,
                                  aFixedItems, aTableCreator, toDo);
    if (NS_FAILED(rv)) return rv;
    groupFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
    if (contentDisplayIsRow) { // called from above
      aPresContext->ResolveStyleContextFor(aContent, groupStyleContext,
                                           PR_FALSE, getter_AddRefs(styleContext));
    } else { // called from cell below
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo,
                                                 groupStyleContext, PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    // only process the row's children if we're called from above
    rv = ConstructTableRowFrameOnly(aPresContext, aContent, groupFrame, styleContext, 
                                    aAbsoluteItems, contentDisplayIsRow, aNewRowFrame, 
                                    aFixedItems, aTableCreator);
    if (NS_FAILED(rv)) return rv;
    groupFrame->SetInitialChildList(*aPresContext, nsnull, aNewRowFrame);
    if (contentDisplayIsRow) { // called from above
      TableProcessTableList(aPresContext, *toDo);
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
  if (NS_FAILED(rv)) return rv;
  aNewRowFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext,
                     nsnull);
  if (aProcessChildren) {
    nsFrameItems childItems;
    rv = TableProcessChildren(aPresContext, aContent, aNewRowFrame, aAbsoluteItems, 
                              childItems, aFixedItems, aTableCreator);
    if (NS_FAILED(rv)) return rv;
    aNewRowFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
  }

  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsIPresContext*  aPresContext,
                                              nsIContent*      aContent,
                                              nsIFrame*        aParentFrame,
                                              nsIStyleContext* aStyleContext,
                                              nsAbsoluteItems& aAbsoluteItems,
                                              nsIFrame*&       aNewTopFrame,
                                              nsIFrame*&       aNewColFrame,
                                              nsAbsoluteItems& aFixedItems,
                                              nsTableCreator&  aTableCreator)
{
  nsresult rv = NS_OK;
  // the content display here is always table-col and were always called from above
  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == parentDisplay->mDisplay) {
    rv = ConstructTableColFrameOnly(aPresContext, aContent, aParentFrame, 
                                    aStyleContext, aAbsoluteItems, aNewColFrame, 
                                    aFixedItems, aTableCreator);
    aNewTopFrame = aNewColFrame;
  } else { // construct anonymous col group frame
    nsTableList toDo;
    nsIFrame* groupFrame;
    rv = ConstructTableGroupFrame(aPresContext, aContent, aParentFrame, aStyleContext,
                                  aAbsoluteItems, PR_FALSE, aNewTopFrame, 
                                  groupFrame, aFixedItems, aTableCreator, &toDo);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIStyleContext> groupStyleContext; 
    groupFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
    nsCOMPtr<nsIStyleContext> styleContext;
    aPresContext->ResolveStyleContextFor(aContent, groupStyleContext,
                                         PR_FALSE, getter_AddRefs(styleContext));
    rv = ConstructTableColFrameOnly(aPresContext, aContent, groupFrame, 
                                    styleContext, aAbsoluteItems, aNewColFrame, 
                                    aFixedItems, aTableCreator);
    if (NS_FAILED(rv)) return rv;
    groupFrame->SetInitialChildList(*aPresContext, nsnull, aNewColFrame);

    // if an anoymous table got created, then set its initial child list
    TableProcessTableList(aPresContext, toDo);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableColFrameOnly(nsIPresContext*  aPresContext,
                                                  nsIContent*      aContent,
                                                  nsIFrame*        aParentFrame,
                                                  nsIStyleContext* aStyleContext,
                                                  nsAbsoluteItems& aAbsoluteItems,
                                                  nsIFrame*&       aNewColFrame,
                                                  nsAbsoluteItems& aFixedItems,
                                                  nsTableCreator&  aTableCreator)
{
  nsresult rv = aTableCreator.CreateTableColFrame(aNewColFrame);
  if (NS_FAILED(rv)) return rv;
  aNewColFrame->Init(*aPresContext, aContent, aParentFrame, 
                     aStyleContext, nsnull);
  nsFrameItems colChildItems;
  nsAbsoluteItems floatingKids(nsnull);
  rv = ProcessChildren(aPresContext, aContent, aNewColFrame, aAbsoluteItems, 
                       colChildItems, aFixedItems, floatingKids);
  NS_ASSERTION(nsnull == floatingKids.childList, "floater in odd spot");
  if (NS_FAILED(rv)) return rv;
  aNewColFrame->SetInitialChildList(*aPresContext, nsnull,
                                    colChildItems.childList);
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsIPresContext*  aPresContext,
                                               nsIContent*      aContent,
                                               nsIFrame*        aParentFrame,
                                               nsIStyleContext* aStyleContext,
                                               nsAbsoluteItems& aAbsoluteItems,
                                               nsIFrame*&       aNewTopFrame,
                                               nsIFrame*&       aNewCellFrame,
                                               nsIFrame*&       aNewCellBodyFrame,
                                               nsAbsoluteItems& aFixedItems,
                                               nsTableCreator&  aTableCreator,
                                               PRBool           aProcessChildren)
{
  nsresult rv = NS_OK;

  const nsStyleDisplay* display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  // FALSE if we are being called to wrap a cell around the content
  PRBool contentDisplayIsCell = (NS_STYLE_DISPLAY_TABLE_CELL == display->mDisplay);

  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  const nsStyleDisplay* parentDisplay = (const nsStyleDisplay*)
    parentStyleContext->GetStyleData(eStyleStruct_Display);

  nsCOMPtr<nsIStyleContext> styleContext( dont_QueryInterface(aStyleContext) );
  PRBool wrapContent = PR_FALSE;

  if (NS_STYLE_DISPLAY_TABLE_ROW == parentDisplay->mDisplay) {
    nsCOMPtr<nsIStyleContext> styleContextRelease;
    if (!contentDisplayIsCell) { // need to wrap
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo,
                                                 parentStyleContext,
                                                 PR_FALSE,
                                                 getter_AddRefs(styleContext));
      wrapContent = PR_TRUE; 
    }
    rv = ConstructTableCellFrameOnly(aPresContext, aContent, aParentFrame, styleContext,
                                     aAbsoluteItems, aNewCellFrame, aNewCellBodyFrame,
                                     aFixedItems, aTableCreator, aProcessChildren);
    aNewTopFrame = aNewCellFrame;
  } else { // the cell needs some ancestors to be fabricated
    NS_WARNING("WARNING - a non table row contains a table cell child. \n");
    nsTableList toDo;
    nsIFrame* rowFrame;
    rv = ConstructTableRowFrame(aPresContext, aContent, aParentFrame, aStyleContext,
                                aAbsoluteItems, aNewTopFrame, rowFrame, aFixedItems, 
                                aTableCreator, &toDo);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIStyleContext> rowStyleContext; 
    rowFrame->GetStyleContext(getter_AddRefs(rowStyleContext));
    if (contentDisplayIsCell) {
      aPresContext->ResolveStyleContextFor(aContent, rowStyleContext,
                                           PR_FALSE, getter_AddRefs(styleContext));
    } else {
      wrapContent = PR_TRUE; // XXX
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo,
                                                 rowStyleContext, PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    rv = ConstructTableCellFrameOnly(aPresContext, aContent, rowFrame, styleContext,
                                     aAbsoluteItems, aNewCellFrame, aNewCellBodyFrame,
                                     aFixedItems, aTableCreator, aProcessChildren);
    if (NS_FAILED(rv)) return rv;
    rowFrame->SetInitialChildList(*aPresContext, nsnull, aNewCellFrame);
    TableProcessTableList(aPresContext, toDo);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrameOnly(nsIPresContext*  aPresContext,
                                                   nsIContent*      aContent,
                                                   nsIFrame*        aParentFrame,
                                                   nsIStyleContext* aStyleContext,
                                                   nsAbsoluteItems& aAbsoluteItems,
                                                   nsIFrame*&       aNewCellFrame,
                                                   nsIFrame*&       aNewCellBodyFrame,
                                                   nsAbsoluteItems& aFixedItems,
                                                   nsTableCreator&  aTableCreator,
                                                   PRBool           aProcessChildren)
{
  nsresult rv;

  // Create a table cell frame
  rv = aTableCreator.CreateTableCellFrame(aNewCellFrame);
  if (NS_FAILED(rv)) return rv;
 
  // Initialize the table cell frame
  aNewCellFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext,
                      nsnull);

  // Create an area frame that will format the cell's content
  rv = NS_NewAreaFrame(aNewCellBodyFrame, 0);
  if (NS_FAILED(rv)) {
    aNewCellFrame->DeleteFrame(*aPresContext);
    aNewCellFrame = nsnull;
    return rv;
  }
  
  // Resolve pseudo style and initialize the body cell frame
  nsCOMPtr<nsIStyleContext>  bodyPseudoStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::cellContentPseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(bodyPseudoStyle));
  aNewCellBodyFrame->Init(*aPresContext, aContent, aNewCellFrame, 
                          bodyPseudoStyle, nsnull);

  if (aProcessChildren) {
    nsFrameItems childItems;
    nsAbsoluteItems floaterList(aNewCellBodyFrame);
    rv = ProcessChildren(aPresContext, aContent, aNewCellBodyFrame, aAbsoluteItems,
                         childItems, aFixedItems, floaterList);
    if (NS_FAILED(rv)) return rv;
    aNewCellBodyFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (floaterList.childList) {
      aNewCellBodyFrame->SetInitialChildList(*aPresContext,
                                             nsLayoutAtoms::floaterList,
                                             floaterList.childList);
    }
  }
  aNewCellFrame->SetInitialChildList(*aPresContext, nsnull, aNewCellBodyFrame);

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
      nsAbsoluteItems floaterList(nsnull);
      rv = ConstructFrame(aPresContext, aChildContent, aParentFrame,
                          aAbsoluteItems, aChildItems, aFixedItems,
                          floaterList);
      NS_ASSERTION(nsnull == floaterList.childList, "floater in odd spot");
    } else {
      nsCOMPtr<nsIAtom> tag;
      aChildContent->GetTag(*getter_AddRefs(tag));

      // XXX this needs to be fixed so that the form can work without
      // a frame. This is *disgusting*

      // forms, form controls need a frame but it can't be a child of an inner table
      nsIFormControl* formControl = nsnull;
      nsresult fcResult = aChildContent->QueryInterface(kIFormControlIID, (void**)&formControl);
      NS_IF_RELEASE(formControl);
      if ((nsHTMLAtoms::form == tag.get()) || NS_SUCCEEDED(fcResult)) {
        // if the parent is a table, put the form in the outer table frame
        const nsStyleDisplay* parentDisplay = (const nsStyleDisplay*)
          aParentStyleContext->GetStyleData(eStyleStruct_Display);
        nsAbsoluteItems floaterList(nsnull);
        if (parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE) {
          nsIFrame* outerFrame; 
          aParentFrame->GetParent(&outerFrame);
          rv = ConstructFrame(aPresContext, aChildContent, outerFrame,
                              aAbsoluteItems, aChildItems, aFixedItems,
                              floaterList);
          // XXX: Seems like this is going into the inner frame's child list instead of the outer frame. - DWH
        } else {
          rv = ConstructFrame(aPresContext, aChildContent, aParentFrame, 
                              aAbsoluteItems, aChildItems, aFixedItems,
                              floaterList);
        }
        NS_ASSERTION(nsnull == floaterList.childList, "floater in odd spot");
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
            nsIFrame* cellBodyFrame;
            nsIFrame* cellFrame;
            nsIFrame* childFrame; // XXX: Need to change the ConstructTableCell function to return lists of frames. - DWH
            rv = ConstructTableCellFrame(aPresContext, aChildContent, aParentFrame, childStyleContext, 
                                         aAbsoluteItems, childFrame, cellFrame, cellBodyFrame,
                                         aFixedItems, aTableCreator);
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
  if (  (nsHTMLAtoms::img      == tag.get()) ||
        (nsHTMLAtoms::hr       == tag.get()) ||
        (nsHTMLAtoms::br       == tag.get()) ||
        (nsHTMLAtoms::wbr      == tag.get()) ||
        (nsHTMLAtoms::input    == tag.get()) ||
        (nsHTMLAtoms::textarea == tag.get()) ||
        (nsHTMLAtoms::select   == tag.get()) ||
        (nsHTMLAtoms::applet   == tag.get()) ||
        (nsHTMLAtoms::embed    == tag.get()) ||
        (nsHTMLAtoms::fieldset == tag.get()) ||
        (nsHTMLAtoms::legend   == tag.get()) ||
        (nsHTMLAtoms::object   == tag.get()) ||
        (nsHTMLAtoms::form     == tag.get()) ||
        (nsHTMLAtoms::iframe   == tag.get()) ||
        (nsHTMLAtoms::spacer   == tag.get()) ||
        (nsHTMLAtoms::button   == tag.get()) ||
        (nsHTMLAtoms::label    == tag.get()    )) {
    return PR_TRUE;
  }

#ifdef INCLUDE_XUL
  if (  (nsXULAtoms::button          == tag.get())  ||
	    (nsXULAtoms::titledbutton    == tag.get())  ||
        (nsXULAtoms::checkbox        == tag.get())  ||
        (nsXULAtoms::radio           == tag.get())  ||
        (nsXULAtoms::text            == tag.get())  ||
        (nsXULAtoms::widget          == tag.get())  ||
        (nsXULAtoms::tree            == tag.get())  ||
        (nsXULAtoms::treechildren    == tag.get())  ||
        (nsXULAtoms::treeitem        == tag.get())  ||
        (nsXULAtoms::treecell        == tag.get())  ||
        (nsXULAtoms::treeindentation == tag.get())  ||
        (nsXULAtoms::toolbox         == tag.get())  ||
        (nsXULAtoms::toolbar         == tag.get())  ||
        (nsXULAtoms::box             == tag.get())  ||
        (nsXULAtoms::progressmeter   == tag.get()  )) {
    return PR_TRUE;
  }
#endif
  
  // we should check for display type as well - later
  return PR_FALSE;
}

nsresult
nsCSSFrameConstructor::TableProcessTableList(nsIPresContext* aPresContext,
                                             nsTableList& aTableList)
{
  nsresult rv = NS_OK;
  nsIFrame* inner = (nsIFrame*)aTableList.mInner;
  if (inner) {
    nsIFrame* child = (nsIFrame*)aTableList.mChild;
    rv = inner->SetInitialChildList(*aPresContext, nsnull, child);
  }
  return rv;
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
    areaFrame->Init(*aPresContext, aDocElement, aParentFrame, styleContext, nsnull);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;

    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsAbsoluteItems  floaterList(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems,
                    childItems, aFixedItems, floaterList);
    
    // Set the initial child lists
    areaFrame->SetInitialChildList(*aPresContext, nsnull,
                                   childItems.childList);
    if (nsnull != absoluteItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext,
                                     nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }
    if (floaterList.childList) {
      areaFrame->SetInitialChildList(*aPresContext,
                                     nsLayoutAtoms::floaterList,
                                     floaterList.childList);
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
      scrollFrame->Init(*aPresContext, aDocElement, aParentFrame, styleContext,
                        nsnull);
    
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
                    aParentFrame, styleContext, nsnull);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, areaFrame,
                                             styleContext, PR_FALSE);

    // The area frame is the "initial containing block"
    mInitialContainingBlock = areaFrame;
    
    // Process the child content
    nsAbsoluteItems  absoluteItems(areaFrame);
    nsAbsoluteItems  floatingItems(areaFrame);
    nsFrameItems     childItems;

    ProcessChildren(aPresContext, aDocElement, areaFrame, absoluteItems,
                    childItems, aFixedItems, floatingItems);
    
    // Set the initial child lists
    areaFrame->SetInitialChildList(*aPresContext, nsnull,
                                   childItems.childList);
    if (nsnull != absoluteItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext,
                                     nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }
    if (nsnull != floatingItems.childList) {
      areaFrame->SetInitialChildList(*aPresContext,
                                     nsLayoutAtoms::floaterList,
                                     floatingItems.childList);
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
  viewportFrame->Init(*aPresContext, nsnull, nsnull, viewportPseudoStyle, nsnull);

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
  PRBool       isScrollable = PR_FALSE;
  nsISupports* container;
  if (nsnull != aPresContext) {
    aPresContext->GetContainer(&container);
    if (nsnull != container) {
      nsIWebShell* webShell = nsnull;
      container->QueryInterface(kIWebShellIID, (void**) &webShell);
      if (nsnull != webShell) {
        PRInt32 scrolling = -1;
        webShell->GetScrolling(scrolling);
        if (NS_STYLE_OVERFLOW_HIDDEN != scrolling) {
          isScrollable = PR_TRUE;
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
    scrollFrame->Init(*aPresContext, nsnull, viewportFrame, viewportPseudoStyle,
                      nsnull);

    // Inform the view manager about the root scrollable view
    nsIView*            scrollFrameView;
    nsIScrollableView*  scrollingView;

    scrollFrame->GetView(&scrollFrameView);
    NS_ASSERTION(scrollFrameView, "scroll frame has no view");
    scrollFrameView->QueryInterface(kScrollViewIID, (void**)&scrollingView);
    viewManager->SetRootScrollableView(scrollingView);
  }

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    nsIFrame* pageSequenceFrame;

    // Create a page sequence frame
    NS_NewSimplePageSequenceFrame(pageSequenceFrame);
    // XXX should probably be a page sequence pseudo style context
    pageSequenceFrame->Init(*aPresContext, nsnull, isScrollable ? scrollFrame :
                            viewportFrame, viewportPseudoStyle, nsnull);
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
    nsCOMPtr<nsIStyleContext> pagePseudoStyle;

    aPresContext->ResolvePseudoStyleContextFor(nsnull, nsLayoutAtoms::pagePseudo,
                                               viewportPseudoStyle, PR_FALSE,
                                               getter_AddRefs(pagePseudoStyle));

    pageFrame->Init(*aPresContext, nsnull, pageSequenceFrame, pagePseudoStyle,
                    nsnull);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, pageFrame,
                                             pagePseudoStyle, PR_TRUE);

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
                    viewportFrame, viewportPseudoStyle, nsnull);
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
                           placeholderPseudoStyle, nsnull);
  
    // Add mapping from absolutely positioned frame to its placeholder frame
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->SetPlaceholderFrameFor(aFrame, placeholderFrame);
  
    aPlaceholderFrame = placeholderFrame;
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreateFloaterPlaceholderFrameFor(nsIPresContext*  aPresContext,
                                                        nsIContent*      aContent,
                                                        nsIFrame*        aFrame,
                                                        nsIStyleContext* aStyleContext,
                                                        nsIFrame*        aParentFrame,
                                                        nsIFrame**       aPlaceholderFrame)
{
  nsPlaceholderFrame* placeholder;
  nsresult  rv = NS_NewPlaceholderFrame((nsIFrame**)&placeholder);
  if (NS_SUCCEEDED(rv)) {
    // The placeholder frame gets a pseudo style context
    nsCOMPtr<nsIStyleContext>  sc;
    aPresContext->ResolvePseudoStyleContextFor(aContent,
                                               nsHTMLAtoms::placeholderPseudo,
                                               aStyleContext,
                                               PR_FALSE,
                                               getter_AddRefs(sc));
    placeholder->Init(*aPresContext, aContent, aParentFrame, sc, nsnull);
  
    // Add mapping from absolutely positioned frame to its placeholder frame
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->SetPlaceholderFrameFor(aFrame, placeholder);
  
    placeholder->SetAnchoredItem(aFrame);

    *aPlaceholderFrame = NS_STATIC_CAST(nsIFrame*, placeholder);
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
                                            PRBool           aIsAbsolutelyPositioned,
                                            PRBool &         aFrameHasBeenInitialized,
                                            PRBool           aIsFixedPositioned,
                                            nsAbsoluteItems& aFixedItems)
{
#define NS_FRAME_BASED_COMBO_WORKS 0
  nsresult rv = NS_OK;
  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);

  if (eWidgetRendering_Gfx == mode) {
    nsIDOMHTMLSelectElement* select   = nsnull;
    PRInt32                  size = 1;
    nsresult result = aContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
    if (NS_OK == result) {
      result = select->GetSize(&size); 
      if (1 == size) {
        if (! NS_FRAME_BASED_COMBO_WORKS) {
          rv = NS_NewSelectControlFrame(aNewFrame);
        } else {
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
                                  aAbsoluteItems,  aNewFrame, aFixedItems, aIsAbsolutelyPositioned, 
                                  aIsFixedPositioned, PR_TRUE);

            nsIFrame* placeholderFrame;

            CreatePlaceholderFrameFor(aPresContext, aContent, aNewFrame, aStyleContext,
                                      aParentFrame, placeholderFrame);

            // Add the absolutely positioned frame to its containing block's list
            // of child frames
            if (aIsAbsolutelyPositioned)
              aAbsoluteItems.AddChild(aNewFrame);

            listFrame = aNewFrame;

            // This needs to be done "after" the ListFrame has it's ChildList set
            // because the SetInitChildList intializes the ListBox selection state
            // and this method initializes the ComboBox's selection state
            comboBox->SetDropDown(placeholderFrame, listFrame);

            // Set up the Pseudo Style contents
            //XXX: What should happend if resolving the pseudo style fails?
            nsCOMPtr<nsIStyleContext> visiblePseudoStyle;
            aPresContext->ResolvePseudoStyleContextFor
                                (aContent, nsHTMLAtoms::dropDownVisible, aStyleContext, PR_FALSE, 
                                 getter_AddRefs(visiblePseudoStyle));

            nsCOMPtr<nsIStyleContext> hiddenPseudoStyle;
            aPresContext->ResolvePseudoStyleContextFor
                                (aContent, nsHTMLAtoms::dropDownHidden, aStyleContext, PR_FALSE,
                                 getter_AddRefs(hiddenPseudoStyle));

            nsCOMPtr<nsIStyleContext> outPseudoStyle;
            aPresContext->ResolvePseudoStyleContextFor
                                (aContent, nsHTMLAtoms::dropDownBtnOut, aStyleContext, PR_FALSE,
                                 getter_AddRefs(outPseudoStyle));

            nsCOMPtr<nsIStyleContext> pressPseudoStyle;
            aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::dropDownBtnPressed, 
                                 aStyleContext, PR_FALSE, getter_AddRefs(pressPseudoStyle));

            comboBox->SetDropDownStyleContexts(visiblePseudoStyle, hiddenPseudoStyle);
            comboBox->SetButtonStyleContexts(outPseudoStyle, pressPseudoStyle);

            aProcessChildren = PR_FALSE;
            nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, listFrame,
                                                     aStyleContext, PR_TRUE);
            aNewFrame = comboboxFrame;
          }
        }

      } else {
        nsIFrame * listFrame;
        rv = NS_NewListControlFrame(listFrame);
        aNewFrame = listFrame;
        InitializeScrollFrame(listFrame, aPresContext, aContent, aParentFrame, aStyleContext,
                              aAbsoluteItems,  aNewFrame, aFixedItems, aIsAbsolutelyPositioned, PR_FALSE, PR_TRUE);

         // Set flag so the events go to the listFrame not child frames.
        nsIView *listView; 
        listFrame->GetView(&listView);
        NS_ASSERTION(nsnull != listView,"ListFrame's view is nsnull");
        listView->SetViewFlags(NS_VIEW_FLAG_DONT_CHECK_CHILDREN);
        aFrameHasBeenInitialized = PR_TRUE;
      }
      NS_RELEASE(select);
    } else {
      rv = NS_NewSelectControlFrame(aNewFrame);
    }
  }
  else {
    // Not frame based. Use a frame which creates a native widget instead.
     rv = NS_NewSelectControlFrame(aNewFrame);
  }

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
                                           nsAbsoluteItems& aFixedItems,
                                           nsAbsoluteItems& aFloatingItems)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isFloating = PR_FALSE;
  PRBool    canBePositioned = PR_TRUE;
  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    newFrameIsFloaterContainer = PR_FALSE;
  PRBool    isReplaced = PR_FALSE;
  nsIFrame* beforeGenerated = nsnull;
  nsIFrame* afterGenerated = nsnull;
  nsresult  rv = NS_OK;

  if (nsLayoutAtoms::textTagName == aTag) {
    rv = NS_NewTextFrame(newFrame);
    isReplaced = PR_TRUE;   // XXX kipp: temporary
  }
  else {
    nsIHTMLContent *htmlContent;

    // Ignore the tag if it's not HTML content
    if (NS_SUCCEEDED(aContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent))) {
      NS_RELEASE(htmlContent);
      
      // See if the element is absolute or fixed positioned
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
      const nsStyleDisplay* display = (const nsStyleDisplay*)
        aStyleContext->GetStyleData(eStyleStruct_Display);
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
      }
      else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
      }
      else if (NS_STYLE_FLOAT_NONE != display->mFloats) {
        isFloating = PR_TRUE;
      }

      // Create a frame based on the tag
      if (nsHTMLAtoms::img == aTag) {
        isReplaced = PR_TRUE;
        // XXX If image display is turned off, then use ConstructAlternateImageFrame()
        // instead...
        rv = NS_NewImageFrame(newFrame);
      }
      else if (nsHTMLAtoms::hr == aTag) {
        rv = NS_NewHRFrame(newFrame);
      }
      else if (nsHTMLAtoms::br == aTag) {
        rv = NS_NewBRFrame(newFrame);
        isReplaced = PR_TRUE;
      }
      else if (nsHTMLAtoms::wbr == aTag) {
        rv = NS_NewWBRFrame(newFrame);
      }
      else if (nsHTMLAtoms::input == aTag) {
        isReplaced = PR_TRUE;
        rv = CreateInputFrame(aContent, newFrame);
      }
      else if (nsHTMLAtoms::textarea == aTag) {
        isReplaced = PR_TRUE;
        rv = NS_NewTextControlFrame(newFrame);
      }
      else if (nsHTMLAtoms::select == aTag) {
        isReplaced = PR_TRUE;
        rv = ConstructSelectFrame(aPresContext, aContent, aParentFrame,
                                  aTag, aStyleContext, aAbsoluteItems,
                                  newFrame,  processChildren,
                                  isAbsolutelyPositioned,
                                  frameHasBeenInitialized,
                                  isFixedPositioned, aFixedItems);
      }
      else if (nsHTMLAtoms::applet == aTag) {
        isReplaced = PR_TRUE;
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
        isReplaced = PR_TRUE;
        rv = NS_NewObjectFrame(newFrame);
      }
      else if (nsHTMLAtoms::form == aTag) {
        rv = NS_NewFormFrame(newFrame);
      }
      else if (nsHTMLAtoms::frameset == aTag) {
        rv = NS_NewHTMLFramesetFrame(newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        isReplaced = PR_TRUE;
        rv = NS_NewHTMLFrameOuterFrame(newFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        rv = NS_NewSpacerFrame(newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::button == aTag) {
        rv = NS_NewHTMLButtonControlFrame(newFrame);
        // the html4 button needs to act just like a 
        // regular button except contain html content
        // so it must be replaced or html outside it will
        // draw into its borders. -EDV
        isReplaced = PR_TRUE;
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
    // If the frame is a replaced element, then set the frame state bit
    if (isReplaced) {
      nsFrameState  state;
      newFrame->GetFrameState(&state);
      newFrame->SetFrameState(state | NS_FRAME_REPLACED_ELEMENT);
    }

    if (!frameHasBeenInitialized) {
      nsIFrame* geometricParent = aParentFrame;
       
      if (canBePositioned) {
        if (isAbsolutelyPositioned) {
          geometricParent = aAbsoluteItems.containingBlock;
        } else if (isFixedPositioned) {
          geometricParent = aFixedItems.containingBlock;
        }
      }
      
      newFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext,
                     nsnull);

      // See if we need to create a view, e.g. the frame is absolutely
      // positioned
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      nsAbsoluteItems floaterList(newFrame);
      if (processChildren) {
        if (newFrameIsFloaterContainer) {
          rv = ProcessChildren(aPresContext, aContent, newFrame,
                               aAbsoluteItems, childItems, aFixedItems,
                               floaterList);
        }
        else {
          rv = ProcessChildren(aPresContext, aContent, newFrame,
                               aAbsoluteItems, childItems, aFixedItems,
                               aFloatingItems);
        }
      } else {
        // Check for generated content before and after.
        // Note: we don't need to do this in the case where we're processing
        // children, because ProcessChildren() takes care of this...
        CreateGeneratedContentFrame(aPresContext, newFrame, aContent, aStyleContext,
                                    nsCSSAtoms::beforePseudo, &beforeGenerated);
        CreateGeneratedContentFrame(aPresContext, newFrame, aContent, aStyleContext,
                                    nsCSSAtoms::afterPseudo, &afterGenerated);

      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);

      // Set the frame's floating child list, too
      if (floaterList.childList) {
        newFrame->SetInitialChildList(*aPresContext,
                                      nsLayoutAtoms::floaterList,
                                      floaterList.childList);
      }
    }

    // If there's :before generated content, then add that frame first.
    // Note: 'position' and 'float' properties don't apply to :before and
    // :after pseudo elements, so we always add the generated content frames
    // to the flow
    if (beforeGenerated) {
      aFrameItems.AddChild(beforeGenerated);
    }

    // If the frame is positioned, then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresContext, aContent, newFrame,
                                aStyleContext, aParentFrame, placeholderFrame);

      // Add the positioned frame to its containing block's list of
      // child frames
      if (isAbsolutelyPositioned) {
        aAbsoluteItems.AddChild(newFrame);
      } else {
        aFixedItems.AddChild(newFrame);
      }

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);

    } else if (isFloating) {
      nsIFrame* placeholderFrame;
      CreateFloaterPlaceholderFrameFor(aPresContext, aContent, newFrame,
                                       aStyleContext, aParentFrame,
                                       &placeholderFrame);

      // Add the floating frame to its containing block's list of child frames
      aFloatingItems.AddChild(newFrame);

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
    } else {
      // Add the newly constructed frame to the flow
      aFrameItems.AddChild(newFrame);
    }

    // If there's :after generated content, then add that frame last
    if (afterGenerated) {
      aFrameItems.AddChild(afterGenerated);
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
                                         nsAbsoluteItems& aFloatingItems,
                                         PRBool&          haltProcessing)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isReplaced = PR_FALSE;

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
          if (pTag.get() == nsXULAtoms::treechildren)
          {
            // Always build rows. Rely on style rules to hide frames.
            // Rely on RDF trickery to hide synthetic content from the content model.
            
            nsAbsoluteItems floaterList(nsnull);
            rv = ProcessChildren(aPresContext, childContent, aParentFrame,
                                 aAbsoluteItems, aFrameItems, aFixedItems,
                                 floaterList);
            NS_ASSERTION(nsnull == floaterList.childList, "floater in odd spot");
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
    nsIFrame* ignore2;
    rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                 aAbsoluteItems, newFrame, ignore, ignore2, aFixedItems, 
                                 treeCreator);
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
      isReplaced = PR_TRUE;
      rv = NS_NewProgressMeterFrame(newFrame);
    }
    // End of PROGRESS METER CONSTRUCTION logic

    // BOX CONSTRUCTION
    else if (aTag == nsXULAtoms::box) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewBoxFrame(newFrame);
    }
    // End of BOX CONSTRUCTION logic

    // TITLED BUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::titledbutton) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTitledButtonFrame(newFrame);
    }
    // End of TITLED BUTTON CONSTRUCTION logic

  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {
    // If the frame is a replaced element, then set the frame state bit
    if (isReplaced) {
      nsFrameState  state;
      newFrame->GetFrameState(&state);
      newFrame->SetFrameState(state | NS_FRAME_REPLACED_ELEMENT);
    }

    nsIFrame* geometricParent = isAbsolutelyPositioned
      ? aAbsoluteItems.containingBlock
      : aParentFrame;
    newFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext,
                   nsnull);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(newFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren) {
      rv = ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                           childItems, aFixedItems, aFloatingItems);
    }


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
  scrollFrame->Init(*aPresContext, aContent, geometricParent, aStyleContext,
                    nsnull);

  // The scroll frame gets the original style context, and the scrolled
  // frame gets a SCROLLED-CONTENT pseudo element style context that
  // inherits the background properties
  nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent,
                                          nsHTMLAtoms::scrolledContentPseudo,
                                          aStyleContext, PR_FALSE,
                                          getter_AddRefs(scrolledPseudoStyle));

  // Create an area container for the frame
  nsIFrame* scrolledFrame;
  NS_NewAreaFrame(scrolledFrame, NS_BLOCK_SHRINK_WRAP);

  // Initialize the frame and force it to have a view
  scrolledFrame->Init(*aPresContext, aContent, scrollFrame,
                      scrolledPseudoStyle, nsnull);
  nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, scrolledFrame,
                                           scrolledPseudoStyle, PR_TRUE);

  // Process children
  nsAbsoluteItems floaterList(scrolledFrame);
  if (aIsAbsolutelyPositioned || aIsFixedPositioned) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    nsAbsoluteItems  absoluteItems(scrolledFrame);
    nsFrameItems     childItems;
    ProcessChildren(aPresContext, aContent, scrolledFrame, absoluteItems,
                    childItems, aFixedItems, floaterList);

    // Set the initial child lists
    scrolledFrame->SetInitialChildList(*aPresContext, nsnull,
                                       childItems.childList);
    if (nsnull != absoluteItems.childList) {
      scrolledFrame->SetInitialChildList(*aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         absoluteItems.childList);
    }
    if (floaterList.childList) {
      scrolledFrame->SetInitialChildList(*aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         floaterList.childList);
    }
  } else {
    nsFrameItems childItems;
    ProcessChildren(aPresContext, aContent, scrolledFrame, aAbsoluteItems,
                    childItems, aFixedItems, floaterList);
  
    // Set the initial child lists
    scrolledFrame->SetInitialChildList(*aPresContext, nsnull,
                                       childItems.childList);
    if (floaterList.childList) {
      scrolledFrame->SetInitialChildList(*aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         floaterList.childList);
    }
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
                                                   nsAbsoluteItems&      aFixedItems,
                                                   nsAbsoluteItems&      aFloatingItems)
{
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isFloating = PR_FALSE;
  PRBool    isBlock = aDisplay->IsBlockLevel();
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsTableCreator tableCreator; // Used to make table frames.
  nsresult  rv = NS_OK;

  // Get the position syle info
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);

  // The frame is also a block if it's an inline frame that's floated or
  // absolutely positioned
  if (NS_STYLE_FLOAT_NONE != aDisplay->mFloats) {
    isFloating = PR_TRUE;
  }
  if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) &&
      (isFloating || position->IsAbsolutelyPositioned())) {
    isBlock = PR_TRUE;
  }

  // If the frame is a block-level frame and is scrollable, then wrap it
  // in a scroll frame.
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
    InitializeScrollFrame(scrollFrame, aPresContext, aContent, aParentFrame,
                          aStyleContext, aAbsoluteItems,  newFrame,
                          aFixedItems, isAbsolutelyPositioned,
                          isFixedPositioned, PR_FALSE);
  }
  // See if the frame is absolute or fixed positioned
  else if (position->IsAbsolutelyPositioned() &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {

    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
      isAbsolutelyPositioned = PR_TRUE;
    } else {
      isFixedPositioned = PR_TRUE;
    }

    // Create an area frame
    NS_NewAreaFrame(newFrame, NS_BLOCK_MARGIN_ROOT);
    newFrame->Init(*aPresContext, aContent,
                   (isAbsolutelyPositioned
                    ? aAbsoluteItems.containingBlock
                    : aFixedItems.containingBlock),
                   aStyleContext, nsnull);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. The area frame becomes a container for child
    // frames that are absolutely positioned
    nsAbsoluteItems  absoluteItems(newFrame);
    nsFrameItems     childItems;
    nsAbsoluteItems  floaterList(newFrame);

    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems,
                    childItems, aFixedItems, floaterList);

    // Set the frame's initial child list(s)
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      newFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                     absoluteItems.childList);
    }
    if (floaterList.childList) {
      newFrame->SetInitialChildList(*aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    floaterList.childList);
    }
  }
  // See if the frame is floated, and it's a block or inline frame
  else if (isFloating &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {
    // Create an area frame
    NS_NewAreaFrame(newFrame, NS_BLOCK_SHRINK_WRAP|NS_BLOCK_MARGIN_ROOT);

    // Initialize the frame
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, nsnull);

    // See if we need to create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content
    nsFrameItems childItems;
    nsAbsoluteItems  floaterList(newFrame);
    ProcessChildren(aPresContext, aContent, newFrame, aAbsoluteItems,
                    childItems, aFixedItems, floaterList);

    // Set the frame's initial child list(s)
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (floaterList.childList) {
      newFrame->SetInitialChildList(*aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    floaterList.childList);
    }
  }
  // See if it's a relatively positioned block
  else if ((NS_STYLE_POSITION_RELATIVE == position->mPosition) &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay))) {

    // Create an area frame. No space manager, though
    NS_NewAreaFrame(newFrame, NS_AREA_NO_SPACE_MGR);
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, nsnull);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. Relatively positioned frames becomes a
    // container for child frames that are positioned
    nsAbsoluteItems  absoluteItems(newFrame);
    nsAbsoluteItems  floaterList(newFrame);
    nsFrameItems     childItems;
    ProcessChildren(aPresContext, aContent, newFrame, absoluteItems,
                    childItems, aFixedItems, floaterList);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
    if (nsnull != absoluteItems.childList) {
      newFrame->SetInitialChildList(*aPresContext, nsLayoutAtoms::absoluteList,
                                    absoluteItems.childList);
    }
    if (floaterList.childList) {
      newFrame->SetInitialChildList(*aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    floaterList.childList);
    }
  }
  // otherwise let the display property influence the frame type to create
  else {
    PRBool  processChildren = PR_FALSE;  // whether we should process child content
    PRBool  newFrameIsFloaterContainer = PR_FALSE;
    nsIFrame* ignore;

    // Use the 'display' property to choose a frame type
    switch (aDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
    case NS_STYLE_DISPLAY_RUN_IN:
    case NS_STYLE_DISPLAY_COMPACT:
      rv = NS_NewBlockFrame(newFrame, 0);
      processChildren = PR_TRUE;
      newFrameIsFloaterContainer = PR_TRUE;
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
      // Note: table construction function takes care of initializing
      // the frame, processing children, and setting the initial child
      // list
      goto nearly_done;
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
      {
        nsIFrame* ignore2;
        rv = ConstructTableCellFrame(aPresContext, aContent, aParentFrame, aStyleContext, 
                                     aAbsoluteItems, newFrame, ignore, ignore2, 
                                     aFixedItems, tableCreator);
        aFrameItems.AddChild(newFrame);
        return rv;
      }
  
    default:
      // Don't create any frame for content that's not displayed...
      break;
    }

    // If we succeeded in creating a frame then initialize the frame and
    // process children if requested
    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, nsnull);

      // See if we need to create a view, e.g. the frame is absolutely positioned
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        if (newFrameIsFloaterContainer) {
          nsAbsoluteItems floaterList(newFrame);
          rv = ProcessChildren(aPresContext, aContent, newFrame,
                               aAbsoluteItems, childItems, aFixedItems,
                               aFloatingItems);

          // Set the frame's initial child list
          newFrame->SetInitialChildList(*aPresContext, nsnull,
                                        childItems.childList);
          if (floaterList.childList) {
            newFrame->SetInitialChildList(*aPresContext,
                                          nsLayoutAtoms::floaterList,
                                          floaterList.childList);
          }
        }
        else {
          rv = ProcessChildren(aPresContext, aContent, newFrame,
                               aAbsoluteItems, childItems, aFixedItems,
                               aFloatingItems);

          // Set the frame's initial child list
          newFrame->SetInitialChildList(*aPresContext, nsnull,
                                        childItems.childList);
        }
      }

    }
  }

  // If the frame is absolutely positioned, then create a placeholder frame
 nearly_done:
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

  } else if (isFloating) {
    nsIFrame* placeholderFrame;
    CreateFloaterPlaceholderFrameFor(aPresContext, aContent, newFrame,
                                     aStyleContext, aParentFrame,
                                     &placeholderFrame);

    // Add the floating frame to its containing block's list of child frames
    aFloatingItems.AddChild(newFrame);

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
      } else { // else the child is a caption and really belongs to the outer table frame 
        nsIFrame* parFrame = nsnull;
        aCurrentParentFrame->GetParent(&parFrame);
        const nsStyleDisplay* parDisplay;
        aCurrentParentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)parDisplay);
        if (NS_STYLE_DISPLAY_TABLE == parDisplay->mDisplay) {
          aNewParentFrame = parFrame; // aNewParentFrame was an inner frame
        }
      }
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
    } else if (nsLayoutAtoms::processingInstructionTagName == aTag) {
    // Use a special pseudo element style context for comments
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::processingInstructionPseudo, 
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
                                      nsAbsoluteItems& aFixedItems,
                                      nsAbsoluteItems& aFloatingItems)
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
  } else if (nsLayoutAtoms::processingInstructionTagName == tag) {
    // Use a special pseudo element style context for comments
    nsCOMPtr<nsIContent> parentContent;
    if (nsnull != aParentFrame) {
      aParentFrame->GetContent(getter_AddRefs(parentContent));
    }
    rv = aPresContext->ResolvePseudoStyleContextFor(parentContent, 
                                                    nsHTMLAtoms::processingInstructionPseudo, 
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
                               aFixedItems, aFloatingItems);

#ifdef INCLUDE_XUL
      // Failing to find a matching HTML frame, try creating a specialized
      // XUL frame. This is temporary, pending planned factoring of this
      // whole process into separate, pluggable steps.
      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                             (lastChild == aFrameItems.lastChild))) {
        PRBool haltProcessing = PR_FALSE;
        rv = ConstructXULFrame(aPresContext, aContent, aParentFrame, tag,
                               styleContext, aAbsoluteItems, aFrameItems,
                               aFixedItems, aFloatingItems, haltProcessing);
        if (haltProcessing) {
          return rv;
        }
      } 
#endif

      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                               (lastChild == aFrameItems.lastChild))) {
        // When there is no explicit frame to create, assume it's a
        // container and let display style dictate the rest
        rv = ConstructFrameByDisplayType(aPresContext, display, aContent,
                                         aParentFrame, styleContext,
                                         aAbsoluteItems, aFrameItems,
                                         aFixedItems, aFloatingItems);
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ReconstructDocElementHierarchy(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  
  if (nsnull != mDocument) {
    nsCOMPtr<nsIContent> rootContent(dont_AddRef(mDocument->GetRootContent()));

    if (rootContent) {
      nsCOMPtr<nsIPresShell> shell;
      rv = aPresContext->GetShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv)) {
        nsIFrame* docElementFrame;
        
        // Get the frame that corresponds to the document element
        shell->GetPrimaryFrameFor(rootContent, &docElementFrame);

        if (nsnull != docElementFrame) {
          nsIFrame* docParentFrame;
          docElementFrame->GetParent(&docParentFrame);

          if (nsnull != docParentFrame) {
            // Remove the old document element hieararchy
            rv = docParentFrame->RemoveFrame(*aPresContext, 
                                             *shell,
                                             nsnull, 
                                             docElementFrame);
            // XXX Remove any existing fixed items...
        
            if (NS_SUCCEEDED(rv)) {
              nsIFrame*                 newChild;
              nsCOMPtr<nsIStyleContext> rootPseudoStyle;
              nsAbsoluteItems           fixedItems(mFixedContainingBlock);
          
              docParentFrame->GetStyleContext(getter_AddRefs(rootPseudoStyle));
              rv = ConstructDocElementFrame(aPresContext, rootContent,
                                            docParentFrame, rootPseudoStyle,
                                            newChild, fixedItems);

              if (NS_SUCCEEDED(rv)) {
                rv = docParentFrame->InsertFrames(*aPresContext, *shell, nsnull,
                                                  nsnull, newChild);

                // Tell the fixed containing block about its 'fixed' frames
                if (nsnull != fixedItems.childList) {
                  mFixedContainingBlock->InsertFrames(*aPresContext, *shell,
                    nsLayoutAtoms::fixedList, nsnull, fixedItems.childList);
                }
              }
            }
          }
        }
      }
    }
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

nsIFrame*
nsCSSFrameConstructor::GetFloaterContainingBlock(nsIPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  NS_PRECONDITION(mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is a real block frame
  nsIFrame* containingBlock = aFrame;
  while (nsnull != containingBlock) {
    const nsStyleDisplay* display;
    containingBlock->GetStyleData(eStyleStruct_Display,
                                  (const nsStyleStruct*&)display);
    if ((NS_STYLE_DISPLAY_BLOCK == display->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay)) {
      break;
    }

    // Continue walking up the hierarchy
    containingBlock->GetParent(&containingBlock);
  }

  // If we didn't find a containing block, then use the initial
  // containing block
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
      parentFrame->GetNextInFlow(&nextInFlow);
      if (nsnull != nextInFlow) {
        parentFrame = nextInFlow;
      }
    }

    // Get the containing block for absolutely positioned elements
    nsIFrame* absoluteContainingBlock =
      GetAbsoluteContainingBlock(aPresContext, parentFrame);

    nsIFrame* floaterContainingBlock =
      GetFloaterContainingBlock(aPresContext, parentFrame);

    // Create some new frames
    PRInt32         count;
    nsIFrame*       firstAppendedFrame = nsnull;
    nsAbsoluteItems absoluteItems(absoluteContainingBlock);
    nsAbsoluteItems fixedItems(mFixedContainingBlock);
    nsAbsoluteItems floaterList(floaterContainingBlock);
    nsFrameItems    frameItems;

    aContainer->ChildCount(count);

    for (PRInt32 i = aNewIndexInContainer; i < count; i++) {
      nsCOMPtr<nsIContent> child;
      
      aContainer->ChildAt(i, *getter_AddRefs(child));
      ConstructFrame(aPresContext, child, parentFrame, absoluteItems,
                     frameItems, fixedItems, floaterList);
    }

    // Adjust parent frame for table inner/outer frame
    // we need to do this here because we need both the parent frame and the constructed frame
    nsresult result = NS_OK;
    nsIFrame *adjustedParentFrame=parentFrame;
    firstAppendedFrame = frameItems.childList;
    if (nsnull != firstAppendedFrame) {
      const nsStyleDisplay* firstAppendedFrameDisplay;
      firstAppendedFrame->GetStyleData(eStyleStruct_Display,
                                       (const nsStyleStruct *&)firstAppendedFrameDisplay);
      result = GetAdjustedParentFrame(parentFrame,
                                      firstAppendedFrameDisplay->mDisplay,
                                      adjustedParentFrame);
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

      // If there are new floating child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (floaterList.childList) {
        floaterList.containingBlock->AppendFrames(*aPresContext, *shell,
                                                  nsLayoutAtoms::floaterList,
                                                  floaterList.childList);
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
        prevSibling->GetNextInFlow(&nextInFlow);
        if (nsnull != nextInFlow) {
          prevSibling = nextInFlow;
        }
      } while (nsnull != nextInFlow);

      // Did we really get the *right* frame?
      const nsStyleDisplay* display;
      prevSibling->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)display);
      if (display->IsFloating()) {
        // Nope. Get the place-holder instead
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
        NS_ASSERTION(nsnull != placeholderFrame, "yikes");
        prevSibling = placeholderFrame;
      }

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
        nextSibling->GetNextInFlow(&nextInFlow);
        if (nsnull != nextInFlow) {
          nextSibling = nextInFlow;
        }
      } while (nsnull != nextInFlow);

      // Did we really get the *right* frame?
      const nsStyleDisplay* display;
      nextSibling->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)display);
      if (display->IsFloating()) {
        // Nope. Get the place-holder instead
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(nextSibling, &placeholderFrame);
        NS_ASSERTION(nsnull != placeholderFrame, "yikes");
        nextSibling = placeholderFrame;
      }

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
    nsIContent* docElement = mDocument->GetRootContent();

    // Get the style context of the containing block frame
    nsCOMPtr<nsIStyleContext> containerStyle;
    mDocElementContainingBlock->GetStyleContext(getter_AddRefs(containerStyle));
    
    // Create frames for the document element and its child elements
    nsIFrame*       docElementFrame;
    nsAbsoluteItems fixedItems(mFixedContainingBlock);
    ConstructDocElementFrame(aPresContext, 
                             docElement, 
                             mDocElementContainingBlock,
                             containerStyle, 
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

#ifdef INCLUDE_XUL
      // Need to (for XUL only) do a special check for the treechildren tag
      PRInt32 nameSpaceID;
      if (NS_SUCCEEDED(aContainer->GetNameSpaceID(nameSpaceID)) &&
          nameSpaceID == nsXULAtoms::nameSpaceID) {
        // See if we're the treechildren tag.  This tag is treated differently,
        // since it has no corresponding frame, but may have children that have
        // frames (the whole hierarchical content model vs. flat table frame model
        // problem).
        nsIAtom* tag;
        aContainer->GetTag(tag);
        nsString tagName;
        tag->ToString(tagName);
        if (tagName == "treechildren")
        {
          // Retrieve the parent treeitem, and then obtain its frame.  This is
          // the prevSibling frame that we should use.
          nsCOMPtr<nsIContent> parentItem;
          aContainer->GetParent(*getter_AddRefs(parentItem));
          
          shell->GetPrimaryFrameFor(parentItem, &prevSibling);
          prevSibling->GetParent(&parentFrame);
          // XXX: Optimize for the lazy frame instantiation case. Need to bail
          // if our frame isn't visible
        }
        else
        {
          // No previous or next sibling so treat this like an appended frame.
          // XXX This won't always be true if there's auto-generated before/after
          // content
          isAppend = PR_TRUE;
          shell->GetPrimaryFrameFor(aContainer, &parentFrame);
        }      
      }
      else {
#endif // INCLUDE_XUL

      // No previous or next sibling so treat this like an appended frame.
      // XXX This won't always be true if there's auto-generated before/after
      // content
      isAppend = PR_TRUE;
      shell->GetPrimaryFrameFor(aContainer, &parentFrame);
#ifdef INCLUDE_XUL
      }
#endif  
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
      nsIFrame* absoluteContainingBlock =
        GetAbsoluteContainingBlock(aPresContext, parentFrame);

      // Get the containing block for floating elements
      nsIFrame* floaterContainingBlock =
        GetFloaterContainingBlock(aPresContext, parentFrame);
      
      nsAbsoluteItems absoluteItems(absoluteContainingBlock);
      nsAbsoluteItems floaterList(floaterContainingBlock);
      nsFrameItems    frameItems;
      nsAbsoluteItems fixedItems(mFixedContainingBlock);
      rv = ConstructFrame(aPresContext, aChild, parentFrame, absoluteItems,
                          frameItems, fixedItems, floaterList);
      
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
        
        // If there are new floating child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (floaterList.childList) {
          rv = floaterList.containingBlock->AppendFrames(*aPresContext, *shell,
                                                   nsLayoutAtoms::floaterList,
                                                   floaterList.childList);
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
    // See if the child frame is a floating frame
    const nsStyleDisplay* display;
    childFrame->GetStyleData(eStyleStruct_Display,
                             (const nsStyleStruct*&)display);
    if (display->IsFloating()) {
      // Get the placeholder frame
      nsIFrame* placeholderFrame;
      shell->GetPlaceholderFrameFor(childFrame, &placeholderFrame);

      // Remove the mapping from the frame to its placeholder
      shell->SetPlaceholderFrameFor(childFrame, nsnull);

      // Now we remove the floating frame

      // XXX has to be done first for now: the blocks line list
      // contains an array of pointers to the placeholder - we have to
      // remove the floater first (which gets rid of the lines
      // reference to the placeholder and floater) and then remove the
      // placeholder
      nsIFrame* parentFrame;
      childFrame->GetParent(&parentFrame);
      rv = parentFrame->RemoveFrame(*aPresContext, *shell,
                                    nsLayoutAtoms::floaterList, childFrame);

      // Remove the placeholder frame first (XXX second for now) (so
      // that it doesn't retain a dangling pointer to memory)
      if (nsnull != placeholderFrame) {
        placeholderFrame->GetParent(&parentFrame);
        rv = parentFrame->RemoveFrame(*aPresContext, *shell, nsnull,
                                      placeholderFrame);
      }
    }
    else {
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

  // XXX this needs to detect the need for a view due to an opacity change and deal with it...

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

    aFrame->GetNextInFlow(&aFrame);
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


PRInt32 
nsCSSFrameConstructor::FindRestyledFramesBelow(nsIFrame* aFrame, 
                                               nsIPresContext* aPresContext,
                                               PRInt32 aParentHint,
                                               nsStyleChangeList& aChanges)
{
  PRInt32 count = 0;
  PRInt32 listIndex = 0;
  PRInt32 childChange;
  nsIAtom* childList = nsnull;

  do {
    nsIFrame* child = nsnull;
    aFrame->FirstChild(childList, &child);
    while (child) {
      nsIStyleContext* oldContext;
      child->GetStyleContext(&oldContext);
      if (oldContext) {
        nsIStyleContext* parentContext = oldContext->GetParent();
        nsresult didChange = child->ReResolveStyleContext(aPresContext, parentContext, 
                                                          aParentHint, &aChanges,
                                                          &childChange);
        NS_IF_RELEASE(parentContext);
        if (NS_SUCCEEDED(didChange)) {
          if (NS_COMFALSE == didChange) {
            count += FindRestyledFramesBelow(child, aPresContext, aParentHint, aChanges);
          }
          else {
            nsIStyleContext* newContext;
            child->GetStyleContext(&newContext);
            if (newContext) {
              PRInt32 hint = NS_STYLE_HINT_NONE;
              newContext->CalcStyleDifference(oldContext, hint);
              if (aParentHint < hint) {  // if child needs more than parent did
                aChanges.AppendChange(child, hint);
                count++;
                if (hint < NS_STYLE_HINT_FRAMECHANGE) { // look for greater change below
                  count += FindRestyledFramesBelow(child, aPresContext, hint, aChanges);
                }
              }
              NS_RELEASE(newContext);
            }
          }
        }
        NS_RELEASE(oldContext);
      }
      child->GetNextSibling(&child);
    }
    NS_IF_RELEASE(childList);
    aFrame->GetAdditionalChildListName(listIndex++, &childList);
  } while (childList);
  NS_IF_RELEASE(childList);
  return count;
}

NS_IMETHODIMP 
nsCSSFrameConstructor::ProcessRestyledFrames(nsStyleChangeList& aChangeList, 
                                             nsIPresContext* aPresContext)
{
  PRInt32 count = aChangeList.Count();
  while (0 < count--) {
    nsIFrame* frame;
    PRInt32 hint;
    aChangeList.ChangeAt(count, frame, hint);
    switch (hint) {
      case NS_STYLE_HINT_RECONSTRUCT_ALL:
        NS_ERROR("This shouldn't happen");
        break;
      case NS_STYLE_HINT_FRAMECHANGE:
        nsIContent* content;
        frame->GetContent(&content);
        RecreateFramesForContent(aPresContext, content);
        NS_IF_RELEASE(content);
        break;
      case NS_STYLE_HINT_REFLOW:
        StyleChangeReflow(aPresContext, frame, nsnull);
        break;
      case NS_STYLE_HINT_VISUAL:
        ApplyRenderingChangeToTree(aPresContext, frame);
        break;
      case NS_STYLE_HINT_CONTENT:
      default:
        break;
    }
  }
  aChangeList.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentStateChanged(nsIPresContext* aPresContext, 
                                           nsIContent* aContent)
{
  nsresult  result = NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame*     primaryFrame;
   
  shell->GetPrimaryFrameFor(aContent, &primaryFrame);

  // XXX need to add mechanism to detect what state changes need even be considered
  // this needs to happen at the style sheet level before this gets called
  
#if 0
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("HTMLStyleSheet::ContentStateChanged: content=%p[%s] frame=%p",
      aContent, ContentTag(aContent, 0), primaryFrame));
#endif

  if (primaryFrame) {
    nsIFrame* frame = primaryFrame;
    nsStyleChangeList changeList;
    PRInt32 frameChange = NS_STYLE_HINT_NONE;
    do {
      nsIStyleContext* oldFrameContext;
      frame->GetStyleContext(&oldFrameContext);
      NS_ASSERTION(nsnull != oldFrameContext, "frame must have style context");
      if (oldFrameContext) {
        nsIStyleContext*  parentContext = oldFrameContext->GetParent();
        nsresult didChange = frame->ReResolveStyleContext(aPresContext, parentContext,
                                                          frameChange, &changeList, 
                                                          &frameChange);
        NS_IF_RELEASE(parentContext);
        if (NS_SUCCEEDED(didChange)) {
          if (NS_COMFALSE == didChange) {
            // need remap? I don't think so
            FindRestyledFramesBelow(frame, aPresContext, frameChange, changeList);
          }
        }
        NS_RELEASE(oldFrameContext);
      }    
      frame->GetNextInFlow(&frame);
    } while (frame);

    switch (frameChange) {  // max change needed for top level frames
      case NS_STYLE_HINT_RECONSTRUCT_ALL:
        result = ReconstructDocElementHierarchy(aPresContext);
      case NS_STYLE_HINT_FRAMECHANGE:
        result = RecreateFramesForContent(aPresContext, aContent);
      case NS_STYLE_HINT_REFLOW:
      case NS_STYLE_HINT_VISUAL:
        ProcessRestyledFrames(changeList, aPresContext);
        break;
      case NS_STYLE_HINT_CONTENT:
        // let primary frame deal with it
        result = primaryFrame->ContentChanged(aPresContext, aContent, nsnull);
        // then process any children that need it
        ProcessRestyledFrames(changeList, aPresContext);
      default:
        break;
    }
  }
  else {
    result = RecreateFramesForContent(aPresContext, aContent);
  }
  return result;
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
  nsIFrame*     primaryFrame;
   
  shell->GetPrimaryFrameFor(aContent, &primaryFrame);

  PRBool  reconstruct = PR_FALSE;
  PRBool  restyle = PR_FALSE;
  PRBool  reframe = PR_FALSE;

#if 0
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("HTMLStyleSheet::AttributeChanged: content=%p[%s] frame=%p",
      aContent, ContentTag(aContent, 0), frame));
#endif

  // the style tag has its own interpretation based on aHint 
  if (NS_STYLE_HINT_UNKNOWN == aHint) { 
    nsIStyledContent* styledContent;
    result = aContent->QueryInterface(nsIStyledContent::GetIID(), (void**)&styledContent);

    if (NS_OK == result) { 
      // Get style hint from HTML content object. 
      styledContent->GetStyleHintForAttributeChange(aAttribute, &aHint);
      NS_RELEASE(styledContent); 
    } 
  } 

  switch (aHint) {
    default:
    case NS_STYLE_HINT_RECONSTRUCT_ALL:
      reconstruct = PR_TRUE;
    case NS_STYLE_HINT_FRAMECHANGE:
      reframe = PR_TRUE;
    case NS_STYLE_HINT_REFLOW:
    case NS_STYLE_HINT_VISUAL:
    case NS_STYLE_HINT_UNKNOWN:
    case NS_STYLE_HINT_CONTENT:
    case NS_STYLE_HINT_AURAL:
      restyle = PR_TRUE;
      break;
    case NS_STYLE_HINT_NONE:
      break;
  }

  // apply changes
  if (PR_TRUE == reconstruct) {
    result = ReconstructDocElementHierarchy(aPresContext);
  }
  else if (PR_TRUE == reframe) {
    result = RecreateFramesForContent(aPresContext, aContent);
  }
  else if (PR_TRUE == restyle) {
    // If there is no frame then there is no point in re-styling it,
    // is there?
    if (primaryFrame) {
      PRInt32 maxHint = aHint;
      nsIFrame* frame = primaryFrame;
      nsStyleChangeList changeList;
      // put primary frame on list to deal with, re-resolve may update or add next in flows
      changeList.AppendChange(primaryFrame, maxHint);
      do {
        nsIStyleContext* oldFrameContext;
        frame->GetStyleContext(&oldFrameContext);
        NS_ASSERTION(nsnull != oldFrameContext, "frame must have style context");
        if (oldFrameContext) {
          nsIStyleContext*  parentContext = oldFrameContext->GetParent();
          nsresult didChange = frame->ReResolveStyleContext(aPresContext, parentContext, maxHint,
                                                            &changeList, &maxHint);
          NS_IF_RELEASE(parentContext);
          if (NS_SUCCEEDED(didChange)) {
            if (NS_COMFALSE == didChange) {
              if (maxHint < NS_STYLE_HINT_FRAMECHANGE) {
                oldFrameContext->RemapStyle(aPresContext);
                FindRestyledFramesBelow(frame, aPresContext, maxHint, changeList);
              }
            }
          }
          NS_RELEASE(oldFrameContext);
        }    
        frame->GetNextInFlow(&frame);
      } while (frame);

      switch (maxHint) {
        case NS_STYLE_HINT_RECONSTRUCT_ALL:
          result = ReconstructDocElementHierarchy(aPresContext);
          break;
        case NS_STYLE_HINT_FRAMECHANGE:
          result = RecreateFramesForContent(aPresContext, aContent);
          break;
        case NS_STYLE_HINT_REFLOW:
        case NS_STYLE_HINT_VISUAL:
          ProcessRestyledFrames(changeList, aPresContext);
          break;
        case NS_STYLE_HINT_CONTENT:
          // let the frame deal with it, since we don't know how to
          result = primaryFrame->AttributeChanged(aPresContext, aContent, aAttribute, maxHint);
          // process any child frames that need it
          ProcessRestyledFrames(changeList, aPresContext);
        default:
          break;
      }
    }
    else {  // no frame now, possibly genetate one with new style data
      result = RecreateFramesForContent(aPresContext, aContent);
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
  nsresult result = NS_OK;
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

  if (reframe) {
    result = ReconstructDocElementHierarchy(aPresContext);
  }
  else if (restyle) {
    nsIStyleContext* sc;
    frame->GetStyleContext(&sc);
    sc->RemapStyle(aPresContext);
    NS_RELEASE(sc);

    // XXX hack, skip the root and scrolling frames
    frame->FirstChild(nsnull, &frame);
    frame->FirstChild(nsnull, &frame);
    if (reflow) {
      StyleChangeReflow(aPresContext, frame, nsnull);
    }
    else if (render) {
      ApplyRenderingChangeToTree(aPresContext, frame);
    }
  }

  return result;
}

NS_IMETHODIMP
nsCSSFrameConstructor::StyleRuleAdded(nsIPresContext* aPresContext,
                                      nsIStyleSheet* aStyleSheet,
                                      nsIStyleRule* aStyleRule)
{
  // XXX TBI: should query rule for impact and do minimal work
  ReconstructDocElementHierarchy(aPresContext);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFrameConstructor::StyleRuleRemoved(nsIPresContext* aPresContext,
                                        nsIStyleSheet* aStyleSheet,
                                        nsIStyleRule* aStyleRule)
{
  // XXX TBI: should query rule for impact and do minimal work
  ReconstructDocElementHierarchy(aPresContext);
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
      inlineFrame->Init(*aPresContext, aContent, aParentFrame, textStyleContext, nsnull);
      textFrame->Init(*aPresContext, altTextContent, inlineFrame,
                      textStyleContext, nsnull);
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

  // Get the previous sibling frame
  // XXX Handle absolutely positioned and floated elements...
  nsIFrame*     firstChild;
  parentFrame->FirstChild(nsnull, &firstChild);
  nsFrameList   frameList(firstChild);
  nsIFrame*     prevSibling = frameList.GetPrevSiblingFor(aFrame);
  
  // See whether it's an IMG or an OBJECT element
  if (nsHTMLAtoms::img == tag.get()) {
    // It's an IMG element. Try and construct an alternate frame to use when the
    // image can't be rendered
    nsIFrame* newFrame;
    rv = ConstructAlternateImageFrame(aPresContext, content, parentFrame, newFrame);

    if (NS_SUCCEEDED(rv)) {
      // Delete the current frame and insert the new frame
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      parentFrame->RemoveFrame(*aPresContext, *presShell, nsnull, aFrame);
      parentFrame->InsertFrames(*aPresContext, *presShell, nsnull, prevSibling, newFrame);
    }

  } else if ((nsHTMLAtoms::object == tag.get()) ||
             (nsHTMLAtoms::applet == tag.get())) {
    // It's an OBJECT element or APPLET, so we should display the contents instead
    nsCOMPtr<nsIStyleContext> styleContext;
    const nsStyleDisplay*     display;

    aFrame->GetStyleContext(getter_AddRefs(styleContext));
    display = (const nsStyleDisplay*)styleContext->GetStyleData(eStyleStruct_Display);

    // Get the containing block for absolutely positioned elements
    nsIFrame* absoluteContainingBlock =
      GetAbsoluteContainingBlock(aPresContext, parentFrame);

    // Get the containing block for floating elements
    nsIFrame* floaterContainingBlock =
      GetFloaterContainingBlock(aPresContext, parentFrame);
    
    // Create a frame based on the display type
    nsAbsoluteItems absoluteItems(absoluteContainingBlock);
    nsAbsoluteItems floaterList(floaterContainingBlock);
    nsFrameItems    frameItems;
    nsAbsoluteItems fixedItems(mFixedContainingBlock);

    rv = ConstructFrameByDisplayType(aPresContext, display, content,
                                     parentFrame, styleContext,
                                     absoluteItems, frameItems,
                                     fixedItems, floaterList);

    nsIFrame* newFrame = frameItems.childList;
    if (NS_SUCCEEDED(rv)) {
      // Delete the current frame and insert the new frame
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      parentFrame->RemoveFrame(*aPresContext, *presShell, nsnull, aFrame);
      parentFrame->InsertFrames(*aPresContext, *presShell, nsnull, prevSibling, newFrame);
    
      // If there are new absolutely positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (nsnull != absoluteItems.childList) {
        rv = absoluteItems.containingBlock->AppendFrames(*aPresContext, *presShell,
                                                         nsLayoutAtoms::absoluteList,
                                                         absoluteItems.childList);
      }

      // If there are new fixed positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (nsnull != fixedItems.childList) {
        rv = fixedItems.containingBlock->AppendFrames(*aPresContext, *presShell,
                                                      nsLayoutAtoms::fixedList,
                                                      fixedItems.childList);
      }

      // If there are new floating child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (floaterList.childList) {
        rv = floaterList.containingBlock->AppendFrames(*aPresContext,
                                                       *presShell,
                                                    nsLayoutAtoms::floaterList,
                                                    floaterList.childList);
      }
    }

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected tag");
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingOuterTableFrame(nsIPresContext*  aPresContext,
                                                       nsIFrame*        aFrame,
                                                       nsIFrame*        aParentFrame,
                                                       nsIContent*      aContent,
                                                       nsIStyleContext* aStyleContext,
                                                       nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;

  rv = NS_NewTableOuterFrame(newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Create a continuing inner table frame, and if there's a caption then
    // replicate the caption
    nsIFrame*     childFrame;
    nsFrameItems  newChildFrames;

    aFrame->FirstChild(nsnull, &childFrame);
    while (childFrame) {
      nsIAtom*  tableType;

      // See if it's the inner table frame
      childFrame->GetFrameType(&tableType);
      if (nsLayoutAtoms::tableFrame == tableType) {
        nsIFrame* continuingTableFrame;

        // It's the inner table frame, so create a continuing frame
        CreateContinuingFrame(aPresContext, childFrame, newFrame, &continuingTableFrame);
        newChildFrames.AddChild(continuingTableFrame);
      } else {
        nsIContent*           caption;
        nsIStyleContext*      captionStyle;
        const nsStyleDisplay* display;

        childFrame->GetContent(&caption);
        childFrame->GetStyleContext(&captionStyle);
        display = (const nsStyleDisplay*)captionStyle->GetStyleData(eStyleStruct_Display);
        NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_CAPTION == display->mDisplay, "expected caption");

        // Get the containing block for absolutely positioned elements
        nsIFrame* absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext,
                                                                       newFrame);

        // Replicate the caption frame
        // XXX We have to do it this way instead of calling ConstructFrameByDisplayType(),
        // because of a bug in the way ConstructTableFrame() handles the initial child
        // list...
        nsIFrame*       captionFrame;
        nsFrameItems    childItems;
        nsAbsoluteItems absoluteItems(absoluteContainingBlock);
        nsAbsoluteItems fixedItems(mFixedContainingBlock);
        nsAbsoluteItems floaterList(captionFrame);
        NS_NewAreaFrame(captionFrame, 0);
        captionFrame->Init(*aPresContext, caption, newFrame, captionStyle, nsnull);
        ProcessChildren(aPresContext, caption, captionFrame, absoluteItems, 
                        childItems, fixedItems, floaterList);
        captionFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);
        // XXX Deal with absolute and fixed frames...
        if (floaterList.childList) {
          captionFrame->SetInitialChildList(*aPresContext,
                                            nsLayoutAtoms::floaterList,
                                            floaterList.childList);
        }
        newChildFrames.AddChild(captionFrame);
        NS_RELEASE(caption);
        NS_RELEASE(captionStyle);
      }
      NS_IF_RELEASE(tableType);
      childFrame->GetNextSibling(&childFrame);
    }

    // Set the outer table's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, newChildFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingTableFrame(nsIPresContext*  aPresContext,
                                                  nsIFrame*        aFrame,
                                                  nsIFrame*        aParentFrame,
                                                  nsIContent*      aContent,
                                                  nsIStyleContext* aStyleContext,
                                                  nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;
    
  rv = NS_NewTableFrame(newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(*aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Replicate any header/footer frames
    nsIFrame*     rowGroupFrame;
    nsFrameItems  childFrames;

    aFrame->FirstChild(nsnull, &rowGroupFrame);
    while (rowGroupFrame) {
      // See if it's a header/footer
      nsIStyleContext*      rowGroupStyle;
      const nsStyleDisplay* display;

      rowGroupFrame->GetStyleContext(&rowGroupStyle);
      display = (const nsStyleDisplay*)rowGroupStyle->GetStyleData(eStyleStruct_Display);

      if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay)) {
        
        // Get the containing block for absolutely positioned elements
        nsIFrame* absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext,
                                                                       newFrame);
        // Replicate the header/footer frame
        nsIFrame*        headerFooterFrame;
        nsFrameItems     childItems;
        nsAbsoluteItems  absoluteItems(absoluteContainingBlock);
        nsAbsoluteItems  fixedItems(mFixedContainingBlock);
        nsAbsoluteItems  floaterList(nsnull);
        nsIContent*      headerFooter;

        NS_NewTableRowGroupFrame(headerFooterFrame);
        rowGroupFrame->GetContent(&headerFooter);
        headerFooterFrame->Init(*aPresContext, headerFooter, newFrame,
                                rowGroupStyle, nsnull);
        ProcessChildren(aPresContext, headerFooter, headerFooterFrame, absoluteItems, 
                        childItems, fixedItems, floaterList);
        NS_ASSERTION(!floaterList.childList, "unexpected floated element");
        NS_RELEASE(headerFooter);
        headerFooterFrame->SetInitialChildList(*aPresContext, nsnull, childItems.childList);

        // Table specific initialization
        ((nsTableRowGroupFrame*)headerFooterFrame)->InitRepeatedFrame
          ((nsTableRowGroupFrame*)rowGroupFrame);

        // XXX Deal with absolute and fixed frames...
        childFrames.AddChild(headerFooterFrame);
      }

      NS_RELEASE(rowGroupStyle);
      
      // Header and footer must be first, and then the body row groups.
      // So if we found a body row group, then stop looking for header and
      // footer elements
      if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == display->mDisplay) {
        break;
      }

      // Get the next row group frame
      rowGroupFrame->GetNextSibling(&rowGroupFrame);
    }
    
    // Set the table frame's initial child list
    newFrame->SetInitialChildList(*aPresContext, nsnull, childFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

NS_IMETHODIMP
nsCSSFrameConstructor::CreateContinuingFrame(nsIPresContext* aPresContext,
                                             nsIFrame*       aFrame,
                                             nsIFrame*       aParentFrame,
                                             nsIFrame**      aContinuingFrame)
{
  nsIAtom*          frameType;
  nsIContent*       content;
  nsIStyleContext*  styleContext;
  nsIFrame*         newFrame = nsnull;
  nsresult          rv;

  // Use the frame type to determine what type of frame to create
  aFrame->GetFrameType(&frameType);
  aFrame->GetContent(&content);
  aFrame->GetStyleContext(&styleContext);

  if (nsLayoutAtoms::textFrame == frameType) {
    rv = NS_NewTextFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
    
  } else if (nsHTMLAtoms::inlineFrame == frameType) {
    rv = NS_NewInlineFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsHTMLAtoms::blockFrame == frameType) {
    rv = NS_NewBlockFrame(newFrame, 0);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::areaFrame == frameType) {
    rv = NS_NewAreaFrame(newFrame, 0);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::pageFrame == frameType) {
    rv = NS_NewPageFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_TRUE);
    }

  } else if (nsLayoutAtoms::tableOuterFrame == frameType) {
    rv = CreateContinuingOuterTableFrame(aPresContext, aFrame, aParentFrame,
                                         content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableFrame == frameType) {
    rv = CreateContinuingTableFrame(aPresContext, aFrame, aParentFrame,
                                    content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
    rv = NS_NewTableRowGroupFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }

  } else if (nsLayoutAtoms::tableRowFrame == frameType) {
    rv = NS_NewTableRowFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);

      // Create a continuing frame for each table cell frame
      nsIFrame*     cellFrame;
      nsFrameItems  newChildList;

      aFrame->FirstChild(nsnull, &cellFrame);
      while (cellFrame) {
        nsIAtom*  tableType;
        
        // See if it's a table cell frame
        cellFrame->GetFrameType(&tableType);
        if (nsLayoutAtoms::tableCellFrame == tableType) {
          nsIFrame* continuingCellFrame;

          CreateContinuingFrame(aPresContext, cellFrame, newFrame, &continuingCellFrame);
          newChildList.AddChild(continuingCellFrame);
        }

        NS_IF_RELEASE(tableType);
        cellFrame->GetNextSibling(&cellFrame);
      }
      
      // Set the table cell's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, newChildList.childList);
    }

  } else if (nsLayoutAtoms::tableCellFrame == frameType) {
    rv = NS_NewTableCellFrame(newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(*aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(*aPresContext, newFrame,
                                               styleContext, PR_FALSE);

      // Create a continuing area frame
      nsIFrame* areaFrame;
      nsIFrame* continuingAreaFrame;
      aFrame->FirstChild(nsnull, &areaFrame);
      CreateContinuingFrame(aPresContext, areaFrame, newFrame, &continuingAreaFrame);

      // Set the table cell's initial child list
      newFrame->SetInitialChildList(*aPresContext, nsnull, continuingAreaFrame);
    }

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected frame type");
    rv = NS_ERROR_UNEXPECTED;
  }

  *aContinuingFrame = newFrame;
  NS_RELEASE(styleContext);
  NS_IF_RELEASE(content);
  NS_IF_RELEASE(frameType);
  return rv;
}

nsresult
nsCSSFrameConstructor::RecreateFramesForContent(nsIPresContext* aPresContext,
                                                nsIContent* aContent)                                   
{
  nsresult rv = NS_OK;

  nsIContent *container;
  rv = aContent->GetParent(container);
  if (container) {
    PRInt32 indexInContainer;
    rv = container->IndexOf(aContent, indexInContainer);
    if (NS_OK == rv) {
      // First, remove the frames associated with the content object on which the
      // attribute change occurred.
      rv = ContentRemoved(aPresContext, container, aContent, indexInContainer);
  
      if (NS_OK == rv) {
        // Now, recreate the frames associated with this content object.
        rv = ContentInserted(aPresContext, container, aContent, indexInContainer);
      }
    }
    NS_RELEASE(container);    
  }

  return rv;
}

