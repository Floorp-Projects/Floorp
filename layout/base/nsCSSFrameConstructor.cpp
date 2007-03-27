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

#include "nsIDOMWindowInternal.h"
#include "nsIMenuFrame.h"

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

nsIFrame*
NS_NewHTMLCanvasFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

#ifdef MOZ_SVG
#include "nsISVGTextContentMetrics.h"
#include "nsStyleUtil.h"

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
#ifdef MOZ_SVG_FOREIGNOBJECT
nsIFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
#endif
nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame* parent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
PRBool 
NS_SVG_TestFeatures (const nsAString& value);
PRBool 
NS_SVG_TestsSupported (const nsIAtom *atom);
PRBool 
NS_SVG_LangSupported (const nsIAtom *atom);
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

nsIFrame*
NS_NewNativeScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);


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

static inline nsIFrame*
GetFieldSetAreaFrame(nsIFrame* aFieldsetFrame)
{
  // Depends on the fieldset child frame order - see ConstructFieldSetFrame() below.
  nsIFrame* firstChild = aFieldsetFrame->GetFirstChild(nsnull);
  return firstChild && firstChild->GetNextSibling() ? firstChild->GetNextSibling() : firstChild;
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

static void
GetSpecialSibling(nsFrameManager* aFrameManager, nsIFrame* aFrame, nsIFrame** aResult)
{
  // We only store the "special sibling" annotation with the first
  // frame in the flow. Walk back to find that frame now.
  aFrame = aFrame->GetFirstInFlow();

  void* value = aFrame->GetProperty(nsGkAtoms::IBSplitSpecialSibling);

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
IsBlockFrame(nsIFrame* aFrame)
{
  // XXXwaterson this seems wrong; see IsInlineFrame() immediately
  // above, which will treat inline-block (e.g.) as an inline. Why
  // don't we use display->IsBlockLevel() here?
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  return NS_STYLE_DISPLAY_INLINE != display->mDisplay;
}

static nsIFrame*
FindFirstBlock(nsIFrame* aKid, nsIFrame** aPrevKid)
{
  nsIFrame* prevKid = nsnull;
  while (aKid) {
    if (IsBlockFrame(aKid)) {
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
    if (IsBlockFrame(aKid)) {
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
                         nsIFrame *aAnonymousFrame,
                         nsIFrame *aSpecialParent)
{
  aPresContext->PropertyTable()->SetProperty(aAnonymousFrame,
                                      nsGkAtoms::IBSplitSpecialPrevSibling,
                                             aSpecialParent, nsnull, nsnull);
}

// -----------------------------------------------------------

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
    nsPlaceholderFrame* placeholder = NS_STATIC_CAST(nsPlaceholderFrame*,
                                                     aFrameIn);
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

  // Remove the frame from the list, return PR_FALSE if not found.
  PRBool RemoveChild(nsIFrame* aChild);
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
nsFrameItems::RemoveChild(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");
  nsIFrame* prev = nsnull;
  nsIFrame* sib = childList;
  for (; sib && sib != aFrame; sib = sib->GetNextSibling()) {
    prev = sib;
  }
  if (!sib) {
    return PR_FALSE;
  }
  if (sib == childList) {
    childList = sib->GetNextSibling();
  } else {
    prev->SetNextSibling(sib->GetNextSibling());
  }
  if (sib == lastChild) {
    lastChild = prev;
  }
  sib->SetNextSibling(nsnull);
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
  NS_ASSERTION(aChild->GetPresContext()->FrameManager()->
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
  printf("        %p\n", NS_STATIC_CAST(void*, mFrame));
  main = mChildList.childList;

 
  second = mChildList2.childList;
  while (main || second) {
    printf("          %p   %p\n", NS_STATIC_CAST(void*, main),
           NS_STATIC_CAST(void*, second));
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

  nsAbsoluteItems  mSavedItems;           // copy of original data
  PRBool           mSavedFirstLetterStyle;
  PRBool           mSavedFirstLineStyle;

  // The name of the child list in which our frames would belong
  nsIAtom* mChildListName;
  nsFrameConstructorState* mState;

  friend class nsFrameConstructorState;
};

// Structure used for maintaining state information during the
// frame construction process
class nsFrameConstructorState {
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
  nsCOMPtr<nsILayoutHistoryState> mFrameState;
  nsPseudoFrames            mPseudoFrames;

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
   * @param aStyleDisplay the display struct for aNewFrame
   * @param aContent the content pointer for aNewFrame
   * @param aStyleContext the style context of aNewFrame
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
                    const nsStyleDisplay* aStyleDisplay,
                    nsIContent* aContent,
                    nsStyleContext* aStyleContext,
                    nsIFrame* aParentFrame,
                    PRBool aCanBePositioned = PR_TRUE,
                    PRBool aCanBeFloated = PR_TRUE,
                    PRBool aIsOutOfFlowPopup = PR_FALSE,
                    PRBool aInsertAfter = PR_FALSE,
                    nsIFrame* aInsertAfterFrame = nsnull);

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
    mFrameState(aHistoryState),
    mPseudoFrames()
{
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
    mPseudoFrames()
{
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
  ProcessFrameInsertions(mFloatedItems, nsGkAtoms::floatList);
  ProcessFrameInsertions(mAbsoluteItems, nsGkAtoms::absoluteList);
  ProcessFrameInsertions(mFixedItems, nsGkAtoms::fixedList);
#ifdef MOZ_XUL
  ProcessFrameInsertions(mPopupItems, nsGkAtoms::popupList);
#endif
}

static nsIFrame*
AdjustAbsoluteContainingBlock(nsPresContext* aPresContext,
                              nsIFrame*       aContainingBlockIn)
{
  if (!aContainingBlockIn) {
    return nsnull;
  }
  
  // Always use the container's first in flow.
  return aContainingBlockIn->GetFirstInFlow();
}

void
nsFrameConstructorState::PushAbsoluteContainingBlock(nsIFrame* aNewAbsoluteContainingBlock,
                                                     nsFrameConstructorSaveState& aSaveState)
{
  aSaveState.mItems = &mAbsoluteItems;
  aSaveState.mSavedItems = mAbsoluteItems;
  aSaveState.mChildListName = nsGkAtoms::absoluteList;
  aSaveState.mState = this;
  mAbsoluteItems = 
    nsAbsoluteItems(AdjustAbsoluteContainingBlock(mPresContext,
                                                  aNewAbsoluteContainingBlock));
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
      mFixedItems.containingBlock) {
    return mFixedItems.containingBlock;
  }

  return aContentParentFrame;
}

nsresult
nsFrameConstructorState::AddChild(nsIFrame* aNewFrame,
                                  nsFrameItems& aFrameItems,
                                  const nsStyleDisplay* aStyleDisplay,
                                  nsIContent* aContent,
                                  nsStyleContext* aStyleContext,
                                  nsIFrame* aParentFrame,
                                  PRBool aCanBePositioned,
                                  PRBool aCanBeFloated,
                                  PRBool aIsOutOfFlowPopup,
                                  PRBool aInsertAfter,
                                  nsIFrame* aInsertAfterFrame)
{
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
  if (aCanBeFloated && aStyleDisplay->IsFloating() &&
      mFloatedItems.containingBlock) {
    NS_ASSERTION(aNewFrame->GetParent() == mFloatedItems.containingBlock,
                 "Float whose parent is not the float containing block?");
    needPlaceholder = PR_TRUE;
    frameItems = &mFloatedItems;
  }
  else if (aCanBePositioned) {
    if (aStyleDisplay->mPosition == NS_STYLE_POSITION_ABSOLUTE &&
        mAbsoluteItems.containingBlock) {
      NS_ASSERTION(aNewFrame->GetParent() == mAbsoluteItems.containingBlock,
                   "Abs pos whose parent is not the abs pos containing block?");
      needPlaceholder = PR_TRUE;
      frameItems = &mAbsoluteItems;
    }
    if (aStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED &&
        mFixedItems.containingBlock) {
      NS_ASSERTION(aNewFrame->GetParent() == mFixedItems.containingBlock,
                   "Fixed pos whose parent is not the fixed pos containing block?");
      needPlaceholder = PR_TRUE;
      frameItems = &mFixedItems;
    }
  }

  if (needPlaceholder) {
    NS_ASSERTION(frameItems != &aFrameItems,
                 "Putting frame in-flow _and_ want a placeholder?");
    nsIFrame* placeholderFrame;
    nsresult rv =
      nsCSSFrameConstructor::CreatePlaceholderFrameFor(mPresShell,
                                                       mPresContext,
                                                       mFrameManager,
                                                       aContent,
                                                       aNewFrame,
                                                       aStyleContext,
                                                       aParentFrame,
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
    GetSpecialSibling(mFrameManager, specialSibling, &specialSibling);
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
    mSavedItems(nsnull),
    mSavedFirstLetterStyle(PR_FALSE),
    mSavedFirstLineStyle(PR_FALSE),
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

      if (aOuterState.mFloatedItems.RemoveChild(outOfFlowFrame)) {
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
 * Moves frames to a new parent, updating the style context and
 * propagating relevant frame state bits. |aNewParentSC| may be null,
 * in which case the child frames' style contexts will remain
 * untouched. |aState| may be null, in which case the parent
 * pointers of out-of-flow frames will remain untouched.
 */
static void
MoveChildrenTo(nsFrameManager*          aFrameManager,
               nsStyleContext*          aNewParentSC,
               nsIFrame*                aNewParent,
               nsIFrame*                aFrameList,
               nsFrameConstructorState* aState,
               nsFrameConstructorState* aOuterState)
{
  PRBool setHasChildWithView = PR_FALSE;

  while (aFrameList) {
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


// Structure used to ensure that bindings are properly enqueued in the
// binding manager's attached queue.
struct nsAutoEnqueueBinding
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
  
  // See if the frame is moved out of the flow
  if (aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStyleDisplay* disp = aChildFrame->GetStyleDisplay();
    
    if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
      listName = nsGkAtoms::absoluteList;
    } else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
      listName = nsGkAtoms::fixedList;
#ifdef MOZ_XUL
    } else if (NS_STYLE_DISPLAY_POPUP == disp->mDisplay) {
      // Out-of-flows that are DISPLAY_POPUP must be kids of the root popup set
#ifdef DEBUG
      nsIFrame* parent = aChildFrame->GetParent();
      if (parent) {
        nsIPopupSetFrame* popupSet;
        CallQueryInterface(parent, &popupSet);
        NS_ASSERTION(popupSet, "Unexpected parent");
      }
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
  , mFixedContainingBlock(nsnull)
  , mDocElementContainingBlock(nsnull)
  , mGfxScrollFrame(nsnull)
  , mPageSequenceFrame(nsnull)
  , mUpdateCount(0)
  , mQuotesDirty(PR_FALSE)
  , mCountersDirty(PR_FALSE)
  , mInitialContainingBlockIsAbsPosContainer(PR_FALSE)
  , mIsDestroyingFrameTree(PR_FALSE)
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

nsresult
nsCSSFrameConstructor::CreateAttributeContent(nsIContent* aParentContent,
                                              nsIFrame* aParentFrame,
                                              PRInt32 aAttrNamespace,
                                              nsIAtom* aAttrName,
                                              nsStyleContext* aStyleContext,
                                              nsIContent** aNewContent,
                                              nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  *aNewContent = nsnull;
  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewAttributeContent(mDocument->NodeInfoManager(),
                                       aAttrNamespace, aAttrName,
                                       getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set aContent as the parent content so that event handling works.
  rv = content->BindToTree(mDocument, aParentContent, content, PR_TRUE);
  if (NS_FAILED(rv)) {
    content->UnbindFromTree();
    return rv;
  }

  content->SetNativeAnonymous(PR_TRUE);

  // Create a text frame and initialize it
  nsIFrame* textFrame = NS_NewTextFrame(mPresShell, aStyleContext);
  rv = textFrame->Init(content, aParentFrame, nsnull);
  if (NS_FAILED(rv)) {
    content->UnbindFromTree();
    textFrame->Destroy();
    textFrame = nsnull;
    content = nsnull;
  }

  *aNewFrame = textFrame;
  content.swap(*aNewContent);
  return rv;
}

nsresult
nsCSSFrameConstructor::CreateGeneratedFrameFor(nsIFrame*             aParentFrame,
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

  nsCOMPtr<nsIContent> content;

  if (eStyleContentType_Image == type) {
    if (!data.mContent.mImage) {
      // CSS had something specified that couldn't be converted to an
      // image object
      return NS_ERROR_FAILURE;
    }
    
    // Create an image content object and pass it the image request.
    // XXX Check if it's an image type we can handle...

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mDocument->NodeInfoManager()->GetNodeInfo(nsGkAtoms::img, nsnull,
                                              kNameSpaceID_None,
                                              getter_AddRefs(nodeInfo));

    nsresult rv = NS_NewGenConImageContent(getter_AddRefs(content), nodeInfo,
                                           data.mContent.mImage);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set aContent as the parent content and set the document object. This
    // way event handling works
    // Hack the binding parent to make document rules not match (not
    // like it matters, since we already have a non-element style
    // context... which is totally wacky, but anyway).
    rv = content->BindToTree(mDocument, aContent, content, PR_TRUE);
    if (NS_FAILED(rv)) {
      content->UnbindFromTree();
      return rv;
    }
    
    content->SetNativeAnonymous(PR_TRUE);
  
    // Create an image frame and initialize it
    nsIFrame* imageFrame = NS_NewImageFrame(mPresShell, aStyleContext);
    if (NS_UNLIKELY(!imageFrame)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = imageFrame->Init(content, aParentFrame, nsnull);
    if (NS_FAILED(rv)) {
      imageFrame->Destroy();
      return rv == NS_ERROR_FRAME_REPLACED ? NS_OK : rv;
    }

    // Return the image frame
    *aFrame = imageFrame;

  } else {

    nsAutoString contentString;

    switch (type) {
    case eStyleContentType_String:
      contentString = data.mContent.mString;
      break;
  
    case eStyleContentType_Attr:
      {
        nsCOMPtr<nsIAtom> attrName;
        PRInt32 attrNameSpace = kNameSpaceID_None;
        contentString = data.mContent.mString;
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

        nsresult rv =
          CreateAttributeContent(aContent, aParentFrame, attrNameSpace,
                                 attrName, aStyleContext,
                                 getter_AddRefs(content), aFrame);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
  
    case eStyleContentType_Counter:
    case eStyleContentType_Counters:
      {
        nsCSSValue::Array *counters = data.mContent.mCounters;
        nsCounterList *counterList = mCounterManager.CounterListFor(
            nsDependentString(counters->Item(0).GetStringBufferValue()));
        if (!counterList)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCounterUseNode* node =
          new nsCounterUseNode(counters, aParentFrame, aContentIndex,
                               type == eStyleContentType_Counters);
        if (!node)
          return NS_ERROR_OUT_OF_MEMORY;

        counterList->Insert(node);
        PRBool dirty = counterList->IsDirty();
        if (!dirty) {
          if (counterList->IsLast(node)) {
            node->Calc(counterList);
            node->GetText(contentString);
          }
          // In all other cases (list already dirty or node not at the end),
          // just start with an empty string for now and when we recalculate
          // the list we'll change the value to the right one.
          else {
            counterList->SetDirty();
            CountersDirty();
          }
        }

        textPtr = &node->mText; // text node assigned below
      }
      break;

    case eStyleContentType_Image:
      NS_NOTREACHED("handled by if above");
      return NS_ERROR_UNEXPECTED;
  
    case eStyleContentType_OpenQuote:
    case eStyleContentType_CloseQuote:
    case eStyleContentType_NoOpenQuote:
    case eStyleContentType_NoCloseQuote:
      {
        nsQuoteNode* node = new nsQuoteNode(type, aParentFrame, aContentIndex);
        if (!node)
          return NS_ERROR_OUT_OF_MEMORY;
        mQuoteList.Insert(node);
        if (mQuoteList.IsLast(node))
          mQuoteList.Calc(node);
        else
          QuotesDirty();

        // Don't generate a text node or any text for 'no-open-quote' and
        // 'no-close-quote'.
        if (node->IsHiddenQuote())
          return NS_OK;

        textPtr = &node->mText; // text node assigned below
        contentString = *node->Text();
      }
      break;
  
    case eStyleContentType_AltContent:
      {
        // Use the "alt" attribute; if that fails and the node is an HTML
        // <input>, try the value attribute and then fall back to some default
        // localized text we have.
        nsresult rv = NS_OK;
        if (aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::alt)) {
          rv = CreateAttributeContent(aContent, aParentFrame,
                                      kNameSpaceID_None, nsGkAtoms::alt,
                                      aStyleContext, getter_AddRefs(content),
                                      aFrame);
        } else if (aContent->IsNodeOfType(nsINode::eHTML) &&
                   aContent->NodeInfo()->Equals(nsGkAtoms::input)) {
          if (aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::value)) {
            rv = CreateAttributeContent(aContent, aParentFrame,
                                        kNameSpaceID_None, nsGkAtoms::value,
                                        aStyleContext, getter_AddRefs(content),
                                        aFrame);
          } else {
            nsXPIDLString temp;
            rv = nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                                    "Submit", temp);
            contentString = temp;
          }
        } else {
          *aFrame = nsnull;
          rv = NS_ERROR_NOT_AVAILABLE;
          return rv; // Don't fall through to the warning below.
        }
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    } // switch
  

    if (!content) {
      // Create a text content node
      nsIFrame* textFrame = nsnull;
      nsCOMPtr<nsIContent> textContent;
      NS_NewTextNode(getter_AddRefs(textContent),
                     mDocument->NodeInfoManager());
      if (textContent) {
        // Set the text
        textContent->SetText(contentString, PR_TRUE);

        if (textPtr) {
          *textPtr = do_QueryInterface(textContent);
          NS_ASSERTION(*textPtr, "must implement nsIDOMCharacterData");
        }

        // Set aContent as the parent content so that event handling works.
        nsresult rv = textContent->BindToTree(mDocument, aContent, textContent,
                                              PR_TRUE);
        if (NS_FAILED(rv)) {
          textContent->UnbindFromTree();
          return rv;
        }

        textContent->SetNativeAnonymous(PR_TRUE);

        // Create a text frame and initialize it
        textFrame = NS_NewTextFrame(mPresShell, aStyleContext);
        if (!textFrame) {
          // XXX The quotes/counters code doesn't like the text pointer
          // being null in case of dynamic changes!
          NS_NOTREACHED("this OOM case isn't handled very well");
          return NS_ERROR_OUT_OF_MEMORY;
        }

        textFrame->Init(textContent, aParentFrame, nsnull);

        content = textContent;
      } else {
        // XXX The quotes/counters code doesn't like the text pointer
        // being null in case of dynamic changes!
        NS_NOTREACHED("this OOM case isn't handled very well");
      }

      // Return the text frame
      *aFrame = textFrame;
    }
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
nsCSSFrameConstructor::CreateGeneratedContentFrame(nsFrameConstructorState& aState,
                                                   nsIFrame*        aFrame,
                                                   nsIContent*      aContent,
                                                   nsStyleContext*  aStyleContext,
                                                   nsIAtom*         aPseudoElement,
                                                   nsIFrame**       aResult)
{
  *aResult = nsnull; // initialize OUT parameter

  if (!aContent->IsNodeOfType(nsINode::eELEMENT))
    return PR_FALSE;

  nsStyleSet *styleSet = mPresShell->StyleSet();

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

    const PRUint8 disp = pseudoStyleContext->GetStyleDisplay()->mDisplay;
    if (disp == NS_STYLE_DISPLAY_BLOCK ||
        disp == NS_STYLE_DISPLAY_INLINE_BLOCK) {
      PRUint32 flags = 0;
      if (disp == NS_STYLE_DISPLAY_INLINE_BLOCK) {
        flags = NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT;
      }
      containerFrame = NS_NewBlockFrame(mPresShell, pseudoStyleContext, flags);
    } else {
      containerFrame = NS_NewInlineFrame(mPresShell, pseudoStyleContext);
    }        
    InitAndRestoreFrame(aState, aContent, aFrame, nsnull, containerFrame);
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
      result = CreateGeneratedFrameFor(containerFrame,
                                       aContent, textStyleContext,
                                       styleContent, contentIndex, &frame);
      // Non-elements can't possibly have a view, so don't bother checking
      if (NS_SUCCEEDED(result) && frame) {
        // Add it to the list of child frames
        childFrames.AddChild(frame);
      }
    }

    if (childFrames.childList) {
      containerFrame->SetInitialChildList(nsnull, childFrames.childList);
    }
    *aResult = containerFrame;
    return PR_TRUE;
  }

  return PR_FALSE;
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
                                        nsFrameItems&            aFrameItems)
{
  // Make sure to keep IsSpecialContent in synch with this code
  
  // Note: do not do anything in this method that assumes pseudo-frames have
  // been processed.  If you feel the urge to do something like that, fix
  // callers accordingly.
  nsCOMPtr<nsIFormControl> control = do_QueryInterface(aContent);
  NS_ASSERTION(control, "input is not an nsIFormControl!");

  switch (control->GetType()) {
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_BUTTON:
    {
      if (gUseXBLForms)
        return NS_OK; // upddate IsSpecialContent if this becomes functional

      nsresult rv = ConstructButtonFrame(aState, aContent, aParentFrame,
                                         aTag, aStyleContext, aFrame,
                                         aStyleDisplay, aFrameItems);
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
           NS_STATIC_CAST(void*, highestFrame));
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
           NS_STATIC_CAST(void*, highestFrame));
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
                           PR_TRUE, items, PR_TRUE, pseudoOuter.mFrame, 
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

    if (aTag == nsGkAtoms::img) {
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
      aTag == nsGkAtoms::canvas;
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
  #ifndef XP_MACOSX
      // keep this in sync  with ConstructXULFrame especially for the MAC
      aTag == nsGkAtoms::menubar ||
  #endif
      aTag == nsGkAtoms::popupgroup ||
      aTag == nsGkAtoms::iframe ||
      aTag == nsGkAtoms::editor ||
      aTag == nsGkAtoms::browser ||
      aTag == nsGkAtoms::progressmeter ||
#endif
      aTag == nsGkAtoms::slider ||
      aTag == nsGkAtoms::scrollbar ||
      aTag == nsGkAtoms::nativescrollbar ||
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
      aTag == nsGkAtoms::mprescripts_;
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
  if (parentType == nsGkAtoms::tableOuterFrame) {
    childIsSpecialContent = IsSpecialContent(aChildContent, aTag, aNameSpaceID,
                                             aChildStyle);
    if (childIsSpecialContent ||
       (aChildStyle->GetStyleDisplay()->mDisplay !=
       NS_STYLE_DISPLAY_TABLE_CAPTION)) {
      aParentFrame = aParentFrame->GetContentInsertionFrame();
    }
  }
  else if (parentType == nsGkAtoms::tableColGroupFrame) {
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
    nsresult rv = GetPseudoCellFrame(aNameSpaceID, aState, *aParentFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }

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
                                           PRBool                   aAllowOutOfFlow,
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

  const nsStyleDisplay* disp = outerStyleContext->GetStyleDisplay();
  // We need the aAllowOutOfFlow thing for MathML.  See bug 355993.
  // Once bug 348577 is fixed, we should remove this code.  At that
  // point, the aAllowOutOfFlow arg can go away.
  nsIFrame* geometricParent =
    aAllowOutOfFlow ? aState.GetGeometricParent(disp, parentFrame) :
                      parentFrame;

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

    rv = aState.AddChild(aNewOuterFrame, *frameItems, disp, aContent,
                         outerStyleContext, parentFrame, aAllowOutOfFlow,
                         aAllowOutOfFlow);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewInnerFrame, PR_FALSE, childItems,
                         PR_FALSE);
    // XXXbz what about cleaning up?
    if (NS_FAILED(rv)) return rv;

    // if there are any anonymous children for the table, create frames for them
    CreateAnonymousFrames(nsnull, aState, aContent, aNewInnerFrame,
                          PR_FALSE, childItems);

    nsFrameItems captionItems;
    nsIFrame *child = childItems.childList;
    while (child) {
      nsIFrame *nextSibling = child->GetNextSibling();
      if (nsGkAtoms::tableCaptionFrame == child->GetType()) {
        childItems.RemoveChild(child);
        captionItems.AddChild(child);
      }
      child = nextSibling;
    }
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
  HaveSpecialBlockStyle(aContent, aStyleContext,
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
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_FALSE, childItems,
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
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_FALSE, childItems,
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
    rv = ProcessChildren(aState, aContent, aNewFrame, PR_FALSE, childItems,
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

  aNewFrame = NS_NewTableColFrame(mPresShell, aStyleContext);
  if (NS_UNLIKELY(!aNewFrame)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, aNewFrame);

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
      nsIFrame* newCol = NS_NewTableColFrame(mPresShell, styleContext);
      if (NS_UNLIKELY(!newCol)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      InitAndRestoreFrame(aState, aContent, parentFrame, nsnull, newCol);
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
#ifdef MOZ_MATHML
  if (kNameSpaceID_MathML == aNameSpaceID)
    aNewCellInnerFrame = NS_NewMathMLmtdInnerFrame(mPresShell, innerPseudoStyle);
  else
#endif
    aNewCellInnerFrame = NS_NewTableCellInnerFrame(mPresShell, innerPseudoStyle);


  if (NS_UNLIKELY(!aNewCellInnerFrame)) {
    aNewCellOuterFrame->Destroy();
    aNewCellOuterFrame = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  InitAndRestoreFrame(aState, aContent, aNewCellOuterFrame, nsnull, aNewCellInnerFrame);

  if (!aIsPseudo) {
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);

    // The block frame is a float container
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(aNewCellInnerFrame, floatSaveState,
                                    haveFirstLetterStyle, haveFirstLineStyle);

    // Process the child content
    nsFrameItems childItems;
    rv = ProcessChildren(aState, aContent, aNewCellInnerFrame, 
                         PR_TRUE, childItems, PR_TRUE);

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
  // don't create a whitespace frame if aParentFrame doesn't want it
  if ((NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE & aParentFrame->GetStateBits())
      && TextIsOnlyWhitespace(aChildContent)) {
    return PR_FALSE;
  }
  return PR_TRUE;
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
nsCSSFrameConstructor::ConstructDocElementTableFrame(nsIContent*     aDocElement,
                                                     nsIFrame*       aParentFrame,
                                                     nsIFrame**      aNewTableFrame,
                                                     nsFrameConstructorState& aState)
{
  nsFrameItems    frameItems;

  // XXXbz this is wrong.  We should at least be setting the fixed container in
  // the framestate here.  Better yet, we should pass through aState
  // unmodified.  Can't do that, though, because then a fixed or absolute
  // positioned root table with auto offsets would look for a block to compute
  // its hypothetical box and crash.  So we just disable fixed positioning
  // altogether in documents where the root is a table.  Oh, well.
  nsFrameConstructorState state(mPresShell, nsnull, nsnull, nsnull,
                                aState.mFrameState);
  ConstructFrame(state, aDocElement, aParentFrame, frameItems);
  *aNewTableFrame = frameItems.childList;
  if (!*aNewTableFrame) {
    NS_WARNING("cannot get table contentFrame");
    // XXXbz maybe better to return the error from ConstructFrame?
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
    // how the root frame hierarchy should look

    /*

---------------No Scrollbars------


     AreaFrame or BoxFrame (InitialContainingBlock)
  


---------------Gfx Scrollbars ------


     ScrollFrame

         ^
         |
         |
     AreaFrame or BoxFrame (InitialContainingBlock)
          
*/    

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
    rv = xblService->LoadBindings(aDocElement, display->mBinding, PR_FALSE,
                                  getter_AddRefs(binding), &resolveStyle);
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

  nsIFrame* contentFrame = nsnull;
  PRBool isBlockFrame = PR_FALSE;
  nsresult rv;

  // The rules from CSS 2.1, section 9.2.4, have already been applied
  // by the style system, so we can assume that display->mDisplay is
  // either NONE, BLOCK, or TABLE.

  PRBool docElemIsTable = (display->mDisplay == NS_STYLE_DISPLAY_TABLE) &&
                          !IsSpecialContent(aDocElement, aDocElement->Tag(),
                                            aDocElement->GetNameSpaceID(),
                                            styleContext);

  if (docElemIsTable) {
    // if the document is a table then just populate it.
    rv = ConstructDocElementTableFrame(aDocElement, aParentFrame, &contentFrame,
                                       aState);
    if (NS_FAILED(rv)) {
      return rv;
    }
    styleContext = contentFrame->GetStyleContext();
  } else {
    // otherwise build a box or a block
#ifdef MOZ_XUL
    if (aDocElement->IsNodeOfType(nsINode::eXUL)) {
      contentFrame = NS_NewDocElementBoxFrame(mPresShell, styleContext);
    }
    else
#endif 
#ifdef MOZ_SVG
    if (aDocElement->GetNameSpaceID() == kNameSpaceID_SVG && NS_SVGEnabled()) {
      contentFrame = NS_NewSVGOuterSVGFrame(mPresShell, aDocElement, styleContext);
    }
    else 
#endif
    {
      contentFrame = NS_NewDocumentElementFrame(mPresShell, styleContext);
      isBlockFrame = PR_TRUE;
    }
    
    if (NS_UNLIKELY(!contentFrame)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // initialize the child
    InitAndRestoreFrame(aState, aDocElement, aParentFrame, nsnull, contentFrame);
  }

  // set the primary frame
  aState.mFrameManager->SetPrimaryFrameFor(aDocElement, contentFrame);

  *aNewFrame = contentFrame;

  mInitialContainingBlock = contentFrame;
  mInitialContainingBlockIsAbsPosContainer = PR_FALSE;

  // if it was a table then we don't need to process our children.
  if (!docElemIsTable) {
    // Process the child content
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameConstructorSaveState floatSaveState;
    nsFrameItems                childItems;

    if (isBlockFrame) {
      PRBool haveFirstLetterStyle, haveFirstLineStyle;
      HaveSpecialBlockStyle(aDocElement, styleContext,
                            &haveFirstLetterStyle, &haveFirstLineStyle);
      mInitialContainingBlockIsAbsPosContainer = PR_TRUE;
      aState.PushAbsoluteContainingBlock(contentFrame, absoluteSaveState);
      aState.PushFloatContainingBlock(contentFrame, floatSaveState,
                                      haveFirstLetterStyle,
                                      haveFirstLineStyle);
    }

    // Create any anonymous frames the doc element frame requires
    // This must happen before ProcessChildren to ensure that popups are
    // never constructed before the popupset.
    CreateAnonymousFrames(nsnull, aState, aDocElement, contentFrame,
                          PR_FALSE, childItems, PR_TRUE);
    ProcessChildren(aState, aDocElement, contentFrame, PR_TRUE, childItems,
                    isBlockFrame);

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
     ScrollFrame

         ^
         |
     RootFrame(DocElementContainingBlock)
          
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
    nsIFrame *pageFrame, *pageContentFrame;
    ConstructPageFrame(mPresShell, presContext, rootFrame, nsnull,
                       pageFrame, pageContentFrame);
    rootFrame->SetInitialChildList(nsnull, pageFrame);

    // The eventual parent of the document element frame.
    // XXX should this be set for every new page (in ConstructPageFrame)?
    mDocElementContainingBlock = pageContentFrame;
  }

  viewportFrame->SetInitialChildList(nsnull, newFrame);
  
  *aNewFrame = viewportFrame;

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

  aPageContentFrame = NS_NewPageContentFrame(aPresShell, pageContentPseudoStyle);
  if (NS_UNLIKELY(!aPageContentFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  // Initialize the page content frame and force it to have a view. Also make it the
  // containing block for fixed elements which are repeated on every page.
  aPageContentFrame->Init(nsnull, aPageFrame, nsnull);
  mFixedContainingBlock = aPageContentFrame;

  aPageFrame->SetInitialChildList(nsnull, aPageContentFrame);

  // Fixed pos kids are taken care of directly in CreateContinuingFrame()

  return NS_OK;
}

/* static */
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
  nsRefPtr<nsStyleContext> placeholderStyle = aPresShell->StyleSet()->
    ResolveStyleForNonElement(aStyleContext->GetParent());
  
  // The placeholder frame gets a pseudo style context
  nsPlaceholderFrame* placeholderFrame =
    (nsPlaceholderFrame*)NS_NewPlaceholderFrame(aPresShell, placeholderStyle);

  if (placeholderFrame) {
    placeholderFrame->Init(aContent, aParentFrame, nsnull);
  
    // The placeholder frame has a pointer back to the out-of-flow frame
    placeholderFrame->SetOutOfFlowFrame(aFrame);
  
    aFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);

    // Add mapping from absolutely positioned frame to its placeholder frame
    aFrameManager->RegisterPlaceholderFrame(placeholderFrame);

    *aPlaceholderFrame = NS_STATIC_CAST(nsIFrame*, placeholderFrame);
    
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
                                            nsFrameItems&            aFrameItems)
{
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
                                                               
  nsIFrame* areaFrame = NS_NewAreaFrame(mPresShell, styleContext,
                                        NS_BLOCK_SPACE_MGR);

  if (NS_UNLIKELY(!areaFrame)) {
    buttonFrame->Destroy();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = InitAndRestoreFrame(aState, aContent, buttonFrame, nsnull, areaFrame);
  if (NS_FAILED(rv)) {
    areaFrame->Destroy();
    buttonFrame->Destroy();
    return rv;
  }

  rv = aState.AddChild(buttonFrame, aFrameItems, aStyleDisplay, aContent,
                                aStyleContext, aParentFrame);
  if (NS_FAILED(rv)) {
    areaFrame->Destroy();
    buttonFrame->Destroy();
    return rv;
  }

  
  if (!buttonFrame->IsLeaf()) { 
    // input type="button" have only anonymous content
    // The area frame is a float container
    PRBool haveFirstLetterStyle, haveFirstLineStyle;
    HaveSpecialBlockStyle(aContent, aStyleContext,
                          &haveFirstLetterStyle, &haveFirstLineStyle);
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(areaFrame, floatSaveState,
                                    haveFirstLetterStyle,
                                    haveFirstLineStyle);

    // Process children
    nsFrameConstructorSaveState absoluteSaveState;
    nsFrameItems                childItems;

    if (aStyleDisplay->IsPositioned()) {
      // The area frame becomes a container for child frames that are
      // absolutely positioned
      aState.PushAbsoluteContainingBlock(areaFrame, absoluteSaveState);
    }

    rv = ProcessChildren(aState, aContent, areaFrame, PR_TRUE, childItems,
                         buttonFrame->GetStyleDisplay()->IsBlockLevel());
    if (NS_FAILED(rv)) return rv;
  
    // Set the areas frame's initial child lists
    areaFrame->SetInitialChildList(nsnull, childItems.childList);
  }

  buttonFrame->SetInitialChildList(nsnull, areaFrame);

  nsFrameItems  anonymousChildItems;
  // if there are any anonymous children create frames for them
  CreateAnonymousFrames(aTag, aState, aContent, buttonFrame,
                          PR_FALSE, anonymousChildItems);
  if (anonymousChildItems.childList) {
    // the anonymous content is already parented to the area frame
    aState.mFrameManager->AppendFrames(areaFrame, nsnull, anonymousChildItems.childList);
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

      rv = aState.AddChild(comboboxFrame, aFrameItems, aStyleDisplay,
                           aContent, aStyleContext, aParentFrame);
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
      comboboxFrame->SetInitialChildList(nsGkAtoms::popupList,
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
    nsresult rv = aState.AddChild(scrollFrame, aFrameItems, display,
                                  aContent, aStyleContext, aParentFrame);
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
  HaveSpecialBlockStyle(aContent, aStyleContext,
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
  
  nsIFrame* areaFrame = NS_NewAreaFrame(mPresShell, styleContext,
                                     NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);
  InitAndRestoreFrame(aState, aContent, newFrame, nsnull, areaFrame);

  nsresult rv = aState.AddChild(newFrame, aFrameItems, aStyleDisplay, aContent,
                                aStyleContext, aParentFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }
  

  // The area frame is a float container
  PRBool haveFirstLetterStyle, haveFirstLineStyle;
  HaveSpecialBlockStyle(aContent, aStyleContext,
                        &haveFirstLetterStyle, &haveFirstLineStyle);
  nsFrameConstructorSaveState floatSaveState;
  aState.PushFloatContainingBlock(areaFrame, floatSaveState,
                                  haveFirstLetterStyle,
                                  haveFirstLineStyle);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;

  if (aStyleDisplay->IsPositioned()) {
    // The area frame becomes a container for child frames that are
    // absolutely positioned
    aState.PushAbsoluteContainingBlock(areaFrame, absoluteSaveState);
  }

  ProcessChildren(aState, aContent, areaFrame, PR_TRUE,
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
      legendFrame->SetNextSibling(areaFrame);
      legendFrame->SetParent(newFrame);
      break;
    }
    previous = child;
    child = child->GetNextSibling();
  }

  // Set the scrolled frame's initial child lists
  areaFrame->SetInitialChildList(nsnull, childItems.childList);

  // Set the scroll frame's initial child list
  newFrame->SetInitialChildList(nsnull, legendFrame ? legendFrame : areaFrame);

  // our new frame returned is the top frame which is the list frame. 
  aNewFrame = newFrame; 

  // yes we have already initialized our frame 
  aFrameHasBeenInitialized = PR_TRUE; 

  return NS_OK;
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
  else {
    newFrame = NS_NewTextFrame(mPresShell, aStyleContext);
  }
#else
  newFrame = NS_NewTextFrame(mPresShell, aStyleContext);
#endif

  if (NS_UNLIKELY(!newFrame))
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = InitAndRestoreFrame(aState, aContent, aParentFrame,
                                    nsnull, newFrame);

  if (NS_FAILED(rv)) {
    newFrame->Destroy();
    return rv;
  }

  // We never need to create a view for a text frame.

  // Set the frame's initial child list to null.
  newFrame->SetInitialChildList(nsnull, nsnull);

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
  // a valid HTML namespace.
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
  if (nsGkAtoms::img == aTag) {
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
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
        ProcessPseudoFrames(aState, aFrameItems); 
    }
    // Make sure to keep IsSpecialContent in synch with this code
    rv = CreateInputFrame(aState, aContent, aParentFrame,
                          aTag, aStyleContext, &newFrame,
                          display, frameHasBeenInitialized,
                          addedToFrameList, aFrameItems);  
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
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    
    rv = ConstructButtonFrame(aState, aContent, aParentFrame,
                              aTag, aStyleContext, &newFrame,
                              display, aFrameItems);
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

    rv = aState.AddChild(newFrame, aFrameItems, display, aContent,
                         aStyleContext, aParentFrame);
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
        HaveSpecialBlockStyle(aContent, aStyleContext,
                              &haveFirstLetterStyle, &haveFirstLineStyle);
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
    rv = aState.AddChild(newFrame, aFrameItems, display, aContent,
                         aStyleContext, aParentFrame);
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
  nsCOMPtr<nsIAnonymousContentCreator> creator(do_QueryInterface(aParentFrame));
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

    content->SetNativeAnonymous(PR_TRUE);

    nsIContent* bindingParent = content;
#ifdef MOZ_SVG
    // least-surprise CSS binding until we do the SVG specified
    // cascading rules for <svg:use> - bug 265894
    if (aParent &&
        aParent->NodeInfo()->Equals(nsGkAtoms::use, kNameSpaceID_SVG))
      bindingParent = aParent;
#endif

    rv = content->BindToTree(aDocument, aParent, bindingParent, PR_TRUE);
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

    creator->PostCreateFrames();
  }

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
          newFrame = NS_NewAreaFrame(mPresShell, aStyleContext,
                                     NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);
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
        // keep this in sync  with IsSpecialContent
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
      else if (aTag == nsGkAtoms::popupgroup) {
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
      else if (aTag == nsGkAtoms::nativescrollbar) {
        newFrame = NS_NewNativeScrollbarFrame(mPresShell, aStyleContext);
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
        nsIMenuFrame* menuFrame;
        CallQueryInterface(aParentFrame, &menuFrame);
        if (!menuFrame) {
          if (!aState.mPopupItems.containingBlock) {
            // Just don't create a frame for this popup; we can't do
            // anything with it, since there is no root popup set.
            *aHaltProcessing = PR_TRUE;
            NS_ASSERTION(!aState.mRootBox, "Popup containing block is missing");
            return NS_OK;
          }

#ifdef NS_DEBUG
          nsIPopupSetFrame* popupSet;
          CallQueryInterface(aState.mPopupItems.containingBlock, &popupSet);
          NS_ASSERTION(popupSet, "Popup containing block isn't a nsIPopupSetFrame");
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
          (frameType == nsGkAtoms::areaFrame)) {
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
    rv = aState.AddChild(topFrame, aFrameItems, display, aContent,
                         aStyleContext, origParentFrame, PR_FALSE, PR_FALSE,
                         isPopup);
    if (NS_FAILED(rv)) {
      return rv;
    }

#ifdef MOZ_XUL
    if (aTag == nsGkAtoms::popupgroup) {
      nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
      if (rootBox) {
        NS_ASSERTION(rootBox->GetPopupSetFrame() == newFrame,
                     "Unexpected PopupSetFrame");
        aState.mPopupItems.containingBlock = rootBox->GetPopupSetFrame();
      }      
    }
#endif

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
               aDisplay->IsBlockLevel(),
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
        NS_NewAreaFrame(mPresShell, aStyleContext,
                        NS_BLOCK_SPACE_MGR | NS_BLOCK_MARGIN_ROOT);

    nsFrameItems blockItem;
    rv = ConstructBlock(aState,
                        scrolledContentStyle->GetStyleDisplay(), aContent,
                        newFrame, newFrame, scrolledContentStyle,
                        &scrolledFrame, blockItem, aDisplay->IsPositioned());
    NS_ASSERTION(blockItem.childList == scrolledFrame,
                 "Scrollframe's frameItems should be exactly the scrolled frame");
    FinishBuildingScrollFrame(newFrame, scrolledFrame);

    rv = aState.AddChild(newFrame, aFrameItems, aDisplay, aContent,
                         aStyleContext, aParentFrame);
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
                        aDisplay->mPosition == NS_STYLE_POSITION_RELATIVE);
    if (NS_FAILED(rv)) {
      return rv;
    }

    addedToFrameList = PR_TRUE;
  }
  // See if it's relatively positioned
  else if ((NS_STYLE_POSITION_RELATIVE == aDisplay->mPosition) &&
           ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_INLINE == aDisplay->mDisplay) ||
            (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay))) {
    if (!aHasPseudoParent && !aState.mPseudoFrames.IsEmpty()) {
      ProcessPseudoFrames(aState, aFrameItems); 
    }
    // Is it block-level or inline-level?
    if ((NS_STYLE_DISPLAY_BLOCK == aDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_LIST_ITEM == aDisplay->mDisplay)) {
      // Create a wrapper frame. No space manager, though
      newFrame = NS_NewRelativeItemWrapperFrame(mPresShell, aStyleContext);
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
                               aNameSpaceID, PR_FALSE, aFrameItems, PR_TRUE,
                               newFrame, innerTable);
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
      nsIFrame* parentFrame = aParentFrame;
      nsIFrame* outerFrame = aParentFrame->GetParent();
      if (outerFrame) {
        if ((nsGkAtoms::tableOuterFrame == outerFrame->GetType()) &&
            (nsGkAtoms::tableFrame == parentFrame->GetType())) {
          parentFrame = outerFrame;
        }
      }
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
    
    rv = aState.AddChild(newFrame, aFrameItems, aDisplay, aContent,
                         aStyleContext, aParentFrame);
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
  nsresult rv = NS_OK;
  
  NS_ASSERTION(aNewFrame, "Null frame cannot be initialized");
  if (!aNewFrame)
    return NS_ERROR_NULL_POINTER;

  // Initialize the frame
  rv = aNewFrame->Init(aContent, aParentFrame, aPrevInFlow);

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
  // Resolve the style context based on the content object and the parent
  // style context
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();

  // skip past any parents that are scrolled-content. We want to inherit directly
  // from the outer scroll frame.
  while (parentStyleContext && parentStyleContext->GetPseudoType() ==
         nsCSSAnonBoxes::scrolledContent) {
    parentStyleContext = parentStyleContext->GetParent();
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
  PRBool    ignoreInterTagWhitespace = PR_TRUE;

  NS_ASSERTION(aTag != nsnull, "null MathML tag");
  if (aTag == nsnull)
    return NS_OK;

  // Initialize the new frame
  nsIFrame* newFrame = nsnull;

  // Make sure to keep IsSpecialContent in synch with this code
  const nsStyleDisplay* disp = aStyleContext->GetStyleDisplay();

  // Leverage IsSpecialContent to check if one of the |if aTag| below will
  // surely match (knowing that aNameSpaceID == kNameSpaceID_MathML here)
  if (IsSpecialContent(aContent, aTag, aNameSpaceID, aStyleContext) ||
      (aTag == nsGkAtoms::mtable_ && 
       disp->mDisplay == NS_STYLE_DISPLAY_TABLE)) {
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
  // CONSTRUCTION of MTABLE elements
  else if (aTag == nsGkAtoms::mtable_ &&
           disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // <mtable> is an inline-table -- but this isn't yet supported.
    // What we do here is to wrap the table in an anonymous containing
    // block so that it can mix better with other surrounding MathML markups
    // This assumes that the <mtable> is not positioned or floated.
    // (MathML does not allow/support positioned or floated elements at all.)

    nsStyleContext* parentContext = aParentFrame->GetStyleContext();
    nsStyleSet *styleSet = mPresShell->StyleSet();

    // first, create a MathML mrow frame that will wrap the block frame
    nsRefPtr<nsStyleContext> mrowContext;
    mrowContext = styleSet->ResolvePseudoStyleFor(aContent,
                                                  nsCSSAnonBoxes::mozMathInline,
                                                  parentContext);
    newFrame = NS_NewMathMLmrowFrame(mPresShell, mrowContext);
    if (NS_UNLIKELY(!newFrame)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, newFrame);

    
    nsRefPtr<nsStyleContext> blockContext;
    blockContext = styleSet->ResolvePseudoStyleFor(aContent,
                                                   nsCSSAnonBoxes::mozAnonymousBlock,
                                                   mrowContext);
    
    // then, create a block frame that will wrap the table frame
    nsIFrame* blockFrame = NS_NewBlockFrame(mPresShell, blockContext,
                                            NS_BLOCK_SPACE_MGR |
                                            NS_BLOCK_MARGIN_ROOT);
    if (NS_UNLIKELY(!newFrame)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    InitAndRestoreFrame(aState, aContent, newFrame, nsnull, blockFrame);

    // then, create the table frame itself
    nsRefPtr<nsStyleContext> tableContext;
    tableContext = styleSet->ResolveStyleFor(aContent, blockContext);

    nsFrameItems tempItems;
    nsIFrame* outerTable;
    nsIFrame* innerTable;
    
    // Pass PR_FALSE for aAllowOutOfFlow so that the resulting table will be
    // guaranteed to be in-flow (and in particular, a descendant of the <math>
    // in the _frame_ tree).
    rv = ConstructTableFrame(aState, aContent, blockFrame, tableContext,
                             aNameSpaceID, PR_FALSE, tempItems, PR_FALSE,
                             outerTable, innerTable);
    // Note: table construction function takes care of initializing the frame,
    // processing children, and setting the initial child list

    // set the outerTable as the initial child of the anonymous block
    blockFrame->SetInitialChildList(nsnull, outerTable);

    // set the block frame as the initial child of the mrow frame
    newFrame->SetInitialChildList(nsnull, blockFrame);

    // add the new frame to the flow
    // XXXbz this is wrong.  What if it's out-of-flow?  For that matter, this
    // is putting the frame in the wrong child list in the "pseudoParent ==
    // PR_TRUE" case, which I assume we can hit.
    aFrameItems.AddChild(newFrame);

    return rv; 
  }
  // End CONSTRUCTION of MTABLE elements 

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
    // record that children that are ignorable whitespace should be excluded
    if (ignoreInterTagWhitespace) {
      newFrame->AddStateBits(NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE);
    }

    // Only <math> elements can be floated or positioned.  All other MathML
    // should be in-flow.
    PRBool isMath = aTag == nsGkAtoms::math;

    nsIFrame* geometricParent =
      isMath ? aState.GetGeometricParent(disp, aParentFrame) : aParentFrame;
    
    InitAndRestoreFrame(aState, aContent, geometricParent, nsnull, newFrame);

    // See if we need to create a view, e.g. the frame is absolutely positioned
    nsHTMLContainerFrame::CreateViewForFrame(newFrame, aParentFrame, PR_FALSE);

    rv = aState.AddChild(newFrame, aFrameItems, disp, aContent, aStyleContext,
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
nsCSSFrameConstructor::TestSVGConditions(nsIContent* aContent,
                                         PRBool&     aHasRequiredExtensions,
                                         PRBool&     aHasRequiredFeatures,
                                         PRBool&     aHasSystemLanguage)
{
  nsAutoString value;

  // Only elements can have tests on them
  if (! aContent->IsNodeOfType(nsINode::eELEMENT)) {
    aHasRequiredExtensions = PR_FALSE;
    aHasRequiredFeatures = PR_FALSE;
    aHasSystemLanguage = PR_FALSE;
    return NS_OK;
  }

  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  // For now, claim that mozilla's SVG implementation supports
  // no extensions.  So, if extensions are required, we don't have
  // them available. Empty-string should always set aHasRequiredExtensions to
  // false.
  aHasRequiredExtensions = !aContent->HasAttr(kNameSpaceID_None,
                                              nsGkAtoms::requiredExtensions);

  // Required Features
  aHasRequiredFeatures = PR_TRUE;
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::requiredFeatures, value)) {
    aHasRequiredFeatures = !value.IsEmpty() && NS_SVG_TestFeatures(value);
  }

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  aHasSystemLanguage = PR_TRUE;
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::systemLanguage,
                        value)) {
    // Get our language preferences
    nsAutoString langPrefs(nsContentUtils::GetLocalizedStringPref("intl.accept_languages"));
    if (!langPrefs.IsEmpty()) {
      langPrefs.StripWhitespace();
      value.StripWhitespace();
#ifdef  DEBUG_scooter
      printf("Calling SVG_TestLanguage('%s','%s')\n", NS_ConvertUTF16toUTF8(value).get(), 
                                                      NS_ConvertUTF16toUTF8(langPrefs).get());
#endif
      aHasSystemLanguage = SVG_TestLanguage(value, langPrefs);
    } else {
      // For now, evaluate to true.
      NS_WARNING("no default language specified for systemLanguage conditional test");
      aHasSystemLanguage = !value.IsEmpty();
    }
  }
  return NS_OK;
}

nsresult
nsCSSFrameConstructor::SVGSwitchProcessChildren(nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsIFrame*                aFrame,
                                                nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
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
  PRInt32 childCount = aContent->GetChildCount();
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsIContent* child = aContent->GetChildAt(i);

    // Skip over children that aren't elements
    if (!child->IsNodeOfType(nsINode::eELEMENT)) {
      continue;
    }

    rv = TestSVGConditions(child,
                           hasRequiredExtensions,
                           hasRequiredFeatures,
                           hasSystemLanguage);
#ifdef DEBUG_scooter
    nsAutoString str;
    child->Tag()->ToString(str);
    printf("Child tag: %s\n", NS_ConvertUTF16toUTF8(str).get());
    printf("SwitchProcessChildren: Required Extensions = %s, Required Features = %s, System Language = %s\n",
            hasRequiredExtensions ? "true" : "false",
            hasRequiredFeatures ? "true" : "false",
            hasSystemLanguage ? "true" : "false");
#endif
    if (NS_FAILED(rv))
      return rv;

    if (hasRequiredExtensions &&
        hasRequiredFeatures &&
        hasSystemLanguage) {

      rv = ConstructFrame(aState, child,
                          aFrame, aFrameItems);

      if (NS_FAILED(rv))
        return rv;

      // No errors -- break out of loop (only render the first matching element)
      break;
    }
  }

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aFrameItems);
  }

  // restore the incoming pseudo frame state
  aState.mPseudoFrames = priorPseudoFrames;


  return rv;
}

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

    parentIsSVG = parentNSID == kNameSpaceID_SVG
#ifdef MOZ_SVG_FOREIGNOBJECT
                  // It's not clear whether the SVG spec intends to allow any SVG
                  // content within svg:foreignObject at all (SVG 1.1, section
                  // 23.2), but if it does, it better be svg:svg.  So given that
                  // we're allowing it, treat it as a non-SVG parent.
                  && parentTag != nsGkAtoms::foreignObject
#endif
                  ;
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
  
  // See if this element supports conditionals & if it does,
  // handle it
  if (((aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::requiredFeatures) ||
        aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::requiredExtensions)) &&
        NS_SVG_TestsSupported(aTag)) ||
      (aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::systemLanguage) &&
       NS_SVG_LangSupported(aTag))) {

    PRBool hasRequiredExtentions = PR_FALSE;
    PRBool hasRequiredFeatures = PR_FALSE;
    PRBool hasSystemLanguage = PR_FALSE;
    TestSVGConditions(aContent, hasRequiredExtentions, 
                      hasRequiredFeatures, hasSystemLanguage);
    // Note that just returning is probably not right.  According
    // to the spec, <use> is allowed to use an element that fails its
    // conditional, but because we never actually create the frame when
    // a conditional fails and when we use GetReferencedFrame to find the
    // references, things don't work right.
    // XXX FIXME XXX
    if (!hasRequiredExtentions || !hasRequiredFeatures ||
        !hasSystemLanguage) {
      *aHaltProcessing = PR_TRUE;
      return NS_OK;
    }
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
  else if (aTag == nsGkAtoms::g ||
           aTag == nsGkAtoms::svgSwitch) {
    newFrame = NS_NewSVGGFrame(mPresShell, aContent, aStyleContext);
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
#ifdef MOZ_SVG_FOREIGNOBJECT
  else if (aTag == nsGkAtoms::foreignObject) {
    newFrame = NS_NewSVGForeignObjectFrame(mPresShell, aContent, aStyleContext);
  }
#endif
  else if (aTag == nsGkAtoms::a) {
    newFrame = NS_NewSVGAFrame(mPresShell, aContent, aStyleContext);
  }
  else if (aTag == nsGkAtoms::text) {
    newFrame = NS_NewSVGTextFrame(mPresShell, aContent, aStyleContext);
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

    rv = aState.AddChild(newFrame, aFrameItems, disp, aContent, aStyleContext,
                         aParentFrame, isOuterSVGNode, isOuterSVGNode);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsFrameItems childItems;
#ifdef MOZ_SVG_FOREIGNOBJECT
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
      const nsStyleDisplay* disp = innerPseudoStyle->GetStyleDisplay();
      rv = ConstructBlock(aState, disp, aContent,
                          newFrame, newFrame, innerPseudoStyle,
                          &blockFrame, childItems, PR_TRUE);
      // Give the blockFrame a view so that GetOffsetTo works for descendants
      // of blockFrame with views...
      nsHTMLContainerFrame::CreateViewForFrame(blockFrame, nsnull, PR_TRUE);
    } else
#endif  // MOZ_SVG_FOREIGNOBJECT
    {
      // Process the child content if requested.
      if (!newFrame->IsLeaf()) {
        if (aTag == nsGkAtoms::svgSwitch) {
          rv = SVGSwitchProcessChildren(aState, aContent, newFrame,
                                        childItems);
        } else {
          rv = ProcessChildren(aState, aContent, newFrame, PR_TRUE, childItems,
                               PR_FALSE);
        }

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

PRBool
nsCSSFrameConstructor::PageBreakBefore(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
                                       nsFrameItems&            aFrameItems)
{
  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();

  // See if page-break-before is set for all elements except row groups, rows, cells 
  // (these are handled internally by tables) and construct a page break frame if so.
  if (NS_STYLE_DISPLAY_NONE != display->mDisplay &&
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

nsresult
nsCSSFrameConstructor::ConstructPageBreakFrame(nsFrameConstructorState& aState,
                                               nsIContent*              aContent,
                                               nsIFrame*                aParentFrame,
                                               nsStyleContext*          aStyleContext,
                                               nsFrameItems&            aFrameItems)
{
  nsRefPtr<nsStyleContext> pseudoStyle;
  pseudoStyle = mPresShell->StyleSet()->ResolvePseudoStyleFor(nsnull,
                                                              nsCSSAnonBoxes::pageBreak,
                                                              aStyleContext);
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
    // See if there is a page break before, if so construct one. Also see if there is one after
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

      rv = xblService->LoadBindings(aContent, display->mBinding, PR_FALSE,
                                    getter_AddRefs(binding.mBinding),
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
      aParentFrame->IsFrameOfType(nsIFrame::eSVG)
#ifdef MOZ_SVG_FOREIGNOBJECT
      && !aParentFrame->IsFrameOfType(nsIFrame::eSVGForeignObject)
#endif
      ) {
    return NS_OK;
  }
#endif

  // Style resolution can normally happen lazily.  However, getting the
  // Visibility struct can cause |SetBidiEnabled| to be called on the
  // pres context, and this needs to happen before we start reflow, so
  // do it now, when constructing frames.  See bug 115921.
  {
    styleContext->GetStyleVisibility();
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
      // Before removing the frames associated with the content object, ask them to save their
      // state onto a temporary state object.
      CaptureStateForFramesOf(rootContent, mTempFrameTreeState);

      nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                    nsnull, nsnull, mTempFrameTreeState);

      // Get the frame that corresponds to the document element
      nsIFrame* docElementFrame =
        state.mFrameManager->GetPrimaryFrameFor(rootContent, -1);
        
      // Remove any existing fixed items: they are always on the
      // FixedContainingBlock.  Note that this has to be done before we call
      // ClearPlaceholderFrameMap(), since RemoveFixedItems uses the
      // placeholder frame map.
      rv = RemoveFixedItems(state);
      if (NS_SUCCEEDED(rv)) {
        // Clear the hash tables that map from content to frame and out-of-flow
        // frame to placeholder frame
        state.mFrameManager->ClearPrimaryFrameMap();
        state.mFrameManager->ClearPlaceholderFrameMap();
        state.mFrameManager->ClearUndisplayedContentMap();

        if (docElementFrame) {
          // Take the docElementFrame, and remove it from its parent.
        
          // XXXbz So why can't we reuse ContentRemoved?

          NS_ASSERTION(docElementFrame->GetParent() == mDocElementContainingBlock,
                       "Unexpected doc element parent frame");

          // Remove the old document element hieararchy
          rv = state.mFrameManager->RemoveFrame(mDocElementContainingBlock,
                                                nsnull, docElementFrame);
          if (NS_FAILED(rv)) {
            return rv;
          }

        }

        // Create the new document element hierarchy
        nsIFrame*                 newChild;
        rv = ConstructDocElementFrame(state, rootContent,
                                      mDocElementContainingBlock, &newChild);

        // newChild could be null even if |rv| is success, thanks to XBL.
        if (NS_SUCCEEDED(rv) && newChild) {
          rv = state.mFrameManager->InsertFrames(mDocElementContainingBlock,
                                                 nsnull, nsnull, newChild);
        }
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
      // condition because of the mInitialContainingBlock stuff at the
      // end of this method...
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
        if (nsGkAtoms::areaFrame == frameType ||
            nsGkAtoms::blockFrame == frameType ||
            nsGkAtoms::positionedInlineFrame == frameType) {
          containingBlock = wrappedFrame;
        } else if (nsGkAtoms::fieldSetFrame == frameType) {
          // If the positioned frame is a fieldset, use the area frame inside it.
          // We don't use GetContentInsertionFrame for fieldsets yet.
          containingBlock = GetFieldSetAreaFrame(wrappedFrame);
        }
      }

#ifdef DEBUG
      if (!containingBlock)
        NS_WARNING("Positioned frame that does not handle positioned kids; looking further up the parent chain");
#endif
    }
  }

  // If we found an absolutely positioned containing block, then use the first-in-flow.
  if (containingBlock)
    return AdjustAbsoluteContainingBlock(mPresShell->GetPresContext(),
                                         containingBlock);

  // If we didn't find it, then use the initial containing block if it
  // supports abs pos kids.
  return mInitialContainingBlockIsAbsPosContainer ? mInitialContainingBlock : nsnull;
}

nsIFrame*
nsCSSFrameConstructor::GetFloatContainingBlock(nsIFrame* aFrame)
{
  NS_PRECONDITION(mInitialContainingBlock, "no initial containing block");
  
  // Starting with aFrame, look for a frame that is a float containing block.
  // IF we hit a mathml frame, bail out; we don't allow floating out of mathml
  // frames, because they don't seem to be able to deal.
  for (nsIFrame* containingBlock = aFrame;
       containingBlock && !containingBlock->IsFrameOfType(nsIFrame::eMathML);
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
nsCSSFrameConstructor::AppendFrames(const nsFrameConstructorState& aState,
                                    nsIContent*                    aContainer,
                                    nsIFrame*                      aParentFrame,
                                    nsIFrame*                      aFrameList,
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
    nsFrameList frames(aParentFrame->GetFirstChild(nsnull));

    // Insert the frames before the :after pseudo-element.
    return frameManager->InsertFrames(aParentFrame, nsnull,
                                      frames.GetPrevSiblingFor(aAfterFrame),
                                      aFrameList);
  }

  return frameManager->AppendFrames(aParentFrame, nsnull, aFrameList);
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
    nsIFrame* prevSibling = aPresShell->GetPrimaryFrameFor(child);
    if (prevSibling) {
      // The frame may be a special frame (a split inline frame that
      // contains a block).  Get the last part of that split.
      if (IsFrameSpecial(prevSibling)) {
        prevSibling = GetLastSpecialSibling(aPresShell->FrameManager(),
                                            prevSibling);
      }

      // The frame may have a continuation. If so, we want the
      // last continuation as our previous sibling.
      prevSibling = prevSibling->GetLastContinuation();

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      if (prevSibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
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
    nsIFrame* nextSibling = aPresShell->GetPrimaryFrameFor(child);
    if (nextSibling) {
      // The primary frame should never be a continuation
      NS_ASSERTION(!nextSibling->GetPrevInFlow(),
                   "primary frame is a continuation!?");

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      if (nextSibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
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
nsCSSFrameConstructor::IsValidSibling(nsIFrame*              aParentFrame,
                                      const nsIFrame&        aSibling,
                                      PRUint8                aSiblingDisplay,
                                      nsIContent&            aContent,
                                      PRUint8&               aDisplay)
{
  if ((NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_COLUMN       == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_CAPTION      == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aSiblingDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aSiblingDisplay)) {
    // if we haven't already, construct a style context to find the display type of aContent
    if (UNSET_DISPLAY == aDisplay) {
      nsRefPtr<nsStyleContext> styleContext;
      styleContext = ResolveStyleContext(aSibling.GetParent(), &aContent);
      if (!styleContext) return PR_FALSE;
      const nsStyleDisplay* display = styleContext->GetStyleDisplay();
      aDisplay = display->mDisplay;
    }
    switch (aSiblingDisplay) {
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
  else if (nsGkAtoms::fieldSetFrame == aParentFrame->GetType()) {
    // Legends can be sibling of legends but not of other content in the fieldset
    nsIAtom* sibType = aSibling.GetType();
    nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(&aContent));

    if ((legendContent  && (nsGkAtoms::legendFrame != sibType)) ||
        (!legendContent && (nsGkAtoms::legendFrame == sibType)))
      return PR_FALSE;
  }

  return PR_TRUE;
}

/**
 * Find the ``rightmost'' frame for the content immediately preceding
 * aIndexInContainer, following continuations if necessary.
 */
nsIFrame*
nsCSSFrameConstructor::FindPreviousSibling(nsIContent*       aContainer,
                                           nsIFrame*         aContainerFrame,
                                           PRInt32           aIndexInContainer,
                                           const nsIContent* aChild)
{
  NS_ASSERTION(aContainer, "null argument");

  ChildIterator first, iter;
  nsresult rv = ChildIterator::Init(aContainer, &first, &iter);
  NS_ENSURE_SUCCESS(rv, nsnull);
  iter.seek(aIndexInContainer);

  PRUint8 childDisplay = UNSET_DISPLAY;
  // Note: not all content objects are associated with a frame (e.g., if it's
  // `display: hidden') so keep looking until we find a previous frame
  while (iter-- != first) {
    nsIFrame* prevSibling = mPresShell->GetPrimaryFrameFor(nsCOMPtr<nsIContent>(*iter));

    if (prevSibling) {
      // The frame may be a special frame (a split inline frame that
      // contains a block).  Get the last part of that split.
      if (IsFrameSpecial(prevSibling)) {
        prevSibling = GetLastSpecialSibling(mPresShell->FrameManager(),
                                            prevSibling);
      }

      // The frame may have a continuation. Get the last continuation
      prevSibling = prevSibling->GetLastContinuation();

      // XXXbz should the IsValidSibling check be after we get the
      // placeholder for out-of-flows?
      const nsStyleDisplay* display = prevSibling->GetStyleDisplay();
  
      if (aChild && !IsValidSibling(aContainerFrame, *prevSibling, 
                                    display->mDisplay, (nsIContent&)*aChild,
                                    childDisplay))
        continue;

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      if (prevSibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
        nsIFrame* placeholderFrame;
        mPresShell->GetPlaceholderFrameFor(prevSibling, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
        prevSibling = placeholderFrame;
      }

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
nsCSSFrameConstructor::FindNextSibling(nsIContent*       aContainer,
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
    nsIFrame* nextSibling =
      mPresShell->GetPrimaryFrameFor(nsCOMPtr<nsIContent>(*iter));

    if (nextSibling) {
      // The frame primary frame should never be a continuation
      NS_ASSERTION(!nextSibling->GetPrevInFlow(),
                   "primary frame is a continuation!?");

      // XXXbz should the IsValidSibling check be after we get the
      // placeholder for out-of-flows?
      const nsStyleDisplay* display = nextSibling->GetStyleDisplay();

      if (aChild && !IsValidSibling(aContainerFrame, *nextSibling, 
                                    display->mDisplay, (nsIContent&)*aChild,
                                    childDisplay))
        continue;

      // If the frame is out-of-flow, GPFF() will have returned the
      // out-of-flow frame; we want the placeholder.
      if (nextSibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
        // Nope. Get the place-holder instead
        nsIFrame* placeholderFrame;
        mPresShell->GetPlaceholderFrameFor(nextSibling, &placeholderFrame);
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

// For tables, returns the inner table, if the child is not a caption. 
// For fieldsets, returns the area frame, if the child is not a legend. 
static nsIFrame*
GetAdjustedParentFrame(nsIFrame*       aParentFrame,
                       nsIAtom*        aParentFrameType,
                       nsIContent*     aParentContent,
                       PRInt32         aChildIndex)
{
  nsIContent *childContent = aParentContent->GetChildAt(aChildIndex);
  nsIFrame* newParent = nsnull;

  if (nsGkAtoms::tableOuterFrame == aParentFrameType) {
    nsCOMPtr<nsIDOMHTMLTableCaptionElement> captionContent(do_QueryInterface(childContent));
    // If the parent frame is an outer table, use the innner table
    // as the parent unless the new content is a caption.
    if (!captionContent) 
      newParent = aParentFrame->GetFirstChild(nsnull);
  }
  else if (nsGkAtoms::fieldSetFrame == aParentFrameType) {
    // If the parent is a fieldSet, use the fieldSet's area frame as the
    // parent unless the new content is a legend. 
    nsCOMPtr<nsIDOMHTMLLegendElement> legendContent(do_QueryInterface(childContent));
    if (!legendContent) {
      newParent = GetFieldSetAreaFrame(aParentFrame);
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
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended container=%p index=%d\n",
           NS_STATIC_CAST(void*, aContainer), aNewIndexInContainer);
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
                            iter.index(), mTempFrameTreeState, PR_FALSE);
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
    nsFrameManager *frameManager = mPresShell->FrameManager();

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
      if (child && child->IsNodeOfType(nsINode::eELEMENT)) {
        nsRefPtr<nsStyleContext> styleContext;
        styleContext = ResolveStyleContext(parentFrame, child);
        // XXX since the block child goes in the last inline of the sacred triad, frames would 
        // need to be moved into the 2nd triad (block) but that is more work, for now bail.
        needReframe = styleContext->GetStyleDisplay()->IsBlockLevel();
      }
      if (needReframe)
        return ReframeContainingBlock(parentFrame);
    }
  }

  // Get the parent frame's last continuation
  parentFrame = parentFrame->GetLastContinuation();

  nsIAtom* frameType = parentFrame->GetType();
  // Deal with inner/outer tables, fieldsets
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
  nsIFrame*               firstAppendedFrame = nsnull;
  nsFrameItems            frameItems;
  nsFrameConstructorState state(mPresShell, mFixedContainingBlock,
                                GetAbsoluteContainingBlock(parentFrame),
                                GetFloatContainingBlock(parentFrame));

  // See if the containing block has :first-letter style applied.
  PRBool haveFirstLetterStyle = PR_FALSE, haveFirstLineStyle = PR_FALSE;
  nsIFrame* containingBlock = state.mFloatedItems.containingBlock;
  if (containingBlock) {
    nsIContent* blockContent = containingBlock->GetContent();
    nsStyleContext* blockSC = containingBlock->GetStyleContext();
    HaveSpecialBlockStyle(blockContent, blockSC,
                          &haveFirstLetterStyle,
                          &haveFirstLineStyle);
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
    // lookup the table child frame type as it is much more difficult to remove a frame
    // and all it descendants (abs. pos. for instance) than to prevent the frame creation.
    if (nsGkAtoms::tableFrame == frameType) {
      nsFrameItems tempItems;
      ConstructFrame(state, childContent, parentFrame, tempItems);
      if (tempItems.childList) {
        if (nsGkAtoms::tableCaptionFrame == tempItems.childList->GetType()) {
          captionItems.AddChild(tempItems.childList);
        }
        else {
          frameItems.AddChild(tempItems.childList);
        }
        newFrame = tempItems.childList;
      }
    }
    else if (nsGkAtoms::tableColGroupFrame == frameType) {
      nsRefPtr<nsStyleContext> childStyleContext;
      childStyleContext = ResolveStyleContext(parentFrame, childContent);
      if (childStyleContext->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_TABLE_COLUMN)
        continue; //don't create anything else than columns below a colgroup
      ConstructFrame(state, childContent, parentFrame, frameItems);
      newFrame = frameItems.lastChild;
    }
    else {
      // Construct a child frame (that does not have a table as parent)
      ConstructFrame(state, childContent, parentFrame, frameItems);
      newFrame = frameItems.lastChild;
    }

    if (newFrame && newFrame != oldNewFrame) {
      InvalidateCanvasIfNeeded(newFrame);
      oldNewFrame = newFrame;
    }
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
  firstAppendedFrame = frameItems.childList;
  if (!firstAppendedFrame) {
    firstAppendedFrame = captionItems.childList;
  }

  // Notify the parent frame passing it the list of new frames
  if (NS_SUCCEEDED(result) && firstAppendedFrame) {
    // Perform special check for diddling around with the frames in
    // a special inline frame.

    if (WipeContainingBlock(state, containingBlock, parentFrame,
                            frameItems.childList)) {
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
        AppendFrames(state, aContainer, parentFrame, frameItems.childList,
                     parentAfterFrame);
      }
    }
    else {
      AppendFrames(state, aContainer, parentFrame, firstAppendedFrame,
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

// Return TRUE if the insertion of aChild into aParent1,2 should force a reframe. aParent1 is 
// the special inline container which contains a block. aParentFrame is approximately aParent1's
// primary frame and will be set to the correct parent of aChild if a reframe is not necessary. 
// aParent2 is aParentFrame's content. aPrevSibling will be set to the correct prev sibling of
// aChild if a reframe is not necessary.
PRBool
nsCSSFrameConstructor::NeedSpecialFrameReframe(nsIContent*     aParent1,     
                                               nsIContent*     aParent2,     
                                               nsIFrame*&      aParentFrame, 
                                               nsIContent*     aChild,
                                               PRInt32         aIndexInContainer,
                                               nsIFrame*&      aPrevSibling,
                                               nsIFrame*       aNextSibling)
{
  // XXXbz aNextSibling is utterly unused.  Why?
  
  if (!IsInlineFrame2(aParentFrame)) 
    return PR_FALSE;

  // find out if aChild is a block or inline
  PRBool childIsBlock = PR_FALSE;
  if (aChild->IsNodeOfType(nsINode::eELEMENT)) {
    nsRefPtr<nsStyleContext> styleContext;
    styleContext = ResolveStyleContext(aParentFrame, aChild);
    const nsStyleDisplay* display = styleContext->GetStyleDisplay();
    childIsBlock = display->IsBlockLevel();
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
      // XXXbz see comments in ContentInserted about this being wrong in many
      // cases!  Why doesn't this just use aNextSibling anyway?  Why are we
      // looking sometimes in aParent1 and sometimes in aParent2?
      nsIFrame* nextSibling = (aIndexInContainer >= 0)
                              ? FindNextSibling(aParent2, aParentFrame,
                                                aIndexInContainer)
                              : FindNextAnonymousSibling(mPresShell, mDocument,
                                                         aParent1, aChild);
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
        // XXXbz see comments in ContentInserted about this being wrong in many
        // cases!  Why doesn't this just use aNextSibling anyway?  Why are we
        // looking sometimes in aParent1 and sometimes in aParent2?
        nsIFrame* nextSibling = (aIndexInContainer >= 0)
                                ? FindNextSibling(aParent2, aParentFrame,
                                                  aIndexInContainer)
                                : FindNextAnonymousSibling(mPresShell,
                                                           mDocument, aParent1,
                                                           aChild);
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

  if (aContainer->IsNodeOfType(nsINode::eXUL) &&
      aChild->IsNodeOfType(nsINode::eXUL) &&
      aContainer->Tag() == nsGkAtoms::listbox &&
      aChild->Tag() == nsGkAtoms::listitem) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(aContainer);
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsPIListBoxObject> listBoxObject = do_QueryInterface(boxObject);
    if (listBoxObject) {
      nsIListBoxObject* listboxBody = listBoxObject->GetListBoxBody();
      if (listboxBody) {
        nsListBoxBodyFrame *listBoxBodyFrame = NS_STATIC_CAST(nsListBoxBodyFrame*, listboxBody);
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
  nsIAtom* tag =
    aDocument->BindingManager()->ResolveTag(aContainer, &namespaceID);

  // Just ignore tree tags, anyway we don't create any frames for them.
  if (tag == nsGkAtoms::treechildren ||
      tag == nsGkAtoms::treeitem ||
      tag == nsGkAtoms::treerow ||
      (namespaceID == kNameSpaceID_XUL && aUseXBLForms &&
       ShouldIgnoreSelectChild(aContainer)))
    return PR_TRUE;

  return PR_FALSE;
}
#endif // MOZ_XUL

nsresult
nsCSSFrameConstructor::ContentInserted(nsIContent*            aContainer,
                                       nsIContent*            aChild,
                                       PRInt32                aIndexInContainer,
                                       nsILayoutHistoryState* aFrameState,
                                       PRBool                 aInReinsertContent)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
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
      ConstructDocElementFrame(state,
                               docElement, 
                               mDocElementContainingBlock,
                               &docElementFrame);
    
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

      if (docElementFrame) {
        InvalidateCanvasIfNeeded(docElementFrame);
      }

#ifdef DEBUG
      if (gReallyNoisyContentUpdates && docElementFrame) {
        nsIFrameDebug* fdbg = nsnull;
        CallQueryInterface(docElementFrame, &fdbg);
        if (fdbg) {
          printf("nsCSSFrameConstructor::ContentInserted: resulting frame model:\n");
          fdbg->List(stdout, 0);
        }
      }
#endif
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
    ? FindPreviousSibling(container, parentFrame, aIndexInContainer, aChild)
    : FindPreviousAnonymousSibling(mPresShell, mDocument, aContainer, aChild);

  PRBool    isAppend = PR_FALSE;
  nsIFrame* appendAfterFrame;  // This is only looked at when isAppend is true
  nsIFrame* nextSibling = nsnull;
    
  // If there is no previous sibling, then find the frame that follows
  if (! prevSibling) {
    nextSibling = (aIndexInContainer >= 0)
      ? FindNextSibling(container, parentFrame, aIndexInContainer, aChild)
      : FindNextAnonymousSibling(mPresShell, mDocument, aContainer, aChild);
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
    // Deal with inner/outer tables, fieldsets
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
    if (NeedSpecialFrameReframe(aContainer, container, parentFrame, 
                                aChild, aIndexInContainer, prevSibling,
                                nextSibling)) {
      return ReframeContainingBlock(parentFrame);
    }
  }

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
  nsStyleContext* blockSC;
  nsIContent* blockContent = nsnull;
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
      blockSC = containingBlock->GetStyleContext();
      blockContent = containingBlock->GetContent();
      HaveSpecialBlockStyle(blockContent, blockSC,
                            &haveFirstLetterStyle,
                            &haveFirstLineStyle);
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
        ? FindPreviousSibling(container, parentFrame, aIndexInContainer,
                              aChild)
        : FindPreviousAnonymousSibling(mPresShell, mDocument, aContainer,
                                       aChild);

      // If there is no previous sibling, then find the frame that follows
      if (! prevSibling) {
        nextSibling = (aIndexInContainer >= 0)
          ? FindNextSibling(container, parentFrame, aIndexInContainer, aChild)
          : FindNextAnonymousSibling(mPresShell, mDocument, aContainer, aChild);
      }

      handleSpecialFrame = IsFrameSpecial(parentFrame) && !aInReinsertContent;
      if (handleSpecialFrame &&
          NeedSpecialFrameReframe(aContainer, container, parentFrame,
                                  aChild, aIndexInContainer, prevSibling,
                                  nextSibling)) {
#ifdef DEBUG
        nsIContent* parentContainer = blockContent->GetParent();
        if (gNoisyContentUpdates) {
          printf("nsCSSFrameConstructor::ContentInserted: parentFrame=");
          nsFrame::ListTag(stdout, parentFrame);
          printf(" is special inline\n");
          printf("  ==> blockContent=%p, parentContainer=%p\n",
                 NS_STATIC_CAST(void*, blockContent),
                 NS_STATIC_CAST(void*, parentContainer));
        }
#endif

        NS_ASSERTION(GetFloatContainingBlock(parentFrame) == containingBlock,
                     "Unexpected block ancestor for parentFrame");

        // Note that in this case we're guaranteed that the closest block
        // containing parentFrame is |containingBlock|.  So
        // ReframeContainingBlock(parentFrame) will make sure to rebuild the
        // first-letter stuff we just blew away.
        return ReframeContainingBlock(parentFrame);
      }
    }
  }
  else if (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == parentDisplay->mDisplay) {
      nsRefPtr<nsStyleContext> childStyleContext;
      childStyleContext = ResolveStyleContext(parentFrame, aChild);
      if (childStyleContext->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_TABLE_COLUMN)
        return NS_OK; //don't create anything else than columns below a colgroup  
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

  // If the final parent frame (decided by AdjustParentFrame()) is different
  // from the parent of the insertion point we calculated above then
  // parentFrame/prevSibling/appendAfterFrame are now invalid and  as it is
  // unknown where to insert correctly we append instead (bug 341858).
  if (frameItems.childList &&
      frameItems.childList->GetParent() != parentFrame) {
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
  if (WipeContainingBlock(state, containingBlock, parentFrame,
                          frameItems.childList))
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
      
  nsIFrame* newFrame = frameItems.childList;
  if (NS_SUCCEEDED(rv) && newFrame) {
    NS_ASSERTION(!captionItems.childList, "leaking caption frames");
    // Notify the parent frame
    if (isAppend) {
      AppendFrames(state, aContainer, parentFrame, newFrame, appendAfterFrame);
    }
    else {
      if (!prevSibling) {
        // We're inserting the new frame as the first child. See if the
        // parent has a :before pseudo-element
        nsIFrame* firstChild = parentFrame->GetFirstChild(nsnull);

        if (firstChild &&
            nsLayoutUtils::IsGeneratedContentFor(aContainer, firstChild,
                                                 nsCSSPseudoElements::before)) {
          // Insert the new frames after the last continuation of the :before
          prevSibling = firstChild->GetLastContinuation();
          nsIFrame* newParent = prevSibling->GetParent();
          if (newParent != parentFrame) {
            nsHTMLContainerFrame::ReparentFrameViewList(state.mPresContext,
                                                        newFrame, parentFrame,
                                                        newParent);
            parentFrame = newParent;
          }
        }
      }
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
        // If we did adjust the parent then the calculated insertion point is
        // now invalid (bug 341382).
        if (parentFrame != outerTableFrame) {
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
nsCSSFrameConstructor::ReinsertContent(nsIContent*     aContainer,
                                       nsIContent*     aChild)
{
  PRInt32 ix = aContainer->IndexOf(aChild);
  // XXX For now, do a brute force remove and insert.
  // XXXbz this probably doesn't work so well with anonymous content
  // XXXbz doesn't this need to do the state-saving stuff that
  // RecreateFramesForContent does?
  nsresult res = ContentRemoved(aContainer, aChild, ix, PR_TRUE);

  if (NS_SUCCEEDED(res)) {
    res = ContentInserted(aContainer, aChild, ix, nsnull, PR_TRUE);
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
 *            can check if out-of-flow frames are a descendant of the frame
 *            being removed
 * @param   aFrame the local subtree that is being deleted. This is initially
 *            the same as aRemovedFrame, but as we recurse down the tree
 *            this changes
 */
static nsresult
DoDeletingFrameSubtree(nsFrameManager* aFrameManager,
                       nsVoidArray&    aDestroyQueue,
                       nsIFrame*       aRemovedFrame,
                       nsIFrame*       aFrame)
{
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
      if (NS_LIKELY(nsGkAtoms::placeholderFrame != childFrame->GetType())) {
        DoDeletingFrameSubtree(aFrameManager, aDestroyQueue,
                               aRemovedFrame, childFrame);

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
          DoDeletingFrameSubtree(aFrameManager, aDestroyQueue,
                                 outOfFlowFrame, outOfFlowFrame);
        }
        else {
          // Also recurse into the out-of-flow when it's a descendant of aRemovedFrame
          // since we don't walk those lists, see |childListName| increment below.
          DoDeletingFrameSubtree(aFrameManager, aDestroyQueue,
                                 aRemovedFrame, outOfFlowFrame);
        }
      }
    }

    // Move to next child list but skip lists with frames we should have
    // a placeholder for.
    do {
      childListName = aFrame->GetAdditionalChildListName(childListIndex++);
    } while (childListName == nsGkAtoms::floatList    ||
             childListName == nsGkAtoms::absoluteList ||
             childListName == nsGkAtoms::overflowOutOfFlowList ||
             childListName == nsGkAtoms::fixedList);
  } while (childListName);

  return NS_OK;
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
    aFrame = aFrame->GetNextContinuation();
  } while (aFrame);

  // Now destroy any out-of-flow frames that have been enqueued for
  // destruction.
  for (PRInt32 i = destroyQueue.Count() - 1; i >= 0; --i) {
    nsIFrame* outOfFlowFrame = NS_STATIC_CAST(nsIFrame*, destroyQueue[i]);

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

nsresult
nsCSSFrameConstructor::ContentRemoved(nsIContent*     aContainer,
                                      nsIContent*     aChild,
                                      PRInt32         aIndexInContainer,
                                      PRBool          aInReinsertContent)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
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

  nsFrameManager *frameManager = mPresShell->FrameManager();
  nsPresContext *presContext = mPresShell->GetPresContext();
  nsresult                  rv = NS_OK;

  // Find the child frame that maps the content
  nsIFrame* childFrame =
    mPresShell->FrameManager()->GetPrimaryFrameFor(aChild, aIndexInContainer);

  if (! childFrame) {
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
      return ReframeContainingBlock(childFrame);
    }

    // Get the childFrame's parent frame
    nsIFrame* parentFrame = childFrame->GetParent();

    if (parentFrame->GetType() == nsGkAtoms::frameSetFrame &&
        IsSpecialFramesetChild(aChild)) {
      // Just reframe the parent, since framesets are weird like that.
      return RecreateFramesForContent(parentFrame->GetContent());
    }

    // Examine the containing-block for the removed content and see if
    // :first-letter style applies.
    nsIFrame* containingBlock = GetFloatContainingBlock(parentFrame);
    PRBool haveFLS = containingBlock ?
      HaveFirstLetterStyle(containingBlock->GetContent(),
                           containingBlock->GetStyleContext()) :
      PR_FALSE;
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
      
      // Remove the mapping from the frame to its placeholder
      frameManager->UnregisterPlaceholderFrame(placeholderFrame);

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
      mInitialContainingBlockIsAbsPosContainer = PR_FALSE;
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
      nsContainerFrame::SyncFrameViewProperties(aFrame->GetPresContext(),
                                                aFrame, nsnull, view);
    }
  }

  // now do children of frame
  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;

  do {
    nsIFrame* child = aFrame->GetFirstChild(childList);
    while (child) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
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
    if (aChange & nsChangeHint_RepaintFrame) {
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

  viewManager->BeginUpdateViewBatch();

#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_TRUE;
#endif
  DoApplyRenderingChangeToTree(aFrame, viewManager, shell->FrameManager(),
                               aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = PR_FALSE;
#endif
  
  viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
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
  nsPresContext* presContext = aFrame->GetPresContext();
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
    ApplyRenderingChangeToTree(presContext, ancestor,
                               nsChangeHint_RepaintFrame);
  }
}

nsresult
nsCSSFrameConstructor::StyleChangeReflow(nsIFrame* aFrame,
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

  // If the frame is part of a split block-in-inline hierarchy, then
  // target the style-change reflow at the first ``normal'' ancestor
  // so we're sure that the style change will propagate to any
  // anonymously created siblings.
  if (IsFrameSpecial(aFrame))
    aFrame = GetIBContainingBlockFor(aFrame);

  aFrame->AddStateBits(NS_FRAME_IS_DIRTY);
  mPresShell->FrameNeedsReflow(aFrame, nsIPresShell::eStyleChange);

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
      nsIContent* blockContent = block->GetContent();
      nsStyleContext* blockSC = block->GetStyleContext();
      haveFirstLetterStyle = HaveFirstLetterStyle(blockContent, blockSC);
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
      if (hint & nsChangeHint_ReflowFrame) {
        StyleChangeReflow(frame, nsnull);
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

#ifdef DEBUG
    // reget from content since it may have been regenerated...
    if (content) {
      nsIFrame* frame = mPresShell->GetPrimaryFrameFor(content);
      if (frame) {
        mPresShell->FrameManager()->DebugVerifyStyleTree(frame);
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
                                nsGkAtoms::changeListProperty);
    }
  }

  aChangeList.Clear();
  return NS_OK;
}

void
nsCSSFrameConstructor::RestyleElement(nsIContent     *aContent,
                                      nsIFrame       *aPrimaryFrame,
                                      nsChangeHint   aMinHint)
{
  if (aMinHint & nsChangeHint_ReconstructFrame) {
    RecreateFramesForContent(aContent);
  } else if (aPrimaryFrame) {
    nsStyleChangeList changeList;
    if (aMinHint) {
      changeList.AppendChange(aPrimaryFrame, aContent, aMinHint);
    }
    nsChangeHint frameChange = mPresShell->FrameManager()->
      ComputeStyleChangeFor(aPrimaryFrame, &changeList, aMinHint);

    if (frameChange & nsChangeHint_ReconstructFrame) {
      RecreateFramesForContent(aContent);
      changeList.Clear();
    } else {
      ProcessRestyledFrames(changeList);
    }
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
      
    PostRestyleEvent(aContent, rshint, hint);
  }
}

nsresult
nsCSSFrameConstructor::AttributeChanged(nsIContent* aContent,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aModType)
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
                                                                  aModType);

  // Menus and such can't deal with asynchronous changes of display
  // when the menugenerated or menuactive attribute changes, so make
  // sure to process that immediately
  if (aNameSpaceID == kNameSpaceID_None &&
      ((aAttribute == nsGkAtoms::menugenerated &&
        aModType != nsIDOMMutationEvent::REMOVAL) ||
       aAttribute == nsGkAtoms::menuactive)) {
    PRInt32 namespaceID;
    nsIAtom* tag =
      mDocument->BindingManager()->ResolveTag(aContent, &namespaceID);

    if (namespaceID == kNameSpaceID_XUL &&
        (tag == nsGkAtoms::menupopup || tag == nsGkAtoms::popup ||
         tag == nsGkAtoms::tooltip || tag == nsGkAtoms::menu)) {
      nsIViewManager* viewManager = mPresShell->GetViewManager();
      viewManager->BeginUpdateViewBatch();
      ProcessOneRestyle(aContent, rshint, hint);
      viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);

      return result;
    }
  }

  PostRestyleEvent(aContent, rshint, hint);

  return result;
}

void
nsCSSFrameConstructor::EndUpdate()
{
  if (mUpdateCount == 1) {
    // This is the end of our last update.  Before we decrement
    // mUpdateCount, recalc quotes and counters as needed.

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
    NS_ASSERTION(mUpdateCount == 1, "Odd update count");
  }

  --mUpdateCount;
}

void
nsCSSFrameConstructor::WillDestroyFrameTree()
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

          headerFooterFrame = NS_STATIC_CAST(nsTableRowGroupFrame*,
            NS_NewTableRowGroupFrame(aPresShell, rowGroupFrame->GetStyleContext()));
          nsIContent* headerFooter = rowGroupFrame->GetContent();
          headerFooterFrame->Init(headerFooter, newFrame, nsnull);
          ProcessChildren(state, headerFooter, headerFooterFrame,
                          PR_FALSE, childItems, PR_FALSE);
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
  
  } else if (nsGkAtoms::areaFrame == frameType) {
    newFrame = NS_NewAreaFrame(shell, styleContext, 0);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);
    }
  
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
    nsIFrame* pageContentFrame;
    rv = ConstructPageFrame(shell, aPresContext, aParentFrame, aFrame,
                            newFrame, pageContentFrame);
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
    newFrame = NS_NewTableCellFrame(shell, styleContext, IsBorderCollapse(aParentFrame));

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);
      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      nsIFrame* continuingAreaFrame;
      nsIFrame* areaFrame = aFrame->GetFirstChild(nsnull);
      rv = CreateContinuingFrame(aPresContext, areaFrame, newFrame,
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
    rv = CreatePlaceholderFrameFor(shell, aPresContext, shell->FrameManager(),
                                   content, oofContFrame, styleContext,
                                   aParentFrame, &newFrame);
    if (NS_FAILED(rv)) {
      oofContFrame->Destroy();
      *aContinuingFrame = nsnull;
      return rv;
    }
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::fieldSetFrame == frameType) {
    newFrame = NS_NewFieldSetFrame(shell, styleContext);

    if (newFrame) {
      newFrame->Init(content, aParentFrame, aFrame);

      // XXXbz should we be passing in a non-null aContentParentFrame?
      nsHTMLContainerFrame::CreateViewForFrame(newFrame, nsnull, PR_FALSE);

      // Create a continuing area frame
      // XXXbz we really shouldn't have to do this by hand!
      nsIFrame* continuingAreaFrame;
      nsIFrame* areaFrame = GetFieldSetAreaFrame(aFrame);
      rv = CreateContinuingFrame(aPresContext, areaFrame, newFrame,
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
  
  // Now deal with fixed-pos things....  They should appear on all pages, and
  // the placeholders must be kids of a block, so we want to move over the
  // placeholders when processing the child of the pageContentFrame.
  if (!aParentFrame) {
    return NS_OK;
  }

  if (aParentFrame->GetType() != nsGkAtoms::pageContentFrame) {
    if (nextInFlow) {
      nextInFlow->SetPrevInFlow(newFrame);
      newFrame->SetNextInFlow(nextInFlow);
    } else if (nextContinuation) {
      nextContinuation->SetPrevContinuation(newFrame);
      newFrame->SetNextContinuation(nextContinuation);
    }
    return NS_OK;
  }

  // Our parent is a page content frame.  Look up its page frame and
  // see whether it has a prev-in-flow.
  nsIFrame* pageFrame = aParentFrame->GetParent();
  if (!pageFrame) {
    NS_ERROR("pageContentFrame does not have parent!");
    newFrame->Destroy();
    *aContinuingFrame = nsnull;
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
    newFrame->Destroy();
    *aContinuingFrame = nsnull;
    return NS_ERROR_UNEXPECTED;
  }
  
  nsFrameItems fixedPlaceholders;
  nsIFrame* firstFixed = prevPageContentFrame->GetFirstChild(nsGkAtoms::fixedList);
  if (!firstFixed) {
    return NS_OK;
  }

  nsFrameConstructorState state(mPresShell, aParentFrame,
                                mInitialContainingBlock,
                                mInitialContainingBlock);
  
  // Iterate the fixed frames and replicate each
  for (nsIFrame* fixed = firstFixed; fixed; fixed = fixed->GetNextSibling()) {
    rv = ConstructFrame(state, fixed->GetContent(),
                        newFrame, fixedPlaceholders);
    if (NS_FAILED(rv)) {
      newFrame->Destroy();
      *aContinuingFrame = nsnull;
      return rv;
    }
  }

  // Add the placeholders to our primary child list.
  // XXXbz this is a little screwed up, since the fixed frames will have the
  // wrong parent block and hence auto-positioning will be broken.  Oh, well.
  newFrame->SetInitialChildList(nsnull, fixedPlaceholders.childList);
  return NS_OK;
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
          if (aParentContent == kidContent ||
              (aParentContent && (aParentContent == kidContent->GetBindingParent()))) 
          {
#ifdef NOISY_FINDFRAME
            FFWC_recursions++;
            printf("  recursing with new parent set to kidframe=%p, parentContent=%p\n", 
                   kidFrame, aParentContent);
#endif
            nsIFrame* matchingFrame =
                FindFrameWithContent(aFrameManager, kidFrame,
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
        listName = aParentFrame->GetAdditionalChildListName(listIndex++);
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
nsCSSFrameConstructor::MaybeRecreateContainerForIBSplitterFrame(nsIFrame* aFrame, nsresult* aResult)
{
  if (!aFrame || !IsFrameSpecial(aFrame))
    return PR_FALSE;

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::RecreateFramesForContent: frame=");
    nsFrame::ListTag(stdout, aFrame);
    printf(" is special\n");
  }
#endif
  *aResult = ReframeContainingBlock(aFrame);
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
  // below!)

  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(aContent);

  nsresult rv = NS_OK;

  if (frame) {
    // If the frame is an anonymous frame created as part of inline-in-block
    // splitting --- or if its parent is such an anonymous frame (i.e., this
    // frame might have been the cause of such splitting), then recreate the
    // containing block.  Note that if |frame| is an inline, then it can't
    // possibly have caused the splitting, and if the inline is changing to a
    // block, any reframing that's needed will happen in ContentInserted.
    if (MaybeRecreateContainerForIBSplitterFrame(frame, &rv) ||
        (!IsInlineFrame(frame) &&
         MaybeRecreateContainerForIBSplitterFrame(frame->GetParent(), &rv)))
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
    rv = ContentRemoved(container, aContent, indexInContainer,
                        PR_FALSE);

    if (NS_SUCCEEDED(rv)) {
      // Now, recreate the frames associated with this content object.
      rv = ContentInserted(container, aContent,
                           indexInContainer, mTempFrameTreeState, PR_FALSE);
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
      event = newFrame ? nsIAccessibleEvent::EVENT_REORDER : nsIAccessibleEvent::EVENT_HIDE;
    }
    else {
      event = nsIAccessibleEvent::EVENT_SHOW;
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
nsCSSFrameConstructor::HaveFirstLetterStyle(nsIContent* aContent,
                                            nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       nsCSSPseudoElements::firstLetter,
                                       mPresShell->GetPresContext());
}

PRBool
nsCSSFrameConstructor::HaveFirstLineStyle(nsIContent* aContent,
                                          nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       nsCSSPseudoElements::firstLine,
                                       mPresShell->GetPresContext());
}

void
nsCSSFrameConstructor::HaveSpecialBlockStyle(nsIContent* aContent,
                                             nsStyleContext* aStyleContext,
                                             PRBool* aHaveFirstLetterStyle,
                                             PRBool* aHaveFirstLineStyle)
{
  *aHaveFirstLetterStyle =
    HaveFirstLetterStyle(aContent, aStyleContext);
  *aHaveFirstLineStyle =
    HaveFirstLineStyle(aContent, aStyleContext);
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
  nsStyleContext* styleContext = aFrame->GetStyleContext();
    
  if (aCanHaveGeneratedContent) {
    // Probe for generated content before
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::before,
                                    &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

 
  // save the incoming pseudo frame state
  nsPseudoFrames priorPseudoFrames;
  aState.mPseudoFrames.Reset(&priorPseudoFrames);

  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last);
       iter != last;
       ++iter) {
    rv = ConstructFrame(aState, nsCOMPtr<nsIContent>(*iter),
                        aFrame, aFrameItems);
    if (NS_FAILED(rv))
      return rv;
  }

  // process the current pseudo frame state
  if (!aState.mPseudoFrames.IsEmpty()) {
    ProcessPseudoFrames(aState, aFrameItems);
  }

  // restore the incoming pseudo frame state
  aState.mPseudoFrames = priorPseudoFrames;

  if (aCanHaveGeneratedContent) {
    // Probe for generated content after
    nsIFrame* generatedFrame;
    if (CreateGeneratedContentFrame(aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::after,
                                    &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
  }

  if (aParentIsBlock) {
    if (aState.mFirstLetterStyle) {
      rv = WrapFramesInFirstLetterFrame(aState, aContent, aFrame, aFrameItems);
    }
    if (aState.mFirstLineStyle) {
      rv = WrapFramesInFirstLineFrame(aState, aContent, aFrame, aFrameItems);
    }
  }

  return rv;
}

//----------------------------------------------------------------------

// Support for :first-line style

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
  nsStyleContext* parentStyle = aBlockFrame->GetStyleContext();
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
  PRBool isInline = IsInlineFrame(newFrame);

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
          nsStyleContext* parentStyle = aBlockFrame->GetStyleContext();
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
  NS_ASSERTION(letterContent->GetBindingParent() != letterContent,
               "Reframes of this letter frame will mess with the root of a "
               "native anonymous content subtree!");
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

  rv = aState.AddChild(letterFrame, aResult, letterFrame->GetStyleDisplay(),
                       letterContent, aStyleContext, aParentFrame, PR_FALSE,
                       PR_TRUE, PR_FALSE, PR_TRUE, insertAfter);

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

  // Get style context for the first-letter-frame
  nsStyleContext* parentStyleContext = aParentFrame->GetStyleContext();
  if (parentStyleContext) {
    // Use content from containing block so that we can actually
    // find a matching style rule.
    nsIContent* blockContent = aState.mFloatedItems.containingBlock->GetContent();

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
          NS_ASSERTION(letterContent->GetBindingParent() != letterContent,
                       "Reframes of this letter frame will mess with the root "
                       "of a native anonymous content subtree!");
          letterFrame->Init(letterContent, aParentFrame, nsnull);

          InitAndRestoreFrame(aState, aTextContent, letterFrame, nsnull,
                              textFrame);

          letterFrame->SetInitialChildList(nsnull, textFrame);
          aResult.childList = aResult.lastChild = letterFrame;
        }
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
    else if ((nsGkAtoms::inlineFrame == frameType) ||
             (nsGkAtoms::lineFrame == frameType) ||
             (nsGkAtoms::positionedInlineFrame == frameType)) {
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
  nsIFrame* nextTextFrame = textFrame->GetNextInFlow();
  if (nextTextFrame) {
    nsIFrame* nextTextParent = nextTextFrame->GetParent();
    if (nextTextParent) {
      nsSplittableFrame::BreakFromPrevFlow(nextTextFrame);
      ::DeletingFrameSubtree(aFrameManager, nextTextFrame);
      aFrameManager->RemoveFrame(nextTextParent, nsnull, nextTextFrame);
    }
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
  // Should we call DeletingFrameSubtree on the placeholder instead
  // and skip this call?
  aFrameManager->UnregisterPlaceholderFrame(placeholderFrame);

  // Remove the float frame
  ::DeletingFrameSubtree(aFrameManager, floatFrame);
  aFrameManager->RemoveFrame(aBlockFrame, nsGkAtoms::floatList,
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
    if (nsGkAtoms::letterFrame == frameType) {
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
      break;
    }
    else if ((nsGkAtoms::inlineFrame == frameType) ||
             (nsGkAtoms::lineFrame == frameType) ||
             (nsGkAtoms::positionedInlineFrame == frameType)) {
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
nsCSSFrameConstructor::RecoverLetterFrames(nsFrameConstructorState& aState,
                                           nsIFrame* aBlockFrame)
{
  nsresult rv = NS_OK;

  nsIFrame* blockKids = aBlockFrame->GetFirstChild(nsnull);
  nsIFrame* parentFrame = nsnull;
  nsIFrame* textFrame = nsnull;
  nsIFrame* prevFrame = nsnull;
  nsFrameItems letterFrames;
  PRBool stopLooking = PR_FALSE;
  rv = WrapFramesInFirstLetterFrame(aState, aBlockFrame, aBlockFrame, blockKids,
                                    &parentFrame, &textFrame, &prevFrame,
                                    letterFrames, &stopLooking);
  if (NS_FAILED(rv)) {
    return rv;
  }
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

    rv = ConstructFrameInternal(state, aChild,
                                aParentFrame, aChild->Tag(),
                                aChild->GetNameSpaceID(),
                                styleContext, frameItems, PR_FALSE);
    
    nsIFrame* newFrame = frameItems.childList;
    *aNewFrame = newFrame;

    if (NS_SUCCEEDED(rv) && (nsnull != newFrame)) {
      // Notify the parent frame
      if (aIsAppend)
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxAppendFrames(newFrame);
      else
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxInsertFrames(aPrevFrame, newFrame);
    }
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

  nsresult rv = aState.AddChild(*aNewFrame, aFrameItems, aDisplay,
                                aContent, aStyleContext,
                                aContentParentFrame ? aContentParentFrame :
                                                      aParentFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // See if we need to create a view, e.g. the frame is absolutely positioned
  nsHTMLContainerFrame::CreateViewForFrame(blockFrame, contentParent, PR_FALSE);

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
  HaveSpecialBlockStyle(aContent, aStyleContext,
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
      
    blockFrame = NS_NewRelativeItemWrapperFrame(mPresShell, blockSC);
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
  MoveChildrenTo(state.mFrameManager, blockSC, blockFrame, list2, &state, &aState);

  // list3's frames belong to another inline frame
  nsIFrame* inlineFrame = nsnull;

  if (list3) {
    if (aIsPositioned) {
      inlineFrame = NS_NewPositionedInlineFrame(mPresShell, aStyleContext);
    }
    else {
      inlineFrame = NS_NewInlineFrame(mPresShell, aStyleContext);
    }

    InitAndRestoreFrame(aState, aContent, aParentFrame, nsnull, inlineFrame, PR_FALSE);

    // Any frame might need a view
    // XXXbz should we be passing in a non-null aContentParentFrame?
    nsHTMLContainerFrame::CreateViewForFrame(inlineFrame, nsnull, PR_FALSE);

    if (inlineFrame->HasView() || aNewFrame->HasView()) {
      // Move list3's frames into the new view
      nsHTMLContainerFrame::ReparentFrameViewList(aState.mPresContext, list3,
                                                  list3->GetParent(), inlineFrame);
    }

    // Reparent (cheaply) the frames in list3 - we don't have to futz
    // with their style context because they already have the right one.
    inlineFrame->SetInitialChildList(nsnull, list3);
    MoveChildrenTo(aState.mFrameManager, nsnull, inlineFrame, list3, nsnull, nsnull);
  }

  // Mark the 3 frames as special. That way if any of the
  // append/insert/remove methods try to fiddle with the children, the
  // containing block will be reframed instead.
  SetFrameIsSpecial(aNewFrame, blockFrame);
  SetFrameIsSpecial(blockFrame, inlineFrame);
  MarkIBSpecialPrevSibling(aState.mPresContext, blockFrame, aNewFrame);

  if (inlineFrame)
    SetFrameIsSpecial(inlineFrame, nsnull);

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
    nsIFrame* generatedFrame;
    styleContext = aFrame->GetStyleContext();
    if (CreateGeneratedContentFrame(aState, aFrame, aContent,
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
    if (CreateGeneratedContentFrame(aState, aFrame, aContent,
                                    styleContext, nsCSSPseudoElements::after,
                                    &generatedFrame)) {
      // Add the generated frame to the child list
      aFrameItems.AddChild(generatedFrame);
    }
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

PRBool
nsCSSFrameConstructor::WipeContainingBlock(nsFrameConstructorState& aState,
                                           nsIFrame* aContainingBlock,
                                           nsIFrame* aFrame,
                                           nsIFrame* aFrameList)
{
  // Before we go and append the frames, check for a special
  // situation: an inline frame that will now contain block
  // frames. This is a no-no and the frame construction logic knows
  // how to fix this.

  // If we don't have a block within an inline, just return false.  Here
  // "an inline" is an actual inline frame (positioned or not) or a lineframe
  // (corresponding to :first-line), since the latter should stop at the first
  // block it runs into and we might be inserting one in the middle of it.
  // Whether we have "a block" is tested for by AreAllKidsInline.
  nsIAtom* frameType = aFrame->GetType();
  if ((frameType != nsGkAtoms::inlineFrame &&
       frameType != nsGkAtoms::positionedInlineFrame &&
       frameType != nsGkAtoms::lineFrame) ||
      AreAllKidsInline(aFrameList))
    return PR_FALSE;

  // Ok, reverse tracks: wipe out the frames we just created
  nsFrameManager *frameManager = aState.mFrameManager;

  // Destroy the frames. As we do make sure any content to frame mappings
  // or entries in the undisplayed content map are removed
  frameManager->ClearAllUndisplayedContentIn(aFrame->GetContent());

  CleanupFrameReferences(frameManager, aFrameList);
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
  nsFrameList tmp(aFrameList);
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
  while (IsFrameSpecial(aContainingBlock) || IsInlineFrame(aContainingBlock) ||
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
           NS_STATIC_CAST(void*, blockContent),
           NS_STATIC_CAST(void*, parentContainer));
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
           NS_STATIC_CAST(void*, aFrame));
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
                 NS_STATIC_CAST(void*, blockContent),
                 NS_STATIC_CAST(void*, parentContainer));
        }
#endif
        return ReinsertContent(parentContainer, blockContent);
      }
    }
  }

  // If we get here, we're screwed!
  return ReconstructDocElementHierarchyInternal();
}

nsresult nsCSSFrameConstructor::RemoveFixedItems(const nsFrameConstructorState& aState)
{
  nsresult rv=NS_OK;

  if (mFixedContainingBlock) {
    nsIFrame *fixedChild = nsnull;
    do {
      fixedChild = mFixedContainingBlock->GetFirstChild(nsGkAtoms::fixedList);
      if (fixedChild) {
        // Remove the placeholder so it doesn't end up sitting about pointing
        // to the removed fixed frame.
        nsIFrame *placeholderFrame;
        mPresShell->GetPlaceholderFrameFor(fixedChild, &placeholderFrame);
        NS_ASSERTION(placeholderFrame, "no placeholder for fixed-pos frame");
        NS_ASSERTION(placeholderFrame->GetType() ==
                     nsGkAtoms::placeholderFrame,
                     "Wrong type");
        aState.mFrameManager->UnregisterPlaceholderFrame(
          NS_STATIC_CAST(nsPlaceholderFrame*, placeholderFrame));
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

void
nsCSSFrameConstructor::ProcessPendingRestyles()
{
  PRUint32 count = mPendingRestyles.Count();
  if (!count) {
    // Nothing to do
    return;
  }
  
  NS_PRECONDITION(mDocument, "No document?  Pshaw!\n");

  nsCSSFrameConstructor::RestyleEnumerateData* restylesToProcess =
    new nsCSSFrameConstructor::RestyleEnumerateData[count];
  if (!restylesToProcess) {
    return;
  }

  nsCSSFrameConstructor::RestyleEnumerateData* lastRestyle = restylesToProcess;
  mPendingRestyles.Enumerate(CollectRestyles, &lastRestyle);

  NS_ASSERTION(lastRestyle - restylesToProcess ==
               PRInt32(count),
               "Enumeration screwed up somehow");

  // Clear the hashtable so we don't end up trying to process a restyle we're
  // already processing, sending us into an infinite loop.
  mPendingRestyles.Clear();

  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  BeginUpdate();

  for (nsCSSFrameConstructor::RestyleEnumerateData* currentRestyle =
         restylesToProcess;
       currentRestyle != lastRestyle;
       ++currentRestyle) {
    ProcessOneRestyle(currentRestyle->mContent,
                      currentRestyle->mRestyleHint,
                      currentRestyle->mChangeHint);
  }

  delete [] restylesToProcess;

  // Run the XBL binding constructors for any new frames we've constructed.
  // Note that the restyle event is holding a strong ref to us, so we're ok
  // here.
  mDocument->BindingManager()->ProcessAttachedQueue();

  EndUpdate();

#ifdef DEBUG
  mPresShell->VerifyStyleTree();
#endif
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

NS_IMETHODIMP nsCSSFrameConstructor::RestyleEvent::Run() {
  if (!mConstructor)
    return NS_OK;  // event was revoked

  nsIViewManager* viewManager =
    mConstructor->mPresShell->GetViewManager();
  NS_ASSERTION(viewManager, "Must have view manager for update");

  viewManager->BeginUpdateViewBatch();
  // Force flushing of any pending content notifications that might have queued
  // up while our event was pending.  That will ensure that we don't construct
  // frames for content right now that's still waiting to be notified on,
  mConstructor->mPresShell->GetDocument()->
    FlushPendingNotifications(Flush_ContentAndNotify);

  // Make sure that any restyles that happen from now on will go into
  // a new event.
  mConstructor->mRestyleEvent.Forget();

  mConstructor->ProcessPendingRestyles();
  viewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);

  return NS_OK;
}

