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
#include "nsICheckboxControlFrame.h"
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
#include "nsIContentIterator.h"
#include "nsBoxLayoutState.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#ifdef INCLUDE_XUL
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIMenuFrame.h"
#endif

// XXX - temporary, this is for GfxList View
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
// to here

#include "nsInlineFrame.h"
#include "nsBlockFrame.h"

#include "nsGfxTextControlFrame.h"
#include "nsIScrollableFrame.h"

#include "nsIServiceManager.h"
#include "nsIXBLService.h"
#include "nsIStyleRuleSupplier.h"

#undef NOISY_FIRST_LETTER

#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#endif

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#endif

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#include "nsSVGContainerFrame.h"
#include "nsPolygonFrame.h"
#include "nsPolylineFrame.h"



nsresult
NS_NewSVGContainerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot );

nsresult
NS_NewPolygonFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewPolylineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
#endif

#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#include "nsTreeIndentationFrame.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsToolbarItemFrame.h"
#include "nsIScrollable.h"

#ifdef DEBUG
static PRBool gNoisyContentUpdates = PR_FALSE;
static PRBool gReallyNoisyContentUpdates = PR_FALSE;
static PRBool gNoisyInlineConstruction = PR_FALSE;
#endif

#include "nsXULTreeFrame.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULTreeSliceFrame.h"
#include "nsXULTreeCellFrame.h"
#include "nsMenuFrame.h"
#include "nsPopupSetFrame.h"

#define NEWGFX_LIST_SCROLLFRAME
//------------------------------------------------------------------

#ifdef NEWGFX_LIST_SCROLLFRAME
nsresult
NS_NewGfxListControlFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
#endif


nsresult
NS_NewAutoRepeatBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewRootBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewThumbFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollPortFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewGfxScrollFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIDocument* aDocument, PRBool aIsRoot);

nsresult
NS_NewTabFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewDeckFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager = nsnull);

nsresult
NS_NewSpringFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewStackFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager = nsnull);

nsresult
NS_NewProgressMeterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitledButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewImageBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTextBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitledBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTitleFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewButtonBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewSliderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewSpinnerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

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
NS_NewScrollBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewMenuFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags );

nsresult
NS_NewMenuBarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

// grid
nsresult
NS_NewGridLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

nsresult
NS_NewObeliskLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

nsresult
NS_NewTempleLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

nsresult
NS_NewTreeLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

nsresult
NS_NewBulletinBoardLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

// end grid

nsresult
NS_NewTitleBarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewResizerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);


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
static NS_DEFINE_IID(kIScrollableFrameIID, NS_ISCROLLABLE_FRAME_IID);

//----------------------------------------------------------------------
//
// When inline frames get weird and have block frames in them, we
// annotate them to help us respond to incremental content changes
// more easily.

static inline PRBool
IsFrameSpecial(nsIFrame* aFrame)
{
  nsFrameState state;
  aFrame->GetFrameState(&state);
  return state & NS_FRAME_IS_SPECIAL;
}

static inline void
GetSpecialSibling(nsIFrameManager* aFrameManager, nsIFrame* aFrame, nsIFrame** aResult)
{
  // We only store the "special sibling" annotation with the first
  // frame in the flow. Walk back to find that frame now.
  while (1) {
    nsIFrame* prev = aFrame;
    aFrame->GetPrevInFlow(&prev);
    if (! prev)
      break;
    aFrame = prev;
  }

  void* value;
  aFrameManager->GetFrameProperty(aFrame, nsLayoutAtoms::inlineFrameAnnotation, 0, &value);
  *aResult = NS_STATIC_CAST(nsIFrame*, value);
}


static void
SetFrameIsSpecial(nsIFrameManager* aFrameManager, nsIFrame* aFrame, nsIFrame* aSpecialSibling)
{
  NS_PRECONDITION(aFrameManager && aFrame, "bad args!");

  // Mark the frame and all of its siblings as "special".
  for (nsIFrame* frame = aFrame; frame != nsnull; frame->GetNextInFlow(&frame)) {
    nsFrameState state;
    frame->GetFrameState(&state);
    state |= NS_FRAME_IS_SPECIAL;
    frame->SetFrameState(state);
  }

  if (aSpecialSibling) {
#ifdef DEBUG
    // We should be the first-in-flow
    nsIFrame* prev;
    aFrame->GetPrevInFlow(&prev);
    NS_ASSERTION(! prev, "assigning special sibling to other than first-in-flow!");
#endif

    // Store the "special sibling" (if we were given one) with the
    // first frame in the flow.
    aFrameManager->SetFrameProperty(aFrame, nsLayoutAtoms::inlineFrameAnnotation,
                                    aSpecialSibling, nsnull);
  }
}


//----------------------------------------------------------------------

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
  // when reparenting a frame, it would seem to be critical to also reparent any views associated with the frame
  // I haven't found a case where this is required yet, but if we ever see a bug where the frame and
  // view models are out of synch, particularly after a "special" block-in-inline situation is encountered,
  // the following 3 lines of code would fix it.
  /*
  nsIFrame *oldParent;
  aFrameList->GetParent(&oldParent);
  nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, aFrameList, oldParent, aNewParent);
  */
  while (aFrameList) {
    aFrameList->SetParent(aNewParent);
    aFrameList->GetNextSibling(&aFrameList);
  }
  return NS_OK;
}



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

// Structures used to record the creation of pseudo table frames where 
// the content belongs to some ancestor. 

struct nsPseudoFrameData {
  nsIFrame*    mFrame;
  nsFrameItems mChildList;
  nsFrameItems mChildList2;

  nsPseudoFrameData();
  nsPseudoFrameData(nsPseudoFrameData& aOther);
  void Reset();
};

struct nsPseudoFrames {
  nsPseudoFrameData mTableOuter; 
  nsPseudoFrameData mTableInner;  
  nsPseudoFrameData mRowGroup;   
  nsPseudoFrameData mColGroup;
  nsPseudoFrameData mRow;   
  nsPseudoFrameData mCellOuter;
  nsPseudoFrameData mCellInner;

  // the frame type of the most descendant pseudo frame, no AddRef
  nsIAtom*          mLowestType;

  nsPseudoFrames();
  nsPseudoFrames& operator=(const nsPseudoFrames& aOther);
  void Reset(nsPseudoFrames* aSave = nsnull);
  PRBool IsEmpty() { return (!mLowestType && !mColGroup.mFrame); }
};

nsPseudoFrameData::nsPseudoFrameData()
: mFrame(nsnull), mChildList(), mChildList2()
{}

nsPseudoFrameData::nsPseudoFrameData(nsPseudoFrameData& aOther)
: mFrame(aOther.mFrame), mChildList(aOther.mChildList), 
  mChildList2(aOther.mChildList2)
{}

void
nsPseudoFrameData::Reset()
{
  mFrame = nsnull;
  mChildList.childList  = mChildList.lastChild  = nsnull;
  mChildList2.childList = mChildList2.lastChild = nsnull;
}

nsPseudoFrames::nsPseudoFrames() 
: mTableOuter(), mTableInner(), mRowGroup(), mColGroup(), 
  mRow(), mCellOuter(), mCellInner(), mLowestType(nsnull)
{}

nsPseudoFrames& nsPseudoFrames::operator=(const nsPseudoFrames& aOther)
{
  mTableOuter = aOther.mTableOuter;
  mTableInner = aOther.mTableInner;
  mColGroup   = aOther.mColGroup;
  mRowGroup   = aOther.mRowGroup;
  mRow        = aOther.mRow;
  mCellOuter  = aOther.mCellOuter;
  mCellInner  = aOther.mCellInner;
  mLowestType = aOther.mLowestType;

  return *this;
}
void
nsPseudoFrames::Reset(nsPseudoFrames* aSave) 
{
  if (aSave) {
    *aSave = *this;
  }

  mTableOuter.Reset();
  mTableInner.Reset();
  mColGroup.Reset();
  mRowGroup.Reset();
  mRow.Reset();
  mCellOuter.Reset();
  mCellInner.Reset();
  mLowestType = nsnull;
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
  nsPseudoFrames            mPseudoFrames;

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
    mFrameState(aFrameState),
    mPseudoFrames()
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
  virtual nsresult CreateTableCaptionFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableRowGroupFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableColFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableColGroupFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableRowFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellInnerFrame(nsIFrame** aNewFrame);

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
nsTableCreator::CreateTableCaptionFrame(nsIFrame** aNewFrame) {
  return NS_NewTableCaptionFrame(mPresShell, aNewFrame);
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

//MathML Mod - RBS
#ifdef MOZ_MATHML

// Structure used when creating MathML mtable frames
struct nsMathMLmtableCreator: public nsTableCreator {
  virtual nsresult CreateTableOuterFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellInnerFrame(nsIFrame** aNewFrame);

  nsMathMLmtableCreator(nsIPresShell* aPresShell)
    :nsTableCreator(aPresShell) {};
};

nsresult
nsMathMLmtableCreator::CreateTableOuterFrame(nsIFrame** aNewFrame)
{
  return NS_NewMathMLmtableOuterFrame(mPresShell, aNewFrame);
}

nsresult
nsMathMLmtableCreator::CreateTableCellInnerFrame(nsIFrame** aNewFrame)
{
  // only works if aNewFrame is an AreaFrame (to take care of the lineLayout logic)
  return NS_NewMathMLmtdFrame(mPresShell, aNewFrame);
}
#endif // MOZ_MATHML

// -----------------------------------------------------------

// return the child list that aFrame belongs on. does not ADDREF
PRBool GetCaptionAdjustedParent(nsIFrame*        aParentFrame,
                                const nsIFrame*  aChildFrame,
                                nsIFrame**       aAdjParentFrame)
{
  *aAdjParentFrame = aParentFrame;
  PRBool haveCaption = PR_FALSE;
  nsCOMPtr<nsIAtom> childFrameType;
  aChildFrame->GetFrameType(getter_AddRefs(childFrameType));

  if (nsLayoutAtoms::tableCaptionFrame == childFrameType.get()) { 
    haveCaption = PR_TRUE;
    nsCOMPtr<nsIAtom> parentFrameType;
    aParentFrame->GetFrameType(getter_AddRefs(parentFrameType));
    if (nsLayoutAtoms::tableFrame == parentFrameType.get()) {
      aParentFrame->GetParent(aAdjParentFrame);
    }
  }
  return haveCaption;
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

  // This initializes the Gfx Scrollbar Prefs booleans
  mGotGfxPrefs = PR_FALSE;
  mHasGfxScrollbars = PR_FALSE;
  mDoGfxListbox     = PR_FALSE;
  mDoGfxCombobox    = PR_FALSE;

  HasGfxScrollbars();

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

class GeneratedContentIterator : public nsIContentIterator
{
public:
  GeneratedContentIterator(nsIPresContext* aPresContext, nsIFrame* aFrame);
  virtual ~GeneratedContentIterator();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentIterator
  NS_IMETHOD Init(nsIContent* aRoot);
  NS_IMETHOD Init(nsIDOMRange* aRange);
  
  NS_IMETHOD First();
  NS_IMETHOD Last();
  NS_IMETHOD Next();
  NS_IMETHOD Prev();

  NS_IMETHOD CurrentNode(nsIContent **aNode);
  NS_IMETHOD IsDone();
  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  NS_IMETHOD MakePre();
  NS_IMETHOD MakePost();

private:
  nsCOMPtr<nsIPresContext>  mPresContext;
  nsIFrame*                 mParentFrame;
  nsIFrame*                 mCurrentChild;
  PRBool                    mIsDone;
};

GeneratedContentIterator::GeneratedContentIterator(nsIPresContext* aPresContext,
                                                   nsIFrame*       aFrame)
  : mPresContext(aPresContext), mParentFrame(aFrame), mIsDone(PR_FALSE)
{
  NS_INIT_REFCNT();
  First();
}

NS_IMPL_ISUPPORTS(GeneratedContentIterator, NS_GET_IID(nsIContentIterator));

GeneratedContentIterator::~GeneratedContentIterator()
{
}

NS_IMETHODIMP
GeneratedContentIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP
GeneratedContentIterator::Init(nsIDOMRange* aRange)
{
  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP
GeneratedContentIterator::First()
{
  // Get the first child frame and make it the current node
  mParentFrame->FirstChild(mPresContext, nsnull, &mCurrentChild);
  if (!mCurrentChild) {
    return NS_ERROR_FAILURE;
  }
  mIsDone = PR_FALSE;
  return NS_OK;
}

static nsIFrame*
GetNextChildFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get the last-in-flow
  while (PR_TRUE) {
    nsIFrame* nextInFlow;
    aFrame->GetNextInFlow(&nextInFlow);
    if (nextInFlow) {
      aFrame = nextInFlow;
    } else {
      break;
    }
  }

  // Get its next sibling
  nsIFrame* nextSibling;
  aFrame->GetNextSibling(&nextSibling);

  // If there's no next sibling, then check if the parent frame
  // has a next-in-flow and look there
  if (!nextSibling) {
    nsIFrame* parent;
    aFrame->GetParent(&parent);
    parent->GetNextInFlow(&parent);

    if (parent) {
      parent->FirstChild(aPresContext, nsnull, &nextSibling);
    }
  }

  return nextSibling;
}

NS_IMETHODIMP
GeneratedContentIterator::Last()
{
  nsIFrame* nextChild;

  // Starting with the first child walk and find the last child
  mCurrentChild = nsnull;
  mParentFrame->FirstChild(mPresContext, nsnull, &nextChild);
  while (nextChild) {
    mCurrentChild = nextChild;
    nextChild = ::GetNextChildFrame(mPresContext, nextChild);
  }

  if (!mCurrentChild) {
    return NS_ERROR_FAILURE;
  }
  mIsDone = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
GeneratedContentIterator::Next()
{
  nsIFrame* nextChild = ::GetNextChildFrame(mPresContext, mCurrentChild);

  if (nextChild) {
    // Advance to the next child
    mCurrentChild = nextChild;

    // If we're at the end then the collection is at the end
    mIsDone = (nsnull == ::GetNextChildFrame(mPresContext, mCurrentChild));
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

static nsIFrame*
GetPrevChildFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null pointer");

  // Get its previous sibling. Because we have a singly linked list we
  // need to search from the first child
  nsIFrame* parent;
  nsIFrame* firstChild;
  nsIFrame* prevSibling;

  aFrame->GetParent(&parent);
  parent->FirstChild(aPresContext, nsnull, &firstChild);

  NS_ASSERTION(firstChild, "parent has no first child");
  nsFrameList frameList(firstChild);
  prevSibling = frameList.GetPrevSiblingFor(aFrame);

  // If there's no previous sibling, then check if the parent frame
  // has a prev-in-flow and look there
  if (!prevSibling) {
    parent->GetPrevInFlow(&parent);

    if (parent) {
      parent->FirstChild(aPresContext, nsnull, &firstChild);
      frameList.SetFrames(firstChild);
      prevSibling = frameList.LastChild();
    }
  }
  
  // Get the first-in-flow
  while (PR_TRUE) {
    nsIFrame* prevInFlow;
    prevSibling->GetPrevInFlow(&prevInFlow);
    if (prevInFlow) {
      prevSibling = prevInFlow;
    } else {
      break;
    }
  }

  return prevSibling;
}

NS_IMETHODIMP
GeneratedContentIterator::Prev()
{
  nsIFrame* prevChild = ::GetPrevChildFrame(mPresContext, mCurrentChild);

  if (prevChild) {
    // Make the previous child the current child
    mCurrentChild = prevChild;
    
    // If we're at the beginning then the collection is at the end
    mIsDone = (nsnull == ::GetPrevChildFrame(mPresContext, mCurrentChild));
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GeneratedContentIterator::CurrentNode(nsIContent **aNode)
{
  if (mCurrentChild) {
    mCurrentChild->GetContent(aNode);
    return NS_OK;

  } else {
    *aNode = nsnull;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
GeneratedContentIterator::IsDone()
{
  return mIsDone ? NS_OK : NS_ENUMERATOR_FALSE;
}

NS_IMETHODIMP
GeneratedContentIterator::PositionAt(nsIContent* aCurNode)
{
  nsIFrame* child;

  // Starting with the first child frame search for the child frame
  // with the matching content object
  mParentFrame->FirstChild(mPresContext, nsnull, &child);
  while (child) {
    nsCOMPtr<nsIContent>  content;

    child->GetContent(getter_AddRefs(content));
    if (content.get() == aCurNode) {
      break;
    }
    child = ::GetNextChildFrame(mPresContext, child);
  }

  if (child) {
    // Make it the current child
    mCurrentChild = child;
    mIsDone = PR_FALSE;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GeneratedContentIterator::MakePre()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeneratedContentIterator::MakePost()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewGeneratedContentIterator(nsIPresContext*      aPresContext,
                               nsIFrame*            aFrame,
                               nsIContentIterator** aIterator)
{
  NS_ENSURE_ARG_POINTER(aIterator);
  if (!aIterator) {
    return NS_ERROR_NULL_POINTER;
  }
  
  // Make sure the frame corresponds to generated content
#ifdef DEBUG
  nsFrameState  frameState;
  aFrame->GetFrameState(&frameState);
  NS_ASSERTION(frameState & NS_FRAME_GENERATED_CONTENT, "unexpected frame");
#endif

  GeneratedContentIterator* it = new GeneratedContentIterator(aPresContext, aFrame);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContentIterator), (void **)aIterator);
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

    nsCOMPtr<nsINodeInfoManager> nimgr;
    nsresult rv = aDocument->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nimgr->GetNodeInfo(nsHTMLAtoms::img, nsnull, kNameSpaceID_None,
                       *getter_AddRefs(nodeInfo));

    NS_NewHTMLImageElement(&imageContent, nodeInfo);
    imageContent->SetHTMLAttribute(nsHTMLAtoms::src, contentString, PR_FALSE);

    // Set aContent as the parent content and set the document object. This
    // way event handling works
    imageContent->SetParent(aContent);
    imageContent->SetDocument(aDocument, PR_TRUE, PR_TRUE);
  
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
            content->SetDocument(aDocument, PR_TRUE, PR_TRUE);

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
          // XXX Don't assume default. Only use what is in 'quotes' property
          contentString.AssignWithConversion('\"');
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
      textContent->SetDocument(aDocument, PR_TRUE, PR_TRUE);
      
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
        result = domdoc->CreateElement(NS_ConvertASCIItoUCS2("SPAN"),getter_AddRefs(containerElement));//is the literal the correct way?
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
      rv = ConstructCheckboxControlFrame(aPresShell, aPresContext, aFrame, aContent, aStyleContext);
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

PRBool IsOnlyWhiteSpace(nsIContent* aContent)
{
  PRBool onlyWhiteSpace = PR_FALSE;
  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aContent);
  if (textContent) {
    textContent->IsOnlyWhitespace(&onlyWhiteSpace);
  }
  return onlyWhiteSpace;
}
    
/****************************************************
 **  BEGIN TABLE SECTION
 ****************************************************/

// The term pseudo frame is being used instead of anonymous frame, since anonymous
// frame has been used elsewhere to refer to frames that have generated content

PRBool
IsTableRelated(PRUint8 aDisplay)
{
  return (aDisplay == NS_STYLE_DISPLAY_TABLE)              ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP) ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP)    ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP) ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW)          ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN)       ||
         (aDisplay == NS_STYLE_DISPLAY_TABLE_CELL);
}

PRBool
IsTableRelated(nsIAtom* aParentType)
{
  return (nsLayoutAtoms::tableFrame         == aParentType)  ||
         (nsLayoutAtoms::tableCaptionFrame  == aParentType)  ||
         (nsLayoutAtoms::tableRowGroupFrame == aParentType)  ||
         (nsLayoutAtoms::tableColGroupFrame == aParentType)  ||
         (nsLayoutAtoms::tableColFrame      == aParentType)  ||
         (nsLayoutAtoms::tableRowFrame      == aParentType)  ||
         (nsLayoutAtoms::tableCellFrame     == aParentType);
}
           
PRBool
IsParentOf(nsIAtom* aParentFrameType,
           PRUint8  aChildDisplay)
{
  switch (aChildDisplay) {
  case NS_STYLE_DISPLAY_TABLE:
    return (!IsTableRelated(aParentFrameType));
  case NS_STYLE_DISPLAY_TABLE_CAPTION:
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    return (nsLayoutAtoms::tableFrame == aParentFrameType);
  case NS_STYLE_DISPLAY_TABLE_ROW:
    return (nsLayoutAtoms::tableRowGroupFrame == aParentFrameType);
  case NS_STYLE_DISPLAY_TABLE_COLUMN:
    return (nsLayoutAtoms::tableColGroupFrame == aParentFrameType);
  case NS_STYLE_DISPLAY_TABLE_CELL:
    return (nsLayoutAtoms::tableRowFrame == aParentFrameType);
  default:
    return (!IsTableRelated(aParentFrameType));
  }    
}

nsIFrame*
GetOuterTableFrame(nsIFrame* aParentFrame) 
{
  nsIFrame* parent;
  nsCOMPtr<nsIAtom> frameType;
  aParentFrame->GetFrameType(getter_AddRefs(frameType));
  if (nsLayoutAtoms::tableOuterFrame == frameType.get()) {
    parent = aParentFrame;
  }
  else {
    aParentFrame->GetParent(&parent);
  }
  return parent;
}
    
void
GetNonScrollFrame(nsIFrame&  aFrameIn,
                  nsIFrame*& aNonScrollFrame,
                  nsIAtom*&  aNonScrollFrameType)
{
  nsIAtom* frameType;
  aFrameIn.GetFrameType(&frameType);

  if (nsLayoutAtoms::scrollFrame == frameType) {
    // get the scrolled frame if there is a scroll frame
    nsIScrollableFrame* scrollable = nsnull;
    nsresult rv = aFrameIn.QueryInterface(kIScrollableFrameIID, (void **)&scrollable);
    if (NS_SUCCEEDED(rv) && (scrollable)) {
      scrollable->GetScrolledFrame(nsnull, aNonScrollFrame);
    }
  }
  else {
    aNonScrollFrame = &aFrameIn;
  }
  aNonScrollFrameType = nsnull;
  if (aNonScrollFrame) {
    aNonScrollFrame->GetFrameType(&aNonScrollFrameType);
  }
}

nsresult 
ProcessPseudoFrame(nsIPresContext*    aPresContext,
                   nsPseudoFrameData& aPseudoData,
                   nsIFrame*&         aParent)
{
  nsresult rv = NS_OK;
  if (!aPresContext) return rv;

  aParent = aPseudoData.mFrame;
  nsFrameItems* items = &aPseudoData.mChildList;
  if (items && items->childList) {
    rv = aParent->SetInitialChildList(aPresContext, nsnull, items->childList);
    if (NS_FAILED(rv)) return rv;
  }
  aPseudoData.Reset();
  return rv;
}

nsresult 
ProcessPseudoTableFrame(nsIPresContext* aPresContext,
                        nsPseudoFrames& aPseudoFrames,
                        nsIFrame*&      aParent)
{
  nsresult rv = NS_OK;
  if (!aPresContext) return rv;

  // process the col group frame, if it exists
  if (aPseudoFrames.mColGroup.mFrame) {
    rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mColGroup, aParent);
  }

  // process the inner table frame
  rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mTableInner, aParent);

  // process the outer table frame
  aParent = aPseudoFrames.mTableOuter.mFrame;
  nsFrameItems* items = &aPseudoFrames.mTableOuter.mChildList;
  if (items && items->childList) {
    rv = aParent->SetInitialChildList(aPresContext, nsnull, items->childList);
    if (NS_FAILED(rv)) return rv;
  }
  nsFrameItems* captions = &aPseudoFrames.mTableOuter.mChildList2;
  if (captions && captions->childList) {
    rv = aParent->SetInitialChildList(aPresContext, nsLayoutAtoms::captionList, captions->childList);
  }
  aPseudoFrames.mTableOuter.Reset();
  return rv;
}

nsresult 
ProcessPseudoCellFrame(nsIPresContext* aPresContext,
                       nsPseudoFrames& aPseudoFrames,
                       nsIFrame*&      aParent)
{
  nsresult rv = NS_OK;
  if (!aPresContext) return rv;

  rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mCellInner, aParent);
  if (NS_FAILED(rv)) return rv;
  rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mCellOuter, aParent);
  return rv;
}


nsresult 
ProcessPseudoFrames(nsIPresContext* aPresContext,
                    nsPseudoFrames& aPseudoFrames,
                    nsIAtom*        aHighestType,
                    nsIFrame*&      aHighestFrame)
{
  nsresult rv = NS_OK;
  if (!aPresContext) return rv;

  aHighestFrame = nsnull;

  if (nsLayoutAtoms::tableFrame == aPseudoFrames.mLowestType) {
    rv = ProcessPseudoTableFrame(aPresContext, aPseudoFrames, aHighestFrame);
    if (nsLayoutAtoms::tableOuterFrame == aHighestType) return rv;
    
    if (aPseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (nsLayoutAtoms::tableCellFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRow, aHighestFrame);
      if (nsLayoutAtoms::tableRowFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRowGroup, aHighestFrame);
      if (nsLayoutAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
  }
  else if (nsLayoutAtoms::tableRowGroupFrame == aPseudoFrames.mLowestType) {
    rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRowGroup, aHighestFrame);
    if (nsLayoutAtoms::tableRowGroupFrame == aHighestType) return rv;

    if (aPseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (nsLayoutAtoms::tableOuterFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (nsLayoutAtoms::tableCellFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRow, aHighestFrame);
      if (nsLayoutAtoms::tableRowFrame == aHighestType) return rv;
    }
  }
  else if (nsLayoutAtoms::tableRowFrame == aPseudoFrames.mLowestType) {
    rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRow, aHighestFrame);
    if (nsLayoutAtoms::tableRowFrame == aHighestType) return rv;

    if (aPseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRowGroup, aHighestFrame);
      if (nsLayoutAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (nsLayoutAtoms::tableOuterFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (nsLayoutAtoms::tableCellFrame == aHighestType) return rv;
    }
  }
  else if (nsLayoutAtoms::tableCellFrame == aPseudoFrames.mLowestType) {
    rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
    if (nsLayoutAtoms::tableCellFrame == aHighestType) return rv;

    if (aPseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRow, aHighestFrame);
      if (nsLayoutAtoms::tableRowFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mRowGroup, aHighestFrame);
      if (nsLayoutAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
    if (aPseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(aPresContext, aPseudoFrames, aHighestFrame);
    }
  }
  else if (aPseudoFrames.mColGroup.mFrame) { 
    // process the col group frame
    rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mColGroup, aHighestFrame);
  }

  return rv;
}

nsresult 
ProcessPseudoFrames(nsIPresContext* aPresContext,
                    nsPseudoFrames& aPseudoFrames,
                    nsFrameItems&   aItems)
{
  nsIFrame* highestFrame;
  nsresult rv = ProcessPseudoFrames(aPresContext, aPseudoFrames, nsnull, highestFrame);
  if (highestFrame) {
    aItems.AddChild(highestFrame);
  }
  aPseudoFrames.Reset();
  return rv;
}

nsresult 
ProcessPseudoFrames(nsIPresContext* aPresContext,
                    nsPseudoFrames& aPseudoFrames,
                    nsIAtom*        aHighestType)
{
  nsIFrame* highestFrame;
  nsresult rv = ProcessPseudoFrames(aPresContext, aPseudoFrames, aHighestType, highestFrame);
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoTableFrame(nsIPresShell*            aPresShell,
                                              nsIPresContext*          aPresContext,
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mCellInner.mFrame) 
                          ? aState.mPseudoFrames.mCellInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsCOMPtr<nsIStyleContext> parentStyle;
  nsCOMPtr<nsIContent>      parentContent;
  nsCOMPtr<nsIStyleContext> childStyle;

  parentFrame->GetStyleContext(getter_AddRefs(parentStyle)); 
  parentFrame->GetContent(getter_AddRefs(parentContent));   

  // create the SC for the inner table which will be the parent of the outer table's SC
  aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::tablePseudo, 
                                             parentStyle, PR_FALSE, 
                                             getter_AddRefs(childStyle));

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mTableOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mTableInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableFrame(aPresShell, aPresContext, aState, parentContent,
                           parentFrame, childStyle.get(), aTableCreator,
                           PR_TRUE, items, pseudoOuter.mFrame, 
                           pseudoInner.mFrame, pseudoParent);

  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  pseudoOuter.mChildList.AddChild(pseudoInner.mFrame);
  aState.mPseudoFrames.mLowestType = nsLayoutAtoms::tableFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mCellInner.mFrame) {
    aState.mPseudoFrames.mCellInner.mChildList.AddChild(pseudoOuter.mFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoRowGroupFrame(nsIPresShell*            aPresShell,
                                                 nsIPresContext*          aPresContext,
                                                 nsTableCreator&          aTableCreator,
                                                 nsFrameConstructorState& aState, 
                                                 nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mTableInner.mFrame) 
                          ? aState.mPseudoFrames.mTableInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsCOMPtr<nsIStyleContext> parentStyle;
  nsCOMPtr<nsIContent>      parentContent;
  nsCOMPtr<nsIStyleContext> childStyle;

  parentFrame->GetStyleContext(getter_AddRefs(parentStyle)); 
  parentFrame->GetContent(getter_AddRefs(parentContent));   

  aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::tableRowGroupPseudo, 
                                             parentStyle, PR_FALSE, 
                                             getter_AddRefs(childStyle));

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRowGroup;

  // construct the pseudo row group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, parentContent,
                                   parentFrame, childStyle.get(), aTableCreator,
                                   PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  aState.mPseudoFrames.mLowestType = nsLayoutAtoms::tableRowGroupFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mTableInner.mFrame) {
    aState.mPseudoFrames.mTableInner.mChildList.AddChild(pseudo.mFrame);
  }

  return rv;
}

nsresult 
nsCSSFrameConstructor::CreatePseudoColGroupFrame(nsIPresShell*            aPresShell,
                                                 nsIPresContext*          aPresContext,
                                                 nsTableCreator&          aTableCreator,
                                                 nsFrameConstructorState& aState, 
                                                 nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mTableInner.mFrame) 
                          ? aState.mPseudoFrames.mTableInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsCOMPtr<nsIStyleContext> parentStyle;
  nsCOMPtr<nsIContent>      parentContent;
  nsCOMPtr<nsIStyleContext> childStyle;

  parentFrame->GetStyleContext(getter_AddRefs(parentStyle)); 
  parentFrame->GetContent(getter_AddRefs(parentContent));   

  aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::tableColGroupPseudo, 
                                             parentStyle, PR_FALSE, 
                                             getter_AddRefs(childStyle));

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mColGroup;

  // construct the pseudo col group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, parentContent,
                                   parentFrame, childStyle.get(), aTableCreator,
                                   PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;
  ((nsTableColGroupFrame*)pseudo.mFrame)->SetType(eColGroupAnonymousCol);

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mTableInner.mFrame) {
    aState.mPseudoFrames.mTableInner.mChildList.AddChild(pseudo.mFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoRowFrame(nsIPresShell*            aPresShell,
                                            nsIPresContext*          aPresContext,
                                            nsTableCreator&          aTableCreator,
                                            nsFrameConstructorState& aState, 
                                            nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mRowGroup.mFrame) 
                          ? aState.mPseudoFrames.mRowGroup.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsCOMPtr<nsIStyleContext> parentStyle;
  nsCOMPtr<nsIContent>      parentContent;
  nsCOMPtr<nsIStyleContext> childStyle;

  parentFrame->GetStyleContext(getter_AddRefs(parentStyle)); 
  parentFrame->GetContent(getter_AddRefs(parentContent));   

  aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::tableRowPseudo, 
                                             parentStyle, PR_FALSE, 
                                             getter_AddRefs(childStyle));

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRow;

  // construct the pseudo row as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, parentContent,
                              parentFrame, childStyle.get(), aTableCreator,
                              PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  aState.mPseudoFrames.mLowestType = nsLayoutAtoms::tableRowFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mRowGroup.mFrame) {
    aState.mPseudoFrames.mRowGroup.mChildList.AddChild(pseudo.mFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoCellFrame(nsIPresShell*            aPresShell,
                                             nsIPresContext*          aPresContext,
                                             nsTableCreator&          aTableCreator,
                                             nsFrameConstructorState& aState, 
                                             nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mRow.mFrame) 
                          ? aState.mPseudoFrames.mRow.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsCOMPtr<nsIStyleContext> parentStyle;
  nsCOMPtr<nsIContent>      parentContent;
  nsCOMPtr<nsIStyleContext> childStyle;

  parentFrame->GetStyleContext(getter_AddRefs(parentStyle)); 
  parentFrame->GetContent(getter_AddRefs(parentContent));   

  aPresContext->ResolvePseudoStyleContextFor(parentContent, nsHTMLAtoms::tableCellPseudo, 
                                             parentStyle, PR_FALSE, 
                                             getter_AddRefs(childStyle));

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mCellOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mCellInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, parentContent,
                               parentFrame, childStyle.get(), aTableCreator,
                               PR_TRUE, items, pseudoOuter.mFrame, 
                               pseudoInner.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  pseudoOuter.mChildList.AddChild(pseudoInner.mFrame);
  aState.mPseudoFrames.mLowestType = nsLayoutAtoms::tableCellFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mRow.mFrame) {
    aState.mPseudoFrames.mRow.mChildList.AddChild(pseudoOuter.mFrame);
  }

  return rv;
}

// called if the parent is not a table
nsresult 
nsCSSFrameConstructor::GetPseudoTableFrame(nsIPresShell*            aPresShell, 
                                           nsIPresContext*          aPresContext, 
                                           nsTableCreator&          aTableCreator,
                                           nsFrameConstructorState& aState, 
                                           nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowGroupFrame == parentFrameType.get()) { // row group parent
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowFrame == parentFrameType.get())) { // row parent
      rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
    }
    rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mTableInner.mFrame) { 
      if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
        rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState);
        if (NS_FAILED(rv)) return rv;
      }
      if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
        rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState);
        if (NS_FAILED(rv)) return rv;
      }
      CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
  }
  return rv;
}

// called if the parent is not a col group
nsresult 
nsCSSFrameConstructor::GetPseudoColGroupFrame(nsIPresShell*            aPresShell, 
                                              nsIPresContext*          aPresContext, 
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowGroupFrame == parentFrameType.get()) {  // row group parent
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowFrame == parentFrameType.get())) { // row parent
      rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableCellFrame == parentFrameType.get()) || // cell parent
                   !IsTableRelated(parentFrameType.get())) { // block parent
      rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
    }
    rv = CreatePseudoColGroupFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mColGroup.mFrame) { 
      if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
        rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
        rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      if (pseudoFrames.mCellOuter.mFrame && !(pseudoFrames.mTableOuter.mFrame)) {
        rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      rv = CreatePseudoColGroupFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
  }
  return rv;
}

// called if the parent is not a row group
nsresult 
nsCSSFrameConstructor::GetPseudoRowGroupFrame(nsIPresShell*            aPresShell, 
                                              nsIPresContext*          aPresContext, 
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowFrame == parentFrameType.get()) {  // row parent
      rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableCellFrame == parentFrameType.get()) || // cell parent
        !IsTableRelated(parentFrameType)) { // block parent
      rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
    }
    rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mRowGroup.mFrame) { 
      if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
        rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      if (pseudoFrames.mCellOuter.mFrame && !(pseudoFrames.mTableOuter.mFrame)) {
        rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
  }
  return rv;
}

// called if the parent is not a row
nsresult
nsCSSFrameConstructor::GetPseudoRowFrame(nsIPresShell*            aPresShell, 
                                         nsIPresContext*          aPresContext, 
                                         nsTableCreator&          aTableCreator,
                                         nsFrameConstructorState& aState, 
                                         nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if ((nsLayoutAtoms::tableCellFrame == parentFrameType.get()) || // cell parent
        !IsTableRelated(parentFrameType)) { // block parent
      rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableFrame == parentFrameType.get())) { // table parent
      rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
    }
    rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mRow.mFrame) { 
      if (pseudoFrames.mCellOuter.mFrame && !pseudoFrames.mTableOuter.mFrame) {
        rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      if (pseudoFrames.mTableInner.mFrame && !(pseudoFrames.mRowGroup.mFrame)) {
        rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState);
      }
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
  }
  return rv;
}

// called if the parent is not a cell or block
nsresult 
nsCSSFrameConstructor::GetPseudoCellFrame(nsIPresShell*            aPresShell, 
                                          nsIPresContext*          aPresContext, 
                                          nsTableCreator&          aTableCreator,
                                          nsFrameConstructorState& aState, 
                                          nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableFrame == parentFrameType.get()) { // table parent
      rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowGroupFrame == parentFrameType.get())) { // row group parent
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
  }
  else if (!pseudoFrames.mCellOuter.mFrame) { 
    if (pseudoFrames.mTableInner.mFrame && !(pseudoFrames.mRowGroup.mFrame)) {
      rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
    if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState);
    }
    rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState);
  }
  return rv;
}

nsresult 
nsCSSFrameConstructor::GetParentFrame(nsIPresShell*            aPresShell,
                                      nsIPresContext*          aPresContext,
                                      nsTableCreator&          aTableCreator,
                                      nsIFrame&                aParentFrameIn, 
                                      nsIAtom*                 aChildFrameType, 
                                      nsFrameConstructorState& aState, 
                                      nsIFrame*&               aParentFrame,
                                      PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrameIn.GetFrameType(getter_AddRefs(parentFrameType));
  nsIFrame* pseudoParentFrame = nsnull;
  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  aParentFrame = &aParentFrameIn;
  aIsPseudoParent = PR_FALSE;

  if (nsLayoutAtoms::tableOuterFrame == aChildFrameType) { // table child
    if (IsTableRelated(parentFrameType)) { // need pseudo cell parent
      rv = GetPseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mCellInner.mFrame;
    }
  } 
  else if (nsLayoutAtoms::tableCaptionFrame == aChildFrameType) { // caption child
    if (nsLayoutAtoms::tableOuterFrame != parentFrameType.get()) { // need pseudo table parent
      rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableOuter.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableColGroupFrame == aChildFrameType) { // col group child
    if (nsLayoutAtoms::tableFrame != parentFrameType.get()) { // need pseudo table parent
      rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableColFrame == aChildFrameType) { // col child
    if (nsLayoutAtoms::tableColGroupFrame != parentFrameType.get()) { // need pseudo col group parent
      rv = GetPseudoColGroupFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mColGroup.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableRowGroupFrame == aChildFrameType) { // row group child
    // XXX can this go away?
    if (nsLayoutAtoms::tableFrame != parentFrameType.get()) {
      // trees allow row groups to contain row groups, so don't create pseudo frames
        rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
        if (NS_FAILED(rv)) return rv;
        pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
     }
  }
  else if (nsLayoutAtoms::tableRowFrame == aChildFrameType) { // row child
    if (nsLayoutAtoms::tableRowGroupFrame != parentFrameType.get()) { // need pseudo row group parent
      rv = GetPseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRowGroup.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableCellFrame == aChildFrameType) { // cell child
    if (nsLayoutAtoms::tableRowFrame != parentFrameType.get()) { // need pseudo row parent
      rv = GetPseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRow.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableFrame == aChildFrameType) { // invalid
    NS_ASSERTION(PR_FALSE, "GetParentFrame called on nsLayoutAtoms::tableFrame child");
  }
  else { // foreign frame
    if (IsTableRelated(parentFrameType) && 
        (nsLayoutAtoms::tableCaptionFrame != parentFrameType.get())) { // need pseudo cell parent
      rv = GetPseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mCellInner.mFrame;
    }
  }
  
  if (pseudoParentFrame) {
    aParentFrame = pseudoParentFrame;
    aIsPseudoParent = PR_TRUE;
  }

  return rv;
}


void
FixUpOuterTableFloat(nsIStyleContext* aOuterSC,
                     nsIStyleContext* aInnerSC)
{
  nsStyleDisplay* outerStyleDisplay = (nsStyleDisplay*)aOuterSC->GetMutableStyleData(eStyleStruct_Display);
  nsStyleDisplay* innerStyleDisplay = (nsStyleDisplay*)aInnerSC->GetStyleData(eStyleStruct_Display);
  if (outerStyleDisplay->mFloats != innerStyleDisplay->mFloats) {
    outerStyleDisplay->mFloats = innerStyleDisplay->mFloats;
  }
}

// Construct the outer, inner table frames and the children frames for the table. 
nsresult
nsCSSFrameConstructor::ConstructTableFrame(nsIPresShell*            aPresShell,
                                           nsIPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrameIn,
                                           nsIStyleContext*         aStyleContext,
                                           nsTableCreator&          aTableCreator,
                                           PRBool                   aIsPseudo,
                                           nsFrameItems&            aChildItems,
                                           nsIFrame*&               aNewOuterFrame,
                                           nsIFrame*&               aNewInnerFrame,
                                           PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  // Create the outer table frame which holds the caption and inner table frame
  aTableCreator.CreateTableOuterFrame(&aNewOuterFrame);

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableOuterFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mTableOuter.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableOuterFrame);
    }
  }

  // create the pseudo SC for the outer table as a child of the inner SC
  nsCOMPtr<nsIStyleContext> outerStyleContext;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::tableOuterPseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(outerStyleContext));
  FixUpOuterTableFloat(outerStyleContext, aStyleContext);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned  
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, outerStyleContext, nsnull, aNewOuterFrame);  
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewOuterFrame,
                                           outerStyleContext, nsnull, PR_FALSE);

  // Create the inner table frame
  aTableCreator.CreateTableFrame(&aNewInnerFrame);

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aNewOuterFrame, aStyleContext, nsnull, aNewInnerFrame);

  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;

    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewInnerFrame,
                              aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;

    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aContent, aNewInnerFrame,
                          childItems);

    // Set the inner table frame's initial primary list 
    aNewInnerFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

    // Set the outer table frame's primary and option lists
    aNewOuterFrame->SetInitialChildList(aPresContext, nsnull, aNewInnerFrame);
    if (captionFrame) {
      aNewOuterFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::captionList, captionFrame);
    }
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mCellInner.mChildList.AddChild(aNewOuterFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCaptionFrame(nsIPresShell*            aPresShell,
                                                  nsIPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrameIn,
                                                  nsIStyleContext*         aStyleContext,
                                                  nsTableCreator&          aTableCreator,
                                                  nsFrameItems&            aChildItems,
                                                  nsIFrame*&               aNewFrame,
                                                  PRBool&                  aIsPseudoParent)

{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  // this frame may have a pseudo parent
  GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                 nsLayoutAtoms::tableCaptionFrame, aState, parentFrame, aIsPseudoParent);
  if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
  }

  rv = aTableCreator.CreateTableCaptionFrame(&aNewFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewFrame);

  nsFrameItems childItems;
  // pass in aTableCreator so ProcessChildren will call TableProcessChildren
  rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                       PR_TRUE, childItems, PR_TRUE, &aTableCreator);
  if (NS_FAILED(rv)) return rv;
  aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
  if (aIsPseudoParent) {
    aState.mPseudoFrames.mTableOuter.mChildList2.AddChild(aNewFrame);
  }
  
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableRowGroupFrame(nsIPresShell*            aPresShell, 
                                                   nsIPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsIStyleContext*         aStyleContext,
                                                   nsTableCreator&          aTableCreator,
                                                   PRBool                   aIsPseudo,
                                                   nsFrameItems&            aChildItems,
                                                   nsIFrame*&               aNewFrame, 
                                                   PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableRowGroupFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mRowGroup.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableRowGroupFrame);
    }
  }

  const nsStyleDisplay* styleDisplay = 
    (const nsStyleDisplay*) aStyleContext->GetStyleData(eStyleStruct_Display);

  rv = aTableCreator.CreateTableRowGroupFrame(&aNewFrame);

  nsIFrame* scrollFrame = nsnull;
  if (IsScrollable(aPresContext, styleDisplay)) {
    // Create an area container for the frame
    BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, 
                     aNewFrame, parentFrame, scrollFrame, aStyleContext);

  } 
  else {
    if (NS_FAILED(rv)) return rv;
    InitAndRestoreFrame(aPresContext, aState, aContent, parentFrame, 
                        aStyleContext, nsnull, aNewFrame);
  }

  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, 
                              aNewFrame, aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aContent, aNewFrame,
                          childItems);

    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aIsPseudoParent) {
      nsIFrame* child = (scrollFrame) ? scrollFrame : aNewFrame;
      aState.mPseudoFrames.mTableInner.mChildList.AddChild(child);
    }
  } 

  // if there is a scroll frame, use it as the one constructed
  if (scrollFrame) {
    aNewFrame = scrollFrame;
  }
  
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableColGroupFrame(nsIPresShell*            aPresShell, 
                                                   nsIPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsIStyleContext*         aStyleContext,
                                                   nsTableCreator&          aTableCreator,
                                                   PRBool                   aIsPseudo,
                                                   nsFrameItems&            aChildItems,
                                                   nsIFrame*&               aNewFrame, 
                                                   PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableColGroupFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mColGroup.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableColGroupFrame);
    }
  }

  rv = aTableCreator.CreateTableColGroupFrame(&aNewFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewFrame);

  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                              aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mTableInner.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableRowFrame(nsIPresShell*            aPresShell, 
                                              nsIPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsIStyleContext*         aStyleContext,
                                              nsTableCreator&          aTableCreator,
                                              PRBool                   aIsPseudo,
                                              nsFrameItems&            aChildItems,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableRowFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mRow.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableRowFrame);
    }
  }

  rv = aTableCreator.CreateTableRowFrame(&aNewFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewFrame);
  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                              aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aContent, aNewFrame,
                          childItems);

    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mRowGroup.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsIPresShell*            aPresShell, 
                                              nsIPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsIStyleContext*         aStyleContext,
                                              nsTableCreator&          aTableCreator,
                                              PRBool                   aIsPseudo,
                                              nsFrameItems&            aChildItems,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableColFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
  }

  rv = aTableCreator.CreateTableColFrame(&aNewFrame);
  if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewFrame);
  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                              aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mColGroup.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsIPresShell*            aPresShell, 
                                               nsIPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrameIn,
                                               nsIStyleContext*         aStyleContext,
                                               nsTableCreator&          aTableCreator,
                                               PRBool                   aIsPseudo,
                                               nsFrameItems&            aChildItems,
                                               nsIFrame*&               aNewCellOuterFrame,
                                               nsIFrame*&               aNewCellInnerFrame,
                                               PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableCellFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mCellOuter.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableCellFrame);
    }
  }

  rv = aTableCreator.CreateTableCellFrame(&aNewCellOuterFrame);
  if (NS_FAILED(rv)) return rv;
 
  // Initialize the table cell frame
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewCellOuterFrame);
  // Create a block frame that will format the cell's content
  rv = aTableCreator.CreateTableCellInnerFrame(&aNewCellInnerFrame);

  if (NS_FAILED(rv)) {
    aNewCellOuterFrame->Destroy(aPresContext);
    aNewCellOuterFrame = nsnull;
    return rv;
  }
  
  // Resolve pseudo style and initialize the body cell frame
  nsCOMPtr<nsIStyleContext> innerPseudoStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::cellContentPseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(innerPseudoStyle));
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aNewCellOuterFrame, innerPseudoStyle, nsnull, aNewCellInnerFrame);

  if (!aIsPseudo) {
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);

    // The block frame is a floater container
    nsFrameConstructorSaveState floaterSaveState;
    aState.PushFloaterContainingBlock(aNewCellInnerFrame, floaterSaveState,
                                      haveFirstLetterStyle, haveFirstLineStyle);

    // Process the child content
    nsFrameItems childItems;
    // pass in null tableCreator so ProcessChildren will not call TableProcessChildren
    rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewCellInnerFrame, 
                         PR_TRUE, childItems, PR_TRUE, nsnull);
    if (NS_FAILED(rv)) return rv;

    // if there are any tree anonymous children create frames for them
    nsCOMPtr<nsIAtom> tagName;
    aContent->GetTag(*getter_AddRefs(tagName));
    CreateAnonymousTableCellFrames(aPresShell, aPresContext, tagName, aState, aContent, 
                                  aNewCellInnerFrame, aNewCellOuterFrame, childItems);

    aNewCellInnerFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mFloatedItems.childList) {
      aNewCellInnerFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::floaterList,
                                             aState.mFloatedItems.childList);
    }

    aNewCellOuterFrame->SetInitialChildList(aPresContext, nsnull, aNewCellInnerFrame);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mRow.mChildList.AddChild(aNewCellOuterFrame);
    }
  }

  return rv;
}

PRBool 
nsCSSFrameConstructor::MustGeneratePseudoParent(nsIPresContext* aPresContext,
                                                nsIFrame*       aParentFrame,
                                                nsIAtom*        aTag,
                                                nsIContent*     aContent)
{
  nsCOMPtr<nsIStyleContext> styleContext;

  nsresult rv = ResolveStyleContext(aPresContext, aParentFrame, aContent, aTag, getter_AddRefs(styleContext));
  if (NS_FAILED(rv)) return PR_FALSE;

  const nsStyleDisplay* display = (const nsStyleDisplay*)
    styleContext->GetStyleData(eStyleStruct_Display);

  if (NS_STYLE_DISPLAY_NONE == display->mDisplay) return PR_FALSE;
    
  // check tags first

  if ((nsLayoutAtoms::textTagName == aTag)) {
    return !IsOnlyWhiteSpace(aContent);
  }

  // exclude tags
  if ( (nsLayoutAtoms::commentTagName == aTag) ||
       (nsHTMLAtoms::form             == aTag) ) {
    return PR_FALSE;
  }

// XXX DJF - when should pseudo frames be constructed for MathML?
#ifdef MOZ_MATHML
  if ( (nsMathMLAtoms::math == aTag) ) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
#endif

  return PR_TRUE;
}

// this is called when a non table related element is a child of a table, row group, 
// or row, but not a cell.
nsresult
nsCSSFrameConstructor::ConstructTableForeignFrame(nsIPresShell*            aPresShell, 
                                                  nsIPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrameIn,
                                                  nsIStyleContext*         aStyleContext,
                                                  nsTableCreator&          aTableCreator,
                                                  nsFrameItems&            aChildItems,
                                                  nsIFrame*&               aNewFrame,
                                                  PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  aNewFrame = nsnull;
  if (!aPresShell || !aPresContext || !aParentFrameIn) return rv;

  nsIFrame* parentFrame = nsnull;
  aIsPseudoParent = PR_FALSE;

  // XXX form code needs to be fixed so that the forms can work without a frame.
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));

  if (nsHTMLAtoms::form == tag.get()) {
    // A form doesn't get a psuedo frame parent, but it needs a frame.
    // if the parent is a table, put the form in the outer table frame
    // otherwise use aParentFrameIn as the parent
    nsCOMPtr<nsIAtom> frameType;
    aParentFrameIn->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::tableFrame == frameType.get()) {
      aParentFrameIn->GetParent(&parentFrame);
    }
    else {
      parentFrame = aParentFrameIn;
    }
  }
  // Do not construct pseudo frames for trees 
  else if (MustGeneratePseudoParent(aPresContext, aParentFrameIn, tag.get(), aContent)) {
    // this frame may have a pseudo parent, use block frame type to trigger foreign
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::blockFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    //char buf[256];
    //sprintf(buf, "anonymous frame constructed for %s", tag.get());
    //NS_WARN_IF_FALSE(PR_FALSE, buf); 
  }

  if (!parentFrame) return rv; // if pseudo frame wasn't created

  // save the pseudo frame state XXX - why
  nsPseudoFrames prevPseudoFrames; 
  aState.mPseudoFrames.Reset(&prevPseudoFrames);

  nsFrameItems items;
  rv = ConstructFrame(aPresShell, aPresContext, aState, aContent, parentFrame, items);
  aNewFrame = items.childList;

  // restore the pseudo frame state XXX - why
  aState.mPseudoFrames = prevPseudoFrames;

  if (aIsPseudoParent) {
    aState.mPseudoFrames.mCellInner.mChildList.AddChild(aNewFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::TableProcessChildren(nsIPresShell*            aPresShell, 
                                            nsIPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsTableCreator&          aTableCreator,
                                            nsFrameItems&            aChildItems,
                                            nsIFrame*&               aCaption)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aContent || !aParentFrame) return rv;

  aCaption = nsnull;

  // save the incoming pseudo frame state 
  nsPseudoFrames priorPseudoFrames; 
  aState.mPseudoFrames.Reset(&priorPseudoFrames);

  nsCOMPtr<nsIAtom> parentFrameType;
  aParentFrame->GetFrameType(getter_AddRefs(parentFrameType));
  nsCOMPtr<nsIStyleContext> parentStyleContext;
  aParentFrame->GetStyleContext(getter_AddRefs(parentStyleContext));

  PRInt32 count;
  aContent->ChildCount(count);

  for (PRInt32 childX = 0; childX < count; childX++) { // iterate the child content
    nsCOMPtr<nsIContent> childContent;
    rv = aContent->ChildAt(childX, *getter_AddRefs(childContent));
    if (childContent.get() && NS_SUCCEEDED(rv)) {
      rv = TableProcessChild(aPresShell, aPresContext, aState, *childContent.get(), aParentFrame,
                             parentFrameType.get(), parentStyleContext.get(),
                             aTableCreator, aChildItems, aCaption);
    }
    if (NS_FAILED(rv)) return rv;
  }
  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
  }

  // restore the incoming pseudo frame state 
  aState.mPseudoFrames = priorPseudoFrames;
  return rv;
}

nsresult
nsCSSFrameConstructor::TableProcessChild(nsIPresShell*            aPresShell, 
                                         nsIPresContext*          aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent&              aChildContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aParentFrameType,
                                         nsIStyleContext*         aParentStyleContext,
                                         nsTableCreator&          aTableCreator,
                                         nsFrameItems&            aChildItems,
                                         nsIFrame*&               aCaption)
{
  nsresult rv = NS_OK;
  
  PRBool childIsCaption = PR_FALSE;
  PRBool isPseudoParent = PR_FALSE;
    
  nsIFrame* childFrame = nsnull;
  nsCOMPtr<nsIStyleContext> childStyleContext;

  // Resolve the style context and get its display
  aPresContext->ResolveStyleContextFor(&aChildContent, aParentStyleContext, PR_FALSE,
                                       getter_AddRefs(childStyleContext));
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    childStyleContext->GetStyleData(eStyleStruct_Display);

  switch (styleDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_TABLE:
    nsIFrame* innerTableFrame;
    rv = ConstructTableFrame(aPresShell, aPresContext, aState, &aChildContent, aParentFrame,
                             childStyleContext, aTableCreator, PR_FALSE, aChildItems,
                             childFrame, innerTableFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_CAPTION:
    if (!aCaption) {  // only allow one caption
      nsIFrame* parentFrame = GetOuterTableFrame(aParentFrame);
      rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                      parentFrame, childStyleContext, aTableCreator, 
                                      aChildItems, aCaption, isPseudoParent);
    }
    childIsCaption = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                     aParentFrame, childStyleContext, aTableCreator, 
                                     PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                     aParentFrame, childStyleContext, aTableCreator, 
                                     PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_ROW:
    rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                aParentFrame, childStyleContext, aTableCreator, 
                                PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN:
    rv = ConstructTableColFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                aParentFrame, childStyleContext, aTableCreator, 
                                PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;


  case NS_STYLE_DISPLAY_TABLE_CELL:
    nsIFrame* innerCell;
    rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                 aParentFrame, childStyleContext, aTableCreator, PR_FALSE, 
                                 aChildItems, childFrame, innerCell, isPseudoParent);
    break;

  default:
    rv = ConstructTableForeignFrame(aPresShell, aPresContext, aState, &aChildContent, 
                                    aParentFrame, childStyleContext, aTableCreator, 
                                    aChildItems, childFrame, isPseudoParent);
    break;
  }

  // for every table related frame except captions and ones with pseudo parents, 
  // link into the child list
  if (childFrame && !childIsCaption && !isPseudoParent) { 
    aChildItems.AddChild(childFrame);
  }
  return rv;
}

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

static PRBool
IsCanvasFrame(nsIFrame* aFrame)
{
  nsCOMPtr<nsIAtom>  parentType;

  aFrame->GetFrameType(getter_AddRefs(parentType));
  return parentType.get() == nsLayoutAtoms::canvasFrame;
}

static void
PropagateBackgroundToParent(nsIStyleContext*    aStyleContext,
                            const nsStyleColor* aColor,
                            nsIStyleContext*    aParentStyleContext)
{
  nsStyleColor* mutableColor;
  mutableColor = (nsStyleColor*)aParentStyleContext->GetMutableStyleData(eStyleStruct_Color);

  mutableColor->mBackgroundAttachment = aColor->mBackgroundAttachment;
  mutableColor->mBackgroundFlags = aColor->mBackgroundFlags | NS_STYLE_BG_PROPAGATED_FROM_CHILD;
  mutableColor->mBackgroundRepeat = aColor->mBackgroundRepeat;
  mutableColor->mBackgroundColor = aColor->mBackgroundColor;
  mutableColor->mBackgroundXPosition = aColor->mBackgroundXPosition;
  mutableColor->mBackgroundYPosition = aColor->mBackgroundYPosition;
  mutableColor->mBackgroundImage = aColor->mBackgroundImage;

  // Reset the BODY's background to transparent
  mutableColor = (nsStyleColor*)aStyleContext->GetMutableStyleData(eStyleStruct_Color);
  mutableColor->mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT |
                                   NS_STYLE_BG_IMAGE_NONE |
                                   NS_STYLE_BG_PROPAGATED_TO_PARENT;
  mutableColor->mBackgroundImage.SetLength(0);
  mutableColor->mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
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
    aPresShell->CaptureHistoryState(getter_AddRefs(mTempFrameTreeState));

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
  const nsStyleColor* color = 
    (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);

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
                              PR_FALSE,
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

          // Since we always create a block frame, we need to make sure that the 
          // style context's display type is block level.
          nsStyleDisplay* disp = (nsStyleDisplay*)styleContext->GetMutableStyleData(eStyleStruct_Display);
          disp->mDisplay = NS_STYLE_DISPLAY_BLOCK;
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
 
    // Create any anonymous frames the doc element frame requires
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aDocElement, contentFrame,
                          childItems);

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

    // this is not sufficient: if the background gets changed via DOM after
    // frame construction we need to do this again...

    // Section 14.2 of the CSS2 spec says that the background of the root element
    // covers the entire canvas. See if a background was specified for the root
    // element
    if (!color->BackgroundIsTransparent() && IsCanvasFrame(aParentFrame)) {
      nsIStyleContext*  parentContext;
      
      // Propagate the document element's background to the canvas so that it
      // renders the background over the entire canvas
      aParentFrame->GetStyleContext(&parentContext);
      PropagateBackgroundToParent(styleContext, color, parentContext);
      NS_RELEASE(parentContext);
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

  // Set up our style rule observer.
  nsCOMPtr<nsIDocument> doc;
  aDocElement->GetDocument(*getter_AddRefs(doc));
  if (doc) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    if (bindingManager) {
      nsCOMPtr<nsIStyleRuleSupplier> ruleSupplier(do_QueryInterface(bindingManager));
      nsCOMPtr<nsIStyleSet> set;
      aPresShell->GetStyleSet(getter_AddRefs(set));
      set->SetStyleRuleSupplier(ruleSupplier);
    }
  }
  
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
    // - renders the document element's background. This ensures the background covers
    //   the entire canvas as specified by the CSS2 spec

    PRBool isPaginated = PR_FALSE;
    aPresContext->IsPaginated(&isPaginated);
    nsIFrame* rootFrame = nsnull;
    nsIAtom* rootPseudo;
        
    if (!isPaginated) {
        PRInt32 nameSpaceID;
        if (NS_SUCCEEDED(aDocElement->GetNameSpaceID(nameSpaceID)) &&
            nameSpaceID == nsXULAtoms::nameSpaceID) 
        {
          NS_NewRootBoxFrame(aPresShell, &rootFrame);
        } else {
          NS_NewCanvasFrame(aPresShell, &rootFrame);
        }

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

  //isScrollable = PR_FALSE;

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
                                PR_TRUE,
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
      if (HasGfxScrollbars()) {
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
                                             pagePseudoStyle, nsnull, PR_TRUE);

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
nsCSSFrameConstructor::ConstructCheckboxControlFrame(nsIPresShell*    aPresShell, 
                                                     nsIPresContext*  aPresContext,
                                                     nsIFrame*&       aNewFrame,
                                                     nsIContent*      aContent,
                                                     nsIStyleContext* aStyleContext)
{
  nsresult rv = NS_OK;
	if (GetFormElementRenderingMode(aPresContext, eWidgetType_Checkbox) == eWidgetRendering_Gfx)
		rv = NS_NewGfxCheckboxControlFrame(aPresShell, &aNewFrame);
	else
    NS_ASSERTION(0, "We longer support native widgets");


  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
  }

  nsCOMPtr<nsIStyleContext> checkboxStyle;
  aPresContext->ResolvePseudoStyleContextFor(aContent, nsHTMLAtoms::checkPseudo, 
                                             aStyleContext, PR_FALSE, getter_AddRefs(checkboxStyle));
  nsICheckboxControlFrame* checkbox = nsnull;
  if (aNewFrame != nsnull && 
      NS_SUCCEEDED(aNewFrame->QueryInterface(NS_GET_IID(nsICheckboxControlFrame), (void**)&checkbox))) {
    checkbox->SetCheckboxFaceStyleContext(checkboxStyle);
    NS_RELEASE(checkbox);
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

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if (eWidgetRendering_Gfx == mode) 
  {
    rv = NS_NewGfxTextControlFrame(aPresShell, &aNewFrame);
    if (NS_FAILED(rv)) {
      aNewFrame = nsnull;
    }
    if (aNewFrame)
    {
#ifndef ENDER_LITE
      ((nsGfxTextControlFrame*)aNewFrame)->SetFrameConstructor(this);
#endif
    }
  }
  if (!aNewFrame)
  {
    NS_ASSERTION(0, "We longer support native widgets");
  }
  return rv;
}

PRBool
nsCSSFrameConstructor::HasGfxScrollbars()
{
  // Get the Prefs
  if (!mGotGfxPrefs) {
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_PROGID));
    if (pref) {
      pref->GetBoolPref("nglayout.widget.gfxscrollbars", &mHasGfxScrollbars);
      pref->GetBoolPref("nglayout.widget.gfxlistbox", &mDoGfxListbox);
      pref->GetBoolPref("nglayout.widget.gfxcombobox", &mDoGfxCombobox);
      mGotGfxPrefs = PR_TRUE;
    } else {
      mHasGfxScrollbars = PR_FALSE;
      mDoGfxListbox     = PR_FALSE;
      mDoGfxCombobox    = PR_FALSE;
    }
  }

  //return mHasGfxScrollbars;
  // we no longer support native scrollbars. Except in form elements. So 
  // we always return true
  return PR_TRUE;
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
                                                 aStyleContext, aParentFrame, PR_FALSE);

        if (HasGfxScrollbars() && mDoGfxCombobox) {
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
                                           gfxListStyle, nsnull, PR_TRUE);
            nsIView * view;
            listFrame->GetView(aPresContext, &view);
            if (view != nsnull) {
              nsWidgetInitData widgetData;
              view->SetFloating(PR_TRUE);
              widgetData.mWindowType  = eWindowType_popup;
              widgetData.mBorderStyle = eBorderStyle_default;
    
#ifdef XP_MAC
              static NS_DEFINE_IID(kCPopUpCID,  NS_POPUP_CID);
              view->CreateWidget(kCPopUpCID, &widgetData, nsnull);
#else
              static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
              view->CreateWidget(kCChildCID, &widgetData, nsnull);
#endif   
            }

            nsCOMPtr<nsIStyleContext> gfxScrolledStyle;
            aPresContext->ResolvePseudoStyleContextFor(aContent, 
                                              nsLayoutAtoms::selectScrolledContentPseudo,
                                              gfxListStyle, PR_FALSE, 
                                              getter_AddRefs(gfxScrolledStyle)); 

            
            // create the area frame we are scrolling 
            flags = NS_BLOCK_SHRINK_WRAP | (aIsAbsolutelyPositioned?NS_BLOCK_SPACE_MGR:0);
            nsIFrame* scrolledFrame = nsnull;
            NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

            nsIFrame* newScrollFrame = nsnull; 
            

            /* scroll frame */
#ifdef NEWGFX_LIST_SCROLLFRAME
            nsIStyleContext* newStyle = nsnull;

            // ok take the style context, the Select area frame to scroll,the listFrame, and its parent 
            // and build the scrollframe. 
            BuildScrollFrame(aPresShell, aPresContext, aState, aContent, gfxScrolledStyle, scrolledFrame, 
                             listFrame, newScrollFrame, newStyle); 

            gfxScrolledStyle = newStyle;

#else
            newScrollFrame = scrolledFrame;
#endif

            /*
            // resolve a style for our gfx scrollframe based on the list frames style 
            
           InitAndRestoreFrame(aPresContext, aState, aContent, 
                                parentFrame, gfxScrolledStyle, nsnull, scrolledFrame);
           */

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

            nsFrameItems anonChildItems; 
              // Create display and button frames from the combobox'es anonymous content
            CreateAnonymousFrames(aPresShell, aPresContext, nsHTMLAtoms::combobox, aState, aContent, comboboxFrame,
                                  anonChildItems);
        
            comboboxFrame->SetInitialChildList(aPresContext, nsnull,
                                   anonChildItems.childList);


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
        } else {
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
        }
      } else if (HasGfxScrollbars() && mDoGfxListbox) {
        
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
      } else {
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
                                           scrolledPseudoStyle, nsnull, PR_TRUE);


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
  /*
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
*/
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
                                           aStyleContext, aParentFrame, PR_FALSE);

    // cache our display type
  const nsStyleDisplay* styleDisplay;
  newFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);

  nsIFrame * areaFrame;
  //NS_NewBlockFrame(shell, &areaFrame, flags);
  NS_NewAreaFrame(shell, &areaFrame, flags | NS_BLOCK_SHRINK_WRAP);// | NS_BLOCK_MARGIN_ROOT);

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
      previous = child;
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
}

nsresult
nsCSSFrameConstructor::ConstructFrameByTag(nsIPresShell*            aPresShell, 
                                           nsIPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsIAtom*                 aTag,
                                           PRInt32                  aNameSpaceID,
                                           nsIStyleContext*         aStyleContext,
                                           nsFrameItems&            aFrameItems)
{
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isFloating = PR_FALSE;
  PRBool    isRelativePositioned = PR_FALSE;
  PRBool    canBePositioned = PR_TRUE;
  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    isReplaced = PR_FALSE;
  PRBool    addToHashTable = PR_TRUE;
  PRBool    isFloaterContainer = PR_FALSE;
  PRBool    isPositionedContainingBlock = PR_FALSE;
  nsresult  rv = NS_OK;

  if (nsLayoutAtoms::textTagName == aTag) {
    // process pending pseudo frames. whitespace doesn't have an effect.
    if (!aState.mPseudoFrames.IsEmpty() && !IsOnlyWhiteSpace(aContent)) { 
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
    }
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
      else {
        if (NS_STYLE_FLOAT_NONE != display->mFloats) {
          isFloating = PR_TRUE;
        }
        if (NS_STYLE_POSITION_RELATIVE == position->mPosition) {
          isRelativePositioned = PR_TRUE;
        }
      }

      // Create a frame based on the tag
      if (nsHTMLAtoms::img == aTag) {
        isReplaced = PR_TRUE;
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        // XXX If image display is turned off, then use ConstructAlternateFrame()
        // instead...
        rv = NS_NewImageFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::hr == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewHRFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::br == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewBRFrame(aPresShell, &newFrame);
        isReplaced = PR_TRUE;
        // BR frames don't go in the content->frame hash table: typically
        // there are many BR content objects and this would increase the size
        // of the hash table, and it's doubtful we need the mapping anyway
        addToHashTable = PR_FALSE;
      }
      else if (nsHTMLAtoms::wbr == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewWBRFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::input == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = CreateInputFrame(aPresShell, aPresContext, aContent, newFrame, aStyleContext);
      }
      else if (nsHTMLAtoms::textarea == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = ConstructTextControlFrame(aPresShell, aPresContext, newFrame, aContent);
      }
      else if (nsHTMLAtoms::select == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = ConstructSelectFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                  aTag, aStyleContext, newFrame,  processChildren,
                                  isAbsolutelyPositioned, frameHasBeenInitialized,
                                  isFixedPositioned, aFrameItems);
      }
      else if (nsHTMLAtoms::object == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::applet == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::embed == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewObjectFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::fieldset == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
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
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewLegendFrame(aPresShell, &newFrame);
        processChildren = PR_TRUE;
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::form == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        PRBool  isOutOfFlow = isFloating || isAbsolutelyPositioned || isFixedPositioned;

        rv = NS_NewFormFrame(aPresShell, &newFrame,
                             isOutOfFlow ? NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT : 0);
        processChildren = PR_TRUE;

        // A form frame is a block frame therefore it can contain floaters
        isFloaterContainer = PR_TRUE;

        // See if it's a containing block for absolutely positioned elements
        isPositionedContainingBlock = isAbsolutelyPositioned || isFixedPositioned || isRelativePositioned;
      }
      else if (nsHTMLAtoms::frameset == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewHTMLFramesetFrame(aPresShell, &newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::iframe == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        isReplaced = PR_TRUE;
        rv = NS_NewHTMLFrameOuterFrame(aPresShell, &newFrame);
      }
      else if (nsHTMLAtoms::spacer == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewSpacerFrame(aPresShell, &newFrame);
        canBePositioned = PR_FALSE;
      }
      else if (nsHTMLAtoms::button == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewHTMLButtonControlFrame(aPresShell, &newFrame);
        // the html4 button needs to act just like a 
        // regular button except contain html content
        // so it must be replaced or html outside it will
        // draw into its borders. -EDV
        isReplaced = PR_TRUE;
        processChildren = PR_TRUE;
      }
      else if (nsHTMLAtoms::label == aTag) {
        if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
          ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
        }
        rv = NS_NewLabelFrame(aPresShell, &newFrame, isAbsolutelyPositioned ? NS_BLOCK_SPACE_MGR : 0);
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
                                               aStyleContext, aParentFrame, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        if (isPositionedContainingBlock) {
          // The area frame becomes a container for child frames that are
          // absolutely positioned
          nsFrameConstructorSaveState absoluteSaveState;
          aState.PushAbsoluteContainingBlock(newFrame, absoluteSaveState);
          
          // Process the child frames
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                               PR_TRUE, childItems, PR_FALSE);
          
          // Set the frame's absolute list if there were any absolutely positioned children
          if (aState.mAbsoluteItems.childList) {
            newFrame->SetInitialChildList(aPresContext,
                                          nsLayoutAtoms::absoluteList,
                                          aState.mAbsoluteItems.childList);
          }
        }
        else if (isFloaterContainer) {
          // If the frame can contain floaters, then push a floater
          // containing block
          PRBool haveFirstLetterStyle, haveFirstLineStyle;
          HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                                &haveFirstLetterStyle, &haveFirstLineStyle);
          nsFrameConstructorSaveState floaterSaveState;
          aState.PushFloaterContainingBlock(newFrame, floaterSaveState,
                                            PR_FALSE, PR_FALSE);
          
          // Process the child frames
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                               PR_TRUE, childItems, PR_FALSE);
          
          // Set the frame's floater list if there were any floated children
          if (aState.mFloatedItems.childList) {
            newFrame->SetInitialChildList(aPresContext,
                                          nsLayoutAtoms::floaterList,
                                          aState.mFloatedItems.childList);
          }

        } else {
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                               PR_TRUE, childItems, PR_FALSE);
        }
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
static void LocateAnonymousFrame(nsIPresContext* aPresContext,
                                 nsIFrame*       aParentFrame,
                                 nsIContent*     aTargetContent,
                                 nsIFrame**      aResult)
{
  // Check ourselves.
  *aResult = nsnull;
  nsCOMPtr<nsIContent> content;
  aParentFrame->GetContent(getter_AddRefs(content));
  if (content.get() == aTargetContent) {
    // We must take into account if the parent is a scrollframe. If it is, we
    // need to bypass the scrolling mechanics and get at the true frame.
    nsCOMPtr<nsIScrollableFrame> scrollFrame ( do_QueryInterface(aParentFrame) );
    if ( scrollFrame )
      scrollFrame->GetScrolledFrame ( aPresContext, *aResult );
    else
      *aResult = aParentFrame;
    return;
  }

  // Check our kids.
  nsIFrame* currFrame;
  aParentFrame->FirstChild(aPresContext, nsnull, &currFrame);
  while (currFrame) {
    LocateAnonymousFrame(aPresContext, currFrame, aTargetContent, aResult);
    if (*aResult)
      return;
    currFrame->GetNextSibling(&currFrame);
  }

  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(aParentFrame));
  if (menuFrame) {
    nsIFrame* popupChild;
    menuFrame->GetMenuChild(&popupChild);
    if (popupChild) {
      LocateAnonymousFrame(aPresContext, popupChild, aTargetContent, aResult);
      if (*aResult)
        return;
    }
  }
}

nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsIPresShell*        aPresShell, 
                                             nsIPresContext*          aPresContext,
                                             nsIAtom*                 aTag,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIFrame*                aNewFrame,
                                             nsFrameItems&            aChildItems)
{
  nsCOMPtr<nsIStyleContext> styleContext;
  aNewFrame->GetStyleContext(getter_AddRefs(styleContext));

  const nsStyleUserInterface* ui= (const nsStyleUserInterface*)
      styleContext->GetStyleData(eStyleStruct_UserInterface);

  if (!ui->mBehavior.IsEmpty()) {
    // Get the XBL loader.
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
    if (!xblService)
      return rv;

    // Load the bindings.
    nsCOMPtr<nsIXBLBinding> binding;
    rv = xblService->LoadBindings(aParent, ui->mBehavior, PR_FALSE, getter_AddRefs(binding));
    if (NS_FAILED(rv))
      return NS_OK;

    // Retrieve the anonymous content that we should build.
    nsCOMPtr<nsISupportsArray> anonymousItems;
    nsCOMPtr<nsIContent> childElement;
    PRBool multiple;
    xblService->GetContentList(aParent, getter_AddRefs(anonymousItems), getter_AddRefs(childElement), &multiple);
    
    if (anonymousItems)
    {
      // See if we have to move our explicit content.
      nsFrameItems explicitItems;
      if (childElement || multiple) {
        // First, remove all of the kids from the frame list and put them
        // in a new frame list.
        explicitItems.childList = aChildItems.childList;
        explicitItems.lastChild = aChildItems.lastChild;
        aChildItems.childList = aChildItems.lastChild = nsnull;
      }

      // Build the frames for the anonymous content.
      PRUint32 count = 0;
      anonymousItems->Count(&count);

      for (PRUint32 i=0; i < count; i++)
      {
        // get our child's content and set its parent to our content
        nsCOMPtr<nsIContent> content;
        if (NS_FAILED(anonymousItems->QueryElementAt(i, NS_GET_IID(nsIContent), getter_AddRefs(content))))
            continue;

        // create the frame and attach it to our frame
        ConstructFrame(aPresShell, aPresContext, aState, content, aNewFrame, aChildItems);
      }

      if (childElement) {
        // Now append the explicit frames 
        // All our explicit content that we built must be reparented.
        nsIFrame* frame = nsnull;
        nsIFrame* currFrame = aChildItems.childList;
        while (currFrame) {
          LocateAnonymousFrame(aPresContext,
                               currFrame,
                               childElement,
                               &frame);
          if (frame)
            break;
          currFrame->GetNextSibling(&currFrame);
        }

        nsCOMPtr<nsIFrameManager> frameManager;
        aPresShell->GetFrameManager(getter_AddRefs(frameManager));
      
        if (frameManager && frame && explicitItems.childList) {
          frameManager->AppendFrames(aPresContext, *aPresShell, frame,
                                     nsnull, explicitItems.childList);
          nsCOMPtr<nsIStyleContext> styleContext;
          frame->GetStyleContext(getter_AddRefs(styleContext));
          nsIFrame* walkit = explicitItems.childList;
          while (walkit) {
            aPresContext->ReParentStyleContext(walkit, styleContext);
            walkit->GetNextSibling(&walkit);
          }
        }
      }
      else if (multiple) {
        nsCOMPtr<nsIDocument> document;
        nsCOMPtr<nsIBindingManager> bindingManager;
        aParent->GetDocument(*getter_AddRefs(document));
        document->GetBindingManager(getter_AddRefs(bindingManager));
        nsCOMPtr<nsIContent> currContent;
        nsCOMPtr<nsIContent> insertionElement;
        nsIFrame* currFrame = explicitItems.childList;
        explicitItems.childList = explicitItems.lastChild = nsnull;
        nsCOMPtr<nsIFrameManager> frameManager;
        aPresShell->GetFrameManager(getter_AddRefs(frameManager));
          
        while (currFrame) {
          nsIFrame* nextFrame;
          currFrame->GetNextSibling(&nextFrame);
          currFrame->SetNextSibling(nsnull);
          
          currFrame->GetContent(getter_AddRefs(currContent));
          bindingManager->GetInsertionPoint(aParent, currContent, getter_AddRefs(insertionElement));
          
          nsIFrame* frame = nsnull;
          if (insertionElement) {
            nsIFrame* childFrame = aChildItems.childList;
            while (childFrame) {
              LocateAnonymousFrame(aPresContext,
                                   childFrame,
                                   insertionElement,
                                   &frame);
              if (frame)
                break;
              childFrame->GetNextSibling(&childFrame);
            }
          }

          if (!frame) {
            if (!explicitItems.childList)
              explicitItems.childList = explicitItems.lastChild = currFrame;
            else {
              explicitItems.lastChild->SetNextSibling(currFrame);
              explicitItems.lastChild = currFrame;
            }
          }

          if (frameManager && frame) {
            frameManager->AppendFrames(aPresContext, *aPresShell, frame,
                                       nsnull, currFrame);
            frame->GetStyleContext(getter_AddRefs(styleContext));
            aPresContext->ReParentStyleContext(currFrame, styleContext);
          }
       
          currFrame = nextFrame;
        }
        if (explicitItems.lastChild) {
          explicitItems.lastChild->SetNextSibling(aChildItems.childList);
          aChildItems.childList = explicitItems.childList;
        }
      }

      return NS_OK;
    }
  }
  // If we have no anonymous content from XBL see if we might have
  // some by looking at the tag rather than doing a QueryInterface on
  // the frame.  Only these tags' frames can have anonymous content
  // through nsIAnonymousContentCreator.  We do this check for
  // performance reasons. If we did a QueryInterface on every tag it
  // would be inefficient.

  // nsGenericElement::SetDocument ought to keep a list like this one,
  // but it can't because nsGfxScrollFrames get around this.

  if (aTag !=  nsHTMLAtoms::input &&
      aTag !=  nsHTMLAtoms::textarea &&
      aTag !=  nsHTMLAtoms::combobox &&
      aTag !=  nsXULAtoms::splitter &&
      aTag !=  nsXULAtoms::scrollbar
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
                                             nsIFrame*                aParentFrame,
                                             nsFrameItems&            aChildItems)
{

  nsCOMPtr<nsIAnonymousContentCreator> creator(do_QueryInterface(aParentFrame));
  
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
    nsCOMPtr<nsIContent> content;
    if (NS_FAILED(anonymousItems->QueryElementAt(i, NS_GET_IID(nsIContent), getter_AddRefs(content))))
        continue;

    content->SetParent(aParent);
    content->SetDocument(aDocument, PR_TRUE, PR_TRUE);
    content->SetBindingParent(content);
    
    nsIFrame * newFrame = nsnull;
    nsresult rv = creator->CreateFrameFor(aPresContext, content, &newFrame);
    if (NS_SUCCEEDED(rv) && newFrame != nsnull) {
      aChildItems.AddChild(newFrame);
    } else {
      // create the frame and attach it to our frame
      ConstructFrame(aPresShell, aPresContext, aState, content, aParentFrame, aChildItems);
    }
  }

  return NS_OK;
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousTableCellFrames(nsIPresShell*        aPresShell, 
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

  if (!ui->mBehavior.IsEmpty()) {
    // Get the XBL loader.
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
    if (!xblService)
      return rv;

    // Load the bindings.
    nsCOMPtr<nsIXBLBinding> binding;
    rv = xblService->LoadBindings(aParent, ui->mBehavior, PR_FALSE, getter_AddRefs(binding));
    if (NS_FAILED(rv))
      return NS_OK;

    // Retrieve the anonymous content that we should build.
    nsCOMPtr<nsIContent> childElement;
    nsCOMPtr<nsISupportsArray> anonymousItems;
    PRBool multiple;
    xblService->GetContentList(aParent, getter_AddRefs(anonymousItems), getter_AddRefs(childElement), &multiple);
    
    if (!anonymousItems)
      return NS_OK;

    // Build the frames for the anonymous content.
    PRUint32 count = 0;
    anonymousItems->Count(&count);

    for (PRUint32 i=0; i < count; i++)
    {
      // get our child's content and set its parent to our content
      nsCOMPtr<nsIContent> content;
      if (NS_FAILED(anonymousItems->QueryElementAt(i, NS_GET_IID(nsIContent), getter_AddRefs(content))))
        continue;

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
                                         PRInt32                  aNameSpaceID,
                                         nsIStyleContext*         aStyleContext,
                                         nsFrameItems&            aFrameItems,
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

  NS_ASSERTION(aTag != nsnull, "null XUL tag");
  if (aTag == nsnull)
    return NS_OK;

  
  if (aNameSpaceID == nsXULAtoms::nameSpaceID) {
  
// was here

    // See if the element is absolutely positioned
    const nsStylePosition* position = (const nsStylePosition*)
      aStyleContext->GetStyleData(eStyleStruct_Position);
    if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition)
      isAbsolutelyPositioned = PR_TRUE;

    // Create a frame based on the tag
    // box is first because it is created the most.
      // BOX CONSTRUCTION
    if (aTag == nsXULAtoms::box || aTag == nsXULAtoms::vbox || aTag == nsXULAtoms::hbox || aTag == nsXULAtoms::tabbox || 
        aTag == nsXULAtoms::tabpage || aTag == nsXULAtoms::tabcontrol
        || aTag == nsXULAtoms::treecell  
        ) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;

      if (aTag == nsXULAtoms::treecell)
        rv = NS_NewXULTreeCellFrame(aPresShell, &newFrame);
      else

      // create a box. Its not root, its layout manager is default (nsnull) which is "sprocket" and
      // its default orientation is horizontal for hbox and vertical for vbox
      rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, nsnull, aTag != nsXULAtoms::vbox);

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
    else if (aTag == nsXULAtoms::button || aTag == nsXULAtoms::checkbox || aTag == nsXULAtoms::radio) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewButtonBoxFrame(aPresShell, &newFrame);

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
 // BUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::autorepeatbutton) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewAutoRepeatBoxFrame(aPresShell, &newFrame);

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


	 // TITLEBAR CONSTRUCTION
	 else if (aTag == nsXULAtoms::titlebar) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTitleBarFrame(aPresShell, &newFrame);

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
    } // End of TITLEBAR CONSTRUCTION logic

	 // RESIZER CONSTRUCTION
	 else if (aTag == nsXULAtoms::resizer) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewResizerFrame(aPresShell, &newFrame);

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
    } // End of RESIZER CONSTRUCTION logic





    // TITLED BUTTON CONSTRUCTION
    else if (aTag == nsXULAtoms::titledbutton) {

      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewTitledButtonFrame(aPresShell, &newFrame);
    }
    // End of TITLED BUTTON CONSTRUCTION logic
    else if (aTag == nsXULAtoms::image) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewImageBoxFrame(aPresShell, &newFrame);
    }
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
      rv = NS_NewTextBoxFrame(aPresShell, &newFrame);
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
      ((nsMenuFrame*) newFrame)->SetFrameConstructor(this);
    }
    else if (aTag == nsXULAtoms::menubar) {
#if (defined(XP_MAC) && !TARGET_CARBON) || defined(RHAPSODY) // The Mac uses its native menu bar.
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
      ((nsPopupSetFrame*) newFrame)->SetFrameConstructor(this);
    }
    else if (aTag == nsXULAtoms::menupopup || aTag == nsXULAtoms::popup) {
      // This is its own frame that derives from
      // box.
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewMenuPopupFrame(aPresShell, &newFrame);
    } 

    // ------- Begin Grid ---------
    else if (aTag == nsXULAtoms::grid || aTag == nsXULAtoms::tree) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      nsCOMPtr<nsIBoxLayout> layout;
      NS_NewGridLayout(aPresShell, layout);

      if (aTag == nsXULAtoms::tree)
        rv = NS_NewXULTreeFrame(aPresShell, &newFrame, PR_FALSE, layout);
      else
        rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, layout);

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
    } //------- End Grid ------

    // ------- Begin Rows/Columns ---------
    else if (aTag == nsXULAtoms::rows || aTag == nsXULAtoms::columns
             || aTag == nsXULAtoms::treechildren || aTag == nsXULAtoms::treecolgroup ||
             aTag == nsXULAtoms::treehead || aTag == nsXULAtoms::treerows || 
             aTag == nsXULAtoms::treecols || aTag == nsXULAtoms::treeitem
      ) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      PRBool isHorizontal = (aTag == nsXULAtoms::columns)
        || (aTag == nsXULAtoms::treecolgroup) || (aTag == nsXULAtoms::treecols) 
        ;
      nsCOMPtr<nsIBoxLayout> layout;
      
      if (aTag == nsXULAtoms::treechildren || aTag == nsXULAtoms::treeitem) {
        NS_NewTreeLayout(aPresShell, layout);
        nsCOMPtr<nsIContent> parentContent;
        aParentFrame->GetContent(getter_AddRefs(parentContent));
        nsCOMPtr<nsIAtom> parentTag;
        parentContent->GetTag(*getter_AddRefs(parentTag));
        if (parentTag.get() == nsXULAtoms::tree) {
          rv = NS_NewXULTreeOuterGroupFrame(aPresShell, &newFrame, PR_FALSE, layout,  PR_FALSE);
          ((nsXULTreeGroupFrame*)newFrame)->InitGroup(this, aPresContext, (nsXULTreeOuterGroupFrame*) newFrame);
        }
        else {
          rv = NS_NewXULTreeGroupFrame(aPresShell, &newFrame, PR_FALSE, layout,  PR_FALSE);
          ((nsXULTreeGroupFrame*)newFrame)->InitGroup(this, aPresContext, ((nsXULTreeGroupFrame*)aParentFrame)->GetOuterFrame());
        }

        processChildren = PR_FALSE;
      }
      else
      {
        NS_NewTempleLayout(aPresShell, layout);
        rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, layout,  isHorizontal);
      }

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
    } //------- End Grid ------

    // ------- Begin Row/Column ---------
    else if (aTag == nsXULAtoms::row || aTag == nsXULAtoms::column
             || aTag == nsXULAtoms::treerow || aTag == nsXULAtoms::treecol
      ) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      PRBool isHorizontal = (aTag == nsXULAtoms::row) || (aTag == nsXULAtoms::treerow);
      nsCOMPtr<nsIBoxLayout> layout;
      NS_NewObeliskLayout(aPresShell, layout);

      if (aTag == nsXULAtoms::treerow)
        rv = NS_NewXULTreeSliceFrame(aPresShell, &newFrame, PR_FALSE, layout, isHorizontal);
      else
        rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, layout, isHorizontal);

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
    } //------- End Grid ------

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

          rv = NS_NewTitledBoxFrame(aPresShell, &newFrame);
 
          //ConstructTitledBoxFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aTag, aStyleContext, newFrame);
          processChildren = PR_TRUE;
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
    else if (aTag == nsXULAtoms::scrollbox) {
          rv = NS_NewScrollBoxFrame(aPresShell, &newFrame);
 
          //ConstructTitledBoxFrame(aPresShell, aPresContext, aState, aContent, aParentFrame, aTag, aStyleContext, newFrame);
          processChildren = PR_TRUE;
          isReplaced = PR_TRUE;
    } 
    
    else if (aTag == nsXULAtoms::spinner)
      rv = NS_NewSpinnerFrame(aPresShell, &newFrame);
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
    else if (aTag == nsXULAtoms::treeindentation)
    {
      rv = NS_NewTreeIndentationFrame(aPresShell, &newFrame);
    }
    // End of TREE CONSTRUCTION code here (there's more later on in the function)

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

    // STACK CONSTRUCTION
    else if (aTag == nsXULAtoms::stack) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;
      rv = NS_NewStackFrame(aPresShell, &newFrame);
    }
    // End of STACK CONSTRUCTION logic

    // BULLETINBOARD CONSTRUCTION
    else if (aTag == nsXULAtoms::bulletinboard) {
      processChildren = PR_TRUE;
      isReplaced = PR_TRUE;


      nsCOMPtr<nsIBoxLayout> layout;
      NS_NewBulletinBoardLayout(aPresShell, layout);

      rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, layout);

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
#if 1
    else if (aTag != nsHTMLAtoms::html) {
      nsCAutoString str("Invalid XUL tag encountered in file. Perhaps you used the wrong namespace?\n\nThe tag name is ");
      nsAutoString tagName;
      aTag->ToString(tagName);
      str.AppendWithConversion(tagName);
      NS_WARNING(str);
    }
#endif
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

      
      /*
      // if our parent is a block frame then do things the way html likes it
      // if not then we are in a box so do what boxes like. On example is boxes
      // do not support the absolute positioning of their children. While html blocks
      // thats why we call different things here.
      nsCOMPtr<nsIAtom> frameType;
      geometricParent->GetFrameType(getter_AddRefs(frameType));
      if ((frameType.get() == nsLayoutAtoms::blockFrame) ||
          (frameType.get() == nsLayoutAtoms::areaFrame)) {
      */
        // See if we need to create a view, e.g. the frame is absolutely positioned
        nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                                 aStyleContext, aParentFrame, PR_FALSE);

      /*
      } else {
          // we are in a box so do the box thing.
        nsBoxFrame::CreateViewForFrame(aPresContext, newFrame,
                                                 aStyleContext, PR_FALSE);
      }
      */
      
    }

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                             PR_FALSE, childItems, PR_FALSE);
      
      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
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

// addToHashTable:

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

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);



nsresult
nsCSSFrameConstructor::BeginBuildingScrollFrame(nsIPresShell* aPresShell, 
                                                nsIPresContext*         aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIStyleContext*         aContentStyle,
                                               nsIFrame*                aParentFrame,
                                               nsIAtom*                 aScrolledPseudo,
                                               nsIDocument*             aDocument,
                                               PRBool                   aIsRoot,
                                               nsIFrame*&               aNewFrame,                                                                                             
                                               nsCOMPtr<nsIStyleContext>& aScrolledChildStyle,
                                               nsIFrame*&               aScrollableFrame)
{
  nsIFrame* scrollFrame = nsnull;
  nsIFrame* parentFrame = nsnull;
  nsIFrame* gfxScrollFrame = nsnull;

  nsFrameItems anonymousItems;

  nsCOMPtr<nsIStyleContext> contentStyle = dont_QueryInterface(aContentStyle);

  PRBool isGfx = HasGfxScrollbars();

  if (isGfx) {
  
    BuildGfxScrollFrame(aPresShell, aPresContext, aState, aContent, aDocument, aParentFrame,
                        contentStyle, aIsRoot, gfxScrollFrame, anonymousItems);

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
                                           aScrolledContentStyle, nsnull, PR_TRUE);

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
                     PR_FALSE,
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
                                             PRBool                   aIsRoot,
                                             nsIFrame*&               aNewFrame,
                                             nsFrameItems&            aAnonymousFrames)
{
  NS_NewGfxScrollFrame(aPresShell, &aNewFrame, aDocument, aIsRoot);

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  // Create a view
  nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame,
                                             aStyleContext, nsnull, PR_FALSE);

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
  PRBool    pseudoParent = PR_FALSE; // is the new frame's parent anonymous
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

    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
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

    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, newFrame,
                            childItems);

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

    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
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
                                             aStyleContext, aParentFrame, PR_FALSE);

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

    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, newFrame,
                          childItems);

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
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Create an area frame
    NS_NewFloatingItemWrapperFrame(aPresShell, &newFrame);

    // Initialize the frame
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aState.mFloatedItems.containingBlock, 
                        aStyleContext, nsnull, newFrame);

    // See if we need to create a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                             aStyleContext, aParentFrame, PR_FALSE);

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

    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, newFrame,
                          childItems);

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
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Is it block-level or inline-level?
    PRBool isBlockFrame = PR_FALSE;
    if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
      // Create a wrapper frame. No space manager, though
      NS_NewRelativeItemWrapperFrame(aPresShell, &newFrame);
      isBlockFrame = PR_TRUE;

      // Initialize the frame    
      InitAndRestoreFrame(aPresContext, aState, aContent, 
                          aParentFrame, aStyleContext, nsnull, newFrame);

      // Create a view
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               aStyleContext, nsnull, PR_FALSE);

      // Process the child content. Relatively positioned frames becomes a
      // container for child frames that are positioned
      nsFrameConstructorSaveState absoluteSaveState;
      nsFrameConstructorSaveState floaterSaveState;
      nsFrameItems                childItems;

      aState.PushAbsoluteContainingBlock(newFrame, absoluteSaveState);
    
      PRBool haveFirstLetterStyle, haveFirstLineStyle;
      HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
      aState.PushFloaterContainingBlock(newFrame, floaterSaveState,
                                        haveFirstLetterStyle,
                                        haveFirstLineStyle);

      ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                      childItems, isBlockFrame);

      nsCOMPtr<nsIAtom> tag;
      aContent->GetTag(*getter_AddRefs(tag));
      CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, newFrame,
                            childItems);

      // Set the frame's initial child list
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
    } else {
      // Create a positioned inline frame
      NS_NewPositionedInlineFrame(aPresShell, &newFrame);
      ConstructInline(aPresShell, aPresContext, aState, aDisplay, aContent,
                      aParentFrame, aStyleContext, PR_TRUE, newFrame,
                      &newBlock, &nextInline);
    }
  }
  // See if it's a block frame of some sort
  else if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_RUN_IN == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_COMPACT == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay->mDisplay)) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
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
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Create the inline frame
    rv = NS_NewInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      // That worked so construct the inline and its children
      rv = ConstructInline(aPresShell, aPresContext, aState, aDisplay, aContent,
                           aParentFrame, aStyleContext, PR_FALSE, newFrame,
                           &newBlock, &nextInline);
    }

    // To keep the hash table small don't add inline frames (they're
    // typically things like FONT and B), because we can quickly
    // find them if we need to
    addToHashTable = PR_FALSE;
  }
  // otherwise let the display property influence the frame type to create
  else {
    // XXX This section now only handles table frames; should be
    // factored out probably

    // Use the 'display' property to choose a frame type
    switch (aDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_TABLE:
    {
      if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
        ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
      }
      nsIFrame* geometricParent = aParentFrame;
      if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
        geometricParent = aState.mAbsoluteItems.containingBlock;
      } else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
        isFixedPositioned = PR_TRUE;
        geometricParent = aState.mFixedItems.containingBlock;
      } else if (isFloating) {
        geometricParent = aState.mFloatedItems.containingBlock;
      }
      nsIFrame* innerTable;
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, 
                               geometricParent, aStyleContext, tableCreator, 
                               PR_FALSE, aFrameItems, newFrame, innerTable, pseudoParent);
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
      rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, aContent, 
                                      parentFrame, aStyleContext, tableCreator, 
                                      aFrameItems, newFrame, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
    }

    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, aContent, 
                                       aParentFrame, aStyleContext, tableCreator, 
                                       PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;

    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, aContent, 
                                       aParentFrame, aStyleContext, tableCreator, 
                                       PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
   
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      rv = ConstructTableColFrame(aPresShell, aPresContext, aState, aContent, 
                                  aParentFrame, aStyleContext, tableCreator, 
                                  PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, 
                                  aParentFrame, aStyleContext, tableCreator, 
                                  PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      {
        nsIFrame* innerTable;
        rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, 
                                     aParentFrame, aStyleContext, tableCreator, 
                                     PR_FALSE, aFrameItems, newFrame, innerTable, pseudoParent);
      if (!pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
        return rv;
      }
  
    default:
      // Don't create any frame for content that's not displayed...
      if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
        ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
      }
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
  } else if ((nsnull != newFrame) && !pseudoParent) {
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
                                            PRInt32                  aNameSpaceID,
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
  if (aNameSpaceID != nsMathMLAtoms::nameSpaceID) 
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
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
  else if (aTag == nsMathMLAtoms::mtext_ ||
           aTag == nsMathMLAtoms::mn_)
     rv = NS_NewMathMLmtextFrame(aPresShell, &newFrame);
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
      nsIFrame* innerTable;
      PRBool pseudoParent;
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, 
                               geometricParent, aStyleContext, mathTableCreator,
                               PR_FALSE, aFrameItems, newFrame, innerTable, pseudoParent);
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
      } else if (!pseudoParent) {
        // Add the table frame to the flow
        aFrameItems.AddChild(newFrame);
      }
      return rv; 
  }
  else if (aTag == nsMathMLAtoms::mtd_) {
    nsIFrame* innerCell;
    PRBool pseudoParent;
    rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, 
                                 aParentFrame, aStyleContext, mathTableCreator,
                                 PR_FALSE, aFrameItems, newFrame, innerCell, pseudoParent);
    if (!pseudoParent) {
      aFrameItems.AddChild(newFrame);
    }
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
                                             aStyleContext, aParentFrame, PR_FALSE);

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(newFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren) {
      rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                           childItems, PR_FALSE);

      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                            childItems);
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

// SVG 
#ifdef MOZ_SVG
nsresult
nsCSSFrameConstructor::ConstructSVGFrame(nsIPresShell*            aPresShell,
                                          nsIPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsIAtom*                 aTag,
                                          PRInt32                  aNameSpaceID,
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

  NS_ASSERTION(aTag != nsnull, "null SVG tag");
  if (aTag == nsnull)
    return NS_OK;

  // Make sure that we remain confined in the SVG world
  if (aNameSpaceID != nsSVGAtoms::nameSpaceID) 
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
  nsIFrame* ignore = nsnull;
  //nsSVGTableCreator svgTableCreator(aPresShell); // Used to make table views.
 
  // See if the element is absolute or fixed positioned
  const nsStylePosition* position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  }
  else if (NS_STYLE_POSITION_FIXED == position->mPosition) {
    isFixedPositioned = PR_TRUE;
  }
  if (aTag == nsSVGAtoms::g)
     rv = NS_NewSVGContainerFrame(aPresShell, &newFrame);
  else if (aTag == nsSVGAtoms::polygon)
     rv = NS_NewPolygonFrame(aPresShell, &newFrame);
  else if (aTag == nsSVGAtoms::polyline)
     rv = NS_NewPolylineFrame(aPresShell, &newFrame);

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
                                             aStyleContext, aParentFrame, PR_FALSE);

    // Add the new frame to our list of frame items.
    aFrameItems.AddChild(newFrame);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren) {
      rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                           childItems, PR_FALSE);

      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                            childItems);
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
#endif // MOZ_SVG

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
    else
    {
      PRInt32 nameSpaceID;
      aContent->GetNameSpaceID(nameSpaceID);
      rv = ConstructFrameInternal(aPresShell,
                                    aPresContext,
                                    aState,
                                    aContent,
                                    aParentFrame,
                                    tag,
                                    nameSpaceID,
                                    styleContext,
                                    aFrameItems,
                                    PR_FALSE);
    }
  }
  
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructFrameInternal( nsIPresShell*            aPresShell, 
                                               nsIPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsIAtom*                 aTag,
                                               PRInt32                  aNameSpaceID,
                                               nsIStyleContext*         aStyleContext,
                                               nsFrameItems&            aFrameItems,
                                               PRBool                   aXBLBaseTag)
{
  // The following code allows the user to specify the base tag
  // of a XUL object using XBL.  XUL objects (like boxes, menus, etc.)
  // can then be extended arbitrarily.
  nsCOMPtr<nsIXBLBinding> binding;
  if (!aXBLBaseTag)
  {
    const nsStyleUserInterface* ui= (const nsStyleUserInterface*)
        aStyleContext->GetStyleData(eStyleStruct_UserInterface);

    // Ensure that our XBL bindings are installed.
    if (!ui->mBehavior.IsEmpty()) {
      // Get the XBL loader.
      nsresult rv;
      NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
      if (!xblService)
        return rv;

      // Load the bindings.
      rv = xblService->LoadBindings(aContent, ui->mBehavior, PR_FALSE, getter_AddRefs(binding));
      if (NS_FAILED(rv))
        return NS_OK;

      nsCOMPtr<nsIAtom> baseTag;
      PRInt32 nameSpaceID;
      xblService->ResolveTag(aContent, &nameSpaceID, getter_AddRefs(baseTag));
 
      if (baseTag.get() != aTag) {
        // Construct the frame using the XBL base tag.
        nsresult rv = ConstructFrameInternal( aPresShell, 
                                  aPresContext,
                                  aState,
                                  aContent,
                                  aParentFrame,
                                  baseTag,
                                  nameSpaceID,
                                  aStyleContext,
                                  aFrameItems,
                                  PR_TRUE);
        if (binding) {
          nsCOMPtr<nsIBindingManager> bm;
          mDocument->GetBindingManager(getter_AddRefs(bm));
          if (bm)
            bm->AddToAttachedQueue(binding);
        }
        return rv;
      }
    }
  }


  nsIFrame* lastChild = aFrameItems.lastChild;

  // Handle specific frame types
  nsresult rv = ConstructFrameByTag(aPresShell, aPresContext, aState, aContent, aParentFrame,
                           aTag, aNameSpaceID, aStyleContext, aFrameItems);

#ifdef INCLUDE_XUL
  // Failing to find a matching HTML frame, try creating a specialized
  // XUL frame. This is temporary, pending planned factoring of this
  // whole process into separate, pluggable steps.
  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                         (lastChild == aFrameItems.lastChild))) {
    PRBool haltProcessing = PR_FALSE;
    rv = ConstructXULFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                           aTag, aNameSpaceID, aStyleContext, aFrameItems, haltProcessing);
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
                              aTag, aNameSpaceID, aStyleContext, aFrameItems);
  }
#endif

// SVG
#ifdef MOZ_SVG
  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                           (lastChild == aFrameItems.lastChild))) {
    rv = ConstructSVGFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                              aTag, aNameSpaceID, aStyleContext, aFrameItems);
  }
#endif

  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                           (lastChild == aFrameItems.lastChild))) {
    // When there is no explicit frame to create, assume it's a
    // container and let display style dictate the rest
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      aStyleContext->GetStyleData(eStyleStruct_Display);

    rv = ConstructFrameByDisplayType(aPresShell, aPresContext, aState, display, aContent,
                                     aParentFrame, aStyleContext, aFrameItems);
  }

  if (binding) {
    nsCOMPtr<nsIBindingManager> bm;
    mDocument->GetBindingManager(getter_AddRefs(bm));
    if (bm)
      bm->AddToAttachedQueue(binding);
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
      } else {
        listFrame = frame;
      }

      if (listFrame != nsnull) {
        nsIListControlFrame * list;
        res = listFrame->QueryInterface(NS_GET_IID(nsIListControlFrame),
                                                 (void**)&list);
        if (NS_SUCCEEDED(res) && list) {
          list->GetOptionsContainer(aPresContext, &frame);
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
    // check for a table caption which goes on an additional child list with a different parent
    nsIFrame* outerTableFrame; 
    if (GetCaptionAdjustedParent(aParentFrame, aFrameList, &outerTableFrame)) {
      rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, outerTableFrame,
                                             nsLayoutAtoms::captionList, aFrameList);
    }
    else {
      rv = aFrameManager->AppendFrames(aPresContext, *aPresShell, aParentFrame,
                                       nsnull, aFrameList);
    }
  }
  NS_IF_RELEASE(parentType);

  return rv;
}

static nsIFrame*
FindPreviousAnonymousSibling(nsIPresShell* aPresShell,
                             nsIContent* aContainer,
                             nsIContent* aChild)
{
  nsIFrame* prevSibling = nsnull;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  
  nsCOMPtr<nsIDocument> doc;
  aContainer->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContainer));
  xblDoc->GetAnonymousNodes(elt, getter_AddRefs(nodeList));
  if (nodeList) {
    PRUint32 ctr,listLength;
    nsCOMPtr<nsIDOMNode> node;
    nodeList->GetLength(&listLength);
    if (listLength == 0)
      return nsnull;
    PRBool found = PR_FALSE;
    for (ctr = listLength; ctr > 0; ctr--) {
      nodeList->Item(ctr-1, getter_AddRefs(node));
      nsCOMPtr<nsIContent> childContent(do_QueryInterface(node));
      if (childContent.get() == aChild) {
        found = PR_TRUE;
        continue;
      }

      if (found) {
        aPresShell->GetPrimaryFrameFor(childContent, &prevSibling);

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
          const nsStylePosition* position;
          prevSibling->GetStyleData(eStyleStruct_Position,
                                    (const nsStyleStruct*&)position);
          if (display->IsFloating() || position->IsPositioned()) {
            // Nope. Get the place-holder instead
            nsIFrame* placeholderFrame;
            aPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
            NS_ASSERTION(nsnull != placeholderFrame, "yikes");
            prevSibling = placeholderFrame;
          }

          break;
        }
      }
    }
  }

  return prevSibling;
}

static nsIFrame*
FindNextAnonymousSibling(nsIPresShell* aPresShell,
                         nsIContent* aContainer,
                         nsIContent* aChild)
{
  nsIFrame* nextSibling = nsnull;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  
  nsCOMPtr<nsIDocument> doc;
  aContainer->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContainer));
  xblDoc->GetAnonymousNodes(elt, getter_AddRefs(nodeList));
  if (nodeList) {
    PRUint32 ctr,listLength;
    nsCOMPtr<nsIDOMNode> node;
    nodeList->GetLength(&listLength);
    PRBool found = PR_FALSE;
    for (ctr = 0; ctr < listLength; ctr++) {
      nodeList->Item(ctr, getter_AddRefs(node));
      nsCOMPtr<nsIContent> childContent(do_QueryInterface(node));
      if (childContent.get() == aChild) {
        found = PR_TRUE;
        continue;
      }

      if (found) {
        aPresShell->GetPrimaryFrameFor(childContent, &nextSibling);

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
          const nsStylePosition* position;
          nextSibling->GetStyleData(eStyleStruct_Position,
                                    (const nsStyleStruct*&)position);
          if (display->IsFloating() || position->IsPositioned()) {
            // Nope. Get the place-holder instead
            nsIFrame* placeholderFrame;
            aPresShell->GetPlaceholderFrameFor(nextSibling, &placeholderFrame);
            NS_ASSERTION(nsnull != placeholderFrame, "yikes");
            nextSibling = placeholderFrame;
          }

          break;
        }
      }
    }
  }

  return nextSibling;
}

static nsIFrame*
FindPreviousSibling(nsIPresShell* aPresShell,
                    nsIContent*   aContainer,
                    PRInt32       aIndexInContainer)
{
  nsIFrame* prevSibling = nsnull;
  
    // Walk the 
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
      const nsStylePosition* position;
      prevSibling->GetStyleData(eStyleStruct_Position,
                                (const nsStyleStruct*&)position);
      if (display->IsFloating() || position->IsPositioned()) {
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
      const nsStylePosition* position;
      nextSibling->GetStyleData(eStyleStruct_Position,
                                (const nsStyleStruct*&)position);
      if (display->IsFloating() || position->IsPositioned()) {
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
        nsXULTreeOuterGroupFrame* treeRowGroup = (nsXULTreeOuterGroupFrame*)outerFrame;
        if (treeRowGroup) {

          // Get the primary frame for the parent of the child that's being added.
          nsIFrame* innerFrame = GetFrameFor(shell, aPresContext, aContainer);
  
          nsXULTreeGroupFrame* innerGroup = (nsXULTreeGroupFrame*) innerFrame;
          if (innerGroup) {
            nsBoxLayoutState state(aPresContext);
            innerGroup->MarkDirtyChildren(state);
          }
          treeRowGroup->ClearRowGroupInfo();
          shell->FlushPendingNotifications();

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
    if (IsFrameSpecial(parentFrame)) {
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

    // We built some new frames.  Initialize any newly-constructed bindings.
    nsCOMPtr<nsIBindingManager> bm;
    mDocument->GetBindingManager(getter_AddRefs(bm));
    bm->ProcessAttachedQueue();

    // process the current pseudo frame state
    if (!state.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, state.mPseudoFrames, frameItems);
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
    if (0 == numOptions) { 
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

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult rv = NS_OK;

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
        nsXULTreeOuterGroupFrame* treeRowGroup = (nsXULTreeOuterGroupFrame*)outerFrame;

        if (treeRowGroup) {

          // Get the primary frame for the parent of the child that's being added.
          nsIFrame* innerFrame = GetFrameFor(shell, aPresContext, aContainer);
  
          nsXULTreeGroupFrame* innerGroup = (nsXULTreeGroupFrame*) innerFrame;
          treeRowGroup->ClearRowGroupInfo();
          nsBoxLayoutState state(aPresContext);
          treeRowGroup->MarkDirtyChildren(state);
   
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
            
            if (innerGroup) {
              //nsBoxLayoutState state(aPresContext);
              //innerGroup->MarkDirtyChildren(state);

              // Good call.  Make sure a full reflow happens.
              nsIFrame* nextSibling = FindNextSibling(shell, aContainer, aIndexInContainer);
              if (nextSibling)
                innerGroup->OnContentInserted(aPresContext, nextSibling, aIndexInContainer);
            }
          }

          shell->FlushPendingNotifications();
          return NS_OK;
        }
      }
    }
  }
#endif // INCLUDE_XUL
  
  // If we have a null parent, then this must be the document element
  // being inserted
  if (nsnull == aContainer) {
    nsCOMPtr<nsIContent> docElement( dont_AddRef( mDocument->GetRootContent() ) );

    if (aChild == docElement.get()) {
      NS_PRECONDITION(nsnull == mInitialContainingBlock, "initial containing block already created");

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
    }

    nsCOMPtr<nsIBindingManager> bm;
    mDocument->GetBindingManager(getter_AddRefs(bm));
    bm->ProcessAttachedQueue();

    // otherwise this is not a child of the root element, and we
    // won't let it have a frame.

  } else {
    // Find the frame that precedes the insertion point.
    nsIFrame* prevSibling = (aIndexInContainer == -1) ? 
                             FindPreviousAnonymousSibling(shell, aContainer, aChild) :
                             FindPreviousSibling(shell, aContainer, aIndexInContainer);

    nsIFrame* nextSibling = nsnull;
    PRBool    isAppend = PR_FALSE;
    
    // If there is no previous sibling, then find the frame that follows
    if (nsnull == prevSibling) {
      nextSibling = (aIndexInContainer == -1) ? 
                    FindNextAnonymousSibling(shell, aContainer, aChild) :
                    FindNextSibling(shell, aContainer, aIndexInContainer);
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
      if (IsFrameSpecial(parentFrame)) {
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
      if (IsFrameSpecial(parentFrame)) {
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
      nsCOMPtr<nsIBindingManager> bm;
      mDocument->GetBindingManager(getter_AddRefs(bm));
      bm->ProcessAttachedQueue();

      // process the current pseudo frame state
      if (!state.mPseudoFrames.IsEmpty()) {
        ProcessPseudoFrames(aPresContext, state.mPseudoFrames, frameItems);
      }

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
          nsIFrame* outerTableFrame; 
          if (GetCaptionAdjustedParent(parentFrame, newFrame, &outerTableFrame)) {
            rv = state.mFrameManager->AppendFrames(aPresContext, *shell, outerTableFrame,
                                                   nsLayoutAtoms::captionList, newFrame);
          }
          else {
            rv = state.mFrameManager->InsertFrames(aPresContext, *shell, parentFrame,
                                                   nsnull, prevSibling, newFrame);
          }
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
  if ( content ) {
	aFrameManager->SetPrimaryFrameFor(content, nsnull);
	aFrameManager->ClearAllUndisplayedContentIn(content);
  }

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
      // If it's a "special" block-in-inline frame, then we need to
      // remember to delete our special siblings, too.
      if (IsFrameSpecial(aFrame)) {
        nsIFrame* specialSibling;
        GetSpecialSibling(aFrameManager, aFrame, &specialSibling);
        DeletingFrameSubtree(aPresContext, aPresShell, aFrameManager, specialSibling);
      }

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
        NS_IF_RELEASE(styleContext);
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
        nsXULTreeGroupFrame* treeRowGroup = (nsXULTreeGroupFrame*)parentFrame;
        if (treeRowGroup) {
          treeRowGroup->OnContentRemoved(aPresContext, childFrame, aIndexInContainer);
        }
      }
      {
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
          nsXULTreeOuterGroupFrame* treeRowGroup = (nsXULTreeOuterGroupFrame*)parentFrame;
          if (treeRowGroup) {
            nsBoxLayoutState state(aPresContext);
            treeRowGroup->MarkDirtyChildren(state);
            treeRowGroup->ClearRowGroupInfo();
            shell->FlushPendingNotifications();
          }
          return NS_OK;
        }
      }
    }
  }
#endif // INCLUDE_XUL

  if (childFrame) {
    // If the frame we are manipulating is a special frame then do
    // something different instead of just inserting newly created
    // frames.
    if (IsFrameSpecial(childFrame)) {
      // We are pretty harsh here (and definitely not optimal) -- we
      // wipe out the entire containing block and recreate it from
      // scratch. The reason is that because we know that a special
      // inline frame has propogated some of its children upward to be
      // children of the block and that those frames may need to move
      // around. This logic guarantees a correct answer.
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::ContentRemoved: childFrame=");
        nsFrame::ListTag(stdout, childFrame);
        printf(" is special\n");
      }
#endif
      return ReframeContainingBlock(aPresContext, childFrame);
    }

    // Get the childFrame's parent frame
    nsIFrame* parentFrame;
    childFrame->GetParent(&parentFrame);

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
    //   (positioned frames are handled below in the "else" clause)
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
        // check for a table caption which goes on an additional child list with a different parent
        nsIFrame* outerTableFrame; 
        if (GetCaptionAdjustedParent(parentFrame, childFrame, &outerTableFrame)) {
          rv = frameManager->RemoveFrame(aPresContext, *shell, outerTableFrame,
                                         nsLayoutAtoms::captionList, childFrame);
        }
        else {
          rv = frameManager->RemoveFrame(aPresContext, *shell, parentFrame,
                                         nsnull, childFrame);
        }
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
      viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
    NS_RELEASE(viewManager);
  }
}

nsresult
nsCSSFrameConstructor::StyleChangeReflow(nsIPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         nsIAtom* aAttribute)
{

  // Is it a box? If so we can coelesce.
  nsresult rv;
  nsCOMPtr<nsIBox> box = do_QueryInterface(aFrame, &rv);
  if (NS_SUCCEEDED(rv) && box) {
    nsBoxLayoutState state(aPresContext);
    box->MarkStyleChange(state);
  }
  else if (IsFrameSpecial(aFrame)) {
    // We are pretty harsh here (and definitely not optimal) -- we
    // wipe out the entire containing block and recreate it from
    // scratch. The reason is that because we know that a special
    // inline frame has propogated some of its children upward to be
    // children of the block and that those frames may need to move
    // around. This logic guarantees a correct answer.
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::StyleChangeReflow: aFrame=");
      nsFrame::ListTag(stdout, aFrame);
      printf(" is special\n");
    }
#endif
    ReframeContainingBlock(aPresContext, aFrame);
  }
  else {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
 

    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                 nsIReflowCommand::StyleChanged,
                                 nsnull,
                                 aAttribute);
  
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return NS_OK;
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

#ifdef INCLUDE_XUL
  // The following tree widget trap prevents offscreen tree widget
  // content from being removed and re-inserted (which is what would
  // happen otherwise).
  if (!primaryFrame) {
    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    if (reframe == PR_FALSE && tag && (tag.get() == nsXULAtoms::treechildren ||
      tag.get() == nsXULAtoms::treeitem || tag.get() == nsXULAtoms::treerow ||
      tag.get() == nsXULAtoms::treecell))
      return NS_OK;
  }
#endif // INCLUDE_XUL

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

      PRBool affects;
      frameManager->AttributeAffectsStyle(aAttribute, aContent, affects);
      if (affects) {
#ifdef DEBUG_shaver
        fputc('+', stderr);
#endif
        // there is an effect, so compute it
        frameManager->ComputeStyleChangeFor(aPresContext, primaryFrame, 
                                            aNameSpaceID, aAttribute,
                                            changeList, aHint, maxHint);
      } else {
#ifdef DEBUG_shaver
        fputc('-', stderr);
#endif
        // let this frame update itself, but don't walk the whole frame tree
        maxHint = NS_STYLE_HINT_VISUAL;
      }

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
          // first check if it is a background change: 
          // - if it is then we may need to notify the canvas frame
          //   so it can take care of invalidating the whole canvas
          if (aAttribute == nsHTMLAtoms::bgcolor || aAttribute == nsHTMLAtoms::background) {
            // see if the content element is the root (HTML) or BODY element
            // NOTE: the assumption here is that the background color or image on
            //       the BODY or HTML element need to have the canvas frame invalidate
            //       so the entire visible regions gets painted
            nsCOMPtr<nsIContent> rootContent;
            nsCOMPtr<nsIContent> parentContent;
            rootContent = getter_AddRefs(mDocument->GetRootContent());
            aContent->GetParent(*getter_AddRefs(parentContent));
            if (aContent == rootContent.get() ||    // content is the root (HTML)
                parentContent == rootContent ) {    // content's parent is root (BODY)
              // Walk the frame tree up and find the canvas frame
              nsIFrame *pCanvasFrameCandidate = nsnull;
              primaryFrame->GetParent(&pCanvasFrameCandidate);
              while (pCanvasFrameCandidate) {
                if (IsCanvasFrame(pCanvasFrameCandidate)) {
                  pCanvasFrameCandidate->AttributeChanged(aPresContext,aContent,aNameSpaceID,aAttribute,maxHint);
                  break;
                } else {
                  pCanvasFrameCandidate->GetParent(&pCanvasFrameCandidate);
                }
              }
            }
          } 
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

  if (!frame) {
    return NS_OK;
  }

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
nsCSSFrameConstructor::ConstructAlternateFrame(nsIPresShell*    aPresShell, 
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
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  GetAlternateTextFor(aContent, tag, altText);

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
  altTextContent->SetDocument(document, PR_TRUE, PR_TRUE);

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
                                           aStyleContext, nsnull, PR_FALSE);

  // If the frame is out-of-flow, then mark it as such
  if (isOutOfFlow) {
    nsFrameState  frameState;
    containerFrame->GetFrameState(&frameState);
    containerFrame->SetFrameState(frameState | NS_FRAME_OUT_OF_FLOW);
  }

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


static PRBool
HasDisplayableChildren(nsIPresContext* aPresContext, nsIFrame* aContainerFrame)
{
  // Returns 'true' if there are frames within aContainerFrame that
  // could be displayed in the frame list.
  NS_PRECONDITION(aContainerFrame != nsnull, "null ptr");
  if (! aContainerFrame)
    return PR_FALSE;

  nsIFrame* frame;
  aContainerFrame->FirstChild(aPresContext, nsnull, &frame);

  while (frame) {
    // If it's not a text frame, then assume that it's displayable.
    nsCOMPtr<nsIAtom> frameType;
    frame->GetFrameType(getter_AddRefs(frameType));
    if (frameType.get() != nsLayoutAtoms::textFrame)
      return PR_TRUE;

    // Get the text content...
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));
    
    nsCOMPtr<nsITextContent> text = do_QueryInterface(content);
    NS_ASSERTION(text != nsnull, "oops, not an nsITextContent");
    if (! text)
      return PR_TRUE;
    
    // Is it only whitespace?
    PRBool onlyWhitespace;
    text->IsOnlyWhitespace(&onlyWhitespace);
    
    // If not, then we have displayable content here.
    if (! onlyWhitespace)
      return PR_TRUE;
    
    // Otherwise, on to the next frame...
    frame->GetNextSibling(&frame);
  }

  // If we get here, then we've iterated through all the child frames,
  // and every one is a text frame containing whitespace. (Or, there
  // weren't any frames at all!) There is nothing to diplay.
  return PR_FALSE;
}



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
  
  // See whether it's an IMG or an INPUT element (for image buttons)
  if (nsHTMLAtoms::img == tag.get() || nsHTMLAtoms::input == tag.get()) {
    // It's an IMG element. Try and construct an alternate frame to use when the
    // image can't be rendered
    nsIFrame* newFrame;
    rv = ConstructAlternateFrame(aPresShell, aPresContext, content, styleContext,
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
    const nsStyleDisplay*   display =
      NS_STATIC_CAST(const nsStyleDisplay*, styleContext->GetStyleData(eStyleStruct_Display));

    // Create a new frame based on the display type.
    // Note: if the old frame was out-of-flow, then so will the new frame
    // and we'll get a new placeholder frame
    rv = ConstructFrameByDisplayType(aPresShell, aPresContext, state, display, content,
                                     inFlowParent, styleContext, frameItems);

    if (NS_FAILED(rv)) return rv;

    nsIFrame* newFrame = frameItems.childList;

    if (nsHTMLAtoms::applet == tag.get()
        && !HasDisplayableChildren(aPresContext, newFrame)) {
      // If it's an <applet> tag without any displayable content, then
      // it may have "alt" text specified that we could use to render
      // text. Nuke the frames we just created, and use
      // ConstructAlternateFrame() to fix stuff up.
      nsFrameList list(newFrame);
      list.DestroyFrames(aPresContext);

      rv = ConstructAlternateFrame(aPresShell, aPresContext, content, styleContext,
                                   parentFrame, newFrame);
    }

    if (NS_SUCCEEDED(rv)) {
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
      if (listName == nsnull) {
        if (IsInlineFrame(aFrame) && !AreAllKidsInline(newFrame)) {
          // We're in the uncomfortable position of being an inline
          // that now contains a block. As in ConstructInline(), break
          // the newly constructed frames into three lists: the inline
          // frames before the first block frame (list1), the inline
          // frames after the last block frame (list3), and all the
          // frames between the first and last block frames (list2).
          nsIFrame* list1 = newFrame;
          nsIFrame* prevToFirstBlock;
          nsIFrame* list2 = FindFirstBlock(aPresContext, list1, &prevToFirstBlock);

          if (prevToFirstBlock) {
            prevToFirstBlock->SetNextSibling(nsnull);
          }
          else {
            list1 = nsnull;
          }

          nsIFrame* afterFirstBlock;
          list2->GetNextSibling(&afterFirstBlock);
          nsIFrame* list3 = nsnull;
          nsIFrame* lastBlock = FindLastBlock(aPresContext, afterFirstBlock);
          if (! lastBlock) {
            lastBlock = list2;
          }
          lastBlock->GetNextSibling(&list3);
          lastBlock->SetNextSibling(nsnull);

          // Create "special" inline-block linkage between the frames
          SetFrameIsSpecial(state.mFrameManager, list1, list2);
          SetFrameIsSpecial(state.mFrameManager, list2, list3);
          SetFrameIsSpecial(state.mFrameManager, list3, nsnull);

          // Recursively split inlines back up to the first containing
          // block frame.
          SplitToContainingBlock(aPresContext, state, aFrame, list1, list2, list3, PR_FALSE);
        }
      } else if (listName.get() == nsLayoutAtoms::absoluteList) {
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
                                             aStyleContext, nsnull, PR_FALSE);

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
                                             aStyleContext, nsnull, PR_FALSE);

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
                                               styleContext, nsnull, PR_FALSE);
    }
    
  } else if (nsLayoutAtoms::inlineFrame == frameType) {
    rv = NS_NewInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::blockFrame == frameType) {
    rv = NS_NewBlockFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::areaFrame == frameType) {
    rv = NS_NewAreaFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext,
                     aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::positionedInlineFrame == frameType) {
    rv = NS_NewPositionedInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);
    }

  } else if (nsLayoutAtoms::pageFrame == frameType) {
    rv = NS_NewPageFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_TRUE);
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
                                               styleContext, nsnull, PR_FALSE);
    }

  } else if (nsLayoutAtoms::tableRowFrame == frameType) {
    rv = NS_NewTableRowFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);

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
                                               styleContext, nsnull, PR_FALSE);

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
                                               styleContext, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::letterFrame == frameType) {
    rv = NS_NewFirstLetterFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, newFrame,
                                               styleContext, nsnull, PR_FALSE);
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

// Helper function that searches the immediate child frames 
// (and their children if the frames are "special")
// for a frame that maps the specified content object
nsIFrame*
nsCSSFrameConstructor::FindFrameWithContent(nsIPresContext* aPresContext,
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
      // child frames, too.
      // We also need to search the child frame's children if the child frame
      // is a "special" frame
      if (kidContent.get() == aParentContent || IsFrameSpecial(kidFrame)) {
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
      else if (IsFrameSpecial(parentFrame)) {
        // If it's a "special" frame (that is, part of an inline
        // that's been split because it contained a block), we need to
        // follow the out-of-flow "special sibling" link, and search
        // *that* subtree as well.
        nsIFrame* specialSibling = nsnull;
        GetSpecialSibling(aFrameManager, parentFrame, &specialSibling);
        parentFrame = specialSibling;
      }
      else {
        break;
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
  if (aContent) {
    aPresContext->ResolvePseudoStyleContextFor(aContent,
                                               nsHTMLAtoms::firstLetterPseudo,
                                               aStyleContext, PR_FALSE, &fls);
  }
  return fls;
}

nsIStyleContext*
nsCSSFrameConstructor::GetFirstLineStyle(nsIPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsIStyleContext* aStyleContext)
{
  nsIStyleContext* fls = nsnull;
  if (aContent) {
    aPresContext->ResolvePseudoStyleContextFor(aContent,
                                               nsHTMLAtoms::firstLinePseudo,
                                               aStyleContext, PR_FALSE, &fls);
  }
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
  if (aContent) {
    aPresContext->ProbePseudoStyleContextFor(aContent,
                                             nsHTMLAtoms::firstLetterPseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(fls));
  }
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
  if (aContent) {
    aPresContext->ProbePseudoStyleContextFor(aContent,
                                             nsHTMLAtoms::firstLinePseudo,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(fls));
  }
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
nsCSSFrameConstructor::ProcessChildren(nsIPresShell*            aPresShell, 
                                       nsIPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aFrame,
                                       PRBool                   aCanHaveGeneratedContent,
                                       nsFrameItems&            aFrameItems,
                                       PRBool                   aParentIsBlock,
                                       nsTableCreator*          aTableCreator)
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

  if (aTableCreator) { // do special table child processing
    // if there is a caption child here, it gets recorded in aState.mPseudoFrames.
    nsIFrame* captionFrame;
    TableProcessChildren(aPresShell, aPresContext, aState, aContent, aFrame, 
                         *aTableCreator, aFrameItems, captionFrame);
  }
  else {
    // save the incoming pseudo frame state 
    nsPseudoFrames priorPseudoFrames; 
    aState.mPseudoFrames.Reset(&priorPseudoFrames);

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
    // process the current pseudo frame state
    if (!aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
    }

    // restore the incoming pseudo frame state 
    aState.mPseudoFrames = priorPseudoFrames;
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

    PRInt32 namespaceID;
    aChild->GetNameSpaceID(namespaceID);

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

    rv = ConstructFrameInternal(shell, aPresContext, state, aChild, aParentFrame, tag, namespaceID, 
                                styleContext, frameItems, PR_FALSE);
    
    nsIFrame* newFrame = frameItems.childList;
    *aNewFrame = newFrame;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      nsCOMPtr<nsIBindingManager> bm;
      mDocument->GetBindingManager(getter_AddRefs(bm));
      bm->ProcessAttachedQueue();

      // Notify the parent frame
      if (aIsAppend)
        rv = ((nsXULTreeGroupFrame*)aParentFrame)->TreeAppendFrames(newFrame);
      else
        rv = ((nsXULTreeGroupFrame*)aParentFrame)->TreeInsertFrames(aPrevFrame, newFrame);
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
                                           aStyleContext, nsnull, PR_FALSE);

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

  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, aNewFrame,
                          childItems);

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

  // process pseudo frames if necessary
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
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
                                       PRBool                   aIsPositioned,
                                       nsIFrame*                aNewFrame,
                                       nsIFrame**               aNewBlockFrame,
                                       nsIFrame**               aNextInlineFrame)
{
  // Initialize the frame
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  nsFrameConstructorSaveState absoluteSaveState;  // definition cannot be inside next block
                                                  // because the object's destructor is significant
                                                  // this is part of the fix for bug 42372
  if (aIsPositioned) {                            
    // Relatively positioned frames need a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame,
                                             aStyleContext, nsnull, PR_FALSE);

    // Relatively positioned frames becomes a container for child
    // frames that are positioned
    aState.PushAbsoluteContainingBlock(aNewFrame, absoluteSaveState);
  }

  // Process the child content
  nsFrameItems childItems;
  PRBool kidsAllInline;
  nsresult rv = ProcessInlineChildren(aPresShell, aPresContext, aState, aContent,
                                      aNewFrame, PR_TRUE, childItems, &kidsAllInline);
  if (kidsAllInline) {
    // Set the inline frame's initial child list
    nsCOMPtr<nsIAtom> tag;
    aContent->GetTag(*getter_AddRefs(tag));
    CreateAnonymousFrames(aPresShell, aPresContext, tag, aState, aContent, aNewFrame,
                            childItems);

    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

    if (aIsPositioned) {
      if (aState.mAbsoluteItems.childList) {
        aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                       aState.mAbsoluteItems.childList);
      }
      if (aState.mFloatedItems.childList) {
        aNewFrame->SetInitialChildList(aPresContext,
                                       nsLayoutAtoms::floaterList,
                                       aState.mFloatedItems.childList);
      }
    }

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

  if (aIsPositioned) {
    // XXXwaterson just for shits n' giggles, we'll give you the
    // absolute and floated items, too.
    if (aState.mAbsoluteItems.childList) {
      aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                     aState.mAbsoluteItems.childList);
    }
    if (aState.mFloatedItems.childList) {
      aNewFrame->SetInitialChildList(aPresContext,
                                     nsLayoutAtoms::floaterList,
                                     aState.mFloatedItems.childList);
    }
  }

  // list2's frames belong to an anonymous block that we create right
  // now. The anonymous block will be the parent of the block children
  // of the inline.
  nsIFrame* blockFrame;
  nsIAtom* blockStyle;
  if (aIsPositioned) {
    NS_NewRelativeItemWrapperFrame(aPresShell, &blockFrame);
    blockStyle = nsHTMLAtoms::mozAnonymousPositionedBlock;
  }
  else {
    NS_NewBlockFrame(aPresShell, &blockFrame);
    blockStyle = nsHTMLAtoms::mozAnonymousBlock;
  }

  nsCOMPtr<nsIStyleContext> blockSC;
  aPresContext->ResolvePseudoStyleContextFor(aContent, blockStyle,
                                             aStyleContext, PR_FALSE,
                                             getter_AddRefs(blockSC));

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, blockSC, nsnull, blockFrame);  

  if (aIsPositioned) {
    // Relatively positioned frames need a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, blockFrame,
                                             aStyleContext, nsnull, PR_FALSE);
  }

  MoveChildrenTo(aPresContext, blockSC, blockFrame, list2);
  blockFrame->SetInitialChildList(aPresContext, nsnull, list2);

  // list3's frames belong to another inline frame
  nsIFrame* inlineFrame = nsnull;

  if (aIsPositioned) {
    NS_NewPositionedInlineFrame(aPresShell, &inlineFrame);
  }
  else {
    NS_NewInlineFrame(aPresShell, &inlineFrame);
  }

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, inlineFrame);

  if (aIsPositioned) {
    // Relatively positioned frames need a view
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, inlineFrame,
                                             aStyleContext, nsnull, PR_FALSE);
  }

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
  SetFrameIsSpecial(aState.mFrameManager, aNewFrame, blockFrame);
  SetFrameIsSpecial(aState.mFrameManager, blockFrame, inlineFrame);
  SetFrameIsSpecial(aState.mFrameManager, inlineFrame, nsnull);

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
nsCSSFrameConstructor::SplitToContainingBlock(nsIPresContext* aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIFrame* aFrame,
                                              nsIFrame* aLeftInlineChildFrame,
                                              nsIFrame* aBlockChildFrame,
                                              nsIFrame* aRightInlineChildFrame,
                                              PRBool aTransfer)
{
  // If aFrame is an inline frame, then recursively "split" it until
  // we reach a block frame. aLeftInlineChildFrame is the original
  // inline child of aFrame; aBlockChildFrame and
  // aRightInlineChildFrame are the newly created frames that were
  // constructed as a result of the previous recursion's "split".
  //
  // aBlockChildFrame and aRightInlineChildFrame will be "orphaned" frames upon
  // entry to this routine; that is, they won't be parented. We'll
  // assign them proper parents.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  if (IsBlockFrame(aPresContext, aFrame)) {
    // If aFrame is a block frame, then we're done: make
    // aBlockChildFrame and aRightInlineChildFrame children of aFrame,
    // and insert aBlockChildFrame and aRightInlineChildFrame after
    // aLeftInlineChildFrame
    aBlockChildFrame->SetParent(aFrame);
    aRightInlineChildFrame->SetParent(aFrame);
    aBlockChildFrame->SetNextSibling(aRightInlineChildFrame);
    aFrame->InsertFrames(aPresContext, *shell, nsnull, aLeftInlineChildFrame, aBlockChildFrame);
    return NS_OK;
  }

  // Otherwise, aFrame is inline. Split it, and recurse to find the
  // containing block frame.
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));

  // Create an "anonymous block" frame that will parent
  // aBlockChildFrame. The new frame won't have a parent yet: the recursion
  // will parent it.
  nsIFrame* blockFrame;
  NS_NewBlockFrame(shell, &blockFrame);

  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));

  nsCOMPtr<nsIStyleContext> blockSC;
  aPresContext->ResolvePseudoStyleContextFor(content,
                                             nsHTMLAtoms::mozAnonymousBlock,
                                             styleContext,
                                             PR_FALSE,
                                             getter_AddRefs(blockSC));

  InitAndRestoreFrame(aPresContext, aState, content,
                      nsnull, blockSC, nsnull, blockFrame);

  blockFrame->SetInitialChildList(aPresContext, nsnull, aBlockChildFrame);
  MoveChildrenTo(aPresContext, blockSC, blockFrame, aBlockChildFrame);

  // Create an anonymous inline frame that will parent
  // aRightInlineChildFrame. The new frame won't have a parent yet:
  // the recursion will parent it.
  nsIFrame* inlineFrame;
  NS_NewInlineFrame(shell, &inlineFrame);

  InitAndRestoreFrame(aPresContext, aState, content,
                      nsnull, styleContext, nsnull, inlineFrame);

  inlineFrame->SetInitialChildList(aPresContext, nsnull, aRightInlineChildFrame);
  MoveChildrenTo(aPresContext, styleContext, inlineFrame, aRightInlineChildFrame);

  // Make the "special" inline-block linkage between aFrame and the
  // newly created anonymous frames. We need to create the linkage
  // between the first in flow, so if we're a continuation frame, walk
  // back to find it.
  nsIFrame* firstInFlow = aFrame;
  while (1) {
    nsIFrame* prevInFlow;
    firstInFlow->GetPrevInFlow(&prevInFlow);
    if (! prevInFlow) break;
    firstInFlow = prevInFlow;
  }

  SetFrameIsSpecial(aState.mFrameManager, firstInFlow, blockFrame);
  SetFrameIsSpecial(aState.mFrameManager, blockFrame, inlineFrame);
  SetFrameIsSpecial(aState.mFrameManager, inlineFrame, nsnull);

  // If we have a continuation frame, then we need to break the
  // continuation.
  nsIFrame* nextInFlow;
  aFrame->GetNextInFlow(&nextInFlow);
  if (nextInFlow) {
    aFrame->SetNextInFlow(nsnull);
    nextInFlow->SetPrevInFlow(nsnull);
  }

  // This is where the mothership lands and we start to get a bit
  // funky. We're going to do a bit of work to ensure that the frames
  // from the *last* recursion are properly hooked up.
  //
  // aTransfer will be set once the recursion begins to nest. (It's
  // not set at the first level of recursion, because
  // aLeftInlineChildFrame, aBlockChildFrame, and
  // aRightInlineChildFrame already have their sibling and parent
  // pointers properly initialized.)
  //
  // Once we begin to nest recursion, aLeftInlineChildFrame
  // corresponds to the original inline that we're trying to split,
  // and aBlockChildFrame and aRightInlineChildFrame are the anonymous
  // frames we created to protect the inline-block invariant.
  if (aTransfer) {
    // We need to move any successors of the original inline
    // (aLeftInlineChildFrame) to aRightInlineChildFrame.
    nsIFrame* nextInlineFrame;
    aLeftInlineChildFrame->GetNextSibling(&nextInlineFrame);
    aLeftInlineChildFrame->SetNextSibling(nsnull);
    aRightInlineChildFrame->SetNextSibling(nextInlineFrame);

    // Any frame that was moved will need its parent pointer fixed,
    // and will need to be marked as dirty.
    while (nextInlineFrame) {
      nextInlineFrame->SetParent(inlineFrame);

      nsFrameState state;
      nextInlineFrame->GetFrameState(&state);
      state |= NS_FRAME_IS_DIRTY;
      nextInlineFrame->SetFrameState(state);

      nextInlineFrame->GetNextSibling(&nextInlineFrame);
    }
  }

  // Recurse to the parent frame. This will assign a parent frame to
  // each new frame we've just created.
  nsIFrame* parent;
  aFrame->GetParent(&parent);

  NS_ASSERTION(parent != nsnull, "frame has no geometric parent");
  if (! parent)
    return NS_ERROR_FAILURE;

  // When we recur, we'll make the "left inline child frame" be the
  // inline frame we've just begun to "split", and we'll pass the
  // newly created anonymous frames as aBlockChildFrame and
  // aRightInlineChildFrame.
  return SplitToContainingBlock(aPresContext, aState, parent, aFrame, blockFrame, inlineFrame, PR_TRUE);
}

nsresult
nsCSSFrameConstructor::ReframeContainingBlock(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  // Get the first "normal" parent of the target frame. From there we
  // look for the containing block in case the target frame is already
  // a block (which can happen when an inline frame wraps some of its
  // content in an anonymous block; see ConstructInline)
  nsIFrame* parentFrame;
  do {
    aFrame->GetParent(&parentFrame);
    if (!parentFrame || !IsFrameSpecial(parentFrame))
      break;

    aFrame = parentFrame;
  } while (1);


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
