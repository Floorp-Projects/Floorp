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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * construction of a frame tree that is nearly isomorphic to the content
 * tree and updating of that tree in response to dynamic changes
 */

#include "nsCSSFrameConstructor.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsTableFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLTableColElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsIViewManager.h"
#include "nsIEventStateManager.h"
#include "nsIScrollableView.h"
#include "nsStyleConsts.h"
#include "nsTableOuterFrame.h"
#include "nsIDOMXULElement.h"
#include "nsHTMLContainerFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIComboboxControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIRadioControlFrame.h"
#include "nsICheckboxControlFrame.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLImageElement.h"
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
#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsITheme.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsObjectFrame.h"
#include "nsRuleNode.h"
#include "nsIDOMMutationEvent.h"
#include "nsChildIterator.h"
#include "nsCSSRendering.h"
#include "nsISelectElement.h"
#include "nsLayoutErrors.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"
#include "nsBoxFrame.h"
#include "nsIBoxLayout.h"
#include "nsImageFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsContentErrors.h"
#include "nsIPrincipal.h"
#include "nsIDOMWindowInternal.h"
#include "nsStyleUtil.h"
#include "nsIFocusEventSuppressor.h"
#include "nsBox.h"

#ifdef MOZ_XUL
#include "nsIRootBox.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIXULDocument.h"
#endif
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#include "nsIAccessibleEvent.h"
#endif

#include "nsInlineFrame.h"
#include "nsBlockFrame.h"

#include "nsIScrollableFrame.h"

#include "nsIXBLService.h"

#undef NOISY_FIRST_LETTER

#ifdef MOZ_MATHML
#include "nsMathMLParts.h"
#endif
#ifdef MOZ_SVG
#include "nsSVGEffects.h"
#endif

nsIFrame*
NS_NewHTMLCanvasFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

#if defined(MOZ_MEDIA)
nsIFrame*
NS_NewHTMLVideoFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);
#endif

#ifdef MOZ_SVG
#include "nsISVGTextContentMetrics.h"

PRBool
NS_SVGEnabled();
nsIFrame*
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGenericContainerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGSwitchFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
PRBool 
NS_SVG_PassesConditionalProcessingTests(nsIContent *aContent);
extern nsIFrame*
NS_NewSVGLinearGradientFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGRadialGradientFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGStopFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsIFrame *aParentFrame, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGImageFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTextPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGPatternFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGLeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
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
#include "nsIServiceManager.h"

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
static PRBool gTablePseudoFrame = PR_FALSE;

struct FrameCtorDebugFlags {
  const char* name;
  PRBool* on;
};

static FrameCtorDebugFlags gFlags[] = {
  { "content-updates",              &gNoisyContentUpdates },
  { "really-noisy-content-updates", &gReallyNoisyContentUpdates },
  { "noisy-inline",                 &gNoisyInlineConstruction },
  { "fast-find-frame",              &gVerifyFastFindFrame },
  { "table-pseudo",                 &gTablePseudoFrame },
};

#define NUM_DEBUG_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))
#endif


#ifdef MOZ_XUL
#include "nsMenuFrame.h"
#include "nsPopupSetFrame.h"
#include "nsTreeColFrame.h"
#include "nsIBoxObject.h"
#include "nsPIListBoxObject.h"
#include "nsListBoxBodyFrame.h"
#include "nsListItemFrame.h"
#include "nsXULLabelFrame.h"

//------------------------------------------------------------------

nsIFrame*
NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewRootBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewThumbFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewDeckFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, nsIBoxLayout* aLayoutManager = nsnull);

nsIFrame*
NS_NewLeafBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewStackFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, nsIBoxLayout* aLayoutManager = nsnull);

nsIFrame*
NS_NewProgressMeterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewImageBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewTextBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewGroupBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewButtonBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewSplitterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewMenuPopupFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewMenuFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

nsIFrame*
NS_NewMenuBarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewTreeBodyFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

// grid
nsresult
NS_NewGridLayout2 ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsresult
NS_NewGridRowLeafLayout ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsIFrame*
NS_NewGridRowLeafFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot, nsIBoxLayout* aLayout);
nsresult
NS_NewGridRowGroupLayout ( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout );
nsIFrame*
NS_NewGridRowGroupFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot, nsIBoxLayout* aLayout);

nsresult
NS_NewListBoxLayout ( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout );

// end grid

nsIFrame*
NS_NewTitleBarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewResizerFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);


#endif

nsIFrame*
NS_NewHTMLScrollFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot);

nsIFrame*
NS_NewXULScrollFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot);

nsIFrame*
NS_NewSliderFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewScrollbarButtonFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);


#ifdef NOISY_FINDFRAME
static PRInt32 FFWC_totalCount=0;
static PRInt32 FFWC_doLoop=0;
static PRInt32 FFWC_doSibling=0;
static PRInt32 FFWC_recursions=0;
static PRInt32 FFWC_nextInFlows=0;
static PRInt32 FFWC_slowSearchForText=0;
#endif

static nsresult
DeletingFrameSubtree(nsFrameManager* aFrameManager,
                     nsIFrame*       aFrame);

#ifdef  MOZ_SVG

static nsIFrame *
SVG_GetFirstNonAAncestorFrame(nsIFrame *aParentFrame)
{
  for (nsIFrame *ancestorFrame = aParentFrame; ancestorFrame != nsnull;
       ancestorFrame = ancestorFrame->GetParent()) {
    if (ancestorFrame->GetType() != nsGkAtoms::svgAFrame) {
      return ancestorFrame;
    }
  }
  return nsnull;
}
#endif

static inline nsIFrame*
GetFieldSetBlockFrame(nsIFrame* aFieldsetFrame)
{
  // Depends on the fieldset child frame order - see ConstructFieldSetFrame() below.
  nsIFrame* firstChild = aFieldsetFrame->GetFirstChild(nsnull);
  return firstChild && firstChild->GetNextSibling() ? firstChild->GetNextSibling() : firstChild;
}

//----------------------------------------------------------------------

static PRBool
IsInlineOutside(nsIFrame* aFrame)
{
  return aFrame->GetStyleDisplay()->IsInlineOutside();
}

/**
 * True if aFrame is an actual inline frame in the sense of non-replaced
 * display:inline CSS boxes.  In other words, it can be affected by {ib}
 * splitting and can contain first-letter frames.  Basically, this is either an
 * inline frame (positioned or otherwise) or an line frame (this last because
 * it can contain first-letter and because inserting blocks in the middle of it
 * needs to terminate it).
 */
static PRBool
IsInlineFrame(const nsIFrame* aFrame)
{
  return aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}

/**
 * If any children require a block parent, return the first such child.
 * Otherwise return null.
 */
static nsIContent*
AnyKidsNeedBlockParent(nsIFrame *aFrameList)
{
  for (nsIFrame *k = aFrameList; k; k = k->GetNextSibling()) {
    // Line participants, such as text and inline frames, can't be
    // directly inside a XUL box; they must be wrapped in an
    // intermediate block.
    if (k->IsFrameOfType(nsIFrame::eLineParticipant)) {
      return k->GetContent();
    }
  }
  return nsnull;
}

// Reparent a frame into a wrapper frame that is a child of its old parent.
static void
ReparentFrame(nsFrameManager* aFrameManager,
              nsIFrame* aNewParentFrame,
              nsIFrame* aFrame)
{
  aFrame->SetParent(aNewParentFrame);
  aFrameManager->ReParentStyleContext(aFrame);
  if (aFrame->GetStateBits() &
      (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    // No need to walk up the tree, since the bits are already set
    // right on the parent of aNewParentFrame.
    NS_ASSERTION(aNewParentFrame->GetParent()->GetStateBits() &
                   NS_FRAME_HAS_CHILD_WITH_VIEW,
                 "aNewParentFrame's parent should have this bit set!");
    aNewParentFrame->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
}

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

static nsIFrame* GetSpecialSibling(nsIFrame* aFrame)
{
  // We only store the "special sibling" annotation with the first
  // frame in the continuation chain. Walk back to find that frame now.
  aFrame = aFrame->GetFirstContinuation();

  void* value = aFrame->GetProperty(nsGkAtoms::IBSplitSpecialSibling);

  return static_cast<nsIFrame*>(value);
}

static nsIFrame*
GetIBSplitSpecialPrevSiblingForAnonymousBlock(nsIFrame* aFrame)
{
  NS_PRECONDITION(IsFrameSpecial(aFrame) && !IsInlineFrame(aFrame),
                  "Shouldn't call this");
  
  // We only store the "special sibling" annotation with the first
  // frame in the continuation chain. Walk back to find that frame now.  
  return
    static_cast<nsIFrame*>
    (aFrame->GetFirstContinuation()->
       GetProperty(nsGkAtoms::IBSplitSpecialPrevSibling));
}

static nsIFrame*
GetLastSpecialSibling(nsIFrame* aFrame)
{
  for (nsIFrame *frame = aFrame, *next; ; frame = next) {
    next = GetSpecialSibling(frame);
    if (!next)
      return frame;
  }
  NS_NOTREACHED("unreachable code");
  return nsnull;
}

static void
SetFrameIsSpecial(nsIFrame* aFrame, nsIFrame* aSpecialSibling)
{
  NS_PRECONDITION(aFrame, "bad args!");

  // Mark the frame and all of its siblings as "special".
  for (nsIFrame* frame = aFrame; frame != nsnull; frame = frame->GetNextContinuation()) {
    frame->AddStateBits(NS_FRAME_IS_SPECIAL);
  }

  if (aSpecialSibling) {
    // We should be the first-in-flow
    NS_ASSERTION(!aFrame->GetPrevInFlow(),
                 "assigning special sibling to other than first-in-flow!");

    // Store the "special sibling" (if we were given one) with the
    // first frame in the flow.
    aFrame->SetProperty(nsGkAtoms::IBSplitSpecialSibling, aSpecialSibling);
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

    // Note that we ignore non-special frames which have a pseudo on their
    // style context -- they're not the frames we're looking for!  In
    // particular, they may be hiding a real parent that _is_ special.
    if (!IsFrameSpecial(parentFrame) &&
        !parentFrame->GetStyleContext()->GetPseudoType())
      break;

    aFrame = parentFrame;
  } while (1);
 
  // post-conditions
  NS_ASSERTION(parentFrame, "no normal ancestor found for special frame in GetIBContainingBlockFor");
  NS_ASSERTION(parentFrame != aFrame, "parentFrame is actually the child frame - bogus reslt");

  return parentFrame;
}

//----------------------------------------------------------------------

// Block/inline frame construction logic. We maintain a few invariants here:
//
// 1. Block frames contain block and inline frames.
//
// 2. Inline frames only contain inline frames. If an inline parent has a block
// child then the block child is migrated upward until it lands in a block
// parent (the inline frames containing block is where it will end up).

static nsIFrame*
FindFirstBlock(nsIFrame* aKid, nsIFrame** aPrevKid)
{
  nsIFrame* prevKid = nsnull;
  while (aKid) {
    if (!IsInlineOutside(aKid)) {
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
FindLastBlock(nsIFrame* aKid)
{
  nsIFrame* lastBlock = nsnull;
  while (aKid) {
    if (!IsInlineOutside(aKid)) {
      lastBlock = aKid;
    }
    aKid = aKid->GetNextSibling();
  }
  return lastBlock;
}

/*
 * The special-prev-sibling is useful for
 * finding the "special parent" of a frame (i.e., a frame from which a
 * good parent style context can be obtained), one looks at the
 * special previous sibling annotation of the real parent of the frame
 * (if the real parent has NS_FRAME_IS_SPECIAL).
 */
inline void
MarkIBSpecialPrevSibling(nsIFrame *aAnonymousFrame,
                         nsIFrame *aSpecialParent)
{
  aAnonymousFrame->SetProperty(nsGkAtoms::IBSplitSpecialPrevSibling,
                               aSpecialParent, nsnull, nsnull);
}

// -----------------------------------------------------------

static PRBool
IsOutOfFlowList(nsIAtom* aListName)
{
  return
    aListName == nsGkAtoms::floatList ||
    aListName == nsGkAtoms::absoluteList ||
    aListName == nsGkAtoms::overflowOutOfFlowList ||
    aListName == nsGkAtoms::fixedList;
}

// Helper function that recursively removes content to frame mappings and
// undisplayed content mappings.
// This differs from DeletingFrameSubtree() because the frames have not yet been
// added to the frame hierarchy.
// XXXbz it would really help if we merged the two methods somehow... :(
static void
DoCleanupFrameReferences(nsFrameManager*  aFrameManager,
                         nsIFrame*        aFrameIn)
{
  nsIContent* content = aFrameIn->GetContent();

  if (aFrameIn->GetType() == nsGkAtoms::placeholderFrame) {
    nsPlaceholderFrame* placeholder = static_cast<nsPlaceholderFrame*>
                                                 (aFrameIn);
    // if the frame is a placeholder use the out of flow frame
    aFrameIn = nsPlaceholderFrame::GetRealFrameForPlaceholder(placeholder);

    // And don't forget to unregister the placeholder mapping.  Note that this
    // means it's the caller's responsibility to actually destroy the
    // out-of-flow pointed to by the placeholder, since after this point the
    // out-of-flow is not reachable via the placeholder.
    aFrameManager->UnregisterPlaceholderFrame(placeholder);
  }

  // Remove the mapping from the content object to its frame
  aFrameManager->RemoveAsPrimaryFrame(content, aFrameIn);
  aFrameManager->ClearAllUndisplayedContentIn(content);

  // Recursively walk the child frames.
  nsIAtom* childListName = nsnull;
  PRInt32 childListIndex = 0;
  do {
    nsIFrame* childFrame = aFrameIn->GetFirstChild(childListName);
    while (childFrame) {
      DoCleanupFrameReferences(aFrameManager, childFrame);
    
      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }

    childListName = aFrameIn->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
}

// Helper function that walks a frame list and calls DoCleanupFrameReference()
static void
CleanupFrameReferences(nsFrameManager*  aFrameManager,
                       nsIFrame*        aFrameList)
{
  while (aFrameList) {
    DoCleanupFrameReferences(aFrameManager, aFrameList);

    // Get the sibling frame
    aFrameList = aFrameList->GetNextSibling();
  }
}

// -----------------------------------------------------------

// Structure used when constructing formatting object trees.
struct nsFrameItems {
  nsIFrame* childList;
  nsIFrame* lastChild;
  
  nsFrameItems(nsIFrame* aFrame = nsnull);

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);

  // Inserts the frame somewhere in the list
  void InsertChildAfter(nsIFrame* aChild, nsIFrame* aAfter);

  // Remove the frame from the list, return PR_FALSE if not found.  If
  // aPrevSibling is given, it must have aChild as its GetNextSibling().
  // aPrevSibling may be null to indicate that the list should be searched.
  PRBool RemoveChild(nsIFrame* aChild, nsIFrame* aPrevSibling);
};

nsFrameItems::nsFrameItems(nsIFrame* aFrame)
  : childList(aFrame), lastChild(aFrame)
{
}

void 
nsFrameItems::AddChild(nsIFrame* aChild)
{
#ifdef DEBUG
  nsIFrame* oldLastChild = lastChild;
#endif
  
  if (childList == nsnull) {
    childList = lastChild = aChild;
  }
  else
  {
    NS_ASSERTION(aChild != lastChild,
                 "Same frame being added to frame list twice?");
    lastChild->SetNextSibling(aChild);
    lastChild = aChild;
  }
  // if aChild has siblings, lastChild needs to be the last one
  for (nsIFrame* sib = lastChild->GetNextSibling(); sib;
       sib = sib->GetNextSibling()) {
    NS_ASSERTION(oldLastChild != sib, "Loop in frame list");
    lastChild = sib;
  }
}

void
nsFrameItems::InsertChildAfter(nsIFrame* aChild, nsIFrame* aAfter)
{
  if (!childList || (aAfter && !aAfter->GetNextSibling())) {
    // Appending to the end of the list
    AddChild(aChild);
    return;
  }
  if (!aAfter) {
    // Inserting at beginning of list
    aChild->SetNextSibling(childList);
    childList = aChild;
    return;
  }
  aChild->SetNextSibling(aAfter->GetNextSibling());
  aAfter->SetNextSibling(aChild);
}

PRBool
nsFrameItems::RemoveChild(nsIFrame* aFrame, nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nsIFrame* prev;
  if (aPrevSibling) {
    prev = aPrevSibling;
  } else {
    prev = nsnull;
    nsIFrame* sib;
    for (sib = childList; sib && sib != aFrame; sib = sib->GetNextSibling()) {
      prev = sib;
    }
    if (!sib) {
      return PR_FALSE;
    }
  }

  NS_ASSERTION(!prev || prev->GetNextSibling() == aFrame,
               "Unexpected prevsibling");

  if (aFrame == childList) {
    childList = aFrame->GetNextSibling();
  } else {
    prev->SetNextSibling(aFrame->GetNextSibling());
  }
  if (aFrame == lastChild) {
    lastChild = prev;
  }
  aFrame->SetNextSibling(nsnull);
  return PR_TRUE;
}

// -----------------------------------------------------------

// Structure used when constructing formatting object trees. Contains
// state information needed for absolutely positioned elements
struct nsAbsoluteItems : nsFrameItems {
  // containing block for absolutely positioned elements
  nsIFrame* containingBlock;
  
  nsAbsoluteItems(nsIFrame* aContainingBlock);
#ifdef DEBUG
  // XXXbz Does this need a debug-only assignment operator that nulls out the
  // childList in the nsAbsoluteItems we're copying?  Introducing a difference
  // between debug and non-debug behavior seems bad, so I guess not...
  ~nsAbsoluteItems() {
    NS_ASSERTION(!childList,
                 "Dangling child list.  Someone forgot to insert it?");
  }
#endif
  
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
  NS_ASSERTION(aChild->PresContext()->FrameManager()->
               GetPlaceholderFrameFor(aChild),
               "Child without placeholder being added to nsAbsoluteItems?");
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
#ifdef DEBUG
  void Dump();
#endif
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
#ifdef DEBUG
  void Dump();
#endif
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

#ifdef DEBUG
void
nsPseudoFrameData::Dump()
{
  nsIFrame* main = nsnull;
  nsIFrame* second = nsnull;
  printf("        %p\n", static_cast<void*>(mFrame));
  main = mChildList.childList;

 
  second = mChildList2.childList;
  while (main || second) {
    printf("          %p   %p\n", static_cast<void*>(main),
           static_cast<void*>(second));
    if (main)
      main = main->GetNextSibling();
    if (second)
      second = second->GetNextSibling();
  }
}
#endif
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

#ifdef DEBUG
void
nsPseudoFrames::Dump()
{
  if (IsEmpty()) {
    // check that it is really empty, warn otherwise
    NS_ASSERTION(!mTableOuter.mFrame,    "Pseudo Outer Table Frame not empty");
    NS_ASSERTION(!mTableOuter.mChildList.childList, "Pseudo Outer Table Frame has primary children");
    NS_ASSERTION(!mTableOuter.mChildList2.childList,"Pseudo Outer Table Frame has secondary children");
    NS_ASSERTION(!mTableInner.mFrame,    "Pseudo Inner Table Frame not empty");
    NS_ASSERTION(!mTableInner.mChildList.childList, "Pseudo Inner Table Frame has primary children");
    NS_ASSERTION(!mTableInner.mChildList2.childList,"Pseudo Inner Table Frame has secondary children");
    NS_ASSERTION(!mColGroup.mFrame,      "Pseudo Colgroup Frame not empty");
    NS_ASSERTION(!mColGroup.mChildList.childList,   "Pseudo Colgroup Table Frame has primary children");
    NS_ASSERTION(!mColGroup.mChildList2.childList,  "Pseudo Colgroup Table Frame has secondary children");
    NS_ASSERTION(!mRowGroup.mFrame,      "Pseudo Rowgroup Frame not empty");
    NS_ASSERTION(!mRowGroup.mChildList.childList,   "Pseudo Rowgroup Frame has primary children");
    NS_ASSERTION(!mRowGroup.mChildList2.childList,  "Pseudo Rowgroup Frame has secondary children");
    NS_ASSERTION(!mRow.mFrame,           "Pseudo Row Frame not empty");
    NS_ASSERTION(!mRow.mChildList.childList,        "Pseudo Row Frame has primary children");
    NS_ASSERTION(!mRow.mChildList2.childList,       "Pseudo Row Frame has secondary children");
    NS_ASSERTION(!mCellOuter.mFrame,     "Pseudo Outer Cell Frame not empty");
    NS_ASSERTION(!mCellOuter.mChildList.childList,  "Pseudo Outer Cell Frame has primary children");
    NS_ASSERTION(!mCellOuter.mChildList2.childList, "Pseudo Outer Cell Frame has secondary children");
    NS_ASSERTION(!mCellInner.mFrame,     "Pseudo Inner Cell Frame not empty");
    NS_ASSERTION(!mCellInner.mChildList.childList,  "Pseudo Inner Cell Frame has primary children");
    NS_ASSERTION(!mCellInner.mChildList2.childList, "Pseudo inner Cell Frame has secondary children");
  }
  else {
    if (mTableOuter.mFrame || mTableOuter.mChildList.childList || mTableOuter.mChildList2.childList) {
      if (nsGkAtoms::tableOuterFrame == mLowestType) {
        printf("LOW OuterTable\n");
      }
      else {
        printf("    OuterTable\n");
      }
      mTableOuter.Dump();
    }
    if (mTableInner.mFrame || mTableInner.mChildList.childList || mTableInner.mChildList2.childList) {
      if (nsGkAtoms::tableFrame == mLowestType) {
        printf("LOW InnerTable\n");
      }
      else {
        printf("    InnerTable\n");
      }
      mTableInner.Dump();
    }
    if (mColGroup.mFrame || mColGroup.mChildList.childList || mColGroup.mChildList2.childList) {
      if (nsGkAtoms::tableColGroupFrame == mLowestType) {
        printf("LOW ColGroup\n");
      }
      else {
        printf("    ColGroup\n");
      }
      mColGroup.Dump();
    }
    if (mRowGroup.mFrame || mRowGroup.mChildList.childList || mRowGroup.mChildList2.childList) {
      if (nsGkAtoms::tableRowGroupFrame == mLowestType) {
        printf("LOW RowGroup\n");
      }
      else {
        printf("    RowGroup\n");
      }
      mRowGroup.Dump();
    }
    if (mRow.mFrame || mRow.mChildList.childList || mRow.mChildList2.childList) {
      if (nsGkAtoms::tableRowFrame == mLowestType) {
        printf("LOW Row\n");
      }
      else {
        printf("    Row\n");
      }
      mRow.Dump();
    }
    
    if (mCellOuter.mFrame || mCellOuter.mChildList.childList || mCellOuter.mChildList2.childList) {
      if (IS_TABLE_CELL(mLowestType)) {
        printf("LOW OuterCell\n");
      }
      else {
        printf("    OuterCell\n");
      }
      mCellOuter.Dump();
    }
    if (mCellInner.mFrame || mCellInner.mChildList.childList || mCellInner.mChildList2.childList) {
      printf("    InnerCell\n");
      mCellInner.Dump();
    }
  }
}
#endif
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
  PRBool*          mFixedPosIsAbsPos;

  nsAbsoluteItems  mSavedItems;           // copy of original data
  PRBool           mSavedFirstLetterStyle;
  PRBool           mSavedFirstLineStyle;
  PRBool           mSavedFixedPosIsAbsPos;

  // The name of the child list in which our frames would belong
  nsIAtom* mChildListName;
  nsFrameConstructorState* mState;

  friend class nsFrameConstructorState;
};

// Structure used for maintaining state information during the
// frame construction process
class NS_STACK_CLASS nsFrameConstructorState {
public:
  nsPresContext            *mPresContext;
  nsIPresShell             *mPresShell;
  nsFrameManager           *mFrameManager;

#ifdef MOZ_XUL
  // The root box, if any.
  nsIRootBox*               mRootBox;
  // Frames destined for the nsGkAtoms::popupList.
  nsAbsoluteItems           mPopupItems;
#endif

  // Containing block information for out-of-flow frames.
  nsAbsoluteItems           mFixedItems;
  nsAbsoluteItems           mAbsoluteItems;
  nsAbsoluteItems           mFloatedItems;
  PRBool                    mFirstLetterStyle;
  PRBool                    mFirstLineStyle;

  // When working with the -moz-transform property, we want to hook
  // the abs-pos and fixed-pos lists together, since transformed
  // elements are fixed-pos containing blocks.  This flag determines
  // whether or not we want to wire the fixed-pos and abs-pos lists
  // together.
  PRBool                    mFixedPosIsAbsPos;

  nsCOMPtr<nsILayoutHistoryState> mFrameState;
  nsPseudoFrames            mPseudoFrames;
  // These bits will be added to the state bits of any frame we construct
  // using this state.
  nsFrameState              mAdditionalStateBits; 

  // Constructor
  // Use the passed-in history state.
  nsFrameConstructorState(nsIPresShell*          aPresShell,
                          nsIFrame*              aFixedContainingBlock,
                          nsIFrame*              aAbsoluteContainingBlock,
                          nsIFrame*              aFloatContainingBlock,
                          nsILayoutHistoryState* aHistoryState);
  // Get the history state from the pres context's pres shell.
  nsFrameConstructorState(nsIPresShell*          aPresShell,
                          nsIFrame*              aFixedContainingBlock,
                          nsIFrame*              aAbsoluteContainingBlock,
                          nsIFrame*              aFloatContainingBlock);

  ~nsFrameConstructorState();
  
  // Function to push the existing absolute containing block state and
  // create a new scope. Code that uses this function should get matching
  // logic in GetAbsoluteContainingBlock.
  void PushAbsoluteContainingBlock(nsIFrame* aNewAbsoluteContainingBlock,
                                   nsFrameConstructorSaveState& aSaveState);

  // Function to push the existing float containing block state and
  // create a new scope. Code that uses this function should get matching
  // logic in GetFloatContainingBlock.
  // Pushing a null float containing block forbids any frames from being
  // floated until a new float containing block is pushed.
  // XXX we should get rid of null float containing blocks and teach the
  // various frame classes to deal with floats instead.
  void PushFloatContainingBlock(nsIFrame* aNewFloatContainingBlock,
                                nsFrameConstructorSaveState& aSaveState,
                                PRBool aFirstLetterStyle,
                                PRBool aFirstLineStyle);

  // Function to return the proper geometric parent for a frame with display
  // struct given by aStyleDisplay and parent's frame given by
  // aContentParentFrame.  If the frame is not allowed to be positioned, pass
  // false for aCanBePositioned.
  nsIFrame* GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                               nsIFrame* aContentParentFrame);

  /**
   * Function to add a new frame to the right frame list.  This MUST be called
   * on frames before their children have been processed if the frames might
   * conceivably be out-of-flow; otherwise cleanup in error cases won't work
   * right.  Also, this MUST be called on frames after they have been
   * initialized.
   * @param aNewFrame the frame to add
   * @param aFrameItems the list to add in-flow frames to
   * @param aContent the content pointer for aNewFrame
   * @param aStyleContext the style context resolved for aContent
   * @param aParentFrame the parent frame for the content if it were in-flow
   * @param aCanBePositioned pass false if the frame isn't allowed to be
   *        positioned
   * @param aCanBeFloated pass false if the frame isn't allowed to be
   *        floated
   * @param aIsOutOfFlowPopup pass true if the frame is an out-of-flow popup
   *        (XUL-only)
   * @throws NS_ERROR_OUT_OF_MEMORY if it happens.
   * @note If this method throws, that means that aNewFrame was not inserted
   *       into any frame lists.  Furthermore, this method will handle cleanup
   *       of aNewFrame (via calling CleanupFrameReferences() and Destroy() on
   *       it).
   */
  nsresult AddChild(nsIFrame* aNewFrame,
                    nsFrameItems& aFrameItems,
                    nsIContent* aContent,
                    nsStyleContext* aStyleContext,
                    nsIFrame* aParentFrame,
                    PRBool aCanBePositioned = PR_TRUE,
                    PRBool aCanBeFloated = PR_TRUE,
                    PRBool aIsOutOfFlowPopup = PR_FALSE,
                    PRBool aInsertAfter = PR_FALSE,
                    nsIFrame* aInsertAfterFrame = nsnull);

  /**
   * Function to return the fixed-pos element list.  Normally this will just hand back the
   * fixed-pos element list, but in case we're dealing with a transformed element that's
   * acting as an abs-pos and fixed-pos container, we'll hand back the abs-pos list.  Callers should
   * use this function if they want to get the list acting as the fixed-pos item parent.
   */
  nsAbsoluteItems& GetFixedItems()
  {
    return mFixedPosIsAbsPos ? mAbsoluteItems : mFixedItems;
  }
  const nsAbsoluteItems& GetFixedItems() const
  {
    return mFixedPosIsAbsPos ? mAbsoluteItems : mFixedItems;
  }

protected:
  friend class nsFrameConstructorSaveState;

  /**
   * ProcessFrameInsertions takes the frames in aFrameItems and adds them as
   * kids to the aChildListName child list of |aFrameItems.containingBlock|.
   */
  void ProcessFrameInsertions(nsAbsoluteItems& aFrameItems,
                              nsIAtom* aChildListName);
};

nsFrameConstructorState::nsFrameConstructorState(nsIPresShell*          aPresShell,
                                                 nsIFrame*              aFixedContainingBlock,
                                                 nsIFrame*              aAbsoluteContainingBlock,
                                                 nsIFrame*              aFloatContainingBlock,
                                                 nsILayoutHistoryState* aHistoryState)
  : mPresContext(aPresShell->GetPresContext()),
    mPresShell(aPresShell),
    mFrameManager(aPresShell->FrameManager()),
#ifdef MOZ_XUL    
    mRootBox(nsIRootBox::GetRootBox(aPresShell)),
    mPopupItems(mRootBox ? mRootBox->GetPopupSetFrame() : nsnull),
#endif
    mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloatContainingBlock),
    mFirstLetterStyle(PR_FALSE),
    mFirstLineStyle(PR_FALSE),
    // See PushAbsoluteContaningBlock below
    mFixedPosIsAbsPos(aAbsoluteContainingBlock &&
                      aAbsoluteContainingBlock->GetStyleDisplay()->
                        HasTransform()),
    mFrameState(aHistoryState),
    mPseudoFrames(),
    mAdditionalStateBits(0)
{
  MOZ_COUNT_CTOR(nsFrameConstructorState);
}

nsFrameConstructorState::nsFrameConstructorState(nsIPresShell* aPresShell,
                                                 nsIFrame*     aFixedContainingBlock,
                                                 nsIFrame*     aAbsoluteContainingBlock,
                                                 nsIFrame*     aFloatContainingBlock)
  : mPresContext(aPresShell->GetPresContext()),
    mPresShell(aPresShell),
    mFrameManager(aPresShell->FrameManager()),
#ifdef MOZ_XUL    
    mRootBox(nsIRootBox::GetRootBox(aPresShell)),
    mPopupItems(mRootBox ? mRootBox->GetPopupSetFrame() : nsnull),
#endif
    mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloatContainingBlock),
    mFirstLetterStyle(PR_FALSE),
    mFirstLineStyle(PR_FALSE),
    // See PushAbsoluteContaningBlock below
    mFixedPosIsAbsPos(aAbsoluteContainingBlock &&
                      aAbsoluteContainingBlock->GetStyleDisplay()->
                        HasTransform()),
    mPseudoFrames(),
    mAdditionalStateBits(0)
{
  MOZ_COUNT_CTOR(nsFrameConstructorState);
  mFrameState = aPresShell->GetDocument()->GetLayoutHistoryState();
}

nsFrameConstructorState::~nsFrameConstructorState()
{
  // Frame order comparison functions only work properly when the placeholders
  // have been inserted into the frame tree. So for example if we have a new float
  // containing the placeholder for a new abs-pos frame, and we process the abs-pos
  // insertion first, then we won't be able to find the right place to insert in
  // in the abs-pos list. So put floats in first, because they can contain placeholders
  // for abs-pos and fixed-pos items whose containing blocks are outside the floats.
  // Then put abs-pos frames in, because they can contain placeholders for fixed-pos
  // items whose containing block is outside the abs-pos frames. 
  MOZ_COUNT_DTOR(nsFrameConstructorState);
  ProcessFrameInsertions(mFloatedItems, nsGkAtoms::floatList);
  ProcessFrameInsertions(mAbsoluteItems, nsGkAtoms::absoluteList);
  ProcessFrameInsertions(mFixedItems, nsGkAtoms::fixedList);
#ifdef MOZ_XUL
  ProcessFrameInsertions(mPopupItems, nsGkAtoms::popupList);
#endif
}

static nsIFrame*
AdjustAbsoluteContainingBlock(nsIFrame* aContainingBlockIn)
{
  if (!aContainingBlockIn) {
    return nsnull;
  }
  
  // Always use the container's first continuation. (Inline frames can have
  // non-fluid bidi continuations...)
  return aContainingBlockIn->GetFirstContinuation();
}

void
nsFrameConstructorState::PushAbsoluteContainingBlock(nsIFrame* aNewAbsoluteContainingBlock,
                                                     nsFrameConstructorSaveState& aSaveState)
{
  aSaveState.mItems = &mAbsoluteItems;
  aSaveState.mSavedItems = mAbsoluteItems;
  aSaveState.mChildListName = nsGkAtoms::absoluteList;
  aSaveState.mState = this;

  /* Store whether we're wiring the abs-pos and fixed-pos lists together. */
  aSaveState.mFixedPosIsAbsPos = &mFixedPosIsAbsPos;
  aSaveState.mSavedFixedPosIsAbsPos = mFixedPosIsAbsPos;

  mAbsoluteItems = 
    nsAbsoluteItems(AdjustAbsoluteContainingBlock(aNewAbsoluteContainingBlock));

  /* See if we're wiring the fixed-pos and abs-pos lists together.  This happens iff
   * we're a transformed element.
   */
  mFixedPosIsAbsPos = (aNewAbsoluteContainingBlock &&
                       aNewAbsoluteContainingBlock->GetStyleDisplay()->HasTransform());
}

void
nsFrameConstructorState::PushFloatContainingBlock(nsIFrame* aNewFloatContainingBlock,
                                                  nsFrameConstructorSaveState& aSaveState,
                                                  PRBool aFirstLetterStyle,
                                                  PRBool aFirstLineStyle)
{
  // XXXbz we should probably just be able to assert that
  // aNewFloatContainingBlock is a float containing block... see XXX comment at
  // the top of ProcessChildren.
  NS_PRECONDITION(!aNewFloatContainingBlock ||
                  aNewFloatContainingBlock->GetContentInsertionFrame()->
                    IsFloatContainingBlock(),
                  "Please push a real float containing block!");
  aSaveState.mItems = &mFloatedItems;
  aSaveState.mFirstLetterStyle = &mFirstLetterStyle;
  aSaveState.mFirstLineStyle = &mFirstLineStyle;
  aSaveState.mSavedItems = mFloatedItems;
  aSaveState.mSavedFirstLetterStyle = mFirstLetterStyle;
  aSaveState.mSavedFirstLineStyle = mFirstLineStyle;
  aSaveState.mChildListName = nsGkAtoms::floatList;
  aSaveState.mState = this;
  mFloatedItems = nsAbsoluteItems(aNewFloatContainingBlock);
  mFirstLetterStyle = aFirstLetterStyle;
  mFirstLineStyle = aFirstLineStyle;
}

nsIFrame*
nsFrameConstructorState::GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                                            nsIFrame* aContentParentFrame)
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

  if (aStyleDisplay->mPosition == NS_STYLE_POSITION_ABSOLUTE &&
      mAbsoluteItems.containingBlock) {
    return mAbsoluteItems.containingBlock;
  }

  if (aStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED &&
      GetFixedItems().containingBlock) {
    return GetFixedItems().containingBlock;
  }

  return aContentParentFrame;
}

nsresult
nsFrameConstructorState::AddChild(nsIFrame* aNewFrame,
                                  nsFrameItems& aFrameItems,
                                  nsIContent* aContent,
                                  nsStyleContext* aStyleContext,
                                  nsIFrame* aParentFrame,
                                  PRBool aCanBePositioned,
                                  PRBool aCanBeFloated,
                                  PRBool aIsOutOfFlowPopup,
                                  PRBool aInsertAfter,
                                  nsIFrame* aInsertAfterFrame)
{
  const nsStyleDisplay* disp = aNewFrame->GetStyleDisplay();
  
  // The comments in GetGeometricParent regarding root table frames
  // all apply here, unfortunately.

  PRBool needPlaceholder = PR_FALSE;
  nsFrameItems* frameItems = &aFrameItems;
#ifdef MOZ_XUL
  if (NS_UNLIKELY(aIsOutOfFlowPopup)) {
      NS_ASSERTION(aNewFrame->GetParent() == mPopupItems.containingBlock,
                   "Popup whose parent is not the popup containing block?");
      NS_ASSERTION(mPopupItems.containingBlock, "Must have a popup set frame!");
      needPlaceholder = PR_TRUE;
      frameItems = &mPopupItems;
  }
  else
#endif // MOZ_XUL
  if (aCanBeFloated && disp->IsFloating() &&
      mFloatedItems.containingBlock) {
    NS_ASSERTION(aNewFrame->GetParent() == mFloatedItems.containingBlock,
                 "Float whose parent is not the float containing block?");
    needPlaceholder = PR_TRUE;
    frameItems = &mFloatedItems;
  }
  else if (aCanBePositioned) {
    if (disp->mPosition == NS_STYLE_POSITION_ABSOLUTE &&
        mAbsoluteItems.containingBlock) {
      NS_ASSERTION(aNewFrame->GetParent() == mAbsoluteItems.containingBlock,
                   "Abs pos whose parent is not the abs pos containing block?");
      needPlaceholder = PR_TRUE;
      frameItems = &mAbsoluteItems;
    }
    if (disp->mPosition == NS_STYLE_POSITION_FIXED &&
        GetFixedItems().containingBlock) {
      NS_ASSERTION(aNewFrame->GetParent() == GetFixedItems().containingBlock,
                   "Fixed pos whose parent is not the fixed pos containing block?");
      needPlaceholder = PR_TRUE;
      frameItems = &GetFixedItems();
    }
  }

  if (needPlaceholder) {
    NS_ASSERTION(frameItems != &aFrameItems,
                 "Putting frame in-flow _and_ want a placeholder?");
    nsIFrame* placeholderFrame;
    nsresult rv =
      nsCSSFrameConstructor::CreatePlaceholderFrameFor(mPresShell,
                                                       aContent,
                                                       aNewFrame,
                                                       aStyleContext,
                                                       aParentFrame,
                                                       nsnull,
                                                       &placeholderFrame);
    if (NS_FAILED(rv)) {
      // Note that aNewFrame could be the top frame for a scrollframe setup,
      // hence already set as the primary frame.  So we have to clean up here.
      // But it shouldn't have any out-of-flow kids.
      // XXXbz Maybe add a utility function to assert that?
      CleanupFrameReferences(mFrameManager, aNewFrame);
      aNewFrame->Destroy();
      return rv;
    }

    placeholderFrame->AddStateBits(mAdditionalStateBits);
    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(aNewFrame->GetParent() == aParentFrame,
                 "In-flow frame has wrong parent");
  }
#endif

  if (aInsertAfter) {
    frameItems->InsertChildAfter(aNewFrame, aInsertAfterFrame);
  } else {
    frameItems->AddChild(aNewFrame);
  }

  // Now add the special siblings too.
  nsIFrame* specialSibling = aNewFrame;
  while (specialSibling && IsFrameSpecial(specialSibling)) {
    specialSibling = GetSpecialSibling(specialSibling);
    if (specialSibling) {
      NS_ASSERTION(frameItems == &aFrameItems,
                   "IB split ending up in an out-of-flow childlist?");
      frameItems->AddChild(specialSibling);
    }
  }
  
  return NS_OK;
}

void
nsFrameConstructorState::ProcessFrameInsertions(nsAbsoluteItems& aFrameItems,
                                                nsIAtom* aChildListName)
{
#define NS_NONXUL_LIST_TEST (&aFrameItems == &mFloatedItems &&             \
                             aChildListName == nsGkAtoms::floatList)    || \
                            (&aFrameItems == &mAbsoluteItems &&            \
                             aChildListName == nsGkAtoms::absoluteList) || \
                            (&aFrameItems == &mFixedItems &&               \
                             aChildListName == nsGkAtoms::fixedList)
#ifdef MOZ_XUL
  NS_PRECONDITION(NS_NONXUL_LIST_TEST ||
                  (&aFrameItems == &mPopupItems &&
                   aChildListName == nsGkAtoms::popupList), 
                  "Unexpected aFrameItems/aChildListName combination");
#else
  NS_PRECONDITION(NS_NONXUL_LIST_TEST,
                  "Unexpected aFrameItems/aChildListName combination");
#endif

  nsIFrame* firstNewFrame = aFrameItems.childList;
  
  if (!firstNewFrame) {
    return;
  }
  
  nsIFrame* containingBlock = aFrameItems.containingBlock;

  NS_ASSERTION(containingBlock,
               "Child list without containing block?");
  
  // Insert the frames hanging out in aItems.  We can use SetInitialChildList()
  // if the containing block hasn't been reflown yet (so NS_FRAME_FIRST_REFLOW
  // is set) and doesn't have any frames in the aChildListName child list yet.
  nsIFrame* firstChild = containingBlock->GetFirstChild(aChildListName);
  nsresult rv = NS_OK;
  if (!firstChild && (containingBlock->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    rv = containingBlock->SetInitialChildList(aChildListName, firstNewFrame);
  } else {
    // Note that whether the frame construction context is doing an append or
    // not is not helpful here, since it could be appending to some frame in
    // the middle of the document, which means we're not necessarily
    // appending to the children of the containing block.
    //
    // We need to make sure the 'append to the end of document' case is fast.
    // So first test the last child of the containing block
    nsIFrame* lastChild = nsLayoutUtils::GetLastSibling(firstChild);

    // CompareTreePosition uses placeholder hierarchy for out of flow frames,
    // so this will make out-of-flows respect the ordering of placeholders,
    // which is great because it takes care of anonymous content.
    if (!lastChild ||
        nsLayoutUtils::CompareTreePosition(lastChild, firstNewFrame, containingBlock) < 0) {
      // no lastChild, or lastChild comes before the new children, so just append
      rv = containingBlock->AppendFrames(aChildListName, firstNewFrame);
    } else {
      nsIFrame* insertionPoint = nsnull;
      // try the other children
      for (nsIFrame* f = firstChild; f != lastChild; f = f->GetNextSibling()) {
        PRInt32 compare =
          nsLayoutUtils::CompareTreePosition(f, firstNewFrame, containingBlock);
        if (compare > 0) {
          // f comes after the new children, so stop here and insert after
          // the previous frame
          break;
        }
        insertionPoint = f;
      }

      rv = containingBlock->InsertFrames(aChildListName, insertionPoint,
                                         firstNewFrame);
    }
  }
  aFrameItems.childList = nsnull;
  // XXXbz And if NS_FAILED(rv), what?  I guess we need to clean up the list
  // and deal with all the placeholders... but what if the placeholders aren't
  // in the document yet?  Could that happen?
  NS_ASSERTION(NS_SUCCEEDED(rv), "Frames getting lost!");
}


nsFrameConstructorSaveState::nsFrameConstructorSaveState()
  : mItems(nsnull),
    mFirstLetterStyle(nsnull),
    mFirstLineStyle(nsnull),
    mFixedPosIsAbsPos(nsnull),
    mSavedItems(nsnull),
    mSavedFirstLetterStyle(PR_FALSE),
    mSavedFirstLineStyle(PR_FALSE),
    mSavedFixedPosIsAbsPos(PR_FALSE),
    mChildListName(nsnull),
    mState(nsnull)
{
}

nsFrameConstructorSaveState::~nsFrameConstructorSaveState()
{
  // Restore the state
  if (mItems) {
    NS_ASSERTION(mState, "Can't have mItems set without having a state!");
    mState->ProcessFrameInsertions(*mItems, mChildListName);
    *mItems = mSavedItems;
#ifdef DEBUG
    // We've transferred the child list, so drop the pointer we held to it.
    // Note that this only matters for the assert in ~nsAbsoluteItems.
    mSavedItems.childList = nsnull;
#endif
  }
  if (mFirstLetterStyle) {
    *mFirstLetterStyle = mSavedFirstLetterStyle;
  }
  if (mFirstLineStyle) {
    *mFirstLineStyle = mSavedFirstLineStyle;
  }
  if (mFixedPosIsAbsPos) {
    *mFixedPosIsAbsPos = mSavedFixedPosIsAbsPos;
  }
}

static 
PRBool IsBorderCollapse(nsIFrame* aFrame)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (nsGkAtoms::tableFrame == frame->GetType()) {
      return ((nsTableFrame*)frame)->IsBorderCollapse();
    }
  }
  NS_ASSERTION(PR_FALSE, "program error");
  return PR_FALSE;
}

/**
 * Utility method, called from MoveChildrenTo(), that recursively
 * descends down the frame hierarchy looking for floating frames that
 * need parent pointer adjustments to account for the containment block
 * changes that could occur as the result of the reparenting done in
 * MoveChildrenTo().
 */
static void
AdjustFloatParentPtrs(nsIFrame*                aFrame,
                      nsFrameConstructorState& aState,
                      nsFrameConstructorState& aOuterState)
{
  NS_PRECONDITION(aFrame, "must have frame to work with");

  nsIFrame *outOfFlowFrame = nsPlaceholderFrame::GetRealFrameFor(aFrame);
  if (outOfFlowFrame != aFrame) {
    if (outOfFlowFrame->GetStyleDisplay()->IsFloating()) {
      // Update the parent pointer for outOfFlowFrame since its
      // containing block has changed as the result of reparenting
      // and move it from the outer state to the inner, bug 307277.
      
      nsIFrame *parent = aState.mFloatedItems.containingBlock;
      NS_ASSERTION(parent, "Should have float containing block here!");
      NS_ASSERTION(outOfFlowFrame->GetParent() == aOuterState.mFloatedItems.containingBlock,
                   "expected the float to be a child of the outer CB");

      if (aOuterState.mFloatedItems.RemoveChild(outOfFlowFrame, nsnull)) {
        aState.mFloatedItems.AddChild(outOfFlowFrame);
      } else {
        NS_NOTREACHED("float wasn't in the outer state float list");
      }

      outOfFlowFrame->SetParent(parent);
      if (outOfFlowFrame->GetStateBits() &
          (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW)) {
        // We don't need to walk up the tree, since we're doing this
        // recursively.
        parent->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
      }
    }

    // All out-of-flows are automatically float containing blocks, so we're
    // done here.
    return;
  }

  if (aFrame->IsFloatContainingBlock()) {
    // No need to recurse further; floats whose placeholders are
    // inside a block already have the right parent.
    return;
  }

  // Dive down into children to see if any of their
  // placeholders need adjusting.
  nsIFrame *childFrame = aFrame->GetFirstChild(nsnull);
  while (childFrame) {
    // XXX_kin: Do we need to prevent descent into anonymous content here?

    AdjustFloatParentPtrs(childFrame, aState, aOuterState);
    childFrame = childFrame->GetNextSibling();
  }
}

/**
 * Moves frames to a new parent, updating the style context and propagating
 * relevant frame state bits. |aState| may be null, in which case the parent
 * pointers of out-of-flow frames will remain untouched.
 */
static void
MoveChildrenTo(nsFrameManager*          aFrameManager,
               nsIFrame*                aNewParent,
               nsIFrame*                aFrameList,
               nsIFrame*                aFrameListEnd,
               nsFrameConstructorState* aState,
               nsFrameConstructorState* aOuterState)
{
  PRBool setHasChildWithView = PR_FALSE;

  while (aFrameList && aFrameList != aFrameListEnd) {
    if (!setHasChildWithView
        && (aFrameList->GetStateBits() & (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW))) {
      setHasChildWithView = PR_TRUE;
    }

    aFrameList->SetParent(aNewParent);

    // If aState is not null, the caller expects us to make adjustments so that
    // floats whose placeholders are descendants of frames in aFrameList point
    // to the correct parent.
    if (aState) {
      NS_ASSERTION(aOuterState, "need an outer state too");
      AdjustFloatParentPtrs(aFrameList, *aState, *aOuterState);
    }

    aFrameList = aFrameList->GetNextSibling();
  }

  if (setHasChildWithView) {
    do {
      aNewParent->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
      aNewParent = aNewParent->GetParent();
    } while (aNewParent &&
             !(aNewParent->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW));
  }
}

// -----------------------------------------------------------


// Structure used to ensure that bindings are properly enqueued in the
// binding manager's attached queue.
struct NS_STACK_CLASS nsAutoEnqueueBinding
{
  nsAutoEnqueueBinding(nsIDocument* aDocument) :
    mDocument(aDocument)
  {}

  ~nsAutoEnqueueBinding();

  nsRefPtr<nsXBLBinding> mBinding;
private:
  nsIDocument* mDocument;
};

nsAutoEnqueueBinding::~nsAutoEnqueueBinding()
{
  if (mBinding) {
    mDocument->BindingManager()->AddToAttachedQueue(mBinding);
  }
}


// Helper function that determines the child list name that aChildFrame
// is contained in
static nsIAtom*
GetChildListNameFor(nsIFrame*       aChildFrame)
{
  nsIAtom*      listName;

  if (aChildFrame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
    listName = nsGkAtoms::overflowContainersList;
  }
  // See if the frame is moved out of the flow
  else if (aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStyleDisplay* disp = aChildFrame->GetStyleDisplay();
    
    if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
      listName = nsGkAtoms::absoluteList;
    } else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
      if (nsLayoutUtils::IsReallyFixedPos(aChildFrame)) {
        listName = nsGkAtoms::fixedList;
      } else {
        listName = nsGkAtoms::absoluteList;
      }
#ifdef MOZ_XUL
    } else if (NS_STYLE_DISPLAY_POPUP == disp->mDisplay) {
      // Out-of-flows that are DISPLAY_POPUP must be kids of the root popup set
#ifdef DEBUG
      nsIFrame* parent = aChildFrame->GetParent();
      NS_ASSERTION(parent && parent->GetType() == nsGkAtoms::popupSetFrame,
                   "Unexpected parent");
#endif // DEBUG

      // XXX FIXME: Bug 350740
      // Return here, because the postcondition for this function actually
      // fails for this case, since the popups are not in a "real" frame list
      // in the popup set.
      return nsGkAtoms::popupList;      
#endif // MOZ_XUL
    } else {
      NS_ASSERTION(aChildFrame->GetStyleDisplay()->IsFloating(),
                   "not a floated frame");
      listName = nsGkAtoms::floatList;
    }

  } else {
    listName = nsnull;
  }

#ifdef NS_DEBUG
  // Verify that the frame is actually in that child list or in the
  // corresponding overflow list.
  nsIFrame* parent = aChildFrame->GetParent();
  PRBool found = nsFrameList(parent->GetFirstChild(listName))
                   .ContainsFrame(aChildFrame);
  if (!found) {
    if (!(aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
      found = nsFrameList(parent->GetFirstChild(nsGkAtoms::overflowList))
                .ContainsFrame(aChildFrame);
    }
    else if (aChildFrame->GetStyleDisplay()->IsFloating()) {
      found = nsFrameList(parent->GetFirstChild(nsGkAtoms::overflowOutOfFlowList))
                .ContainsFrame(aChildFrame);
    }
    // else it's positioned and should have been on the 'listName' child list.
    NS_POSTCONDITION(found, "not in child list");
  }
#endif

  return listName;
}

//----------------------------------------------------------------------

nsCSSFrameConstructor::nsCSSFrameConstructor(nsIDocument *aDocument,
                                             nsIPresShell *aPresShell)
  : mDocument(aDocument)
  , mPresShell(aPresShell)
  , mInitialContainingBlock(nsnull)
  , mRootElementStyleFrame(nsnull)
  , mFixedContainingBlock(nsnull)
  , mDocElementContainingBlock(nsnull)
  , mGfxScrollFrame(nsnull)
  , mPageSequenceFrame(nsnull)
  , mUpdateCount(0)
  , mFocusSuppressCount(0)
  , mQuotesDirty(PR_FALSE)
  , mCountersDirty(PR_FALSE)
  , mIsDestroyingFrameTree(PR_FALSE)
  , mRebuildAllStyleData(PR_FALSE)
  , mHasRootAbsPosContainingBlock(PR_FALSE)
  , mHoverGeneration(0)
  , mRebuildAllExtraHint(nsChangeHint(0))
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
nsCSSFrameConstructor::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while destroying frames");

  if (aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT) {
    if (mQuoteList.DestroyNodesFor(aFrame))
      QuotesDirty();
  }

  if (mCounterManager.DestroyNodesFor(aFrame)) {
    // Technically we don't need to update anything if we destroyed only
    // USE nodes.  However, this is unlikely to happen in the real world
    // since USE nodes generally go along with INCREMENT nodes.
    CountersDirty();
  }
}

struct nsGenConInitializer {
  nsAutoPtr<nsGenConNode> mNode;
  nsGenConList*           mList;
  void (nsCSSFrameConstructor::*mDirtyAll)();
  
  nsGenConInitializer(nsGenConNode* aNode, nsGenConList* aList,
                      void (nsCSSFrameConstructor::*aDirtyAll)())
    : mNode(aNode), mList(aList), mDirtyAll(aDirtyAll) {}
};

static void
DestroyGenConInitializer(void*    aFrame,
                         nsIAtom* aPropertyName,
                         void*    aPropertyValue,
                         void*    aDtorData)
{
  delete static_cast<nsGenConInitializer*>(aPropertyValue);
}

already_AddRefed<nsIContent>
nsCSSFrameConstructor::CreateGenConTextNode(const nsString& aString,
                                            nsCOMPtr<nsIDOMCharacterData>* aText,
                                            nsGenConInitializer* aInitializer)
{
  nsCOMPtr<nsIContent> content;
  NS_NewTextNode(getter_AddRefs(content), mDocument->NodeInfoManager());
  if (!content) {
    // XXX The quotes/counters code doesn't like the text pointer
    // being null in case of dynamic changes!
    NS_ASSERTION(!aText, "this OOM case isn't handled very well");
    return nsnull;
  }
  content->SetText(aString, PR_FALSE);
  if (aText) {
    *aText = do_QueryInterface(content);
  }
  if (aInitializer) {
    content->SetProperty(nsGkAtoms::genConInitializerProperty, aInitializer,
                         DestroyGenConInitializer);
  }
  return content.forget();
}

already_AddRefed<nsIContent>
nsCSSFrameConstructor::CreateGeneratedContent(nsIContent*     aParentContent,
                                              nsStyleContext* aStyleContext,
                                              PRUint32        aContentIndex)
{
  // Get the content value
  const nsStyleContentData &data =
    aStyleContext->GetStyleContent()->ContentAt(aContentIndex);
  nsStyleContentType type = data.mType;

  if (eStyleContentType_Image == type) {
    if (!data.mContent.mImage) {
      // CSS had something specified that couldn't be converted to an
      // image object
      return nsnull;
    }
    
    // Create an image content object and pass it the image request.
    // XXX Check if it's an image type we can handle...

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfo = mDocument->NodeInfoManager()->GetNodeInfo(nsGkAtoms::mozgeneratedcontentimage, nsnull,
                                                         kNameSpaceID_XHTML);

    nsCOMPtr<nsIContent> content;
    NS_NewGenConImageContent(getter_AddRefs(content), nodeInfo,
                             data.mContent.mImage);
    return content.forget();
  }

  switch (type) {
  case eStyleContentType_String:
    return CreateGenConTextNode(nsDependentString(data.mContent.mString), nsnull,
                                nsnull);

  case eStyleContentType_Attr:
    {
      nsCOMPtr<nsIAtom> attrName;
      PRInt32 attrNameSpace = kNameSpaceID_None;
      nsAutoString contentString(data.mContent.mString);
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
        return nsnull;
      }

      nsCOMPtr<nsIContent> content;
      NS_NewAttributeContent(mDocument->NodeInfoManager(),
                             attrNameSpace, attrName, getter_AddRefs(content));
      return content.forget();
    }
  
  case eStyleContentType_Counter:
  case eStyleContentType_Counters:
    {
      nsCSSValue::Array* counters = data.mContent.mCounters;
      nsCounterList* counterList = mCounterManager.CounterListFor(
          nsDependentString(counters->Item(0).GetStringBufferValue()));
      if (!counterList)
        return nsnull;

      nsCounterUseNode* node =
        new nsCounterUseNode(counters, aContentIndex,
                             type == eStyleContentType_Counters);
      if (!node)
        return nsnull;

      nsGenConInitializer* initializer =
        new nsGenConInitializer(node, counterList,
                                &nsCSSFrameConstructor::CountersDirty);
      return CreateGenConTextNode(EmptyString(), &node->mText, initializer);
    }

  case eStyleContentType_Image:
    NS_NOTREACHED("handled by if above");
    return nsnull;

  case eStyleContentType_OpenQuote:
  case eStyleContentType_CloseQuote:
  case eStyleContentType_NoOpenQuote:
  case eStyleContentType_NoCloseQuote:
    {
      nsQuoteNode* node =
        new nsQuoteNode(type, aContentIndex);
      if (!node)
        return nsnull;

      nsGenConInitializer* initializer =
        new nsGenConInitializer(node, &mQuoteList,
                                &nsCSSFrameConstructor::QuotesDirty);
      return CreateGenConTextNode(EmptyString(), &node->mText, initializer);
    }
  
  case eStyleContentType_AltContent:
    {
      // Use the "alt" attribute; if that fails and the node is an HTML
      // <input>, try the value attribute and then fall back to some default
      // localized text we have.
      // XXX what if the 'alt' attribute is added later, how will we
      // detect that and do the right thing here?
      if (aParentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::alt)) {
        nsCOMPtr<nsIContent> content;
        NS_NewAttributeContent(mDocument->NodeInfoManager(),
                               kNameSpaceID_None, nsGkAtoms::alt, getter_AddRefs(content));
        return content.forget();
      }

      if (aParentContent->IsNodeOfType(nsINode::eHTML) &&
          aParentContent->NodeInfo()->Equals(nsGkAtoms::input)) {
        if (aParentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::value)) {
          nsCOMPtr<nsIContent> content;
          NS_NewAttributeContent(mDocument->NodeInfoManager(),
                                 kNameSpaceID_None, nsGkAtoms::value, getter_AddRefs(content));
          return content.forget();
        }

        nsXPIDLString temp;
        nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                           "Submit", temp);
        return CreateGenConTextNode(temp, nsnull, nsnull);
      }

      break;
    }
  } // switch

  return nsnull;
}

static void DestroyContent(void *aObject,
                           nsIAtom *aPropertyName,
                           void *aPropertyValue,
                           void *aData)
{
  nsIContent* content = static_cast<nsIContent*>(aPropertyValue);
  content->UnbindFromTree();
  NS_RELEASE(content);
}

/*
 * aParentFrame - the frame that should be the parent of the generated
 *   content.  This is the frame for the corresponding content node,
 *   which must not be a leaf frame.
 * 
 * Any frames created are added to aFrameItems (or possibly left
 * in the table pseudoframe state in aState).
 * 
 * We create an XML element (tag _moz_generated_content_before or
 * _moz_generated_content_after) representing the pseudoelement. We
 * create a DOM node for each 'content' item and make those nodes the
 * children of the XML element. Then we create a frame subtree for
 * the XML element as if it were a regular child of
 * aParentFrame/aParentContent, giving the XML element the ::before or
 * ::after style.
 */
void
nsCSSFrameConstructor::CreateGeneratedContentFrame(nsFrameConstructorState& aState,
                                                   nsIFrame*        aParentFrame,
                                                   nsIContent*      aParentContent,
                                                   nsStyleContext*  aStyleContext,
                                                   nsIAtom*         aPseudoElement,
                                                   nsFrameItems&    aFrameItems)
{
  if (!aParentContent->IsNodeOfType(nsINode::eELEMENT))
    return;

  nsStyleSet *styleSet = mPresShell->StyleSet();

  // Probe for the existence of the pseudo-element
  nsRefPtr<nsStyleContext> pseudoStyleContext;
  pseudoStyleContext = styleSet->ProbePseudoStyleFor(aParentContent,
                                                     aPseudoElement,
                                                     aStyleContext);
  if (!pseudoStyleContext)
    return;
  // |ProbePseudoStyleFor| checked the 'display' property and the
  // |ContentCount()| of the 'content' property for us.
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsIAtom* elemName = aPseudoElement == nsCSSPseudoElements::before ?
    nsGkAtoms::mozgeneratedcontentbefore : nsGkAtoms::mozgeneratedcontentafter;
  nodeInfo = mDocument->NodeInfoManager()->GetNodeInfo(elemName, nsnull,
                                                       kNameSpaceID_None);
  nsIContent* container;
  nsresult rv = NS_NewXMLElement(&container, nodeInfo);
  if (NS_FAILED(rv))
    return;
  container->SetNativeAnonymous();
  // Transfer ownership to the frame
  aParentFrame->SetProperty(aPseudoElement, container, DestroyContent);

  rv = container->BindToTree(mDocument, aParentContent, aParentContent, PR_TRUE);
  if (NS_FAILED(rv)) {
    container->UnbindFromTree();
    return;
  }

  PRUint32 contentCount = pseudoStyleContext->GetStyleContent()->ContentCount();
  for (PRUint32 contentIndex = 0; contentIndex < contentCount; contentIndex++) {
    nsCOMPtr<nsIContent> content =
      CreateGeneratedContent(aParentContent, pseudoStyleContext, contentIndex);
    if (content) {
      container->AppendChildTo(content, PR_FALSE);
    }
  }

  nsFrameState savedStateBits = aState.mAdditionalStateBits;
  // Ensure that frames created here are all tagged with
  // NS_FRAME_GENERATED_CONTENT.
  aState.mAdditionalStateBits |= NS_FRAME_GENERATED_CONTENT;

  ConstructFrameInternal(aState, container, aParentFrame,
    elemName, kNameSpaceID_None, pseudoStyleContext, aFrameItems, PR_TRUE);
  aState.mAdditionalStateBits = savedStateBits;
}

nsresult
nsCSSFrameConstructor::CreateInputFrame(nsFrameConstructorState& aState,
                                        nsIContent*              aContent,
                                        nsIFrame*                aParentFrame,
                                        nsIAtom*                 aTag,
                                        nsStyleContext*          aStyleContext,
                                        nsIFrame**               aFrame,
                                        const nsStyleDisplay*    aStyleDisplay,
                                        PRBool&                  aFrameHasBeenInitialized,
                                        PRBool&                  aAddedToFrameList,
                                        nsFrameItems&            aFrameItems,
                                        PRBool                   aHasPseudoParent)
{
  // Make sure to keep IsSpecialContent in synch with this code
  
  // Note: do not do anything in this method that assumes pseudo-frames have
  // been processed.  If you feel the urge to do something like that, fix
  // callers accordingly.
  nsCOMPtr<nsIFormControl> control = do_QueryInterface(aContent);
  if (!control) {
    NS_ERROR("input doesn't implement nsIFormControl?");
    return NS_OK;
  }

  switch (control->GetType()) {
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_BUTTON:
    {
      if (gUseXBLForms)
        return NS_OK; // update IsSpecialContent if this becomes functional

      nsresult rv = ConstructButtonFrame(aState, aContent, aParentFrame,
                                         aTag, aStyleContext, aFrame,
                                         aStyleDisplay, aFrameItems,
                                         aHasPseudoParent);
      aAddedToFrameList = PR_TRUE;
      aFrameHasBeenInitialized = PR_TRUE;
      return rv;
    }

    case NS_FORM_INPUT_CHECKBOX:
      if (gUseXBLForms)
        return NS_OK; // see comment above
      return ConstructCheckboxControlFrame(aFrame, aContent, aStyleContext);

    case NS_FORM_INPUT_RADIO:
      if (gUseXBLForms)
        return NS_OK; // see comment above
      return ConstructRadioControlFrame(aFrame, aContent, aStyleContext);

    case NS_FORM_INPUT_FILE:
    {
      *aFrame = NS_NewFileControlFrame(mPresShell, aStyleContext);

      if (*aFrame) {
        // The (block-like) file control frame should have a space manager
        (*aFrame)->AddStateBits(NS_BLOCK_SPACE_MGR);
        return NS_OK;
      }
      else {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    case NS_FORM_INPUT_HIDDEN:
      return NS_OK; // this does not create a frame so it needs special handling
                    // in IsSpecialContent

    case NS_FORM_INPUT_IMAGE:
      return CreateHTMLImageFrame(aContent, aStyleContext,
                                  NS_NewImageControlFrame, aFrame);

    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    {
      *aFrame = NS_NewTextControlFrame(mPresShell, aStyleContext);
      
      return NS_UNLIKELY(!*aFrame) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }

    default:
      NS_ASSERTION(0, "Unknown input type!");
      return NS_ERROR_INVALID_ARG;
  }
}

nsresult
nsCSSFrameConstructor::CreateHTMLImageFrame(nsIContent* aContent,
                                            nsStyleContext* aStyleContext,
                                            ImageFrameCreatorFunc aFunc,
                                            nsIFrame** aFrame)
{
  *aFrame = nsnull;

  // Make sure to keep IsSpecialContent in synch with this code
  if (nsImageFrame::ShouldCreateImageFrameFor(aContent, aStyleContext)) {
    *aFrame = (*aFunc)(mPresShell, aStyleContext);
     
    if (NS_UNLIKELY(!*aFrame))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

static PRBool
TextIsOnlyWhitespace(nsIContent* aContent)
{
  return aContent->IsNodeOfType(nsINode::eTEXT) &&
         aContent->TextIsOnlyWhitespace();
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
      (aDisplay == NS_STYLE_DISPLAY_INLINE_TABLE)       ||
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
  if ((nsGkAtoms::tableFrame         == aParentType)  ||
      (nsGkAtoms::tableRowGroupFrame == aParentType)  ||
      (nsGkAtoms::tableRowFrame      == aParentType)) {
    return PR_TRUE;
  }
  else if (aIncludeSpecial && 
           ((nsGkAtoms::tableCaptionFrame  == aParentType)  ||
            (nsGkAtoms::tableColGroupFrame == aParentType)  ||
            (nsGkAtoms::tableColFrame      == aParentType)  ||
            IS_TABLE_CELL(aParentType))) {
    return PR_TRUE;
  }
  else return PR_FALSE;
}
           
static nsIFrame*
AdjustCaptionParentFrame(nsIFrame* aParentFrame) 
{
  if (nsGkAtoms::tableFrame == aParentFrame->GetType()) {
    return aParentFrame->GetParent();;
  }
  return aParentFrame;
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

  if (nsGkAtoms::tableCaptionFrame == aChildFrame->GetType()) {
    haveCaption = PR_TRUE;
    *aAdjParentFrame = AdjustCaptionParentFrame(aParentFrame);
  }
  return haveCaption;
}
   
static nsresult 
ProcessPseudoFrame(nsPseudoFrameData& aPseudoData,
                   nsIFrame*&         aParent)
{
  nsresult rv = NS_OK;

  aParent = aPseudoData.mFrame;
  nsFrameItems* items = &aPseudoData.mChildList;
  if (items && items->childList) {
    rv = aParent->SetInitialChildList(nsnull, items->childList);
    if (NS_FAILED(rv)) return rv;
  }
  aPseudoData.Reset();
  return rv;
}

static nsresult 
ProcessPseudoRowGroupFrame(nsPseudoFrameData& aPseudoData,
                           nsIFrame*&         aParent)
{
  nsresult rv = NS_OK;

  aParent = aPseudoData.mFrame;
  nsFrameItems* items = &aPseudoData.mChildList;
  if (items && items->childList) {
    nsTableRowGroupFrame* rgFrame = nsTableFrame::GetRowGroupFrame(aParent);
    rv = rgFrame->SetInitialChildList(nsnull, items->childList);
    if (NS_FAILED(rv)) return rv;
  }
  aPseudoData.Reset();
  return rv;
}

static nsresult 
ProcessPseudoTableFrame(nsPseudoFrames& aPseudoFrames,
                        nsIFrame*&      aParent)
{
  nsresult rv = NS_OK;

  // process the col group frame, if it exists
  if (aPseudoFrames.mColGroup.mFrame) {
    rv = ProcessPseudoFrame(aPseudoFrames.mColGroup, aParent);
  }

  // process the inner table frame
  rv = ProcessPseudoFrame(aPseudoFrames.mTableInner, aParent);

  // process the outer table frame
  aParent = aPseudoFrames.mTableOuter.mFrame;
  nsFrameItems* items = &aPseudoFrames.mTableOuter.mChildList;
  if (items && items->childList) {
    rv = aParent->SetInitialChildList(nsnull, items->childList);
    if (NS_FAILED(rv)) return rv;
  }
  nsFrameItems* captions = &aPseudoFrames.mTableOuter.mChildList2;
  if (captions && captions->childList) {
    rv = aParent->SetInitialChildList(nsGkAtoms::captionList, captions->childList);
  }
  aPseudoFrames.mTableOuter.Reset();
  return rv;
}

static nsresult 
ProcessPseudoCellFrame(nsPseudoFrames& aPseudoFrames,
                       nsIFrame*&      aParent)
{
  nsresult rv = NS_OK;

  rv = ProcessPseudoFrame(aPseudoFrames.mCellInner, aParent);
  if (NS_FAILED(rv)) return rv;
  rv = ProcessPseudoFrame(aPseudoFrames.mCellOuter, aParent);
  return rv;
}

// limit the processing up to the frame type indicated by aHighestType.
// make a complete processing when aHighestType is null
static nsresult 
ProcessPseudoFrames(nsFrameConstructorState& aState,
                    nsIAtom*        aHighestType,
                    nsIFrame*&      aHighestFrame)
{
  nsresult rv = NS_OK;

  aHighestFrame = nsnull;

#ifdef DEBUG
  if (gTablePseudoFrame) {
    printf("*** ProcessPseudoFrames enter***\n");
    aState.mPseudoFrames.Dump();
  }
#endif

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;

  if (nsGkAtoms::tableFrame == pseudoFrames.mLowestType) {
    if (pseudoFrames.mColGroup.mFrame) {
      rv = ProcessPseudoFrame(pseudoFrames.mColGroup, aHighestFrame);
      if (nsGkAtoms::tableColGroupFrame == aHighestType) return rv;
    }
    rv = ProcessPseudoTableFrame(pseudoFrames, aHighestFrame);
    if (nsGkAtoms::tableOuterFrame == aHighestType) return rv;
    
    if (pseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(pseudoFrames, aHighestFrame);
      if (IS_TABLE_CELL(aHighestType)) return rv;
    }
    if (pseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(pseudoFrames.mRow, aHighestFrame);
      if (nsGkAtoms::tableRowFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoRowGroupFrame(pseudoFrames.mRowGroup, aHighestFrame);
      if (nsGkAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
  }
  else if (nsGkAtoms::tableRowGroupFrame == pseudoFrames.mLowestType) {
    rv = ProcessPseudoRowGroupFrame(pseudoFrames.mRowGroup, aHighestFrame);
    if (nsGkAtoms::tableRowGroupFrame == aHighestType) return rv;
    if (pseudoFrames.mColGroup.mFrame) {
      nsIFrame* colGroupHigh;
      rv = ProcessPseudoFrame(pseudoFrames.mColGroup, colGroupHigh);
      if (aHighestFrame &&
          nsGkAtoms::tableRowGroupFrame == aHighestFrame->GetType() &&
          !pseudoFrames.mTableInner.mFrame) {
        // table frames are special they can have two types of pseudo frames as
        // children that need to be processed in one pass, we only need to link
        // them if the parent is not a pseudo where the link is already done
        // We sort this later out inside nsTableFrame.
        colGroupHigh->SetNextSibling(aHighestFrame); 
      }
      aHighestFrame = colGroupHigh;
      if (nsGkAtoms::tableColGroupFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(pseudoFrames, aHighestFrame);
      if (nsGkAtoms::tableOuterFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(pseudoFrames, aHighestFrame);
      if (IS_TABLE_CELL(aHighestType)) return rv;
    }
    if (pseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(pseudoFrames.mRow, aHighestFrame);
      if (nsGkAtoms::tableRowFrame == aHighestType) return rv;
    }
  }
  else if (nsGkAtoms::tableRowFrame == pseudoFrames.mLowestType) {
    rv = ProcessPseudoFrame(pseudoFrames.mRow, aHighestFrame);
    if (nsGkAtoms::tableRowFrame == aHighestType) return rv;

    if (pseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoRowGroupFrame(pseudoFrames.mRowGroup, aHighestFrame);
      if (nsGkAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mColGroup.mFrame) {
      nsIFrame* colGroupHigh;
      rv = ProcessPseudoFrame(pseudoFrames.mColGroup, colGroupHigh);
      if (aHighestFrame &&
          nsGkAtoms::tableRowGroupFrame == aHighestFrame->GetType() &&
          !pseudoFrames.mTableInner.mFrame) {
        // table frames are special they can have two types of pseudo frames as
        // children that need to be processed in one pass, we only need to link
        // them if the parent is not a pseudo where the link is already done
        // We sort this later out inside nsTableFrame.
        colGroupHigh->SetNextSibling(aHighestFrame); 
      }
      aHighestFrame = colGroupHigh;
      if (nsGkAtoms::tableColGroupFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(pseudoFrames, aHighestFrame);
      if (nsGkAtoms::tableOuterFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mCellOuter.mFrame) {
      rv = ProcessPseudoCellFrame(pseudoFrames, aHighestFrame);
      if (IS_TABLE_CELL(aHighestType)) return rv;
    }
  }
  else if (IS_TABLE_CELL(pseudoFrames.mLowestType)) {
    rv = ProcessPseudoCellFrame(pseudoFrames, aHighestFrame);
    if (IS_TABLE_CELL(aHighestType)) return rv;

    if (pseudoFrames.mRow.mFrame) {
      rv = ProcessPseudoFrame(pseudoFrames.mRow, aHighestFrame);
      if (nsGkAtoms::tableRowFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mRowGroup.mFrame) {
      rv = ProcessPseudoRowGroupFrame(pseudoFrames.mRowGroup, aHighestFrame);
      if (nsGkAtoms::tableRowGroupFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mColGroup.mFrame) {
      nsIFrame* colGroupHigh;
      rv = ProcessPseudoFrame(pseudoFrames.mColGroup, colGroupHigh);
      if (aHighestFrame &&
          nsGkAtoms::tableRowGroupFrame == aHighestFrame->GetType() &&
          !pseudoFrames.mTableInner.mFrame) {
        // table frames are special they can have two types of pseudo frames as
        // children that need to be processed in one pass, we only need to link
        // them if the parent is not a pseudo where the link is already done
        // We sort this later out inside nsTableFrame.
        colGroupHigh->SetNextSibling(aHighestFrame); 
      }
      aHighestFrame = colGroupHigh;
      if (nsGkAtoms::tableColGroupFrame == aHighestType) return rv;
    }
    if (pseudoFrames.mTableOuter.mFrame) {
      rv = ProcessPseudoTableFrame(pseudoFrames, aHighestFrame);
    }
  }
  else if (pseudoFrames.mColGroup.mFrame) { 
    // process the col group frame
    rv = ProcessPseudoFrame(pseudoFrames.mColGroup, aHighestFrame);
  }

  return rv;
}

static nsresult 
ProcessPseudoFrames(nsFrameConstructorState& aState,
                    nsFrameItems&   aItems)
{

#ifdef DEBUG
  if (gTablePseudoFrame) {
    printf("*** ProcessPseudoFrames complete enter***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
 
  nsIFrame* highestFrame;
  nsresult rv = ProcessPseudoFrames(aState, nsnull, highestFrame);
  if (highestFrame) {
    aItems.AddChild(highestFrame);
  }
 
#ifdef DEBUG
  if (gTablePseudoFrame) {
    printf("*** ProcessPseudoFrames complete leave, highestframe:%p***\n",
           static_cast<void*>(highestFrame));
    aState.mPseudoFrames.Dump();
  }
#endif
  aState.mPseudoFrames.Reset();
  return rv;
}

static nsresult 
ProcessPseudoFrames(nsFrameConstructorState& aState,
                    nsIAtom*        aHighestType)
{
#ifdef DEBUG
  if (gTablePseudoFrame) {
    printf("*** ProcessPseudoFrames limited enter highest:");
    if (nsGkAtoms::tableOuterFrame == aHighestType) 
      printf("OuterTable");
    else if (nsGkAtoms::tableFrame == aHighestType) 
      printf("InnerTable");
    else if (nsGkAtoms::tableColGroupFrame == aHighestType) 
      printf("ColGroup");
    else if (nsGkAtoms::tableRowGroupFrame == aHighestType) 
      printf("RowGroup");
    else if (nsGkAtoms::tableRowFrame == aHighestType) 
      printf("Row");
    else if (IS_TABLE_CELL(aHighestType)) 
      printf("Cell");
    else 
      NS_ASSERTION(PR_FALSE, "invalid call to ProcessPseudoFrames ");
    printf("***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
 
  nsIFrame* highestFrame;
  nsresult rv = ProcessPseudoFrames(aState, aHighestType, highestFrame);

#ifdef DEBUG
  if (gTablePseudoFrame) {
    printf("*** ProcessPseudoFrames limited leave:%p***\n",
           static_cast<void*>(highestFrame));
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoTableFrame(PRInt32                  aNameSpaceID,
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

  // Thankfully, the parent can't change display type without causing
  // frame reconstruction, so this won't need to change.
  nsIAtom *pseudoType;
  if (parentStyle->GetStyleDisplay()->mDisplay == NS_STYLE_DISPLAY_INLINE)
    pseudoType = nsCSSAnonBoxes::inlineTable;
  else
    pseudoType = nsCSSAnonBoxes::table;

  // create the SC for the inner table which will be the parent of the outer table's SC
  childStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             pseudoType,
                                                             parentStyle);

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mTableOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mTableInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  nsFrameItems items;
  rv = ConstructTableFrame(aState, parentContent,
                           parentFrame, childStyle, aNameSpaceID,
                           PR_TRUE, items, pseudoOuter.mFrame, 
                           pseudoInner.mFrame);

  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  pseudoOuter.mChildList.AddChild(pseudoInner.mFrame);
  aState.mPseudoFrames.mLowestType = nsGkAtoms::tableFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mCellInner.mFrame) {
    aState.mPseudoFrames.mCellInner.mChildList.AddChild(pseudoOuter.mFrame);
  }
#ifdef DEBUG
  if (gTablePseudoFrame) {
     printf("*** CreatePseudoTableFrame ***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoRowGroupFrame(PRInt32                  aNameSpaceID,
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

  childStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableRowGroup, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRowGroup;

  // construct the pseudo row group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowGroupFrame(aState, parentContent,
                                   parentFrame, childStyle, aNameSpaceID,
                                   PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  aState.mPseudoFrames.mLowestType = nsGkAtoms::tableRowGroupFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mTableInner.mFrame) {
    aState.mPseudoFrames.mTableInner.mChildList.AddChild(pseudo.mFrame);
  }
#ifdef DEBUG
  if (gTablePseudoFrame) {
     printf("*** CreatePseudoRowGroupFrame ***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

nsresult 
nsCSSFrameConstructor::CreatePseudoColGroupFrame(PRInt32                  aNameSpaceID,
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

  childStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableColGroup, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mColGroup;

  // construct the pseudo col group as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableColGroupFrame(aState, parentContent,
                                   parentFrame, childStyle, aNameSpaceID,
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
#ifdef DEBUG
  if (gTablePseudoFrame) {
     printf("*** CreatePseudoColGroupFrame ***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoRowFrame(PRInt32                  aNameSpaceID,
                                            nsFrameConstructorState& aState, 
                                            nsIFrame*                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsIFrame* parentFrame = aParentFrameIn;
  if (aState.mPseudoFrames.mRowGroup.mFrame) {
    parentFrame = (nsIFrame*) nsTableFrame::GetRowGroupFrame(aState.mPseudoFrames.mRowGroup.mFrame);
  }
  if (!parentFrame) return rv;

  nsStyleContext *parentStyle;
  nsRefPtr<nsStyleContext> childStyle;

  parentStyle = parentFrame->GetStyleContext();
  nsIContent* parentContent = parentFrame->GetContent();

  childStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableRow, 
                                                             parentStyle);

  nsPseudoFrameData& pseudo = aState.mPseudoFrames.mRow;

  // construct the pseudo row as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableRowFrame(aState, parentContent,
                              parentFrame, childStyle, aNameSpaceID,
                              PR_TRUE, items, pseudo.mFrame, pseudoParent);
  if (NS_FAILED(rv)) return rv;

  aState.mPseudoFrames.mLowestType = nsGkAtoms::tableRowFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mRowGroup.mFrame) {
    aState.mPseudoFrames.mRowGroup.mChildList.AddChild(pseudo.mFrame);
  }
#ifdef DEBUG
  if (gTablePseudoFrame) {
     printf("*** CreatePseudoRowFrame ***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

nsresult
nsCSSFrameConstructor::CreatePseudoCellFrame(PRInt32                  aNameSpaceID,
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

  childStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(parentContent,
                                                             nsCSSAnonBoxes::tableCell, 
                                                             parentStyle);

  nsPseudoFrameData& pseudoOuter = aState.mPseudoFrames.mCellOuter;
  nsPseudoFrameData& pseudoInner = aState.mPseudoFrames.mCellInner;

  // construct the pseudo outer and inner as part of the pseudo frames
  PRBool pseudoParent;
  nsFrameItems items;
  rv = ConstructTableCellFrame(aState, parentContent, parentFrame, childStyle,
                               aNameSpaceID, PR_TRUE, items,
                               pseudoOuter.mFrame, pseudoInner.mFrame,
                               pseudoParent);
  if (NS_FAILED(rv)) return rv;

  // set pseudo data for the newly created frames
  pseudoOuter.mChildList.AddChild(pseudoInner.mFrame);
  // give it nsGkAtoms::tableCellFrame, if it is really nsGkAtoms::bcTableCellFrame, it will match later
  aState.mPseudoFrames.mLowestType = nsGkAtoms::tableCellFrame;

  // set pseudo data for the parent
  if (aState.mPseudoFrames.mRow.mFrame) {
    aState.mPseudoFrames.mRow.mChildList.AddChild(pseudoOuter.mFrame);
  }
#ifdef DEBUG
  if (gTablePseudoFrame) {
     printf("*** CreatePseudoCellFrame ***\n");
    aState.mPseudoFrames.Dump();
  }
#endif
  return rv;
}

// called if the parent is not a table
nsresult 
nsCSSFrameConstructor::GetPseudoTableFrame(PRInt32                  aNameSpaceID,
                                           nsFrameConstructorState& aState, 
                                           nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsGkAtoms::tableRowGroupFrame == parentFrameType) { // row group parent
      rv = CreatePseudoRowFrame(aNameSpaceID, aState, &aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      created = PR_TRUE;
    }
    if (created || (nsGkAtoms::tableRowFrame == parentFrameType)) { // row parent
      rv = CreatePseudoCellFrame(aNameSpaceID, aState, &aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
    }
    rv = CreatePseudoTableFrame(aNameSpaceID, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mTableInner.mFrame) { 
      if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
        rv = CreatePseudoRowFrame(aNameSpaceID, aState);
        if (NS_FAILED(rv)) return rv;
      }
      if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
        rv = CreatePseudoCellFrame(aNameSpaceID, aState);
        if (NS_FAILED(rv)) return rv;
      }
      CreatePseudoTableFrame(aNameSpaceID, aState);
    }
  }
  return rv;
}

// called if the parent is not a col group
nsresult 
nsCSSFrameConstructor::GetPseudoColGroupFrame(PRInt32                  aNameSpaceID,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (pseudoFrames.IsEmpty()) {
    PRBool created = PR_FALSE;
    if (nsGkAtoms::tableRowGroupFrame == parentFrameType) {  // row group parent
      rv = CreatePseudoRowFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsGkAtoms::tableRowFrame == parentFrameType)) { // row parent
      rv = CreatePseudoCellFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || IS_TABLE_CELL(parentFrameType) || // cell parent
        (nsGkAtoms::tableCaptionFrame == parentFrameType)  || // caption parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
      rv = CreatePseudoTableFrame(aNameSpaceID, aState, &aParentFrameIn);
    }
    rv = CreatePseudoColGroupFrame(aNameSpaceID, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mColGroup.mFrame) {
      if (!pseudoFrames.mTableInner.mFrame) {
        if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
          rv = CreatePseudoRowFrame(aNameSpaceID, aState);
        }
        if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
          rv = CreatePseudoCellFrame(aNameSpaceID, aState);
        }
        if (pseudoFrames.mCellOuter.mFrame && !(pseudoFrames.mTableOuter.mFrame)) {
          rv = CreatePseudoTableFrame(aNameSpaceID, aState);
        }
      }
      rv = CreatePseudoColGroupFrame(aNameSpaceID, aState);
    }
  }
  return rv;
}

// called if the parent is not a row group
nsresult 
nsCSSFrameConstructor::GetPseudoRowGroupFrame(PRInt32                  aNameSpaceID,
                                              nsFrameConstructorState& aState, 
                                              nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (!pseudoFrames.mLowestType) {
    PRBool created = PR_FALSE;
    if (nsGkAtoms::tableRowFrame == parentFrameType) {  // row parent
      rv = CreatePseudoCellFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || IS_TABLE_CELL(parentFrameType) || // cell parent
        (nsGkAtoms::tableCaptionFrame == parentFrameType)  || // caption parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
      rv = CreatePseudoTableFrame(aNameSpaceID, aState, &aParentFrameIn);
    }
    rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mRowGroup.mFrame) { 
      if (pseudoFrames.mRow.mFrame && !(pseudoFrames.mCellOuter.mFrame)) {
        rv = CreatePseudoCellFrame(aNameSpaceID, aState);
      }
      if (pseudoFrames.mCellOuter.mFrame && !(pseudoFrames.mTableOuter.mFrame)) {
        rv = CreatePseudoTableFrame(aNameSpaceID, aState);
      }
      rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState);
    }
  }
  return rv;
}

// called if the parent is not a row
nsresult
nsCSSFrameConstructor::GetPseudoRowFrame(PRInt32                  aNameSpaceID,
                                         nsFrameConstructorState& aState, 
                                         nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (!pseudoFrames.mLowestType) {
    PRBool created = PR_FALSE;
    if (IS_TABLE_CELL(parentFrameType) || // cell parent
       (nsGkAtoms::tableCaptionFrame == parentFrameType)  || // caption parent
        !IsTableRelated(parentFrameType, PR_TRUE)) { // block parent
      rv = CreatePseudoTableFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsGkAtoms::tableFrame == parentFrameType)) { // table parent
      rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState, &aParentFrameIn);
    }
    rv = CreatePseudoRowFrame(aNameSpaceID, aState, &aParentFrameIn);
  }
  else {
    if (!pseudoFrames.mRow.mFrame) { 
      if (pseudoFrames.mCellOuter.mFrame && !pseudoFrames.mTableOuter.mFrame) {
        rv = CreatePseudoTableFrame(aNameSpaceID, aState);
      }
      if (pseudoFrames.mTableInner.mFrame && !(pseudoFrames.mRowGroup.mFrame)) {
        rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState);
      }
      rv = CreatePseudoRowFrame(aNameSpaceID, aState);
    }
  }
  return rv;
}

// called if the parent is not a cell or block
nsresult 
nsCSSFrameConstructor::GetPseudoCellFrame(PRInt32                  aNameSpaceID,
                                          nsFrameConstructorState& aState, 
                                          nsIFrame&                aParentFrameIn)
{
  nsresult rv = NS_OK;

  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  nsIAtom* parentFrameType = aParentFrameIn.GetType();

  if (!pseudoFrames.mLowestType) {
    PRBool created = PR_FALSE;
    if (nsGkAtoms::tableFrame == parentFrameType) { // table parent
      rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    if (created || (nsGkAtoms::tableRowGroupFrame == parentFrameType)) { // row group parent
      rv = CreatePseudoRowFrame(aNameSpaceID, aState, &aParentFrameIn);
      created = PR_TRUE;
    }
    rv = CreatePseudoCellFrame(aNameSpaceID, aState, &aParentFrameIn);
  }
  else if (!pseudoFrames.mCellOuter.mFrame) { 
    if (pseudoFrames.mTableInner.mFrame && !(pseudoFrames.mRowGroup.mFrame)) {
      rv = CreatePseudoRowGroupFrame(aNameSpaceID, aState);
    }
    if (pseudoFrames.mRowGroup.mFrame && !(pseudoFrames.mRow.mFrame)) {
      rv = CreatePseudoRowFrame(aNameSpaceID, aState);
    }
    rv = CreatePseudoCellFrame(aNameSpaceID, aState);
  }
  return rv;
}

nsresult 
nsCSSFrameConstructor::GetParentFrame(PRInt32                  aNameSpaceID,
                                      nsIFrame&                aParentFrameIn, 
                                      nsIAtom*                 aChildFrameType, 
                                      nsFrameConstructorState& aState, 
                                      nsIFrame*&               aParentFrame,
                                      PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;

  nsIAtom* parentFrameType = aParentFrameIn.GetType();
  nsIFrame* pseudoParentFrame = nsnull;
  nsPseudoFrames& pseudoFrames = aState.mPseudoFrames;
  aParentFrame = &aParentFrameIn;
  aIsPseudoParent = PR_FALSE;

  nsFrameState savedStateBits  = aState.mAdditionalStateBits;
  aState.mAdditionalStateBits &= ~NS_FRAME_GENERATED_CONTENT;

  if (nsGkAtoms::tableOuterFrame == aChildFrameType) { // table child
    if (IsTableRelated(parentFrameType, PR_TRUE) &&
        (nsGkAtoms::tableCaptionFrame != parentFrameType) ) { // need pseudo cell parent
      rv = GetPseudoCellFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mCellInner.mFrame;
    }
  } 
  else if (nsGkAtoms::tableCaptionFrame == aChildFrameType) { // caption child
    if (nsGkAtoms::tableOuterFrame != parentFrameType) { // need pseudo table parent
      rv = GetPseudoTableFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableOuter.mFrame;
    }
  }
  else if (nsGkAtoms::tableColGroupFrame == aChildFrameType) { // col group child
    if (nsGkAtoms::tableFrame != parentFrameType) { // need pseudo table parent
      rv = GetPseudoTableFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
    }
  }
  else if (nsGkAtoms::tableColFrame == aChildFrameType) { // col child
    if (nsGkAtoms::tableColGroupFrame != parentFrameType) { // need pseudo col group parent
      rv = GetPseudoColGroupFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mColGroup.mFrame;
    }
  }
  else if (nsGkAtoms::tableRowGroupFrame == aChildFrameType) { // row group child
    // XXX can this go away?
    if (nsGkAtoms::tableFrame != parentFrameType) {
      // trees allow row groups to contain row groups, so don't create pseudo frames
        rv = GetPseudoTableFrame(aNameSpaceID, aState, aParentFrameIn);
        if (NS_FAILED(rv)) return rv;
        pseudoParentFrame = pseudoFrames.mTableInner.mFrame;
     }
  }
  else if (nsGkAtoms::tableRowFrame == aChildFrameType) { // row child
    if (nsGkAtoms::tableRowGroupFrame != parentFrameType) { // need pseudo row group parent
      rv = GetPseudoRowGroupFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRowGroup.mFrame;
    }
  }
  else if (IS_TABLE_CELL(aChildFrameType)) { // cell child
    if (nsGkAtoms::tableRowFrame != parentFrameType) { // need pseudo row parent
      rv = GetPseudoRowFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mRow.mFrame;
    }
  }
  else if (nsGkAtoms::tableFrame == aChildFrameType) { // invalid
    NS_ASSERTION(PR_FALSE, "GetParentFrame called on nsGkAtoms::tableFrame child");
  }
  else { // foreign frame
    if (IsTableRelated(parentFrameType, PR_FALSE)) { // need pseudo cell parent
      rv = GetPseudoCellFrame(aNameSpaceID, aState, aParentFrameIn);
      if (NS_FAILED(rv)) return rv;
      pseudoParentFrame = pseudoFrames.mCellInner.mFrame;
    }
  }
  
  if (pseudoParentFrame) {
    aParentFrame = pseudoParentFrame;
    aIsPseudoParent = PR_TRUE;
  }

  aState.mAdditionalStateBits = savedStateBits;
  return rv;
}

static PRBool
IsSpecialContent(nsIContent*     aContent,
                 nsIAtom*        aTag,
                 PRInt32         aNameSpaceID,
                 nsStyleContext* aStyleContext)
{
  // Gross hack. Return true if this is a content node that we'd create a
  // frame for based on something other than display -- in other words if this
  // is a node that could never have a nsTableCellFrame, for example.
  if (aContent->IsNodeOfType(nsINode::eHTML) ||
      aNameSpaceID == kNameSpaceID_XHTML) {
    // XXXbz this is duplicating some logic from ConstructHTMLFrame....
    // Would be nice to avoid that.  :(

    if (aTag == nsGkAtoms::input) {
      nsCOMPtr<nsIFormControl> control = do_QueryInterface(aContent);
      if (control) {
        PRInt32 type = control->GetType();
        if (NS_FORM_INPUT_HIDDEN == type) {
          return PR_FALSE; // input hidden does not create a special frame
        }
        else if (NS_FORM_INPUT_IMAGE == type) {
          return nsImageFrame::ShouldCreateImageFrameFor(aContent, aStyleContext);
        }
      }

      return PR_TRUE;
    }

    if (aTag == nsGkAtoms::img ||
        aTag == nsGkAtoms::mozgeneratedcontentimage) {
      return nsImageFrame::ShouldCreateImageFrameFor(aContent, aStyleContext);
    }

    if (aTag == nsGkAtoms::object ||
        aTag == nsGkAtoms::applet ||
        aTag == nsGkAtoms::embed) {
      return !(aContent->IntrinsicState() &
             (NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED |
              NS_EVENT_STATE_SUPPRESSED));
    }

    return
      aTag == nsGkAtoms::br ||
      aTag == nsGkAtoms::wbr ||
      aTag == nsGkAtoms::textarea ||
      aTag == nsGkAtoms::select ||
      aTag == nsGkAtoms::fieldset ||
      aTag == nsGkAtoms::legend ||
      aTag == nsGkAtoms::frameset ||
      aTag == nsGkAtoms::iframe ||
      aTag == nsGkAtoms::spacer ||
      aTag == nsGkAtoms::button ||
      aTag == nsGkAtoms::isindex ||
      aTag == nsGkAtoms::canvas ||
#if defined(MOZ_MEDIA)
      aTag == nsGkAtoms::video ||
      aTag == nsGkAtoms::audio ||
#endif
      PR_FALSE;
  }


  if (aNameSpaceID == kNameSpaceID_XUL)
    return
#ifdef MOZ_XUL
      aTag == nsGkAtoms::button ||
      aTag == nsGkAtoms::checkbox ||
      aTag == nsGkAtoms::radio ||
      aTag == nsGkAtoms::autorepeatbutton ||
      aTag == nsGkAtoms::titlebar ||
      aTag == nsGkAtoms::resizer ||
      aTag == nsGkAtoms::image ||
      aTag == nsGkAtoms::spring ||
      aTag == nsGkAtoms::spacer ||
      aTag == nsGkAtoms::treechildren ||
      aTag == nsGkAtoms::treecol ||
      aTag == nsGkAtoms::text ||
      aTag == nsGkAtoms::description ||
      aTag == nsGkAtoms::label ||
      aTag == nsGkAtoms::menu ||
      aTag == nsGkAtoms::menuitem ||
      aTag == nsGkAtoms::menubutton ||
      aTag == nsGkAtoms::menubar ||
      (aTag == nsGkAtoms::popupgroup &&
       aContent->IsRootOfNativeAnonymousSubtree()) ||
      aTag == nsGkAtoms::iframe ||
      aTag == nsGkAtoms::editor ||
      aTag == nsGkAtoms::browser ||
      aTag == nsGkAtoms::progressmeter ||
#endif
      aTag == nsGkAtoms::slider ||
      aTag == nsGkAtoms::scrollbar ||
      aTag == nsGkAtoms::scrollbarbutton ||
#ifdef MOZ_XUL
      aTag == nsGkAtoms::splitter ||
#endif
      PR_FALSE;

#ifdef MOZ_SVG
  if (aNameSpaceID == kNameSpaceID_SVG && NS_SVGEnabled()) {
    // All SVG content is special...
    return PR_TRUE;
  }
#endif

#ifdef MOZ_MATHML
  if (aNameSpaceID == kNameSpaceID_MathML)
    return
      aTag == nsGkAtoms::mi_ ||
      aTag == nsGkAtoms::mn_ ||
      aTag == nsGkAtoms::ms_ ||
      aTag == nsGkAtoms::mtext_ ||
      aTag == nsGkAtoms::mo_ ||
      aTag == nsGkAtoms::mfrac_ ||
      aTag == nsGkAtoms::msup_ ||
      aTag == nsGkAtoms::msub_ ||
      aTag == nsGkAtoms::msubsup_ ||
      aTag == nsGkAtoms::munder_ ||
      aTag == nsGkAtoms::mover_ ||
      aTag == nsGkAtoms::munderover_ ||
      aTag == nsGkAtoms::mphantom_ ||
      aTag == nsGkAtoms::mpadded_ ||
      aTag == nsGkAtoms::mspace_ ||
      aTag == nsGkAtoms::mfenced_ ||
      aTag == nsGkAtoms::mmultiscripts_ ||
      aTag == nsGkAtoms::mstyle_ ||
      aTag == nsGkAtoms::msqrt_ ||
      aTag == nsGkAtoms::mroot_ ||
      aTag == nsGkAtoms::maction_ ||
      aTag == nsGkAtoms::mrow_   ||
      aTag == nsGkAtoms::merror_ ||
      aTag == nsGkAtoms::none   ||
      aTag == nsGkAtoms::mprescripts_ ||
      aTag == nsGkAtoms::math;
#endif
  return PR_FALSE;
}
                                      
nsresult
nsCSSFrameConstructor::AdjustParentFrame(nsFrameConstructorState&     aState,
                                         nsIContent*                  aChildContent,
                                         nsIFrame* &                  aParentFrame,
                                         nsIAtom*                     aTag,
                                         PRInt32                      aNameSpaceID,
                                         nsStyleContext*              aChildStyle,
                                         nsFrameItems* &              aFrameItems,
                                         nsFrameConstructorSaveState& aSaveState,
                                         PRBool&                      aSuppressFrame,
                                         PRBool&                      aCreatedPseudo)
{
  NS_PRECONDITION(aChildStyle, "Must have child's style context");
  NS_PRECONDITION(aFrameItems, "Must have frame items to work with");

  aSuppressFrame = PR_FALSE;
  aCreatedPseudo = PR_FALSE;
  if (!aParentFrame) {
    // Nothing to do here
    return NS_OK;
  }

  PRBool childIsSpecialContent = PR_FALSE; // lazy lookup
  // Only use the outer table frame as parent if the child is going to use a
  // tableCaptionFrame, otherwise the inner table frame is the parent
  // (bug 341858).
  nsIAtom* parentType = aParentFrame->GetType();
  NS_ASSERTION(parentType != nsGkAtoms::tableOuterFrame,
               "Shouldn't be happening");
  if (parentType == nsGkAtoms::tableColGroupFrame) {
    childIsSpecialContent = IsSpecialContent(aChildContent, aTag, aNameSpaceID,
                                             aChildStyle);
    if (childIsSpecialContent ||
        (aChildStyle->GetStyleDisplay()->mDisplay !=
         NS_STYLE_DISPLAY_TABLE_COLUMN)) {
      aSuppressFrame = PR_TRUE;
      return NS_OK;
    }
  }
 
  // If our parent is a table, table-row-group, or table-row, and
  // we're not table-related in any way, we have to create table
  // pseudo-frames so that we have a table cell to live in.
  if (IsTableRelated(aParentFrame->GetType(), PR_FALSE) &&
      (!IsTableRelated(aChildStyle->GetStyleDisplay()->mDisplay, PR_TRUE) ||
       // Also need to create a pseudo-parent if the child is going to end up
       // with a frame based on something other than display.
       childIsSpecialContent || // looked it up before
       IsSpecialContent(aChildContent, aTag, aNameSpaceID, aChildStyle))) {
    nsFrameState savedStateBits  = aState.mAdditionalStateBits;
    aState.mAdditionalStateBits &= ~NS_FRAME_GENERATED_CONTENT;
    nsresult rv = GetPseudoCellFrame(aNameSpaceID, aState, *aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aState.mAdditionalStateBits = savedStateBits;

    NS_ASSERTION(aState.mPseudoFrames.mCellInner.mFrame,
                 "Must have inner cell frame now!");

    aParentFrame = aState.mPseudoFrames.mCellInner.mFrame;
    aFrameItems = &aState.mPseudoFrames.mCellInner.mChildList;
    // We pushed an anonymous table cell.  The inner block of this
    // needs to become the float containing block.
    aState.PushFloatContainingBlock(aParentFrame, aSaveState, PR_FALSE,
                                    PR_FALSE);
    aCreatedPseudo = PR_TRUE;
  }
  return NS_OK;
}

// Pull all the captions present in aItems out  into aCaptions
static void
PullOutCaptionFrames(nsFrameItems& aItems, nsFrameItems& aCaptions)
{
  nsIFrame *child = aItems.childList;
  nsIFrame* prev = nsnull;
  while (child) {
    nsIFrame *nextSibling = child->GetNextSibling();
    if (nsGkAtoms::tableCaptionFrame == child->GetType()) {
      aItems.RemoveChild(child, prev);
      aCaptions.AddChild(child);
    } else {
      prev = child;
    }
    child = nextSibling;
  }
}


// Construct the outer, inner table frames and the children frames for the table. 
// XXX Page break frames for pseudo table frames are not constructed to avoid the risk
// associated with revising the pseudo frame mechanism. The long term solution
// of having frames handle page-break-before/after will solve the problem. 
nsresult
nsCSSFrameConstructor::ConstructTableFrame(nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aContentParent,
                                           nsStyleContext*          aStyleContext,
                                           PRInt32                  aNameSpaceID,
                                           PRBool                   aIsPseudo,
                                           nsFrameItems&            aChildItems,
                                           nsIFrame*&               aNewOuterFrame,
                                           nsIFrame*&               aNewInnerFrame)
{
  nsresult rv = NS_OK;


  // create the pseudo SC for the outer table as a child of the inner SC
  nsRefPtr<nsStyleContext> outerStyleContext;
  outerStyleContext = mPresShell->StyleSet()->
    ResolvePseudoStyleFor(aContent, nsCSSAnonBoxes::tableOuter, aStyleContext);

  // Create the outer table frame which holds the caption and inner table frame
#ifdef MOZ_MATHML
  if (kNameSpaceID_MathML == aNameSpaceID)
    aNewOuterFrame = NS_NewMathMLmtableOuterFrame(mPresShell,
                                                  outerStyleContext);
  else
#endif
    aNewOuterFrame = NS_NewTableOuterFrame(mPresShell, outerStyleContext);

  nsIFrame* parentFrame = aContentParent;
  nsFrameItems* frameItems = &aChildItems;
  // We may need to push a float containing block
  nsFrameConstructorSaveState floatSaveState;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    PRBool hasPseudoParent = PR_FALSE;
    GetParentFrame(aNameSpaceID,*parentFrame, nsGkAtoms::tableOuterFrame,
                   aState, parentFrame, hasPseudoParent);
    if (!hasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
    if (hasPseudoParent) {
      aState.PushFloatContainingBlock(parentFrame, floatSaveState,
                                      PR_FALSE, PR_FALSE);
      frameItems = &aState.mPseudoFrames.mCellInner.mChildList;
      if (aState.mPseudoFrames.mTableOuter.mFrame) {
        ProcessPseudoFrames(aState, nsGkAtoms::tableOuterFrame);
      }
    }
  }

  nsIFrame* geometricParent = aState.GetGeometricParent
                                (outerStyleContext->GetStyleDisplay(),
                                 parentFrame);

  // Init the table outer frame and see if we need to create a view, e.g.
  // the frame is absolutely positioned  
  InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, aNewOuterFrame);  
  nsHTMLContainerFrame::CreateViewForFrame(aNewOuterFrame, aContentParent,
                                           PR_FALSE);

  // Create the inner table frame
#ifdef MOZ_MATHML
  if (kNameSpaceID_MathML == aNameSpaceID)
    aNewInnerFrame = NS_NewMathMLmtableFrame(mPresShell, aStyleContext);
  else
#endif
    aNewInnerFrame = NS_NewTableFrame(mPresShell, aStyleContext);
 
  InitAndRestoreFrame(aState, aContent, aNewOuterFrame, nsnull,
                      aNewInnerFrame);

  if (!aIsPseudo) {
    // Put the newly created frames into the right child list
    aNewOuterFrame->SetInitialChildList(nsnull, aNewInnerFrame);

    rv = aState.AddChild(aNewOuterFrame, *frameItems, aContent,
                         aStyleContext, parentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!mInitialContainingBlock) {
      // The frame we're constructing will be the initial containing block.
      // Set mInitialContainingBlock before processing children.
      mInitialContainingBlock = aNewOuterFrame;
    }

    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewInnerFrame, PR_TRUE, childItems,
                         PR_FALSE);
    // XXXbz what about cleaning up?
    if (NS_FAILED(rv)) return rv;

    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(nsnull, aState, aContent, aNewInnerFrame,
                          PR_FALSE, childItems);

    nsFrameItems captionItems;
    PullOutCaptionFrames(childItems, captionItems);

    // Set the inner table frame's initial primary list 
    aNewInnerFrame->SetInitialChildList(nsnull, childItems.childList);

    // Set the outer table frame's secondary childlist lists
    if (captionItems.childList) {
        aNewOuterFrame->SetInitialChildList(nsGkAtoms::captionList,
                                            captionItems.childList);
    }
 }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCaptionFrame(nsFrameConstructorState& aState,
                                                  nsIContent*              aContent,
                                                  nsIFrame*                aParentFrameIn,
                                                  nsStyleContext*          aStyleContext,
                                                  PRInt32                  aNameSpaceID,
                                                  nsFrameItems&            aChildItems,
                                                  nsIFrame*&               aNewFrame,
                                                  PRBool&                  aIsPseudoParent)

{
  nsresult rv = NS_OK;
  if (!aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  // this frame may have a pseudo parent
  GetParentFrame(aNameSpaceID, *aParentFrameIn, 
                 nsGkAtoms::tableCaptionFrame, aState, parentFrame,
                 aIsPseudoParent);
  if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aChildItems);
  }

  aNewFrame = NS_NewTableCaptionFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);

  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                              &haveFirstLetterStyle, &haveFirstLineStyle);

  // The caption frame is a float container
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(aNewFrame, floatSaveState,
                                  haveFirstLetterStyle, haveFirstLineStyle);
  nsFrameItems childItems;
  rv = ProcessChildren(aState, aContent, aNewFrame,
                       PR_TRUE, childItems, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  aNewFrame->SetInitialChildList(nsnull, childItems.childList);
  if (aIsPseudoParent) {
    aState.mPseudoFrames.mTableOuter.mChildList2.AddChild(aNewFrame);
  }
  
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructTableRowGroupFrame(nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsStyleContext*          aStyleContext,
                                                   PRInt32                  aNameSpaceID,
                                                   PRBool                   aIsPseudo,
                                                   nsFrameItems&            aChildItems,
                                                   nsIFrame*&               aNewFrame, 
                                                   PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aNameSpaceID, *aParentFrameIn, 
                   nsGkAtoms::tableRowGroupFrame, aState, parentFrame,
                   aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mRowGroup.mFrame) {
      ProcessPseudoFrames(aState, nsGkAtoms::tableRowGroupFrame);
    }
  }

  const nsStyleDisplay* styleDisplay = aStyleContext->GetStyleDisplay();

  aNewFrame = NS_NewTableRowGroupFrame(mPresShell, aStyleContext);

  nsIFrame* scrollFrame = nsnull;
  if (styleDisplay->IsScrollableOverflow()) {
    // Create an area container for the frame
    BuildScrollFrame(aState, aContent, aStyleContext, aNewFrame, parentFrame,
                     nsnull, scrollFrame, aStyleContext);

  } 
  else {
    if (NS_UNLIKELY(!aNewFrame)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);
  }

  if (!aIsPseudo) {
    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_TRUE, childItems,
                         PR_FALSE);
    
    if (NS_FAILED(rv)) return rv;

    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(nsnull, aState, aContent, aNewFrame,
                          PR_FALSE, childItems);

    aNewFrame->SetInitialChildList(nsnull, childItems.childList);
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
nsCSSFrameConstructor::ConstructTableColGroupFrame(nsFrameConstructorState& aState,
                                                   nsIContent*              aContent,
                                                   nsIFrame*                aParentFrameIn,
                                                   nsStyleContext*          aStyleContext,
                                                   PRInt32                  aNameSpaceID,
                                                   PRBool                   aIsPseudo,
                                                   nsFrameItems&            aChildItems,
                                                   nsIFrame*&               aNewFrame, 
                                                   PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aNameSpaceID, *aParentFrameIn,
                   nsGkAtoms::tableColGroupFrame, aState, parentFrame,
                   aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mColGroup.mFrame) {
      ProcessPseudoFrames(aState, nsGkAtoms::tableColGroupFrame);
    }
  }

  aNewFrame = NS_NewTableColGroupFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);

  if (!aIsPseudo) {
    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_TRUE, childItems,
                         PR_FALSE);
    if (NS_FAILED(rv)) return rv;
    aNewFrame->SetInitialChildList(nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mTableInner.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableRowFrame(nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsStyleContext*          aStyleContext,
                                              PRInt32                  aNameSpaceID,
                                              PRBool                   aIsPseudo,
                                              nsFrameItems&            aChildItems,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aNameSpaceID, *aParentFrameIn,
                   nsGkAtoms::tableRowFrame, aState, parentFrame,
                   aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mRow.mFrame) {
      ProcessPseudoFrames(aState, nsGkAtoms::tableRowFrame);
    }
  }

#ifdef MOZ_MATHML
  if (kNameSpaceID_MathML == aNameSpaceID)
    aNewFrame = NS_NewMathMLmtrFrame(mPresShell, aStyleContext);
  else
#endif
    aNewFrame = NS_NewTableRowFrame(mPresShell, aStyleContext);

  if (NS_UNLIKELY(!aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);
  if (!aIsPseudo) {
    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_TRUE, childItems,
                         PR_FALSE);
    if (NS_FAILED(rv)) return rv;
    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(nsnull, aState, aContent, aNewFrame,
                          PR_FALSE, childItems);

    aNewFrame->SetInitialChildList(nsnull, childItems.childList);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mRowGroup.mChildList.AddChild(aNewFrame);
    }
  }

  return rv;
}
      
nsresult
nsCSSFrameConstructor::ConstructTableColFrame(nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrameIn,
                                              nsStyleContext*          aStyleContext,
                                              PRInt32                  aNameSpaceID,
                                              PRBool                   aIsPseudo,
                                              nsFrameItems&            aChildItems,
                                              nsIFrame*&               aNewFrame,
                                              PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aParentFrameIn || !aStyleContext) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    GetParentFrame(aNameSpaceID, *aParentFrameIn,
                   nsGkAtoms::tableColFrame, aState, parentFrame,
                   aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
  }

  nsTableColFrame* colFrame = NS_NewTableColFrame(mPresShell, aStyleContext);
  aNewFrame = colFrame;
  if (NS_UNLIKELY(!aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);

  // construct additional col frames if the col frame has a span > 1
  PRInt32 span = colFrame->GetSpan();
  nsIFrame* lastCol = aNewFrame;
  nsStyleContext* styleContext = nsnull;
  for (PRInt32 spanX = 1; spanX < span; spanX++) {
    // The same content node should always resolve to the same style context.
    if (1 == spanX)
      styleContext = aNewFrame->GetStyleContext();
    nsTableColFrame* newCol = NS_NewTableColFrame(mPresShell, styleContext);
    if (NS_UNLIKELY(!newCol)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, newCol, PR_FALSE);
    lastCol->SetNextSibling(newCol);
    lastCol->SetNextContinuation(newCol);
    newCol->SetPrevContinuation(lastCol);
    newCol->SetColType(eColAnonymousCol);
    lastCol = newCol;
  }

  if (!aIsPseudo && aIsPseudoParent) {
      aState.mPseudoFrames.mColGroup.mChildList.AddChild(aNewFrame);
  }
  
  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructTableCellFrame(nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrameIn,
                                               nsStyleContext*          aStyleContext,
                                               PRInt32                  aNameSpaceID,
                                               PRBool                   aIsPseudo,
                                               nsFrameItems&            aChildItems,
                                               nsIFrame*&               aNewCellOuterFrame,
                                               nsIFrame*&               aNewCellInnerFrame,
                                               PRBool&                  aIsPseudoParent)
{
  nsresult rv = NS_OK;
  if (!aParentFrameIn) return rv;

  nsIFrame* parentFrame = aParentFrameIn;
  aIsPseudoParent = PR_FALSE;
  if (!aIsPseudo) {
    // this frame may have a pseudo parent
    // use nsGkAtoms::tableCellFrame which will match if it is really nsGkAtoms::bcTableCellFrame
    GetParentFrame(aNameSpaceID, *aParentFrameIn,
                   nsGkAtoms::tableCellFrame, aState, parentFrame,
                   aIsPseudoParent);
    if (!aIsPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aChildItems);
    }
    if (!aIsPseudo && aIsPseudoParent && aState.mPseudoFrames.mCellOuter.mFrame) {
      ProcessPseudoFrames(aState, nsGkAtoms::tableCellFrame);
    }
  }
#ifdef MOZ_MATHML
  // <mtable> is border separate in mathml.css and the MathML code doesn't implement
  // border collapse. For those users who style <mtable> with border collapse,
  // give them the default non-MathML table frames that understand border collpase.
  // This won't break us because MathML table frames are all subclasses of the default
  // table code, and so we can freely mix <mtable> with <mtr> or <tr>, <mtd> or <td>.
  // What will happen is just that non-MathML frames won't understand MathML attributes
  // and will therefore miss the special handling that the MathML code does.
  if (kNameSpaceID_MathML == aNameSpaceID && !IsBorderCollapse(parentFrame))
    aNewCellOuterFrame = NS_NewMathMLmtdFrame(mPresShell, aStyleContext);
  else
#endif
    // Warning: If you change this and add a wrapper frame around table cell
    // frames, make sure Bug 368554 doesn't regress!
    // See IsInAutoWidthTableCellForQuirk() in nsImageFrame.cpp.    
    aNewCellOuterFrame = NS_NewTableCellFrame(mPresShell, aStyleContext,
                                              IsBorderCollapse(parentFrame));

  if (NS_UNLIKELY(!aNewCellOuterFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Initialize the table cell frame
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewCellOuterFrame);
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewCellOuterFrame, nsnull, PR_FALSE);
  
  // Resolve pseudo style and initialize the body cell frame
  nsRefPtr<nsStyleContext> innerPseudoStyle;
  innerPseudoStyle = mPresShell->StyleSet()->
    ResolvePseudoStyleFor(aContent,
                          nsCSSAnonBoxes::cellContent, aStyleContext);

  // Create a block frame that will format the cell's content
  PRBool isBlock;
#ifdef MOZ_MATHML
  if (kNameSpaceID_MathML == aNameSpaceID) {
    aNewCellInnerFrame = NS_NewMathMLmtdInnerFrame(mPresShell, innerPseudoStyle);
    isBlock = PR_FALSE;
  }
  else
#endif
  {
    aNewCellInnerFrame = NS_NewTableCellInnerFrame(mPresShell, innerPseudoStyle);
    isBlock = PR_TRUE;
  }


  if (NS_UNLIKELY(!aNewCellInnerFrame)) {
    aNewCellOuterFrame->Destroy();
    aNewCellOuterFrame = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InitAndRestoreFrame(aState, aContent, aNewCellOuterFrame, nsnull, aNewCellInnerFrame);

  if (!aIsPseudo) {
    PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
    if (isBlock) {
      ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                                  &haveFirstLetterStyle, &haveFirstLineStyle);
    }

    // The block frame is a float container
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(isBlock ? aNewCellInnerFrame : nsnull,
                                    floatSaveState,
                                    haveFirstLetterStyle, haveFirstLineStyle);

    // Process the child content
    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewCellInnerFrame, 
                         PR_TRUE, childItems, isBlock);

    if (NS_FAILED(rv)) {
      // Clean up
      // XXXbz kids of this stuff need to be cleaned up too!
      aNewCellInnerFrame->Destroy();
      aNewCellInnerFrame = nsnull;
      aNewCellOuterFrame->Destroy();
      aNewCellOuterFrame = nsnull;
      return rv;
    }

    aNewCellInnerFrame->SetInitialChildList(nsnull, childItems.childList);
    aNewCellOuterFrame->SetInitialChildList(nsnull, aNewCellInnerFrame);
    if (aIsPseudoParent) {
      aState.mPseudoFrames.mRow.mChildList.AddChild(aNewCellOuterFrame);
    }
  }

  return rv;
}

static PRBool 
NeedFrameFor(nsIFrame*   aParentFrame,
             nsIContent* aChildContent) 
{
  // don't create a whitespace frame if aParentFrame doesn't want it.
  // always create frames for children in generated content. counter(),
  // quotes, and attr() content can easily change dynamically and we don't
  // want to be reconstructing frames. It's not even clear that these
  // should be considered ignorable just because they evaluate to
  // whitespace.
  return !aParentFrame->IsFrameOfType(nsIFrame::eExcludesIgnorableWhitespace)
    || !TextIsOnlyWhitespace(aChildContent)
    || aParentFrame->IsGeneratedContentFrame();
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
 * @return if scroll was propagated from some content node, the content node it
 *         was propagated from.
 */
nsIContent*
nsCSSFrameConstructor::PropagateScrollToViewport()
{
  // Set default
  nsPresContext* presContext = mPresShell->GetPresContext();
  presContext->SetViewportOverflowOverride(NS_STYLE_OVERFLOW_AUTO,
                                           NS_STYLE_OVERFLOW_AUTO);

  // We never mess with the viewport scroll state
  // when printing or in print preview
  if (presContext->IsPaginated()) {
    return nsnull;
  }

  nsIContent* docElement = mDocument->GetRootContent();

  // Check the style on the document root element
  nsStyleSet *styleSet = mPresShell->StyleSet();
  nsRefPtr<nsStyleContext> rootStyle;
  rootStyle = styleSet->ResolveStyleFor(docElement, nsnull);
  if (!rootStyle) {
    return nsnull;
  }
  if (CheckOverflow(presContext, rootStyle->GetStyleDisplay())) {
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
  if (!htmlDoc || !docElement->IsNodeOfType(nsINode::eHTML)) {
    return nsnull;
  }
  
  nsCOMPtr<nsIDOMHTMLElement> body;
  htmlDoc->GetBody(getter_AddRefs(body));
  nsCOMPtr<nsIContent> bodyElement = do_QueryInterface(body);
  
  if (!bodyElement ||
      !bodyElement->NodeInfo()->Equals(nsGkAtoms::body)) {
    // The body is not a <body> tag, it's a <frameset>.
    return nsnull;
  }

  nsRefPtr<nsStyleContext> bodyStyle;
  bodyStyle = styleSet->ResolveStyleFor(bodyElement, rootStyle);
  if (!bodyStyle) {
    return nsnull;
  }

  if (CheckOverflow(presContext, bodyStyle->GetStyleDisplay())) {
    // tell caller we stole the overflow style from the body element
    return bodyElement;
  }

  return nsnull;
}

/**
 * New one
 */
nsresult
nsCSSFrameConstructor::ConstructDocElementFrame(nsFrameConstructorState& aState,
                                                nsIContent*              aDocElement,
                                                nsIFrame*                aParentFrame,
                                                nsIFrame**               aNewFrame)
{
  *aNewFrame = nsnull;

  if (!mTempFrameTreeState)
    aState.mPresShell->CaptureHistoryState(getter_AddRefs(mTempFrameTreeState));

  // ----- reattach gfx scrollbars ------
  // Gfx scrollframes were created in the root frame but the primary frame map may have been destroyed if a 
  // new style sheet was loaded so lets reattach the frames to their content.
  // XXX this seems truly bogus, we wipe out mGfxScrollFrame below
  if (mGfxScrollFrame) {
    nsIFrame* gfxScrollbarFrame1 = mGfxScrollFrame->GetFirstChild(nsnull);
    if (gfxScrollbarFrame1) {
      // XXX This works, but why?
      aState.mFrameManager->
        SetPrimaryFrameFor(gfxScrollbarFrame1->GetContent(), gfxScrollbarFrame1);

      nsIFrame* gfxScrollbarFrame2 = gfxScrollbarFrame1->GetNextSibling();
      if (gfxScrollbarFrame2) {
        // XXX This works, but why?
        aState.mFrameManager->
          SetPrimaryFrameFor(gfxScrollbarFrame2->GetContent(), gfxScrollbarFrame2);
      }
    }
  }

  // --------- CREATE AREA OR BOX FRAME -------
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = mPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                         nsnull);

  const nsStyleDisplay* display = styleContext->GetStyleDisplay();

  // Ensure that our XBL bindings are installed.
  if (display->mBinding) {
    // Get the XBL loader.
    nsresult rv;
    PRBool resolveStyle;
    
    nsIXBLService * xblService = GetXBLService();
    if (!xblService)
      return NS_ERROR_FAILURE;

    nsRefPtr<nsXBLBinding> binding;
    rv = xblService->LoadBindings(aDocElement, display->mBinding->mURI,
                                  display->mBinding->mOriginPrincipal,
                                  PR_FALSE, getter_AddRefs(binding),
                                  &resolveStyle);
    if (NS_FAILED(rv))
      return NS_OK; // Binding will load asynchronously.

    if (binding) {
      mDocument->BindingManager()->AddToAttachedQueue(binding);
    }

    if (resolveStyle) {
      styleContext = mPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                             nsnull);
      display = styleContext->GetStyleDisplay();
    }
  }

  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

#ifdef DEBUG
  PRBool propagatedScrollToViewport =
    PropagateScrollToViewport() == aDocElement;

  NS_ASSERTION(!display->IsScrollableOverflow() || 
               aState.mPresContext->IsPaginated() ||
               propagatedScrollToViewport,
               "Scrollbars should have been propagated to the viewport");
#endif

  if (NS_UNLIKELY(display->mDisplay == NS_STYLE_DISPLAY_NONE)) {
    mInitialContainingBlock = nsnull;
    mRootElementStyleFrame = nsnull;
    return NS_OK;
  }

  nsFrameConstructorSaveState absoluteSaveState;
  if (mHasRootAbsPosContainingBlock) {
    // Push the absolute containing block now so we can absolutely position
    // the root element
    aState.PushAbsoluteContainingBlock(mDocElementContainingBlock, absoluteSaveState);
  }

  nsresult rv;

  // The rules from CSS 2.1, section 9.2.4, have already been applied
  // by the style system, so we can assume that display->mDisplay is
  // either NONE, BLOCK, or TABLE.

  PRBool docElemIsTable = (display->mDisplay == NS_STYLE_DISPLAY_TABLE) &&
                          !IsSpecialContent(aDocElement, aDocElement->Tag(),
                                            aDocElement->GetNameSpaceID(),
                                            styleContext);

  // contentFrame is the primary frame for the root element. *aNewFrame
  // is the frame that will be the child of the initial containing block.
  // These are usually the same frame but they can be different, in
  // particular if the root frame is positioned, in which case
  // contentFrame is the out-of-flow frame and *aNewFrame is the
  // placeholder.
  nsIFrame* contentFrame;
  PRBool processChildren = PR_FALSE;
  if (docElemIsTable) {
    nsIFrame* innerTableFrame;
    nsFrameItems frameItems;
    // if the document is a table then just populate it.
    rv = ConstructTableFrame(aState, aDocElement,
                             aParentFrame, styleContext,
                             kNameSpaceID_None, PR_FALSE, frameItems, contentFrame,
                             innerTableFrame);
    if (NS_FAILED(rv))
      return rv;
    if (!contentFrame || !frameItems.childList)
      return NS_ERROR_FAILURE;
    *aNewFrame = frameItems.childList;
    NS_ASSERTION(!frameItems.childList->GetNextSibling(),
                 "multiple root element frames");
  } else {
    // otherwise build a box or a block
#ifdef MOZ_XUL
    if (aDocElement->IsNodeOfType(nsINode::eXUL)) {
      contentFrame = NS_NewDocElementBoxFrame(mPresShell, styleContext);
      if (NS_UNLIKELY(!contentFrame)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      InitAndRestoreFrame(aState, aDocElement, aParentFrame, nsnull, contentFrame);
      *aNewFrame = contentFrame;
      processChildren = PR_TRUE;
    }
    else
#endif 
#ifdef MOZ_SVG
    if (aDocElement->GetNameSpaceID() == kNameSpaceID_SVG) {
      if (aDocElement->Tag() == nsGkAtoms::svg && NS_SVGEnabled()) {
        contentFrame = NS_NewSVGOuterSVGFrame(mPresShell, aDocElement, styleContext);
        if (NS_UNLIKELY(!contentFrame)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        InitAndRestoreFrame(aState, aDocElement,
                            aState.GetGeometricParent(display, aParentFrame),
                            nsnull, contentFrame);

        // AddChild takes care of transforming the frame tree for fixed-pos
        // or abs-pos situations
        nsFrameItems frameItems;
        rv = aState.AddChild(contentFrame, frameItems, aDocElement,
                             styleContext, aParentFrame);
        if (NS_FAILED(rv) || !frameItems.childList) {
          return rv;
        }
        *aNewFrame = frameItems.childList;
        processChildren = PR_TRUE;

        // See if we need to create a view, e.g. the frame is absolutely positioned
        nsHTMLContainerFrame::CreateViewForFrame(contentFrame, aParentFrame, PR_FALSE);
      } else {
        return NS_ERROR_FAILURE;
      }
    }
    else 
#endif
    {
      contentFrame = NS_NewBlockFrame(mPresShell, styleContext,
        NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
      if (!contentFrame)
        return NS_ERROR_OUT_OF_MEMORY;
      nsFrameItems frameItems;
      rv = ConstructBlock(aState, display, aDocElement,
                          aState.GetGeometricParent(display, aParentFrame),
                          aParentFrame, styleContext, &contentFrame,
                          frameItems, display->IsPositioned());
      if (NS_FAILED(rv) || !frameItems.childList)
        return rv;
      *aNewFrame = frameItems.childList;
      NS_ASSERTION(!frameItems.childList->GetNextSibling(),
                   "multiple root element frames");
    }
  }

  // set the primary frame
  aState.mFrameManager->SetPrimaryFrameFor(aDocElement, contentFrame);

  NS_ASSERTION(processChildren ? !mInitialContainingBlock :
                 mInitialContainingBlock == contentFrame,
               "unexpected mInitialContainingBlock");
  mInitialContainingBlock = contentFrame;

  // Figure out which frame has the main style for the document element,
  // assigning it to mRootElementStyleFrame.
  // Backgrounds should be propagated from that frame to the viewport.
  PRBool isChild;
  contentFrame->GetParentStyleContextFrame(aState.mPresContext,
          &mRootElementStyleFrame, &isChild);
  if (!isChild) {
    mRootElementStyleFrame = mInitialContainingBlock;
  }

  if (processChildren) {
    // Still need to process the child content
    nsFrameItems childItems;

    // Create any anonymous frames the doc element frame requires
    // This must happen before ProcessChildren to ensure that popups are
    // never constructed before the popupset.
    CreateAnonymousFrames(nsnull, aState, aDocElement, contentFrame,
                          PR_FALSE, childItems, PR_TRUE);
    NS_ASSERTION(!nsLayoutUtils::GetAsBlock(contentFrame),
                 "Only XUL and SVG frames should reach here");
    ProcessChildren(aState, aDocElement, contentFrame, PR_TRUE, childItems,
                    PR_FALSE);

    // Set the initial child lists
    contentFrame->SetInitialChildList(nsnull, childItems.childList);
  }

  return NS_OK;
}


nsresult
nsCSSFrameConstructor::ConstructRootFrame(nsIContent*     aDocElement,
                                          nsIFrame**      aNewFrame)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(aNewFrame, "null out param");
  
    /*
       how the root frame hierarchy should look

  Galley presentation, non-XUL, with scrolling (i.e. not a frameset):
  
      ViewportFrame [fixed-cb]
        nsHTMLScrollFrame
          CanvasFrame [abs-cb]
            root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                nsTableOuterFrame, nsPlaceholderFrame)

  Galley presentation, non-XUL, without scrolling (i.e. a frameset):
  
      ViewportFrame [fixed-cb]
        CanvasFrame [abs-cb]
          root element frame (nsBlockFrame)

  Galley presentation, XUL
  
      ViewportFrame [fixed-cb]
        nsRootBoxFrame
          root element frame (nsDocElementBoxFrame)

  Print presentation, non-XUL

      ViewportFrame
        nsSimplePageSequenceFrame
          nsPageFrame [fixed-cb]
            nsPageContentFrame
              CanvasFrame [abs-cb]
                root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                    nsTableOuterFrame, nsPlaceholderFrame)

  Print-preview presentation, non-XUL

      ViewportFrame
        nsHTMLScrollFrame
          nsSimplePageSequenceFrame
            nsPageFrame [fixed-cb]
              nsPageContentFrame
                CanvasFrame [abs-cb]
                  root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                      nsTableOuterFrame, nsPlaceholderFrame)

  Print/print preview of XUL is not supported.
  [fixed-cb]: the default containing block for fixed-pos content
  [abs-cb]: the default containing block for abs-pos content
 
  Meaning of nsCSSFrameConstructor fields:
    mInitialContainingBlock is "root element frame".
    mDocElementContainingBlock is the parent of mInitialContainingBlock
      (i.e. CanvasFrame or nsRootBoxFrame)
    mFixedContainingBlock is the [fixed-cb]
    mGfxScrollFrame is the nsHTMLScrollFrame mentioned above, or null if there isn't one
    mPageSequenceFrame is the nsSimplePageSequenceFrame, or null if there isn't one
*/

  // Set up our style rule observer.
  {
    mPresShell->StyleSet()->SetBindingManager(mDocument->BindingManager());
  }

  // --------- BUILD VIEWPORT -----------
  nsIFrame*                 viewportFrame = nsnull;
  nsRefPtr<nsStyleContext> viewportPseudoStyle;
  nsStyleSet *styleSet = mPresShell->StyleSet();

  viewportPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                        nsCSSAnonBoxes::viewport,
                                                        nsnull);

  viewportFrame = NS_NewViewportFrame(mPresShell, viewportPseudoStyle);

  nsPresContext* presContext = mPresShell->GetPresContext();

  // XXXbz do we _have_ to pass a null content pointer to that frame?
  // Would it really kill us to pass in the root element or something?
  // What would that break?
  viewportFrame->Init(nsnull, nsnull, nsnull);

  // Bind the viewport frame to the root view
  nsIViewManager* viewManager = mPresShell->GetViewManager();
  nsIView*        rootView;

  viewManager->GetRootView(rootView);
  viewportFrame->SetView(rootView);

  nsContainerFrame::SyncFrameViewProperties(presContext, viewportFrame,
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

  PRBool isPaginated = presContext->IsRootPaginatedDocument();

  nsIFrame* rootFrame = nsnull;
  nsIAtom* rootPseudo;
        
  if (!isPaginated) {
#ifdef MOZ_XUL
    if (aDocElement->IsNodeOfType(nsINode::eXUL))
    {
      // pass a temporary stylecontext, the correct one will be set later
      rootFrame = NS_NewRootBoxFrame(mPresShell, viewportPseudoStyle);
    } else
#endif
    {
      // pass a temporary stylecontext, the correct one will be set later
      rootFrame = NS_NewCanvasFrame(mPresShell, viewportPseudoStyle);
      mHasRootAbsPosContainingBlock = PR_TRUE;
    }

    rootPseudo = nsCSSAnonBoxes::canvas;
    mDocElementContainingBlock = rootFrame;
  } else {
    // Create a page sequence frame
    rootFrame = NS_NewSimplePageSequenceFrame(mPresShell, viewportPseudoStyle);
    mPageSequenceFrame = rootFrame;
    rootPseudo = nsCSSAnonBoxes::pageSequence;
  }


  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

  // If the device supports scrolling (e.g., in galley mode on the screen and
  // for print-preview, but not when printing), then create a scroll frame that
  // will act as the scrolling mechanism for the viewport. 
  // XXX Do we even need a viewport when printing to a printer?

  // As long as the webshell doesn't prohibit it, and the device supports
  // it, create a scroll frame that will act as the scolling mechanism for
  // the viewport.
  //
  // Threre are three possible values stored in the docshell:
  //  1) nsIScrollable::Scrollbar_Never = no scrollbars
  //  2) nsIScrollable::Scrollbar_Auto = scrollbars appear if needed
  //  3) nsIScrollable::Scrollbar_Always = scrollbars always
  // Only need to create a scroll frame/view for cases 2 and 3.

  PRBool isHTML = aDocElement->IsNodeOfType(nsINode::eHTML);
  PRBool isXUL = PR_FALSE;

  if (!isHTML) {
    isXUL = aDocElement->IsNodeOfType(nsINode::eXUL);
  }

  // Never create scrollbars for XUL documents
  PRBool isScrollable = !isXUL;

  // Never create scrollbars for frameset documents.
  if (isHTML) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc && htmlDoc->GetIsFrameset())
      isScrollable = PR_FALSE;
  }

  if (isPaginated) {
    isScrollable = presContext->HasPaginatedScrolling();
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
  nsFrameConstructorState state(mPresShell, nsnull, nsnull, nsnull);

  nsIFrame* parentFrame = viewportFrame;

  // If paginated, make sure we don't put scrollbars in
  if (!isScrollable) {
    rootPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                      rootPseudo,
                                                      viewportPseudoStyle);
  } else {
      if (rootPseudo == nsCSSAnonBoxes::canvas) {
        rootPseudo = nsCSSAnonBoxes::scrolledCanvas;
      } else {
        NS_ASSERTION(rootPseudo == nsCSSAnonBoxes::pageSequence,
                     "Unknown root pseudo");
        rootPseudo = nsCSSAnonBoxes::scrolledPageSequence;
      }

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
      newFrame = nsnull;
      rootPseudoStyle = BeginBuildingScrollFrame( state,
                                                  aDocElement,
                                                  styleContext,
                                                  viewportFrame,
                                                  nsnull,
                                                  rootPseudo,
                                                  PR_TRUE,
                                                  newFrame);

      nsIScrollableFrame* scrollable;
      CallQueryInterface(newFrame, &scrollable);
      NS_ENSURE_TRUE(scrollable, NS_ERROR_FAILURE);

      nsIScrollableView* scrollableView = scrollable->GetScrollableView();
      NS_ENSURE_TRUE(scrollableView, NS_ERROR_FAILURE);

      viewManager->SetRootScrollableView(scrollableView);
      parentFrame = newFrame;

      mGfxScrollFrame = newFrame;
  }
  
  rootFrame->SetStyleContextWithoutNotification(rootPseudoStyle);
  rootFrame->Init(aDocElement, parentFrame, nsnull);
  
  if (isScrollable) {
    FinishBuildingScrollFrame(parentFrame, rootFrame);
  }
  
  if (isPaginated) { // paginated
    // Create the first page
    // Set the initial child lists
    nsIFrame *pageFrame, *canvasFrame;
    ConstructPageFrame(mPresShell, presContext, rootFrame, nsnull,
                       pageFrame, canvasFrame);
    rootFrame->SetInitialChildList(nsnull, pageFrame);

    // The eventual parent of the document element frame.
    // XXX should this be set for every new page (in ConstructPageFrame)?
    mDocElementContainingBlock = canvasFrame;
    mHasRootAbsPosContainingBlock = PR_TRUE;
  }

  viewportFrame->SetInitialChildList(nsnull, newFrame);
  
  *aNewFrame = viewportFrame;

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructPageFrame(nsIPresShell*  aPresShell,
                                          nsPresContext* aPresContext,
                                          nsIFrame*      aParentFrame,
                                          nsIFrame*      aPrevPageFrame,
                                          nsIFrame*&     aPageFrame,
                                          nsIFrame*&     aCanvasFrame)
{
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();
  nsStyleSet *styleSet = aPresShell->StyleSet();

  nsRefPtr<nsStyleContext> pagePseudoStyle;
  pagePseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                    nsCSSAnonBoxes::page,
                                                    parentStyleContext);

  aPageFrame = NS_NewPageFrame(aPresShell, pagePseudoStyle);
  if (NS_UNLIKELY(!aPageFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  // Initialize the page frame and force it to have a view. This makes printing of
  // the pages easier and faster.
  aPageFrame->Init(nsnull, aParentFrame, aPrevPageFrame);

  nsRefPtr<nsStyleContext> pageContentPseudoStyle;
  pageContentPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                           nsCSSAnonBoxes::pageContent,
                                                           pagePseudoStyle);

  nsIFrame* pageContentFrame = NS_NewPageContentFrame(aPresShell, pageContentPseudoStyle);
  if (NS_UNLIKELY(!pageContentFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  // Initialize the page content frame and force it to have a view. Also make it the
  // containing block for fixed elements which are repeated on every page.
  nsIFrame* prevPageContentFrame = nsnull;
  if (aPrevPageFrame) {
    prevPageContentFrame = aPrevPageFrame->GetFirstChild(nsnull);
    NS_ASSERTION(prevPageContentFrame, "missing page content frame");
  }
  pageContentFrame->Init(nsnull, aPageFrame, prevPageContentFrame);
  aPageFrame->SetInitialChildList(nsnull, pageContentFrame);
  mFixedContainingBlock = pageContentFrame;

  nsRefPtr<nsStyleContext> canvasPseudoStyle;
  canvasPseudoStyle = styleSet->ResolvePseudoStyleFor(nsnull,
                                                      nsCSSAnonBoxes::canvas,
                                                      pageContentPseudoStyle);

  aCanvasFrame = NS_NewCanvasFrame(aPresShell, canvasPseudoStyle);
  if (NS_UNLIKELY(!aCanvasFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  nsIFrame* prevCanvasFrame = nsnull;
  if (prevPageContentFrame) {
    prevCanvasFrame = prevPageContentFrame->GetFirstChild(nsnull);
    NS_ASSERTION(prevCanvasFrame, "missing canvas frame");
  }
  aCanvasFrame->Init(nsnull, pageContentFrame, prevCanvasFrame);
  pageContentFrame->SetInitialChildList(nsnull, aCanvasFrame);

  return NS_OK;
}

/* static */
nsresult
nsCSSFrameConstructor::CreatePlaceholderFrameFor(nsIPresShell*    aPresShell, 
                                                 nsIContent*      aContent,
                                                 nsIFrame*        aFrame,
                                                 nsStyleContext*  aStyleContext,
                                                 nsIFrame*        aParentFrame,
                                                 nsIFrame*        aPrevInFlow,
                                                 nsIFrame**       aPlaceholderFrame)
{
  nsRefPtr<nsStyleContext> placeholderStyle = aPresShell->StyleSet()->
    ResolveStyleForNonElement(aStyleContext->GetParent());
  
  // The placeholder frame gets a pseudo style context
  nsPlaceholderFrame* placeholderFrame =
    (nsPlaceholderFrame*)NS_NewPlaceholderFrame(aPresShell, placeholderStyle);

  if (placeholderFrame) {
    placeholderFrame->Init(aContent, aParentFrame, aPrevInFlow);
  
    // The placeholder frame has a pointer back to the out-of-flow frame
    placeholderFrame->SetOutOfFlowFrame(aFrame);
  
    aFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);

    // Add mapping from absolutely positioned frame to its placeholder frame
    aPresShell->FrameManager()->RegisterPlaceholderFrame(placeholderFrame);

    *aPlaceholderFrame = static_cast<nsIFrame*>(placeholderFrame);
    
    return NS_OK;
  }
  else {
    return NS_ERROR_OUT_OF_MEMORY;
  }
}

nsresult
nsCSSFrameConstructor::ConstructRadioControlFrame(nsIFrame**      aNewFrame,
                                                  nsIContent*     aContent,
                                                  nsStyleContext* aStyleContext)
{
  *aNewFrame = NS_NewGfxRadioControlFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!*aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<nsStyleContext> radioStyle;
  radioStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                             nsCSSAnonBoxes::radio,
                                                             aStyleContext);
  nsIRadioControlFrame* radio = nsnull;
  if (*aNewFrame && NS_SUCCEEDED(CallQueryInterface(*aNewFrame, &radio))) {
    radio->SetRadioButtonFaceStyleContext(radioStyle);
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructCheckboxControlFrame(nsIFrame**      aNewFrame,
                                                     nsIContent*     aContent,
                                                     nsStyleContext* aStyleContext)
{
  *aNewFrame = NS_NewGfxCheckboxControlFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!*aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<nsStyleContext> checkboxStyle;
  checkboxStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                                nsCSSAnonBoxes::check, 
                                                                aStyleContext);
  nsICheckboxControlFrame* checkbox = nsnull;
  if (*aNewFrame && NS_SUCCEEDED(CallQueryInterface(*aNewFrame, &checkbox))) {
    checkbox->SetCheckboxFaceStyleContext(checkboxStyle);
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructButtonFrame(nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsStyleContext*          aStyleContext,
                                            nsIFrame**               aNewFrame,
                                            const nsStyleDisplay*    aStyleDisplay,
                                            nsFrameItems&            aFrameItems,
                                            PRBool                   aHasPseudoParent)
{
  if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aFrameItems); 
  }      

  *aNewFrame = nsnull;
  nsIFrame* buttonFrame = nsnull;
  
  if (nsGkAtoms::button == aTag) {
    buttonFrame = NS_NewHTMLButtonControlFrame(mPresShell, aStyleContext);
  }
  else {
    buttonFrame = NS_NewGfxButtonControlFrame(mPresShell, aStyleContext);
  }
  if (NS_UNLIKELY(!buttonFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // Initialize the button frame
  nsresult rv = InitAndRestoreFrame(aState, aContent,
                                    aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                                    nsnull, buttonFrame);
  if (NS_FAILED(rv)) {
    buttonFrame->Destroy();
    return rv;
  }
  // See if we need to create a view, e.g. the frame is absolutely positioned
  nsHTMLContainerFrame::CreateViewForFrame(buttonFrame, aParentFrame, PR_FALSE);

  
  
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = mPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                               nsCSSAnonBoxes::buttonContent,
                                                               aStyleContext);
                                                               
  nsIFrame* blockFrame = NS_NewBlockFrame(mPresShell, styleContext,
                                          NS_BLOCK_SPACE_MGR);

  if (NS_UNLIKELY(!blockFrame)) {
    buttonFrame->Destroy();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = InitAndRestoreFrame(aState, aContent, buttonFrame, nsnull, blockFrame);
  if (NS_FAILED(rv)) {
    blockFrame->Destroy();
    buttonFrame->Destroy();
    return rv;
  }

  rv = aState.AddChild(buttonFrame, aFrameItems, aContent, aStyleContext,
                       aParentFrame);
  if (NS_FAILED(rv)) {
    blockFrame->Destroy();
    buttonFrame->Destroy();
    return rv;
  }

  
  if (!buttonFrame->IsLeaf()) { 
    // input type="button" have only anonymous content
    // The area frame is a float container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                                &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(blockFrame, floatSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;

    if (aStyleDisplay->IsPositioned()) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(blockFrame, absoluteSaveState);
    }

    rv = ProcessChildren(aState, aContent, blockFrame, PR_TRUE, childItems,
                         buttonFrame->GetStyleDisplay()->IsBlockOutside());
    if (NS_FAILED(rv)) return rv;
  
    // Set the areas frame's initial child lists
    blockFrame->SetInitialChildList(nsnull, childItems.childList);
  }

  buttonFrame->SetInitialChildList(nsnull, blockFrame);

  nsFrameItems  anonymousChildItems;
  // if there are any anonymous children create frames for them
  CreateAnonymousFrames(aTag, aState, aContent, buttonFrame,
                          PR_FALSE, anonymousChildItems);
  if (anonymousChildItems.childList) {
    // the anonymous content is already parented to the area frame
    aState.mFrameManager->AppendFrames(blockFrame, nsnull,
                                       anonymousChildItems.childList);
  }

  // our new button frame returned is the top frame. 
  *aNewFrame = buttonFrame; 

  return NS_OK;  
}

nsresult
nsCSSFrameConstructor::ConstructSelectFrame(nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            nsStyleContext*          aStyleContext,
                                            nsIFrame*&               aNewFrame,
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
        // The drop-down list's frame is created explicitly. The combobox frame shares its content
        // with the drop-down list.
      PRUint32 flags = NS_BLOCK_SPACE_MGR;
      nsIFrame* comboboxFrame = NS_NewComboboxControlFrame(mPresShell, aStyleContext, flags);

      // Save the history state so we don't restore during construction
      // since the complete tree is required before we restore.
      nsILayoutHistoryState *historyState = aState.mFrameState;
      aState.mFrameState = nsnull;
      // Initialize the combobox frame
      InitAndRestoreFrame(aState, aContent,
                          aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                          nsnull, comboboxFrame);

      nsHTMLContainerFrame::CreateViewForFrame(comboboxFrame, aParentFrame, PR_FALSE);

      rv = aState.AddChild(comboboxFrame, aFrameItems, aContent, aStyleContext,
                           aParentFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
      
      ///////////////////////////////////////////////////////////////////
      // Combobox - Old Native Implementation
      ///////////////////////////////////////////////////////////////////
      nsIComboboxControlFrame* comboBox = nsnull;
      CallQueryInterface(comboboxFrame, &comboBox);
      NS_ASSERTION(comboBox, "NS_NewComboboxControlFrame returned frame that "
                             "doesn't implement nsIComboboxControlFrame");

        // Resolve pseudo element style for the dropdown list
      nsRefPtr<nsStyleContext> listStyle;
      listStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                                nsCSSAnonBoxes::dropDownList, 
                                                                aStyleContext);

        // Create a listbox
      nsIFrame* listFrame = NS_NewListControlFrame(mPresShell, listStyle);

        // Notify the listbox that it is being used as a dropdown list.
      nsIListControlFrame * listControlFrame;
      rv = CallQueryInterface(listFrame, &listControlFrame);
      if (NS_SUCCEEDED(rv)) {
        listControlFrame->SetComboboxFrame(comboboxFrame);
      }
         // Notify combobox that it should use the listbox as it's popup
      comboBox->SetDropDown(listFrame);

      NS_ASSERTION(!listStyle->GetStyleDisplay()->IsPositioned(),
                   "Ended up with positioned dropdown list somehow.");
      NS_ASSERTION(!listStyle->GetStyleDisplay()->IsFloating(),
                   "Ended up with floating dropdown list somehow.");
      
      // Initialize the scroll frame positioned. Note that it is NOT
      // initialized as absolutely positioned.
      nsIFrame* scrolledFrame = NS_NewSelectsAreaFrame(mPresShell, aStyleContext, flags);

      InitializeSelectFrame(aState, listFrame, scrolledFrame, aContent,
                            comboboxFrame, listStyle, PR_TRUE, aFrameItems);

        // Set flag so the events go to the listFrame not child frames.
        // XXX: We should replace this with a real widget manager similar
        // to how the nsFormControlFrame works. Re-directing events is a temporary Kludge.
      NS_ASSERTION(listFrame->GetView(), "ListFrame's view is nsnull");
      //listFrame->GetView()->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

      // Create display and button frames from the combobox's anonymous content.
      // The anonymous content is appended to existing anonymous content for this
      // element (the scrollbars).

      nsFrameItems childItems;
      CreateAnonymousFrames(nsGkAtoms::combobox, aState, aContent,
                            comboboxFrame, PR_TRUE, childItems);
  
      comboboxFrame->SetInitialChildList(nsnull, childItems.childList);

      // Initialize the additional popup child list which contains the
      // dropdown list frame.
      nsFrameItems popupItems;
      popupItems.AddChild(listFrame);
      comboboxFrame->SetInitialChildList(nsGkAtoms::selectPopupList,
                                         popupItems.childList);

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
      nsIFrame* listFrame = NS_NewListControlFrame(mPresShell, aStyleContext);
      if (listFrame) {
        rv = NS_OK;
      }
      else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }

      nsIFrame* scrolledFrame = NS_NewSelectsAreaFrame(
        mPresShell, aStyleContext, NS_BLOCK_SPACE_MGR);

      // ******* this code stolen from Initialze ScrollFrame ********
      // please adjust this code to use BuildScrollFrame.

      InitializeSelectFrame(aState, listFrame, scrolledFrame, aContent,
                            aParentFrame, aStyleContext, PR_FALSE, aFrameItems);

      aNewFrame = listFrame;

      aFrameHasBeenInitialized = PR_TRUE;
    }
  }
  return rv;

}

/**
 * Used to be InitializeScrollFrame but now it's only used for the select tag
 * But the select tag should really be fixed to use GFX scrollbars that can
 * be create with BuildScrollFrame.
 */
nsresult
nsCSSFrameConstructor::InitializeSelectFrame(nsFrameConstructorState& aState,
                                             nsIFrame*                scrollFrame,
                                             nsIFrame*                scrolledFrame,
                                             nsIContent*              aContent,
                                             nsIFrame*                aParentFrame,
                                             nsStyleContext*          aStyleContext,
                                             PRBool                   aBuildCombobox,
                                             nsFrameItems&            aFrameItems)
{
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // Initialize it
  nsIFrame* geometricParent = aState.GetGeometricParent(display, aParentFrame);
    
  // We don't call InitAndRestoreFrame for scrollFrame because we can only
  // restore the frame state after its parts have been created (in particular,
  // the scrollable view). So we have to split Init and Restore.

  // Initialize the frame
  scrollFrame->Init(aContent, geometricParent, nsnull);

  if (!aBuildCombobox) {
    nsresult rv = aState.AddChild(scrollFrame, aFrameItems, aContent,
                                  aStyleContext, aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
      
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

#if defined(XP_MACOSX) || defined(XP_BEOS) 
      static NS_DEFINE_IID(kCPopUpCID,  NS_POPUP_CID);
      view->CreateWidget(kCPopUpCID, &widgetData, nsnull);
#else
      static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
      view->CreateWidget(kCChildCID, &widgetData, nsnull);
#endif
    }
  }

  nsStyleContext* scrolledPseudoStyle;
  BuildScrollFrame(aState, aContent, aStyleContext, scrolledFrame,
                   geometricParent, aParentFrame, scrollFrame,
                   scrolledPseudoStyle);

  if (aState.mFrameState && aState.mFrameManager) {
    // Restore frame state for the scroll frame
    aState.mFrameManager->RestoreFrameStateFor(scrollFrame, aState.mFrameState);
  }

  // The area frame is a float container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                              &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(scrolledFrame, floatSaveState,
                                  haveFirstLetterStyle, haveFirstLineStyle);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;

  if (display->IsPositioned()) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    aState.PushAbsoluteContainingBlock(scrolledFrame, absoluteSaveState);
  }

  ProcessChildren(aState, aContent, scrolledFrame, PR_FALSE,
                  childItems, PR_TRUE);

  // Set the scrolled frame's initial child lists
  scrolledFrame->SetInitialChildList(nsnull, childItems.childList);
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructFieldSetFrame(nsFrameConstructorState& aState,
                                              nsIContent*              aContent,
                                              nsIFrame*                aParentFrame,
                                              nsIAtom*                 aTag,
                                              nsStyleContext*          aStyleContext,
                                              nsIFrame*&               aNewFrame,
                                              nsFrameItems&            aFrameItems,
                                              const nsStyleDisplay*    aStyleDisplay,
                                              PRBool&                  aFrameHasBeenInitialized)
{
  nsIFrame* newFrame = NS_NewFieldSetFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!newFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Initialize it
  InitAndRestoreFrame(aState, aContent, 
                      aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                      nsnull, newFrame);

  // See if we need to create a view, e.g. the frame is absolutely
  // positioned
  nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

  // Resolve style and initialize the frame
  nsRefPtr<nsStyleContext> styleContext;
  styleContext = mPresShell->StyleSet()->ResolvePseudoStyleFor(aContent,
                                                               nsCSSAnonBoxes::fieldsetContent,
                                                               aStyleContext);
  
  nsIFrame* blockFrame = NS_NewBlockFrame(mPresShell, styleContext,
                                          NS_BLOCK_SPACE_MGR |
                                          NS_BLOCK_MARGIN_ROOT);
  InitAndRestoreFrame(aState, aContent, newFrame, nsnull, blockFrame);

  nsresult rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                                aParentFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }
  

  // The area frame is a float container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                              &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(blockFrame, floatSaveState,
                                  haveFirstLetterStyle,
                                  haveFirstLineStyle);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;

  if (aStyleDisplay->IsPositioned()) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    aState.PushAbsoluteContainingBlock(blockFrame, absoluteSaveState);
  }

  ProcessChildren(aState, aContent, blockFrame, PR_TRUE,
                  childItems, PR_TRUE);

  static NS_DEFINE_IID(kLegendFrameCID, NS_LEGEND_FRAME_CID);
  nsIFrame * child      = childItems.childList;
  nsIFrame * previous   = nsnull;
  nsIFrame* legendFrame = nsnull;
  while (nsnull != child) {
    nsresult result = child->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
    if (NS_SUCCEEDED(result) && legendFrame) {
      // We want the legend to be the first frame in the fieldset child list.
      // That way the EventStateManager will do the right thing when tabbing
      // from a selection point within the legend (bug 236071), which is
      // used for implementing legend access keys (bug 81481).
      // GetAdjustedParentFrame() below depends on this frame order.
      if (nsnull != previous) {
        previous->SetNextSibling(legendFrame->GetNextSibling());
      } else {
        childItems.childList = legendFrame->GetNextSibling();
      }
      legendFrame->SetNextSibling(blockFrame);
      legendFrame->SetParent(newFrame);
      break;
    }
    previous = child;
    child = child->GetNextSibling();
  }

  // Set the scrolled frame's initial child lists
  blockFrame->SetInitialChildList(nsnull, childItems.childList);

  // Set the scroll frame's initial child list
  newFrame->SetInitialChildList(nsnull, legendFrame ? legendFrame : blockFrame);

  // our new frame returned is the top frame which is the list frame. 
  aNewFrame = newFrame; 

  // yes we have already initialized our frame 
  aFrameHasBeenInitialized = PR_TRUE; 

  return NS_OK;
}

static nsIFrame*
FindAncestorWithGeneratedContentPseudo(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame->GetParent(); f; f = f->GetParent()) {
    NS_ASSERTION(f->IsGeneratedContentFrame(),
                 "should not have exited generated content");
    nsIAtom* pseudo = f->GetStyleContext()->GetPseudoType();
    if (pseudo == nsCSSPseudoElements::before ||
        pseudo == nsCSSPseudoElements::after)
      return f;
  }
  return nsnull;
}

nsresult
nsCSSFrameConstructor::ConstructTextFrame(nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems,
                                          PRBool                   aPseudoParent)
{
  // process pending pseudo frames. whitespace doesn't have an effect.
  if (!aPseudoParent && !aState.mPseudoFrames.IsEmpty() &&
      !TextIsOnlyWhitespace(aContent))
    ProcessPseudoFrames(aState, aFrameItems);

  nsIFrame* newFrame = nsnull;

#ifdef MOZ_SVG
  if (aParentFrame->IsFrameOfType(nsIFrame::eSVG)) {
    nsIFrame *ancestorFrame = SVG_GetFirstNonAAncestorFrame(aParentFrame);
    if (ancestorFrame) {
      nsISVGTextContentMetrics* metrics;
      CallQueryInterface(ancestorFrame, &metrics);
      if (!metrics) {
        return NS_OK;
      }
      newFrame = NS_NewSVGGlyphFrame(mPresShell, aContent,
                                     ancestorFrame, aStyleContext);
    }
  }
  else
#endif
  {
    newFrame = NS_NewTextFrame(mPresShell, aStyleContext);
  }

  if (NS_UNLIKELY(!newFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = InitAndRestoreFrame(aState, aContent, aParentFrame,
                                    nsnull, newFrame);

  if (NS_FAILED(rv)) {
    newFrame->Destroy();
    return rv;
  }

  // We never need to create a view for a text frame.

  if (newFrame->IsGeneratedContentFrame()) {
    nsAutoPtr<nsGenConInitializer> initializer;
    initializer =
      static_cast<nsGenConInitializer*>(
        aContent->UnsetProperty(nsGkAtoms::genConInitializerProperty));
    if (initializer) {
      if (initializer->mNode->InitTextFrame(initializer->mList,
              FindAncestorWithGeneratedContentPseudo(newFrame), newFrame)) {
        (this->*(initializer->mDirtyAll))();
      }
      initializer->mNode.forget();
    }
  }
  
  // Add the newly constructed frame to the flow
  aFrameItems.AddChild(newFrame);

  // Text frames don't go in the content->frame hash table, because
  // they're anonymous. This keeps the hash table smaller

  return rv;
}

nsresult
nsCSSFrameConstructor::ConstructHTMLFrame(nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsIFrame*                aParentFrame,
                                          nsIAtom*                 aTag,
                                          PRInt32                  aNameSpaceID,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems,
                                          PRBool                   aHasPseudoParent)
{
  // Ignore the tag if it's not HTML content and if it doesn't extend (via XBL)
  // a valid HTML namespace.  This check must match the one in
  // ShouldHaveFirstLineStyle.
  if (!aContent->IsNodeOfType(nsINode::eHTML) &&
      aNameSpaceID != kNameSpaceID_XHTML) {
    return NS_OK;
  }

  PRBool    frameHasBeenInitialized = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    addToHashTable = PR_TRUE;
  PRBool    isFloatContainer = PR_FALSE;
  PRBool    addedToFrameList = PR_FALSE;
  nsresult  rv = NS_OK;
  
  PRBool triedFrame = PR_FALSE;

  // See if the element is absolute or fixed positioned
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // Create a frame based on the tag
  if (nsGkAtoms::img == aTag || nsGkAtoms::mozgeneratedcontentimage == aTag) {
    // Make sure to keep IsSpecialContent in synch with this code
    rv = CreateHTMLImageFrame(aContent, aStyleContext, NS_NewImageFrame,
                              &newFrame);
    if (newFrame) {
      if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
        ProcessPseudoFrames(aState, aFrameItems); 
      }
    }
  }
  else if (nsGkAtoms::br == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewBRFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;

    // BR frames don't go in the content->frame hash table: typically
    // there are many BR content objects and this would increase the size
    // of the hash table, and it's doubtful we need the mapping anyway
    addToHashTable = PR_FALSE;
  }
  else if (nsGkAtoms::wbr == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewWBRFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
  else if (nsGkAtoms::input == aTag) {
    // Make sure to keep IsSpecialContent in synch with this code
    rv = CreateInputFrame(aState, aContent, aParentFrame,
                          aTag, aStyleContext, &newFrame,
                          display, frameHasBeenInitialized,
                          addedToFrameList, aFrameItems,
                          aHasPseudoParent);
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty() &&
        newFrame && !addedToFrameList) {
      // We'll still be adding this new frame, and it's a replaced
      // element, so process pseudo-frames now.
      ProcessPseudoFrames(aState, aFrameItems);       
    }
  }
  else if (nsGkAtoms::textarea == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewTextControlFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
  else if (nsGkAtoms::select == aTag) {
    if (!gUseXBLForms) {
      if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
        ProcessPseudoFrames(aState, aFrameItems); 
      }
      rv = ConstructSelectFrame(aState, aContent, aParentFrame,
                                aTag, aStyleContext, newFrame,
                                display, frameHasBeenInitialized,
                                aFrameItems);
      if (newFrame) {
        NS_ASSERTION(nsPlaceholderFrame::GetRealFrameFor(aFrameItems.lastChild) ==
                     newFrame,
                     "Frame didn't get added to aFrameItems?");
        addedToFrameList = PR_TRUE;
      }
    }
  }
  else if (nsGkAtoms::object == aTag ||
           nsGkAtoms::applet == aTag ||
           nsGkAtoms::embed == aTag) {
    // Make sure to keep IsSpecialContent in synch with this code
    if (!(aContent->IntrinsicState() &
          (NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED |
           NS_EVENT_STATE_SUPPRESSED))) {
      if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
        ProcessPseudoFrames(aState, aFrameItems); 
      }

      nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(aContent));
      NS_ASSERTION(objContent,
                   "applet, embed and object must implement nsIObjectLoadingContent!");
      if (!objContent) {
        // XBL might trigger this... 
        return NS_ERROR_UNEXPECTED;
      }

      PRUint32 type;
      objContent->GetDisplayedType(&type);
      if (type == nsIObjectLoadingContent::TYPE_LOADING) {
        // Ideally, this should show the standby attribute
        // XXX Should we return something that is replaced, or make
        // nsFrame replaced but not its subclasses?
        newFrame = NS_NewEmptyFrame(mPresShell, aStyleContext);
      }
      else if (type == nsIObjectLoadingContent::TYPE_PLUGIN)
        newFrame = NS_NewObjectFrame(mPresShell, aStyleContext);
      else if (type == nsIObjectLoadingContent::TYPE_IMAGE)
        newFrame = NS_NewImageFrame(mPresShell, aStyleContext);
      else if (type == nsIObjectLoadingContent::TYPE_DOCUMENT)
        newFrame = NS_NewSubDocumentFrame(mPresShell, aStyleContext);
#ifdef DEBUG
      else
        NS_ERROR("Shouldn't get here if we're not broken and not "
                 "suppressed and not blocked");
#endif

      triedFrame = PR_TRUE;
    }
  }
  else if (nsGkAtoms::fieldset == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    rv = ConstructFieldSetFrame(aState, aContent, aParentFrame,
                                aTag, aStyleContext, newFrame,
                                aFrameItems, display, frameHasBeenInitialized);
    NS_ASSERTION(nsPlaceholderFrame::GetRealFrameFor(aFrameItems.lastChild) ==
                 newFrame,
                 "Frame didn't get added to aFrameItems?");
    addedToFrameList = PR_TRUE;
  }
  else if (nsGkAtoms::legend == aTag) {
    NS_ASSERTION(!display->IsAbsolutelyPositioned() && !display->IsFloating(),
                 "Legends should not be positioned and should not float");
    
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewLegendFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;

    isFloatContainer = PR_TRUE;
  }
  else if (nsGkAtoms::frameset == aTag) {
    NS_ASSERTION(!display->IsAbsolutelyPositioned() && !display->IsFloating(),
                 "Framesets should not be positioned and should not float");
    
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
   
    newFrame = NS_NewHTMLFramesetFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
  else if (nsGkAtoms::iframe == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    
    newFrame = NS_NewSubDocumentFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;

    if (newFrame) {
      // the nsSubDocumentFrame needs to know about its content parent during ::Init.
      // there is no reasonable way to get the value there.
      // so we store it as a frame property.
      nsCOMPtr<nsIAtom> contentParentAtom = do_GetAtom("contentParent");
      aState.mPresContext->PropertyTable()->
        SetProperty(newFrame, contentParentAtom,
                    aParentFrame, nsnull, nsnull);
    }
  }
  else if (nsGkAtoms::spacer == aTag) {
    NS_ASSERTION(!display->IsAbsolutelyPositioned() && !display->IsFloating(),
                 "Spacers should not be positioned and should not float");
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewSpacerFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
  else if (nsGkAtoms::button == aTag) {
    rv = ConstructButtonFrame(aState, aContent, aParentFrame,
                              aTag, aStyleContext, &newFrame,
                              display, aFrameItems, aHasPseudoParent);
    // the html4 button needs to act just like a 
    // regular button except contain html content
    // so it must be replaced or html outside it will
    // draw into its borders. -EDV
    frameHasBeenInitialized = PR_TRUE;
    addedToFrameList = PR_TRUE;
    isFloatContainer = PR_TRUE;
  }
  else if (nsGkAtoms::isindex == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems);
    }
    newFrame = NS_NewIsIndexFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
  else if (nsGkAtoms::canvas == aTag) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewHTMLCanvasFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
#if defined(MOZ_MEDIA)
  else if (nsGkAtoms::video == aTag || nsGkAtoms::audio == aTag) {
    // We create video frames for audio elements so we can show controls.
    // Note that html.css specifies display:none for audio elements
    // without the "controls" attribute.
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    newFrame = NS_NewHTMLVideoFrame(mPresShell, aStyleContext);
    triedFrame = PR_TRUE;
  }
#endif
  if (NS_UNLIKELY(triedFrame && !newFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  else if (NS_FAILED(rv) || !newFrame) {
    return rv;
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list

  // Note: at this point we should construct kids for newFrame only if
  // it's not a leaf and hasn't been initialized yet.
  
  if (!frameHasBeenInitialized) {
    NS_ASSERTION(!addedToFrameList,
                 "Frames that were already added to the frame list should be "
                 "initialized by now!");
    nsIFrame* geometricParent = aState.GetGeometricParent(display,
                                                          aParentFrame);
     
    rv = InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, newFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv), "InitAndRestoreFrame failed");
    // See if we need to create a view, e.g. the frame is absolutely
    // positioned
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    addedToFrameList = PR_TRUE;
      
    // Process the child content if requested
    nsFrameItems childItems;
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floatSaveState;
    if (!newFrame->IsLeaf()) {
      if (display->IsPositioned()) {
        aState.PushAbsoluteContainingBlock(newFrame, absoluteSaveState);
      }
      if (isFloatContainer) {
        PRBool haveFirstLetterStyle, haveFirstLineStyle;
        ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                                    &haveFirstLetterStyle,
                                    &haveFirstLineStyle);
        aState.PushFloatContainingBlock(newFrame, floatSaveState,
                                        PR_FALSE, PR_FALSE);
      }

      // Process the child frames
      rv = ProcessChildren(aState, aContent, newFrame,
                           PR_TRUE, childItems, PR_FALSE);
    }

    // if there are any anonymous children create frames for them
    CreateAnonymousFrames(aTag, aState, aContent, newFrame,
                          PR_FALSE, childItems);

    // Set the frame's initial child list
    if (childItems.childList) {
      newFrame->SetInitialChildList(nsnull, childItems.childList);
    }
  }

  if (!addedToFrameList) {
    // Gotta do it here.  Note that things like absolutely positioned replaced
    // elements and the like will end up in this code.   So use the AddChild
    // on the state.
    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (addToHashTable) {
    // Add a mapping from content object to primary frame. Note that for
    // floated and positioned frames this is the out-of-flow frame and not
    // the placeholder frame
    aState.mFrameManager->SetPrimaryFrameFor(aContent, newFrame);
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsIAtom*                 aTag,
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
      aTag != nsGkAtoms::input &&
      aTag != nsGkAtoms::textarea &&
      aTag != nsGkAtoms::combobox &&
      aTag != nsGkAtoms::isindex &&
      aTag != nsGkAtoms::scrollbar
#ifdef MOZ_SVG
      && aTag != nsGkAtoms::use
#endif
#ifdef MOZ_MEDIA
      && aTag != nsGkAtoms::video
      && aTag != nsGkAtoms::audio
#endif
      )
    return NS_OK;

  return CreateAnonymousFrames(aState, aParent, mDocument, aNewFrame,
                               aAppendToExisting, aChildItems);
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsIDocument*             aDocument,
                                             nsIFrame*                aParentFrame,
                                             PRBool                   aAppendToExisting,
                                             nsFrameItems&            aChildItems)
{
  nsIAnonymousContentCreator* creator = nsnull;
  CallQueryInterface(aParentFrame, &creator);
  if (!creator)
    return NS_OK;

  nsresult rv;

  nsAutoTArray<nsIContent*, 4> newAnonymousItems;
  rv = creator->CreateAnonymousContent(newAnonymousItems);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 count = newAnonymousItems.Length();
  if (count == 0) {
    return NS_OK;
  }

  // save the incoming pseudo frame state, so that we don't end up
  // with those pseudoframes in aChildItems
  nsPseudoFrames priorPseudoFrames; 
  aState.mPseudoFrames.Reset(&priorPseudoFrames);

  for (PRUint32 i=0; i < count; i++) {
    // get our child's content and set its parent to our content
    nsIContent* content = newAnonymousItems[i];
    NS_ASSERTION(content, "null anonymous content?");

#ifdef MOZ_SVG
    // least-surprise CSS binding until we do the SVG specified
    // cascading rules for <svg:use> - bug 265894
    if (!aParent ||
        !aParent->NodeInfo()->Equals(nsGkAtoms::use, kNameSpaceID_SVG))
#endif
    {
      content->SetNativeAnonymous();
    }

    rv = content->BindToTree(aDocument, aParent, aParent, PR_TRUE);
    if (NS_FAILED(rv)) {
      content->UnbindFromTree();
      return rv;
    }

    nsIFrame* newFrame = creator->CreateFrameFor(content);
    if (newFrame) {
      aChildItems.AddChild(newFrame);
    }
    else {
      // create the frame and attach it to our frame
      ConstructFrame(aState, content, aParentFrame, aChildItems);
    }
  }

  creator->PostCreateFrames();

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aChildItems);
  }

  // restore the incoming pseudo frame state 
  aState.mPseudoFrames = priorPseudoFrames;

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
nsCSSFrameConstructor::ConstructXULFrame(nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         PRInt32                  aNameSpaceID,
                                         nsStyleContext*          aStyleContext,
                                         nsFrameItems&            aFrameItems,
                                         PRBool                   aXBLBaseTag,
                                         PRBool                   aHasPseudoParent,
                                         PRBool*                  aHaltProcessing)
{
  *aHaltProcessing = PR_FALSE;

  PRBool    primaryFrameSet = PR_FALSE;
  nsresult  rv = NS_OK;
  PRBool    isPopup = PR_FALSE;
  PRBool    frameHasBeenInitialized = PR_FALSE;

  // XXXbz somewhere here we should process pseudo frames if !aHasPseudoParent
  
  // this is the new frame that will be created
  nsIFrame* newFrame = nsnull;
  
  // this is the also the new frame that is created. But if a scroll frame is needed
  // the content will be mapped to the scrollframe and topFrame will point to it.
  // newFrame will still point to the child that we created like a "div" for example.
  nsIFrame* topFrame = nsnull;

  // Store aParentFrame away, since we plan to stomp on it later
  nsIFrame* origParentFrame = aParentFrame;

  NS_ASSERTION(aTag != nsnull, "null XUL tag");
  if (aTag == nsnull)
    return NS_OK;

  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  
  PRBool isXULNS = (aNameSpaceID == kNameSpaceID_XUL);
  PRBool isXULDisplay = IsXULDisplayType(display);

   // don't apply xul display types to tag based frames
  if (isXULDisplay && !isXULNS) {
    isXULDisplay = !IsSpecialContent(aContent, aTag, aNameSpaceID, aStyleContext);
  }

  PRBool triedFrame = PR_FALSE;

  if (isXULNS || isXULDisplay) {
    PRBool mayBeScrollable = PR_FALSE;

    if (isXULNS) {
      triedFrame = PR_TRUE;
    
      // First try creating a frame based on the tag
      // Make sure to keep IsSpecialContent in synch with this code
#ifdef MOZ_XUL
      // BUTTON CONSTRUCTION
      if (aTag == nsGkAtoms::button || aTag == nsGkAtoms::checkbox || aTag == nsGkAtoms::radio) {
        newFrame = NS_NewButtonBoxFrame(mPresShell, aStyleContext);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of BUTTON CONSTRUCTION logic
      // AUTOREPEATBUTTON CONSTRUCTION
      else if (aTag == nsGkAtoms::autorepeatbutton) {
        newFrame = NS_NewAutoRepeatBoxFrame(mPresShell, aStyleContext);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of AUTOREPEATBUTTON CONSTRUCTION logic

      // TITLEBAR CONSTRUCTION
      else if (aTag == nsGkAtoms::titlebar) {
        newFrame = NS_NewTitleBarFrame(mPresShell, aStyleContext);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of TITLEBAR CONSTRUCTION logic

      // RESIZER CONSTRUCTION
      else if (aTag == nsGkAtoms::resizer) {
        newFrame = NS_NewResizerFrame(mPresShell, aStyleContext);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of RESIZER CONSTRUCTION logic

      else if (aTag == nsGkAtoms::image) {
        newFrame = NS_NewImageBoxFrame(mPresShell, aStyleContext);
      }
      else if (aTag == nsGkAtoms::spring ||
               aTag == nsGkAtoms::spacer) {
        newFrame = NS_NewLeafBoxFrame(mPresShell, aStyleContext);
      }
       else if (aTag == nsGkAtoms::treechildren) {
        newFrame = NS_NewTreeBodyFrame(mPresShell, aStyleContext);
      }
      else if (aTag == nsGkAtoms::treecol) {
        newFrame = NS_NewTreeColFrame(mPresShell, aStyleContext);
      }
      // TEXT CONSTRUCTION
      else if (aTag == nsGkAtoms::text || aTag == nsGkAtoms::label ||
               aTag == nsGkAtoms::description) {
        if ((aTag == nsGkAtoms::label || aTag == nsGkAtoms::description) && 
            (! aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::value))) {
          // XXX we should probably be calling ConstructBlock here to handle
          // things like columns etc
          if (aTag == nsGkAtoms::label) {
            newFrame = NS_NewXULLabelFrame(mPresShell, aStyleContext,
                                           NS_BLOCK_SPACE_MGR |
                                           NS_BLOCK_MARGIN_ROOT);
          } else {
            newFrame = NS_NewBlockFrame(mPresShell, aStyleContext,
                                        NS_BLOCK_SPACE_MGR |
                                        NS_BLOCK_MARGIN_ROOT);
          }
        }
        else {
          newFrame = NS_NewTextBoxFrame(mPresShell, aStyleContext);
        }
      }
      // End of TEXT CONSTRUCTION logic

       // Menu Construction    
      else if (aTag == nsGkAtoms::menu ||
               aTag == nsGkAtoms::menuitem || 
               aTag == nsGkAtoms::menubutton) {
        // A derived class box frame
        // that has custom reflow to prevent menu children
        // from becoming part of the flow.
        newFrame = NS_NewMenuFrame(mPresShell, aStyleContext,
          (aTag != nsGkAtoms::menuitem));
      }
      else if (aTag == nsGkAtoms::menubar) {
  #ifdef XP_MACOSX
        // On Mac OS X, we use the system menubar for any root chrome shell
        // XUL menubars.
        PRBool isRootChromeShell = PR_FALSE;
        nsCOMPtr<nsISupports> container = aState.mPresContext->GetContainer();
        if (container) {
          nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
          if (treeItem) {
            PRInt32 type;
            treeItem->GetItemType(&type);
            if (nsIDocShellTreeItem::typeChrome == type) {
              nsCOMPtr<nsIDocShellTreeItem> parent;
              treeItem->GetParent(getter_AddRefs(parent));
              isRootChromeShell = !parent;
            }
          }
        }

        if (isRootChromeShell) {
          *aHaltProcessing = PR_TRUE;
          return NS_OK;
        }
  #endif

        newFrame = NS_NewMenuBarFrame(mPresShell, aStyleContext);
      }
      else if (aTag == nsGkAtoms::popupgroup &&
               aContent->IsRootOfNativeAnonymousSubtree()) {
        // This frame contains child popups
        newFrame = NS_NewPopupSetFrame(mPresShell, aStyleContext);
      }
      else if (aTag == nsGkAtoms::iframe || aTag == nsGkAtoms::editor ||
               aTag == nsGkAtoms::browser) {
        newFrame = NS_NewSubDocumentFrame(mPresShell, aStyleContext);
      }
      // PROGRESS METER CONSTRUCTION
      else if (aTag == nsGkAtoms::progressmeter) {
        newFrame = NS_NewProgressMeterFrame(mPresShell, aStyleContext);
      }
      // End of PROGRESS METER CONSTRUCTION logic
      else
#endif
      // SLIDER CONSTRUCTION
      if (aTag == nsGkAtoms::slider) {
        newFrame = NS_NewSliderFrame(mPresShell, aStyleContext);
      }
      // End of SLIDER CONSTRUCTION logic

      // SCROLLBAR CONSTRUCTION
      else if (aTag == nsGkAtoms::scrollbar) {
        newFrame = NS_NewScrollbarFrame(mPresShell, aStyleContext);
      }
      // End of SCROLLBAR CONSTRUCTION logic

      // SCROLLBUTTON CONSTRUCTION
      else if (aTag == nsGkAtoms::scrollbarbutton) {
        newFrame = NS_NewScrollbarButtonFrame(mPresShell, aStyleContext);
      }
      // End of SCROLLBUTTON CONSTRUCTION logic

#ifdef MOZ_XUL
      // SPLITTER CONSTRUCTION
      else if (aTag == nsGkAtoms::splitter) {
        newFrame = NS_NewSplitterFrame(mPresShell, aStyleContext);
      }
      // End of SPLITTER CONSTRUCTION logic

      else {
        triedFrame = PR_FALSE;
      }
#endif
    }

    // Display types for XUL start here
    // Make sure this is kept in sync with nsCSSProps::kDisplayKTable
    // First is BOX
    if (!newFrame && isXULDisplay) {
      triedFrame = PR_TRUE;
  
      if (display->mDisplay == NS_STYLE_DISPLAY_INLINE_BOX ||
               display->mDisplay == NS_STYLE_DISPLAY_BOX) {
        newFrame = NS_NewBoxFrame(mPresShell, aStyleContext, PR_FALSE, nsnull);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } // End of BOX CONSTRUCTION logic
#ifdef MOZ_XUL
      // ------- Begin Grid ---------
      else if (display->mDisplay == NS_STYLE_DISPLAY_INLINE_GRID ||
               display->mDisplay == NS_STYLE_DISPLAY_GRID) {
        nsCOMPtr<nsIBoxLayout> layout;
        NS_NewGridLayout2(mPresShell, getter_AddRefs(layout));
        newFrame = NS_NewBoxFrame(mPresShell, aStyleContext, PR_FALSE, layout);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } //------- End Grid ------

      // ------- Begin Rows/Columns ---------
      else if (display->mDisplay == NS_STYLE_DISPLAY_GRID_GROUP) {
        nsCOMPtr<nsIBoxLayout> layout;
      
        if (aTag == nsGkAtoms::listboxbody) {
          NS_NewListBoxLayout(mPresShell, layout);
          newFrame = NS_NewListBoxBodyFrame(mPresShell, aStyleContext, PR_FALSE, layout);
        }
        else
        {
          NS_NewGridRowGroupLayout(mPresShell, getter_AddRefs(layout));
          newFrame = NS_NewGridRowGroupFrame(mPresShell, aStyleContext, PR_FALSE, layout);
        }

        // Boxes can scroll.
        if (display->IsScrollableOverflow()) {
          // set the top to be the newly created scrollframe
          BuildScrollFrame(aState, aContent, aStyleContext, newFrame,
                           aParentFrame, nsnull, topFrame, aStyleContext);

          // we have a scrollframe so the parent becomes the scroll frame.
          aParentFrame = newFrame->GetParent();

          primaryFrameSet = PR_TRUE;

          frameHasBeenInitialized = PR_TRUE;
        }
      } //------- End Grid ------

      // ------- Begin Row/Column ---------
      else if (display->mDisplay == NS_STYLE_DISPLAY_GRID_LINE) {
        nsCOMPtr<nsIBoxLayout> layout;


        NS_NewGridRowLeafLayout(mPresShell, getter_AddRefs(layout));

        if (aTag == nsGkAtoms::listitem)
          newFrame = NS_NewListItemFrame(mPresShell, aStyleContext, PR_FALSE, layout);
        else
          newFrame = NS_NewGridRowLeafFrame(mPresShell, aStyleContext, PR_FALSE, layout);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } //------- End Grid ------
      // End of STACK CONSTRUCTION logic
       // DECK CONSTRUCTION
      else if (display->mDisplay == NS_STYLE_DISPLAY_DECK) {
        newFrame = NS_NewDeckFrame(mPresShell, aStyleContext);
      }
      // End of DECK CONSTRUCTION logic
      else if (display->mDisplay == NS_STYLE_DISPLAY_GROUPBOX) {
        newFrame = NS_NewGroupBoxFrame(mPresShell, aStyleContext);

        // Boxes can scroll.
        mayBeScrollable = PR_TRUE;
      } 
      // STACK CONSTRUCTION
      else if (display->mDisplay == NS_STYLE_DISPLAY_STACK ||
               display->mDisplay == NS_STYLE_DISPLAY_INLINE_STACK) {
        newFrame = NS_NewStackFrame(mPresShell, aStyleContext);

        mayBeScrollable = PR_TRUE;
      }
      else if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
        // If a popup is inside a menu, then the menu understands the complex
        // rules/behavior governing the cascade of multiple menu popups and can handle
        // having the real popup frame placed under it as a child.  
        // If, however, the parent is *not* a menu frame, then we need to create
        // a placeholder frame for the popup, and then we add the popup frame to the
        // root popup set (that manages all such "detached" popups).
        if (aParentFrame->GetType() != nsGkAtoms::menuFrame) {
          if (!aState.mPopupItems.containingBlock) {
            // Just don't create a frame for this popup; we can't do
            // anything with it, since there is no root popup set.
            *aHaltProcessing = PR_TRUE;
            NS_ASSERTION(!aState.mRootBox, "Popup containing block is missing");
            return NS_OK;
          }

#ifdef NS_DEBUG
          NS_ASSERTION(aState.mPopupItems.containingBlock->GetType() ==
                       nsGkAtoms::popupSetFrame,
                       "Popup containing block isn't a nsIPopupSetFrame");
#endif
          isPopup = PR_TRUE;
        }

        // This is its own frame that derives from box.
        newFrame = NS_NewMenuPopupFrame(mPresShell, aStyleContext);

        if (aTag == nsGkAtoms::tooltip) {
          if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::_default,
                                    nsGkAtoms::_true, eIgnoreCase)) {
            // Tell the root box about the tooltip.
            if (aState.mRootBox)
              aState.mRootBox->SetDefaultTooltip(aContent);
          }
        }
      }
      
      else {
        triedFrame = PR_FALSE;
      }
#endif // MOZ_XUL
    }

    if (mayBeScrollable && display->IsScrollableOverflow()) {
      // set the top to be the newly created scrollframe
      BuildScrollFrame(aState, aContent, aStyleContext, newFrame,
                       aParentFrame, nsnull, topFrame, aStyleContext);

      // we have a scrollframe so the parent becomes the scroll frame.
      // XXXldb Do we really want to do this?  The one case where it
      // matters when |frameHasBeenInitialized| is true is one where
      // I think we'd be better off the other way around.
      aParentFrame = newFrame->GetParent();
      primaryFrameSet = PR_TRUE;
      frameHasBeenInitialized = PR_TRUE;
    }
  }
  
  if (NS_UNLIKELY(triedFrame && !newFrame))
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (NS_SUCCEEDED(rv) && newFrame != nsnull) {

    // if no top frame was created then the top is the new frame
    if (topFrame == nsnull)
        topFrame = newFrame;

    // xul does not support absolute positioning
    nsIFrame* geometricParent;
#ifdef MOZ_XUL
    if (isPopup) {
      NS_ASSERTION(aState.mPopupItems.containingBlock, "How did we get here?");
      geometricParent = aState.mPopupItems.containingBlock;
    }
    else
#endif
    {
      geometricParent = aParentFrame;
    }
    
    /*
      nsIFrame* geometricParent = aState.GetGeometricParent(display, aParentFrame);
    */
    // if the new frame was already initialized to initialize it again.
    if (!frameHasBeenInitialized) {

      rv = InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, newFrame);

      if (NS_FAILED(rv)) {
        newFrame->Destroy();
        return rv;
      }
      
      /*
      // if our parent is a block frame then do things the way html likes it
      // if not then we are in a box so do what boxes like. On example is boxes
      // do not support the absolute positioning of their children. While html blocks
      // that's why we call different things here.
      nsIAtom* frameType = geometricParent->GetType();
      if ((frameType == nsGkAtoms::blockFrame) ||
          (frameType == nsGkAtoms::XULLabelFrame)) {
      */
        // See if we need to create a view, e.g. the frame is absolutely positioned
        nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

      /*
      } else {
          // we are in a box so do the box thing.
        nsBoxFrame::CreateViewForFrame(aState.mPresContext, newFrame,
                                                 aStyleContext, PR_FALSE);
      }
      */
      
    }

    // Add the new frame to our list of frame items.  Note that we
    // don't support floating or positioning of XUL frames.
    rv = aState.AddChild(topFrame, aFrameItems, aContent, aStyleContext,
                         origParentFrame, PR_FALSE, PR_FALSE, isPopup);
    if (NS_FAILED(rv)) {
      return rv;
    }

#ifdef MOZ_XUL
    if (aTag == nsGkAtoms::popupgroup &&
        aContent->IsRootOfNativeAnonymousSubtree()) {
      nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
      if (rootBox) {
        NS_ASSERTION(rootBox->GetPopupSetFrame() == newFrame,
                     "Unexpected PopupSetFrame");
        aState.mPopupItems.containingBlock = rootBox->GetPopupSetFrame();
      }      
    }
#endif

    // If the new frame isn't a float containing block, then push a null
    // float containing block to disable floats. This is needed to disable
    // floats within XUL frames.
    nsFrameConstructorSaveState floatSaveState;
    PRBool isFloatContainingBlock =
      newFrame->GetContentInsertionFrame()->IsFloatContainingBlock();
    aState.PushFloatContainingBlock(isFloatContainingBlock ? newFrame : nsnull,
                                    floatSaveState, PR_FALSE, PR_FALSE);

    // Process the child content if requested
    nsFrameItems childItems;
    if (!newFrame->IsLeaf()) {
      // XXXbz don't we need calls to ShouldBuildChildFrames
      // elsewhere too?  Why only for XUL?
      if (mDocument->BindingManager()->ShouldBuildChildFrames(aContent)) {
        rv = ProcessChildren(aState, aContent, newFrame, PR_FALSE,
                             childItems, PR_FALSE);
      }
    }
      
    // XXX These should go after the wrapper!
    CreateAnonymousFrames(aTag, aState, aContent, newFrame, PR_FALSE,
                          childItems);

    // Set the frame's initial child list
    newFrame->SetInitialChildList(nsnull, childItems.childList);
  }

#ifdef MOZ_XUL
  // register tooltip support if needed
  if (aTag == nsGkAtoms::treechildren || // trees always need titletips
      aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext) ||
      aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltip))
  {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
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

nsresult
nsCSSFrameConstructor::AddLazyChildren(nsIContent* aContent,
                                       nsLazyFrameConstructionCallback* aCallback,
                                       void* aArg, PRBool aIsSynch)
{
  nsCOMPtr<nsIRunnable> event =
    new LazyGenerateChildrenEvent(aContent, mPresShell, aCallback, aArg);
  return aIsSynch ? event->Run() :
                    NS_DispatchToCurrentThread(event);
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::BeginBuildingScrollFrame(nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsStyleContext*          aContentStyle,
                                                nsIFrame*                aParentFrame,
                                                nsIFrame*                aContentParentFrame,
                                                nsIAtom*                 aScrolledPseudo,
                                                PRBool                   aIsRoot,
                                                nsIFrame*&               aNewFrame)
{
  nsIFrame* parentFrame = nsnull;
  nsIFrame* gfxScrollFrame = aNewFrame;

  nsFrameItems anonymousItems;

  nsRefPtr<nsStyleContext> contentStyle = aContentStyle;

  if (!gfxScrollFrame) {
    // Build a XULScrollFrame when the child is a box, otherwise an
    // HTMLScrollFrame
    if (IsXULDisplayType(aContentStyle->GetStyleDisplay())) {
      gfxScrollFrame = NS_NewXULScrollFrame(mPresShell, contentStyle, aIsRoot);
    } else {
      gfxScrollFrame = NS_NewHTMLScrollFrame(mPresShell, contentStyle, aIsRoot);
    }

    InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, gfxScrollFrame);

    // Create a view
    nsHTMLContainerFrame::CreateViewForFrame(gfxScrollFrame,
                                             aContentParentFrame, PR_FALSE);
  }

  // if there are any anonymous children for the scroll frame, create
  // frames for them.
  CreateAnonymousFrames(aState, aContent, mDocument, gfxScrollFrame,
                        PR_FALSE, anonymousItems);

  parentFrame = gfxScrollFrame;
  aNewFrame = gfxScrollFrame;

  // we used the style that was passed in. So resolve another one.
  nsStyleSet *styleSet = mPresShell->StyleSet();
  nsStyleContext* aScrolledChildStyle = styleSet->ResolvePseudoStyleFor(aContent,
                                                                        aScrolledPseudo,
                                                                        contentStyle).get();

  if (gfxScrollFrame) {
     gfxScrollFrame->SetInitialChildList(nsnull, anonymousItems.childList);
  }

  return aScrolledChildStyle;
}

void
nsCSSFrameConstructor::FinishBuildingScrollFrame(nsIFrame* aScrollFrame,
                                                 nsIFrame* aScrolledFrame)
{
  aScrollFrame->AppendFrames(nsnull, aScrolledFrame);

  // force the scrolled frame to have a view. The view will be parented to
  // the correct anonymous inner view because the scrollframes override
  // nsIFrame::GetParentViewForChildFrame.
  nsHTMLContainerFrame::CreateViewForFrame(aScrolledFrame, nsnull, PR_TRUE);
  nsIView* view = aScrolledFrame->GetView();
  if (!view)
    return;
}


/**
 * Called to wrap a gfx scrollframe around a frame. The hierarchy will look like this
 *
 * ------- for gfx scrollbars ------
 *
 *
 *            ScrollFrame
 *                 ^
 *                 |
 *               Frame (scrolled frame you passed in)
 *
 *
 *-----------------------------------
 * LEGEND:
 * 
 * ScrollFrame: This is a frame that manages gfx cross platform frame based scrollbars.
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
nsCSSFrameConstructor::BuildScrollFrame(nsFrameConstructorState& aState,
                                        nsIContent*              aContent,
                                        nsStyleContext*          aContentStyle,
                                        nsIFrame*                aScrolledFrame,
                                        nsIFrame*                aParentFrame,
                                        nsIFrame*                aContentParentFrame,
                                        nsIFrame*&               aNewFrame, 
                                        nsStyleContext*&         aScrolledContentStyle)
{
    nsRefPtr<nsStyleContext> scrolledContentStyle =
      BeginBuildingScrollFrame(aState, aContent, aContentStyle, aParentFrame,
                               aContentParentFrame, nsCSSAnonBoxes::scrolledContent,
                               PR_FALSE, aNewFrame);
    
    aScrolledFrame->SetStyleContextWithoutNotification(scrolledContentStyle);
    InitAndRestoreFrame(aState, aContent, aNewFrame, nsnull, aScrolledFrame);

    FinishBuildingScrollFrame(aNewFrame, aScrolledFrame);

    aScrolledContentStyle = scrolledContentStyle;

    // now set the primary frame to the ScrollFrame
    aState.mFrameManager->SetPrimaryFrameFor( aContent, aNewFrame );
    return NS_OK;

}

nsresult
nsCSSFrameConstructor::ConstructFrameByDisplayType(nsFrameConstructorState& aState,
                                                   const nsStyleDisplay*    aDisplay,
                                                   nsIContent*              aContent,
                                                   PRInt32                  aNameSpaceID,
                                                   nsIAtom*                 aTag,
                                                   nsIFrame*                aParentFrame,
                                                   nsStyleContext*          aStyleContext,
                                                   nsFrameItems&            aFrameItems,
                                                   PRBool                   aHasPseudoParent)
{
  PRBool    primaryFrameSet = PR_FALSE;
  nsIFrame* newFrame = nsnull;  // the frame we construct
  PRBool    addToHashTable = PR_TRUE;
  PRBool    addedToFrameList = PR_FALSE;
  nsresult  rv = NS_OK;

  // The style system ensures that floated and positioned frames are
  // block-level.
  NS_ASSERTION(!(aDisplay->IsFloating() ||
                 aDisplay->IsAbsolutelyPositioned()) ||
               aDisplay->IsBlockOutside(),
               "Style system did not apply CSS2.1 section 9.7 fixups");

  // If this is "body", try propagating its scroll style to the viewport
  // Note that we need to do this even if the body is NOT scrollable;
  // it might have dynamically changed from scrollable to not scrollable,
  // and that might need to be propagated.
  PRBool propagatedScrollToViewport = PR_FALSE;
  if (aContent->NodeInfo()->Equals(nsGkAtoms::body) &&
      aContent->IsNodeOfType(nsINode::eHTML)) {
    propagatedScrollToViewport =
      PropagateScrollToViewport() == aContent;
  }

  // If the frame is a block-level frame and is scrollable, then wrap it
  // in a scroll frame.
  // XXX Ignore tables for the time being
  if (aDisplay->IsBlockInside() &&
      aDisplay->IsScrollableOverflow() &&
      !propagatedScrollToViewport) {

    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }

    nsRefPtr<nsStyleContext> scrolledContentStyle
      = BeginBuildingScrollFrame(aState, aContent, aStyleContext,
                                 aState.GetGeometricParent(aDisplay, aParentFrame),
                                 aParentFrame,
                                 nsCSSAnonBoxes::scrolledContent,
                                 PR_FALSE, newFrame);
    
    // Initialize it
    // pass a temporary stylecontext, the correct one will be set later
    nsIFrame* scrolledFrame =
        NS_NewBlockFrame(mPresShell, aStyleContext,
                         NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);

    nsFrameItems blockItem;
    rv = ConstructBlock(aState,
                        scrolledContentStyle->GetStyleDisplay(), aContent,
                        newFrame, newFrame, scrolledContentStyle,
                        &scrolledFrame, blockItem, aDisplay->IsPositioned());
    NS_ASSERTION(blockItem.childList == scrolledFrame,
                 "Scrollframe's frameItems should be exactly the scrolled frame");
    FinishBuildingScrollFrame(newFrame, scrolledFrame);

    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }

    addedToFrameList = PR_TRUE;
  }
  // See if the frame is absolute or fixed positioned
  else if (aDisplay->IsAbsolutelyPositioned() &&
           (NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay ||
            NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {

    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }

    // Create a frame to wrap up the absolute positioned item
    // pass a temporary stylecontext, the correct one will be set later
    newFrame = NS_NewAbsoluteItemWrapperFrame(mPresShell, aStyleContext);

    rv = ConstructBlock(aState, aDisplay, aContent,
                        aState.GetGeometricParent(aDisplay, aParentFrame), aParentFrame,
                        aStyleContext, &newFrame, aFrameItems, PR_TRUE);
    if (NS_FAILED(rv)) {
      return rv;
    }

    addedToFrameList = PR_TRUE;
  }
  // See if the frame is floated and it's a block frame
  else if (aDisplay->IsFloating() &&
           (NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay ||
            NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    // Create an area frame
    // pass a temporary stylecontext, the correct one will be set later
    newFrame = NS_NewFloatingItemWrapperFrame(mPresShell, aStyleContext);

    rv = ConstructBlock(aState, aDisplay, aContent, 
                        aState.GetGeometricParent(aDisplay, aParentFrame),
                        aParentFrame, aStyleContext, &newFrame, aFrameItems,
                        aDisplay->mPosition == NS_STYLE_POSITION_RELATIVE ||
                        aDisplay->HasTransform());
    if (NS_FAILED(rv)) {
      return rv;
    }

    addedToFrameList = PR_TRUE;
  }
  // See if it's relatively positioned or transformed
  else if ((NS_STYLE_POSITION_RELATIVE == aDisplay->mPosition ||
            aDisplay->HasTransform()) &&
           (aDisplay->IsBlockInside() ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay))) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    // Is it block-level or inline-level?
    if (aDisplay->IsBlockInside()) {
      // Create a wrapper frame. Only need space manager if it's inline-block
      PRUint32 flags = (aDisplay->mDisplay == NS_STYLE_DISPLAY_INLINE_BLOCK ?
                        NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT : 0);
      newFrame = NS_NewRelativeItemWrapperFrame(mPresShell, aStyleContext, 
                                                flags);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      ConstructBlock(aState, aDisplay, aContent,
                     aParentFrame, nsnull, aStyleContext, &newFrame,
                     aFrameItems, PR_TRUE);
      addedToFrameList = PR_TRUE;
    } else {
      // Create a positioned inline frame
      newFrame = NS_NewPositionedInlineFrame(mPresShell, aStyleContext);
      // Note that we want to insert the inline after processing kids, since
      // processing of kids may split the inline.
      ConstructInline(aState, aDisplay, aContent,
                      aParentFrame, aStyleContext, PR_TRUE, newFrame);
    }
  }
  // See if it's a block frame of some sort
  else if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_RUN_IN == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_COMPACT == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay->mDisplay)) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    PRUint32 flags = 0;
    if (NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay->mDisplay) {
      flags = NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT;
    }
    // Create the block frame
    newFrame = NS_NewBlockFrame(mPresShell, aStyleContext, flags);
    if (newFrame) { // That worked so construct the block and its children
      // XXXbz should we be passing in a non-null aContentParentFrame?
      rv = ConstructBlock(aState, aDisplay, aContent,
                          aParentFrame, nsnull, aStyleContext, &newFrame,
                          aFrameItems, PR_FALSE);
      addedToFrameList = PR_TRUE;
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  // See if it's an inline frame of some sort
  else if ((NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
           (NS_STYLE_DISPLAY_MARKER == aDisplay->mDisplay)) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    // Create the inline frame
    newFrame = NS_NewInlineFrame(mPresShell, aStyleContext);
    if (newFrame) { // That worked so construct the inline and its children
      // Note that we want to insert the inline after processing kids, since
      // processing of kids may split the inline.
      rv = ConstructInline(aState, aDisplay, aContent,
                           aParentFrame, aStyleContext, PR_FALSE, newFrame);
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
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
    case NS_STYLE_DISPLAY_INLINE_TABLE:
    {
      nsIFrame* innerTable;
      rv = ConstructTableFrame(aState, aContent, 
                               aParentFrame, aStyleContext,
                               aNameSpaceID, PR_FALSE, aFrameItems, newFrame,
                               innerTable);
      addedToFrameList = PR_TRUE;
      // Note: table construction function takes care of initializing
      // the frame, processing children, and setting the initial child
      // list
      break;
    }
  
    // the next 5 cases are only relevant if the parent is not a table, ConstructTableFrame handles children
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
    {
      // aParentFrame may be an inner table frame rather than an outer frame 
      // In this case we need to get the outer frame.
      nsIFrame* parentFrame = AdjustCaptionParentFrame(aParentFrame);
      rv = ConstructTableCaptionFrame(aState, aContent, parentFrame,
                                      aStyleContext, aNameSpaceID, aFrameItems,
                                      newFrame, aHasPseudoParent);
      if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
    }

    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      rv = ConstructTableRowGroupFrame(aState, aContent, aParentFrame,
                                       aStyleContext, aNameSpaceID, PR_FALSE,
                                       aFrameItems, newFrame,
                                       aHasPseudoParent);
      if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;

    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      rv = ConstructTableColGroupFrame(aState, aContent, aParentFrame,
                                       aStyleContext, aNameSpaceID,
                                       PR_FALSE, aFrameItems, newFrame,
                                       aHasPseudoParent);
      if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
   
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      rv = ConstructTableColFrame(aState, aContent, aParentFrame,
                                  aStyleContext, aNameSpaceID, PR_FALSE,
                                  aFrameItems, newFrame, aHasPseudoParent);
      if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_ROW:
      rv = ConstructTableRowFrame(aState, aContent, aParentFrame,
                                  aStyleContext, aNameSpaceID, PR_FALSE,
                                  aFrameItems, newFrame, aHasPseudoParent);
      if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
        aFrameItems.AddChild(newFrame);
      }
      return rv;
  
    case NS_STYLE_DISPLAY_TABLE_CELL:
      {
        nsIFrame* innerTable;
        rv = ConstructTableCellFrame(aState, aContent, aParentFrame,
                                     aStyleContext, aNameSpaceID,
                                     PR_FALSE, aFrameItems, newFrame,
                                     innerTable, aHasPseudoParent);
        if (NS_SUCCEEDED(rv) && !aHasPseudoParent) {
          aFrameItems.AddChild(newFrame);
        }
        return rv;
      }
  
    default:
      NS_NOTREACHED("How did we get here?");
      break;
    }
  }

  if (!addedToFrameList) {
    // Gotta do it here
    NS_ASSERTION(!aDisplay->IsAbsolutelyPositioned() &&
                 !aDisplay->IsFloating(),
                 "Things that could be out-of-flow need to handle adding "
                 "to the frame list themselves");
    
    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Cases where AddChild() can fail must handle it themselves");
  }

  if (newFrame && addToHashTable) {
    // Add a mapping from content object to primary frame. Note that for
    // floated and positioned frames this is the out-of-flow frame and not
    // the placeholder frame
    if (!primaryFrameSet) {
      aState.mFrameManager->SetPrimaryFrameFor(aContent, newFrame);
    }
  }

  return rv;
}

nsresult 
nsCSSFrameConstructor::InitAndRestoreFrame(const nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsIFrame*                aParentFrame,
                                           nsIFrame*                aPrevInFlow,
                                           nsIFrame*                aNewFrame,
                                           PRBool                   aAllowCounters)
{
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");
  
  nsresult rv = NS_OK;
  
  NS_ASSERTION(aNewFrame, "Null frame cannot be initialized");
  if (!aNewFrame)
    return NS_ERROR_NULL_POINTER;

  // Initialize the frame
  rv = aNewFrame->Init(aContent, aParentFrame, aPrevInFlow);
  aNewFrame->AddStateBits(aState.mAdditionalStateBits);

  if (aState.mFrameState && aState.mFrameManager) {
    // Restore frame state for just the newly created frame.
    aState.mFrameManager->RestoreFrameStateFor(aNewFrame, aState.mFrameState);
  }

  if (aAllowCounters && !aPrevInFlow &&
      mCounterManager.AddCounterResetsAndIncrements(aNewFrame)) {
    CountersDirty();
  }

  return rv;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(nsIFrame*         aParentFrame,
                                           nsIContent*       aContent)
{
  nsStyleContext* parentStyleContext = nsnull;
  if (aContent->GetParent()) {
    aParentFrame = nsFrame::CorrectStyleParentFrame(aParentFrame, nsnull);
  
    if (aParentFrame) {
      // Resolve the style context based on the content object and the parent
      // style context
      parentStyleContext = aParentFrame->GetStyleContext();
    } else {
      // Perhaps aParentFrame is a canvasFrame and we're replicating
      // fixed-pos frames.
      // XXX should we create a way to tell ConstructFrame which style
      // context to use, and pass it the style context for the
      // previous page's fixed-pos frame?
    }
  } else {
    // This has got to be a call from ConstructDocElementTableFrame.
    // Not sure how best to assert that here.
  }

  nsStyleSet *styleSet = mPresShell->StyleSet();

  if (aContent->IsNodeOfType(nsINode::eELEMENT)) {
    return styleSet->ResolveStyleFor(aContent, parentStyleContext);
  } else {

    NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
                 "shouldn't waste time creating style contexts for "
                 "comments and processing instructions");

    return styleSet->ResolveStyleForNonElement(parentStyleContext);
  }
}

// MathML Mod - RBS
#ifdef MOZ_MATHML
nsresult
nsCSSFrameConstructor::FlushAccumulatedBlock(nsFrameConstructorState& aState,
                                             nsIContent* aContent,
                                             nsIFrame* aParentFrame,
                                             nsFrameItems* aBlockItems,
                                             nsFrameItems* aNewItems)
{
  if (!aBlockItems->childList) {
    // Nothing to do
    return NS_OK;
  }

  nsStyleContext* parentContext =
    nsFrame::CorrectStyleParentFrame(aParentFrame,
                                     nsCSSAnonBoxes::mozMathMLAnonymousBlock)->GetStyleContext(); 
  nsStyleSet *styleSet = mPresShell->StyleSet();
  nsRefPtr<nsStyleContext> blockContext;
  blockContext = styleSet->ResolvePseudoStyleFor(aContent,
                                                 nsCSSAnonBoxes::mozMathMLAnonymousBlock,
                                                 parentContext);

  // then, create a block frame that will wrap the child frames. Make it a
  // MathML frame so that Get(Absolute/Float)ContainingBlockFor know that this
  // is not a suitable block.
  nsIFrame* blockFrame = NS_NewMathMLmathBlockFrame(mPresShell, blockContext,
                          NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);
  if (NS_UNLIKELY(!blockFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, blockFrame);
  for (nsIFrame* f = aBlockItems->childList; f; f = f->GetNextSibling()) {
    ReparentFrame(aState.mFrameManager, blockFrame, f);
  }
  // abs-pos and floats are disabled in MathML children so we don't have to
  // worry about messing up those.
  blockFrame->SetInitialChildList(nsnull, aBlockItems->childList);
  *aBlockItems = nsFrameItems();
  aNewItems->AddChild(blockFrame);
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ConstructMathMLFrame(nsFrameConstructorState& aState,
                                            nsIContent*              aContent,
                                            nsIFrame*                aParentFrame,
                                            nsIAtom*                 aTag,
                                            PRInt32                  aNameSpaceID,
                                            nsStyleContext*          aStyleContext,
                                            nsFrameItems&            aFrameItems,
                                            PRBool                   aHasPseudoParent)
{
  // Make sure that we remain confined in the MathML world
  if (aNameSpaceID != kNameSpaceID_MathML) 
    return NS_OK;

  nsresult  rv = NS_OK;

  NS_ASSERTION(aTag != nsnull, "null MathML tag");
  if (aTag == nsnull)
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;

  // Make sure to keep IsSpecialContent in synch with this code
  const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();

  // Leverage IsSpecialContent to check if one of the |if aTag| below will
  // surely match (knowing that aNameSpaceID == kNameSpaceID_MathML here)
  if (IsSpecialContent(aContent, aTag, aNameSpaceID, aStyleContext)) {
    // process pending pseudo frames
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
  }

  if (aTag == nsGkAtoms::mi_ ||
      aTag == nsGkAtoms::mn_ ||
      aTag == nsGkAtoms::ms_ ||
      aTag == nsGkAtoms::mtext_)
    newFrame = NS_NewMathMLTokenFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mo_)
    newFrame = NS_NewMathMLmoFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mfrac_)
    newFrame = NS_NewMathMLmfracFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::msup_)
    newFrame = NS_NewMathMLmsupFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::msub_)
    newFrame = NS_NewMathMLmsubFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::msubsup_)
    newFrame = NS_NewMathMLmsubsupFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::munder_)
    newFrame = NS_NewMathMLmunderFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mover_)
    newFrame = NS_NewMathMLmoverFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::munderover_)
    newFrame = NS_NewMathMLmunderoverFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mphantom_)
    newFrame = NS_NewMathMLmphantomFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mpadded_)
    newFrame = NS_NewMathMLmpaddedFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mspace_ ||
           aTag == nsGkAtoms::none    ||
           aTag == nsGkAtoms::mprescripts_)
    newFrame = NS_NewMathMLmspaceFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mfenced_)
    newFrame = NS_NewMathMLmfencedFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mmultiscripts_)
    newFrame = NS_NewMathMLmmultiscriptsFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mstyle_)
    newFrame = NS_NewMathMLmstyleFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::msqrt_)
    newFrame = NS_NewMathMLmsqrtFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mroot_)
    newFrame = NS_NewMathMLmrootFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::maction_)
    newFrame = NS_NewMathMLmactionFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::mrow_ ||
           aTag == nsGkAtoms::merror_)
    newFrame = NS_NewMathMLmrowFrame(mPresShell, aStyleContext);
  else if (aTag == nsGkAtoms::math) { 
    // root <math> element
    const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
    PRBool isBlock = (NS_STYLE_DISPLAY_BLOCK == display->mDisplay);
    newFrame = NS_NewMathMLmathFrame(mPresShell, isBlock, aStyleContext);
  }
  else {
    return NS_OK;
  }

  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (newFrame) {
    NS_ASSERTION(newFrame->IsFrameOfType(nsIFrame::eExcludesIgnorableWhitespace),
                 "Ignorable whitespace should be excluded");

    // Only <math> elements can be floated or positioned.  All other MathML
    // should be in-flow.
    PRBool isMath = aTag == nsGkAtoms::math;

    nsIFrame* geometricParent =
      isMath ? aState.GetGeometricParent(disp, aParentFrame) : aParentFrame;
    
    InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, newFrame);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame, isMath, isMath);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Push a null float containing block to disable floating within mathml
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(nsnull, floatSaveState, PR_FALSE,
                                    PR_FALSE);

    // Same for absolute positioning
    nsFrameConstructorSaveState absoluteSaveState;
    aState.PushAbsoluteContainingBlock(nsnull, absoluteSaveState);

    // MathML frames are inline frames, so just process their kids
    nsFrameItems childItems;
    if (!newFrame->IsLeaf()) {
      rv = ProcessChildren(aState, aContent, newFrame, PR_TRUE,
                           childItems, PR_FALSE);
    }

    CreateAnonymousFrames(aTag, aState, aContent, newFrame, PR_FALSE,
                          childItems);

    // Wrap runs of inline children in a block
    if (NS_SUCCEEDED(rv)) {
      nsFrameItems newItems;
      nsFrameItems currentBlock;
      nsIFrame* f;
      while ((f = childItems.childList) != nsnull) {
        PRBool wrapFrame = IsInlineFrame(f) || IsFrameSpecial(f);
        if (!wrapFrame) {
          rv = FlushAccumulatedBlock(aState, aContent, newFrame, &currentBlock, &newItems);
          if (NS_FAILED(rv))
            break;
        }
          
        childItems.RemoveChild(f, nsnull);
        if (wrapFrame) {
          currentBlock.AddChild(f);
        } else {
          newItems.AddChild(f);
        }
      }
      rv = FlushAccumulatedBlock(aState, aContent, newFrame, &currentBlock, &newItems);

      if (childItems.childList) {
        // an error must have occurred, delete unprocessed frames
        CleanupFrameReferences(aState.mFrameManager, childItems.childList);
        nsFrameList(childItems.childList).DestroyFrames();
      }
      
      childItems = newItems;
    }
    
    // Set the frame's initial child list
    newFrame->SetInitialChildList(nsnull, childItems.childList);
 
    return rv;
  }
  else {
    return NS_ERROR_OUT_OF_MEMORY;
  }
}
#endif // MOZ_MATHML

// SVG 
#ifdef MOZ_SVG
nsresult
nsCSSFrameConstructor::ConstructSVGFrame(nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         PRInt32                  aNameSpaceID,
                                         nsStyleContext*          aStyleContext,
                                         nsFrameItems&            aFrameItems,
                                         PRBool                   aHasPseudoParent,
                                         PRBool*                  aHaltProcessing)
{
  NS_ASSERTION(aNameSpaceID == kNameSpaceID_SVG, "SVG frame constructed in wrong namespace");
  *aHaltProcessing = PR_FALSE;

  nsresult  rv = NS_OK;
  PRBool forceView = PR_FALSE;
  PRBool isOuterSVGNode = PR_FALSE;
  const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();
  
  NS_ASSERTION(aTag != nsnull, "null SVG tag");
  if (aTag == nsnull)
    return NS_OK;

  // XXXbz somewhere here we should process pseudo frames if !aHasPseudoParent

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;
 
  // Default to aParentFrame for the geometricParent; it's adjusted in
  // cases when we allow anything else.
  nsIFrame* geometricParent = aParentFrame;

  PRBool parentIsSVG = PR_FALSE;
  if (aParentFrame && aParentFrame->GetContent()) {
    PRInt32 parentNSID;
    nsIAtom* parentTag =
      mDocument->BindingManager()->ResolveTag(aParentFrame->GetContent(),
                                              &parentNSID);

    // It's not clear whether the SVG spec intends to allow any SVG
    // content within svg:foreignObject at all (SVG 1.1, section
    // 23.2), but if it does, it better be svg:svg.  So given that
    // we're allowing it, treat it as a non-SVG parent.
    parentIsSVG = parentNSID == kNameSpaceID_SVG &&
                  parentTag != nsGkAtoms::foreignObject;
  }

  if ((aTag != nsGkAtoms::svg && !parentIsSVG) ||
      (aTag == nsGkAtoms::desc || aTag == nsGkAtoms::title)) {
    // Sections 5.1 and G.4 of SVG 1.1 say that SVG elements other than
    // svg:svg not contained within svg:svg are incorrect, although they
    // don't seem to specify error handling.  Ignore them, since many of
    // our frame classes can't deal.  It *may* be that the document
    // should at that point be considered in error according to F.2, but
    // it's hard to tell.
    //
    // Style mutation can't change this situation, so don't bother
    // adding to the undisplayed content map.
    //
    // We don't currently handle any UI for desc/title
    *aHaltProcessing = PR_TRUE;
    return NS_OK;
  }
  
  // Reduce the number of frames we create unnecessarily. Note that this is not
  // where we select which frame in a <switch> to render! That happens in
  // nsSVGSwitchFrame::PaintSVG.
  if (!NS_SVG_PassesConditionalProcessingTests(aContent)) {
    // Note that just returning is probably not right.  According
    // to the spec, <use> is allowed to use an element that fails its
    // conditional, but because we never actually create the frame when
    // a conditional fails and when we use GetReferencedFrame to find the
    // references, things don't work right.
    // XXX FIXME XXX
    *aHaltProcessing = PR_TRUE;
    return NS_OK;
  }

  // Make sure to keep IsSpecialContent in synch with this code
  if (aTag == nsGkAtoms::svg) {
    if (!parentIsSVG) {
      // This is the outermost <svg> element.
      isOuterSVGNode = PR_TRUE;

      // Set the right geometricParent
      geometricParent = aState.GetGeometricParent(disp, aParentFrame);
      
      forceView = PR_TRUE;
      newFrame = NS_NewSVGOuterSVGFrame(mPresShell, aContent, aStyleContext);
    }
    else {
      // This is an inner <svg> element
      newFrame = NS_NewSVGInnerSVGFrame(mPresShell, aContent, aStyleContext);
    }
  }
  else if (aTag == nsGkAtoms::g) {
    newFrame = NS_NewSVGGFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::svgSwitch) {
    newFrame = NS_NewSVGSwitchFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::polygon ||
           aTag == nsGkAtoms::polyline ||
           aTag == nsGkAtoms::circle ||
           aTag == nsGkAtoms::ellipse ||
           aTag == nsGkAtoms::line ||
           aTag == nsGkAtoms::rect ||
           aTag == nsGkAtoms::path)
    newFrame = NS_NewSVGPathGeometryFrame(mPresShell, aContent, aStyleContext);
  else if (aTag == nsGkAtoms::defs) {
    newFrame = NS_NewSVGContainerFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::foreignObject) {
    newFrame = NS_NewSVGForeignObjectFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::a) {
    newFrame = NS_NewSVGAFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::text) {
    nsIFrame *ancestorFrame = SVG_GetFirstNonAAncestorFrame(aParentFrame);
    if (ancestorFrame) {
      nsISVGTextContentMetrics* metrics;
      CallQueryInterface(ancestorFrame, &metrics);
      // Text cannot be nested
      if (!metrics)
        newFrame = NS_NewSVGTextFrame(mPresShell, aContent, aStyleContext);
    }
  }
  else if (aTag == nsGkAtoms::tspan) {
    nsIFrame *ancestorFrame = SVG_GetFirstNonAAncestorFrame(aParentFrame);
    if (ancestorFrame) {
      nsISVGTextContentMetrics* metrics;
      CallQueryInterface(ancestorFrame, &metrics);
      if (metrics)
        newFrame = NS_NewSVGTSpanFrame(mPresShell, aContent,
                                       ancestorFrame, aStyleContext);
    }
  }
  else if (aTag == nsGkAtoms::linearGradient) {
    newFrame = NS_NewSVGLinearGradientFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::radialGradient) {
    newFrame = NS_NewSVGRadialGradientFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::stop) {
    newFrame = NS_NewSVGStopFrame(mPresShell, aContent, aParentFrame, aStyleContext);
  }
  else if (aTag == nsGkAtoms::use) {
    newFrame = NS_NewSVGUseFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::marker) {
    newFrame = NS_NewSVGMarkerFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::image) {
    newFrame = NS_NewSVGImageFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::clipPath) {
    newFrame = NS_NewSVGClipPathFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::textPath) {
    nsIFrame *ancestorFrame = SVG_GetFirstNonAAncestorFrame(aParentFrame);
    if (ancestorFrame &&
        ancestorFrame->GetType() == nsGkAtoms::svgTextFrame) {
      newFrame = NS_NewSVGTextPathFrame(mPresShell, aContent,
                                        ancestorFrame, aStyleContext);
    }
  }
  else if (aTag == nsGkAtoms::filter) {
    newFrame = NS_NewSVGFilterFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::pattern) {
    newFrame = NS_NewSVGPatternFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::mask) {
    newFrame = NS_NewSVGMaskFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::feDistantLight ||
           aTag == nsGkAtoms::fePointLight ||
           aTag == nsGkAtoms::feSpotLight ||
           aTag == nsGkAtoms::feBlend ||
           aTag == nsGkAtoms::feColorMatrix ||
           aTag == nsGkAtoms::feFuncR ||
           aTag == nsGkAtoms::feFuncG ||
           aTag == nsGkAtoms::feFuncB ||
           aTag == nsGkAtoms::feFuncA ||
           aTag == nsGkAtoms::feComposite ||
           aTag == nsGkAtoms::feConvolveMatrix ||
           aTag == nsGkAtoms::feDisplacementMap ||
           aTag == nsGkAtoms::feFlood ||
           aTag == nsGkAtoms::feGaussianBlur ||
           aTag == nsGkAtoms::feImage ||
           aTag == nsGkAtoms::feMergeNode ||
           aTag == nsGkAtoms::feMorphology ||
           aTag == nsGkAtoms::feOffset ||
           aTag == nsGkAtoms::feTile ||
           aTag == nsGkAtoms::feTurbulence) {
    // We don't really use the frame, just need it for the style
    // information, so create the simplest possible frame.
    newFrame = NS_NewSVGLeafFrame(mPresShell, aStyleContext);
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
    // XXXldb This really isn't what the SVG spec says to do.
#ifdef DEBUG
    // printf("Warning: Creating SVGGenericContainerFrame for tag <");
    // nsAutoString str;
    // aTag->ToString(str);
    // printf("%s>\n", NS_ConvertUTF16toUTF8(str).get());
#endif
    newFrame = NS_NewSVGGenericContainerFrame(mPresShell, aContent, aStyleContext);
  }  
  // If we succeeded in creating a frame then initialize it, process its
  // children (if requested), and set the initial child list
  if (newFrame != nsnull) {
    InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, newFrame);
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, forceView);

    rv = aState.AddChild(newFrame, aFrameItems, aContent, aStyleContext,
                         aParentFrame, isOuterSVGNode, isOuterSVGNode);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsFrameItems childItems;
    if (aTag == nsGkAtoms::foreignObject) { 
      // Resolve pseudo style and create an inner block frame
      // XXX this breaks style inheritance
      nsRefPtr<nsStyleContext> innerPseudoStyle;
      innerPseudoStyle = mPresShell->StyleSet()->
        ResolvePseudoStyleFor(aContent,
                              nsCSSAnonBoxes::mozSVGForeignContent, aStyleContext);
    
      nsIFrame* blockFrame = NS_NewBlockFrame(mPresShell, innerPseudoStyle,
                                              NS_BLOCK_SPACE_MGR |
                                                NS_BLOCK_MARGIN_ROOT);
      if (NS_UNLIKELY(!blockFrame))
        return NS_ERROR_OUT_OF_MEMORY;
    
      // Claim to be relatively positioned so that we end up being the
      // absolute containing block.
      nsFrameConstructorSaveState saveState;
      aState.PushFloatContainingBlock(nsnull, saveState, PR_FALSE, PR_FALSE);
      rv = ConstructBlock(aState, innerPseudoStyle->GetStyleDisplay(), aContent,
                          newFrame, newFrame, innerPseudoStyle,
                          &blockFrame, childItems, PR_TRUE);
      // Give the blockFrame a view so that GetOffsetTo works for descendants
      // of blockFrame with views...
      nsHTMLContainerFrame::CreateViewForFrame(blockFrame, nsnull, PR_TRUE);
    } else {
      // Process the child content if requested.
      if (!newFrame->IsLeaf()) {
        rv = ProcessChildren(aState, aContent, newFrame, PR_FALSE, childItems,
                             PR_FALSE);
      }
      CreateAnonymousFrames(aTag, aState, aContent, newFrame,
                            PR_FALSE, childItems);
    }

    // Set the frame's initial child list
    newFrame->SetInitialChildList(nsnull, childItems.childList);
    return rv;
  }
  else {
    return NS_ERROR_FAILURE;
  }
}
#endif // MOZ_SVG

// If page-break-before is set, this function constructs a page break frame,
// EXCEPT for on these types of elements:
//  * row groups, rows, cells (these are handled internally by tables)
//  * fixed- and absolutely-positioned elements (currently, our positioning
//    code doesn't expect positioned frames to have nsPageBreakFrame siblings)
//
// Returns true iff we should construct a page break frame after this element.
// aStyleContext is the style context of the frame for which we're
// constructing the page break
PRBool
nsCSSFrameConstructor::PageBreakBefore(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
                                       nsFrameItems&            aFrameItems)
{
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  if (NS_STYLE_DISPLAY_NONE != display->mDisplay &&
      NS_STYLE_POSITION_FIXED    != display->mPosition &&
      NS_STYLE_POSITION_ABSOLUTE != display->mPosition &&
      (NS_STYLE_DISPLAY_TABLE == display->mDisplay ||
       !IsTableRelated(display->mDisplay, PR_TRUE))) { 
    if (display->mBreakBefore) {
      ConstructPageBreakFrame(aState, aContent, aParentFrame, aStyleContext,
                              aFrameItems);
    }
    return display->mBreakAfter;
  }
  return PR_FALSE;
}

// aStyleContext is the style context of the frame for which we're
// constructing the page break
nsresult
nsCSSFrameConstructor::ConstructPageBreakFrame(nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsStyleContext*          aStyleContext,
                                               nsFrameItems&            aFrameItems)
{
  nsRefPtr<nsStyleContext> pseudoStyle;
  // Use the same parent style context that |aStyleContext| has, since
  // that's easier to re-resolve and it doesn't matter in practice.
  // (Getting different parents can result in framechange hints, e.g.,
  // for user-modify.)
  pseudoStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(nsnull,
                   nsCSSAnonBoxes::pageBreak, aStyleContext->GetParent());
  nsIFrame* pageBreakFrame = NS_NewPageBreakFrame(mPresShell, pseudoStyle);
  if (pageBreakFrame) {
    InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, pageBreakFrame);
    aFrameItems.AddChild(pageBreakFrame);

    return NS_OK;
  }
  else {
    return NS_ERROR_OUT_OF_MEMORY;
  }
}

nsresult
nsCSSFrameConstructor::ConstructFrame(nsFrameConstructorState& aState,
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

  // never create frames for comments or PIs
  if (aContent->IsNodeOfType(nsINode::eCOMMENT) ||
      aContent->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION))
    return rv;

  nsRefPtr<nsStyleContext> styleContext;
  styleContext = ResolveStyleContext(aParentFrame, aContent);

  PRBool pageBreakAfter = PR_FALSE;

  if (aState.mPresContext->IsPaginated()) {
    // Construct a page break frame for page-break-before, and remember if
    // we need one for page-break-after.
    pageBreakAfter = PageBreakBefore(aState, aContent, aParentFrame,
                                     styleContext, aFrameItems);
  }

  // construct the frame
  rv = ConstructFrameInternal(aState, aContent, aParentFrame,
                              aContent->Tag(), aContent->GetNameSpaceID(),
                              styleContext, aFrameItems, PR_FALSE);

  if (NS_SUCCEEDED(rv) && pageBreakAfter) {
    // Construct the page break after
    ConstructPageBreakFrame(aState, aContent, aParentFrame, styleContext,
                            aFrameItems);
  }
  
  return rv;
}


nsresult
nsCSSFrameConstructor::ConstructFrameInternal( nsFrameConstructorState& aState,
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

      rv = xblService->LoadBindings(aContent, display->mBinding->mURI,
                                    display->mBinding->mOriginPrincipal,
                                    PR_FALSE, getter_AddRefs(binding.mBinding),
                                    &resolveStyle);
      if (NS_FAILED(rv))
        return NS_OK;

      if (resolveStyle) {
        styleContext = ResolveStyleContext(aParentFrame, aContent);
        display = styleContext->GetStyleDisplay();
      }

      PRInt32 nameSpaceID;
      nsCOMPtr<nsIAtom> baseTag =
        mDocument->BindingManager()->ResolveTag(aContent, &nameSpaceID);

      if (baseTag != aTag || aNameSpaceID != nameSpaceID) {
        // Construct the frame using the XBL base tag.
        rv = ConstructFrameInternal(aState,
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

  nsIFrame* adjParentFrame = aParentFrame;
  nsFrameItems* frameItems = &aFrameItems;
  PRBool pseudoParent = PR_FALSE;
  PRBool suppressFrame = PR_FALSE;
  nsFrameConstructorSaveState pseudoSaveState;
  nsresult rv = AdjustParentFrame(aState, aContent, adjParentFrame,
                                  aTag, aNameSpaceID, styleContext,
                                  frameItems, pseudoSaveState,
                                  suppressFrame, pseudoParent);
  if (NS_FAILED(rv) || suppressFrame) {
    return rv;
  }

  if (aContent->IsNodeOfType(nsINode::eTEXT)) 
    return ConstructTextFrame(aState, aContent, adjParentFrame, styleContext,
                              *frameItems, pseudoParent);

#ifdef MOZ_SVG
  // Don't create frames for non-SVG children of SVG elements
  if (aNameSpaceID != kNameSpaceID_SVG &&
      aParentFrame &&
      aParentFrame->IsFrameOfType(nsIFrame::eSVG) &&
      !aParentFrame->IsFrameOfType(nsIFrame::eSVGForeignObject)
      ) {
    return NS_OK;
  }
#endif

  // If the page contains markup that overrides text direction, and
  // does not contain any characters that would activate the Unicode
  // bidi algorithm, we need to call |SetBidiEnabled| on the pres
  // context before reflow starts.  This requires us to resolve some
  // style information now.  See bug 115921.
  {
    if (styleContext->GetStyleVisibility()->mDirection ==
        NS_STYLE_DIRECTION_RTL)
      aState.mPresContext->SetBidiEnabled();
  }
  // Start background loads during frame construction. This is just
  // a hint; the paint code will do the right thing in any case.
  {
    styleContext->GetStyleBackground();
  }

  nsIFrame* lastChild = frameItems->lastChild;

  // Handle specific frame types
  rv = ConstructHTMLFrame(aState, aContent, adjParentFrame, aTag, aNameSpaceID,
                          styleContext, *frameItems, pseudoParent);

  // Failing to find a matching HTML frame, try creating a specialized
  // XUL frame. This is temporary, pending planned factoring of this
  // whole process into separate, pluggable steps.
  if (NS_SUCCEEDED(rv) &&
      (!frameItems->childList || lastChild == frameItems->lastChild)) {
    PRBool haltProcessing;

    rv = ConstructXULFrame(aState, aContent, adjParentFrame, aTag,
                           aNameSpaceID, styleContext,
                           *frameItems, aXBLBaseTag, pseudoParent,
                           &haltProcessing);

    if (haltProcessing) {
      return rv;
    }
  } 

// MathML Mod - RBS
#ifdef MOZ_MATHML
  if (NS_SUCCEEDED(rv) &&
      (!frameItems->childList || lastChild == frameItems->lastChild)) {
    rv = ConstructMathMLFrame(aState, aContent, adjParentFrame, aTag,
                              aNameSpaceID, styleContext, *frameItems,
                              pseudoParent);
  }
#endif

// SVG
#ifdef MOZ_SVG
  if (NS_SUCCEEDED(rv) &&
      (!frameItems->childList || lastChild == frameItems->lastChild) &&
      aNameSpaceID == kNameSpaceID_SVG &&
      NS_SVGEnabled()) {
    PRBool haltProcessing;
    rv = ConstructSVGFrame(aState, aContent, adjParentFrame, aTag,
                           aNameSpaceID, styleContext,
                           *frameItems, pseudoParent, &haltProcessing);
    if (haltProcessing) {
      return rv;
    }
  }
#endif

  if (NS_SUCCEEDED(rv) &&
      (!frameItems->childList || lastChild == frameItems->lastChild)) {
    // When there is no explicit frame to create, assume it's a
    // container and let display style dictate the rest
    rv = ConstructFrameByDisplayType(aState, display, aContent, aNameSpaceID,
                                     aTag, adjParentFrame, styleContext,
                                     *frameItems, pseudoParent);
  }

  return rv;
}


inline PRBool
IsRootBoxFrame(nsIFrame *aFrame)
{
  return (aFrame->GetType() == nsGkAtoms::rootFrame);
}

nsresult
nsCSSFrameConstructor::ReconstructDocElementHierarchy()
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  return ReconstructDocElementHierarchyInternal();
}

nsresult
nsCSSFrameConstructor::ReconstructDocElementHierarchyInternal()
{
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ReconstructDocElementHierarchy\n");
  }
#endif

  nsresult rv = NS_OK;

  // XXXbz is that null-check needed?  Why?
  if (mDocument && mPresShell) {
    nsIContent *rootContent = mDocument->GetRootContent();
    
    if (rootContent) {
      nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                    nsnull, nsnull, mTempFrameTreeState);

      // Before removing the frames associated with the content object, ask them to save their
      // state onto a temporary state object.
      CaptureStateFor(state.mFrameManager->GetRootFrame(), mTempFrameTreeState);

      // Get the frame that corresponds to the document element
      nsIFrame* docElementFrame =
        state.mFrameManager->GetPrimaryFrameFor(rootContent, -1);

      if (docElementFrame) {
        // Destroy out-of-flow frames that might not be in the frame subtree
        // rooted at docElementFrame
        ::DeletingFrameSubtree(state.mFrameManager, docElementFrame);
      }

      // Remove any existing fixed items: they are always on the
      // FixedContainingBlock.  Note that this has to be done before we call
      // ClearPlaceholderFrameMap(), since RemoveFixedItems uses the
      // placeholder frame map.
      rv = RemoveFixedItems(state, docElementFrame);

      if (NS_SUCCEEDED(rv)) {
        nsPlaceholderFrame* placeholderFrame = nsnull;
        if (docElementFrame &&
            (docElementFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
          // Get the placeholder frame now, before we tear down the
          // placeholder frame map
          placeholderFrame =
            state.mFrameManager->GetPlaceholderFrameFor(docElementFrame);
          NS_ASSERTION(placeholderFrame, "No placeholder for out-of-flow?");
        }

        // Clear the hash tables that map from content to frame and out-of-flow
        // frame to placeholder frame
        state.mFrameManager->ClearPrimaryFrameMap();
        state.mFrameManager->ClearPlaceholderFrameMap();
        state.mFrameManager->ClearUndisplayedContentMap();

        if (docElementFrame) {
          // Take the docElementFrame, and remove it from its parent.
          // XXXbz So why can't we reuse ContentRemoved?

          // Notify self that we will destroy the entire frame tree, this blocks
          // RemoveMappingsForFrameSubtree() which would otherwise lead to a
          // crash since we cleared the placeholder map above (bug 398982).
          PRBool wasDestroyingFrameTree = mIsDestroyingFrameTree;
          WillDestroyFrameTree(PR_FALSE);

          rv = state.mFrameManager->RemoveFrame(docElementFrame->GetParent(),
                    GetChildListNameFor(docElementFrame), docElementFrame);
          
          if (placeholderFrame) {
            // Remove the placeholder frame first (XXX second for now) (so
            // that it doesn't retain a dangling pointer to memory)
            rv |= state.mFrameManager->RemoveFrame(placeholderFrame->GetParent(),
                                            nsnull, placeholderFrame);
          }

          mIsDestroyingFrameTree = wasDestroyingFrameTree;
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
      }
    }
    
    if (rootContent && NS_SUCCEEDED(rv)) {
      mInitialContainingBlock = nsnull;
      mRootElementStyleFrame = nsnull;

      // We don't reuse the old frame constructor state because,
      // for example, its mPopupItems may be stale
      nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                    nsnull, nsnull, mTempFrameTreeState);

      // Create the new document element hierarchy
      nsIFrame* newChild;
      rv = ConstructDocElementFrame(state, rootContent,
                                    mDocElementContainingBlock, &newChild);

      // newChild could be null even if |rv| is success, thanks to XBL.
      if (NS_SUCCEEDED(rv) && newChild) {
        rv = state.mFrameManager->InsertFrames(mDocElementContainingBlock,
                                               nsnull, nsnull, newChild);
      }
    }
  }

  return rv;
}


nsIFrame*
nsCSSFrameConstructor::GetFrameFor(nsIContent* aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(aContent);

  if (!frame)
    return nsnull;

  nsIFrame* insertionFrame = frame->GetContentInsertionFrame();

  NS_ASSERTION(insertionFrame == frame || !frame->IsLeaf(),
    "The insertion frame is the primary frame or the primary frame isn't a leaf");

  return insertionFrame;
}

nsIFrame*
nsCSSFrameConstructor::GetAbsoluteContainingBlock(nsIFrame* aFrame)
{
  NS_PRECONDITION(nsnull != mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is absolutely positioned or
  // relatively positioned
  nsIFrame* containingBlock = nsnull;
  for (nsIFrame* frame = aFrame; frame && !containingBlock;
       frame = frame->GetParent()) {
    if (frame->IsFrameOfType(nsIFrame::eMathML)) {
      // If it's mathml, bail out -- no absolute positioning out from inside
      // mathml frames.  Note that we don't make this part of the loop
      // condition because of the stuff at the end of this method...
      return nsnull;
    }
    
    // Is it positioned?
    // If it's table-related then ignore it, because for the time
    // being table-related frames are not containers for absolutely
    // positioned child frames.
    const nsStyleDisplay* disp = frame->GetStyleDisplay();

    if (disp->IsPositioned() && !IsTableRelated(disp->mDisplay, PR_TRUE)) {
      // Find the outermost wrapped block under this frame
      for (nsIFrame* wrappedFrame = aFrame; wrappedFrame != frame->GetParent();
           wrappedFrame = wrappedFrame->GetParent()) {
        nsIAtom* frameType = wrappedFrame->GetType();
        if (nsGkAtoms::blockFrame == frameType ||
#ifdef MOZ_XUL
            nsGkAtoms::XULLabelFrame == frameType ||
#endif
            nsGkAtoms::positionedInlineFrame == frameType) {
          containingBlock = wrappedFrame;
        } else if (nsGkAtoms::fieldSetFrame == frameType) {
          // If the positioned frame is a fieldset, use the area frame inside it.
          // We don't use GetContentInsertionFrame for fieldsets yet.
          containingBlock = GetFieldSetBlockFrame(wrappedFrame);
        }
      }

#ifdef DEBUG
      if (!containingBlock)
        NS_WARNING("Positioned frame that does not handle positioned kids; looking further up the parent chain");
#endif
    }
  }

  // If we found an absolutely positioned containing block, then use the
  // first-continuation.
  if (containingBlock)
    return AdjustAbsoluteContainingBlock(containingBlock);

  // If we didn't find it, then use the document element containing block
  return mHasRootAbsPosContainingBlock ? mDocElementContainingBlock : nsnull;
}

nsIFrame*
nsCSSFrameConstructor::GetFloatContainingBlock(nsIFrame* aFrame)
{
  NS_PRECONDITION(mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is a float containing block.
  // IF we hit a mathml frame, bail out; we don't allow floating out of mathml
  // frames, because they don't seem to be able to deal.
  for (nsIFrame* containingBlock = aFrame;
       containingBlock && !containingBlock->IsFrameOfType(nsIFrame::eMathML) &&
       !containingBlock->IsBoxFrame();
       containingBlock = containingBlock->GetParent()) {
    if (containingBlock->IsFloatContainingBlock()) {
      return containingBlock;
    }
  }

  // If we didn't find a containing block, then there just isn't
  // one.... return null
  return nsnull;
}

/**
 * This function will check whether aContainer has :after generated content.
 * If so, appending to it should actually insert.  The return value is the
 * parent to use for newly-appended content.  *aAfterFrame points to the :after
 * frame before which appended content should go, if there is one.
 */
static nsIFrame*
AdjustAppendParentForAfterContent(nsPresContext* aPresContext,
                                  nsIContent* aContainer,
                                  nsIFrame* aParentFrame,
                                  nsIFrame** aAfterFrame)
{
  // See if the parent has an :after pseudo-element.  Check for the presence
  // of style first, since nsLayoutUtils::GetAfterFrame is sorta expensive.
  nsStyleContext* parentStyle = aParentFrame->GetStyleContext();
  if (nsLayoutUtils::HasPseudoStyle(aContainer, parentStyle,
                                    nsCSSPseudoElements::after,
                                    aPresContext)) {
    nsIFrame* afterFrame = nsLayoutUtils::GetAfterFrame(aParentFrame);
    if (afterFrame) {
      *aAfterFrame = afterFrame;
      return afterFrame->GetParent();
    }
  }

  *aAfterFrame = nsnull;
  return aParentFrame;
}

/**
 * This function is called by ContentAppended() and ContentInserted()
 * when appending flowed frames to a parent's principal child list. It
 * handles the case where the parent frame has :after pseudo-element
 * generated content.
 */
nsresult
nsCSSFrameConstructor::AppendFrames(nsFrameConstructorState&       aState,
                                    nsIContent*                    aContainer,
                                    nsIFrame*                      aParentFrame,
                                    nsFrameItems&                  aFrameList,
                                    nsIFrame*                      aAfterFrame)
{
#ifdef DEBUG
  nsIFrame* debugAfterFrame;
  nsIFrame* debugNewParent =
    ::AdjustAppendParentForAfterContent(aState.mPresContext, aContainer,
                                        aParentFrame, &debugAfterFrame);
  NS_ASSERTION(debugNewParent == aParentFrame, "Incorrect parent");
  NS_ASSERTION(debugAfterFrame == aAfterFrame, "Incorrect after frame");
#endif

  nsFrameManager* frameManager = aState.mFrameManager;
  if (aAfterFrame) {
    NS_ASSERTION(!IsFrameSpecial(aParentFrame) ||
                 IsInlineFrame(aParentFrame) ||
                 !IsInlineOutside(aAfterFrame),
                 "Shouldn't have inline :after content on the block in an "
                 "{ib} split");
    nsFrameList frames(aParentFrame->GetFirstChild(nsnull));

    // Insert the frames before the :after pseudo-element.
    return frameManager->InsertFrames(aParentFrame, nsnull,
                                      frames.GetPrevSiblingFor(aAfterFrame),
                                      aFrameList.childList);
  }

  if (IsFrameSpecial(aParentFrame) &&
      !IsInlineFrame(aParentFrame) &&
      IsInlineOutside(aFrameList.lastChild)) {
    NS_ASSERTION(!aParentFrame->GetNextContinuation() ||
                 !aParentFrame->GetNextContinuation()->GetFirstChild(nsnull),
                 "Shouldn't happen");
    
    // We want to put some of the frames into the following inline frame.
    nsIFrame* lastBlock = FindLastBlock(aFrameList.childList);
    nsIFrame* firstTrailingInline;
    if (lastBlock) {
      firstTrailingInline = lastBlock->GetNextSibling();
      lastBlock->SetNextSibling(nsnull);
      aFrameList.lastChild = lastBlock;
    } else {
      firstTrailingInline = aFrameList.childList;
      aFrameList = nsFrameItems();
    }

    NS_ASSERTION(firstTrailingInline, "How did that happen?");
    nsIFrame* parentFrame = aParentFrame;

    // As we go up the tree creating trailing inlines, we have to move floats
    // up to ancestor blocks.  This means that at any given time we'll be
    // working with two frame constructor states, and aState is one of the two
    // only at the first step.  Create some space to do this so we don't have
    // to allocate as we go.
    char stateBuf[2 * sizeof(nsFrameConstructorState)];
    nsFrameConstructorState* sourceState = &aState;
    nsFrameConstructorState* targetState =
      reinterpret_cast<nsFrameConstructorState*>(stateBuf);

    // Now we loop, because it might be the case that the parent of our special
    // block is another special block, and that we're at the very end of it,
    // and in that case if we create a new special inline we'll have to create
    // a parent for it too.
    do {
      NS_ASSERTION(IsFrameSpecial(parentFrame) && !IsInlineFrame(parentFrame),
                   "Shouldn't be in this code");
      nsIFrame* inlineSibling = GetSpecialSibling(parentFrame);
      PRBool isPositioned = PR_FALSE;
      nsIContent* content = nsnull;
      nsStyleContext* styleContext = nsnull;
      if (!inlineSibling) {
        nsIFrame* firstInline =
          GetIBSplitSpecialPrevSiblingForAnonymousBlock(parentFrame);
        NS_ASSERTION(firstInline, "How did that happen?");

        content = firstInline->GetContent();
        styleContext = firstInline->GetStyleContext();
        isPositioned = (styleContext->GetStyleDisplay()->mPosition ==
                        NS_STYLE_POSITION_RELATIVE);
      }

      nsIFrame* stateParent =
        inlineSibling ? inlineSibling->GetParent() : parentFrame->GetParent();

      new (targetState)
        nsFrameConstructorState(mPresShell, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(stateParent),
                                GetFloatContainingBlock(stateParent));
      nsIFrame* newInlineSibling =
        MoveFramesToEndOfIBSplit(*sourceState, inlineSibling,
                                 isPositioned, content,
                                 styleContext, firstTrailingInline,
                                 parentFrame, targetState);

      if (sourceState == &aState) {
        NS_ASSERTION(targetState ==
                       reinterpret_cast<nsFrameConstructorState*>(stateBuf),
                     "Bogus target state?");
        // Set sourceState to the value targetState should have next.
        sourceState = targetState + 1;
      } else {
        // Go ahead and process whatever insertions we didn't move out
        sourceState->~nsFrameConstructorState();
      }

      // We're done with the source state.  The target becomes the new source,
      // and we point the target pointer to the available memory.
      nsFrameConstructorState* temp = sourceState;
      sourceState = targetState;
      targetState = temp;;

      if (inlineSibling) {
        // we're all set -- we just moved things to a frame that was already
        // there.
        NS_ASSERTION(newInlineSibling == inlineSibling, "What happened?");
        break;
      }

      SetFrameIsSpecial(parentFrame->GetFirstContinuation(), newInlineSibling);
      
      // We had to create a frame for this new inline sibling.  Figure out
      // the right parentage for it.
      // XXXbz add a test for this?
      nsIFrame* newParentFrame = parentFrame->GetParent();
      NS_ASSERTION(!IsInlineFrame(newParentFrame),
                   "The block in an {ib} split shouldn't be living inside "
                   "an inline");
      if (!IsFrameSpecial(newParentFrame) ||
          newParentFrame->GetNextContinuation() ||
          parentFrame->GetNextSibling()) {
        // Just insert after parentFrame
        frameManager->InsertFrames(newParentFrame, nsnull, parentFrame,
                                   newInlineSibling);
        firstTrailingInline = nsnull;
      } else {
        // recurse up the tree
        parentFrame = newParentFrame;
        firstTrailingInline = newInlineSibling;
      }      
    } while (firstTrailingInline);

    // Process the float insertions on the last target state we had.
    sourceState->~nsFrameConstructorState();
  }
    
  if (!aFrameList.childList) {
    // It all got eaten by the special inline
    return NS_OK;
  }
  
  return frameManager->AppendFrames(aParentFrame, nsnull,
                                    aFrameList.childList);
}

#define UNSET_DISPLAY 255

nsIFrame*
nsCSSFrameConstructor::FindPreviousAnonymousSibling(nsIContent*   aContainer,
                                                    nsIContent*   aChild)
{
  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(mDocument));
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
  PRUint8 childDisplay = UNSET_DISPLAY;
  while (--index >= 0) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);

    // Get its frame. If it doesn't have one, continue on to the
    // anonymous element that preceded it.
    nsIFrame* prevSibling = FindFrameForContentSibling(child, aChild,
                                                       childDisplay, PR_TRUE);
    if (prevSibling) {
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
nsIFrame*
nsCSSFrameConstructor::FindNextAnonymousSibling(nsIContent*   aContainer,
                                                nsIContent*   aChild)
{
  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(mDocument));
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
  PRUint8 childDisplay = UNSET_DISPLAY;
  while (++index < PRInt32(length)) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(PRUint32(index), getter_AddRefs(node));

    nsCOMPtr<nsIContent> child = do_QueryInterface(node);

    // Get its frame
    nsIFrame* nextSibling = FindFrameForContentSibling(child, aChild,
                                                       childDisplay, PR_FALSE);
    if (nextSibling) {
      // Found a next sibling, we're done!
      return nextSibling;
    }
  }

  return nsnull;
}

// This gets called to see if the frames corresponding to aSiblingDisplay and aDisplay
// should be siblings in the frame tree. Although (1) rows and cols, (2) row groups 
// and col groups, (3) row groups and captions, (4) legends and content inside fieldsets, (5) popups and other kids of the menu
// are siblings from a content perspective, they are not considered siblings in the 
// frame tree.
PRBool
nsCSSFrameConstructor::IsValidSibling(nsIFrame*              aSibling,
                                      nsIContent*            aContent,
                                      PRUint8&               aDisplay)
{
  nsIFrame* parentFrame = aSibling->GetParent();
  nsIAtom* parentType = nsnull;
  nsIAtom* grandparentType = nsnull;
  if (parentFrame) {
    parentType = parentFrame->GetType();
    nsIFrame* grandparentFrame = parentFrame->GetParent();
    if (grandparentFrame) {
      grandparentType = grandparentFrame->GetType();
    }
  }
    
  PRUint8 siblingDisplay = aSibling->GetStyleDisplay()->mDisplay;
  if ((NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == siblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_COLUMN       == siblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_CAPTION      == siblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == siblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == siblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == siblingDisplay) ||
      nsGkAtoms::menuFrame == parentType) {
    // if we haven't already, construct a style context to find the display type of aContent
    if (UNSET_DISPLAY == aDisplay) {
      nsRefPtr<nsStyleContext> styleContext;
      nsIFrame* styleParent;
      PRBool providerIsChild;
      if (NS_FAILED(aSibling->
                      GetParentStyleContextFrame(aSibling->PresContext(),
                                                 &styleParent,
                                                 &providerIsChild)) ||
          !styleParent) {
        NS_NOTREACHED("Shouldn't happen");
        return PR_FALSE;
      }
      styleContext = ResolveStyleContext(styleParent, aContent);
      if (!styleContext) return PR_FALSE;
      const nsStyleDisplay* display = styleContext->GetStyleDisplay();
      aDisplay = display->mDisplay;
    }
    if (nsGkAtoms::menuFrame == parentType) {
      return
        (NS_STYLE_DISPLAY_POPUP == aDisplay) ==
        (NS_STYLE_DISPLAY_POPUP == siblingDisplay);
    }
    switch (siblingDisplay) {
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
      return (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == aDisplay);
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
      return (NS_STYLE_DISPLAY_TABLE_COLUMN == aDisplay);
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
      return (NS_STYLE_DISPLAY_TABLE_CAPTION == aDisplay);
    default: // all of the row group types
      return (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aDisplay) ||
             (NS_STYLE_DISPLAY_TABLE_CAPTION      == aDisplay);
    }
  }
  else if (nsGkAtoms::fieldSetFrame == parentType ||
           (nsGkAtoms::fieldSetFrame == grandparentType &&
            nsGkAtoms::blockFrame == parentType)) {
    // Legends can be sibling of legends but not of other content in the fieldset
    nsIAtom* sibType = aSibling->GetType();
    nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(aContent));

    if ((legendContent  && (nsGkAtoms::legendFrame != sibType)) ||
        (!legendContent && (nsGkAtoms::legendFrame == sibType)))
      return PR_FALSE;
  }

  return PR_TRUE;
}

nsIFrame*
nsCSSFrameConstructor::FindFrameForContentSibling(nsIContent* aContent,
                                                  nsIContent* aTargetContent,
                                                  PRUint8& aTargetContentDisplay,
                                                  PRBool aPrevSibling)
{
  nsIFrame* sibling = mPresShell->GetPrimaryFrameFor(aContent);
  if (!sibling) {
    return nsnull;
  }

  // If the frame is out-of-flow, GPFF() will have returned the
  // out-of-flow frame; we want the placeholder.
  if (sibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    nsIFrame* placeholderFrame;
    mPresShell->GetPlaceholderFrameFor(sibling, &placeholderFrame);
    NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
    sibling = placeholderFrame;
  }

  // The frame we have now should never be a continuation
  NS_ASSERTION(!sibling->GetPrevContinuation(), "How did that happen?");

  if (aPrevSibling) {
    // The frame may be a special frame (a split inline frame that
    // contains a block).  Get the last part of that split.
    if (IsFrameSpecial(sibling)) {
      sibling = GetLastSpecialSibling(sibling);
    }

    // The frame may have a continuation. If so, we want the last
    // non-overflow-container continuation as our previous sibling.
    sibling = sibling->GetTailContinuation();
  }

  if (aTargetContent &&
      !IsValidSibling(sibling, aTargetContent, aTargetContentDisplay)) {
    sibling = nsnull;
  }

  return sibling;
}

/**
 * Find the ``rightmost'' frame for the content immediately preceding
 * aIndexInContainer, following continuations if necessary.
 */
nsIFrame*
nsCSSFrameConstructor::FindPreviousSibling(nsIContent* aContainer,
                                           PRInt32     aIndexInContainer,
                                           nsIContent* aChild)
{
  NS_ASSERTION(aContainer, "null argument");

  ChildIterator first, iter;
  nsresult rv = ChildIterator::Init(aContainer, &first, &iter);
  NS_ENSURE_SUCCESS(rv, nsnull);
  iter.seek(aIndexInContainer);

  PRUint8 childDisplay = UNSET_DISPLAY;
  // Note: not all content objects are associated with a frame (e.g., if it's
  // `display: none') so keep looking until we find a previous frame
  while (iter-- != first) {
    nsIFrame* prevSibling =
      FindFrameForContentSibling(nsCOMPtr<nsIContent>(*iter), aChild,
                                 childDisplay, PR_TRUE);

    if (prevSibling) {
#ifdef DEBUG
      nsIFrame* containerFrame = nsnull;
      containerFrame = mPresShell->GetPrimaryFrameFor(aContainer);
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
nsCSSFrameConstructor::FindNextSibling(nsIContent* aContainer,
                                       PRInt32     aIndexInContainer,
                                       nsIContent* aChild)
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
    nsIFrame* nextSibling =
      FindFrameForContentSibling(nsCOMPtr<nsIContent>(*iter), aChild,
                                 childDisplay, PR_FALSE);

    if (nextSibling) {
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

  if (containerTag == nsGkAtoms::optgroup ||
      containerTag == nsGkAtoms::select) {
    nsIContent* selectContent = aContainer;

    while (containerTag != nsGkAtoms::select) {
      selectContent = selectContent->GetParent();
      if (!selectContent) {
        break;
      }
      containerTag = selectContent->Tag();
    }

    nsCOMPtr<nsISelectElement> selectElement = do_QueryInterface(selectContent);
    if (selectElement) {
      nsAutoString selSize;
      aContainer->GetAttr(kNameSpaceID_None, nsGkAtoms::size, selSize);
      if (!selSize.IsEmpty()) {
        PRInt32 err;
        return (selSize.ToInteger(&err) > 1);
      }
    }
  }

  return PR_FALSE;
}

// For fieldsets, returns the area frame, if the child is not a legend. 
static nsIFrame*
GetAdjustedParentFrame(nsIFrame*       aParentFrame,
                       nsIAtom*        aParentFrameType,
                       nsIContent*     aParentContent,
                       PRInt32         aChildIndex)
{
  NS_PRECONDITION(nsGkAtoms::tableOuterFrame != aParentFrameType,
                  "Shouldn't be happening!");
  
  nsIContent *childContent = aParentContent->GetChildAt(aChildIndex);
  nsIFrame* newParent = nsnull;

  if (nsGkAtoms::fieldSetFrame == aParentFrameType) {
    // If the parent is a fieldSet, use the fieldSet's area frame as the
    // parent unless the new content is a legend. 
    nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(childContent));
    if (!legendContent) {
      newParent = GetFieldSetBlockFrame(aParentFrame);
    }
  }
  return (newParent) ? newParent : aParentFrame;
}

static void
InvalidateCanvasIfNeeded(nsIFrame* aFrame);

static PRBool
IsSpecialFramesetChild(nsIContent* aContent)
{
  // IMPORTANT: This must match the conditions in nsHTMLFramesetFrame::Init.
  return aContent->IsNodeOfType(nsINode::eHTML) &&
    (aContent->Tag() == nsGkAtoms::frameset ||
     aContent->Tag() == nsGkAtoms::frame);
}

nsresult
nsCSSFrameConstructor::ContentAppended(nsIContent*     aContainer,
                                       PRInt32         aNewIndexInContainer)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended container=%p index=%d\n",
           static_cast<void*>(aContainer), aNewIndexInContainer);
    if (gReallyNoisyContentUpdates && aContainer) {
      aContainer->List(stdout, 0);
    }
  }
#endif

#ifdef MOZ_XUL
  if (aContainer) {
    PRInt32 namespaceID;
    nsIAtom* tag =
      mDocument->BindingManager()->ResolveTag(aContainer, &namespaceID);

    // Just ignore tree tags, anyway we don't create any frames for them.
    if (tag == nsGkAtoms::treechildren ||
        tag == nsGkAtoms::treeitem ||
        tag == nsGkAtoms::treerow ||
        (namespaceID == kNameSpaceID_XUL && gUseXBLForms &&
         ShouldIgnoreSelectChild(aContainer)))
      return NS_OK;

  }
#endif // MOZ_XUL

  // Get the frame associated with the content
  nsIFrame* parentFrame = GetFrameFor(aContainer);
  if (! parentFrame)
    return NS_OK;

  // See if we have an XBL insertion point. If so, then that's our
  // real parent frame; if not, then the frame hasn't been built yet
  // and we just bail.
  //
  nsIFrame* insertionPoint;
  PRBool multiple = PR_FALSE;
  GetInsertionPoint(parentFrame, nsnull, &insertionPoint, &multiple);
  if (! insertionPoint)
    return NS_OK; // Don't build the frames.

  PRBool hasInsertion = PR_FALSE;
  if (!multiple) {
    nsIDocument* document = nsnull; 
    nsIContent *firstAppendedChild =
      aContainer->GetChildAt(aNewIndexInContainer);
    if (firstAppendedChild) {
      document = firstAppendedChild->GetDocument();
    }
    if (document &&
        document->BindingManager()->GetInsertionParent(firstAppendedChild)) {
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
          GetInsertionPoint(parentFrame, child, &insertionPoint);
          if (!insertionPoint) {
            // This content node doesn't have an insertion point, so we just
            // skip over it
            continue;
          }
          insertionContent = insertionPoint->GetContent();
        }

        // Construct an iterator to locate this child at its correct index.
        ChildIterator iter, last;
        for (ChildIterator::Init(insertionContent, &iter, &last);
         iter != last;
         ++iter) {
          LAYOUT_PHASE_TEMP_EXIT();
          nsIContent* item = nsCOMPtr<nsIContent>(*iter);
          if (item == child)
            // Call ContentInserted with this index.
            ContentInserted(aContainer, child,
                            iter.position(), mTempFrameTreeState);
          LAYOUT_PHASE_TEMP_REENTER();
        }
      }

      return NS_OK;
    }
  }

  parentFrame = insertionPoint;

  if (parentFrame->GetType() == nsGkAtoms::frameSetFrame) {
    // Check whether we have any kids we care about.
    PRUint32 count = aContainer->GetChildCount();
    for (PRUint32 i = aNewIndexInContainer; i < count; ++i) {
      if (IsSpecialFramesetChild(aContainer->GetChildAt(i))) {
        // Just reframe the parent, since framesets are weird like that.
        return RecreateFramesForContent(parentFrame->GetContent());
      }
    }
  }
  
  if (parentFrame->IsLeaf()) {
    // Nothing to do here; we shouldn't be constructing kids of leaves
    return NS_OK;
  }
  
#ifdef MOZ_MATHML
  if (parentFrame->IsFrameOfType(nsIFrame::eMathML))
    return RecreateFramesForContent(parentFrame->GetContent());
#endif

  // If the frame we are manipulating is a ``special'' frame (that is, one
  // that's been created as a result of a block-in-inline situation) then we
  // need to append to the last special sibling, not to the frame itself.
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
    parentFrame = GetLastSpecialSibling(parentFrame);
  }

  // Get continuation that parents the last child
  parentFrame = nsLayoutUtils::GetLastContinuationWithChild(parentFrame);

  nsIAtom* frameType = parentFrame->GetType();
  // Deal with fieldsets
  parentFrame = ::GetAdjustedParentFrame(parentFrame, frameType,
                                         aContainer, aNewIndexInContainer);

  // Deal with possible :after generated content on the parent
  nsIFrame* parentAfterFrame;
  parentFrame =
    ::AdjustAppendParentForAfterContent(mPresShell->GetPresContext(),
                                        aContainer, parentFrame,
                                        &parentAfterFrame);
  
  // Create some new frames
  PRUint32                count;
  nsFrameItems            frameItems;
  nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(parentFrame),
                                GetFloatContainingBlock(parentFrame));

  // See if the containing block has :first-letter style applied.
  PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
  nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
  if (containingBlock) {
    haveFirstLetterStyle = HasFirstLetterStyle(containingBlock);
    haveFirstLineStyle =
      ShouldHaveFirstLineStyle(containingBlock->GetContent(),
                               containingBlock->GetStyleContext());
  }

  if (haveFirstLetterStyle) {
    // Before we get going, remove the current letter frames
    RemoveLetterFrames(state.mPresContext, state.mPresShell,
                       state.mFrameManager, containingBlock);
  }

  // if the container is a table and a caption was appended, it needs to be put in
  // the outer table frame's additional child list. 
  nsFrameItems captionItems;

  // The last frame that we added to the list.
  nsIFrame* oldNewFrame = nsnull;

  PRUint32 i;
  count = aContainer->GetChildCount();
  for (i = aNewIndexInContainer; i < count; i++) {
    nsIFrame* newFrame = nsnull;
    nsIContent *childContent = aContainer->GetChildAt(i);

    ConstructFrame(state, childContent, parentFrame, frameItems);
    newFrame = frameItems.lastChild;

    if (newFrame && newFrame != oldNewFrame) {
      InvalidateCanvasIfNeeded(newFrame);
      oldNewFrame = newFrame;
    }
  }

  if (nsGkAtoms::tableFrame == frameType) {
    // Pull out the captions.  Note that we don't want to do that as we go,
    // because processing a single caption can add a whole bunch of things to
    // the frame items due to pseudoframe processing.  So we'd have to pull
    // captions from a list anyway; might as well do that here.
    PullOutCaptionFrames(frameItems, captionItems);
  }
  

  // process the current pseudo frame state
  if (!state.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(state, frameItems);
  }

  if (haveFirstLineStyle && parentFrame == containingBlock) {
    // It's possible that some of the new frames go into a
    // first-line frame. Look at them and see...
    AppendFirstLineFrames(state, containingBlock->GetContent(),
                          containingBlock, frameItems); 
  }

  nsresult result = NS_OK;

  // Notify the parent frame passing it the list of new frames
  if (NS_SUCCEEDED(result) &&
      (frameItems.childList || captionItems.childList)) {
    // Perform special check for diddling around with the frames in
    // a special inline frame.

    // If we're appending before :after content, then we're not really
    // appending, so let WipeContainingBlock know that.
    if (WipeContainingBlock(state, containingBlock, parentFrame, frameItems,
                            !parentAfterFrame, nsnull)) {
      return NS_OK;
    }

    // Append the flowed frames to the principal child list, tables need special treatment
    if (nsGkAtoms::tableFrame == frameType) {
      if (captionItems.childList) { // append the caption to the outer table
        nsIFrame* outerTable = parentFrame->GetParent();
        if (outerTable) { 
          state.mFrameManager->AppendFrames(outerTable,
                                            nsGkAtoms::captionList,
                                            captionItems.childList);
        }
      }
      if (frameItems.childList) { // append children of the inner table
        AppendFrames(state, aContainer, parentFrame, frameItems,
                     parentAfterFrame);
      }
    }
    else {
      AppendFrames(state, aContainer, parentFrame, frameItems,
                   parentAfterFrame);
    }
  }

  // Recover first-letter frames
  if (haveFirstLetterStyle) {
    RecoverLetterFrames(state, containingBlock);
  }

#ifdef DEBUG
  if (gReallyNoisyContentUpdates) {
    nsIFrameDebug* fdbg = nsnull;
    CallQueryInterface(parentFrame, &fdbg);
    if (fdbg) {
      printf("nsCSSFrameConstructor::ContentAppended: resulting frame model:\n");
      fdbg->List(stdout, 0);
    }
  }
#endif

  return NS_OK;
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

  if (aContainer->IsNodeOfType(nsINode::eXUL) &&
      aChild->IsNodeOfType(nsINode::eXUL) &&
      aContainer->Tag() == nsGkAtoms::listbox &&
      aChild->Tag() == nsGkAtoms::listitem) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(aContainer);
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsPIListBoxObject> listBoxObject = do_QueryInterface(boxObject);
    if (listBoxObject) {
      nsListBoxBodyFrame* listBoxBodyFrame = listBoxObject->GetListBoxBody(PR_FALSE);
      if (listBoxBodyFrame) {
        if (aOperation == CONTENT_REMOVED) {
          // Except if we have an aChildFrame and its parent is not the right
          // thing, then we don't do this.  Pseudo frames are so much fun....
          if (!aChildFrame || aChildFrame->GetParent() == listBoxBodyFrame) {
            listBoxBodyFrame->OnContentRemoved(aPresContext, aChildFrame,
                                               aIndexInContainer);
            return PR_TRUE;
          }
        } else {
          listBoxBodyFrame->OnContentInserted(aPresContext, aChild);
          return PR_TRUE;
        }
      }
    }
  }

  PRInt32 namespaceID;
  aDocument->BindingManager()->ResolveTag(aContainer, &namespaceID);

  // XBL form control cruft... should that really be testing that the
  // namespace is XUL?  Seems odd...
  if (aUseXBLForms && aContainer->GetParent() &&
      namespaceID == kNameSpaceID_XUL && ShouldIgnoreSelectChild(aContainer))
    return PR_TRUE;

  return PR_FALSE;
}
#endif // MOZ_XUL

nsresult
nsCSSFrameConstructor::ContentInserted(nsIContent*            aContainer,
                                       nsIContent*            aChild,
                                       PRInt32                aIndexInContainer,
                                       nsILayoutHistoryState* aFrameState)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");

  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentInserted container=%p child=%p index=%d\n",
           static_cast<void*>(aContainer),
           static_cast<void*>(aChild),
           aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      (aContainer ? aContainer : aChild)->List(stdout, 0);
    }
  }
#endif

  nsresult rv = NS_OK;

#ifdef MOZ_XUL
  if (NotifyListBoxBody(mPresShell->GetPresContext(), aContainer, aChild,
                        aIndexInContainer, 
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
      nsFrameConstructorState state(mPresShell, mFixedContainingBlock, nsnull,
                                    nsnull, aFrameState);
      rv = ConstructDocElementFrame(state,
                                    docElement, 
                                    mDocElementContainingBlock,
                                    &docElementFrame);
    
      if (NS_SUCCEEDED(rv) && docElementFrame) {
        if (mDocElementContainingBlock->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
          // Set the initial child list for the parent and wait on the initial
          // reflow.
          mDocElementContainingBlock->SetInitialChildList(nsnull, 
                                                          docElementFrame);
        } else {
          // Whoops, we've already received our initial reflow! Insert the doc.
          // element as a child so it reflows (note that containing block is
          // empty, so we can simply append).
          NS_ASSERTION(mDocElementContainingBlock->GetFirstChild(nsnull) == nsnull,
                       "Unexpected child of document element containing block");
          mDocElementContainingBlock->AppendFrames(nsnull, docElementFrame);
        }
        InvalidateCanvasIfNeeded(docElementFrame);
#ifdef DEBUG
        if (gReallyNoisyContentUpdates) {
          nsIFrameDebug* fdbg = nsnull;
          CallQueryInterface(docElementFrame, &fdbg);
          if (fdbg) {
            printf("nsCSSFrameConstructor::ContentInserted: resulting frame model:\n");
            fdbg->List(stdout, 0);
          }
        }
#endif
      }
    }

    // otherwise this is not a child of the root element, and we
    // won't let it have a frame.
    return NS_OK;
  }

  // Otherwise, we've got parent content. Find its frame.
  nsIFrame* parentFrame = GetFrameFor(aContainer);
  if (! parentFrame)
    return NS_OK; // XXXwaterson will this break selects? (See ``Here
    // we have been notified...'' below.)

  // See if we have an XBL insertion point. If so, then that's our
  // real parent frame; if not, then the frame hasn't been built yet
  // and we just bail.
  nsIFrame* insertionPoint;
  GetInsertionPoint(parentFrame, aChild, &insertionPoint);
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
    ? FindPreviousSibling(container, aIndexInContainer, aChild)
    : FindPreviousAnonymousSibling(aContainer, aChild);

  PRBool    isAppend = PR_FALSE;
  nsIFrame* appendAfterFrame;  // This is only looked at when isAppend is true
  nsIFrame* nextSibling = nsnull;
    
  // If there is no previous sibling, then find the frame that follows
  if (! prevSibling) {
    nextSibling = (aIndexInContainer >= 0)
      ? FindNextSibling(container, aIndexInContainer, aChild)
      : FindNextAnonymousSibling(aContainer, aChild);
  }

  // Now, find the geometric parent so that we can handle
  // continuations properly. Use the prev sibling if we have it;
  // otherwise use the next sibling.
  if (prevSibling) {
    parentFrame = prevSibling->GetParent()->GetContentInsertionFrame();
  }
  else if (nextSibling) {
    parentFrame = nextSibling->GetParent()->GetContentInsertionFrame();
  }
  else {
    // No previous or next sibling, so treat this like an appended frame.
    isAppend = PR_TRUE;
    // Deal with fieldsets
    parentFrame = ::GetAdjustedParentFrame(parentFrame, parentFrame->GetType(),
                                           aContainer, aIndexInContainer);
    parentFrame =
      ::AdjustAppendParentForAfterContent(mPresShell->GetPresContext(),
                                          aContainer, parentFrame,
                                          &appendAfterFrame);
  }

  if (parentFrame->GetType() == nsGkAtoms::frameSetFrame &&
      IsSpecialFramesetChild(aChild)) {
    // Just reframe the parent, since framesets are weird like that.
    return RecreateFramesForContent(parentFrame->GetContent());
  }
  
  // Don't construct kids of leaves
  if (parentFrame->IsLeaf()) {
    return NS_OK;
  }

#ifdef MOZ_MATHML
  if (parentFrame->IsFrameOfType(nsIFrame::eMathML))
    return RecreateFramesForContent(parentFrame->GetContent());
#endif

  nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(parentFrame),
                                GetFloatContainingBlock(parentFrame),
                                aFrameState);


  // Recover state for the containing block - we need to know if
  // it has :first-letter or :first-line style applied to it. The
  // reason we care is that the internal structure in these cases
  // is not the normal structure and requires custom updating
  // logic.
  nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
  PRBool haveFirstLetterStyle = PR_FALSE;
  PRBool haveFirstLineStyle = PR_FALSE;

  // In order to shave off some cycles, we only dig up the
  // containing block haveFirst* flags if the parent frame where
  // the insertion/append is occurring is an inline or block
  // container. For other types of containers this isn't relevant.
  const nsStyleDisplay* parentDisplay = parentFrame->GetStyleDisplay();

  // Examine the parentFrame where the insertion is taking
  // place. If it's a certain kind of container then some special
  // processing is done.
  if ((NS_STYLE_DISPLAY_BLOCK == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_LIST_ITEM == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_INLINE == parentDisplay->mDisplay) ||
      (NS_STYLE_DISPLAY_INLINE_BLOCK == parentDisplay->mDisplay)) {
    // Recover the special style flags for the containing block
    if (containingBlock) {
      haveFirstLetterStyle = HasFirstLetterStyle(containingBlock);
      haveFirstLineStyle =
        ShouldHaveFirstLineStyle(containingBlock->GetContent(),
                                 containingBlock->GetStyleContext());
    }

    if (haveFirstLetterStyle) {
      // Get the correct parentFrame and prevSibling - if a
      // letter-frame is present, use its parent.
      if (parentFrame->GetType() == nsGkAtoms::letterFrame) {
        parentFrame = parentFrame->GetParent();
        container = parentFrame->GetContent();
      }

      // Remove the old letter frames before doing the insertion
      RemoveLetterFrames(state.mPresContext, mPresShell,
                         state.mFrameManager,
                         state.mFloatedItems.containingBlock);

      // Removing the letterframes messes around with the frame tree, removing
      // and creating frames.  We need to reget our prevsibling.
      // See XXX comment the first time we do this in this method....
      prevSibling = (aIndexInContainer >= 0)
        ? FindPreviousSibling(container, aIndexInContainer, aChild)
        : FindPreviousAnonymousSibling(aContainer, aChild);

      // If there is no previous sibling, then find the frame that follows
      if (! prevSibling) {
        nextSibling = (aIndexInContainer >= 0)
          ? FindNextSibling(container, aIndexInContainer, aChild)
          : FindNextAnonymousSibling(aContainer, aChild);
      }
    }
  }

  if (!prevSibling) {
    // We're inserting the new frame as the first child. See if the
    // parent has a :before pseudo-element
    nsIFrame* firstChild = parentFrame->GetFirstChild(nsnull);

    if (firstChild &&
        nsLayoutUtils::IsGeneratedContentFor(aContainer, firstChild,
                                             nsCSSPseudoElements::before)) {
      // Insert the new frames after the last continuation of the :before
      prevSibling = firstChild->GetTailContinuation();
      parentFrame = prevSibling->GetParent();
      // We perhaps could leave this true and take the AppendFrames path
      // below, but we'd have to update appendAfterFrame and it seems safer
      // to force all insert-after-:before cases to take these to take the
      // InsertFrames path
      isAppend = PR_FALSE;
    }
  }

  // if the container is a table and a caption will be appended, it needs to be
  // put in the outer table frame's additional child list.
  
  nsFrameItems frameItems, captionItems;

  ConstructFrame(state, aChild, parentFrame, frameItems);
  if (frameItems.childList) {
    InvalidateCanvasIfNeeded(frameItems.childList);
    
    if (nsGkAtoms::tableCaptionFrame == frameItems.childList->GetType()) {
      NS_ASSERTION(frameItems.childList == frameItems.lastChild ,
                   "adding a non caption frame to the caption childlist?");
      captionItems.AddChild(frameItems.childList);
      frameItems = nsFrameItems();
    }
  }

  // process the current pseudo frame state
  if (!state.mPseudoFrames.IsEmpty())
    ProcessPseudoFrames(state, frameItems);

  // If the parent of our current prevSibling is different from the frame we'll
  // actually use as the parent, then the calculated insertion point is now
  // invalid and as it is unknown where to insert correctly we append instead
  // (bug 341858).
  if (prevSibling && frameItems.childList &&
      frameItems.childList->GetParent() != prevSibling->GetParent()) {
    prevSibling = nsnull;
    isAppend = PR_TRUE;
    parentFrame =
      ::AdjustAppendParentForAfterContent(mPresShell->GetPresContext(),
                                          aContainer,
                                          frameItems.childList->GetParent(),
                                          &appendAfterFrame);
  }

  // Perform special check for diddling around with the frames in
  // a special inline frame.

  // If we're appending before :after content, then we're not really
  // appending, so let WipeContainingBlock know that.
  if (WipeContainingBlock(state, containingBlock, parentFrame, frameItems,
                          isAppend && !appendAfterFrame, prevSibling))
    return NS_OK;

  if (haveFirstLineStyle && parentFrame == containingBlock) {
    // It's possible that the new frame goes into a first-line
    // frame. Look at it and see...
    if (isAppend) {
      // Use append logic when appending
      AppendFirstLineFrames(state, containingBlock->GetContent(),
                            containingBlock, frameItems); 
    }
    else {
      // Use more complicated insert logic when inserting
      InsertFirstLineFrames(state, aContainer, containingBlock, &parentFrame,
                            prevSibling, frameItems);
    }
  }
      
  nsIFrame* const newFrame = frameItems.childList;
  if (NS_SUCCEEDED(rv) && newFrame) {
    NS_ASSERTION(!captionItems.childList, "leaking caption frames");
    // Notify the parent frame
    if (isAppend) {
      AppendFrames(state, aContainer, parentFrame, frameItems,
                   appendAfterFrame);
    } else {
      state.mFrameManager->InsertFrames(parentFrame,
                                        nsnull, prevSibling, newFrame);
    }
  }
  else {
    // we might have a caption treat it here
    nsIFrame* newCaptionFrame = captionItems.childList;
    if (NS_SUCCEEDED(rv) && newCaptionFrame) {
      nsIFrame* outerTableFrame;
      if (GetCaptionAdjustedParent(parentFrame, newCaptionFrame, &outerTableFrame)) {
        // If the parent of our current prevSibling is different from the frame
        // we'll actually use as the parent, then the calculated insertion
        // point is now invalid (bug 341382).
        if (prevSibling && prevSibling->GetParent() != outerTableFrame) {
          prevSibling = nsnull;
        }
        // If the parent is not a outer table frame we will try to add frames
        // to a named child list that the parent does not honour and the frames
        // will get lost
        NS_ASSERTION(nsGkAtoms::tableOuterFrame == outerTableFrame->GetType(),
                     "Pseudo frame construction failure, "
                     "a caption can be only a child of a outer table frame");
        if (isAppend) {
          state.mFrameManager->AppendFrames(outerTableFrame,
                                            nsGkAtoms::captionList,
                                            newCaptionFrame);
        }
        else {
          state.mFrameManager->InsertFrames(outerTableFrame,
                                            nsGkAtoms::captionList,
                                            prevSibling, newCaptionFrame);
        }
      }
    }
  }

  if (haveFirstLetterStyle) {
    // Recover the letter frames for the containing block when
    // it has first-letter style.
    RecoverLetterFrames(state, state.mFloatedItems.containingBlock);
  }

#ifdef DEBUG
  if (gReallyNoisyContentUpdates && parentFrame) {
    nsIFrameDebug* fdbg = nsnull;
    CallQueryInterface(parentFrame, &fdbg);
    if (fdbg) {
      printf("nsCSSFrameConstructor::ContentInserted: resulting frame model:\n");
      fdbg->List(stdout, 0);
    }
  }
#endif

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ReinsertContent(nsIContent* aContainer,
                                       nsIContent* aChild)
{
  PRInt32 ix = aContainer->IndexOf(aChild);
  // XXX For now, do a brute force remove and insert.
  // XXXbz this probably doesn't work so well with anonymous content
  // XXXbz doesn't this need to do the state-saving stuff that
  // RecreateFramesForContent does?
  PRBool didReconstruct;
  nsresult res = ContentRemoved(aContainer, aChild, ix, &didReconstruct);

  if (NS_SUCCEEDED(res) && !didReconstruct) {
    // If ContentRemoved just reconstructed everything, there is no need to
    // reinsert the content here
    res = ContentInserted(aContainer, aChild, ix, nsnull);
  }

  return res;
}

static void
DoDeletingFrameSubtree(nsFrameManager* aFrameManager,
                       nsVoidArray&    aDestroyQueue,
                       nsIFrame*       aRemovedFrame,
                       nsIFrame*       aFrame);

static void
DoDeletingOverflowContainers(nsFrameManager* aFrameManager,
                             nsVoidArray&    aDestroyQueue,
                             nsIFrame*       aRemovedFrame,
                             nsIFrame*       aFrame)
{
  // The invariant that "continuing frames should be found as part of the
  // walk over the top-most frame's continuing frames" does not hold for
  // out-of-flow overflow containers, so we need to walk them too.
  // Note that DoDeletingFrameSubtree() skips the child lists where
  // overflow containers live so we won't process them twice.
  const PRBool orphanSubtree = aRemovedFrame == aFrame;
  for (nsIFrame* next = aFrame->GetNextContinuation();
       next && (next->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER);
       next = next->GetNextContinuation()) {
    DoDeletingFrameSubtree(aFrameManager, aDestroyQueue,
                           orphanSubtree ? next : aRemovedFrame,
                           next);
  }
}

/**
 * Called when a frame subtree is about to be deleted. Two important
 * things happen:
 *
 * 1. For each frame in the subtree, we remove the mapping from the
 *    content object to its frame
 *
 * 2. For child frames that have been moved out of the flow, we enqueue
 *    the out-of-flow frame for deletion *if* the out-of-flow frame's
 *    geometric parent is not in |aRemovedFrame|'s hierarchy (e.g., an
 *    absolutely positioned element that has been promoted to be a direct
 *    descendant of an area frame).
 *
 * Note: this function should only be called by DeletingFrameSubtree()
 *
 * @param   aRemovedFrame this is the frame that was removed from the
 *            content model. As we recurse we need to remember this so we
 *            can check if out-of-flow frames are a descendant of the frame
 *            being removed
 * @param   aFrame the local subtree that is being deleted. This is initially
 *            the same as aRemovedFrame, but as we recurse down the tree
 *            this changes
 */
static void
DoDeletingFrameSubtree(nsFrameManager* aFrameManager,
                       nsVoidArray&    aDestroyQueue,
                       nsIFrame*       aRemovedFrame,
                       nsIFrame*       aFrame)
{
#undef RECURSE
#define RECURSE(top, child)                                                  \
  DoDeletingFrameSubtree(aFrameManager, aDestroyQueue, (top), (child));      \
  DoDeletingOverflowContainers(aFrameManager, aDestroyQueue, (top), (child));

  // Remove the mapping from the content object to its frame.
  nsIContent* content = aFrame->GetContent();
  if (content) {
    aFrameManager->RemoveAsPrimaryFrame(content, aFrame);
    aFrameManager->ClearAllUndisplayedContentIn(content);
  }

  nsIAtom* childListName = nsnull;
  PRInt32 childListIndex = 0;

  do {
    // Walk aFrame's normal flow child frames looking for placeholder frames.
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    for (; childFrame; childFrame = childFrame->GetNextSibling()) {
      NS_ASSERTION(!(childFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
                   "out-of-flow on wrong child list");
      if (NS_LIKELY(nsGkAtoms::placeholderFrame != childFrame->GetType())) {
        RECURSE(aRemovedFrame, childFrame);
      } else {
        nsIFrame* outOfFlowFrame =
          nsPlaceholderFrame::GetRealFrameForPlaceholder(childFrame);
  
        // Remove the mapping from the out-of-flow frame to its placeholder.
        aFrameManager->UnregisterPlaceholderFrame((nsPlaceholderFrame*)childFrame);
        // Don't SetOutOfFlowFrame(nsnull) here because the float cache depends
        // on it when the float is removed later on, see bug 348688 comment 6.
        
        // Queue the out-of-flow frame to be destroyed only if aRemovedFrame is _not_
        // one of its ancestor frames or if it is a popup frame. 
        // If aRemovedFrame is an ancestor of the out-of-flow frame, then 
        // the out-of-flow frame will be destroyed by aRemovedFrame.
        if (outOfFlowFrame->GetStyleDisplay()->mDisplay == NS_STYLE_DISPLAY_POPUP ||
            !nsLayoutUtils::IsProperAncestorFrame(aRemovedFrame, outOfFlowFrame)) {
          NS_ASSERTION(aDestroyQueue.IndexOf(outOfFlowFrame) == kNotFound,
                       "out-of-flow is already in the destroy queue");
          aDestroyQueue.AppendElement(outOfFlowFrame);
          // Recurse into the out-of-flow, it is now the aRemovedFrame.
          RECURSE(outOfFlowFrame, outOfFlowFrame);
        }
        else {
          // Also recurse into the out-of-flow when it's a descendant of aRemovedFrame
          // since we don't walk those lists, see |childListName| increment below.
          RECURSE(aRemovedFrame, outOfFlowFrame);
        }
      }
    }

    // Move to next child list but skip lists with frames we should have
    // a placeholder for or that contains only next-in-flow overflow containers
    // (which we walk explicitly above).
    do {
      childListName = aFrame->GetAdditionalChildListName(childListIndex++);
    } while (IsOutOfFlowList(childListName) ||
             childListName == nsGkAtoms::overflowContainersList ||
             childListName == nsGkAtoms::excessOverflowContainersList);
  } while (childListName);
}

/**
 * Called when a frame is about to be deleted. Calls DoDeletingFrameSubtree()
 * for aFrame and each of its continuing frames
 */
static nsresult
DeletingFrameSubtree(nsFrameManager* aFrameManager,
                     nsIFrame*       aFrame)
{
  NS_ENSURE_TRUE(aFrame, NS_OK); // XXXldb Remove this sometime in the future.

  // If there's no frame manager it's probably because the pres shell is
  // being destroyed.
  if (NS_UNLIKELY(!aFrameManager)) {
    return NS_OK;
  }

  nsAutoVoidArray destroyQueue;

  // If it's a "special" block-in-inline frame, then we can't really deal.
  // That really shouldn't be happening.
  NS_ASSERTION(!IsFrameSpecial(aFrame),
               "DeletingFrameSubtree on a special frame.  Prepare to crash.");

  do {
    DoDeletingFrameSubtree(aFrameManager, destroyQueue, aFrame, aFrame);

    // If it's split, then get the continuing frame. Note that we only do
    // this for the top-most frame being deleted. Don't do it if we're
    // recursing over a subtree, because those continuing frames should be
    // found as part of the walk over the top-most frame's continuing frames.
    // Walking them again will make this an N^2/2 algorithm.
    // The above is true for normal child next-in-flows but not overflow
    // containers which we do walk because they *can* escape the subtree
    // we're deleting.  We skip [excess]overflowContainersList where
    // they live to avoid processing them more than once.
    aFrame = aFrame->GetNextContinuation();
  } while (aFrame);

  // Now destroy any out-of-flow frames that have been enqueued for
  // destruction.
  for (PRInt32 i = destroyQueue.Count() - 1; i >= 0; --i) {
    nsIFrame* outOfFlowFrame = static_cast<nsIFrame*>(destroyQueue[i]);

    // Ask the out-of-flow's parent to delete the out-of-flow
    // frame from the right list.
    aFrameManager->RemoveFrame(outOfFlowFrame->GetParent(),
                               GetChildListNameFor(outOfFlowFrame),
                               outOfFlowFrame);
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveMappingsForFrameSubtree(nsIFrame* aRemovedFrame)
{
  if (NS_UNLIKELY(mIsDestroyingFrameTree)) {
    // The frame tree might not be in a consistent state after
    // WillDestroyFrameTree() has been called. Most likely we're destroying
    // the pres shell which means the frame manager takes care of clearing all
    // mappings so there is no need to walk the frame tree here, bug 372576.
    return NS_OK;
  }

  // Save the frame tree's state before deleting it
  CaptureStateFor(aRemovedFrame, mTempFrameTreeState);

  return ::DeletingFrameSubtree(mPresShell->FrameManager(), aRemovedFrame);
}

static void UnregisterPlaceholderChain(nsFrameManager* frameManager,
                                       nsPlaceholderFrame* placeholderFrame)
{
  // Remove the mapping from the frame to its placeholder
  nsPlaceholderFrame* curFrame = placeholderFrame;
  do {
    frameManager->UnregisterPlaceholderFrame(curFrame);
    curFrame->SetOutOfFlowFrame(nsnull);
    curFrame = static_cast<nsPlaceholderFrame*>(curFrame->GetNextContinuation());
  } while (curFrame);
}

nsresult
nsCSSFrameConstructor::ContentRemoved(nsIContent* aContainer,
                                      nsIContent* aChild,
                                      PRInt32     aIndexInContainer,
                                      PRBool*     aDidReconstruct)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while destroying frames");

  *aDidReconstruct = PR_FALSE;
  
  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentRemoved container=%p child=%p index=%d\n",
           static_cast<void*>(aContainer),
           static_cast<void*>(aChild),
           aIndexInContainer);
    if (gReallyNoisyContentUpdates) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsFrameManager *frameManager = mPresShell->FrameManager();
  nsPresContext *presContext = mPresShell->GetPresContext();
  nsresult                  rv = NS_OK;

  // Find the child frame that maps the content
  nsIFrame* childFrame =
    mPresShell->FrameManager()->GetPrimaryFrameFor(aChild, aIndexInContainer);

  if (!childFrame || childFrame->GetContent() != aChild) {
    // XXXbz the GetContent() != aChild check is needed due to bug 135040.
    // Remove it once that's fixed.
    frameManager->ClearUndisplayedContentIn(aChild, aContainer);
  }

#ifdef MOZ_XUL
  if (NotifyListBoxBody(presContext, aContainer, aChild, aIndexInContainer, 
                        mDocument, childFrame, gUseXBLForms, CONTENT_REMOVED))
    return NS_OK;

#endif // MOZ_XUL

  if (childFrame) {
    InvalidateCanvasIfNeeded(childFrame);
    
    // If the frame we are manipulating is a special frame then do
    // something different instead of just inserting newly created
    // frames.
    // NOTE: if we are in ReinsertContent, 
    //       then do not reframe as we are already doing just that!
    if (MaybeRecreateContainerForIBSplitterFrame(childFrame, &rv)) {
      *aDidReconstruct = PR_TRUE;
      return rv;
    }

    // Get the childFrame's parent frame
    nsIFrame* parentFrame = childFrame->GetParent();
    nsIAtom* parentType = parentFrame->GetType();

    if (parentType == nsGkAtoms::frameSetFrame &&
        IsSpecialFramesetChild(aChild)) {
      // Just reframe the parent, since framesets are weird like that.
      *aDidReconstruct = PR_TRUE;
      return RecreateFramesForContent(parentFrame->GetContent());
    }

#ifdef MOZ_MATHML
    // If we're a child of MathML, then we should reframe the MathML content.
    // If we're non-MathML, then we would be wrapped in a block so we need to
    // check our grandparent in that case.
    nsIFrame* possibleMathMLAncestor = parentType == nsGkAtoms::blockFrame ? 
         parentFrame->GetParent() : parentFrame;
    if (possibleMathMLAncestor->IsFrameOfType(nsIFrame::eMathML)) {
      *aDidReconstruct = PR_TRUE;
      return RecreateFramesForContent(possibleMathMLAncestor->GetContent());
    }
#endif

    // Undo XUL wrapping if it's no longer needed.
    // (If we're in the XUL block-wrapping situation, parentFrame is the
    // wrapper frame.)
    nsIFrame* grandparentFrame = parentFrame->GetParent();
    if (grandparentFrame && grandparentFrame->IsBoxFrame() &&
        (grandparentFrame->GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK) &&
        // check if this frame is the only one needing wrapping
        aChild == AnyKidsNeedBlockParent(parentFrame->GetFirstChild(nsnull)) &&
        !AnyKidsNeedBlockParent(childFrame->GetNextSibling())) {
      *aDidReconstruct = PR_TRUE;
      return RecreateFramesForContent(grandparentFrame->GetContent());
    }
    
    // Examine the containing-block for the removed content and see if
    // :first-letter style applies.
    nsIFrame* containingBlock = GetFloatContainingBlock(parentFrame);
    PRBool haveFLS = containingBlock && HasFirstLetterStyle(containingBlock);
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
      RemoveLetterFrames(presContext, mPresShell, frameManager,
                         containingBlock);

      // Recover childFrame and parentFrame
      childFrame = mPresShell->GetPrimaryFrameFor(aChild);
      if (!childFrame || childFrame->GetContent() != aChild) {
        // XXXbz the GetContent() != aChild check is needed due to bug 135040.
        // Remove it once that's fixed.
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

      nsIFrameDebug* fdbg = nsnull;
      CallQueryInterface(parentFrame, &fdbg);
      if (fdbg)
        fdbg->List(stdout, 0);
    }
#endif

    // Walk the frame subtree deleting any out-of-flow frames, and
    // remove the mapping from content objects to frames
    ::DeletingFrameSubtree(frameManager, childFrame);

    // See if the child frame is an out-of-flow
    if (childFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      nsPlaceholderFrame* placeholderFrame =
        frameManager->GetPlaceholderFrameFor(childFrame);
      NS_ASSERTION(placeholderFrame, "No placeholder for out-of-flow?");

      UnregisterPlaceholderChain(frameManager, placeholderFrame);

      // Now we remove the out-of-flow frame
      // XXX has to be done first for now: for floats, the block's line list
      // contains an array of pointers to the placeholder - we have to
      // remove the float first (which gets rid of the lines
      // reference to the placeholder and float) and then remove the
      // placeholder
      rv = frameManager->RemoveFrame(parentFrame,
                                     GetChildListNameFor(childFrame),
                                     childFrame);

      // Remove the placeholder frame first (XXX second for now) (so
      // that it doesn't retain a dangling pointer to memory)
      nsIFrame* placeholderParent = placeholderFrame->GetParent();
      ::DeletingFrameSubtree(frameManager, placeholderFrame);
      rv |= frameManager->RemoveFrame(placeholderParent,
                                      nsnull, placeholderFrame);
    } else {
      // Notify the parent frame that it should delete the frame
      // check for a table caption which goes on an additional child list with a different parent
      nsIFrame* outerTableFrame; 
      if (GetCaptionAdjustedParent(parentFrame, childFrame, &outerTableFrame)) {
        rv = frameManager->RemoveFrame(outerTableFrame,
                                       nsGkAtoms::captionList,
                                       childFrame);
      }
      else {
        rv = frameManager->RemoveFrame(parentFrame, nsnull, childFrame);
      }
    }

    if (mInitialContainingBlock == childFrame) {
      mInitialContainingBlock = nsnull;
      mRootElementStyleFrame = nsnull;
    }

    if (haveFLS && mInitialContainingBlock) {
      NS_ASSERTION(containingBlock == GetFloatContainingBlock(parentFrame),
                   "What happened here?");
      nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                    GetAbsoluteContainingBlock(parentFrame),
                                    containingBlock);
      RecoverLetterFrames(state, containingBlock);
    }

#ifdef DEBUG
    if (gReallyNoisyContentUpdates && parentFrame) {
      nsIFrameDebug* fdbg = nsnull;
      CallQueryInterface(parentFrame, &fdbg);
      if (fdbg) {
        printf("nsCSSFrameConstructor::ContentRemoved: resulting frame model:\n");
        fdbg->List(stdout, 0);
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
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsIViewManager* aViewManager,
                             nsFrameManager* aFrameManager,
                             nsChangeHint aChange);

/**
 * @param aBoundsRect returns the bounds enclosing the areas covered by aFrame and its childre
 * This rect is relative to aFrame's parent
 */
static void
UpdateViewsForTree(nsIFrame* aFrame, nsIViewManager* aViewManager,
                   nsFrameManager* aFrameManager,
                   nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  nsIView* view = aFrame->GetView();
  if (view) {
    if (aChange & nsChangeHint_SyncFrameView) {
      nsContainerFrame::SyncFrameViewProperties(aFrame->PresContext(),
                                                aFrame, nsnull, view);
    }
  }

  // now do children of frame
  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;

  do {
    nsIFrame* child = aFrame->GetFirstChild(childList);
    while (child) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
          || (child->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
        // only do frames that don't have placeholders
        if (nsGkAtoms::placeholderFrame == child->GetType()) { // placeholder
          // get out of flow frame and start over there
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);

          DoApplyRenderingChangeToTree(outOfFlowFrame, aViewManager,
                                       aFrameManager, aChange);
        }
        else {  // regular frame
          UpdateViewsForTree(child, aViewManager, aFrameManager, aChange);
        }
      }
      child = child->GetNextSibling();
    }
    childList = aFrame->GetAdditionalChildListName(listIndex++);
  } while (childList);
}

static void
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsIViewManager* aViewManager,
                             nsFrameManager* aFrameManager,
                             nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  for ( ; aFrame; aFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(aFrame)) {
    // Get view if this frame has one and trigger an update. If the
    // frame doesn't have a view, find the nearest containing view
    // (adjusting r's coordinate system to reflect the nesting) and
    // update there.
    UpdateViewsForTree(aFrame, aViewManager, aFrameManager, aChange);

    // if frame has view, will already be invalidated
    if ((aChange & nsChangeHint_RepaintFrame) &&
        !aFrame->IsFrameOfType(nsIFrame::eSVG)) {
      aFrame->Invalidate(aFrame->GetOverflowRect());
    }
  }
}

static void
ApplyRenderingChangeToTree(nsPresContext* aPresContext,
                           nsIFrame* aFrame,
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

  nsIViewManager* viewManager = aPresContext->GetViewManager();

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.

  // XXX this needs to detect the need for a view due to an opacity change and deal with it...

  nsIViewManager::UpdateViewBatch batch(viewManager);

#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_TRUE;
#endif
  DoApplyRenderingChangeToTree(aFrame, viewManager, shell->FrameManager(),
                               aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_FALSE;
#endif
  
  batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
}

/**
 * This method invalidates the canvas when frames are removed or added for a
 * node that might have its background propagated to the canvas, i.e., a
 * document root node or an HTML BODY which is a child of the root node.
 *
 * @param aFrame a frame for a content node about to be removed or a frme that
 *               was just created for a content node that was inserted.
 */ 
static void
InvalidateCanvasIfNeeded(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Must have frame!");

  //  Note that for both in ContentRemoved and ContentInserted the content node
  //  will still have the right parent pointer, so looking at that is ok.
  
  nsIContent* node = aFrame->GetContent();
  nsIContent* parent = node->GetParent();
  if (parent) {
    // Has a parent; might not be what we want
    nsIContent* grandParent = parent->GetParent();
    if (grandParent) {
      // Has a grandparent, so not what we want
      return;
    }

    // Check whether it's an HTML body
    if (node->Tag() != nsGkAtoms::body ||
        !node->IsNodeOfType(nsINode::eHTML)) {
      return;
    }
  }

  // At this point the node has no parent or it's an HTML <body> child of the
  // root.  We might not need to invalidate in this case (eg we might be in
  // XHTML or something), but chances are we want to.  Play it safe.  Find the
  // frame to invalidate and do it.
  nsIFrame *ancestor = aFrame;
  const nsStyleBackground *bg;
  PRBool isCanvas;
  nsPresContext* presContext = aFrame->PresContext();
  while (!nsCSSRendering::FindBackground(presContext, ancestor,
                                         &bg, &isCanvas)) {
    ancestor = ancestor->GetParent();
    NS_ASSERTION(ancestor, "canvas must paint");
  }

  if (ancestor->GetType() == nsGkAtoms::canvasFrame) {
    // The canvas frame's dimensions are not meaningful; invalidate the
    // viewport instead.
    ancestor = ancestor->GetParent();
  }

  if (ancestor != aFrame) {
    // Wrap this in a DEFERRED view update batch so we don't try to
    // flush out layout here

    nsIViewManager::UpdateViewBatch batch(presContext->GetViewManager());  
    ApplyRenderingChangeToTree(presContext, ancestor,
                               nsChangeHint_RepaintFrame);
    batch.EndUpdateViewBatch(NS_VMREFRESH_DEFERRED);
  }
}

nsresult
nsCSSFrameConstructor::StyleChangeReflow(nsIFrame* aFrame)
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

  // If the frame is part of a split block-in-inline hierarchy, then
  // target the style-change reflow at the first ``normal'' ancestor
  // so we're sure that the style change will propagate to any
  // anonymously created siblings.
  if (IsFrameSpecial(aFrame))
    aFrame = GetIBContainingBlockFor(aFrame);

  do {
    mPresShell->FrameNeedsReflow(aFrame, nsIPresShell::eStyleChange,
                                 NS_FRAME_IS_DIRTY);
    aFrame = aFrame->GetNextContinuation();
  } while (aFrame);

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::CharacterDataChanged(nsIContent* aContent,
                                            PRBool aAppend)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  nsresult      rv = NS_OK;

  // Find the child frame
  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(aContent);

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

    // Special check for text content that is a child of a letter frame.  If
    // this happens, we should remove the letter frame, do whatever we're
    // planning to do with this notification, then put the letter frame back.
    // Note that this is basically what ReinsertContent ends up doing; the
    // reason we dont' want to call that here is that our text content could be
    // native anonymous, in which case ReinsertContent would completely barf on
    // it.  And reinserting the non-anonymous ancestor would just lead us to
    // come back into this notification (e.g. if quotes or counters are
    // involved), leading to a loop.
    nsIFrame* block = GetFloatContainingBlock(frame);
    PRBool haveFirstLetterStyle = PR_FALSE;
    if (block) {
      // See if the block has first-letter style applied to it.
      haveFirstLetterStyle = HasFirstLetterStyle(block);
      if (haveFirstLetterStyle) {
        RemoveLetterFrames(mPresShell->GetPresContext(), mPresShell,
                           mPresShell->FrameManager(), block);
        // Reget |frame|, since we might have killed it.
        // Do we really need to call CharacterDataChanged in this case, though?
        frame = mPresShell->GetPrimaryFrameFor(aContent);
        NS_ASSERTION(frame, "Should have frame here!");
      }
    }

    frame->CharacterDataChanged(mPresShell->GetPresContext(), aContent,
                                aAppend);

    if (haveFirstLetterStyle) {
      nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                    GetAbsoluteContainingBlock(frame),
                                    block, nsnull);
      RecoverLetterFrames(state, block);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  PRInt32 count = aChangeList.Count();
  if (!count)
    return NS_OK;

  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  BeginUpdate();

  nsPropertyTable *propTable = mPresShell->GetPresContext()->PropertyTable();

  // Mark frames so that we skip frames that die along the way, bug 123049.
  // A frame can be in the list multiple times with different hints. Further
  // optmization is possible if nsStyleChangeList::AppendChange could coalesce
  PRInt32 index = count;

  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->SetProperty(changeData->mFrame,
                             nsGkAtoms::changeListProperty,
                             nsnull, nsnull, nsnull);
    }
  }

  index = count;
  while (0 <= --index) {
    nsIFrame* frame;
    nsIContent* content;
    nsChangeHint hint;
    aChangeList.ChangeAt(index, frame, content, hint);
    if (frame && frame->GetContent() != content) {
      // XXXbz this is due to image maps messing with the primary frame map.
      // See bug 135040.  Remove this block once that's fixed.
      frame = nsnull;
      if (!(hint & nsChangeHint_ReconstructFrame)) {
        continue;
      }
    }

    // skip any frame that has been destroyed due to a ripple effect
    if (frame) {
      nsresult res;

      propTable->GetProperty(frame, nsGkAtoms::changeListProperty, &res);

      if (NS_PROPTABLE_PROP_NOT_THERE == res)
        continue;
    }

    if (hint & nsChangeHint_ReconstructFrame) {
      RecreateFramesForContent(content);
    } else {
      NS_ASSERTION(frame, "This shouldn't happen");
#ifdef MOZ_SVG
      if (hint & nsChangeHint_UpdateEffects) {
        nsSVGEffects::UpdateEffects(frame);
      }
#endif
      if (hint & nsChangeHint_ReflowFrame) {
        StyleChangeReflow(frame);
      }
      if (hint & (nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView)) {
        ApplyRenderingChangeToTree(mPresShell->GetPresContext(), frame, hint);
      }
      if (hint & nsChangeHint_UpdateCursor) {
        nsIViewManager* viewMgr = mPresShell->GetViewManager();
        if (viewMgr)
          viewMgr->SynthesizeMouseMove(PR_FALSE);
      }
    }
  }

  EndUpdate();
  
  // cleanup references and verify the style tree.  Note that the latter needs
  // to happen once we've processed the whole list, since until then the tree
  // is not in fact in a consistent state.
  index = count;
  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->DeleteProperty(changeData->mFrame,
                                nsGkAtoms::changeListProperty);
    }

#ifdef DEBUG
    // reget frame from content since it may have been regenerated...
    if (changeData->mContent) {
      nsIFrame* frame = mPresShell->GetPrimaryFrameFor(changeData->mContent);
      if (frame) {
        mPresShell->FrameManager()->DebugVerifyStyleTree(frame);
      }
    } else {
      NS_WARNING("Unable to test style tree integrity -- no content node");
    }
#endif
  }

  aChangeList.Clear();
  return NS_OK;
}

void
nsCSSFrameConstructor::RestyleElement(nsIContent     *aContent,
                                      nsIFrame       *aPrimaryFrame,
                                      nsChangeHint   aMinHint)
{
  NS_ASSERTION(aPrimaryFrame == mPresShell->GetPrimaryFrameFor(aContent),
               "frame/content mismatch");
  if (aPrimaryFrame && aPrimaryFrame->GetContent() != aContent) {
    // XXXbz this is due to image maps messing with the primary frame mapping.
    // See bug 135040.  We can remove this block once that's fixed.
    aPrimaryFrame = nsnull;
  }
  NS_ASSERTION(!aPrimaryFrame || aPrimaryFrame->GetContent() == aContent,
               "frame/content mismatch");

  if (aMinHint & nsChangeHint_ReconstructFrame) {
    RecreateFramesForContent(aContent);
  } else if (aPrimaryFrame) {
    nsStyleChangeList changeList;
    mPresShell->FrameManager()->
      ComputeStyleChangeFor(aPrimaryFrame, &changeList, aMinHint);
    ProcessRestyledFrames(changeList);
  } else {
    // no frames, reconstruct for content
    MaybeRecreateFramesForContent(aContent);
  }
}

void
nsCSSFrameConstructor::RestyleLaterSiblings(nsIContent *aContent)
{
  nsIContent *parent = aContent->GetParent();
  if (!parent)
    return; // root element has no later siblings

  for (PRInt32 index = parent->IndexOf(aContent) + 1,
               index_end = parent->GetChildCount();
       index != index_end; ++index) {
    nsIContent *child = parent->GetChildAt(index);
    if (!child->IsNodeOfType(nsINode::eELEMENT))
      continue;

    nsIFrame* primaryFrame = mPresShell->GetPrimaryFrameFor(child);
    RestyleElement(child, primaryFrame, NS_STYLE_HINT_NONE);
  }
}

nsresult
nsCSSFrameConstructor::ContentStatesChanged(nsIContent* aContent1,
                                            nsIContent* aContent2,
                                            PRInt32 aStateMask) 
{
  DoContentStateChanged(aContent1, aStateMask);
  DoContentStateChanged(aContent2, aStateMask);
  return NS_OK;
}

void
nsCSSFrameConstructor::DoContentStateChanged(nsIContent* aContent,
                                             PRInt32 aStateMask) 
{
  nsStyleSet *styleSet = mPresShell->StyleSet();
  nsPresContext *presContext = mPresShell->GetPresContext();
  NS_ASSERTION(styleSet, "couldn't get style set");

  if (aContent) {
    nsChangeHint hint = NS_STYLE_HINT_NONE;
    // Any change to a content state that affects which frames we construct
    // must lead to a frame reconstruct here if we already have a frame.
    // Note that we never decide through non-CSS means to not create frames
    // based on content states, so if we already don't have a frame we don't
    // need to force a reframe -- if it's needed, the HasStateDependentStyle
    // call will handle things.
    nsIFrame* primaryFrame = mPresShell->GetPrimaryFrameFor(aContent);
    if (primaryFrame) {
      // If it's generated content, ignore LOADING/etc state changes on it.
      if (!primaryFrame->IsGeneratedContentFrame() &&
          (aStateMask & (NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED |
                         NS_EVENT_STATE_SUPPRESSED | NS_EVENT_STATE_LOADING))) {
        hint = nsChangeHint_ReconstructFrame;
      } else {          
        PRUint8 app = primaryFrame->GetStyleDisplay()->mAppearance;
        if (app) {
          nsITheme *theme = presContext->GetTheme();
          if (theme && theme->ThemeSupportsWidget(presContext,
                                                  primaryFrame, app)) {
            PRBool repaint = PR_FALSE;
            theme->WidgetStateChanged(primaryFrame, app, nsnull, &repaint);
            if (repaint) {
              NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
            }
          }
        }
      }
    }

    nsReStyleHint rshint = 
      styleSet->HasStateDependentStyle(presContext, aContent, aStateMask);
      
    if ((aStateMask & NS_EVENT_STATE_HOVER) && rshint != 0) {
      ++mHoverGeneration;
    }

    PostRestyleEvent(aContent, rshint, hint);
  }
}

nsresult
nsCSSFrameConstructor::AttributeChanged(nsIContent* aContent,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aModType,
                                        PRUint32 aStateMask)
{
  nsresult  result = NS_OK;

  // Hold onto the PresShell to prevent ourselves from being destroyed.
  // XXXbz how, exactly, would this attribute change cause us to be
  // destroyed from inside this function?
  nsCOMPtr<nsIPresShell> shell = mPresShell;

  // Get the frame associated with the content which is the highest in the frame tree
  nsIFrame* primaryFrame = shell->GetPrimaryFrameFor(aContent); 

#if 0
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("HTMLStyleSheet::AttributeChanged: content=%p[%s] frame=%p",
      aContent, ContentTag(aContent, 0), frame));
#endif

  // the style tag has its own interpretation based on aHint 
  nsChangeHint hint = aContent->GetAttributeChangeHint(aAttribute, aModType);

  PRBool reframe = (hint & nsChangeHint_ReconstructFrame) != 0;

#ifdef MOZ_XUL
  // The following listbox widget trap prevents offscreen listbox widget
  // content from being removed and re-inserted (which is what would
  // happen otherwise).
  if (!primaryFrame && !reframe) {
    PRInt32 namespaceID;
    nsIAtom* tag =
      mDocument->BindingManager()->ResolveTag(aContent, &namespaceID);

    if (namespaceID == kNameSpaceID_XUL &&
        (tag == nsGkAtoms::listitem ||
         tag == nsGkAtoms::listcell))
      return NS_OK;
  }

  if (aAttribute == nsGkAtoms::tooltiptext ||
      aAttribute == nsGkAtoms::tooltip) 
  {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
    if (rootBox) {
      if (aModType == nsIDOMMutationEvent::REMOVAL)
        rootBox->RemoveTooltipSupport(aContent);
      if (aModType == nsIDOMMutationEvent::ADDITION)
        rootBox->AddTooltipSupport(aContent);
    }
  }

#endif // MOZ_XUL

  if (primaryFrame) {
    // See if we have appearance information for a theme.
    const nsStyleDisplay* disp = primaryFrame->GetStyleDisplay();
    if (disp->mAppearance) {
      nsPresContext* presContext = mPresShell->GetPresContext();
      nsITheme *theme = presContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(presContext, primaryFrame, disp->mAppearance)) {
        PRBool repaint = PR_FALSE;
        theme->WidgetStateChanged(primaryFrame, disp->mAppearance, aAttribute, &repaint);
        if (repaint)
          NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
      }
    }
   
    // let the frame deal with it now, so we don't have to deal later
    result = primaryFrame->AttributeChanged(aNameSpaceID, aAttribute,
                                            aModType);
    // XXXwaterson should probably check for special IB siblings
    // here, and propagate the AttributeChanged notification to
    // them, as well. Currently, inline frames don't do anything on
    // this notification, so it's not that big a deal.
  }

  // See if we can optimize away the style re-resolution -- must be called after
  // the frame's AttributeChanged() in case it does something that affects the style
  nsFrameManager *frameManager = shell->FrameManager();
  nsReStyleHint rshint = frameManager->HasAttributeDependentStyle(aContent,
                                                                  aAttribute,
                                                                  aModType,
                                                                  aStateMask);

  PostRestyleEvent(aContent, rshint, hint);

  return result;
}

void
nsCSSFrameConstructor::BeginUpdate() {
  NS_SuppressFocusEvent();
  ++mFocusSuppressCount;
  ++mUpdateCount;
}

void
nsCSSFrameConstructor::EndUpdate()
{
  if (mUpdateCount == 1) {
    // This is the end of our last update.  Before we decrement
    // mUpdateCount, recalc quotes and counters as needed.

    RecalcQuotesAndCounters();
    NS_ASSERTION(mUpdateCount == 1, "Odd update count");
  }
  --mUpdateCount;
  if (mFocusSuppressCount) {
    NS_UnsuppressFocusEvent();
    --mFocusSuppressCount;
  }
}

void
nsCSSFrameConstructor::RecalcQuotesAndCounters()
{
  if (mQuotesDirty) {
    mQuotesDirty = PR_FALSE;
    mQuoteList.RecalcAll();
  }

  if (mCountersDirty) {
    mCountersDirty = PR_FALSE;
    mCounterManager.RecalcAll();
  }

  NS_ASSERTION(!mQuotesDirty, "Quotes updates will be lost");
  NS_ASSERTION(!mCountersDirty, "Counter updates will be lost");  
}

class nsFocusUnsuppressEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    nsFocusUnsuppressEvent(PRUint32 aCount) : mCount(aCount) {}
  private:
    PRUint32 mCount;
  };

NS_IMETHODIMP nsFocusUnsuppressEvent::Run()
{
  while (mCount) {
    --mCount;
    NS_UnsuppressFocusEvent();
  }
  return NS_OK;
}

void
nsCSSFrameConstructor::WillDestroyFrameTree(PRBool aDestroyingPresShell)
{
#if defined(DEBUG_dbaron_off)
  mCounterManager.Dump();
#endif

  mIsDestroyingFrameTree = PR_TRUE;

  // Prevent frame tree destruction from being O(N^2)
  mQuoteList.Clear();
  mCounterManager.Clear();

  // Cancel all pending re-resolves
  mRestyleEvent.Revoke();

  if (mFocusSuppressCount && aDestroyingPresShell) {
    nsRefPtr<nsFocusUnsuppressEvent> ev =
      new nsFocusUnsuppressEvent(mFocusSuppressCount);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mFocusSuppressCount = 0;
    }
  }
}

//STATIC

// XXXbz I'd really like this method to go away. Once we have inline-block and
// I can just use that for sized broken images, that can happen, maybe.
void nsCSSFrameConstructor::GetAlternateTextFor(nsIContent*    aContent,
                                                nsIAtom*       aTag,  // content object's tag
                                                nsXPIDLString& aAltText)
{
  // The "alt" attribute specifies alternate text that is rendered
  // when the image can not be displayed

  // If there's no "alt" attribute, and aContent is an input    
  // element, then use the value of the "value" attribute
  if (!aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aAltText) &&
      nsGkAtoms::input == aTag) {
    // If there's no "value" attribute either, then use the localized string 
    // for "Submit" as the alternate text.
    if (!aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aAltText)) {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "Submit", aAltText);      
    }
  }
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
  nsIFrame* newFrame = NS_NewTableOuterFrame(aPresShell, aStyleContext);

  if (newFrame) {
    newFrame->Init(aContent, aParentFrame, aFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

    // Create a continuing inner table frame, and if there's a caption then
    // replicate the caption
    nsFrameItems  newChildFrames;

    nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);
    if (childFrame) {
      nsIFrame* continuingTableFrame;
      nsresult rv = CreateContinuingFrame(aPresContext, childFrame, newFrame,
                                          &continuingTableFrame);
      if (NS_FAILED(rv)) {
        newFrame->Destroy();
        *aContinuingFrame = nsnull;
        return rv;
      }
      newChildFrames.AddChild(continuingTableFrame);
      
      NS_ASSERTION(!childFrame->GetNextSibling(),"there can be only one inner table frame");
    }

    // Set the outer table's initial child list
    newFrame->SetInitialChildList(nsnull, newChildFrames.childList);
    
    *aContinuingFrame = newFrame;
    return NS_OK;
  }
  else {
    *aContinuingFrame = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
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
  nsIFrame* newFrame = NS_NewTableFrame(aPresShell, aStyleContext);

  if (newFrame) {
    newFrame->Init(aContent, aParentFrame, aFrame);
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

    // Replicate any header/footer frames
    nsFrameItems  childFrames;
    nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);
    for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
      // See if it's a header/footer, possibly wrapped in a scroll frame.
      nsTableRowGroupFrame* rowGroupFrame =
        nsTableFrame::GetRowGroupFrame(childFrame);
      if (rowGroupFrame) {
        // If the row group was continued, then don't replicate it.
        nsIFrame* rgNextInFlow = rowGroupFrame->GetNextInFlow();
        if (rgNextInFlow) {
          rowGroupFrame->SetRepeatable(PR_FALSE);
        }
        else if (rowGroupFrame->IsRepeatable()) {        
          // Replicate the header/footer frame.
          nsTableRowGroupFrame*   headerFooterFrame;
          nsFrameItems            childItems;
          nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                        GetAbsoluteContainingBlock(newFrame),
                                        nsnull);

          headerFooterFrame = static_cast<nsTableRowGroupFrame*>
                                         (NS_NewTableRowGroupFrame(aPresShell, rowGroupFrame->GetStyleContext()));
          nsIContent* headerFooter = rowGroupFrame->GetContent();
          headerFooterFrame->Init(headerFooter, newFrame, nsnull);
          ProcessChildren(state, headerFooter, headerFooterFrame,
                          PR_TRUE, childItems, PR_FALSE);
          NS_ASSERTION(!state.mFloatedItems.childList, "unexpected floated element");
          headerFooterFrame->SetInitialChildList(nsnull, childItems.childList);
          headerFooterFrame->SetRepeatable(PR_TRUE);

          // Table specific initialization
          headerFooterFrame->InitRepeatedFrame(aPresContext, rowGroupFrame);

          // XXX Deal with absolute and fixed frames...
          childFrames.AddChild(headerFooterFrame);
        }
      }
    }
    
    // Set the table frame's initial child list
    newFrame->SetInitialChildList(nsnull, childFrames.childList);
    
    *aContinuingFrame = newFrame;
    return NS_OK;
  }
  else {
    *aContinuingFrame = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
}

nsresult
nsCSSFrameConstructor::CreateContinuingFrame(nsPresContext* aPresContext,
                                             nsIFrame*       aFrame,
                                             nsIFrame*       aParentFrame,
                                             nsIFrame**      aContinuingFrame,
                                             PRBool          aIsFluid)
{
  nsIPresShell*              shell = aPresContext->PresShell();
  nsStyleContext*            styleContext = aFrame->GetStyleContext();
  nsIFrame*                  newFrame = nsnull;
  nsresult                   rv = NS_OK;
  nsIFrame*                  nextContinuation = aFrame->GetNextContinuation();
  nsIFrame*                  nextInFlow = aFrame->GetNextInFlow();

  // Use the frame type to determine what type of frame to create
  nsIAtom* frameType = aFrame->GetType();
  nsIContent* content = aFrame->GetContent();

  NS_ASSERTION(aFrame->GetSplittableType() != NS_FRAME_NOT_SPLITTABLE,
               "why CreateContinuingFrame for a non-splittable frame?");
  
  if (nsGkAtoms::textFrame == frameType) {
    newFrame = NS_NewContinuingTextFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
    
  } else if (nsGkAtoms::inlineFrame == frameType) {
    newFrame = NS_NewInlineFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsGkAtoms::blockFrame == frameType) {
    newFrame = NS_NewBlockFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
#ifdef MOZ_XUL
  } else if (nsGkAtoms::XULLabelFrame == frameType) {
    newFrame = NS_NewXULLabelFrame(shell, styleContext, 0);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
#endif  
  } else if (nsGkAtoms::columnSetFrame == frameType) {
    newFrame = NS_NewColumnSetFrame(shell, styleContext, 0);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsGkAtoms::positionedInlineFrame == frameType) {
    newFrame = NS_NewPositionedInlineFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsGkAtoms::pageFrame == frameType) {
    nsIFrame* canvasFrame;
    rv = ConstructPageFrame(shell, aPresContext, aParentFrame, aFrame,
                            newFrame, canvasFrame);
  } else if (nsGkAtoms::tableOuterFrame == frameType) {
    rv = CreateContinuingOuterTableFrame(shell, aPresContext, aFrame, aParentFrame,
                                         content, styleContext, &newFrame);

  } else if (nsGkAtoms::tableFrame == frameType) {
    rv = CreateContinuingTableFrame(shell, aPresContext, aFrame, aParentFrame,
                                    content, styleContext, &newFrame);

  } else if (nsGkAtoms::tableRowGroupFrame == frameType) {
    newFrame = NS_NewTableRowGroupFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsGkAtoms::tableRowFrame == frameType) {
    newFrame = NS_NewTableRowFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing frame for each table cell frame
      nsFrameItems  newChildList;
      nsIFrame* cellFrame = aFrame->GetFirstChild(nsnull);
      while (cellFrame) {
        // See if it's a table cell frame
        if (IS_TABLE_CELL(cellFrame->GetType())) {
          nsIFrame* continuingCellFrame;
          rv = CreateContinuingFrame(aPresContext, cellFrame, newFrame,
                                     &continuingCellFrame);
          if (NS_FAILED(rv)) {
            nsFrameList tmp(newChildList.childList);
            tmp.DestroyFrames();
            newFrame->Destroy();
            *aContinuingFrame = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
          }
          newChildList.AddChild(continuingCellFrame);
        }
        cellFrame = cellFrame->GetNextSibling();
      }
      
      // Set the table cell's initial child list
      newFrame->SetInitialChildList(nsnull, newChildList.childList);
    }

  } else if (IS_TABLE_CELL(frameType)) {
    // Warning: If you change this and add a wrapper frame around table cell
    // frames, make sure Bug 368554 doesn't regress!
    // See IsInAutoWidthTableCellForQuirk() in nsImageFrame.cpp.
    newFrame = NS_NewTableCellFrame(shell, styleContext, IsBorderCollapse(aParentFrame));

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      nsIFrame* continuingAreaFrame;
      nsIFrame* blockFrame = aFrame->GetFirstChild(nsnull);
      rv = CreateContinuingFrame(aPresContext, blockFrame, newFrame,
                                 &continuingAreaFrame);
      if (NS_FAILED(rv)) {
        newFrame->Destroy();
        *aContinuingFrame = nsnull;
        return rv;
      }

      // Set the table cell's initial child list
      newFrame->SetInitialChildList(nsnull, continuingAreaFrame);
    }
  
  } else if (nsGkAtoms::lineFrame == frameType) {
    newFrame = NS_NewFirstLineFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
  } else if (nsGkAtoms::letterFrame == frameType) {
    newFrame = NS_NewFirstLetterFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }

  } else if (nsGkAtoms::imageFrame == frameType) {
    newFrame = NS_NewImageFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
    }
  } else if (nsGkAtoms::imageControlFrame == frameType) {
    newFrame = NS_NewImageControlFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
    }    
  } else if (nsGkAtoms::placeholderFrame == frameType) {
    // create a continuing out of flow frame
    nsIFrame* oofFrame = nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
    nsIFrame* oofContFrame;
    rv = CreateContinuingFrame(aPresContext, oofFrame, aParentFrame, &oofContFrame);
    if (NS_FAILED(rv)) {
      *aContinuingFrame = nsnull;
      return rv;
    }
    // create a continuing placeholder frame
    rv = CreatePlaceholderFrameFor(shell, content, oofContFrame, styleContext,
                                   aParentFrame, aFrame, &newFrame);
    if (NS_FAILED(rv)) {
      oofContFrame->Destroy();
      *aContinuingFrame = nsnull;
      return rv;
    }
  } else if (nsGkAtoms::fieldSetFrame == frameType) {
    newFrame = NS_NewFieldSetFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);

      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      // XXXbz we really shouldn't have to do this by hand!
      nsIFrame* continuingAreaFrame;
      nsIFrame* blockFrame = GetFieldSetBlockFrame(aFrame);
      rv = CreateContinuingFrame(aPresContext, blockFrame, newFrame,
                                 &continuingAreaFrame);
      if (NS_FAILED(rv)) {
        newFrame->Destroy();
        *aContinuingFrame = nsnull;
        return rv;
      }
      // Set the fieldset's initial child list
      newFrame->SetInitialChildList(nsnull, continuingAreaFrame);
    }
  } else {
    NS_NOTREACHED("unexpected frame type");
    *aContinuingFrame = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  *aContinuingFrame = newFrame;

  if (!newFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Init() set newFrame to be a fluid continuation of aFrame.
  // If we want a non-fluid continuation, we need to call SetPrevContinuation()
  // to reset NS_FRAME_IS_FLUID_CONTINUATION.
  if (!aIsFluid) {
    newFrame->SetPrevContinuation(aFrame);
  }

  // A continuation of generated content is also generated content
  if (aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT) {
    newFrame->AddStateBits(NS_FRAME_GENERATED_CONTENT);
  }

  // A continuation of an out-of-flow is also an out-of-flow
  if (aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    newFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);
  }

  if (nextInFlow) {
    nextInFlow->SetPrevInFlow(newFrame);
    newFrame->SetNextInFlow(nextInFlow);
  } else if (nextContinuation) {
    nextContinuation->SetPrevContinuation(newFrame);
    newFrame->SetNextContinuation(nextContinuation);
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ReplicateFixedFrames(nsPageContentFrame* aParentFrame)
{
  // Now deal with fixed-pos things....  They should appear on all pages,
  // so we want to move over the placeholders when processing the child
  // of the pageContentFrame.

  nsIFrame* prevPageContentFrame = aParentFrame->GetPrevInFlow();
  if (!prevPageContentFrame) {
    return NS_OK;
  }
  nsIFrame* canvasFrame = aParentFrame->GetFirstChild(nsnull);
  nsIFrame* prevCanvasFrame = prevPageContentFrame->GetFirstChild(nsnull);
  if (!canvasFrame || !prevCanvasFrame) {
    // document's root element frame missing
    return NS_ERROR_UNEXPECTED;
  }

  nsFrameItems fixedPlaceholders;
  nsIFrame* firstFixed = prevPageContentFrame->GetFirstChild(nsGkAtoms::fixedList);
  if (!firstFixed) {
    return NS_OK;
  }

  //XXXbz Should mInitialContainingBlock be docRootFrame? It probably doesn't matter.
  // Don't allow abs-pos descendants of the fixed content to escape the content.
  // This should not normally be possible (because fixed-pos elements should
  // be absolute containers) but fixed-pos tables currently aren't abs-pos
  // containers.
  nsFrameConstructorState state(mPresShell, aParentFrame,
                                nsnull,
                                mInitialContainingBlock);

  // Iterate across fixed frames and replicate each whose placeholder is a
  // descendant of aFrame. (We don't want to explicitly copy placeholders that
  // are within fixed frames, because that would cause duplicates on the new
  // page - bug 389619)
  for (nsIFrame* fixed = firstFixed; fixed; fixed = fixed->GetNextSibling()) {
    nsIFrame* prevPlaceholder = nsnull;
    mPresShell->GetPlaceholderFrameFor(fixed, &prevPlaceholder);
    if (prevPlaceholder &&
        nsLayoutUtils::IsProperAncestorFrame(prevCanvasFrame, prevPlaceholder)) {
      nsresult rv = ConstructFrame(state, fixed->GetContent(),
                                   canvasFrame, fixedPlaceholders);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Add the placeholders to our primary child list.
  // XXXbz this is a little screwed up, since the fixed frames will have 
  // broken auto-positioning. Oh, well.
  NS_ASSERTION(!canvasFrame->GetFirstChild(nsnull),
               "leaking frames; doc root continuation must be empty");
  canvasFrame->SetInitialChildList(nsnull, fixedPlaceholders.childList);
  return NS_OK;
}

static PRBool
IsBindingAncestor(nsIContent* aContent, nsIContent* aBindingRoot)
{
  while (PR_TRUE) {
    // Native-anonymous content doesn't contain insertion points, so
    // we don't need to search through it.
    if (aContent->IsRootOfNativeAnonymousSubtree())
      return PR_FALSE;
    nsIContent* bindingParent = aContent->GetBindingParent();
    if (!bindingParent)
      return PR_FALSE;
    if (bindingParent == aBindingRoot)
      return PR_TRUE;
    aContent = bindingParent;
  }
}

// Helper function that searches the immediate child frames 
// (and their children if the frames are "special")
// for a frame that maps the specified content object
nsIFrame*
nsCSSFrameConstructor::FindFrameWithContent(nsFrameManager*  aFrameManager,
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
    PRBool searchAgain;

    do {
#ifdef NOISY_FINDFRAME
      FFWC_doLoop++;
#endif
      nsIFrame* kidFrame = nsnull;

      searchAgain = PR_FALSE;

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
          if (kidFrame->GetNextSibling()) {
            kidFrame = kidFrame->GetNextSibling();
          }
          else {
            // The hint frame had no next sibling. Try the next-in-flow or
            // special sibling of the parent of the hint frame (or its
            // associated placeholder).
            nsIFrame *parentFrame = kidFrame->GetParent();
            kidFrame = nsnull;
            if (parentFrame) {
              parentFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(parentFrame);
            }
            if (parentFrame) {
              // Found it, continue the search with its first child.
              kidFrame = parentFrame->GetFirstChild(listName);
              // Leave |aParentFrame| as-is, since the only time we'll
              // reuse it is if the hint fails.
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

          // We found a match.  Return the out-of-flow if it's a placeholder
          return nsPlaceholderFrame::GetRealFrameFor(kidFrame);
        }

        // only do this if there is content
        if (kidContent) {
          // We search the immediate children only, but if the child frame has
          // the same content pointer as its parent then we need to search its
          // child frames, too.
          // We also need to search if the child content is anonymous and scoped
          // to the parent content.
          // XXXldb What makes us continue the search once we're inside
          // the anonymous subtree?
          if (aParentContent == kidContent ||
              (aParentContent && IsBindingAncestor(kidContent, aParentContent))) 
          {
#ifdef NOISY_FINDFRAME
            FFWC_recursions++;
            printf("  recursing with new parent set to kidframe=%p, parentContent=%p\n", 
                   kidFrame, aParentContent);
#endif
            nsIFrame* matchingFrame =
                FindFrameWithContent(aFrameManager,
                                     nsPlaceholderFrame::GetRealFrameFor(kidFrame),
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
        // If we get here, and we had a hint, then we didn't find a frame.
        // The hint may have been a frame whose location in the frame tree
        // doesn't match the location of its corresponding element in the
        // DOM tree, e.g. a floated or absolutely positioned frame, or e.g.
        // a <col> frame, in which case we'd be off in the weeds looking
        // through something other than the primary frame list.
        // Reboot the search from scratch, without the hint, but using the
        // null child list again.
        aHint = nsnull;
        searchAgain = PR_TRUE;
      } else {
        do {
          listName = aParentFrame->GetAdditionalChildListName(listIndex++);
        } while (IsOutOfFlowList(listName));
      }
    } while(listName || searchAgain);

    // We didn't find a matching frame. If aFrame has a next-in-flow,
    // then continue looking there
    aParentFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(aParentFrame);
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
nsCSSFrameConstructor::FindPrimaryFrameFor(nsFrameManager*  aFrameManager,
                                           nsIContent*      aContent,
                                           nsIFrame**       aFrame,
                                           nsFindFrameHint* aHint)
{
  NS_ASSERTION(aFrameManager && aContent && aFrame, "bad arg");

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
    parentFrame = aFrameManager->GetPrimaryFrameFor(parentContent, -1);
    while (parentFrame) {
      // Search the child frames for a match
      *aFrame = FindFrameWithContent(aFrameManager, parentFrame,
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
            FindFrameWithContent(aFrameManager, parentFrame,
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
        parentFrame = GetSpecialSibling(parentFrame);
      }
      else {
        break;
      }
    }
  }

  if (aHint && !*aFrame)
  { // if we had a hint, and we didn't get a frame, see if we should try the slow way
    if (aContent->IsNodeOfType(nsINode::eTEXT)) 
    {
#ifdef NOISY_FINDFRAME
      FFWC_slowSearchForText++;
#endif
      // since we're passing in a null hint, we're guaranteed to only recurse once
      return FindPrimaryFrameFor(aFrameManager, aContent, aFrame, nsnull);
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
nsCSSFrameConstructor::GetInsertionPoint(nsIFrame*     aParentFrame,
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

  nsBindingManager *bindingManager = mDocument->BindingManager();

  nsIContent* insertionElement;
  if (aChildContent) {
    // We've got an explicit insertion child. Check to see if it's
    // anonymous.
    if (aChildContent->GetBindingParent() == container) {
      // This child content is anonymous. Don't use the insertion
      // point, since that's only for the explicit kids.
      return NS_OK;
    }

    PRUint32 index;
    insertionElement = bindingManager->GetInsertionPoint(container,
                                                         aChildContent,
                                                         &index);
  }
  else {
    PRBool multiple;
    PRUint32 index;
    insertionElement = bindingManager->GetSingleInsertionPoint(container,
                                                               &index,
                                                               &multiple);
    if (multiple && aMultiple)
      *aMultiple = multiple; // Record the fact that filters are in use.
  }

  if (insertionElement) {
    nsIFrame* insertionPoint = mPresShell->GetPrimaryFrameFor(insertionElement);
    if (insertionPoint) {
      // Use the content insertion frame of the insertion point.
      insertionPoint = insertionPoint->GetContentInsertionFrame();
      if (insertionPoint && insertionPoint != aParentFrame) 
        GetInsertionPoint(insertionPoint, aChildContent, aInsertionPoint, aMultiple);
    }
    else {
      // There was no frame created yet for the insertion point.
      *aInsertionPoint = nsnull;
    }
  }

  // fieldsets have multiple insertion points.  Note that we might
  // have to look at insertionElement here...
  if (aMultiple && !*aMultiple) {
    nsIContent* content = insertionElement ? insertionElement : container;
    if (content->IsNodeOfType(nsINode::eHTML) &&
        content->Tag() == nsGkAtoms::fieldset) {
      *aMultiple = PR_TRUE;
    }
  }

  return NS_OK;
}

// Capture state for the frame tree rooted at the frame associated with the
// content object, aContent
nsresult
nsCSSFrameConstructor::CaptureStateForFramesOf(nsIContent* aContent,
                                               nsILayoutHistoryState* aHistoryState)
{
  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(aContent);
  if (frame) {
    CaptureStateFor(frame, aHistoryState);
  }
  return NS_OK;
}

// Capture state for the frame tree rooted at aFrame.
nsresult
nsCSSFrameConstructor::CaptureStateFor(nsIFrame* aFrame,
                                       nsILayoutHistoryState* aHistoryState)
{
  if (aFrame && aHistoryState) {
    mPresShell->FrameManager()->CaptureFrameState(aFrame, aHistoryState);
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::MaybeRecreateFramesForContent(nsIContent* aContent)
{
  nsresult result = NS_OK;
  nsFrameManager *frameManager = mPresShell->FrameManager();

  nsStyleContext *oldContext = frameManager->GetUndisplayedContent(aContent);
  if (oldContext) {
    // The parent has a frame, so try resolving a new context.
    nsRefPtr<nsStyleContext> newContext = mPresShell->StyleSet()->
      ResolveStyleFor(aContent, oldContext->GetParent());

    frameManager->ChangeUndisplayedContent(aContent, newContext);
    if (newContext->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_NONE) {
      result = RecreateFramesForContent(aContent);
    }
  }
  return result;
}

PRBool
nsCSSFrameConstructor::MaybeRecreateContainerForIBSplitterFrame(nsIFrame* aFrame,
                                                                nsresult* aResult)
{
  NS_PRECONDITION(aFrame, "Must have a frame");
  NS_PRECONDITION(aFrame->GetParent(), "Frame shouldn't be root");
  NS_PRECONDITION(aResult, "Null out param?");
  NS_PRECONDITION(aFrame == aFrame->GetFirstContinuation(),
                  "aFrame not the result of GetPrimaryFrameFor()?");

  if (IsFrameSpecial(aFrame)) {
    // The removal functions can't handle removal of an {ib} split directly; we
    // need to rebuild the containing block.
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::MaybeRecreateContainerForIBSplitterFrame: "
             "frame=");
      nsFrame::ListTag(stdout, aFrame);
      printf(" is special\n");
    }
#endif

    *aResult = ReframeContainingBlock(aFrame);
    return PR_TRUE;
  }

  // We might still need to reconstruct things if the parent of aFrame is
  // special, since in that case the removal of aFrame might affect the
  // splitting of its parent.
  nsIFrame* parent = aFrame->GetParent();
  if (!IsFrameSpecial(parent)) {
    return PR_FALSE;
  }

  // If aFrame is an inline, then it cannot possibly have caused the splitting.
  // If the frame is being reconstructed and being changed to a block, the
  // ContentInserted call will handle the containing block reframe.  So in this
  // case, try to be conservative about whether we need to reframe.  The only
  // case when it's needed is if the inline is the only child of the tail end
  // of an {ib} split, because the splitting code doesn't produce this tail end
  // if it would have no kids.  If that ever changes, this code should change.
  if (IsInlineOutside(aFrame) &&
      (
       // Not a kid of the third part of the IB split
       GetSpecialSibling(parent) || !IsInlineOutside(parent) ||
       // Or not the only child
       aFrame->GetTailContinuation()->GetNextSibling() ||
       aFrame != parent->GetFirstContinuation()->GetFirstChild(nsnull)
      )) {
    return PR_FALSE;
  }

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::MaybeRecreateContainerForIBSplitterFrame: "
           "frame=");
    nsFrame::ListTag(stdout, parent);
    printf(" is special\n");
  }
#endif

  *aResult = ReframeContainingBlock(parent);
  return PR_TRUE;
}
 
nsresult
nsCSSFrameConstructor::RecreateFramesForContent(nsIContent* aContent)
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
  // below!).  We'd really like to optimize away one of those
  // containing block reframes, hence the code here.

  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(aContent);
  if (frame && frame->IsFrameOfType(nsIFrame::eMathML)) {
    // Reframe the topmost MathML element to prevent exponential blowup
    // (see bug 397518)
    while (PR_TRUE) {
      nsIContent* parentContent = aContent->GetParent();
      nsIFrame* parentContentFrame = mPresShell->GetPrimaryFrameFor(parentContent);
      if (!parentContentFrame || !parentContentFrame->IsFrameOfType(nsIFrame::eMathML))
        break;
      aContent = parentContent;
      frame = parentContentFrame;
    }
  }

  if (frame) {
    nsIFrame* nonGeneratedAncestor = nsLayoutUtils::GetNonGeneratedAncestor(frame);
    if (nonGeneratedAncestor->GetContent() != aContent) {
      return RecreateFramesForContent(nonGeneratedAncestor->GetContent());
    }
  }

  nsresult rv = NS_OK;

  if (frame && MaybeRecreateContainerForIBSplitterFrame(frame, &rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> container = aContent->GetParent();
  if (container) {
    // XXXbz what if this is anonymous content?
    PRInt32 indexInContainer = container->IndexOf(aContent);
    // Before removing the frames associated with the content object,
    // ask them to save their state onto a temporary state object.
    CaptureStateForFramesOf(aContent, mTempFrameTreeState);

    // Remove the frames associated with the content object on which
    // the attribute change occurred.
    PRBool didReconstruct;
    rv = ContentRemoved(container, aContent, indexInContainer, &didReconstruct);

    if (NS_SUCCEEDED(rv) && !didReconstruct) {
      // Now, recreate the frames associated with this content object. If
      // ContentRemoved triggered reconstruction, then we don't need to do this
      // because the frames will already have been built.
      rv = ContentInserted(container, aContent,
                           indexInContainer, mTempFrameTreeState);
    }
  } else {
    // The content is the root node, so just rebuild the world.
    ReconstructDocElementHierarchy();
  }

#ifdef ACCESSIBILITY
  if (mPresShell->IsAccessibilityActive()) {
    PRUint32 event;
    if (frame) {
      nsIFrame *newFrame = mPresShell->GetPrimaryFrameFor(aContent);
      event = newFrame ? PRUint32(nsIAccessibleEvent::EVENT_ASYNCH_SIGNIFICANT_CHANGE) :
                         PRUint32(nsIAccessibleEvent::EVENT_ASYNCH_HIDE);
    }
    else {
      event = nsIAccessibleEvent::EVENT_ASYNCH_SHOW;
    }

    // A significant enough change occured that this part
    // of the accessible tree is no longer valid.
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->InvalidateSubtreeFor(mPresShell, aContent, event);
    }
  }
#endif

  return rv;
}

//////////////////////////////////////////////////////////////////////

// Block frame construction code

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLetterStyle(nsIContent* aContent,
                                           nsStyleContext* aStyleContext)
{
  if (aContent) {
    return mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent,
                            nsCSSPseudoElements::firstLetter, aStyleContext);
  }
  return nsnull;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLineStyle(nsIContent* aContent,
                                         nsStyleContext* aStyleContext)
{
  if (aContent) {
    return mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent,
                            nsCSSPseudoElements::firstLine, aStyleContext);
  }
  return nsnull;
}

// Predicate to see if a given content (block element) has
// first-letter style applied to it.
PRBool
nsCSSFrameConstructor::ShouldHaveFirstLetterStyle(nsIContent* aContent,
                                                  nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       nsCSSPseudoElements::firstLetter,
                                       mPresShell->GetPresContext());
}

PRBool
nsCSSFrameConstructor::HasFirstLetterStyle(nsIFrame* aBlockFrame)
{
  NS_PRECONDITION(aBlockFrame, "Need a frame");
  NS_ASSERTION(nsLayoutUtils::GetAsBlock(aBlockFrame),
               "Not a block frame?");

  return (aBlockFrame->GetStateBits() & NS_BLOCK_HAS_FIRST_LETTER_STYLE) != 0;
}

PRBool
nsCSSFrameConstructor::ShouldHaveFirstLineStyle(nsIContent* aContent,
                                                nsStyleContext* aStyleContext)
{
  PRBool hasFirstLine =
    nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                  nsCSSPseudoElements::firstLine,
                                  mPresShell->GetPresContext());
  if (hasFirstLine) {
    // But disable for fieldsets
    PRInt32 namespaceID;
    nsIAtom* tag = mDocument->BindingManager()->ResolveTag(aContent,
                                                           &namespaceID);
    // This check must match the one in ConstructHTMLFrame.
    hasFirstLine = tag != nsGkAtoms::fieldset ||
      (namespaceID != kNameSpaceID_XHTML &&
       !aContent->IsNodeOfType(nsINode::eHTML));
  }

  return hasFirstLine;
}

void
nsCSSFrameConstructor::ShouldHaveSpecialBlockStyle(nsIContent* aContent,
                                                   nsStyleContext* aStyleContext,
                                                   PRBool* aHaveFirstLetterStyle,
                                                   PRBool* aHaveFirstLineStyle)
{
  *aHaveFirstLetterStyle =
    ShouldHaveFirstLetterStyle(aContent, aStyleContext);
  *aHaveFirstLineStyle =
    ShouldHaveFirstLineStyle(aContent, aStyleContext);
}

/**
 * Request to process the child content elements and create frames.
 *
 * @param   aContent the content object whose child elements to process
 * @param   aFrame the frame associated with aContent. This will be the
 *            parent frame (both content and geometric) for the flowed
 *            child frames
 */
nsresult
nsCSSFrameConstructor::ProcessChildren(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aFrame,
                                       PRBool                   aCanHaveGeneratedContent,
                                       nsFrameItems&            aFrameItems,
                                       PRBool                   aParentIsBlock)
{
  NS_PRECONDITION(!aFrame->IsLeaf(), "Bogus ProcessChildren caller!");
  // XXXbz ideally, this would do all the pushing of various
  // containing blocks as needed, so callers don't have to do it...
  nsresult rv = NS_OK;
  // :before/:after content should have the same style context parent
  // as normal kids.
  nsStyleContext* styleContext =
    nsFrame::CorrectStyleParentFrame(aFrame, nsnull)->GetStyleContext();
    
  // save the incoming pseudo frame state
  nsPseudoFrames priorPseudoFrames;
  aState.mPseudoFrames.Reset(&priorPseudoFrames);

  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    CreateGeneratedContentFrame(aState, aFrame, aContent,
                                styleContext, nsCSSPseudoElements::before,
                                aFrameItems);
  }

  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       iter != last;
       ++iter) {
    rv = ConstructFrame(aState, nsCOMPtr<nsIContent>(*iter),
                        aFrame, aFrameItems);
    if (NS_FAILED(rv))
      return rv;
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    CreateGeneratedContentFrame(aState, aFrame, aContent,
                                styleContext, nsCSSPseudoElements::after,
                                aFrameItems);
  }

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aFrameItems);
  }

  // restore the incoming pseudo frame state
  aState.mPseudoFrames = priorPseudoFrames;

  NS_ASSERTION(!aParentIsBlock || !aFrame->IsBoxFrame(),
               "can't be both block and box");

  if (aParentIsBlock) {
    if (aState.mFirstLetterStyle) {
      rv = WrapFramesInFirstLetterFrame(aState, aContent, aFrame, aFrameItems);
    }
    if (aState.mFirstLineStyle) {
      rv = WrapFramesInFirstLineFrame(aState, aContent, aFrame, aFrameItems);
    }
  }

  nsIContent *badKid;
  if (aFrame->IsBoxFrame() &&
      (badKid = AnyKidsNeedBlockParent(aFrameItems.childList))) {
    nsAutoString parentTag, kidTag;
    aContent->Tag()->ToString(parentTag);
    badKid->Tag()->ToString(kidTag);
    const PRUnichar* params[] = { parentTag.get(), kidTag.get() };
    nsStyleContext *frameStyleContext = aFrame->GetStyleContext();
    const nsStyleDisplay *display = frameStyleContext->GetStyleDisplay();
    const char *message =
      (display->mDisplay == NS_STYLE_DISPLAY_INLINE_BOX)
        ? "NeededToWrapXULInlineBox" : "NeededToWrapXUL";
    nsContentUtils::ReportToConsole(nsContentUtils::eXUL_PROPERTIES,
                                    message,
                                    params, NS_ARRAY_LENGTH(params),
                                    mDocument->GetDocumentURI(),
                                    EmptyString(), 0, 0, // not useful
                                    nsIScriptError::warningFlag,
                                    "FrameConstructor");

    nsRefPtr<nsStyleContext> blockSC = mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent,
                            nsCSSAnonBoxes::mozXULAnonymousBlock,
                            frameStyleContext);
    nsIFrame *blockFrame = NS_NewBlockFrame(mPresShell, blockSC);
    // We might, in theory, want to set NS_BLOCK_SPACE_MGR and
    // NS_BLOCK_MARGIN_ROOT, but I think it's a bad idea given that
    // a real block placed here wouldn't get those set on it.

    InitAndRestoreFrame(aState, aContent, aFrame, nsnull,
                        blockFrame, PR_FALSE);

    NS_ASSERTION(!blockFrame->HasView(), "need to do view reparenting");
    for (nsIFrame *f = aFrameItems.childList; f; f = f->GetNextSibling()) {
      ReparentFrame(aState.mFrameManager, blockFrame, f);
    }

    blockFrame->AppendFrames(nsnull, aFrameItems.childList);
    aFrameItems = nsFrameItems();
    aFrameItems.AddChild(blockFrame);

    aFrame->AddStateBits(NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK);
  }

  return rv;
}

//----------------------------------------------------------------------

// Support for :first-line style

// Special routine to handle placing a list of frames into a block
// frame that has first-line style. The routine ensures that the first
// collection of inline frames end up in a first-line frame.
nsresult
nsCSSFrameConstructor::WrapFramesInFirstLineFrame(
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;

  // Find the first and last inline frame in aFrameItems
  nsIFrame* kid = aFrameItems.childList;
  nsIFrame* firstInlineFrame = nsnull;
  nsIFrame* lastInlineFrame = nsnull;
  while (kid) {
    if (IsInlineOutside(kid)) {
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
  nsStyleContext* parentStyle =
    nsFrame::CorrectStyleParentFrame(aBlockFrame,
                                     nsCSSPseudoElements::firstLine)->
      GetStyleContext();
  nsRefPtr<nsStyleContext> firstLineStyle = GetFirstLineStyle(aBlockContent,
                                                              parentStyle);

  nsIFrame* lineFrame = NS_NewFirstLineFrame(mPresShell, firstLineStyle);

  if (lineFrame) {
    // Initialize the line frame
    rv = InitAndRestoreFrame(aState, aBlockContent, aBlockFrame, nsnull,
                             lineFrame);

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
    NS_ASSERTION(lineFrame->GetStyleContext() == firstLineStyle,
                 "Bogus style context on line frame");
    while (kid) {
      ReparentFrame(aState.mFrameManager, lineFrame, kid);
      kid = kid->GetNextSibling();
    }
    lineFrame->SetInitialChildList(nsnull, firstInlineFrame);
  }
  else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// Special routine to handle appending a new frame to a block frame's
// child list. Takes care of placing the new frame into the right
// place when first-line style is present.
nsresult
nsCSSFrameConstructor::AppendFirstLineFrames(
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aFrameItems)
{
  // It's possible that aBlockFrame needs to have a first-line frame
  // created because it doesn't currently have any children.
  nsIFrame* blockKid = aBlockFrame->GetFirstChild(nsnull);
  if (!blockKid) {
    return WrapFramesInFirstLineFrame(aState, aBlockContent,
                                      aBlockFrame, aFrameItems);
  }

  // Examine the last block child - if it's a first-line frame then
  // appended frames need special treatment.
  nsresult rv = NS_OK;
  nsFrameList blockFrames(blockKid);
  nsIFrame* lastBlockKid = blockFrames.LastChild();
  if (lastBlockKid->GetType() != nsGkAtoms::lineFrame) {
    // No first-line frame at the end of the list, therefore there is
    // an interveening block between any first-line frame the frames
    // we are appending. Therefore, we don't need any special
    // treatment of the appended frames.
    return rv;
  }
  nsIFrame* lineFrame = lastBlockKid;

  // Find the first and last inline frame in aFrameItems
  nsIFrame* kid = aFrameItems.childList;
  nsIFrame* firstInlineFrame = nsnull;
  nsIFrame* lastInlineFrame = nsnull;
  while (kid) {
    if (IsInlineOutside(kid)) {
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
    ReparentFrame(aState.mFrameManager, lineFrame, kid);
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
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aBlockFrame,
  nsIFrame**               aParentFrame,
  nsIFrame*                aPrevSibling,
  nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
  // XXXbz If you make this method actually do something, check to make sure
  // that the caller is passing what you expect.  In particular, which content
  // is aContent?
#if 0
  nsIFrame* parentFrame = *aParentFrame;
  nsIFrame* newFrame = aFrameItems.childList;
  PRBool isInline = IsInlineOutside(newFrame);

  if (!aPrevSibling) {
    // Insertion will become the first frame. Two cases: we either
    // already have a first-line frame or we don't.
    nsIFrame* firstBlockKid = aBlockFrame->GetFirstChild(nsnull);
    if (firstBlockKid->GetType() == nsGkAtoms::lineFrame) {
      // We already have a first-line frame
      nsIFrame* lineFrame = firstBlockKid;

      if (isInline) {
        // Easy case: the new inline frame will go into the lineFrame.
        ReparentFrame(aState.mFrameManager, lineFrame, newFrame);
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
        nsIFrame* lineFrame = NS_NewFirstLineFrame(firstLineStyle);
        if (!lineFrame) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }

        if (NS_SUCCEEDED(rv)) {
          // Lookup first-line style context
          nsStyleContext* parentStyle =
            nsFrame::CorrectStyleParentFrame(aBlockFrame,
                                             nsCSSPseudoElements::firstLine)->
              GetStyleContext();
          nsRefPtr<nsStyleContext> firstLineStyle =
            GetFirstLineStyle(aContent, parentStyle);

          // Initialize the line frame
          rv = InitAndRestoreFrame(aState, aContent, aBlockFrame,
                                   nsnull, lineFrame);

          // Make sure the caller inserts the lineFrame into the
          // blocks list of children.
          aFrameItems.childList = lineFrame;
          aFrameItems.lastChild = lineFrame;

          // Give the inline frames to the lineFrame <b>after</b>
          // reparenting them
          NS_ASSERTION(lineFrame->GetStyleContext() == firstLineStyle,
                       "Bogus style context on line frame");
          ReparentFrame(aPresContext, lineFrame, newFrame);
          lineFrame->SetInitialChildList(nsnull, newFrame);
        }
      }
      else {
        // Easy case: the regular insertion logic can insert the new
        // frame because it's a block frame.
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
          ReparentFrame(aState.mFrameManager, aBlockFrame, newFrame);
          aState.mFrameManager->InsertFrames(aBlockFrame, nsnull,
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
    const nsTextFragment* frag = aContent->GetText();
    if (frag) {
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
  return aContent->TextLength() &&
         !aContent->TextIsOnlyWhitespace();
}

/**
 * Create a letter frame, only make it a floating frame.
 */
void
nsCSSFrameConstructor::CreateFloatingLetterFrame(
  nsFrameConstructorState& aState,
  nsIFrame* aBlockFrame,
  nsIContent* aTextContent,
  nsIFrame* aTextFrame,
  nsIContent* aBlockContent,
  nsIFrame* aParentFrame,
  nsStyleContext* aStyleContext,
  nsFrameItems& aResult)
{
  // Create the first-letter-frame
  nsresult rv;
  nsIFrame* letterFrame;
  nsStyleSet *styleSet = mPresShell->StyleSet();

  letterFrame = NS_NewFirstLetterFrame(mPresShell, aStyleContext);
  // We don't want to use a text content for a non-text frame (because we want
  // its primary frame to be a text frame).  So use its parent for the
  // first-letter.
  nsIContent* letterContent = aTextContent->GetParent();
  InitAndRestoreFrame(aState, letterContent,
                      aState.GetGeometricParent(aStyleContext->GetStyleDisplay(),
                                                aParentFrame),
                      nsnull, letterFrame);

  // Init the text frame to refer to the letter frame. Make sure we
  // get a proper style context for it (the one passed in is for the
  // letter frame and will have the float property set on it; the text
  // frame shouldn't have that set).
  nsRefPtr<nsStyleContext> textSC;
  textSC = styleSet->ResolveStyleForNonElement(aStyleContext);
  aTextFrame->SetStyleContextWithoutNotification(textSC);
  InitAndRestoreFrame(aState, aTextContent, letterFrame, nsnull, aTextFrame);

  // And then give the text frame to the letter frame
  letterFrame->SetInitialChildList(nsnull, aTextFrame);

  // See if we will need to continue the text frame (does it contain
  // more than just the first-letter text or not?) If it does, then we
  // create (in advance) a continuation frame for it.
  nsIFrame* nextTextFrame = nsnull;
  if (NeedFirstLetterContinuation(aTextContent)) {
    // Create continuation
    rv = CreateContinuingFrame(aState.mPresContext, aTextFrame, aParentFrame,
                               &nextTextFrame);
    if (NS_FAILED(rv)) {
      letterFrame->Destroy();
      return;
    }
    // Repair the continuations style context
    nsStyleContext* parentStyleContext = aStyleContext->GetParent();
    if (parentStyleContext) {
      nsRefPtr<nsStyleContext> newSC;
      newSC = styleSet->ResolveStyleForNonElement(parentStyleContext);
      if (newSC) {
        nextTextFrame->SetStyleContext(newSC);
      }
    }
  }

  NS_ASSERTION(aResult.childList == nsnull,
               "aResult should be an empty nsFrameItems!");
  nsIFrame* insertAfter = nsnull;
  nsIFrame* f;
  // Put the new float before any of the floats in the block we're
  // doing first-letter for, that is, before any floats whose parent is aBlockFrame
  for (f = aState.mFloatedItems.childList; f; f = f->GetNextSibling()) {
    if (f->GetParent() == aBlockFrame)
      break;
    insertAfter = f;
  }

  rv = aState.AddChild(letterFrame, aResult, letterContent, aStyleContext,
                       aParentFrame, PR_FALSE, PR_TRUE, PR_FALSE, PR_TRUE,
                       insertAfter);

  if (nextTextFrame) {
    if (NS_FAILED(rv)) {
      nextTextFrame->Destroy();
    } else {
      aResult.AddChild(nextTextFrame);
    }
  }
}

/**
 * Create a new letter frame for aTextFrame. The letter frame will be
 * a child of aParentFrame.
 */
nsresult
nsCSSFrameConstructor::CreateLetterFrame(nsFrameConstructorState& aState,
                                         nsIFrame* aBlockFrame,
                                         nsIContent* aTextContent,
                                         nsIFrame* aParentFrame,
                                         nsFrameItems& aResult)
{
  NS_PRECONDITION(aTextContent->IsNodeOfType(nsINode::eTEXT),
                  "aTextContent isn't text");
  NS_ASSERTION(nsLayoutUtils::GetAsBlock(aBlockFrame),
                 "Not a block frame?");

  // Get style context for the first-letter-frame
  nsStyleContext* parentStyleContext =
    nsFrame::CorrectStyleParentFrame(aParentFrame,
                                     nsCSSPseudoElements::firstLetter)->
      GetStyleContext();
  // Use content from containing block so that we can actually
  // find a matching style rule.
  nsIContent* blockContent =
    aState.mFloatedItems.containingBlock->GetContent();

  NS_ASSERTION(blockContent == aBlockFrame->GetContent(),
               "Unexpected block content");

  // Create first-letter style rule
  nsRefPtr<nsStyleContext> sc = GetFirstLetterStyle(blockContent,
                                                    parentStyleContext);
  if (sc) {
    nsRefPtr<nsStyleContext> textSC;
    textSC = mPresShell->StyleSet()->ResolveStyleForNonElement(sc);
    
    // Create a new text frame (the original one will be discarded)
    // pass a temporary stylecontext, the correct one will be set later
    nsIFrame* textFrame = NS_NewTextFrame(mPresShell, textSC);

    // Create the right type of first-letter frame
    const nsStyleDisplay* display = sc->GetStyleDisplay();
    if (display->IsFloating()) {
      // Make a floating first-letter frame
      CreateFloatingLetterFrame(aState, aBlockFrame, aTextContent, textFrame,
                                blockContent, aParentFrame,
                                sc, aResult);
    }
    else {
      // Make an inflow first-letter frame
      nsIFrame* letterFrame = NS_NewFirstLetterFrame(mPresShell, sc);

      if (letterFrame) {
        // Initialize the first-letter-frame.  We don't want to use a text
        // content for a non-text frame (because we want its primary frame to
        // be a text frame).  So use its parent for the first-letter.
        nsIContent* letterContent = aTextContent->GetParent();
        letterFrame->Init(letterContent, aParentFrame, nsnull);

        InitAndRestoreFrame(aState, aTextContent, letterFrame, nsnull,
                            textFrame);

        letterFrame->SetInitialChildList(nsnull, textFrame);
        aResult.childList = aResult.lastChild = letterFrame;
        aBlockFrame->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_CHILD);
      }
    }
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsIFrame*                aBlockFrame,
  nsFrameItems&            aBlockFrames)
{
  nsresult rv = NS_OK;

  aBlockFrame->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_STYLE);

  nsIFrame* parentFrame = nsnull;
  nsIFrame* textFrame = nsnull;
  nsIFrame* prevFrame = nsnull;
  nsFrameItems letterFrames;
  PRBool stopLooking = PR_FALSE;
  rv = WrapFramesInFirstLetterFrame(aState, aBlockFrame, aBlockFrame,
                                    aBlockFrames.childList,
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
      textFrame->Destroy();

      // Repair lastChild; the only time this needs to happen is when
      // the block had one child (the text frame).
      if (!nextSibling) {
        aBlockFrames.lastChild = letterFrames.lastChild;
      }
    }
    else {
      // Take the old textFrame out of the inline parents child list
      ::DeletingFrameSubtree(aState.mFrameManager, textFrame);
      parentFrame->RemoveFrame(nsnull, textFrame);

      // Insert in the letter frame(s)
      parentFrame->InsertFrames(nsnull, prevFrame, letterFrames.childList);
    }
  }

  return rv;
}

nsresult
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsFrameConstructorState& aState,
  nsIFrame*                aBlockFrame,
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
    if (nsGkAtoms::textFrame == frameType) {
      // Wrap up first-letter content in a letter frame
      nsIContent* textContent = frame->GetContent();
      if (IsFirstLetterContent(textContent)) {
        // Create letter frame to wrap up the text
        rv = CreateLetterFrame(aState, aBlockFrame, textContent,
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
    else if (IsInlineFrame(frame) && frameType != nsGkAtoms::brFrame) {
      nsIFrame* kids = frame->GetFirstChild(nsnull);
      WrapFramesInFirstLetterFrame(aState, aBlockFrame, frame, kids,
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
  nsIFrame* floatFrame = aBlockFrame->GetFirstChild(nsGkAtoms::floatList);
  while (floatFrame) {
    // See if we found a floating letter frame
    if (nsGkAtoms::letterFrame == floatFrame->GetType()) {
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
  nsIFrame* newTextFrame = NS_NewTextFrame(aPresShell, newSC);
  if (NS_UNLIKELY(!newTextFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;;
  }
  newTextFrame->Init(textContent, parentFrame, nsnull);

  // Destroy the old text frame's continuations (the old text frame
  // will be destroyed when its letter frame is destroyed).
  nsIFrame* nextTextFrame = textFrame->GetNextContinuation();
  while (nextTextFrame) {
    nsIFrame* nextTextParent = nextTextFrame->GetParent();
    if (nextTextParent) {
      nsSplittableFrame::RemoveFromFlow(nextTextFrame);
      ::DeletingFrameSubtree(aFrameManager, nextTextFrame);
      aFrameManager->RemoveFrame(nextTextParent, nsnull, nextTextFrame);
    }
    nextTextFrame = textFrame->GetNextContinuation();
  }

  // First find out where (in the content) the placeholder frames
  // text is and its previous sibling frame, if any.  Note that:
  // 1)  The placeholder had better be in the principal child list of
  //     parentFrame.
  // 2)  It's probably near the beginning (since we're a first-letter frame),
  //     so just doing a linear search for the prevSibling is ok.
  // 3)  Trying to use FindPreviousSibling will fail if the first-letter is in
  //     anonymous content (eg generated content).
  nsFrameList siblingList(parentFrame->GetFirstChild(nsnull));
  NS_ASSERTION(siblingList.ContainsFrame(placeholderFrame),
               "Placeholder not in parent's principal child list?");
  nsIFrame* prevSibling = siblingList.GetPrevSiblingFor(placeholderFrame);

  // Now that everything is set...
#ifdef NOISY_FIRST_LETTER
  printf("RemoveFloatingFirstLetterFrames: textContent=%p oldTextFrame=%p newTextFrame=%p\n",
         textContent.get(), textFrame, newTextFrame);
#endif

  UnregisterPlaceholderChain(aFrameManager, placeholderFrame);

  // Remove the float frame
  ::DeletingFrameSubtree(aFrameManager, floatFrame);
  aFrameManager->RemoveFrame(aBlockFrame, nsGkAtoms::floatList,
                             floatFrame);

  // Remove placeholder frame
  ::DeletingFrameSubtree(aFrameManager, placeholderFrame);
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
    if (nsGkAtoms::letterFrame == kid->GetType()) {
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
      textFrame = NS_NewTextFrame(aPresShell, newSC);
      textFrame->Init(textContent, aFrame, nsnull);

      // Next rip out the kid and replace it with the text frame
      ::DeletingFrameSubtree(aFrameManager, kid);
      aFrameManager->RemoveFrame(aFrame, nsnull, kid);

      // Insert text frame in its place
      aFrameManager->InsertFrames(aFrame, nsnull, prevSibling, textFrame);

      *aStopLooking = PR_TRUE;
      aFrame->RemoveStateBits(NS_BLOCK_HAS_FIRST_LETTER_CHILD);
      break;
    }
    else if (IsInlineFrame(kid)) {
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
  aBlockFrame = aBlockFrame->GetFirstContinuation();
  
  PRBool stopLooking = PR_FALSE;
  nsresult rv;
  do {
    rv = RemoveFloatingFirstLetterFrames(aPresContext, aPresShell,
                                         aFrameManager,
                                         aBlockFrame, &stopLooking);
    if (NS_SUCCEEDED(rv) && !stopLooking) {
      rv = RemoveFirstLetterFrames(aPresContext, aPresShell, aFrameManager,
                                   aBlockFrame, &stopLooking);
    }
    if (stopLooking) {
      break;
    }
    aBlockFrame = aBlockFrame->GetNextContinuation();
  }  while (aBlockFrame);
  return rv;
}

// Fixup the letter frame situation for the given block
nsresult
nsCSSFrameConstructor::RecoverLetterFrames(nsFrameConstructorState& aState,
                                           nsIFrame* aBlockFrame)
{
  aBlockFrame = aBlockFrame->GetFirstContinuation();
  
  nsIFrame* parentFrame = nsnull;
  nsIFrame* textFrame = nsnull;
  nsIFrame* prevFrame = nsnull;
  nsFrameItems letterFrames;
  PRBool stopLooking = PR_FALSE;
  nsresult rv;
  do {
    // XXX shouldn't this bit be set already (bug 408493), assert instead?
    aBlockFrame->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_STYLE);
    rv = WrapFramesInFirstLetterFrame(aState, aBlockFrame, aBlockFrame,
                                      aBlockFrame->GetFirstChild(nsnull),
                                      &parentFrame, &textFrame, &prevFrame,
                                      letterFrames, &stopLooking);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (stopLooking) {
      break;
    }
    aBlockFrame = aBlockFrame->GetNextContinuation();
  } while (aBlockFrame);

  if (parentFrame) {
    // Take the old textFrame out of the parents child list
    ::DeletingFrameSubtree(aState.mFrameManager, textFrame);
    parentFrame->RemoveFrame(nsnull, textFrame);

    // Insert in the letter frame(s)
    parentFrame->InsertFrames(nsnull, prevFrame, letterFrames.childList);
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
    nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                  GetAbsoluteContainingBlock(aParentFrame),
                                  GetFloatContainingBlock(aParentFrame), 
                                  mTempFrameTreeState);

    nsRefPtr<nsStyleContext> styleContext;
    styleContext = ResolveStyleContext(aParentFrame, aChild);

    // Pre-check for display "none" - only if we find that, do we create
    // any frame at all
    const nsStyleDisplay* display = styleContext->GetStyleDisplay();

    if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
      *aNewFrame = nsnull;
      return NS_OK;
    }

    BeginUpdate();

    rv = ConstructFrameInternal(state, aChild,
                                aParentFrame, aChild->Tag(),
                                aChild->GetNameSpaceID(),
                                styleContext, frameItems, PR_FALSE);
    if (!state.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(state, frameItems); 
    }

    nsIFrame* newFrame = frameItems.childList;
    *aNewFrame = newFrame;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      // Notify the parent frame
      if (aIsAppend)
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxAppendFrames(newFrame);
      else
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxInsertFrames(aPrevFrame, newFrame);
    }

    EndUpdate();
  }

  return rv;
#else
  return NS_ERROR_FAILURE;
#endif
}

//----------------------------------------

nsresult
nsCSSFrameConstructor::ConstructBlock(nsFrameConstructorState& aState,
                                      const nsStyleDisplay*    aDisplay,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParentFrame,
                                      nsIFrame*                aContentParentFrame,
                                      nsStyleContext*          aStyleContext,
                                      nsIFrame**               aNewFrame,
                                      nsFrameItems&            aFrameItems,
                                      PRBool                   aAbsPosContainer)
{
  // Create column wrapper if necessary
  nsIFrame* blockFrame = *aNewFrame;
  nsIFrame* parent = aParentFrame;
  nsIFrame* contentParent = aContentParentFrame;
  nsRefPtr<nsStyleContext> blockStyle = aStyleContext;
  const nsStyleColumn* columns = aStyleContext->GetStyleColumn();

  if (columns->mColumnCount != NS_STYLE_COLUMN_COUNT_AUTO
      || columns->mColumnWidth.GetUnit() != eStyleUnit_Auto) {
    nsIFrame* columnSetFrame = nsnull;
    columnSetFrame = NS_NewColumnSetFrame(mPresShell, aStyleContext, 0);
    if (!columnSetFrame) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, columnSetFrame);
    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(columnSetFrame, aContentParentFrame,
                                             PR_FALSE);
    blockStyle = mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent, nsCSSAnonBoxes::columnContent,
                            aStyleContext);
    contentParent = columnSetFrame;
    parent = columnSetFrame;
    *aNewFrame = columnSetFrame;

    columnSetFrame->SetInitialChildList(nsnull, blockFrame);
  }

  blockFrame->SetStyleContextWithoutNotification(blockStyle);
  InitAndRestoreFrame(aState, aContent, parent, nsnull, blockFrame);

  nsresult rv = aState.AddChild(*aNewFrame, aFrameItems, aContent,
                                aStyleContext,
                                aContentParentFrame ? aContentParentFrame :
                                                      aParentFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // See if we need to create a view, e.g. the frame is absolutely positioned
  nsHTMLContainerFrame::CreateViewForFrame(blockFrame, contentParent, PR_FALSE);

  if (!mInitialContainingBlock) {
    // The frame we're constructing will be the initial containing block.
    // Set mInitialContainingBlock before processing children.
    mInitialContainingBlock = *aNewFrame;
  }

  // We should make the outer frame be the absolute containing block,
  // if one is required. We have to do this because absolute
  // positioning must be computed with respect to the CSS dimensions
  // of the element, which are the dimensions of the outer block. But
  // we can't really do that because only blocks can have absolute
  // children. So use the block and try to compensate with hacks
  // in nsBlockFrame::CalculateContainingBlockSizeForAbsolutes.
  nsFrameConstructorSaveState absoluteSaveState;
  if (aAbsPosContainer) {
    //    NS_ASSERTION(aRelPos, "should have made area frame for this");
    aState.PushAbsoluteContainingBlock(blockFrame, absoluteSaveState);
  }

  // See if the block has first-letter style applied to it...
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  ShouldHaveSpecialBlockStyle(aContent, aStyleContext,
                              &haveFirstLetterStyle, &haveFirstLineStyle);

  // Process the child content
  nsFrameItems childItems;
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(blockFrame, floatSaveState,
                                  haveFirstLetterStyle,
                                  haveFirstLineStyle);
  rv = ProcessChildren(aState, aContent, blockFrame, PR_TRUE, childItems,
                       PR_TRUE);

  CreateAnonymousFrames(aContent->Tag(), aState, aContent, blockFrame,
                        PR_FALSE, childItems);

  // Set the frame's initial child list
  blockFrame->SetInitialChildList(nsnull, childItems.childList);

  return rv;
}

static PRBool
AreAllKidsInline(nsIFrame* aFrameList)
{
  nsIFrame* kid = aFrameList;
  while (kid) {
    if (!IsInlineOutside(kid)) {
      return PR_FALSE;
    }
    kid = kid->GetNextSibling();
  }
  return PR_TRUE;
}

nsresult
nsCSSFrameConstructor::ConstructInline(nsFrameConstructorState& aState,
                                       const nsStyleDisplay*    aDisplay,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
                                       PRBool                   aIsPositioned,
                                       nsIFrame*                aNewFrame)
{
  // Initialize the frame
  InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, aNewFrame);

  nsFrameConstructorSaveState absoluteSaveState;  // definition cannot be inside next block
                                                  // because the object's destructor is significant
                                                  // this is part of the fix for bug 42372

  // Any inline frame might need a view (because of opacity, or fixed background)
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(aNewFrame, nsnull, PR_FALSE);

  if (aIsPositioned) {                            
    // Relatively positioned frames becomes a container for child
    // frames that are positioned
    aState.PushAbsoluteContainingBlock(aNewFrame, absoluteSaveState);
  }

  // Process the child content
  nsFrameItems childItems;
  PRBool kidsAllInline;
  nsresult rv = ProcessInlineChildren(aState, aContent, aNewFrame, PR_TRUE,
                                      childItems, &kidsAllInline);
  if (kidsAllInline) {
    // Set the inline frame's initial child list
    CreateAnonymousFrames(aContent->Tag(), aState, aContent, aNewFrame,
                          PR_FALSE, childItems);

    aNewFrame->SetInitialChildList(nsnull, childItems.childList);
    return rv;
  }

  // This inline frame contains several types of children. Therefore
  // this frame has to be chopped into several pieces. We will produce
  // as a result of this 3 lists of children. The first list contains
  // all of the inline children that precede the first block child
  // (and may be empty). The second list contains all of the block
  // children and any inlines that are between them (and must not be
  // empty, otherwise - why are we here?). The final list contains all
  // of the inline children that follow the final block child.

  // Find the first block child which defines list1 and list2
  nsIFrame* list1 = childItems.childList;
  nsIFrame* prevToFirstBlock;
  nsIFrame* list2 = FindFirstBlock(list1, &prevToFirstBlock);
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
  nsIFrame* lastBlock = FindLastBlock(afterFirstBlock);
  if (!lastBlock) {
    lastBlock = list2;
  }
  list3 = lastBlock->GetNextSibling();
  lastBlock->SetNextSibling(nsnull);

  // list1's frames belong to this inline frame so go ahead and take them
  aNewFrame->SetInitialChildList(nsnull, list1);
                                             
  // list2's frames belong to an anonymous block that we create right
  // now. The anonymous block will be the parent of the block children
  // of the inline.
  nsIAtom* blockStyle;
  nsRefPtr<nsStyleContext> blockSC;
  nsIFrame* blockFrame;
  if (aIsPositioned) {
    blockStyle = nsCSSAnonBoxes::mozAnonymousPositionedBlock;
    
    blockSC = mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent, blockStyle, aStyleContext);
      
    blockFrame = NS_NewRelativeItemWrapperFrame(mPresShell, blockSC, 0);
  }
  else {
    blockStyle = nsCSSAnonBoxes::mozAnonymousBlock;

    blockSC = mPresShell->StyleSet()->
      ResolvePseudoStyleFor(aContent, blockStyle, aStyleContext);

    blockFrame = NS_NewBlockFrame(mPresShell, blockSC);
  }

  InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, blockFrame, PR_FALSE);  

  // Any inline frame could have a view (e.g., opacity)
  // XXXbz should we be passing in a non-null aContentParentFrame?
  nsHTMLContainerFrame::CreateViewForFrame(blockFrame, nsnull, PR_FALSE);

  if (blockFrame->HasView() || aNewFrame->HasView()) {
    // Move list2's frames into the new view
    nsHTMLContainerFrame::ReparentFrameViewList(aState.mPresContext, list2,
                                                list2->GetParent(), blockFrame);
  }

  blockFrame->SetInitialChildList(nsnull, list2);

  nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(blockFrame),
                                GetFloatContainingBlock(blockFrame));

  // If we have an inline between two blocks all inside an inline and the inner
  // inline contains a float, the float will end up in the float list of the
  // parent block of the inline, but its parent pointer will be the anonymous
  // block we create...  AdjustFloatParentPtrs() deals with this by moving the
  // float from the outer state |aState| to the inner |state|.
  MoveChildrenTo(state.mFrameManager, blockFrame, list2, nsnull, &state,
                 &aState);

  // list3's frames belong to another inline frame
  nsIFrame* inlineFrame = nsnull;

  // If we ever start constructing a second inline in the split even when
  // list3 is null, the logic in MaybeRecreateContainerForIBSplitterFrame
  // needs to be adjusted.  Also, if you're changing this code also change
  // AppendFrames().
  if (list3) {
    inlineFrame = MoveFramesToEndOfIBSplit(aState, nsnull,
                                           aIsPositioned, aContent,
                                           aStyleContext, list3,
                                           blockFrame, nsnull);
    
  }

  // Mark the frames as special (note: marking for inlineFrame is handled by
  // MoveFramesToEndOfIBSplit). That way if any of the append/insert/remove
  // methods try to fiddle with the children, the containing block will be
  // reframed instead.
  SetFrameIsSpecial(aNewFrame, blockFrame);
  SetFrameIsSpecial(blockFrame, inlineFrame);
  MarkIBSpecialPrevSibling(blockFrame, aNewFrame);
  if (inlineFrame) {
    MarkIBSpecialPrevSibling(inlineFrame, blockFrame);
  }

  #ifdef DEBUG
  if (gNoisyInlineConstruction) {
    nsIFrameDebug*  frameDebug;

    printf("nsCSSFrameConstructor::ConstructInline:\n");
    if (NS_SUCCEEDED(CallQueryInterface(aNewFrame, &frameDebug))) {
      printf("  ==> leading inline frame:\n");
      frameDebug->List(stdout, 2);
    }
    if (NS_SUCCEEDED(CallQueryInterface(blockFrame, &frameDebug))) {
      printf("  ==> block frame:\n");
      frameDebug->List(stdout, 2);
    }
    if (inlineFrame &&
        NS_SUCCEEDED(CallQueryInterface(inlineFrame, &frameDebug))) {
      printf("  ==> trailing inline frame:\n");
      frameDebug->List(stdout, 2);
    }
  }
#endif

  return rv;
}

nsIFrame*
nsCSSFrameConstructor::MoveFramesToEndOfIBSplit(nsFrameConstructorState& aState,
                                                nsIFrame* aExistingEndFrame,
                                                PRBool aIsPositioned,
                                                nsIContent* aContent,
                                                nsStyleContext* aStyleContext,
                                                nsIFrame* aFramesToMove,
                                                nsIFrame* aBlockPart,
                                                nsFrameConstructorState* aTargetState)
{
  NS_PRECONDITION(aFramesToMove, "Must have frames to move");
  NS_PRECONDITION(aBlockPart, "Must have a block part");

  nsIFrame* inlineFrame = aExistingEndFrame;
  if (!inlineFrame) {
    if (aIsPositioned) {
      inlineFrame = NS_NewPositionedInlineFrame(mPresShell, aStyleContext);
    }
    else {
      inlineFrame = NS_NewInlineFrame(mPresShell, aStyleContext);
    }

    InitAndRestoreFrame(aState, aContent, aBlockPart->GetParent(), nsnull,
                        inlineFrame, PR_FALSE);

    // Any frame might need a view
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(inlineFrame, nsnull, PR_FALSE);
  }
  
  if (inlineFrame->HasView() || aFramesToMove->GetParent()->HasView()) {
    // Move list3's frames into the new view
    nsHTMLContainerFrame::ReparentFrameViewList(aState.mPresContext,
                                                aFramesToMove,
                                                aFramesToMove->GetParent(),
                                                inlineFrame);
  }

  // Reparent (cheaply) the frames in list3
  nsIFrame* existingFirstChild = inlineFrame->GetFirstChild(nsnull);
  if (!existingFirstChild &&
      (inlineFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    inlineFrame->SetInitialChildList(nsnull, aFramesToMove);
  } else {
    inlineFrame->InsertFrames(nsnull, nsnull, aFramesToMove);
  }
  nsFrameConstructorState* startState = aTargetState ? &aState : nsnull;
  MoveChildrenTo(aState.mFrameManager, inlineFrame, aFramesToMove,
                 existingFirstChild, aTargetState, startState);
  SetFrameIsSpecial(inlineFrame, nsnull);
  return inlineFrame;
}
 
nsresult
nsCSSFrameConstructor::ProcessInlineChildren(nsFrameConstructorState& aState,
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
    styleContext = aFrame->GetStyleContext();
    CreateGeneratedContentFrame(aState, aFrame, aContent,
                                styleContext, nsCSSPseudoElements::before,
                                aFrameItems);
  }

  // Iterate the child content objects and construct frames
  PRBool allKidsInline = PR_TRUE;
  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       iter != last;
       ++iter) {
    // Construct a child frame
    nsIFrame* oldLastChild = aFrameItems.lastChild;
    rv = ConstructFrame(aState, nsCOMPtr<nsIContent>(*iter),
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
        if (!IsInlineOutside(kid)) {
          allKidsInline = PR_FALSE;
          break;
        }
        kid = kid->GetNextSibling();
      }
    }
  }

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    CreateGeneratedContentFrame(aState, aFrame, aContent,
                                styleContext, nsCSSPseudoElements::after,
                                aFrameItems);
  }

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aFrameItems);
    // recompute allKidsInline to take into account new child frames
    // XXX we DON'T do this yet because anonymous table children should
    // be accepted as inline children, until we turn on inline-table.
    // See bug 297537.
    // allKidsInline = AreAllKidsInline(aFrameItems.childList);
  }
  // restore the pseudo frame state
  aState.mPseudoFrames = prevPseudoFrames;

  *aKidsAllInline = allKidsInline;

  return rv;
}

static void
DestroyNewlyCreatedFrames(nsFrameConstructorState& aState,
                          nsIFrame* aParentFrame,
                          const nsFrameItems& aFrameList)
{
  // Ok, reverse tracks: wipe out the frames we just created
  nsFrameManager *frameManager = aState.mFrameManager;

  // Destroy the frames. As we do make sure any content to frame mappings
  // or entries in the undisplayed content map are removed
  frameManager->ClearAllUndisplayedContentIn(aParentFrame->GetContent());

  CleanupFrameReferences(frameManager, aFrameList.childList);
  if (aState.mAbsoluteItems.childList) {
    CleanupFrameReferences(frameManager, aState.mAbsoluteItems.childList);
  }
  if (aState.mFixedItems.childList) {
    CleanupFrameReferences(frameManager, aState.mFixedItems.childList);
  }
  if (aState.mFloatedItems.childList) {
    CleanupFrameReferences(frameManager, aState.mFloatedItems.childList);
  }
#ifdef MOZ_XUL
  if (aState.mPopupItems.childList) {
    CleanupFrameReferences(frameManager, aState.mPopupItems.childList);
  }
#endif
  nsFrameList tmp(aFrameList.childList);
  tmp.DestroyFrames();

  tmp.SetFrames(aState.mAbsoluteItems.childList);
  tmp.DestroyFrames();
  aState.mAbsoluteItems.childList = nsnull;

  tmp.SetFrames(aState.mFixedItems.childList);
  tmp.DestroyFrames();
  aState.mFixedItems.childList = nsnull;

  tmp.SetFrames(aState.mFloatedItems.childList);
  tmp.DestroyFrames();
  aState.mFloatedItems.childList = nsnull;

#ifdef MOZ_XUL
  tmp.SetFrames(aState.mPopupItems.childList);
  tmp.DestroyFrames();
  aState.mPopupItems.childList = nsnull;
#endif
}

PRBool
nsCSSFrameConstructor::WipeContainingBlock(nsFrameConstructorState& aState,
                                           nsIFrame* aContainingBlock,
                                           nsIFrame* aFrame,
                                           const nsFrameItems& aFrameList,
                                           PRBool aIsAppend,
                                           nsIFrame* aPrevSibling)
{
  if (!aFrameList.childList) {
    return PR_FALSE;
  }
  
  // Before we go and append the frames, we must check for two
  // special situations.

  // Situation #1 is a XUL frame that contains frames that are required
  // to be wrapped in blocks.
  if (aFrame->IsBoxFrame() &&
      !(aFrame->GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK) &&
      AnyKidsNeedBlockParent(aFrameList.childList)) {
    DestroyNewlyCreatedFrames(aState, aFrame, aFrameList);
    RecreateFramesForContent(aFrame->GetContent());
    return PR_TRUE;
  }

  // Situation #2 is an inline frame that will now contain block
  // frames. This is a no-no and the frame construction logic knows
  // how to fix this.  See defition of IsInlineFrame() for what "an
  // inline" is.  Whether we have "a block" is tested for by
  // AreAllKidsInline.

  // We also need to check for an append of content ending in an
  // inline to the block in an {ib} split or an insert of content
  // starting with an inline to the start of that block.  If that
  // happens, we also need to reframe, since that content needs to go
  // into the following or preceding inline in the split.

  if (IsInlineFrame(aFrame)) {
    // Nothing to do if all kids are inline
    if (AreAllKidsInline(aFrameList.childList)) {
      return PR_FALSE;
    }
  } else if (!IsFrameSpecial(aFrame)) {
    return PR_FALSE;
  } else {
    // aFrame is the block in an {ib} split.  Check that we're not
    // messing up either end of it.
    if (aIsAppend) {
      // Will be handled in AppendFrames(), unless we have floats that we can't
      // move out because there might be no float containing block to move them
      // into.
      if (!aState.mFloatedItems.childList) {
        return PR_FALSE;
      }

      // Walk up until we get a float containing block that's not part of an
      // {ib} split, since otherwise we might have to ship floats out of it
      // too.
      nsIFrame* floatContainer = aFrame;
      do {
        floatContainer = GetFloatContainingBlock(
          GetIBSplitSpecialPrevSiblingForAnonymousBlock(floatContainer));
        if (!floatContainer) {
          break;
        }
        if (!IsFrameSpecial(floatContainer)) {
          return PR_FALSE;
        }
      } while (1);
    }
    
    if (aPrevSibling && !aPrevSibling->GetNextSibling()) {
      // This is an append that won't go through AppendFrames.  We can bail out
      // if the last frame we're appending is not inline
      if (!aFrameList.lastChild->GetStyleDisplay()->IsInlineOutside()) {
        return PR_FALSE;
      }
    } else {
      // We can bail out if we're not inserting at the beginning or if
      // the first frame we're inserting is not inline.
      if (aPrevSibling ||
          !aFrameList.childList->GetStyleDisplay()->IsInlineOutside()) {
        return PR_FALSE;
      }
    }
  }

  DestroyNewlyCreatedFrames(aState, aFrame, aFrameList);

  // If we don't have a containing block, start with aFrame and look for one.
  if (!aContainingBlock) {
    aContainingBlock = aFrame;
  }
  
  // To find the right block to reframe, just walk up the tree until we find a
  // frame that is:
  // 1)  Not part of an IB split (not special)
  // 2)  Not a pseudo-frame
  // 3)  Not an inline frame
  // We're guaranteed to find one, since nsStyleContext::ApplyStyleFixups
  // enforces that the root is display:none, display:table, or display:block.
  // Note that walking up "too far" is OK in terms of correctness, even if it
  // might be a little inefficient.  This is why we walk out of all
  // pseudo-frames -- telling which ones are or are not OK to walk out of is
  // too hard (and I suspect that we do in fact need to walk out of all of
  // them).
  while (IsFrameSpecial(aContainingBlock) || IsInlineOutside(aContainingBlock) ||
         aContainingBlock->GetStyleContext()->GetPseudoType()) {
    aContainingBlock = aContainingBlock->GetParent();
    NS_ASSERTION(aContainingBlock,
                 "Must have non-inline, non-special, non-pseudo frame as root "
                 "(or child of root, for a table root)!");
  }

  // Tell parent of the containing block to reformulate the
  // entire block. This is painful and definitely not optimal
  // but it will *always* get the right answer.

  nsIContent *blockContent = aContainingBlock->GetContent();
  nsCOMPtr<nsIContent> parentContainer = blockContent->GetParent();
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::WipeContainingBlock: blockContent=%p parentContainer=%p\n",
           static_cast<void*>(blockContent),
           static_cast<void*>(parentContainer));
  }
#endif
  if (parentContainer) {
    ReinsertContent(parentContainer, blockContent);
  }
  else if (blockContent->GetCurrentDoc() == mDocument) {
    ReconstructDocElementHierarchyInternal();
  }
  return PR_TRUE;
}

nsresult
nsCSSFrameConstructor::ReframeContainingBlock(nsIFrame* aFrame)
{

#ifdef DEBUG
  // ReframeContainingBlock is a NASTY routine, it causes terrible performance problems
  // so I want to see when it is happening!  Unfortunately, it is happening way to often because
  // so much content on the web causes 'special' block-in-inline frame situations and we handle them
  // very poorly
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ReframeContainingBlock frame=%p\n",
           static_cast<void*>(aFrame));
  }
#endif

  PRBool isReflowing;
  mPresShell->IsReflowLocked(&isReflowing);
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
    // so GetFloatContainingBlock(aFrame) has been removed

    // And get the containingBlock's content
    nsCOMPtr<nsIContent> blockContent = containingBlock->GetContent();
    if (blockContent) {
      // Now find the containingBlock's content's parent
      nsCOMPtr<nsIContent> parentContainer = blockContent->GetParent();
      if (parentContainer) {
#ifdef DEBUG
        if (gNoisyContentUpdates) {
          printf("  ==> blockContent=%p, parentContainer=%p\n",
                 static_cast<void*>(blockContent),
                 static_cast<void*>(parentContainer));
        }
#endif
        return ReinsertContent(parentContainer, blockContent);
      }
    }
  }

  // If we get here, we're screwed!
  return ReconstructDocElementHierarchyInternal();
}

nsresult
nsCSSFrameConstructor::RemoveFixedItems(const nsFrameConstructorState& aState,
                                        nsIFrame *aRootElementFrame)
{
  nsresult rv=NS_OK;

  if (mFixedContainingBlock) {
    nsIFrame *fixedChild = nsnull;
    do {
      fixedChild = mFixedContainingBlock->GetFirstChild(nsGkAtoms::fixedList);
      if (fixedChild && fixedChild == aRootElementFrame) {
        // Skip the root element frame, if it happens to be fixed-positioned
        // It will be explicitly removed later in
        // ReconstructDocElementHierarchyInternal
        fixedChild = fixedChild->GetNextSibling();
      }
      if (fixedChild) {
        // Remove the placeholder so it doesn't end up sitting about pointing
        // to the removed fixed frame.
        nsPlaceholderFrame *placeholderFrame =
          aState.mFrameManager->GetPlaceholderFrameFor(fixedChild);
        NS_ASSERTION(placeholderFrame, "no placeholder for fixed-pos frame");
        NS_ASSERTION(placeholderFrame->GetType() ==
                     nsGkAtoms::placeholderFrame,
                     "Wrong type");
        UnregisterPlaceholderChain(aState.mFrameManager, placeholderFrame);
        nsIFrame* placeholderParent = placeholderFrame->GetParent();
        ::DeletingFrameSubtree(aState.mFrameManager, placeholderFrame);
        rv = aState.mFrameManager->RemoveFrame(placeholderParent, nsnull,
                                               placeholderFrame);
        if (NS_FAILED(rv)) {
          NS_WARNING("Error removing placeholder for fixed frame in RemoveFixedItems");
          break;
        }

        ::DeletingFrameSubtree(aState.mFrameManager, fixedChild);
        rv = aState.mFrameManager->RemoveFrame(mFixedContainingBlock,
                                               nsGkAtoms::fixedList,
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

void
nsCSSFrameConstructor::RestyleForAppend(nsIContent* aContainer,
                                        PRInt32 aNewIndexInContainer)
{
  NS_ASSERTION(aContainer, "must have container for append");
#ifdef DEBUG
  {
    for (PRInt32 index = aNewIndexInContainer;; ++index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (!content) {
        NS_ASSERTION(index != aNewIndexInContainer, "yikes, nothing appended");
        break;
      }
      NS_ASSERTION(!content->IsRootOfAnonymousSubtree(),
                   "anonymous nodes should not be in child lists");
    }
  }
#endif
  PRUint32 selectorFlags =
    aContainer->GetFlags() & (NODE_ALL_SELECTOR_FLAGS &
                              ~NODE_HAS_SLOW_SELECTOR_NOAPPEND);
  if (selectorFlags == 0)
    return;

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR) {
    PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    PRBool wasEmpty = PR_TRUE; // :empty or :-moz-only-whitespace
    for (PRInt32 index = 0; index < aNewIndexInContainer; ++index) {
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(aContainer->GetChildAt(index),
                                          PR_TRUE, PR_FALSE)) {
        wasEmpty = PR_FALSE;
        break;
      }
    }
    if (wasEmpty) {
      PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
      // Restyling the container is the most we can do here, so we're done.
      return;
    }
  }
  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the last element child before this node
    for (PRInt32 index = aNewIndexInContainer - 1; index >= 0; --index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (content->IsNodeOfType(nsINode::eELEMENT)) {
        PostRestyleEvent(content, eReStyle_Self, NS_STYLE_HINT_NONE);
        break;
      }
    }
  }
}

// Restyling for a ContentInserted or CharacterDataChanged notification.
// This could be used for ContentRemoved as well if we got the
// notification before the removal happened (and sometimes
// CharacterDataChanged is more like a removal than an addition).
// The comments are written and variables are named in terms of it being
// a ContentInserted notification.
void
nsCSSFrameConstructor::RestyleForInsertOrChange(nsIContent* aContainer,
                                                nsIContent* aChild)
{
  NS_ASSERTION(!aChild->IsRootOfAnonymousSubtree(),
               "anonymous nodes should not be in child lists");
  PRUint32 selectorFlags =
    aContainer ? (aContainer->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags == 0)
    return;

  if (selectorFlags & (NODE_HAS_SLOW_SELECTOR |
                       NODE_HAS_SLOW_SELECTOR_NOAPPEND)) {
    PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    PRBool wasEmpty = PR_TRUE; // :empty or :-moz-only-whitespace
    for (PRInt32 index = 0; ; ++index) {
      nsIContent *child = aContainer->GetChildAt(index);
      if (!child) // last child
        break;
      if (child == aChild)
        continue;
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(child, PR_TRUE, PR_FALSE)) {
        wasEmpty = PR_FALSE;
        break;
      }
    }
    if (wasEmpty) {
      PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
      // Restyling the container is the most we can do here, so we're done.
      return;
    }
  }

  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the previously-first element child if it is after this node
    PRBool passedChild = PR_FALSE;
    for (PRInt32 index = 0; ; ++index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (!content)
        break; // went through all children
      if (content == aChild) {
        passedChild = PR_TRUE;
        continue;
      }
      if (content->IsNodeOfType(nsINode::eELEMENT)) {
        if (passedChild) {
          PostRestyleEvent(content, eReStyle_Self, NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
    // restyle the previously-last element child if it is before this node
    passedChild = PR_FALSE;
    for (PRInt32 index = aContainer->GetChildCount() - 1;
         index >= 0; --index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (content == aChild) {
        passedChild = PR_TRUE;
        continue;
      }
      if (content->IsNodeOfType(nsINode::eELEMENT)) {
        if (passedChild) {
          PostRestyleEvent(content, eReStyle_Self, NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
  }
}

void
nsCSSFrameConstructor::RestyleForRemove(nsIContent* aContainer,
                                        nsIContent* aOldChild,
                                        PRInt32 aIndexInContainer)
{
  NS_ASSERTION(!aOldChild->IsRootOfAnonymousSubtree(),
               "anonymous nodes should not be in child lists");
  PRUint32 selectorFlags =
    aContainer ? (aContainer->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags == 0)
    return;

  if (selectorFlags & (NODE_HAS_SLOW_SELECTOR |
                       NODE_HAS_SLOW_SELECTOR_NOAPPEND)) {
    PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    PRBool isEmpty = PR_TRUE; // :empty or :-moz-only-whitespace
    for (PRInt32 index = 0; ; ++index) {
      nsIContent *child = aContainer->GetChildAt(index);
      if (!child) // last child
        break;
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(child, PR_TRUE, PR_FALSE)) {
        isEmpty = PR_FALSE;
        break;
      }
    }
    if (isEmpty) {
      PostRestyleEvent(aContainer, eReStyle_Self, NS_STYLE_HINT_NONE);
      // Restyling the container is the most we can do here, so we're done.
      return;
    }
  }

  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the previously-first element child if it is after aOldChild
    for (PRInt32 index = 0; ; ++index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (!content)
        break; // went through all children
      if (content->IsNodeOfType(nsINode::eELEMENT)) {
        if (index >= aIndexInContainer) {
          PostRestyleEvent(content, eReStyle_Self, NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
    // restyle the previously-last element child if it is before aOldChild
    for (PRInt32 index = aContainer->GetChildCount() - 1;
         index >= 0; --index) {
      nsIContent *content = aContainer->GetChildAt(index);
      if (content->IsNodeOfType(nsINode::eELEMENT)) {
        if (index < aIndexInContainer) {
          PostRestyleEvent(content, eReStyle_Self, NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
  }
}


static PLDHashOperator
CollectRestyles(nsISupports* aContent,
                nsCSSFrameConstructor::RestyleData& aData,
                void* aRestyleArrayPtr)
{
  nsCSSFrameConstructor::RestyleEnumerateData** restyleArrayPtr =
    static_cast<nsCSSFrameConstructor::RestyleEnumerateData**>
               (aRestyleArrayPtr);
  nsCSSFrameConstructor::RestyleEnumerateData* currentRestyle =
    *restyleArrayPtr;
  currentRestyle->mContent = static_cast<nsIContent*>(aContent);
  currentRestyle->mRestyleHint = aData.mRestyleHint;
  currentRestyle->mChangeHint = aData.mChangeHint;

  // Increment to the next slot in the array
  *restyleArrayPtr = currentRestyle + 1; 

  return PL_DHASH_NEXT;
}

void
nsCSSFrameConstructor::ProcessOneRestyle(nsIContent* aContent,
                                         nsReStyleHint aRestyleHint,
                                         nsChangeHint aChangeHint)
{
  NS_PRECONDITION(aContent, "Must have content node");
  
  if (!aContent->IsInDoc() ||
      aContent->GetCurrentDoc() != mDocument) {
    // Content node has been removed from our document; nothing else
    // to do here
    return;
  }
  
  nsIFrame* primaryFrame = mPresShell->GetPrimaryFrameFor(aContent);
  if (aRestyleHint & eReStyle_Self) {
    RestyleElement(aContent, primaryFrame, aChangeHint);
  } else if (aChangeHint &&
               (primaryFrame ||
                (aChangeHint & nsChangeHint_ReconstructFrame))) {
    // Don't need to recompute style; just apply the hint
    nsStyleChangeList changeList;
    changeList.AppendChange(primaryFrame, aContent, aChangeHint);
    ProcessRestyledFrames(changeList);
  }

  if (aRestyleHint & eReStyle_LaterSiblings) {
    RestyleLaterSiblings(aContent);
  }
}

#define RESTYLE_ARRAY_STACKSIZE 128

void
nsCSSFrameConstructor::RebuildAllStyleData(nsChangeHint aExtraHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  mRebuildAllStyleData = PR_FALSE;
  NS_UpdateHint(aExtraHint, mRebuildAllExtraHint);
  mRebuildAllExtraHint = nsChangeHint(0);

  if (!mPresShell || !mPresShell->GetRootFrame())
    return;

  if (mPresShell->GetPresContext()->IsPaginated()) {
    // We don't support doing this because of bug 470929 and probably
    // other issues.
    NS_NOTREACHED("not allowed to rebuild all style data when paginated");
    return;
  }

  // Processing the style changes could cause a flush that propagates to
  // the parent frame and thus destroys the pres shell.
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(mPresShell);

  // Tell the style set to get the old rule tree out of the way
  // so we can recalculate while maintaining rule tree immutability
  nsresult rv = mPresShell->StyleSet()->BeginReconstruct();
  if (NS_FAILED(rv))
    return;

  // Recalculate all of the style contexts for the document
  // Note that we can ignore the return value of ComputeStyleChangeFor
  // because we never need to reframe the root frame
  // XXX This could be made faster by not rerunning rule matching
  // (but note that nsPresShell::SetPreferenceStyleRules currently depends
  // on us re-running rule matching here
  nsStyleChangeList changeList;
  // XXX Does it matter that we're passing aExtraHint to the real root
  // frame and not the root node's primary frame?
  mPresShell->FrameManager()->ComputeStyleChangeFor(mPresShell->GetRootFrame(),
                                                    &changeList, aExtraHint);
  // Process the required changes
  ProcessRestyledFrames(changeList);
  // Tell the style set it's safe to destroy the old rule tree.  We
  // must do this after the ProcessRestyledFrames call in case the
  // change list has frame reconstructs in it (since frames to be
  // reconstructed will still have their old style context pointers
  // until they are destroyed).
  mPresShell->StyleSet()->EndReconstruct();
}

void
nsCSSFrameConstructor::ProcessPendingRestyles()
{
  NS_PRECONDITION(mDocument, "No document?  Pshaw!\n");

  PRUint32 count = mPendingRestyles.Count();

  if (count) {
    // Use the stack if we can, otherwise fall back on heap-allocation.
    nsAutoTArray<RestyleEnumerateData, RESTYLE_ARRAY_STACKSIZE> restyleArr;
    RestyleEnumerateData* restylesToProcess = restyleArr.AppendElements(count);
  
    if (!restylesToProcess) {
      return;
    }

    RestyleEnumerateData* lastRestyle = restylesToProcess;
    mPendingRestyles.Enumerate(CollectRestyles, &lastRestyle);

    NS_ASSERTION(lastRestyle - restylesToProcess == PRInt32(count),
                 "Enumeration screwed up somehow");

    // Clear the hashtable so we don't end up trying to process a restyle we're
    // already processing, sending us into an infinite loop.
    mPendingRestyles.Clear();

    // Make sure to not rebuild quote or counter lists while we're
    // processing restyles
    BeginUpdate();

    for (RestyleEnumerateData* currentRestyle = restylesToProcess;
         currentRestyle != lastRestyle;
         ++currentRestyle) {
      ProcessOneRestyle(currentRestyle->mContent,
                        currentRestyle->mRestyleHint,
                        currentRestyle->mChangeHint);
    }

    EndUpdate();

#ifdef DEBUG
    mPresShell->VerifyStyleTree();
#endif
  }

  if (mRebuildAllStyleData) {
    // We probably wasted a lot of work up above, but this seems safest
    // and it should be rarely used.
    RebuildAllStyleData(nsChangeHint(0));
  }
}

void
nsCSSFrameConstructor::PostRestyleEvent(nsIContent* aContent,
                                        nsReStyleHint aRestyleHint,
                                        nsChangeHint aMinChangeHint)
{
  if (NS_UNLIKELY(mIsDestroyingFrameTree)) {
    NS_NOTREACHED("PostRestyleEvent after the shell is destroyed (bug 279505)");
    return;
  }

  if (aRestyleHint == 0 && !aMinChangeHint) {
    // Nothing to do here
    return;
  }

  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eELEMENT),
               "Shouldn't be trying to restyle non-elements directly");

  RestyleData existingData;
  existingData.mRestyleHint = nsReStyleHint(0);
  existingData.mChangeHint = NS_STYLE_HINT_NONE;

  mPendingRestyles.Get(aContent, &existingData);
  existingData.mRestyleHint =
    nsReStyleHint(existingData.mRestyleHint | aRestyleHint);
  NS_UpdateHint(existingData.mChangeHint, aMinChangeHint);

  mPendingRestyles.Put(aContent, existingData);

  PostRestyleEventInternal();
}
    
void
nsCSSFrameConstructor::PostRestyleEventInternal()
{
  if (!mRestyleEvent.IsPending()) {
    nsRefPtr<RestyleEvent> ev = new RestyleEvent(this);
    if (NS_FAILED(NS_DispatchToCurrentThread(ev))) {
      NS_WARNING("failed to dispatch restyle event");
      // XXXbz and what?
    } else {
      mRestyleEvent = ev;
    }
  }
}

void
nsCSSFrameConstructor::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  mRebuildAllStyleData = PR_TRUE;
  NS_UpdateHint(mRebuildAllExtraHint, aExtraHint);
  // Get a restyle event posted if necessary
  PostRestyleEventInternal();
}

NS_IMETHODIMP nsCSSFrameConstructor::RestyleEvent::Run()
{
  if (!mConstructor)
    return NS_OK;  // event was revoked

  // Make sure that any restyles that happen from now on will go into
  // a new event.
  mConstructor->mRestyleEvent.Forget();  
  
  return mConstructor->mPresShell->FlushPendingNotifications(Flush_Style);
}

NS_IMETHODIMP
nsCSSFrameConstructor::LazyGenerateChildrenEvent::Run()
{
  mPresShell->GetDocument()->FlushPendingNotifications(Flush_Layout);

  // this is hard-coded to handle only menu popup frames
  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(mContent);
  if (frame && frame->GetType() == nsGkAtoms::menuPopupFrame) {
#ifdef MOZ_XUL
    // it is possible that the frame is different than the one that requested
    // the lazy generation, but as long as it's a popup frame that hasn't
    // generated its children yet, that's OK.
    nsMenuPopupFrame* menuPopupFrame = static_cast<nsMenuPopupFrame *>(frame);
    if (menuPopupFrame->HasGeneratedChildren()) {
      if (mCallback)
        mCallback(mContent, frame, mArg);
      
      return NS_OK;
    }     

    // indicate that the children have been generated
    menuPopupFrame->SetGeneratedChildren();
#endif

    nsCSSFrameConstructor* fc = mPresShell->FrameConstructor();
    fc->BeginUpdate();

    nsFrameItems childItems;
    nsFrameConstructorState state(mPresShell, nsnull, nsnull, nsnull);
    nsresult rv = fc->ProcessChildren(state, mContent, frame, PR_FALSE,
                                      childItems, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;

    fc->CreateAnonymousFrames(mContent->Tag(), state, mContent, frame,
                              PR_FALSE, childItems);
    frame->SetInitialChildList(nsnull, childItems.childList);

    fc->EndUpdate();

    if (mCallback)
      mCallback(mContent, frame, mArg);

    // call XBL constructors after the frames are created
    mPresShell->GetDocument()->BindingManager()->ProcessAttachedQueue();
  }

  return NS_OK;
}
