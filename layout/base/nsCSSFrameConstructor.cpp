/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleFrameConstruction.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
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
#include "nsIRadioControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsITextContent.h"
#include "nsPlaceholderFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsStyleChangeList.h"
#include "nsIFormControl.h"
#include "nsCSSAtoms.h"
#include "nsIDeviceContext.h"
#include "nsTextFragment.h"
#include "nsISupportsArray.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIFrameManager.h"
#include "nsIAttributeContent.h"
#include "nsIPref.h"
#include "nsLegendFrame.h"
#include "nsTitleFrame.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#ifdef INCLUDE_XUL
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#endif


#include "nsInlineFrame.h"
#include "nsBlockFrame.h"

#include "nsGfxTextControlFrame.h"
#include "nsIScrollableFrame.h"

#include "nsIServiceManager.h"
#include "nsIXBLService.h"

#undef NOISY_FIRST_LETTER

#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#endif

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsTreeFrame.h"
#include "nsTreeOuterFrame.h"
#include "nsTreeRowGroupFrame.h"
#include "nsTreeRowFrame.h"
#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h"
#include "nsTreeIndentationFrame.h"
#include "nsTreeCellFrame.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsToolbarItemFrame.h"
#include "nsXULCheckboxFrame.h"
#include "nsIScrollable.h"

#ifdef DEBUG
static PRBool gNoisyContentUpdates = PR_FALSE;
static PRBool gReallyNoisyContentUpdates = PR_FALSE;
static PRBool gNoisyInlineConstruction = PR_FALSE;
#endif

//#define NEWGFX_LIST
//#define NEWGFX_DROPDOWN
#define NEWGFX_LIST_SCROLLFRAME

#if defined(NEWGFX_LIST) || defined(NEWGFX_DROPDOWN) 
nsresult
NS_NewGfxListControlFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
#endif

nsresult
NS_NewThumbFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollPortFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewGfxScrollFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIDocument* aDocument );

nsresult
NS_NewTabFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewDeckFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewSpringFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewStackFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewProgressMeterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitledButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewXULTextFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitledBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitledBoxInnerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitleFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot);

nsresult
NS_NewXULButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewSliderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewSpinnerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewColorPickerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewFontPickerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewGrippyFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewSplitterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewMenuPopupFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewMenuFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags );

nsresult
NS_NewMenuBarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

#endif

//static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID, NS_ICOMBOBOXCONTROLFRAME_IID);
static NS_DEFINE_IID(kIRadioControlFrameIID,    NS_IRADIOCONTROLFRAME_IID);
static NS_DEFINE_IID(kIListControlFrameIID,     NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID, NS_IDOMCHARACTERDATA_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

// -----------------------------------------------------------

// Structure used when constructing formatting object trees.
struct nsFrameItems {
  nsIFrame* childList;
  nsIFrame* lastChild;
  
  nsFrameItems(nsIFrame* aFrame = nsnull);

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

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
  nsIFrame* containingBlock;

  nsAbsoluteItems(nsIFrame* aContainingBlock = nsnull);

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

nsAbsoluteItems::nsAbsoluteItems(nsIFrame* aContainingBlock)
  : containingBlock(aContainingBlock)
{
}

// Additional behavior is that it sets the frame's NS_FRAME_OUT_OF_FLOW flag
void
nsAbsoluteItems::AddChild(nsIFrame* aChild)
{
  nsFrameState  frameState;

  // Mark the frame as being moved out of the flow
  aChild->GetFrameState(&frameState);
  aChild->SetFrameState(frameState | NS_FRAME_OUT_OF_FLOW);
  nsFrameItems::AddChild(aChild);
}

// -----------------------------------------------------------

// Structure for saving the existing state when pushing/poping containing
// blocks. The destructor restores the state to its previous state
class nsFrameConstructorSaveState {
public:
  nsFrameConstructorSaveState();
  ~nsFrameConstructorSaveState();

private:
  nsAbsoluteItems* mItems;                // pointer to struct whose data we save/restore
  PRBool*          mFirstLetterStyle;
  PRBool*          mFirstLineStyle;

  nsAbsoluteItems  mSavedItems;           // copy of original data
  PRBool           mSavedFirstLetterStyle;
  PRBool           mSavedFirstLineStyle;

  friend class nsFrameConstructorState;
};

// Structure used for maintaining state information during the
// frame construction process
class nsFrameConstructorState {
public:
  nsCOMPtr<nsIPresShell>    mPresShell;
  nsCOMPtr<nsIFrameManager> mFrameManager;

  // Containing block information for out-of-flow frammes
  nsAbsoluteItems           mFixedItems;
  nsAbsoluteItems           mAbsoluteItems;
  nsAbsoluteItems           mFloatedItems;
  PRBool                    mFirstLetterStyle;
  PRBool                    mFirstLineStyle;
  nsCOMPtr<nsILayoutHistoryState> mFrameState;

  // Constructor
  nsFrameConstructorState(nsIPresContext*        aPresContext,
                          nsIFrame*              aFixedContainingBlock,
                          nsIFrame*              aAbsoluteContainingBlock,
                          nsIFrame*              aFloaterContainingBlock,
                          nsILayoutHistoryState* aFrameState);

  // Function to push the existing absolute containing block state and
  // create a new scope
  void PushAbsoluteContainingBlock(nsIFrame* aNewAbsoluteContainingBlock,
                                   nsFrameConstructorSaveState& aSaveState);

  // Function to push the existing floater containing block state and
  // create a new scope
  void PushFloaterContainingBlock(nsIFrame* aNewFloaterContainingBlock,
                                  nsFrameConstructorSaveState& aSaveState,
                                  PRBool aFirstLetterStyle,
                                  PRBool aFirstLineStyle);
};

nsFrameConstructorState::nsFrameConstructorState(nsIPresContext*        aPresContext,
                                                 nsIFrame*              aFixedContainingBlock,
                                                 nsIFrame*              aAbsoluteContainingBlock,
                                                 nsIFrame*              aFloaterContainingBlock,
                                                 nsILayoutHistoryState* aFrameState)
  : mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloaterContainingBlock),
    mFirstLetterStyle(PR_FALSE),
    mFirstLineStyle(PR_FALSE),
    mFrameState(aFrameState)
{
  aPresContext->GetShell(getter_AddRefs(mPresShell));
  mPresShell->GetFrameManager(getter_AddRefs(mFrameManager));
}

void
nsFrameConstructorState::PushAbsoluteContainingBlock(nsIFrame* aNewAbsoluteContainingBlock,
                                                     nsFrameConstructorSaveState& aSaveState)
{
  aSaveState.mItems = &mAbsoluteItems;
  aSaveState.mSavedItems = mAbsoluteItems;
  mAbsoluteItems = nsAbsoluteItems(aNewAbsoluteContainingBlock);
}

void
nsFrameConstructorState::PushFloaterContainingBlock(nsIFrame* aNewFloaterContainingBlock,
                                                    nsFrameConstructorSaveState& aSaveState,
                                                    PRBool aFirstLetterStyle,
                                                    PRBool aFirstLineStyle)
{
  aSaveState.mItems = &mFloatedItems;
  aSaveState.mFirstLetterStyle = &mFirstLetterStyle;
  aSaveState.mFirstLineStyle = &mFirstLineStyle;
  aSaveState.mSavedItems = mFloatedItems;
  aSaveState.mSavedFirstLetterStyle = mFirstLetterStyle;
  aSaveState.mSavedFirstLineStyle = mFirstLineStyle;
  mFloatedItems = nsAbsoluteItems(aNewFloaterContainingBlock);
  mFirstLetterStyle = aFirstLetterStyle;
  mFirstLineStyle = aFirstLineStyle;
}

nsFrameConstructorSaveState::nsFrameConstructorSaveState()
  : mItems(nsnull), mFirstLetterStyle(nsnull), mFirstLineStyle(nsnull), 
    mSavedFirstLetterStyle(PR_FALSE), mSavedFirstLineStyle(PR_FALSE)
{
}

nsFrameConstructorSaveState::~nsFrameConstructorSaveState()
{
  // Restore the state
  if (mItems) {
    *mItems = mSavedItems;
  }
  if (mFirstLetterStyle) {
    *mFirstLetterStyle = mSavedFirstLetterStyle;
  }
  if (mFirstLineStyle) {
    *mFirstLineStyle = mSavedFirstLineStyle;
  }
}

// -----------------------------------------------------------

// Structure used when creating table frames.
struct nsTableCreator {
  virtual nsresult CreateTableOuterFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableRowGroupFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableColFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableColGroupFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableRowFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellInnerFrame(nsIFrame** aNewFrame);

  virtual PRBool IsTreeCreator() { return PR_FALSE; };

  nsTableCreator(nsIPresShell* aPresShell)
  {
    mPresShell = aPresShell;
  }

  virtual ~nsTableCreator() {};

  nsCOMPtr<nsIPresShell> mPresShell;
};

nsresult
nsTableCreator::CreateTableOuterFrame(nsIFrame** aNewFrame) {
  return NS_NewTableOuterFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableFrame(nsIFrame** aNewFrame) {
  return NS_NewTableFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableRowGroupFrame(nsIFrame** aNewFrame) {
  return NS_NewTableRowGroupFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableColFrame(nsIFrame** aNewFrame) {
  return NS_NewTableColFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableColGroupFrame(nsIFrame** aNewFrame) {
  return NS_NewTableColGroupFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableRowFrame(nsIFrame** aNewFrame) {
  return NS_NewTableRowFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableCellFrame(nsIFrame** aNewFrame) {
  return NS_NewTableCellFrame(mPresShell, aNewFrame);
}

nsresult
nsTableCreator::CreateTableCellInnerFrame(nsIFrame** aNewFrame) {
  return NS_NewTableCellInnerFrame(mPresShell, aNewFrame);
}

#ifdef INCLUDE_XUL

// Structure used when creating tree frames
struct nsTreeCreator: public nsTableCreator {
  nsresult CreateTableOuterFrame(nsIFrame** aNewFrame);
  nsresult CreateTableFrame(nsIFrame** aNewFrame);
  nsresult CreateTableCellFrame(nsIFrame** aNewFrame);
  nsresult CreateTableRowGroupFrame(nsIFrame** aNewFrame);
  nsresult CreateTableRowFrame(nsIFrame** aNewFrame);
  nsresult CreateTableCellInnerFrame(nsIFrame** aNewFrame);

  PRBool IsTreeCreator() { return PR_TRUE; };

  nsTreeCreator(nsIPresShell* aPresShell)
    :nsTableCreator(aPresShell) {};
};

nsresult
nsTreeCreator::CreateTableOuterFrame(nsIFrame** aNewFrame)
{
  return NS_NewTreeOuterFrame(mPresShell, aNewFrame);
}

nsresult
nsTreeCreator::CreateTableFrame(nsIFrame** aNewFrame)
{
  return NS_NewTreeFrame(mPresShell, aNewFrame);
}

nsresult
nsTreeCreator::CreateTableCellFrame(nsIFrame** aNewFrame)
{
  return NS_NewTreeCellFrame(mPresShell, aNewFrame);
}

nsresult
nsTreeCreator::CreateTableRowGroupFrame(nsIFrame** aNewFrame)
{
  return NS_NewTreeRowGroupFrame(mPresShell, aNewFrame);
}

nsresult
nsTreeCreator::CreateTableRowFrame(nsIFrame** aNewFrame) {
  return NS_NewTreeRowFrame(mPresShell, aNewFrame);
}

nsresult
nsTreeCreator::CreateTableCellInnerFrame(nsIFrame** aNewFrame) {
  return NS_NewBoxFrame(mPresShell, aNewFrame);
  //return NS_NewTableCellInnerFrame(mPresShell, aNewFrame);
}

#endif // INCLUDE_XUL

//MathML Mod - RBS
#ifdef MOZ_MATHML

// Structure used when creating MathML mtable frames
struct nsMathMLmtableCreator: public nsTableCreator {
  nsresult CreateTableCellInnerFrame(nsIFrame** aNewFrame);

  nsMathMLmtableCreator(nsIPresShell* aPresShell)
    :nsTableCreator(aPresShell) {};
};

nsresult
nsMathMLmtableCreator::CreateTableCellInnerFrame(nsIFrame** aNewFrame)
{
  // only works if aNewFrame is an AreaFrame (to take care of the lineLayout logic)
  return NS_NewMathMLmtdFrame(mPresShell, aNewFrame);
}
#endif // MOZ_MATHML

// -----------------------------------------------------------

// return the child list that aFrame belongs on. does not ADDREF
nsIAtom* GetChildListFor(const nsIFrame* aFrame)
{
  nsIAtom* childList = nsnull;
  nsCOMPtr<nsIAtom> frameType;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  if (nsLayoutAtoms::tableCaptionFrame == frameType.get()) { 
    childList = nsLayoutAtoms::captionList;
  }
  return childList;
}

nsCSSFrameConstructor::nsCSSFrameConstructor(void)
  : nsIStyleFrameConstruction(),
    mDocument(nsnull),
    mInitialContainingBlock(nsnull),
    mFixedContainingBlock(nsnull),
    mDocElementContainingBlock(nsnull),
    mGfxScrollFrame(nsnull)
{
  NS_INIT_REFCNT();
}

nsCSSFrameConstructor::~nsCSSFrameConstructor(void)
{
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

// Helper function that determines the child list name that aChildFrame
// is contained in
static void
GetChildListNameFor(nsIPresContext* aPresContext,
                    nsIFrame*       aParentFrame,
                    nsIFrame*       aChildFrame,
                    nsIAtom**       aListName)
{
  nsFrameState  frameState;
  nsIAtom*      listName;
  
  // See if the frame is moved out of the flow
  aChildFrame->GetFrameState(&frameState);
  if (frameState & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStylePosition* position;
    aChildFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
      listName = nsLayoutAtoms::absoluteList;
    } else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
      listName = nsLayoutAtoms::fixedList;
    } else {
#ifdef NS_DEBUG
      const nsStyleDisplay* display;
      aChildFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      NS_ASSERTION(display->IsFloating(), "not a floated frame");
#endif
      listName = nsLayoutAtoms::floaterList;
    }

  } else {
    listName = nsnull;
  }

  // Verify that the frame is actually in that child list
#ifdef NS_DEBUG
  nsIFrame* firstChild;
  aParentFrame->FirstChild(aPresContext, listName, &firstChild);

  nsFrameList frameList(firstChild);
  NS_ASSERTION(frameList.ContainsFrame(aChildFrame), "not in child list");
#endif

  NS_IF_ADDREF(listName);
  *aListName = listName;
}

nsresult
nsCSSFrameConstructor::CreateGeneratedFrameFor(nsIPresContext*       aPresContext,
                                               nsIDocument*          aDocument,
                                               nsIFrame*             aParentFrame,
                                               nsIContent*           aContent,
                                               nsIStyleContext*      aStyleContext,
                                               const nsStyleContent* aStyleContent,
                                               PRUint32              aContentIndex,
                                               nsIFrame**            aFrame)
{
  *aFrame = nsnull;  // initialize OUT parameter

  // Get the content value
  nsStyleContentType  type;
  nsAutoString        contentString;
  aStyleContent->GetContentAt(aContentIndex, type, contentString);

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  if (eStyleContentType_URL == type) {
    nsIHTMLContent* imageContent;
    
    // Create an HTML image content object, and set the SRC.
    // XXX Check if it's an image type we can handle...
    NS_NewHTMLImageElement(&imageContent, nsHTMLAtoms::img);
    imageContent->SetHTMLAttribute(nsHTMLAtoms::src, contentString, PR_FALSE);

    // Set aContent as the parent content and set the document object. This
    // way event handling works
    imageContent->SetParent(aContent);
    imageContent->SetDocument(aDocument, PR_TRUE);
  
    // Create an image frame and initialize it
    nsIFrame* imageFrame;
    NS_NewImageFrame(shell, &imageFrame);
    imageFrame->Init(aPresContext, imageContent, aParentFrame, aStyleContext, nsnull);
    NS_RELEASE(imageContent);
  
    // Return the image frame
    *aFrame = imageFrame;

  } else {

    switch (type) {
    case eStyleContentType_String:
      break;
  
    case eStyleContentType_Attr:
      {  
        nsIAtom* attrName = nsnull;
        PRInt32 attrNameSpace = kNameSpaceID_None;
        PRInt32 barIndex = contentString.FindChar('|'); // CSS namespace delimiter
        if (-1 != barIndex) {
          nsAutoString  nameSpaceVal;
          contentString.Left(nameSpaceVal, barIndex);
          PRInt32 error;
          attrNameSpace = nameSpaceVal.ToInteger(&error, 10);
          contentString.Cut(0, barIndex + 1);
          if (contentString.Length()) {
            attrName = NS_NewAtom(contentString);
          }
        }
        else {
          attrName = NS_NewAtom(contentString);
        }

        // Creates the content and frame and return if successful
        nsresult rv = NS_ERROR_FAILURE;
        if (nsnull != attrName) {
          nsCOMPtr<nsIContent> content;
          nsIFrame*   textFrame = nsnull;
          NS_NewAttributeContent(getter_AddRefs(content));
          if (nsnull != content) {
            nsCOMPtr<nsIAttributeContent> attrContent(do_QueryInterface(content));
            if (attrContent) {
              attrContent->Init(aContent, attrNameSpace, attrName);  
            }

            // Set aContent as the parent content and set the document object. This
            // way event handling works
            content->SetParent(aContent);
            content->SetDocument(aDocument, PR_TRUE);

            // Create a text frame and initialize it
            NS_NewTextFrame(shell, &textFrame);
            textFrame->Init(aPresContext, content, aParentFrame, aStyleContext, nsnull);

            // Return the text frame
            *aFrame = textFrame;
            rv = NS_OK;
          }
          NS_RELEASE(attrName);
        }
        return rv;
      }
      break;
  
    case eStyleContentType_Counter:
    case eStyleContentType_Counters:
    case eStyleContentType_URL:
      return NS_ERROR_NOT_IMPLEMENTED;  // XXX not supported yet...
  
    case eStyleContentType_OpenQuote:
    case eStyleContentType_CloseQuote:
      {
        PRUint32  quotesCount = aStyleContent->QuotesCount();
        if (quotesCount > 0) {
          nsAutoString  openQuote, closeQuote;
  
          // If the depth is greater than the number of pairs, the last pair
          // is repeated
          PRUint32  quoteDepth = 0;  // XXX really track the nested quotes...
          if (quoteDepth > quotesCount) {
            quoteDepth = quotesCount - 1;
          }
          aStyleContent->GetQuotesAt(quoteDepth, openQuote, closeQuote);
          if (eStyleContentType_OpenQuote == type) {
            contentString = openQuote;
          } else {
            contentString = closeQuote;
          }
  
        } else {
          contentString = '\"';
        }
      }
      break;
  
    case eStyleContentType_NoOpenQuote:
    case eStyleContentType_NoCloseQuote:
      // XXX Adjust quote depth...
      return NS_OK;
    } // switch
  

    // Create a text content node
    nsIContent* textContent = nsnull;
    nsIDOMCharacterData*  domData;
    nsIFrame*             textFrame = nsnull;
    
    NS_NewTextNode(&textContent);
    if (textContent) {
      // Set the text
      textContent->QueryInterface(kIDOMCharacterDataIID, (void**)&domData);
      domData->SetData(contentString);
      NS_RELEASE(domData);
  
      // Set aContent as the parent content and set the document object. This
      // way event handling works
      textContent->SetParent(aContent);
      textContent->SetDocument(aDocument, PR_TRUE);
      
      // Create a text frame and initialize it
      NS_NewTextFrame(shell, &textFrame);
      textFrame->Init(aPresContext, textContent, aParentFrame, aStyleContext, nsnull);
  
      NS_RELEASE(textContent);
    }
  
    // Return the text frame
    *aFrame = textFrame;
  }

  return NS_OK;
}

PRBool
nsCSSFrameConstructor::CreateGeneratedContentFrame(nsIPresShell*        aPresShell, 
                                                   nsIPresContext*  aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIFrame*        aFrame,
                                                   nsIContent*      aContent,
                                                   nsIStyleContext* aStyleContext,
                                                   nsIAtom*         aPseudoElement,
                                                   PRBool           aForBlock,
                                                   nsIFrame**       aResult)
{
  *aResult = nsnull; // initialize OUT parameter

  // Probe for the existence of the pseudo-element
  nsCOMPtr<nsIStyleContext> pseudoStyleContext;
  aPresContext->ProbePseudoStyleContextFor(aContent, aPseudoElement, aStyleContext,
                                           PR_FALSE, getter_AddRefs(pseudoStyleContext));

  if (pseudoStyleContext) {
    const nsStyleDisplay* display;

    // See whether the generated content should be displayed.
    display = (const nsStyleDisplay*)pseudoStyleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
      aState.mFrameManager->SetUndisplayedPseudoIn(pseudoStyleContext, aContent);
    }
    else {  // has valid display type
      // See if there was any content specified
      const nsStyleContent* styleContent =
        (const nsStyleContent*)pseudoStyleContext->GetStyleData(eStyleStruct_Content);
      PRUint32  contentCount = styleContent->ContentCount();

      if (contentCount > 0) {
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

        // Also make sure the 'position' property is 'static'. :before and :after
        // pseudo-elements can not be floated or positioned
        const nsStylePosition * stylePosition =
          (const nsStylePosition*)pseudoStyleContext->GetStyleData(eStyleStruct_Position);
        if (NS_STYLE_POSITION_NORMAL != stylePosition->mPosition) {
          // Reset the value
          nsStylePosition* mutablePosition = (nsStylePosition*)
            pseudoStyleContext->GetMutableStyleData(eStyleStruct_Position);
  
          mutablePosition->mPosition = NS_STYLE_POSITION_NORMAL;
        }

        // Create a block box or an inline box depending on the value of
        // the 'display' property
        nsIFrame*     containerFrame;
        nsFrameItems  childFrames;

        nsCOMPtr<nsIDOMElement>containerElement;
        nsCOMPtr<nsIContent>   containerContent;
        nsCOMPtr<nsIDocument>  document;

        aContent->GetDocument(*getter_AddRefs(document));

        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(document));
        nsresult  result;
        result = domdoc->CreateElement("SPAN",getter_AddRefs(containerElement));//is the literal the correct way?
        if (NS_SUCCEEDED(result) && containerElement)
        {
          containerContent = do_QueryInterface(containerElement);
        }//do NOT use YET set this up for later checkin. this will be the content of the new frame.

        if (NS_STYLE_DISPLAY_BLOCK == displayValue) {
          NS_NewBlockFrame(aPresShell, &containerFrame);
        } else {
          NS_NewInlineFrame(aPresShell, &containerFrame);
        }        
        InitAndRestoreFrame(aPresContext, aState, aContent, 
                            aFrame, pseudoStyleContext, nsnull, containerFrame);

        // Mark the frame as being associated with generated content
        nsFrameState  frameState;
        containerFrame->GetFrameState(&frameState);
        frameState |= NS_FRAME_GENERATED_CONTENT;
        containerFrame->SetFrameState(frameState);

        // Create another pseudo style context to use for all the generated child
        // frames
        nsIStyleContext*  textStyleContext;
        aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::textPseudo,
                                                   pseudoStyleContext, PR_FALSE,
                                                   &textStyleContext);

        // Now create content objects (and child frames) for each value of the
        // 'content' property

        for (PRUint32 contentIndex = 0; contentIndex < contentCount; contentIndex++) {
          nsIFrame* frame;
          nsresult  result;

          // Create a frame
          result = CreateGeneratedFrameFor(aPresContext, document, containerFrame,
                                           aContent, textStyleContext,
                                           styleContent, contentIndex, &frame);
          if (NS_SUCCEEDED(result) && frame) {
            // Add it to the list of child frames
            childFrames.AddChild(frame);
          }
        }
  
        NS_RELEASE(textStyleContext);
        if (childFrames.childList) {
          containerFrame->SetInitialChildList(aPresContext, nsnull, childFrames.childList);
        }
        *aResult = containerFrame;
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

#if 0
static PRBool
IsEmptyTextContent(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
  nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
  if (tc) {
    tc->IsOnlyWhitespace(&result);
  }
  return result;
}

#include "nsIDOMHTMLParagraphElement.h"

static PRBool
IsHTMLParagraph(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMHTMLParagraphElement> p(do_QueryInterface(aContent));
  PRBool status = PR_FALSE;
  if (p) {
    status = PR_TRUE;
  }
  return status;
}

static PRBool
IsEmptyContainer(nsIContent* aContainer)
{
  // Check each kid and if each kid is empty text content (or there
  // are no kids) then return true.
  PRInt32 i, numKids;
  aContainer->ChildCount(numKids);
  for (i = 0; i < numKids; i++) {
    nsCOMPtr<nsIContent> kid;
    aContainer->ChildAt(i, *getter_AddRefs(kid));
    if (kid.get()) {
      if (!IsEmptyTextContent(kid)) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

static PRBool
IsEmptyHTMLParagraph(nsIContent* aContent)
{
  if (!IsHTMLParagraph(aContent)) {
    // It's not an HTML paragraph so return false
    return PR_FALSE;
  }
  return IsEmptyContainer(aContent);
}
#endif

nsresult
nsCSSFrameConstructor::CreateInputFrame(nsIPresShell    *aPresShell,
                                        nsIPresContext  *aPresContext,
                                        nsIContent      *aContent, 
                                        nsIFrame        *&aFrame,
                                        nsIStyleContext *aStyleContext)
{
  nsresult  rv;

  // Figure out which type of input frame to create
  nsAutoString  val;
  if (NS_OK == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, val)) {
    if (val.EqualsIgnoreCase("submit")) {
      rv = ConstructButtonControlFrame(aPresShell, aPresContext, aFrame);
    }
    else if (val.EqualsIgnoreCase("reset")) {
      rv = ConstructButtonControlFrame(aPresShell, aPresContext, aFrame);
    }
    else if (val.EqualsIgnoreCase("button")) {
      rv = ConstructButtonControlFrame(aPresShell, aPresContext, aFrame);
    }
    else if (val.EqualsIgnoreCase("checkbox")) {
      rv = ConstructCheckboxControlFrame(aPresShell, aPresContext, aFrame);
    }
    else if (val.EqualsIgnoreCase("file")) {
      rv = NS_NewFileControlFrame(aPresShell, &aFrame);
    }
    else if (val.EqualsIgnoreCase("hidden")) {
      rv = ConstructButtonControlFrame(aPresShell, aPresContext, aFrame);
    }
    else if (val.EqualsIgnoreCase("image")) {
      rv = NS_NewImageControlFrame(aPresShell, &aFrame);
    }
    else if (val.EqualsIgnoreCase("password")) {
      rv = ConstructTextControlFrame(aPresShell, aPresContext, aFrame, aContent);
    }
    else if (val.EqualsIgnoreCase("radio")) {
      rv = ConstructRadioControlFrame(aPresShell, aPresContext, aFrame, aContent, aStyleContext);
    }
    else if (val.EqualsIgnoreCase("text")) {
      rv = ConstructTextControlFrame(aPresShell, aPresContext, aFrame, aContent);
    }
    else {
      rv = ConstructTextControlFrame(aPresShell, aPresContext, aFrame, aContent);
    }
  } else {
    rv = ConstructTextControlFrame(aPresShell, aPresContext, aFrame, aContent);
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
nsCSSFrameConstructor::ConstructTableFrame(nsIPresShell*            aPresShell,
                                           nsIPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsIStyleContext*         aStyleContext,
                                           nsIFrame*&               aNewFrame,
                                           nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_OK;
  nsIFrame* childList;
  nsIFrame* innerFrame;
  nsIFrame* innerChildList = nsnull;
  nsIFrame* captionFrame   = nsnull;

  // Create an anonymous table outer frame which holds the caption and table frame
  aTableCreator.CreateTableOuterFrame(&aNewFrame);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned  
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);  
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame,
                                           aStyleContext, PR_FALSE);
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
#if 0
  nsCOMPtr<nsIStyleContext> outerStyleContext;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableOuterPseudo,
                                             parentStyleContext,
                                             getter_AddRefs(outerStyleContext));
  aNewFrame->Init(aPresContext, aContent, aParentFrame, outerStyleContext);
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame,
                                           outerStyleContext, PR_FALSE);
#endif
  // Create the inner table frame
  aTableCreator.CreateTableFrame(&innerFrame);
  childList = innerFrame;

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aNewFrame, aStyleContext, nsnull, innerFrame);

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
          rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, childContent, aNewFrame,
                                          childStyleContext, ignore1, captionFrame, aTableCreator);
        }
        break;

      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      {
        PRBool isRowGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != styleDisplay->mDisplay);
        rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, childContent, innerFrame,
                                      childStyleContext, isRowGroup, childFrame, ignore1,
                                      aTableCreator);
        break;
      }
      case NS_STYLE_DISPLAY_TABLE_ROW:
        rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, childContent, innerFrame,
                                    childStyleContext, childFrame, ignore1, aTableCreator);
        break;

      case NS_STYLE_DISPLAY_TABLE_COLUMN:
        rv = ConstructTableColFrame(aPresShell, aPresContext, aState, childContent, innerFrame,
                                    childStyleContext, childFrame, ignore1, aTableCreator);
        break;


      case NS_STYLE_DISPLAY_TABLE_CELL:
        rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, childContent, innerFrame,
                                     childStyleContext, childFrame, ignore1, ignore2, aTableCreator);
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
        TableProcessChild(aPresShell, aPresContext, aState, childContent, innerFrame, parentStyleContext,
                          childItems, aTableCreator);
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

  // Set the inner table frame's initial primary list 
  innerFrame->SetInitialChildList(aPresContext, nsnull, innerChildList);

  // Set the outer table frame's primary and option lists
  aNewFrame->SetInitialChildList(aPresContext, nsnull, childList);
  if (captionFrame) {
    aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::captionList, captionFrame);
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructAnonymousTableFrame(nsIPresShell*        aPresShell, 
                                                    nsIPresContext*          aPresContext, 
                                                    nsFrameConstructorState& aState,
                                                    nsIContent*              aContent, 
                                                    nsIFrame*                aParentFrame, 
                                                    nsIFrame*&               aNewTopFrame,
                                                    nsIFrame*&               aOuterFrame, 
                                                    nsIFrame*&               aInnerFrame,
                                                    nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_OK;
  //NS_WARNING("an anonymous table frame was created. \n");
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
    rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, cellStyleContext,
                                 aNewTopFrame, cellFrame, cellBodyFrame, aTableCreator, PR_FALSE); 
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
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, rowStyleContext, 
                                  aNewTopFrame, rowFrame, aTableCreator);
      if (NS_FAILED(rv)) return rv;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo, parentStyleContext,
                                                 PR_FALSE, getter_AddRefs(cellStyleContext));
      rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, rowFrame, cellStyleContext, 
                                   ignore, cellFrame, cellBodyFrame, aTableCreator, PR_FALSE);
      rowFrame->SetInitialChildList(aPresContext, nsnull, cellFrame);
      break;
    }
  case NS_STYLE_DISPLAY_TABLE: // construct a row group, a row, & a cell
    {
      nsIFrame* groupFrame;
      nsCOMPtr<nsIStyleContext> groupStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowGroupPseudo, 
                                                 parentStyleContext, PR_FALSE, 
                                                 getter_AddRefs(groupStyleContext));
      rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, groupStyleContext, 
                                    PR_TRUE, aNewTopFrame, groupFrame, aTableCreator);
      if (NS_FAILED(rv)) return rv;
      nsIFrame* rowFrame;
      nsCOMPtr<nsIStyleContext> rowStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo, 
                                                 parentStyleContext, PR_FALSE, 
                                                 getter_AddRefs(rowStyleContext));
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, rowStyleContext, 
                                  ignore, rowFrame, aTableCreator);
      if (NS_FAILED(rv)) return rv;
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableCellPseudo, parentStyleContext,
                                               PR_FALSE, getter_AddRefs(cellStyleContext));
      rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, rowFrame, cellStyleContext, 
                                   ignore, cellFrame, cellBodyFrame, aTableCreator, PR_FALSE);
      rowFrame->SetInitialChildList(aPresContext, nsnull, cellFrame);
      groupFrame->SetInitialChildList(aPresContext, nsnull, rowFrame);
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
    rv = aTableCreator.CreateTableOuterFrame(&aOuterFrame);
    if (NS_FAILED(rv)) return rv;
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        tableParent, outerStyleContext, nsnull, aOuterFrame);  

    // create the inner table frame
    nsCOMPtr<nsIStyleContext> innerStyleContext;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tablePseudo, 
                                               outerStyleContext, PR_FALSE,
                                               getter_AddRefs(innerStyleContext));
    rv = aTableCreator.CreateTableFrame(&aInnerFrame);
    if (NS_FAILED(rv)) return rv;    
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aOuterFrame, innerStyleContext, nsnull, aInnerFrame);

    //XXX when bug 2479 is fixed this may be a duplicate call
    aOuterFrame->SetInitialChildList(aPresContext, nsnull, aInnerFrame);

    if (cellIsParent) {
      cellBodyFrame->SetInitialChildList(aPresContext, nsnull, aOuterFrame);
    } else {
      aNewTopFrame = aOuterFrame;
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCaptionFrame(nsIPresShell*            aPresShell,
                                                  nsIPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrame,
                                                  nsIStyleContext*         aStyleContext,
                                                  nsIFrame*&               aNewTopFrame,
                                                  nsIFrame*&               aNewCaptionFrame,
                                                  nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_NewTableCaptionFrame(aPresShell, &aNewCaptionFrame);
  if (NS_FAILED(rv)) return rv;

  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);

  nsIFrame* outerFrame = aParentFrame;
  if (NS_STYLE_DISPLAY_TABLE == parentDisplay->mDisplay) { // parent is an outer table
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aParentFrame, aStyleContext, nsnull, aNewCaptionFrame);

    // the caller is responsible for calling SetInitialChildList 
    aNewTopFrame = aNewCaptionFrame;
  } else { // parent is not a table, need to create a new table
    //NS_WARNING("a non table contains a table caption child. \n");
    nsIFrame* innerFrame;
    ConstructAnonymousTableFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                 aNewTopFrame, outerFrame, innerFrame, aTableCreator);
    nsCOMPtr<nsIStyleContext> outerStyleContext;
    outerFrame->GetStyleContext(getter_AddRefs(outerStyleContext));
    nsCOMPtr<nsIStyleContext> adjStyleContext;
    aPresContext->ResolveStyleContextFor(aContent, outerStyleContext,
                                         PR_FALSE, getter_AddRefs(adjStyleContext));
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        outerFrame, adjStyleContext, nsnull, aNewCaptionFrame);
    aState.mFrameManager->SetPrimaryFrameFor(aContent, aNewCaptionFrame);
  }

  // The caption frame is a floater container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floaterSaveState;
  aState.PushFloaterContainingBlock(aNewCaptionFrame, floaterSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);

  // Process the child content
  nsFrameItems childItems;
  ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewCaptionFrame,
                  PR_TRUE, childItems, PR_TRUE);
  aNewCaptionFrame->SetInitialChildList(aPresContext, nsnull,
                                        childItems.childList);
  if (aState.mFloatedItems.childList) {
    aNewCaptionFrame->SetInitialChildList(aPresContext,
                                          nsLayoutAtoms::floaterList,
                                          aState.mFloatedItems.childList);
  }
  return rv;
}

// if aParentFrame is a table, it is assummed that it is an inner table 
nsresult
nsCSSFrameConstructor::ConstructTableGroupFrame(nsIPresShell*        aPresShell, 
                                                nsIPresContext*          aPresContext,
                                                nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsIFrame*                aParentFrame,
                                                nsIStyleContext*         aStyleContext,
                                                PRBool                   aIsRowGroup,
                                                nsIFrame*&               aNewTopFrame, 
                                                nsIFrame*&               aNewGroupFrame,
                                                nsTableCreator&          aTableCreator,
                                                nsTableList*             aToDo)   
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
		if (!aToDo)
			return NS_ERROR_NULL_POINTER;
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
    rv = ConstructTableGroupFrameOnly(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                      styleContext, aIsRowGroup, aNewTopFrame, aNewGroupFrame, 
                                      aTableCreator, contentDisplayIsGroup);
  } else if (aTableCreator.IsTreeCreator() && 
             parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP) {
    // We're a tree view. We want to allow nested row groups.
    // only process the group's children if we're called from above
    rv = ConstructTableGroupFrameOnly(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                      styleContext, aIsRowGroup, aNewTopFrame, aNewGroupFrame, 
                                      aTableCreator, contentDisplayIsGroup);
  } else { // construct anonymous frames
    //NS_WARNING("a non table contains a table row or col group child. \n");
    nsIFrame* innerFrame;
    nsIFrame* outerFrame;

    ConstructAnonymousTableFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aNewTopFrame,
                                 outerFrame, innerFrame, aTableCreator);
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
    rv = ConstructTableGroupFrameOnly(aPresShell, aPresContext, aState, aContent, innerFrame,
                                      styleContext, aIsRowGroup, topFrame, aNewGroupFrame, 
                                      aTableCreator, contentDisplayIsGroup);
    if (NS_FAILED(rv)) return rv;
    if (contentDisplayIsGroup) { // called from above
      innerFrame->SetInitialChildList(aPresContext, nsnull, topFrame);
      // set the primary frame to avoid getting the anonymous table frame 
      aState.mFrameManager->SetPrimaryFrameFor(aContent, aNewGroupFrame);
    } else { // called from row below
      aToDo->Set(innerFrame, topFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableGroupFrameOnly(nsIPresShell*        aPresShell, 
                                                    nsIPresContext*          aPresContext,
                                                    nsFrameConstructorState& aState,
                                                    nsIContent*              aContent,
                                                    nsIFrame*                aParentFrame,
                                                    nsIStyleContext*         aStyleContext,
                                                    PRBool                   aIsRowGroup,
                                                    nsIFrame*&               aNewTopFrame, 
                                                    nsIFrame*&               aNewGroupFrame,
                                                    nsTableCreator&          aTableCreator,
                                                    PRBool                   aProcessChildren) 
{
  nsresult rv = NS_OK;
  const nsStyleDisplay* styleDisplay = 
    (const nsStyleDisplay*) aStyleContext->GetStyleData(eStyleStruct_Display);

  rv = (aIsRowGroup) ? aTableCreator.CreateTableRowGroupFrame(&aNewGroupFrame)
                   : aTableCreator.CreateTableColGroupFrame(&aNewGroupFrame);

  if (IsScrollable(aPresContext, styleDisplay)) {

    // Create an area container for the frame

    BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, aNewGroupFrame, aParentFrame,
                     aNewTopFrame, aStyleContext);

  } else {
    if (NS_FAILED(rv)) return rv;
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aParentFrame, aStyleContext, nsnull, aNewGroupFrame);
    aNewTopFrame = aNewGroupFrame;
  }

  if (aProcessChildren) {
    nsFrameItems childItems;

    if (aIsRowGroup) {
      
      // Create some anonymous extras within the tree body.
      if (aTableCreator.IsTreeCreator()) {

     		const nsStyleDisplay *parentDisplay;
        aParentFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)parentDisplay);
        if (parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP) {
          // We're the child of another row group. If it's lazy, we're lazy.
          ((nsTreeRowGroupFrame*)aNewGroupFrame)->SetFrameConstructor(this);
        }
        else if (parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE) {
          // We're the child of a table.
          // See if our parent is a tree.
          // We will want to have a scrollbar.
          ((nsTreeRowGroupFrame*)aNewGroupFrame)->SetFrameConstructor(this);
        }
      }

      TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewGroupFrame,
                           childItems, aTableCreator);
    } else {
      ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewGroupFrame,
                      PR_FALSE, childItems, PR_FALSE);
    }
    aNewGroupFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
  } 
  
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableRowFrame(nsIPresShell*        aPresShell, 
                                              nsIPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrame,
                                              nsIStyleContext*         aStyleContext,
                                              nsIFrame*&               aNewTopFrame,
                                              nsIFrame*&               aNewRowFrame,
                                              nsTableCreator&          aTableCreator,
                                              nsTableList*             aToDo)
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
      // EDV this will not work.
      // 1) This crashes because the aParent is already what you think it is, event if it
      // is scrollable.
      // 2) If the we had gfx scrollbars turned on the first child would have been
      //    a scrollport!!
      //aParentFrame = TableGetAsNonScrollFrame(aPresContext, aParentFrame, parentDisplay); 
      aParentFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
      aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableRowPseudo,
                                                 groupStyleContext, PR_FALSE,
                                                 getter_AddRefs(styleContext));
    }
    // only process the row's children if we're called from above
    rv = ConstructTableRowFrameOnly(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                    styleContext, contentDisplayIsRow, aNewRowFrame, 
                                    aTableCreator);
    aNewTopFrame = aNewRowFrame;
  } else { // construct an anonymous row group frame
    nsIFrame* groupFrame;
    nsTableList localToDo;
    nsTableList* toDo = (aToDo) ? aToDo : &localToDo;

    // it may also need to create a table frame
    rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, styleContext,
                                  PR_TRUE, aNewTopFrame, groupFrame, aTableCreator, toDo);
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
    rv = ConstructTableRowFrameOnly(aPresShell, aPresContext, aState, aContent, groupFrame, styleContext, 
                                    contentDisplayIsRow, aNewRowFrame, aTableCreator);
    if (NS_FAILED(rv)) return rv;

    //XXX when bug 2479 is fixed this may be a duplicate call
    groupFrame->SetInitialChildList(aPresContext, nsnull, aNewRowFrame);
    if (contentDisplayIsRow) { // called from above
      // set the primary frame to avoid getting the anonymous row group frame 
      aState.mFrameManager->SetPrimaryFrameFor(aContent, aNewRowFrame);
    }

    if (contentDisplayIsRow) { // called from above
      TableProcessTableList(aPresContext, *toDo);
    }
  }
    
  if (aState.mFrameManager)
    aState.mFrameManager->SetPrimaryFrameFor(aContent, aNewRowFrame);

  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableRowFrameOnly(nsIPresShell*        aPresShell, 
                                                  nsIPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrame,
                                                  nsIStyleContext*         aStyleContext,
                                                  PRBool                   aProcessChildren,
                                                  nsIFrame*&               aNewRowFrame,
                                                  nsTableCreator&          aTableCreator)
{
  nsresult rv = aTableCreator.CreateTableRowFrame(&aNewRowFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewRowFrame);
  if (aProcessChildren) {
    nsFrameItems childItems;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewRowFrame,
                              childItems, aTableCreator);
    if (NS_FAILED(rv)) return rv;
    aNewRowFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsIPresShell*            aPresShell, 
                                              nsIPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrame,
                                              nsIStyleContext*         aStyleContext,
                                              nsIFrame*&               aNewTopFrame,
                                              nsIFrame*&               aNewColFrame,
                                              nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_OK;
  // the content display here is always table-col and were always called from above
  const nsStyleDisplay* parentDisplay = GetDisplay(aParentFrame);
  if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == parentDisplay->mDisplay) {
    rv = ConstructTableColFrameOnly(aPresShell, aPresContext, aState, aContent, aParentFrame, 
                                    aStyleContext, aNewColFrame, aTableCreator);
    aNewTopFrame = aNewColFrame;
  } else { // construct anonymous col group frame
    nsTableList toDo;
    nsIFrame* groupFrame;
    rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                  aStyleContext, PR_FALSE, aNewTopFrame, groupFrame,
                                  aTableCreator, &toDo);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIStyleContext> groupStyleContext; 
    groupFrame->GetStyleContext(getter_AddRefs(groupStyleContext));
    nsCOMPtr<nsIStyleContext> styleContext;
    aPresContext->ResolveStyleContextFor(aContent, groupStyleContext,
                                         PR_FALSE, getter_AddRefs(styleContext));
    rv = ConstructTableColFrameOnly(aPresShell, aPresContext, aState, aContent, groupFrame, 
                                    styleContext, aNewColFrame, aTableCreator);
    if (NS_FAILED(rv)) return rv;
    aState.mFrameManager->SetPrimaryFrameFor(aContent, aNewColFrame);
    //XXX when bug 2479 is fixed this may be a duplicate call
    groupFrame->SetInitialChildList(aPresContext, nsnull, aNewColFrame);

    // if an anoymous table got created, then set its initial child list
    TableProcessTableList(aPresContext, toDo);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableColFrameOnly(nsIPresShell*        aPresShell, 
                                                  nsIPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrame,
                                                  nsIStyleContext*         aStyleContext,
                                                  nsIFrame*&               aNewColFrame,
                                                  nsTableCreator&          aTableCreator)
{
  nsresult rv = aTableCreator.CreateTableColFrame(&aNewColFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewColFrame);
  nsFrameItems colChildItems;
  rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewColFrame,
                       PR_FALSE, colChildItems, PR_FALSE);
  if (NS_FAILED(rv)) return rv;
  aNewColFrame->SetInitialChildList(aPresContext, nsnull,
                                    colChildItems.childList);
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsIPresShell*        aPresShell, 
                                               nsIPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsIStyleContext*         aStyleContext,
                                               nsIFrame*&               aNewTopFrame,
                                               nsIFrame*&               aNewCellFrame,
                                               nsIFrame*&               aNewCellBodyFrame,
                                               nsTableCreator&          aTableCreator,
                                               PRBool                   aProcessChildren)
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
    rv = ConstructTableCellFrameOnly(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                     styleContext, aNewCellFrame, aNewCellBodyFrame,
                                     aTableCreator, aProcessChildren);
    aNewTopFrame = aNewCellFrame;
  } else { // the cell needs some ancestors to be fabricated
    //NS_WARNING("WARNING - a non table row contains a table cell child. \n");
    nsTableList toDo;
    nsIFrame* rowFrame;
    rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                aStyleContext, aNewTopFrame, rowFrame, aTableCreator,
                                &toDo);
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
    rv = ConstructTableCellFrameOnly(aPresShell, aPresContext, aState, aContent, rowFrame,
                                     styleContext, aNewCellFrame, aNewCellBodyFrame,
                                     aTableCreator, aProcessChildren);
    if (NS_FAILED(rv)) return rv;
    //XXX when bug 2479 is fixed this may be a duplicate call
    rowFrame->SetInitialChildList(aPresContext, nsnull, aNewCellFrame);
    TableProcessTableList(aPresContext, toDo);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrameOnly(nsIPresShell*        aPresShell, 
                                                   nsIPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrame,
                                                   nsIStyleContext*         aStyleContext,
                                                   nsIFrame*&               aNewCellFrame,
                                                   nsIFrame*&               aNewCellBodyFrame,
                                                   nsTableCreator&          aTableCreator,
                                                   PRBool                   aProcessChildren)
{
  nsresult rv;

  // Create a table cell frame
  rv = aTableCreator.CreateTableCellFrame(&aNewCellFrame);
  if (NS_FAILED(rv)) return rv;
 
  // Initialize the table cell frame
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewCellFrame);
  // Create an area frame that will format the cell's content
  rv = aTableCreator.CreateTableCellInnerFrame(&aNewCellBodyFrame);

  if (NS_FAILED(rv)) {
    aNewCellFrame->Destroy(aPresContext);
    aNewCellFrame = nsnull;
    return rv;
  }
  
  // Resolve pseudo style and initialize the body cell frame
  nsCOMPtr<nsIStyleContext>  bodyPseudoStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent,
                                             nsHTMLAtoms::cellContentPseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(bodyPseudoStyle));
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aNewCellFrame, bodyPseudoStyle, nsnull, aNewCellBodyFrame);

  if (aProcessChildren) {
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);

    // The area frame is a floater container
    nsFrameConstructorSaveState floaterSaveState;
    aState.PushFloaterContainingBlock(aNewCellBodyFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);

    // Process the child content
    nsFrameItems childItems;
    rv = ProcessChildren(aPresShell, aPresContext, aState, aContent,
                         aNewCellBodyFrame, PR_TRUE, childItems, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // if there are any anonymous children create frames for them
    nsCOMPtr<nsIAtom> tagName;
    aContent->GetTag(*getter_AddRefs(tagName));
    if (tagName && tagName.get() == nsXULAtoms::treecell) {
      CreateAnonymousTreeCellFrames(aPresShell, aPresContext, tagName, aState, aContent, aNewCellBodyFrame,
                                    aNewCellFrame, childItems);
    }

    aNewCellBodyFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mFloatedItems.childList) {
      aNewCellBodyFrame->SetInitialChildList(aPresContext,
                                             nsLayoutAtoms::floaterList,
                                             aState.mFloatedItems.childList);
    }
  }
  aNewCellFrame->SetInitialChildList(aPresContext, nsnull, aNewCellBodyFrame);

  return rv;
}

// This is only called by table row groups and rows. It allows children that are not
// table related to have a cell wrapped around them.
nsresult
nsCSSFrameConstructor::TableProcessChildren(nsIPresShell*        aPresShell, 
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsFrameItems&            aChildItems,
                                            nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_OK;
  
  // Iterate the child content objects and construct a frame
  PRInt32   count;

  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));

  const nsStyleDisplay* display = (const nsStyleDisplay*)
        parentStyleContext->GetStyleData(eStyleStruct_Display);
  if (aTableCreator.IsTreeCreator() &&
      (display->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP)) {
      // Stop the processing if we're lazy. The tree row group frame builds its children
      // as needed.
      return NS_OK;
  }
  
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> childContent;
    aContent->ChildAt(i, *getter_AddRefs(childContent));
    rv = TableProcessChild(aPresShell, aPresContext, aState, childContent, aParentFrame, parentStyleContext,
                           aChildItems, aTableCreator);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::TableProcessChild(nsIPresShell*        aPresShell, 
                                         nsIPresContext*          aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent*              aChildContent,
                                         nsIFrame*                aParentFrame,
                                         nsIStyleContext*         aParentStyleContext,
                                         nsFrameItems&            aChildItems,
                                         nsTableCreator&          aTableCreator)
{
  nsresult rv = NS_OK;
  
  if (nsnull != aChildContent) {
    nsCOMPtr<nsIStyleContext> childStyleContext;
    aPresContext->ResolveStyleContextFor(aChildContent, aParentStyleContext,
                                         PR_FALSE,
                                         getter_AddRefs(childStyleContext));
    const nsStyleDisplay* childDisplay = (const nsStyleDisplay*)
      childStyleContext->GetStyleData(eStyleStruct_Display);
    if (IsTableRelated(childDisplay->mDisplay)) {
      rv = ConstructFrame(aPresShell, aPresContext, aState, aChildContent, aParentFrame, aChildItems);
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
        if (parentDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE) {
          nsIFrame* outerFrame; 
          aParentFrame->GetParent(&outerFrame);
          rv = ConstructFrame(aPresShell, aPresContext, aState, aChildContent, outerFrame, aChildItems);
          // XXX: Seems like this is going into the inner frame's child list instead of the outer frame. - DWH
        } else {
          rv = ConstructFrame(aPresShell, aPresContext, aState, aChildContent, aParentFrame, aChildItems);
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
            nsIFrame* cellBodyFrame;
            nsIFrame* cellFrame;
            nsIFrame* childFrame; // XXX: Need to change the ConstructTableCell function to return lists of frames. - DWH
            rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aChildContent, aParentFrame, childStyleContext, 
                                         childFrame, cellFrame, cellBodyFrame, aTableCreator);
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
	      (nsXULAtoms::titledbutton      == tag.get())  ||
        (nsXULAtoms::image == tag.get()) ||
        (nsXULAtoms::grippy          == tag.get())  ||
        (nsXULAtoms::splitter        == tag.get())  ||
        (nsXULAtoms::slider == tag.get())  ||
        (nsXULAtoms::spinner == tag.get())  ||
        (nsXULAtoms::scrollbar == tag.get())  ||
        (nsXULAtoms::scrollbarbutton == tag.get())  ||
        (nsXULAtoms::thumb == tag.get())  ||
        (nsXULAtoms::colorpicker == tag.get())  ||
        (nsXULAtoms::fontpicker == tag.get())  ||
        (nsXULAtoms::text            == tag.get())  ||
        (nsXULAtoms::widget          == tag.get())  ||
        (nsXULAtoms::tree            == tag.get())  ||
        (nsXULAtoms::treechildren    == tag.get())  ||
        (nsXULAtoms::treeitem        == tag.get())  ||
        (nsXULAtoms::treerow         == tag.get())  ||
        (nsXULAtoms::treecell        == tag.get())  ||
        (nsXULAtoms::treeindentation == tag.get())  ||
        (nsXULAtoms::treecol         == tag.get())  ||
        (nsXULAtoms::treecolgroup    == tag.get())  ||
        (nsXULAtoms::treefoot        == tag.get())  ||
        (nsXULAtoms::menu          == tag.get())  ||
        (nsXULAtoms::menuitem      == tag.get())  || 
        (nsXULAtoms::menubar       == tag.get())  ||
        (nsXULAtoms::menupopup     == tag.get())  ||
        (nsXULAtoms::popupset  == tag.get())  ||
        (nsXULAtoms::popup == tag.get()) ||
        (nsXULAtoms::toolbox         == tag.get())  ||
        (nsXULAtoms::toolbar         == tag.get())  ||
        (nsXULAtoms::toolbaritem     == tag.get())  ||
        (nsXULAtoms::stack           == tag.get())  ||
        (nsXULAtoms::deck            == tag.get())  ||
        (nsXULAtoms::spring          == tag.get())  ||
        (nsXULAtoms::title           == tag.get())  ||
        (nsXULAtoms::titledbox       == tag.get())  ||
        (nsXULAtoms::tabcontrol      == tag.get())  ||
        (nsXULAtoms::tabbox          == tag.get())  ||
        (nsXULAtoms::tabpanel        == tag.get())  ||
        (nsXULAtoms::tabpage         == tag.get())  ||
        (nsXULAtoms::progressbar     == tag.get())  ||        
        (nsXULAtoms::checkbox        == tag.get())  ||
        (nsXULAtoms::radio           == tag.get())  ||
        (nsXULAtoms::menulist        == tag.get())  ||
        (nsXULAtoms::menubutton      == tag.get())  ||
        (nsXULAtoms::textfield       == tag.get())  ||
        (nsXULAtoms::textarea        == tag.get())  
) {
    return PR_TRUE;
  }
#endif

//MathML Mod - DJF
#ifdef MOZ_MATHML
  if (  (nsMathMLAtoms::math          == tag.get())  ) {
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
    rv = inner->SetInitialChildList(aPresContext, nsnull, child);
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
    aFrame->FirstChild(aPresContext, nsnull, &result);
  }
  return result;
}

//  nsIAtom* pseudoTag;
//  styleContext->GetPseudoType(pseudoTag);
//  if (pseudoTag != nsLayoutAtoms::scrolledContentPseudo) {
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

PRBool
nsCSSFrameConstructor::IsTableRelated(PRUint8 aDisplay)
{
  return (aDisplay == NS_STYLE_DISPLAY_TABLE)              ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP) ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP)    ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP) ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW)          ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN)       ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_CELL);
}

/***********************************************
 * END TABLE SECTION
 ***********************************************/

nsresult
nsCSSFrameConstructor::ConstructDocElementTableFrame(nsIPresShell*        aPresShell, 
                                                     nsIPresContext* aPresContext,
                                                     nsIContent*     aDocElement,
                                                     nsIFrame*       aParentFrame,
                                                     nsIFrame*&      aNewTableFrame,
                                                     nsILayoutHistoryState* aFrameState)
{
  nsFrameConstructorState state(aPresContext, nsnull, nsnull, nsnull, aFrameState);
  nsFrameItems    frameItems;

  ConstructFrame(aPresShell, aPresContext, state, aDocElement, aParentFrame, frameItems);
  aNewTableFrame = frameItems.childList;
  return NS_OK;
}

/**
 * New one
 */
nsresult
nsCSSFrameConstructor::ConstructDocElementFrame(nsIPresShell*        aPresShell, 
                                                nsIPresContext*          aPresContext,
                                                nsFrameConstructorState& aState,
                                                nsIContent*              aDocElement,
                                                nsIFrame*                aParentFrame,
                                                nsIStyleContext*         aParentStyleContext,
                                                nsIFrame*&               aNewFrame)
{
    // how the root frame hierarchy should look

    /*

---------------No Scrollbars------


     AreaFrame or BoxFrame (InitialContainingBlock)
  


---------------Native Scrollbars------



     ScrollFrame

         ^
         |
     AreaFrame or BoxFrame (InitialContainingBlock)
  

---------------Gfx Scrollbars ------


     GfxScrollFrame

         ^
         |
     ScrollPort

         ^
         |
     AreaFrame or BoxFrame (InitialContainingBlock)
          
*/    

  if (!mTempFrameTreeState)
    aPresShell->GetHistoryState(getter_AddRefs(mTempFrameTreeState));

  // ----- reattach gfx scrollbars ------
  // Gfx scrollframes were created in the root frame but the primary frame map may have been destroyed if a 
  // new style sheet was loaded so lets reattach the frames to their content.
    if (mGfxScrollFrame)
    {
        nsIFrame* scrollPort = nsnull;
        mGfxScrollFrame->FirstChild(aPresContext, nsnull, &scrollPort);

        nsIFrame* gfxScrollbarFrame1 = nsnull;
        nsIFrame* gfxScrollbarFrame2 = nsnull;
        nsresult rv = scrollPort->GetNextSibling(&gfxScrollbarFrame1);
        rv = gfxScrollbarFrame1->GetNextSibling(&gfxScrollbarFrame2);
        nsCOMPtr<nsIContent> content;
        gfxScrollbarFrame1->GetContent(getter_AddRefs(content));
        aState.mFrameManager->SetPrimaryFrameFor(content, gfxScrollbarFrame1);
        gfxScrollbarFrame2->GetContent(getter_AddRefs(content));
        aState.mFrameManager->SetPrimaryFrameFor(content, gfxScrollbarFrame2);
    }

  // --------- CREATE AREA OR BOX FRAME -------
  nsCOMPtr<nsIStyleContext>  styleContext;
  aPresContext->ResolveStyleContextFor(aDocElement, aParentStyleContext,
                                       PR_FALSE,
                                       getter_AddRefs(styleContext));

  const nsStyleDisplay* display = 
    (const nsStyleDisplay*)styleContext->GetStyleData(eStyleStruct_Display);

  PRBool docElemIsTable = IsTableRelated(display->mDisplay);
 

  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

  PRBool isScrollable = IsScrollable(aPresContext, display);
  PRBool isPaginated = PR_FALSE;
  aPresContext->IsPaginated(&isPaginated);
  nsIFrame* scrollFrame = nsnull;

  // build a scrollframe
  if (!isPaginated && isScrollable) {
    nsIFrame* newScrollFrame = nsnull;
    nsCOMPtr<nsIDocument> document;
    aDocElement->GetDocument(*getter_AddRefs(document));
    nsCOMPtr<nsIStyleContext> newContext;

    BeginBuildingScrollFrame( aPresShell, aPresContext,
                              aState,
                              aDocElement,
                              styleContext,
                              aParentFrame,
                              nsLayoutAtoms::scrolledContentPseudo,
                              document,
                              scrollFrame,
                              newContext,
                              newScrollFrame);

    styleContext = newContext;
    aParentFrame = newScrollFrame;
  }

  nsIFrame* contentFrame = nsnull;
  PRBool isBlockFrame = PR_FALSE;

  if (docElemIsTable) {
      // if the document is a table then just populate it.
      ConstructDocElementTableFrame(aPresShell, aPresContext, aDocElement, 
                                    aParentFrame, contentFrame,
                                    aState.mFrameState);
      contentFrame->GetStyleContext(getter_AddRefs(styleContext));
  } else {
        // otherwise build a box or a block
        PRInt32 nameSpaceID;
        if (NS_SUCCEEDED(aDocElement->GetNameSpaceID(nameSpaceID)) &&
            nameSpaceID == nsXULAtoms::nameSpaceID) {
          NS_NewBoxFrame(aPresShell, &contentFrame, PR_TRUE);
        }
        else {
          NS_NewDocumentElementFrame(aPresShell, &contentFrame);
          isBlockFrame = PR_TRUE;
        }

        // initialize the child
        InitAndRestoreFrame(aPresContext, aState, aDocElement, 
                            aParentFrame, styleContext, nsnull, contentFrame);
  }
  
  // set the primary frame
  aState.mFrameManager->SetPrimaryFrameFor(aDocElement, contentFrame);

  // Finish building the scrollframe
  if (isScrollable) {
    FinishBuildingScrollFrame(aPresContext, 
                          aState,
                          aDocElement,
                          aParentFrame,
                          contentFrame,
                          styleContext);
    // primary is set above (to the contentFrame)
    
    aNewFrame = scrollFrame;
  } else {
     // if not scrollable the new frame is the content frame.
     aNewFrame = contentFrame;
  }

  mInitialContainingBlock = contentFrame;

  // if it was a table then we don't need to process our children.
  if (!docElemIsTable) {
    // Process the child content
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floaterSaveState;
    nsFrameItems                childItems;

    // XXX these next lines are wrong for BoxFrame
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aDocElement, styleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    aState.PushAbsoluteContainingBlock(contentFrame, absoluteSaveState);
    aState.PushFloaterContainingBlock(contentFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    ProcessChildren(aPresShell, aPresContext, aState, aDocElement, contentFrame,
                    PR_TRUE, childItems, isBlockFrame);

    // See if the document element has a fixed background attachment.
    // Note: the reason we wait until after processing the document element's
    // children is because of special treatment of the background for the HTML
    // element. See BodyFixupRule::MapStyleInto() for details
    const nsStyleColor* color;
    color = (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);
    if (NS_STYLE_BG_ATTACHMENT_FIXED == color->mBackgroundAttachment) {
      // Fixed background attachments are handled by setting the
      // NS_VIEW_PUBLIC_FLAG_DONT_BITBLT flag bit on the view.
      //
      // If the document element's frame is scrollable, then set the bit on its
      // view; otherwise, set it on the root frame's view. This avoids
      // unnecessarily creating another view and should be faster
      nsIView*  view;

      if (IsScrollable(aPresContext, display)) {
        contentFrame->GetView(aPresContext, &view);
      } else {
        nsIFrame* parentFrame;

        contentFrame->GetParent(&parentFrame);
        parentFrame->GetView(aPresContext, &view);
      }

      // Not all shells have scroll frames, even in scrollable presContext (bug 30317)
      if (view) {
        PRUint32  viewFlags;
        view->GetViewFlags(&viewFlags);
        view->SetViewFlags(viewFlags | NS_VIEW_PUBLIC_FLAG_DONT_BITBLT);
      }
    }

    // Set the initial child lists
    contentFrame->SetInitialChildList(aPresContext, nsnull,
                                      childItems.childList);
 
    // only support absolute positioning if we are a block.
    // if we are a box don't do it.
    if (isBlockFrame) {
        if (aState.mAbsoluteItems.childList) {
          contentFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         aState.mAbsoluteItems.childList);
        }
        if (aState.mFloatedItems.childList) {
          contentFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         aState.mFloatedItems.childList);
        }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsCSSFrameConstructor::ConstructRootFrame(nsIPresShell*        aPresShell, 
                                          nsIPresContext* aPresContext,
                                          nsIContent*     aDocElement,
                                          nsIFrame*&      aNewFrame)
{

  // how the root frame hierarchy should look

    /*

---------------No Scrollbars------



     ViewPortFrame (FixedContainingBlock) <---- RootView

         ^
         |
     RootFrame(DocElementContainingBlock)
  


---------------Native Scrollbars------



     ViewPortFrame (FixedContainingBlock) <---- RootView

         ^
         |
     ScrollFrame <--- RootScrollableView

         ^
         |
     RootFrame(DocElementContainingBlock)
  

---------------Gfx Scrollbars ------


     ViewPortFrame (FixedContainingBlock) <---- RootView

         ^
         |
     GfxScrollFrame

         ^
         |
     ScrollPort <--- RootScrollableView

         ^
         |
     RootFrame(DocElementContainingBlock)
          
*/    


  // --------- BUILD VIEWPORT -----------
  nsIFrame*                 viewportFrame = nsnull;
  nsCOMPtr<nsIStyleContext> viewportPseudoStyle;

  aPresContext->ResolvePseudoStyleContextFor(nsnull, nsLayoutAtoms::viewportPseudo,
                                           nsnull, PR_FALSE,
                                           getter_AddRefs(viewportPseudoStyle));

  { // ensure that the viewport thinks it is a block frame, layout goes pootsy if it doesn't
    nsStyleDisplay* display = (nsStyleDisplay*)viewportPseudoStyle->GetMutableStyleData(eStyleStruct_Display);
    display->mDisplay = NS_STYLE_DISPLAY_BLOCK;
  }

  NS_NewViewportFrame(aPresShell, &viewportFrame);


  viewportFrame->Init(aPresContext, nsnull, nsnull, viewportPseudoStyle, nsnull);

  // Bind the viewport frame to the root view
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIViewManager> viewManager;
  presShell->GetViewManager(getter_AddRefs(viewManager));
  nsIView*        rootView;

  viewManager->GetRootView(rootView);
  viewportFrame->SetView(aPresContext, rootView);

  // The viewport is the containing block for 'fixed' elements
  mFixedContainingBlock = viewportFrame;

  // --------- CREATE ROOT FRAME -------


    // Create the root frame. The document element's frame is a child of the
    // root frame.
    //
    // The root frame serves two purposes:
    // - reserves space for any margins needed for the document element's frame
    // - makes sure that the document element's frame covers the entire canvas

    PRBool isPaginated = PR_FALSE;
    aPresContext->IsPaginated(&isPaginated);
    nsIFrame* rootFrame = nsnull;
    nsIAtom* rootPseudo;
        
    if (!isPaginated) {
        NS_NewRootFrame(aPresShell, &rootFrame);
        rootPseudo = nsLayoutAtoms::canvasPseudo;
        mDocElementContainingBlock = rootFrame;
    } else {
        // Create a page sequence frame
        NS_NewSimplePageSequenceFrame(aPresShell, &rootFrame);
        rootPseudo = nsLayoutAtoms::pageSequencePseudo;
    }


  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

      // If the device supports scrolling (e.g., in galley mode on the screen and
  // for print-preview, but not when printing), then create a scroll frame that
  // will act as the scrolling mechanism for the viewport. 
  // XXX Do we even need a viewport when printing to a printer?
  // XXX It would be nice to have a better way to query for whether the device
  // is scrollable
  PRBool  isScrollable = PR_TRUE;
  if (aPresContext) {
    nsIDeviceContext* dc;
    aPresContext->GetDeviceContext(&dc);
    if (dc) {
      PRBool  supportsWidgets;
      if (NS_SUCCEEDED(dc->SupportsNativeWidgets(supportsWidgets))) {
        isScrollable = supportsWidgets;
      }
      NS_RELEASE(dc);
    }
  }

  // As long as the webshell doesn't prohibit it, and the device supports
  // it, create a scroll frame that will act as the scolling mechanism for
  // the viewport.
  //
  // Threre are three possible values stored in the docshell:
  //  1) NS_STYLE_OVERFLOW_HIDDEN = no scrollbars
  //  2) NS_STYLE_OVERFLOW_AUTO = scrollbars appear if needed
  //  3) NS_STYLE_OVERFLOW_SCROLL = scrollbars always
  // Only need to create a scroll frame/view for cases 2 and 3.
  // Currently OVERFLOW_SCROLL isn't honored, as
  // scrollportview::SetScrollPref is not implemented.
  PRInt32 nameSpaceID; // Never create scrollbars for XUL documents
  if (NS_SUCCEEDED(aDocElement->GetNameSpaceID(nameSpaceID)) &&
      nameSpaceID == nsXULAtoms::nameSpaceID) {
    isScrollable = PR_FALSE;
  } else {
    nsresult rv;
    nsCOMPtr<nsISupports> container;
    if (nsnull != aPresContext) {
      aPresContext->GetContainer(getter_AddRefs(container));
      if (nsnull != container) {
        nsCOMPtr<nsIScrollable> scrollableContainer = do_QueryInterface(container, &rv);
        if (NS_SUCCEEDED(rv) && scrollableContainer) {
          PRInt32 scrolling = -1;
          // XXX We should get prefs for X and Y and deal with these independently!
          scrollableContainer->GetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,&scrolling);
          if (NS_STYLE_OVERFLOW_HIDDEN == scrolling) {
            isScrollable = PR_FALSE;
          }
          // XXX NS_STYLE_OVERFLOW_SCROLL should create 'always on' scrollbars
        }
      }
    }
  }

  nsIFrame* newFrame = rootFrame;
  nsCOMPtr<nsIStyleContext> rootPseudoStyle;
  // we must create a state because if the scrollbars are GFX it needs the 
  // state to build the scrollbar frames.
  nsFrameConstructorState state(aPresContext,
                                      nsnull,
                                      nsnull,
                                      nsnull,
                                      nsnull);

  nsIFrame* parentFrame = viewportFrame;

  if (isScrollable) {

      // built the frame. We give it the content we are wrapping which is the document,
      // the root frame, the parent view port frame, and we should get back the new
      // frame and the scrollable view if one was created.

      // resolve a context for the scrollframe
      nsCOMPtr<nsIStyleContext>  styleContext;
      aPresContext->ResolvePseudoStyleContextFor(nsnull,
                                                 nsLayoutAtoms::viewportScrollPseudo,
                                                 viewportPseudoStyle, PR_FALSE,
                                                 getter_AddRefs(styleContext));


      nsIFrame* newScrollableFrame = nsnull;
      nsCOMPtr<nsIDocument> document;
      aDocElement->GetDocument(*getter_AddRefs(document));

      BeginBuildingScrollFrame( aPresShell,
                                aPresContext,
                                state,
                                nsnull,
                                styleContext,
                                viewportFrame,
                                rootPseudo,
                                document,
                                newFrame,
                                rootPseudoStyle,
                                newScrollableFrame);

      // Inform the view manager about the root scrollable view
      // get the scrolling view
      nsIView* view = nsnull;
      newScrollableFrame->GetView(aPresContext, &view);
      nsIScrollableView* scrollableView;
      view->QueryInterface(kScrollViewIID, (void**)&scrollableView);
      viewManager->SetRootScrollableView(scrollableView);
      parentFrame = newScrollableFrame;

      // if gfx scrollbars store them
      if (HasGfxScrollbars(aPresContext)) {
          mGfxScrollFrame = newFrame;
      } else {
          mGfxScrollFrame = nsnull;
      }
  } else {
    aPresContext->ResolvePseudoStyleContextFor(nsnull, rootPseudo,
                                               viewportPseudoStyle,
                                               PR_FALSE,
                                               getter_AddRefs(rootPseudoStyle));
  }

  rootFrame->Init(aPresContext, nsnull, parentFrame, rootPseudoStyle, nsnull);
  
  if (isScrollable) {
    FinishBuildingScrollFrame(aPresContext, 
                              state,
                              aDocElement,
                              parentFrame,
                              rootFrame,
                              rootPseudoStyle);

    // set the primary frame to the root frame
    state.mFrameManager->SetPrimaryFrameFor(aDocElement, rootFrame);
  }

  if (isPaginated) {
    // Create the first page
    nsIFrame* pageFrame;
    NS_NewPageFrame(aPresShell, &pageFrame);

    // The page is the containing block for 'fixed' elements. which are repeated
    // on every page
    mFixedContainingBlock = pageFrame;

    // Initialize the page and force it to have a view. This makes printing of
    // the pages easier and faster.
    nsCOMPtr<nsIStyleContext> pagePseudoStyle;

    aPresContext->ResolvePseudoStyleContextFor(nsnull, nsLayoutAtoms::pagePseudo,
                                               rootPseudoStyle, PR_FALSE,
                                               getter_AddRefs(pagePseudoStyle));

    pageFrame->Init(aPresContext, nsnull, rootFrame, pagePseudoStyle,
                    nsnull);
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, pageFrame,
                                             pagePseudoStyle, PR_TRUE);

    // The eventual parent of the document element frame
    mDocElementContainingBlock = pageFrame;


    // Set the initial child lists
    rootFrame->SetInitialChildList(aPresContext, nsnull, pageFrame);

  }

  viewportFrame->SetInitialChildList(aPresContext, nsnull, newFrame);
  
  aNewFrame = viewportFrame;



  return NS_OK;  
}


nsresult
nsCSSFrameConstructor::CreatePlaceholderFrameFor(nsIPresShell*        aPresShell, 
                                                 nsIPresContext*  aPresContext,
                                                 nsIFrameManager* aFrameManager,
                                                 nsIContent*      aContent,
                                                 nsIFrame*        aFrame,
                                                 nsIStyleContext* aStyleContext,
                                                 nsIFrame*        aParentFrame,
                                                 nsIFrame**       aPlaceholderFrame)
{
  nsPlaceholderFrame* placeholderFrame;
  nsresult            rv = NS_NewPlaceholderFrame(aPresShell, (nsIFrame**)&placeholderFrame);

  if (NS_SUCCEEDED(rv)) {
    // The placeholder frame gets a pseudo style context
    nsCOMPtr<nsIStyleContext>  placeholderPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::placeholderPseudo,
                                               aStyleContext,
                                               PR_FALSE,
                                               getter_AddRefs(placeholderPseudoStyle));
    placeholderFrame->Init(aPresContext, aContent, aParentFrame,
                           placeholderPseudoStyle, nsnull);
  
    // Add mapping from absolutely positioned frame to its placeholder frame
    aFrameManager->SetPlaceholderFrameFor(aFrame, placeholderFrame);

    // The placeholder frame has a pointer back to the out-of-flow frame
    placeholderFrame->SetOutOfFlowFrame(aFrame);
  
    *aPlaceholderFrame = NS_STATIC_CAST(nsIFrame*, placeholderFrame);
  }

  return rv;
}


nsWidgetRendering
nsCSSFrameConstructor::GetFormElementRenderingMode(nsIPresContext*		aPresContext,
																									 nsWidgetType				aWidgetType) 
{ 
  if (!aPresContext) { return eWidgetRendering_Gfx;}

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);

	switch (mode)
	{ 
		case eWidgetRendering_Gfx: 
			return eWidgetRendering_Gfx; 

		case eWidgetRendering_PartialGfx: 
			switch (aWidgetType)
			{
				case eWidgetType_Button:
				case eWidgetType_Checkbox:
				case eWidgetType_Radio:
				case eWidgetType_Text:
					return eWidgetRendering_Gfx; 

				default: 
					return eWidgetRendering_Native; 
			} 

		case eWidgetRendering_Native: 
		  PRBool useNativeWidgets = PR_FALSE;
	    nsIDeviceContext* dc;
	    aPresContext->GetDeviceContext(&dc);
	    if (dc) {
	      PRBool  supportsWidgets;
	      if (NS_SUCCEEDED(dc->SupportsNativeWidgets(supportsWidgets))) {
	        useNativeWidgets = supportsWidgets;
	      }
	      NS_RELEASE(dc);
	    }
			if (useNativeWidgets) 
				return eWidgetRendering_Native;
			else
				return eWidgetRendering_Gfx;
	}
	return eWidgetRendering_Gfx; 
}


nsresult
nsCSSFrameConstructor::ConstructRadioControlFrame(nsIPresShell*        aPresShell, 
                                                 nsIPresContext*  aPresContext,
                                                 nsIFrame*&   aNewFrame,
                                                 nsIContent*  aContent,
                                                 nsIStyleContext* aStyleContext)
{
  nsresult rv = NS_OK;
	if (GetFormElementRenderingMode(aPresContext, eWidgetType_Radio) == eWidgetRendering_Gfx)
		rv = NS_NewGfxRadioControlFrame(aPresShell, &aNewFrame);
	else
    NS_ASSERTION(0, "We longer support native widgets");

  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
    return rv;
  }

  nsCOMPtr<nsIStyleContext> radioStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::radioPseudo, 
    aStyleContext, PR_FALSE, getter_AddRefs(radioStyle));
  nsIRadioControlFrame* radio = nsnull;
  if (aNewFrame != nsnull && NS_SUCCEEDED(aNewFrame->QueryInterface(kIRadioControlFrameIID, (void**)&radio))) {
    radio->SetRadioButtonFaceStyleContext(radioStyle);
    NS_RELEASE(radio);
  }
 return rv;
}

nsresult
nsCSSFrameConstructor::ConstructCheckboxControlFrame(nsIPresShell*        aPresShell, 
                                                     nsIPresContext*     		aPresContext,
                                                	 nsIFrame*&          		aNewFrame)
{
  nsresult rv = NS_OK;
	if (GetFormElementRenderingMode(aPresContext, eWidgetType_Checkbox) == eWidgetRendering_Gfx)
		rv = NS_NewGfxCheckboxControlFrame(aPresShell, &aNewFrame);
	else
    NS_ASSERTION(0, "We longer support native widgets");


  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructButtonControlFrame(nsIPresShell*        aPresShell, 
                                                   nsIPresContext*     		aPresContext,
                                                	 nsIFrame*&          		aNewFrame)
{
  nsresult rv = NS_OK;
	if (GetFormElementRenderingMode(aPresContext, eWidgetType_Button) == eWidgetRendering_Gfx)
		rv = NS_NewGfxButtonControlFrame(aPresShell, &aNewFrame);
	else
    NS_ASSERTION(0, "We longer support native widgets");

  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTextControlFrame(nsIPresShell*        aPresShell, 
                                                 nsIPresContext*          aPresContext,
                                                 nsIFrame*&               aNewFrame,
                                                 nsIContent*              aContent)
{
  if (!aPresContext) { return NS_ERROR_NULL_POINTER;}
  nsresult rv = NS_OK;

  //Do we want an Autocomplete input text widget?
  nsString val1;
  nsString val2;
  if ((NS_OK == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::autocompletetimeout, val1)) ||
  	  (NS_OK == aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::autocompletetype, val2))) {
    if (! val1.IsEmpty() || ! val2.IsEmpty()) {
    	//ducarroz: How can I check if I am in a xul document?
	      rv = NS_NewGfxAutoTextControlFrame(aPresShell, &aNewFrame);
	      if (NS_FAILED(rv)) {
	        aNewFrame = nsnull;
	      }
    	  else
    	    return rv;
    }
  }

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if (eWidgetRendering_Gfx == mode) 
  {
    rv = NS_NewGfxTextControlFrame(aPresShell, &aNewFrame);
    if (NS_FAILED(rv)) {
      aNewFrame = nsnull;
    }
    if (aNewFrame)
      ((nsGfxTextControlFrame*)aNewFrame)->SetFrameConstructor(this);
  }
  if (!aNewFrame)
  {
    NS_ASSERTION(0, "We longer support native widgets");
  }
  return rv;
}

PRBool
nsCSSFrameConstructor::HasGfxScrollbars(nsIPresContext* aPresContext)
{
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_PROGID));
    PRBool gfx = PR_FALSE;
    if (pref) {
	    pref->GetBoolPref("nglayout.widget.gfxscrollbars", &gfx);
    }

    return gfx;

  /*
  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  return (eWidgetRendering_Native != mode);
  */
}

nsresult
nsCSSFrameConstructor::ConstructSelectFrame(nsIPresShell*        aPresShell, 
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsIStyleContext*         aStyleContext,
                                            nsIFrame*&               aNewFrame,
                                            PRBool&                  aProcessChildren,
                                            PRBool                   aIsAbsolutelyPositioned,
                                            PRBool&                  aFrameHasBeenInitialized,
                                            PRBool                   aIsFixedPositioned,
                                            nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  const PRInt32 kNoSizeSpecified = -1;

  if (eWidgetRendering_Gfx == mode) {
    // Construct a frame-based listbox or combobox
    nsCOMPtr<nsIDOMHTMLSelectElement> sel(do_QueryInterface(aContent));
    PRInt32 size = 1;
    if (sel) {
      sel->GetSize(&size); 
      PRBool multipleSelect = PR_FALSE;
      sel->GetMultiple(&multipleSelect);
       // Construct a combobox if size=1 or no size is specified and its multiple select
      if (((1 == size || 0 == size) || (kNoSizeSpecified  == size)) && (PR_FALSE == multipleSelect)) {
          // Construct a frame-based combo box.
          // The frame-based combo box is built out of tree parts. A display area, a button and
          // a dropdown list. The display area and button are created through anonymous content.
          // The drop-down list's frame is created explicitly. The combobox frame shares it's content
          // with the drop-down list.
        PRUint32 flags = NS_BLOCK_SHRINK_WRAP | (aIsAbsolutelyPositioned?NS_BLOCK_SPACE_MGR:0);
        nsIFrame * comboboxFrame;
        rv = NS_NewComboboxControlFrame(aPresShell, &comboboxFrame, flags);

          // Determine geometric parent for the combobox frame
        nsIFrame* geometricParent = aParentFrame;
        if (aIsAbsolutelyPositioned) {
          geometricParent = aState.mAbsoluteItems.containingBlock;
        } else if (aIsFixedPositioned) {
          geometricParent = aState.mFixedItems.containingBlock;
        }
        // Initialize the combobox frame
        InitAndRestoreFrame(aPresContext, aState, aContent, 
                            geometricParent, aStyleContext, nsnull, comboboxFrame);

        nsHTMLContainerFrame::CreateViewForFrame(aPresContext, comboboxFrame,
                                                 aStyleContext, PR_FALSE);
#ifdef NEWGFX_DROPDOWN
        ///////////////////////////////////////////////////////////////////
        // Combobox - New GFX Implementation
        ///////////////////////////////////////////////////////////////////
        // Construct a frame-based list box 
        nsIComboboxControlFrame* comboBox = nsnull;
        if (NS_SUCCEEDED(comboboxFrame->QueryInterface(kIComboboxControlFrameIID, (void**)&comboBox))) {
          comboBox->SetFrameConstructor(this);

          nsIFrame * listFrame; 
          rv = NS_NewGfxListControlFrame(aPresShell, &listFrame);

            // Notify the listbox that it is being used as a dropdown list.
          nsIListControlFrame * listControlFrame;
          if (NS_SUCCEEDED(listFrame->QueryInterface(kIListControlFrameIID, (void**)&listControlFrame))) {
            listControlFrame->SetComboboxFrame(comboboxFrame);
          }
             // Notify combobox that it should use the listbox as it's popup
          comboBox->SetDropDown(listFrame);

          nsCOMPtr<nsIStyleContext> gfxListStyle;
          aPresContext->ResolvePseudoStyleContextFor(aContent, 
                                            nsHTMLAtoms::dropDownListPseudo, 
                                            aStyleContext, PR_FALSE, 
                                            getter_AddRefs(gfxListStyle)); 

          // initialize the list control 
          InitAndRestoreFrame(aPresContext, aState, aContent, 
                              comboboxFrame, gfxListStyle, nsnull, listFrame);

          nsHTMLContainerFrame::CreateViewForFrame(aPresContext, listFrame,
                                         gfxListStyle, PR_TRUE);

          // create the area frame we are scrolling 
          PRUint32 flags = NS_BLOCK_SHRINK_WRAP | (aIsAbsolutelyPositioned?NS_BLOCK_SPACE_MGR:0);
          nsIFrame* scrolledFrame = nsnull;
          NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

          // resolve a style for our gfx scrollframe based on the list frames style 
          // resolve a style for our gfx scrollframe based on the list frames style 
          nsCOMPtr<nsIStyleContext> scrollFrameStyle;
          aPresContext->ResolvePseudoStyleContextFor(aContent, 
                                            nsLayoutAtoms::selectScrolledContentPseudo,
                                            gfxListStyle, PR_FALSE, 
                                            getter_AddRefs(scrollFrameStyle)); 
        
         InitAndRestoreFrame(aPresContext, aState, aContent, 
                              listFrame, scrollFrameStyle, nsnull, scrolledFrame);

          // XXX Temporary for Bug 19416
          {
            nsIView * lstView;
            scrolledFrame->GetView(aPresContext, &lstView);
            if (lstView) {
              lstView->IgnoreSetPosition(PR_TRUE);
            }
          }
          // this is what is returned when the scrollframe is created. 
          // newScrollFrame - Either a gfx scrollframe or a native scrollframe that was created 
          // scrolledFrameStyleContext - The resolved style context of the scrolledframe you passed in. 
          // this is not the style of the scrollFrame. 

          nsIFrame* newScrollFrame = nsnull; 

          nsFrameItems anonChildItems; 
            // Create display and button frames from the combobox'es anonymous content
          CreateAnonymousFrames(aPresShell, aPresContext, nsHTMLAtoms::combobox, aState, aContent, comboboxFrame,
                                anonChildItems);
        
          comboboxFrame->SetInitialChildList(aPresContext, nsnull,
                                 anonChildItems.childList);


          /* scroll frame */

#ifdef NEWGFX_LIST_SCROLLFRAME
          nsIStyleContext * scrolledFrameStyleContext = nsnull; 

          // ok take the style context, the Select area frame to scroll,the listFrame, and its parent 
          // and build the scrollframe. 
          BuildScrollFrame(aPresShell, aPresContext, aState, aContent, scrollFrameStyle, scrolledFrame, 
                           listFrame, newScrollFrame, scrolledFrameStyleContext); 
#else
          newScrollFrame = scrolledFrame;
#endif



          // The area frame is a floater container 
          PRBool haveFirstLetterStyle, haveFirstLineStyle; 
          HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext, 
                                &haveFirstLetterStyle, &haveFirstLineStyle); 
          nsFrameConstructorSaveState floaterSaveState; 
          aState.PushFloaterContainingBlock(scrolledFrame, floaterSaveState, 
                                            haveFirstLetterStyle, 
                                            haveFirstLineStyle); 

          // Process children 
          nsFrameItems childItems; 

          ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_FALSE, 
                          childItems, PR_TRUE); 

          // if a select is being created with zero options we need to create 
          // a special pseudo frame so it can be sized as best it can 
          nsCOMPtr<nsIDOMHTMLSelectElement> selectElement; 
          nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement), 
                                                       (void**)getter_AddRefs(selectElement)); 
          if (NS_SUCCEEDED(result) && selectElement) { 
            PRUint32 numOptions = 0; 
            result = selectElement->GetLength(&numOptions); 
            if (NS_SUCCEEDED(result) && 0 == numOptions) { 
              nsCOMPtr<nsIStyleContext> styleContext;
              nsIFrame*         generatedFrame = nsnull; 
              scrolledFrame->GetStyleContext(getter_AddRefs(styleContext)); 
              if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, scrolledFrame, aContent, 
                                              styleContext, nsLayoutAtoms::dummyOptionPseudo, 
                                              PR_FALSE, &generatedFrame)) { 
                // Add the generated frame to the child list 
                childItems.AddChild(generatedFrame); 
              } 
            } 
          } 

          // Set the scrolled frame's initial child lists 
          scrolledFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList); 

          // set the floated list 
          if (aState.mFloatedItems.childList) { 
            scrolledFrame->SetInitialChildList(aPresContext, 
                                               nsLayoutAtoms::floaterList, 
                                               aState.mFloatedItems.childList); 
          } 

          // now we need to set the initial child list of the listFrame and this of course will be the gfx scrollframe 
          listFrame->SetInitialChildList(aPresContext, nsnull, newScrollFrame); 

          // Initialize the additional popup child list which contains the dropdown list frame.
          nsFrameItems popupItems;
          popupItems.AddChild(listFrame);
          comboboxFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::popupList,
                                             popupItems.childList);

          // our new frame retured is the top frame which is the list frame. 
          //aNewFrame = listFrame; 

          // yes we have already initialized our frame 
           // Don't process, the children, They are already processed by the InitializeScrollFrame
           // call above.
          aProcessChildren = PR_FALSE;
          aNewFrame = comboboxFrame;
          aFrameHasBeenInitialized = PR_TRUE;
        }
#else
        ///////////////////////////////////////////////////////////////////
        // Combobox - Old Native Implementation
        ///////////////////////////////////////////////////////////////////
        nsIComboboxControlFrame* comboBox = nsnull;
        if (NS_SUCCEEDED(comboboxFrame->QueryInterface(kIComboboxControlFrameIID, (void**)&comboBox))) {
          comboBox->SetFrameConstructor(this);

            // Create a listbox
          nsIFrame * listFrame;
          rv = NS_NewListControlFrame(aPresShell, &listFrame);

            // Notify the listbox that it is being used as a dropdown list.
          nsIListControlFrame * listControlFrame;
          if (NS_SUCCEEDED(listFrame->QueryInterface(kIListControlFrameIID, (void**)&listControlFrame))) {
            listControlFrame->SetComboboxFrame(comboboxFrame);
          }
             // Notify combobox that it should use the listbox as it's popup
          comboBox->SetDropDown(listFrame);
         
            // Resolve psuedo element style for the dropdown list 
          nsCOMPtr<nsIStyleContext> listStyle;
          rv = aPresContext->ResolvePseudoStyleContextFor(aContent, 
                                                  nsHTMLAtoms::dropDownListPseudo, 
                                                  aStyleContext,
                                                  PR_FALSE,
                                                  getter_AddRefs(listStyle));

          // Initialize the scroll frame positioned. Note that it is NOT initialized as
          // absolutely positioned.
          nsIFrame* newFrame = nsnull;
          nsIFrame* scrolledFrame = nsnull;
          NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

          InitializeSelectFrame(aPresShell, aPresContext, aState, listFrame, scrolledFrame, aContent, comboboxFrame,
                               listStyle, PR_FALSE, PR_FALSE, PR_TRUE);

          newFrame = listFrame;
          // XXX Temporary for Bug 19416
          {
            nsIView * lstView;
            scrolledFrame->GetView(aPresContext, &lstView);
            if (lstView) {
              lstView->IgnoreSetPosition(PR_TRUE);
            }
          }

            // Set flag so the events go to the listFrame not child frames.
            // XXX: We should replace this with a real widget manager similar
            // to how the nsFormControlFrame works. Re-directing events is a temporary Kludge.
          nsIView *listView; 
          listFrame->GetView(aPresContext, &listView);
          NS_ASSERTION(nsnull != listView,"ListFrame's view is nsnull");
          nsIWidget * viewWidget;
          listView->GetWidget(viewWidget);
          //viewWidget->SetOverrideShow(PR_TRUE);
          NS_RELEASE(viewWidget);
          //listView->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

          nsIFrame* frame = nsnull;
          if (NS_SUCCEEDED(comboboxFrame->QueryInterface(kIFrameIID, (void**)&frame))) {
            nsFrameItems childItems;
            
              // Create display and button frames from the combobox'es anonymous content
            CreateAnonymousFrames(aPresShell, aPresContext, nsHTMLAtoms::combobox, aState, aContent, frame,
                                  childItems);
          
            frame->SetInitialChildList(aPresContext, nsnull,
                                   childItems.childList);

              // Initialize the additional popup child list which contains the dropdown list frame.
            nsFrameItems popupItems;
            popupItems.AddChild(listFrame);
            frame->SetInitialChildList(aPresContext, nsLayoutAtoms::popupList,
                                        popupItems.childList);
          }
           // Don't process, the children, They are already processed by the InitializeScrollFrame
           // call above.
          aProcessChildren = PR_FALSE;
          aNewFrame = comboboxFrame;
          aFrameHasBeenInitialized = PR_TRUE;
        }
#endif
      } else {
#ifdef NEWGFX_LIST
        ///////////////////////////////////////////////////////////////////
        // ListBox - New GFX Implementation
        ///////////////////////////////////////////////////////////////////
    // Construct a frame-based list box 
        nsIFrame * listFrame; 
        rv = NS_NewGfxListControlFrame(aPresShell, &listFrame);

        // initialize the list control 
        InitAndRestoreFrame(aPresContext, aState, aContent, 
                            aParentFrame, aStyleContext, nsnull, listFrame);

        // create the area frame we are scrolling 
        PRUint32 flags = NS_BLOCK_SHRINK_WRAP | (aIsAbsolutelyPositioned?NS_BLOCK_SPACE_MGR:0);
        nsIFrame* scrolledFrame = nsnull;
        NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

        // resolve a style for our gfx scrollframe based on the list frames style 
        nsCOMPtr<nsIStyleContext> scrollFrameStyle;
        aPresContext->ResolvePseudoStyleContextFor(aContent, 
                                          nsLayoutAtoms::selectScrolledContentPseudo,
                                          aStyleContext, PR_FALSE, 
                                          getter_AddRefs(scrollFrameStyle)); 
        
       InitAndRestoreFrame(aPresContext, aState, aContent, 
                            listFrame, scrollFrameStyle, nsnull, scrolledFrame);

        // this is what is returned when the scrollframe is created. 
        // newScrollFrame - Either a gfx scrollframe or a native scrollframe that was created 
        // scrolledFrameStyleContext - The resolved style context of the scrolledframe you passed in. 
        // this is not the style of the scrollFrame. 

        nsIFrame* newScrollFrame = nsnull; 

        /* scroll frame */

#ifdef NEWGFX_LIST_SCROLLFRAME
        nsIStyleContext * scrolledFrameStyleContext = nsnull; 

        // ok take the style context, the Select area frame to scroll,the listFrame, and its parent 
        // and build the scrollframe. 
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, scrollFrameStyle, scrolledFrame, 
                         listFrame, newScrollFrame, scrolledFrameStyleContext); 
#else
        newScrollFrame = scrolledFrame;
#endif

        // The area frame is a floater container 
        PRBool haveFirstLetterStyle, haveFirstLineStyle; 
        HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext, 
                              &haveFirstLetterStyle, &haveFirstLineStyle); 
        nsFrameConstructorSaveState floaterSaveState; 
        aState.PushFloaterContainingBlock(scrolledFrame, floaterSaveState, 
                                          haveFirstLetterStyle, 
                                          haveFirstLineStyle); 

        // Process children 
        nsFrameItems childItems; 

        ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_FALSE, 
                        childItems, PR_TRUE); 

        // if a select is being created with zero options we need to create 
        // a special pseudo frame so it can be sized as best it can 
        nsCOMPtr<nsIDOMHTMLSelectElement> selectElement; 
        nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement), 
                                                     (void**)getter_AddRefs(selectElement)); 
        if (NS_SUCCEEDED(result) && selectElement) { 
          PRUint32 numOptions = 0; 
          result = selectElement->GetLength(&numOptions); 
          if (NS_SUCCEEDED(result) && 0 == numOptions) { 
            nsCOMPtr<nsIStyleContext> styleContext;
            nsIFrame*         generatedFrame = nsnull; 
            scrolledFrame->GetStyleContext(getter_AddRefs(styleContext)); 
            if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, scrolledFrame, aContent, 
                                            styleContext, nsLayoutAtoms::dummyOptionPseudo, 
                                            PR_FALSE, &generatedFrame)) { 
              // Add the generated frame to the child list 
              childItems.AddChild(generatedFrame); 
            } 
          } 
        } 

        // Set the scrolled frame's initial child lists 
        scrolledFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList); 

        // set the floated list 
        if (aState.mFloatedItems.childList) { 
          scrolledFrame->SetInitialChildList(aPresContext, 
                                             nsLayoutAtoms::floaterList, 
                                             aState.mFloatedItems.childList); 
        } 

        // now we need to set the initial child list of the listFrame and this of course will be the gfx scrollframe 
        listFrame->SetInitialChildList(aPresContext, nsnull, newScrollFrame); 

        // our new frame retured is the top frame which is the list frame. 
        aNewFrame = listFrame; 

        // yes we have already initialized our frame 
        aFrameHasBeenInitialized = PR_TRUE; 
#else
        ///////////////////////////////////////////////////////////////////
        // ListBox - Old Native Implementation
        ///////////////////////////////////////////////////////////////////
        nsIFrame * listFrame;
        rv = NS_NewListControlFrame(aPresShell, &listFrame);
        aNewFrame = listFrame;

        PRUint32 flags = NS_BLOCK_SHRINK_WRAP | (aIsAbsolutelyPositioned?NS_BLOCK_SPACE_MGR:0);
        nsIFrame* scrolledFrame = nsnull;
        NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

        // ******* this code stolen from Initialze ScrollFrame ********
        // please adjust this code to use BuildScrollFrame.

        InitializeSelectFrame(aPresShell, aPresContext, aState, listFrame, scrolledFrame, aContent, aParentFrame,
                              aStyleContext, aIsAbsolutelyPositioned, aIsFixedPositioned, PR_FALSE);

        aNewFrame = listFrame;

          // Set flag so the events go to the listFrame not child frames.
          // XXX: We should replace this with a real widget manager similar
          // to how the nsFormControlFrame works.
          // Re-directing events is a temporary Kludge.
        nsIView *listView; 
        listFrame->GetView(aPresContext, &listView);
        NS_ASSERTION(nsnull != listView,"ListFrame's view is nsnull");
        //listView->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);
        aFrameHasBeenInitialized = PR_TRUE;
#endif
      }
    } else {
      NS_ASSERTION(0, "We longer support native widgets");
    }
  }
  else {
    // Not frame based. Use a SelectFrame which creates a native widget.
    NS_ASSERTION(0, "We longer support native widgets");
  }

  return rv;

}

/**
 * Used to be InitializeScrollFrame but now its only used for the select tag
 * But the select tag should really be fixed to use GFX scrollbars that can
 * be create with BuildScrollFrame.
 */
nsresult
nsCSSFrameConstructor::InitializeSelectFrame(nsIPresShell*        aPresShell, 
                                             nsIPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIFrame*                scrollFrame,
                                             nsIFrame*                scrolledFrame,
                                             nsIContent*              aContent,
                                             nsIFrame*                aParentFrame,
                                             nsIStyleContext*         aStyleContext,
                                             PRBool                   aIsAbsolutelyPositioned,
                                             PRBool                   aIsFixedPositioned,
                                             PRBool                   aCreateBlock)
{
  // Initialize it
  nsIFrame* geometricParent = aParentFrame;
    
  if (aIsAbsolutelyPositioned) {
    geometricParent = aState.mAbsoluteItems.containingBlock;
  } else if (aIsFixedPositioned) {
    geometricParent = aState.mFixedItems.containingBlock;
  }
  
  nsCOMPtr<nsIStyleContext> scrollPseudoStyle;
  nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;

  
    aPresContext->ResolvePseudoStyleContextFor(aContent,
                                  nsLayoutAtoms::scrolledContentPseudo,
                                  aStyleContext, PR_FALSE,
                                  getter_AddRefs(scrolledPseudoStyle));

    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        geometricParent, aStyleContext, nsnull, scrollFrame);

    // Initialize the frame and force it to have a view
    // the scrolled frame is anonymous and does not have a content node
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        scrollFrame, scrolledPseudoStyle, nsnull, scrolledFrame);

    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, scrolledFrame,
                                           scrolledPseudoStyle, PR_TRUE);


    // The area frame is a floater container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floaterSaveState;
    aState.PushFloaterContainingBlock(scrolledFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;
    PRBool                      isPositionedContainingBlock = aIsAbsolutelyPositioned ||
                                                              aIsFixedPositioned;

    if (isPositionedContainingBlock) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(scrolledFrame, absoluteSaveState);
    }
     
    ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_FALSE,
                    childItems, PR_TRUE);

    // if a select is being created with zero options we need to create
    // a special pseudo frame so it can be sized as best it can
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
    nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                                 (void**)getter_AddRefs(selectElement));
    if (NS_SUCCEEDED(result) && selectElement) {
      PRUint32 numOptions = 0;
      result = selectElement->GetLength(&numOptions);
      if (NS_SUCCEEDED(result) && 0 == numOptions) { 
        nsCOMPtr<nsIStyleContext> styleContext;
        nsIFrame*         generatedFrame = nsnull; 
        scrolledFrame->GetStyleContext(getter_AddRefs(styleContext));
        if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, scrolledFrame, aContent, 
                                        styleContext, nsLayoutAtoms::dummyOptionPseudo, 
                                        PR_FALSE, &generatedFrame)) { 
          // Add the generated frame to the child list 
          childItems.AddChild(generatedFrame); 
        } 
      }
    } 
    //////////////////////////////////////////////////
    //////////////////////////////////////////////////
    
    // Set the scrolled frame's initial child lists
    scrolledFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (isPositionedContainingBlock && aState.mAbsoluteItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         aState.mAbsoluteItems.childList);
    }

    if (aState.mFloatedItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         aState.mFloatedItems.childList);
    }

  // Set the scroll frame's initial child list
  scrollFrame->SetInitialChildList(aPresContext, nsnull, scrolledFrame);
                                            
  return NS_OK;
}


                                             /**
 * Used to be InitializeScrollFrame but now its only used for the select tag
 * But the select tag should really be fixed to use GFX scrollbars that can
 * be create with BuildScrollFrame.
 */
nsresult
nsCSSFrameConstructor::ConstructTitledBoxFrame(nsIPresShell*        aPresShell, 
                                          nsIPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsIAtom*                 aTag,
                                          nsIStyleContext*         aStyleContext,
                                          nsIFrame*&               aNewFrame)
{
  nsIFrame * newFrame;
  nsresult rv = NS_NewTitledBoxFrame(aPresShell, &newFrame);
  if (!NS_SUCCEEDED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  // Initialize it
  nsIFrame* geometricParent = aParentFrame;
      
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      geometricParent, aStyleContext, nsnull, newFrame);

    // cache our display type
  const nsStyleDisplay* styleDisplay;
  newFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);

  nsIFrame * boxFrame;
  NS_NewTitledBoxInnerFrame(shell, &boxFrame);


  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsXULAtoms::titledboxContentPseudo,
                                             aStyleContext, PR_FALSE, &styleContext);
  InitAndRestoreFrame(aPresContext, aState, nsnull, 
                      newFrame, styleContext, nsnull, boxFrame);

  NS_RELEASE(styleContext);          
  
    nsFrameItems                childItems;

    ProcessChildren(aPresShell, aPresContext, aState, aContent, boxFrame, PR_FALSE,
                    childItems, PR_TRUE);

    static NS_DEFINE_IID(kTitleFrameCID, NS_TITLE_FRAME_CID);
    nsIFrame * child      = childItems.childList;
    nsIFrame * previous   = nsnull;
    nsIFrame* titleFrame = nsnull;
    while (nsnull != child) {
      nsresult result = child->QueryInterface(kTitleFrameCID, (void**)&titleFrame);
      if (NS_SUCCEEDED(result) && titleFrame) {
        if (nsnull != previous) {
          nsIFrame * nxt;
          titleFrame->GetNextSibling(&nxt);
          previous->SetNextSibling(nxt);
          titleFrame->SetNextSibling(boxFrame);
          break;
        } else {
          nsIFrame * nxt;
          titleFrame->GetNextSibling(&nxt);
          childItems.childList = nxt;
          titleFrame->SetNextSibling(boxFrame);
          break;
        }
      }
      previous = child;
      child->GetNextSibling(&child);
    }

    // Set the scrolled frame's initial child lists
    boxFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    
    if (titleFrame) 
        newFrame->SetInitialChildList(aPresContext, nsnull, titleFrame);
    else
        newFrame->SetInitialChildList(aPresContext, nsnull, boxFrame);

    // our new frame retured is the top frame which is the list frame. 
    aNewFrame = newFrame; 

  return NS_OK;
}


/**
 * Used to be InitializeScrollFrame but now its only used for the select tag
 * But the select tag should really be fixed to use GFX scrollbars that can
 * be create with BuildScrollFrame.
 */
nsresult
nsCSSFrameConstructor::ConstructFieldSetFrame(nsIPresShell*        aPresShell, 
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsIStyleContext*         aStyleContext,
                                            nsIFrame*&               aNewFrame,
                                            PRBool&                  aProcessChildren,
                                            PRBool                   aIsAbsolutelyPositioned,
                                            PRBool&                  aFrameHasBeenInitialized,
                                            PRBool                   aIsFixedPositioned)
{
  nsIFrame * newFrame;
  PRUint32 flags = aIsAbsolutelyPositioned ? NS_BLOCK_SPACE_MGR : 0;
  nsresult rv = NS_NewFieldSetFrame(aPresShell, &newFrame, flags);
  if (!NS_SUCCEEDED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  // Initialize it
  nsIFrame* geometricParent = aParentFrame;
    
  if (aIsAbsolutelyPositioned) {
    geometricParent = aState.mAbsoluteItems.containingBlock;
  } else if (aIsFixedPositioned) {
    geometricParent = aState.mFixedItems.containingBlock;
  }
  
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      geometricParent, aStyleContext, nsnull, newFrame);

  // See if we need to create a view, e.g. the frame is absolutely
  // positioned
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                           aStyleContext, PR_FALSE);

    // cache our display type
  const nsStyleDisplay* styleDisplay;
  newFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);

  /*
  PRUint32 childFlags = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay) ? NS_BLOCK_SHRINK_WRAP : 0;
  // inherit the FieldSet state 
  PRUint32 parentState;
  newFrame->GetFrameState( &parentState );
  childFlags |= parentState;
  childFlags |= NS_BLOCK_SHRINK_WRAP;
*/

  nsIFrame * areaFrame;
//  NS_NewAreaFrame(shell, &areaFrame, childFlags);


  NS_NewBlockFrame(shell, &areaFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::fieldsetContentPseudo,
                                             aStyleContext, PR_FALSE, &styleContext);
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      newFrame, styleContext, nsnull, areaFrame);

  NS_RELEASE(styleContext);          
  

    // The area frame is a floater container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floaterSaveState;
    aState.PushFloaterContainingBlock(areaFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;
    PRBool                      isPositionedContainingBlock = aIsAbsolutelyPositioned ||
                                                              aIsFixedPositioned;

    if (isPositionedContainingBlock) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(areaFrame, absoluteSaveState);
    }
     
    ProcessChildren(aPresShell, aPresContext, aState, aContent, areaFrame, PR_FALSE,
                    childItems, PR_TRUE);

    static NS_DEFINE_IID(kLegendFrameCID, NS_LEGEND_FRAME_CID);
    nsIFrame * child      = childItems.childList;
    nsIFrame * previous   = nsnull;
    nsIFrame* legendFrame = nsnull;
    while (nsnull != child) {
      nsresult result = child->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
      if (NS_SUCCEEDED(result) && legendFrame) {
        if (nsnull != previous) {
          nsIFrame * nxt;
          legendFrame->GetNextSibling(&nxt);
          previous->SetNextSibling(nxt);
          areaFrame->SetNextSibling(legendFrame);
          legendFrame->SetParent(newFrame);
          legendFrame->SetNextSibling(nsnull);
          break;
        } else {
          nsIFrame * nxt;
          legendFrame->GetNextSibling(&nxt);
          childItems.childList = nxt;
          areaFrame->SetNextSibling(legendFrame);
          legendFrame->SetParent(newFrame);
          legendFrame->SetNextSibling(nsnull);
          break;
        }
      }
      child->GetNextSibling(&child);
    }

    // Set the scrolled frame's initial child lists
    areaFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (isPositionedContainingBlock && aState.mAbsoluteItems.childList) {
      areaFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         aState.mAbsoluteItems.childList);
    }

    if (aState.mFloatedItems.childList) {
      areaFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         aState.mFloatedItems.childList);
    }

  // Set the scroll frame's initial child list
  newFrame->SetInitialChildList(aPresContext, nsnull, areaFrame);

  // our new frame retured is the top frame which is the list frame. 
  aNewFrame = newFrame; 

  // yes we have already initialized our frame 
  aFrameHasBeenInitialized = PR_TRUE; 

  return NS_OK;

#if 0

  nsIFrame* newChildList = aChildList;

  // Set the geometric and content parent for each of the child frames 
  // that will go into the area frame's child list.
  // The legend frame does not go into the list
  nsIFrame* lastNewFrame = nsnull;
  for (nsIFrame* frame = aChildList; nsnull != frame;) {
    nsIFrame* legendFrame = nsnull;
    nsresult result = frame->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
    if (NS_SUCCEEDED(result) && legendFrame) {
      if (mLegendFrame) { // we already have a legend, destroy it
        frame->GetNextSibling(&frame);
        if (lastNewFrame) {
          lastNewFrame->SetNextSibling(frame);
        } 
        else {
          aChildList = frame;
        }
        legendFrame->Destroy(aPresContext);
      } 
      else {
        nsIFrame* nextFrame;
        frame->GetNextSibling(&nextFrame);
        if (lastNewFrame) {
          lastNewFrame->SetNextSibling(nextFrame);
        } else {
          newChildList = nextFrame;
        }
        frame->SetParent(this);
        mFrames.FirstChild()->SetNextSibling(frame);
        mLegendFrame = frame;
        mLegendFrame->SetNextSibling(nsnull);
        frame = nextFrame;
      }
    } else {
      frame->SetParent(mFrames.FirstChild());
      lastNewFrame = frame;
      frame->GetNextSibling(&frame);
    }
  }
  // Queue up the frames for the content frame
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, newChildList);  
#endif
}

nsresult
nsCSSFrameConstructor::ConstructFrameByTag(nsIPresShell*        aPresShell, 
                                           nsIPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsIAtom*                 aTag,
                                           nsIStyleContext*         aStyleContext,
                                           nsFrameItems&            aFrameItems)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isFloating = PR_FALSE;
  PRBool    canBePositioned = PR_TRUE;
  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    isReplaced = PR_FALSE;
  PRBool    addToHashTable = PR_TRUE;
  nsresult  rv = NS_OK;

  if (nsLayoutAtoms::textTagName == aTag) {
    rv = NS_NewTextFrame(aPresShell, &newFrame);
    // Text frames don't go in the content->frame hash table, because
    // they're anonymous. This keeps the hash table smaller
    addToHashTable = PR_FALSE;
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
        rv = NS_NewImageFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::hr == aTag) {
        rv = NS_NewHRFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::br == aTag) {
        rv = NS_NewBRFrame(aPresShell, &newFrame);
        isReplaced = PR_TRUE;
        // BR frames don't go in the content->frame hash table: typically
        // there are many BR content objects and this would increase the size
        // of the hash table, and it's doubtful we need the mapping anyway
        addToHashTable = PR_FALSE;
      }
      else if (nsHTMLAtoms::wbr == aTag) {
        rv = NS_NewWBRFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::input == aTag) {
        isReplaced = PR_TRUE;
        rv = CreateInputFrame(aPresShell, aPresContext, aContent, newFrame, aStyleContext);
      }
      else if (nsHTMLAtoms::textarea == aTag) {
        isReplaced = PR_TRUE;
        rv = ConstructTextControlFrame(aPresShell, aPresContext, newFrame, aContent);
      }
      else if (nsHTMLAtoms::select == aTag) {
        isReplaced = PR_TRUE;
        rv = ConstructSelectFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                  aTag, aStyleContext, newFrame,  processChildren,
                                  isAbsolutelyPositioned, frameHasBeenInitialized,
                                  isFixedPositioned, aFrameItems);
      }
      else if (nsHTMLAtoms::applet == aTag) {
        isReplaced = PR_TRUE;
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::embed == aTag) {
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::fieldset == aTag) {
#define DO_NEWFIELDSET
#ifdef DO_NEWFIELDSET
        rv = ConstructFieldSetFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                    aTag, aStyleContext, newFrame,  processChildren,
                                    isAbsolutelyPositioned, frameHasBeenInitialized,
                                    isFixedPositioned);
        processChildren = PR_FALSE;
#else
        rv = NS_NewFieldSetFrame(aPresShell, &newFrame, isAbsolutelyPositioned ? NS_BLOCK_SPACE_MGR : 0);
        processChildren = PR_TRUE;
#endif
      }
      else if (nsHTMLAtoms::legend == aTag) {
        rv = NS_NewLegendFrame(aPresShell, &newFrame);
        processChildren = PR_TRUE;
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::object == aTag) {
        isReplaced = PR_TRUE;
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::form == aTag) {
        PRBool  isOutOfFlow = isFloating || isAbsolutelyPositioned || isFixedPositioned;

        rv = NS_NewFormFrame(aPresShell, &newFrame,
                             isOutOfFlow ? NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT : 0);
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::frameset == aTag) {
        rv = NS_NewHTMLFramesetFrame(aPresShell, &newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        isReplaced = PR_TRUE;
        rv = NS_NewHTMLFrameOuterFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        rv = NS_NewSpacerFrame(aPresShell, &newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::button == aTag) {
        rv = NS_NewHTMLButtonControlFrame(aPresShell, &newFrame);
        // the html4 button needs to act just like a 
        // regular button except contain html content
        // so it must be replaced or html outside it will
        // draw into its borders. -EDV
        isReplaced = PR_TRUE;
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::label == aTag) {
        rv = NS_NewLabelFrame(aPresShell, &newFrame);
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
       
      // Makes sure we use the correct parent frame pointer when initializing
      // the frame
      if (isFloating) {
        geometricParent = aState.mFloatedItems.containingBlock;

      } else if (canBePositioned) {
        if (isAbsolutelyPositioned) {
          geometricParent = aState.mAbsoluteItems.containingBlock;
        } else if (isFixedPositioned) {
          geometricParent = aState.mFixedItems.containingBlock;
        }
      }
      
      InitAndRestoreFrame(aPresContext, aState, aContent, 
                          geometricParent, aStyleContext, nsnull, newFrame);

      // See if we need to create a view, e.g. the frame is absolutely
      // positioned
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                             PR_TRUE, childItems, PR_FALSE);
      }

      // if there are any anonymous children create frames for them
      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                            childItems);


      // Set the frame's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    }

    // If the frame is positioned, then create a placeholder frame
    if (canBePositioned && (isAbsolutelyPositioned || isFixedPositioned)) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                                newFrame, aStyleContext, aParentFrame, &placeholderFrame);

      // Add the positioned frame to its containing block's list of
      // child frames
      if (isAbsolutelyPositioned) {
        aState.mAbsoluteItems.AddChild(newFrame);
      } else {
        aState.mFixedItems.AddChild(newFrame);
      }

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);

    } else if (isFloating) {
      nsIFrame* placeholderFrame;
      CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent, newFrame,
                                aStyleContext, aParentFrame, &placeholderFrame);

      // Add the floating frame to its containing block's list of child frames
      aState.mFloatedItems.AddChild(newFrame);

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);

    } else {
      // Add the newly constructed frame to the flow
      aFrameItems.AddChild(newFrame);
    }

    if (addToHashTable) {
      // Add a mapping from content object to primary frame. Note that for
      // floated and positioned frames this is the out-of-flow frame and not
      // the placeholder frame
      aState.mFrameManager->SetPrimaryFrameFor(aContent, newFrame);
    }
  }

  return rv;
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsIPresShell*        aPresShell, 
                                             nsIPresContext*          aPresContext,
                                             nsIAtom*                 aTag,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIFrame*                aNewFrame,
                                             nsFrameItems&            aChildItems)
{
  if (aTag == nsXULAtoms::treecell)
    return NS_OK; // Don't even allow the XBL check.  The inner cell frame throws it off.
                  // There's a separate special method for XBL treecells.

  nsCOMPtr<nsIStyleContext> styleContext;
  aNewFrame->GetStyleContext(getter_AddRefs(styleContext));

  const nsStyleUserInterface* ui= (const nsStyleUserInterface*)
      styleContext->GetStyleData(eStyleStruct_UserInterface);

  if (ui->mBehavior != "") {
    // Get the XBL loader.
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
    if (!xblService)
      return rv;

    // Load the bindings.
    xblService->LoadBindings(aParent, ui->mBehavior);
  
    // Retrieve the anonymous content that we should build.
    nsCOMPtr<nsISupportsArray> anonymousItems;
    nsCOMPtr<nsIContent> childElement;
    xblService->GetContentList(aParent, getter_AddRefs(anonymousItems), getter_AddRefs(childElement));
    
    if (!anonymousItems)
      return NS_OK;

    // Build the frames for the anonymous content.
    PRUint32 count = 0;
    anonymousItems->Count(&count);

    for (PRUint32 i=0; i < count; i++)
    {
      // get our child's content and set its parent to our content
      nsCOMPtr<nsISupports> node;
      anonymousItems->GetElementAt(i,getter_AddRefs(node));

      nsCOMPtr<nsIContent> content(do_QueryInterface(node));
      
      // create the frame and attach it to our frame
      ConstructFrame(aPresShell, aPresContext, aState, content, aNewFrame, aChildItems);
    }

    return NS_OK;
  }

   // only these tags types can have anonymous content. We do this check for performance
  // reasons. If we did a query interface on every tag it would be very inefficient.
  if (aTag !=  nsHTMLAtoms::input &&
      aTag !=  nsHTMLAtoms::combobox &&
      aTag !=  nsXULAtoms::slider &&
      aTag !=  nsXULAtoms::splitter &&
      aTag !=  nsXULAtoms::scrollbar &&
      aTag !=  nsXULAtoms::menu &&
      aTag !=  nsXULAtoms::menuitem
     ) {
     return NS_OK;

  }
  
  // get the document
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = aParent->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc)
    return rv;

  return CreateAnonymousFrames(aPresShell, aPresContext, aState, aParent, doc, aNewFrame, aChildItems);
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsIPresShell*        aPresShell, 
                                             nsIPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIDocument*             aDocument,
                                             nsIFrame*                aNewFrame,
                                             nsFrameItems&            aChildItems)
{

  nsCOMPtr<nsIAnonymousContentCreator> creator(do_QueryInterface(aNewFrame));
  
  if (!creator)
     return NS_OK;

  nsCOMPtr<nsISupportsArray> anonymousItems;
  NS_NewISupportsArray(getter_AddRefs(anonymousItems));


  creator->CreateAnonymousContent(aPresContext, *anonymousItems);
  
  PRUint32 count = 0;
  anonymousItems->Count(&count);

  for (PRUint32 i=0; i < count; i++)
  {
    // get our child's content and set its parent to our content
    nsCOMPtr<nsISupports> node;
    anonymousItems->GetElementAt(i,getter_AddRefs(node));

    nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    content->SetParent(aParent);
    content->SetDocument(aDocument, PR_TRUE);

    // create the frame and attach it to our frame
    ConstructFrame(aPresShell, aPresContext, aState, content, aNewFrame, aChildItems);
  }

  return NS_OK;
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousTreeCellFrames(nsIPresShell*        aPresShell, 
                                             nsIPresContext*  aPresContext,
                                             nsIAtom*                 aTag,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIFrame*                aNewFrame,
                                             nsIFrame*                aNewCellFrame,
                                             nsFrameItems&            aChildItems)
{
  nsCOMPtr<nsIStyleContext> styleContext;
  aNewCellFrame->GetStyleContext(getter_AddRefs(styleContext));

  const nsStyleUserInterface* ui= (const nsStyleUserInterface*)
      styleContext->GetStyleData(eStyleStruct_UserInterface);

  if (ui->mBehavior != "") {
    // Get the XBL loader.
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
    if (!xblService)
      return rv;

    // Load the bindings.
    xblService->LoadBindings(aParent, ui->mBehavior);
  
    // Retrieve the anonymous content that we should build.
    nsCOMPtr<nsIContent> childElement;
    nsCOMPtr<nsISupportsArray> anonymousItems;
    xblService->GetContentList(aParent, getter_AddRefs(anonymousItems), getter_AddRefs(childElement));
    
    if (!anonymousItems)
      return NS_OK;

    // Build the frames for the anonymous content.
    PRUint32 count = 0;
    anonymousItems->Count(&count);

    for (PRUint32 i=0; i < count; i++)
    {
      // get our child's content and set its parent to our content
      nsCOMPtr<nsISupports> node;
      anonymousItems->GetElementAt(i,getter_AddRefs(node));

      nsCOMPtr<nsIContent> content(do_QueryInterface(node));
      
      // create the frame and attach it to our frame
      ConstructFrame(aPresShell, aPresContext, aState, content, aNewFrame, aChildItems);
    }

    return NS_OK;
  }
  return NS_OK;
}

#ifdef INCLUDE_XUL
nsresult
nsCSSFrameConstructor::ConstructXULFrame(nsIPresShell*            aPresShell, 
                                         nsIPresContext*          aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         nsIStyleContext*         aStyleContext,
                                         nsFrameItems&            aFrameItems,
                                         PRBool                   aXBLBaseTag,
                                         PRBool&                  aHaltProcessing)
{ 
  PRBool    primaryFrameSet = PR_FALSE;
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isReplaced = PR_FALSE;
  PRBool    frameHasBeenInitialized = PR_FALSE;

  // this is the new frame that will be created
  nsIFrame* newFrame = nsnull;
  
  // this is the also the new frame that is created. But if a scroll frame is needed
  // the content will be mapped to the scrollframe and topFrame will point to it.
  // newFrame will still point to the child that we created like a "div" for example.
  nsIFrame* topFrame = nsnull;

  nsIFrame* ignore = nsnull; // I have no idea what this is used for.
  nsTreeCreator treeCreator(aPresShell); // Used to make tree views.

  NS_ASSERTION(aTag != nsnull, "null XUL tag");
  if (aTag == nsnull)
    return NS_OK;

  
  PRInt32 nameSpaceID;
  if (NS_SUCCEEDED(aContent->GetNameSpaceID(nameSpaceID)) &&
      nameSpaceID == nsXULAtoms::nameSpaceID) {
  
    // The following code allows the user to specify the base tag
    // of a XUL object using XBL.  XUL objects (like boxes, menus, etc.)
    // can then be extended arbitrarily.
    if (!aXBLBaseTag) {
      const nsStyleUserInterface* ui= (const nsStyleUserInterface*)
          aStyleContext->GetStyleData(eStyleStruct_UserInterface);

      // Ensure that our XBL bindings are installed.
      if (ui->mBehavior != "") {
        // Get the XBL loader.
        nsresult rv;
        NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
        if (!xblService)
          return rv;

        // Load the bindings.
        xblService->LoadBindings(aContent, ui->mBehavior);

        nsCOMPtr<nsIAtom> baseTag;
        xblService->GetBaseTag(aContent, getter_AddRefs(baseTag));
   
        if (baseTag) {
          // Construct the frame using the XBL base tag.
          return ConstructXULFrame( aPresShell, 
                                    aPresContext,
                                    aState,
                                    aContent,
                                    aParentFrame,
                                    baseTag,
                                    aStyleContext,
                                    aFrameItems,
                                    PR_TRUE,
                                    aHaltProcessing );
        }
      }
    }

    // See if the element is absolutely positioned
    const nsStylePosition* position = (const nsStylePosition*)
      aStyleContext->GetStyleData(eStyleStruct_Position);
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition)
      isAbsolutelyPositioned = PR_TRUE;

    // Create a frame based on the tag
    // box is first because it is created the most.
      // BOX CONSTRUCTION
    if (aTag == nsXULAtoms::box || aTag == nsXULAtoms::tabbox || 
        aTag == nsXULAtoms::tabpage || aTag == nsXULAtoms::tabcontrol ||
        aTag == nsXULAtoms::radiogroup) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewBoxFrame(aPresShell, &newFrame);

      const nsStyleDisplay* display = (const nsStyleDisplay*)
           aStyleContext->GetStyleData(eStyleStruct_Display);

      // Boxes can scroll.
      if (IsScrollable(aPresContext, display)) {

        // set the top to be the newly created scrollframe
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                         topFrame, aStyleContext);

        // we have a scrollframe so the parent becomes the scroll frame.
        newFrame->GetParent(&aParentFrame);

        primaryFrameSet = PR_TRUE;

        frameHasBeenInitialized = PR_TRUE;

      } 
    } // End of BOX CONSTRUCTION logic
    
    // BUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::button) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewXULButtonFrame(aPresShell, &newFrame);

      const nsStyleDisplay* display = (const nsStyleDisplay*)
           aStyleContext->GetStyleData(eStyleStruct_Display);

      // Boxes can scroll.
      if (IsScrollable(aPresContext, display)) {

        // set the top to be the newly created scrollframe
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                         topFrame, aStyleContext);

        // we have a scrollframe so the parent becomes the scroll frame.
        newFrame->GetParent(&aParentFrame);

        primaryFrameSet = PR_TRUE;

        frameHasBeenInitialized = PR_TRUE;

      } 
    } // End of BUTTON CONSTRUCTION logic

         // TITLED BUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::titledbutton ||
             aTag == nsXULAtoms::image) {

      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTitledButtonFrame(aPresShell, &newFrame);
    }
    // End of TITLED BUTTON CONSTRUCTION logic

    else if (aTag == nsXULAtoms::spring) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewSpringFrame(aPresShell, &newFrame);
    }
    // End of TITLED BUTTON CONSTRUCTION logic
    
    // TEXT CONSTRUCTION
    else if (aTag == nsXULAtoms::text) {
        processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewXULTextFrame(aPresShell, &newFrame);
    }
    // End of TEXT CONSTRUCTION logic

     // Menu Construction    
    else if (aTag == nsXULAtoms::menu ||
             aTag == nsXULAtoms::menuitem || 
             aTag == nsXULAtoms::menulist ||
             aTag == nsXULAtoms::menubutton) {
      // A derived class box frame
      // that has custom reflow to prevent menu children
      // from becoming part of the flow.
      processChildren = PR_TRUE; // Will need this to be custom.
      isReplaced = PR_TRUE;
      rv = NS_NewMenuFrame(aPresShell, &newFrame, (aTag != nsXULAtoms::menuitem));
    }
    else if (aTag == nsXULAtoms::menubar) {
#if defined(XP_MAC) || defined(RHAPSODY) // The Mac uses its native menu bar.
      aHaltProcessing = PR_TRUE;
      return NS_OK;
#else
      processChildren = PR_TRUE;
      rv = NS_NewMenuBarFrame(aPresShell, &newFrame);
#endif
    }
    else if (aTag == nsXULAtoms::popupset) {
      // This frame contains child popups
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewPopupSetFrame(aPresShell, &newFrame);
    }
    else if (aTag == nsXULAtoms::menupopup || aTag == nsXULAtoms::popup) {
      // This is its own frame that derives from
      // box.
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewMenuPopupFrame(aPresShell, &newFrame);
    }
    else if (aTag == nsXULAtoms::title) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTitleFrame(aPresShell, &newFrame);

      const nsStyleDisplay* display = (const nsStyleDisplay*)
           aStyleContext->GetStyleData(eStyleStruct_Display);

      // Boxes can scroll.
      if (IsScrollable(aPresContext, display)) {

        // set the top to be the newly created scrollframe
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                         topFrame, aStyleContext);

        // we have a scrollframe so the parent becomes the scroll frame.
        newFrame->GetParent(&aParentFrame);

        primaryFrameSet = PR_TRUE;

        frameHasBeenInitialized = PR_TRUE;

      } 
    } // End of BOX CONSTRUCTION logic

    else if (aTag == nsXULAtoms::titledbox) {

          ConstructTitledBoxFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aTag, aStyleContext, newFrame);
          processChildren = PR_FALSE;
          isReplaced = PR_TRUE;

          const nsStyleDisplay* display = (const nsStyleDisplay*)
               aStyleContext->GetStyleData(eStyleStruct_Display);

          // Boxes can scroll.
          if (IsScrollable(aPresContext, display)) {

            // set the top to be the newly created scrollframe
            BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                             topFrame, aStyleContext);

            // we have a scrollframe so the parent becomes the scroll frame.
            newFrame->GetParent(&aParentFrame);

            primaryFrameSet = PR_TRUE;

            frameHasBeenInitialized = PR_TRUE;
          }
    } 
    
    else if (aTag == nsXULAtoms::spinner)
      rv = NS_NewSpinnerFrame(aPresShell, &newFrame);
    else if (aTag == nsXULAtoms::colorpicker)
      rv = NS_NewColorPickerFrame(aPresShell, &newFrame);
    else if (aTag == nsXULAtoms::fontpicker)
      rv = NS_NewFontPickerFrame(aPresShell, &newFrame);
    else if (aTag == nsXULAtoms::iframe) {
      isReplaced = PR_TRUE;
      rv = NS_NewHTMLFrameOuterFrame(aPresShell, &newFrame);
    }
    else if (aTag == nsXULAtoms::editor) {
      isReplaced = PR_TRUE;
      rv = NS_NewHTMLFrameOuterFrame(aPresShell, &newFrame);
    }
    else if (aTag == nsXULAtoms::browser) {
      isReplaced = PR_TRUE;
      rv = NS_NewHTMLFrameOuterFrame(aPresShell, &newFrame);
    }
  
    // TREE CONSTRUCTION
    // The following code is used to construct a tree view from the XUL content
    // model.  
    else if (aTag == nsXULAtoms::treeitem ||
             aTag == nsXULAtoms::treechildren) {
      nsIFrame* newTopFrame;
      rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext,
                                    PR_TRUE, newTopFrame, newFrame, treeCreator, nsnull);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
    else if (aTag == nsXULAtoms::tree)
    {
      nsIFrame* geometricParent = aParentFrame;
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
        aParentFrame = aState.mAbsoluteItems.containingBlock;
      }
      if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
        aParentFrame = aState.mFixedItems.containingBlock;
      }
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, geometricParent, aStyleContext,
                               newFrame, treeCreator);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned || isFixedPositioned) {
        nsIFrame* placeholderFrame;

        CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                                  newFrame, aStyleContext, aParentFrame, &placeholderFrame);

        // Add the positioned frame to its containing block's list of child frames
        if (isAbsolutelyPositioned) {
          aState.mAbsoluteItems.AddChild(newFrame);
        } else {
          aState.mFixedItems.AddChild(newFrame);
        }

        // Add the placeholder frame to the flow
        aFrameItems.AddChild(placeholderFrame);
      
      } else {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      // Make sure we add a mapping in the content->frame hash table
      goto addToHashTable;
    }
    else if (aTag == nsXULAtoms::treerow)
    {
      // A tree item causes a table row to be constructed that is always
      // slaved to the nearest enclosing table row group (regardless of how
      // deeply nested it is within other tree items).
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                  newFrame, ignore, treeCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
    else if (aTag == nsXULAtoms::treecell)
    {
      // We could be in a hidden column.
      // XXX This is so disgusting.
      if (nsTreeCellFrame::ShouldBuildCell(aParentFrame, aContent)) {
        // We make a tree cell frame and process the children.
        nsIFrame* ignore2;
        rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                   newFrame, ignore, ignore2, treeCreator);
        aFrameItems.AddChild(newFrame);
      }
      else aHaltProcessing = PR_TRUE;
      return rv;
    }
    else if (aTag == nsXULAtoms::treeindentation)
    {
      rv = NS_NewTreeIndentationFrame(aPresShell, &newFrame);
    }
    // End of TREE CONSTRUCTION code here (there's more later on in the function)

    // TOOLBAR CONSTRUCTION
    else if (aTag == nsXULAtoms::toolbox) {
      processChildren = PR_TRUE;
      rv = NS_NewToolboxFrame(aPresShell, &newFrame);

      const nsStyleDisplay* display = (const nsStyleDisplay*)
      aStyleContext->GetStyleData(eStyleStruct_Display);

      if (IsScrollable(aPresContext, display)) {

        // build the scrollframe
        // set the top to be the newly created scrollframe
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                         topFrame, aStyleContext);

        // we have a scrollframe so the parent becomes the scroll frame.
        newFrame->GetParent(&aParentFrame);
        primaryFrameSet = PR_TRUE;

        frameHasBeenInitialized = PR_TRUE;

      }
    }
    else if (aTag == nsXULAtoms::toolbar) {

      processChildren = PR_TRUE;
      rv = NS_NewToolbarFrame(aPresShell, &newFrame);

      const nsStyleDisplay* display = (const nsStyleDisplay*)
      aStyleContext->GetStyleData(eStyleStruct_Display);

       if (IsScrollable(aPresContext, display)) {

        // set the top to be the newly created scrollframe
        BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, newFrame, aParentFrame,
                         topFrame, aStyleContext);

        // we have a scrollframe so the parent becomes the scroll frame.
        newFrame->GetParent(&aParentFrame);
        primaryFrameSet = PR_TRUE;

        frameHasBeenInitialized = PR_TRUE;

      }
    }
    else if (aTag == nsXULAtoms::toolbaritem) {
      processChildren = PR_TRUE;
      rv = NS_NewToolbarItemFrame(aPresShell, &newFrame);
    }
    // End of TOOLBAR CONSTRUCTION logic

    // PROGRESS METER CONSTRUCTION
    else if (aTag == nsXULAtoms::progressbar) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewProgressMeterFrame(aPresShell, &newFrame);
    }
    // End of PROGRESS METER CONSTRUCTION logic

    // XULCHECKBOX CONSTRUCTION
    else if (aTag == nsXULAtoms::checkbox ||
             aTag == nsXULAtoms::radio) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewXULCheckboxFrame(aPresShell, &newFrame);
    }
    // End of XULCHECKBOX CONSTRUCTION logic

    // STACK CONSTRUCTION
    else if (aTag == nsXULAtoms::stack) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewStackFrame(aPresShell, &newFrame);
    }
    // End of STACK CONSTRUCTION logic

    // DECK CONSTRUCTION
    else if (aTag == nsXULAtoms::deck || aTag == nsXULAtoms::tabpanel) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewDeckFrame(aPresShell, &newFrame);
    }
    // End of DECK CONSTRUCTION logic

    // TAB CONSTRUCTION
    else if (aTag == nsXULAtoms::tab) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTabFrame(aPresShell, &newFrame);
    }
    // End of TAB CONSTRUCTION logic

    // SLIDER CONSTRUCTION
    else if (aTag == nsXULAtoms::slider) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewSliderFrame(aPresShell, &newFrame);
    }
    // End of SLIDER CONSTRUCTION logic

    // SCROLLBAR CONSTRUCTION
    else if (aTag == nsXULAtoms::scrollbar) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewScrollbarFrame(aPresShell, &newFrame);

    }
    // End of SCROLLBAR CONSTRUCTION logic

    // SCROLLBUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::scrollbarbutton) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewScrollbarButtonFrame(aPresShell, &newFrame);
    }
    // End of SCROLLBUTTON CONSTRUCTION logic

    // SPLITTER CONSTRUCTION
    else if (aTag == nsXULAtoms::splitter) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewSplitterFrame(aPresShell, &newFrame);
    }
    // End of SPLITTER CONSTRUCTION logic

    // GRIPPY CONSTRUCTION
    else if (aTag == nsXULAtoms::grippy) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewGrippyFrame(aPresShell, &newFrame);
    }
    // End of GRIPPY CONSTRUCTION logic
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {

    // if no top frame was created then the top is the new frame
    if (topFrame == nsnull)
        topFrame = newFrame;

    // If the frame is a replaced element, then set the frame state bit
    if (isReplaced) {
      nsFrameState  state;
      newFrame->GetFrameState(&state);
      newFrame->SetFrameState(state | NS_FRAME_REPLACED_ELEMENT);
    }

    // xul does not support absolute positioning
    nsIFrame* geometricParent = aParentFrame;

    /*
    nsIFrame* geometricParent = isAbsolutelyPositioned
        ? aState.mAbsoluteItems.containingBlock
        : aParentFrame;
    */
    // if the new frame was already initialized to initialize it again.
    if (!frameHasBeenInitialized) {

      InitAndRestoreFrame(aPresContext, aState, aContent, 
                      geometricParent, aStyleContext, nsnull, newFrame);

      // See if we need to create a view, e.g. the frame is absolutely positioned
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               aStyleContext, PR_FALSE);
    }

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                             PR_FALSE, childItems, PR_FALSE);
      

      // if there are any anonymous children create frames for them
        nsCOMPtr<nsIAtom> tag(aTag);
        if (aXBLBaseTag) {
          aContent->GetTag(*getter_AddRefs(tag));
        }
          
        CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, newFrame,
                            childItems);

      // Set the frame's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
  
    }

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(topFrame);

    // If the frame is absolutely positioned, then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                                newFrame, aStyleContext, aParentFrame, &placeholderFrame);

      // Add the positioned frame to its containing block's list of child frames
      if (isAbsolutelyPositioned) {
        aState.mAbsoluteItems.AddChild(newFrame);
      } else {
        aState.mFixedItems.AddChild(newFrame);
      }

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
    }
  }

 addToHashTable:

  if (topFrame) {
    // the top frame is always what we map the content to. This is the frame that contains a pointer
    // to the content node.

    // Add a mapping from content object to primary frame. Note that for
    // floated and positioned frames this is the out-of-flow frame and not
    // the placeholder frame
    if (!primaryFrameSet)
        aState.mFrameManager->SetPrimaryFrameFor(aContent, topFrame);
  }

  return rv;
}
#endif


nsresult
nsCSSFrameConstructor::BeginBuildingScrollFrame(nsIPresShell* aPresShell, 
                                                nsIPresContext*         aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIStyleContext*         aContentStyle,
                                               nsIFrame*                aParentFrame,
                                               nsIAtom*                 aScrolledPseudo,
                                               nsIDocument*             aDocument,
                                               nsIFrame*&               aNewFrame,                                                                                             
                                               nsCOMPtr<nsIStyleContext>& aScrolledChildStyle,
                                               nsIFrame*&               aScrollableFrame)
{
  nsIFrame* scrollFrame = nsnull;
  nsIFrame* parentFrame = nsnull;
  nsIFrame* gfxScrollFrame = nsnull;

  nsFrameItems anonymousItems;

  nsCOMPtr<nsIStyleContext> contentStyle = dont_QueryInterface(aContentStyle);

  PRBool isGfx = HasGfxScrollbars(aPresContext);

  if (isGfx) {
  
    BuildGfxScrollFrame(aPresShell, aPresContext, aState, aContent, aDocument, aParentFrame,
                        contentStyle, gfxScrollFrame, anonymousItems);

    scrollFrame = anonymousItems.childList;
    parentFrame = gfxScrollFrame;
    aNewFrame = gfxScrollFrame;

    // we used the style that was passed in. So resolve another one.
    nsCOMPtr<nsIStyleContext> scrollPseudoStyle;
    aPresContext->ResolvePseudoStyleContextFor(aContent,
                                              nsLayoutAtoms::scrolledContentPseudo,
                                              contentStyle, PR_FALSE,
                                              getter_AddRefs(scrollPseudoStyle));

    contentStyle = scrollPseudoStyle;
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        parentFrame, contentStyle, nsnull, scrollFrame);
  } else {
    NS_NewScrollFrame(aPresShell, &scrollFrame);
    aNewFrame = scrollFrame;
    parentFrame = aParentFrame;
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        parentFrame, contentStyle, nsnull, scrollFrame);
  }

  // initialize the scrolled frame
  nsCOMPtr<nsIStyleContext> scrolledPseudoStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent,
                                          aScrolledPseudo,
                                          contentStyle, PR_FALSE,
                                          getter_AddRefs(scrolledPseudoStyle));


  aScrollableFrame = scrollFrame;
  
  // set the child frame for the gfxscrollbar if the is one. This frames will be the 
  // 2 scrollbars and the scrolled frame.
  if (gfxScrollFrame) {
     gfxScrollFrame->SetInitialChildList(aPresContext, nsnull, anonymousItems.childList);
  }


  aScrolledChildStyle = scrolledPseudoStyle;


  return NS_OK;
}

nsresult
nsCSSFrameConstructor::FinishBuildingScrollFrame(nsIPresContext*      aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIFrame*                aScrollFrame,
                                             nsIFrame*                aScrolledFrame,
                                             nsIStyleContext*         aScrolledContentStyle)
                                             
{
  // create a view
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aScrolledFrame,
                                           aScrolledContentStyle, PR_TRUE);

  // the the scroll frames child list
  aScrollFrame->SetInitialChildList(aPresContext, nsnull, aScrolledFrame);

  return NS_OK;
}
 

/**
 * Called to wrap a scrollframe or gfx scrollframe around a frame. The hierarchy will look like this
 *
 *  ------ for native scrollbars -----
 *
 *
 *            ScrollFrame
 *                 ^
 *                 |
 *               Frame (scrolled frame you passed in)
 *
 *
 * ------- for gfx scrollbars ------
 *
 *
 *            GfxScrollFrame
 *                 ^
 *                 |
 *              ScrollPort
 *                 ^
 *                 |
 *               Frame (scrolled frame you passed in)
 *
 *
 *-----------------------------------
 * LEGEND:
 * 
 * ScrollFrame: This is a frame that has a view that manages native scrollbars. It implements
 *              nsIScrollableView. It also manages clipping and scrolling of native widgets by 
 *              having a native scrolling window.
 *
 * GfxScrollFrame: This is a frame that manages gfx cross platform frame based scrollbars.
 *
 * ScrollPort: This is similar to the ScrollFrame above in that is clips and scrolls its children
 *             with a native scrolling window. But because it is contained in a GfxScrollFrame
 *             it does not have any code to do scrollbars so it is much simpler. Infact it only has
 *             1 view attached to it. Where the ScrollFrame above has 5! 
 *             
 *
 * @param aContent the content node of the child to wrap.
 * @param aScrolledFrame The frame of the content to wrap. This should not be
 *                    Initialized. This method will initialize it with a scrolled pseudo
 *                    and no nsIContent. The content will be attached to the scrollframe 
 *                    returned.
 * @param aContentStyle the style context that has already been resolved for the content being passed in.
 *
 * @param aParentFrame The parent to attach the scroll frame to
 *
 * @param aNewFrame The new scrollframe or gfx scrollframe that we create. It will contain the
 *                  scrolled frame you passed in. (returned)
 * @param aScrolledContentStyle the style that was resolved for the scrolled frame. (returned)
 */
nsresult
nsCSSFrameConstructor::BuildScrollFrame       (nsIPresShell* aPresShell, 
                                               nsIPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIStyleContext*         aContentStyle,
                                               nsIFrame*                aScrolledFrame,
                                               nsIFrame*                aParentFrame,
                                               nsIFrame*&               aNewFrame, 
                                               nsIStyleContext*&        aScrolledContentStyle)                                                                                                                                          
{
    nsIFrame *scrollFrame;
    nsCOMPtr<nsIDocument> document;
    aContent->GetDocument(*getter_AddRefs(document));
    nsCOMPtr<nsIStyleContext> scrolledContentStyle;

    
    BeginBuildingScrollFrame(aPresShell, aPresContext,
                     aState,
                     aContent,
                     aContentStyle,
                     aParentFrame,
                     nsLayoutAtoms::scrolledContentPseudo,
                     document,
                     aNewFrame,
                     scrolledContentStyle,
                     scrollFrame);
    
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        scrollFrame, scrolledContentStyle, nsnull, aScrolledFrame);

    FinishBuildingScrollFrame(aPresContext, 
                          aState,
                          aContent,
                          scrollFrame,
                          aScrolledFrame,
                          scrolledContentStyle);

    aScrolledContentStyle = scrolledContentStyle;

    // now set the primary frame to the ScrollFrame
    aState.mFrameManager->SetPrimaryFrameFor( aContent, aNewFrame );

    return NS_OK;

}

/** 
 * If we are building GFX scrollframes this will create one
 */
nsresult
nsCSSFrameConstructor::BuildGfxScrollFrame (nsIPresShell* aPresShell, 
                                            nsIPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIDocument*             aDocument,
                                             nsIFrame*                aParentFrame,
                                             nsIStyleContext*         aStyleContext,
                                             nsIFrame*&               aNewFrame,
                                             nsFrameItems&            aAnonymousFrames)
{
  NS_NewGfxScrollFrame(aPresShell, &aNewFrame,aDocument);

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  nsIFrame* scrollbox = nsnull;
  NS_NewScrollPortFrame(aPresShell, &scrollbox);


  aAnonymousFrames.AddChild(scrollbox);

  // if there are any anonymous children for the nsScrollFrame create frames for them.
  CreateAnonymousFrames(aPresShell, aPresContext, aState, aContent, aDocument, aNewFrame,
                        aAnonymousFrames);

  return NS_OK;
} 

nsresult
nsCSSFrameConstructor::ConstructFrameByDisplayType(nsIPresShell* aPresShell, 
                                                   nsIPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   const nsStyleDisplay*    aDisplay,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrame,
                                                   nsIStyleContext*         aStyleContext,
                                                   nsFrameItems&            aFrameItems)
{
  PRBool    primaryFrameSet = PR_FALSE;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isFloating = PR_FALSE;
  PRBool    isBlock = aDisplay->IsBlockLevel();
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsIFrame* newBlock = nsnull;
  nsIFrame* nextInline = nsnull;
  nsTableCreator tableCreator(aPresShell); // Used to make table frames.
  PRBool    addToHashTable = PR_TRUE;
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

     // Initialize it
    nsIFrame* geometricParent = aParentFrame;

    if (isAbsolutelyPositioned) {
      geometricParent = aState.mAbsoluteItems.containingBlock;
    } else if (isFixedPositioned) {
      geometricParent = aState.mFixedItems.containingBlock;
    }

    nsIFrame* scrolledFrame = nsnull;

    NS_NewAreaFrame(aPresShell, &scrolledFrame, NS_BLOCK_SPACE_MGR |
                    NS_BLOCK_SHRINK_WRAP | NS_BLOCK_MARGIN_ROOT);


    nsIStyleContext* newStyle = nsnull;
    // Build the scrollframe it
    BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, scrolledFrame, geometricParent,
                     newFrame, newStyle);

    // buildscrollframe sets the primary frame.
    primaryFrameSet = PR_TRUE;

    
    //-----

    // The area frame is a floater container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floaterSaveState;
    aState.PushFloaterContainingBlock(scrolledFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;
    PRBool                      isPositionedContainingBlock = isAbsolutelyPositioned ||
                                                              isFixedPositioned;

    if (isPositionedContainingBlock) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(scrolledFrame, absoluteSaveState);
    }
     
    ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_FALSE,
                    childItems, PR_TRUE);

      // Set the scrolled frame's initial child lists
    scrolledFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (isPositionedContainingBlock && aState.mAbsoluteItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         aState.mAbsoluteItems.childList);
    }

    if (aState.mFloatedItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::floaterList,
                                         aState.mFloatedItems.childList);
    }
    ///------
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

    // Create a frame to wrap up the absolute positioned item
    NS_NewAbsoluteItemWrapperFrame(aPresShell, &newFrame);
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                      (isAbsolutelyPositioned
                       ? aState.mAbsoluteItems.containingBlock
                       : aState.mFixedItems.containingBlock), 
                        aStyleContext, nsnull, newFrame);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. The area frame becomes a container for child
    // frames that are absolutely positioned
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floaterSaveState;
    nsFrameItems                childItems;

    PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
    if (aDisplay->IsBlockLevel()) {
      HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
    }
    aState.PushAbsoluteContainingBlock(newFrame, absoluteSaveState);
    aState.PushFloaterContainingBlock(newFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                    childItems, PR_TRUE);

    // Set the frame's initial child list(s)
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mAbsoluteItems.childList) {
      newFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                     aState.mAbsoluteItems.childList);
    }
    if (aState.mFloatedItems.childList) {
      newFrame->SetInitialChildList(aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    aState.mFloatedItems.childList);
    }
  }
  // See if the frame is floated, and it's a block or inline frame
  else if (isFloating &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {
    // Create an area frame
    NS_NewFloatingItemWrapperFrame(aPresShell, &newFrame);

    // Initialize the frame
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aState.mFloatedItems.containingBlock, 
                        aStyleContext, nsnull, newFrame);

    // See if we need to create a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content
    nsFrameConstructorSaveState floaterSaveState;
    nsFrameItems                childItems;

    PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
    if (aDisplay->IsBlockLevel()) {
      HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
    }
    aState.PushFloaterContainingBlock(newFrame, floaterSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                    PR_TRUE, childItems, PR_TRUE);

    // Set the frame's initial child list(s)
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mFloatedItems.childList) {
      newFrame->SetInitialChildList(aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    aState.mFloatedItems.childList);
    }
  }
  // See if it's relatively positioned
  else if ((NS_STYLE_POSITION_RELATIVE == position->mPosition) &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {
    // Is it block-level or inline-level?
    PRBool isBlockFrame = PR_FALSE;
    if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
      // Create a wrapper frame. No space manager, though
      NS_NewRelativeItemWrapperFrame(aPresShell, &newFrame);
      isBlockFrame = PR_TRUE;
    } else {
      // Create a positioned inline frame
      NS_NewPositionedInlineFrame(aPresShell, &newFrame);
    }

    // Initialize the frame    
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, newFrame);
    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Process the child content. Relatively positioned frames becomes a
    // container for child frames that are positioned
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floaterSaveState;
    nsFrameItems                childItems;

    aState.PushAbsoluteContainingBlock(newFrame, absoluteSaveState);
    
    if (isBlockFrame) {
      PRBool haveFirstLetterStyle, haveFirstLineStyle;
      HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
      aState.PushFloaterContainingBlock(newFrame, floaterSaveState,
                                        haveFirstLetterStyle,
                                        haveFirstLineStyle);
    }
    ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                    childItems, isBlockFrame);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mAbsoluteItems.childList) {
      newFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                    aState.mAbsoluteItems.childList);
    }
    if (isBlockFrame && aState.mFloatedItems.childList) {
      newFrame->SetInitialChildList(aPresContext,
                                    nsLayoutAtoms::floaterList,
                                    aState.mFloatedItems.childList);
    }
  }
  // See if it's a block frame of some sort
  else if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_RUN_IN == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_COMPACT == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay->mDisplay)) {
    // Create the block frame
    rv = NS_NewBlockFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      // That worked so construct the block and its children
      rv = ConstructBlock(aPresShell, aPresContext, aState, aDisplay, aContent,
                          aParentFrame, aStyleContext, newFrame);
    }
  }
  // See if it's an inline frame of some sort
  else if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_MARKER == aDisplay->mDisplay)) {
    // Create the inline frame
    rv = NS_NewInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      // That worked so construct the inline and its children
      rv = ConstructInline(aPresShell, aPresContext, aState, aDisplay, aContent,
                           aParentFrame, aStyleContext, newFrame,
                           &newBlock, &nextInline);
    }

    // To keep the hash table small don't add inline frames (they're
    // typically things like FONT and B), because we can quickly
    // find them if we need to
    addToHashTable = PR_FALSE;
  }
  // otherwise let the display property influence the frame type to create
  else {
    nsIFrame* ignore;

    // XXX This section now only handles table frames; should be
    // factored out probably

    // Use the 'display' property to choose a frame type
    switch (aDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_TABLE:
    {
      nsIFrame* geometricParent = aParentFrame;
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
        geometricParent = aState.mAbsoluteItems.containingBlock;
      }
      if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
        geometricParent = aState.mFixedItems.containingBlock;
      }
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, geometricParent,
                               aStyleContext, newFrame, tableCreator);
      // Note: table construction function takes care of initializing
      // the frame, processing children, and setting the initial child
      // list
      goto nearly_done;
    }
  
    // the next 5 cases are only relevant if the parent is not a table, ConstructTableFrame handles children
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
    {
      // aParentFrame may be an inner table frame rather than an outer frame 
      // In this case we need to get the outer frame.
      nsIFrame* parentFrame = aParentFrame;
      nsIFrame* outerFrame = nsnull;
      aParentFrame->GetParent(&outerFrame);
      nsCOMPtr<nsIAtom> frameType;
      if (outerFrame) {
        outerFrame->GetFrameType(getter_AddRefs(frameType));
        if (nsLayoutAtoms::tableOuterFrame == frameType.get()) {
          parentFrame = outerFrame;
        }
      }
      rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, aContent, parentFrame,
                                      aStyleContext, newFrame, ignore, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    {
      PRBool isRowGroup = (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != aDisplay->mDisplay);
      rv = ConstructTableGroupFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                    aStyleContext, isRowGroup, newFrame, ignore, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
    }
   
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      rv = ConstructTableColFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                  newFrame, ignore, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
  
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                  newFrame, ignore, tableCreator);
      aFrameItems.AddChild(newFrame);
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      {
        nsIFrame* ignore2;
        rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                     newFrame, ignore, ignore2, tableCreator);
        aFrameItems.AddChild(newFrame);
        return rv;
      }
  
    default:
      // Don't create any frame for content that's not displayed...
      break;
    }
  }

  // If the frame is absolutely positioned, then create a placeholder frame
 nearly_done:
  if (isAbsolutelyPositioned || isFixedPositioned) {
    nsIFrame* placeholderFrame;

    CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                              newFrame, aStyleContext, aParentFrame, &placeholderFrame);

    // Add the positioned frame to its containing block's list of child frames
    if (isAbsolutelyPositioned) {
      aState.mAbsoluteItems.AddChild(newFrame);
    } else {
      aState.mFixedItems.AddChild(newFrame);
    }

    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);

  } else if (isFloating) {
    nsIFrame* placeholderFrame;
    CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent, newFrame,
                              aStyleContext, aParentFrame, &placeholderFrame);

    // Add the floating frame to its containing block's list of child frames
    aState.mFloatedItems.AddChild(newFrame);

    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);
  } else if (nsnull != newFrame) {
    // Add the frame we just created to the flowed list
    aFrameItems.AddChild(newFrame);
    if (newBlock) {
      aFrameItems.AddChild(newBlock);
      if (nextInline) {
        aFrameItems.AddChild(nextInline);
      }
    }
  }

  if (newFrame && addToHashTable) {
    // Add a mapping from content object to primary frame. Note that for
    // floated and positioned frames this is the out-of-flow frame and not
    // the placeholder frame
    if (!primaryFrameSet)
      aState.mFrameManager->SetPrimaryFrameFor(aContent, newFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::GetAdjustedParentFrame(nsIPresContext* aPresContext,
                                              nsIFrame*       aCurrentParentFrame, 
                                              PRUint8         aChildDisplayType, 
                                              nsIFrame*&      aNewParentFrame)
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
        aCurrentParentFrame->FirstChild(aPresContext, nsnull, &innerTableFrame);
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
  switch (aDisplay->mOverflow) {
  	case NS_STYLE_OVERFLOW_SCROLL:
  	case NS_STYLE_OVERFLOW_AUTO:
  	case NS_STYLE_OVERFLOW_SCROLLBARS_NONE:
  	case NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL:
  	case NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL:
	    return PR_TRUE;
  }
  return PR_FALSE;
}


nsresult 
nsCSSFrameConstructor::InitAndRestoreFrame(nsIPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsIStyleContext*         aStyleContext,
                                           nsIFrame*                aPrevInFlow,
                                           nsIFrame*                aNewFrame)
{
  nsresult rv = NS_OK;
  
  NS_ASSERTION(aNewFrame, "Null frame cannot be initialized");

  // Initialize the frame
  rv = aNewFrame->Init(aPresContext, aContent, aParentFrame, 
                       aStyleContext, aPrevInFlow);

  if (aState.mFrameState && aState.mFrameManager) {
    aState.mFrameManager->RestoreFrameState(aPresContext, aNewFrame, aState.mFrameState);
  }

  return rv;
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

// MathML Mod - RBS
#ifdef MOZ_MATHML
nsresult
nsCSSFrameConstructor::ConstructMathMLFrame(nsIPresShell*            aPresShell,
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsIStyleContext*         aStyleContext,
                                            nsFrameItems&            aFrameItems)
{
  PRBool    processChildren = PR_TRUE;  // Whether we should process child content.
                                        // MathML frames are inline frames.
                                        // processChildren = PR_TRUE for inline frames.
                                        // see case NS_STYLE_DISPLAY_INLINE in
                                        // ConstructFrameByDisplayType()
  
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isReplaced = PR_FALSE;

  NS_ASSERTION(aTag != nsnull, "null MathML tag");
  if (aTag == nsnull)
    return NS_OK;

  // Make sure that we remain confined in the MathML world
  PRInt32 nameSpaceID;
  rv = aContent->GetNameSpaceID(nameSpaceID);
  if (NS_FAILED(rv) || nameSpaceID != nsMathMLAtoms::nameSpaceID) 
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
  nsIFrame* ignore = nsnull;
  nsMathMLmtableCreator mathTableCreator(aPresShell); // Used to make table views.
 
  // See if the element is absolute or fixed positioned
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  }
  else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
    isFixedPositioned = PR_TRUE;
  }

  if (aTag == nsMathMLAtoms::mi_)
     rv = NS_NewMathMLmiFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mn_)
     rv = NS_NewMathMLmnFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mo_)
     rv = NS_NewMathMLmoFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mfrac_)
     rv = NS_NewMathMLmfracFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::msup_)
     rv = NS_NewMathMLmsupFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::msub_)
     rv = NS_NewMathMLmsubFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::msubsup_)
     rv = NS_NewMathMLmsubsupFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::munder_)
     rv = NS_NewMathMLmunderFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mover_)
     rv = NS_NewMathMLmoverFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::munderover_)
     rv = NS_NewMathMLmunderoverFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mphantom_)
     rv = NS_NewMathMLmphantomFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mpadded_)
     rv = NS_NewMathMLmpaddedFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mspace_)
     rv = NS_NewMathMLmspaceFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::ms_)
     rv = NS_NewMathMLmsFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mfenced_)
     rv = NS_NewMathMLmfencedFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mmultiscripts_)
     rv = NS_NewMathMLmmultiscriptsFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mstyle_)
     rv = NS_NewMathMLmstyleFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::msqrt_)
     rv = NS_NewMathMLmsqrtFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mroot_)
     rv = NS_NewMathMLmrootFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::maction_)
     rv = NS_NewMathMLmactionFrame(aPresShell, &newFrame);
  else if (aTag == nsMathMLAtoms::mrow_   ||
           aTag == nsMathMLAtoms::mtext_  ||
           aTag == nsMathMLAtoms::merror_ ||
           aTag == nsMathMLAtoms::none_   ||
           aTag == nsMathMLAtoms::mprescripts_ )
     rv = NS_NewMathMLmrowFrame(aPresShell, &newFrame);
  // CONSTRUCTION of MTABLE elements
  else if (aTag == nsMathMLAtoms::mtable_)  {
  // <mtable> is an inline-table, for the moment, we just do what  
  // <table> does, and wait until nsLineLayout::TreatFrameAsBlock
  // can handle NS_STYLE_DISPLAY_INLINE_TABLE.
      nsIFrame* geometricParent = aParentFrame;
      if (isAbsolutelyPositioned) {
        aParentFrame = aState.mAbsoluteItems.containingBlock;
      }
      else if (isFixedPositioned) {
        aParentFrame = aState.mFixedItems.containingBlock;
      }            
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, geometricParent, aStyleContext,
                               newFrame, mathTableCreator);
      // Note: table construction function takes care of initializing the frame,
      // processing children, and setting the initial child list
      if (isAbsolutelyPositioned || isFixedPositioned) {
        nsIFrame* placeholderFrame;
        CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent, newFrame,
                                  aStyleContext, aParentFrame, &placeholderFrame);
        // Add the positioned frame to its containing block's list of child frames
        if (isAbsolutelyPositioned) {
          aState.mAbsoluteItems.AddChild(newFrame);
        } else {
          aState.mFixedItems.AddChild(newFrame);
        }
        // Add the placeholder frame to the flow
        aFrameItems.AddChild(placeholderFrame);
      } else {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      return rv; 
  }
  else if (aTag == nsMathMLAtoms::mtd_) {
    nsIFrame* ignore2;
    rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aStyleContext, 
                                 newFrame, ignore, ignore2, mathTableCreator);
    aFrameItems.AddChild(newFrame);
    return rv;
  }
  // End CONSTRUCTION of MTABLE elements 
  
  else {
     return rv;
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
                              ? aState.mAbsoluteItems.containingBlock
                              : aParentFrame;
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        geometricParent, aStyleContext, nsnull, newFrame);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(newFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren) {
      rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                           childItems, PR_FALSE);
    }

    // Set the frame's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
  
    // If the frame is absolutely positioned then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent, newFrame, 
                                aStyleContext, aParentFrame, &placeholderFrame);

      // Add the positioned frame to its containing block's list of child frames
      if (isAbsolutelyPositioned) {
        aState.mAbsoluteItems.AddChild(newFrame);
      } else {
        aState.mFixedItems.AddChild(newFrame);
      }

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
    }
  }
  return rv;
}
#endif // MOZ_MATHML

nsresult
nsCSSFrameConstructor::ConstructFrame(nsIPresShell*        aPresShell, 
                                      nsIPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsFrameItems&            aFrameItems)

{
  NS_PRECONDITION(nsnull != aParentFrame, "no parent frame");

  nsresult  rv;

  // Get the element's tag
  nsCOMPtr<nsIAtom>  tag;
  aContent->GetTag(*getter_AddRefs(tag));

  nsCOMPtr<nsIStyleContext> styleContext;
  rv = ResolveStyleContext(aPresContext, aParentFrame, aContent, tag, getter_AddRefs(styleContext));

  if (NS_SUCCEEDED(rv)) {
    // Pre-check for display "none" - if we find that, don't create
    // any frame at all
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
      aState.mFrameManager->SetUndisplayedContent(aContent, styleContext);
    }
    else {
      nsIFrame* lastChild = aFrameItems.lastChild;

      // Handle specific frame types
      rv = ConstructFrameByTag(aPresShell, aPresContext, aState, aContent, aParentFrame,
                               tag, styleContext, aFrameItems);

#ifdef INCLUDE_XUL
      // Failing to find a matching HTML frame, try creating a specialized
      // XUL frame. This is temporary, pending planned factoring of this
      // whole process into separate, pluggable steps.
      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                             (lastChild == aFrameItems.lastChild))) {
        PRBool haltProcessing = PR_FALSE;
        rv = ConstructXULFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                               tag, styleContext, aFrameItems, PR_FALSE, haltProcessing);
        if (haltProcessing) {
          return rv;
        }
      } 
#endif

// MathML Mod - RBS
#ifdef MOZ_MATHML
      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                               (lastChild == aFrameItems.lastChild))) {
        rv = ConstructMathMLFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                  tag, styleContext, aFrameItems);
      }
#endif

      if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                               (lastChild == aFrameItems.lastChild))) {
        // When there is no explicit frame to create, assume it's a
        // container and let display style dictate the rest
        rv = ConstructFrameByDisplayType(aPresShell, aPresContext, aState, display, aContent,
                                         aParentFrame, styleContext, aFrameItems);
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ReconstructDocElementHierarchy(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  if (nsnull != mDocument) {
    nsCOMPtr<nsIContent> rootContent(dont_AddRef(mDocument->GetRootContent()));
    
    if (rootContent) {
      // Before removing the frames associated with the content object, ask them to save their
      // state onto a temporary state object.
      CaptureStateForFramesOf(aPresContext, rootContent, mTempFrameTreeState);

      nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                    nsnull, nsnull, mTempFrameTreeState);
      nsIFrame* docElementFrame;
        
      // Get the frame that corresponds to the document element
      state.mFrameManager->GetPrimaryFrameFor(rootContent, &docElementFrame);

      // Clear the hash tables that map from content to frame and out-of-flow
      // frame to placeholder frame
      state.mFrameManager->ClearPrimaryFrameMap();
      state.mFrameManager->ClearPlaceholderFrameMap();
      state.mFrameManager->ClearUndisplayedContentMap();

      if (docElementFrame) {
        nsIFrame* docParentFrame;
        docElementFrame->GetParent(&docParentFrame);

        if (docParentFrame) {
          // Remove the old document element hieararchy
          rv = state.mFrameManager->RemoveFrame(aPresContext, *shell,
                                                docParentFrame, nsnull, 
                                                docElementFrame);
          // XXX Remove any existing fixed items...          
          if (NS_SUCCEEDED(rv)) {
            // Create the new document element hierarchy
            nsIFrame*                 newChild;
            nsCOMPtr<nsIStyleContext> rootPseudoStyle;
          
            docParentFrame->GetStyleContext(getter_AddRefs(rootPseudoStyle));
            rv = ConstructDocElementFrame(shell, aPresContext, state, rootContent,
                                          docParentFrame, rootPseudoStyle,
                                          newChild);

            if (NS_SUCCEEDED(rv)) {
              rv = state.mFrameManager->InsertFrames(aPresContext, *shell,
                                                     docParentFrame, nsnull,
                                                     nsnull, newChild);

              // Tell the fixed containing block about its 'fixed' frames
              if (state.mFixedItems.childList) {
                state.mFrameManager->InsertFrames(aPresContext, *shell,
                                       mFixedContainingBlock, nsLayoutAtoms::fixedList,
                                       nsnull, state.mFixedItems.childList);
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
nsCSSFrameConstructor::GetFrameFor(nsIPresShell*    aPresShell,
                                   nsIPresContext*  aPresContext,
                                   nsIContent*      aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame;
  aPresShell->GetPrimaryFrameFor(aContent, &frame);

  if (nsnull != frame) {
    // Check to see if the content is a select and 
    // then if it has a drop down (thus making it a combobox)
    // The drop down is a ListControlFrame derived from a 
    // nsScrollFrame then get the area frame and that will be the parent
    // What is unclear here, is if any of this fails, should it return
    // the nsComboboxControlFrame or null?
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
    nsresult res = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                                 (void**)getter_AddRefs(selectElement));
    if (NS_SUCCEEDED(res) && selectElement) {
      nsIComboboxControlFrame * comboboxFrame;
      res = frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame),
                                               (void**)&comboboxFrame);
      nsIFrame * listFrame;
      if (NS_SUCCEEDED(res) && comboboxFrame) {
        comboboxFrame->GetDropDown(&listFrame);
        if (nsnull != listFrame) {
          listFrame->FirstChild(aPresContext, nsnull, &frame);
        }
      } else {
        res = frame->QueryInterface(NS_GET_IID(nsIListControlFrame),
                                                 (void**)&listFrame);
        if (NS_SUCCEEDED(res) && listFrame) {
          frame->FirstChild(aPresContext, nsnull, &frame);
        } 
      }
    } else {
      // If the primary frame is a scroll frame, then get the scrolled frame.
      // That's the frame that gets the reflow command
      const nsStyleDisplay* display;
      frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

      // If the primary frame supports IScrollableFrame, then get the scrolled frame.
      // That's the frame that gets the reflow command                          
      nsIScrollableFrame *pScrollableFrame = nsnull;                            
      if (NS_SUCCEEDED( frame->QueryInterface(nsIScrollableFrame::GetIID(),     
                                              (void **)&pScrollableFrame) ))    
      {                                                                         
        pScrollableFrame->GetScrolledFrame( aPresContext, frame );              
      } 

      // if we get an outer table frame use its 1st child which is a table inner frame
      // if we get a table cell frame   use its 1st child which is an area frame
      else if ((NS_STYLE_DISPLAY_TABLE      == display->mDisplay) ||
               (NS_STYLE_DISPLAY_TABLE_CELL == display->mDisplay)) {
        frame->FirstChild(aPresContext, nsnull, &frame);                   
      }
    }
  }

  return frame;
}

nsIFrame*
nsCSSFrameConstructor::GetAbsoluteContainingBlock(nsIPresContext* aPresContext,
                                                  nsIFrame*       aFrame)
{
  NS_PRECONDITION(nsnull != mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is absolutely positioned or
  // relatively positioned
  nsIFrame* containingBlock = nsnull;
  for (nsIFrame* frame = aFrame; frame; frame->GetParent(&frame)) {
    const nsStylePosition* position;

    // Is it positioned?
    frame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if ((position->mPosition == NS_STYLE_POSITION_ABSOLUTE) ||
        (position->mPosition == NS_STYLE_POSITION_RELATIVE)) {
      const nsStyleDisplay* display;
      
      // If it's a table then ignore it, because for the time being tables
      // are not containers for absolutely positioned child frames
      frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      if (display->mDisplay != NS_STYLE_DISPLAY_TABLE) {
        nsIAtom*  frameType;
        frame->GetFrameType(&frameType);

        if (nsLayoutAtoms::scrollFrame == frameType) {
          // We want the scrolled frame, not the scroll frame
          nsIFrame* scrolledFrame;
          frame->FirstChild(aPresContext, nsnull, &scrolledFrame);
          NS_RELEASE(frameType);
          if (scrolledFrame) {
            scrolledFrame->GetFrameType(&frameType);
            if (nsLayoutAtoms::areaFrame == frameType) {
              containingBlock = scrolledFrame;
            }
          }

        } else if ((nsLayoutAtoms::areaFrame == frameType) ||
                   (nsLayoutAtoms::positionedInlineFrame == frameType)) {
          containingBlock = frame;
        }
        NS_RELEASE(frameType);
      }
    }

    // See if we found a containing block
    if (containingBlock) {
      break;
    }
  }

  // If we didn't find an absolutely positioned containing block, then use the
  // initial containing block
  if (!containingBlock) {
    containingBlock = mInitialContainingBlock;
  }
  
  return containingBlock;
}

nsIFrame*
nsCSSFrameConstructor::GetFloaterContainingBlock(nsIPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  NS_PRECONDITION(mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is a real block frame,
  // or a floated inline or absolutely positioned inline frame
  nsIFrame* containingBlock = aFrame;
  while (nsnull != containingBlock) {
    const nsStyleDisplay* display;
    containingBlock->GetStyleData(eStyleStruct_Display,
                                  (const nsStyleStruct*&)display);
    if ((NS_STYLE_DISPLAY_BLOCK == display->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay)) {
      break;
    }
    else if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
      const nsStylePosition* position;
      containingBlock->GetStyleData(eStyleStruct_Position,
                                    (const nsStyleStruct*&)position);

      if ((NS_STYLE_FLOAT_NONE != display->mFloats) ||
          (position->IsAbsolutelyPositioned())) {
        if (NS_STYLE_FLOAT_NONE != display->mFloats) {
          nsCOMPtr<nsIAtom> frameType;
          containingBlock->GetFrameType(getter_AddRefs(frameType));
          if (nsLayoutAtoms::letterFrame != frameType.get()) {
            break;
          }
        }
        else {
          break;
        }
      }
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

// Helper function to determine whether a given frame is generated content
// for the specified content object. Returns PR_TRUE if the frame is associated
// with generated content and PR_FALSE otherwise
static inline PRBool
IsGeneratedContentFor(nsIContent* aContent, nsIFrame* aFrame, nsIAtom* aPseudoElement)
{
  NS_PRECONDITION(aFrame, "null frame pointer");
  nsFrameState  state;
  PRBool        result = PR_FALSE;

  // First check the frame state bit
  aFrame->GetFrameState(&state);
  if (state & NS_FRAME_GENERATED_CONTENT) {
    nsIContent* content;

    // Check that it has the same content pointer
    aFrame->GetContent(&content);
    if (content == aContent) {
      nsIStyleContext* styleContext;
      nsIAtom*         pseudoType;

      // See if the pseudo element type matches
      aFrame->GetStyleContext(&styleContext);
      styleContext->GetPseudoType(pseudoType);
      result = (pseudoType == aPseudoElement);
      NS_RELEASE(styleContext);
      NS_IF_RELEASE(pseudoType);
    }
    NS_IF_RELEASE(content);
  }

  return result;
}

// This function is called by ContentAppended() and ContentInserted() when
// appending flowed frames to a parent's principal child list. It handles the
// case where the parent frame has :after pseudo-element generated content
nsresult
nsCSSFrameConstructor::AppendFrames(nsIPresContext*  aPresContext,
                                    nsIPresShell*    aPresShell,
                                    nsIFrameManager* aFrameManager,
                                    nsIContent*      aContainer,
                                    nsIFrame*        aParentFrame,
                                    nsIFrame*        aFrameList)
{
  nsIFrame* firstChild;
  aParentFrame->FirstChild(aPresContext, nsnull, &firstChild);
  nsFrameList frames(firstChild);
  nsIFrame* lastChild = frames.LastChild();

  // See if the parent has an :after pseudo-element
  if (lastChild && IsGeneratedContentFor(aContainer, lastChild, nsCSSAtoms::afterPseudo)) {
    // Insert the frames before the :after pseudo-element
    return aFrameManager->InsertFrames(aPresContext, *aPresShell, aParentFrame,
                                       nsnull, frames.GetPrevSiblingFor(lastChild),
                                       aFrameList);
  }

  nsresult rv = NS_OK;

  // a col group or col appended to a table may result in an insert rather than an append
  nsIAtom* parentType;
  aParentFrame->GetFrameType(&parentType);
  if (nsLayoutAtoms::tableFrame == parentType) { 
    nsTableFrame* tableFrame = (nsTableFrame *)aParentFrame;
    nsIAtom* childType;
    aFrameList->GetFrameType(&childType);
    if (nsLayoutAtoms::tableColFrame == childType) {
      // table column
      nsIFrame* parentFrame = aParentFrame;
      aFrameList->GetParent(&parentFrame);
      NS_RELEASE(childType);
      rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, parentFrame,
                                       nsLayoutAtoms::colGroupList, aFrameList);
    }
    else if (nsLayoutAtoms::tableColGroupFrame == childType) {
      // table col group
      nsIFrame* prevSibling;
      PRBool doAppend = nsTableColGroupFrame::GetLastRealColGroup(tableFrame, &prevSibling);
      if (doAppend) {
        rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, aParentFrame,
                                         nsLayoutAtoms::colGroupList, aFrameList);
      }
      else {
        rv = aFrameManager->InsertFrames(aPresContext, *aPresShell, aParentFrame, 
                                         nsLayoutAtoms::colGroupList, prevSibling, aFrameList);
      }
    }
    else if (nsLayoutAtoms::tableCaptionFrame == childType) {
      // table caption
      rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, aParentFrame,
                                       nsLayoutAtoms::captionList, aFrameList);
    }
    else {
      rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, aParentFrame,
                                       nsnull, aFrameList);
    }
    NS_IF_RELEASE(childType);
  }
  else {
    // Append the frames to the end of the parent's child list
    rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, aParentFrame,
                                     GetChildListFor(aFrameList), aFrameList);
  }
  NS_IF_RELEASE(parentType);

  return rv;
}

static nsIFrame*
FindPreviousSibling(nsIPresShell* aPresShell,
                    nsIContent*   aContainer,
                    PRInt32       aIndexInContainer)
{
  nsIFrame* prevSibling = nsnull;

  // Note: not all content objects are associated with a frame (e.g., if their
  // 'display' type is 'hidden') so keep looking until we find a previous frame
  for (PRInt32 i = aIndexInContainer - 1; i >= 0; i--) {
    nsCOMPtr<nsIContent> precedingContent;

    aContainer->ChildAt(i, *getter_AddRefs(precedingContent));
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
  for (PRInt32 i = aIndexInContainer + 1; i < count; i++) {
    nsCOMPtr<nsIContent> nextContent;

    aContainer->ChildAt(i, *getter_AddRefs(nextContent));
    aPresShell->GetPrimaryFrameFor(nextContent, &nextSibling);

    if (nsnull != nextSibling) {
      // The frame may have a next-in-flow. Get the first-in-flow
      nsIFrame* prevInFlow;
      do {
        nextSibling->GetPrevInFlow(&prevInFlow);
        if (nsnull != prevInFlow) {
          nextSibling = prevInFlow;
        }
      } while (nsnull != prevInFlow);

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
nsCSSFrameConstructor::ContentAppended(nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       PRInt32         aNewIndexInContainer)
{
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended container=%p index=%d\n",
           aContainer, aNewIndexInContainer);
    if (gReallyNoisyContentUpdates && aContainer) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

#ifdef INCLUDE_XUL
  if (aContainer) {
    nsCOMPtr<nsIAtom> tag;
    aContainer->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::treechildren ||
        tag.get() == nsXULAtoms::treeitem)) {
      // Walk up to the outermost tree row group frame and tell it that
      // content was added.
      nsCOMPtr<nsIContent> parent;
      nsCOMPtr<nsIContent> child = dont_QueryInterface(aContainer);
      child->GetParent(*getter_AddRefs(parent));
      while (parent) {
        parent->GetTag(*getter_AddRefs(tag));
        if (tag.get() == nsXULAtoms::tree)
          break;
        child = parent;
        child->GetParent(*getter_AddRefs(parent));
      }

      if (parent) {
        // We found it.  Get the primary frame.
        nsIFrame* outerFrame = GetFrameFor(shell, aPresContext, child);

        // Convert to a tree row group frame.
        nsTreeRowGroupFrame* treeRowGroup = (nsTreeRowGroupFrame*)outerFrame;
        if (treeRowGroup) {

          // Get the primary frame for the parent of the child that's being added.
          nsIFrame* innerFrame = GetFrameFor(shell, aPresContext, aContainer);
  
          // See if there's a previous sibling.
          nsIFrame* prevSibling = FindPreviousSibling(shell,
                                                      aContainer,
                                                      aNewIndexInContainer);

          // This needs to really be a previous sibling.
          if (prevSibling && aNewIndexInContainer > 0) {
            nsCOMPtr<nsIContent> prevContent;
            nsCOMPtr<nsIContent> frameContent;
            aContainer->ChildAt(aNewIndexInContainer-1, *getter_AddRefs(prevContent));
            prevSibling->GetContent(getter_AddRefs(frameContent));
            if (frameContent.get() != prevContent.get()) 
              prevSibling = nsnull;
          }

          if (prevSibling || (innerFrame && aNewIndexInContainer == 0)) {
            // We're onscreen. Make sure a full reflow happens.
            treeRowGroup->OnContentAdded(aPresContext);
          }
          else {
            // We're going to be offscreen.
            treeRowGroup->ReflowScrollbar(aPresContext);
          }

          return NS_OK;
        }
      }
    }
  }
#endif // INCLUDE_XUL

  // Get the frame associated with the content
  nsIFrame* parentFrame = GetFrameFor(shell, aPresContext, aContainer);
  if (nsnull != parentFrame) {

    // If the frame we are manipulating is a special frame then do
    // something different instead of just appending newly created
    // frames. Note that only the first-in-flow is marked so we check
    // before getting to the last-in-flow.
    if (IsFrameSpecial(aPresContext, parentFrame)) {
      // We are pretty harsh here (and definitely not optimal) -- we
      // wipe out the entire containing block and recreate it from
      // scratch. The reason is that because we know that a special
      // inline frame has propogated some of its children upward to be
      // children of the block and that those frames may need to move
      // around. This logic guarantees a correct answer.
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::ContentAppended: parentFrame=");
        nsFrame::ListTag(stdout, parentFrame);
        printf(" is special\n");
      }
#endif
      return ReframeContainingBlock(aPresContext, parentFrame);
    }

    // Get the parent frame's last-in-flow
    nsIFrame* nextInFlow = parentFrame;
    while (nsnull != nextInFlow) {
      parentFrame->GetNextInFlow(&nextInFlow);
      if (nsnull != nextInFlow) {
        parentFrame = nextInFlow;
      }
    }

    // If we didn't process children when we originally created the frame,
    // then don't do any processing now
    nsCOMPtr<nsIAtom>  frameType;
    parentFrame->GetFrameType(getter_AddRefs(frameType));
    if (frameType.get() == nsLayoutAtoms::objectFrame) {
      // This handles APPLET, EMBED, and OBJECT
      return NS_OK;
    }

    // Create some new frames
    PRInt32                 count;
    nsIFrame*               firstAppendedFrame = nsnull;
    nsFrameItems            frameItems;
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                  GetAbsoluteContainingBlock(aPresContext, parentFrame),
                                  GetFloaterContainingBlock(aPresContext, parentFrame),
                                  nsnull);

    // See if the containing block has :first-letter style applied.
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
    nsCOMPtr<nsIContent> blockContent;
    nsCOMPtr<nsIStyleContext> blockSC;
    containingBlock->GetStyleContext(getter_AddRefs(blockSC));
    containingBlock->GetContent(getter_AddRefs(blockContent));
    HaveSpecialBlockStyle(aPresContext, blockContent, blockSC,
                          &haveFirstLetterStyle,
                          &haveFirstLineStyle);

    if (haveFirstLetterStyle) {
      // Before we get going, remove the current letter frames
      RemoveLetterFrames(aPresContext, state.mPresShell,
                         state.mFrameManager, containingBlock);
    }

    PRInt32 i;
    aContainer->ChildCount(count);
    for (i = aNewIndexInContainer; i < count; i++) {
      nsCOMPtr<nsIContent> childContent;
      aContainer->ChildAt(i, *getter_AddRefs(childContent));

      // Construct a child frame
      ConstructFrame(shell, aPresContext, state, childContent, parentFrame, frameItems);
    }

    if (haveFirstLineStyle) {
      // It's possible that some of the new frames go into a
      // first-line frame. Look at them and see...
      AppendFirstLineFrames(shell, aPresContext, state, aContainer, parentFrame,
                            frameItems); 
    }

    // Adjust parent frame for table inner/outer frame.  We need to do
    // this here because we need both the parent frame and the
    // constructed frame
    nsresult result = NS_OK;
    nsIFrame* adjustedParentFrame = parentFrame;
    firstAppendedFrame = frameItems.childList;
    if (nsnull != firstAppendedFrame) {
      const nsStyleDisplay* firstAppendedFrameDisplay;
      firstAppendedFrame->GetStyleData(eStyleStruct_Display,
                                       (const nsStyleStruct *&)firstAppendedFrameDisplay);
      result = GetAdjustedParentFrame(aPresContext, parentFrame,
                                      firstAppendedFrameDisplay->mDisplay,
                                      adjustedParentFrame);
    }

    // Notify the parent frame passing it the list of new frames
    if (NS_SUCCEEDED(result) && firstAppendedFrame) {
      // Perform special check for diddling around with the frames in
      // a special inline frame.

      // XXX Bug 18366
      // Although select frame are inline we do not want to call
      // WipeContainingBlock because it will throw away the entire selct frame and 
      // start over which is something we do not want to do
      //
      nsCOMPtr<nsIDOMHTMLSelectElement> selectContent(do_QueryInterface(aContainer));
      if (!selectContent) {
        if (WipeContainingBlock(aPresContext, state, blockContent, adjustedParentFrame,
                                frameItems.childList)) {
          return NS_OK;
        }
      }

      // Append the flowed frames to the principal child list
      AppendFrames(aPresContext, shell, state.mFrameManager, aContainer,
                   adjustedParentFrame, firstAppendedFrame);

      // If there are new absolutely positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mAbsoluteItems.childList) {
        state.mAbsoluteItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                           nsLayoutAtoms::absoluteList,
                                                           state.mAbsoluteItems.childList);
      }

      // If there are new fixed positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFixedItems.childList) {
        state.mFixedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                        nsLayoutAtoms::fixedList,
                                                        state.mFixedItems.childList);
      }

      // If there are new floating child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFloatedItems.childList) {
        state.mFloatedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                          nsLayoutAtoms::floaterList,
                                                          state.mFloatedItems.childList);
      }

      // Recover first-letter frames
      if (haveFirstLetterStyle) {
        RecoverLetterFrames(shell, aPresContext, state, containingBlock);
      }
    }

    // Here we have been notified that content has been appended so if
    // the select now has a single item we need to go in and removed
    // the dummy frame.
    nsCOMPtr<nsIDOMHTMLSelectElement> sel(do_QueryInterface(aContainer));
    if (sel) {
      nsCOMPtr<nsIContent> childContent;
      aContainer->ChildAt(aNewIndexInContainer, *getter_AddRefs(childContent));
      if (childContent) {
        RemoveDummyFrameFromSelect(aPresContext, shell, aContainer,
                                   childContent, sel);
      }
    } 

  }

  return NS_OK;
}


nsresult
nsCSSFrameConstructor::RemoveDummyFrameFromSelect(nsIPresContext* aPresContext,
                                                  nsIPresShell *  aPresShell,
                                                  nsIContent*     aContainer,
                                                  nsIContent*     aChild,
                                                  nsIDOMHTMLSelectElement * aSelectElement)
{
  //check to see if there is one item,
  // meaning we need to remove the dummy frame
  PRUint32 numOptions = 0;
  nsresult result = aSelectElement->GetLength(&numOptions);
  if (NS_SUCCEEDED(result)) {
    if (1 == numOptions) { 
      nsIFrame* parentFrame;
      nsIFrame* childFrame;
      // Get the childFrame for the added child (option)
      // then get the child's parent frame which should be an area frame
      aPresShell->GetPrimaryFrameFor(aChild, &childFrame);
      if (nsnull != childFrame) {
        childFrame->GetParent(&parentFrame);

        // Now loop through all the child looking fr the frame whose content 
        // is equal to the select element's content
        // this is because when gernated content is created it stuff the parent content
        // pointer into the generated frame, so in this case it has the select content
        parentFrame->FirstChild(aPresContext, nsnull, &childFrame);
        nsCOMPtr<nsIContent> selectContent = do_QueryInterface(aSelectElement);
        while (nsnull != childFrame) {
          nsIContent * content;
          childFrame->GetContent(&content);
      
          // Found the dummy frame so get the FrameManager and 
          // delete/remove the dummy frame
          if (selectContent.get() == content) {
            nsCOMPtr<nsIFrameManager> frameManager;
            aPresShell->GetFrameManager(getter_AddRefs(frameManager));
            frameManager->RemoveFrame(aPresContext, *aPresShell, parentFrame, nsnull, childFrame);
            NS_IF_RELEASE(content);
            return NS_OK;
          }

          NS_IF_RELEASE(content);
          childFrame->GetNextSibling(&childFrame);
        }
      }
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentInserted(nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       nsIContent*     aChild,
                                       PRInt32         aIndexInContainer,
                                       nsILayoutHistoryState* aFrameState)
{
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentInserted container=%p child=%p index=%d\n",
           aContainer, aChild, aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      (aContainer ? aContainer : aChild)->List(stdout, 0);
    }
  }
#endif

#ifdef INCLUDE_XUL
  if (aContainer) {
    nsCOMPtr<nsIAtom> tag;
    aContainer->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::treechildren ||
                tag.get() == nsXULAtoms::treeitem)) {
      // Walk up to the outermost tree row group frame and tell it that
      // content was added.
      nsCOMPtr<nsIContent> parent;
      nsCOMPtr<nsIContent> child = dont_QueryInterface(aContainer);
      child->GetParent(*getter_AddRefs(parent));
      while (parent) {
        parent->GetTag(*getter_AddRefs(tag));
        if (tag.get() == nsXULAtoms::tree)
          break;
        child = parent;
        child->GetParent(*getter_AddRefs(parent));
      }

      if (parent) {
        // We found it.  Get the primary frame.
        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        nsIFrame*     outerFrame = GetFrameFor(shell, aPresContext, child);

        // Convert to a tree row group frame.
        nsTreeRowGroupFrame* treeRowGroup = (nsTreeRowGroupFrame*)outerFrame;
        if (treeRowGroup) {

          // Get the primary frame for the parent of the child that's being added.
          nsIFrame* innerFrame = GetFrameFor(shell, aPresContext, aContainer);
  
          // See if there's a previous sibling.
          nsIFrame* prevSibling = FindPreviousSibling(shell,
                                                       aContainer,
                                                       aIndexInContainer);

          if (prevSibling || innerFrame) {
            // We're onscreen, but because of the fact that we can be called to
            // "kill" a displayed frame (e.g., when you close a tree node), we
            // have to see if this slaying is taking place.  If so, then we don't
            // really want to do anything.  Instead we need to add our node to
            // the undisplayed content list so that the style system will know
            // to check it for subsequent display changes (e.g., when you next
            // reopen).
            nsCOMPtr<nsIStyleContext> styleContext;
            nsCOMPtr<nsIAtom> tagName;
            aChild->GetTag(*getter_AddRefs(tagName));
            ResolveStyleContext(aPresContext, innerFrame, aChild, tagName, getter_AddRefs(styleContext));

            // Pre-check for display "none" - if we find that, don't reflow at all.
            const nsStyleDisplay* display = (const nsStyleDisplay*)
              styleContext->GetStyleData(eStyleStruct_Display);

            if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {

              nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                    GetAbsoluteContainingBlock(aPresContext, innerFrame),
                                    GetFloaterContainingBlock(aPresContext, innerFrame),
                                    aFrameState);
              state.mFrameManager->SetUndisplayedContent(aChild, styleContext);
              return NS_OK;
            }
            
            // Good call.  Make sure a full reflow happens.
            nsIFrame* nextSibling = FindNextSibling(shell, aContainer, aIndexInContainer);
            if(!nextSibling)
              treeRowGroup->OnContentAdded(aPresContext);
            else {
              nsIFrame*     frame = GetFrameFor(shell, aPresContext, aContainer);
              nsTreeRowGroupFrame* frameTreeRowGroup = (nsTreeRowGroupFrame*)frame;
              if(frameTreeRowGroup)
                frameTreeRowGroup->OnContentInserted(aPresContext, nextSibling, aIndexInContainer);
            }
          }
          else {
            // We're going to be offscreen.
            treeRowGroup->ReflowScrollbar(aPresContext);
          }

          return NS_OK;
        }
      }
    }
  }
#endif // INCLUDE_XUL

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
    nsIFrame*               docElementFrame;
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock, nsnull, nsnull, aFrameState);
    ConstructDocElementFrame(shell, aPresContext, 
                             state,
                             docElement, 
                             mDocElementContainingBlock,
                             containerStyle, 
                             docElementFrame);
    NS_IF_RELEASE(docElement);
    
    // Set the initial child list for the parent
    mDocElementContainingBlock->SetInitialChildList(aPresContext, 
                                                    nsnull, 
                                                    docElementFrame);
    
    // Tell the fixed containing block about its 'fixed' frames
    if (state.mFixedItems.childList) {
      mFixedContainingBlock->SetInitialChildList(aPresContext, 
                                                 nsLayoutAtoms::fixedList,
                                                 state.mFixedItems.childList);
    }

  } else {
    // Find the frame that precedes the insertion point.
    nsIFrame* prevSibling = FindPreviousSibling(shell, aContainer, aIndexInContainer);
    nsIFrame* nextSibling = nsnull;
    PRBool    isAppend = PR_FALSE;
    
    // If there is no previous sibling, then find the frame that follows
    if (nsnull == prevSibling) {
      nextSibling = FindNextSibling(shell, aContainer, aIndexInContainer);
    }

    // Get the geometric parent. Use the prev sibling if we have it;
    // otherwise use the next sibling
    nsIFrame* parentFrame;
    if (nsnull != prevSibling) {
      prevSibling->GetParent(&parentFrame);
    }
    else if (nextSibling) {
      nextSibling->GetParent(&parentFrame);
    }
    else {
      // No previous or next sibling so treat this like an appended frame.
      isAppend = PR_TRUE;
      parentFrame = GetFrameFor(shell, aPresContext, aContainer);
      
      if (parentFrame) {
        // If we didn't process children when we originally created the frame,
        // then don't do any processing now
        nsCOMPtr<nsIAtom>  frameType;
        parentFrame->GetFrameType(getter_AddRefs(frameType));
        if (frameType.get() == nsLayoutAtoms::objectFrame) {
          // This handles APPLET, EMBED, and OBJECT
          return NS_OK;
        }
      }
    }

    // Construct a new frame
    if (nsnull != parentFrame) {
      // If the frame we are manipulating is a special frame then do
      // something different instead of just inserting newly created
      // frames.
      if (IsFrameSpecial(aPresContext, parentFrame)) {
        // We are pretty harsh here (and definitely not optimal) -- we
        // wipe out the entire containing block and recreate it from
        // scratch. The reason is that because we know that a special
        // inline frame has propogated some of its children upward to be
        // children of the block and that those frames may need to move
        // around. This logic guarantees a correct answer.
#ifdef DEBUG
        if (gNoisyContentUpdates) {
          printf("nsCSSFrameConstructor::ContentInserted: parentFrame=");
          nsFrame::ListTag(stdout, parentFrame);
          printf(" is special\n");
        }
#endif
        return ReframeContainingBlock(aPresContext, parentFrame);
      }

      nsFrameItems            frameItems;
      nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                    GetAbsoluteContainingBlock(aPresContext, parentFrame),
                                    GetFloaterContainingBlock(aPresContext, parentFrame),
                                    aFrameState);

      // Recover state for the containing block - we need to know if
      // it has :first-letter or :first-line style applied to it. The
      // reason we care is that the internal structure in these cases
      // is not the normal structure and requires custom updating
      // logic.
      nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
      nsCOMPtr<nsIStyleContext> blockSC;
      nsCOMPtr<nsIContent> blockContent;
      PRBool haveFirstLetterStyle = PR_FALSE;
      PRBool haveFirstLineStyle = PR_FALSE;

      // In order to shave off some cycles, we only dig up the
      // containing block haveFirst* flags if the parent frame where
      // the insertion/append is occuring is an inline or block
      // container. For other types of containers this isn't relevant.
      const nsStyleDisplay* parentDisplay;
      parentFrame->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)parentDisplay);

      // Examine the parentFrame where the insertion is taking
      // place. If its a certain kind of container then some special
      // processing is done.
      if ((NS_STYLE_DISPLAY_BLOCK == parentDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_LIST_ITEM == parentDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_INLINE == parentDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_INLINE_BLOCK == parentDisplay->mDisplay)) {
        // Recover the special style flags for the containing block
        containingBlock->GetStyleContext(getter_AddRefs(blockSC));
        containingBlock->GetContent(getter_AddRefs(blockContent));
        HaveSpecialBlockStyle(aPresContext, blockContent, blockSC,
                              &haveFirstLetterStyle,
                              &haveFirstLineStyle);

        if (haveFirstLetterStyle) {
          // Get the correct parentFrame and prevSibling - if a
          // letter-frame is present, use its parent.
          nsCOMPtr<nsIAtom> parentFrameType;
          parentFrame->GetFrameType(getter_AddRefs(parentFrameType));
          if (parentFrameType.get() == nsLayoutAtoms::letterFrame) {
            if (prevSibling) {
              prevSibling = parentFrame;
            }
            parentFrame->GetParent(&parentFrame);
          }

          // Remove the old letter frames before doing the insertion
          RemoveLetterFrames(aPresContext, state.mPresShell,
                             state.mFrameManager,
                             state.mFloatedItems.containingBlock);

        }
      }

      // If the frame we are manipulating is a special inline frame
      // then do something different instead of just inserting newly
      // created frames.
      if (IsFrameSpecial(state.mFrameManager, parentFrame)) {
        // We are pretty harsh here (and definitely not optimal) -- we
        // wipe out the entire containing block and recreate it from
        // scratch. The reason is that because we know that a special
        // inline frame has propogated some of its children upward to be
        // children of the block and that those frames may need to move
        // around. This logic guarantees a correct answer.
        nsCOMPtr<nsIContent> parentContainer;
        blockContent->GetParent(*getter_AddRefs(parentContainer));
#ifdef DEBUG
        if (gNoisyContentUpdates) {
          printf("nsCSSFrameConstructor::ContentInserted: parentFrame=");
          nsFrame::ListTag(stdout, parentFrame);
          printf(" is special inline\n");
          printf("  ==> blockContent=%p, parentContainer=%p\n",
                 blockContent.get(), parentContainer.get());
        }
#endif
        if (parentContainer) {
          PRInt32 ix;
          parentContainer->IndexOf(blockContent, ix);
          ContentReplaced(aPresContext, parentContainer, blockContent, blockContent, ix);
        }
        else {
          // XXX uh oh. the block that needs reworking has no parent...
        }
        return NS_OK;
      }

      rv =  ConstructFrame(shell, aPresContext, state, aChild, parentFrame, frameItems);

      // XXX Bug 19949
      // Although select frame are inline we do not want to call
      // WipeContainingBlock because it will throw away the entire select frame and 
      // start over which is something we do not want to do
      //
      nsCOMPtr<nsIDOMHTMLSelectElement> selectContent(do_QueryInterface(aContainer));
      if (!selectContent) {
        // Perform special check for diddling around with the frames in
        // a special inline frame.
        if (WipeContainingBlock(aPresContext, state, blockContent, parentFrame, frameItems.childList)) {
          return NS_OK;
        }
      }

      if (haveFirstLineStyle) {
        // It's possible that the new frame goes into a first-line
        // frame. Look at it and see...
        if (isAppend) {
          // Use append logic when appending
          AppendFirstLineFrames(shell, aPresContext, state, aContainer, parentFrame,
                                frameItems); 
        }
        else {
          // Use more complicated insert logic when inserting
          InsertFirstLineFrames(aPresContext, state, aContainer,
                                containingBlock, &parentFrame,
                                prevSibling, frameItems);
        }
      }
      
      nsIFrame* newFrame = frameItems.childList;
      if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {

        // Notify the parent frame
        if (isAppend) {
          rv = AppendFrames(aPresContext, shell, state.mFrameManager,
                            aContainer, parentFrame, newFrame);
        } else {
          if (!prevSibling) {
            // We're inserting the new frame as the first child. See if the
            // parent has a :before pseudo-element
            nsIFrame* firstChild;
            parentFrame->FirstChild(aPresContext, nsnull, &firstChild);

            if (firstChild && IsGeneratedContentFor(aContainer, firstChild, nsCSSAtoms::beforePseudo)) {
              // Insert the new frames after the :before pseudo-element
              prevSibling = firstChild;
            }
          }
          // check for a table caption which goes on an additional child list
          nsIAtom* childList = GetChildListFor(newFrame);
          rv = state.mFrameManager->InsertFrames(aPresContext, *shell,
                                                 parentFrame,
                                                 childList, prevSibling,
                                                 newFrame);
        }
        
        // If there are new absolutely positioned child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (state.mAbsoluteItems.childList) {
          rv = state.mAbsoluteItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                           nsLayoutAtoms::absoluteList,
                                                           state.mAbsoluteItems.childList);
        }
        
        // If there are new fixed positioned child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (state.mFixedItems.childList) {
          rv = state.mFixedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                        nsLayoutAtoms::fixedList,
                                                        state.mFixedItems.childList);
        }
        
        // If there are new floating child frames, then notify
        // the parent
        // XXX We can't just assume these frames are being appended, we need to
        // determine where in the list they should be inserted...
        if (state.mFloatedItems.childList) {
          rv = state.mFloatedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                      nsLayoutAtoms::floaterList,
                                                      state.mFloatedItems.childList);
        }

        if (haveFirstLetterStyle) {
          // Recover the letter frames for the containing block when
          // it has first-letter style.
          RecoverLetterFrames(shell, aPresContext, state,
                              state.mFloatedItems.containingBlock);
        }

      }
    }

    // Here we have been notified that content has been insert
    // so if the select now has a single item 
    // we need to go in and removed the dummy frame
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
    nsresult result = aContainer->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                                 (void**)getter_AddRefs(selectElement));
    if (NS_SUCCEEDED(result) && selectElement) {
      RemoveDummyFrameFromSelect(aPresContext, shell, aContainer, aChild, selectElement);
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
                          aNewChild, aIndexInContainer, nsnull);
  }

  return res;
}

// Returns PR_TRUE if aAncestorFrame is an ancestor frame of aFrame
static PRBool
IsAncestorFrame(nsIFrame* aFrame, nsIFrame* aAncestorFrame)
{
  nsIFrame* parentFrame;
  aFrame->GetParent(&parentFrame);

  while (parentFrame) {
    if (parentFrame == aAncestorFrame) {
      return PR_TRUE;
    }

    parentFrame->GetParent(&parentFrame);
  }

  return PR_FALSE;
}

/**
 * Called to delete a frame subtree. Two important things happen:
 * 1. for each frame in the subtree we remove the mapping from the
 *    content object to its frame
 * 2. for child frames that have been moved out of the flow we delete
 *    the out-of-flow frame as well
 *
 * Note: this function should only be called by DeletingFrameSubtree()
 *
 * @param   aRemovedFrame this is the frame that was removed from the
 *            content model. As we recurse we need to remember this so we
 *            can check if out-of-flow frames are a descendent of the frame
 *            being removed
 * @param   aFrame the local subtree that is being deleted. This is initially
 *            the same as aRemovedFrame, but as we recurse down the tree
 *            this changes
 */
static nsresult
DoDeletingFrameSubtree(nsIPresContext*  aPresContext,
                       nsIPresShell*    aPresShell,
                       nsIFrameManager* aFrameManager,
                       nsIFrame*        aRemovedFrame,
                       nsIFrame*        aFrame)
{
  NS_PRECONDITION(aFrameManager, "no frame manager");
  
  // Remove the mapping from the content object to its frame
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  aFrameManager->SetPrimaryFrameFor(content, nsnull);
  aFrameManager->ClearAllUndisplayedContentIn(content);

  // Walk aFrame's child frames
  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;

  do {
    // Recursively walk aFrame's child frames looking for placeholder frames
    nsIFrame* childFrame;
    aFrame->FirstChild(aPresContext, childListName, &childFrame);
    while (childFrame) {
      nsIAtom*  frameType;
      PRBool    isPlaceholder;
  
      // See if it's a placeholder frame
      childFrame->GetFrameType(&frameType);
      isPlaceholder = (nsLayoutAtoms::placeholderFrame == frameType);
      NS_IF_RELEASE(frameType);
  
      if (isPlaceholder) {
        // Get the out-of-flow frame
        nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)childFrame)->GetOutOfFlowFrame();
        NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
  
        // Remove the mapping from the out-of-flow frame to its placeholder
        aFrameManager->SetPlaceholderFrameFor(outOfFlowFrame, nsnull);
        
        // Recursively find and delete any of its out-of-flow frames, and remove
        // the mapping from content objects to frames
        DoDeletingFrameSubtree(aPresContext, aPresShell, aFrameManager, aRemovedFrame,
                               outOfFlowFrame);
        
        // Don't delete the out-of-flow frame if aRemovedFrame is one of its
        // ancestor frames, because when aRemovedFrame is deleted it will delete
        // its child frames including this out-of-flow frame
        if (!IsAncestorFrame(outOfFlowFrame, aRemovedFrame)) {
          // Get the out-of-flow frame's parent
          nsIFrame* parentFrame;
          outOfFlowFrame->GetParent(&parentFrame);
  
          // Get the child list name for the out-of-flow frame
          nsIAtom* listName;
          GetChildListNameFor(aPresContext, parentFrame, outOfFlowFrame, &listName);
  
          // Ask the parent to delete the out-of-flow frame
          aFrameManager->RemoveFrame(aPresContext, *aPresShell, parentFrame,
                                     listName, outOfFlowFrame);
          NS_IF_RELEASE(listName);
        }
  
      } else {
        // Recursively find and delete any of its out-of-flow frames, and remove
        // the mapping from content objects to frames
        DoDeletingFrameSubtree(aPresContext, aPresShell, aFrameManager, aRemovedFrame,
                               childFrame);
      }
  
      // Get the next sibling child frame
      childFrame->GetNextSibling(&childFrame);
    }

    // Look for any additional named child lists
    NS_IF_RELEASE(childListName);
    aFrame->GetAdditionalChildListName(childListIndex++, &childListName);
  } while (childListName);

  return NS_OK;
}

/**
 * Called to delete a frame subtree. Calls DoDeletingFrameSubtree()
 * for aFrame and each of its continuing frames
 */
static nsresult
DeletingFrameSubtree(nsIPresContext*  aPresContext,
                     nsIPresShell*    aPresShell,
                     nsIFrameManager* aFrameManager,
                     nsIFrame*        aFrame)
{
  // If there's no frame manager it's probably because the pres shell is
  // being destroyed
  if (aFrameManager) {
    while (aFrame) {
      DoDeletingFrameSubtree(aPresContext, aPresShell, aFrameManager,
                             aFrame, aFrame);

      // If it's split, then get the continuing frame. Note that we only do
      // this for the top-most frame being deleted. Don't do it if we're
      // recursing over a subtree, because those continuing frames should be
      // found as part of the walk over the top-most frame's continuing frames.
      // Walking them again will make this an N^2/2 algorithm
      aFrame->GetNextInFlow(&aFrame);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSFrameConstructor::RemoveMappingsForFrameSubtree(nsIPresContext* aPresContext,
                                                     nsIFrame* aRemovedFrame, 
                                                     nsILayoutHistoryState* aFrameState)
{
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIFrameManager> frameManager;
  presShell->GetFrameManager(getter_AddRefs(frameManager));
  
  // Save the frame tree's state before deleting it
  CaptureStateFor(aPresContext, aRemovedFrame, mTempFrameTreeState);

  return DeletingFrameSubtree(aPresContext, presShell, frameManager, aRemovedFrame);
}

NS_IMETHODIMP
nsCSSFrameConstructor::ContentRemoved(nsIPresContext* aPresContext,
                                      nsIContent*     aContainer,
                                      nsIContent*     aChild,
                                      PRInt32         aIndexInContainer)
{
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentRemoved container=%p child=%p index=%d\n",
           aContainer, aChild, aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsCOMPtr<nsIPresShell>    shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsCOMPtr<nsIFrameManager> frameManager;
  shell->GetFrameManager(getter_AddRefs(frameManager));
  nsresult                  rv = NS_OK;

  // Find the child frame that maps the content
  nsIFrame* childFrame;
  shell->GetPrimaryFrameFor(aChild, &childFrame);

  if (! childFrame) {
    frameManager->ClearUndisplayedContentIn(aChild, aContainer);
  }

  // When the last item is removed from a select, 
  // we need to add a pseudo frame so select gets sized as the best it can
  // so here we see if it is a select and then we get the number of options
  nsresult result = NS_ERROR_FAILURE;
  if (aContainer && childFrame) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
    result = aContainer->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                               (void**)getter_AddRefs(selectElement));
    if (NS_SUCCEEDED(result) && selectElement) {
      PRUint32 numOptions = 0;
      result = selectElement->GetLength(&numOptions);
      nsIFrame * selectFrame;                             // XXX temp needed only native controls
      shell->GetPrimaryFrameFor(aContainer, &selectFrame);// XXX temp needed only native controls

      // For "select" add the pseudo frame after the last item is deleted
      nsIFrame* parentFrame = nsnull;
      childFrame->GetParent(&parentFrame);
      if (parentFrame == selectFrame) { // XXX temp needed only native controls
        return NS_ERROR_FAILURE;
      }
      if (NS_SUCCEEDED(result) && shell && parentFrame && 1 == numOptions) { 
  
        nsIStyleContext*          styleContext   = nsnull; 
        nsIFrame*                 generatedFrame = nsnull; 
        nsFrameConstructorState   state(aPresContext, nsnull, nsnull, nsnull, nsnull);

        //shell->GetPrimaryFrameFor(aContainer, &contentFrame);
        parentFrame->GetStyleContext(&styleContext); 
        if (CreateGeneratedContentFrame(shell, aPresContext, state, parentFrame, aContainer, 
                                        styleContext, nsLayoutAtoms::dummyOptionPseudo, 
                                        PR_FALSE, &generatedFrame)) { 
          // Add the generated frame to the child list 
          frameManager->AppendFrames(aPresContext, *shell, parentFrame, nsnull, generatedFrame);
        }
      } 
    } 
  }

#ifdef INCLUDE_XUL
  if (aContainer) {
    nsCOMPtr<nsIAtom> tag;
    aContainer->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::treechildren ||
        tag.get() == nsXULAtoms::treeitem) {
      if (childFrame) {
        // Convert to a tree row group frame.
        nsIFrame* parentFrame;
        childFrame->GetParent(&parentFrame);
        nsTreeRowGroupFrame* treeRowGroup = (nsTreeRowGroupFrame*)parentFrame;
        if (treeRowGroup) {
          treeRowGroup->OnContentRemoved(aPresContext, childFrame, aIndexInContainer);
          return NS_OK;
        }
      }
      else {
        // Ensure that we notify the outermost row group that the item
        // has been removed (so that we can update the scrollbar state).
        // Walk up to the outermost tree row group frame and tell it that
        // the scrollbar thumb should be updated.
        nsCOMPtr<nsIContent> parent;
        nsCOMPtr<nsIContent> child = dont_QueryInterface(aContainer);
        child->GetParent(*getter_AddRefs(parent));
        while (parent) {
          parent->GetTag(*getter_AddRefs(tag));
          if (tag.get() == nsXULAtoms::tree)
            break;
          child = parent;
          child->GetParent(*getter_AddRefs(parent));
        }

        if (parent) {
          // We found it.  Get the primary frame.
          nsIFrame*     parentFrame = GetFrameFor(shell, aPresContext, child);

          // Convert to a tree row group frame.
          nsTreeRowGroupFrame* treeRowGroup = (nsTreeRowGroupFrame*)parentFrame;
          if (treeRowGroup) {
            treeRowGroup->ReflowScrollbar(aPresContext);
            return NS_OK;
          }
        }
      }
    }
  }
#endif // INCLUDE_XUL

  if (childFrame) {
    // Get the childFrame's parent frame
    nsIFrame* parentFrame;
    childFrame->GetParent(&parentFrame);

    // If the frame we are manipulating is a special frame then do
    // something different instead of just inserting newly created
    // frames.
    if (IsFrameSpecial(aPresContext, parentFrame)) {
      // We are pretty harsh here (and definitely not optimal) -- we
      // wipe out the entire containing block and recreate it from
      // scratch. The reason is that because we know that a special
      // inline frame has propogated some of its children upward to be
      // children of the block and that those frames may need to move
      // around. This logic guarantees a correct answer.
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::ContentRemoved: parentFrame=");
        nsFrame::ListTag(stdout, parentFrame);
        printf(" is special\n");
      }
#endif
      return ReframeContainingBlock(aPresContext, parentFrame);
    }

    // Examine the containing-block for the removed content and see if
    // :first-letter style applies.
    nsIFrame* containingBlock =
      GetFloaterContainingBlock(aPresContext, parentFrame);
    nsCOMPtr<nsIStyleContext> blockSC;
    containingBlock->GetStyleContext(getter_AddRefs(blockSC));
    nsCOMPtr<nsIContent> blockContent;
    containingBlock->GetContent(getter_AddRefs(blockContent));
    PRBool haveFLS = HaveFirstLetterStyle(aPresContext, blockContent, blockSC);
    if (haveFLS) {
      // Trap out to special routine that handles adjusting a blocks
      // frame tree when first-letter style is present.
#ifdef NOISY_FIRST_LETTER
      printf("ContentRemoved: containingBlock=");
      nsFrame::ListTag(stdout, containingBlock);
      printf(" parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" childFrame=");
      nsFrame::ListTag(stdout, childFrame);
      printf("\n");
#endif

      // First update the containing blocks structure by removing the
      // existing letter frames. This makes the subsequent logic
      // simpler.
      RemoveLetterFrames(aPresContext, shell, frameManager, containingBlock);

      // Recover childFrame and parentFrame
      shell->GetPrimaryFrameFor(aChild, &childFrame);
      if (!childFrame) {
        frameManager->ClearUndisplayedContentIn(aChild, aContainer);
        return NS_OK;
      }
      childFrame->GetParent(&parentFrame);

#ifdef NOISY_FIRST_LETTER
      printf("  ==> revised parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" childFrame=");
      nsFrame::ListTag(stdout, childFrame);
      printf("\n");
#endif
    }

    // Walk the frame subtree deleting any out-of-flow frames, and
    // remove the mapping from content objects to frames
    DeletingFrameSubtree(aPresContext, shell, frameManager, childFrame);

    // See if the child frame is a floating frame
    const nsStyleDisplay* display;
    childFrame->GetStyleData(eStyleStruct_Display,
                             (const nsStyleStruct*&)display);
    if (display->IsFloating()) {
#ifdef NOISY_FIRST_LETTER
      printf("  ==> child display is still floating!\n");
#endif
      // Get the placeholder frame
      nsIFrame* placeholderFrame;
      frameManager->GetPlaceholderFrameFor(childFrame, &placeholderFrame);

      // Remove the mapping from the frame to its placeholder
      frameManager->SetPlaceholderFrameFor(childFrame, nsnull);

      // Now we remove the floating frame

      // XXX has to be done first for now: the blocks line list
      // contains an array of pointers to the placeholder - we have to
      // remove the floater first (which gets rid of the lines
      // reference to the placeholder and floater) and then remove the
      // placeholder
      rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame,
                                     nsLayoutAtoms::floaterList, childFrame);

      // Remove the placeholder frame first (XXX second for now) (so
      // that it doesn't retain a dangling pointer to memory)
      if (nsnull != placeholderFrame) {
        placeholderFrame->GetParent(&parentFrame);
        DeletingFrameSubtree(aPresContext, shell, frameManager, placeholderFrame);
        rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame,
                                       nsnull, placeholderFrame);
      }
    }
    else {
      // See if it's absolutely or fixed positioned
      const nsStylePosition* position;
      childFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
      if (position->IsAbsolutelyPositioned()) {
        // Get the placeholder frame
        nsIFrame* placeholderFrame;
        frameManager->GetPlaceholderFrameFor(childFrame, &placeholderFrame);

        // Remove the mapping from the frame to its placeholder
        frameManager->SetPlaceholderFrameFor(childFrame, nsnull);

        // Generate two notifications. First for the absolutely positioned
        // frame
        rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame,
          (NS_STYLE_POSITION_FIXED == position->mPosition) ?
          nsLayoutAtoms::fixedList : nsLayoutAtoms::absoluteList, childFrame);

        // Now the placeholder frame
        if (nsnull != placeholderFrame) {
          placeholderFrame->GetParent(&parentFrame);
          rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame, nsnull,
                                         placeholderFrame);
        }

      } else {
        // Notify the parent frame that it should delete the frame
        nsIAtom* childList = GetChildListFor(childFrame);
        rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame,
                                       childList, childFrame);
      }
    }

    if (mInitialContainingBlock == childFrame) {
      mInitialContainingBlock = nsnull;
    }

    if (haveFLS && mInitialContainingBlock) {
      nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                    GetAbsoluteContainingBlock(aPresContext,
                                                               parentFrame),
                                    GetFloaterContainingBlock(aPresContext,
                                                              parentFrame),
                                    nsnull);
      RecoverLetterFrames(shell, aPresContext, state, containingBlock);
    }
  }

  return rv;
}

static void
ApplyRenderingChangeToTree(nsIPresContext* aPresContext,
                           nsIFrame* aFrame,
                           nsIViewManager* aViewManager);

static void
SyncAndInvalidateView(nsIPresContext* aPresContext,
                      nsIView*        aView,
                      nsIFrame*       aFrame, 
                      nsIViewManager* aViewManager)
{
  const nsStyleColor* color;
  const nsStyleDisplay* disp; 
  aFrame->GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&) color);
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) disp);

  aViewManager->SetViewOpacity(aView, color->mOpacity);

  // See if the view should be hidden or visible
  PRBool  viewIsVisible = PR_TRUE;
  PRBool  viewHasTransparentContent = (color->mBackgroundFlags &
            NS_STYLE_BG_COLOR_TRANSPARENT) == NS_STYLE_BG_COLOR_TRANSPARENT;

  if (NS_STYLE_VISIBILITY_COLLAPSE == disp->mVisible) {
    viewIsVisible = PR_FALSE;
  }
  else if (NS_STYLE_VISIBILITY_HIDDEN == disp->mVisible) {
    // If it has a widget, hide the view because the widget can't deal with it
    nsIWidget* widget = nsnull;
    aView->GetWidget(widget);
    if (widget) {
      viewIsVisible = PR_FALSE;
      NS_RELEASE(widget);
    }
    else {
      // If it's a scroll frame, then hide the view. This means that
      // child elements can't override their parent's visibility, but
      // it's not practical to leave it visible in all cases because
      // the scrollbars will be showing
      nsIAtom*  frameType;
      aFrame->GetFrameType(&frameType);

      if (frameType == nsLayoutAtoms::scrollFrame) {
        viewIsVisible = PR_FALSE;
      } else {
        // If it's a container element, then leave the view visible, but
        // mark it as having transparent content. The reason we need to
        // do this is that child elements can override their parent's
        // hidden visibility and be visible anyway
        nsIFrame* firstChild;
  
        aFrame->FirstChild(aPresContext, nsnull, &firstChild);
        if (firstChild) {
          // It's not a left frame, so the view needs to be visible, but
          // marked as having transparent content
          viewHasTransparentContent = PR_TRUE;
        } else {
          // It's a leaf frame so go ahead and hide the view
          viewIsVisible = PR_FALSE;
        }
      }
      NS_IF_RELEASE(frameType);
    }
  }

  // If the frame has visible content that overflows the content area, then we
  // need the view marked as having transparent content
  if (NS_STYLE_OVERFLOW_VISIBLE == disp->mOverflow) {
    nsFrameState  frameState;

    aFrame->GetFrameState(&frameState);
    if (frameState & NS_FRAME_OUTSIDE_CHILDREN) {
      viewHasTransparentContent = PR_TRUE;
    }
  }
  
  if (viewIsVisible) {
    aViewManager->SetViewContentTransparency(aView, viewHasTransparentContent);
    aViewManager->SetViewVisibility(aView, nsViewVisibility_kShow);
    aViewManager->UpdateView(aView, NS_VMREFRESH_NO_SYNC);
  }
  else {
    aViewManager->SetViewVisibility(aView, nsViewVisibility_kHide); 
  }
}

static void
UpdateViewsForTree(nsIPresContext* aPresContext, nsIFrame* aFrame, 
                   nsIViewManager* aViewManager, nsRect& aBoundsRect)
{
  nsIView* view;
  aFrame->GetView(aPresContext, &view);

  if (view) {
    SyncAndInvalidateView(aPresContext, view, aFrame, aViewManager);
  }

  nsRect bounds;
  aFrame->GetRect(bounds);
  nsPoint parentOffset(bounds.x, bounds.y);
  bounds.x = 0;
  bounds.y = 0;

  // now do children of frame
  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;
  nsIAtom* frameType = nsnull;

  do {
    nsIFrame* child = nsnull;
    aFrame->FirstChild(aPresContext, childList, &child);
    while (child) {
      nsFrameState  childState;
      child->GetFrameState(&childState);
      if (NS_FRAME_OUT_OF_FLOW != (childState & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        child->GetFrameType(&frameType);
        if (nsLayoutAtoms::placeholderFrame == frameType) { // placeholder
          // get out of flow frame and start over there
          nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

          ApplyRenderingChangeToTree(aPresContext, outOfFlowFrame, aViewManager);
        }
        else {  // regular frame
          nsRect  childBounds;
          UpdateViewsForTree(aPresContext, child, aViewManager, childBounds);
          bounds.UnionRect(bounds, childBounds);
        }
        NS_IF_RELEASE(frameType);
      }
      child->GetNextSibling(&child);
    }
    NS_IF_RELEASE(childList);
    aFrame->GetAdditionalChildListName(listIndex++, &childList);
  } while (childList);
  NS_IF_RELEASE(childList);
  aBoundsRect = bounds;
  aBoundsRect += parentOffset;
}

static void
ApplyRenderingChangeToTree(nsIPresContext* aPresContext,
                           nsIFrame* aFrame,
                           nsIViewManager* aViewManager)
{
  nsIViewManager* viewManager = aViewManager;

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.

  // XXX this needs to detect the need for a view due to an opacity change and deal with it...

  if (viewManager) {
    NS_ADDREF(viewManager); // add local ref
    viewManager->BeginUpdateViewBatch();
  }
  while (nsnull != aFrame) {
    // Get the frame's bounding rect
    nsRect invalidRect;
    nsPoint viewOffset;

    // Get view if this frame has one and trigger an update. If the
    // frame doesn't have a view, find the nearest containing view
    // (adjusting r's coordinate system to reflect the nesting) and
    // update there.
    nsIView* view = nsnull;
    aFrame->GetView(aPresContext, &view);
    nsIView* parentView;
    if (! view) { // XXX can view have children outside it?
      aFrame->GetOffsetFromView(aPresContext, viewOffset, &parentView);
      NS_ASSERTION(nsnull != parentView, "no view");
      if (! viewManager) {
        parentView->GetViewManager(viewManager);
        viewManager->BeginUpdateViewBatch();
      }
    }
    else {
      if (! viewManager) {
        view->GetViewManager(viewManager);
        viewManager->BeginUpdateViewBatch();
      }
    }
    UpdateViewsForTree(aPresContext, aFrame, viewManager, invalidRect);

    if (! view) { // if frame has view, will already be invalidated
      // XXX Instead of calling this we should really be calling
      // Invalidate on on the nsFrame (which does this)
      const nsStyleSpacing* spacing;
      aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
      nscoord width;
      spacing->GetOutlineWidth(width);
      if (width > 0) {
        invalidRect.Inflate(width, width);
      }
      nsPoint frameOrigin;
      aFrame->GetOrigin(frameOrigin);
      invalidRect -= frameOrigin;
      invalidRect += viewOffset;
      viewManager->UpdateView(parentView, invalidRect, NS_VMREFRESH_NO_SYNC);
    }

    aFrame->GetNextInFlow(&aFrame);
  }

  if (viewManager) {
    nsCOMPtr<nsIPresShell> presShell;
    nsresult rv = aPresContext->GetShell(getter_AddRefs(presShell));
    if (NS_SUCCEEDED(rv)) {
      PRBool isReflowLocked = PR_FALSE;
      presShell->IsReflowLocked(&isReflowLocked);
      if (isReflowLocked) {
        viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
      } else {
        viewManager->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
      }
    } else {
      viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
    }
//    viewManager->Composite();
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
  
  /*

    nsFrameState state;
    aFrame->GetFrameState(&state);
    state |= NS_FRAME_IS_DIRTY;
    aFrame->SetFrameState(state);
    nsIFrame* parent;
    aFrame->GetParent(&parent);
    parent->ReflowDirtyChild(shell, aFrame);
*/
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

    // Special check for text content that is a child of a letter
    // frame. There are two interesting cases that we have to handle
    // carefully: text content that is going empty (which means we
    // should select a new text node as the first-letter text) or text
    // content that empty but is no longer empty (it might be the
    // first-letter text but isn't currently).
    //
    // To deal with both of these we make a simple change: map a
    // ContentChanged into a ContentReplaced when we are changing text
    // that is part of a first-letter situation.
    PRBool doContentChanged = PR_TRUE;
    nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
    if (textContent) {
      // Ok, it's text content. Now do some real work...
      nsIFrame* block = GetFloaterContainingBlock(aPresContext, frame);
      if (block) {
        // See if the block has first-letter style applied to it.
        nsCOMPtr<nsIContent> blockContent;
        block->GetContent(getter_AddRefs(blockContent));
        nsCOMPtr<nsIStyleContext> blockSC;
        block->GetStyleContext(getter_AddRefs(blockSC));
        PRBool haveFirstLetterStyle =
          HaveFirstLetterStyle(aPresContext, blockContent, blockSC);
        if (haveFirstLetterStyle) {
          // The block has first-letter style. Use content-replaced to
          // repair the blocks frame structure properly.
          nsCOMPtr<nsIContent> container;
          aContent->GetParent(*getter_AddRefs(container));
          if (container) {
            PRInt32 ix;
            container->IndexOf(aContent, ix);
            doContentChanged = PR_FALSE;
            rv = ContentReplaced(aPresContext, container,
                                 aContent, aContent, ix);
          }
        }
      }
    }

    if (doContentChanged) {
      frame->ContentChanged(aPresContext, aContent, aSubContent);
    }
  }

  return rv;
}


NS_IMETHODIMP 
nsCSSFrameConstructor::ProcessRestyledFrames(nsStyleChangeList& aChangeList, 
                                             nsIPresContext* aPresContext)
{
  PRInt32 count = aChangeList.Count();
  while (0 < count--) {
    nsIFrame* frame;
    nsIContent* content;
    PRInt32 hint;
    aChangeList.ChangeAt(count, frame, content, hint);
    switch (hint) {
      case NS_STYLE_HINT_RECONSTRUCT_ALL:
        NS_ERROR("This shouldn't happen");
        break;
      case NS_STYLE_HINT_FRAMECHANGE:
        RecreateFramesForContent(aPresContext, content);
        break;
      case NS_STYLE_HINT_REFLOW:
        StyleChangeReflow(aPresContext, frame, nsnull);
        break;
      case NS_STYLE_HINT_VISUAL:
        ApplyRenderingChangeToTree(aPresContext, frame, nsnull);
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
nsCSSFrameConstructor::ContentStatesChanged(nsIPresContext* aPresContext, 
                                            nsIContent* aContent1,
                                            nsIContent* aContent2)
{
  nsresult  result = NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  NS_ASSERTION(shell, "couldn't get pres shell");
  if (shell) {
    nsIStyleSet* styleSet;
    shell->GetStyleSet(&styleSet);

    NS_ASSERTION(styleSet, "couldn't get style set");
    if (styleSet) { // test if any style rules exist which are dependent on content state
      nsIFrame* primaryFrame1 = nsnull;
      nsIFrame* primaryFrame2 = nsnull;
      if (aContent1 && (NS_OK == styleSet->HasStateDependentStyle(aPresContext, aContent1))) {
        shell->GetPrimaryFrameFor(aContent1, &primaryFrame1);
      }
      else {
        aContent1 = nsnull;
      }

      if (aContent2 && (aContent2 != aContent1) && 
          (NS_OK == styleSet->HasStateDependentStyle(aPresContext, aContent2))) {
        shell->GetPrimaryFrameFor(aContent2, &primaryFrame2);
      }
      else {
        aContent2 = nsnull;
      }
      NS_RELEASE(styleSet);

      if (primaryFrame1 && primaryFrame2) { // detect if one is parent of other, skip child
        nsIFrame* parent;
        primaryFrame1->GetParent(&parent);
        while (parent) {
          if (parent == primaryFrame2) {  // frame2 is frame1's parent, skip frame1
            primaryFrame1 = nsnull;
            break;
          }
          parent->GetParent(&parent);
        }
        if (primaryFrame1) {
          primaryFrame2->GetParent(&parent);
          while (parent) {
            if (parent == primaryFrame1) {  // frame1 is frame2's parent, skip frame2
              primaryFrame2 = nsnull;
              break;
            }
            parent->GetParent(&parent);
          }
        }
      }

      nsCOMPtr<nsIFrameManager> frameManager;
      shell->GetFrameManager(getter_AddRefs(frameManager));

      if (primaryFrame1) {
        nsStyleChangeList changeList1;
        nsStyleChangeList changeList2;
        PRInt32 frameChange1 = NS_STYLE_HINT_NONE;
        PRInt32 frameChange2 = NS_STYLE_HINT_NONE;
        frameManager->ComputeStyleChangeFor(aPresContext, primaryFrame1, 
                                            kNameSpaceID_Unknown, nsnull,
                                            changeList1, NS_STYLE_HINT_NONE, frameChange1);

        if ((frameChange1 != NS_STYLE_HINT_RECONSTRUCT_ALL) && (primaryFrame2)) {
          frameManager->ComputeStyleChangeFor(aPresContext, primaryFrame2, 
                                              kNameSpaceID_Unknown, nsnull,
                                              changeList2, NS_STYLE_HINT_NONE, frameChange2);
        }

        if ((frameChange1 == NS_STYLE_HINT_RECONSTRUCT_ALL) || 
            (frameChange2 == NS_STYLE_HINT_RECONSTRUCT_ALL)) {
          result = ReconstructDocElementHierarchy(aPresContext);
        }
        else {
          switch (frameChange1) {
            case NS_STYLE_HINT_FRAMECHANGE:
              result = RecreateFramesForContent(aPresContext, aContent1);
              changeList1.Clear();
              break;
            case NS_STYLE_HINT_REFLOW:
            case NS_STYLE_HINT_VISUAL:
            case NS_STYLE_HINT_CONTENT:
              // let primary frame deal with it
              result = primaryFrame1->ContentStateChanged(aPresContext, aContent1, frameChange1);
            default:
              break;
          }
          switch (frameChange2) {
            case NS_STYLE_HINT_FRAMECHANGE:
              result = RecreateFramesForContent(aPresContext, aContent2);
              changeList2.Clear();
              break;
            case NS_STYLE_HINT_REFLOW:
            case NS_STYLE_HINT_VISUAL:
            case NS_STYLE_HINT_CONTENT:
              // let primary frame deal with it
              result = primaryFrame2->ContentStateChanged(aPresContext, aContent2, frameChange2);
              // then process any children that need it
            default:
              break;
          }
          ProcessRestyledFrames(changeList1, aPresContext);
          ProcessRestyledFrames(changeList2, aPresContext);
        }
      }
      else if (primaryFrame2) {
        nsStyleChangeList changeList;
        PRInt32 frameChange = NS_STYLE_HINT_NONE;
        frameManager->ComputeStyleChangeFor(aPresContext, primaryFrame2, 
                                            kNameSpaceID_Unknown, nsnull,
                                            changeList, NS_STYLE_HINT_NONE, frameChange);

        switch (frameChange) {  // max change needed for top level frames
          case NS_STYLE_HINT_RECONSTRUCT_ALL:
            result = ReconstructDocElementHierarchy(aPresContext);
            changeList.Clear();
            break;
          case NS_STYLE_HINT_FRAMECHANGE:
            result = RecreateFramesForContent(aPresContext, aContent2);
            changeList.Clear();
            break;
          case NS_STYLE_HINT_REFLOW:
          case NS_STYLE_HINT_VISUAL:
          case NS_STYLE_HINT_CONTENT:
            // let primary frame deal with it
            result = primaryFrame2->ContentStateChanged(aPresContext, aContent2, frameChange);
            // then process any children that need it
          default:
            break;
        }
        ProcessRestyledFrames(changeList, aPresContext);
      }
      else {  // no frames, reconstruct for content
        if (aContent1) {
          result = RecreateFramesForContent(aPresContext, aContent1);
        }
        if (aContent2) {
          result = RecreateFramesForContent(aPresContext, aContent2);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsCSSFrameConstructor::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent* aContent,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aHint)
{
  nsresult  result = NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame*     primaryFrame;
   
  shell->GetPrimaryFrameFor(aContent, &primaryFrame);

#ifdef INCLUDE_XUL
  // The following tree widget trap prevents offscreen tree widget
  // content from being removed and re-inserted (which is what would
  // happen otherwise).
  if (!primaryFrame) {
    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::treechildren ||
                tag.get() == nsXULAtoms::treeitem))
      return NS_OK;
  }
#endif // INCLUDE_XUL

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
    result = aContent->QueryInterface(NS_GET_IID(nsIStyledContent), (void**)&styledContent);

    if (NS_OK == result) { 
      // Get style hint from HTML content object. 
      styledContent->GetMappedAttributeImpact(aAttribute, aHint);
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
      nsStyleChangeList changeList;
      // put primary frame on list to deal with, re-resolve may update or add next in flows
      changeList.AppendChange(primaryFrame, aContent, maxHint);
      nsCOMPtr<nsIFrameManager> frameManager;
      shell->GetFrameManager(getter_AddRefs(frameManager));
      frameManager->ComputeStyleChangeFor(aPresContext, primaryFrame, 
                                          aNameSpaceID, aAttribute,
                                          changeList, aHint, maxHint);

      switch (maxHint) {  // maxHint is hint for primary only
        case NS_STYLE_HINT_RECONSTRUCT_ALL:
          result = ReconstructDocElementHierarchy(aPresContext);
          changeList.Clear();
          break;
        case NS_STYLE_HINT_FRAMECHANGE:
          result = RecreateFramesForContent(aPresContext, aContent);
          changeList.Clear();
          break;
        case NS_STYLE_HINT_REFLOW:
        case NS_STYLE_HINT_VISUAL:
        case NS_STYLE_HINT_CONTENT:
          // let the frame deal with it, since we don't know how to
          result = primaryFrame->AttributeChanged(aPresContext, aContent, aNameSpaceID, aAttribute, maxHint);
        default:
          break;
      }
      // handle any children (primary may be on list too)
      ProcessRestyledFrames(changeList, aPresContext);
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

  if (restyle) {
    nsIStyleContext* sc;
    frame->GetStyleContext(&sc);
    sc->RemapStyle(aPresContext);
    NS_RELEASE(sc);
  }

  if (reframe) {
    result = ReconstructDocElementHierarchy(aPresContext);
  }
  else {
    // XXX hack, skip the root and scrolling frames
    frame->FirstChild(aPresContext, nsnull, &frame);
    frame->FirstChild(aPresContext, nsnull, &frame);
    if (reflow) {
      StyleChangeReflow(aPresContext, frame, nsnull);
    }
    else if (render) {
      ApplyRenderingChangeToTree(aPresContext, frame, nsnull);
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

static void
GetAlternateTextFor(nsIContent* aContent,
                    nsIAtom*    aTag,  // content object's tag
                    nsString&   aAltText)
{
  nsresult  rv;

  // The "alt" attribute specifies alternate text that is rendered
  // when the image can not be displayed
  rv = aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::alt, aAltText);
  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // If there's no "alt" attribute, then use the value of the "title"
    // attribute. Note that this is not the same as a value of ""
    rv = aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::title, aAltText);
  }
  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // If there's no "title" attribute, then what we do depends on the type
    // of element
    if (nsHTMLAtoms::img == aTag) {
      // Use the filename minus the extension
      aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, aAltText);
      if (aAltText.Length() > 0) {
        // The path is a hierarchical structure of segments. Get the last substring
        // in the path
        PRInt32 offset = aAltText.RFindChar('/');
        if (offset >= 0) {
          aAltText.Cut(0, offset + 1);
        }

        // Ignore fragment identifiers ('#' delimiter), query strings ('?'
        // delimiter), and anything beginning with ';'
        offset = aAltText.FindCharInSet("#?;");
        if (offset >= 0) {
          aAltText.Truncate(offset);
        }

        // Trim off any extension (including the '.')
        offset = aAltText.RFindChar('.');
        if (offset >= 0) {
          aAltText.Truncate(offset);
        }
      }

    } else if (nsHTMLAtoms::input == aTag) {
      // Use the valuf of the "name" attribute
      aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, aAltText);
    }
  }
}

// Construct an alternate frame to use when the image can't be rendered
nsresult
nsCSSFrameConstructor::ConstructAlternateImageFrame(nsIPresShell*    aPresShell, 
                                                    nsIPresContext*  aPresContext,
                                                    nsIContent*      aContent,
                                                    nsIStyleContext* aStyleContext,
                                                    nsIFrame*        aParentFrame,
                                                    nsIFrame*&       aFrame)
{
  nsAutoString  altText;

  // Initialize OUT parameter
  aFrame = nsnull;

  // Get the alternate text to use
  GetAlternateTextFor(aContent, nsHTMLAtoms::img, altText);

  // Create a text content element for the alternate text
  nsCOMPtr<nsIContent> altTextContent;
  NS_NewTextNode(getter_AddRefs(altTextContent));

  // Set the content's text
  nsIDOMCharacterData* domData;
  altTextContent->QueryInterface(kIDOMCharacterDataIID, (void**)&domData);
  domData->SetData(altText);
  NS_RELEASE(domData);
  
  // Set aContent as the parent content and set the document object
  nsCOMPtr<nsIDocument> document;
  aContent->GetDocument(*getter_AddRefs(document));
  altTextContent->SetParent(aContent);
  altTextContent->SetDocument(document, PR_TRUE);

  // Create either an inline frame, block frame, or area frame
  nsIFrame* containerFrame;
  PRBool    isOutOfFlow = PR_FALSE;
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);

  if (position->IsAbsolutelyPositioned()) {
    NS_NewAbsoluteItemWrapperFrame(aPresShell, &containerFrame);
    isOutOfFlow = PR_TRUE;
  } else if (display->IsFloating()) {
    NS_NewFloatingItemWrapperFrame(aPresShell, &containerFrame);
    isOutOfFlow = PR_TRUE;
  } else if (NS_STYLE_DISPLAY_BLOCK == display->mDisplay) {
    NS_NewBlockFrame(aPresShell, &containerFrame);
  } else {
    NS_NewInlineFrame(aPresShell, &containerFrame);
  }
  containerFrame->Init(aPresContext, aContent, aParentFrame, aStyleContext, nsnull);
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, containerFrame,
                                           aStyleContext, PR_FALSE);

  // If the frame is out-of-flow, then mark it as such
  nsFrameState  frameState;
  containerFrame->GetFrameState(&frameState);
  containerFrame->SetFrameState(frameState | NS_FRAME_OUT_OF_FLOW);

  // Create a text frame to display the alt-text. It gets a pseudo-element
  // style context
  nsIFrame*        textFrame;
  nsIStyleContext* textStyleContext;

  NS_NewTextFrame(aPresShell, &textFrame);
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::textPseudo,
                                             aStyleContext, PR_FALSE,
                                             &textStyleContext);

  textFrame->Init(aPresContext, altTextContent, containerFrame,
                  textStyleContext, nsnull);
  NS_RELEASE(textStyleContext);
  containerFrame->SetInitialChildList(aPresContext, nsnull, textFrame);

  // Return the container frame
  aFrame = containerFrame;

  return NS_OK;
}

#ifdef NS_DEBUG
static PRBool
IsPlaceholderFrame(nsIFrame* aFrame)
{
  nsIAtom*  frameType;
  PRBool    result;

  aFrame->GetFrameType(&frameType);
  result = frameType == nsLayoutAtoms::placeholderFrame;
  NS_IF_RELEASE(frameType);
  return result;
}
#endif

NS_IMETHODIMP
nsCSSFrameConstructor::CantRenderReplacedElement(nsIPresShell* aPresShell, 
                                                 nsIPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  nsIFrame*                 parentFrame;
  nsCOMPtr<nsIStyleContext> styleContext;
  nsCOMPtr<nsIContent>      content;
  nsCOMPtr<nsIAtom>         tag;
  nsresult                  rv = NS_OK;

  aFrame->GetParent(&parentFrame);
  aFrame->GetStyleContext(getter_AddRefs(styleContext));

  // Get aFrame's content object and the tag name
  aFrame->GetContent(getter_AddRefs(content));
  NS_ASSERTION(content, "null content object");
  content->GetTag(*getter_AddRefs(tag));

  // Get the child list name that the frame is contained in
  nsCOMPtr<nsIAtom>  listName;
  GetChildListNameFor(aPresContext, parentFrame, aFrame, getter_AddRefs(listName));

  // If the frame is out of the flow, then it has a placeholder frame.
  nsIFrame* placeholderFrame = nsnull;
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (listName) {
    presShell->GetPlaceholderFrameFor(aFrame, &placeholderFrame);
  }

  // Get the previous sibling frame
  nsIFrame*     firstChild;
  parentFrame->FirstChild(aPresContext, listName, &firstChild);
  nsFrameList   frameList(firstChild);
  
  // See whether it's an IMG or an OBJECT element
  if (nsHTMLAtoms::img == tag.get()) {
    // It's an IMG element. Try and construct an alternate frame to use when the
    // image can't be rendered
    nsIFrame* newFrame;
    rv = ConstructAlternateImageFrame(aPresShell, aPresContext, content, styleContext,
                                      parentFrame, newFrame);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFrameManager> frameManager;
      presShell->GetFrameManager(getter_AddRefs(frameManager));

      // Replace the old frame with the new frame
      // Reset the primary frame mapping
      frameManager->SetPrimaryFrameFor(content, newFrame);

      if (placeholderFrame) {
        // Reuse the existing placeholder frame, and add an association to the
        // new frame
        frameManager->SetPlaceholderFrameFor(newFrame, placeholderFrame);
      }

      // Replace the old frame with the new frame
      frameManager->ReplaceFrame(aPresContext, *presShell, parentFrame,
                                 listName, aFrame, newFrame);

      // Now that we've replaced the primary frame, if there's a placeholder
      // frame then complete the transition from image frame to new frame
      if (placeholderFrame) {
        // Remove the association between the old frame and its placeholder
        frameManager->SetPlaceholderFrameFor(aFrame, nsnull);
        
        // Placeholder frames have a pointer back to the out-of-flow frame.
        // Make sure that's correct, too.
        ((nsPlaceholderFrame*)placeholderFrame)->SetOutOfFlowFrame(newFrame);

        // XXX Work around a bug in the block code where the floater won't get
        // reflowed unless the line containing the placeholder frame is reflowed...
        nsIFrame* placeholderParentFrame;
        placeholderFrame->GetParent(&placeholderParentFrame);
        placeholderParentFrame->ReflowDirtyChild(aPresShell, placeholderFrame);
      }
    }

  } else if ((nsHTMLAtoms::object == tag.get()) ||
             (nsHTMLAtoms::embed == tag.get()) ||
             (nsHTMLAtoms::applet == tag.get())) {

    // It's an OBJECT, EMBED, or APPLET, so we should display the contents
    // instead
    nsIFrame* absoluteContainingBlock;
    nsIFrame* floaterContainingBlock;
    nsIFrame* inFlowParent = parentFrame;

    // If the OBJECT frame is out-of-flow, then get the placeholder frame's
    // parent and use that when determining the absolute containing block and
    // floater containing block
    if (placeholderFrame) {
      placeholderFrame->GetParent(&inFlowParent);
    }

    absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext, inFlowParent),
    floaterContainingBlock = GetFloaterContainingBlock(aPresContext, inFlowParent);

#ifdef NS_DEBUG
    // Verify that we calculated the same containing block
    if (listName.get() == nsLayoutAtoms::absoluteList) {
      NS_ASSERTION(absoluteContainingBlock == parentFrame,
                   "wrong absolute containing block");
    } else if (listName.get() == nsLayoutAtoms::floaterList) {
      NS_ASSERTION(floaterContainingBlock == parentFrame,
                   "wrong floater containing block");
    }
#endif

    // Now initialize the frame construction state
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                  absoluteContainingBlock, floaterContainingBlock, nsnull);
    nsFrameItems            frameItems;
    const nsStyleDisplay*   display = (const nsStyleDisplay*)
      styleContext->GetStyleData(eStyleStruct_Display);

    // Create a new frame based on the display type.
    // Note: if the old frame was out-of-flow, then so will the new frame
    // and we'll get a new placeholder frame
    rv = ConstructFrameByDisplayType(aPresShell, aPresContext, state, display, content,
                                     inFlowParent, styleContext, frameItems);

    if (NS_SUCCEEDED(rv)) {
      nsIFrame* newFrame = frameItems.childList;

      if (placeholderFrame) {
        // Remove the association between the old frame and its placeholder
        // Note: ConstructFrameByDisplayType() will already have added an
        // association for the new placeholder frame
        state.mFrameManager->SetPlaceholderFrameFor(aFrame, nsnull);

        // Verify that the new frame is also a placeholder frame
        NS_ASSERTION(IsPlaceholderFrame(newFrame), "unexpected frame type");

        // Replace the old placeholder frame with the new placeholder frame
        state.mFrameManager->ReplaceFrame(aPresContext, *presShell, inFlowParent,
                                          nsnull, placeholderFrame, newFrame);
      }

      // Replace the primary frame
      if (listName.get() == nsLayoutAtoms::absoluteList) {
        newFrame = state.mAbsoluteItems.childList;
        state.mAbsoluteItems.childList = nsnull;
      } else if (listName.get() == nsLayoutAtoms::fixedList) {
        newFrame = state.mFixedItems.childList;
        state.mFixedItems.childList = nsnull;
      } else if (listName.get() == nsLayoutAtoms::floaterList) {
        newFrame = state.mFloatedItems.childList;
        state.mFloatedItems.childList = nsnull;
      }
      state.mFrameManager->ReplaceFrame(aPresContext, *presShell, parentFrame,
                                        listName, aFrame, newFrame);

      // Reset the primary frame mapping. Don't assume that
      // ConstructFrameByDisplayType() has done this
      state.mFrameManager->SetPrimaryFrameFor(content, newFrame);
      
      // If there are new absolutely positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mAbsoluteItems.childList) {
        rv = state.mAbsoluteItems.containingBlock->AppendFrames(aPresContext, *presShell,
                                                     nsLayoutAtoms::absoluteList,
                                                     state.mAbsoluteItems.childList);
      }
  
      // If there are new fixed positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFixedItems.childList) {
        rv = state.mFixedItems.containingBlock->AppendFrames(aPresContext, *presShell,
                                                  nsLayoutAtoms::fixedList,
                                                  state.mFixedItems.childList);
      }
  
      // If there are new floating child frames, then notify the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFloatedItems.childList) {
        rv = state.mFloatedItems.containingBlock->AppendFrames(aPresContext,
                                                    *presShell,
                                                    nsLayoutAtoms::floaterList,
                                                    state.mFloatedItems.childList);
      }
    }

  } else if (nsHTMLAtoms::input == tag.get()) {
    // XXX image INPUT elements are also image frames, but don't throw away the
    // image frame, because the frame class has extra logic that is specific to
    // INPUT elements

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected tag");
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingOuterTableFrame(nsIPresShell* aPresShell, 
                                                       nsIPresContext*  aPresContext,
                                                       nsIFrame*        aFrame,
                                                       nsIFrame*        aParentFrame,
                                                       nsIContent*      aContent,
                                                       nsIStyleContext* aStyleContext,
                                                       nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;

  rv = NS_NewTableOuterFrame(aPresShell, &newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Create a continuing inner table frame, and if there's a caption then
    // replicate the caption
    nsIFrame*     childFrame;
    nsFrameItems  newChildFrames;

    aFrame->FirstChild(aPresContext, nsnull, &childFrame);
    while (childFrame) {
      nsIAtom*  tableType;

      // See if it's the inner table frame
      childFrame->GetFrameType(&tableType);
      if (nsLayoutAtoms::tableFrame == tableType) {
        nsIFrame* continuingTableFrame;

        // It's the inner table frame, so create a continuing frame
        CreateContinuingFrame(aPresShell, aPresContext, childFrame, newFrame, &continuingTableFrame);
        newChildFrames.AddChild(continuingTableFrame);
      } else {
        nsIContent*           caption;
        nsIStyleContext*      captionStyle;
        const nsStyleDisplay* display;

        childFrame->GetContent(&caption);
        childFrame->GetStyleContext(&captionStyle);
        display = (const nsStyleDisplay*)captionStyle->GetStyleData(eStyleStruct_Display);
        NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_CAPTION == display->mDisplay, "expected caption");

        // Replicate the caption frame
        // XXX We have to do it this way instead of calling ConstructFrameByDisplayType(),
        // because of a bug in the way ConstructTableFrame() handles the initial child
        // list...
        nsIFrame*               captionFrame;
        nsFrameItems            childItems;
        NS_NewTableCaptionFrame(aPresShell, &captionFrame);
        nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                      GetAbsoluteContainingBlock(aPresContext, newFrame),
                                      captionFrame, nsnull);
        captionFrame->Init(aPresContext, caption, newFrame, captionStyle, nsnull);
        ProcessChildren(aPresShell, aPresContext, state, caption, captionFrame,
                        PR_TRUE, childItems, PR_TRUE);
        captionFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
        // XXX Deal with absolute and fixed frames...
        if (state.mFloatedItems.childList) {
          captionFrame->SetInitialChildList(aPresContext,
                                            nsLayoutAtoms::floaterList,
                                            state.mFloatedItems.childList);
        }
        newChildFrames.AddChild(captionFrame);
        NS_RELEASE(caption);
        NS_RELEASE(captionStyle);
      }
      NS_IF_RELEASE(tableType);
      childFrame->GetNextSibling(&childFrame);
    }

    // Set the outer table's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, newChildFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingTableFrame(nsIPresShell* aPresShell, 
                                                  nsIPresContext*  aPresContext,
                                                  nsIFrame*        aFrame,
                                                  nsIFrame*        aParentFrame,
                                                  nsIContent*      aContent,
                                                  nsIStyleContext* aStyleContext,
                                                  nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;
    
  rv = NS_NewTableFrame(aPresShell, &newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, PR_FALSE);

    // Replicate any header/footer frames
    nsIFrame*     rowGroupFrame;
    nsFrameItems  childFrames;

    aFrame->FirstChild(aPresContext, nsnull, &rowGroupFrame);
    while (rowGroupFrame) {
      // See if it's a header/footer
      nsIStyleContext*      rowGroupStyle;
      const nsStyleDisplay* display;

      rowGroupFrame->GetStyleContext(&rowGroupStyle);
      display = (const nsStyleDisplay*)rowGroupStyle->GetStyleData(eStyleStruct_Display);

      if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay)) {
        
        // Replicate the header/footer frame
        nsIFrame*               headerFooterFrame;
        nsFrameItems            childItems;
        nsIContent*             headerFooter;
        nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                      GetAbsoluteContainingBlock(aPresContext, newFrame),
                                      nsnull, nsnull);

        NS_NewTableRowGroupFrame(aPresShell, &headerFooterFrame);
        rowGroupFrame->GetContent(&headerFooter);
        headerFooterFrame->Init(aPresContext, headerFooter, newFrame,
                                rowGroupStyle, nsnull);
        ProcessChildren(aPresShell, aPresContext, state, headerFooter, headerFooterFrame,
                        PR_FALSE, childItems, PR_FALSE);
        NS_ASSERTION(!state.mFloatedItems.childList, "unexpected floated element");
        NS_RELEASE(headerFooter);
        headerFooterFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

        // Table specific initialization
        ((nsTableRowGroupFrame*)headerFooterFrame)->InitRepeatedFrame
          (aPresContext, (nsTableRowGroupFrame*)rowGroupFrame);

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
    newFrame->SetInitialChildList(aPresContext, nsnull, childFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

NS_IMETHODIMP
nsCSSFrameConstructor::CreateContinuingFrame(nsIPresShell* aPresShell, 
                                             nsIPresContext* aPresContext,
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
    rv = NS_NewContinuingTextFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
    
  } else if (nsLayoutAtoms::inlineFrame == frameType) {
    rv = NS_NewInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::blockFrame == frameType) {
    rv = NS_NewBlockFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::areaFrame == frameType) {
    rv = NS_NewAreaFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext,
                     aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::positionedInlineFrame == frameType) {
    rv = NS_NewPositionedInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }

  } else if (nsLayoutAtoms::pageFrame == frameType) {
    rv = NS_NewPageFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_TRUE);
    }

  } else if (nsLayoutAtoms::tableOuterFrame == frameType) {
    rv = CreateContinuingOuterTableFrame(aPresShell, aPresContext, aFrame, aParentFrame,
                                         content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableFrame == frameType) {
    rv = CreateContinuingTableFrame(aPresShell, aPresContext, aFrame, aParentFrame,
                                    content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
    rv = NS_NewTableRowGroupFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }

  } else if (nsLayoutAtoms::tableRowFrame == frameType) {
    rv = NS_NewTableRowFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);

      // Create a continuing frame for each table cell frame
      nsIFrame*     cellFrame;
      nsFrameItems  newChildList;

      aFrame->FirstChild(aPresContext, nsnull, &cellFrame);
      while (cellFrame) {
        nsIAtom*  tableType;
        
        // See if it's a table cell frame
        cellFrame->GetFrameType(&tableType);
        if (nsLayoutAtoms::tableCellFrame == tableType) {
          nsIFrame* continuingCellFrame;

          CreateContinuingFrame(aPresShell, aPresContext, cellFrame, newFrame, &continuingCellFrame);
          newChildList.AddChild(continuingCellFrame);
        }

        NS_IF_RELEASE(tableType);
        cellFrame->GetNextSibling(&cellFrame);
      }
      
      // Set the table cell's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, newChildList.childList);
    }

  } else if (nsLayoutAtoms::tableCellFrame == frameType) {
    rv = NS_NewTableCellFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);

      // Create a continuing area frame
      nsIFrame* areaFrame;
      nsIFrame* continuingAreaFrame;
      aFrame->FirstChild(aPresContext, nsnull, &areaFrame);
      CreateContinuingFrame(aPresShell, aPresContext, areaFrame, newFrame, &continuingAreaFrame);

      // Set the table cell's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, continuingAreaFrame);
    }
  
  } else if (nsLayoutAtoms::lineFrame == frameType) {
    rv = NS_NewFirstLineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::letterFrame == frameType) {
    rv = NS_NewFirstLetterFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, PR_FALSE);
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

// Helper function that searches the immediate child frames for a frame that
// maps the specified content object
static nsIFrame*
FindFrameWithContent(nsIPresContext* aPresContext,
                     nsIFrame*       aParentFrame,
                     nsIContent*     aParentContent,
                     nsIContent*     aContent)
{
  NS_PRECONDITION(aParentFrame, "No frame to search!");
  if (!aParentFrame) {
    return nsnull;
  }

keepLooking:
  // Search for the frame in each child list that aParentFrame supports
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* kidFrame;
    aParentFrame->FirstChild(aPresContext, listName, &kidFrame);
    while (kidFrame) {
      nsCOMPtr<nsIContent>  kidContent;
      
      // See if the child frame points to the content object we're
      // looking for
      kidFrame->GetContent(getter_AddRefs(kidContent));
      if (kidContent.get() == aContent) {
        nsCOMPtr<nsIAtom>  frameType;

        // We found a match. See if it's a placeholder frame
        kidFrame->GetFrameType(getter_AddRefs(frameType));
        if (nsLayoutAtoms::placeholderFrame == frameType.get()) {
          // Ignore the placeholder and return the out-of-flow frame instead
          return ((nsPlaceholderFrame*)kidFrame)->GetOutOfFlowFrame();
        } else {
          // Return the matching child frame
          return kidFrame;
        }
      }

      // We search the immediate children only, but if the child frame has
      // the same content pointer as its parent then we need to search its
      // child frames, too
      if (kidContent.get() == aParentContent) {
        nsIFrame* matchingFrame = FindFrameWithContent(aPresContext, kidFrame, aParentContent,
                                                       aContent);

        if (matchingFrame) {
          return matchingFrame;
        }
      }

      // Get the next sibling frame
      kidFrame->GetNextSibling(&kidFrame);
    }

    NS_IF_RELEASE(listName);
    aParentFrame->GetAdditionalChildListName(listIndex++, &listName);
  } while(listName);

  // We didn't find a matching frame. If aFrame has a next-in-flow,
  // then continue looking there
  aParentFrame->GetNextInFlow(&aParentFrame);
  if (aParentFrame) {
    goto keepLooking;
  }

  // No matching frame
  return nsnull;
}

// Request to find the primary frame associated with a given content object.
// This is typically called by the pres shell when there is no mapping in
// the pres shell hash table
NS_IMETHODIMP
nsCSSFrameConstructor::FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                           nsIFrameManager* aFrameManager,
                                           nsIContent*      aContent,
                                           nsIFrame**       aFrame)
{
  *aFrame = nsnull;  // initialize OUT parameter 

  // Get the pres shell
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // We want to be able to quickly map from a content object to its frame,
  // but we also want to keep the hash table small. Therefore, many frames
  // are not added to the hash table when they're first created:
  // - text frames
  // - inline frames (often things like FONT and B)
  // - BR frames
  // - internal table frames (row-group, row, cell, col-group, col)
  //
  // That means we need to need to search for the frame
  nsCOMPtr<nsIContent>   parentContent; // we get this one time
  nsIFrame*              parentFrame;   // this pointer is used to iterate across all frames that map to parentContent

  // Get the frame that corresponds to the parent content object.
  // Note that this may recurse indirectly, because the pres shell will
  // call us back if there is no mapping in the hash table
  aContent->GetParent(*getter_AddRefs(parentContent));
  if (parentContent.get()) {
    aFrameManager->GetPrimaryFrameFor(parentContent, &parentFrame);
    while (parentFrame) {
      // Search the child frames for a match
      *aFrame = FindFrameWithContent(aPresContext, parentFrame, parentContent.get(), aContent);

      // If we found a match, then add a mapping to the hash table so
      // next time this will be quick
      if (*aFrame) {
        aFrameManager->SetPrimaryFrameFor(aContent, *aFrame);
        break;
      }
      else { 
        // We need to check parentFrame's sibling frame as well 
        parentFrame->GetNextSibling(&parentFrame); 
        if (!parentFrame) {
          break;
        }
        else {
          nsCOMPtr<nsIContent>nextParentContent;
          parentFrame->GetContent(getter_AddRefs(nextParentContent));
          if (parentContent!=nextParentContent) {
            break; 
          }
        } 
      }
    }
  }

  return NS_OK;
}

// Capture state for the frame tree rooted at the frame associated with the
// content object, aContent
nsresult
nsCSSFrameConstructor::CaptureStateForFramesOf(nsIPresContext* aPresContext,
                                               nsIContent* aContent,
                                               nsILayoutHistoryState* aHistoryState)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = NS_OK;

  rv = aPresContext->GetShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv) && presShell) {                    
    nsIFrame* frame;
    rv = presShell->GetPrimaryFrameFor(aContent, &frame);
    if (NS_SUCCEEDED(rv) && frame) {
      CaptureStateFor(aPresContext, frame, aHistoryState);
    }
  }
  return rv;
}

// Capture state for the frame tree rooted at aFrame.
nsresult
nsCSSFrameConstructor::CaptureStateFor(nsIPresContext* aPresContext,
                                       nsIFrame* aFrame,
                                       nsILayoutHistoryState* aHistoryState)
{
  nsresult rv = NS_OK;

  if (aFrame && aPresContext && aHistoryState) {
    nsCOMPtr<nsIPresShell> presShell;
    rv = aPresContext->GetShell(getter_AddRefs(presShell));
    if (NS_SUCCEEDED(rv) && presShell) {                    
      nsCOMPtr<nsIFrameManager> frameManager;
      rv = presShell->GetFrameManager(getter_AddRefs(frameManager));
      if (NS_SUCCEEDED(rv) && frameManager) {
        rv = frameManager->CaptureFrameState(aPresContext, aFrame, aHistoryState);
      }
    }
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::RecreateFramesForContent(nsIPresContext* aPresContext,
                                                nsIContent* aContent)                                   
{
  nsresult rv = NS_OK;

  nsIContent* container;
  rv = aContent->GetParent(container);
  if (NS_SUCCEEDED(rv) && container) {
    PRInt32 indexInContainer;    
    rv = container->IndexOf(aContent, indexInContainer);
    if (NS_SUCCEEDED(rv)) {
      // Before removing the frames associated with the content object, ask them to save their
      // state onto a temporary state object.
      CaptureStateForFramesOf(aPresContext, aContent, mTempFrameTreeState);

      // Remove the frames associated with the content object on which the
      // attribute change occurred.
      rv = ContentRemoved(aPresContext, container, aContent, indexInContainer);
  
      if (NS_SUCCEEDED(rv)) {
        // Now, recreate the frames associated with this content object.
        rv = ContentInserted(aPresContext, container, aContent, indexInContainer, mTempFrameTreeState);
      }      
    }
    NS_RELEASE(container);
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////

// Block frame construction code

nsIStyleContext*
nsCSSFrameConstructor::GetFirstLetterStyle(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIStyleContext* aStyleContext)
{
  nsIStyleContext* fls = nsnull;
  aPresContext->ResolvePseudoStyleContextFor(aContent,
                                             nsHTMLAtoms::firstLetterPseudo,
                                             aStyleContext, PR_FALSE, &fls);
  return fls;
}

nsIStyleContext*
nsCSSFrameConstructor::GetFirstLineStyle(nsIPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsIStyleContext* aStyleContext)
{
  nsIStyleContext* fls = nsnull;
  aPresContext->ResolvePseudoStyleContextFor(aContent,
                                             nsHTMLAtoms::firstLinePseudo,
                                             aStyleContext, PR_FALSE, &fls);
  return fls;
}

// Predicate to see if a given content (block element) has
// first-letter style applied to it.
PRBool
nsCSSFrameConstructor::HaveFirstLetterStyle(nsIPresContext* aPresContext,
                                            nsIContent* aContent,
                                            nsIStyleContext* aStyleContext)
{
  nsCOMPtr<nsIStyleContext> fls;
  aPresContext->ProbePseudoStyleContextFor(aContent,
                                           nsHTMLAtoms::firstLetterPseudo,
                                           aStyleContext, PR_FALSE,
                                           getter_AddRefs(fls));
  PRBool result = PR_FALSE;
  if (fls) {
    result = PR_TRUE;
  }
  return result;
}

PRBool
nsCSSFrameConstructor::HaveFirstLineStyle(nsIPresContext* aPresContext,
                                          nsIContent* aContent,
                                          nsIStyleContext* aStyleContext)
{
  nsCOMPtr<nsIStyleContext> fls;
  aPresContext->ProbePseudoStyleContextFor(aContent,
                                           nsHTMLAtoms::firstLinePseudo,
                                           aStyleContext, PR_FALSE,
                                           getter_AddRefs(fls));
  PRBool result = PR_FALSE;
  if (fls) {
    result = PR_TRUE;
  }
  return result;
}

void
nsCSSFrameConstructor::HaveSpecialBlockStyle(nsIPresContext* aPresContext,
                                             nsIContent* aContent,
                                             nsIStyleContext* aStyleContext,
                                             PRBool* aHaveFirstLetterStyle,
                                             PRBool* aHaveFirstLineStyle)
{
  *aHaveFirstLetterStyle =
    HaveFirstLetterStyle(aPresContext, aContent, aStyleContext);
  *aHaveFirstLineStyle =
    HaveFirstLineStyle(aPresContext, aContent, aStyleContext);
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
nsCSSFrameConstructor::ProcessChildren(nsIPresShell* aPresShell, 
                                       nsIPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aFrame,
                                       PRBool                   aCanHaveGeneratedContent,
                                       nsFrameItems&            aFrameItems,
                                       PRBool                   aParentIsBlock)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIStyleContext> styleContext;

  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    aFrame->GetStyleContext(getter_AddRefs(styleContext));
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::beforePseudo,
                                    aParentIsBlock, &generatedFrame)) {
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
      rv = ConstructFrame(aPresShell, aPresContext, aState, childContent, aFrame, aFrameItems);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::afterPseudo,
                                    aParentIsBlock, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  if (aParentIsBlock) {
    if (aState.mFirstLetterStyle) {
      rv = WrapFramesInFirstLetterFrame(aPresShell, aPresContext, aState, aContent, aFrame, aFrameItems);
    }
    if (aState.mFirstLineStyle) {
      rv = WrapFramesInFirstLineFrame(aPresShell, aPresContext, aState, aContent, aFrame, aFrameItems);
    }
  }

  return rv;
}

//----------------------------------------------------------------------

// Support for :first-line style

// XXX this predicate and its cousins need to migrated to a single
// place in layout - something in nsStyleDisplay maybe?
static PRBool
IsInlineFrame(nsIFrame* aFrame)
{
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  switch (display->mDisplay) {
    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_INLINE_BLOCK:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
      return PR_TRUE;
    default:
      break;
  }
  return PR_FALSE;
}

static void
ReparentFrame(nsIPresContext* aPresContext,
              nsIFrame* aNewParentFrame,
              nsIStyleContext* aParentStyleContext,
              nsIFrame* aFrame)
{
  aPresContext->ReParentStyleContext(aFrame, aParentStyleContext);
  aFrame->SetParent(aNewParentFrame);
}

// Special routine to handle placing a list of frames into a block
// frame that has first-line style. The routine ensures that the first
// collection of inline frames end up in a first-line frame.
nsresult
nsCSSFrameConstructor::WrapFramesInFirstLineFrame(
  nsIPresShell* aPresShell, 
  nsIPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aFrame,
  nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;

  // Find the first and last inline frame in aFrameItems
  nsIFrame* kid = aFrameItems.childList;
  nsIFrame* firstInlineFrame = nsnull;
  nsIFrame* lastInlineFrame = nsnull;
  while (kid) {
    if (IsInlineFrame(kid)) {
      if (!firstInlineFrame) firstInlineFrame = kid;
      lastInlineFrame = kid;
    }
    else {
      break;
    }
    kid->GetNextSibling(&kid);
  }

  // If we don't find any inline frames, then there is nothing to do
  if (!firstInlineFrame) {
    return rv;
  }

  // Create line frame
  nsCOMPtr<nsIStyleContext> parentStyle;
  aFrame->GetStyleContext(getter_AddRefs(parentStyle));
  nsCOMPtr<nsIStyleContext> firstLineStyle(
    getter_AddRefs(GetFirstLineStyle(aPresContext, aContent, parentStyle))
    );
  nsIFrame* lineFrame;
  rv = NS_NewFirstLineFrame(aPresShell, &lineFrame);
  if (NS_SUCCEEDED(rv)) {
    // Initialize the line frame
    rv = InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aFrame, firstLineStyle, nsnull, lineFrame);    

    // Mangle the list of frames we are giving to the block: first
    // chop the list in two after lastInlineFrame
    nsIFrame* secondBlockFrame;
    lastInlineFrame->GetNextSibling(&secondBlockFrame);
    lastInlineFrame->SetNextSibling(nsnull);

    // The lineFrame will be the block's first child; the rest of the
    // frame list (after lastInlineFrame) will be the second and
    // subsequent children; join the list together and reset
    // aFrameItems appropriately.
    if (secondBlockFrame) {
      lineFrame->SetNextSibling(secondBlockFrame);
    }
    if (aFrameItems.childList == lastInlineFrame) {
      // Just in case the block had exactly one inline child
      aFrameItems.lastChild = lineFrame;
    }
    aFrameItems.childList = lineFrame;

    // Give the inline frames to the lineFrame <b>after</b> reparenting them
    kid = firstInlineFrame;
    while (kid) {
      ReparentFrame(aPresContext, lineFrame, firstLineStyle, kid);
      kid->GetNextSibling(&kid);
    }
    lineFrame->SetInitialChildList(aPresContext, nsnull, firstInlineFrame);
  }

  return rv;
}

// Special routine to handle appending a new frame to a block frame's
// child list. Takes care of placing the new frame into the right
// place when first-line style is present.
nsresult
nsCSSFrameConstructor::AppendFirstLineFrames(
  nsIPresShell* aPresShell, 
  nsIPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aFrameItems)
{
  // It's possible that aBlockFrame needs to have a first-line frame
  // created because it doesn't currently have any children.
  nsIFrame* blockKid;
  aBlockFrame->FirstChild(aPresContext, nsnull, &blockKid);
  if (!blockKid) {
    return WrapFramesInFirstLineFrame(aPresShell, aPresContext, aState, aContent,
                                      aBlockFrame, aFrameItems);
  }

  // Examine the last block child - if it's a first-line frame then
  // appended frames need special treatment.
  nsresult rv = NS_OK;
  nsFrameList blockFrames(blockKid);
  nsIFrame* lastBlockKid = blockFrames.LastChild();
  nsCOMPtr<nsIAtom> frameType;
  lastBlockKid->GetFrameType(getter_AddRefs(frameType));
  if (frameType.get() != nsLayoutAtoms::lineFrame) {
    // No first-line frame at the end of the list, therefore there is
    // an interveening block between any first-line frame the frames
    // we are appending. Therefore, we don't need any special
    // treatment of the appended frames.
    return rv;
  }
  nsIFrame* lineFrame = lastBlockKid;
  nsCOMPtr<nsIStyleContext> firstLineStyle;
  lineFrame->GetStyleContext(getter_AddRefs(firstLineStyle));

  // Find the first and last inline frame in aFrameItems
  nsIFrame* kid = aFrameItems.childList;
  nsIFrame* firstInlineFrame = nsnull;
  nsIFrame* lastInlineFrame = nsnull;
  while (kid) {
    if (IsInlineFrame(kid)) {
      if (!firstInlineFrame) firstInlineFrame = kid;
      lastInlineFrame = kid;
    }
    else {
      break;
    }
    kid->GetNextSibling(&kid);
  }

  // If we don't find any inline frames, then there is nothing to do
  if (!firstInlineFrame) {
    return rv;
  }

  // The inline frames get appended to the lineFrame. Make sure they
  // are reparented properly.
  nsIFrame* remainingFrames;
  lastInlineFrame->GetNextSibling(&remainingFrames);
  lastInlineFrame->SetNextSibling(nsnull);
  kid = firstInlineFrame;
  while (kid) {
    ReparentFrame(aPresContext, lineFrame, firstLineStyle, kid);
    kid->GetNextSibling(&kid);
  }
  aState.mFrameManager->AppendFrames(aPresContext, *aState.mPresShell,
                                     lineFrame, nsnull, firstInlineFrame);

  // The remaining frames get appended to the block frame
  if (remainingFrames) {
    aFrameItems.childList = remainingFrames;
  }
  else {
    aFrameItems.childList = nsnull;
    aFrameItems.lastChild = nsnull;
  }

  return rv;
}

// Special routine to handle inserting a new frame into a block
// frame's child list. Takes care of placing the new frame into the
// right place when first-line style is present.
nsresult
nsCSSFrameConstructor::InsertFirstLineFrames(
  nsIPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aBlockFrame,
  nsIFrame**               aParentFrame,
  nsIFrame*                aPrevSibling,
  nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
#if 0
  nsIFrame* parentFrame = *aParentFrame;
  nsIFrame* newFrame = aFrameItems.childList;
  PRBool isInline = IsInlineFrame(newFrame);

  if (!aPrevSibling) {
    // Insertion will become the first frame. Two cases: we either
    // already have a first-line frame or we don't.
    nsIFrame* firstBlockKid;
    aBlockFrame->FirstChild(nsnull, &firstBlockKid);
    nsCOMPtr<nsIAtom> frameType;
    firstBlockKid->GetFrameType(getter_AddRefs(frameType));
    if (frameType.get() == nsLayoutAtoms::lineFrame) {
      // We already have a first-line frame
      nsIFrame* lineFrame = firstBlockKid;
      nsCOMPtr<nsIStyleContext> firstLineStyle;
      lineFrame->GetStyleContext(getter_AddRefs(firstLineStyle));

      if (isInline) {
        // Easy case: the new inline frame will go into the lineFrame.
        ReparentFrame(aPresContext, lineFrame, firstLineStyle, newFrame);
        aState.mFrameManager->InsertFrames(aPresContext, *aState.mPresShell,
                                           lineFrame, nsnull, nsnull,
                                           newFrame);

        // Since the frame is going into the lineFrame, don't let it
        // go into the block too.
        aFrameItems.childList = nsnull;
        aFrameItems.lastChild = nsnull;
      }
      else {
        // Harder case: We are about to insert a block level element
        // before the first-line frame.
        // XXX need a method to steal away frames from the line-frame
      }
    }
    else {
      // We do not have a first-line frame
      if (isInline) {
        // We now need a first-line frame to contain the inline frame.
        nsIFrame* lineFrame;
        rv = NS_NewFirstLineFrame(&lineFrame);
        if (NS_SUCCEEDED(rv)) {
          // Lookup first-line style context
          nsCOMPtr<nsIStyleContext> parentStyle;
          aBlockFrame->GetStyleContext(getter_AddRefs(parentStyle));
          nsCOMPtr<nsIStyleContext> firstLineStyle(
            getter_AddRefs(GetFirstLineStyle(aPresContext, aContent,
                                             parentStyle))
            );

          // Initialize the line frame
          rv = InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aBlockFrame, firstLineStyle, nsnull, lineFrame);

          // Make sure the caller inserts the lineFrame into the
          // blocks list of children.
          aFrameItems.childList = lineFrame;
          aFrameItems.lastChild = lineFrame;

          // Give the inline frames to the lineFrame <b>after</b>
          // reparenting them
          ReparentFrame(aPresContext, lineFrame, firstLineStyle, newFrame);
          lineFrame->SetInitialChildList(aPresContext, nsnull, newFrame);
        }
      }
      else {
        // Easy case: the regular insertion logic can insert the new
        // frame because its a block frame.
      }
    }
  }
  else {
    // Insertion will not be the first frame.
    nsIFrame* prevSiblingParent;
    aPrevSibling->GetParent(&prevSiblingParent);
    if (prevSiblingParent == aBlockFrame) {
      // Easy case: The prev-siblings parent is the block
      // frame. Therefore the prev-sibling is not currently in a
      // line-frame. Therefore the new frame which is going after it,
      // regardless of type, is not going into a line-frame.
    }
    else {
      // If the prevSiblingParent is not the block-frame then it must
      // be a line-frame (if it were a letter-frame, that logic would
      // already have adjusted the prev-sibling to be the
      // letter-frame).
      if (isInline) {
        // Easy case: the insertion can go where the caller thinks it
        // should go (which is into prevSiblingParent).
      }
      else {
        // Block elements don't end up in line-frames, therefore
        // change the insertion point to aBlockFrame. However, there
        // might be more inline elements following aPrevSibling that
        // need to be pulled out of the line-frame and become children
        // of the block.
        nsIFrame* nextSibling;
        aPrevSibling->GetNextSibling(&nextSibling);
        nsIFrame* nextLineFrame;
        prevSiblingParent->GetNextInFlow(&nextLineFrame);
        if (nextSibling || nextLineFrame) {
          // Oy. We have work to do. Create a list of the new frames
          // that are going into the block by stripping them away from
          // the line-frame(s).
          nsFrameList list(nextSibling);
          if (nextSibling) {
            nsLineFrame* lineFrame = (nsLineFrame*) prevSiblingParent;
            lineFrame->StealFramesFrom(nextSibling);
          }

          nsLineFrame* nextLineFrame = (nsLineFrame*) lineFrame;
          for (;;) {
            nextLineFrame->GetNextInFlow(&nextLineFrame);
            if (!nextLineFrame) {
              break;
            }
            nsIFrame* kids;
            nextLineFrame->FirstChild(nsnull, &kids);
          }
        }
        else {
          // We got lucky: aPrevSibling was the last inline frame in
          // the line-frame.
          ReparentFrame(aPresContext, aBlockFrame, firstLineStyle, newFrame);
          aState.mFrameManager->InsertFrames(aPresContext, *aState.mPresShell,
                                             aBlockFrame, nsnull,
                                             prevSiblingParent, newFrame);
          aFrameItems.childList = nsnull;
          aFrameItems.lastChild = nsnull;
        }
      }
    }
  }

#endif
  return rv;
}

//----------------------------------------------------------------------

// First-letter support

// Determine how many characters in the text fragment apply to the
// first letter
static PRInt32
FirstLetterCount(const nsTextFragment* aFragment)
{
  PRInt32 count = 0;
  PRInt32 firstLetterLength = 0;
  PRBool done = PR_FALSE;

  PRInt32 i, n = aFragment->GetLength();
  for (i = 0; i < n; i++) {
    PRUnichar ch = aFragment->CharAt(i);
    if (XP_IS_SPACE(ch)) {
      if (firstLetterLength) {
        done = PR_TRUE;
        break;
      }
      count++;
      continue;
    }
    // XXX I18n
    if ((ch == '\'') || (ch == '\"')) {
      if (firstLetterLength) {
        done = PR_TRUE;
        break;
      }
      // keep looping
      firstLetterLength = 1;
    }
    else {
      count++;
      done = PR_TRUE;
      break;
    }
  }

  return count;
}

static PRBool
NeedFirstLetterContinuation(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "null ptr");

  PRBool result = PR_FALSE;
  if (aContent) {
    nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
    if (tc) {
      const nsTextFragment* frag = nsnull;
      tc->GetText(&frag);
      PRInt32 flc = FirstLetterCount(frag);
      PRInt32 tl = frag->GetLength();
      if (flc < tl) {
        result = PR_TRUE;
      }
    }
  }
  return result;
}

static PRBool IsFirstLetterContent(nsIContent* aContent)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aContent);
  if (textContent) {
    PRInt32 textLength;
    textContent->GetTextLength(&textLength);
    if (textLength) {
      PRBool onlyWhiteSpace;
      textContent->IsOnlyWhitespace(&onlyWhiteSpace);
      result = !onlyWhiteSpace;
    }
  }

  return result;
}

/**
 * Create a letter frame, only make it a floating frame.
 */
void
nsCSSFrameConstructor::CreateFloatingLetterFrame(
  nsIPresShell* aPresShell, 
  nsIPresContext* aPresContext,
  nsFrameConstructorState& aState,
  nsIContent* aTextContent,
  nsIFrame* aTextFrame,
  nsIContent* aBlockContent,
  nsIFrame* aParentFrame,
  nsIStyleContext* aStyleContext,
  nsFrameItems& aResult)
{
  // Create the first-letter-frame
  nsIFrame* letterFrame;

  NS_NewFirstLetterFrame(aPresShell, &letterFrame);  
  InitAndRestoreFrame(aPresContext, aState, aTextContent, 
                      aParentFrame, aStyleContext, nsnull, letterFrame);

  // Init the text frame to refer to the letter frame. Make sure we
  // get a proper style context for it (the one passed in is for the
  // letter frame and will have the float property set on it; the text
  // frame shouldn't have that set).
  nsCOMPtr<nsIStyleContext> textSC;
  aPresContext->ResolveStyleContextFor(aTextContent, aStyleContext, PR_FALSE,
                                       getter_AddRefs(textSC));  
  InitAndRestoreFrame(aPresContext, aState, aTextContent, 
                      letterFrame, textSC, nsnull, aTextFrame);

  // And then give the text frame to the letter frame
  letterFrame->SetInitialChildList(aPresContext, nsnull, aTextFrame);

  // Now make the placeholder
  nsIFrame* placeholderFrame;
  CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aTextContent,
                            letterFrame, aStyleContext, aParentFrame,
                            &placeholderFrame);

  // See if we will need to continue the text frame (does it contain
  // more than just the first-letter text or not?) If it does, then we
  // create (in advance) a continuation frame for it.
  nsIFrame* nextTextFrame = nsnull;
  if (NeedFirstLetterContinuation(aTextContent)) {
    // Create continuation
    CreateContinuingFrame(aPresShell, aPresContext, aTextFrame, aParentFrame,
                          &nextTextFrame);

    // Repair the continuations style context
    nsCOMPtr<nsIStyleContext> parentStyleContext;
    parentStyleContext = getter_AddRefs(aStyleContext->GetParent());
    if (parentStyleContext) {
      nsCOMPtr<nsIStyleContext> newSC;
      aPresContext->ResolveStyleContextFor(aTextContent, parentStyleContext,
                                           PR_FALSE, getter_AddRefs(newSC));
      if (newSC) {
        nextTextFrame->SetStyleContext(aPresContext, newSC);
      }
    }
  }

  // Update the child lists for the frame containing the floating first
  // letter frame.
  aState.mFloatedItems.AddChild(letterFrame);
  aResult.childList = aResult.lastChild = placeholderFrame;
  if (nextTextFrame) {
    aResult.AddChild(nextTextFrame);
  }
}

/**
 * Create a new letter frame for aTextFrame. The letter frame will be
 * a child of aParentFrame.
 */
nsresult
nsCSSFrameConstructor::CreateLetterFrame(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent* aTextContent,
                                         nsIFrame* aParentFrame,
                                         nsFrameItems& aResult)
{
  nsCOMPtr<nsIContent> parentContent;
  aParentFrame->GetContent(getter_AddRefs(parentContent));

  // Get style context for the first-letter-frame
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));
  if (parentStyleContext) {
    // Use content from containing block so that we can actually
    // find a matching style rule.
    nsCOMPtr<nsIContent> blockContent;
    aState.mFloatedItems.containingBlock->GetContent(
      getter_AddRefs(blockContent));

    // Create first-letter style rule
    nsCOMPtr<nsIStyleContext> sc = getter_AddRefs(
      GetFirstLetterStyle(aPresContext, blockContent, parentStyleContext));
    if (sc) {
      // Create a new text frame (the original one will be discarded)
      nsIFrame* textFrame;
      NS_NewTextFrame(aPresShell, &textFrame);

      // Create the right type of first-letter frame
      const nsStyleDisplay* display = (const nsStyleDisplay*)
        sc->GetStyleData(eStyleStruct_Display);
      if (display->IsFloating()) {
        // Make a floating first-letter frame
        CreateFloatingLetterFrame(aPresShell, aPresContext, aState,
                                  aTextContent, textFrame,
                                  blockContent, aParentFrame,
                                  sc, aResult);
      }
      else {
        // Make an inflow first-letter frame
        nsIFrame* letterFrame;
        nsresult rv = NS_NewFirstLetterFrame(aPresShell, &letterFrame);
        if (NS_SUCCEEDED(rv)) {
          // Initialize the first-letter-frame.
          letterFrame->Init(aPresContext, aTextContent, aParentFrame,
                            sc, nsnull);
          nsCOMPtr<nsIStyleContext> textSC;
          aPresContext->ResolveStyleContextFor(aTextContent, sc, PR_FALSE,
                                               getter_AddRefs(textSC));
          InitAndRestoreFrame(aPresContext, aState, aTextContent, 
                              letterFrame, textSC, nsnull, textFrame);          
          letterFrame->SetInitialChildList(aPresContext, nsnull, textFrame);
          aResult.childList = aResult.lastChild = letterFrame;
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsIPresShell* aPresShell, 
  nsIPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aBlockFrames)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = nsnull;
  nsIFrame* textFrame = nsnull;
  nsIFrame* prevFrame = nsnull;
  nsFrameItems letterFrames;
  PRBool stopLooking = PR_FALSE;
  rv = WrapFramesInFirstLetterFrame(aPresShell, aPresContext, aState,
                                    aBlockFrame, aBlockFrames.childList,
                                    &parentFrame, &textFrame, &prevFrame,
                                    letterFrames, &stopLooking);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (parentFrame) {
    if (parentFrame == aBlockFrame) {
      // Text textFrame out of the blocks frame list and substitute the
      // letter frame(s) instead.
      nsIFrame* nextSibling;
      textFrame->GetNextSibling(&nextSibling);
      textFrame->SetNextSibling(nsnull);
      if (prevFrame) {
        prevFrame->SetNextSibling(letterFrames.childList);
      }
      else {
        aBlockFrames.childList = letterFrames.childList;
      }
      letterFrames.lastChild->SetNextSibling(nextSibling);

      // Destroy the old textFrame
      textFrame->Destroy(aPresContext);

      // Repair lastChild; the only time this needs to happen is when
      // the block had one child (the text frame).
      if (!nextSibling) {
        aBlockFrames.lastChild = letterFrames.lastChild;
      }
    }
    else {
      // Take the old textFrame out of the inline parents child list
      parentFrame->RemoveFrame(aPresContext, *aState.mPresShell.get(),
                               nsnull, textFrame);

      // Insert in the letter frame(s)
      parentFrame->InsertFrames(aPresContext, *aState.mPresShell.get(),
                                nsnull, prevFrame, letterFrames.childList);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsIPresShell* aPresShell, 
  nsIPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIFrame*                aParentFrame,
  nsIFrame*                aParentFrameList,
  nsIFrame**               aModifiedParent,
  nsIFrame**               aTextFrame,
  nsIFrame**               aPrevFrame,
  nsFrameItems&            aLetterFrames,
  PRBool*                  aStopLooking)
{
  nsresult rv = NS_OK;

  nsIFrame* prevFrame = nsnull;
  nsIFrame* frame = aParentFrameList;

  while (frame) {
    nsIFrame* nextFrame;
    frame->GetNextSibling(&nextFrame);

    nsCOMPtr<nsIAtom> frameType;
    frame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::textFrame == frameType.get()) {
      // Wrap up first-letter content in a letter frame
      nsCOMPtr<nsIContent> textContent;
      frame->GetContent(getter_AddRefs(textContent));
      if (IsFirstLetterContent(textContent)) {
        // Create letter frame to wrap up the text
        rv = CreateLetterFrame(aPresShell, aPresContext, aState, textContent,
                               aParentFrame, aLetterFrames);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Provide adjustment information for parent
        *aModifiedParent = aParentFrame;
        *aTextFrame = frame;
        *aPrevFrame = prevFrame;
        *aStopLooking = PR_TRUE;
        return NS_OK;
      }
    }
    else if ((nsLayoutAtoms::inlineFrame == frameType.get()) ||
             (nsLayoutAtoms::lineFrame == frameType.get())) {
      nsIFrame* kids;
      frame->FirstChild(aPresContext, nsnull, &kids);
      WrapFramesInFirstLetterFrame(aPresShell, aPresContext, aState, frame, kids,
                                   aModifiedParent, aTextFrame,
                                   aPrevFrame, aLetterFrames, aStopLooking);
      if (*aStopLooking) {
        return NS_OK;
      }
    }
    else {
      // This will stop us looking to create more letter frames. For
      // example, maybe the frame-type is "letterFrame" or
      // "placeholderFrame". This keeps us from creating extra letter
      // frames, and also prevents us from creating letter frames when
      // the first real content child of a block is not text (e.g. an
      // image, hr, etc.)
      *aStopLooking = PR_TRUE;
      break;
    }

    prevFrame = frame;
    frame = nextFrame;
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::RemoveFloatingFirstLetterFrames(
  nsIPresContext* aPresContext,
  nsIPresShell* aPresShell,
  nsIFrameManager* aFrameManager,
  nsIFrame* aBlockFrame,
  PRBool* aStopLooking)
{
  // First look for the floater frame that is a letter frame
  nsIFrame* floater;
  aBlockFrame->FirstChild(aPresContext, nsLayoutAtoms::floaterList, &floater);
  while (floater) {
    // See if we found a floating letter frame
    nsCOMPtr<nsIAtom> frameType;
    floater->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::letterFrame == frameType.get()) {
      break;
    }
    floater->GetNextSibling(&floater);
  }
  if (!floater) {
    // No such frame
    return NS_OK;
  }

  // Take the text frame away from the letter frame (so it isn't
  // destroyed when we destroy the letter frame).
  nsIFrame* textFrame;
  floater->FirstChild(aPresContext, nsnull, &textFrame);
  if (!textFrame) {
    return NS_OK;
  }

  // Discover the placeholder frame for the letter frame
  nsIFrame* parentFrame;
  nsIFrame* placeholderFrame;
  aFrameManager->GetPlaceholderFrameFor(floater, &placeholderFrame);
  if (!placeholderFrame) {
    // Somethings really wrong
    return NS_OK;
  }
  placeholderFrame->GetParent(&parentFrame);
  if (!parentFrame) {
    // Somethings really wrong
    return NS_OK;
  }

  // Create a new text frame with the right style context that maps
  // all of the content that was previously part of the letter frame
  // (and probably continued elsewhere).
  nsCOMPtr<nsIStyleContext> parentSC;
  parentFrame->GetStyleContext(getter_AddRefs(parentSC));
  if (!parentSC) {
    return NS_OK;
  }
  nsCOMPtr<nsIContent> textContent;
  textFrame->GetContent(getter_AddRefs(textContent));
  if (!textContent) {
    return NS_OK;
  }
  nsCOMPtr<nsIStyleContext> newSC;
  aPresContext->ResolveStyleContextFor(textContent, parentSC,
                                       PR_FALSE,
                                       getter_AddRefs(newSC));
  if (!newSC) {
    return NS_OK;
  }
  nsIFrame* newTextFrame;
  nsresult rv = NS_NewTextFrame(aPresShell, &newTextFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }
  newTextFrame->Init(aPresContext, textContent, parentFrame, newSC, nsnull);

  // Destroy the old text frame's continuations (the old text frame
  // will be destroyed when its letter frame is destroyed).
  nsIFrame* nextTextFrame;
  textFrame->GetNextInFlow(&nextTextFrame);
  if (nextTextFrame) {
    nsIFrame* nextTextParent;
    nextTextFrame->GetParent(&nextTextParent);
    if (nextTextParent) {
      nsSplittableFrame::BreakFromPrevFlow(nextTextFrame);
      aFrameManager->RemoveFrame(aPresContext, *aPresShell, nextTextParent,
                                 nsnull, nextTextFrame);
    }
  }

  // First find out where (in the content) the placeholder frames
  // text is and its previous sibling frame, if any.
  nsIFrame* prevSibling = nsnull;

  nsCOMPtr<nsIContent> container;
  parentFrame->GetContent(getter_AddRefs(container));
  if (container.get() && textContent.get()) {
    PRInt32 ix = 0;
    container->IndexOf(textContent, ix);
    prevSibling = FindPreviousSibling(aPresShell, container, ix);
  }

  // Now that everything is set...
#ifdef NOISY_FIRST_LETTER
  printf("RemoveFloatingFirstLetterFrames: textContent=%p oldTextFrame=%p newTextFrame=%p\n",
         textContent.get(), textFrame, newTextFrame);
#endif
  aFrameManager->SetPlaceholderFrameFor(floater, nsnull);
  aFrameManager->SetPrimaryFrameFor(textContent, nsnull);

  // Remove the floater frame
  aFrameManager->RemoveFrame(aPresContext, *aPresShell,
                             aBlockFrame, nsLayoutAtoms::floaterList,
                             floater);

  // Remove placeholder frame
  aFrameManager->RemoveFrame(aPresContext, *aPresShell,
                             parentFrame, nsnull, placeholderFrame);

  // Insert text frame in its place
  aFrameManager->InsertFrames(aPresContext, *aPresShell, parentFrame, nsnull,
                              prevSibling, newTextFrame);

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveFirstLetterFrames(nsIPresContext* aPresContext,
                                               nsIPresShell* aPresShell,
                                               nsIFrameManager* aFrameManager,
                                               nsIFrame* aFrame,
                                               PRBool* aStopLooking)
{
  nsIFrame* kid;
  nsIFrame* prevSibling = nsnull;
  aFrame->FirstChild(aPresContext, nsnull, &kid);

  while (kid) {
    nsCOMPtr<nsIAtom> frameType;
    kid->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::letterFrame == frameType.get()) {
      // Bingo. Found it. First steal away the text frame.
      nsIFrame* textFrame;
      kid->FirstChild(aPresContext, nsnull, &textFrame);
      if (!textFrame) {
        break;
      }

      // Create a new textframe
      nsCOMPtr<nsIStyleContext> parentSC;
      aFrame->GetStyleContext(getter_AddRefs(parentSC));
      if (!parentSC) {
        break;
      }
      nsCOMPtr<nsIContent> textContent;
      textFrame->GetContent(getter_AddRefs(textContent));
      if (!textContent) {
        break;
      }
      nsCOMPtr<nsIStyleContext> newSC;
      aPresContext->ResolveStyleContextFor(textContent, parentSC,
                                           PR_FALSE,
                                           getter_AddRefs(newSC));
      if (!newSC) {
        break;
      }
      NS_NewTextFrame(aPresShell, &textFrame);
      textFrame->Init(aPresContext, textContent, aFrame, newSC, nsnull);

      // Next rip out the kid and replace it with the text frame
      nsIFrameManager* frameManager = aFrameManager;
      frameManager->SetPrimaryFrameFor(textContent, nsnull);
      frameManager->RemoveFrame(aPresContext, *aPresShell,
                                aFrame, nsnull, kid);

      // Insert text frame in its place
      frameManager->InsertFrames(aPresContext, *aPresShell, aFrame, nsnull,
                                 prevSibling, textFrame);

      *aStopLooking = PR_TRUE;
      break;
    }
    else if ((nsLayoutAtoms::inlineFrame == frameType.get()) ||
             (nsLayoutAtoms::lineFrame == frameType.get())) {
      // Look inside child inline frame for the letter frame
      RemoveFirstLetterFrames(aPresContext, aPresShell, aFrameManager, kid,
                              aStopLooking);
      if (*aStopLooking) {
        break;
      }
    }
    prevSibling = kid;
    kid->GetNextSibling(&kid);
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveLetterFrames(nsIPresContext* aPresContext,
                                          nsIPresShell* aPresShell,
                                          nsIFrameManager* aFrameManager,
                                          nsIFrame* aBlockFrame)
{
  PRBool stopLooking = PR_FALSE;
  nsresult rv = RemoveFloatingFirstLetterFrames(aPresContext, aPresShell,
                                                aFrameManager,
                                                aBlockFrame, &stopLooking);
  if (NS_SUCCEEDED(rv) && !stopLooking) {
    rv = RemoveFirstLetterFrames(aPresContext, aPresShell, aFrameManager,
                                 aBlockFrame, &stopLooking);
  }
  return rv;
}

// Fixup the letter frame situation for the given block
nsresult
nsCSSFrameConstructor::RecoverLetterFrames(nsIPresShell* aPresShell, nsIPresContext* aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIFrame* aBlockFrame)
{
  nsresult rv = NS_OK;

  nsIFrame* blockKids;
  aBlockFrame->FirstChild(aPresContext, nsnull, &blockKids);
  nsIFrame* parentFrame = nsnull;
  nsIFrame* textFrame = nsnull;
  nsIFrame* prevFrame = nsnull;
  nsFrameItems letterFrames;
  PRBool stopLooking = PR_FALSE;
  rv = WrapFramesInFirstLetterFrame(aPresShell, aPresContext, aState,
                                    aBlockFrame, blockKids,
                                    &parentFrame, &textFrame, &prevFrame,
                                    letterFrames, &stopLooking);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (parentFrame) {
    // Take the old textFrame out of the parents child list
    parentFrame->RemoveFrame(aPresContext, *aState.mPresShell.get(),
                             nsnull, textFrame);

    // Insert in the letter frame(s)
    parentFrame->InsertFrames(aPresContext, *aState.mPresShell.get(),
                              nsnull, prevFrame, letterFrames.childList);

    // Insert in floaters too if needed
    if (aState.mFloatedItems.childList) {
      aBlockFrame->AppendFrames(aPresContext, *aState.mPresShell.get(),
                                nsLayoutAtoms::floaterList,
                                aState.mFloatedItems.childList);
    }
  }
  return rv;
}

//----------------------------------------------------------------------

// Tree Widget Routines

NS_IMETHODIMP
nsCSSFrameConstructor::CreateTreeWidgetContent(nsIPresContext* aPresContext,
                                               nsIFrame*       aParentFrame,
                                               nsIFrame*       aPrevFrame,
                                               nsIContent*     aChild,
                                               nsIFrame**      aNewFrame,
                                               PRBool          aIsAppend,
                                               PRBool          aIsScrollbar,
                                               nsILayoutHistoryState* aFrameState)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult rv = NS_OK;

  // Construct a new frame
  if (nsnull != aParentFrame) {
    nsFrameItems            frameItems;
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                  GetAbsoluteContainingBlock(aPresContext, aParentFrame),
                                  GetFloaterContainingBlock(aPresContext, aParentFrame), 
                                  mTempFrameTreeState);

    // Get the element's tag
    nsCOMPtr<nsIAtom>  tag;
    aChild->GetTag(*getter_AddRefs(tag));

    nsCOMPtr<nsIStyleContext> styleContext;
    rv = ResolveStyleContext(aPresContext, aParentFrame, aChild, tag, getter_AddRefs(styleContext));

    if (NS_SUCCEEDED(rv)) {
      // Pre-check for display "none" - only if we find that, do we create
      // any frame at all
      const nsStyleDisplay* display = (const nsStyleDisplay*)
        styleContext->GetStyleData(eStyleStruct_Display);

      if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
        *aNewFrame = nsnull;
        return NS_OK;
      }
    }

    rv = ConstructFrame(shell, aPresContext, state, aChild, aParentFrame, frameItems);
    
    nsIFrame* newFrame = frameItems.childList;
    *aNewFrame = newFrame;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      // Notify the parent frame
      if (aIsScrollbar)
        ((nsTreeRowGroupFrame*)aParentFrame)->SetScrollbarFrame(aPresContext, newFrame);
      else if (aIsAppend)
        rv = ((nsTreeRowGroupFrame*)aParentFrame)->TreeAppendFrames(newFrame);
      else
        rv = ((nsTreeRowGroupFrame*)aParentFrame)->TreeInsertFrames(aPrevFrame, newFrame);
        
      // If there are new absolutely positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mAbsoluteItems.childList) {
        rv = state.mAbsoluteItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                         nsLayoutAtoms::absoluteList,
                                                         state.mAbsoluteItems.childList);
      }
      
      // If there are new fixed positioned child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFixedItems.childList) {
        rv = state.mFixedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                      nsLayoutAtoms::fixedList,
                                                      state.mFixedItems.childList);
      }
      
      // If there are new floating child frames, then notify
      // the parent
      // XXX We can't just assume these frames are being appended, we need to
      // determine where in the list they should be inserted...
      if (state.mFloatedItems.childList) {
        rv = state.mFloatedItems.containingBlock->AppendFrames(aPresContext, *shell,
                                                    nsLayoutAtoms::floaterList,
                                                    state.mFloatedItems.childList);
      }
    }
  }

  return rv;
}

//----------------------------------------------------------------------

// Block/inline frame construction logic. We maintain a few invariants here:
//
// 1. Block frames contain block and inline frames.
//
// 2. Inline frames only contain inline frames. If an inline parent has a block
// child then the block child is migrated upward until it lands in a block
// parent (the inline frames containing block is where it will end up).

// XXX consolidate these things
static PRBool
IsBlockFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

static nsIFrame*
FindFirstBlock(nsIPresContext* aPresContext, nsIFrame* aKid, nsIFrame** aPrevKid)
{
  nsIFrame* prevKid = nsnull;
  while (aKid) {
    if (IsBlockFrame(aPresContext, aKid)) {
      *aPrevKid = prevKid;
      return aKid;
    }
    prevKid = aKid;
    aKid->GetNextSibling(&aKid);
  }
  *aPrevKid = nsnull;
  return nsnull;
}

static nsIFrame*
FindLastBlock(nsIPresContext* aPresContext, nsIFrame* aKid)
{
  nsIFrame* lastBlock = nsnull;
  while (aKid) {
    if (IsBlockFrame(aPresContext, aKid)) {
      lastBlock = aKid;
    }
    aKid->GetNextSibling(&aKid);
  }
  return lastBlock;
}

static nsresult
MoveChildrenTo(nsIPresContext* aPresContext,
               nsIStyleContext* aNewParentSC,
               nsIFrame* aNewParent,
               nsIFrame* aFrameList)
{
  while (aFrameList) {
    aFrameList->SetParent(aNewParent);
    aFrameList->GetNextSibling(&aFrameList);
  }
  return NS_OK;
}

//----------------------------------------

nsresult
nsCSSFrameConstructor::ConstructBlock(nsIPresShell* aPresShell, 
                                      nsIPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      const nsStyleDisplay*    aDisplay,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsIStyleContext*         aStyleContext,
                                      nsIFrame*                aNewFrame)
{
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  // See if we need to create a view, e.g. the frame is absolutely positioned
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame,
                                           aStyleContext, PR_FALSE);

  // See if the block has first-letter style applied to it...
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);

  // Process the child content
  nsFrameItems childItems;
  nsFrameConstructorSaveState floaterSaveState;
  aState.PushFloaterContainingBlock(aNewFrame, floaterSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);
  nsresult rv = ProcessBlockChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                                     PR_TRUE, childItems, PR_TRUE);

  // Set the frame's initial child list
  aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

  // Set the frame's floater list if there were any floated children
  if (aState.mFloatedItems.childList) {
    aNewFrame->SetInitialChildList(aPresContext,
                                   nsLayoutAtoms::floaterList,
                                   aState.mFloatedItems.childList);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ProcessBlockChildren(nsIPresShell* aPresShell, 
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aFrame,
                                            PRBool                   aCanHaveGeneratedContent,
                                            nsFrameItems&            aFrameItems,
                                            PRBool                   aParentIsBlock)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIStyleContext> styleContext;

  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    aFrame->GetStyleContext(getter_AddRefs(styleContext));
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::beforePseudo,
                                    aParentIsBlock, &generatedFrame)) {
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
      rv = ConstructFrame(aPresShell, aPresContext, aState, childContent, aFrame, aFrameItems);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::afterPseudo,
                                    aParentIsBlock, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  if (aParentIsBlock) {
    if (aState.mFirstLetterStyle) {
      rv = WrapFramesInFirstLetterFrame(aPresShell, aPresContext, aState, aContent, aFrame, aFrameItems);
    }
    if (aState.mFirstLineStyle) {
      rv = WrapFramesInFirstLineFrame(aPresShell, aPresContext, aState, aContent, aFrame, aFrameItems);
    }
  }

  return rv;
}

// When inline frames get weird and have block frames in them, we
// annotate them to help us respond to incremental content changes
// more easily.

static void
DestroyInlineFrameAnnotation(nsIPresContext* aPresContext,
                             nsIFrame*       aFrame,
                             nsIAtom*        aPropertyName,
                             void*           aPropertyValue)
{
}

PRBool
nsCSSFrameConstructor::IsFrameSpecial(nsIFrameManager* aFrameManager, nsIFrame* aFrame)
{
  void* value;
  nsresult rv = aFrameManager->GetFrameProperty(aFrame, nsLayoutAtoms::inlineFrameAnnotation, 0, &value);
  if (NS_OK == rv) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsCSSFrameConstructor::IsFrameSpecial(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  // Get to aFrame's first-in-flow; only the first-in-flow is marked
  // with an annotation.
  nsSplittableType splits;
  aFrame->IsSplittable(splits);
  if (splits != NS_FRAME_NOT_SPLITTABLE) {
    nsIFrame* prevInFlow = aFrame;
    while (prevInFlow) {
      aFrame = prevInFlow;
      prevInFlow->GetPrevInFlow(&prevInFlow);
    }
  }

  PRBool result = PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIFrameManager> frameManager;
    shell->GetFrameManager(getter_AddRefs(frameManager));
    if (frameManager) {
      void* value;
      nsresult rv = frameManager->GetFrameProperty(aFrame, nsLayoutAtoms::inlineFrameAnnotation,
                                                   0, &value);
      if (NS_OK == rv) {
        result = PR_TRUE;
      }
    }
  }
  return result;
}

void
nsCSSFrameConstructor::SetFrameIsSpecial(nsIFrameManager* aFrameManager, nsIFrame* aFrame)
{
  aFrameManager->SetFrameProperty(aFrame, nsLayoutAtoms::inlineFrameAnnotation,
                                  (void*) PR_TRUE, DestroyInlineFrameAnnotation);
}

PRBool
nsCSSFrameConstructor::AreAllKidsInline(nsIFrame* aFrameList)
{
  nsIFrame* kid = aFrameList;
  while (kid) {
    if (!IsInlineFrame(kid)) {
      return PR_FALSE;
    }
    kid->GetNextSibling(&kid);
  }
  return PR_TRUE;
}

nsresult
nsCSSFrameConstructor::ConstructInline(nsIPresShell* aPresShell, 
                                       nsIPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       const nsStyleDisplay*    aDisplay,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsIStyleContext*         aStyleContext,
                                       nsIFrame*                aNewFrame,
                                       nsIFrame**               aNewBlockFrame,
                                       nsIFrame**               aNextInlineFrame)
{
  // Initialize the frame
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  // Process the child content
  nsFrameItems childItems;
  PRBool kidsAllInline;
  nsresult rv = ProcessInlineChildren(aPresShell, aPresContext, aState, aContent,
                                      aNewFrame, PR_TRUE, childItems, &kidsAllInline);
  if (kidsAllInline) {
    // Set the inline frame's initial child list
    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    *aNewBlockFrame = nsnull;
    *aNextInlineFrame = nsnull;
    return rv;
  }

  // This inline frame contains several types of children. Therefore
  // this frame has to be chopped into several pieces. We will produce
  // as a result of this 3 lists of children. The first list contains
  // all of the inline children that preceed the first block child
  // (and may be empty). The second list contains all of the block
  // children and any inlines that are between them (and must not be
  // empty, otherwise - why are we here?). The final list contains all
  // of the inline children that follow the final block child.

  // Find the first block child which defines list1 and list2
  nsIFrame* list1 = childItems.childList;
  nsIFrame* prevToFirstBlock;
  nsIFrame* list2 = FindFirstBlock(aPresContext, list1, &prevToFirstBlock);
  if (prevToFirstBlock) {
    prevToFirstBlock->SetNextSibling(nsnull);
  }
  else {
    list1 = nsnull;
  }

  // Find the last block child which defines the end of list2 and the
  // start of list3
  nsIFrame* afterFirstBlock;
  list2->GetNextSibling(&afterFirstBlock);
  nsIFrame* list3 = nsnull;
  nsIFrame* lastBlock = FindLastBlock(aPresContext, afterFirstBlock);
  if (!lastBlock) {
    lastBlock = list2;
  }
  lastBlock->GetNextSibling(&list3);
  lastBlock->SetNextSibling(nsnull);

  // list1's frames belong to this inline frame so go ahead and take them
  aNewFrame->SetInitialChildList(aPresContext, nsnull, list1);

  // list2's frames belong to an anonymous block that we create right
  // now. The anonymous block will be the parent of the block children
  // of the inline.
  nsIFrame* blockFrame;
  NS_NewBlockFrame(aPresShell, &blockFrame);
  nsCOMPtr<nsIStyleContext> blockSC;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::mozAnonymousBlock,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(blockSC));

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, blockSC, nsnull, blockFrame);  

  MoveChildrenTo(aPresContext, blockSC, blockFrame, list2);
  blockFrame->SetInitialChildList(aPresContext, nsnull, list2);

  // list3's frames belong to another inline frame
  nsIFrame* inlineFrame = nsnull;
  NS_NewInlineFrame(aPresShell, &inlineFrame);
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, inlineFrame);  
  if (list3) {
    // Reparent (cheaply) the frames in list3 - we don't have to futz
    // with their style context because they already have the right one.
    nsFrameList list;
    list.AppendFrames(inlineFrame, list3);
  }
  inlineFrame->SetInitialChildList(aPresContext, nsnull, list3);

  // Mark the 3 frames as special. That way if any of the
  // append/insert/remove methods try to fiddle with the children, the
  // containing block will be reframed instead.
  SetFrameIsSpecial(aState.mFrameManager, aNewFrame);
  SetFrameIsSpecial(aState.mFrameManager, blockFrame);
  SetFrameIsSpecial(aState.mFrameManager, inlineFrame);

#ifdef DEBUG
  if (gNoisyInlineConstruction) {
    nsIFrameDebug*  frameDebug;

    printf("nsCSSFrameConstructor::ConstructInline:\n");
    if (NS_SUCCEEDED(aNewFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      printf("  ==> leading inline frame:\n");
      frameDebug->List(aPresContext, stdout, 2);
    }
    if (NS_SUCCEEDED(blockFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      printf("  ==> block frame:\n");
      frameDebug->List(aPresContext, stdout, 2);
    }
    if (NS_SUCCEEDED(inlineFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      printf("  ==> trailing inline frame:\n");
      frameDebug->List(aPresContext, stdout, 2);
    }
  }
#endif

  *aNewBlockFrame = blockFrame;
  *aNextInlineFrame = inlineFrame;

  return rv;
}

nsresult
nsCSSFrameConstructor::ProcessInlineChildren(nsIPresShell* aPresShell, 
                                             nsIPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIFrame*                aFrame,
                                             PRBool                   aCanHaveGeneratedContent,
                                             nsFrameItems&            aFrameItems,
                                             PRBool*                  aKidsAllInline)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIStyleContext> styleContext;

  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    aFrame->GetStyleContext(getter_AddRefs(styleContext));
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::beforePseudo,
                                    PR_FALSE, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  // Iterate the child content objects and construct frames
  PRBool allKidsInline = PR_TRUE;
  PRInt32 count;
  aContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> childContent;
    if (NS_SUCCEEDED(aContent->ChildAt(i, *getter_AddRefs(childContent)))) {
      // Construct a child frame
      nsIFrame* oldLastChild = aFrameItems.lastChild;
      rv = ConstructFrame(aPresShell, aPresContext, aState, childContent, aFrame, aFrameItems);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Examine newly added children (we may have added more than one
      // child if the child was another inline frame that ends up
      // being carved in 3 pieces) to maintain the allKidsInline flag.
      if (allKidsInline) {
        nsIFrame* kid;
        if (oldLastChild) {
          oldLastChild->GetNextSibling(&kid);
        }
        else {
          kid = aFrameItems.childList;
        }
        while (kid) {
          if (!IsInlineFrame(kid)) {
            allKidsInline = PR_FALSE;
            break;
          }
          kid->GetNextSibling(&kid);
        }
      }
    }
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSAtoms::afterPseudo,
                                    PR_FALSE, &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  *aKidsAllInline = allKidsInline;

  return rv;
}

// Helper function that recursively removes content to frame mappings and
// undisplayed content mappings.
// This differs from DeletingFrameSubtree() because the frames have not yet been
// added to the frame hierarchy
static void
DoCleanupFrameReferences(nsIPresContext*  aPresContext,
                         nsIFrameManager* aFrameManager,
                         nsIFrame*        aFrame)
{
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  
  // Remove the mapping from the content object to its frame
  aFrameManager->SetPrimaryFrameFor(content, nsnull);
  aFrameManager->ClearAllUndisplayedContentIn(content);

  // Recursively walk the child frames.
  // Note: we only need to look at the principal child list
  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    DoCleanupFrameReferences(aPresContext, aFrameManager, childFrame);
    
    // Get the next sibling child frame
    childFrame->GetNextSibling(&childFrame);
  }
}

// Helper function that walks a frame list and calls DoCleanupFrameReference()
static void
CleanupFrameReferences(nsIPresContext*  aPresContext,
                       nsIFrameManager* aFrameManager,
                       nsIFrame*        aFrameList)
{
  while (aFrameList) {
    DoCleanupFrameReferences(aPresContext, aFrameManager, aFrameList);

    // Get the sibling frame
    aFrameList->GetNextSibling(&aFrameList);
  }
}

PRBool
nsCSSFrameConstructor::WipeContainingBlock(nsIPresContext* aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent* aBlockContent,
                                           nsIFrame* aFrame,
                                           nsIFrame* aFrameList)
{
  // Before we go and append the frames, check for a special
  // situation: an inline frame that will now contain block
  // frames. This is a no-no and the frame construction logic knows
  // how to fix this.
  const nsStyleDisplay* parentDisplay;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&) parentDisplay);
  if (NS_STYLE_DISPLAY_INLINE == parentDisplay->mDisplay) {
    if (!AreAllKidsInline(aFrameList)) {
      // Ok, reverse tracks: wipe out the frames we just created
      nsCOMPtr<nsIPresShell>    presShell;
      nsCOMPtr<nsIFrameManager> frameManager;

      aPresContext->GetShell(getter_AddRefs(presShell));
      presShell->GetFrameManager(getter_AddRefs(frameManager));

      // Destroy the frames. As we do make sure any content to frame mappings
      // or entries in the undisplayed content map are removed
      CleanupFrameReferences(aPresContext, frameManager, aFrameList);
      nsFrameList tmp(aFrameList);
      tmp.DestroyFrames(aPresContext);
      if (aState.mAbsoluteItems.childList) {
        CleanupFrameReferences(aPresContext, frameManager, aState.mAbsoluteItems.childList);
        tmp.SetFrames(aState.mAbsoluteItems.childList);
        tmp.DestroyFrames(aPresContext);
      }
      if (aState.mFixedItems.childList) {
        CleanupFrameReferences(aPresContext, frameManager, aState.mFixedItems.childList);
        tmp.SetFrames(aState.mFixedItems.childList);
        tmp.DestroyFrames(aPresContext);
      }
      if (aState.mFloatedItems.childList) {
        CleanupFrameReferences(aPresContext, frameManager, aState.mFloatedItems.childList);
        tmp.SetFrames(aState.mFloatedItems.childList);
        tmp.DestroyFrames(aPresContext);
      }

      // Tell parent of the containing block to reformulate the
      // entire block. This is painful and definitely not optimal
      // but it will *always* get the right answer.
      nsCOMPtr<nsIContent> parentContainer;
      aBlockContent->GetParent(*getter_AddRefs(parentContainer));
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::WipeContainingBlock: aBlockContent=%p parentContainer=%p\n",
               aBlockContent, parentContainer.get());
      }
#endif
      if (parentContainer) {
        PRInt32 ix;
        parentContainer->IndexOf(aBlockContent, ix);
        ContentReplaced(aPresContext, parentContainer, aBlockContent, aBlockContent, ix);
      }
      else {
        // XXX uh oh. the block we need to reframe has no parent!
      }
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult
nsCSSFrameConstructor::ReframeContainingBlock(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  // Get the parent of the target frame. From there we look for the
  // containing block in case the target frame is already a block
  // (which can happen when an inline frame wraps some of its content
  // in an anonymous block; see ConstructInline)
  nsIFrame* parentFrame;
  aFrame->GetParent(&parentFrame);
  if (!parentFrame) {
    return RecreateEntireFrameTree(aPresContext);
  }

  // Now find the containing block
  nsIFrame* containingBlock = GetFloaterContainingBlock(aPresContext, parentFrame);
  if (!containingBlock) {
    return RecreateEntireFrameTree(aPresContext);
  }

  // And get the containingBlock's content
  nsCOMPtr<nsIContent> blockContent;
  containingBlock->GetContent(getter_AddRefs(blockContent));
  if (!blockContent) {
    return RecreateEntireFrameTree(aPresContext);
  }

  // Now find the containingBlock's content's parent
  nsCOMPtr<nsIContent> parentContainer;
  blockContent->GetParent(*getter_AddRefs(parentContainer));
  if (!parentContainer) {
    return RecreateEntireFrameTree(aPresContext);
  }

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("  ==> blockContent=%p, parentContainer=%p\n",
           blockContent.get(), parentContainer.get());
  }
#endif

  PRInt32 ix;
  parentContainer->IndexOf(blockContent, ix);
  nsresult rv = ContentReplaced(aPresContext, parentContainer, blockContent, blockContent, ix);
  return rv;
}

nsresult
nsCSSFrameConstructor::RecreateEntireFrameTree(nsIPresContext* aPresContext)
{
  // XXX write me some day
  return NS_OK;
}
