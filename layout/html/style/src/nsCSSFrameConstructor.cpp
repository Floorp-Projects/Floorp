/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCSSFrameConstructor.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsHTMLAtoms.h"
#include "nsPresContext.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsTableFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLTableColElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsTableCellFrame.h" // to get IS_CELL_FRAME
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsStyleConsts.h"
#include "nsTableOuterFrame.h"
#include "nsIDOMXULElement.h"
#include "nsIWebShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIComboboxControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIRadioControlFrame.h"
#include "nsICheckboxControlFrame.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsITextContent.h"
#include "nsPlaceholderFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsStyleChangeList.h"
#include "nsIFormControl.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsIDeviceContext.h"
#include "nsTextFragment.h"
#include "nsISupportsArray.h"
#include "nsIAnonymousContentCreator.h"
#include "nsFrameManager.h"
#include "nsLegendFrame.h"
#include "nsIContentIterator.h"
#include "nsBoxLayoutState.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsITheme.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsFormControlHelper.h"
#include "nsObjectFrame.h"
#include "nsRuleNode.h"
#include "nsIDOMMutationEvent.h"
#include "nsChildIterator.h"
#include "nsCSSRendering.h"
#include "nsISelectElement.h"
#include "nsLayoutErrors.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"
#include "nsScrollPortFrame.h"
#include "nsXULAtoms.h"
#include "nsBoxFrame.h"
#include "nsIBoxLayout.h"
#ifdef MOZ_ENABLE_CAIRO
#include "nsCanvasFrame.h"
#endif

static NS_DEFINE_CID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);

#include "nsIDOMWindowInternal.h"
#include "nsIMenuFrame.h"

#include "nsBox.h"

#ifdef MOZ_XUL
#include "nsIRootBox.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIXULDocument.h"
#endif

#include "nsInlineFrame.h"
#include "nsBlockFrame.h"

#include "nsIScrollableFrame.h"

#include "nsIXBLService.h"
#include "nsIStyleRuleSupplier.h"

#undef NOISY_FIRST_LETTER

#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#endif

#ifdef MOZ_XTF
#include "nsIXTFElement.h"
#include "nsIXTFElementWrapperPrivate.h"
#include "nsIXTFVisualWrapperPrivate.h"
nsresult
NS_NewXTFXULDisplayFrame(nsIPresShell*, nsIFrame**);
nsresult
NS_NewXTFXMLDisplayFrame(nsIPresShell*, PRBool isBlock, nsIFrame**);
#ifdef MOZ_SVG
nsresult
NS_NewXTFSVGDisplayFrame(nsIPresShell*, nsIContent*, nsIFrame**);
#endif
#endif

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#include "nsISVGTextContainerFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsStyleUtil.h"

nsresult
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGPolylineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGPolygonFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGCircleFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGEllipseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGLineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGRectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGGenericContainerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsIFrame** aNewFrame);
nsresult
NS_NewSVGDefsFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
PRBool 
NS_SVG_TestFeatures (const nsAString& value);
#endif

#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIScrollable.h"
#include "nsINodeInfo.h"
#include "prenv.h"
#include "nsWidgetsCID.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

// Global object maintenance
nsIXBLService * nsCSSFrameConstructor::gXBLService = nsnull;

// Global prefs
static PRBool gGotXBLFormPrefs = PR_FALSE;
static PRBool gUseXBLForms = PR_FALSE;

#ifdef DEBUG
// Set the environment variable GECKO_FRAMECTOR_DEBUG_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static PRBool gNoisyContentUpdates = PR_FALSE;
static PRBool gReallyNoisyContentUpdates = PR_FALSE;
static PRBool gNoisyInlineConstruction = PR_FALSE;
static PRBool gVerifyFastFindFrame = PR_FALSE;

struct FrameCtorDebugFlags {
  const char* name;
  PRBool* on;
};

static FrameCtorDebugFlags gFlags[] = {
  { "content-updates",              &gNoisyContentUpdates },
  { "really-noisy-content-updates", &gReallyNoisyContentUpdates },
  { "noisy-inline",                 &gNoisyInlineConstruction },
  { "fast-find-frame",              &gVerifyFastFindFrame },
};

#define NUM_DEBUG_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))
#endif


#ifdef MOZ_XUL
#include "nsMenuFrame.h"
#include "nsPopupSetFrame.h"
#include "nsTreeColFrame.h"
#include "nsIBoxObject.h"
#include "nsIListBoxObject.h"
#include "nsListBoxBodyFrame.h"
#include "nsListItemFrame.h"

//------------------------------------------------------------------

nsresult
NS_NewAutoRepeatBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewRootBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewThumbFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewDeckFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager = nsnull);

nsresult
NS_NewLeafBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewStackFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager = nsnull);

nsresult
NS_NewProgressMeterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewImageBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTextBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewGroupBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewButtonBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

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

nsresult
NS_NewListBoxScrollPortFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewTreeBodyFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame);

#ifdef MOZ_ENABLE_CAIRO
nsresult
NS_NewCanvasXULFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame);
#endif

// grid
nsresult
NS_NewGridLayout2 ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsresult
NS_NewGridRowLeafLayout ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsresult
NS_NewGridRowLeafFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, nsIBoxLayout* aLayout);
nsresult
NS_NewGridRowGroupLayout ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsresult
NS_NewGridRowGroupFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, nsIBoxLayout* aLayout);

nsresult
NS_NewListBoxLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

// end grid

nsresult
NS_NewTitleBarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewResizerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame);


#endif

nsresult
NS_NewScrollPortFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewHTMLScrollFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot);

nsresult
NS_NewXULScrollFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot);

nsresult
NS_NewSliderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewScrollbarButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

nsresult
NS_NewNativeScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );


#ifdef NOISY_FINDFRAME
static PRInt32 FFWC_totalCount=0;
static PRInt32 FFWC_doLoop=0;
static PRInt32 FFWC_doSibling=0;
static PRInt32 FFWC_recursions=0;
static PRInt32 FFWC_nextInFlows=0;
static PRInt32 FFWC_slowSearchForText=0;
#endif

#ifdef  MOZ_SVG

// Test to see if this language is supported
static PRBool
SVG_TestLanguage(const nsSubstring& lstr, const nsSubstring& prefs) 
{
  // Compare list to attribute value, which may be a list
  // According to the SVG 1.1 Spec (at least as I read it), we should take
  // the first attribute value and check it for any matches in the users
  // preferences, including any prefix matches.
  // This algorithm is O(M*N)
  PRInt32 vbegin = 0;
  PRInt32 vlen = lstr.Length();
  while (vbegin < vlen) {
    PRInt32 vend = lstr.FindChar(PRUnichar(','), vbegin);
    if (vend == kNotFound) {
      vend = vlen;
    }
    PRInt32 gbegin = 0;
    PRInt32 glen = prefs.Length();
    while (gbegin < glen) {
      PRInt32 gend = prefs.FindChar(PRUnichar(','), gbegin);
      if (gend == kNotFound) {
        gend = glen;
      }
      const nsDefaultStringComparator defaultComparator;
      const nsStringComparator& comparator = 
                  NS_STATIC_CAST(const nsStringComparator&, defaultComparator);
      if (nsStyleUtil::DashMatchCompare(Substring(lstr, vbegin, vend-vbegin),
                                        Substring(prefs, gbegin, gend-gbegin),
                                        comparator)) {
        return PR_TRUE;
      }
      gbegin = gend + 1;
    }
    vbegin = vend + 1;
  }
  return PR_FALSE;
}
#endif

//----------------------------------------------------------------------
//
// When inline frames get weird and have block frames in them, we
// annotate them to help us respond to incremental content changes
// more easily.

static inline PRBool
IsFrameSpecial(nsIFrame* aFrame)
{
  return (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) != 0;
}

static void
GetSpecialSibling(nsFrameManager* aFrameManager, nsIFrame* aFrame, nsIFrame** aResult)
{
  // We only store the "special sibling" annotation with the first
  // frame in the flow. Walk back to find that frame now.
  aFrame = aFrame->GetFirstInFlow();

  void* value = aFrame->GetProperty(nsLayoutAtoms::IBSplitSpecialSibling);

  *aResult = NS_STATIC_CAST(nsIFrame*, value);
}

static nsIFrame*
GetLastSpecialSibling(nsFrameManager* aFrameManager, nsIFrame* aFrame)
{
  for (nsIFrame *frame = aFrame, *next; ; frame = next) {
    GetSpecialSibling(aFrameManager, frame, &next);
    if (!next)
      return frame;
  }
  NS_NOTREACHED("unreachable code");
  return nsnull;
}

// Get the frame's next-in-flow, or, if it doesn't have one,
// its special sibling.
static nsIFrame*
GetNifOrSpecialSibling(nsFrameManager *aFrameManager, nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->GetNextInFlow();
  if (result)
    return result;

  if (IsFrameSpecial(aFrame))
    GetSpecialSibling(aFrameManager, aFrame, &result);
  return result;
}

static void
SetFrameIsSpecial(nsFrameManager* aFrameManager, nsIFrame* aFrame, nsIFrame* aSpecialSibling)
{
  NS_PRECONDITION(aFrameManager && aFrame, "bad args!");

  // Mark the frame and all of its siblings as "special".
  for (nsIFrame* frame = aFrame; frame != nsnull; frame = frame->GetNextInFlow()) {
    frame->AddStateBits(NS_FRAME_IS_SPECIAL);
  }

  if (aSpecialSibling) {
    // We should be the first-in-flow
    NS_ASSERTION(!aFrame->GetPrevInFlow(),
                 "assigning special sibling to other than first-in-flow!");

    // Store the "special sibling" (if we were given one) with the
    // first frame in the flow.
    aFrame->SetProperty(nsLayoutAtoms::IBSplitSpecialSibling, aSpecialSibling);
  }
}

static nsIFrame*
GetIBContainingBlockFor(nsIFrame* aFrame)
{
  NS_PRECONDITION(IsFrameSpecial(aFrame),
                  "GetIBContainingBlockFor() should only be called on known IB frames");

  // Get the first "normal" ancestor of the target frame.
  nsIFrame* parentFrame;
  do {
    parentFrame = aFrame->GetParent();

    if (! parentFrame) {
      NS_ERROR("no unsplit block frame in IB hierarchy");
      return aFrame;
    }

    if (!IsFrameSpecial(parentFrame))
      break;

    aFrame = parentFrame;
  } while (1);
 
  // post-conditions
  NS_ASSERTION(parentFrame, "no normal ancestor found for special frame in GetIBContainingBlockFor");
  NS_ASSERTION(parentFrame != aFrame, "parentFrame is actually the child frame - bogus reslt");

  return parentFrame;
}

//----------------------------------------------------------------------

// XXX this predicate and its cousins need to migrated to a single
// place in layout - something in nsStyleDisplay maybe?
static PRBool
IsInlineFrame(nsIFrame* aFrame)
{
  // XXXwaterson why don't we use |! display->IsBlockLevel()| here?
  switch (aFrame->GetStyleDisplay()->mDisplay) {
    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_INLINE_BLOCK:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
    case NS_STYLE_DISPLAY_INLINE_BOX:
    case NS_STYLE_DISPLAY_INLINE_GRID:
    case NS_STYLE_DISPLAY_INLINE_STACK:
    case NS_STYLE_DISPLAY_DECK:
    case NS_STYLE_DISPLAY_POPUP:
    case NS_STYLE_DISPLAY_GROUPBOX:
      return PR_TRUE;
    default:
      break;
  }
  return PR_FALSE;
}

// NeedSpecialFrameReframe uses this until we decide what to do about IsInlineFrame() above
static PRBool
IsInlineFrame2(nsIFrame* aFrame)
{
  return !aFrame->GetStyleDisplay()->IsBlockLevel();
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
IsBlockFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  // XXXwaterson this seems wrong; see IsInlineFrame() immediately
  // above, which will treat inline-block (e.g.) as an inline. Why
  // don't we use display->IsBlockLevel() here?
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  return NS_STYLE_DISPLAY_INLINE != display->mDisplay;
}

static nsIFrame*
FindFirstBlock(nsPresContext* aPresContext, nsIFrame* aKid, nsIFrame** aPrevKid)
{
  nsIFrame* prevKid = nsnull;
  while (aKid) {
    if (IsBlockFrame(aPresContext, aKid)) {
      *aPrevKid = prevKid;
      return aKid;
    }
    prevKid = aKid;
    aKid = aKid->GetNextSibling();
  }
  *aPrevKid = nsnull;
  return nsnull;
}

static nsIFrame*
FindLastBlock(nsPresContext* aPresContext, nsIFrame* aKid)
{
  nsIFrame* lastBlock = nsnull;
  while (aKid) {
    if (IsBlockFrame(aPresContext, aKid)) {
      lastBlock = aKid;
    }
    aKid = aKid->GetNextSibling();
  }
  return lastBlock;
}

/*
 * Unlike the special (next) sibling, the special previous sibling
 * property points only from the anonymous block to the original
 * inline that preceded it.  DO NOT CHANGE THAT -- the
 * GetParentStyleContextFrame code depends on it!  It is useful for
 * finding the "special parent" of a frame (i.e., a frame from which a
 * good parent style context can be obtained), one looks at the
 * special previous sibling annotation of the real parent of the frame
 * (if the real parent has NS_FRAME_IS_SPECIAL).
 */
inline void
MarkIBSpecialPrevSibling(nsPresContext* aPresContext,
                         nsFrameManager *aFrameManager,
                         nsIFrame *aAnonymousFrame,
                         nsIFrame *aSpecialParent)
{
  aPresContext->PropertyTable()->SetProperty(aAnonymousFrame,
                                      nsLayoutAtoms::IBSplitSpecialPrevSibling,
                                             aSpecialParent, nsnull, nsnull);
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
  // if aChild has siblings, lastChild needs to be the last one
  for (nsIFrame* sib = lastChild->GetNextSibling(); sib;
       sib = sib->GetNextSibling()) {
    lastChild = sib;
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
  aChild->AddStateBits(NS_FRAME_OUT_OF_FLOW);
  nsFrameItems::AddChild(aChild);
}

// Structures used to record the creation of pseudo table frames where 
// the content belongs to some ancestor. 
// PseudoFrames are necessary when the childframe cannot be the direct
// ancestor of the content based parent frame. The amount of necessary pseudo
// frames is limited as the worst case would be table frame nested directly
// into another table frame. So the member structures of nsPseudoFrames can be
// viewed as a ring buffer where you start with the necessary frame type and
// add higher frames as long as necessary to fit into the initial parent frame.
// mLowestType is some sort of stack pointer which shows the start of the
// ringbuffer. The insertion of pseudo frames can happen between every
// two frames so we need to push and pop the pseudo frame data when children
// of a frame are created.
// The colgroup frame is special as it can harbour only col children.
// Once all children of given frame are known, the pseudo frames can be
// processed that means attached to the corresponding parent frames.
// The behaviour is in general described at
// http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
// however there are implementation details that extend the CSS 2.1
// specification:
// 1. every table frame is wrapped in an outer table frame, which is always a
//    pseudo frame.
// 2. the outer table frame will be also created to hold a caption.
// 3. each table cell will have a pseudo inner table cell frame.
// 4. a colgroup frame is created between a column and a table
// 5. a rowgroup frame is created between a row and a table
// A table frame can only have rowgroups or column groups as children.
// A outer table frame can only have one caption and one table frame
// as children.
// Every table even if all table frames are specified will require the
// creation of two types of pseudo frames: the outer table frame and the inner
// table cell frames.

struct nsPseudoFrameData {
  nsIFrame*    mFrame; // created pseudo frame
  nsFrameItems mChildList;  // child frames pending to be added to the pseudo
  nsFrameItems mChildList2; // child frames pending to be added to the pseudo

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
  nsFrameManager           *mFrameManager;

  // Containing block information for out-of-flow frammes
  nsAbsoluteItems           mFixedItems;
  nsAbsoluteItems           mAbsoluteItems;
  nsAbsoluteItems           mFloatedItems;
  PRBool                    mFirstLetterStyle;
  PRBool                    mFirstLineStyle;
  nsCOMPtr<nsILayoutHistoryState> mFrameState;
  nsPseudoFrames            mPseudoFrames;

  // Constructor
  // Use the passed-in history state.
  nsFrameConstructorState(nsPresContext*        aPresContext,
                          nsIFrame*              aFixedContainingBlock,
                          nsIFrame*              aAbsoluteContainingBlock,
                          nsIFrame*              aFloatContainingBlock,
                          nsILayoutHistoryState* aHistoryState);
  // Get the history state from the pres context's pres shell.
  nsFrameConstructorState(nsPresContext*        aPresContext,
                          nsIFrame*              aFixedContainingBlock,
                          nsIFrame*              aAbsoluteContainingBlock,
                          nsIFrame*              aFloatContainingBlock);

  // Function to push the existing absolute containing block state and
  // create a new scope
  void PushAbsoluteContainingBlock(nsPresContext* aPresContext,
                                   nsIFrame* aNewAbsoluteContainingBlock,
                                   nsFrameConstructorSaveState& aSaveState);

  // Function to push the existing float containing block state and
  // create a new scope
  void PushFloatContainingBlock(nsIFrame* aNewFloatContainingBlock,
                                  nsFrameConstructorSaveState& aSaveState,
                                  PRBool aFirstLetterStyle,
                                  PRBool aFirstLineStyle);

  // Function to return the proper geometric parent for a frame with display
  // struct given by aStyleDisplay and parent's frame given by
  // aContentParentFrame.  If the frame is not allowed to be positioned, pass
  // false for aCanBePositioned.
  nsIFrame* GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                               nsIFrame* aContentParentFrame,
                               PRBool aCanBePositioned = PR_TRUE);
};

nsFrameConstructorState::nsFrameConstructorState(nsPresContext*        aPresContext,
                                                 nsIFrame*              aFixedContainingBlock,
                                                 nsIFrame*              aAbsoluteContainingBlock,
                                                 nsIFrame*              aFloatContainingBlock,
                                                 nsILayoutHistoryState* aHistoryState)
  : mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloatContainingBlock),
    mFirstLetterStyle(PR_FALSE),
    mFirstLineStyle(PR_FALSE),
    mFrameState(aHistoryState),
    mPseudoFrames()
{
  mPresShell = aPresContext->PresShell();
  mFrameManager = mPresShell->FrameManager();
}

nsFrameConstructorState::nsFrameConstructorState(nsPresContext*        aPresContext,
                                                 nsIFrame*              aFixedContainingBlock,
                                                 nsIFrame*              aAbsoluteContainingBlock,
                                                 nsIFrame*              aFloatContainingBlock)
  : mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloatContainingBlock),
    mFirstLetterStyle(PR_FALSE),
    mFirstLineStyle(PR_FALSE),
    mPseudoFrames()
{
  mPresShell = aPresContext->PresShell();
  mFrameManager = mPresShell->FrameManager();
  nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) 
    docShell->GetLayoutHistoryState(getter_AddRefs(mFrameState));
}

// Use the first-in-flow of a positioned inline frame in galley mode as the 
// containing block. We don't need to do this for a block, since blocks aren't 
// continued in galley mode.
static nsIFrame*
AdjustAbsoluteContainingBlock(nsPresContext* aPresContext,
                              nsIFrame*       aContainingBlockIn)
{
  nsIFrame* containingBlock = aContainingBlockIn;
  if (!aPresContext->IsPaginated()) {
    if (nsLayoutAtoms::positionedInlineFrame == containingBlock->GetType()) {
      containingBlock = ((nsPositionedInlineFrame*)containingBlock)->GetFirstInFlow();
    }
  }
  return containingBlock;
}

void
nsFrameConstructorState::PushAbsoluteContainingBlock(nsPresContext* aPresContext,
                                                     nsIFrame* aNewAbsoluteContainingBlock,
                                                     nsFrameConstructorSaveState& aSaveState)
{
  aSaveState.mItems = &mAbsoluteItems;
  aSaveState.mSavedItems = mAbsoluteItems;
  mAbsoluteItems = 
    nsAbsoluteItems(AdjustAbsoluteContainingBlock(aPresContext, aNewAbsoluteContainingBlock));
}

void
nsFrameConstructorState::PushFloatContainingBlock(nsIFrame* aNewFloatContainingBlock,
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
  mFloatedItems = nsAbsoluteItems(aNewFloatContainingBlock);
  mFirstLetterStyle = aFirstLetterStyle;
  mFirstLineStyle = aFirstLineStyle;
}

nsIFrame*
nsFrameConstructorState::GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                                            nsIFrame* aContentParentFrame,
                                            PRBool aCanBePositioned)
{
  NS_PRECONDITION(aStyleDisplay, "Must have display struct!");

  // If there is no container for a fixed, absolute, or floating root
  // frame, we will ignore the positioning.  This hack is originally
  // brought to you by the letter T: tables, since other roots don't
  // even call into this code.  See bug 178855.
  //
  // XXX Disabling positioning in this case is a hack.  If one was so inclined,
  // one could support this either by (1) inserting a dummy block between the
  // table and the canvas or (2) teaching the canvas how to reflow positioned
  // elements. (1) has the usual problems when multiple frames share the same
  // content (notice all the special cases in this file dealing with inner
  // tables and outer tables which share the same content). (2) requires some
  // work and possible factoring.
  //
  // XXXbz couldn't we just force position to "static" on roots and
  // float to "none"?  That's OK per CSS 2.1, as far as I can tell.
  
  if (aStyleDisplay->IsFloating() && mFloatedItems.containingBlock) {
    NS_ASSERTION(!aStyleDisplay->IsAbsolutelyPositioned(),
                 "Absolutely positioned _and_ floating?");
    return mFloatedItems.containingBlock;
  }
  if (aCanBePositioned) {
    if (aStyleDisplay->mPosition == NS_STYLE_POSITION_ABSOLUTE &&
        mAbsoluteItems.containingBlock) {
      return mAbsoluteItems.containingBlock;
    }
    if (aStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED &&
        mFixedItems.containingBlock) {
      return mFixedItems.containingBlock;
    }
  }
  return aContentParentFrame;
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


static 
PRBool IsBorderCollapse(nsIFrame* aFrame)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (nsLayoutAtoms::tableFrame == frame->GetType()) {
      return ((nsTableFrame*)frame)->IsBorderCollapse();
    }
  }
  NS_ASSERTION(PR_FALSE, "program error");
  return PR_FALSE;
}

// a helper routine that automatically navigates placeholders.
static nsIFrame*
GetRealFrame(nsIFrame* aFrame)
{
  nsIFrame* result = aFrame;

  // We may be a placeholder.  If we are, go to the real frame.
  // See if it's a placeholder frame for a float.
  PRBool isPlaceholder = (nsLayoutAtoms::placeholderFrame == aFrame->GetType());
  if (isPlaceholder) {
    // Get the out-of-flow frame that the placeholder points to.
    // This is the real float that we should examine.
    result = NS_STATIC_CAST(nsPlaceholderFrame*,aFrame)->GetOutOfFlowFrame();
    NS_ASSERTION(result, "No out of flow frame found for placeholder!\n");
  }
  
  return result;
}

/**
 * Utility method, called from MoveChildrenTo(), that recursively
 * descends down the frame hierarchy looking for out-of-flow frames that
 * need parent pointer adjustments to account for the containment block
 * changes that could occur as the result of the reparenting done in
 * MoveChildrenTo().
 */
static void
AdjustOutOfFlowFrameParentPtrs(nsPresContext*          aPresContext,
                               nsIFrame*                aFrame,
                               nsFrameConstructorState* aState)
{
  nsIFrame *outOfFlowFrame = GetRealFrame(aFrame);

  if (outOfFlowFrame && outOfFlowFrame != aFrame) {

    // Get the display data for the outOfFlowFrame so we can
    // figure out if it is a float or absolutely positioned element.

    const nsStyleDisplay* display = outOfFlowFrame->GetStyleDisplay();

    // Update the parent pointer for outOfFlowFrame if it's
    // containing block has changed as the result of reparenting,

    if (NS_STYLE_POSITION_ABSOLUTE == display->mPosition) {
      // XXX_kin: I think we'll need to add code here to handle the
      // XXX_kin: reparenting that can happen in ConstructInline()?
      // XXX_kin:
      // XXX_kin: The case I'm thinking about here is when the inline being
      // XXX_kin: constructed has a style="position: relative;" property
      // XXX_kin: on it, and ConstructInline() moves/reparents all child block
      // XXX_kin: frames and any inlines (including placeholders) that happen
      // XXX_kin: to be between these blocks, under the new inline-block it created.
      // XXX_kin: I think right now this case generates an assertion during
      // XXX_kin: reflow, and as a result things fail to render since I believe
      // XXX_kin: the placeholder is parented to the inline-block, and the
      // XXX_kin: outOfFlowFrame is in the original inline frame's absolute list,
      // XXX_kin: and is also parented to it.
    }
    else if (NS_STYLE_FLOAT_NONE != display->mFloats) {
      nsIFrame *parent = aState->mFloatedItems.containingBlock;
      outOfFlowFrame->SetParent(parent);
      if (outOfFlowFrame->GetStateBits() &
          (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW)) {
        // We don't need to walk up the tree, since each level of
        // recursion of the SplitToContainingBlock will propagate the
        // bit.
        parent->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
      }
    }

    // XXX_kin: We'll need to remove the return below when we support
    // XXX_kin: descending into out-of-flow frames. Here are some notes
    // XXX_kin: from waterson:
    // XXX_kin:
    // XXX_kin:     We'd want to continue to descend into placeholders
    // XXX_kin:     until we found an out-of-flow frame that established
    // XXX_kin:     a new containing block for absolutely positioned
    // XXX_kin:     elements. At that point, we could terminate the descent,
    // XXX_kin:     because any absolutely positioned frames below that frame
    // XXX_kin:     would be properly parented.

    return;
  }

  // XXX_kin: Since we're only handling floats at the moment,
  // XXX_kin: we don't need to cross block boundaries.

  if (IsBlockFrame(aPresContext, aFrame))
    return;

  // Dive down into children to see if any of their
  // placeholders need adjusting.

  nsIFrame *childFrame = aFrame->GetFirstChild(nsnull);

  while (childFrame)
  {
    // XXX_kin: Once we add support for adjusting absolutely positioned
    // XXX_kin: frames, we will be crossing block boundaries, we/ll need
    // XXX_kin: to update aState's containingBlock info to avoid incorrectly
    // XXX_kin: reparenting floats, etc.
    // XXX_kin:
    // XXX_kin: Do we need to prevent descent into anonymous content here?

    AdjustOutOfFlowFrameParentPtrs(aPresContext, childFrame, aState);
    childFrame = childFrame->GetNextSibling();
  }
}

/**
 * Moves frames to a new parent, updating the style context and
 * propagating relevant frame state bits. |aNewParentSC| may be null,
 * in which case the child frames' style contexts will remain
 * untouched. |aState| may be null, in which case the parent
 * pointers of out-of-flow frames will remain untouched.
 */
static void
MoveChildrenTo(nsPresContext*          aPresContext,
               nsStyleContext*          aNewParentSC,
               nsIFrame*                aNewParent,
               nsIFrame*                aFrameList,
               nsFrameConstructorState* aState)
{
  PRBool setHasChildWithView = PR_FALSE;

  while (aFrameList) {
    if (!setHasChildWithView
        && (aFrameList->GetStateBits() & (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW))) {
      setHasChildWithView = PR_TRUE;
    }

    aFrameList->SetParent(aNewParent);

    // If aState is not null, the caller expects us to make adjustments
    // so that placeholder out of flow frames point to the correct parent.

    if (aState)
      AdjustOutOfFlowFrameParentPtrs(aPresContext, aFrameList, aState);

#if 0
    // XXX When this is used with {ib} frame hierarchies, it seems
    // fine to leave the style contexts of the children of the
    // anonymous block frame parented by the original inline
    // frame. (In fact, one would expect some inheritance
    // relationships to be broken if we reparented them to the
    // anonymous block frame, but oddly they aren't -- need to
    // investigate that...)
    if (aNewParentSC)
      aPresContext->FrameManager()->ReParentStyleContext(aFrameList,
                                                         aNewParentSC);
#endif

    aFrameList = aFrameList->GetNextSibling();
  }

  if (setHasChildWithView) {
    aNewParent->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
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
  virtual nsresult CreateTableCellFrame(nsIFrame* aParentFrame, nsIFrame** aNewFrame);
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
nsTableCreator::CreateTableCellFrame(nsIFrame*  aParentFrame,
                                     nsIFrame** aNewFrame) {
  return NS_NewTableCellFrame(mPresShell, IsBorderCollapse(aParentFrame), aNewFrame);
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
  virtual nsresult CreateTableFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableRowFrame(nsIFrame** aNewFrame);
  virtual nsresult CreateTableCellFrame(nsIFrame* aParentFrame, nsIFrame** aNewFrame);
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
nsMathMLmtableCreator::CreateTableFrame(nsIFrame** aNewFrame)
{
  return NS_NewMathMLmtableFrame(mPresShell, aNewFrame);
}

nsresult
nsMathMLmtableCreator::CreateTableRowFrame(nsIFrame** aNewFrame)
{
  return NS_NewMathMLmtrFrame(mPresShell, aNewFrame);
}

nsresult
nsMathMLmtableCreator::CreateTableCellFrame(nsIFrame*  aParentFrame,
                                            nsIFrame** aNewFrame)
{
  NS_ASSERTION(!IsBorderCollapse(aParentFrame), "not implemented");
  return NS_NewMathMLmtdFrame(mPresShell, aNewFrame);
}

nsresult
nsMathMLmtableCreator::CreateTableCellInnerFrame(nsIFrame** aNewFrame)
{
  // only works if aNewFrame can take care of the lineLayout logic
  return NS_NewMathMLmtdInnerFrame(mPresShell, aNewFrame);
}
#endif // MOZ_MATHML

// Structure used to ensure that bindings are properly enqueued in the
// binding manager's attached queue.
struct nsAutoEnqueueBinding
{
  nsAutoEnqueueBinding(nsIDocument* aDocument) :
    mDocument(aDocument)
  {}

  ~nsAutoEnqueueBinding();

  nsCOMPtr<nsIXBLBinding> mBinding;
private:
  nsIDocument* mDocument;
};

nsAutoEnqueueBinding::~nsAutoEnqueueBinding()
{
  if (mBinding) {
    nsIBindingManager *bm = mDocument->GetBindingManager();
    if (bm) {
      bm->AddToAttachedQueue(mBinding);
    }
  }
}

/**
 * If the parent frame is a |tableFrame| and the child is a
 * |captionFrame|, then we want to insert the frames beneath the
 * |tableFrame|'s parent frame. Returns |PR_TRUE| if the parent frame
 * needed to be fixed up.
 */
static PRBool
GetCaptionAdjustedParent(nsIFrame*        aParentFrame,
                         const nsIFrame*  aChildFrame,
                         nsIFrame**       aAdjParentFrame)
{
  *aAdjParentFrame = aParentFrame;
  PRBool haveCaption = PR_FALSE;

  if (nsLayoutAtoms::tableCaptionFrame == aChildFrame->GetType()) {
    haveCaption = PR_TRUE;
    if (nsLayoutAtoms::tableFrame == aParentFrame->GetType()) {
      *aAdjParentFrame = aParentFrame->GetParent();
    }
  }
  return haveCaption;
}

// Helper function that determines the child list name that aChildFrame
// is contained in
static void
GetChildListNameFor(nsPresContext* aPresContext,
                    nsIFrame*       aParentFrame,
                    nsIFrame*       aChildFrame,
                    nsIAtom**       aListName)
{
  nsIAtom*      listName;
  
  // See if the frame is moved out of the flow
  if (aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStyleDisplay* disp = aChildFrame->GetStyleDisplay();
    
    if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
      listName = nsLayoutAtoms::absoluteList;
    } else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
      listName = nsLayoutAtoms::fixedList;
    } else {
      NS_ASSERTION(aChildFrame->GetStyleDisplay()->IsFloating(),
                   "not a floated frame");
      listName = nsLayoutAtoms::floatList;
    }

  } else {
    listName = nsnull;
  }

  // Verify that the frame is actually in that child list
  NS_ASSERTION(nsFrameList(aParentFrame->GetFirstChild(listName))
               .ContainsFrame(aChildFrame), "not in child list");

  NS_IF_ADDREF(listName);
  *aListName = listName;
}

//----------------------------------------------------------------------

nsCSSFrameConstructor::nsCSSFrameConstructor(nsIDocument *aDocument)
  : mDocument(aDocument)
  , mInitialContainingBlock(nsnull)
  , mFixedContainingBlock(nsnull)
  , mDocElementContainingBlock(nsnull)
  , mGfxScrollFrame(nsnull)
  , mUpdateCount(0)
  , mQuotesDirty(PR_FALSE)
{
  if (!gGotXBLFormPrefs) {
    gGotXBLFormPrefs = PR_TRUE;

    gUseXBLForms =
      nsContentUtils::GetBoolPref("nglayout.debug.enable_xbl_forms");
  }

  // XXXbz this should be in Init() or something!
  if (!mPendingRestyles.Init()) {
    // now what?
  }

  // XXXbz this should be in Init() or something!
  mEventQueueService = do_GetService(kEventQueueServiceCID);
  
#ifdef DEBUG
  static PRBool gFirstTime = PR_TRUE;
  if (gFirstTime) {
    gFirstTime = PR_FALSE;
    char* flags = PR_GetEnv("GECKO_FRAMECTOR_DEBUG_FLAGS");
    if (flags) {
      PRBool error = PR_FALSE;
      for (;;) {
        char* comma = PL_strchr(flags, ',');
        if (comma)
          *comma = '\0';

        PRBool found = PR_FALSE;
        FrameCtorDebugFlags* flag = gFlags;
        FrameCtorDebugFlags* limit = gFlags + NUM_DEBUG_FLAGS;
        while (flag < limit) {
          if (PL_strcasecmp(flag->name, flags) == 0) {
            *(flag->on) = PR_TRUE;
            printf("nsCSSFrameConstructor: setting %s debug flag on\n", flag->name);
            found = PR_TRUE;
            break;
          }
          ++flag;
        }

        if (! found)
          error = PR_TRUE;

        if (! comma)
          break;

        *comma = ',';
        flags = comma + 1;
      }

      if (error) {
        printf("Here are the available GECKO_FRAMECTOR_DEBUG_FLAGS:\n");
        FrameCtorDebugFlags* flag = gFlags;
        FrameCtorDebugFlags* limit = gFlags + NUM_DEBUG_FLAGS;
        while (flag < limit) {
          printf("  %s\n", flag->name);
          ++flag;
        }
        printf("Note: GECKO_FRAMECTOR_DEBUG_FLAGS is a comma separated list of flag\n");
        printf("names (no whitespace)\n");
      }
    }
  }
#endif
}

nsIXBLService * nsCSSFrameConstructor::GetXBLService()
{
  if (!gXBLService) {
    nsresult rv = CallGetService("@mozilla.org/xbl;1", &gXBLService);
    if (NS_FAILED(rv))
      gXBLService = nsnull;
  }
  
  return gXBLService;
}

void
nsCSSFrameConstructor::GeneratedContentFrameRemoved(nsIFrame* aFrame)
{
  if (mQuoteList.DestroyNodesFor(aFrame))
    QuotesDirty();
}

nsresult
nsCSSFrameConstructor::CreateGeneratedFrameFor(nsPresContext*       aPresContext,
                                               nsIDocument*          aDocument,
                                               nsIFrame*             aParentFrame,
                                               nsIContent*           aContent,
                                               nsStyleContext*       aStyleContext,
                                               const nsStyleContent* aStyleContent,
                                               PRUint32              aContentIndex,
                                               nsIFrame**            aFrame)
{
  *aFrame = nsnull;  // initialize OUT parameter

  // The QuoteList needs the content attached to the frame.
  nsCOMPtr<nsIDOMCharacterData>* textPtr = nsnull;

  // Get the content value
  const nsStyleContentData &data = aStyleContent->ContentAt(aContentIndex);
  nsStyleContentType  type = data.mType;

  nsIPresShell *shell = aPresContext->PresShell();

  nsCOMPtr<nsIContent> content;

  if (eStyleContentType_Image == type) {
    if (!data.mContent.mImage) {
      // CSS had something specified that couldn't be converted to an
      // image object
      *aFrame = nsnull;
      return NS_ERROR_FAILURE;
    }
    
    // Create an image content object and pass it the image request.
    // XXX Check if it's an image type we can handle...

    nsCOMPtr<nsINodeInfo> nodeInfo;
    aDocument->NodeInfoManager()->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                              kNameSpaceID_None,
                                              getter_AddRefs(nodeInfo));

    nsresult rv = NS_NewGenConImageContent(getter_AddRefs(content), nodeInfo,
                                           data.mContent.mImage);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set aContent as the parent content and set the document object. This
    // way event handling works
    content->SetParent(aContent);
    content->SetDocument(aDocument, PR_TRUE, PR_TRUE);
    content->SetNativeAnonymous(PR_TRUE);
    // hack to make document rules not match (not like it matters, since we
    // already have a non-element style context... which is totally wacky, but
    // anyway).
    content->SetBindingParent(content);
  
    // Create an image frame and initialize it
    nsIFrame* imageFrame = nsnull;
    rv = NS_NewImageFrame(shell, &imageFrame);
    if (!imageFrame) {
      return rv;
    }

    rv = imageFrame->Init(aPresContext, content, aParentFrame, aStyleContext, nsnull);
    if (NS_FAILED(rv)) {
      imageFrame->Destroy(aPresContext);
      return rv == NS_ERROR_FRAME_REPLACED ? NS_OK : rv;
    }

    // Return the image frame
    *aFrame = imageFrame;

  } else {

    nsAutoString contentString(data.mContent.mString);

    switch (type) {
    case eStyleContentType_String:
      break;
  
    case eStyleContentType_Attr:
      {  
        nsCOMPtr<nsIAtom> attrName;
        PRInt32 attrNameSpace = kNameSpaceID_None;
        PRInt32 barIndex = contentString.FindChar('|'); // CSS namespace delimiter
        if (-1 != barIndex) {
          nsAutoString  nameSpaceVal;
          contentString.Left(nameSpaceVal, barIndex);
          PRInt32 error;
          attrNameSpace = nameSpaceVal.ToInteger(&error, 10);
          contentString.Cut(0, barIndex + 1);
          if (contentString.Length()) {
            attrName = do_GetAtom(contentString);
          }
        }
        else {
          attrName = do_GetAtom(contentString);
        }

        if (!attrName) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        // Creates the content and frame and return if successful
        nsresult rv = NS_ERROR_FAILURE;
        if (attrName) {
          nsIFrame*   textFrame = nsnull;
          rv = NS_NewAttributeContent(aContent, attrNameSpace, attrName,
                                      getter_AddRefs(content));
          NS_ENSURE_SUCCESS(rv, rv);

          // Set aContent as the parent content so that event handling works.
          content->SetParent(aContent);
          content->SetDocument(aDocument, PR_TRUE, PR_TRUE);
          content->SetNativeAnonymous(PR_TRUE);
          content->SetBindingParent(content);

          // Create a text frame and initialize it
          NS_NewTextFrame(shell, &textFrame);
          textFrame->Init(aPresContext, content, aParentFrame, aStyleContext,
                          nsnull);

          // Return the text frame
          *aFrame = textFrame;
          rv = NS_OK;
        }
      }
      break;
  
    case eStyleContentType_Counter:
    case eStyleContentType_Counters:
      return NS_ERROR_NOT_IMPLEMENTED;  // XXX not supported yet...

    case eStyleContentType_Image:
      NS_NOTREACHED("handled by if above");
      return NS_ERROR_UNEXPECTED;
  
    case eStyleContentType_OpenQuote:
    case eStyleContentType_CloseQuote:
    case eStyleContentType_NoOpenQuote:
    case eStyleContentType_NoCloseQuote:
      {
        nsQuoteListNode* node =
          new nsQuoteListNode(type, aParentFrame, aContentIndex);
        if (!node)
          return NS_ERROR_OUT_OF_MEMORY;
        mQuoteList.Insert(node);
        if (!mQuoteList.IsLast(node))
          QuotesDirty();

        // Don't generate a text node or any text for 'no-open-quote' and
        // 'no-close-quote'.
        if (node->IsHiddenQuote())
          return NS_OK;

        textPtr = &node->mText; // Delayed storage of text node.
        contentString = *node->Text();
      }
      break;
  
    } // switch
  

    if (!content) {
      // Create a text content node
      nsIFrame* textFrame = nsnull;
      nsCOMPtr<nsITextContent> textContent;
      NS_NewTextNode(getter_AddRefs(textContent));
      if (textContent) {
        // Set the text
        textContent->SetText(contentString, PR_TRUE);

        if (textPtr)
          *textPtr = do_QueryInterface(textContent);

        // Set aContent as the parent content so that event handling works.
        textContent->SetParent(aContent);
        textContent->SetDocument(aDocument, PR_TRUE, PR_TRUE);
        textContent->SetNativeAnonymous(PR_TRUE);
        textContent->SetBindingParent(textContent);

        // Create a text frame and initialize it
        NS_NewTextFrame(shell, &textFrame);
        if (!textFrame) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        textFrame->Init(aPresContext, textContent, aParentFrame, aStyleContext, nsnull);

        content = textContent;
      }

      // Return the text frame
      *aFrame = textFrame;
    }
  }

  if (content) {
    nsCOMPtr<nsISupportsArray> anonymousItems;
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(anonymousItems));
    NS_ENSURE_SUCCESS(rv, rv);

    anonymousItems->AppendElement(content);

    shell->SetAnonymousContentFor(aContent, anonymousItems);
  }

  return NS_OK;
}

/*
 *
 * aFrame - the frame that should be the parent of the generated
 *   content.  This is the frame for the corresponding content node,
 *   which must not be a leaf frame.
 */
PRBool
nsCSSFrameConstructor::CreateGeneratedContentFrame(nsIPresShell*        aPresShell, 
                                                   nsPresContext*  aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIFrame*        aFrame,
                                                   nsIContent*      aContent,
                                                   nsStyleContext*  aStyleContext,
                                                   nsIAtom*         aPseudoElement,
                                                   nsIFrame**       aResult)
{
  *aResult = nsnull; // initialize OUT parameter

  if (!aContent->IsContentOfType(nsIContent::eELEMENT))
    return PR_FALSE;

  nsStyleSet *styleSet = aPresShell->StyleSet();

  // Probe for the existence of the pseudo-element
  nsRefPtr<nsStyleContext> pseudoStyleContext;
  pseudoStyleContext = styleSet->ProbePseudoStyleFor(aContent,
                                                     aPseudoElement,
                                                     aStyleContext);

  if (pseudoStyleContext) {
    // |ProbePseudoStyleContext| checks the 'display' property and the
    // |ContentCount()| of the 'content' property for us.

    // Create a block box or an inline box depending on the value of
    // the 'display' property
    nsIFrame*     containerFrame;
    nsFrameItems  childFrames;

    if (NS_STYLE_DISPLAY_BLOCK ==
        pseudoStyleContext->GetStyleDisplay()->mDisplay) {
      NS_NewBlockFrame(aPresShell, &containerFrame);
    } else {
      NS_NewInlineFrame(aPresShell, &containerFrame);
    }        
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aFrame, pseudoStyleContext, nsnull, containerFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(containerFrame, nsnull, PR_FALSE);

    // Mark the frame as being associated with generated content
    containerFrame->AddStateBits(NS_FRAME_GENERATED_CONTENT);

    // Create another pseudo style context to use for all the generated child
    // frames
    nsRefPtr<nsStyleContext> textStyleContext;
    textStyleContext = styleSet->ResolveStyleForNonElement(pseudoStyleContext);

    // Now create content objects (and child frames) for each value of the
    // 'content' property

    const nsStyleContent* styleContent = pseudoStyleContext->GetStyleContent();
    PRUint32 contentCount = styleContent->ContentCount();
    for (PRUint32 contentIndex = 0; contentIndex < contentCount; contentIndex++) {
      nsIFrame* frame;

      // Create a frame
      nsresult result;
      result = CreateGeneratedFrameFor(aPresContext, mDocument, containerFrame,
                                       aContent, textStyleContext,
                                       styleContent, contentIndex, &frame);
      // Non-elements can't possibly have a view, so don't bother checking
      if (NS_SUCCEEDED(result) && frame) {
        // Add it to the list of child frames
        childFrames.AddChild(frame);
      }
    }

    if (childFrames.childList) {
      containerFrame->SetInitialChildList(aPresContext, nsnull, childFrames.childList);
    }
    *aResult = containerFrame;
    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
nsCSSFrameConstructor::CreateInputFrame(nsIPresShell    *aPresShell,
                                        nsPresContext  *aPresContext,
                                        nsIContent      *aContent,
                                        nsIFrame        *&aFrame,
                                        nsStyleContext  *aStyleContext)
{
  nsCOMPtr<nsIFormControl> control = do_QueryInterface(aContent);
  NS_ASSERTION(control, "input is not an nsIFormControl!");
  
  switch (control->GetType()) {
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_BUTTON:
      if (gUseXBLForms)
        return NS_OK;
      return NS_NewGfxButtonControlFrame(aPresShell, &aFrame);

    case NS_FORM_INPUT_CHECKBOX:
      if (gUseXBLForms)
        return NS_OK;
      return ConstructCheckboxControlFrame(aPresShell, aPresContext, aFrame, aContent, aStyleContext);

    case NS_FORM_INPUT_RADIO:
      if (gUseXBLForms)
        return NS_OK;
      return ConstructRadioControlFrame(aPresShell, aPresContext, aFrame, aContent, aStyleContext);

    case NS_FORM_INPUT_FILE:
    {
      nsresult rv = NS_NewFileControlFrame(aPresShell, &aFrame);
      if (NS_SUCCEEDED(rv)) {
        // The (block-like) file control frame should have a space manager
        aFrame->AddStateBits(NS_BLOCK_SPACE_MGR);
      }
      return rv;
    }

    case NS_FORM_INPUT_HIDDEN:
      return NS_OK;

    case NS_FORM_INPUT_IMAGE:
      return NS_NewImageControlFrame(aPresShell, &aFrame);

    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
      return NS_NewTextControlFrame(aPresShell, &aFrame);

    default:
      NS_ASSERTION(0, "Unknown input type!");
      return NS_ERROR_INVALID_ARG;
  }
}

static PRBool
IsOnlyWhitespace(nsIContent* aContent)
{
  PRBool onlyWhiteSpace = PR_FALSE;
  if (aContent->IsContentOfType(nsIContent::eTEXT)) {
    nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aContent);

    onlyWhiteSpace = textContent->IsOnlyWhitespace();
  }

  return onlyWhiteSpace;
}
    
/****************************************************
 **  BEGIN TABLE SECTION
 ****************************************************/

// The term pseudo frame is being used instead of anonymous frame, since anonymous
// frame has been used elsewhere to refer to frames that have generated content

// aIncludeSpecial applies to captions, col groups, cols and cells.
// These do not generate pseudo frame wrappers for foreign children. 

static PRBool
IsTableRelated(PRUint8 aDisplay,
               PRBool  aIncludeSpecial) 
{
  if ((aDisplay == NS_STYLE_DISPLAY_TABLE)              ||
      (aDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP) ||
      (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP)    ||
      (aDisplay == NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP) ||
      (aDisplay == NS_STYLE_DISPLAY_TABLE_ROW)) {
    return PR_TRUE;
  }
  else if (aIncludeSpecial && 
           ((aDisplay == NS_STYLE_DISPLAY_TABLE_CAPTION)      ||
            (aDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP) ||
            (aDisplay == NS_STYLE_DISPLAY_TABLE_COLUMN)       ||
            (aDisplay == NS_STYLE_DISPLAY_TABLE_CELL))) {
    return PR_TRUE;
  }
  else return PR_FALSE;
}

static PRBool
IsTableRelated(nsIAtom* aParentType,
               PRBool   aIncludeSpecial)
{
  if ((nsLayoutAtoms::tableFrame         == aParentType)  ||
      (nsLayoutAtoms::tableRowGroupFrame == aParentType)  ||
      (nsLayoutAtoms::tableRowFrame      == aParentType)) {
    return PR_TRUE;
  }
  else if (aIncludeSpecial && 
           ((nsLayoutAtoms::tableCaptionFrame  == aParentType)  ||
            (nsLayoutAtoms::tableColGroupFrame == aParentType)  ||
            (nsLayoutAtoms::tableColFrame      == aParentType)  ||
            IS_TABLE_CELL(aParentType))) {
    return PR_TRUE;
  }
  else return PR_FALSE;
}
           
static nsIFrame*
GetOuterTableFrame(nsIFrame* aParentFrame) 
{
  if (nsLayoutAtoms::tableOuterFrame == aParentFrame->GetType()) {
    return aParentFrame;
  }
  return aParentFrame->GetParent();
}
    
static nsresult 
ProcessPseudoFrame(nsPresContext*    aPresContext,
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

static nsresult 
ProcessPseudoTableFrame(nsPresContext* aPresContext,
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

static nsresult 
ProcessPseudoCellFrame(nsPresContext* aPresContext,
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

// limit the processing up to the frame type indicated by aHighestType.
// make a complete processing when aHighestType is null
static nsresult 
ProcessPseudoFrames(nsPresContext* aPresContext,
                    nsPseudoFrames& aPseudoFrames,
                    nsIAtom*        aHighestType,
                    nsIFrame*&      aHighestFrame)
{
  nsresult rv = NS_OK;
  if (!aPresContext) return rv;

  aHighestFrame = nsnull;

  if (nsLayoutAtoms::tableFrame == aPseudoFrames.mLowestType) {
    // if the processing should be limited to the colgroup frame and the
    // table frame is the lowest type of created pseudo frames that
    // can have pseudo frame children, process only the colgroup pseudo frames
    // and leave the table frame untouched.
    if (nsLayoutAtoms::tableColGroupFrame == aHighestType) {
      if (aPseudoFrames.mColGroup.mFrame) {
        rv = ProcessPseudoFrame(aPresContext, aPseudoFrames.mColGroup, aHighestFrame);
      }
      return rv;
    }
    rv = ProcessPseudoTableFrame(aPresContext, aPseudoFrames, aHighestFrame);
    if (nsLayoutAtoms::tableOuterFrame == aHighestType) return rv;
    
    if (aPseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
      if (IS_TABLE_CELL(aHighestType)) return rv;
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
      if (IS_TABLE_CELL(aHighestType)) return rv;
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
      if (IS_TABLE_CELL(aHighestType)) return rv;
    }
  }
  else if (IS_TABLE_CELL(aPseudoFrames.mLowestType)) {
    rv = ProcessPseudoCellFrame(aPresContext, aPseudoFrames, aHighestFrame);
    if (IS_TABLE_CELL(aHighestType)) return rv;

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

static nsresult 
ProcessPseudoFrames(nsPresContext* aPresContext,
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

static nsresult 
ProcessPseudoFrames(nsPresContext* aPresContext,
                    nsPseudoFrames& aPseudoFrames,
                    nsIAtom*        aHighestType)
{
  nsIFrame* highestFrame;
  nsresult rv = ProcessPseudoFrames(aPresContext, aPseudoFrames, aHighestType, highestFrame);
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoTableFrame(nsIPresShell*            aPresShell,
                                              nsPresContext*          aPresContext,
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mCellInner.mFrame) 
                          ? aState.mPseudoFrames.mCellInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext(); 
  nsIContent* parentContent = parentFrame->GetContent();   

  // create the SC for the inner table which will be the parent of the outer table's SC
  childStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::table,
                                                             parentStyle);

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mTableOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mTableInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableFrame(aPresShell, aPresContext, aState, parentContent,
                           parentFrame, parentFrame, childStyle, aTableCreator,
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
                                                 nsPresContext*          aPresContext,
                                                 nsTableCreator&          aTableCreator,
                                                 nsFrameConstructorState& aState, 
                                                 nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mTableInner.mFrame) 
                          ? aState.mPseudoFrames.mTableInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext();
  nsIContent* parentContent = parentFrame->GetContent();

  childStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableRowGroup, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRowGroup;

  // construct the pseudo row group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, parentContent,
                                   parentFrame, childStyle, aTableCreator,
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
                                                 nsPresContext*          aPresContext,
                                                 nsTableCreator&          aTableCreator,
                                                 nsFrameConstructorState& aState, 
                                                 nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mTableInner.mFrame) 
                          ? aState.mPseudoFrames.mTableInner.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext();
  nsIContent* parentContent = parentFrame->GetContent();

  childStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableColGroup, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mColGroup;

  // construct the pseudo col group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, parentContent,
                                   parentFrame, childStyle, aTableCreator,
                                   PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;
  ((nsTableColGroupFrame*)pseudo.mFrame)->SetColType(eColGroupAnonymousCol);

  // Do not set  aState.mPseudoFrames.mLowestType here as colgroup frame will
  // be always below a table frame but we can not descent any further as col
  // frames can not have children and will not wrap table foreign frames.

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mTableInner.mFrame) {
    aState.mPseudoFrames.mTableInner.mChildList.AddChild(pseudo.mFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoRowFrame(nsIPresShell*            aPresShell,
                                            nsPresContext*          aPresContext,
                                            nsTableCreator&          aTableCreator,
                                            nsFrameConstructorState& aState, 
                                            nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mRowGroup.mFrame) 
                          ? aState.mPseudoFrames.mRowGroup.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext();
  nsIContent* parentContent = parentFrame->GetContent();

  childStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableRow, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRow;

  // construct the pseudo row as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, parentContent,
                              parentFrame, childStyle, aTableCreator,
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
                                             nsPresContext*          aPresContext,
                                             nsTableCreator&          aTableCreator,
                                             nsFrameConstructorState& aState, 
                                             nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = (aState.mPseudoFrames.mRow.mFrame) 
                          ? aState.mPseudoFrames.mRow.mFrame : aParentFrameIn;
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext();
  nsIContent* parentContent = parentFrame->GetContent();

  childStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableCell, 
                                                             parentStyle);

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mCellOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mCellInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, parentContent,
                               parentFrame, childStyle, aTableCreator,
                               PR_TRUE, items, pseudoOuter.mFrame, 
                               pseudoInner.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  pseudoOuter.mChildList.AddChild(pseudoInner.mFrame);
  // give it nsLayoutAtoms::tableCellFrame, if it is really nsLayoutAtoms::bcTableCellFrame, it will match later
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
                                           nsPresContext*          aPresContext, 
                                           nsTableCreator&          aTableCreator,
                                           nsFrameConstructorState& aState, 
                                           nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowGroupFrame == parentFrameType) { // row group parent
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowFrame == parentFrameType)) { // row parent
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
                                              nsPresContext*          aPresContext, 
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowGroupFrame == parentFrameType) {  // row group parent
      rv = CreatePseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowFrame == parentFrameType)) { // row parent
      rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || IS_TABLE_CELL(parentFrameType) || // cell parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
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
                                              nsPresContext*          aPresContext, 
                                              nsTableCreator&          aTableCreator,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableRowFrame == parentFrameType) {  // row parent
      rv = CreatePseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || IS_TABLE_CELL(parentFrameType) || // cell parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
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
                                         nsPresContext*          aPresContext, 
                                         nsTableCreator&          aTableCreator,
                                         nsFrameConstructorState& aState, 
                                         nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (IS_TABLE_CELL(parentFrameType) || // cell parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
      rv = CreatePseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableFrame == parentFrameType)) { // table parent
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
                                          nsPresContext*          aPresContext, 
                                          nsTableCreator&          aTableCreator,
                                          nsFrameConstructorState& aState, 
                                          nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsLayoutAtoms::tableFrame == parentFrameType) { // table parent
      rv = CreatePseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsLayoutAtoms::tableRowGroupFrame == parentFrameType)) { // row group parent
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
                                      nsPresContext*          aPresContext,
                                      nsTableCreator&          aTableCreator,
                                      nsIFrame&                aParentFrameIn, 
                                      nsIAtom*                 aChildFrameType, 
                                      nsFrameConstructorState& aState, 
                                      nsIFrame*&               aParentFrame,
                                      PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext) return rv;

  nsIAtom* parentFrameType = aParentFrameIn.GetType();
  nsIFrame* pseudoParentFrame = nsnull;
  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  aParentFrame = &aParentFrameIn;
  aIsPseudoParent = PR_FALSE;

  if (nsLayoutAtoms::tableOuterFrame == aChildFrameType) { // table child
    if (IsTableRelated(parentFrameType, PR_TRUE) &&
        (nsLayoutAtoms::tableCaptionFrame != parentFrameType) ) { // need pseudo cell parent
      rv = GetPseudoCellFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mCellInner.mFrame;
    }
  } 
  else if (nsLayoutAtoms::tableCaptionFrame == aChildFrameType) { // caption child
    if (nsLayoutAtoms::tableOuterFrame != parentFrameType) { // need pseudo table parent
      rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableOuter.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableColGroupFrame == aChildFrameType) { // col group child
    if (nsLayoutAtoms::tableFrame != parentFrameType) { // need pseudo table parent
      rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableColFrame == aChildFrameType) { // col child
    if (nsLayoutAtoms::tableColGroupFrame != parentFrameType) { // need pseudo col group parent
      rv = GetPseudoColGroupFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mColGroup.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableRowGroupFrame == aChildFrameType) { // row group child
    // XXX can this go away?
    if (nsLayoutAtoms::tableFrame != parentFrameType) {
      // trees allow row groups to contain row groups, so don't create pseudo frames
        rv = GetPseudoTableFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
        if (NS_FAILED(rv)) return rv;
        pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
     }
  }
  else if (nsLayoutAtoms::tableRowFrame == aChildFrameType) { // row child
    if (nsLayoutAtoms::tableRowGroupFrame != parentFrameType) { // need pseudo row group parent
      rv = GetPseudoRowGroupFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRowGroup.mFrame;
    }
  }
  else if (IS_TABLE_CELL(aChildFrameType)) { // cell child
    if (nsLayoutAtoms::tableRowFrame != parentFrameType) { // need pseudo row parent
      rv = GetPseudoRowFrame(aPresShell, aPresContext, aTableCreator, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRow.mFrame;
    }
  }
  else if (nsLayoutAtoms::tableFrame == aChildFrameType) { // invalid
    NS_ASSERTION(PR_FALSE, "GetParentFrame called on nsLayoutAtoms::tableFrame child");
  }
  else { // foreign frame
    if (IsTableRelated(parentFrameType, PR_FALSE)) { // need pseudo cell parent
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

// Construct the outer, inner table frames and the children frames for the table. 
// XXX Page break frames for pseudo table frames are not constructed to avoid the risk
// associated with revising the pseudo frame mechanism. The long term solution
// of having frames handle page-break-before/after will solve the problem. 
nsresult
nsCSSFrameConstructor::ConstructTableFrame(nsIPresShell*            aPresShell,
                                           nsPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aGeometricParent,
                                           nsIFrame*                aContentParent,
                                           nsStyleContext*          aStyleContext,
                                           nsTableCreator&          aTableCreator,
                                           PRBool                   aIsPseudo,
                                           nsFrameItems&            aChildItems,
                                           nsIFrame*&               aNewOuterFrame,
                                           nsIFrame*&               aNewInnerFrame,
                                           PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aGeometricParent) return rv;

  // Create the outer table frame which holds the caption and inner table frame
  aTableCreator.CreateTableOuterFrame(&aNewOuterFrame);

  nsIFrame* parentFrame = aGeometricParent;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aGeometricParent, 
                   nsLayoutAtoms::tableOuterFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mTableOuter.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableOuterFrame);
    }
  }

  // create the pseudo SC for the outer table as a child of the inner SC
  nsRefPtr<nsStyleContext> outerStyleContext;
  outerStyleContext = aPresShell->StyleSet()->
    ResolvePseudoStyleFor(aContent, nsCSSAnonBoxes::tableOuter, aStyleContext);
  
  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned  
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, outerStyleContext, nsnull, aNewOuterFrame);  
  nsHTMLContainerFrame::CreateViewForFrame(aNewOuterFrame, aContentParent,
                                           PR_FALSE);

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
                          PR_FALSE, childItems);

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
                                                  nsPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrameIn,
                                                  nsStyleContext*          aStyleContext,
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
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);

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
                                                   nsPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsStyleContext*          aStyleContext,
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

  const nsStyleDisplay* styleDisplay = aStyleContext->GetStyleDisplay();

  rv = aTableCreator.CreateTableRowGroupFrame(&aNewFrame);

  nsIFrame* scrollFrame = nsnull;
  if (styleDisplay->IsScrollableOverflow()) {
    // Create an area container for the frame
    BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext, 
                     aNewFrame, parentFrame, nsnull, scrollFrame, aStyleContext);

  } 
  else {
    if (NS_FAILED(rv)) return rv;
    InitAndRestoreFrame(aPresContext, aState, aContent, parentFrame, 
                        aStyleContext, nsnull, aNewFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);
  }

  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, 
                              aNewFrame, aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aContent, aNewFrame,
                          PR_FALSE, childItems);

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
                                                   nsPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsStyleContext*          aStyleContext,
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
                                              nsPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsStyleContext*          aStyleContext,
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
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);
  if (!aIsPseudo) {
    nsFrameItems childItems;
    nsIFrame* captionFrame;
    rv = TableProcessChildren(aPresShell, aPresContext, aState, aContent, aNewFrame,
                              aTableCreator, childItems, captionFrame);
    if (NS_FAILED(rv)) return rv;
    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState, aContent, aNewFrame,
                          PR_FALSE, childItems);

    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mRowGroup.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}
      
nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsIPresShell*            aPresShell, 
                                              nsPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsStyleContext*          aStyleContext,
                                              nsTableCreator&          aTableCreator,
                                              PRBool                   aIsPseudo,
                                              nsFrameItems&            aChildItems,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aPresShell || !aPresContext || !aParentFrameIn || !aStyleContext) return rv;

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

  rv = aTableCreator.CreateTableColFrame(&aNewFrame); if (NS_FAILED(rv)) return rv;
  InitAndRestoreFrame(aPresContext, aState, aContent, parentFrame, aStyleContext, nsnull, aNewFrame);
  // if the parent frame was anonymous then reparent the style context
  if (aIsPseudoParent) {
    aPresContext->FrameManager()->
      ReParentStyleContext(aNewFrame, parentFrame->GetStyleContext());
  }

  // construct additional col frames if the col frame has a span > 1
  PRInt32 span = 1;
  nsCOMPtr<nsIDOMHTMLTableColElement> cgContent(do_QueryInterface(aContent));
  if (cgContent) { 
    cgContent->GetSpan(&span);
    nsIFrame* lastCol = aNewFrame;
    nsStyleContext* styleContext = nsnull;
    for (PRInt32 spanX = 1; spanX < span; spanX++) {
      // The same content node should always resolve to the same style context.
      if (1 == spanX)
        styleContext = aNewFrame->GetStyleContext();
      nsIFrame* newCol;
      rv = aTableCreator.CreateTableColFrame(&newCol); if (NS_FAILED(rv)) return rv;
      InitAndRestoreFrame(aPresContext, aState, aContent, parentFrame,
                          styleContext, nsnull, newCol);
      ((nsTableColFrame*)newCol)->SetColType(eColAnonymousCol);
      lastCol->SetNextSibling(newCol);
      lastCol = newCol;
    }
  }

  if (!aIsPseudo && aIsPseudoParent) {
      aState.mPseudoFrames.mColGroup.mChildList.AddChild(aNewFrame);
  }
  
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsIPresShell*            aPresShell, 
                                               nsPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrameIn,
                                               nsStyleContext*          aStyleContext,
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
    // use nsLayoutAtoms::tableCellFrame which will match if it is really nsLayoutAtoms::bcTableCellFrame
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::tableCellFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mCellOuter.mFrame) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, nsLayoutAtoms::tableCellFrame);
    }
  }

  rv = aTableCreator.CreateTableCellFrame(parentFrame, &aNewCellOuterFrame);
  if (NS_FAILED(rv)) return rv;
 
  // Initialize the table cell frame
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, aStyleContext, nsnull, aNewCellOuterFrame);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewCellOuterFrame, nsnull, PR_FALSE);

  // Create a block frame that will format the cell's content
  rv = aTableCreator.CreateTableCellInnerFrame(&aNewCellInnerFrame);

  if (NS_FAILED(rv)) {
    aNewCellOuterFrame->Destroy(aPresContext);
    aNewCellOuterFrame = nsnull;
    return rv;
  }
  
  // Resolve pseudo style and initialize the body cell frame
  nsRefPtr<nsStyleContext> innerPseudoStyle;
  innerPseudoStyle = aPresShell->StyleSet()->
    ResolvePseudoStyleFor(aContent,
                          nsCSSAnonBoxes::cellContent, aStyleContext);

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aNewCellOuterFrame, innerPseudoStyle, nsnull, aNewCellInnerFrame);

  if (!aIsPseudo) {
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);

    // The block frame is a float container
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(aNewCellInnerFrame, floatSaveState,
                                      haveFirstLetterStyle, haveFirstLineStyle);

    // Process the child content
    nsFrameItems childItems;
    // pass in null tableCreator so ProcessChildren will not call TableProcessChildren
    rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, aNewCellInnerFrame, 
                         PR_TRUE, childItems, PR_TRUE, nsnull);
    if (NS_FAILED(rv)) {
      // Clean up
      aNewCellInnerFrame->Destroy(aPresContext);
      aNewCellInnerFrame = nsnull;
      aNewCellOuterFrame->Destroy(aPresContext);
      aNewCellOuterFrame = nsnull;
      return rv;
    }

    aNewCellInnerFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mFloatedItems.childList) {
      aNewCellInnerFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::floatList,
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
nsCSSFrameConstructor::MustGeneratePseudoParent(nsPresContext*  aPresContext,
                                                nsIFrame*        aParentFrame,
                                                nsIAtom*         aTag,
                                                nsIContent*      aContent,
                                                nsStyleContext*  aStyleContext)
{
  if (!aStyleContext)
    return PR_FALSE;

  if (NS_STYLE_DISPLAY_NONE == aStyleContext->GetStyleDisplay()->mDisplay)
    return PR_FALSE;
    
  // check tags first

  if ((nsLayoutAtoms::textTagName == aTag)) {
    return !IsOnlyWhitespace(aContent);
  }

  // exclude tags
  if ( nsLayoutAtoms::commentTagName == aTag) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

// this is called when a non table related element is a child of a table, row group, 
// or row, but not a cell.
nsresult
nsCSSFrameConstructor::ConstructTableForeignFrame(nsIPresShell*            aPresShell, 
                                                  nsPresContext*          aPresContext,
                                                  nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrameIn,
                                                  nsStyleContext*          aStyleContext,
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

  nsIAtom *tag = aContent->Tag();
 
  if (MustGeneratePseudoParent(aPresContext, aParentFrameIn, tag, aContent,
                               aStyleContext)) {
    // this frame may have a pseudo parent, use block frame type to
    // trigger foreign
    GetParentFrame(aPresShell, aPresContext, aTableCreator, *aParentFrameIn, 
                   nsLayoutAtoms::blockFrame, aState, parentFrame, aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aChildItems);
    }
  }

  if (!parentFrame) return rv; // if pseudo frame wasn't created
 
  // there are two situations where table related frames will wrap around
  // foreign frames
  // a) inner table cell, which is a pseudo frame
  // b) caption frame which will be always a real frame.
  NS_ASSERTION(nsLayoutAtoms::tableCaptionFrame == parentFrame->GetType() ||
               parentFrame == aState.mPseudoFrames.mCellInner.mFrame,
               "Weird parent in ConstructTableForeignFrame");

  // Push the parent as the floater containing block
  nsFrameConstructorSaveState saveState;
  aState.PushFloatContainingBlock(parentFrame, saveState, PR_FALSE, PR_FALSE);
  
  // save the pseudo frame state now, as descendants of the child frame may require
  // other pseudo frame creations
  nsPseudoFrames prevPseudoFrames; 
  aState.mPseudoFrames.Reset(&prevPseudoFrames);

  nsFrameItems items;
  rv = ConstructFrame(aPresShell, aPresContext, aState, aContent, parentFrame, items);
  aNewFrame = items.childList;

  // restore the pseudo frame state
  aState.mPseudoFrames = prevPseudoFrames;

  if (aIsPseudoParent) {
    aState.mPseudoFrames.mCellInner.mChildList.AddChild(aNewFrame);
  }

  return rv;
}

static PRBool 
NeedFrameFor(nsIFrame*   aParentFrame,
             nsIContent* aChildContent) 
{
  // don't create a whitespace frame if aParentFrame doesn't want it
  if ((NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE & aParentFrame->GetStateBits())
      && IsOnlyWhitespace(aChildContent)) {
    return PR_FALSE;
  }
  return PR_TRUE;
}


nsresult
nsCSSFrameConstructor::TableProcessChildren(nsIPresShell*            aPresShell, 
                                            nsPresContext*          aPresContext,
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

  nsIAtom* parentFrameType = aParentFrame->GetType();
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();

  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       iter != last;
       ++iter) {
    nsCOMPtr<nsIContent> childContent = *iter;
    if (childContent &&
        (childContent->IsContentOfType(nsIContent::eELEMENT) ||
         childContent->IsContentOfType(nsIContent::eTEXT)) &&
        NeedFrameFor(aParentFrame, childContent)) {
      rv = TableProcessChild(aPresShell, aPresContext, aState, childContent,
                             aContent, aParentFrame,
                             parentFrameType, parentStyleContext,
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
                                         nsPresContext*          aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent*              aChildContent,
                                         nsIContent*              aParentContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aParentFrameType,
                                         nsStyleContext*          aParentStyleContext,
                                         nsTableCreator&          aTableCreator,
                                         nsFrameItems&            aChildItems,
                                         nsIFrame*&               aCaption)
{
  nsresult rv = NS_OK;
  
  PRBool childIsCaption = PR_FALSE;
  PRBool isPseudoParent = PR_FALSE;
    
  nsIFrame* childFrame = nsnull;
  nsRefPtr<nsStyleContext> childStyleContext;

  // Resolve the style context and get its display
  childStyleContext = ResolveStyleContext(aPresContext, aParentFrame,
                                          aChildContent);
  switch (childStyleContext->GetStyleDisplay()->mDisplay) {
  case NS_STYLE_DISPLAY_TABLE:
    {
      PRBool pageBreakAfter = PR_FALSE;

      if (aPresContext->IsPaginated()) {
        // See if there is a page break before, if so construct one. Also see if there is one after
        pageBreakAfter = PageBreakBefore(aPresShell, aPresContext, aState, aChildContent, 
                                       aParentFrame, childStyleContext, aChildItems);
      }
      // construct the table frame
      nsIFrame* innerTableFrame;
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aChildContent,
                               aParentFrame, aParentFrame, childStyleContext,
                               aTableCreator, PR_FALSE, aChildItems,
                               childFrame, innerTableFrame, isPseudoParent);
      if (NS_SUCCEEDED(rv) && pageBreakAfter) {
        // Construct the page break after
        ConstructPageBreakFrame(aPresShell, aPresContext, aState, aChildContent,
                                aParentFrame, childStyleContext, aChildItems);
      }
    }
    break;

  case NS_STYLE_DISPLAY_TABLE_CAPTION:
    if (!aCaption) {  // only allow one caption
      nsIFrame* parentFrame = GetOuterTableFrame(aParentFrame);
      rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, aChildContent, 
                                      parentFrame, childStyleContext, aTableCreator, 
                                      aChildItems, aCaption, isPseudoParent);
    }
    childIsCaption = PR_TRUE;
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, aChildContent, 
                                     aParentFrame, childStyleContext, aTableCreator, 
                                     PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, aChildContent, 
                                     aParentFrame, childStyleContext, aTableCreator, 
                                     PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_ROW:
    rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aChildContent, 
                                aParentFrame, childStyleContext, aTableCreator, 
                                PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_TABLE_COLUMN:
    rv = ConstructTableColFrame(aPresShell, aPresContext, aState, aChildContent, 
                                aParentFrame, childStyleContext, aTableCreator, 
                                PR_FALSE, aChildItems, childFrame, isPseudoParent);
    break;


  case NS_STYLE_DISPLAY_TABLE_CELL:
    nsIFrame* innerCell;
    rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aChildContent, 
                                 aParentFrame, childStyleContext, aTableCreator, PR_FALSE, 
                                 aChildItems, childFrame, innerCell, isPseudoParent);
    break;

  case NS_STYLE_DISPLAY_NONE:
    aState.mFrameManager->SetUndisplayedContent(aChildContent,
                                                childStyleContext);
    break;

  default:
    {

      // if <form>'s parent is <tr>/<table>/<tbody>/<thead>/<tfoot> in html,
      // NOT create psuedoframe for it.
      // see bug 159359
      nsINodeInfo *childNodeInfo = aChildContent->GetNodeInfo();
      // Sometimes aChildContent is a #text node.  In those cases it
      // does not have a nodeinfo, and in those cases we want to
      // construct a foreign frame for it in any case.  So we can just
      // null-check the nodeinfo here.
      NS_ASSERTION(childNodeInfo ||
                   aChildContent->IsContentOfType(nsIContent::eTEXT),
                   "Non-#text nodes should have a nodeinfo here!");
      if (aChildContent->IsContentOfType(nsIContent::eHTML) &&
          childNodeInfo->Equals(nsHTMLAtoms::form) &&
          aParentContent->IsContentOfType(nsIContent::eHTML)) {
        nsINodeInfo *parentNodeInfo = aParentContent->GetNodeInfo();

        if (parentNodeInfo->Equals(nsHTMLAtoms::table) ||
            parentNodeInfo->Equals(nsHTMLAtoms::tr)    ||
            parentNodeInfo->Equals(nsHTMLAtoms::tbody) ||
            parentNodeInfo->Equals(nsHTMLAtoms::thead) ||
            parentNodeInfo->Equals(nsHTMLAtoms::tfoot)) {
          break;
        }
      }

      rv = ConstructTableForeignFrame(aPresShell, aPresContext, aState, aChildContent, 
                                      aParentFrame, childStyleContext, aTableCreator, 
                                      aChildItems, childFrame, isPseudoParent);
    }
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
nsCSSFrameConstructor::GetDisplay(nsIFrame* aFrame)
{
  if (nsnull == aFrame) {
    return nsnull;
  }
  return aFrame->GetStyleContext()->GetStyleDisplay();
}

/***********************************************
 * END TABLE SECTION
 ***********************************************/

nsresult
nsCSSFrameConstructor::ConstructDocElementTableFrame(nsIPresShell*        aPresShell, 
                                                     nsPresContext* aPresContext,
                                                     nsIContent*     aDocElement,
                                                     nsIFrame*       aParentFrame,
                                                     nsIFrame*&      aNewTableFrame,
                                                     nsILayoutHistoryState* aFrameState)
{
  nsFrameConstructorState state(aPresContext, nsnull, nsnull, nsnull, aFrameState);
  nsFrameItems    frameItems;

  ConstructFrame(aPresShell, aPresContext, state, aDocElement, aParentFrame, frameItems);
  aNewTableFrame = frameItems.childList;
  if (!aNewTableFrame) {
    NS_WARNING("cannot get table contentFrame");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static PRBool CheckOverflow(nsPresContext* aPresContext,
                            const nsStyleDisplay* aDisplay)
{
  if (aDisplay->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE)
    return PR_FALSE;

  if (aDisplay->mOverflowX == NS_STYLE_OVERFLOW_CLIP)
    aPresContext->SetViewportOverflowOverride(NS_STYLE_OVERFLOW_HIDDEN,
                                              NS_STYLE_OVERFLOW_HIDDEN);
  else
    aPresContext->SetViewportOverflowOverride(aDisplay->mOverflowX,
                                              aDisplay->mOverflowY);
  return PR_TRUE;
}

/**
 * This checks the root element and the HTML BODY, if any, for an "overflow" property
 * that should be applied to the viewport. If one is found then we return the
 * element that we took the overflow from (which should then be treated as
 * "overflow:visible"), and we store the overflow style in the prescontext.
 * @param aDocElement is mDocument->GetRootContent()
 * @param aIsScrollable if nonnull, is set to indicate whether scrolling is desired
 * on the viewport at all
 */
nsIContent*
nsCSSFrameConstructor::PropagateScrollToViewport(nsPresContext* aPresContext)
{
  // Set default
  aPresContext->SetViewportOverflowOverride(NS_STYLE_OVERFLOW_AUTO,
                                            NS_STYLE_OVERFLOW_AUTO);

  // We never mess with the viewport scroll state
  // when printing or in print preview
  if (aPresContext->IsPaginated()) {
    return nsnull;
  }

  nsIContent* docElement = mDocument->GetRootContent();

  // Check the style on the document root element
  nsStyleSet *styleSet = aPresContext->PresShell()->StyleSet();
  nsRefPtr<nsStyleContext> rootStyle;
  rootStyle = styleSet->ResolveStyleFor(docElement, nsnull);
  if (!rootStyle) {
    return nsnull;
  }
  if (CheckOverflow(aPresContext, rootStyle->GetStyleDisplay())) {
    // tell caller we stole the overflow style from the root element
    return docElement;
  }
  
  // Don't look in the BODY for non-HTML documents or HTML documents
  // with non-HTML roots
  // XXX this should be earlier; we shouldn't even look at the document root
  // for non-HTML documents. Fix this once we support explicit CSS styling
  // of the viewport
  // XXX what about XHTML?
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
  if (!htmlDoc || !docElement->IsContentOfType(nsIContent::eHTML)) {
    return nsnull;
  }
  
  nsCOMPtr<nsIDOMHTMLElement> body;
  htmlDoc->GetBody(getter_AddRefs(body));
  nsCOMPtr<nsIContent> bodyElement = do_QueryInterface(body);
  
  if (!bodyElement ||
      !bodyElement->GetNodeInfo()->Equals(nsHTMLAtoms::body)) {
    // The body is not a <body> tag, it's a <frameset>.
    return nsnull;
  }

  nsRefPtr<nsStyleContext> bodyStyle;
  bodyStyle = styleSet->ResolveStyleFor(bodyElement, rootStyle);
  if (!bodyStyle) {
    return nsnull;
  }

  if (CheckOverflow(aPresContext, bodyStyle->GetStyleDisplay())) {
    // tell caller we stole the overflow style from the body element
    return bodyElement;
  }

  return nsnull;
}

/**
 * New one
 */
nsresult
nsCSSFrameConstructor::ConstructDocElementFrame(nsIPresShell*        aPresShell, 
                                                nsPresContext*          aPresContext,
                                                nsFrameConstructorState& aState,
                                                nsIContent*              aDocElement,
                                                nsIFrame*                aParentFrame,
                                                nsIFrame*&               aNewFrame)
{
    // how the root frame hierarchy should look

    /*

---------------No Scrollbars------


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

  aNewFrame = nsnull;

  if (!mTempFrameTreeState)
    aPresShell->CaptureHistoryState(getter_AddRefs(mTempFrameTreeState));

  // ----- reattach gfx scrollbars ------
  // Gfx scrollframes were created in the root frame but the primary frame map may have been destroyed if a 
  // new style sheet was loaded so lets reattach the frames to their content.
  if (mGfxScrollFrame) {
    nsIFrame* scrollPort = mGfxScrollFrame->GetFirstChild(nsnull);

    nsIFrame* gfxScrollbarFrame1 = nsnull;
    nsIFrame* gfxScrollbarFrame2 = nsnull;
    gfxScrollbarFrame1 = scrollPort->GetNextSibling();
    if (gfxScrollbarFrame1) {
      // XXX This works, but why?
      aState.mFrameManager->
        SetPrimaryFrameFor(gfxScrollbarFrame1->GetContent(), gfxScrollbarFrame1);

      gfxScrollbarFrame2 = gfxScrollbarFrame1->GetNextSibling();
      if (gfxScrollbarFrame2) {
        // XXX This works, but why?
        aState.mFrameManager->
          SetPrimaryFrameFor(gfxScrollbarFrame2->GetContent(), gfxScrollbarFrame2);
      }
    }
  }

  // --------- CREATE AREA OR BOX FRAME -------
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = aPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                         nsnull);

  const nsStyleDisplay* display = styleContext->GetStyleDisplay();

  // Ensure that our XBL bindings are installed.
  if (display->mBinding) {
    // Get the XBL loader.
    nsresult rv;
    PRBool resolveStyle;
    nsCOMPtr<nsIXBLBinding> binding;
    
    nsIXBLService * xblService = GetXBLService();
    if (!xblService)
      return NS_ERROR_FAILURE;

    rv = xblService->LoadBindings(aDocElement, display->mBinding, PR_FALSE, getter_AddRefs(binding), &resolveStyle);
    if (NS_FAILED(rv))
      return NS_OK; // Binding will load asynchronously.

    if (binding) {
      nsIBindingManager *bm = mDocument->GetBindingManager();
      if (bm)
        bm->AddToAttachedQueue(binding);
    }

    if (resolveStyle) {
      styleContext = aPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                             nsnull);
      display = styleContext->GetStyleDisplay();
    }
  }

  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

  PRBool propagatedScrollToViewport =
    PropagateScrollToViewport(aPresContext) == aDocElement;

  // The document root should not be scrollable in any paginated context,
  // even in print preview.
  PRBool isScrollable = display->IsScrollableOverflow()
    && !aPresContext->IsPaginated()
    && !propagatedScrollToViewport;

  nsIFrame* scrollFrame = nsnull;

  // build a scrollframe
  if (isScrollable) {
    nsIFrame* newScrollFrame = nsnull;
    nsRefPtr<nsStyleContext> newContext;

    newContext = BeginBuildingScrollFrame( aPresShell, aPresContext,
                                           aState,
                                           aDocElement,
                                           styleContext,
                                           aParentFrame,
                                           nsnull,
                                           nsCSSAnonBoxes::scrolledContent,
                                           mDocument,
                                           PR_FALSE,
                                           scrollFrame,
                                           newScrollFrame);

    styleContext = newContext;
    aParentFrame = newScrollFrame;
  }

  nsIFrame* contentFrame = nsnull;
  PRBool isBlockFrame = PR_FALSE;
  nsresult rv;

  // The rules from CSS 2.1, section 9.2.4, have already been applied
  // by the style system, so we can assume that display->mDisplay is
  // either NONE, BLOCK, or TABLE.

  PRBool docElemIsTable = display->mDisplay == NS_STYLE_DISPLAY_TABLE;

  if (docElemIsTable) {
    // if the document is a table then just populate it.
    rv = ConstructDocElementTableFrame(aPresShell, aPresContext, aDocElement, 
                                       aParentFrame, contentFrame,
                                       aState.mFrameState);
    if (NS_FAILED(rv)) {
      return rv;
    }
    styleContext = contentFrame->GetStyleContext();
  } else {
    // otherwise build a box or a block
#if defined(MOZ_SVG)
    PRInt32 nameSpaceID;
#endif
#ifdef MOZ_XUL
    if (aDocElement->IsContentOfType(nsIContent::eXUL)) {
      rv = NS_NewDocElementBoxFrame(aPresShell, &contentFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else
#endif 
#ifdef MOZ_SVG
    if (aDocElement->GetNameSpaceID(&nameSpaceID),
        (nameSpaceID == kNameSpaceID_SVG)) {
      rv = NS_NewSVGOuterSVGFrame(aPresShell, aDocElement, &contentFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
      isBlockFrame = PR_TRUE;
    }
    else 
#endif
    {
      rv = NS_NewDocumentElementFrame(aPresShell, &contentFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
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
    FinishBuildingScrollFrame(aPresContext, aState, aDocElement,
                              aParentFrame, contentFrame, styleContext);
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
    nsFrameConstructorSaveState floatSaveState;
    nsFrameItems                childItems;

    if (isBlockFrame) {
      PRBool haveFirstLetterStyle, haveFirstLineStyle;
      HaveSpecialBlockStyle(aPresContext, aDocElement, styleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
      aState.PushAbsoluteContainingBlock(aPresContext, contentFrame,
                                         absoluteSaveState);
      aState.PushFloatContainingBlock(contentFrame, floatSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    }

    // Create any anonymous frames the doc element frame requires
    // This must happen before ProcessChildren to ensure that popups are
    // never constructed before the popupset.
    CreateAnonymousFrames(aPresShell, aPresContext, nsnull, aState,
                          aDocElement, contentFrame,
                          PR_FALSE, childItems, PR_TRUE);
    ProcessChildren(aPresShell, aPresContext, aState, aDocElement,
                    contentFrame, PR_TRUE, childItems, isBlockFrame);

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
                                          nsLayoutAtoms::floatList,
                                          aState.mFloatedItems.childList);
      }
    }
  }

  return NS_OK;
}


nsresult
nsCSSFrameConstructor::ConstructRootFrame(nsIPresShell*        aPresShell, 
                                          nsPresContext* aPresContext,
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
  nsIBindingManager *bindingManager = mDocument->GetBindingManager();
  if (bindingManager) {
    nsCOMPtr<nsIStyleRuleSupplier> ruleSupplier(do_QueryInterface(bindingManager));
    aPresShell->StyleSet()->SetStyleRuleSupplier(ruleSupplier);
  }
  
  // --------- BUILD VIEWPORT -----------
  nsIFrame*                 viewportFrame = nsnull;
  nsRefPtr<nsStyleContext> viewportPseudoStyle;
  nsStyleSet *styleSet = aPresShell->StyleSet();

  viewportPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                        nsCSSAnonBoxes::viewport,
                                                        nsnull);

  NS_NewViewportFrame(aPresShell, &viewportFrame);


  // XXXbz do we _have_ to pass a null content pointer to that frame?
  // Would it really kill us to pass in the root element or something?
  // What would that break?
  viewportFrame->Init(aPresContext, nsnull, nsnull, viewportPseudoStyle, nsnull);

  // Bind the viewport frame to the root view
  nsIViewManager* viewManager = aPresContext->GetViewManager();
  nsIView*        rootView;

  viewManager->GetRootView(rootView);
  viewportFrame->SetView(rootView);

  nsContainerFrame::SyncFrameViewProperties(aPresContext, viewportFrame,
                                            viewportPseudoStyle, rootView);

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

    PRBool isPaginated = aPresContext->IsPaginated();
    PRBool isPrintPreview =
      aPresContext->Type() == nsPresContext::eContext_PrintPreview;

    nsIFrame* rootFrame = nsnull;
    nsIAtom* rootPseudo;
        
    if (!isPaginated) {
#ifdef MOZ_XUL
        if (aDocElement->IsContentOfType(nsIContent::eXUL)) 
        {
          NS_NewRootBoxFrame(aPresShell, &rootFrame);
        } else 
#endif
        {
          NS_NewCanvasFrame(aPresShell, &rootFrame);
        }

        rootPseudo = nsCSSAnonBoxes::canvas;
        mDocElementContainingBlock = rootFrame;
    } else {
        // Create a page sequence frame
        NS_NewSimplePageSequenceFrame(aPresShell, &rootFrame);
        rootPseudo = nsCSSAnonBoxes::pageSequence;
    }


  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

  // If the device supports scrolling (e.g., in galley mode on the screen and
  // for print-preview, but not when printing), then create a scroll frame that
  // will act as the scrolling mechanism for the viewport. 
  // XXX Do we even need a viewport when printing to a printer?

  //isScrollable = PR_FALSE;

  // As long as the webshell doesn't prohibit it, and the device supports
  // it, create a scroll frame that will act as the scolling mechanism for
  // the viewport.
  //
  // Threre are three possible values stored in the docshell:
  //  1) nsIScrollable::Scrollbar_Never = no scrollbars
  //  2) nsIScrollable::Scrollbar_Auto = scrollbars appear if needed
  //  3) nsIScrollable::Scrollbar_Always = scrollbars always
  // Only need to create a scroll frame/view for cases 2 and 3.

  PRBool isHTML = aDocElement->IsContentOfType(nsIContent::eHTML);
  PRBool isXUL = PR_FALSE;

  if (!isHTML) {
    isXUL = aDocElement->IsContentOfType(nsIContent::eXUL);
  }

  // Never create scrollbars for XUL documents
  PRBool isScrollable = !isXUL;

  // Never create scrollbars for frameset documents.
  if (isHTML) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc && htmlDoc->GetIsFrameset())
      isScrollable = PR_FALSE;
  }

  // Don't create a scrollframe when we're inside a frame or iframe with
  // scrolling="no".  This makes the frame hierarchy inconsistent and is
  // unnecessary for correctness (since
  // nsGfxScrollFrameInner::GetScrollbarStyles handles all the necessary
  // cases), but it seems to be needed for performance.
  if (isScrollable) {
    nsresult rv;
    if (aPresContext) {
      nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
      if (container) {
        nsCOMPtr<nsIScrollable> scrollableContainer = do_QueryInterface(container, &rv);
        if (NS_SUCCEEDED(rv) && scrollableContainer) {
          PRInt32 scrolling = -1;
          scrollableContainer->GetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,&scrolling);
          if (nsIScrollable::Scrollbar_Never == scrolling) {
            scrollableContainer->GetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,&scrolling);
            if (nsIScrollable::Scrollbar_Never == scrolling) {
              isScrollable = PR_FALSE;
            }
          }
        }
      }
    }
  }

  if (isPaginated) {
    if (isPrintPreview) {
      isScrollable = aPresContext->HasPaginatedScrolling();
    } else {
      isScrollable = PR_FALSE; // we are printing
    }
  }

  // We no longer need to do overflow propagation here. It's taken care of
  // when we construct frames for the element whose overflow might be
  // propagated
  NS_ASSERTION(!isScrollable || !isXUL,
               "XUL documents should never be scrollable - see above");

  nsIFrame* newFrame = rootFrame;
  nsRefPtr<nsStyleContext> rootPseudoStyle;
  // we must create a state because if the scrollbars are GFX it needs the 
  // state to build the scrollbar frames.
  nsFrameConstructorState state(aPresContext,
                                      nsnull,
                                      nsnull,
                                      nsnull);

  nsIFrame* parentFrame = viewportFrame;

  // If paginated, make sure we don't put scrollbars in
  if (isPaginated && !isPrintPreview)
    rootPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                      rootPseudo,
                                                      viewportPseudoStyle);
  else if (isScrollable) {

      // Build the frame. We give it the content we are wrapping which is the document,
      // the root frame, the parent view port frame, and we should get back the new
      // frame and the scrollable view if one was created.

      // resolve a context for the scrollframe
      nsRefPtr<nsStyleContext>  styleContext;
      styleContext = styleSet->ResolvePseudoStyleFor(nsnull,
                                                     nsCSSAnonBoxes::viewportScroll,
                                                     viewportPseudoStyle);

      // Note that the viewport scrollframe is always built with
      // overflow:auto style. This forces the scroll frame to create
      // anonymous content for both scrollbars. This is necessary even
      // if the HTML or BODY elements are overriding the viewport
      // scroll style to 'hidden' --- dynamic style changes might put
      // scrollbars back on the viewport and we don't want to have to
      // reframe the viewport to create the scrollbar content.
      nsIFrame* newScrollableFrame = nsnull;

      newFrame = nsnull;
      rootPseudoStyle = BeginBuildingScrollFrame( aPresShell,
                                                  aPresContext,
                                                  state,
                                                  aDocElement,
                                                  styleContext,
                                                  viewportFrame,
                                                  nsnull,
                                                  rootPseudo,
                                                  mDocument,
                                                  PR_TRUE,
                                                  newFrame,
                                                  newScrollableFrame);

      // Inform the view manager about the root scrollable view
      nsIView* view = newScrollableFrame->GetView();
      NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

      nsIScrollableView* scrollableView = nsnull;
      NS_ENSURE_SUCCESS(CallQueryInterface(view, &scrollableView), NS_ERROR_FAILURE);

      viewManager->SetRootScrollableView(scrollableView);
      parentFrame = newScrollableFrame;

      mGfxScrollFrame = newFrame;

  } else {
    // If no scrollbars and xul, don't build a scrollframe at all. 
    if (isXUL) {
      rootPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                        rootPseudo,
                                                        viewportPseudoStyle);
    } else {
      // if HTML the always create a scrollframe so anchors work. That way you can scroll to 
      // anchors even if we don't have scrollbars.

      // create a style context for the scrollport of the viewport
      nsRefPtr<nsStyleContext> scrollPseudoStyle;
      scrollPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                          nsCSSAnonBoxes::scrolledContent,
                                                          viewportPseudoStyle);

      // create scrollframe
      nsIFrame* scrollFrame = nsnull;
      NS_NewScrollPortFrame(aPresShell, &scrollFrame);
      NS_ENSURE_TRUE(scrollFrame, NS_ERROR_FAILURE);

      scrollFrame->Init(aPresContext, nsnull, parentFrame, scrollPseudoStyle, nsnull);

      // resolve a new style for the root frame
      rootPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                        rootPseudo,
                                                        scrollPseudoStyle);

      // Inform the view manager about the root scrollable view
      nsIView* view = scrollFrame->GetView();
      NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

      nsIScrollableView* scrollableView = nsnull;
      NS_ENSURE_SUCCESS(CallQueryInterface(view, &scrollableView), NS_ERROR_FAILURE);

      viewManager->SetRootScrollableView(scrollableView);

      parentFrame = scrollFrame;
      newFrame = scrollFrame;
    }
  }
  

  rootFrame->Init(aPresContext, aDocElement, parentFrame,
                  rootPseudoStyle, nsnull);
  
  if (!isPaginated || isPrintPreview) {
    if (isScrollable) {
      FinishBuildingScrollFrame(aPresContext, 
                                state,
                                aDocElement,
                                parentFrame,
                                rootFrame,
                                rootPseudoStyle);

      // set the primary frame to the root frame
      state.mFrameManager->SetPrimaryFrameFor(aDocElement, rootFrame);
    } else { // if not scrollable
      if (!isXUL) { // if not XUL
        parentFrame->SetInitialChildList(aPresContext, nsnull, rootFrame);
      }
    }
  } 
  
  if (isPaginated) { // paginated
    // Create the first page
    // Set the initial child lists
    nsIFrame *pageFrame, *pageContentFrame;
    ConstructPageFrame(aPresShell, aPresContext, rootFrame, nsnull,
                       pageFrame, pageContentFrame);
    rootFrame->SetInitialChildList(aPresContext, nsnull, pageFrame);

    // The eventual parent of the document element frame.
    // XXX should this be set for every new page (in ConstructPageFrame)?
    mDocElementContainingBlock = pageContentFrame;
  }

  viewportFrame->SetInitialChildList(aPresContext, nsnull, newFrame);
  
  aNewFrame = viewportFrame;



  return NS_OK;  
}

nsresult
nsCSSFrameConstructor::ConstructPageFrame(nsIPresShell*   aPresShell,
                                          nsPresContext* aPresContext,
                                          nsIFrame*       aParentFrame,
                                          nsIFrame*       aPrevPageFrame,
                                          nsIFrame*&      aPageFrame,
                                          nsIFrame*&      aPageContentFrame)
{
  nsresult rv = NS_OK;
  rv = NS_NewPageFrame(aPresShell, &aPageFrame);
  if (NS_FAILED(rv))
    return rv;

  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();
  nsStyleSet *styleSet = aPresShell->StyleSet();

  nsRefPtr<nsStyleContext> pagePseudoStyle;
  pagePseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                    nsCSSAnonBoxes::page,
                                                    parentStyleContext);

  // Initialize the page frame and force it to have a view. This makes printing of
  // the pages easier and faster.
  aPageFrame->Init(aPresContext, nsnull, aParentFrame, pagePseudoStyle, aPrevPageFrame);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  rv = nsHTMLContainerFrame::CreateViewForFrame(aPageFrame, nsnull, PR_TRUE);
  if (NS_FAILED(rv))
    return NS_ERROR_NULL_POINTER;

  NS_NewPageContentFrame(aPresShell, &aPageContentFrame);

  nsRefPtr<nsStyleContext> pageContentPseudoStyle;
  pageContentPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                           nsCSSAnonBoxes::pageContent,
                                                           pagePseudoStyle);

  // Initialize the page content frame and force it to have a view. Also make it the
  // containing block for fixed elements which are repeated on every page.
  aPageContentFrame->Init(aPresContext, nsnull, aPageFrame, pageContentPseudoStyle, nsnull);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aPageContentFrame, nsnull, PR_TRUE);
  if (NS_FAILED(rv))
    return NS_ERROR_NULL_POINTER;
  mFixedContainingBlock = aPageContentFrame;

  aPageFrame->SetInitialChildList(aPresContext, nsnull, aPageContentFrame);

  // Fixed pos kids are taken care of directly in CreateContinuingFrame()
  
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePlaceholderFrameFor(nsIPresShell*    aPresShell, 
                                                 nsPresContext*  aPresContext,
                                                 nsFrameManager*  aFrameManager,
                                                 nsIContent*      aContent,
                                                 nsIFrame*        aFrame,
                                                 nsStyleContext*  aStyleContext,
                                                 nsIFrame*        aParentFrame,
                                                 nsIFrame**       aPlaceholderFrame)
{
  nsPlaceholderFrame* placeholderFrame;
  nsresult            rv = NS_NewPlaceholderFrame(aPresShell, (nsIFrame**)&placeholderFrame);

  if (NS_SUCCEEDED(rv)) {
    // The placeholder frame gets a pseudo style context
    nsRefPtr<nsStyleContext> placeholderStyle;
    nsStyleContext* parentContext = aStyleContext->GetParent();
    placeholderStyle = aPresShell->StyleSet()->
      ResolveStyleForNonElement(parentContext);
    placeholderFrame->Init(aPresContext, aContent, aParentFrame,
                           placeholderStyle, nsnull);
  
    // The placeholder frame has a pointer back to the out-of-flow frame
    placeholderFrame->SetOutOfFlowFrame(aFrame);
  
    aFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);

    // Add mapping from absolutely positioned frame to its placeholder frame
    aFrameManager->RegisterPlaceholderFrame(placeholderFrame);

    *aPlaceholderFrame = NS_STATIC_CAST(nsIFrame*, placeholderFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructRadioControlFrame(nsIPresShell*        aPresShell, 
                                                 nsPresContext*  aPresContext,
                                                 nsIFrame*&   aNewFrame,
                                                 nsIContent*  aContent,
                                                 nsStyleContext* aStyleContext)
{
  nsresult rv = NS_NewGfxRadioControlFrame(aPresShell, &aNewFrame);
  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
    return rv;
  }

  nsRefPtr<nsStyleContext> radioStyle;
  radioStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                             nsCSSAnonBoxes::radio,
                                                             aStyleContext);
  nsIRadioControlFrame* radio = nsnull;
  if (aNewFrame != nsnull && NS_SUCCEEDED(aNewFrame->QueryInterface(NS_GET_IID(nsIRadioControlFrame), (void**)&radio))) {
    radio->SetRadioButtonFaceStyleContext(radioStyle);
    NS_RELEASE(radio);
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructCheckboxControlFrame(nsIPresShell*    aPresShell, 
                                                     nsPresContext*  aPresContext,
                                                     nsIFrame*&       aNewFrame,
                                                     nsIContent*      aContent,
                                                     nsStyleContext*  aStyleContext)
{
  nsresult rv = NS_NewGfxCheckboxControlFrame(aPresShell, &aNewFrame);
  if (NS_FAILED(rv)) {
    aNewFrame = nsnull;
    return rv;
  }

  nsRefPtr<nsStyleContext> checkboxStyle;
  checkboxStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                                nsCSSAnonBoxes::check, 
                                                                aStyleContext);
  nsICheckboxControlFrame* checkbox = nsnull;
  if (aNewFrame != nsnull && 
      NS_SUCCEEDED(aNewFrame->QueryInterface(NS_GET_IID(nsICheckboxControlFrame), (void**)&checkbox))) {
    checkbox->SetCheckboxFaceStyleContext(checkboxStyle);
    NS_RELEASE(checkbox);
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructSelectFrame(nsIPresShell*        aPresShell, 
                                            nsPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsStyleContext*          aStyleContext,
                                            nsIFrame*&               aNewFrame,
                                            PRBool&                  aProcessChildren,
                                            const nsStyleDisplay*    aStyleDisplay,
                                            PRBool&                  aFrameHasBeenInitialized,
                                            nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
  const PRInt32 kNoSizeSpecified = -1;

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
        // The frame-based combo box is built out of three parts. A display area, a button and
        // a dropdown list. The display area and button are created through anonymous content.
        // The drop-down list's frame is created explicitly. The combobox frame shares it's content
        // with the drop-down list.
      PRUint32 flags = NS_BLOCK_SHRINK_WRAP | NS_BLOCK_SPACE_MGR;
      nsIFrame * comboboxFrame;
      rv = NS_NewComboboxControlFrame(aPresShell, &comboboxFrame, flags);

      // Save the history state so we don't restore during construction
      // since the complete tree is required before we restore.
      nsILayoutHistoryState *historyState = aState.mFrameState;
      aState.mFrameState = nsnull;
      // Initialize the combobox frame
      InitAndRestoreFrame(aPresContext, aState, aContent,
                          aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                          aStyleContext, nsnull, comboboxFrame);

      nsHTMLContainerFrame::CreateViewForFrame(comboboxFrame, aParentFrame, PR_FALSE);
      ///////////////////////////////////////////////////////////////////
      // Combobox - Old Native Implementation
      ///////////////////////////////////////////////////////////////////
      nsIComboboxControlFrame* comboBox = nsnull;
      CallQueryInterface(comboboxFrame, &comboBox);
      NS_ASSERTION(comboBox, "NS_NewComboboxControlFrame returned frame that "
                             "doesn't implement nsIComboboxControlFrame");
      comboBox->SetFrameConstructor(this);

        // Create a listbox
      nsIFrame * listFrame;
      rv = NS_NewListControlFrame(aPresShell, &listFrame);

        // Notify the listbox that it is being used as a dropdown list.
      nsIListControlFrame * listControlFrame;
      rv = CallQueryInterface(listFrame, &listControlFrame);
      if (NS_SUCCEEDED(rv)) {
        listControlFrame->SetComboboxFrame(comboboxFrame);
      }
         // Notify combobox that it should use the listbox as it's popup
      comboBox->SetDropDown(listFrame);

        // Resolve psuedo element style for the dropdown list 
      nsRefPtr<nsStyleContext> listStyle;
      listStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(aContent, 
                                                                nsCSSAnonBoxes::dropDownList, 
                                                                aStyleContext);

      NS_ASSERTION(!listStyle->GetStyleDisplay()->IsPositioned(),
                   "Ended up with positioned dropdown list somehow.");
      NS_ASSERTION(!listStyle->GetStyleDisplay()->IsFloating(),
                   "Ended up with floating dropdown list somehow.");
      
      // Initialize the scroll frame positioned. Note that it is NOT
      // initialized as absolutely positioned.
      nsIFrame* newFrame = nsnull;
      nsIFrame* scrolledFrame = nsnull;
      NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

      // make sure any existing anonymous content is cleared out. Gfx scroll frame construction
      // should reset it to just be the anonymous scrollbars, but we don't want to depend
      // on that.
      aPresShell->SetAnonymousContentFor(aContent, nsnull);

      InitializeSelectFrame(aPresShell, aPresContext, aState, listFrame,
                            scrolledFrame, aContent, comboboxFrame,
                            listStyle, PR_TRUE);
      newFrame = listFrame;

        // Set flag so the events go to the listFrame not child frames.
        // XXX: We should replace this with a real widget manager similar
        // to how the nsFormControlFrame works. Re-directing events is a temporary Kludge.
      NS_ASSERTION(listFrame->GetView(), "ListFrame's view is nsnull");
      //listFrame->GetView()->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

      // Create display and button frames from the combobox's anonymous content.
      // The anonymous content is appended to existing anonymous content for this
      // element (the scrollbars).
      nsFrameItems childItems;
      CreateAnonymousFrames(aPresShell, aPresContext, nsHTMLAtoms::combobox,
                            aState, aContent, comboboxFrame, PR_TRUE, childItems);
  
      comboboxFrame->SetInitialChildList(aPresContext, nsnull,
                                         childItems.childList);

      // Initialize the additional popup child list which contains the
      // dropdown list frame.
      nsFrameItems popupItems;
      popupItems.AddChild(listFrame);
      comboboxFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::popupList,
                                         popupItems.childList);

      // Don't process, the children, They are already processed by the
      // InitializeScrollFrame call above.
      aProcessChildren = PR_FALSE;
      aNewFrame = comboboxFrame;
      aFrameHasBeenInitialized = PR_TRUE;
      aState.mFrameState = historyState;
      if (aState.mFrameState && aState.mFrameManager) {
        // Restore frame state for the entire subtree of |comboboxFrame|.
        aState.mFrameManager->RestoreFrameState(comboboxFrame,
                                                aState.mFrameState);
      }
    } else {
      ///////////////////////////////////////////////////////////////////
      // ListBox - Old Native Implementation
      ///////////////////////////////////////////////////////////////////
      nsIFrame * listFrame;
      rv = NS_NewListControlFrame(aPresShell, &listFrame);
      aNewFrame = listFrame;

      PRUint32 flags = NS_BLOCK_SHRINK_WRAP | NS_BLOCK_SPACE_MGR;
      nsIFrame* scrolledFrame = nsnull;
      NS_NewSelectsAreaFrame(aPresShell, &scrolledFrame, flags);

      // ******* this code stolen from Initialze ScrollFrame ********
      // please adjust this code to use BuildScrollFrame.

      InitializeSelectFrame(aPresShell, aPresContext, aState, listFrame,
                            scrolledFrame, aContent, aParentFrame,
                            aStyleContext, PR_FALSE);

      aNewFrame = listFrame;

      aFrameHasBeenInitialized = PR_TRUE;
    }
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
                                             nsPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIFrame*                scrollFrame,
                                             nsIFrame*                scrolledFrame,
                                             nsIContent*              aContent,
                                             nsIFrame*                aParentFrame,
                                             nsStyleContext*          aStyleContext,
                                             PRBool                   aBuildCombobox)
{
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // Initialize it
  nsIFrame* geometricParent = aState.GetGeometricParent(display, aParentFrame);
    
  // We don't call InitAndRestoreFrame for scrollFrame because we can only
  // restore the frame state after its parts have been created (in particular,
  // the scrollable view). So we have to split Init and Restore.

  // Initialize the frame
  scrollFrame->Init(aPresContext, aContent, geometricParent, aStyleContext, nsnull);

  nsHTMLContainerFrame::CreateViewForFrame(scrollFrame, aParentFrame,
                                           aBuildCombobox);
  if (aBuildCombobox) {
    // Give the drop-down list a popup widget
    nsIView* view = scrollFrame->GetView();
    NS_ASSERTION(view, "We asked for a view but didn't get one");
    if (view) {
      view->GetViewManager()->SetViewFloating(view, PR_TRUE);

      nsWidgetInitData widgetData;
      widgetData.mWindowType  = eWindowType_popup;
      widgetData.mBorderStyle = eBorderStyle_default;

#if defined(XP_MAC) || defined(XP_MACOSX)
      static NS_DEFINE_IID(kCPopUpCID,  NS_POPUP_CID);
      view->CreateWidget(kCPopUpCID, &widgetData, nsnull);
#else
      static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
      view->CreateWidget(kCChildCID, &widgetData, nsnull);
#endif
    }
  }

  nsStyleContext* scrolledPseudoStyle;
  BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext,
                   scrolledFrame, geometricParent, aParentFrame, scrollFrame,
                   scrolledPseudoStyle);

  if (aState.mFrameState && aState.mFrameManager) {
    // Restore frame state for the scroll frame
    aState.mFrameManager->RestoreFrameStateFor(scrollFrame, aState.mFrameState);
  }

  // The area frame is a float container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(scrolledFrame, floatSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;
  PRBool isPositionedContainingBlock = display->IsPositioned();

  if (isPositionedContainingBlock) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    aState.PushAbsoluteContainingBlock(aPresContext, scrolledFrame, absoluteSaveState);
  }

  ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_FALSE,
                  childItems, PR_TRUE);

  // if a select is being created with zero options we need to create
  // a special pseudo frame so it can be sized as best it can
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
  nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                             (void**)getter_AddRefs(selectElement));
  if (NS_SUCCEEDED(result) && selectElement) {
    AddDummyFrameToSelect(aPresContext, aPresShell, aState,
                          scrollFrame, scrolledFrame, &childItems,
                          aContent, selectElement);
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
                                       nsLayoutAtoms::floatList,
                                       aState.mFloatedItems.childList);
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructFieldSetFrame(nsIPresShell*            aPresShell, 
                                              nsPresContext*          aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrame,
                                              nsIAtom*                 aTag,
                                              nsStyleContext*          aStyleContext,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aProcessChildren,
                                              const nsStyleDisplay*    aStyleDisplay,
                                              PRBool&                  aFrameHasBeenInitialized)
{
  nsIFrame * newFrame;
  nsresult rv = NS_NewFieldSetFrame(aPresShell, &newFrame, NS_BLOCK_SPACE_MGR);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Initialize it
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                      aStyleContext, nsnull, newFrame);

  // See if we need to create a view, e.g. the frame is absolutely
  // positioned
  nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

  nsIFrame* areaFrame;
  NS_NewAreaFrame(aPresContext->PresShell(), &areaFrame,
                  NS_BLOCK_SPACE_MGR | NS_BLOCK_SHRINK_WRAP);

  // Resolve style and initialize the frame
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = aPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                               nsCSSAnonBoxes::fieldsetContent,
                                                               aStyleContext);
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      newFrame, styleContext, nsnull, areaFrame);

  

  // The area frame is a float container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(areaFrame, floatSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;
  PRBool isPositionedContainingBlock = aStyleDisplay->IsPositioned();

  if (isPositionedContainingBlock) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    aState.PushAbsoluteContainingBlock(aPresContext, areaFrame, absoluteSaveState);
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
        previous->SetNextSibling(legendFrame->GetNextSibling());
      } else {
        childItems.childList = legendFrame->GetNextSibling();
      }
      areaFrame->SetNextSibling(legendFrame);
      legendFrame->SetParent(newFrame);
      legendFrame->SetNextSibling(nsnull);
      break;
    }
    previous = child;
    child = child->GetNextSibling();
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
                                   nsLayoutAtoms::floatList,
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
nsCSSFrameConstructor::ConstructTextFrame(nsIPresShell*            aPresShell, 
                                          nsPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems)
{
  // process pending pseudo frames. whitespace doesn't have an effect.
  if (!aState.mPseudoFrames.IsEmpty() && !IsOnlyWhitespace(aContent))
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);

  nsIFrame* newFrame = nsnull;

#ifdef MOZ_SVG
  nsresult rv;
  nsCOMPtr<nsISVGTextContainerFrame> svg_parent = do_QueryInterface(aParentFrame);
  if (svg_parent)
    rv = NS_NewSVGGlyphFrame(aPresShell, aContent, aParentFrame, &newFrame);
  else
    rv = NS_NewTextFrame(aPresShell, &newFrame);
#else
  nsresult rv = NS_NewTextFrame(aPresShell, &newFrame);
#endif
  if (NS_FAILED(rv) || !newFrame)
    return rv;

  // Set the frame state bit for text frames to mark them as replaced.
  // XXX kipp: temporary
  newFrame->AddStateBits(NS_FRAME_REPLACED_ELEMENT);

  InitAndRestoreFrame(aPresContext, aState, aContent, aParentFrame,
                      aStyleContext, nsnull, newFrame);

  // We never need to create a view for a text frame.

  // Set the frame's initial child list to null.
  newFrame->SetInitialChildList(aPresContext, nsnull, nsnull);

  // Add the newly constructed frame to the flow
  aFrameItems.AddChild(newFrame);

  // Text frames don't go in the content->frame hash table, because
  // they're anonymous. This keeps the hash table smaller

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructHTMLFrame(nsIPresShell*            aPresShell, 
                                          nsPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsIAtom*                 aTag,
                                          PRInt32                  aNameSpaceID,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems)
{
  // Ignore the tag if it's not HTML content and if it doesn't extend (via XBL)
  // a valid HTML namespace.
  if (!aContent->IsContentOfType(nsIContent::eHTML) &&
      aNameSpaceID != kNameSpaceID_XHTML) {
    return NS_OK;
  }

  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    canBePositioned = PR_TRUE;
  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    isReplaced = PR_FALSE;
  PRBool    addToHashTable = PR_TRUE;
  PRBool    isFloatContainer = PR_FALSE;
  PRBool    isPositionedContainingBlock = PR_FALSE;
  nsresult  rv = NS_OK;

  // See if the element is absolute or fixed positioned
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  if (NS_STYLE_POSITION_ABSOLUTE == display->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  }
  else if (NS_STYLE_POSITION_FIXED == display->mPosition) {
    isFixedPositioned = PR_TRUE;
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
    rv = CreateInputFrame(aPresShell, aPresContext,
                          aContent, newFrame, aStyleContext);
  }
  else if (nsHTMLAtoms::textarea == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    isReplaced = PR_TRUE;
    rv = NS_NewTextControlFrame(aPresShell, &newFrame);
  }
  else if (nsHTMLAtoms::select == aTag) {
    if (!gUseXBLForms) {
      if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
        ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
      }
      isReplaced = PR_TRUE;
      rv = ConstructSelectFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                aTag, aStyleContext, newFrame,  processChildren,
                                display, frameHasBeenInitialized,
                                aFrameItems);
    }
  }
  else if (nsHTMLAtoms::object == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    isReplaced = PR_TRUE;
    rv = NS_NewObjectFrame(aPresShell, &newFrame);
    processChildren = PR_FALSE;
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
    isReplaced = PR_TRUE;
    rv = NS_NewObjectFrame(aPresShell, &newFrame);
  }
  else if (nsHTMLAtoms::fieldset == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    rv = ConstructFieldSetFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                                aTag, aStyleContext, newFrame, processChildren,
                                display, frameHasBeenInitialized);
    processChildren = PR_FALSE;
  }
  else if (nsHTMLAtoms::legend == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    rv = NS_NewLegendFrame(aPresShell, &newFrame);
    processChildren = PR_TRUE;
    canBePositioned = PR_FALSE;
  }
  else if (nsHTMLAtoms::frameset == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    
    canBePositioned = PR_FALSE;
    
    PRBool allowSubframes = PR_TRUE;
    if (aPresContext) {
      nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        docShell->GetAllowSubframes(&allowSubframes);
      }
    }
    if (allowSubframes) {
      rv = NS_NewHTMLFramesetFrame(aPresShell, &newFrame);
    }
  }
  else if (nsHTMLAtoms::iframe == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    
    isReplaced = PR_TRUE;
    PRBool allowSubframes = PR_TRUE;
    if (aPresContext) {
      nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        docShell->GetAllowSubframes(&allowSubframes);
      }
    }
    if (allowSubframes) {
      rv = NS_NewSubDocumentFrame(aPresShell, &newFrame);
      if (newFrame) {
        // the nsSubDocumentFrame needs to know about its content parent during ::Init.
        // there is no reasonable way to get the value there.
        // so we store it as a frame property.
        nsCOMPtr<nsIAtom> contentParentAtom = do_GetAtom("contentParent");
        aPresContext->PropertyTable()->SetProperty(newFrame, contentParentAtom,
                                                   aParentFrame, nsnull,
                                                   nsnull);
      }
    }
  }
  else if (nsHTMLAtoms::noframes == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    isReplaced = PR_TRUE;

    PRBool allowSubframes = PR_TRUE;
    if (aPresContext) {
      nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        docShell->GetAllowSubframes(&allowSubframes);
      }
    }
    if (allowSubframes) {
      // make <noframes> be display:none if frames are enabled
      nsStyleDisplay* mutdisplay = (nsStyleDisplay*)aStyleContext->GetUniqueStyleData(eStyleStruct_Display);
      mutdisplay->mDisplay = NS_STYLE_DISPLAY_NONE;
      aState.mFrameManager->SetUndisplayedContent(aContent, aStyleContext);
    } 
    else {
      processChildren = PR_TRUE;
      rv = NS_NewBlockFrame(aPresShell, &newFrame);
    }
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
  else if (nsHTMLAtoms::isindex == aTag) {
    if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
    }
    isReplaced = PR_TRUE;
    rv = NS_NewIsIndexFrame(aPresShell, &newFrame);
  }

  if (NS_FAILED(rv) || !newFrame)
    return rv;

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list

  // If the frame is a replaced element, then set the frame state bit
  if (isReplaced) {
    newFrame->AddStateBits(NS_FRAME_REPLACED_ELEMENT);
  }

  if (!frameHasBeenInitialized) {
    nsIFrame* geometricParent = aState.GetGeometricParent(display,
                                                          aParentFrame,
                                                          canBePositioned);
     
    rv = InitAndRestoreFrame(aPresContext, aState, aContent, 
                             geometricParent, aStyleContext, nsnull, newFrame);
    if (rv == NS_ERROR_FRAME_REPLACED) {
      // The frame called CantRenderReplacedElement from inside Init().  That
      // failed to do anything useful, since the frame was not in the frame
      // tree yet... Create an alternate frame ourselves
      newFrame->Destroy(aPresContext);

      if (aTag != nsHTMLAtoms::img && aTag != nsHTMLAtoms::input) {
        // XXXbz This should really be made to work for <object> too...
        return NS_OK;
      }

      // Try to construct the alternate frame
      newFrame = nsnull;
      rv = ConstructAlternateFrame(aPresShell, aPresContext, aContent,
                                   aStyleContext, geometricParent,
                                   aParentFrame, newFrame);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(newFrame, "ConstructAlternateFrame needs better error-checking");
    } else {
      NS_ASSERTION(NS_SUCCEEDED(rv), "InitAndRestoreFrame failed");
      // See if we need to create a view, e.g. the frame is absolutely
      // positioned
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        if (isPositionedContainingBlock) {
          // The area frame becomes a container for child frames that are
          // absolutely positioned
          nsFrameConstructorSaveState absoluteSaveState;
          aState.PushAbsoluteContainingBlock(aPresContext, newFrame, absoluteSaveState);
        
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
        else if (isFloatContainer) {
          // If the frame can contain floats, then push a float
          // containing block
          PRBool haveFirstLetterStyle, haveFirstLineStyle;
          HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                                &haveFirstLetterStyle, &haveFirstLineStyle);
          nsFrameConstructorSaveState floatSaveState;
          aState.PushFloatContainingBlock(newFrame, floatSaveState,
                                            PR_FALSE, PR_FALSE);
        
          // Process the child frames
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                               PR_TRUE, childItems, PR_FALSE);
        
          // Set the frame's float list if there were any floated children
          if (aState.mFloatedItems.childList) {
            newFrame->SetInitialChildList(aPresContext,
                                          nsLayoutAtoms::floatList,
                                          aState.mFloatedItems.childList);
          }

        } else {
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame,
                               PR_TRUE, childItems, PR_FALSE);
        }
      }

      // if there are any anonymous children create frames for them
      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                            PR_FALSE, childItems);

      // Set the frame's initial child list
      if (childItems.childList) {
        newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
      }
    }
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

  } else if (display->IsFloating()) {
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

  return rv;
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
static void
LocateAnonymousFrame(nsPresContext* aPresContext,
                     nsIFrame*       aParentFrame,
                     nsIContent*     aTargetContent,
                     nsIFrame**      aResult)
{
  // We may be a placeholder.  If we are, go to the real frame.
  aParentFrame = GetRealFrame(aParentFrame);

  // Check ourselves.
  *aResult = nsnull;
  if (aParentFrame->GetContent() == aTargetContent) {
    // We must take into account if the parent is a scrollframe. If it is, we
    // need to bypass the scrolling mechanics and get at the true frame.
    nsCOMPtr<nsIScrollableFrame> scrollFrame ( do_QueryInterface(aParentFrame) );
    if (scrollFrame)
      *aResult = scrollFrame->GetScrolledFrame();
    else
      *aResult = aParentFrame;
    return;
  }

  // Check our kids.
  nsIFrame* currFrame = aParentFrame->GetFirstChild(nsnull);
  while (currFrame) {
    LocateAnonymousFrame(aPresContext, currFrame, aTargetContent, aResult);
    if (*aResult)
      return;
    currFrame = currFrame->GetNextSibling();
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
nsCSSFrameConstructor::CreateAnonymousFrames(nsIPresShell*            aPresShell, 
                                             nsPresContext*          aPresContext,
                                             nsIAtom*                 aTag,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIFrame*                aNewFrame,
                                             PRBool                   aAppendToExisting,
                                             nsFrameItems&            aChildItems,
                                             PRBool                   aIsRoot)
{
  // See if we might have anonymous content
  // by looking at the tag rather than doing a QueryInterface on
  // the frame.  Only these tags' frames can have anonymous content
  // through nsIAnonymousContentCreator.  We do this check for
  // performance reasons. If we did a QueryInterface on every tag it
  // would be inefficient.

  // nsGenericElement::SetDocument ought to keep a list like this one,
  // but it can't because scroll frames get around this.
  if (!aIsRoot &&
      aTag != nsHTMLAtoms::input &&
      aTag != nsHTMLAtoms::textarea &&
      aTag != nsHTMLAtoms::combobox &&
      aTag != nsHTMLAtoms::isindex &&
      aTag != nsXULAtoms::scrollbar
      )
    return NS_OK;

  return CreateAnonymousFrames(aPresShell, aPresContext, aState, aParent,
                               mDocument, aNewFrame, aAppendToExisting, aChildItems);
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsIPresShell*            aPresShell, 
                                             nsPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIDocument*             aDocument,
                                             nsIFrame*                aParentFrame,
                                             PRBool                   aAppendToExisting,
                                             nsFrameItems&            aChildItems)
{
  nsCOMPtr<nsIAnonymousContentCreator> creator(do_QueryInterface(aParentFrame));

  if (!creator)
    return NS_OK;

#ifdef MOZ_XTF
  PRBool forceBindingParent = PR_FALSE;
  nsCOMPtr<nsIXTFVisualWrapperPrivate> xtfElem = do_QueryInterface(aParent);
  if (xtfElem) {
    if (xtfElem->ApplyDocumentStyleSheets()) 
      forceBindingParent = PR_TRUE;
  }
#endif

  nsCOMPtr<nsISupportsArray> anonymousItems;
  NS_NewISupportsArray(getter_AddRefs(anonymousItems));

  creator->CreateAnonymousContent(aPresContext, *anonymousItems);
  
  PRUint32 count = 0;
  anonymousItems->Count(&count);

  if (count) {
    // A content element can have multiple sources of anonymous content. For example,
    // SELECTs have a combobox dropdown button and also scrollbars in the list view.
    // nsPresShell doesn't handle this very well. It's a problem because a reframe could
    // cause anonymous content from one source to be destroyed and recreated while
    // (in theory) leaving the rest intact, but the presshell doesn't have a way of tracking
    // the anonymous content at that granularity.

    // So what we're doing right now is wiping out existing content whenever we get new
    // anonymous content, except for the one case we care about where there are multiple
    // sources (SELECTs). This case is handled by having SELECT initialization tell us
    // explicitly not to wipe out the scrollbars when the combobox anonymous content is
    // added.
    // Note that we only wipe out existing content when there is actual new content.
    // Otherwise we wipe out scrollbars and other anonymous content when we check sources
    // that never provide anonymous content (e.g. the call to CreateAnonymousFrames
    // from ConstructBlock).

    // What we SHOULD do is get rid of the presshell's need to track anonymous
    // content. It's only used for cleanup as far as I can tell.
    if (!aAppendToExisting) {
      aPresShell->SetAnonymousContentFor(aParent, nsnull);
    }

    // Inform the pres shell about the anonymous content
    aPresShell->SetAnonymousContentFor(aParent, anonymousItems);

    for (PRUint32 i=0; i < count; i++) {
      // get our child's content and set its parent to our content
      nsCOMPtr<nsIContent> content;
      if (NS_FAILED(anonymousItems->QueryElementAt(i, NS_GET_IID(nsIContent), getter_AddRefs(content))))
        continue;

      content->SetNativeAnonymous(PR_TRUE);
      content->SetParent(aParent);
      content->SetDocument(aDocument, PR_TRUE, PR_TRUE);

      nsresult rv;
#ifdef MOZ_XUL
      // Only cut XUL scrollbars off if they're not in a XUL document.
      // This allows scrollbars to be styled from XUL (although not
      // from XML or HTML).
      nsINodeInfo *ni = content->GetNodeInfo();

      if (ni && (ni->Equals(nsXULAtoms::scrollbar, kNameSpaceID_XUL) ||
                 ni->Equals(nsXULAtoms::scrollcorner, kNameSpaceID_XUL))) {
        nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(aDocument));
        if (xulDoc)
          rv = content->SetBindingParent(aParent);
        else
          rv = content->SetBindingParent(content);
      }
      else
#endif
#ifdef MOZ_XTF
      if (forceBindingParent)
        rv = content->SetBindingParent(aParent);
      else
#endif
        rv = content->SetBindingParent(content);

      NS_ENSURE_SUCCESS(rv, rv);

      nsIFrame * newFrame = nsnull;
      rv = creator->CreateFrameFor(aPresContext, content, &newFrame);
      if (NS_SUCCEEDED(rv) && newFrame != nsnull) {
        aChildItems.AddChild(newFrame);
      }
      else {
        // create the frame and attach it to our frame
        ConstructFrame(aPresShell, aPresContext, aState, content, aParentFrame, aChildItems);
      }

      creator->PostCreateFrames();
    }
  }

  return NS_OK;
}

static
PRBool IsXULDisplayType(const nsStyleDisplay* aDisplay)
{
  return (aDisplay->mDisplay == NS_STYLE_DISPLAY_INLINE_BOX || 
          aDisplay->mDisplay == NS_STYLE_DISPLAY_INLINE_GRID || 
          aDisplay->mDisplay == NS_STYLE_DISPLAY_INLINE_STACK ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_BOX ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_GRID ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_STACK ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_GRID_GROUP ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_GRID_LINE ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_DECK ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_POPUP ||
          aDisplay->mDisplay == NS_STYLE_DISPLAY_GROUPBOX
          );
}

nsresult
nsCSSFrameConstructor::ConstructXULFrame(nsIPresShell*            aPresShell, 
                                         nsPresContext*          aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         PRInt32                  aNameSpaceID,
                                         nsStyleContext*          aStyleContext,
                                         nsFrameItems&            aFrameItems,
                                         PRBool                   aXBLBaseTag,
                                         PRBool&                  aHaltProcessing)
{
  PRBool    primaryFrameSet = PR_FALSE;
  PRBool    processChildren = PR_FALSE;  // whether we should process child content
  PRBool    processAnonymousChildren = PR_FALSE; // whether or not we process anonymous content.
  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isPopup = PR_FALSE;
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

  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  
  PRBool isXULNS = (aNameSpaceID == kNameSpaceID_XUL);
  PRBool isXULDisplay = IsXULDisplayType(display);

  if (isXULNS || isXULDisplay) {
    // See if the element is absolutely positioned
    if (NS_STYLE_POSITION_ABSOLUTE == display->mPosition)
      isAbsolutelyPositioned = PR_TRUE;

    PRBool mayBeScrollable = PR_FALSE;

    if (isXULNS) {
      // First try creating a frame based on the tag
#ifdef MOZ_XUL
      // BUTTON CONSTRUCTION
      if (aTag == nsXULAtoms::button || aTag == nsXULAtoms::checkbox || aTag == nsXULAtoms::radio) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewButtonBoxFrame(aPresShell, &newFrame);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of BUTTON CONSTRUCTION logic
      // AUTOREPEATBUTTON CONSTRUCTION
      else if (aTag == nsXULAtoms::autorepeatbutton) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewAutoRepeatBoxFrame(aPresShell, &newFrame);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of AUTOREPEATBUTTON CONSTRUCTION logic


      // TITLEBAR CONSTRUCTION
      else if (aTag == nsXULAtoms::titlebar) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewTitleBarFrame(aPresShell, &newFrame);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of TITLEBAR CONSTRUCTION logic

      // RESIZER CONSTRUCTION
      else if (aTag == nsXULAtoms::resizer) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewResizerFrame(aPresShell, &newFrame);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of RESIZER CONSTRUCTION logic

      else if (aTag == nsXULAtoms::image) {
        isReplaced = PR_TRUE;
        rv = NS_NewImageBoxFrame(aPresShell, &newFrame);
      }
      else if (aTag == nsXULAtoms::spring ||
               aTag == nsHTMLAtoms::spacer) {
        isReplaced = PR_TRUE;
        rv = NS_NewLeafBoxFrame(aPresShell, &newFrame);
      }
       else if (aTag == nsXULAtoms::treechildren) {
        isReplaced = PR_TRUE;
        rv = NS_NewTreeBodyFrame(aPresShell, &newFrame);
      }
      else if (aTag == nsXULAtoms::treecol) {
        isReplaced = PR_TRUE;
        processChildren = PR_TRUE;
        rv = NS_NewTreeColFrame(aPresShell, &newFrame);
      }
#ifdef MOZ_ENABLE_CAIRO
      else if (aTag == nsXULAtoms::canvas) {
        isReplaced = PR_TRUE;
        rv = NS_NewCanvasXULFrame(aPresShell, &newFrame);
      }
#endif
      // TEXT CONSTRUCTION
      else if (aTag == nsXULAtoms::text || aTag == nsHTMLAtoms::label ||
               aTag == nsXULAtoms::description) {
        if ((aTag == nsHTMLAtoms::label || aTag == nsXULAtoms::description) && 
            (! aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::value))) {
          processChildren = PR_TRUE;
          rv = NS_NewAreaFrame(aPresShell, &newFrame,
                               NS_BLOCK_SPACE_MGR | NS_BLOCK_SHRINK_WRAP | NS_BLOCK_MARGIN_ROOT);
        }
        else {
          isReplaced = PR_TRUE;
          rv = NS_NewTextBoxFrame(aPresShell, &newFrame);
        }
      }
      // End of TEXT CONSTRUCTION logic

       // Menu Construction    
      else if (aTag == nsXULAtoms::menu ||
               aTag == nsXULAtoms::menuitem || 
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
  #if defined(XP_MAC) || defined(XP_MACOSX) // The Mac uses its native menu bar.
        aHaltProcessing = PR_TRUE;
        return NS_OK;
  #else
        processChildren = PR_TRUE;
        rv = NS_NewMenuBarFrame(aPresShell, &newFrame);
  #endif
      }
      else if (aTag == nsXULAtoms::popupgroup) {
        // This frame contains child popups
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewPopupSetFrame(aPresShell, &newFrame);
        ((nsPopupSetFrame*) newFrame)->SetFrameConstructor(this);
      }
      else if (aTag == nsXULAtoms::iframe || aTag == nsXULAtoms::editor ||
               aTag == nsXULAtoms::browser) {
        isReplaced = PR_TRUE;

        // XXX should turning off frames allow XUL iframes?
        PRBool allowSubframes = PR_TRUE;
        if (aPresContext) {
          nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
          nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
          if (docShell) {
            docShell->GetAllowSubframes(&allowSubframes);
          }
        }
        if (allowSubframes) {
           rv = NS_NewSubDocumentFrame(aPresShell, &newFrame);
        }
      }
      // PROGRESS METER CONSTRUCTION
      else if (aTag == nsXULAtoms::progressmeter) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewProgressMeterFrame(aPresShell, &newFrame);
      }
      // End of PROGRESS METER CONSTRUCTION logic
      else
#endif
      // SLIDER CONSTRUCTION
      if (aTag == nsXULAtoms::slider) {
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
      else if (aTag == nsXULAtoms::nativescrollbar) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewNativeScrollbarFrame(aPresShell, &newFrame);
      }
      // End of SCROLLBAR CONSTRUCTION logic

      // SCROLLBUTTON CONSTRUCTION
      else if (aTag == nsXULAtoms::scrollbarbutton) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewScrollbarButtonFrame(aPresShell, &newFrame);
      }
      // End of SCROLLBUTTON CONSTRUCTION logic

#ifdef MOZ_XUL
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
#endif
    }

    // Display types for XUL start here
    // First is BOX
    if (!newFrame && isXULDisplay) {
      if (display->mDisplay == NS_STYLE_DISPLAY_INLINE_BOX ||
               display->mDisplay == NS_STYLE_DISPLAY_BOX) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;

        rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, nsnull);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of BOX CONSTRUCTION logic
#ifdef MOZ_XUL
      // ------- Begin Grid ---------
      else if ((!aXBLBaseTag && (display->mDisplay == NS_STYLE_DISPLAY_INLINE_GRID ||
                                 display->mDisplay == NS_STYLE_DISPLAY_GRID)) ||
                aTag == nsXULAtoms::grid) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        nsCOMPtr<nsIBoxLayout> layout;
        NS_NewGridLayout2(aPresShell, getter_AddRefs(layout));
        rv = NS_NewBoxFrame(aPresShell, &newFrame, PR_FALSE, layout);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } //------- End Grid ------

      // ------- Begin Rows/Columns ---------
      else if (display->mDisplay == NS_STYLE_DISPLAY_GRID_GROUP) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;

        nsCOMPtr<nsIBoxLayout> layout;
      
        PRBool listboxScrollPort = PR_FALSE;
        
        if (aTag == nsXULAtoms::listboxbody) {
          NS_NewListBoxLayout(aPresShell, layout);

          rv = NS_NewListBoxBodyFrame(aPresShell, &newFrame, PR_FALSE, layout);
          ((nsListBoxBodyFrame*)newFrame)->InitGroup(this, aPresContext);
          listboxScrollPort = PR_TRUE;

          processChildren = PR_FALSE;
        }
        else
        {
          NS_NewGridRowGroupLayout(aPresShell, getter_AddRefs(layout));
          rv = NS_NewGridRowGroupFrame(aPresShell, &newFrame, PR_FALSE, layout);
        }

        // Boxes can scroll.
        if (display->IsScrollableOverflow()) {

          nsIFrame* scrollPort = nsnull;
          if (listboxScrollPort) {
            NS_NewListBoxScrollPortFrame(aPresShell, &scrollPort);
          }
          // set the top to be the newly created scrollframe
          BuildScrollFrame(aPresShell, aPresContext, aState, aContent,
                           aStyleContext, newFrame, aParentFrame, nsnull,
                           topFrame, aStyleContext, scrollPort);

          // we have a scrollframe so the parent becomes the scroll frame.
          aParentFrame = newFrame->GetParent();

          primaryFrameSet = PR_TRUE;

          frameHasBeenInitialized = PR_TRUE;

        } 
      } //------- End Grid ------

      // ------- Begin Row/Column ---------
      else if (display->mDisplay == NS_STYLE_DISPLAY_GRID_LINE) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
      
        nsCOMPtr<nsIBoxLayout> layout;


        NS_NewGridRowLeafLayout(aPresShell, getter_AddRefs(layout));

        if (aTag == nsXULAtoms::listitem)
          rv = NS_NewListItemFrame(aPresShell, &newFrame, PR_FALSE, layout);
        else
          rv = NS_NewGridRowLeafFrame(aPresShell, &newFrame, PR_FALSE, layout);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } //------- End Grid ------
      // End of STACK CONSTRUCTION logic
       // DECK CONSTRUCTION
      else if (display->mDisplay == NS_STYLE_DISPLAY_DECK) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewDeckFrame(aPresShell, &newFrame);
      }
      // End of DECK CONSTRUCTION logic
      else if (display->mDisplay == NS_STYLE_DISPLAY_GROUPBOX) {
        rv = NS_NewGroupBoxFrame(aPresShell, &newFrame);
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } 
      // STACK CONSTRUCTION
      else if (display->mDisplay == NS_STYLE_DISPLAY_STACK ||
               display->mDisplay == NS_STYLE_DISPLAY_INLINE_STACK) {
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;


        rv = NS_NewStackFrame(aPresShell, &newFrame);

        mayBeScrollable = PR_TRUE;
      }
      else if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
        // This is its own frame that derives from
        // box.
        processChildren = PR_TRUE;
        isReplaced = PR_TRUE;
        rv = NS_NewMenuPopupFrame(aPresShell, &newFrame);

        if (aTag == nsXULAtoms::tooltip) {
          nsAutoString defaultTooltip;
          aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::defaultz, defaultTooltip);
          if (defaultTooltip.LowerCaseEqualsLiteral("true")) {
            // Locate the root frame and tell it about the tooltip.
            nsIFrame* rootFrame = aState.mFrameManager->GetRootFrame();
            if (rootFrame)
              rootFrame = rootFrame->GetFirstChild(nsnull);
            nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
            if (rootBox)
              rootBox->SetDefaultTooltip(aContent);
          }
        }

        // If a popup is inside a menu, then the menu understands the complex
        // rules/behavior governing the cascade of multiple menu popups and can handle
        // having the real popup frame placed under it as a child.  
        // If, however, the parent is *not* a menu frame, then we need to create
        // a placeholder frame for the popup, and then we add the popup frame to the
        // root popup set (that manages all such "detached" popups).
        nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(aParentFrame));
        if (!menuFrame)
          isPopup = PR_TRUE;
      } 
#endif
    }

    if (mayBeScrollable && display->IsScrollableOverflow()) {
      // set the top to be the newly created scrollframe
      BuildScrollFrame(aPresShell, aPresContext, aState, aContent,
                       aStyleContext, newFrame, aParentFrame, nsnull,
                       topFrame, aStyleContext);

      // we have a scrollframe so the parent becomes the scroll frame.
      // XXXldb Do we really want to do this?  The one case where it
      // matters when |frameHasBeenInitialized| is true is one where
      // I think we'd be better off the other way around.
      aParentFrame = newFrame->GetParent();
      primaryFrameSet = PR_TRUE;
      frameHasBeenInitialized = PR_TRUE;
    }
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {

    // if no top frame was created then the top is the new frame
    if (topFrame == nsnull)
        topFrame = newFrame;

    // If the frame is a replaced element, then set the frame state bit
    if (isReplaced) {
      newFrame->AddStateBits(NS_FRAME_REPLACED_ELEMENT);
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

      rv = InitAndRestoreFrame(aPresContext, aState, aContent, 
                      geometricParent, aStyleContext, nsnull, newFrame);

      NS_ENSURE_SUCCESS(rv,rv);
      
      /*
      // if our parent is a block frame then do things the way html likes it
      // if not then we are in a box so do what boxes like. On example is boxes
      // do not support the absolute positioning of their children. While html blocks
      // thats why we call different things here.
      nsIAtom* frameType = geometricParent->GetType();
      if ((frameType == nsLayoutAtoms::blockFrame) ||
          (frameType == nsLayoutAtoms::areaFrame)) {
      */
        // See if we need to create a view, e.g. the frame is absolutely positioned
        nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

      /*
      } else {
          // we are in a box so do the box thing.
        nsBoxFrame::CreateViewForFrame(aPresContext, newFrame,
                                                 aStyleContext, PR_FALSE);
      }
      */
      
    }

    // Process the child content if requested
    if (processChildren || processAnonymousChildren) {
      nsFrameItems childItems;
      if (processChildren) {
        mDocument->GetBindingManager()->ShouldBuildChildFrames(aContent,
                                                               &processChildren);
        if (processChildren)
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent,
                               newFrame, PR_FALSE, childItems, PR_FALSE);
      }
      
      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent,
                            newFrame, PR_FALSE, childItems);

      // Set the frame's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    }

    // If the frame is absolutely positioned, then create a placeholder frame
    if (isAbsolutelyPositioned || isFixedPositioned || isPopup) {
      nsIFrame* placeholderFrame;

      CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                                newFrame, aStyleContext, aParentFrame, &placeholderFrame);

      // Add the positioned frame to its containing block's list of child frames
      if (isAbsolutelyPositioned) {
        aState.mAbsoluteItems.AddChild(newFrame);
      } else if (isFixedPositioned) {
        aState.mFixedItems.AddChild(newFrame);
      }
#ifdef MOZ_XUL
      else if (isPopup) {
        // Locate the root popup set and add ourselves to the popup set's list
        // of popup frames.
        nsIFrame* rootFrame = aState.mFrameManager->GetRootFrame();
        if (rootFrame)
          rootFrame = rootFrame->GetFirstChild(nsnull);
        nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
        NS_ASSERTION(rootBox, "unexpected null pointer");
        if (rootBox) {
          nsIFrame* popupSetFrame;
          rootBox->GetPopupSetFrame(&popupSetFrame);
          NS_ASSERTION(popupSetFrame, "unexpected null pointer");
          if (popupSetFrame) {
            nsCOMPtr<nsIPopupSetFrame> popupSet(do_QueryInterface(popupSetFrame));
            NS_ASSERTION(popupSet, "unexpected null pointer");
            if (popupSet)
              popupSet->AddPopupFrame(newFrame);
          }
        }
      }
#endif

      // Add the placeholder frame to the flow
      aFrameItems.AddChild(placeholderFrame);
    }
    else
      // Add the new frame to our list of frame items.
      aFrameItems.AddChild(topFrame);
  }

#ifdef MOZ_XUL
  // register tooltip support if needed
  nsAutoString value;
  if (aTag == nsXULAtoms::treechildren || // trees always need titletips
      aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::tooltiptext, value) !=
        NS_CONTENT_ATTR_NOT_THERE ||
      aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::tooltip, value) !=
        NS_CONTENT_ATTR_NOT_THERE)
  {
    nsIFrame* rootFrame = aState.mFrameManager->GetRootFrame();
    if (rootFrame)
      rootFrame = rootFrame->GetFirstChild(nsnull);
    nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
    if (rootBox)
      rootBox->AddTooltipSupport(aContent);
  }
#endif

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

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::BeginBuildingScrollFrame(nsIPresShell*            aPresShell, 
                                                nsPresContext*          aPresContext,
                                                nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsStyleContext*          aContentStyle,
                                                nsIFrame*                aParentFrame,
                                                nsIFrame*                aContentParentFrame,
                                                nsIAtom*                 aScrolledPseudo,
                                                nsIDocument*             aDocument,
                                                PRBool                   aIsRoot,
                                                nsIFrame*&               aNewFrame,
                                                nsIFrame*&               aScrollableFrame,
                                                nsIFrame*                aScrollPortFrame)
{
  // Check to see the type of parent frame so we know whether we need to 
  // turn off/on scaling for the scrollbars
  //
  // If the parent is a viewportFrame then we are the scrollbars for the UI
  // if not then we are scrollbars inside the document.
  PRBool noScalingOfTwips = PR_FALSE;
  PRBool isPrintPreview =
    aPresContext->Type() == nsPresContext::eContext_PrintPreview;

  if (isPrintPreview) {
    noScalingOfTwips = aParentFrame->GetType() == nsLayoutAtoms::viewportFrame;
    if (noScalingOfTwips) {
      aPresContext->SetScalingOfTwips(PR_FALSE);
    }
  }

  nsIFrame* scrollFrame = nsnull;
  nsIFrame* parentFrame = nsnull;
  nsIFrame* gfxScrollFrame = aNewFrame;

  nsFrameItems anonymousItems;

  nsRefPtr<nsStyleContext> contentStyle = aContentStyle;

  if (!gfxScrollFrame) {
    // Build a XULScrollFrame when the parent is a box, because XULScrollFrames
    // do box layout well. Otherwise build an HTMLScrollFrame.
    if (aParentFrame->IsBoxFrame()) {
      NS_NewXULScrollFrame(aPresShell, &gfxScrollFrame, aIsRoot);
    } else {
      NS_NewHTMLScrollFrame(aPresShell, &gfxScrollFrame, aIsRoot);
    }

    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aParentFrame, contentStyle, nsnull, gfxScrollFrame);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(gfxScrollFrame,
                                             aContentParentFrame, PR_FALSE);
  }

  InitGfxScrollFrame(aPresShell, aPresContext, aState, aContent, aDocument,
                     aParentFrame, aContentParentFrame, contentStyle,
                     aIsRoot, gfxScrollFrame, anonymousItems, aScrollPortFrame);

  scrollFrame = anonymousItems.childList; // get the scrollport from the anonymous list
  parentFrame = gfxScrollFrame;
  aNewFrame = gfxScrollFrame;

  // we used the style that was passed in. So resolve another one.
  nsRefPtr<nsStyleContext> scrollPseudoStyle;
  nsStyleSet *styleSet = aPresShell->StyleSet();

  scrollPseudoStyle = styleSet->ResolvePseudoStyleFor(aContent,
                                                      nsCSSAnonBoxes::scrolledContent,
                                                      contentStyle);

  contentStyle = scrollPseudoStyle;
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      parentFrame, contentStyle, nsnull, scrollFrame);

  nsStyleContext* aScrolledChildStyle = styleSet->ResolvePseudoStyleFor(aContent,
                                                                        aScrolledPseudo,
                                                                        contentStyle).get();

  aScrollableFrame = scrollFrame;
  
  // set the child frame for the gfxscrollbar if the is one. This frames will be the 
  // 2 scrollbars and the scrolled frame.
  if (gfxScrollFrame) {
     gfxScrollFrame->SetInitialChildList(aPresContext, nsnull, anonymousItems.childList);
  }


  if (isPrintPreview && noScalingOfTwips) {
    aPresContext->SetScalingOfTwips(PR_TRUE);
  }

  return aScrolledChildStyle;;
}

nsresult
nsCSSFrameConstructor::FinishBuildingScrollFrame(nsPresContext*      aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIFrame*                aScrollFrame,
                                             nsIFrame*                aScrolledFrame,
                                             nsStyleContext*          aScrolledContentStyle)
                                             
{
  // create a view
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aScrolledFrame, nsnull, PR_TRUE);

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
 *                  If this is not null, we'll just use it
 * @param aScrolledContentStyle the style that was resolved for the scrolled frame. (returned)
 */
nsresult
nsCSSFrameConstructor::BuildScrollFrame(nsIPresShell*            aPresShell, 
                                        nsPresContext*          aPresContext,
                                        nsFrameConstructorState& aState,
                                        nsIContent*              aContent,
                                        nsStyleContext*          aContentStyle,
                                        nsIFrame*                aScrolledFrame,
                                        nsIFrame*                aParentFrame,
                                        nsIFrame*                aContentParentFrame,
                                        nsIFrame*&               aNewFrame, 
                                        nsStyleContext*&         aScrolledContentStyle,
                                        nsIFrame*                aScrollPortFrame)
{
    // Check to see the type of parent frame so we know whether we need to 
    // turn off/on scaling for the scrollbars
    //
    // If the parent is a viewportFrame then we are the scrollbars for the UI
    // if not then we are scrollbars inside the document.
    PRBool noScalingOfTwips = PR_FALSE;
    if (aPresContext->Type() == nsPresContext::eContext_PrintPreview) {
      noScalingOfTwips = aParentFrame->GetType() == nsLayoutAtoms::viewportFrame;
      if (noScalingOfTwips) {
        aPresContext->SetScalingOfTwips(PR_FALSE);
      }
    }

    nsIFrame *scrollFrame;

    nsRefPtr<nsStyleContext> scrolledContentStyle;

    
    scrolledContentStyle = BeginBuildingScrollFrame(aPresShell, aPresContext,
                                                    aState,
                                                    aContent,
                                                    aContentStyle,
                                                    aParentFrame,
                                                    aContentParentFrame,
                                                    nsCSSAnonBoxes::scrolledContent,
                                                    mDocument,
                                                    PR_FALSE,
                                                    aNewFrame,
                                                    scrollFrame,
                                                    aScrollPortFrame);
    
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

    if (noScalingOfTwips) {
      aPresContext->SetScalingOfTwips(PR_TRUE);
    }

    return NS_OK;

}

/** 
 * If we are building GFX scrollframes this will create one
 * if aNewFrame gets a frame passed in, we'll use it instead
 * of creating a new frame
 */
nsresult
nsCSSFrameConstructor::InitGfxScrollFrame(nsIPresShell*            aPresShell, 
                                          nsPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIDocument*             aDocument,
                                          nsIFrame*                aParentFrame,
                                          nsIFrame*                aContentParentFrame,
                                          nsStyleContext*          aStyleContext,
                                          PRBool                   aIsRoot,
                                          nsIFrame*&               aNewFrame,
                                          nsFrameItems&            aAnonymousFrames,
                                          nsIFrame*                aScrollPortFrame)
{
  if (!aScrollPortFrame)
    NS_NewScrollPortFrame(aPresShell, &aScrollPortFrame);

  aAnonymousFrames.AddChild(aScrollPortFrame);

  // if there are any anonymous children for the scroll frame, create frames for them.
  CreateAnonymousFrames(aPresShell, aPresContext, aState, aContent, aDocument, aNewFrame,
                        PR_FALSE, aAnonymousFrames);

  return NS_OK;
} 

nsresult
nsCSSFrameConstructor::ConstructFrameByDisplayType(nsIPresShell*            aPresShell, 
                                                   nsPresContext*          aPresContext,
                                                   nsFrameConstructorState& aState,
                                                   const nsStyleDisplay*    aDisplay,
                                                   nsIContent*              aContent,
                                                   PRInt32                  aNameSpaceID,
                                                   nsIAtom*                 aTag,
                                                   nsIFrame*                aParentFrame,
                                                   nsStyleContext*          aStyleContext,
                                                   nsFrameItems&            aFrameItems)
{
  PRBool    primaryFrameSet = PR_FALSE;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  nsIFrame* newBlock = nsnull;
  nsIFrame* nextInline = nsnull;
  nsTableCreator tableCreator(aPresShell); // Used to make table frames.
  PRBool    addToHashTable = PR_TRUE;
  PRBool    pseudoParent = PR_FALSE; // is the new frame's parent anonymous
  nsresult  rv = NS_OK;
  PRBool    addNewFrameToChildList = PR_TRUE; 

  // The style system ensures that floated and positioned frames are
  // block-level.
  NS_ASSERTION(!(aDisplay->IsFloating() ||
                 aDisplay->IsAbsolutelyPositioned()) ||
               aDisplay->IsBlockLevel(),
               "Style system did not apply CSS2.1 section 9.7 fixups");

  // See if it's absolute positioned or fixed positioned.
  if (NS_STYLE_POSITION_ABSOLUTE == aDisplay->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  } else if (NS_STYLE_POSITION_FIXED == aDisplay->mPosition) {
    isFixedPositioned = PR_TRUE;
  }

  nsIFrame* adjParentFrame = aParentFrame;
  // if the new frame is not table related and the parent is a table, row group, or row,
  // then we need to get or create the pseudo table cell frame and use it as the parent.
  if (adjParentFrame) {
    if (!IsTableRelated(aDisplay->mDisplay, PR_TRUE) &&
        IsTableRelated(adjParentFrame->GetType(), PR_FALSE) &&
        aTag != nsHTMLAtoms::form) {
      GetPseudoCellFrame(aPresShell, aPresContext, tableCreator, aState, *adjParentFrame);
      if (aState.mPseudoFrames.mCellInner.mFrame) {
        adjParentFrame = aState.mPseudoFrames.mCellInner.mFrame;
        pseudoParent = PR_TRUE;
      }
      else {
        return NS_ERROR_FAILURE;
      }
    }
  }

  nsFrameConstructorSaveState pseudoSaveState;
  if (pseudoParent) {
    // We pushed an anonymous table cell.  The inner block of this
    // needs to become the float containing block.  Luckily, it's
    // hanging about in adjParentFrame!
    NS_ASSERTION(adjParentFrame == aState.mPseudoFrames.mCellInner.mFrame,
                 "Weird parent frame in ConstructFrameByDisplayType");
    aState.PushFloatContainingBlock(adjParentFrame, pseudoSaveState,
                                    PR_FALSE, PR_FALSE);
  }

  // If this is "body", try propagating its scroll style to the viewport
  // Note that we need to do this even if the body is NOT scrollable;
  // it might have dynamically changed from scrollable to not scrollable,
  // and that might need to be propagated.
  PRBool propagatedScrollToViewport = PR_FALSE;
  if (aContent->GetNodeInfo()->Equals(nsHTMLAtoms::body) &&
      aContent->IsContentOfType(nsIContent::eHTML)) {
    propagatedScrollToViewport =
      PropagateScrollToViewport(aPresContext) == aContent;
  }

  // If the frame is a block-level frame and is scrollable, then wrap it
  // in a scroll frame.
  // XXX Ignore tables for the time being
  if (aDisplay->IsBlockLevel() &&
      aDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE &&
      aDisplay->IsScrollableOverflow() &&
      !propagatedScrollToViewport) {

    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }

     // Initialize it
    nsIFrame* scrolledFrame = nsnull;

    NS_NewAreaFrame(aPresShell, &scrolledFrame, NS_BLOCK_SPACE_MGR |
                    NS_BLOCK_SHRINK_WRAP | NS_BLOCK_MARGIN_ROOT);


    nsStyleContext* newStyle;
    // Build the scrollframe it
    BuildScrollFrame(aPresShell, aPresContext, aState, aContent, aStyleContext,
                     scrolledFrame,
                     aState.GetGeometricParent(aDisplay, adjParentFrame),
                     adjParentFrame, newFrame, newStyle);

    // buildscrollframe sets the primary frame.
    primaryFrameSet = PR_TRUE;
    
    //-----

    // The area frame is a float container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(scrolledFrame, floatSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;
    PRBool isPositionedContainingBlock = aDisplay->IsPositioned();

    if (isPositionedContainingBlock) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(aPresContext, scrolledFrame, absoluteSaveState);
    }
     
    ProcessChildren(aPresShell, aPresContext, aState, aContent, scrolledFrame, PR_TRUE,
                    childItems, PR_TRUE);

    CreateAnonymousFrames(aPresShell, aPresContext, aContent->Tag(), aState,
                          aContent, newFrame, PR_FALSE, childItems);

    // Set the scrolled frame's initial child lists
    scrolledFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (isPositionedContainingBlock && aState.mAbsoluteItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::absoluteList,
                                         aState.mAbsoluteItems.childList);
    }

    if (aState.mFloatedItems.childList) {
      scrolledFrame->SetInitialChildList(aPresContext,
                                         nsLayoutAtoms::floatList,
                                         aState.mFloatedItems.childList);
    }
    ///------
  }
  // See if the frame is absolute or fixed positioned
  else if (aDisplay->IsAbsolutelyPositioned() &&
           (NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay ||
            NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {

    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }

    // Create a frame to wrap up the absolute positioned item
    NS_NewAbsoluteItemWrapperFrame(aPresShell, &newFrame);
    InitAndRestoreFrame(aPresContext, aState, aContent, 
                      (isAbsolutelyPositioned
                       ? aState.mAbsoluteItems.containingBlock
                       : aState.mFixedItems.containingBlock), 
                        aStyleContext, nsnull, newFrame);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, adjParentFrame, PR_FALSE);

    // Process the child content. The area frame becomes a container for child
    // frames that are absolutely positioned
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floatSaveState;
    nsFrameItems                childItems;

    PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
    if (aDisplay->IsBlockLevel()) {
      HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
    }
    aState.PushAbsoluteContainingBlock(aPresContext, newFrame, absoluteSaveState);
    aState.PushFloatContainingBlock(newFrame, floatSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                    childItems, PR_TRUE);

    CreateAnonymousFrames(aPresShell, aPresContext, aContent->Tag(), aState,
                          aContent, newFrame, PR_FALSE, childItems);

    // Set the frame's initial child list(s)
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    if (aState.mAbsoluteItems.childList) {
      newFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                     aState.mAbsoluteItems.childList);
    }
    if (aState.mFloatedItems.childList) {
      newFrame->SetInitialChildList(aPresContext,
                                    nsLayoutAtoms::floatList,
                                    aState.mFloatedItems.childList);
    }
  }
  // See if the frame is floated and it's a block frame
  else if (aDisplay->IsFloating() &&
           (NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay ||
            NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Create an area frame
    NS_NewFloatingItemWrapperFrame(aPresShell, &newFrame);

    ConstructBlock(aPresShell, aPresContext, aState, aDisplay, aContent, 
                   aState.mFloatedItems.containingBlock, adjParentFrame,
                   aStyleContext, newFrame,
                   aDisplay->mPosition == NS_STYLE_POSITION_RELATIVE);
  }
  // See if it's relatively positioned
  else if ((NS_STYLE_POSITION_RELATIVE == aDisplay->mPosition) &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {
    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Is it block-level or inline-level?
    if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
      // Create a wrapper frame. No space manager, though
      NS_NewRelativeItemWrapperFrame(aPresShell, &newFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      ConstructBlock(aPresShell, aPresContext, aState, aDisplay, aContent,
                     adjParentFrame, nsnull, aStyleContext, newFrame, PR_TRUE);
    } else {
      // Create a positioned inline frame
      NS_NewPositionedInlineFrame(aPresShell, &newFrame);
      ConstructInline(aPresShell, aPresContext, aState, aDisplay, aContent,
                      adjParentFrame, aStyleContext, PR_TRUE, newFrame,
                      &newBlock, &nextInline);
    }
  }
  // See if it's a block frame of some sort
  else if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_RUN_IN == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_COMPACT == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay->mDisplay)) {
    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Create the block frame
    rv = NS_NewBlockFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) { // That worked so construct the block and its children
      nsPseudoFrames savePseudo;
      aState.mPseudoFrames.Reset(&savePseudo);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      rv = ConstructBlock(aPresShell, aPresContext, aState, aDisplay, aContent,
                          adjParentFrame, nsnull, aStyleContext, newFrame,
                          PR_FALSE);
      if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
        ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
      }
      aState.mPseudoFrames = savePseudo;
    }
  }
  // See if it's an inline frame of some sort
  else if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_MARKER == aDisplay->mDisplay)) {
    if (!pseudoParent && !aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
      ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
    }
    // Create the inline frame
    rv = NS_NewInlineFrame(aPresShell, &newFrame);
    if (NS_SUCCEEDED(rv)) { // That worked so construct the inline and its children
      nsPseudoFrames savePseudo;
      aState.mPseudoFrames.Reset(&savePseudo);
      rv = ConstructInline(aPresShell, aPresContext, aState, aDisplay, aContent,
                           adjParentFrame, aStyleContext, PR_FALSE, newFrame,
                           &newBlock, &nextInline);
      if (!aState.mPseudoFrames.IsEmpty()) { // process pending pseudo frames
        ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems); 
      }
      aState.mPseudoFrames = savePseudo;
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
      nsIFrame* innerTable;
      rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, 
                               aState.GetGeometricParent(aDisplay,
                                                         adjParentFrame),
                               adjParentFrame, aStyleContext,
                               tableCreator, PR_FALSE, aFrameItems, newFrame,
                               innerTable, pseudoParent);
      // if there is a pseudoParent, then newFrame was added to the pseudo cell's child list
      addNewFrameToChildList = !pseudoParent;
      // Note: table construction function takes care of initializing
      // the frame, processing children, and setting the initial child
      // list
      goto nearly_done;
    }
  
    // the next 5 cases are only relevant if the parent is not a table, ConstructTableFrame handles children
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
    {
      // adjParentFrame may be an inner table frame rather than an outer frame 
      // In this case we need to get the outer frame.
      nsIFrame* parentFrame = adjParentFrame;
      nsIFrame* outerFrame = adjParentFrame->GetParent();
      if (outerFrame) {
        if (nsLayoutAtoms::tableOuterFrame == outerFrame->GetType()) {
          parentFrame = outerFrame;
        }
      }
      rv = ConstructTableCaptionFrame(aPresShell, aPresContext, aState, aContent, 
                                      parentFrame, aStyleContext, tableCreator, 
                                      aFrameItems, newFrame, pseudoParent);
      if (NS_SUCCEEDED(rv) && !pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
    }

    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      rv = ConstructTableRowGroupFrame(aPresShell, aPresContext, aState, aContent, 
                                       adjParentFrame, aStyleContext, tableCreator, 
                                       PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (NS_SUCCEEDED(rv) && !pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;

    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      rv = ConstructTableColGroupFrame(aPresShell, aPresContext, aState, aContent, 
                                       adjParentFrame, aStyleContext, tableCreator, 
                                       PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (NS_SUCCEEDED(rv) && !pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
   
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      rv = ConstructTableColFrame(aPresShell, aPresContext, aState, aContent, 
                                  adjParentFrame, aStyleContext, tableCreator, 
                                  PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (NS_SUCCEEDED(rv) && !pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      rv = ConstructTableRowFrame(aPresShell, aPresContext, aState, aContent, 
                                  adjParentFrame, aStyleContext, tableCreator, 
                                  PR_FALSE, aFrameItems, newFrame, pseudoParent);
      if (NS_SUCCEEDED(rv) && !pseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      {
        nsIFrame* innerTable;
        rv = ConstructTableCellFrame(aPresShell, aPresContext, aState, aContent, 
                                     adjParentFrame, aStyleContext, tableCreator, 
                                     PR_FALSE, aFrameItems, newFrame, innerTable, pseudoParent);
        if (NS_SUCCEEDED(rv) && !pseudoParent) {
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
  nsFrameItems& frameItems = (pseudoParent) ? aState.mPseudoFrames.mCellInner.mChildList 
                                            : aFrameItems;
  if (isAbsolutelyPositioned || isFixedPositioned) {
    nsIFrame* placeholderFrame;

    CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent,
                              newFrame, aStyleContext, adjParentFrame, &placeholderFrame);

    // Add the positioned frame to its containing block's list of child frames
    if (isAbsolutelyPositioned) {
      aState.mAbsoluteItems.AddChild(newFrame);
    } else {
      aState.mFixedItems.AddChild(newFrame);
    }

    // Add the placeholder frame to the flow
    frameItems.AddChild(placeholderFrame);

  } else if (aDisplay->IsFloating()) {
    nsIFrame* placeholderFrame;
    CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager, aContent, newFrame,
                              aStyleContext, adjParentFrame, &placeholderFrame);

    // Add the floating frame to its containing block's list of child frames
    aState.mFloatedItems.AddChild(newFrame);

    // Add the placeholder frame to the flow
    frameItems.AddChild(placeholderFrame);
  } else if (newFrame && addNewFrameToChildList) {
    // Add the frame we just created to the flowed list
    frameItems.AddChild(newFrame);
    if (newBlock) {
      frameItems.AddChild(newBlock);
      if (nextInline) {
        frameItems.AddChild(nextInline);
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
nsCSSFrameConstructor::InitAndRestoreFrame(nsPresContext*          aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsStyleContext*          aStyleContext,
                                           nsIFrame*                aPrevInFlow,
                                           nsIFrame*                aNewFrame)
{
  nsresult rv = NS_OK;
  
  NS_ASSERTION(aNewFrame, "Null frame cannot be initialized");
  if (!aNewFrame)
    return NS_ERROR_NULL_POINTER;

  // Initialize the frame
  rv = aNewFrame->Init(aPresContext, aContent, aParentFrame, 
                       aStyleContext, aPrevInFlow);

  if (aState.mFrameState && aState.mFrameManager) {
    // Restore frame state for just the newly created frame.
    aState.mFrameManager->RestoreFrameStateFor(aNewFrame, aState.mFrameState);
  }

  return rv;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(nsPresContext*   aPresContext,
                                           nsIFrame*         aParentFrame,
                                           nsIContent*       aContent)
{
  // Resolve the style context based on the content object and the parent
  // style context
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();

  // skip past any parents that are scrolled-content. We want to inherit directly
  // from the outer scroll frame.
  while (parentStyleContext && parentStyleContext->GetPseudoType() ==
         nsCSSAnonBoxes::scrolledContent) {
    parentStyleContext = parentStyleContext->GetParent();
  }

  nsStyleSet *styleSet = aPresContext->StyleSet();

  if (aContent->IsContentOfType(nsIContent::eELEMENT)) {
    return styleSet->ResolveStyleFor(aContent, parentStyleContext);
  } else {

    NS_ASSERTION(aContent->Tag() == nsLayoutAtoms::textTagName,
                 "shouldn't waste time creating style contexts for "
                 "comments and processing instructions");

    return styleSet->ResolveStyleForNonElement(parentStyleContext);
  }
}

// MathML Mod - RBS
#ifdef MOZ_MATHML
nsresult
nsCSSFrameConstructor::ConstructMathMLFrame(nsIPresShell*            aPresShell,
                                            nsPresContext*          aPresContext,
                                            nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            PRInt32                  aNameSpaceID,
                                            nsStyleContext*          aStyleContext,
                                            nsFrameItems&            aFrameItems)
{
  // Make sure that we remain confined in the MathML world
  if (aNameSpaceID != kNameSpaceID_MathML) 
    return NS_OK;

  PRBool    processChildren = PR_TRUE;  // Whether we should process child content.
                                        // MathML frames are inline frames.
                                        // processChildren = PR_TRUE for inline frames.
                                        // see case NS_STYLE_DISPLAY_INLINE in
                                        // ConstructFrameByDisplayType()

  nsresult  rv = NS_OK;
  PRBool    isAbsolutelyPositioned = PR_FALSE;
  PRBool    isFixedPositioned = PR_FALSE;
  PRBool    isReplaced = PR_FALSE;
  PRBool    ignoreInterTagWhitespace = PR_TRUE;

  NS_ASSERTION(aTag != nsnull, "null MathML tag");
  if (aTag == nsnull)
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;

  // See if the element is absolute or fixed positioned
  const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();
  if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  }
  else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
    isFixedPositioned = PR_TRUE;
  }

  if (aTag == nsMathMLAtoms::mi_ ||
      aTag == nsMathMLAtoms::mn_ ||
      aTag == nsMathMLAtoms::ms_ ||
      aTag == nsMathMLAtoms::mtext_)
    rv = NS_NewMathMLTokenFrame(aPresShell, &newFrame);
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
  else if (aTag == nsMathMLAtoms::mtable_ &&
           disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // <mtable> is an inline-table -- but this isn't yet supported.
    // What we do here is to wrap the table in an anonymous containing
    // block so that it can mix better with other surrounding MathML markups

    nsStyleContext* parentContext = aParentFrame->GetStyleContext();
    nsStyleSet *styleSet = aPresShell->StyleSet();

    // first, create a MathML mrow frame that will wrap the block frame
    rv = NS_NewMathMLmrowFrame(aPresShell, &newFrame);
    if (NS_FAILED(rv)) return rv;
    nsRefPtr<nsStyleContext> mrowContext;
    mrowContext = styleSet->ResolvePseudoStyleFor(aContent,
                                                  nsCSSAnonBoxes::mozMathInline,
                                                  parentContext);
    InitAndRestoreFrame(aPresContext, aState, aContent, aParentFrame,
                        mrowContext, nsnull, newFrame);

    // then, create a block frame that will wrap the table frame
    nsIFrame* blockFrame;
    rv = NS_NewBlockFrame(aPresShell, &blockFrame);
    if (NS_FAILED(rv)) return rv;
    nsRefPtr<nsStyleContext> blockContext;
    blockContext = styleSet->ResolvePseudoStyleFor(aContent,
                                                   nsCSSAnonBoxes::mozAnonymousBlock,
                                                   mrowContext);
    InitAndRestoreFrame(aPresContext, aState, aContent, newFrame,
                        blockContext, nsnull, blockFrame);

    // then, create the table frame itself
    nsRefPtr<nsStyleContext> tableContext;
    tableContext = styleSet->ResolveStyleFor(aContent, blockContext);

    nsFrameItems tempItems;
    nsIFrame* outerTable;
    nsIFrame* innerTable;
    PRBool pseudoParent;
    nsMathMLmtableCreator mathTableCreator(aPresShell);
    rv = ConstructTableFrame(aPresShell, aPresContext, aState, aContent, 
                             blockFrame, blockFrame, tableContext,
                             mathTableCreator, PR_FALSE, tempItems, outerTable,
                             innerTable, pseudoParent);
    // Note: table construction function takes care of initializing the frame,
    // processing children, and setting the initial child list

    // set the outerTable as the initial child of the anonymous block
    blockFrame->SetInitialChildList(aPresContext, nsnull, outerTable);

    // set the block frame as the initial child of the mrow frame
    newFrame->SetInitialChildList(aPresContext, nsnull, blockFrame);

    // add the new frame to the flow
    aFrameItems.AddChild(newFrame);

    return rv; 
  }
  // End CONSTRUCTION of MTABLE elements 

  else if (aTag == nsMathMLAtoms::math) { 
    // root <math> element
    const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
    PRBool isBlock = (NS_STYLE_DISPLAY_BLOCK == display->mDisplay);
    rv = NS_NewMathMLmathFrame(aPresShell, &newFrame, isBlock);
  }
  else {
    return rv;
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame) {
    // If the frame is a replaced element, then set the frame state bit
    if (isReplaced) {
      newFrame->AddStateBits(NS_FRAME_REPLACED_ELEMENT);
    }
    // record that children that are ignorable whitespace should be excluded
    if (ignoreInterTagWhitespace) {
      newFrame->AddStateBits(NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE);
    }

    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aState.GetGeometricParent(disp, aParentFrame),
                        aStyleContext, nsnull, newFrame);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

    // Process the child content if requested
    nsFrameItems childItems;
    if (processChildren) {
      rv = ProcessChildren(aPresShell, aPresContext, aState, aContent, newFrame, PR_TRUE,
                           childItems, PR_FALSE);

      CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                            PR_FALSE, childItems);
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
    else {
      // Add the new frame to our list of frame items.
      aFrameItems.AddChild(newFrame);
    }
  }
  return rv;
}
#endif // MOZ_MATHML

// XTF 
#ifdef MOZ_XTF
nsresult
nsCSSFrameConstructor::ConstructXTFFrame(nsIPresShell*            aPresShell,
                                         nsPresContext*           aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         PRInt32                  aNameSpaceID,
                                         nsStyleContext*          aStyleContext,
                                         nsFrameItems&            aFrameItems)
{
#ifdef DEBUG
//  printf("nsCSSFrameConstructor::ConstructXTFFrame\n");
#endif
  nsresult  rv = NS_OK;
  PRBool isAbsolutelyPositioned = PR_FALSE;
  PRBool isFixedPositioned = PR_FALSE;
  PRBool forceView = PR_FALSE;
  PRBool isBlock = PR_FALSE;
  
  //NS_ASSERTION(aTag != nsnull, "null XTF tag");
  //if (aTag == nsnull)
  //  return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
   
  // See if the element is absolute or fixed positioned
  const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();
  if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
    isAbsolutelyPositioned = PR_TRUE;
  }
  else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
    isFixedPositioned = PR_TRUE;
  }

  nsCOMPtr<nsIXTFElementWrapperPrivate> xtfElem = do_QueryInterface(aContent);
  NS_ASSERTION(xtfElem, "huh? no xtf element?");
  switch(xtfElem->GetElementType()) {
    case nsIXTFElement::ELEMENT_TYPE_SVG_VISUAL:
#ifdef MOZ_SVG
      rv = NS_NewXTFSVGDisplayFrame(aPresShell, aContent, &newFrame);
#else
      NS_ERROR("xtf svg visuals are only supported in mozilla builds with native svg");
#endif
      break;
    case nsIXTFElement::ELEMENT_TYPE_XML_VISUAL:
    {
      PRBool isBlock = (NS_STYLE_DISPLAY_BLOCK == disp->mDisplay);
      rv = NS_NewXTFXMLDisplayFrame(aPresShell, isBlock, &newFrame);
    }
      break;
    case nsIXTFElement::ELEMENT_TYPE_XUL_VISUAL:
      rv = NS_NewXTFXULDisplayFrame(aPresShell, &newFrame);
      break;
    case nsIXTFElement::ELEMENT_TYPE_GENERIC_ELEMENT:
      NS_ERROR("huh? ELEMENT_TYPE_GENERIC_ELEMENT should have been flagged by caller");
      break;
    default:
      NS_ERROR("unknown xtf frame!");
      return NS_OK;
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {

    nsIFrame* geometricParent = isAbsolutelyPositioned
                              ? aState.mAbsoluteItems.containingBlock
                              : aParentFrame;

    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        geometricParent, aStyleContext, nsnull, newFrame);

    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, forceView);

    // Create anonymous frames before processing children, so that
    // explicit child content can be appended to the correct anonymous
    // frame. Call version of CreateAnonymousFrames that doesn't check
    // tag:
    nsFrameItems childItems;
    CreateAnonymousFrames(aPresShell, aPresContext, aState, aContent, mDocument, newFrame,
                          PR_FALSE, childItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    
    // Process the child content if requested
    nsIFrame *insertionFrame = newFrame->GetContentInsertionFrame();
    if (insertionFrame) {
        nsFrameItems insertionItems;
        rv = ProcessChildren(aPresShell, aPresContext, aState, aContent,
                             insertionFrame, PR_TRUE, insertionItems, isBlock);
        if (insertionItems.childList) {
          AppendFrames(aPresContext, aPresShell,aState.mFrameManager,
                       aContent, insertionFrame,
                       insertionItems.childList);
        }
    }
    
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
    } else {
      // Add the new frame to our list of frame items.
      aFrameItems.AddChild(newFrame);
    }
  }
  return rv;
}
#endif // MOZ_XTF


// SVG 
#ifdef MOZ_SVG
nsresult
nsCSSFrameConstructor::TestSVGConditions(nsIContent* aContent,
                                         PRBool&     aHasRequiredExtensions,
                                         PRBool&     aHasRequiredFeatures,
                                         PRBool&     aHasSystemLanguage)
{
  nsresult rv = NS_OK;
  nsAutoString value;

  // Only elements can have tests on them
  if (! aContent->IsContentOfType(nsIContent::eELEMENT)) {
    aHasRequiredExtensions = PR_FALSE;
    aHasRequiredFeatures = PR_FALSE;
    aHasSystemLanguage = PR_FALSE;
    return rv;
  }

  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  rv = aContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::requiredExtensions, value);
  if (NS_FAILED(rv))
    return rv;

  aHasRequiredExtensions = PR_TRUE;
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    // For now, claim that mozilla's SVG implementation supports
    // no extensions.  So, if extensions are required, we don't have
    // them available.
    aHasRequiredExtensions = PR_FALSE;
  }

  // Required Features
  aHasRequiredFeatures = PR_TRUE;
  rv = aContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::requiredFeatures, value);
  if (NS_FAILED(rv))
    return rv;
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    aHasRequiredFeatures = NS_SVG_TestFeatures(value);
  }

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  aHasSystemLanguage = PR_TRUE;
  rv = aContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::systemLanguage, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    // Get our language preferences
    nsAutoString langPrefs(nsContentUtils::GetLocalizedStringPref("intl.accept_languages"));
    if (!langPrefs.IsEmpty()) {
      langPrefs.StripWhitespace();
      value.StripWhitespace();
#ifdef  DEBUG_scooter
      printf("Calling SVG_TestLanguage('%s','%s')\n", NS_ConvertUCS2toUTF8(value).get(), 
                                                      NS_ConvertUCS2toUTF8(langPrefs).get());
#endif
      aHasSystemLanguage = SVG_TestLanguage(value, langPrefs);
    } else {
      // For now, evaluate to true.
      NS_WARNING("no default language specified for systemLanguage conditional test");
      aHasSystemLanguage = PR_TRUE;
    }
    return NS_OK;
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::SVGSwitchProcessChildren(nsIPresShell*            aPresShell,
                                                nsPresContext*           aPresContext,
                                                nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsIFrame*                aFrame,
                                                nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
  PRBool isFinished = PR_FALSE;
  PRBool hasRequiredExtensions = PR_FALSE;
  PRBool hasRequiredFeatures = PR_FALSE;
  PRBool hasSystemLanguage = PR_FALSE;

  // save the incoming pseudo frame state
  nsPseudoFrames priorPseudoFrames;
  aState.mPseudoFrames.Reset(&priorPseudoFrames);

  // The 'switch' element evaluates the requiredFeatures,
  // requiredExtensions and systemLanguage attributes on its direct child
  // elements in order, and then processes and renders the first child for
  // which these attributes evaluate to true. All others will be bypassed and
  // therefore not rendered.
  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       (iter != last) && (! isFinished);
       ++iter) {

    nsCOMPtr<nsIContent> child(*iter);

    rv = TestSVGConditions(child,
                           hasRequiredExtensions,
                           hasRequiredFeatures,
                           hasSystemLanguage);
#ifdef DEBUG_scooter
    printf("SwitchProcessChildren: Required Extentions = %s, Required Features = %s, System Language = %s\n",
            hasRequiredExtensions ? "true" : "false",
            hasRequiredFeatures ? "true" : "false",
            hasSystemLanguage ? "true" : "false");
#endif
    if (NS_FAILED(rv))
      return rv;

    if (hasRequiredExtensions &&
        hasRequiredFeatures &&
        hasSystemLanguage) {

      rv = ConstructFrame(aPresShell, aPresContext, aState,
                          nsCOMPtr<nsIContent>(*iter),
                          aFrame, aFrameItems);

      if (NS_FAILED(rv))
        return rv;

      if (child->IsContentOfType(nsIContent::eELEMENT)) {
        break;
      }
    }
  }

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
  }

  // restore the incoming pseudo frame state
  aState.mPseudoFrames = priorPseudoFrames;


  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructSVGFrame(nsIPresShell*            aPresShell,
                                          nsPresContext*          aPresContext,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsIAtom*                 aTag,
                                          PRInt32                  aNameSpaceID,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems)
{
  NS_ASSERTION(aNameSpaceID == kNameSpaceID_SVG, "SVG frame constructed in wrong namespace");

  nsresult  rv = NS_OK;
  PRBool isAbsolutelyPositioned = PR_FALSE;
  PRBool isFixedPositioned = PR_FALSE;
  PRBool forceView = PR_FALSE;
  PRBool processChildren = PR_FALSE;
  
  NS_ASSERTION(aTag != nsnull, "null SVG tag");
  if (aTag == nsnull)
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
  //nsSVGTableCreator svgTableCreator(aPresShell); // Used to make table views.
 
  // Default to aParentFrame for the geometricParent; it's adjusted in
  // cases when we allow anything else.
  nsIFrame* geometricParent = aParentFrame;
  
  if (aTag == nsSVGAtoms::svg) {
    nsCOMPtr<nsISVGContainerFrame> container = do_QueryInterface(aParentFrame);
    processChildren = PR_TRUE;
    if (!container) {
      // This is the outermost <svg> element.
      // See if the element is absolute or fixed positioned. These
      // properties only apply to outer SVG elements.
      const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();
      if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
        isAbsolutelyPositioned = PR_TRUE;
      }
      else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
        isFixedPositioned = PR_TRUE;
      }

      // Set the right geometricParent
      geometricParent = aState.GetGeometricParent(disp, aParentFrame);
      
      forceView = PR_TRUE;
      rv = NS_NewSVGOuterSVGFrame(aPresShell, aContent, &newFrame);
    }
    else {
      // This is an inner <svg> element
      rv = NS_NewSVGInnerSVGFrame(aPresShell, aContent, &newFrame);
    }
  }
  else if (aTag == nsSVGAtoms::g) {
    processChildren = PR_TRUE;
    rv = NS_NewSVGGFrame(aPresShell, aContent, &newFrame);
  }
  else if (aTag == nsSVGAtoms::polygon)
    rv = NS_NewSVGPolygonFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::polyline)
    rv = NS_NewSVGPolylineFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::circle)
    rv = NS_NewSVGCircleFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::defs) {
    processChildren = PR_TRUE;
    rv = NS_NewSVGDefsFrame(aPresShell, aContent, &newFrame);
  }
  else if (aTag == nsSVGAtoms::ellipse)
    rv = NS_NewSVGEllipseFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::line)
    rv = NS_NewSVGLineFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::rect)
    rv = NS_NewSVGRectFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::foreignObject) {
    processChildren = PR_TRUE;
    rv = NS_NewSVGForeignObjectFrame(aPresShell, aContent, &newFrame);
  }
  else if (aTag == nsSVGAtoms::path)
    rv = NS_NewSVGPathFrame(aPresShell, aContent, &newFrame);
  else if (aTag == nsSVGAtoms::text) {
    processChildren = PR_TRUE;
    rv = NS_NewSVGTextFrame(aPresShell, aContent, &newFrame);
  }
  else if (aTag == nsSVGAtoms::tspan) {
    processChildren = PR_TRUE;
    rv = NS_NewSVGTSpanFrame(aPresShell, aContent, aParentFrame, &newFrame);
  }
  
  if (newFrame == nsnull) {
    // Either we have an unknown tag, or construction of a frame
    // failed. One reason why frame construction for a known tag might
    // have failed is that the content element doesn't implement all
    // interfaces required by the frame. This happens e.g. when using
    // 'extends' in xbl to extend an xbl binding from an svg
    // element. In that case, the bound content element will always be
    // a standard xml element, and not be of the right type.
    // The best we can do here is to create a generic svg container frame.
#ifdef DEBUG
    //printf("Warning: Creating SVGGenericContainerFrame for tag <");
    //nsAutoString str;
    //aTag->ToString(str);
    //printf("%s>\n", NS_ConvertUCS2toUTF8(str).get());
#endif
    processChildren = PR_TRUE;
    rv = NS_NewSVGGenericContainerFrame(aPresShell, aContent, &newFrame);
  }  
  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {
    if (aTag == nsSVGAtoms::foreignObject) {
      // Claim to be relatively positioned so that we end up being the
      // absolute containing block.  Also, push "null" as the floater
      // containing block so that we get the SPACE_MGR bit set.
      nsFrameConstructorSaveState saveState;
      aState.PushFloatContainingBlock(nsnull, saveState, PR_FALSE, PR_FALSE);
      const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();
      rv = ConstructBlock(aPresShell, aPresContext, aState, disp, aContent,
                          geometricParent, aParentFrame, aStyleContext,
                          newFrame, PR_TRUE);
    } else {
      InitAndRestoreFrame(aPresContext, aState, aContent, 
                          geometricParent, aStyleContext, nsnull, newFrame);

      nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, forceView);

      // Process the child content if requested
      nsFrameItems childItems;
      if (processChildren) {
        if (aTag == nsSVGAtoms::svgSwitch) {
          rv = SVGSwitchProcessChildren(aPresShell, aPresContext, aState, aContent,
                                     newFrame, childItems);
        } else {
          rv = ProcessChildren(aPresShell, aPresContext, aState, aContent,
                               newFrame, PR_TRUE, childItems, PR_FALSE);
        }

        CreateAnonymousFrames(aPresShell, aPresContext, aTag, aState, aContent, newFrame,
                              PR_FALSE, childItems);
      }

      // Set the frame's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
    }
    
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
    } else {
      // Add the new frame to our list of frame items.
      aFrameItems.AddChild(newFrame);
    }
  }
  return rv;
}
#endif // MOZ_SVG

PRBool
nsCSSFrameConstructor::PageBreakBefore(nsIPresShell*            aPresShell,
                                       nsPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
                                       nsFrameItems&            aFrameItems)
{
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // See if page-break-before is set for all elements except row groups, rows, cells 
  // (these are handled internally by tables) and construct a page break frame if so.
  if (display && ((NS_STYLE_DISPLAY_TABLE == display->mDisplay) ||
                  (!IsTableRelated(display->mDisplay, PR_TRUE)))) { 
    if (display->mBreakBefore) {
      ConstructPageBreakFrame(aPresShell, aPresContext, aState, aContent,
                              aParentFrame, aStyleContext, aFrameItems);
    }
    return display->mBreakAfter;
  }
  return PR_FALSE;
}

nsresult
nsCSSFrameConstructor::ConstructPageBreakFrame(nsIPresShell*            aPresShell, 
                                               nsPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsStyleContext*          aStyleContext,
                                               nsFrameItems&            aFrameItems)
{
  nsRefPtr<nsStyleContext> pseudoStyle;
  pseudoStyle = aPresShell->StyleSet()->ResolvePseudoStyleFor(nsnull,
                                                              nsCSSAnonBoxes::pageBreak,
                                                              aStyleContext);
  nsIFrame* pageBreakFrame;
  nsresult rv = NS_NewPageBreakFrame(aPresShell, &pageBreakFrame); 
  if (NS_SUCCEEDED(rv)) {
    InitAndRestoreFrame(aPresContext, aState, aContent, aParentFrame, 
                        pseudoStyle, nsnull, pageBreakFrame);
    aFrameItems.AddChild(pageBreakFrame);
  }
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructFrame(nsIPresShell*            aPresShell, 
                                      nsPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsFrameItems&            aFrameItems)

{
  NS_PRECONDITION(nsnull != aParentFrame, "no parent frame");

  nsresult rv = NS_OK;

  // don't create a whitespace frame if aParent doesn't want it
  if (!NeedFrameFor(aParentFrame, aContent)) {
    return rv;
  }

  // Get the element's tag
  nsIAtom *tag = aContent->Tag();

  // never create frames for comments on PIs
  if (tag == nsLayoutAtoms::commentTagName ||
      tag == nsLayoutAtoms::processingInstructionTagName)
    return rv;

  nsRefPtr<nsStyleContext> styleContext;
  styleContext = ResolveStyleContext(aPresContext, aParentFrame, aContent);

  PRInt32 nameSpaceID;
  aContent->GetNameSpaceID(&nameSpaceID);

  PRBool pageBreakAfter = PR_FALSE;

  if (aPresContext->IsPaginated()) {
    // See if there is a page break before, if so construct one. Also see if there is one after
    pageBreakAfter = PageBreakBefore(aPresShell, aPresContext, aState, aContent, 
                                     aParentFrame, styleContext, aFrameItems);
  }
  // construct the frame
  rv = ConstructFrameInternal(aPresShell, aPresContext, aState, aContent, aParentFrame,
                              tag, nameSpaceID, styleContext, aFrameItems, PR_FALSE);
  if (NS_SUCCEEDED(rv) && pageBreakAfter) {
    // Construct the page break after
    ConstructPageBreakFrame(aPresShell, aPresContext, aState, aContent,
                            aParentFrame, styleContext, aFrameItems);
  }
  
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructFrameInternal( nsIPresShell*            aPresShell, 
                                               nsPresContext*          aPresContext,
                                               nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsIAtom*                 aTag,
                                               PRInt32                  aNameSpaceID,
                                               nsStyleContext*          aStyleContext,
                                               nsFrameItems&            aFrameItems,
                                               PRBool                   aXBLBaseTag)
{
  // The following code allows the user to specify the base tag
  // of an element using XBL.  XUL and HTML objects (like boxes, menus, etc.)
  // can then be extended arbitrarily.
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  nsRefPtr<nsStyleContext> styleContext(aStyleContext);
  nsAutoEnqueueBinding binding(mDocument);
  if (!aXBLBaseTag)
  {
    
    // Ensure that our XBL bindings are installed.
    if (display->mBinding) {
      // Get the XBL loader.
      nsresult rv;
      // Load the bindings.
      PRBool resolveStyle;
      
      nsIXBLService * xblService = GetXBLService();
      if (!xblService)
        return NS_ERROR_FAILURE;

      rv = xblService->LoadBindings(aContent, display->mBinding, PR_FALSE, getter_AddRefs(binding.mBinding), &resolveStyle);
      if (NS_FAILED(rv))
        return NS_OK;

      if (resolveStyle) {
        styleContext = ResolveStyleContext(aPresContext, aParentFrame,
                                           aContent);
        display = styleContext->GetStyleDisplay();
      }

      nsCOMPtr<nsIAtom> baseTag;
      PRInt32 nameSpaceID;
      xblService->ResolveTag(aContent, &nameSpaceID, getter_AddRefs(baseTag));
 
      if (baseTag != aTag || aNameSpaceID != nameSpaceID) {
        // Construct the frame using the XBL base tag.
        rv = ConstructFrameInternal( aPresShell, 
                                  aPresContext,
                                  aState,
                                  aContent,
                                  aParentFrame,
                                  baseTag,
                                  nameSpaceID,
                                  styleContext,
                                  aFrameItems,
                                  PR_TRUE);
        return rv;
      }
    }
  }

  // Pre-check for display "none" - if we find that, don't create
  // any frame at all
  if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
    aState.mFrameManager->SetUndisplayedContent(aContent, styleContext);
    return NS_OK;
  }
  
  if (aContent->IsContentOfType(nsIContent::eTEXT)) 
    return ConstructTextFrame(aPresShell, aPresContext, aState,
                              aContent, aParentFrame, styleContext,
                              aFrameItems);

  // Style resolution can normally happen lazily.  However, getting the
  // Visibility struct can cause |SetBidiEnabled| to be called on the
  // pres context, and this needs to happen before we start reflow, so
  // do it now, when constructing frames.  See bug 115921.
  {
    styleContext->GetStyleVisibility();
  }

  nsIFrame* lastChild = aFrameItems.lastChild;

  // Handle specific frame types
  nsresult rv = ConstructHTMLFrame(aPresShell, aPresContext, aState,
                                   aContent, aParentFrame, aTag,
                                   aNameSpaceID, styleContext, aFrameItems);

  // Failing to find a matching HTML frame, try creating a specialized
  // XUL frame. This is temporary, pending planned factoring of this
  // whole process into separate, pluggable steps.
  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                         (lastChild == aFrameItems.lastChild))) {
    PRBool haltProcessing = PR_FALSE;
    rv = ConstructXULFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                           aTag, aNameSpaceID, styleContext, aFrameItems, aXBLBaseTag, haltProcessing);
    if (haltProcessing) {
      return rv;
    }
  } 

// MathML Mod - RBS
#ifdef MOZ_MATHML
  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                           (lastChild == aFrameItems.lastChild))) {
    rv = ConstructMathMLFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                              aTag, aNameSpaceID, styleContext, aFrameItems);
  }
#endif

// SVG
#ifdef MOZ_SVG
  if (NS_SUCCEEDED(rv) &&
      ((nsnull == aFrameItems.childList) ||
       (lastChild == aFrameItems.lastChild)) &&
      (aNameSpaceID == kNameSpaceID_SVG)) {
    rv = ConstructSVGFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                              aTag, aNameSpaceID, styleContext, aFrameItems);
  }
#endif

// XTF
#ifdef MOZ_XTF
  if (NS_SUCCEEDED(rv) &&
      ((nsnull == aFrameItems.childList) ||
       (lastChild == aFrameItems.lastChild))) {
    nsCOMPtr<nsIXTFElementWrapperPrivate> xtfElem = do_QueryInterface(aContent);
    if (xtfElem) {
      if (xtfElem->GetElementType() == nsIXTFElement::ELEMENT_TYPE_GENERIC_ELEMENT) {
        // we don't build frames for generic elements, only for visuals
        aState.mFrameManager->SetUndisplayedContent(aContent, styleContext);
        return NS_OK;
      } else 
        rv = ConstructXTFFrame(aPresShell, aPresContext, aState, aContent, aParentFrame,
                               aTag, aNameSpaceID, styleContext, aFrameItems);
    }
  }
#endif
  
  if (NS_SUCCEEDED(rv) && ((nsnull == aFrameItems.childList) ||
                           (lastChild == aFrameItems.lastChild))) {
    // When there is no explicit frame to create, assume it's a
    // container and let display style dictate the rest
    rv = ConstructFrameByDisplayType(aPresShell, aPresContext, aState, display,
                                     aContent, aNameSpaceID, aTag,
                                     aParentFrame, styleContext, aFrameItems);
  }

  return rv;
}


inline PRBool
IsRootBoxFrame(nsIFrame *aFrame)
{
  return (aFrame->GetType() == nsLayoutAtoms::rootFrame);
}

nsresult
nsCSSFrameConstructor::ReconstructDocElementHierarchy(nsPresContext* aPresContext)
{
  NS_PRECONDITION(aPresContext, "null pres context argument");
  
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ReconstructDocElementHierarchy\n");
  }
#endif

  nsresult rv = NS_OK;
  nsIPresShell *shell = aPresContext->GetPresShell();

  if (mDocument && shell) {
    nsIContent *rootContent = mDocument->GetRootContent();
    
    if (rootContent) {
      // Before removing the frames associated with the content object, ask them to save their
      // state onto a temporary state object.
      CaptureStateForFramesOf(aPresContext, rootContent, mTempFrameTreeState);

      nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                    nsnull, nsnull, mTempFrameTreeState);

      // Get the frame that corresponds to the document element
      nsIFrame* docElementFrame =
        state.mFrameManager->GetPrimaryFrameFor(rootContent);
        
      // Remove any existing fixed items: they are always on the
      // FixedContainingBlock.  Note that this has to be done before we call
      // ClearPlaceholderFrameMap(), since RemoveFixedItems uses the
      // placeholder frame map.
      rv = RemoveFixedItems(aPresContext, shell, state.mFrameManager);
      if (NS_SUCCEEDED(rv)) {
        // Clear the hash tables that map from content to frame and out-of-flow
        // frame to placeholder frame
        state.mFrameManager->ClearPrimaryFrameMap();
        state.mFrameManager->ClearPlaceholderFrameMap();
        state.mFrameManager->ClearUndisplayedContentMap();

        // Take the docElementFrame, and remove it from its parent. For
        // HTML, we'll be removing the Area frame from the Canvas; for
        // XUL, we'll remove the GfxScroll or Box from the RootBoxFrame.
        //
        // The three possible structures (at least the ones observed so
        // far, see bugs 70258 and 93558) are:
        //
        // (HTML)
        //    ScrollBoxFrame(html)<
        //     ScrollPortFrame(html)<
        //      Canvas(-1)<
        //       Area(html)<
        //        (etc.)
        //
        // (XUL #1)
        //    RootBoxFrame(window)<
        //     GfxScroll<
        //      ScrollBoxFrame(window)<
        //       ScrollPortFrame(window)<
        //        (etc.)
        //
        // (XUL #2)
        //    RootBox<
        //     Box<
        //      (etc.)
        //
        if (docElementFrame) {
          nsIFrame* docParentFrame = docElementFrame->GetParent();

#ifdef MOZ_XUL
          // If we're in a XUL document, then we need to crawl up to the
          // RootBoxFrame and remove _its_ child.
          nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
          if (xuldoc) {
            nsCOMPtr<nsIAtom> frameType;
            while (docParentFrame && !IsRootBoxFrame(docParentFrame)) {
              docElementFrame = docParentFrame;
              docParentFrame = docParentFrame->GetParent();
            }
          }
#endif

          NS_ASSERTION(docParentFrame, "should have a parent frame");
          if (docParentFrame) {
            // Remove the old document element hieararchy
            rv = state.mFrameManager->RemoveFrame(docParentFrame, nsnull, 
                                                  docElementFrame);
            if (NS_SUCCEEDED(rv)) {
              // Create the new document element hierarchy
              nsIFrame*                 newChild;
              rv = ConstructDocElementFrame(shell, aPresContext, state, rootContent,
                                            docParentFrame, newChild);

              if (NS_SUCCEEDED(rv)) {
                rv = state.mFrameManager->InsertFrames(docParentFrame, nsnull,
                                                       nsnull, newChild);

                // Tell the fixed containing block about its 'fixed' frames
                if (state.mFixedItems.childList) {
                  state.mFrameManager->InsertFrames(mFixedContainingBlock,
                                                    nsLayoutAtoms::fixedList,
                                                    nsnull,
                                                    state.mFixedItems.childList);
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
nsCSSFrameConstructor::GetFrameFor(nsIPresShell*    aPresShell,
                                   nsPresContext*  aPresContext,
                                   nsIContent*      aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame;
  aPresShell->GetPrimaryFrameFor(aContent, &frame);

  if (!frame)
    return nsnull;

  return frame->GetContentInsertionFrame();
}

nsIFrame*
nsCSSFrameConstructor::GetAbsoluteContainingBlock(nsPresContext* aPresContext,
                                                  nsIFrame*       aFrame)
{
  NS_PRECONDITION(nsnull != mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is absolutely positioned or
  // relatively positioned
  nsIFrame* containingBlock = nsnull;
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    // Is it positioned?
    // If it's a table then ignore it, because for the time being tables
    // are not containers for absolutely positioned child frames
    const nsStyleDisplay* disp = frame->GetStyleDisplay();

    if (disp->IsPositioned() && disp->mDisplay != NS_STYLE_DISPLAY_TABLE) {
      nsIAtom* frameType = frame->GetType();
      if (nsLayoutAtoms::scrollFrame == frameType) {
        // We want the scrolled frame, not either of the two scroll frames
        nsIFrame* scrolledFrame = frame->GetFirstChild(nsnull);
        if (scrolledFrame) {
          frameType = scrolledFrame->GetType();
          if (nsLayoutAtoms::areaFrame == frameType) {
            containingBlock = scrolledFrame;
            break;
          } else if (nsLayoutAtoms::scrollFrame == frameType) {
            scrolledFrame = scrolledFrame->GetFirstChild(nsnull);
            if (scrolledFrame) {
              if (nsLayoutAtoms::areaFrame == scrolledFrame->GetType()) {
                containingBlock = scrolledFrame;
                break;
              }
            }
          }
        }
      } else if ((nsLayoutAtoms::areaFrame == frameType) ||
                 (nsLayoutAtoms::positionedInlineFrame == frameType)) {
        containingBlock = frame;
        break;
      } else if (nsLayoutAtoms::fieldSetFrame == frameType) {
        // If the positioned frame is a fieldset, use the area frame inside it
        containingBlock = frame->GetFirstChild(nsnull);
        break;
      }
#ifdef DEBUG
      else {
        NS_WARNING("Positioned frame that does not handle positioned kids; looking further up the parent chain");
      }
#endif
    }
  }

  // If we found an absolutely positioned containing block, then use the first-in-flow if 
  // it is a positioned inline. If we didn't find it, then use the initial containing block. 
  return (containingBlock) ? AdjustAbsoluteContainingBlock(aPresContext, containingBlock)
                           : mInitialContainingBlock;
}

nsIFrame*
nsCSSFrameConstructor::GetFloatContainingBlock(nsPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  NS_PRECONDITION(mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is a real block frame,
  // or a floated inline or absolutely positioned inline frame
  nsIFrame* containingBlock = aFrame;
  while (nsnull != containingBlock) {
    const nsStyleDisplay* display = containingBlock->GetStyleDisplay();
    if ((NS_STYLE_DISPLAY_BLOCK == display->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay)) {
      break;
    }
    else if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
      if ((NS_STYLE_FLOAT_NONE != display->mFloats) ||
          (display->IsAbsolutelyPositioned())) {
        if (NS_STYLE_FLOAT_NONE != display->mFloats) {
          if (nsLayoutAtoms::letterFrame != containingBlock->GetType()) {
            break;
          }
        }
        else {
          break;
        }
      }
    }

    // Continue walking up the hierarchy
    containingBlock = containingBlock->GetParent();
  }

  // If we didn't find a containing block, then use the initial
  // containing block
  if (nsnull == containingBlock) {
    containingBlock = mInitialContainingBlock;
  }
  return containingBlock;
}

/**
 * This function is called by ContentAppended() and ContentInserted()
 * when appending flowed frames to a parent's principal child list. It
 * handles the case where the parent frame has :after pseudo-element
 * generated content.
 */
nsresult
nsCSSFrameConstructor::AppendFrames(nsPresContext*  aPresContext,
                                    nsIPresShell*    aPresShell,
                                    nsFrameManager*  aFrameManager,
                                    nsIContent*      aContainer,
                                    nsIFrame*        aParentFrame,
                                    nsIFrame*        aFrameList)
{
  // See if the parent has an :after pseudo-element.  Check for the presence
  // of style first, since nsLayoutUtils::GetAfterFrame is sorta expensive.
  nsStyleContext* parentStyle = aParentFrame->GetStyleContext();
  if (nsLayoutUtils::HasPseudoStyle(aContainer, parentStyle,
                                    nsCSSPseudoElements::after,
                                    aPresContext)) {
    nsIFrame* afterFrame = nsLayoutUtils::GetAfterFrame(aParentFrame,
                                                        aPresContext);
    if (afterFrame) {
      nsFrameList frames(aParentFrame->GetFirstChild(nsnull));

      // Insert the frames before the :after pseudo-element.
      return aFrameManager->InsertFrames(aParentFrame,
                                       nsnull, frames.GetPrevSiblingFor(afterFrame),
                                       aFrameList);
    }
  }

  nsresult rv = NS_OK;

  // a col group or col appended to a table may result in an insert rather than an append
  if (nsLayoutAtoms::tableFrame == aParentFrame->GetType()) { 
    nsTableFrame* tableFrame = NS_REINTERPRET_CAST(nsTableFrame*, aParentFrame);
    nsIAtom* childType = aFrameList->GetType();
    if (nsLayoutAtoms::tableColFrame == childType) {
      // table column
      nsIFrame* parentFrame = aFrameList->GetParent();
      rv = aFrameManager->AppendFrames(parentFrame,
                                       nsLayoutAtoms::colGroupList, aFrameList);
    }
    else if (nsLayoutAtoms::tableColGroupFrame == childType) {
      // table col group
      nsIFrame* prevSibling;
      PRBool doAppend = nsTableColGroupFrame::GetLastRealColGroup(tableFrame, &prevSibling);
      if (doAppend) {
        rv = aFrameManager->AppendFrames(aParentFrame,
                                         nsLayoutAtoms::colGroupList, aFrameList);
      }
      else {
        rv = aFrameManager->InsertFrames(aParentFrame, 
                                         nsLayoutAtoms::colGroupList, prevSibling, aFrameList);
      }
    }
    else if (nsLayoutAtoms::tableCaptionFrame == childType) {
      // table caption
      rv = aFrameManager->AppendFrames(aParentFrame,
                                       nsLayoutAtoms::captionList, aFrameList);
    }
    else {
      rv = aFrameManager->AppendFrames(aParentFrame, nsnull, aFrameList);
    }
  }
  else {
    // Append the frames to the end of the parent's child list
    // check for a table caption which goes on an additional child list with a different parent
    nsIFrame* outerTableFrame; 
    if (GetCaptionAdjustedParent(aParentFrame, aFrameList, &outerTableFrame)) {
      rv = aFrameManager->AppendFrames(outerTableFrame,
                                       nsLayoutAtoms::captionList, aFrameList);
    }
    else {
      rv = aFrameManager->AppendFrames(aParentFrame, nsnull, aFrameList);
    }
  }

  return rv;
}

/**
 * Find the ``rightmost'' frame for the anonymous content immediately
 * preceding aChild, following continuation if necessary.
 */
static nsIFrame*
FindPreviousAnonymousSibling(nsIPresShell* aPresShell,
                             nsIDocument*  aDocument,
                             nsIContent*   aContainer,
                             nsIContent*   aChild)
{
  NS_PRECONDITION(aDocument, "null document from content element in FindNextAnonymousSibling");

  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(aDocument));
  NS_ASSERTION(xblDoc, "null xblDoc for content element in FindNextAnonymousSibling");
  if (! xblDoc)
    return nsnull;

  // Grovel through the anonymous elements looking for aChild. We'll
  // start our search for a previous frame there.
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContainer));
  xblDoc->GetAnonymousNodes(elt, getter_AddRefs(nodeList));

  if (! nodeList)
    return nsnull;

  PRUint32 length;
  nodeList->GetLength(&length);

  PRInt32 index;
  for (index = PRInt32(length) - 1; index >= 0; --index) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);
    if (child == aChild)
      break;
  }

  // We want the node immediately before aChild. Keep going until we
  // run off the beginning of the nodeList, or we find a frame.
  while (--index >= 0) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);

    // Get its frame. If it doesn't have one, continue on to the
    // anonymous element that preceded it.
    nsIFrame* prevSibling;
    aPresShell->GetPrimaryFrameFor(child, &prevSibling);
    if (prevSibling) {
      // The frame may be a special frame (a split inline frame that
      // contains a block).  Get the last part of that split.
      if (IsFrameSpecial(prevSibling)) {
        prevSibling = GetLastSpecialSibling(aPresShell->FrameManager(),
                                            prevSibling);
      }

      // The frame may have a continuation. If so, we want the
      // last-in-flow as our previous sibling.
      prevSibling = prevSibling->GetLastInFlow();

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      const nsStyleDisplay* display = prevSibling->GetStyleDisplay();

      if (display->IsFloating() || display->IsAbsolutelyPositioned()) {
        nsIFrame *placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
        prevSibling = placeholderFrame;
      }

      // Found a previous sibling, we're done!
      return prevSibling;
    }
  }

  return nsnull;
}

/**
 * Find the frame for the anonymous content immediately following
 * aChild.
 */
static nsIFrame*
FindNextAnonymousSibling(nsIPresShell* aPresShell,
                         nsIDocument*  aDocument,
                         nsIContent*   aContainer,
                         nsIContent*   aChild)
{
  NS_PRECONDITION(aDocument, "null document from content element in FindNextAnonymousSibling");

  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(aDocument));
  NS_ASSERTION(xblDoc, "null xblDoc for content element in FindNextAnonymousSibling");
  if (! xblDoc)
    return nsnull;

  // Grovel through the anonymous elements looking for aChild
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContainer));
  xblDoc->GetAnonymousNodes(elt, getter_AddRefs(nodeList));

  if (! nodeList)
    return nsnull;

  PRUint32 length;
  nodeList->GetLength(&length);

  PRInt32 index;
  for (index = 0; index < PRInt32(length); ++index) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);
    if (child == aChild)
      break;
  }

  // We want the node immediately after aChild. Keep going until we
  // run off the end of the nodeList, or we find a next sibling.
  while (++index < PRInt32(length)) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);

    // Get its frame
    nsIFrame* nextSibling;
    aPresShell->GetPrimaryFrameFor(child, &nextSibling);
    if (nextSibling) {
      // The primary frame should never be a continuation
      NS_ASSERTION(!nextSibling->GetPrevInFlow(),
                   "primary frame is a continuation!?");

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      const nsStyleDisplay* display = nextSibling->GetStyleDisplay();

      if (display->IsFloating() || display->IsAbsolutelyPositioned()) {
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(nextSibling, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
        nextSibling = placeholderFrame;
      }

      // Found a next sibling, we're done!
      return nextSibling;
    }
  }

  return nsnull;
}

#define UNSET_DISPLAY 255
// This gets called to see if the frames corresponding to aSiblingDisplay and aDisplay
// should be siblings in the frame tree. Although (1) rows and cols, (2) row groups 
// and col groups, (3) row groups and captions (4) legends and content inside fieldsets
// are siblings from a content perspective, they are not considered siblings in the 
// frame tree.
PRBool
nsCSSFrameConstructor::IsValidSibling(nsIPresShell&          aPresShell,
                                      nsIFrame*              aParentFrame,
                                      const nsIFrame&        aSibling,
                                      PRUint8                aSiblingDisplay,
                                      nsIContent&            aContent,
                                      PRUint8&               aDisplay)
{
  if ((NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_COLUMN       == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aSiblingDisplay)) {
    // if we haven't already, construct a style context to find the display type of aContent
    if (UNSET_DISPLAY == aDisplay) {
      nsRefPtr<nsStyleContext> styleContext;
      styleContext = ResolveStyleContext(aPresShell.GetPresContext(),
                                         aSibling.GetParent(), &aContent);
      if (!styleContext) return PR_FALSE;
      const nsStyleDisplay* display = styleContext->GetStyleDisplay();
      aDisplay = display->mDisplay;
    }
    switch (aSiblingDisplay) {
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      return (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == aDisplay);
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      return (NS_STYLE_DISPLAY_TABLE_COLUMN == aDisplay);
    default: // all of the row group types
      return (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_CAPTION      == aDisplay);
    }
  }
  else if (NS_STYLE_DISPLAY_TABLE_CAPTION == aSiblingDisplay) {
    // Nothing can be a sibling of a caption since there can only be one caption.
    // But this check is necessary since a row group and caption are siblings
    // from a content perspective (they share the table content as parent)
    return PR_FALSE;
  }
  else {
    if (nsLayoutAtoms::fieldSetFrame == aParentFrame->GetType()) {
      // Legends can be sibling of legends but not of other content in the fieldset
      nsIAtom* sibType = aSibling.GetType();
      nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(&aContent));

      if ((legendContent  && (nsLayoutAtoms::legendFrame != sibType)) ||
          (!legendContent && (nsLayoutAtoms::legendFrame == sibType)))
        return PR_FALSE;
    }
  }

  return PR_TRUE;
}

/**
 * Find the ``rightmost'' frame for the content immediately preceding
 * aIndexInContainer, following continuations if necessary.
 */
nsIFrame*
nsCSSFrameConstructor::FindPreviousSibling(nsIPresShell*     aPresShell,
                                           nsIContent*       aContainer,
                                           nsIFrame*         aContainerFrame,
                                           PRInt32           aIndexInContainer,
                                           const nsIContent* aChild)
{
  NS_ASSERTION(aPresShell && aContainer, "null arguments");

  ChildIterator first, iter;
  nsresult rv = ChildIterator::Init(aContainer, &first, &iter);
  NS_ENSURE_SUCCESS(rv, nsnull);
  iter.seek(aIndexInContainer);

  PRUint8 childDisplay = UNSET_DISPLAY;
  // Note: not all content objects are associated with a frame (e.g., if it's
  // `display: hidden') so keep looking until we find a previous frame
  while (iter-- != first) {
    nsIFrame* prevSibling = nsnull;
    aPresShell->GetPrimaryFrameFor(nsCOMPtr<nsIContent>(*iter), &prevSibling);

    if (prevSibling) {
      // The frame may be a special frame (a split inline frame that
      // contains a block).  Get the last part of that split.
      if (IsFrameSpecial(prevSibling)) {
        prevSibling = GetLastSpecialSibling(aPresShell->FrameManager(),
                                            prevSibling);
      }

      // The frame may have a continuation. Get the last-in-flow
      prevSibling = prevSibling->GetLastInFlow();

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      // XXXldb Why not check NS_FRAME_OUT_OF_FLOW state bit?
      const nsStyleDisplay* display = prevSibling->GetStyleDisplay();
  
      if (aChild && !IsValidSibling(*aPresShell, aContainerFrame, *prevSibling, 
                                    display->mDisplay, (nsIContent&)*aChild, childDisplay))
        continue;

      if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
        // XXXldb Was this supposed to be a null-check of placeholderFrame?
        if (prevSibling)
          prevSibling = placeholderFrame;
      }
      else if (display->IsFloating() || display->IsAbsolutelyPositioned()) {
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
        prevSibling = placeholderFrame;
      }

#ifdef DEBUG
      nsIFrame* containerFrame = nsnull;
      aPresShell->GetPrimaryFrameFor(aContainer, &containerFrame);
      NS_ASSERTION(prevSibling != containerFrame, "Previous Sibling is the Container's frame");
#endif
      // Found a previous sibling, we're done!
      return prevSibling;
    }
  }

  return nsnull;
}

/**
 * Find the frame for the content node immediately following
 * aIndexInContainer.
 */
nsIFrame*
nsCSSFrameConstructor::FindNextSibling(nsIPresShell*     aPresShell,
                                       nsIContent*       aContainer,
                                       nsIFrame*         aContainerFrame,
                                       PRInt32           aIndexInContainer,
                                       const nsIContent* aChild)
{
  ChildIterator iter, last;
  nsresult rv = ChildIterator::Init(aContainer, &iter, &last);
  NS_ENSURE_SUCCESS(rv, nsnull);
  iter.seek(aIndexInContainer);

  // Catch the case where someone tries to append
  if (iter == last)
    return nsnull;

  PRUint8 childDisplay = UNSET_DISPLAY;

  while (++iter != last) {
    nsIFrame* nextSibling = nsnull;
    aPresShell->GetPrimaryFrameFor(nsCOMPtr<nsIContent>(*iter), &nextSibling);

    if (nextSibling) {
      // The frame primary frame should never be a continuation
      NS_ASSERTION(!nextSibling->GetPrevInFlow(),
                   "primary frame is a continuation!?");

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      const nsStyleDisplay* display = nextSibling->GetStyleDisplay();

      if (aChild && !IsValidSibling(*aPresShell, aContainerFrame, *nextSibling, 
                                    display->mDisplay, (nsIContent&)*aChild, childDisplay))
        continue;

      if (display->IsFloating() || display->IsAbsolutelyPositioned()) {
        // Nope. Get the place-holder instead
        nsIFrame* placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(nextSibling, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
        nextSibling = placeholderFrame;
      }

      // We found a next sibling, we're done!
      return nextSibling;
    }
  }

  return nsnull;
}

inline PRBool
ShouldIgnoreSelectChild(nsIContent* aContainer)
{
  // Ignore options and optgroups inside a select (size > 1)
  nsIAtom *containerTag = aContainer->Tag();

  if (containerTag == nsHTMLAtoms::optgroup ||
      containerTag == nsHTMLAtoms::select) {
    nsIContent* selectContent = aContainer;

    while (containerTag != nsHTMLAtoms::select) {
      selectContent = selectContent->GetParent();
      if (!selectContent) {
        break;
      }
      containerTag = selectContent->Tag();
    }

    nsCOMPtr<nsISelectElement> selectElement = do_QueryInterface(selectContent);
    if (selectElement) {
      nsAutoString selSize;
      aContainer->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, selSize);
      if (!selSize.IsEmpty()) {
        PRInt32 err;
        return (selSize.ToInteger(&err) > 1);
      }
    }
  }

  return PR_FALSE;
}

// For tables, returns the inner table, if the child is not a caption. 
// For fieldsets, returns the area frame, if the child is not a legend. 
static nsIFrame*
GetAdjustedParentFrame(nsPresContext* aPresContext,
                       nsIFrame*       aParentFrame,
                       nsIAtom*        aParentFrameType,
                       nsIContent*     aParentContent,
                       PRInt32         aChildIndex)
{
  nsIContent *childContent = aParentContent->GetChildAt(aChildIndex);
  nsIFrame* newParent = nsnull;

  if (nsLayoutAtoms::tableOuterFrame == aParentFrameType) {
    nsCOMPtr<nsIDOMHTMLTableCaptionElement> captionContent(do_QueryInterface(childContent));
    // If the parent frame is an outer table, use the innner table
    // as the parent unless the new content is a caption.
    if (!captionContent) 
      newParent = aParentFrame->GetFirstChild(nsnull);
  }
  else if (nsLayoutAtoms::fieldSetFrame == aParentFrameType) {
    // If the parent is a fieldSet, use the fieldSet's area frame as the
    // parent unless the new content is a legend. 
    nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(childContent));
    if (!legendContent) 
      newParent = aParentFrame->GetFirstChild(nsnull);
  }
  return (newParent) ? newParent : aParentFrame;
}

static nsresult InsertOutOfFlow(nsPresContext* aPresContext,
                                const nsAbsoluteItems& aFrameItems,
                                nsIAtom* aChildListName) {
  if (!aFrameItems.childList) {
    return NS_OK;
  }

  nsIFrame* firstChild = aFrameItems.containingBlock->GetFirstChild(aChildListName);

  // Note that whether the frame construction context is doing an append or
  // not is not helpful here, since it could be appending to some frame in
  // the middle of the document, which means we're not necessarily appending
  // to the children of the containing block.
  //
  // We need to make sure the 'append to the end of document' case is fast.
  // So first test the last child of the containing block
  nsIFrame* lastChild = nsLayoutUtils::GetLastSibling(firstChild);
  if (lastChild) {
    if (nsLayoutUtils::CompareTreePosition(lastChild->GetContent(),
                                           aFrameItems.childList->GetContent(),
                                           aFrameItems.containingBlock->GetContent()) < 0) {
      // lastChild comes before the new children, so insert after lastChild
      return aFrameItems.containingBlock->
        AppendFrames(aPresContext, *(aPresContext->PresShell()),
                     aChildListName, aFrameItems.childList);
    }
  }

  nsIFrame* insertionPoint = nsnull;
  // try the other children
  for (nsIFrame* f = firstChild; f != lastChild; f = f->GetNextSibling()) {
    if (nsLayoutUtils::CompareTreePosition(f->GetContent(),
                                           aFrameItems.childList->GetContent(),
                                           aFrameItems.containingBlock->GetContent()) > 0) {
      // f comes after the new children, so stop here and insert after
      // the previous frame
      break;
    }
    insertionPoint = f;
  }

  return aFrameItems.containingBlock->
    InsertFrames(aPresContext, *(aPresContext->PresShell()),
                 aChildListName, insertionPoint, aFrameItems.childList);
}

static nsresult
InsertOutOfFlowFrames(nsFrameConstructorState& aState,
                      nsPresContext* aPresContext) {
  // If there are new absolutely positioned child frames, then notify
  // the parent
  nsresult rv = InsertOutOfFlow(aPresContext, aState.mAbsoluteItems,
                                nsLayoutAtoms::absoluteList);
  NS_ENSURE_SUCCESS(rv, rv);
  aState.mAbsoluteItems.childList = nsnull;
  
  // If there are new fixed positioned child frames, then notify
  // the parent
  rv = InsertOutOfFlow(aPresContext, aState.mFixedItems,
                       nsLayoutAtoms::fixedList);
  NS_ENSURE_SUCCESS(rv, rv);
  aState.mAbsoluteItems.childList = nsnull;

  // If there are new floating child frames, then notify
  // the parent
  rv = InsertOutOfFlow(aPresContext, aState.mFloatedItems,
                       nsLayoutAtoms::floatList);
  NS_ENSURE_SUCCESS(rv, rv);
  aState.mFloatedItems.childList = nsnull;

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ContentAppended(nsPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       PRInt32         aNewIndexInContainer)
{
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended container=%p index=%d\n",
           NS_STATIC_CAST(void*, aContainer), aNewIndexInContainer);
    if (gReallyNoisyContentUpdates && aContainer) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsIPresShell *shell = aPresContext->PresShell();

#ifdef MOZ_XUL
  if (aContainer) {
    nsIBindingManager *bindingManager = mDocument->GetBindingManager();

    nsCOMPtr<nsIAtom> tag;
    PRInt32 namespaceID;
    bindingManager->ResolveTag(aContainer, &namespaceID, getter_AddRefs(tag));

    // Just ignore tree tags, anyway we don't create any frames for them.
    if (tag == nsXULAtoms::treechildren ||
        tag == nsXULAtoms::treeitem ||
        tag == nsXULAtoms::treerow ||
        (namespaceID == kNameSpaceID_XUL && gUseXBLForms &&
         ShouldIgnoreSelectChild(aContainer)))
      return NS_OK;

  }
#endif // MOZ_XUL

  // Get the frame associated with the content
  nsIFrame* parentFrame = GetFrameFor(shell, aPresContext, aContainer);
  if (! parentFrame)
    return NS_OK;

  // See if we have an XBL insertion point. If so, then that's our
  // real parent frame; if not, then the frame hasn't been built yet
  // and we just bail.
  //
  nsIFrame* insertionPoint;
  PRBool multiple = PR_FALSE;
  GetInsertionPoint(shell, parentFrame, nsnull, &insertionPoint, &multiple);
  if (! insertionPoint)
    return NS_OK; // Don't build the frames.

  PRBool hasInsertion = PR_FALSE;
  if (!multiple) {
    nsIBindingManager *bindingManager = nsnull;
    nsIDocument* document = nsnull; 
    nsIContent *firstAppendedChild =
      aContainer->GetChildAt(aNewIndexInContainer);
    if (firstAppendedChild) {
      document = firstAppendedChild->GetDocument();
    }
    if (document)
      bindingManager = document->GetBindingManager();
    if (bindingManager) {
      nsCOMPtr<nsIContent> insParent;
      bindingManager->GetInsertionParent(firstAppendedChild, getter_AddRefs(insParent));
      if (insParent)
        hasInsertion = PR_TRUE;
    }
  }
  
  if (multiple || hasInsertion) {
    // We have an insertion point.  There are some additional tests we need to do
    // in order to ensure that an append is a safe operation.
    PRUint32 childCount = 0;
      
    if (!multiple) {
      // We may need to make multiple ContentInserted calls instead.  A
      // reasonable heuristic to employ (in order to maintain good performance)
      // is to find out if the insertion point's content node contains any
      // explicit children.  If it does not, then it is highly likely that 
      // an append is occurring.  (Note it is not definite, and there are insane
      // cases we will not deal with by employing this heuristic, but it beats
      // always falling back to multiple ContentInserted calls).
      //
      // In the multiple insertion point case, we know we're going to need to do
      // multiple ContentInserted calls anyway.
      childCount = insertionPoint->GetContent()->GetChildCount();
    }

    if (multiple || childCount > 0) {
      // Now comes the fun part.  For each appended child, we must obtain its
      // insertion point and find its exact position within that insertion point.
      // We then make a ContentInserted call with the correct computed index.
      nsIContent* insertionContent = insertionPoint->GetContent();
      
      PRUint32 containerCount = aContainer->GetChildCount();
      for (PRUint32 i = aNewIndexInContainer; i < containerCount; i++) {
        nsIContent *child = aContainer->GetChildAt(i);
        if (multiple) {
          // Filters are in effect, so the insertion point needs to be refetched for
          // each child.
          GetInsertionPoint(shell, parentFrame, child, &insertionPoint);
          insertionContent = insertionPoint->GetContent();
        }

        // Construct an iterator to locate this child at its correct index.
        ChildIterator iter, last;
        for (ChildIterator::Init(insertionContent, &iter, &last);
         iter != last;
         ++iter) {
          nsIContent* item = nsCOMPtr<nsIContent>(*iter);
          if (item == child)
            // Call ContentInserted with this index.
            ContentInserted(aPresContext, aContainer, nsnull, child,
                            iter.index(), mTempFrameTreeState, PR_FALSE);
        }
      }

      return NS_OK;
    }
  }

  parentFrame = insertionPoint;

  // If the frame we are manipulating is a ``special'' frame (that
  // is, one that's been created as a result of a block-in-inline
  // situation) then do something different instead of just
  // appending newly created frames. Note that only the
  // first-in-flow is marked so we check before getting to the
  // last-in-flow.
  //
  // We run into this situation occasionally while loading web
  // pages, typically when some content generation tool has
  // sprinkled invalid markup into the document. More often than
  // not, we'll be able to just use the normal fast-path frame
  // construction code, because the frames will be appended to the
  // ``anonymous'' block that got created to parent the block
  // children of the inline.
  if (IsFrameSpecial(parentFrame)) {
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::ContentAppended: parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" is special\n");
    }
#endif

    // Since we're appending, we'll walk to the last anonymous frame
    // that was created for the broken inline frame.
    nsFrameManager *frameManager = shell->FrameManager();

    while (1) {
      nsIFrame* sibling;
      GetSpecialSibling(frameManager, parentFrame, &sibling);
      if (! sibling)
        break;

      parentFrame = sibling;
    }

    // If this frame is the anonymous block frame, then all's well:
    // just append frames as usual.
    const nsStyleDisplay* display = parentFrame->GetStyleDisplay();

    if (NS_STYLE_DISPLAY_BLOCK != display->mDisplay) {
      // Nope, it's an inline, so just reframe the entire stinkin' mess if the
      // content is a block. We _could_ do better here with a little more work...
      // find out if the child is a block or inline, an inline means we don't have to reframe
      nsIContent *child = aContainer->GetChildAt(aNewIndexInContainer);
      PRBool needReframe = !child;
      if (child && child->IsContentOfType(nsIContent::eELEMENT)) {
        nsRefPtr<nsStyleContext> styleContext;
        styleContext = ResolveStyleContext(aPresContext, parentFrame, child);
        // XXX since the block child goes in the last inline of the sacred triad, frames would 
        // need to be moved into the 2nd triad (block) but that is more work, for now bail.
        needReframe = styleContext->GetStyleDisplay()->IsBlockLevel();
      }
      if (needReframe)
        return ReframeContainingBlock(aPresContext, parentFrame);
    }
  }

  // Get the parent frame's last-in-flow
  parentFrame = parentFrame->GetLastInFlow();

  // If we didn't process children when we originally created the frame,
  // then don't do any processing now
  nsIAtom* frameType = parentFrame->GetType();
  if (frameType == nsLayoutAtoms::objectFrame) {
    // This handles APPLET, EMBED, and OBJECT
    return NS_OK;
  }
  // Deal with inner/outer tables, fieldsets
  parentFrame = ::GetAdjustedParentFrame(aPresContext, parentFrame, frameType,
                                         aContainer, aNewIndexInContainer);
  // Create some new frames
  PRUint32                count;
  nsIFrame*               firstAppendedFrame = nsnull;
  nsFrameItems            frameItems;
  nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(aPresContext, parentFrame),
                                GetFloatContainingBlock(aPresContext, parentFrame));

  // See if the containing block has :first-letter style applied.
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
  nsIContent* blockContent = containingBlock->GetContent();
  nsStyleContext* blockSC = containingBlock->GetStyleContext();
  HaveSpecialBlockStyle(aPresContext, blockContent, blockSC,
                        &haveFirstLetterStyle,
                        &haveFirstLineStyle);

  if (haveFirstLetterStyle) {
    // Before we get going, remove the current letter frames
    RemoveLetterFrames(aPresContext, state.mPresShell,
                       state.mFrameManager, containingBlock);
  }

  // if the container is a table and a caption was appended, it needs to be put in
  // the outer table frame's additional child list. 
  nsFrameItems captionItems; 

  PRUint32 i;
  count = aContainer->GetChildCount();
  for (i = aNewIndexInContainer; i < count; i++) {
    nsIContent *childContent = aContainer->GetChildAt(i);

    // construct a child of a table frame by putting it on a temporary list and then
    // moving it into the appropriate list. This is more efficient than checking the display
    // type of childContent. During the construction of a caption frame, the outer 
    // table frame will be used as its parent.
    if (nsLayoutAtoms::tableFrame == frameType) {
      nsFrameItems tempItems;
      ConstructFrame(shell, aPresContext, state, childContent, parentFrame, tempItems);
      if (tempItems.childList) {
        if (nsLayoutAtoms::tableCaptionFrame == tempItems.childList->GetType()) {
          PRBool abortCaption = PR_FALSE;
          // check if a caption already exists in captionItems, and if so, abort the caption
          if (captionItems.childList) {
            abortCaption = PR_TRUE;
          }
          else {
            // check for a caption already appended to the outer table
            nsIFrame* outerTable = parentFrame->GetParent();
            if (outerTable) { 
              nsIFrame* existingCaption =
                outerTable->GetFirstChild(nsLayoutAtoms::captionList);
              if (existingCaption) {
                abortCaption = PR_TRUE;
              }
            }
          }
          if (abortCaption) {
            tempItems.childList->Destroy(aPresContext);
          }
          else {
            captionItems.AddChild(tempItems.childList);
          }
        }
        else {
          frameItems.AddChild(tempItems.childList);
        }
      }
    }
    // Don't create child frames for iframes/frames, they should not
    // display any content that they contain.
    else if (nsLayoutAtoms::subDocumentFrame != frameType) {
      // Construct a child frame (that does not have a table as parent)
      ConstructFrame(shell, aPresContext, state, childContent, parentFrame, frameItems);
    }
  }

  // We built some new frames.  Initialize any newly-constructed bindings.
  mDocument->GetBindingManager()->ProcessAttachedQueue();

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

  nsresult result = NS_OK;
  firstAppendedFrame = frameItems.childList;
  if (!firstAppendedFrame) {
    firstAppendedFrame = captionItems.childList;
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
      if (WipeContainingBlock(aPresContext, state, containingBlock,
                              parentFrame, frameItems.childList)) {
        return NS_OK;
      }
    }

    // Append the flowed frames to the principal child list, tables need special treatment
    if (nsLayoutAtoms::tableFrame == frameType) {
      if (captionItems.childList) { // append the caption to the outer table
        nsIFrame* outerTable = parentFrame->GetParent();
        if (outerTable) { 
          AppendFrames(aPresContext, shell, state.mFrameManager, aContainer,
                       outerTable, captionItems.childList); 
        }
      }
      if (frameItems.childList) { // append children of the inner table
        AppendFrames(aPresContext, shell, state.mFrameManager, aContainer,
                     parentFrame, frameItems.childList);
      }
    }
    else {
      AppendFrames(aPresContext, shell, state.mFrameManager, aContainer,
                   parentFrame, firstAppendedFrame);
    }

    InsertOutOfFlowFrames(state, aPresContext);

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
    nsIContent *childContent = aContainer->GetChildAt(aNewIndexInContainer);
    if (childContent) {
      RemoveDummyFrameFromSelect(aPresContext, shell, aContainer,
                                 childContent, sel);
    }
  } 

#ifdef DEBUG
  if (gReallyNoisyContentUpdates) {
    nsIFrameDebug* fdbg = nsnull;
    CallQueryInterface(parentFrame, &fdbg);
    if (fdbg) {
      printf("nsCSSFrameConstructor::ContentAppended: resulting frame model:\n");
      fdbg->List(aPresContext, stdout, 0);
    }
  }
#endif

  return NS_OK;
}


nsresult
nsCSSFrameConstructor::AddDummyFrameToSelect(nsPresContext*  aPresContext,
                                        nsIPresShell*    aPresShell,
                                        nsFrameConstructorState& aState,
                                        nsIFrame*        aListFrame,
                                        nsIFrame*        aParentFrame,
                                        nsFrameItems*    aChildItems,
                                        nsIContent*      aContainer,
                                        nsIDOMHTMLSelectElement* aSelectElement)
{
  PRUint32 numOptions = 0;
  nsresult rv = aSelectElement->GetLength(&numOptions);
  if (NS_SUCCEEDED(rv) && 0 == numOptions) {
    nsISelectControlFrame* listFrame = nsnull;
    CallQueryInterface(aListFrame, &listFrame);
    if (listFrame) {
      nsIFrame* dummyFrame;
      listFrame->GetDummyFrame(&dummyFrame);

      if (!dummyFrame) {
        nsStyleContext* styleContext = aParentFrame->GetStyleContext();
        nsIFrame*         generatedFrame = nsnull;
        if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState,
                                        aParentFrame, aContainer,
                                        styleContext,
                                        nsCSSAnonBoxes::dummyOption,
                                        &generatedFrame)) {
          // Add the generated frame to the child list
          if (aChildItems) {
            aChildItems->AddChild(generatedFrame);
          } else {
            aPresShell->FrameManager()->AppendFrames(aParentFrame, nsnull,
                                                     generatedFrame);
          }

          listFrame->SetDummyFrame(generatedFrame);
          return NS_OK;
        }
      }
    }
  }

  return NS_ERROR_FAILURE;
}

// defined below
static nsresult
DeletingFrameSubtree(nsPresContext*  aPresContext,
                     nsIPresShell*    aPresShell,
                     nsFrameManager*  aFrameManager,
                     nsIFrame*        aFrame);

nsresult
nsCSSFrameConstructor::RemoveDummyFrameFromSelect(nsPresContext* aPresContext,
                                                  nsIPresShell *  aPresShell,
                                                  nsIContent*     aContainer,
                                                  nsIContent*     aChild,
                                                  nsIDOMHTMLSelectElement * aSelectElement)
{
  // Check to see if this is the first thing we have added to this frame.
  
  PRUint32 numOptions = 0;
  nsresult rv = aSelectElement->GetLength(&numOptions);
  if (NS_SUCCEEDED(rv) && numOptions > 0) {
    nsIFrame* frame;
    aPresShell->GetPrimaryFrameFor(aContainer, &frame);
    if (frame) {
      nsISelectControlFrame* listFrame = nsnull;
      CallQueryInterface(frame, &listFrame);

      if (listFrame) {
        nsIFrame* dummyFrame;
        listFrame->GetDummyFrame(&dummyFrame);

        if (dummyFrame) {
          listFrame->SetDummyFrame(nsnull);

          // get the child's parent frame (which ought to be the list frame)
          nsIFrame* parentFrame = dummyFrame->GetParent();

          nsFrameManager *frameManager = aPresShell->FrameManager();
          DeletingFrameSubtree(aPresContext, aPresShell, frameManager,
                               dummyFrame);
          frameManager->RemoveFrame(parentFrame, nsnull, dummyFrame);
          return NS_OK;
        }
      }
    }
  }

  return NS_ERROR_FAILURE;
}

// Return TRUE if the insertion of aChild into aParent1,2 should force a reframe. aParent1 is 
// the special inline container which contains a block. aParentFrame is approximately aParent1's
// primary frame and will be set to the correct parent of aChild if a reframe is not necessary. 
// aParent2 is aParentFrame's content. aPrevSibling will be set to the correct prev sibling of
// aChild if a reframe is not necessary.
PRBool
nsCSSFrameConstructor::NeedSpecialFrameReframe(nsIPresShell*   aPresShell,
                                               nsPresContext* aPresContext,
                                               nsIContent*     aParent1,     
                                               nsIContent*     aParent2,     
                                               nsIFrame*&      aParentFrame, 
                                               nsIContent*     aChild,
                                               PRInt32         aIndexInContainer,
                                               nsIFrame*&      aPrevSibling,
                                               nsIFrame*       aNextSibling)
{
  NS_ENSURE_TRUE(aPrevSibling || aNextSibling, PR_TRUE);

  if (!IsInlineFrame2(aParentFrame)) 
    return PR_FALSE;

  // find out if aChild is a block or inline
  PRBool childIsBlock = PR_FALSE;
  if (aChild->IsContentOfType(nsIContent::eELEMENT)) {
    nsRefPtr<nsStyleContext> styleContext;
    styleContext = ResolveStyleContext(aPresContext, aParentFrame, aChild);
    childIsBlock = styleContext->GetStyleDisplay()->IsBlockLevel();
  }
  nsIFrame* prevParent; // parent of prev sibling
  nsIFrame* nextParent; // parent of next sibling

  if (childIsBlock) { 
    if (aPrevSibling) {
      prevParent = aPrevSibling->GetParent(); 
      NS_ASSERTION(prevParent, "program error - null parent frame");
      if (IsInlineFrame2(prevParent)) { // prevParent is an inline
        // XXX we need to find out if prevParent is the 1st inline or the last. If it
        // is the 1st, then aChild and the frames after aPrevSibling within the 1st inline
        // need to be moved to the block(inline). If it is the last, then aChild and the
        // frames before aPrevSibling within the last need to be moved to the block(inline). 
        return PR_TRUE; // For now, bail.
      }        
      aParentFrame = prevParent; // prevParent is a block, put aChild there
    }
    else {
      nsIFrame* nextSibling = (aIndexInContainer >= 0)
                              ? FindNextSibling(aPresShell, aParent2, aParentFrame, aIndexInContainer)
                              : FindNextAnonymousSibling(aPresShell, mDocument, aParent1, aChild);
      if (nextSibling) {
        nextParent = nextSibling->GetParent(); 
        NS_ASSERTION(nextParent, "program error - null parent frame");
        if (IsInlineFrame2(nextParent)) {
          // XXX we need to move aChild, aNextSibling and all the frames after aNextSibling within
          // the 1st inline to the block(inline).
          return PR_TRUE; // for now, bail
        }
        // put aChild in nextParent which is the block(inline) and leave aPrevSibling null
        aParentFrame = nextParent;
      }
    }           
  }
  else { // aChild is an inline
    if (aPrevSibling) {
      prevParent = aPrevSibling->GetParent(); 
      NS_ASSERTION(prevParent, "program error - null parent frame");
      if (IsInlineFrame2(prevParent)) { // prevParent is an inline
        // aChild goes into the same inline frame as aPrevSibling
        aParentFrame = aPrevSibling->GetParent();
        NS_ASSERTION(aParentFrame, "program error - null parent frame");
      }
      else { // prevParent is a block
        nsIFrame* nextSibling = (aIndexInContainer >= 0)
                                ? FindNextSibling(aPresShell, aParent2, aParentFrame, aIndexInContainer)
                                : FindNextAnonymousSibling(aPresShell, mDocument, aParent1, aChild);
        if (nextSibling) {
          nextParent = nextSibling->GetParent();
          NS_ASSERTION(nextParent, "program error - null parent frame");
          if (IsInlineFrame2(nextParent)) {
            // nextParent is the ending inline frame. Put aChild there and
            // set aPrevSibling to null so aChild is its first element.
            aParentFrame = nextSibling->GetParent(); 
            NS_ASSERTION(aParentFrame, "program error - null parent frame");
            aPrevSibling = nsnull; 
          }
          else { // nextParent is a block
            // prevParent and nextParent should be the same, and aChild goes there
            NS_ASSERTION(prevParent == nextParent, "special frame error");
            aParentFrame = prevParent;
          }
        }
        else { 
          // there is no ending enline frame (which should never happen) but aChild needs to go 
          // there, so for now just bail and force a reframe.
          NS_ASSERTION(PR_FALSE, "no last inline frame");
          return PR_TRUE;
        }
      }
    }
    // else aChild goes into the 1st inline frame which is aParentFrame
  }
  return PR_FALSE;
}

#ifdef MOZ_XUL

enum content_operation
{
    CONTENT_INSERTED,
    CONTENT_REMOVED
};

// Helper function to lookup the listbox body frame and send a notification
// for insertion or removal of content
static
PRBool NotifyListBoxBody(nsPresContext*    aPresContext,
                         nsIContent*        aContainer,
                         nsIContent*        aChild,
                         PRInt32            aIndexInContainer,
                         nsIDocument*       aDocument,                         
                         nsIFrame*          aChildFrame,
                         PRBool             aUseXBLForms,
                         content_operation  aOperation)
{
  if (!aContainer)
    return PR_FALSE;

  if (aContainer->IsContentOfType(nsIContent::eXUL) &&
      aChild->IsContentOfType(nsIContent::eXUL) &&
      aContainer->Tag() == nsXULAtoms::listbox &&
      aChild->Tag() == nsXULAtoms::listitem) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(aContainer);
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsIListBoxObject> listBoxObject = do_QueryInterface(boxObject);
    if (listBoxObject) {
      nsIListBoxObject* listboxBody;
      listBoxObject->GetListboxBody(&listboxBody);
      if (listboxBody) {
        nsListBoxBodyFrame *listBoxBodyFrame = NS_STATIC_CAST(nsListBoxBodyFrame*, listboxBody);
        if (aOperation == CONTENT_REMOVED)
          listBoxBodyFrame->OnContentRemoved(aPresContext, aChildFrame, aIndexInContainer);
        else
          listBoxBodyFrame->OnContentInserted(aPresContext, aChild);
        //NS_RELEASE(listBoxBodyFrame); frames aren't refcounted
      }
      return PR_TRUE;
    }
  }

  nsCOMPtr<nsIAtom> tag;
  PRInt32 namespaceID;
  aDocument->GetBindingManager()->ResolveTag(aContainer, &namespaceID,
                                             getter_AddRefs(tag));

  // Just ignore tree tags, anyway we don't create any frames for them.
  if (tag == nsXULAtoms::treechildren ||
      tag == nsXULAtoms::treeitem ||
      tag == nsXULAtoms::treerow ||
      (namespaceID == kNameSpaceID_XUL && aUseXBLForms &&
       ShouldIgnoreSelectChild(aContainer)))
    return PR_TRUE;

  return PR_FALSE;
}
#endif // MOZ_XUL

nsresult
nsCSSFrameConstructor::ContentInserted(nsPresContext*        aPresContext,
                                       nsIContent*            aContainer,
                                       nsIFrame*              aContainerFrame,
                                       nsIContent*            aChild,
                                       PRInt32                aIndexInContainer,
                                       nsILayoutHistoryState* aFrameState,
                                       PRBool                 aInReinsertContent)
{
  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentInserted container=%p child=%p index=%d\n",
           NS_STATIC_CAST(void*, aContainer),
           NS_STATIC_CAST(void*, aChild),
           aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      (aContainer ? aContainer : aChild)->List(stdout, 0);
    }
  }
#endif

  nsIPresShell *shell = aPresContext->PresShell();
  nsresult rv = NS_OK;

#ifdef MOZ_XUL
  if (NotifyListBoxBody(aPresContext, aContainer, aChild, aIndexInContainer, 
                        mDocument, nsnull, gUseXBLForms, CONTENT_INSERTED))
    return NS_OK;
#endif // MOZ_XUL
  
  // If we have a null parent, then this must be the document element
  // being inserted
  if (! aContainer) {
    nsIContent *docElement = mDocument->GetRootContent();

    if (aChild == docElement) {
      NS_PRECONDITION(nsnull == mInitialContainingBlock, "initial containing block already created");
      
      if (!mDocElementContainingBlock)
        return NS_OK; // We get into this situation when an XBL binding is asynchronously
                      // applied to the root tag (e.g., <window> in XUL).  It's ok.  We can
                      // just bail here because the root will really be built later during
                      // InitialReflow.

      // Create frames for the document element and its child elements
      nsIFrame*               docElementFrame;
      nsFrameConstructorState state(aPresContext, mFixedContainingBlock, nsnull, nsnull, aFrameState);
      ConstructDocElementFrame(shell, aPresContext, 
                               state,
                               docElement, 
                               mDocElementContainingBlock,
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

#ifdef DEBUG
      if (gReallyNoisyContentUpdates && docElementFrame) {
        nsIFrameDebug* fdbg = nsnull;
        CallQueryInterface(docElementFrame, &fdbg);
        if (fdbg) {
          printf("nsCSSFrameConstructor::ContentInserted: resulting frame model:\n");
          fdbg->List(aPresContext, stdout, 0);
        }
      }
#endif
    }

    mDocument->GetBindingManager()->ProcessAttachedQueue();

    // otherwise this is not a child of the root element, and we
    // won't let it have a frame.
    return NS_OK;
  }

  // Otherwise, we've got parent content. Find its frame.
  nsIFrame* parentFrame = aContainerFrame;
  if (!parentFrame) {
    parentFrame = GetFrameFor(shell, aPresContext, aContainer);
    if (! parentFrame)
      return NS_OK; // XXXwaterson will this break selects? (See ``Here
    // we have been notified...'' below.)
  }

  // See if we have an XBL insertion point. If so, then that's our
  // real parent frame; if not, then the frame hasn't been built yet
  // and we just bail.
  nsIFrame* insertionPoint;
  GetInsertionPoint(shell, parentFrame, aChild, &insertionPoint);
  if (! insertionPoint)
    return NS_OK; // Don't build the frames.

  parentFrame = insertionPoint;

  // Find the frame that precedes the insertion point. Walk backwards
  // from the parent frame to get the parent content, because if an
  // XBL insertion point is involved, we'll need to use _that_ to find
  // the preceding frame.
  nsIContent* container = parentFrame->GetContent();

  // XXX if the insertionPoint was different from the original
  // parentFrame, then aIndexInContainer is most likely completely
  // wrong. What we need to do here is remember the original index,
  // then as we insert, search the child list where we're about to put
  // the new frame to make sure that it appears after any siblings
  // with a lower index, and before any siblings with a higher
  // index. Same with FindNextSibling(), below.
  nsIFrame* prevSibling = (aIndexInContainer >= 0)
    ? FindPreviousSibling(shell, container, parentFrame, aIndexInContainer, aChild)
    : FindPreviousAnonymousSibling(shell, mDocument, aContainer, aChild);

  PRBool    isAppend = PR_FALSE;
  nsIFrame* nextSibling = nsnull;
    
  // If there is no previous sibling, then find the frame that follows
  if (! prevSibling) {
    nextSibling = (aIndexInContainer >= 0)
      ? FindNextSibling(shell, container, parentFrame, aIndexInContainer, aChild)
      : FindNextAnonymousSibling(shell, mDocument, aContainer, aChild);
  }

  PRBool handleSpecialFrame = IsFrameSpecial(parentFrame) && !aInReinsertContent;

  // Now, find the geometric parent so that we can handle
  // continuations properly. Use the prev sibling if we have it;
  // otherwise use the next sibling.
  if (prevSibling) {
    if (!handleSpecialFrame)
      parentFrame = prevSibling->GetParent();
  }
  else if (nextSibling) {
    if (!handleSpecialFrame)
      parentFrame = nextSibling->GetParent();
  }
  else {
    // No previous or next sibling, so treat this like an appended frame.
    isAppend = PR_TRUE;
      
    // If we didn't process children when we originally created the frame,
    // then don't do any processing now
    nsIAtom* frameType = parentFrame->GetType();
    if (frameType == nsLayoutAtoms::objectFrame) {
      // This handles APPLET, EMBED, and OBJECT
      return NS_OK;
    }
    // Deal with inner/outer tables, fieldsets
    parentFrame = ::GetAdjustedParentFrame(aPresContext, parentFrame, frameType,
                                           aContainer, aIndexInContainer);
  }

  // If the frame we are manipulating is a special frame then see if we need to reframe 
  // NOTE: if we are in ReinsertContent, then don't reframe as we are already doing just that!
  if (handleSpecialFrame) {
    // a special inline frame has propagated some of its children upward to be children 
    // of the block and those frames may need to move around. Sometimes we may need to reframe
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::ContentInserted: parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" is special\n");
    }
#endif
    // if we don't need to reframe then set parentFrame and prevSibling to the correct values
    if (NeedSpecialFrameReframe(shell, aPresContext, aContainer, container, parentFrame, 
                                aChild, aIndexInContainer, prevSibling, nextSibling)) {
      return ReframeContainingBlock(aPresContext, parentFrame);
    }
  }

  nsFrameItems            frameItems;
  nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(aPresContext, parentFrame),
                                GetFloatContainingBlock(aPresContext, parentFrame),
                                aFrameState);

  // Recover state for the containing block - we need to know if
  // it has :first-letter or :first-line style applied to it. The
  // reason we care is that the internal structure in these cases
  // is not the normal structure and requires custom updating
  // logic.
  nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
  nsStyleContext* blockSC;
  nsIContent* blockContent = nsnull;
  PRBool haveFirstLetterStyle = PR_FALSE;
  PRBool haveFirstLineStyle = PR_FALSE;

  // In order to shave off some cycles, we only dig up the
  // containing block haveFirst* flags if the parent frame where
  // the insertion/append is occuring is an inline or block
  // container. For other types of containers this isn't relevant.
  const nsStyleDisplay* parentDisplay = parentFrame->GetStyleDisplay();

  // Examine the parentFrame where the insertion is taking
  // place. If its a certain kind of container then some special
  // processing is done.
  if ((NS_STYLE_DISPLAY_BLOCK == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_LIST_ITEM == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_INLINE == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_INLINE_BLOCK == parentDisplay->mDisplay)) {
    // Recover the special style flags for the containing block
    blockSC = containingBlock->GetStyleContext();
    blockContent = containingBlock->GetContent();
    HaveSpecialBlockStyle(aPresContext, blockContent, blockSC,
                          &haveFirstLetterStyle,
                          &haveFirstLineStyle);

    if (haveFirstLetterStyle) {
      // Get the correct parentFrame and prevSibling - if a
      // letter-frame is present, use its parent.
      if (parentFrame->GetType() == nsLayoutAtoms::letterFrame) {
        if (prevSibling)
          prevSibling = parentFrame;

        parentFrame = parentFrame->GetParent();
      }

      // Remove the old letter frames before doing the insertion
      RemoveLetterFrames(aPresContext, state.mPresShell,
                         state.mFrameManager,
                         state.mFloatedItems.containingBlock);

      // Check again to see if the frame we are manipulating is part
      // of a block-in-inline hierarchy.
      if (IsFrameSpecial(parentFrame)) {
        nsCOMPtr<nsIContent> parentContainer = blockContent->GetParent();
#ifdef DEBUG
        if (gNoisyContentUpdates) {
          printf("nsCSSFrameConstructor::ContentInserted: parentFrame=");
          nsFrame::ListTag(stdout, parentFrame);
          printf(" is special inline\n");
          printf("  ==> blockContent=%p, parentContainer=%p\n",
                 NS_STATIC_CAST(void*, blockContent),
                 NS_STATIC_CAST(void*, parentContainer));
        }
#endif
        if (parentContainer) {
          ReinsertContent(aPresContext, parentContainer, blockContent);
        }
        else {
          // XXX uh oh. the block that needs reworking has no parent...
          NS_NOTREACHED("block that needs recreation has no parent");
        }

        return NS_OK;
      }
    }
  }

  ConstructFrame(shell, aPresContext, state, aChild, parentFrame, frameItems);

  // Now that we've created frames, run the attach queue.
  //XXXwaterson should we do this after we've processed pseudos, too?
  mDocument->GetBindingManager()->ProcessAttachedQueue();

  // process the current pseudo frame state
  if (!state.mPseudoFrames.IsEmpty())
    ProcessPseudoFrames(aPresContext, state.mPseudoFrames, frameItems);

  // XXX Bug 19949
  // Although select frame are inline we do not want to call
  // WipeContainingBlock because it will throw away the entire select frame and 
  // start over which is something we do not want to do
  //
  nsCOMPtr<nsIDOMHTMLSelectElement> selectContent = do_QueryInterface(aContainer);
  if (!selectContent) {
    // Perform special check for diddling around with the frames in
    // a special inline frame.
    if (WipeContainingBlock(aPresContext, state, containingBlock, parentFrame,
                            frameItems.childList))
      return NS_OK;
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
  if (NS_SUCCEEDED(rv) && newFrame) {
    // Notify the parent frame
    if (isAppend) {
      AppendFrames(aPresContext, shell, state.mFrameManager,
                   aContainer, parentFrame, newFrame);
    }
    else {
      if (!prevSibling) {
        // We're inserting the new frame as the first child. See if the
        // parent has a :before pseudo-element
        nsIFrame* firstChild = parentFrame->GetFirstChild(nsnull);

        if (firstChild &&
            nsLayoutUtils::IsGeneratedContentFor(aContainer, firstChild,
                                                 nsCSSPseudoElements::before)) {
          // Insert the new frames after the :before pseudo-element
          prevSibling = firstChild;
        }
      }

      // check for a table caption which goes on an additional child list
      nsIFrame* outerTableFrame; 
      if (GetCaptionAdjustedParent(parentFrame, newFrame, &outerTableFrame)) {
        // XXXwaterson this seems wrong; i.e., how can we assume
        // that appending is the right thing to do here?
        state.mFrameManager->AppendFrames(outerTableFrame,
                                          nsLayoutAtoms::captionList, newFrame);
      }
      else {
        state.mFrameManager->InsertFrames(parentFrame,
                                          nsnull, prevSibling, newFrame);
      }
    }

    InsertOutOfFlowFrames(state, aPresContext);

    if (haveFirstLetterStyle) {
      // Recover the letter frames for the containing block when
      // it has first-letter style.
      RecoverLetterFrames(shell, aPresContext, state,
                          state.mFloatedItems.containingBlock);
    }
  }

  // Here we have been notified that content has been insert
  // so if the select now has a single item 
  // we need to go in and removed the dummy frame
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = do_QueryInterface(aContainer);
  if (selectElement)
    RemoveDummyFrameFromSelect(aPresContext, shell, aContainer, aChild, selectElement);

#ifdef DEBUG
  if (gReallyNoisyContentUpdates && parentFrame) {
    nsIFrameDebug* fdbg = nsnull;
    CallQueryInterface(parentFrame, &fdbg);
    if (fdbg) {
      printf("nsCSSFrameConstructor::ContentInserted: resulting frame model:\n");
      fdbg->List(aPresContext, stdout, 0);
    }
  }
#endif

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ReinsertContent(nsPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       nsIContent*     aChild)
{
  PRInt32 ix = aContainer->IndexOf(aChild);
  // XXX For now, do a brute force remove and insert.
  nsresult res = ContentRemoved(aPresContext, aContainer, 
                                aChild, ix, PR_TRUE);

  if (NS_SUCCEEDED(res)) {
    res = ContentInserted(aPresContext, aContainer,  nsnull,
                          aChild, ix, nsnull, PR_TRUE);
  }

  return res;
}

/**
 * Called when a frame subtree is about to be deleted. Two important
 * things happen:
 *
 * 1. For each frame in the subtree, we remove the mapping from the
 *    content object to its frame
 *
 * 2. For child frames that have been moved out of the flow, we
 *    enqueue the out-of-frame for deletion *if* the out-of-flow frame's
 *    geometric parent is not in |aRemovedFrame|'s hierarchy (e.g., an
 *    absolutely positioned element that has been promoted to be a direct
 *    descendant of an area frame).
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
DoDeletingFrameSubtree(nsPresContext*  aPresContext,
                       nsIPresShell*    aPresShell,
                       nsFrameManager*  aFrameManager,
                       nsVoidArray&     aDestroyQueue,
                       nsIFrame*        aRemovedFrame,
                       nsIFrame*        aFrame)
{
  NS_PRECONDITION(aFrameManager, "no frame manager");

  // Remove the mapping from the content object to its frame
  nsIContent* content = aFrame->GetContent();
  if (content) {
    aFrameManager->SetPrimaryFrameFor(content, nsnull);
    aFrame->RemovedAsPrimaryFrame(aPresContext);
    aFrameManager->ClearAllUndisplayedContentIn(content);
  }

  // Walk aFrame's child frames
  nsIAtom* childListName = nsnull;
  PRInt32 childListIndex = 0;

  do {
    // Walk aFrame's child frames looking for placeholder frames
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {
      // The subtree we need to follow to get to the children; by
      // default, the childFrame.
      nsIFrame* subtree = childFrame;

      // See if it's a placeholder frame
      if (nsLayoutAtoms::placeholderFrame == childFrame->GetType()) {
        // Get the out-of-flow frame
        nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)childFrame)->GetOutOfFlowFrame();
        NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
  
        // Remove the mapping from the out-of-flow frame to its placeholder
        aFrameManager->UnregisterPlaceholderFrame((nsPlaceholderFrame*)childFrame);
        
        // Destroy the out-of-flow frame only if aRemovedFrame is _not_
        // one of its ancestor frames or if it is a popup frame. 
        // If aRemovedFrame is an ancestor of the out-of-flow frame, then 
        // the out-of-flow frame will be destroyed by aRemovedFrame.
        const nsStyleDisplay* display = outOfFlowFrame->GetStyleDisplay();
        if (display->mDisplay == NS_STYLE_DISPLAY_POPUP ||
            !nsLayoutUtils::IsProperAncestorFrame(aRemovedFrame, outOfFlowFrame)) {
          if (aDestroyQueue.IndexOf(outOfFlowFrame) < 0)
            aDestroyQueue.AppendElement(outOfFlowFrame);
        }

        // We want to descend into the out-of-flow frame's subtree,
        // not the placeholder frame's!
        subtree = outOfFlowFrame;
      }

      // Recursively find and delete any of its out-of-flow frames,
      // and remove the mapping from content objects to frames
      DoDeletingFrameSubtree(aPresContext, aPresShell, aFrameManager, aDestroyQueue,
                             aRemovedFrame, subtree);
  
      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }

    childListName = aFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);

  return NS_OK;
}

/**
 * Called when a frame is about to be deleted. Calls DoDeletingFrameSubtree()
 * for aFrame and each of its continuing frames
 */
static nsresult
DeletingFrameSubtree(nsPresContext*  aPresContext,
                     nsIPresShell*    aPresShell,
                     nsFrameManager*  aFrameManager,
                     nsIFrame*        aFrame)
{
  // If there's no frame manager it's probably because the pres shell is
  // being destroyed
  NS_ENSURE_TRUE(aFrame, NS_OK); // XXXldb Remove this sometime in the future.
  if (aFrameManager) {
    nsAutoVoidArray destroyQueue;

    // If it's a "special" block-in-inline frame, then we need to
    // remember to delete our special siblings, too.  Since every one of
    // the next-in-flows has the same special sibling, just do this
    // once, rather than in the loop below.
    if (IsFrameSpecial(aFrame)) {
      nsIFrame* specialSibling;
      GetSpecialSibling(aFrameManager, aFrame, &specialSibling);
      if (specialSibling)
        DeletingFrameSubtree(aPresContext, aPresShell, aFrameManager,
                             specialSibling);
    }

    do {
      DoDeletingFrameSubtree(aPresContext, aPresShell, aFrameManager,
                             destroyQueue, aFrame, aFrame);

      // If it's split, then get the continuing frame. Note that we only do
      // this for the top-most frame being deleted. Don't do it if we're
      // recursing over a subtree, because those continuing frames should be
      // found as part of the walk over the top-most frame's continuing frames.
      // Walking them again will make this an N^2/2 algorithm
      aFrame = aFrame->GetNextInFlow();
    } while (aFrame);

    // Now destroy any frames that have been enqueued for destruction.
    for (PRInt32 i = destroyQueue.Count() - 1; i >= 0; --i) {
      nsIFrame* outOfFlowFrame = NS_STATIC_CAST(nsIFrame*, destroyQueue[i]);

#ifdef MOZ_XUL
      const nsStyleDisplay* display = outOfFlowFrame->GetStyleDisplay();
      if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
        // Locate the root popup set and remove ourselves from the popup set's list
        // of popup frames.
        nsIFrame* rootFrame = aFrameManager->GetRootFrame();
        if (rootFrame)
          rootFrame = rootFrame->GetFirstChild(nsnull);
        nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
        NS_ASSERTION(rootBox, "unexpected null pointer");
        if (rootBox) {
          nsIFrame* popupSetFrame;
          rootBox->GetPopupSetFrame(&popupSetFrame);
          NS_ASSERTION(popupSetFrame, "unexpected null pointer");
          if (popupSetFrame) {
            nsCOMPtr<nsIPopupSetFrame> popupSet(do_QueryInterface(popupSetFrame));
            NS_ASSERTION(popupSet, "unexpected null pointer");
            if (popupSet)
              popupSet->RemovePopupFrame(outOfFlowFrame);
          }
        }
      } else
#endif
      {
        // Get the out-of-flow frame's parent
        nsIFrame* parentFrame = outOfFlowFrame->GetParent();
  
        // Get the child list name for the out-of-flow frame
        nsCOMPtr<nsIAtom> listName;
        GetChildListNameFor(aPresContext, parentFrame, outOfFlowFrame,
                            getter_AddRefs(listName));
  
        // Ask the parent to delete the out-of-flow frame
        aFrameManager->RemoveFrame(parentFrame,
                                   listName, outOfFlowFrame);
      }
    }
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveMappingsForFrameSubtree(nsPresContext* aPresContext,
                                                     nsIFrame* aRemovedFrame, 
                                                     nsILayoutHistoryState* aFrameState)
{
  nsIPresShell *presShell = aPresContext->PresShell();
  
  // Save the frame tree's state before deleting it
  CaptureStateFor(aPresContext, aRemovedFrame, mTempFrameTreeState);

  return DeletingFrameSubtree(aPresContext, presShell,
                              presShell->FrameManager(), aRemovedFrame);
}

nsresult
nsCSSFrameConstructor::ContentRemoved(nsPresContext* aPresContext,
                                      nsIContent*     aContainer,
                                      nsIContent*     aChild,
                                      PRInt32         aIndexInContainer,
                                      PRBool          aInReinsertContent)
{
  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentRemoved container=%p child=%p index=%d\n",
           NS_STATIC_CAST(void*, aContainer),
           NS_STATIC_CAST(void*, aChild),
           aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsIPresShell *shell = aPresContext->PresShell();
  nsFrameManager *frameManager = shell->FrameManager();
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
  if (aContainer && childFrame) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = do_QueryInterface(aContainer);
    if (selectElement) {
      // XXX temp needed only native controls
      nsIFrame* selectFrame;
      // XXX temp needed only native controls
      shell->GetPrimaryFrameFor(aContainer, &selectFrame);

      // For "select" add the pseudo frame after the last item is deleted
      nsIFrame* parentFrame = childFrame->GetParent();
      if (shell && parentFrame && parentFrame != selectFrame) {
        nsFrameConstructorState state(aPresContext,
                                      nsnull, nsnull, nsnull);
        AddDummyFrameToSelect(aPresContext, shell, state,
                              selectFrame, parentFrame, nsnull,
                              aContainer, selectElement);
      }
    } 
  }

#ifdef MOZ_XUL
  if (NotifyListBoxBody(aPresContext, aContainer, aChild, aIndexInContainer, 
                        mDocument, childFrame, gUseXBLForms, CONTENT_REMOVED))
    return NS_OK;

#endif // MOZ_XUL

  if (childFrame) {
    // If the frame we are manipulating is a special frame then do
    // something different instead of just inserting newly created
    // frames.
    // NOTE: if we are in ReinsertContent, 
    //       then do not reframe as we are already doing just that!
    if (IsFrameSpecial(childFrame) && !aInReinsertContent) {
      // We are pretty harsh here (and definitely not optimal) -- we
      // wipe out the entire containing block and recreate it from
      // scratch. The reason is that because we know that a special
      // inline frame has propagated some of its children upward to be
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
    nsIFrame* parentFrame = childFrame->GetParent();

    // See if we have an XBL insertion point. If so, then that's our
    // real parent frame; if not, then the frame hasn't been built yet
    // and we just bail.
    nsIFrame* insertionPoint;
    GetInsertionPoint(shell, parentFrame, aChild, &insertionPoint);
    if (! insertionPoint)
      return NS_OK;

    parentFrame = insertionPoint;

    // Examine the containing-block for the removed content and see if
    // :first-letter style applies.
    nsIFrame* containingBlock =
      GetFloatContainingBlock(aPresContext, parentFrame);
    nsStyleContext* blockSC = containingBlock->GetStyleContext();
    nsIContent* blockContent = containingBlock->GetContent();
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
      parentFrame = childFrame->GetParent();

#ifdef NOISY_FIRST_LETTER
      printf("  ==> revised parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" childFrame=");
      nsFrame::ListTag(stdout, childFrame);
      printf("\n");
#endif
    }

#ifdef DEBUG
    if (gReallyNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::ContentRemoved: childFrame=");
      nsFrame::ListTag(stdout, childFrame);
      printf("\n");

      if (parentFrame) {
        nsIFrameDebug* fdbg = nsnull;
        CallQueryInterface(parentFrame, &fdbg);
        if (fdbg)
          fdbg->List(aPresContext, stdout, 0);
      }
      else
        printf("  ==> no parent frame\n");
    }
#endif

    // Walk the frame subtree deleting any out-of-flow frames, and
    // remove the mapping from content objects to frames
    DeletingFrameSubtree(aPresContext, shell, frameManager, childFrame);

    // See if the child frame is a floating frame
    //   (positioned frames are handled below in the "else" clause)
    const nsStyleDisplay* display = childFrame->GetStyleDisplay();
    nsPlaceholderFrame* placeholderFrame = nsnull;
    if (display->mDisplay == NS_STYLE_DISPLAY_POPUP)
      // Get the placeholder frame
      placeholderFrame = frameManager->GetPlaceholderFrameFor(childFrame);
      if (placeholderFrame) {
        // Remove the mapping from the frame to its placeholder
        frameManager->UnregisterPlaceholderFrame(placeholderFrame);
    
        // Locate the root popup set and remove ourselves from the popup set's list
        // of popup frames.
        nsIFrame* rootFrame = frameManager->GetRootFrame();
        if (rootFrame)
          rootFrame = rootFrame->GetFirstChild(nsnull);
#ifdef MOZ_XUL
        nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
        if (rootBox) {
          nsIFrame* popupSetFrame;
          rootBox->GetPopupSetFrame(&popupSetFrame);
          if (popupSetFrame) {
            nsCOMPtr<nsIPopupSetFrame> popupSet(do_QueryInterface(popupSetFrame));
            if (popupSet)
              popupSet->RemovePopupFrame(childFrame);
          }
        }
#endif

        // Remove the placeholder frame first (XXX second for now) (so
        // that it doesn't retain a dangling pointer to memory)
        if (placeholderFrame) {
          parentFrame = placeholderFrame->GetParent();
          DeletingFrameSubtree(aPresContext, shell, frameManager, placeholderFrame);
          frameManager->RemoveFrame(parentFrame, nsnull, placeholderFrame);
          return NS_OK;
        }
      }
      else if (display->IsFloating()) {
#ifdef NOISY_FIRST_LETTER
        printf("  ==> child display is still floating!\n");
#endif
        // Get the placeholder frame
        nsPlaceholderFrame* placeholderFrame =
          frameManager->GetPlaceholderFrameFor(childFrame);

        // Remove the mapping from the frame to its placeholder
        if (placeholderFrame)
          frameManager->UnregisterPlaceholderFrame(placeholderFrame);

        // Now we remove the floating frame

        // XXX has to be done first for now: the blocks line list
        // contains an array of pointers to the placeholder - we have to
        // remove the float first (which gets rid of the lines
        // reference to the placeholder and float) and then remove the
        // placeholder
        rv = frameManager->RemoveFrame(parentFrame,
                                       nsLayoutAtoms::floatList, childFrame);

        // Remove the placeholder frame first (XXX second for now) (so
        // that it doesn't retain a dangling pointer to memory)
        if (placeholderFrame) {
          parentFrame = placeholderFrame->GetParent();
          DeletingFrameSubtree(aPresContext, shell, frameManager, placeholderFrame);
          rv = frameManager->RemoveFrame(parentFrame,
                                         nsnull, placeholderFrame);
        }
      }
      // See if it's absolutely or fixed positioned
      else if (display->IsAbsolutelyPositioned()) {
        // Get the placeholder frame
        nsPlaceholderFrame* placeholderFrame =
          frameManager->GetPlaceholderFrameFor(childFrame);

        // Remove the mapping from the frame to its placeholder
        if (placeholderFrame)
          frameManager->UnregisterPlaceholderFrame(placeholderFrame);

        // Generate two notifications. First for the absolutely positioned
        // frame
        rv = frameManager->RemoveFrame(parentFrame,
           (NS_STYLE_POSITION_FIXED == display->mPosition) ?
           nsLayoutAtoms::fixedList : nsLayoutAtoms::absoluteList, childFrame);

        // Now the placeholder frame
        if (placeholderFrame) {
          parentFrame = placeholderFrame->GetParent();
          rv = frameManager->RemoveFrame(parentFrame, nsnull,
                                         placeholderFrame);
        }

      } else {
        // Notify the parent frame that it should delete the frame
        // check for a table caption which goes on an additional child list with a different parent
        nsIFrame* outerTableFrame; 
        if (GetCaptionAdjustedParent(parentFrame, childFrame, &outerTableFrame)) {
          rv = frameManager->RemoveFrame(outerTableFrame,
                                         nsLayoutAtoms::captionList,
                                         childFrame);
        }
        else {
          rv = frameManager->RemoveFrame(insertionPoint, nsnull, childFrame);
        }
      }

      if (mInitialContainingBlock == childFrame) {
        mInitialContainingBlock = nsnull;
      }

      if (haveFLS && mInitialContainingBlock) {
        nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                      GetAbsoluteContainingBlock(aPresContext,
                                                                 parentFrame),
                                      GetFloatContainingBlock(aPresContext,
                                                              parentFrame));
        RecoverLetterFrames(shell, aPresContext, state, containingBlock);
      }

#ifdef DEBUG
      if (gReallyNoisyContentUpdates && parentFrame) {
        nsIFrameDebug* fdbg = nsnull;
        CallQueryInterface(parentFrame, &fdbg);
        if (fdbg) {
          printf("nsCSSFrameConstructor::ContentRemoved: resulting frame model:\n");
          fdbg->List(aPresContext, stdout, 0);
        }
      }
#endif
  }

  return rv;
}

#ifdef DEBUG
  // To ensure that the functions below are only called within
  // |ApplyRenderingChangeToTree|.
static PRBool gInApplyRenderingChangeToTree = PR_FALSE;
#endif

static void
DoApplyRenderingChangeToTree(nsPresContext* aPresContext,
                             nsIFrame* aFrame,
                             nsIViewManager* aViewManager,
                             nsFrameManager* aFrameManager,
                             nsChangeHint aChange);

/**
 * @param aBoundsRect returns the bounds enclosing the areas covered by aFrame and its childre
 * This rect is relative to aFrame's parent
 */
static void
UpdateViewsForTree(nsPresContext* aPresContext, nsIFrame* aFrame, 
                   nsIViewManager* aViewManager, nsFrameManager* aFrameManager,
                   nsRect& aBoundsRect, nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  nsIView* view = aFrame->GetView();
  if (view) {
    if (aChange & nsChangeHint_RepaintFrame) {
      aViewManager->UpdateView(view, NS_VMREFRESH_NO_SYNC);
    }
    if (aChange & nsChangeHint_SyncFrameView) {
      nsContainerFrame::SyncFrameViewProperties(aPresContext, aFrame, nsnull, view);
    }
  }

  nsRect bounds = aFrame->GetOverflowRect();

  // now do children of frame
  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;

  do {
    nsIFrame* child = aFrame->GetFirstChild(childList);
    while (child) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        if (nsLayoutAtoms::placeholderFrame == child->GetType()) { // placeholder
          // get out of flow frame and start over there
          nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

          DoApplyRenderingChangeToTree(aPresContext, outOfFlowFrame,
                                       aViewManager, aFrameManager, aChange);
        }
        else {  // regular frame
          nsRect  childBounds;
          UpdateViewsForTree(aPresContext, child, aViewManager, aFrameManager, childBounds, aChange);
          bounds.UnionRect(bounds, childBounds);
        }
      }
      child = child->GetNextSibling();
    }
    childList = aFrame->GetAdditionalChildListName(listIndex++);
  } while (childList);

  nsPoint parentOffset = aFrame->GetPosition();
  aBoundsRect = bounds + parentOffset;
}

static void
DoApplyRenderingChangeToTree(nsPresContext* aPresContext,
                             nsIFrame* aFrame,
                             nsIViewManager* aViewManager,
                             nsFrameManager* aFrameManager,
                             nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  for ( ; aFrame; aFrame = GetNifOrSpecialSibling(aFrameManager, aFrame)) {
    // Get view if this frame has one and trigger an update. If the
    // frame doesn't have a view, find the nearest containing view
    // (adjusting r's coordinate system to reflect the nesting) and
    // update there.
    nsRect invalidRect;
    UpdateViewsForTree(aPresContext, aFrame, aViewManager, aFrameManager,
                       invalidRect, aChange);

    if (!aFrame->HasView()
        && (aChange & nsChangeHint_RepaintFrame)) {
      // if frame has view, will already be invalidated
      invalidRect -= aFrame->GetPosition();

      aFrame->Invalidate(invalidRect, PR_FALSE);
    }
  }
}

static void
ApplyRenderingChangeToTree(nsPresContext* aPresContext,
                           nsIFrame* aFrame,
                           nsIViewManager* aViewManager,
                           nsChangeHint aChange)
{
  nsIPresShell *shell = aPresContext->PresShell();
  PRBool isPaintingSuppressed = PR_FALSE;
  shell->IsPaintingSuppressed(&isPaintingSuppressed);
  if (isPaintingSuppressed) {
    // Don't allow synchronous rendering changes when painting is turned off.
    aChange = NS_SubtractHint(aChange, nsChangeHint_RepaintFrame);
    if (!aChange) {
      return;
    }
  }

  // If the frame's background is propagated to an ancestor, walk up to
  // that ancestor.
  const nsStyleBackground *bg;
  PRBool isCanvas;
  while (!nsCSSRendering::FindBackground(aPresContext, aFrame,
                                         &bg, &isCanvas)) {
    aFrame = aFrame->GetParent();
    NS_ASSERTION(aFrame, "root frame must paint");
  }

  nsIViewManager* viewManager = aViewManager;
  if (!viewManager) {
    viewManager = aPresContext->GetViewManager();
  }

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.

  // XXX this needs to detect the need for a view due to an opacity change and deal with it...

  viewManager->BeginUpdateViewBatch();

#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_TRUE;
#endif
  DoApplyRenderingChangeToTree(aPresContext, aFrame, viewManager,
                               shell->FrameManager(), aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_FALSE;
#endif

  viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
}

nsresult
nsCSSFrameConstructor::StyleChangeReflow(nsPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         nsIAtom* aAttribute)
{
  // If the frame hasn't even received an initial reflow, then don't
  // send it a style-change reflow!
  if (aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)
    return NS_OK;

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::StyleChangeReflow: aFrame=");
    nsFrame::ListTag(stdout, aFrame);
    printf("\n");
  }
#endif

  // Is it a box? If so we can coelesce.
  if (aFrame->IsBoxFrame()) {
    nsBoxLayoutState state(aPresContext);
    aFrame->MarkStyleChange(state);
  }
  else {
    // If the frame is part of a split block-in-inline hierarchy, then
    // target the style-change reflow at the first ``normal'' ancestor
    // so we're sure that the style change will propagate to any
    // anonymously created siblings.
    if (IsFrameSpecial(aFrame))
      aFrame = GetIBContainingBlockFor(aFrame);

    // Target a style-change reflow at the frame.
    nsHTMLReflowCommand *reflowCmd;
    nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                          eReflowType_StyleChanged,
                                          nsnull,
                                          aAttribute);
  
    if (NS_SUCCEEDED(rv))
      aPresContext->PresShell()->AppendReflowCommand(reflowCmd);
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::CharacterDataChanged(nsPresContext* aPresContext,
                                            nsIContent* aContent,
                                            PRBool aAppend)
{
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* frame;
  aPresContext->PresShell()->GetPrimaryFrameFor(aContent, &frame);

  // Notify the first frame that maps the content. It will generate a reflow
  // command

  // It's possible the frame whose content changed isn't inserted into the
  // frame hierarchy yet, or that there is no frame that maps the content
  if (nsnull != frame) {
#if 0
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsCSSFrameConstructor::CharacterDataChanged: content=%p[%s] subcontent=%p frame=%p",
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
    // CharacterDataChanged into a ReinsertContent when we are changing text
    // that is part of a first-letter situation.
    PRBool doCharacterDataChanged = PR_TRUE;
    nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
    if (textContent) {
      // Ok, it's text content. Now do some real work...
      nsIFrame* block = GetFloatContainingBlock(aPresContext, frame);
      if (block) {
        // See if the block has first-letter style applied to it.
        nsIContent* blockContent = block->GetContent();
        nsStyleContext* blockSC = block->GetStyleContext();
        PRBool haveFirstLetterStyle =
          HaveFirstLetterStyle(aPresContext, blockContent, blockSC);
        if (haveFirstLetterStyle) {
          // The block has first-letter style. Use content-replaced to
          // repair the blocks frame structure properly.
          nsCOMPtr<nsIContent> container = aContent->GetParent();
          if (container) {
            doCharacterDataChanged = PR_FALSE;
            rv = ReinsertContent(aPresContext, container, aContent);
          }
        }
      }
    }

    if (doCharacterDataChanged) {
      frame->CharacterDataChanged(aPresContext, aContent, aAppend);
    }
  }

  return rv;
}


nsresult
nsCSSFrameConstructor::ProcessRestyledFrames(nsStyleChangeList& aChangeList, 
                                             nsPresContext* aPresContext)
{
  PRInt32 count = aChangeList.Count();
  if (!count)
    return NS_OK;

  nsPropertyTable *propTable = aPresContext->PropertyTable();

  // Mark frames so that we skip frames that die along the way, bug 123049.
  // A frame can be in the list multiple times with different hints. Further
  // optmization is possible if nsStyleChangeList::AppendChange could coalesce
  PRInt32 index = count;

  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->SetProperty(changeData->mFrame,
                             nsLayoutAtoms::changeListProperty,
                             nsnull, nsnull, nsnull);
    }
  }

  index = count;
  while (0 <= --index) {
    nsIFrame* frame;
    nsIContent* content;
    nsChangeHint hint;
    aChangeList.ChangeAt(index, frame, content, hint);

    // skip any frame that has been destroyed due to a ripple effect
    if (frame) {
      nsresult res;

      propTable->GetProperty(frame, nsLayoutAtoms::changeListProperty, &res);

      if (NS_PROPTABLE_PROP_NOT_THERE == res)
        continue;
    }

    if (hint & nsChangeHint_ReconstructFrame) {
      RecreateFramesForContent(aPresContext, content);
    } else {
      NS_ASSERTION(frame, "This shouldn't happen");
      if (hint & nsChangeHint_ReflowFrame) {
        StyleChangeReflow(aPresContext, frame, nsnull);
      }
      if (hint & (nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView)) {
        ApplyRenderingChangeToTree(aPresContext, frame, nsnull, hint);
      }
    }
#ifdef DEBUG
    // reget from content since it may have been regenerated...
    if (content) {
      nsIFrame* frame;
      nsIPresShell *shell = aPresContext->PresShell();
      shell->GetPrimaryFrameFor(content, &frame);
      if (frame) {
        shell->FrameManager()->DebugVerifyStyleTree(frame);
      }
    } else {
      NS_WARNING("Unable to test style tree integrity -- no content node");
    }
#endif
  }

  // cleanup references
  index = count;
  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->DeleteProperty(changeData->mFrame,
                                nsLayoutAtoms::changeListProperty);
    }
  }

  aChangeList.Clear();
  return NS_OK;
}

void
nsCSSFrameConstructor::RestyleElement(nsPresContext *aPresContext,
                                      nsIContent     *aContent,
                                      nsIFrame       *aPrimaryFrame,
                                      nsChangeHint   aMinHint)
{
  if (aMinHint & nsChangeHint_ReconstructFrame) {
    RecreateFramesForContent(aPresContext, aContent);
  } else if (aPrimaryFrame) {
    nsStyleChangeList changeList;
    if (aMinHint) {
      changeList.AppendChange(aPrimaryFrame, aContent, aMinHint);
    }
    nsChangeHint frameChange = aPresContext->GetPresShell()->FrameManager()->
      ComputeStyleChangeFor(aPrimaryFrame, &changeList, aMinHint);

    if (frameChange & nsChangeHint_ReconstructFrame) {
      RecreateFramesForContent(aPresContext, aContent);
      changeList.Clear();
    } else {
      ProcessRestyledFrames(changeList, aPresContext);
    }
  } else {
    // no frames, reconstruct for content
    MaybeRecreateFramesForContent(aPresContext, aContent);
  }
}

void
nsCSSFrameConstructor::RestyleLaterSiblings(nsPresContext *aPresContext,
                                            nsIContent *aContent)
{
  nsIContent *parent = aContent->GetParent();
  if (!parent)
    return; // root element has no later siblings

  nsIPresShell *shell = aPresContext->GetPresShell();

  for (PRInt32 index = parent->IndexOf(aContent) + 1,
               index_end = parent->GetChildCount();
       index != index_end; ++index) {
    nsIContent *child = parent->GetChildAt(index);
    if (!child->IsContentOfType(nsIContent::eELEMENT))
      continue;

    nsIFrame* primaryFrame = nsnull;
    shell->GetPrimaryFrameFor(child, &primaryFrame);
    RestyleElement(aPresContext, child, primaryFrame, NS_STYLE_HINT_NONE);
  }
}

nsresult
nsCSSFrameConstructor::ContentStatesChanged(nsPresContext* aPresContext, 
                                            nsIContent* aContent1,
                                            nsIContent* aContent2,
                                            PRInt32 aStateMask) 
{
  DoContentStateChanged(aPresContext, aContent1, aStateMask);
  DoContentStateChanged(aPresContext, aContent2, aStateMask);
  return NS_OK;
}

void
nsCSSFrameConstructor::DoContentStateChanged(nsPresContext* aPresContext, 
                                             nsIContent* aContent,
                                             PRInt32 aStateMask) 
{
  nsIPresShell *shell = aPresContext->GetPresShell();

  NS_ASSERTION(shell, "couldn't get pres shell");
  if (shell) {
    nsStyleSet *styleSet = shell->StyleSet();
    NS_ASSERTION(styleSet, "couldn't get style set");

    if (aContent) {
      nsIFrame* primaryFrame = nsnull;
      shell->GetPrimaryFrameFor(aContent, &primaryFrame);

      if (primaryFrame) {
        PRUint8 app = primaryFrame->GetStyleDisplay()->mAppearance;
        if (app) {
          nsITheme *theme = aPresContext->GetTheme();
          if (theme && theme->ThemeSupportsWidget(aPresContext, primaryFrame, app)) {
            PRBool repaint = PR_FALSE;
            theme->WidgetStateChanged(primaryFrame, app, nsnull, &repaint);
            if (repaint) {
              ApplyRenderingChangeToTree(aPresContext, primaryFrame, nsnull, nsChangeHint_RepaintFrame);
            }
          }
        }
      }

      nsReStyleHint rshint = 
        styleSet->HasStateDependentStyle(aPresContext, aContent, aStateMask);
      
      PostRestyleEvent(aContent, rshint, NS_STYLE_HINT_NONE);
    }
  }
}

nsresult
nsCSSFrameConstructor::AttributeChanged(nsPresContext* aPresContext,
                                        nsIContent* aContent,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aModType)
{
  nsresult  result = NS_OK;

  // Hold onto the PresShell to prevent ourselves from being destroyed.
  nsCOMPtr<nsIPresShell> shell = aPresContext->PresShell();

  // Get the frame associated with the content which is the highest in the frame tree
  nsIFrame* primaryFrame;
  shell->GetPrimaryFrameFor(aContent, &primaryFrame); 

#if 0
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("HTMLStyleSheet::AttributeChanged: content=%p[%s] frame=%p",
      aContent, ContentTag(aContent, 0), frame));
#endif

  // the style tag has its own interpretation based on aHint 
  nsChangeHint hint = NS_STYLE_HINT_NONE;
  nsCOMPtr<nsIStyledContent> styledContent = do_QueryInterface(aContent);
  if (styledContent) { 
    // Get style hint from HTML content object. 
    styledContent->GetAttributeChangeHint(aAttribute, aModType, hint);
  } 

  PRBool reframe = (hint & nsChangeHint_ReconstructFrame) != 0;

#ifdef MOZ_XUL
  // The following listbox widget trap prevents offscreen listbox widget
  // content from being removed and re-inserted (which is what would
  // happen otherwise).
  if (!primaryFrame && !reframe) {
    PRInt32 namespaceID;
    nsCOMPtr<nsIAtom> tag;
    mDocument->GetBindingManager()->ResolveTag(aContent, &namespaceID,
                                               getter_AddRefs(tag));

    if (namespaceID == kNameSpaceID_XUL &&
        (tag == nsXULAtoms::listitem ||
         tag == nsXULAtoms::listcell))
      return NS_OK;
  }

  if (aAttribute == nsXULAtoms::tooltiptext ||
      aAttribute == nsXULAtoms::tooltip) 
  {
    nsIFrame* rootFrame = shell->FrameManager()->GetRootFrame();
    if (rootFrame)
      rootFrame = rootFrame->GetFirstChild(nsnull);
    nsCOMPtr<nsIRootBox> rootBox(do_QueryInterface(rootFrame));
    if (rootBox) {
      if (aModType == nsIDOMMutationEvent::REMOVAL)
        rootBox->RemoveTooltipSupport(aContent);
      if (aModType == nsIDOMMutationEvent::ADDITION)
        rootBox->AddTooltipSupport(aContent);
    }
  }

#endif // MOZ_XUL

  // See if we have appearance information for a theme.
  if (primaryFrame) {
    const nsStyleDisplay* disp = primaryFrame->GetStyleDisplay();
    if (disp->mAppearance) {
      nsITheme *theme = aPresContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aPresContext, primaryFrame, disp->mAppearance)) {
        PRBool repaint = PR_FALSE;
        theme->WidgetStateChanged(primaryFrame, disp->mAppearance, aAttribute, &repaint);
        if (repaint)
          ApplyRenderingChangeToTree(aPresContext, primaryFrame, nsnull, nsChangeHint_RepaintFrame);
      }
    }
  }

  nsFrameManager *frameManager = shell->FrameManager();
  nsReStyleHint rshint = frameManager->HasAttributeDependentStyle(aContent,
                                                                  aAttribute,
                                                                  aModType);

  
  // let the frame deal with it now, so we don't have to deal later
  if (primaryFrame) {
    result = primaryFrame->AttributeChanged(aPresContext, aContent,
                                            aNameSpaceID, aAttribute,
                                            aModType);
    // XXXwaterson should probably check for special IB siblings
    // here, and propagate the AttributeChanged notification to
    // them, as well. Currently, inline frames don't do anything on
    // this notification, so it's not that big a deal.
  }

  PostRestyleEvent(aContent, rshint, hint);

  return result;
}

void
nsCSSFrameConstructor::EndUpdate()
{
  if (--mUpdateCount == 0) {
    if (mQuotesDirty) {
      mQuoteList.RecalcAll();
      mQuotesDirty = PR_FALSE;
    }
  }
}

void
nsCSSFrameConstructor::WillDestroyFrameTree()
{
  // Prevent frame tree destruction from being O(N^2)
  mQuoteList.Clear();

  // Cancel all pending reresolves
  mRestyleEventQueue = nsnull;
  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQueue));
  eventQueue->RevokeEvents(this);
}

//STATIC
void nsCSSFrameConstructor::GetAlternateTextFor(nsIContent* aContent,
                                                nsIAtom*    aTag,  // content object's tag
                                                nsString&   aAltText)
{
  nsresult  rv;

  // The "alt" attribute specifies alternate text that is rendered
  // when the image can not be displayed
  rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::alt, aAltText);

  // If there's no "alt" attribute, and aContent is an input    
  // element, then use the value of the "value" attribute
  if ((NS_CONTENT_ATTR_NOT_THERE == rv) && (nsHTMLAtoms::input == aTag)) {
    rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value,
                           aAltText);

    // If there's no "value" attribute either, then use the localized string 
    // for "Submit" as the alternate text.
    if (NS_CONTENT_ATTR_NOT_THERE == rv) {
      nsFormControlHelper::GetLocalizedString(nsFormControlHelper::GetHTMLPropertiesFileName(),
                                              NS_LITERAL_STRING("Submit").get(), aAltText);      
    }
  }
}

// Construct an alternate frame to use when the image can't be rendered
nsresult
nsCSSFrameConstructor::ConstructAlternateFrame(nsIPresShell*    aPresShell, 
                                               nsPresContext*  aPresContext,
                                               nsIContent*      aContent,
                                               nsStyleContext*  aStyleContext,
                                               nsIFrame*        aGeometricParent,
                                               nsIFrame*        aContentParent,
                                               nsIFrame*&       aFrame)
{
  nsresult rv;
  nsAutoString  altText;

  // Initialize OUT parameter
  aFrame = nsnull;

  // Get the alternate text to use
  GetAlternateTextFor(aContent, aContent->Tag(), altText);

  // Create a text content element for the alternate text
  nsCOMPtr<nsITextContent> altTextContent;
  rv = NS_NewTextNode(getter_AddRefs(altTextContent));
  if (NS_FAILED(rv))
    return rv;

  // Set the content's text
  altTextContent->SetText(altText, PR_TRUE);
  
  // Set aContent as the parent content.
  altTextContent->SetParent(aContent);
  altTextContent->SetDocument(mDocument, PR_TRUE, PR_TRUE);

  // Create either an inline frame, block frame, or area frame
  nsIFrame* containerFrame;
  PRBool    isOutOfFlow = PR_FALSE;
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  
  if (display->IsAbsolutelyPositioned()) {
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
  containerFrame->Init(aPresContext, aContent, aGeometricParent, aStyleContext, nsnull);
  nsHTMLContainerFrame::CreateViewForFrame(containerFrame, aContentParent,
                                           PR_FALSE);

  // If the frame is out-of-flow, then mark it as such
  if (isOutOfFlow) {
    containerFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);
  }

  // Create a text frame to display the alt-text. It gets a pseudo-element
  // style context
  nsIFrame*        textFrame;
  NS_NewTextFrame(aPresShell, &textFrame);

  nsRefPtr<nsStyleContext> textStyleContext;
  textStyleContext = aPresShell->StyleSet()->
    ResolveStyleForNonElement(aStyleContext);

  textFrame->Init(aPresContext, altTextContent, containerFrame,
                  textStyleContext, nsnull);
  containerFrame->SetInitialChildList(aPresContext, nsnull, textFrame);

  // Return the container frame
  aFrame = containerFrame;

  return NS_OK;
}

#ifdef NS_DEBUG
static PRBool
IsPlaceholderFrame(nsIFrame* aFrame)
{
  return aFrame->GetType() == nsLayoutAtoms::placeholderFrame;
}
#endif


static PRBool
HasDisplayableChildren(nsPresContext* aPresContext, nsIFrame* aContainerFrame)
{
  // Returns 'true' if there are frames within aContainerFrame that
  // could be displayed in the frame list.
  NS_PRECONDITION(aContainerFrame != nsnull, "null ptr");
  if (! aContainerFrame)
    return PR_FALSE;

  nsIFrame* frame = aContainerFrame->GetFirstChild(nsnull);

  while (frame) {
    // If it's not a text frame, then assume that it's displayable.
    if (frame->GetType() != nsLayoutAtoms::textFrame)
      return PR_TRUE;

    // If not only whitespace, then we have displayable content here.
    if (! IsOnlyWhitespace(frame->GetContent()))
      return PR_TRUE;
    
    // Otherwise, on to the next frame...
    frame = frame->GetNextSibling();
  }

  // If we get here, then we've iterated through all the child frames,
  // and every one is a text frame containing whitespace. (Or, there
  // weren't any frames at all!) There is nothing to diplay.
  return PR_FALSE;
}



nsresult
nsCSSFrameConstructor::CantRenderReplacedElement(nsIPresShell* aPresShell, 
                                                 nsPresContext* aPresContext,
                                                 nsIFrame*       aFrame)
{
  nsresult                  rv = NS_OK;

  // Get parent frame and style context
  nsIFrame*                 parentFrame = aFrame->GetParent();
  nsStyleContext* styleContext = aFrame->GetStyleContext();

  // Get aFrame's content object and the tag name
  PRInt32                   nameSpaceID;
  nsIAtom*                  tag;

  nsIContent* content = aFrame->GetContent();
  NS_ASSERTION(content, "null content object");
  content->GetNameSpaceID(&nameSpaceID);
  tag = content->Tag();

  // Get the child list name that the frame is contained in
  nsCOMPtr<nsIAtom>  listName;
  GetChildListNameFor(aPresContext, parentFrame, aFrame, getter_AddRefs(listName));

  // If the frame is out of the flow, then it has a placeholder frame.
  nsPlaceholderFrame* placeholderFrame = nsnull;
  nsIPresShell *presShell = aPresContext->PresShell();
  if (listName) {
    presShell->GetPlaceholderFrameFor(aFrame, (nsIFrame**) &placeholderFrame);
  }

  // Get the previous sibling frame
  nsFrameList frameList(parentFrame->GetFirstChild(listName));
  
  // See whether it's an IMG or an INPUT element (for image buttons)
  // or if it is an applet with no displayable children
  // XXX need to check nameSpaceID in these spots
  if (nsHTMLAtoms::img == tag || nsHTMLAtoms::input == tag ||
      (nsHTMLAtoms::applet == tag && !HasDisplayableChildren(aPresContext, aFrame))) {
    // Try and construct an alternate frame to use when the
    // image can't be rendered
    nsIFrame* newFrame;
    rv = ConstructAlternateFrame(aPresShell, aPresContext, content, styleContext,
                                 parentFrame, nsnull, newFrame);

    if (NS_SUCCEEDED(rv)) {
      nsFrameManager *frameManager = presShell->FrameManager();

      // Replace the old frame with the new frame

      DeletingFrameSubtree(aPresContext, presShell, frameManager, aFrame);

      // Reset the primary frame mapping
      frameManager->SetPrimaryFrameFor(content, newFrame);

      // Replace the old frame with the new frame
      // XXXbz If this fails, we leak the content node newFrame points to!
      frameManager->ReplaceFrame(parentFrame, listName, aFrame, newFrame);

      // Now that we've replaced the primary frame, if there's a placeholder
      // frame then complete the transition from image frame to new frame
      if (placeholderFrame) {
        // Remove the association between the old frame and its placeholder
        frameManager->UnregisterPlaceholderFrame(placeholderFrame);
        
        // Placeholder frames have a pointer back to the out-of-flow frame.
        // Make sure that's correct, too.
        placeholderFrame->SetOutOfFlowFrame(newFrame);

        // Reuse the existing placeholder frame, and add an association to the
        // new frame
        frameManager->RegisterPlaceholderFrame(placeholderFrame);

        // XXX Work around a bug in the block code where the float won't get
        // reflowed unless the line containing the placeholder frame is reflowed...
        placeholderFrame->GetParent()->
          ReflowDirtyChild(aPresShell, placeholderFrame);
      }
    }

  } else if ((nsHTMLAtoms::object == tag) ||
             (nsHTMLAtoms::embed == tag) ||
             (nsHTMLAtoms::applet == tag)) {
    // It's an OBJECT, EMBED, or APPLET, so we should display the contents
    // instead
    nsIFrame* absoluteContainingBlock;
    nsIFrame* floatContainingBlock;
    nsIFrame* inFlowParent = parentFrame;

    // If the OBJECT frame is out-of-flow, then get the placeholder frame's
    // parent and use that when determining the absolute containing block and
    // float containing block
    if (placeholderFrame) {
      inFlowParent = placeholderFrame->GetParent();
    }

    absoluteContainingBlock = GetAbsoluteContainingBlock(aPresContext, inFlowParent);
    floatContainingBlock = GetFloatContainingBlock(aPresContext, inFlowParent);

#ifdef NS_DEBUG
    // Verify that we calculated the same containing block
    if (listName == nsLayoutAtoms::absoluteList) {
      NS_ASSERTION(absoluteContainingBlock == parentFrame,
                   "wrong absolute containing block");
    } else if (listName == nsLayoutAtoms::floatList) {
      NS_ASSERTION(floatContainingBlock == parentFrame,
                   "wrong float containing block");
    }
#endif

    // Now initialize the frame construction state
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                  absoluteContainingBlock,
                                  floatContainingBlock);
    nsFrameItems            frameItems;
    const nsStyleDisplay* display = styleContext->GetStyleDisplay();

    // Create a new frame based on the display type.
    // Note: if the old frame was out-of-flow, then so will the new frame
    // and we'll get a new placeholder frame
    rv = ConstructFrameByDisplayType(aPresShell, aPresContext, state, display,
                                     content, nameSpaceID, tag,
                                     inFlowParent, styleContext, frameItems);

    if (NS_FAILED(rv)) return rv;

    nsIFrame* newFrame = frameItems.childList;


    if (NS_SUCCEEDED(rv)) {
      if (placeholderFrame) {
        // Remove the association between the old frame and its placeholder
        // Note: ConstructFrameByDisplayType() will already have added an
        // association for the new placeholder frame
        state.mFrameManager->UnregisterPlaceholderFrame(placeholderFrame);

        // Verify that the new frame is also a placeholder frame
        NS_ASSERTION(IsPlaceholderFrame(newFrame), "unexpected frame type");

        // Replace the old placeholder frame with the new placeholder frame
        state.mFrameManager->ReplaceFrame(inFlowParent, nsnull,
                                          placeholderFrame, newFrame);
      }

      // Replace the primary frame
      if (listName == nsnull) {
        if (IsInlineFrame(parentFrame) && !AreAllKidsInline(newFrame)) {
          // We're in the uncomfortable position of being an inline
          // that now contains a block. As in ConstructInline(), break
          // the newly constructed frames into three lists: the inline
          // frames before the first block frame (list1), the inline
          // frames after the last block frame (list3), and all the
          // frames between the first and last block frames (list2).
          nsIFrame* list1 = newFrame;

          nsIFrame* prevToFirstBlock;
          nsIFrame* list2 = FindFirstBlock(aPresContext, list1, &prevToFirstBlock);

          if (prevToFirstBlock)
            prevToFirstBlock->SetNextSibling(nsnull);
          else list1 = nsnull;

          nsIFrame* afterFirstBlock = list2->GetNextSibling();

          nsIFrame* lastBlock = FindLastBlock(aPresContext, afterFirstBlock);
          if (! lastBlock)
            lastBlock = list2;

          nsIFrame* list3 = lastBlock->GetNextSibling();
          lastBlock->SetNextSibling(nsnull);

          // Create "special" inline-block linkage between the frames
          // XXXldb Do we really need to do this?  It doesn't seem
          // consistent with the use in ConstructInline.
          SetFrameIsSpecial(state.mFrameManager, list1, list2);
          SetFrameIsSpecial(state.mFrameManager, list2, list3);
          SetFrameIsSpecial(state.mFrameManager, list3, nsnull);

          // Recursively split inlines back up to the first containing
          // block frame.
          SplitToContainingBlock(aPresContext, state, parentFrame, list1, list2, list3, PR_FALSE);
        }
      } else if (listName == nsLayoutAtoms::absoluteList) {
        newFrame = state.mAbsoluteItems.childList;
        state.mAbsoluteItems.childList = nsnull;
      } else if (listName == nsLayoutAtoms::fixedList) {
        newFrame = state.mFixedItems.childList;
        state.mFixedItems.childList = nsnull;
      } else if (listName == nsLayoutAtoms::floatList) {
        newFrame = state.mFloatedItems.childList;
        state.mFloatedItems.childList = nsnull;
      }
      DeletingFrameSubtree(aPresContext, presShell,
                           state.mFrameManager, aFrame);
      state.mFrameManager->ReplaceFrame(parentFrame, listName, aFrame,
                                        newFrame);

      // Reset the primary frame mapping. Don't assume that
      // ConstructFrameByDisplayType() has done this
      state.mFrameManager->SetPrimaryFrameFor(content, newFrame);
      
      InsertOutOfFlowFrames(state, aPresContext);
    }
  } else if (nsHTMLAtoms::input == tag) {
    // XXX image INPUT elements are also image frames, but don't throw away the
    // image frame, because the frame class has extra logic that is specific to
    // INPUT elements

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected tag");
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingOuterTableFrame(nsIPresShell*    aPresShell,
                                                       nsPresContext*  aPresContext,
                                                       nsIFrame*        aFrame,
                                                       nsIFrame*        aParentFrame,
                                                       nsIContent*      aContent,
                                                       nsStyleContext*  aStyleContext,
                                                       nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;

  rv = NS_NewTableOuterFrame(aPresShell, &newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

    // Create a continuing inner table frame, and if there's a caption then
    // replicate the caption
    nsFrameItems  newChildFrames;

    nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);
    while (childFrame) {
      // See if it's the inner table frame
      if (nsLayoutAtoms::tableFrame == childFrame->GetType()) {
        nsIFrame* continuingTableFrame;

        // It's the inner table frame, so create a continuing frame
        CreateContinuingFrame(aPresContext, childFrame, newFrame, &continuingTableFrame);
        newChildFrames.AddChild(continuingTableFrame);
      } else {
        // XXX remove this code and the above checks. We don't want to replicate 
        // the caption (that is what the thead is for). This code is not executed 
        // anyway, because the caption was put in a different child list.
        nsStyleContext*       captionStyle = childFrame->GetStyleContext();
        nsIContent*           caption = childFrame->GetContent();
        NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_CAPTION ==
                       captionStyle->GetStyleDisplay()->mDisplay,
                     "expected caption");

        // Replicate the caption frame
        // XXX We have to do it this way instead of calling ConstructFrameByDisplayType(),
        // because of a bug in the way ConstructTableFrame() handles the initial child
        // list...
        nsIFrame*               captionFrame;
        nsFrameItems            childItems;
        NS_NewTableCaptionFrame(aPresShell, &captionFrame);
        nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                      GetAbsoluteContainingBlock(aPresContext, newFrame),
                                      captionFrame);
        captionFrame->Init(aPresContext, caption, newFrame, captionStyle, nsnull);
        ProcessChildren(aPresShell, aPresContext, state, caption, captionFrame,
                        PR_TRUE, childItems, PR_TRUE);
        captionFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
        // XXX Deal with absolute and fixed frames...
        if (state.mFloatedItems.childList) {
          captionFrame->SetInitialChildList(aPresContext,
                                            nsLayoutAtoms::floatList,
                                            state.mFloatedItems.childList);
        }
        newChildFrames.AddChild(captionFrame);
      }
      childFrame = childFrame->GetNextSibling();
    }

    // Set the outer table's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, newChildFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingTableFrame(nsIPresShell* aPresShell, 
                                                  nsPresContext*  aPresContext,
                                                  nsIFrame*        aFrame,
                                                  nsIFrame*        aParentFrame,
                                                  nsIContent*      aContent,
                                                  nsStyleContext*  aStyleContext,
                                                  nsIFrame**       aContinuingFrame)
{
  nsIFrame* newFrame;
  nsresult  rv;
    
  rv = NS_NewTableFrame(aPresShell, &newFrame);
  if (NS_SUCCEEDED(rv)) {
    newFrame->Init(aPresContext, aContent, aParentFrame, aStyleContext, aFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

    // Replicate any header/footer frames
    nsFrameItems  childFrames;
    nsIFrame* rowGroupFrame = aFrame->GetFirstChild(nsnull);
    while (rowGroupFrame) {
      // See if it's a header/footer
      nsStyleContext*       rowGroupStyle = rowGroupFrame->GetStyleContext();
      const nsStyleDisplay* display = rowGroupStyle->GetStyleDisplay();

      if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay)) {
        // If the row group has was continued, then don't replicate it
        nsIFrame* rgNextInFlow = rowGroupFrame->GetNextInFlow();
        if (rgNextInFlow) {
          ((nsTableRowGroupFrame*)rowGroupFrame)->SetRepeatable(PR_FALSE);
        }
        // Replicate the header/footer frame if it is not too tall
        else if (((nsTableRowGroupFrame*)rowGroupFrame)->IsRepeatable()) {        
          nsIFrame*               headerFooterFrame;
          nsFrameItems            childItems;
          nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                        GetAbsoluteContainingBlock(aPresContext, newFrame),
                                        nsnull);

          NS_NewTableRowGroupFrame(aPresShell, &headerFooterFrame);
          nsIContent* headerFooter = rowGroupFrame->GetContent();
          headerFooterFrame->Init(aPresContext, headerFooter, newFrame,
                                  rowGroupStyle, nsnull);
          nsTableCreator tableCreator(aPresShell); 
          ProcessChildren(aPresShell, aPresContext, state, headerFooter, headerFooterFrame,
                          PR_FALSE, childItems, PR_FALSE, &tableCreator);
          NS_ASSERTION(!state.mFloatedItems.childList, "unexpected floated element");
          headerFooterFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);
          ((nsTableRowGroupFrame*)headerFooterFrame)->SetRepeatable(PR_TRUE);

          // Table specific initialization
          ((nsTableRowGroupFrame*)headerFooterFrame)->InitRepeatedFrame
            (aPresContext, (nsTableRowGroupFrame*)rowGroupFrame);

          // XXX Deal with absolute and fixed frames...
          childFrames.AddChild(headerFooterFrame);
        }
      }

      // Get the next row group frame
      rowGroupFrame = rowGroupFrame->GetNextSibling();
    }
    
    // Set the table frame's initial child list
    newFrame->SetInitialChildList(aPresContext, nsnull, childFrames.childList);
  }

  *aContinuingFrame = newFrame;
  return rv;
}

nsresult
nsCSSFrameConstructor::CreateContinuingFrame(nsPresContext* aPresContext,
                                             nsIFrame*       aFrame,
                                             nsIFrame*       aParentFrame,
                                             nsIFrame**      aContinuingFrame)
{
  nsIPresShell*              shell = aPresContext->PresShell();
  nsStyleContext*            styleContext = aFrame->GetStyleContext();
  nsIFrame*                  newFrame = nsnull;
  nsresult                   rv = NS_OK;
  nsIFrame*                  nextInFlow = aFrame->GetNextInFlow();

  // Use the frame type to determine what type of frame to create
  nsIAtom* frameType = aFrame->GetType();
  nsIContent* content = aFrame->GetContent();

  if (nsLayoutAtoms::textFrame == frameType) {
    rv = NS_NewContinuingTextFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
    
  } else if (nsLayoutAtoms::inlineFrame == frameType) {
    rv = NS_NewInlineFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::blockFrame == frameType) {
    rv = NS_NewBlockFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::areaFrame == frameType) {
    rv = NS_NewAreaFrame(shell, &newFrame, 0);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext,
                     aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::positionedInlineFrame == frameType) {
    rv = NS_NewPositionedInlineFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsLayoutAtoms::pageFrame == frameType) {
    nsIFrame* pageContentFrame;
    rv = ConstructPageFrame(shell, aPresContext, aParentFrame, aFrame,
                            newFrame, pageContentFrame);
  } else if (nsLayoutAtoms::tableOuterFrame == frameType) {
    rv = CreateContinuingOuterTableFrame(shell, aPresContext, aFrame, aParentFrame,
                                         content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableFrame == frameType) {
    rv = CreateContinuingTableFrame(shell, aPresContext, aFrame, aParentFrame,
                                    content, styleContext, &newFrame);

  } else if (nsLayoutAtoms::tableRowGroupFrame == frameType) {
    rv = NS_NewTableRowGroupFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsLayoutAtoms::tableRowFrame == frameType) {
    rv = NS_NewTableRowFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing frame for each table cell frame
      nsFrameItems  newChildList;
      nsIFrame* cellFrame = aFrame->GetFirstChild(nsnull);
      while (cellFrame) {
        // See if it's a table cell frame
        if (IS_TABLE_CELL(cellFrame->GetType())) {
          nsIFrame* continuingCellFrame;

          CreateContinuingFrame(aPresContext, cellFrame, newFrame, &continuingCellFrame);
          newChildList.AddChild(continuingCellFrame);
        }
        cellFrame = cellFrame->GetNextSibling();
      }
      
      // Set the table cell's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, newChildList.childList);
    }

  } else if (IS_TABLE_CELL(frameType)) {
    rv = NS_NewTableCellFrame(shell, IsBorderCollapse(aParentFrame), &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      nsIFrame* continuingAreaFrame;
      nsIFrame* areaFrame = aFrame->GetFirstChild(nsnull);
      CreateContinuingFrame(aPresContext, areaFrame, newFrame, &continuingAreaFrame);

      // Set the table cell's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, continuingAreaFrame);
    }
  
  } else if (nsLayoutAtoms::lineFrame == frameType) {
    rv = NS_NewFirstLineFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsLayoutAtoms::letterFrame == frameType) {
    rv = NS_NewFirstLetterFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsLayoutAtoms::imageFrame == frameType) {
    rv = NS_NewImageFrame(shell, &newFrame);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
    }
  } else if (nsLayoutAtoms::placeholderFrame == frameType) {
    // create a continuing out of flow frame
    nsIFrame* oofFrame = ((nsPlaceholderFrame*)aFrame)->GetOutOfFlowFrame();
    nsIFrame* oofContFrame;
    CreateContinuingFrame(aPresContext, oofFrame, aParentFrame, &oofContFrame);
    if (!oofContFrame) 
      return NS_ERROR_NULL_POINTER;
    // create a continuing placeholder frame
    CreatePlaceholderFrameFor(shell, aPresContext,
                              shell->FrameManager(), content, 
                              oofContFrame, styleContext, aParentFrame, &newFrame);
    if (!newFrame) 
      return NS_ERROR_NULL_POINTER;
    newFrame->Init(aPresContext, content, aParentFrame, styleContext, aFrame);
  } else if (nsLayoutAtoms::fieldSetFrame == frameType) {
    rv = NS_NewFieldSetFrame(aPresContext->PresShell(), &newFrame,
                             NS_BLOCK_SPACE_MGR);
    if (NS_SUCCEEDED(rv)) {
      newFrame->Init(aPresContext, content, aParentFrame, styleContext,
                     aFrame);

      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      // XXXbz we really shouldn't have to do this by hand!
      nsIFrame* continuingAreaFrame;
      nsIFrame* areaFrame = aFrame->GetFirstChild(nsnull);
      CreateContinuingFrame(aPresContext, areaFrame, newFrame, &continuingAreaFrame);

      // Set the fieldset's initial child list
      newFrame->SetInitialChildList(aPresContext, nsnull, continuingAreaFrame);
    }
  } else {
    NS_ASSERTION(PR_FALSE, "unexpected frame type");
    rv = NS_ERROR_UNEXPECTED;
  }

  *aContinuingFrame = newFrame;

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Now deal with fixed-pos things....  They should appear on all pages, and
  // the placeholders must be kids of a block, so we want to move over the
  // placeholders when processing the child of the pageContentFrame.
  if (!aParentFrame) {
    return NS_OK;
  }

  if (aParentFrame->GetType() != nsLayoutAtoms::pageContentFrame) {
    if (nextInFlow) {
      nextInFlow->SetPrevInFlow(newFrame);
      newFrame->SetNextInFlow(nextInFlow);
    }
    return NS_OK;
  }

  // Our parent is a page content frame.  Look up its page frame and
  // see whether it has a prev-in-flow.
  nsIFrame* pageFrame = aParentFrame->GetParent();
  if (!pageFrame) {
    NS_ERROR("pageContentFrame does not have parent!");
    return NS_ERROR_UNEXPECTED;
  }

  nsIFrame* prevPage = pageFrame->GetPrevInFlow();
  if (!prevPage) {
    return NS_OK;
  }

  // OK.  now we need to do this fixed-pos game.
  // Get prevPage's page content frame
  nsIFrame* prevPageContentFrame = prevPage->GetFirstChild(nsnull);

  if (!prevPageContentFrame) {
    return NS_ERROR_UNEXPECTED;
  }
  
  nsFrameItems fixedPlaceholders;
  nsIFrame* firstFixed = prevPageContentFrame->GetFirstChild(nsLayoutAtoms::fixedList);
  if (!firstFixed) {
    return NS_OK;
  }

  nsFrameConstructorState state(aPresContext, aParentFrame,
                                mInitialContainingBlock,
                                mInitialContainingBlock);
  
  // Iterate the fixed frames and replicate each
  for (nsIFrame* fixed = firstFixed; fixed; fixed = fixed->GetNextSibling()) {
    rv = ConstructFrame(shell, aPresContext, state, fixed->GetContent(), 
                        newFrame, fixedPlaceholders);
    if (NS_FAILED(rv))
      return rv;
  }

  // Add the placeholders to our primary child list.
  // XXXbz this is a little screwed up, since the fixed frames will have the
  // wrong parent block and hence auto-positioning will be broken.  Oh, well.
  newFrame->SetInitialChildList(aPresContext, nsnull, fixedPlaceholders.childList);
  // Add the fixed frames to the fixed child list of the page content frame
  aParentFrame->SetInitialChildList(aPresContext,
                                    nsLayoutAtoms::fixedList, 
                                    state.mFixedItems.childList);
  return NS_OK;
}

// Helper function that searches the immediate child frames 
// (and their children if the frames are "special")
// for a frame that maps the specified content object
nsIFrame*
nsCSSFrameConstructor::FindFrameWithContent(nsPresContext*  aPresContext,
                                            nsFrameManager*  aFrameManager,
                                            nsIFrame*        aParentFrame,
                                            nsIContent*      aParentContent,
                                            nsIContent*      aContent,
                                            nsFindFrameHint* aHint)
{
  
#ifdef NOISY_FINDFRAME
  FFWC_totalCount++;
  printf("looking for content=%p, given aParentFrame %p parentContent %p, hint is %s\n", 
         aContent, aParentFrame, aParentContent, aHint ? "set" : "NULL");
#endif

  NS_ENSURE_TRUE(aParentFrame != nsnull, nsnull);

  do {
    // Search for the frame in each child list that aParentFrame supports
    nsIAtom* listName = nsnull;
    PRInt32 listIndex = 0;
    do {
#ifdef NOISY_FINDFRAME
      FFWC_doLoop++;
#endif
      nsIFrame* kidFrame=nsnull;
      // if we were given an hint, try to use it here to find a good
      // previous frame to start our search (|kidFrame|).
      if (aHint) {
#ifdef NOISY_FINDFRAME
        printf("  hint frame is %p\n", aHint->mPrimaryFrameForPrevSibling);
#endif
        // start with the primary frame for aContent's previous sibling
        kidFrame = aHint->mPrimaryFrameForPrevSibling;
        // But if it's out of flow, start from its placeholder.
        if (kidFrame && (kidFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
          kidFrame = aFrameManager->GetPlaceholderFrameFor(kidFrame);
        }

        if (kidFrame) {
          // then use the next sibling frame as our starting point
          kidFrame = kidFrame->GetNextSibling();
          if (!kidFrame)
          { // the hint frame had no next frame.  try the next-in-flow fo the parent of the hint frame
            // if there is one
            nsIFrame *parentFrame = aHint->mPrimaryFrameForPrevSibling->GetParent();
            if (parentFrame) {
              parentFrame = GetNifOrSpecialSibling(aFrameManager, parentFrame);
            }
            if (parentFrame) 
            { // if we found the next-in-flow for the parent of the hint frame, start with it's first child
              kidFrame = parentFrame->GetFirstChild(listName);
              // Leave |aParentFrame| as-is, since the only time we'll
              // reuse it is if the hint fails.
              NS_ASSERTION(!kidFrame || parentFrame->GetContent() == aParentContent,
                           "next-in-flow has different content");
            }
          }
#ifdef NOISY_FINDFRAME
          printf("  hint gives us kidFrame=%p with parent frame %p content %p\n", 
                  kidFrame, aParentFrame, aParentContent);
#endif
        }
      }
      if (!kidFrame) {  // we didn't have enough info to prune, start searching from the beginning
        kidFrame = aParentFrame->GetFirstChild(listName);
      }
      while (kidFrame) {
        // See if the child frame points to the content object we're
        // looking for
        nsIContent* kidContent = kidFrame->GetContent();
        if (kidContent == aContent) {

          // We found a match. See if it's a placeholder frame
          if (nsLayoutAtoms::placeholderFrame == kidFrame->GetType()) {
            // Ignore the placeholder and return the out-of-flow frame instead
            return ((nsPlaceholderFrame*)kidFrame)->GetOutOfFlowFrame();
          }

          // Return the matching child frame
          return kidFrame;
        }

        // only do this if there is content
        if (kidContent) {
          // We search the immediate children only, but if the child frame has
          // the same content pointer as its parent then we need to search its
          // child frames, too.
          // We also need to search if the child content is anonymous and scoped
          // to the parent content.
          if (aParentContent == kidContent ||
              (aParentContent && (aParentContent == kidContent->GetBindingParent()))) 
          {
#ifdef NOISY_FINDFRAME
            FFWC_recursions++;
            printf("  recursing with new parent set to kidframe=%p, parentContent=%p\n", 
                   kidFrame, aParentContent.get());
#endif
            nsIFrame* matchingFrame =
                FindFrameWithContent(aPresContext, aFrameManager, kidFrame,
                                     aParentContent, aContent, nsnull);

            if (matchingFrame) {
              return matchingFrame;
            }
          }
        }

        // Get the next sibling frame
        kidFrame = kidFrame->GetNextSibling();
#ifdef NOISY_FINDFRAME
        FFWC_doSibling++;
        if (kidFrame) {
          printf("  searching sibling frame %p\n", kidFrame);
        }
#endif
      }

      if (aHint) {
        // If we get here, and we had a hint, then we didn't find a
        // frame. The hint may have been a floated or absolutely
        // positioned frame, in which case we'd be off in the weeds
        // looking through something other than primary frame
        // list. Reboot the search from scratch, without the hint, but
        // using the null child list again.
        aHint = nsnull;
      } else {
        listName = aParentFrame->GetAdditionalChildListName(listIndex++);
      }
    } while(listName);

    // We didn't find a matching frame. If aFrame has a next-in-flow,
    // then continue looking there
    aParentFrame = GetNifOrSpecialSibling(aFrameManager, aParentFrame);
#ifdef NOISY_FINDFRAME
    if (aParentFrame) {
      FFWC_nextInFlows++;
      printf("  searching NIF frame %p\n", aParentFrame);
    }
#endif
  } while (aParentFrame);

  // No matching frame
  return nsnull;
}

// Request to find the primary frame associated with a given content object.
// This is typically called by the pres shell when there is no mapping in
// the pres shell hash table
nsresult
nsCSSFrameConstructor::FindPrimaryFrameFor(nsPresContext*  aPresContext,
                                           nsFrameManager*  aFrameManager,
                                           nsIContent*      aContent,
                                           nsIFrame**       aFrame,
                                           nsFindFrameHint* aHint)
{
  NS_ASSERTION(aPresContext && aFrameManager && aContent && aFrame, "bad arg");

  *aFrame = nsnull;  // initialize OUT parameter 

  // We want to be able to quickly map from a content object to its frame,
  // but we also want to keep the hash table small. Therefore, many frames
  // are not added to the hash table when they're first created:
  // - text frames
  // - inline frames (often things like FONT and B)
  // - BR frames
  // - internal table frames (row-group, row, cell, col-group, col)
  //
  // That means we need to need to search for the frame
  nsIFrame*              parentFrame;   // this pointer is used to iterate across all frames that map to parentContent

  // Get the frame that corresponds to the parent content object.
  // Note that this may recurse indirectly, because the pres shell will
  // call us back if there is no mapping in the hash table
  nsCOMPtr<nsIContent> parentContent = aContent->GetParent(); // Get this once
  if (parentContent) {
    parentFrame = aFrameManager->GetPrimaryFrameFor(parentContent);
    while (parentFrame) {
      // Search the child frames for a match
      *aFrame = FindFrameWithContent(aPresContext, aFrameManager, parentFrame,
                                     parentContent, aContent, aHint);
#ifdef NOISY_FINDFRAME
      printf("FindFrameWithContent returned %p\n", *aFrame);
#endif

#ifdef DEBUG
      // if we're given a hint and we were told to verify, then compare the resulting frame with
      // the frame we get by calling FindFrameWithContent *without* the hint.  
      // Assert if they do not match
      // Note that this makes finding frames *slower* than it was before the fix.
      if (gVerifyFastFindFrame && aHint) 
      {
#ifdef NOISY_FINDFRAME
        printf("VERIFYING...\n");
#endif
        nsIFrame *verifyTestFrame =
            FindFrameWithContent(aPresContext, aFrameManager, parentFrame,
                                 parentContent, aContent, nsnull);
#ifdef NOISY_FINDFRAME
        printf("VERIFY returned %p\n", verifyTestFrame);
#endif
        NS_ASSERTION(verifyTestFrame == *aFrame, "hint shortcut found wrong frame");
      }
#endif
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

  if (aHint && !*aFrame)
  { // if we had a hint, and we didn't get a frame, see if we should try the slow way
    if (aContent->Tag() == nsLayoutAtoms::textTagName) 
    {
#ifdef NOISY_FINDFRAME
      FFWC_slowSearchForText++;
#endif
      // since we're passing in a null hint, we're guaranteed to only recurse once
      return FindPrimaryFrameFor(aPresContext, aFrameManager, aContent, aFrame, nsnull);
    }
  }

#ifdef NOISY_FINDFRAME
  printf("%10s %10s %10s %10s %10s \n", 
         "total", "doLoop", "doSibling", "recur", "nextIF", "slowSearch");
  printf("%10d %10d %10d %10d %10d \n", 
         FFWC_totalCount, FFWC_doLoop, FFWC_doSibling, FFWC_recursions, 
         FFWC_nextInFlows, FFWC_slowSearchForText);
#endif

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::GetInsertionPoint(nsIPresShell* aPresShell,
                                         nsIFrame*     aParentFrame,
                                         nsIContent*   aChildContent,
                                         nsIFrame**    aInsertionPoint,
                                         PRBool*       aMultiple)
{
  // Make the insertion point be the parent frame by default, in case
  // we have to bail early.
  *aInsertionPoint = aParentFrame;

  nsIContent* container = aParentFrame->GetContent();
  if (!container)
    return NS_OK;

  nsIDocument* document = container->GetDocument();
  if (!document)
    return NS_OK;

  nsIBindingManager *bindingManager = document->GetBindingManager();
  if (!bindingManager)
    return NS_OK;

  nsCOMPtr<nsIContent> insertionElement;
  if (aChildContent) {
    // We've got an explicit insertion child. Check to see if it's
    // anonymous.
    if (aChildContent->GetBindingParent() == container) {
      // This child content is anonymous. Don't use the insertion
      // point, since that's only for the explicit kids.
      return NS_OK;
    }

    PRUint32 index;
    bindingManager->GetInsertionPoint(container, aChildContent, getter_AddRefs(insertionElement), &index);
  }
  else {
    PRBool multiple;
    PRUint32 index;
    bindingManager->GetSingleInsertionPoint(container, getter_AddRefs(insertionElement), &index, &multiple);
    if (multiple && aMultiple)
      *aMultiple = multiple; // Record the fact that filters are in use.
  }

  if (insertionElement) {
    nsIFrame* insertionPoint = nsnull;
    aPresShell->GetPrimaryFrameFor(insertionElement, &insertionPoint);
    if (insertionPoint) {
      // If the insertion point is a scrollable, then walk ``through''
      // it to get the scrolled frame.
      nsIScrollableFrame* scroll = nsnull;
      CallQueryInterface(insertionPoint, &scroll);
      if (scroll)
        insertionPoint = scroll->GetScrolledFrame();

      if (insertionPoint != aParentFrame) 
        GetInsertionPoint(aPresShell, insertionPoint, aChildContent, aInsertionPoint, aMultiple);
    }
    else {
      // There was no frame created yet for the insertion point.
      *aInsertionPoint = nsnull;
    }
  }

  return NS_OK;
}

// Capture state for the frame tree rooted at the frame associated with the
// content object, aContent
nsresult
nsCSSFrameConstructor::CaptureStateForFramesOf(nsPresContext* aPresContext,
                                               nsIContent* aContent,
                                               nsILayoutHistoryState* aHistoryState)
{
  nsresult rv = NS_OK;

  nsIPresShell *presShell = aPresContext->GetPresShell();
  if (presShell) {
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
nsCSSFrameConstructor::CaptureStateFor(nsPresContext* aPresContext,
                                       nsIFrame* aFrame,
                                       nsILayoutHistoryState* aHistoryState)
{
  if (aFrame && aPresContext && aHistoryState) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
      presShell->FrameManager()->CaptureFrameState(aFrame, aHistoryState);
    }
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::MaybeRecreateFramesForContent(nsPresContext* aPresContext,
                                                     nsIContent* aContent)
{
  nsresult result = NS_OK;
  nsIPresShell *shell = aPresContext->PresShell();
  nsFrameManager *frameManager = shell->FrameManager();

  nsStyleContext *oldContext = frameManager->GetUndisplayedContent(aContent);
  if (oldContext) {
    // The parent has a frame, so try resolving a new context.
    nsRefPtr<nsStyleContext> newContext = shell->StyleSet()->
      ResolveStyleFor(aContent, oldContext->GetParent());

    frameManager->ChangeUndisplayedContent(aContent, newContext);
    if (newContext->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_NONE) {
      result = RecreateFramesForContent(aPresContext, aContent);
    }
  }
  return result;
}

nsresult
nsCSSFrameConstructor::RecreateFramesForContent(nsPresContext* aPresContext,
                                                nsIContent* aContent)
{
  // If there is no document, we don't want to recreate frames for it.  (You
  // shouldn't generally be giving this method content without a document
  // anyway).
  // Rebuilding the frame tree can have bad effects, especially if it's the
  // frame tree for chrome (see bug 157322).
  NS_ENSURE_TRUE(aContent->GetDocument(), NS_ERROR_FAILURE);

  // Is the frame `special'? If so, we need to reframe the containing
  // block *here*, rather than trying to remove and re-insert the
  // content (which would otherwise result in *two* nested reframe
  // containing block from ContentRemoved() and ContentInserted(),
  // below!)
  nsIPresShell *shell = aPresContext->PresShell();

  nsIFrame* frame;
  shell->GetPrimaryFrameFor(aContent, &frame);

  if (frame) {
    // If the background of the frame is painted on one of its ancestors,
    // the frame reconstruct might not invalidate correctly.
    nsIFrame *ancestor = frame;
    const nsStyleBackground *bg;
    PRBool isCanvas;
    while (!nsCSSRendering::FindBackground(aPresContext, ancestor,
                                           &bg, &isCanvas)) {
      ancestor = ancestor->GetParent();
      NS_ASSERTION(ancestor, "canvas must paint");
    }
    // This isn't the most efficient way to do it, but it saves code
    // size and doesn't add much cost compared to the frame reconstruct.
    if (ancestor != frame)
      ApplyRenderingChangeToTree(aPresContext, ancestor, nsnull,
                                 nsChangeHint_RepaintFrame);
  }

  if (frame && IsFrameSpecial(frame)) {
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::RecreateFramesForContent: frame=");
      nsFrame::ListTag(stdout, frame);
      printf(" is special\n");
    }
#endif
    return ReframeContainingBlock(aPresContext, frame);
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIContent> container = aContent->GetParent();
  if (container) {
    PRInt32 indexInContainer = container->IndexOf(aContent);
    // Before removing the frames associated with the content object,
    // ask them to save their state onto a temporary state object.
    CaptureStateForFramesOf(aPresContext, aContent, mTempFrameTreeState);

    // Save parent frame because this frame is going away.  But if
    // this is an out-of-flow, we want to get the _placeholder_'s
    // parent.
    nsIFrame* parent = nsnull;
    if (frame) {
      if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
        shell->GetPlaceholderFrameFor(frame, &frame);
        NS_ASSERTION(frame, "Out-of-flow with no placeholder?");
      }

      frame = frame->GetParent();
    }

    // Remove the frames associated with the content object on which
    // the attribute change occurred.
    rv = ContentRemoved(aPresContext, container, aContent, indexInContainer,
                        PR_FALSE);

    if (NS_SUCCEEDED(rv)) {
      // Now, recreate the frames associated with this content object.
      rv = ContentInserted(aPresContext, container, parent, aContent,
                           indexInContainer, mTempFrameTreeState, PR_FALSE);
    }
  } else {
    // The content is the root node, so just rebuild the world.
    ReconstructDocElementHierarchy(aPresContext);
  }

  return rv;
}

//////////////////////////////////////////////////////////////////////

// Block frame construction code

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLetterStyle(nsPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsStyleContext* aStyleContext)
{
  if (aContent) {
    return aPresContext->StyleSet()->
      ResolvePseudoStyleFor(aContent,
                            nsCSSPseudoElements::firstLetter, aStyleContext);
  }
  return nsnull;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLineStyle(nsPresContext* aPresContext,
                                         nsIContent* aContent,
                                         nsStyleContext* aStyleContext)
{
  if (aContent) {
    return aPresContext->StyleSet()->
      ResolvePseudoStyleFor(aContent,
                            nsCSSPseudoElements::firstLine, aStyleContext);
  }
  return nsnull;
}

// Predicate to see if a given content (block element) has
// first-letter style applied to it.
PRBool
nsCSSFrameConstructor::HaveFirstLetterStyle(nsPresContext* aPresContext,
                                            nsIContent* aContent,
                                            nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       nsCSSPseudoElements::firstLetter,
                                       aPresContext);
}

PRBool
nsCSSFrameConstructor::HaveFirstLineStyle(nsPresContext* aPresContext,
                                          nsIContent* aContent,
                                          nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       nsCSSPseudoElements::firstLine,
                                       aPresContext);
}

void
nsCSSFrameConstructor::HaveSpecialBlockStyle(nsPresContext* aPresContext,
                                             nsIContent* aContent,
                                             nsStyleContext* aStyleContext,
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
                                       nsPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aFrame,
                                       PRBool                   aCanHaveGeneratedContent,
                                       nsFrameItems&            aFrameItems,
                                       PRBool                   aParentIsBlock,
                                       nsTableCreator*          aTableCreator)
{
  nsresult rv = NS_OK;
  nsStyleContext* styleContext = aFrame->GetStyleContext();
    
  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::before,
                                    &generatedFrame)) {
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

    ChildIterator iter, last;
    for (ChildIterator::Init(aContent, &iter, &last);
         iter != last;
         ++iter) {
      rv = ConstructFrame(aPresShell, aPresContext, aState, nsCOMPtr<nsIContent>(*iter),
                          aFrame, aFrameItems);

      if (NS_FAILED(rv))
        return rv;
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
                                    styleContext, nsCSSPseudoElements::after,
                                    &generatedFrame)) {
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
ReparentFrame(nsPresContext* aPresContext,
              nsIFrame* aNewParentFrame,
              nsStyleContext* aParentStyleContext,
              nsIFrame* aFrame)
{
  aFrame->SetParent(aNewParentFrame);
  aPresContext->FrameManager()->ReParentStyleContext(aFrame,
                                                     aParentStyleContext);
}

// Special routine to handle placing a list of frames into a block
// frame that has first-line style. The routine ensures that the first
// collection of inline frames end up in a first-line frame.
nsresult
nsCSSFrameConstructor::WrapFramesInFirstLineFrame(
  nsIPresShell* aPresShell, 
  nsPresContext*          aPresContext,
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
    kid = kid->GetNextSibling();
  }

  // If we don't find any inline frames, then there is nothing to do
  if (!firstInlineFrame) {
    return rv;
  }

  // Create line frame
  nsStyleContext* parentStyle = aFrame->GetStyleContext();
  nsRefPtr<nsStyleContext> firstLineStyle(
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
    nsIFrame* secondBlockFrame = lastInlineFrame->GetNextSibling();
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
      kid = kid->GetNextSibling();
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
  nsPresContext*          aPresContext,
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aFrameItems)
{
  // It's possible that aBlockFrame needs to have a first-line frame
  // created because it doesn't currently have any children.
  nsIFrame* blockKid = aBlockFrame->GetFirstChild(nsnull);
  if (!blockKid) {
    return WrapFramesInFirstLineFrame(aPresShell, aPresContext, aState, aContent,
                                      aBlockFrame, aFrameItems);
  }

  // Examine the last block child - if it's a first-line frame then
  // appended frames need special treatment.
  nsresult rv = NS_OK;
  nsFrameList blockFrames(blockKid);
  nsIFrame* lastBlockKid = blockFrames.LastChild();
  if (lastBlockKid->GetType() != nsLayoutAtoms::lineFrame) {
    // No first-line frame at the end of the list, therefore there is
    // an interveening block between any first-line frame the frames
    // we are appending. Therefore, we don't need any special
    // treatment of the appended frames.
    return rv;
  }
  nsIFrame* lineFrame = lastBlockKid;
  nsStyleContext* firstLineStyle = lineFrame->GetStyleContext();

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
    kid = kid->GetNextSibling();
  }

  // If we don't find any inline frames, then there is nothing to do
  if (!firstInlineFrame) {
    return rv;
  }

  // The inline frames get appended to the lineFrame. Make sure they
  // are reparented properly.
  nsIFrame* remainingFrames = lastInlineFrame->GetNextSibling();
  lastInlineFrame->SetNextSibling(nsnull);
  kid = firstInlineFrame;
  while (kid) {
    ReparentFrame(aPresContext, lineFrame, firstLineStyle, kid);
    kid = kid->GetNextSibling();
  }
  aState.mFrameManager->AppendFrames(lineFrame, nsnull, firstInlineFrame);

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
  nsPresContext*          aPresContext,
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
    nsIFrame* firstBlockKid = aBlockFrame->GetFirstChild(nsnull);
    if (firstBlockKid->GetType() == nsLayoutAtoms::lineFrame) {
      // We already have a first-line frame
      nsIFrame* lineFrame = firstBlockKid;
      nsStyleContext* firstLineStyle = lineFrame->GetStyleContext();

      if (isInline) {
        // Easy case: the new inline frame will go into the lineFrame.
        ReparentFrame(aPresContext, lineFrame, firstLineStyle, newFrame);
        aState.mFrameManager->InsertFrames(lineFrame, nsnull, nsnull,
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
          nsStyleContext* parentStyle = aBlockFrame->GetStyleContext();
          nsRefPtr<nsStyleContext> firstLineStyle(
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
    nsIFrame* prevSiblingParent = aPrevSibling->GetParent();
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
        nsIFrame* nextSibling = aPrevSibling->GetNextSibling();
        nsIFrame* nextLineFrame = prevSiblingParent->GetNextInFlow();
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
            nextLineFrame = nextLineFrame->GetNextInFlow();
            if (!nextLineFrame) {
              break;
            }
            nsIFrame* kids = nextLineFrame->GetFirstChild(nsnull);
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
      const nsTextFragment* frag = tc->Text();
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
  if (textContent && textContent->TextLength()) {
    result = !textContent->IsOnlyWhitespace();
  }

  return result;
}

/**
 * Create a letter frame, only make it a floating frame.
 */
void
nsCSSFrameConstructor::CreateFloatingLetterFrame(
  nsIPresShell* aPresShell, 
  nsPresContext* aPresContext,
  nsFrameConstructorState& aState,
  nsIContent* aTextContent,
  nsIFrame* aTextFrame,
  nsIContent* aBlockContent,
  nsIFrame* aParentFrame,
  nsStyleContext* aStyleContext,
  nsFrameItems& aResult)
{
  // Create the first-letter-frame
  nsIFrame* letterFrame;
  nsStyleSet *styleSet = aPresShell->StyleSet();

  NS_NewFirstLetterFrame(aPresShell, &letterFrame);  
  InitAndRestoreFrame(aPresContext, aState, aTextContent, 
                      aParentFrame, aStyleContext, nsnull, letterFrame);

  // Init the text frame to refer to the letter frame. Make sure we
  // get a proper style context for it (the one passed in is for the
  // letter frame and will have the float property set on it; the text
  // frame shouldn't have that set).
  nsRefPtr<nsStyleContext> textSC;
  textSC = styleSet->ResolveStyleForNonElement(aStyleContext);
  InitAndRestoreFrame(aPresContext, aState, aTextContent, 
                      letterFrame, textSC, nsnull, aTextFrame);

  // And then give the text frame to the letter frame
  letterFrame->SetInitialChildList(aPresContext, nsnull, aTextFrame);

  // Now make the placeholder
  nsIFrame* placeholderFrame;
  CreatePlaceholderFrameFor(aPresShell, aPresContext, aState.mFrameManager,
                            aTextContent, letterFrame,
                            aStyleContext, aParentFrame,
                            &placeholderFrame);

  // See if we will need to continue the text frame (does it contain
  // more than just the first-letter text or not?) If it does, then we
  // create (in advance) a continuation frame for it.
  nsIFrame* nextTextFrame = nsnull;
  if (NeedFirstLetterContinuation(aTextContent)) {
    // Create continuation
    CreateContinuingFrame(aPresContext, aTextFrame, aParentFrame,
                          &nextTextFrame);

    // Repair the continuations style context
    nsStyleContext* parentStyleContext = aStyleContext->GetParent();
    if (parentStyleContext) {
      nsRefPtr<nsStyleContext> newSC;
      newSC = styleSet->ResolveStyleForNonElement(parentStyleContext);
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
nsCSSFrameConstructor::CreateLetterFrame(nsIPresShell* aPresShell, nsPresContext* aPresContext,
                                         nsFrameConstructorState& aState,
                                         nsIContent* aTextContent,
                                         nsIFrame* aParentFrame,
                                         nsFrameItems& aResult)
{
  NS_PRECONDITION(aTextContent->IsContentOfType(nsIContent::eTEXT),
                  "aTextContent isn't text");

  // Get style context for the first-letter-frame
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();
  if (parentStyleContext) {
    // Use content from containing block so that we can actually
    // find a matching style rule.
    nsIContent* blockContent = aState.mFloatedItems.containingBlock->GetContent();

    // Create first-letter style rule
    nsRefPtr<nsStyleContext> sc = getter_AddRefs(
      GetFirstLetterStyle(aPresContext, blockContent, parentStyleContext));
    if (sc) {
      // Create a new text frame (the original one will be discarded)
      nsIFrame* textFrame;
      NS_NewTextFrame(aPresShell, &textFrame);

      // Create the right type of first-letter frame
      const nsStyleDisplay* display = sc->GetStyleDisplay();
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
          nsRefPtr<nsStyleContext> textSC;
          textSC = aPresContext->StyleSet()->ResolveStyleForNonElement(sc);

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
  nsIPresShell*            aPresShell, 
  nsPresContext*          aPresContext,
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
      nsIFrame* nextSibling = textFrame->GetNextSibling();
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
      DeletingFrameSubtree(aPresContext, aState.mPresShell, 
                           aState.mFrameManager, textFrame);
      parentFrame->RemoveFrame(aPresContext, *aState.mPresShell,
                               nsnull, textFrame);

      // Insert in the letter frame(s)
      parentFrame->InsertFrames(aPresContext, *aState.mPresShell,
                                nsnull, prevFrame, letterFrames.childList);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsIPresShell*            aPresShell, 
  nsPresContext*          aPresContext,
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
    nsIFrame* nextFrame = frame->GetNextSibling();

    nsIAtom* frameType = frame->GetType();
    if (nsLayoutAtoms::textFrame == frameType) {
      // Wrap up first-letter content in a letter frame
      nsIContent* textContent = frame->GetContent();
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
    else if ((nsLayoutAtoms::inlineFrame == frameType) ||
             (nsLayoutAtoms::lineFrame == frameType)) {
      nsIFrame* kids = frame->GetFirstChild(nsnull);
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
  nsPresContext* aPresContext,
  nsIPresShell* aPresShell,
  nsFrameManager* aFrameManager,
  nsIFrame* aBlockFrame,
  PRBool* aStopLooking)
{
  // First look for the float frame that is a letter frame
  nsIFrame* floatFrame = aBlockFrame->GetFirstChild(nsLayoutAtoms::floatList);
  while (floatFrame) {
    // See if we found a floating letter frame
    if (nsLayoutAtoms::letterFrame == floatFrame->GetType()) {
      break;
    }
    floatFrame = floatFrame->GetNextSibling();
  }
  if (!floatFrame) {
    // No such frame
    return NS_OK;
  }

  // Take the text frame away from the letter frame (so it isn't
  // destroyed when we destroy the letter frame).
  nsIFrame* textFrame = floatFrame->GetFirstChild(nsnull);
  if (!textFrame) {
    return NS_OK;
  }

  // Discover the placeholder frame for the letter frame
  nsIFrame* parentFrame;
  nsPlaceholderFrame* placeholderFrame = 
    aFrameManager->GetPlaceholderFrameFor(floatFrame);

  if (!placeholderFrame) {
    // Somethings really wrong
    return NS_OK;
  }
  parentFrame = placeholderFrame->GetParent();
  if (!parentFrame) {
    // Somethings really wrong
    return NS_OK;
  }

  // Create a new text frame with the right style context that maps
  // all of the content that was previously part of the letter frame
  // (and probably continued elsewhere).
  nsStyleContext* parentSC = parentFrame->GetStyleContext();
  if (!parentSC) {
    return NS_OK;
  }
  nsIContent* textContent = textFrame->GetContent();
  if (!textContent) {
    return NS_OK;
  }
  nsRefPtr<nsStyleContext> newSC;
  newSC = aPresShell->StyleSet()->ResolveStyleForNonElement(parentSC);
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
  nsIFrame* nextTextFrame = textFrame->GetNextInFlow();
  if (nextTextFrame) {
    nsIFrame* nextTextParent = nextTextFrame->GetParent();
    if (nextTextParent) {
      nsSplittableFrame::BreakFromPrevFlow(nextTextFrame);
      DeletingFrameSubtree(aPresContext, aPresShell, 
                           aFrameManager, nextTextFrame);
      aFrameManager->RemoveFrame(nextTextParent, nsnull, nextTextFrame);
    }
  }

  // First find out where (in the content) the placeholder frames
  // text is and its previous sibling frame, if any.
  nsIFrame* prevSibling = nsnull;

  nsIContent* container = parentFrame->GetContent();
  if (container && textContent) {
    PRInt32 ix = container->IndexOf(textContent);
    prevSibling = FindPreviousSibling(aPresShell, container, aBlockFrame, ix);
  }

  // Now that everything is set...
#ifdef NOISY_FIRST_LETTER
  printf("RemoveFloatingFirstLetterFrames: textContent=%p oldTextFrame=%p newTextFrame=%p\n",
         textContent.get(), textFrame, newTextFrame);
#endif
  // Should we call DeletingFrameSubtree on the placeholder instead
  // and skip this call?
  aFrameManager->UnregisterPlaceholderFrame(placeholderFrame);

  // Remove the float frame
  DeletingFrameSubtree(aPresContext, aPresShell, aFrameManager, floatFrame);
  aFrameManager->RemoveFrame(aBlockFrame, nsLayoutAtoms::floatList,
                             floatFrame);

  // Remove placeholder frame
  aFrameManager->RemoveFrame(parentFrame, nsnull, placeholderFrame);

  // Insert text frame in its place
  aFrameManager->InsertFrames(parentFrame, nsnull,
                              prevSibling, newTextFrame);

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveFirstLetterFrames(nsPresContext* aPresContext,
                                               nsIPresShell* aPresShell,
                                               nsFrameManager* aFrameManager,
                                               nsIFrame* aFrame,
                                               PRBool* aStopLooking)
{
  nsIFrame* prevSibling = nsnull;
  nsIFrame* kid = aFrame->GetFirstChild(nsnull);

  while (kid) {
    nsIAtom* frameType = kid->GetType();
    if (nsLayoutAtoms::letterFrame == frameType) {
      // Bingo. Found it. First steal away the text frame.
      nsIFrame* textFrame = kid->GetFirstChild(nsnull);
      if (!textFrame) {
        break;
      }

      // Create a new textframe
      nsStyleContext* parentSC = aFrame->GetStyleContext();
      if (!parentSC) {
        break;
      }
      nsIContent* textContent = textFrame->GetContent();
      if (!textContent) {
        break;
      }
      nsRefPtr<nsStyleContext> newSC;
      newSC = aPresShell->StyleSet()->ResolveStyleForNonElement(parentSC);
      if (!newSC) {
        break;
      }
      NS_NewTextFrame(aPresShell, &textFrame);
      textFrame->Init(aPresContext, textContent, aFrame, newSC, nsnull);

      // Next rip out the kid and replace it with the text frame
      nsFrameManager* frameManager = aFrameManager;
      DeletingFrameSubtree(aPresContext, aPresShell, frameManager, kid);
      frameManager->RemoveFrame(aFrame, nsnull, kid);

      // Insert text frame in its place
      frameManager->InsertFrames(aFrame, nsnull,
                                 prevSibling, textFrame);

      *aStopLooking = PR_TRUE;
      break;
    }
    else if ((nsLayoutAtoms::inlineFrame == frameType) ||
             (nsLayoutAtoms::lineFrame == frameType)) {
      // Look inside child inline frame for the letter frame
      RemoveFirstLetterFrames(aPresContext, aPresShell, aFrameManager, kid,
                              aStopLooking);
      if (*aStopLooking) {
        break;
      }
    }
    prevSibling = kid;
    kid = kid->GetNextSibling();
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveLetterFrames(nsPresContext* aPresContext,
                                          nsIPresShell* aPresShell,
                                          nsFrameManager* aFrameManager,
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
nsCSSFrameConstructor::RecoverLetterFrames(nsIPresShell* aPresShell, nsPresContext* aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIFrame* aBlockFrame)
{
  nsresult rv = NS_OK;

  nsIFrame* blockKids = aBlockFrame->GetFirstChild(nsnull);
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
    DeletingFrameSubtree(aPresContext, aState.mPresShell,
                         aState.mFrameManager, textFrame);
    parentFrame->RemoveFrame(aPresContext, *aState.mPresShell,
                             nsnull, textFrame);

    // Insert in the letter frame(s)
    parentFrame->InsertFrames(aPresContext, *aState.mPresShell,
                              nsnull, prevFrame, letterFrames.childList);

    // Insert in floats too if needed
    if (aState.mFloatedItems.childList) {
      aBlockFrame->AppendFrames(aPresContext, *aState.mPresShell,
                                nsLayoutAtoms::floatList,
                                aState.mFloatedItems.childList);
    }
  }
  return rv;
}

//----------------------------------------------------------------------

// listbox Widget Routines

nsresult
nsCSSFrameConstructor::CreateListBoxContent(nsPresContext* aPresContext,
                                            nsIFrame*       aParentFrame,
                                            nsIFrame*       aPrevFrame,
                                            nsIContent*     aChild,
                                            nsIFrame**      aNewFrame,
                                            PRBool          aIsAppend,
                                            PRBool          aIsScrollbar,
                                            nsILayoutHistoryState* aFrameState)
{
#ifdef MOZ_XUL
  nsresult rv = NS_OK;

  // Construct a new frame
  if (nsnull != aParentFrame) {
    nsFrameItems            frameItems;
    nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                  GetAbsoluteContainingBlock(aPresContext, aParentFrame),
                                  GetFloatContainingBlock(aPresContext, aParentFrame), 
                                  mTempFrameTreeState);

    nsRefPtr<nsStyleContext> styleContext;
    styleContext = ResolveStyleContext(aPresContext, aParentFrame, aChild);

    // Pre-check for display "none" - only if we find that, do we create
    // any frame at all
    const nsStyleDisplay* display = styleContext->GetStyleDisplay();

    if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
      *aNewFrame = nsnull;
      return NS_OK;
    }

    PRInt32 namespaceID;
    aChild->GetNameSpaceID(&namespaceID);

    rv = ConstructFrameInternal(aPresContext->PresShell(),
                                aPresContext, state, aChild,
                                aParentFrame, aChild->Tag(), namespaceID, 
                                styleContext, frameItems, PR_FALSE);
    
    nsIFrame* newFrame = frameItems.childList;
    *aNewFrame = newFrame;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      mDocument->GetBindingManager()->ProcessAttachedQueue();

      // Notify the parent frame
      if (aIsAppend)
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxAppendFrames(newFrame);
      else
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxInsertFrames(aPrevFrame, newFrame);

      InsertOutOfFlowFrames(state, aPresContext);
    }
  }

  return rv;
#else
  return NS_ERROR_FAILURE;
#endif
}

//----------------------------------------

nsresult
nsCSSFrameConstructor::ConstructBlock(nsIPresShell*            aPresShell, 
                                      nsPresContext*          aPresContext,
                                      nsFrameConstructorState& aState,
                                      const nsStyleDisplay*    aDisplay,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsIFrame*                aContentParentFrame,
                                      nsStyleContext*          aStyleContext,
                                      nsIFrame*                aNewFrame,
                                      PRBool                   aRelPos)
{
  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, aStyleContext, nsnull, aNewFrame);

  // See if we need to create a view, e.g. the frame is absolutely positioned
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, aContentParentFrame,
                                           PR_FALSE);

  // If we're the first block to be created (e.g., because we're
  // contained inside a XUL document), then make sure that we've got a
  // space manager so we can handle floats...
  if (! aState.mFloatedItems.containingBlock) {
    aNewFrame->AddStateBits(NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);
  }

  // ...and that we're the absolute containing block.
  nsFrameConstructorSaveState absoluteSaveState;
  if (aRelPos || !aState.mAbsoluteItems.containingBlock) {
    //    NS_ASSERTION(aRelPos, "should have made area frame for this");
    aState.PushAbsoluteContainingBlock(aPresContext, aNewFrame, absoluteSaveState);
  }

  // See if the block has first-letter style applied to it...
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aPresContext, aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);

  // Process the child content
  nsFrameItems childItems;
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(aNewFrame, floatSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);
  nsresult rv = ProcessChildren(aPresShell, aPresContext, aState, aContent,
                                aNewFrame, PR_TRUE, childItems, PR_TRUE);

  CreateAnonymousFrames(aPresShell, aPresContext, aContent->Tag(), aState,
                        aContent, aNewFrame, PR_FALSE, childItems);

  // Set the frame's initial child list
  aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

  // Set the frame's float list if there were any floated children
  if (aState.mFloatedItems.childList) {
    aNewFrame->SetInitialChildList(aPresContext,
                                   nsLayoutAtoms::floatList,
                                   aState.mFloatedItems.childList);
  }
  // and the same for absolutely positioned children.
  if (aRelPos && aState.mAbsoluteItems.childList) {
    aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                   aState.mAbsoluteItems.childList);
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
    kid = kid->GetNextSibling();
  }
  return PR_TRUE;
}

nsresult
nsCSSFrameConstructor::ConstructInline(nsIPresShell*            aPresShell, 
                                       nsPresContext*          aPresContext,
                                       nsFrameConstructorState& aState,
                                       const nsStyleDisplay*    aDisplay,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
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

  // Any inline frame might need a view (because of opacity, or fixed background)
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);

  if (aIsPositioned) {                            
    // Relatively positioned frames becomes a container for child
    // frames that are positioned
    aState.PushAbsoluteContainingBlock(aPresContext, aNewFrame, absoluteSaveState);
  }

  // Process the child content
  nsFrameItems childItems;
  PRBool kidsAllInline;
  nsresult rv = ProcessInlineChildren(aPresShell, aPresContext, aState, aContent,
                                      aNewFrame, PR_TRUE, childItems, &kidsAllInline);
  if (kidsAllInline) {
    // Set the inline frame's initial child list
    CreateAnonymousFrames(aPresShell, aPresContext, aContent->Tag(), aState,
                          aContent, aNewFrame, PR_FALSE, childItems);

    aNewFrame->SetInitialChildList(aPresContext, nsnull, childItems.childList);

    if (aIsPositioned) {
      if (aState.mAbsoluteItems.childList) {
        aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                       aState.mAbsoluteItems.childList);
      }
      if (aState.mFloatedItems.childList) {
        aNewFrame->SetInitialChildList(aPresContext,
                                       nsLayoutAtoms::floatList,
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
  nsIFrame* afterFirstBlock = list2->GetNextSibling();
  nsIFrame* list3 = nsnull;
  nsIFrame* lastBlock = FindLastBlock(aPresContext, afterFirstBlock);
  if (!lastBlock) {
    lastBlock = list2;
  }
  list3 = lastBlock->GetNextSibling();
  lastBlock->SetNextSibling(nsnull);

  // list1's frames belong to this inline frame so go ahead and take them
  aNewFrame->SetInitialChildList(aPresContext, nsnull, list1);

  if (aIsPositioned) {
    // XXXwaterson just for shits n' giggles, we'll give you the
    // absolute and floated items, too. Is this right?
    if (aState.mAbsoluteItems.childList) {
      aNewFrame->SetInitialChildList(aPresContext, nsLayoutAtoms::absoluteList,
                                     aState.mAbsoluteItems.childList);
    }
    if (aState.mFloatedItems.childList) {
      aNewFrame->SetInitialChildList(aPresContext,
                                     nsLayoutAtoms::floatList,
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
    blockStyle = nsCSSAnonBoxes::mozAnonymousPositionedBlock;
  }
  else {
    NS_NewBlockFrame(aPresShell, &blockFrame);
    blockStyle = nsCSSAnonBoxes::mozAnonymousBlock;
  }

  nsRefPtr<nsStyleContext> blockSC;
  blockSC = aPresShell->StyleSet()->ResolvePseudoStyleFor(aContent, blockStyle,
                                                          aStyleContext);

  InitAndRestoreFrame(aPresContext, aState, aContent, 
                      aParentFrame, blockSC, nsnull, blockFrame);  

  // Any inline frame could have a view (e.g., opacity)
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(blockFrame, nsnull, PR_FALSE);

  if (blockFrame->HasView() || aNewFrame->HasView()) {
    // Move list2's frames into the new view
    nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, list2,
                                                list2->GetParent(), blockFrame);
  }

  blockFrame->SetInitialChildList(aPresContext, nsnull, list2);

  nsFrameConstructorState state(aPresContext, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(aPresContext, blockFrame),
                                GetFloatContainingBlock(aPresContext, blockFrame));

  MoveChildrenTo(aPresContext, blockSC, blockFrame, list2, &state);

  // list3's frames belong to another inline frame
  nsIFrame* inlineFrame = nsnull;

  if (list3) {
    if (aIsPositioned) {
      NS_NewPositionedInlineFrame(aPresShell, &inlineFrame);
    }
    else {
      NS_NewInlineFrame(aPresShell, &inlineFrame);
    }

    InitAndRestoreFrame(aPresContext, aState, aContent, 
                        aParentFrame, aStyleContext, nsnull, inlineFrame);

    // Any frame might need a view
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(inlineFrame, nsnull, PR_FALSE);

    if (inlineFrame->HasView() || aNewFrame->HasView()) {
      // Move list3's frames into the new view
      nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, list3,
                                                  list3->GetParent(), inlineFrame);
    }

    // Reparent (cheaply) the frames in list3 - we don't have to futz
    // with their style context because they already have the right one.
    inlineFrame->SetInitialChildList(aPresContext, nsnull, list3);
    MoveChildrenTo(aPresContext, nsnull, inlineFrame, list3, nsnull);
  }

  // Mark the 3 frames as special. That way if any of the
  // append/insert/remove methods try to fiddle with the children, the
  // containing block will be reframed instead.
  SetFrameIsSpecial(aState.mFrameManager, aNewFrame, blockFrame);
  SetFrameIsSpecial(aState.mFrameManager, blockFrame, inlineFrame);
  MarkIBSpecialPrevSibling(aPresContext, aState.mFrameManager,
                           blockFrame, aNewFrame);

  if (inlineFrame)
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
    if (inlineFrame && NS_SUCCEEDED(inlineFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
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
                                             nsPresContext*          aPresContext,
                                             nsFrameConstructorState& aState,
                                             nsIContent*              aContent,
                                             nsIFrame*                aFrame,
                                             PRBool                   aCanHaveGeneratedContent,
                                             nsFrameItems&            aFrameItems,
                                             PRBool*                  aKidsAllInline)
{
  nsresult rv = NS_OK;
  nsStyleContext* styleContext = nsnull;

  // save the pseudo frame state 
  nsPseudoFrames prevPseudoFrames; 
  aState.mPseudoFrames.Reset(&prevPseudoFrames);

  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    styleContext = aFrame->GetStyleContext();
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::before,
                                    &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  // Iterate the child content objects and construct frames
  PRBool allKidsInline = PR_TRUE;
  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       iter != last;
       ++iter) {
    // Construct a child frame
    nsIFrame* oldLastChild = aFrameItems.lastChild;
    rv = ConstructFrame(aPresShell, aPresContext, aState, nsCOMPtr<nsIContent>(*iter),
                        aFrame, aFrameItems);

    if (NS_FAILED(rv)) {
      return rv;
    }

    // Examine newly added children (we may have added more than one
    // child if the child was another inline frame that ends up
    // being carved in 3 pieces) to maintain the allKidsInline flag.
    if (allKidsInline) {
      nsIFrame* kid;
      if (oldLastChild) {
        kid = oldLastChild->GetNextSibling();
      }
      else {
        kid = aFrameItems.childList;
      }
      while (kid) {
        if (!IsInlineFrame(kid)) {
          allKidsInline = PR_FALSE;
          break;
        }
        kid = kid->GetNextSibling();
      }
    }
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aPresShell, aPresContext, aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::after,
                                    &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  *aKidsAllInline = allKidsInline;

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aPresContext, aState.mPseudoFrames, aFrameItems);
  }
  // restore the pseudo frame state
  aState.mPseudoFrames = prevPseudoFrames;

  return rv;
}

// Helper function that recursively removes content to frame mappings and
// undisplayed content mappings.
// This differs from DeletingFrameSubtree() because the frames have not yet been
// added to the frame hierarchy
static void
DoCleanupFrameReferences(nsPresContext*  aPresContext,
                         nsFrameManager*  aFrameManager,
                         nsIFrame*        aFrameIn)
{
  nsIContent* content = aFrameIn->GetContent();

  nsIFrame* frame = aFrameIn;
  // if the frame is a placeholder use the out of flow frame
  if (nsLayoutAtoms::placeholderFrame == aFrameIn->GetType()) {
    frame = ((nsPlaceholderFrame*)frame)->GetOutOfFlowFrame();
    NS_ASSERTION(frame, "program error - null of of flow frame in placeholder");
  }

  // Remove the mapping from the content object to its frame
  aFrameManager->SetPrimaryFrameFor(content, nsnull);
  frame->RemovedAsPrimaryFrame(aPresContext);
  aFrameManager->ClearAllUndisplayedContentIn(content);

  // Recursively walk the child frames.
  // Note: we only need to look at the principal child list
  nsIFrame* childFrame = frame->GetFirstChild(nsnull);
  while (childFrame) {
    DoCleanupFrameReferences(aPresContext, aFrameManager, childFrame);
    
    // Get the next sibling child frame
    childFrame = childFrame->GetNextSibling();
  }
}

// Helper function that walks a frame list and calls DoCleanupFrameReference()
static void
CleanupFrameReferences(nsPresContext*  aPresContext,
                       nsFrameManager*  aFrameManager,
                       nsIFrame*        aFrameList)
{
  while (aFrameList) {
    DoCleanupFrameReferences(aPresContext, aFrameManager, aFrameList);

    // Get the sibling frame
    aFrameList = aFrameList->GetNextSibling();
  }
}

PRBool
nsCSSFrameConstructor::WipeContainingBlock(nsPresContext* aPresContext,
                                           nsFrameConstructorState& aState,
                                           nsIFrame* aContainingBlock,
                                           nsIFrame* aFrame,
                                           nsIFrame* aFrameList)
{
  // Before we go and append the frames, check for a special
  // situation: an inline frame that will now contain block
  // frames. This is a no-no and the frame construction logic knows
  // how to fix this.

  // If we don't have a block within an inline, just return false.
  // XXX We should be more careful about |aFrame| being something
  // constructed by tag name (see the SELECT check all callers currenly
  // do).
  if (NS_STYLE_DISPLAY_INLINE != aFrame->GetStyleDisplay()->mDisplay ||
      AreAllKidsInline(aFrameList))
    return PR_FALSE;

  // Ok, reverse tracks: wipe out the frames we just created
  nsFrameManager *frameManager = aPresContext->FrameManager();

  // Destroy the frames. As we do make sure any content to frame mappings
  // or entries in the undisplayed content map are removed
  frameManager->ClearAllUndisplayedContentIn(aFrame->GetContent());

  CleanupFrameReferences(aPresContext, frameManager, aFrameList);
  if (aState.mAbsoluteItems.childList) {
    CleanupFrameReferences(aPresContext, frameManager, aState.mAbsoluteItems.childList);
  }
  if (aState.mFixedItems.childList) {
    CleanupFrameReferences(aPresContext, frameManager, aState.mFixedItems.childList);
  }
  if (aState.mFloatedItems.childList) {
    CleanupFrameReferences(aPresContext, frameManager, aState.mFloatedItems.childList);
  }
  nsFrameList tmp(aFrameList);
  tmp.DestroyFrames(aPresContext);
  tmp.SetFrames(aState.mAbsoluteItems.childList);
  tmp.DestroyFrames(aPresContext);
  tmp.SetFrames(aState.mFixedItems.childList);
  tmp.DestroyFrames(aPresContext);
  tmp.SetFrames(aState.mFloatedItems.childList);
  tmp.DestroyFrames(aPresContext);
  
  // Tell parent of the containing block to reformulate the
  // entire block. This is painful and definitely not optimal
  // but it will *always* get the right answer.

  // First, if the containing block is really a block wrapper for something
  // that's really an inline, walk up the parent chain until we hit something
  // that's not.
  while (IsFrameSpecial(aContainingBlock))
    aContainingBlock = aContainingBlock->GetParent();

  nsIContent *blockContent = aContainingBlock->GetContent();
  nsCOMPtr<nsIContent> parentContainer = blockContent->GetParent();
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::WipeContainingBlock: blockContent=%p parentContainer=%p\n",
           NS_STATIC_CAST(void*, blockContent),
           NS_STATIC_CAST(void*, parentContainer));
  }
#endif
  if (parentContainer) {
    ReinsertContent(aPresContext, parentContainer, blockContent);
  }
  else {
    NS_ERROR("uh oh. the block we need to reframe has no parent!");
  }
  return PR_TRUE;
}


/*
 * Recursively split an inline frame until we reach a block frame.
 * Below is an example of how SplitToContainingBlock() works.
 *
 * 1. In the initial state, you've got a block frame |B| that contains
 *    a bunch of inline frames eventually winding down to the <object>
 *    frame |o|. (Block frames are indicated with upper case letters,
 *    inline frames are indicated with lower case.)
 *
 *     A-->B-->C
 *         |
 *         i-->j-->k
 *             |
 *             l-->m-->n
 *                 |
 *                 o
 *
 * 2. Now the object frame |o| gets split into the left inlines |o|,
 *    the block frames |O|, and the right inlines |o'|.
 *
 *             o O o'
 *
 * 3. We call SplitToContainingBlock(), which will split |m| as follows. 
 *
 *             .--------.
 *            /          \
 *       l-->m==>M==>m'   n
 *           |   |   |
 *           o   O   o'
 *
 *    Note that |m| gets split into |M| and |m'|, which correspond to
 *    the anonymous block and inline frames,
 *    respectively. Furthermore, note that |m| still refers to |n| as
 *    its next sibling, and that |m'| does not yet have a next sibling.
 *
 *    The double-arrow line indicates that an annotation is made in
 *    the frame manager that indicates |M| is the ``special sibling''
 *    of |m|, and that |m'| is the ``special sibling'' of |M|.
 *
 * 4. We recurse again to split |j|. At this point, we'll break the
 *    link between |m| and |n|, and make |n| be the next sibling of
 *    |m'|.
 *
 *             .-----------.
 *            /             \
 *       i-->j=====>J==>j'   k
 *           |      |   |
 *           l-->m=>M==>m'-->n
 *               |  |   |
 *               o  O   o'
 *
 *     As before, |j| retains |k| as its next sibling, and |j'| is not
 *     yet assigned its next sibling.
 *
 * 5. When we hit B, the recursion terminates. We now insert |J| and
 *    |j'| immediately after |j|, resulting in the following frame
 *    model. This is done using the "normal" frame insertion
 *    mechanism, nsIFrame::InsertFrames(), which properly recomputes
 *    the line boxes.
 *
 *     A-->B-->C
 *         |
 *         i-->j-=-=-=>J-=->j'-->k
 *             |       |    |
 *             l-->m==>M===>m'-->n
 *                 |   |    |
 *                 o   O    o'
 *
 *    Since B is a block, it is allowed to contain both block and
 *    inline frames, so we can let |J| and |j'| be "real siblings" of
 *    |j|.
 *
 *    Note that |J| is both the ``normal'' sibling and ``special''
 *    sibling of |j|, and |j'| is both the ``normal'' and ``special''
 *    sibling of |J|.
 */
nsresult
nsCSSFrameConstructor::SplitToContainingBlock(nsPresContext* aPresContext,
                                              nsFrameConstructorState& aState,
                                              nsIFrame* aFrame,
                                              nsIFrame* aLeftInlineChildFrame,
                                              nsIFrame* aBlockChildFrame,
                                              nsIFrame* aRightInlineChildFrame,
                                              PRBool aTransfer)
{
  // If aFrame is an inline frame, then recursively "split" it until
  // we reach a block frame. aLeftInlineChildFrame is the original
  // inline child of aFrame (or null, if there were no frames to the
  // left of the new block); aBlockChildFrame and
  // aRightInlineChildFrame are the newly created frames that were
  // constructed as a result of the previous recursion's
  // "split". aRightInlineChildFrame may be null if there are no
  // inlines to the right of the new block.
  //
  // aBlockChildFrame and aRightInlineChildFrame will be "orphaned" frames upon
  // entry to this routine; that is, they won't be parented. We'll
  // assign them proper parents.
  NS_PRECONDITION(aFrame != nsnull, "no frame to split");
  if (! aFrame)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aBlockChildFrame != nsnull, "no block child");
  if (! aBlockChildFrame)
    return NS_ERROR_NULL_POINTER;

  nsIPresShell *shell = aPresContext->PresShell();

  if (IsBlockFrame(aPresContext, aFrame)) {
    // If aFrame is a block frame, then we're done: make
    // aBlockChildFrame and aRightInlineChildFrame children of aFrame,
    // and insert aBlockChildFrame and aRightInlineChildFrame after
    // aLeftInlineChildFrame
    aBlockChildFrame->SetParent(aFrame);

    if (aRightInlineChildFrame)
      aRightInlineChildFrame->SetParent(aFrame);

    aBlockChildFrame->SetNextSibling(aRightInlineChildFrame);
    aFrame->InsertFrames(aPresContext, *shell, nsnull, aLeftInlineChildFrame, aBlockChildFrame);

    // If aLeftInlineChild has a view...
    if (aLeftInlineChildFrame && aLeftInlineChildFrame->HasView()) {
      // ...create a new view for the block child, and reparent views
      // XXXbz should we be passing in a non-null aContentParentFrame?
      // force creation of a view; it'll probably need one anyway since it
      // has the same style context, and it's easier to think about if we can
      // be sure the left inlines, the block and the right inlines all have a view
      nsHTMLContainerFrame::CreateViewForFrame(aBlockChildFrame, nsnull, PR_TRUE);
      
      nsIFrame* frame = aBlockChildFrame->GetFirstChild(nsnull);
      nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, frame, aLeftInlineChildFrame, aBlockChildFrame);
      
      if (aRightInlineChildFrame) {
        // Same for the right inline children
        // XXXbz should we be passing in a non-null aContentParentFrame?
        nsHTMLContainerFrame::CreateViewForFrame(aRightInlineChildFrame, nsnull, PR_TRUE);
        
        frame = aRightInlineChildFrame->GetFirstChild(nsnull);
        nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, frame, aLeftInlineChildFrame, aRightInlineChildFrame);
      }
    }

    return NS_OK;
  }

  // Otherwise, aFrame is inline. Split it, and recurse to find the
  // containing block frame.
  nsIContent* content = aFrame->GetContent();

  // Create an "anonymous block" frame that will parent
  // aBlockChildFrame. The new frame won't have a parent yet: the recursion
  // will parent it.
  nsIFrame* blockFrame;
  NS_NewBlockFrame(shell, &blockFrame);
  if (! blockFrame)
    return NS_ERROR_OUT_OF_MEMORY;

  nsStyleContext* styleContext = aFrame->GetStyleContext();

  nsRefPtr<nsStyleContext> blockSC;
  blockSC = shell->StyleSet()->ResolvePseudoStyleFor(content,
                                                     nsCSSAnonBoxes::mozAnonymousBlock,
                                                     styleContext);

  InitAndRestoreFrame(aPresContext, aState, content,
                      nsnull, blockSC, nsnull, blockFrame);

  blockFrame->SetInitialChildList(aPresContext, nsnull, aBlockChildFrame);
  MoveChildrenTo(aPresContext, blockSC, blockFrame, aBlockChildFrame, nsnull);

  // Create an anonymous inline frame that will parent
  // aRightInlineChildFrame. The new frame won't have a parent yet:
  // the recursion will parent it.
  // XXXldb Why bother if |aRightInlineChildFrame| is null?
  nsIFrame* inlineFrame = nsnull;
  NS_NewInlineFrame(shell, &inlineFrame);
  if (! inlineFrame)
    return NS_ERROR_OUT_OF_MEMORY;

  InitAndRestoreFrame(aPresContext, aState, content,
                      nsnull, styleContext, nsnull, inlineFrame);

  inlineFrame->SetInitialChildList(aPresContext, nsnull, aRightInlineChildFrame);
  MoveChildrenTo(aPresContext, nsnull, inlineFrame, aRightInlineChildFrame, nsnull);

  // Make the "special" inline-block linkage between aFrame and the
  // newly created anonymous frames. We need to create the linkage
  // between the first in flow, so if we're a continuation frame, walk
  // back to find it.
  nsIFrame* firstInFlow = aFrame->GetFirstInFlow();

  SetFrameIsSpecial(aState.mFrameManager, firstInFlow, blockFrame);
  SetFrameIsSpecial(aState.mFrameManager, blockFrame, inlineFrame);
  SetFrameIsSpecial(aState.mFrameManager, inlineFrame, nsnull);

  MarkIBSpecialPrevSibling(aPresContext, aState.mFrameManager,
                           blockFrame, firstInFlow);

  // If we have a continuation frame, then we need to break the
  // continuation.
  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
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
    // We'd better have the left- and right-inline children!
    NS_ASSERTION(aLeftInlineChildFrame != nsnull, "no left inline child frame");
    NS_ASSERTION(aRightInlineChildFrame != nsnull, "no right inline child frame");

    // We need to move any successors of the original inline
    // (aLeftInlineChildFrame) to aRightInlineChildFrame.
    nsIFrame* nextInlineFrame = aLeftInlineChildFrame->GetNextSibling();
    aLeftInlineChildFrame->SetNextSibling(nsnull);
    aRightInlineChildFrame->SetNextSibling(nextInlineFrame);

    // Any frame that was moved will need its parent pointer fixed,
    // and will need to be marked as dirty.
    while (nextInlineFrame) {
      nextInlineFrame->SetParent(inlineFrame);
      nextInlineFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      nextInlineFrame = nextInlineFrame->GetNextSibling();
    }
  }

  // Recurse to the parent frame. This will assign a parent frame to
  // each new frame we've just created.
  nsIFrame* parent = aFrame->GetParent();

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
nsCSSFrameConstructor::ReframeContainingBlock(nsPresContext* aPresContext, nsIFrame* aFrame)
{
#ifdef DEBUG
  PRBool isAttinasi = PR_FALSE;
#ifdef DEBUG_attinasi
  isAttinasi = PR_TRUE;
#endif // DEBUG_attinasi

  // ReframeContainingBlock is a NASTY routine, it causes terrible performance problems
  // so I want to see when it is happening!  Unfortunately, it is happening way to often because
  // so much content on the web causes 'special' block-in-inline frame situations and we handle them
  // very poorly
  if (gNoisyContentUpdates || isAttinasi) {
    printf("nsCSSFrameConstructor::ReframeContainingBlock frame=%p\n",
           NS_STATIC_CAST(void*, aFrame));
  }
#endif

  PRBool isReflowing;
  aPresContext->PresShell()->IsReflowLocked(&isReflowing);
  if(isReflowing) {
    // don't ReframeContainingBlock, this will result in a crash
    // if we remove a tree that's in reflow - see bug 121368 for testcase
    NS_ASSERTION(0, "Atemptted to nsCSSFrameConstructor::ReframeContainingBlock during a Reflow!!!");
    return NS_OK;
  }

  // Get the first "normal" ancestor of the target frame.
  nsIFrame* containingBlock = GetIBContainingBlockFor(aFrame);
  if (containingBlock) {
    // From here we look for the containing block in case the target
    // frame is already a block (which can happen when an inline frame
    // wraps some of its content in an anonymous block; see
    // ConstructInline)
   
    // NOTE: We used to get the FloatContainingBlock here, but it was often wrong.
    // GetIBContainingBlock works much better and provides the correct container in all cases
    // so GetFloatContainingBlock(aPresContext, aFrame) has been removed

    // And get the containingBlock's content
    nsCOMPtr<nsIContent> blockContent = containingBlock->GetContent();
    if (blockContent) {
      // Now find the containingBlock's content's parent
      nsCOMPtr<nsIContent> parentContainer = blockContent->GetParent();
      if (parentContainer) {
#ifdef DEBUG
        if (gNoisyContentUpdates) {
          printf("  ==> blockContent=%p, parentContainer=%p\n",
                 NS_STATIC_CAST(void*, blockContent),
                 NS_STATIC_CAST(void*, parentContainer));
        }
#endif
        return ReinsertContent(aPresContext, parentContainer, blockContent);
      }
    }
  }

  // If we get here, we're screwed!
  return ReconstructDocElementHierarchy(aPresContext);
}

nsresult nsCSSFrameConstructor::RemoveFixedItems(nsPresContext* aPresContext,
                                                 nsIPresShell*   aPresShell,
                                                 nsFrameManager* aFrameManager)
{
  nsresult rv=NS_OK;

  if (mFixedContainingBlock) {
    nsIFrame *fixedChild = nsnull;
    do {
      fixedChild = mFixedContainingBlock->GetFirstChild(nsLayoutAtoms::fixedList);
      if (fixedChild) {
        // Remove the placeholder so it doesn't end up sitting about pointing
        // to the removed fixed frame.
        nsIFrame *placeholderFrame;
        aPresShell->GetPlaceholderFrameFor(fixedChild, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for fixed-pos frame");
        nsIFrame* placeholderParent = placeholderFrame->GetParent();
        DeletingFrameSubtree(aPresContext, aPresShell, aFrameManager,
                             placeholderFrame);
        rv = aFrameManager->RemoveFrame(placeholderParent, nsnull,
                                        placeholderFrame);
        if (NS_FAILED(rv)) {
          NS_WARNING("Error removing placeholder for fixed frame in RemoveFixedItems");
          break;
        }

        DeletingFrameSubtree(aPresContext, aPresShell, aFrameManager,
                             fixedChild);
        rv = aFrameManager->RemoveFrame(mFixedContainingBlock,
                                        nsLayoutAtoms::fixedList,
                                        fixedChild);
        if (NS_FAILED(rv)) {
          NS_WARNING("Error removing frame from fixed containing block in RemoveFixedItems");
          break;
        }
      }
    } while(fixedChild);
  } else {
    NS_WARNING( "RemoveFixedItems called with no FixedContainingBlock data member set");
  }
  return rv;
}

PR_STATIC_CALLBACK(PLDHashOperator)
CollectRestyles(nsISupports* aContent,
                nsCSSFrameConstructor::RestyleData& aData,
                void* aRestyleArrayPtr)
{
  nsCSSFrameConstructor::RestyleEnumerateData** restyleArrayPtr =
    NS_STATIC_CAST(nsCSSFrameConstructor::RestyleEnumerateData**,
                   aRestyleArrayPtr);
  nsCSSFrameConstructor::RestyleEnumerateData* currentRestyle =
    *restyleArrayPtr;
  currentRestyle->mContent = NS_STATIC_CAST(nsIContent*, aContent);
  currentRestyle->mRestyleHint = aData.mRestyleHint;
  currentRestyle->mChangeHint = aData.mChangeHint;

  // Increment to the next slot in the array
  *restyleArrayPtr = currentRestyle + 1; 

  return PL_DHASH_NEXT;
}

void
nsCSSFrameConstructor::ProcessPendingRestyles()
{
  NS_PRECONDITION(mDocument, "No document?  Pshaw!\n");
  nsIPresShell* shell = mDocument->GetShellAt(0);
  nsPresContext* context = shell->GetPresContext();

  nsCSSFrameConstructor::RestyleEnumerateData* restylesToProcess =
    new nsCSSFrameConstructor::RestyleEnumerateData[mPendingRestyles.Count()];
  if (!restylesToProcess) {
    return;
  }

  nsCSSFrameConstructor::RestyleEnumerateData* lastRestyle = restylesToProcess;
  mPendingRestyles.Enumerate(CollectRestyles, &lastRestyle);

  NS_ASSERTION(lastRestyle - restylesToProcess ==
               PRInt32(mPendingRestyles.Count()),
               "Enumeration screwed up somehow");

  // Clear the hashtable so we don't end up trying to process a restyle we're
  // already processing, sending us into an infinite loop.
  mPendingRestyles.Clear();

  for (nsCSSFrameConstructor::RestyleEnumerateData* currentRestyle =
         restylesToProcess;
       currentRestyle != lastRestyle;
       ++currentRestyle) {
    nsIContent* content = currentRestyle->mContent;

    if (!content->GetDocument() ||
        content->GetDocument() != context->GetDocument()) {
      // Content node has been removed from our document; nothing else
      // to do here
      continue;
    }
  
    nsChangeHint changeHint = currentRestyle->mChangeHint;
    nsReStyleHint restyleHint = currentRestyle->mRestyleHint;

    nsIFrame* primaryFrame = nsnull;
    shell->GetPrimaryFrameFor(content, &primaryFrame);
    if (restyleHint & eReStyle_Self) {
      shell->FrameConstructor()->RestyleElement(context, content, primaryFrame,
                                                currentRestyle->mChangeHint);
    } else if (changeHint &&
               (primaryFrame ||
                (changeHint & nsChangeHint_ReconstructFrame))) {
      // Don't need to recompute style; just apply the hint
      nsStyleChangeList changeList;
      changeList.AppendChange(primaryFrame, content, changeHint);
      shell->FrameConstructor()->ProcessRestyledFrames(changeList, context);
    }

    if (restyleHint & eReStyle_LaterSiblings) {
      shell->FrameConstructor()->RestyleLaterSiblings(context, content);
    }
  }

  delete [] restylesToProcess;
}

void
nsCSSFrameConstructor::PostRestyleEvent(nsIContent* aContent,
                                        nsReStyleHint aRestyleHint,
                                        nsChangeHint aMinChangeHint)
{
  if (aRestyleHint == 0 && !aMinChangeHint) {
    // Nothing to do here
    return;
  }
  
  RestyleData existingData;
  existingData.mRestyleHint = nsReStyleHint(0);
  existingData.mChangeHint = NS_STYLE_HINT_NONE;

  mPendingRestyles.Get(aContent, &existingData);
  existingData.mRestyleHint =
    nsReStyleHint(existingData.mRestyleHint | aRestyleHint);
  NS_UpdateHint(existingData.mChangeHint, aMinChangeHint);

  mPendingRestyles.Put(aContent, existingData);
    
  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQueue));
    
  if (eventQueue != mRestyleEventQueue) {
    RestyleEvent* ev = new RestyleEvent(this);
    if (NS_FAILED(eventQueue->PostEvent(ev))) {
      PL_DestroyEvent(ev);
      // XXXbz and what?
    } else {
      mRestyleEventQueue = eventQueue;
    }
  }
}

PR_STATIC_CALLBACK(void*)
HandleRestyleEvent(PLEvent* aEvent)
{
  nsCSSFrameConstructor::RestyleEvent* evt =
    NS_STATIC_CAST(nsCSSFrameConstructor::RestyleEvent*, aEvent);
  evt->HandleEvent();
  return nsnull;
}

PR_STATIC_CALLBACK(void)
DestroyRestyleEvent(PLEvent* aEvent)
{
  delete NS_STATIC_CAST(nsCSSFrameConstructor::RestyleEvent*, aEvent);
}

nsCSSFrameConstructor::RestyleEvent::RestyleEvent(nsCSSFrameConstructor* aConstructor)
{
  NS_PRECONDITION(aConstructor, "Must have a constructor!");

  PL_InitEvent(this, aConstructor,
               ::HandleRestyleEvent, ::DestroyRestyleEvent);
}
  
